/* argparse.h: Argument parser for bonding example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Denis Kurka <kurka@dyna-nic.com>, April 2026
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_BONDING_ARGPARSE__
#define __DYNANIC_BONDING_ARGPARSE__

#include <stdint.h>
#include "common_dpdk.h"

struct application_settings
{
    // DPDK settings
    struct dpdk_settings dpdk;

    // How often print stats
    uint64_t next_stats_delay_ms;
};

int parse_custom_args(int argc, char *argv[], struct application_settings *app_settings);
void print_help();

#endif // __DYNANIC_BONDING_ARGPARSE__
