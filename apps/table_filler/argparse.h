/* argparse.h: Argument parser for table filler.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Jan Privara <privara@dyna-nic.com>, January 2026
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_TABLE_FILLER_ARGPARSE__
#define __DYNANIC_TABLE_FILLER_ARGPARSE__

#include <stdint.h>

#include "common_dpdk.h"

struct application_settings
{
    // DPDK settings
    struct dpdk_settings dpdk;
    unsigned group;
    unsigned key;
    uint64_t rules;
    bool gen_rand;
    bool failure_exit;
    // How often print stats.
    uint64_t next_stats_delay_ms;
};

int parse_custom_args(int argc, char *argv[], struct application_settings *app_settings);

void print_help();

#endif //__DYNANIC_TABLE_FILLER_ARGPARSE__
