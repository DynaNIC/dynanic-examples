/* argparse.h: Argument parser for generate example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, November 2024
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_GENERATE_ARGPARSE__
#define __DYNANIC_GENERATE_ARGPARSE__

#include <stdint.h>

#include "common_dpdk.h"

struct application_settings
{
    // DPDK settings
    struct dpdk_settings dpdk;

    // How often print stats
    uint64_t next_stats_delay_ms;

    // Size of send packets.
    uint16_t packet_length;

    // Flag to create packet.
    bool create_packet;
};

int parse_custom_args(int argc, char *argv[], struct application_settings *app_settings);

void print_help();

#endif //__DYNANIC_GENERATE_ARGPARSE__
