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

# Modify
TODO musime ještě prvně doiplementovat

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