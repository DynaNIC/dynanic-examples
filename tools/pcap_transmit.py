# pcap_transmit.py: Transmit packets from a PCAP file
# Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
# SPDX-License-Identifier: BSD-3-Clause
# Author: Jan Privara <privara@dyna-nic.com>, January 2026
#
# Unauthorized copying of this file, via any medium is strictly prohibited.
# Proprietary and confidential, additional license terms may apply.

import nfb
import argparse
from scapy import all as scapy


def main():
    # Parse options
    parser = argparse.ArgumentParser()
    parser.add_argument("--nfb", type=str, help="Path to NFB device. Default: /dev/nfb0", default="/dev/nfb0")
    parser.add_argument("--queue", type=int, help="TX queue ID. Default: 0", default=0)
    parser.add_argument("--pcap", type=str, help="Name of the input pcap file. Default: in.pcap.", default="in.pcap")

    args = parser.parse_args()

    packets = scapy.rdpcap(args.pcap)

    dev = nfb.open(args.nfb)

    pkt_num = len(packets)
    # print("Transmitting", pkt_num, "packets...")

    for p in packets:
        dev.ndp.tx[args.queue].send(bytes(p))

    # print("Done")

if __name__ == "__main__":
    main()
