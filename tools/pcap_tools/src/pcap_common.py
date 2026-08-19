# pcap_common.py: Shared packet-generation helpers for pcap_gen.py and flow_transmitter.py.
# Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
# SPDX-License-Identifier: BSD-3-Clause
# Author: Pavlina Patova <patova@dyna-nic.com>, August 2026
#
# Unauthorized copying of this file, via any medium is strictly prohibited.
# Proprietary and confidential, additional license terms may apply.

import sys
from scapy import all as scapy

ETH_DST = 'b8:af:67:d7:33:33'
ETH_SRC = '24:6e:96:85:c6:78'

L4_PROTO_MAP = {"tcp": scapy.TCP, "udp": scapy.UDP}


def add_packet_args(parser):
    """Register the CLI options shared by every packet-generating tool."""
    parser.add_argument("--src-ip", type=str, help="Source IP", default="1.2.3.4")
    parser.add_argument("--dst-ip", type=str, help="Destination IP", default="5.6.7.8")
    parser.add_argument("--src-port", type=int, help="Source port", default=100)
    parser.add_argument("--dst-port", type=int, help="Destination port", default=200)
    parser.add_argument("--prot", type=str, help="L4 Protocol [tcp/udp]. Default: tcp", default="tcp")
    parser.add_argument("--pkt-len", type=int, help="Length of the packet. Default: 128", default=128)


def build_packet(src_ip, dst_ip, src_port, dst_port, prot, pkt_len):
    """Build a single Ethernet/IP/L4 packet from explicit packet-shaping values.

    Takes plain values rather than a parsed args namespace so callers that need to
    vary src_ip/dst_ip per packet (e.g. flow_transmitter.py) can do so without
    mutating argparse's Namespace.
    """
    if prot not in L4_PROTO_MAP:
        print("Error: unknown protocol")
        sys.exit(1)

    ether_hdr_len = 14
    ip_hdr_len = 20
    l4_hdr_len = 8 if prot == "udp" else 20
    payload_len = pkt_len - ether_hdr_len - ip_hdr_len - l4_hdr_len

    data = scapy.RandString(size=payload_len)
    l4_layer = L4_PROTO_MAP[prot](sport=src_port, dport=dst_port)

    return (
        scapy.Ether(dst=ETH_DST, src=ETH_SRC) /
        scapy.IP(dst=dst_ip, src=src_ip) /
        l4_layer /
        scapy.Raw(load=data)
    )
