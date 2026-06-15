# RXMode.py: Class which handles measurement for RX application.
# Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
# SPDX-License-Identifier: BSD-3-Clause
# Author: Pavlina Patova <patova@dyna-nic.com>, October 2025
#
# Unauthorized copying of this file, via any medium is strictly prohibited.
# Proprietary and confidential, additional license terms may apply.

import subprocess
import time
import signal

from src.Mode import Mode
from src.common import print_cmd

from ofm.comp.mfb_tools.debug.gen_loop_switch import GenLoopSwitch


class RXMode(Mode):
    def __init__(self, args, pci, packet_sizes):
        super().__init__(args, pci, packet_sizes)
        self.rx_program_name = args.path_to_executable
        self.other_params = []

        self.gls = [GenLoopSwitch(dev=self.device, index=i) for i in range(args.gls_count)]


    def __del__(self):
        # Return MUX to default state
        super().__del__()
        for g in self.gls:
            if g is not None:
                g.enabled = False
                g.l2r.mux_generator = 0


    def prepare(self):
        super().prepare()
        for g in self.gls:
            if g is not None:
                g.l2r.mux_generator = 1


    def measured_program(self, packet_size):
        nics = self.get_nic()

        dpdk_cmd = [
                self.rx_program_name,
                "--log-level=info",
                "-l",
                f"0-{self.lcores}",
        ] + nics

        app_cmd = [
                "--",
                "--ports", f"{self.ports}",
                "--queues-per-port", f"{self.queues_per_port}",
        ] + self.other_params

        cmd = dpdk_cmd + app_cmd

        if self.debug:
            print_cmd(cmd)

        gen_length = packet_size - 4

        # Start HW generator with specified packet length.
        for g in self.gls:
            if g is not None:
                g.l2r.gen.frame_length = gen_length
                g.l2r.gen.enabled = True

        time.sleep(1)

        # Start RX process.
        self.process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        # Let it run for selected time.
        time.sleep(self.time)

        # Stop read process.
        self.process.send_signal(signal.SIGINT)

        self.wait_for_process()

        error_code = self.process.poll()

        # Get result from aplication.
        stdout, stderr = self.process.communicate()
        if self.debug:
            print("Output:", stdout.decode("utf-8"))

        if error_code != 0:
            print(f"Error ({error_code}):", stderr.decode("utf-8"))

        send_bytes_raw = stdout.decode("utf-8")

        # Stop HW generator.
        for g in self.gls:
            if g is not None:
                g.l2r.gen.enabled = False

        time.sleep(0.5)

        # Parse raw value and return it.
        return self.get_bytes(send_bytes_raw)
