# bf_tester

Hairpin forwarding on a BlueField / ConnectX (mlx5) port with large HW steering
rule sets. Traffic received on port 0 is steered by `rte_flow` rules to hairpin
queues and sent back out of the same port without touching the CPU.

Rules are created with the template (async) flow API: every worker lcore owns a
flow queue and inserts its share of rules with postponed operations, pushed in
bursts. This is what makes millions of rules practical; synchronous
`rte_flow_create()` (as used by testpmd's `flow create`) is far slower.

## Modes

The modes follow the measurement plan and are incremental: each one contains
everything from the previous one.

| Mode       | Step | Group 1 rules                                              | Status          |
|------------|------|------------------------------------------------------------|-----------------|
| `single`   | base | one `eth / ipv4 / tcp` rule → RSS(ipv4-tcp) to hairpin q.  | implemented     |
| `per-flow` | 1    | one exact TCP 5-tuple rule per flow → RSS to hairpin q.    | implemented     |
| `edit_mac` | 2a   | + source MAC := port MAC (fixed value)                     | implemented     |
| `insert`   | 2b   | + push VLAN 3 (fixed value)                                | implemented     |
| `count`    | 3a   | + counter per rule                                         | not implemented |
| `meter`    | 3b   | + meter per rule                                           | not implemented |
| `all`      | 4    | + counter and two meters per rule                          | not implemented |

In every mode group 0 holds one rule jumping to group 1, and group 1 holds a
lower-priority miss rule that drops packets matching no rule.

## Requirements

* The async rule API (default) needs HW steering on the device:
  `-a <pci>,dv_flow_en=2`. With `--sync` the default SW steering works too.
* Root privileges (hugepages, device access).

## Build

```bash
mkdir build && cd build
cmake ../apps && make bf_tester
```

## Options

| Option | Meaning |
|--------|---------|
| `-M, --mode` | Measurement mode, see above (default `single`). |
| `--src-ip`, `--dst-ip`, `--src-port`, `--dst-port` | 5-tuple of the generated rules as `BASE[,COUNT[,STEP]]`, the same as the Spirent modifiers. Defaults are the stream block bases (192.85.1.2, 192.0.0.1, 1024, 1024), so usually only the counts are given. |
| `-n, --flows` | Number of rules; default is all distinct flows of the stream. |
| `-H, --hp-queues` | Number of hairpin queues the traffic is spread over (default 8). |
| `--check-miss` | Count packets matching no rule and report them every interval and at exit. |
| `--insert-only` | Insert the rules, print the insertion rate and exit (no traffic phase). |
| `-t, --stats-interval` | Seconds between stats lines (default 1). |
| `--no-stats` | Do not read any statistics or counters while forwarding; the app just waits for Ctrl+C (the numbers come from Spirent). Cannot be combined with `--check-miss`. |
| `--sync` | Create rules synchronously with `rte_flow_create()` (as testpmd) instead of the async template API. Works with SW steering (default `dv_flow_en=1`) and with HW steering (`dv_flow_en=2`). |
| `--flow-queue-size`, `--push-burst` | Async insertion tuning: operations in flight per lcore (1024) and operations pushed to HW at once (64). |

The number of lcores (`-l`) sets the insertion parallelism: every lcore except
the main one inserts rules, in both async and `--sync` mode (the mlx5 flow API
is thread-safe).

## Run

Baseline, equivalent of the testpmd setup in `bf_tester/config.md`:

```bash
sudo ./flow/bf_tester/bf_tester -a 0000:21:00.0,dv_flow_en=2 -n 6 -l 96-112 --socket-mem=4096 -- \
    -M single
```

If 8 hairpin queues do not reach 400G, try more, e.g. `-H 32`.

Per-flow rules for the Spirent stream block (src IP 192.85.1.2 x2000,
dst IP 192.0.0.1, src port 1024 x1001, dst port 1024), 2 002 000 rules:

```bash
sudo ./flow/bf_tester/bf_tester -a 0000:21:00.0,dv_flow_en=2 -n 6 -l 96-112 --socket-mem=4096 -- \
    -M per-flow --src-ip 192.85.1.2,2000 --src-port 1024,1001 --check-miss
```

For 4M/8M/16M rules use 4000/8000/16000 source IPs both in Spirent and in
`--src-ip`.

The rules are generated the way Spirent generates the stream (modifiers not
linked): all fields step with every packet and each wraps at its own count, so
the stream repeats after the least common multiple of the counts. Keep the
counts coprime (2000/4000/8000/16000 and 1001 are) to get count x count flows.
The app prints the number of flows and the first and last tuple.

Synchronous insertion as in testpmd (SW steering), for comparison:

```bash
sudo ./flow/bf_tester/bf_tester -a 0000:21:00.0 -n 6 -l 96-112 --socket-mem=4096 -- \
    -M per-flow --src-ip 192.85.1.2,2000 --src-port 1024,1001 --sync
```

`Ctrl+C` ends the traffic phase and prints a summary (not with `--no-stats`).

## Output

```
[   12.0s] RX ... Mpps ... Gbps | TX ... Mpps ... Gbps | rx_disc 0 tx_disc 0 rx_oob 0 hp_oob 0 | miss 0
```

Rates come from the mlx5 physical port xstats (`rx_phy_*`, `tx_phy_*`), because
hairpinned packets never reach the SW queues. Byte rates are L2 (no
preamble/IFG); the reference numbers are taken from the Spirent counters.
`hp_oob` is `hairpin_out_of_buffer` (drops caused by full hairpin buffers).
With `--check-miss` the line ends with `miss 0` or `MISS <n> pkts !!` and the
summary prints an OK/FAILED verdict.

## Implementing the next modes

The features of each mode are in `bf_mode_table` (`argparse.c`). To implement
one, set `.implemented = true` and fill the TODOs in `flow_rules.c`:

* `main_actions_build()`: add the actions (e.g. `METER_MARK`) in the order the
  HW applies them (counter, meters, push VLAN, modify, fate action);
* `bf_flow_engine_configure()`: reserve meters (`nb_meters`) in
  `rte_flow_configure()`;
* `bf_sync_worker()`: fill per-rule action values if they differ per flow.

Counters are already wired: in `count` and later modes a counter is attached
to every rule and at exit the app reports how many flows received no packets.
