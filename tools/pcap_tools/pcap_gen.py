# pcap_gen.py: Generate PCAP with packets according to input parameters.
# Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
# SPDX-License-Identifier: BSD-3-Clause
# Author: Jan Privara <privara@dyna-nic.com>, January 2026
#
# Unauthorized copying of this file, via any medium is strictly prohibited.
# Proprietary and confidential, additional license terms may apply.

import argparse
from scapy import all as scapy

from src.pcap_common import add_packet_args, build_packet


def main():
    # Parse options
    parser = argparse.ArgumentParser()
    add_packet_args(parser)
    parser.add_argument("--pcap", type=str, help="Name of the pcap file where to store created packets. Default: out.pcap.", default="out.pcap")

    args = parser.parse_args()

    packet = build_packet(args.src_ip, args.dst_ip, args.src_port, args.dst_port, args.prot, args.pkt_len)

    # Save PCAP
    scapy.wrpcap(args.pcap, packet)


if __name__ == "__main__":
    main()
