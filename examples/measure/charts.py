# charts.py: Script for creating charts.
# Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
# SPDX-License-Identifier: BSD-3-Clause
# Author: Pavlina Patova <patova@dyna-nic.com>, October 2024
#
# Unauthorized copying of this file, via any medium is strictly prohibited.
# Proprietary and confidential, additional license terms may apply.

import argparse
import math
import matplotlib.pyplot as plt
import pandas as pd
import os
import subprocess

def get_card_name(device):
    base_cmd = [
        "nfb-info",
        "-d", device,
        "-q"
    ]

    process = subprocess.Popen(base_cmd + ["card"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    card_name, _ = process.communicate()
    card_name = card_name.decode("utf-8").strip()

    process = subprocess.Popen(base_cmd + ["project"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    project_name, _ = process.communicate()
    project_name = project_name.decode("utf-8").strip()

    return f"{card_name}--{project_name}"


def print_chart(speed, device, rx_csv, tx_csv, suffix="pdf", benchmark_chart_folder=None, separator=";", rx_column="L2 Speed", tx_column="L2 Speed") -> str:
    """Print benchmark chart to the PNG image.

    Args:
        T: Measurement speed.
        card_name: Name of the card for which we want to create documents.

    Return:
        Name of the saved figure.
    """

    file = f"{get_card_name(device)}--{rx_csv.split('.')[0]}--{tx_csv.split('.')[0]}.{suffix}"

    if benchmark_chart_folder is None:
        benchmark_chart_folder = "tmp/benchmark_chart"

    if not os.path.exists(benchmark_chart_folder):
        os.makedirs(benchmark_chart_folder)
    save_file_name = f'{benchmark_chart_folder}/{file}'

    # On X axis we only want some values/ticks.
    x_ticks = [64, 192, 320, 448, 576, 704, 832, 960]

    rx = pd.read_csv(rx_csv, sep=separator)
    tx = pd.read_csv(tx_csv, sep=separator)

    rx_x = rx['Length']
    rx_y = rx[rx_column]

    tx_x = tx['Length']
    tx_y = tx[tx_column]

    x_vals = range(64, x_ticks[len(x_ticks) - 1])

    # Count card thresholds.
    black = []
    gray = []

    for x in x_vals:
        black.append(speed*x/(x+20))
        gray.append(speed*(x)/(math.ceil((x-4)/8)*8+16))

    plt.subplots(figsize=(8.54,4.15))
    plt.xlim(64, 960)
    plt.ylim(0, speed+10)
    plt.xticks([64, 192, 320, 448, 576, 704, 832, 960])

    plt.xlabel("Frame size [B]")
    plt.ylabel("Throughput [Gbps]")

    plt.plot(rx_x, rx_y, color="royalblue", label="Packet capture over PCIe (net-to-host)")
    plt.plot(tx_x, tx_y, color="crimson", label="Traffic replay over PCIe (host-to-net)")

    # Plot from counted data.
    plt.plot(x_vals, black, color='black', label='Direct fast path forwarding (net-to-net)')
    plt.plot(x_vals, gray, color='gray', label='DYNANIC FPGA pipeline limit')

    plt.legend(loc="lower right")
    plt.grid(color="gray", linestyle='--', linewidth="0.3")

    # Option 'tight' removes borders from figure. If you use plt.show() after this command
    # you may not to see any difference.
    plt.savefig(save_file_name, bbox_inches='tight')

    return save_file_name


if __name__ == "__main__":
    """Main is only used for testing.
    """

    parser = argparse.ArgumentParser()
    parser.add_argument("-s", "--speed", type=int, help="Card speed. Default: 400.")
    parser.add_argument("-r", "--rx", type=str, default="rx-output.csv", help="RX data. Default: rx-output.csv.")
    parser.add_argument("-t", "--tx", type=str, default="tx-output.csv", help="TX data. Default: tx-output.csv.")
    parser.add_argument("-f", "--folder", type=str, default=".", help="Folder to put benchmark. Default: current folder.")
    parser.add_argument("-p", "--separator", type=str, default=",", help="Select default data separator. Default is ','.")
    parser.add_argument("--file-suffix", type=str, default="pdf", help="File suffix for result chart. Default is pdf.")
    parser.add_argument("-d", "--dev", type=str, help="Path to device. Default: /dev/nfb0.", default="/dev/nfb0")
    parser.add_argument("--rx-col", type=str, help="Name of the column with RX data. Default: 'L2 Speed'.", default="L2 Speed")
    parser.add_argument("--tx-col", type=str, help="Name of the column with TX data. Default: 'L2 Speed'.", default="L2 Speed")

    args = parser.parse_args()

    speed = 400
    if args.speed is not None:
        speed = args.speed

    rx_csv = args.rx
    tx_csv = args.tx
    folder = args.folder
    separator = args.separator
    file_suffix = args.file_suffix
    device = args.dev
    rx_column = args.rx_col
    tx_column = args.tx_col

    print_chart(speed, device, rx_csv=rx_csv, tx_csv=tx_csv, suffix=file_suffix, benchmark_chart_folder=folder, separator=separator, rx_column=rx_column, tx_column=tx_column)
