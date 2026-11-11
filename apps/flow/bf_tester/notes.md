# Base test

```
[xpatov00@dpdk-test12 bf_tester]$ sudo ./bf_tester -a 0000:21:00.0,dv_flow_en=2 -n 6 -l 96-112 --socket-mem=4096 -- --src-ip 192.85.1.2,16000 --src-port 1024,1001 -M single --sync
EAL: Detected CPU lcores: 128
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: VFIO support initialized
EAL: TOPOLOGY - core count: l1 17, l2 17, l3 17, l4 0, io 17

=== bf_tester ===
Mode:            single (baseline: one eth/ipv4/tcp rule -> RSS to hairpin queues)
Queues:          1 standard (0-0), 8 hairpin (1-8)
Rule API:        sync (rte_flow_create)
Statistics:      on
Miss check:      off
Lcores:          17 (1 main + 16 inserting workers)


=== Flow setup ===
Port:            0 (mlx5_pci), max rxq 1024, max txq 1024
Steering:        HW (dv_flow_en=2), counters 0 used of max 15728640
Rules:           group 0  all -> jump group 1
                 group 1  eth/ipv4/tcp -> RSS to hairpin queues 1-8
                 group 1  miss -> drop
Sync API:
  Insertion:     rte_flow_create(), 16 worker lcore(s) in parallel
```

```
=== Traffic summary (60.4 s) ===
  RX average                      65.47 Mpps     389.74 Gbps
  TX average                      65.47 Mpps     389.74 Gbps
  rx_phy_packets                     3951731754
  rx_phy_bytes                    2940713915346
  tx_phy_packets                     3951706156
  tx_phy_bytes                    2940695493964
  rx_phy_discard_packets                      0
  tx_phy_discard_packets                      0
  rx_out_of_buffer                            0
  hairpin_out_of_buffer                       0
  rx - tx                                 25598
```

Speedmeter TX i RX na spirentu ukazuje 400G.

# Per-Flow

## 16M 1 tabulka
```
[xpatov00@dpdk-test12 bf_tester]$ sudo ./bf_tester -a 0000:21:00.0,dv_flow_en=2 -n 6 -l 96-112 --socket-mem=4096 -- --src-ip 192.85.1.2,16000 --src-port 1024,1001 -M per-flow --sync
EAL: Detected CPU lcores: 128
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: VFIO support initialized
EAL: TOPOLOGY - core count: l1 17, l2 17, l3 17, l4 0, io 17

=== bf_tester ===
Mode:            per-flow (one exact 5-tuple rule per flow -> RSS to hairpin queues)
Queues:          1 standard (0-0), 8 hairpin (1-8)
Rule API:        sync (rte_flow_create)
Statistics:      on
Miss check:      off
Flows:           16016000
  first #0: 192.85.1.2:1024 -> 192.0.0.1:1024 tcp
  last  #16015999: 192.85.63.129:2024 -> 192.0.0.1:1024 tcp
Lcores:          17 (1 main + 16 inserting workers)


=== Flow setup ===
Port:            0 (mlx5_pci), max rxq 1024, max txq 1024
Steering:        HW (dv_flow_en=2), counters 0 used of max 15728640
Rules:           group 0  all -> jump group 1
                 group 1  16016000 x tcp 5-tuple -> RSS to hairpin queues 1-8 (inserted below)
                 group 1  miss -> drop
Sync API:
  Insertion:     rte_flow_create(), 16 worker lcore(s) in parallel

Inserting 16016000 rules on 16 lcore(s), sync rte_flow_create()...
Rules inserted: 16016000 ok, 0 failed in 47.161 s = 0.340 Mrules/s
```

```
=== Traffic summary (71.0 s) ===
  RX average                      65.46 Mpps     389.73 Gbps
  TX average                      39.51 Mpps     237.40 Gbps
  rx_phy_packets                     4646614173
  rx_phy_bytes                    3457816477500
  tx_phy_packets                     2804683464
  tx_phy_bytes                    2106287566802
  rx_phy_discard_packets             1809924174
  tx_phy_discard_packets                      0
  rx_out_of_buffer                            0
  hairpin_out_of_buffer                       0
  rx - tx                            1841930709
```

Speedmeter na RX spitent ukazuje cca 244. TX plnych 400

## 16M 8 tabulek
```
[xpatov00@dpdk-test12 bf_tester]$ sudo ./bf_tester -a 0000:21:00.0,dv_flow_en=2 -n 6 -l 96-112 --socket-mem=4096 -- --src-ip 192.85.1.2,16000 --src-port 1024,1001 -M per-flow --sync --tables 8
EAL: Detected CPU lcores: 128
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: VFIO support initialized
EAL: TOPOLOGY - core count: l1 17, l2 17, l3 17, l4 0, io 17

=== bf_tester ===
Mode:            per-flow (one exact 5-tuple rule per flow -> RSS to hairpin queues)
Queues:          1 standard (0-0), 8 hairpin (1-8)
Rule API:        sync (rte_flow_create)
Statistics:      on
Miss check:      off
Flows:           16016000
  first #0: 192.85.1.2:1024 -> 192.0.0.1:1024 tcp
  last  #16015999: 192.85.63.129:2024 -> 192.0.0.1:1024 tcp
Lcores:          17 (1 main + 16 inserting workers)


=== Flow setup ===
Port:            0 (mlx5_pci), max rxq 1024, max txq 1024
Steering:        HW (dv_flow_en=2), counters 0 used of max 15728640
Rules:           group 0  src IP & 7 == t -> jump group 1+t (8 rules), rest -> group 1
                 groups 1-8  16016000 x tcp 5-tuple -> RSS to hairpin queues 1-8 (inserted below)
                 groups 1-8  miss -> drop
                 rules per group: 2002000 2002000 2002000 2002000 2002000 2002000 2002000 2002000
Sync API:
  Insertion:     rte_flow_create(), 16 worker lcore(s) in parallel

Inserting 16016000 rules on 16 lcore(s), sync rte_flow_create()...

Rules inserted: 16016000 ok, 0 failed in 158.134 s = 0.101 Mrules/s
```

```
=== Traffic summary (60.3 s) ===
  RX average                      65.47 Mpps     389.76 Gbps
  TX average                      62.29 Mpps     371.62 Gbps
  rx_phy_packets                     3948637935
  rx_phy_bytes                    2938410807316
  tx_phy_packets                     3757083066
  tx_phy_bytes                    2801667994580
  rx_phy_discard_packets              191258326
  tx_phy_discard_packets                      0
  rx_out_of_buffer                            0
  hairpin_out_of_buffer                       0
  rx - tx                             191554869
```

## 8M 1 tabulka
```
[xpatov00@dpdk-test12 bf_tester]$ sudo ./bf_tester -a 0000:21:00.0,dv_flow_en=2 -n 6 -l 96-112 --socket-mem=4096 -- --src-ip 192.85.1.2,8000 --src-port 1024,1001 -M per-flow --sync
EAL: Detected CPU lcores: 128
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: VFIO support initialized
EAL: TOPOLOGY - core count: l1 17, l2 17, l3 17, l4 0, io 17

=== bf_tester ===
Mode:            per-flow (one exact 5-tuple rule per flow -> RSS to hairpin queues)
Queues:          1 standard (0-0), 8 hairpin (1-8)
Rule API:        sync (rte_flow_create)
Statistics:      on
Miss check:      off
Flows:           8008000
  first #0: 192.85.1.2:1024 -> 192.0.0.1:1024 tcp
  last  #8007999: 192.85.32.65:2024 -> 192.0.0.1:1024 tcp
Lcores:          17 (1 main + 16 inserting workers)


=== Flow setup ===
Port:            0 (mlx5_pci), max rxq 1024, max txq 1024
Steering:        HW (dv_flow_en=2), counters 0 used of max 15728640
Rules:           group 0  all -> jump group 1
                 group 1  8008000 x tcp 5-tuple -> RSS to hairpin queues 1-8 (inserted below)
                 group 1  miss -> drop
Sync API:
  Insertion:     rte_flow_create(), 16 worker lcore(s) in parallel

Inserting 8008000 rules on 16 lcore(s), sync rte_flow_create()...
```

```
=== Traffic summary (60.3 s) ===
  RX average                      65.47 Mpps     389.76 Gbps
  TX average                      55.73 Mpps     332.69 Gbps
  rx_phy_packets                     3951006118
  rx_phy_bytes                    2940166624456
  tx_phy_packets                     3363193128
  tx_phy_bytes                    2509633466430
  rx_phy_discard_packets              541442464
  tx_phy_discard_packets                      0
  rx_out_of_buffer                            0
  hairpin_out_of_buffer                       0
  rx - tx                             587812990
```

## 8M 8 tabulka
```
[xpatov00@dpdk-test12 bf_tester]$ sudo ./bf_tester -a 0000:21:00.0,dv_flow_en=2 -n 6 -l 96-112 --socket-mem=4096 -- --src-ip 192.85.1.2,8000 --src-port 1024,1001 -M per-flow --sync --tables 8
EAL: Detected CPU lcores: 128
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: VFIO support initialized
EAL: TOPOLOGY - core count: l1 17, l2 17, l3 17, l4 0, io 17

=== bf_tester ===
Mode:            per-flow (one exact 5-tuple rule per flow -> RSS to hairpin queues)
Queues:          1 standard (0-0), 8 hairpin (1-8)
Rule API:        sync (rte_flow_create)
Statistics:      on
Miss check:      off
Flows:           8008000
  first #0: 192.85.1.2:1024 -> 192.0.0.1:1024 tcp
  last  #8007999: 192.85.32.65:2024 -> 192.0.0.1:1024 tcp
Lcores:          17 (1 main + 16 inserting workers)


=== Flow setup ===
Port:            0 (mlx5_pci), max rxq 1024, max txq 1024
Steering:        HW (dv_flow_en=2), counters 0 used of max 15728640
Rules:           group 0  src IP & 7 == t -> jump group 1+t (8 rules), rest -> group 1
                 groups 1-8  8008000 x tcp 5-tuple -> RSS to hairpin queues 1-8 (inserted below)
                 groups 1-8  miss -> drop
                 rules per group: 1001000 1001000 1001000 1001000 1001000 1001000 1001000 1001000
Sync API:
  Insertion:     rte_flow_create(), 16 worker lcore(s) in parallel

Inserting 8008000 rules on 16 lcore(s), sync rte_flow_create()...
Rules inserted: 8008000 ok, 0 failed in 78.917 s = 0.101 Mrules/s
```

```
=== Traffic summary (63.3 s) ===
  RX average                      65.47 Mpps     389.75 Gbps
  TX average                      64.22 Mpps     382.55 Gbps
  rx_phy_packets                     4144205243
  rx_phy_bytes                    3083944386410
  tx_phy_packets                     4064944350
  tx_phy_bytes                    3026975563600
  rx_phy_discard_packets               76942577
  tx_phy_discard_packets                      0
  rx_out_of_buffer                            0
  hairpin_out_of_buffer                       0
  rx - tx                              79260893
```

## 4M 1 tabulka
```
[xpatov00@dpdk-test12 bf_tester]$ sudo ./bf_tester -a 0000:21:00.0,dv_flow_en=2 -n 6 -l 96-112 --socket-mem=4096 -- --src-ip 192.85.1.2,4000 --src-port 1024,1001 -M per-flow --sync
EAL: Detected CPU lcores: 128
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: VFIO support initialized
EAL: TOPOLOGY - core count: l1 17, l2 17, l3 17, l4 0, io 17

=== bf_tester ===
Mode:            per-flow (one exact 5-tuple rule per flow -> RSS to hairpin queues)
Queues:          1 standard (0-0), 8 hairpin (1-8)
Rule API:        sync (rte_flow_create)
Statistics:      on
Miss check:      off
Flows:           4004000
  first #0: 192.85.1.2:1024 -> 192.0.0.1:1024 tcp
  last  #4003999: 192.85.16.161:2024 -> 192.0.0.1:1024 tcp
Lcores:          17 (1 main + 16 inserting workers)


=== Flow setup ===
Port:            0 (mlx5_pci), max rxq 1024, max txq 1024
Steering:        HW (dv_flow_en=2), counters 0 used of max 15728640
Rules:           group 0  all -> jump group 1
                 group 1  4004000 x tcp 5-tuple -> RSS to hairpin queues 1-8 (inserted below)
                 group 1  miss -> drop
Sync API:
  Insertion:     rte_flow_create(), 16 worker lcore(s) in parallel
Rules inserted: 4004000 ok, 0 failed in 24.285 s = 0.165 Mrules/s
```

```
=== Traffic summary (60.7 s) ===
  RX average                      65.47 Mpps     389.76 Gbps
  TX average                      65.39 Mpps     389.21 Gbps
  rx_phy_packets                     3976453766
  rx_phy_bytes                    2959109810344
  tx_phy_packets                     3971716325
  tx_phy_bytes                    2954998245692
  rx_phy_discard_packets                      0
  tx_phy_discard_packets                      0
  rx_out_of_buffer                            0
  hairpin_out_of_buffer                       0
  rx - tx                               4737441
```

## 2M 1 tabulka
```
[xpatov00@dpdk-test12 bf_tester]$ sudo ./bf_tester -a 0000:21:00.0,dv_flow_en=2 -n 6 -l 96-112 --socket-mem=4096 -- --src-ip 192.85.1.2,2000 --src-port 1024,1001 -M per-flow --sync
EAL: Detected CPU lcores: 128
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: VFIO support initialized
EAL: TOPOLOGY - core count: l1 17, l2 17, l3 17, l4 0, io 17

=== bf_tester ===
Mode:            per-flow (one exact 5-tuple rule per flow -> RSS to hairpin queues)
Queues:          1 standard (0-0), 8 hairpin (1-8)
Rule API:        sync (rte_flow_create)
Statistics:      on
Miss check:      off
Flows:           2002000
  first #0: 192.85.1.2:1024 -> 192.0.0.1:1024 tcp
  last  #2001999: 192.85.8.209:2024 -> 192.0.0.1:1024 tcp
Lcores:          17 (1 main + 16 inserting workers)


=== Flow setup ===
Port:            0 (mlx5_pci), max rxq 1024, max txq 1024
Steering:        HW (dv_flow_en=2), counters 0 used of max 15728640
Rules:           group 0  all -> jump group 1
                 group 1  2002000 x tcp 5-tuple -> RSS to hairpin queues 1-8 (inserted below)
                 group 1  miss -> drop
Sync API:
  Insertion:     rte_flow_create(), 16 worker lcore(s) in parallel

Inserting 2002000 rules on 16 lcore(s), sync rte_flow_create()...
Rules inserted: 2002000 ok, 0 failed in 21.315 s = 0.094 Mrules/s
```

```
=== Traffic summary (60.4 s) ===
  RX average                      65.47 Mpps     389.76 Gbps
  TX average                      65.46 Mpps     389.71 Gbps
  rx_phy_packets                     3952889815
  rx_phy_bytes                    2941572436236
  tx_phy_packets                     3952518878
  tx_phy_bytes                    2941244338498
  rx_phy_discard_packets                      0
  tx_phy_discard_packets                      0
  rx_out_of_buffer                            0
  hairpin_out_of_buffer                       0
  rx - tx                                370937
```

Už na 4M vidíme mírný pokles, který začne být více patrný pro 8M a nejvíc pak pro 16M. Trochu pomůže rozdělit pravidla přes tabulky, ale i tam nakonec není kam zvětšovat. Více tabulek už nepřinese zlepšení.

Druhy den jsem pro mereni 2M s jednou tabulkou dostala jina cislo... Proc thou
```
[xpatov00@dpdk-test12 xpatov00]$ sudo ./bf_tester_build/build/flow/bf_tester/bf_tester -a 0000:21:00.0,dv_flow_en=2 -n 6 -l 96-112 --socket-mem=4096 -- --src-ip 192.85.1.2,2000 --src-port 1024,1001 -M per-flow --sync
EAL: Detected CPU lcores: 128
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: VFIO support initialized
EAL: TOPOLOGY - core count: l1 17, l2 17, l3 17, l4 0, io 17

=== bf_tester ===
Mode:            per-flow (one exact 5-tuple rule per flow -> RSS to hairpin queues)
Queues:          1 standard (0-0), 8 hairpin (1-8)
Rule API:        sync (rte_flow_create)
Statistics:      on
Miss check:      off
Flows:           2002000
  first #0: 192.85.1.2:1024 -> 192.0.0.1:1024 tcp
  last  #2001999: 192.85.8.209:2024 -> 192.0.0.1:1024 tcp
Lcores:          17 (1 main + 16 inserting workers)


=== Flow setup ===
Port:            0 (mlx5_pci), max rxq 1024, max txq 1024
Steering:        HW (dv_flow_en=2), counters 0 used of max 15728640
Rules:           group 0  all -> jump group 1
                 group 1  2002000 x tcp 5-tuple -> RSS to hairpin queues 1-8 (inserted below)
                 group 1  miss -> drop
Sync API:
  Insertion:     rte_flow_create(), 16 worker lcore(s) in parallel

Inserting 2002000 rules on 16 lcore(s), sync rte_flow_create()...
Rules inserted: 2002000 ok, 0 failed in 18.104 s = 0.111 Mrules/s
```

```
=== Traffic summary (23.6 s) ===
  RX average                      65.53 Mpps     390.11 Gbps
  TX average                      35.49 Mpps     211.58 Gbps
  rx_phy_packets                     1548715342
  rx_phy_bytes                    1152489456938
  tx_phy_packets                      838701345
  tx_phy_bytes                     625080973684
  rx_phy_discard_packets              710220979
  tx_phy_discard_packets                      0
  rx_out_of_buffer                            0
  hairpin_out_of_buffer                       0
  rx - tx                             710013997
```

Nevím jestli to tak bylo i včera, ale vyhodila jsem druhý mlx port z VLAN a dala jsem ho down a už je rychlost zpátky (down port sám o sobě nepomohlo, možná nemá vliv)

# Modify
Po pokusech jsem rozdělila na dvě části jednu čistě edit a druhou plus vkládání vlan. Aby bylo vidět co z toho stojí více. Ukázalo se že jen edit MAC je v pohodě.

Používám jednu tabulku.

## 2M edit_mac
```
[xpatov00@dpdk-test12 xpatov00]$ sudo ./bf_tester_build/build/flow/bf_tester/bf_tester -a 0000:21:00.0,dv_flow_en=2 -n 6 -l 96-112 --socket-mem=4096 -- --src-ip 192.85.1.2,2000 --src-port 1024,1001 -M edit_mac --sync
EAL: Detected CPU lcores: 128
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: VFIO support initialized
EAL: TOPOLOGY - core count: l1 17, l2 17, l3 17, l4 0, io 17

=== bf_tester ===
Mode:            edit_mac (per-flow + set the source MAC to the port MAC)
Queues:          1 standard (0-0), 8 hairpin (1-8)
Rule API:        sync (rte_flow_create)
Statistics:      on
Miss check:      off
Flows:           2002000
  first #0: 192.85.1.2:1024 -> 192.0.0.1:1024 tcp
  last  #2001999: 192.85.8.209:2024 -> 192.0.0.1:1024 tcp
Lcores:          17 (1 main + 16 inserting workers)


=== Flow setup ===
Port:            0 (mlx5_pci), max rxq 1024, max txq 1024
Steering:        HW (dv_flow_en=2), counters 0 used of max 15728640
Rules:           group 0  all -> jump group 1
                 group 1  2002000 x tcp 5-tuple -> src MAC := E8:9E:49:7F:27:CE, RSS to hairpin queues 1-8 (inserted below)
                 group 1  miss -> drop
Sync API:
  Insertion:     rte_flow_create(), 16 worker lcore(s) in parallel

Inserting 2002000 rules on 16 lcore(s), sync rte_flow_create()...
Rules inserted: 2002000 ok, 0 failed in 22.785 s = 0.088 Mrules/s
```

```
=== Traffic summary (60.6 s) ===
  RX average                      65.47 Mpps     389.76 Gbps
  TX average                      65.47 Mpps     389.73 Gbps
  rx_phy_packets                     3970013610
  rx_phy_bytes                    2954309483584
  tx_phy_packets                     3969727898
  tx_phy_bytes                    2954061010750
  rx_phy_discard_packets                      0
  tx_phy_discard_packets                      0
  rx_out_of_buffer                            0
  hairpin_out_of_buffer                       0
  rx - tx                                285712
```

## 16M edit_mac
```
[xpatov00@dpdk-test12 xpatov00]$ sudo ./bf_tester_build/build/flow/bf_tester/bf_tester -a 0000:21:00.0,dv_flow_en=2 -n 6 -l 96-112 --socket-mem=4096 -- --src-ip 192.85.1.2,16000 --src-port 1024,1001 -M edit_mac --sync
EAL: Detected CPU lcores: 128
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: VFIO support initialized
EAL: TOPOLOGY - core count: l1 17, l2 17, l3 17, l4 0, io 17

=== bf_tester ===
Mode:            edit_mac (per-flow + set the source MAC to the port MAC)
Queues:          1 standard (0-0), 8 hairpin (1-8)
Rule API:        sync (rte_flow_create)
Statistics:      on
Miss check:      off
Flows:           16016000
  first #0: 192.85.1.2:1024 -> 192.0.0.1:1024 tcp
  last  #16015999: 192.85.63.129:2024 -> 192.0.0.1:1024 tcp
Lcores:          17 (1 main + 16 inserting workers)


=== Flow setup ===
Port:            0 (mlx5_pci), max rxq 1024, max txq 1024
Steering:        HW (dv_flow_en=2), counters 0 used of max 15728640
Rules:           group 0  all -> jump group 1
                 group 1  16016000 x tcp 5-tuple -> src MAC := E8:9E:49:7F:27:CE, RSS to hairpin queues 1-8 (inserted below)
                 group 1  miss -> drop
Sync API:
  Insertion:     rte_flow_create(), 16 worker lcore(s) in parallel

Inserting 16016000 rules on 16 lcore(s), sync rte_flow_create()...
Rules inserted: 16016000 ok, 0 failed in 56.228 s = 0.285 Mrules/s
```

```
=== Traffic summary (60.3 s) ===
  RX average                      65.47 Mpps     389.76 Gbps
  TX average                      39.41 Mpps     236.76 Gbps
  rx_phy_packets                     3951068007
  rx_phy_bytes                    2940213270116
  tx_phy_packets                     2378218204
  tx_phy_bytes                    1786002321966
  rx_phy_discard_packets             1551677311
  tx_phy_discard_packets                      0
  rx_out_of_buffer                            0
  hairpin_out_of_buffer                       0
  rx - tx                            1572849803
```

Toto měření odpovídá měření bez editu takže zhoršení je jen kvůli přeplnení tabulky viz předchozí měření.

## Add Vlan
Používám 2M v jedné tabulce

```
[xpatov00@dpdk-test12 xpatov00]$ sudo ./bf_tester_build/build/flow/bf_tester/bf_tester -a 0000:21:00.0,dv_flow_en=2 -n 6 -l 96-112 --socket-mem=4096 -- --src-ip 192.85.1.2,2000 --src-port 1024,1001 -M insert --sync
EAL: Detected CPU lcores: 128
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: VFIO support initialized
EAL: TOPOLOGY - core count: l1 17, l2 17, l3 17, l4 0, io 17

=== bf_tester ===
Mode:            insert (edit_mac + push VLAN 3)
Queues:          1 standard (0-0), 8 hairpin (1-8)
Rule API:        sync (rte_flow_create)
Statistics:      on
Miss check:      off
Flows:           2002000
  first #0: 192.85.1.2:1024 -> 192.0.0.1:1024 tcp
  last  #2001999: 192.85.8.209:2024 -> 192.0.0.1:1024 tcp
Lcores:          17 (1 main + 16 inserting workers)


=== Flow setup ===
Port:            0 (mlx5_pci), max rxq 1024, max txq 1024
Steering:        HW (dv_flow_en=2), counters 0 used of max 15728640
Rules:           group 0  all -> jump group 1
                 group 1  2002000 x tcp 5-tuple -> push VLAN 3, src MAC := E8:9E:49:7F:27:CE, RSS to hairpin queues 1-8 (inserted below)
                 group 1  miss -> drop
Sync API:
  Insertion:     rte_flow_create(), 16 worker lcore(s) in parallel

Inserting 2002000 rules on 16 lcore(s), sync rte_flow_create()...
Rules inserted: 2002000 ok, 0 failed in 25.315 s = 0.079 Mrules/s
```

```
=== Traffic summary (65.2 s) ===
  RX average                      65.47 Mpps     389.75 Gbps
  TX average                      43.97 Mpps     266.14 Gbps
  rx_phy_packets                     4267420032
  rx_phy_bytes                    3175634581312
  tx_phy_packets                     2866091262
  tx_phy_bytes                    2168487118042
  rx_phy_discard_packets             1385485312
  tx_phy_discard_packets                      0
  rx_out_of_buffer                            0
  hairpin_out_of_buffer                       0
  rx - tx                            1401328770
```

Co by se dalo zkusit:

- Přesunout push VLAN na odchozí stranu. Při tx_explicit hairpinu přidat egress pravidlo, které VLAN vloží až při odesílání. Ingress by pak dělal jen vyhledání a RSS a reparse by se přesunul do TX části karty, která je teď volnější. Jistota, že to pomůže: nízká až střední. Že HW steering egress pravidla s push VLAN podporuje, je podle tabulky pořadí akcí pro NIC_TX ověřené. Jak přesně v HWS vybrat jen hairpin provoz, jsem neověřovala.
- Diagnostika: změřit push VLAN s jediným pravidlem (jako single). Pokud propad zůstane, je to pevný náklad na paket (reparse) a s počtem pravidel nesouvisí. Pokud ne, souvisí s velikostí tabulky. Jistota, že to od sebe odliší: vysoká. Vyžadovalo by to malou úpravu kódu.
- Spíš nepomůže: vložit VLAN jinou cestou (RAW_ENCAP). Encap čte data z dodatečné paměti a reparse má taky. Jistota střední.

Egress vypadá, že opravdu pomohl...
```
[xpatov00@dpdk-test12 xpatov00]$ sudo ./bf_tester_build/build/flow/bf_tester/bf_tester -a 0000:21:00.0,dv_flow_en=2 -n 6 -l 96-112 --socket-mem=4096 -- --src-ip 192.85.1.2,2000 --src-port 1024,1001 -M insert --sync
EAL: Detected CPU lcores: 128
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: VFIO support initialized
EAL: TOPOLOGY - core count: l1 17, l2 17, l3 17, l4 0, io 17

=== bf_tester ===
Mode:            insert (edit_mac + push VLAN 3)
Queues:          1 standard (0-0), 8 hairpin (1-8)
Rule API:        sync (rte_flow_create)
Statistics:      on
Miss check:      off
Flows:           2002000
  first #0: 192.85.1.2:1024 -> 192.0.0.1:1024 tcp
  last  #2001999: 192.85.8.209:2024 -> 192.0.0.1:1024 tcp
Lcores:          17 (1 main + 16 inserting workers)


=== Flow setup ===
Port:            0 (mlx5_pci), max rxq 1024, max txq 1024
Steering:        HW (dv_flow_en=2), counters 0 used of max 15728640
Rules:           group 0  all -> jump group 1
                 group 1  2002000 x tcp 5-tuple -> push VLAN 3 (egress), src MAC := E8:9E:49:7F:27:CE, RSS to hairpin queues 1-8 (inserted below)
                 group 1  miss -> drop
Sync API:
  Insertion:     rte_flow_create(), 16 worker lcore(s) in parallel

Inserting 2002000 rules on 16 lcore(s), sync rte_flow_create()...
Rules inserted: 2002000 ok, 0 failed in 22.783 s = 0.088 Mrules/s
```

```
=== Traffic summary (60.4 s) ===
  RX average                      65.47 Mpps     389.76 Gbps
  TX average                      65.18 Mpps     389.79 Gbps
  rx_phy_packets                     3953163687
  rx_phy_bytes                    2941775174526
  tx_phy_packets                     3935817728
  tx_phy_bytes                    2942022249412
  rx_phy_discard_packets                      0
  tx_phy_discard_packets                      0
  rx_out_of_buffer                            0
  hairpin_out_of_buffer                       0
  rx - tx                              17345959
```

# Counters
Maximální počet countrů je cca 15,7M

## 2M ingress
```
[xpatov00@dpdk-test12 ~]$ sudo ./bf_tester_build/build/flow/bf_tester/bf_tester -a 0000:21:00.0,dv_flow_en=2 -n 6 -l 96-112 --socket-mem=4096 -- --src-ip 192.85.1.2,2000 --src-port 1024,1001 -M count --sync
EAL: Detected CPU lcores: 128
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: VFIO support initialized
EAL: TOPOLOGY - core count: l1 17, l2 17, l3 17, l4 0, io 17

=== bf_tester ===
Mode:            count (insert + counter per rule)
Queues:          1 standard (0-0), 8 hairpin (1-8)
Rule API:        sync (rte_flow_create)
Statistics:      on
Miss check:      off
Flows:           2002000
  first #0: 192.85.1.2:1024 -> 192.0.0.1:1024 tcp
  last  #2001999: 192.85.8.209:2024 -> 192.0.0.1:1024 tcp
Lcores:          17 (1 main + 16 inserting workers)


=== Flow setup ===
Port:            0 (mlx5_pci), max rxq 1024, max txq 1024
Steering:        HW (dv_flow_en=2), counters 2002000 used of max 15728640
Rules:           group 0  all -> jump group 1
                 group 1  2002000 x tcp 5-tuple -> count, push VLAN 3, src MAC := E8:9E:49:7F:27:CE, RSS to hairpin queues 1-8 (inserted below)
                 group 1  miss -> drop
Sync API:
  Insertion:     rte_flow_create(), 16 worker lcore(s) serialized (mlx5 counter cache is not thread-safe)

Inserting 2002000 rules on 16 lcore(s), sync rte_flow_create()...
Rules inserted: 2002000 ok, 0 failed in 31.988 s = 0.063 Mrules/s

=== Traffic summary (36.7 s) ===
  RX average                      65.50 Mpps     389.91 Gbps
  TX average                      45.50 Mpps     275.25 Gbps
  rx_phy_packets                     2402434714
  rx_phy_bytes                    1787786222052
  tx_phy_packets                     1669156849
  tx_phy_bytes                    1262070088736
  rx_phy_discard_packets              733801289
  tx_phy_discard_packets                      0
  rx_out_of_buffer                            0
  hairpin_out_of_buffer                       0
  rx - tx                             733277865

Querying 2002000 flow counters...
Flow counters since forwarding start (0.03 s): packets 1702608145, bytes 1273529869370, flows without hits 0, not inserted 0, query errors 0
```

Jde vidět že count má i s VLAN na ingressu o cca 10G lepší výsledek než jen VLAN bez countru. Jediný rozdíl je v vkládání, kdy pro VLAN ještě dovolujeme vkládat více pravidel zaráz díky zámkům. Pro count to ale už nejde, takže jsou vkládána po jednom. Claude nabízí dvě vysvětlení:

```
Pořadí vkládání samo dělá ~10 Gbps. Stejný mód count dal s 1 lcore 285,5 Gbps a se 16 lcore 275,3 Gbps. Jediný rozdíl je v tom, jak se pravidla vkládala. Je to ale jen jeden běh s 1 lcore. Pokud se to potvrdí, rozdíl dělá rozložení pravidel v HW tabulce. Pravidla vložená v jiném pořadí skončí v BWC matcheru jinak, protože se v jiných chvílích zvětšuje (rehash) a jinak padají kolize. To ovlivní, kolik práce stojí vyhledání každého paketu.

insert se vkládal 16 vlákny skutečně paralelně. count teď kvůli zámku vkládá postupně. Zlepšení tedy může jít za jiným pořadím vkládání, ne za samotnými countery.
Jistota: střední. Vychází z jednoho běhu s 1 lcore.

Nebo
Countery mohou změnit rozložení akcí v HW. S counterem má pravidlo jinou sadu akcí, a HWS je proto může rozdělit do STE jinak. Mohlo by to změnit cenu vkládání VLAN (reparse), kterou podezíráme ze ztrát na ingressu. Tohle je čistá spekulace bez opory ve zdrojácích, jistota nízká.
```

Při použití jen jednoho vlákna (-l96-97) jsem opravdu pro vkládání VLAN na ingress dostala o něco lepší výsledek
```
RX average                      65.44 Mpps     389.60 Gbps
TX average                      48.48 Mpps     292.69 Gbps
```

Pro count to dopadlo trochu lépe, ale hůř než bez count
```
RX average                      65.51 Mpps     389.99 Gbps
TX average                      47.25 Mpps     285.60 Gbps
```

## 2M egress

Nejprve jen vlan s egress na jednom vlákně
```
RX average                      65.65 Mpps     390.81 Gbps
TX average                      65.36 Mpps     390.84 Gbps
```
Tady je trochu podezřelé, že je TX víc než RX.

A pak counter
```
[xpatov00@dpdk-test12 ~]$ sudo ./bf_tester_build/build/flow/bf_tester/bf_tester -a 0000:21:00.0,dv_flow_en=2 -n 6 -l 96-97 --socket-mem=4096 -- --src-ip 192.85.1.2,2000 --src-port 1024,1001 -M count --sync --egress-vlan
EAL: Detected CPU lcores: 128
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: VFIO support initialized
EAL: TOPOLOGY - core count: l1 2, l2 2, l3 2, l4 0, io 2

=== bf_tester ===
Mode:            count (insert + counter per rule)
Queues:          1 standard (0-0), 8 hairpin (1-8)
Rule API:        sync (rte_flow_create)
Statistics:      on
Miss check:      off
Flows:           2002000
  first #0: 192.85.1.2:1024 -> 192.0.0.1:1024 tcp
  last  #2001999: 192.85.8.209:2024 -> 192.0.0.1:1024 tcp
Lcores:          2 (1 main + 1 inserting workers)


=== Flow setup ===
Port:            0 (mlx5_pci), max rxq 1024, max txq 1024
Steering:        HW (dv_flow_en=2), counters 2002000 used of max 15728640
Rules:           group 0  all -> jump group 1
                 group 1  2002000 x tcp 5-tuple -> count, src MAC := E8:9E:49:7F:27:CE, RSS to hairpin queues 1-8 (inserted below)
                 group 1  miss -> drop
                 egress group 0  all -> jump egress group 1
                 egress group 1  all -> push VLAN 3
Sync API:
  Insertion:     rte_flow_create(), 1 worker lcore(s) serialized (mlx5 counter cache is not thread-safe)

Inserting 2002000 rules on 1 lcore(s), sync rte_flow_create()...
Rules inserted: 2002000 ok, 0 failed in 26.788 s = 0.075 Mrules/s

=== Traffic summary (11.5 s) ===
  RX average                      65.65 Mpps     390.86 Gbps
  TX average                      63.13 Mpps     376.49 Gbps
  rx_phy_packets                      754908843
  rx_phy_bytes                     561769985516
  tx_phy_packets                      725863284
  tx_phy_bytes                     541115218802
  rx_phy_discard_packets                5486293
  tx_phy_discard_packets                      0
  rx_out_of_buffer                            0
  hairpin_out_of_buffer                       0
  rx - tx                              29045559

Querying 2002000 flow counters...
Flow counters since forwarding start (0.02 s): packets 780563491, bytes 577997228054, flows without hits 0, not inserted 0, query errors 0
```

Pro counter už to o něco kleslo, zkusíme co se stane pro count když dáme zpět lcory
```
[xpatov00@dpdk-test12 ~]$ sudo ./bf_tester_build/build/flow/bf_tester/bf_tester -a 0000:21:00.0,dv_flow_en=2 -n 6 -l 96-112 --socket-mem=4096 -- --src-ip 192.85.1.2,2000 --src-port 1024,1001 -M count --sync --egress-vlan
EAL: Detected CPU lcores: 128
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: VFIO support initialized
EAL: TOPOLOGY - core count: l1 17, l2 17, l3 17, l4 0, io 17

=== bf_tester ===
Mode:            count (insert + counter per rule)
Queues:          1 standard (0-0), 8 hairpin (1-8)
Rule API:        sync (rte_flow_create)
Statistics:      on
Miss check:      off
Flows:           2002000
  first #0: 192.85.1.2:1024 -> 192.0.0.1:1024 tcp
  last  #2001999: 192.85.8.209:2024 -> 192.0.0.1:1024 tcp
Lcores:          17 (1 main + 16 inserting workers)


=== Flow setup ===
Port:            0 (mlx5_pci), max rxq 1024, max txq 1024
Steering:        HW (dv_flow_en=2), counters 2002000 used of max 15728640
Rules:           group 0  all -> jump group 1
                 group 1  2002000 x tcp 5-tuple -> count, src MAC := E8:9E:49:7F:27:CE, RSS to hairpin queues 1-8 (inserted below)
                 group 1  miss -> drop
                 egress group 0  all -> jump egress group 1
                 egress group 1  all -> push VLAN 3
Sync API:
  Insertion:     rte_flow_create(), 16 worker lcore(s) serialized (mlx5 counter cache is not thread-safe)

Inserting 2002000 rules on 16 lcore(s), sync rte_flow_create()...
Rules inserted: 2002000 ok, 0 failed in 28.400 s = 0.070 Mrules/s

=== Traffic summary (34.1 s) ===
  RX average                      65.50 Mpps     389.94 Gbps
  TX average                      58.80 Mpps     351.53 Gbps
  rx_phy_packets                     2233027137
  rx_phy_bytes                    1661721196406
  tx_phy_packets                     2004720358
  tx_phy_bytes                    1498028188198
  rx_phy_discard_packets              144177742
  tx_phy_discard_packets                      0
  rx_out_of_buffer                            0
  hairpin_out_of_buffer                       0
  rx - tx                             228306779

Querying 2002000 flow counters...
Flow counters since forwarding start (0.03 s): packets 2148435469, bytes 1592550030118, flows without hits 0, not inserted 0, query errors 0
```

### Proc je to o neco horsi
Hlavní teorie: přístupy do paměti counterů (jistota střední, neověřeno)

Každý paket musí v HW přičíst k svému counteru pakety a bajty, tedy přečíst hodnotu, upravit ji a zapsat zpět.
Spirent posílá 2M flow dokola, takže každý paket míří na jiný counter než ten předchozí. 2M counterů se do cache v NIC nevejde a counter se pak čte z paměti hostitele (ICM) přes PCIe.
Je to stejný mechanismus jako u hypotézy, že se do cache nevejdou samotná pravidla při 16M flow. Sedí k tomu, že s counterem rostou ztráty právě na vstupu: rx_phy_discard 25,4 % → 27,9 %.

# Meters

---

## Poznámky

Base test vyzkoušen nejprve pro TestPMD:

```
sudo dpdk-testpmd -a 0000:21:00.0 -n 6 -l 96-112 --socket-mem=4096 -- -ia --rxq=16 --txq=16 --nb-cores=16 --burst=64 --mbcache=512 --hairpinq=8 --hairpin-mode=0x12 --hairpin-map=0:16:0:16:8
```

Pravidla
```
flow create 0 ingress group 0 pattern end actions jump group 1 / end
flow create 0 ingress group 1 pattern eth / ipv4 / tcp / end actions count / rss types ipv4-tcp end queues 16 17 18 19 20 21 22 23 end / end
```

Výkon byl 400 TX i RX.


Pak jsme s claudem napsali aplikaci, která by měla provádět to samé o co jsme se snažili s TestPMD. Hlavním důvodem bylo to, že nahrávání pravidel do TestPMD by trvalo dlouho.

Aplikace dovoluje přidávat pravidla synchronně i asynchronně.

Baseline test proběhl v pořádku a dá taktéž plný výkon jako TestPMD.

U Per-flow testu jsme narazili na problém kdy pro asynchronní pravidla je výkon o něco nižší tedy 370G

### Single

=== Traffic summary (20.2 s) ===
  RX average                      65.54 Mpps     390.15 Gbps
  TX average                      65.53 Mpps     390.14 Gbps
  rx_phy_packets                     1321121612
  rx_phy_bytes                     983116908040
  tx_phy_packets                     1321102329
  tx_phy_bytes                     983108992000
  rx_phy_discard_packets                      0
  tx_phy_discard_packets                      0
  rx_out_of_buffer                            0
  hairpin_out_of_buffer                       0
  rx - tx                                 19283


### Per-flow async

=== Traffic summary (20.3 s) ===
  RX average                      65.54 Mpps     390.19 Gbps
  TX average                      56.05 Mpps     334.42 Gbps
  rx_phy_packets                     1333659558
  rx_phy_bytes                     992450925880
  tx_phy_packets                     1140459957
  tx_phy_bytes                     850587906602
  rx_phy_discard_packets              184413311
  tx_phy_discard_packets                      0
  rx_out_of_buffer                            0
  hairpin_out_of_buffer                       0
  rx - tx                             193199601

### Per-flow sync

=== Traffic summary (20.5 s) ===
  RX average                      65.54 Mpps     390.18 Gbps
  TX average                      65.54 Mpps     390.15 Gbps
  rx_phy_packets                     1340579883
  rx_phy_bytes                     997594772502
  tx_phy_packets                     1340459866
  tx_phy_bytes                     997501999106
  rx_phy_discard_packets                      0
  tx_phy_discard_packets                      0
  rx_out_of_buffer                            0
  hairpin_out_of_buffer                       0
  rx - tx                                120017

  Zajímavé zjištění je, že pokud se tabulka zaplní z 95% tak pak má karta horší výkon. Tohle myslím už potvrdil do jisté míry měření od Lukáše Š.

  Karta ale umí dělat hacky kdy zvětší svoji kapacitu pokud dosáhne jisté míry zaplnění. Pro async pravidla to funguje trochu jinak jež pro sync, proto to vypadá že pro sync je to lepší. Pro asynch se to dá zlepšit/ohackovat tím, že v podstatě vytvoříme více pravidel, díky čemuž se kapacita v podstatě zvětší. Tj. dimenzovat template tabulku s rezervou, třeba jako BWC (synchronní tabulka) na maximálně 70 % zaplnění

  Do kódu jsme tak přidali změnu, kdy se async budou chovat stejně jako sync. Pro 2M to pomohlo, ale pak pro 4M pozorujeme stejné chybné chování. Nechám to Clauda ještě otestovat a pak se na asynchronní vykašlu.

  Mělo by jít vložit něco mezi 16-32M pravidel, ale pro 32M bychom museli zvolit jiný přístup, protože zhruba u 10M začnou kolidovat hashe.


  Pro 16M už nestíhá ani sync. Zkusíme optimalizaci s více tabulkama.

  Více tabulek zdá se pomůže. S 8 je cca 5% paketů zahozených na vstupu.

  Pro 16 tabulek se výkon opět zhoršil takže optimální je 8 tabulek. S tím že Calude říká že s těmi 5% paketu se nedá už nic moc dělat při použitém rozložení pravidel.

  Takže pro Per Flow 16M uzavíráme s tím že má vlic kolik tabulek se použije. Nejlepší je 8 s tím že zde jsou další doporučení co s tím, ale nebudu je zkoušet

```
  Dokumentace NVIDIA: ladění v mlx5 PMD guide (mprq, rxq_cqe_comp_en atd.) se týká SW front, ne steeringu. Nastavení velikosti steering cache ani místa ICM paměti nemám ověřené. Pokud existují (mlxconfig, DOCA), neznám je a nechci si je vymýšlet. Konkrétní odpověď by mohla dát podpora NVIDIA nebo dokument „performance tuning“ pro BF-3 / ConnectX-7. Jistota, že v dostupných DPDK zdrojích nic takového není, střední: prošla jsem jen části mlx5 a mlx5dr, které jsme řešili.

  Pravidla jsou možná mimo cache.  Spirent střídá všech 16M flow dokola, takže pravidlo pro skoro každý paket chybí v cache karty a musí se číst z paměti. Pomohlo by změnit test tak že se nastaví Spirent tak, aby s 16M pravidly na kartě posílal jen část flow, třeba 2M, nebo flow s opakováním. Když výkon stoupne na 390 Gbps, je limit v cache a paměti. Pak je to vlastnost testu, kde se všech 16M flow střídá dokola, a skutečný provoz s lokalitou by na tom byl lépe. Jistota, že to od sebe příčiny oddělí, vysoká.
  ```