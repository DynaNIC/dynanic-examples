/* argparse.h: Argument parser for transmit example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, January 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_TRANSMIT_ARGPARSE__
#define __DYNANIC_TRANSMIT_ARGPARSE__

#include <stdint.h>

#include "common_dpdk.h"

#define PCAP_FILE_LEN 100

struct application_settings
{
    // DPDK settings
    struct dpdk_settings dpdk;

    // File with packets to send.
    char pcap_file [PCAP_FILE_LEN];

    uint16_t transmit_port;
    uint32_t transmit_queue;
    bool transmit_single;
};

int parse_custom_args(int argc, char *argv[], struct application_settings *app_settings);

void print_help();

#endif //__DYNANIC_TRANSMIT_ARGPARSE__
