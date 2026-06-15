/* argparse.h: Argument parser for metadata_viewer example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, April 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_METAVIEWER_ARGPARSE__
#define __DYNANIC_METAVIEWER_ARGPARSE__

#include <stdint.h>

#include "common_dpdk.h"

struct application_settings
{
    // DPDK settings
    struct dpdk_settings dpdk;

    // Enable RTE Flow rule.
    bool enable_flow;
};

int parse_custom_args(int argc, char *argv[], struct application_settings *app_settings);

void print_help();

#endif //__DYNANIC_METAVIEWER_ARGPARSE__
