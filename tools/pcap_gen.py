# pcap_gen.py: Generate PCAP with packets according to input parameters.
# Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
# SPDX-License-Identifier: BSD-3-Clause
# Author: Jan Privara <privara@dyna-nic.com>, January 2026
#
# Unauthorized copying of this file, via any medium is strictly prohibited.
# Proprietary and confidential, additional license terms may apply.

import argparse
from scapy import all as scapy


def main():
    # Parse options
    parser = argparse.ArgumentParser()
    parser.add_argument("--src-ip", type=str, help="Source IP", default="1.2.3.4")
    parser.add_argument("--dst-ip", type=str, help="Destination IP", default="5.6.7.8")
    parser.add_argument("--src-port", type=int, help="Source port", default=100)
    parser.add_argument("--dst-port", type=int, help="Destination port", default=200)
    parser.add_argument("--prot", type=str, help="L4 Protocol [tcp/udp]. Default: tcp", default="tcp")
    parser.add_argument("--pkt-len", type=int, help="Length of the packet. Default: 128", default=128)
    parser.add_argument("--pcap", type=str, help="Name of the pcap file where to store created packets. Default: out.pcap.", default="out.pcap")

    args = parser.parse_args()

    # Define length of headers
    ether_hdr_len = 14
    ip_hdr_len = 20
    l4_hdr_len = 8 if args.prot == "udp" else 20

    payload_len = args.pkt_len - ether_hdr_len - ip_hdr_len - l4_hdr_len

    # Compute data length so we have packet exactly the size we want
    data = scapy.RandString(size=payload_len)

    # Create packet(s)
    if args.prot == "tcp":
        packet = scapy.Ether(dst='b8:af:67:d7:33:33', src='24:6e:96:85:c6:78')/scapy.IP(dst=args.dst_ip, src=args.src_ip)/scapy.TCP(sport=args.src_port, dport=args.dst_port)/scapy.Raw(load=data)
    elif args.prot == "udp":
        packet = scapy.Ether(dst='b8:af:67:d7:33:33', src='24:6e:96:85:c6:78')/scapy.IP(dst=args.dst_ip, src=args.src_ip)/scapy.UDP(sport=args.src_port, dport=args.dst_port)/scapy.Raw(load=data)
    else:
        print("Error: unknown protocol")
        exit

    # Save PCAP
    scapy.wrpcap(args.pcap, packet)


if __name__ == "__main__":
    main()
