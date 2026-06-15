# TXMode.py: Class which handles measurement for TX application.
# Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
# SPDX-License-Identifier: BSD-3-Clause
# Author: Pavlina Patova <patova@dyna-nic.com>, October 2024
#
# Unauthorized copying of this file, via any medium is strictly prohibited.
# Proprietary and confidential, additional license terms may apply.

import subprocess
import time
import signal

from src.Mode import Mode
from src.common import print_cmd


class TXMode(Mode):
    def __init__(self, args, pci, packet_sizes):
        super().__init__(args, pci, packet_sizes)
        self.tx_program_name = args.path_to_executable
        self.other_params = []


    # Start measurement for TX measurement.
    def measured_program(self, packet_size:int)->(int, int):
        nics = self.get_nic()

        dpdk_cmd = [
                self.tx_program_name,
                "--log-level=error",
                "-l",
                f"0-{self.lcores}",
        ] + nics

        app_cmd = [
                "--",
                "--packet-length",
                f"{packet_size}",
                "--queues-per-port",
                f"{self.queues_per_port}",
                "--ports",
                f"{self.ports}",
        ] + self.other_params

        cmd = dpdk_cmd + app_cmd

        if self.debug:
            print_cmd(cmd)

        # Start TX process.
        self.process = subprocess.Popen(cmd , stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        # Let it run for selected time.
        time.sleep(self.time)

        # Then interupt it.
        self.process.send_signal(signal.SIGINT)

        self.wait_for_process()

        # Get result from aplication.
        send_bytes_raw = self.process.stdout.read().decode("utf-8")
        err = self.process.stderr.read().decode("utf-8")
        print(err)

        # Parse raw value and return it.
        return self.get_bytes(send_bytes_raw)
