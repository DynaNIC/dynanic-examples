/* argparse.h: Argument parser for receive example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, January 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_RECEIVE_ARGPARSE__
#define __DYNANIC_RECEIVE_ARGPARSE__

#include <stdint.h>

#include "common_dpdk.h"

#define FILE_PREFIX_LEN 50

struct application_settings
{
    // DPDK settings
    struct dpdk_settings dpdk;

    // Packet dump thresholds.
    int max_packets_per_queue;

    // Use packet number threshold.
    bool check_packet_th;

    // Packet file prefix.
    char file_prefix[FILE_PREFIX_LEN];

    // Dump packets to console.
    bool dump_packets;

    // Packet dumping frequency [packets]
    int frequency;
};

int parse_custom_args(int argc, char *argv[], struct application_settings *app_settings);

void print_help();

#endif //__DYNANIC_RECEIVE_ARGPARSE__
