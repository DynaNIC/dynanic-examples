# Mode.py: Abstract class for measurable application.
# Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
# SPDX-License-Identifier: BSD-3-Clause
# Author: Pavlina Patova <patova@dyna-nic.com>, October 2025
#
# Unauthorized copying of this file, via any medium is strictly prohibited.
# Proprietary and confidential, additional license terms may apply.

import signal
import time

from src.common import (
    FCS,
    IPG,
    PREAMBULE,
    run_cmd,
)


class Mode(object):
    def __init__(self, args, pci, packet_sizes):
        self.time = args.time
        self.packet_sizes = packet_sizes
        self.file_name = args.file_name
        self.pci = pci
        self.native = args.native
        self.device = args.dev

        self.ports = args.ports
        self.queues_per_port = args.queues_per_port
        self.lcores = args.lcores

        self.debug = args.debug

        self.process = None

        self.write_stats_header()


    def __del__(self):
        print("\nExiting...")
        if self.process is not None:
            self.process.send_signal(signal.SIGINT)

        time.sleep(2)
        self.__delete_hugepages()


    def wait_for_process(self):
        warn_printed = False
        while self.process.poll() is None:
            if not warn_printed:
                print("Waiting for process to finish")
                warn_printed = True


    def measured_program(self, packet_size):
        print("Mode does not provide default implementation...")
        return 0, 0


    def __enable_eth(self):
        cmd = [
            "nfb-eth",
            "-d", f"{self.device}",
            "-e", "1"
        ]

        run_cmd(cmd, debug=self.debug)


    def __enable_hugepages(self):
        run_cmd([
            "dpdk-hugepages.py",
            "--setup", "10G",
            "-p", "1G"
        ], debug=self.debug)

    def __delete_hugepages(self):
        run_cmd([
            "dpdk-hugepages.py",
            "--unmount"
        ], debug=self.debug)

        run_cmd([
            "dpdk-hugepages.py",
            "--clear"
        ], debug=self.debug)


    def prepare(self):
        self.__enable_eth()
        self.__enable_hugepages()


    def mode_start(self):
        self.prepare()
        self.start_measurement()


    def start_measurement(self):
        measurement_time = self.time

        # Run measurement for every pre selected packet size.
        for p in self.packet_sizes:
            print("\n-----------------------------\n")
            print(f"Measuring for {p}B packet.")

            # Start measured aplication.
            processed_packets, processed_bytes = self.measured_program(p)

            dma_bytes = processed_bytes + processed_packets
            l2_bytes = processed_bytes + processed_packets * FCS
            l1_bytes = processed_bytes + processed_packets * (FCS + IPG + PREAMBULE)

            # Calculate all types of speed.
            speed_dma = self.get_speed(dma_bytes, measurement_time)
            speed_l2 = self.get_speed(l2_bytes, measurement_time)
            speed_l1 = self.get_speed(l1_bytes, measurement_time)

            # Save data for later use.
            # If you wish to update output format do that here and in the stats variable definition.
            print(f"L2 speed for {p}B packet is {speed_l2} Gbps.")
            self.write_stats(f"{p},{speed_dma},{speed_l2},{speed_l1}")


    def get_bytes(self, ret_text:str) -> int:
        """Parse text from measured program to get total processed bytes.
        Format is: Packets: <desired_value1>, Bytes: <desired_value2>\n
        """

        ret = []

        # Remove white chars at the end of lines and split by lines
        lines = ret_text.strip().split('\n')

        # Get last line of the output.
        last_line = lines[-1] if lines else ""

        # Split packet cnt and byte cnt.
        individual_stats = last_line.split(",")
        for s in individual_stats:
            q = s.split(":")
            if len(q) < 2:
                print(f"Something wrong we should have data in format <name>: <data> got: {s}")
                return 0, 0
            else:
                ret.append(int(q[1]))

        if len(ret) != 2:
            print(f"Something is wrong we should get 2 values but got {len(ret)}. Data got = {ret_text}, expected = Packets: <desired_value1>, Bytes: <desired_value2>.")
            return 0, 0

        return ret[0], ret[1]


    def get_speed(self, processed_bytes:int, measurement_time:int)->int:
        giga = 1000000000
        bit = 8
        return processed_bytes / measurement_time * bit / giga


    def get_nic(self):
        nics = []
        for nic in self.pci:
            if self.native:
                nic = f"{nic},queue_driver=native"

            nics.append("-a")
            nics.append(nic)

        return nics


    def write_stats(self, data, write_mode="a"):
        with open(self.file_name, write_mode) as f:
            f.write(data)
            f.write("\n")


    def write_stats_header(self):
        self.write_stats("Length,DMA Speed,L2 Speed,L1 Speed", write_mode="w")
