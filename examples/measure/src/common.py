# common.py: Common functions for mesure application.
# Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
# SPDX-License-Identifier: BSD-3-Clause
# Author: Pavlina Patova <patova@dyna-nic.com>, October 2025
#
# Unauthorized copying of this file, via any medium is strictly prohibited.
# Proprietary and confidential, additional license terms may apply.

import subprocess

FCS = 4
IPG = 12
PREAMBULE = 8


def run_cmd(cmd, debug=False):
    if debug:
        print_cmd(cmd)

    subprocess.run(cmd, stdout=subprocess.PIPE)


def print_cmd(cmd):
    print(" ".join(cmd))
