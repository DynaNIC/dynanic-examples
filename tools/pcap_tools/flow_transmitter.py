# flow_transmitter.py: Generate and transmit a flow of packets over NFB, optionally saving them to a PCAP file.
# Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
# SPDX-License-Identifier: BSD-3-Clause
# Author: Pavlina Patova <patova@dyna-nic.com>, August 2026
#
# Unauthorized copying of this file, via any medium is strictly prohibited.
# Proprietary and confidential, additional license terms may apply.

import argparse
import ipaddress
import nfb
from scapy import all as scapy

from src.pcap_common import add_packet_args, build_packet


def main():
    parser = argparse.ArgumentParser()
    add_packet_args(parser)
    parser.add_argument("--pcap", type=str, help="Name of the pcap file where to store the transmitted packets. If not set, packets are only transmitted, not saved.", default=None)
    parser.add_argument("--nfb", type=str, help="Path to NFB device. Default: /dev/nfb0", default="/dev/nfb0")
    parser.add_argument("-i", "--dma-channel", type=int, help="TX queue ID. Default: 0", default=0)
    parser.add_argument("-l", "--pkt-cnt", type=int, help="How many packets to send. -1 means infinite. Default: 1", default=1)
    parser.add_argument("--no-increment", action="store_true", help="Keep --src-ip/--dst-ip fixed instead of incrementing them by one for each packet sent")

    args = parser.parse_args()

    src_ip_base = ipaddress.IPv4Address(args.src_ip)
    dst_ip_base = ipaddress.IPv4Address(args.dst_ip)

    dev = nfb.open(args.nfb)
    tx_queue = dev.ndp.tx[args.dma_channel]

    # Written incrementally (not via wrpcap at the end) so that if generation is
    # interrupted, the pcap still ends up containing exactly the packets that
    # were actually sent, no more and no less.
    pcap_writer = scapy.PcapWriter(args.pcap) if args.pcap else None

    sent = 0
    print("Sending packets... Press Ctrl+C to stop.")

    try:
        while args.pkt_cnt == -1 or sent < args.pkt_cnt:
            if args.no_increment:
                src_ip, dst_ip = str(src_ip_base), str(dst_ip_base)
            else:
                src_ip, dst_ip = str(src_ip_base + sent), str(dst_ip_base + sent)

            packet = build_packet(src_ip, dst_ip, args.src_port, args.dst_port, args.prot, args.pkt_len)

            tx_queue.send(bytes(packet))
            if pcap_writer:
                pcap_writer.write(packet)

            sent += 1

    except KeyboardInterrupt:
        print("\nInterrupted by the user (Ctrl+C).")
    finally:
        if pcap_writer:
            pcap_writer.close()
        print(f"Total number of packets sent: {sent}")


if __name__ == "__main__":
    main()
