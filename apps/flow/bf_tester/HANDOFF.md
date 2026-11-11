# bf_tester: handoff

State of the work on `bf_tester` for whoever continues it. Measurement numbers are
from the user's own notes (`notes.md`) and from runs made during development.

## Goal

Measure a BlueField-3 forwarding Spirent traffic back through hairpin queues with
`rte_flow` rules, in incremental steps (spec from the user):

| Step | Mode | Status |
|---|---|---|
| base | `single`: one eth/ipv4/tcp rule → RSS over hairpin queues | done |
| 1 | `per-flow`: one exact TCP 5-tuple rule per flow, 2M/4M/8M/16M flows | done |
| 2a | `edit_mac`: + source MAC := port MAC | done |
| 2b | `insert`: + push VLAN 3 | done (slow on ingress, see below) |
| 3a | `count`: + counter per rule | not implemented |
| 3b | `meter`: + meter per rule | not implemented |
| 4 | `all`: + counter and two meters per rule | not implemented |

Each mode includes everything from the previous one. Output of each measurement:
Mpps and Gbps (the reference numbers come from the Spirent counters).

## Working rules agreed with the user

- The user writes in Czech; reply in Czech. Code, comments and commit messages are in English.
- **Never edit `notes.md`.** It is the user's own file. Report results in chat.
- **Commit only when asked.** Use Conventional Commits and the `Co-Authored-By` line.
- **Say so whenever a change touches an existing mode.**
- **No new command line options** unless the user asks for them. Use fixed values.
- **Measurements use `--sync`.** The async template API is kept for future
  reference only: do not tune it and do not run it.
- **`sudo` is allowed only to run `bf_tester` on dpdk-test12.** Ask the user to
  run anything else privileged or network-related (switch, `mlxlink`, `ip link`).
- **Analyses must state what is verified and how sure you are; never invent facts.**
  When the user asks for analysis only, don't change code and don't run anything.

## Environment

- **Host:** `dpdk-test12` (ssh alias, user `xpatov00`). It has 128 cores, 1 NUMA node and 10× 1 GB hugepages.
  - **Software:** DOCA-OFED 3.3 (mlx5_core 26.01) and DPDK 25.11 from the `lbr_dpdk` rpm.
    The DPDK sources are in `/usr/src/debug/lbr_dpdk-25.11-6.el9.x86_64` and are useful for reading the mlx5/HWS code.
  - **NIC:** BlueField-3, port 0 is `0000:21:00.0` = `enp33s0f0np0` (MAC `e8:9e:49:7f:27:ce`).
    It runs 400G on PCIe Gen5 x16, FW 32.49.1014. Port 1 (`0000:21:00.1`) is not used.
- **Build:** copy `apps/` to `~/bf_tester_build/apps` on the host, then
  `cd ~/bf_tester_build/build && cmake ../apps && make bf_tester`.
- **Run:**
  ```
  sudo ~/bf_tester_build/build/flow/bf_tester/bf_tester -a 0000:21:00.0,dv_flow_en=2 \
      -n 6 -l 96-112 --socket-mem=4096 -- -M per-flow --src-ip 192.85.1.2,2000 --src-port 1024,1001 --sync
  ```
  Argument parsing can be tested without the card: `--no-pci --no-huge --in-memory -l 0`.
- **Traffic:** Spirent TestCenter, a single port `//10/1`, connected through a switch in VLAN 3
  (access ports, managed with `/home/patova/work/switchctl`). The stream blocks are 2M/4M/8M/16M:
  - fields: src IP `192.85.1.2`, count 2000/4000/8000/16000; dst IP `192.0.0.1`;
    TCP sport `1024`, count 1001; dport `1024`.
  - modifiers are not linked, so all fields step together (lockstep), as `bf_tester` generates them.
  - dst MAC `00:00:01:00:00:01`, iMIX with average ~744 B. 400G corresponds to ~65.5 Mpps.
  - To see the incoming rate: delta of `ethtool -S enp33s0f0np0 | grep rx_packets_phy`.

### Pitfalls

- **The switch floods.** It never learns the dst MAC, so it floods every frame to all ports in VLAN 3.
  - After a reboot, BF port 1 received a copy of the stream. That halved the throughput, even in `single` mode.
  - It was fixed by removing port 1 from the VLAN. `ip link set down` does **not** take the physical link down.
  - Before measuring, check that `rx_packets_phy` on `enp33s0f1np1` is not increasing.
- **The rules must match the Spirent stream.** If `rx - tx` is much larger than `rx_phy_discard_packets`,
  packets are missing the rules: the miss rule drops them. That usually means Spirent runs a different stream block.
- **Losses on the NIC show as `rx_phy_discard_packets`** together with `tx_pause_ctrl_phy`
  (the NIC sends pause). `hairpin_out_of_buffer` stayed 0 in every run.

## Code

All the code is in `apps/flow/bf_tester/`. See `README.md` for the options.

| File | Content |
|---|---|
| `argparse.c/.h` | Options and `bf_mode_table` (the features of each mode) |
| `bf_tester.c` | `main`, port and hairpin setup (self-peered, manual bind, tx_explicit), config/setup print, traffic loop |
| `flow_rules.c/.h` | Common part: flow generation, engine configure, rule building blocks (`main_actions_build`), parallel insertion, counters |
| `flow_sync.c` | `rte_flow_create()` path (used for measurements) |
| `flow_async.c` | Template/async path (future reference only) |
| `flow_internal.h` | Internals shared by the `flow_*.c` files |
| `stats.c/.h` | Rates from mlx5 xstats (`rx_phy_*`, `tx_phy_*`), summary |

Rule layout:
- **group 0:** a jump to group 1, or with `--tables N` a jump to group `1+(src IP & (N-1))`.
- **group 1..N:** the matching rules, plus a lower-priority miss rule that drops (with a counter when `--check-miss` is given).
- Actions must be listed in HW order: counter, meters, push VLAN, modify, fate (RSS).

## Findings

**Insertion speed:**
- Sync: 0.1–0.34 Mrules/s with 16 lcores in parallel. The mlx5 flow API is thread-safe.
- Async: ~23 Mrules/s.

**Capacity:**
- An HWS template table holds at most 2^23 rules (the card reports `ste_alloc_log_max = 25`).
  With the table sized to 70 % fill, async tops out at 5.87M rules.
- Sync takes 16,016,000 rules. 32M fails at ~10.5M (BWC collisions).
- Counters: at most 15,728,640, so 16M rules with a counter each do not fit.

**Throughput, sync, 1 table, Spirent at 400G:**

| Case | TX |
|---|---|
| single | ~390 Gbps (full) |
| per-flow 2M | 389.7 Gbps |
| per-flow 4M | 389.2 Gbps |
| per-flow 16M | 237.5 Gbps |

**16M rules spread over tables (`--tables`, sync):**

| Tables | TX |
|---|---|
| 4 | 300.7 Gbps |
| 8 | 371.5 Gbps (best, ~5 % dropped on ingress) |
| 16 | 354.4 Gbps |

- More tables help, but only without chaining: group 0 sends each packet to exactly one table.
- Hypothesis for the remaining loss, not verified: with 16M flows in round robin the rules miss
  the NIC cache. Test: let Spirent send only a subset of the flows.

**Modes 2a/2b, 2M, sync:**

| Mode | TX |
|---|---|
| edit_mac | 389.8 Gbps (full) |
| insert, VLAN pushed on ingress | 267.6 Gbps (~32 % dropped on ingress) |
| insert, VLAN pushed on **egress** (`--egress-vlan`) | 389.9 Gbps |

- With the egress VLAN, the remaining ~0.44 % of packets is the extra 4 B per packet over 400G line rate.
- In HWS, push VLAN is an inline `HEADER_INSERT` with `REPARSE_ALWAYS` (`hws/mlx5dr_action.c`).
  The hypothesis is that the reparse costs ingress capacity. No DPDK devarg controls it.
- `QUEUE` instead of `RSS` would not help: both map to `MLX5DR_ACTION_TYP_TIR`.

**Async (reference only):** forwards slower than sync with many rules:
- 2M rules at 95 % fill: 334 Gbps.
- 4M rules at 48 % fill: 330 Gbps.

The probable cause is `mlx5dr_definer_optimize_order()`. For non-resizable template matchers it
reorders the key for IPv4-address hashing, which suits this stream badly. An untested fix is
`RTE_FLOW_TABLE_SPECIALIZE_RESIZABLE`, which is what the sync (BWC) matchers use.

## Open items

1. **Egress VLAN is now the `--egress-vlan` option** (it replaced the `BF_TMP_EGRESS_VLAN` define).
   - Off by default, so `insert --sync` pushes the VLAN on ingress again.
   - Allowed only in modes that push a VLAN (`insert` and later) and only with `--sync`.
   - It adds two egress rules (egress group 0 jumps to 1, group 1 pushes VLAN 3) and
     no VLAN push in the ingress rules.
2. **`--check-miss` does not print the miss result.**
   `bf_flow_counter_read()` → `rte_flow_query()` fails on the miss rules,
   so the summary has no miss line or verdict. Not investigated.
3. **Modes `count`, `meter`, `all`.** See the TODOs in `main_actions_build()`.
   Meters need `nb_meters` in `rte_flow_configure()`. Mind the 15.7M counter limit.
4. **16M: the remaining ~5 % loss** with 8 tables (cache hypothesis above).
5. **NFB support (analysis only, nothing implemented).**
   - The DynaNIC nfb PMD in `~/work/sn4dc-dpdk` has hairpin, sync rte_flow, and async only for FST groups.
   - It has no `rte_flow_info_get`, no xstats and no meters, and its groups are fixed by the firmware.
   - Needed: a per-driver layer for groups and the insertion type, and stats from `rte_eth_stats_get()`.
