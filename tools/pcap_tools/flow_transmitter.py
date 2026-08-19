# pkt-blaster.py: Send packets using nfb.
# Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
# SPDX-License-Identifier: BSD-3-Clause
# Author: Pavlina Patova <patova@dyna-nic.com>, August 2026
#
# Unauthorized copying of this file, via any medium is strictly prohibited.
# Proprietary and confidential, additional license terms may apply.

import argparse
import nfb
import ipaddress
import sys
from scapy import all as scapy


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--src-ip", type=str, help="Starting source IP", default="1.2.3.4")
    parser.add_argument("--dst-ip", type=str, help="Starting destination IP", default="5.6.7.8")
    parser.add_argument("--src-port", type=int, help="Source port", default=100)
    parser.add_argument("--dst-port", type=int, help="Destination port", default=200)
    parser.add_argument("--prot", type=str, help="L4 Protocol [tcp/udp]. Default: tcp", default="tcp")
    parser.add_argument("--pkt-len", type=int, help="Length of the packet. Default: 128", default=128)

    parser.add_argument("-i", "--dma-channel", type=int, help="TX queue ID. Default: 2", default=2)
    parser.add_argument("--nfb", type=str, help="Path to NFB device. Default: /dev/nfb0", default="/dev/nfb0")
    parser.add_argument("-l", "--pkt-cnt", type=int, help="How many packets to send. -1 means infinite", default=1)

    args = parser.parse_args()

    ether_hdr_len = 14
    ip_hdr_len = 20
    l4_hdr_len = 8 if args.prot == "udp" else 20
    payload_len = args.pkt_len - ether_hdr_len - ip_hdr_len - l4_hdr_len

    data = scapy.RandString(size=payload_len)
    src_ip_base = ipaddress.IPv4Address(args.src_ip)
    dst_ip_base = ipaddress.IPv4Address(args.dst_ip)

    l4_proto_map = {"tcp": scapy.TCP, "udp": scapy.UDP}
    if args.prot not in l4_proto_map:
        print("Error: unknown protocol")
        sys.exit(1)

    dev = nfb.open(args.nfb)
    tx_queue = dev.ndp.tx[args.dma_channel]

    send_pkts = 0
    print("Sending packets... Press Ctrl+C to stop.")

    try:
        while True:
            src_ip = str(src_ip_base + send_pkts)
            dst_ip = str(dst_ip_base + send_pkts)

            l4_layer = l4_proto_map[args.prot](sport=args.src_port, dport=args.dst_port)
            packet = (
                scapy.Ether(dst='b8:af:67:d7:33:33', src='24:6e:96:85:c6:78') /
                scapy.IP(dst=dst_ip, src=src_ip) /
                l4_layer /
                scapy.Raw(load=data)
            )

            tx_queue.send(bytes(packet))
            send_pkts += 1

            if args.pkt_cnt != -1 and send_pkts >= args.pkt_cnt:
                break

    except KeyboardInterrupt:
        print("\nInterrupted by the user (Ctrl+C).")
    finally:
        print(f"Total number of packets sent: {send_pkts}")


if __name__ == "__main__":
    main()