# measure.py: Mesure throughtput of RX and TX application.
# Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
# SPDX-License-Identifier: BSD-3-Clause
# Author: Pavlina Patova <patova@dyna-nic.com>, October 2024
#
# Unauthorized copying of this file, via any medium is strictly prohibited.
# Proprietary and confidential, additional license terms may apply.

import argparse
import nfb
import re
import subprocess
import sys

from src.RXMode import RXMode
from src.TXMode import TXMode


def get_gls_cnt(device):
    fdt_firmware = nfb.Nfb(device).fdt.get_node("firmware")

    gls_count = 0
    pattern = re.compile(r"^dbg_gls(\d+)$")
    fdt_mi_pci0_bar0 = fdt_firmware.get_subnode("mi_pci0_bar0")

    for node in fdt_mi_pci0_bar0.nodes:
        if re.search(pattern, node.name):
            gls_count += 1
    print(f"GLS modules: {gls_count}")

    if (gls_count == 0):
        raise RuntimeError("ERROR: Unsupported NDK firmware, no GLS modules found!")

    return gls_count


def main():
    default_path_rx = "../build/read/read"
    default_path_tx = "../build/generate/generate"

    parser = argparse.ArgumentParser()
    parser.add_argument("-m", "--mode", type=str, choices=["rx", "tx"], help="Measure mode. Default: tx", default="tx")
    parser.add_argument("-t", "--time", type=int, help="How long to measure for one packet len. Default: 10", default=10)
    parser.add_argument("-f", "--file-name", type=str, help="Name of the csv output file. Default: output.csv", default="output.csv")
    parser.add_argument("-l", "--lcores", type=int, help="Number of lcores to use. Default: 8", default=8)
    parser.add_argument("--queues-per-port", type=int, help="Queues per port. Default: all", default=None)
    parser.add_argument("--ports", type=int, help="How many ports to use. Default: all", default=None)
    parser.add_argument("-n", "--native", action='store_true', help="Use native dpdk mode.",)
    parser.add_argument("-d", "--dev", type=str, help="Path to device. Default: /dev/nfb0.", default="/dev/nfb0")
    parser.add_argument("--path-to-executable", type=str, help=f"Path to executable. Default for tx: {default_path_tx} ; Default for rx: {default_path_rx}", default=None)
    parser.add_argument("--min-len", type=int, help=f"Minimal length. Default is: 64", default=64)
    parser.add_argument("--max-len", type=int, help=f"Maximal length. Default is: 960", default=960)
    parser.add_argument("--stride", type=int, help=f"Stride. Default is: 8", default=8)
    parser.add_argument("--debug", action='store_true', help=f"Debug mode. Default is: False")

    args = parser.parse_args()
    nfb_device = nfb.open(args.dev)

    mode_name = args.mode

    if args.ports is None:
        args.ports = len(list(nfb_device.eth))

    if args.queues_per_port is None:
        args.queues_per_port = int(len(nfb_device.ndp.rx) / args.ports)

    args.gls_count = get_gls_cnt(args.dev)
    args.file_name = f"{mode_name}-" + args.file_name

    # nfb-info -d <path> -q pci
    cmd = [
        "nfb-info",
        "-d", args.dev,
        "-q", "pci"
    ]
    get_pci = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    pci, _ = get_pci.communicate()
    pci = [pci.decode("utf-8").strip()]

    # Determine packets lenghts to measure.
    # Here we start from 64B len and create packets to the 1024B with stride 8B.
    ticks = (args.max_len - args.min_len) // args.stride + 1
    pkt_sizes = []
    for i in range(ticks):
        pkt_sizes.append(args.min_len + i * args.stride)

    # Run selected measurement.
    if mode_name == "tx":
        if args.path_to_executable is None:
            args.path_to_executable = default_path_tx

        mode_obj = TXMode(args, pci, pkt_sizes)
    elif mode_name == "rx":
        if args.path_to_executable is None:
            args.path_to_executable = default_path_rx

        mode_obj = RXMode(args, pci, pkt_sizes)
    else:
        print(f"Invalid mode {mode_name}")
        return -1

    try:
        mode_obj.mode_start()
    except KeyboardInterrupt:
        sys.exit(0)

if __name__ == "__main__":
    main()
