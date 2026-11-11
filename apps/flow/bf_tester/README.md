# bf_tester

Hairpin forwarding on a BlueField / ConnectX (mlx5) port with large HW steering
rule sets. Traffic received on port 0 is steered by `rte_flow` rules to hairpin
queues and sent back out of the same port without touching the CPU.

Measurements use synchronous `rte_flow_create()` (`--sync`), with all worker
lcores inserting in parallel. The async template API (default without
`--sync`) inserts ~100x faster but forwards slower with millions of rules on
this card; it is kept for future reference only and is not tuned.

## Modes

The modes follow the measurement plan and are incremental: each one contains
everything from the previous one.

| Mode       | Step | Group 1 rules                                              | Status          |
|------------|------|------------------------------------------------------------|-----------------|
| `single`   | base | one `eth / ipv4 / tcp` rule → RSS(ipv4-tcp) to hairpin q.  | implemented     |
| `per-flow` | 1    | one exact TCP 5-tuple rule per flow → RSS to hairpin q.    | implemented     |
| `edit_mac` | 2a   | + source MAC := port MAC (fixed value)                     | implemented     |
| `insert`   | 2b   | + push VLAN 3 (fixed value)                                | implemented     |
| `count`    | 3a   | + counter per rule                                         | implemented     |
| `meter`    | 3b   | + meter per rule (1 Gbit/s, mark only)                     | implemented     |
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
| `--tables` | Spread the rules over N tables/groups (power of two, default 1) by the lowest bits of the source IP; group 0 jumps straight to the right one. 8 was best for 16M rules. |
| `-H, --hp-queues` | Number of hairpin queues the traffic is spread over (default 8). |
| `--check-miss` | Count packets matching no rule and report them every interval and at exit. |
| `--insert-only` | Insert the rules, print the insertion rate and exit (no traffic phase). |
| `-t, --stats-interval` | Seconds between stats lines (default 1). |
| `--no-stats` | Do not read any statistics or counters while forwarding; the app just waits for Ctrl+C (the numbers come from Spirent). Cannot be combined with `--check-miss`. |
| `--sync` | Create rules synchronously with `rte_flow_create()` (as testpmd) instead of the async template API. Use it for measurements. Works with SW steering (default `dv_flow_en=1`) and with HW steering (`dv_flow_en=2`). |
| `--egress-vlan` | Modes `insert` and later, `--sync` only: push the VLAN in two egress rules (egress group 0 jumps to group 1, group 1 pushes VLAN 3 on everything leaving the port) instead of in the main ingress rules. Pushing on ingress drops ~32 % at 400G with 2M rules; on egress it forwards at full rate. |
| `--flow-queue-size`, `--push-burst` | Async only (future reference): operations in flight per lcore (1024) and operations pushed to HW at once (64). |

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

* `main_actions_build()`: add the actions in the order the HW applies them
  (counter, meters, push VLAN, modify, fate action);
* `bf_flow_engine_configure()`: reserve the objects (counters, meters) in
  `rte_flow_configure()`;
* `bf_sync_worker()`: fill per-rule action values if they differ per flow.

Meters (`meter` mode): every rule has a `METER_MARK` action with one shared
srTCM profile, CIR 1 Gbit/s (`BF_METER_CIR_BYTES`). One flow carries ~0.2
Mbit/s, so all packets stay green: the meter only marks the color, nothing is
dropped, and the measurement shows the cost of metering alone. The PMD creates
a meter object for every rule and waits for the HW to set it up, so 2M rules
take ~160 s. Meters need HW steering (`dv_flow_en=2`); the card has at most
33,554,432.

Counters are wired: in `count` and later modes a counter is attached to every
rule and at exit the app reports how many flows received no packets. The card
has at most 15,728,640 counters, so 16M rules with a counter each do not fit
(the app stops with an error before inserting anything).

With `--sync` the rules with a counter or a meter are inserted one at a time
(the worker lcores take a lock around `rte_flow_create()`): mlx5 in DPDK 25.11
takes the counter of every sync rule from the cache of flow queue 0, a
single-producer / single-consumer ring, and parallel calls corrupt it (crash or
failed insertion); a sync meter takes a job from the control queue without a
lock. 2M rules with a counter take ~30 s.

Before forwarding starts the app waits 1 s, because the mlx5 counter values
are refreshed in the background (every 500 ms by default) and a query returns
the last refreshed value. Then it resets the per-flow counters, so the counters,
the rates and the summary all cover only the forwarding phase. Spirent can run
the whole time.
