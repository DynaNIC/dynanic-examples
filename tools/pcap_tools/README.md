# PCAP Tools

Three small Python utilities for generating and transmitting test traffic through an NFB device. They all build the same kind of packet (Ethernet/IP/TCP or Ethernet/IP/UDP with a random payload, padded to a requested total length) and share that logic through [src/pcap_common.py](src/pcap_common.py).

## Requirements

* Python 3
* `scapy`
* `nfb` package (only needed by `pcap_transmit.py` and `flow_transmitter.py`, which talk to real hardware)

## pcap_gen.py

Generates a single packet and writes it to a PCAP file. Does not touch any NFB device.

**Use case:** preparing a PCAP file ahead of time, e.g. to inspect it in Wireshark, to feed into another tool, or to reuse the same packet across multiple later transmit runs with `pcap_transmit.py`.

```bash
./pcap_gen.py --src-ip 1.2.3.4 --dst-ip 5.6.7.8 --prot udp --pkt-len 128 --pcap out.pcap
```

## pcap_transmit.py

Reads packets from an existing PCAP file and sends them out over an NFB TX queue. Does not generate or modify packets.

**Use case:** replaying a previously captured or generated PCAP file (e.g. one made with `pcap_gen.py`, or captured from a real network) onto the card, possibly multiple times or from different files, without regenerating packet contents each time.

```bash
./pcap_transmit.py --nfb /dev/nfb0 -i 0 --pcap in.pcap
```

## flow_transmitter.py

Generates a flow of packets and transmits them over NFB directly, one after another. Covers everything from a single one-shot packet to a continuous, never-ending flow:

* `-l/--pkt-cnt`: how many packets to send. Default `1`. `-1` means run until interrupted (Ctrl+C).
* By default, `--src-ip`/`--dst-ip` are incremented by one for every packet sent, so a run with `--pkt-cnt` > 1 produces a spread of distinct flows rather than the same packet repeated. Pass `--no-increment` to keep them fixed instead.
* `--pcap`: optional. If set, every packet that is actually transmitted is also written to this file, streamed as it's sent — so if the run is interrupted (Ctrl+C, or hits an error), the PCAP still ends up containing exactly the packets that were sent, no more and no less. There is intentionally no restriction on combining `--pcap` with a large or infinite (`-1`) `--pkt-cnt`: it's the user's call how much traffic (and how large a resulting file) they want, same as picking a large `--pkt-cnt` without saving.

**Use case:** covers both a quick one-shot "generate and send" for ad-hoc testing, and continuous multi-flow traffic generation (the same job `pcap_gen.py` + `pcap_transmit.py` would need two separate steps for), with an optional PCAP copy of exactly what went out on the wire.

```bash
# Send a single packet, no file left behind
./flow_transmitter.py --nfb /dev/nfb0 -i 0 --prot tcp --pkt-len 128

# Send a single packet and also keep a copy of it
./flow_transmitter.py --nfb /dev/nfb0 -i 0 --prot tcp --pkt-len 128 --pcap sent.pcap

# Send a burst of 1000 packets across incrementing src/dst IPs, saving all of them
./flow_transmitter.py --nfb /dev/nfb0 -i 0 --src-ip 10.0.0.1 --dst-ip 20.0.0.1 --pkt-cnt 1000 --pcap sent.pcap

# Send a continuous flow until stopped with Ctrl+C
./flow_transmitter.py --nfb /dev/nfb0 -i 0 --src-ip 10.0.0.1 --dst-ip 20.0.0.1 --pkt-cnt -1
```

## Shared options

`pcap_gen.py` and `flow_transmitter.py` accept the same packet-shaping options (defined once in `pcap_common.py`):

* `--src-ip`: Source IP. Default: `1.2.3.4`.
* `--dst-ip`: Destination IP. Default: `5.6.7.8`.
* `--src-port`: Source port. Default: `100`.
* `--dst-port`: Destination port. Default: `200`.
* `--prot`: L4 protocol, `tcp` or `udp`. Default: `tcp`.
* `--pkt-len`: Total packet length in bytes (including Ethernet/IP/L4 headers). Default: `128`.

`pcap_transmit.py` and `flow_transmitter.py` additionally accept:

* `--nfb`: Path to the NFB device. Default: `/dev/nfb0`.
* `-i/--dma-channel`: TX queue (DMA channel) ID. Default: `0`.

## src/pcap_common.py

Not a standalone tool. Holds the packet-building code (`build_packet`) and the shared argparse options (`add_packet_args`) used by `pcap_gen.py` and `flow_transmitter.py`, so the two don't duplicate the same header-construction logic.
