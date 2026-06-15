/* argparse.c: Argument parser for generate example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, November 2024
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "argparse.h"

#include "common_args.h"

// Parse custom arguments.
int parse_custom_args(
    int argc,
    char *argv[],
    struct application_settings *app_settings)
{
    int opt;
    int option_index = 0;
    static struct option long_options[] = {
        {"next-stats", required_argument, 0, 't'},
        {"packet-length", required_argument, 0, 'l'},
        {"create-packet", no_argument, 0, 'r'},
        {"help", no_argument, 0, 'h'},
        {0,0,0,0}
    };

    // Setup default values.
    int ret = set_default_dpdk_setts(&app_settings->dpdk);
    if (ret < 0)
        return -2;

    app_settings->next_stats_delay_ms = 5000;
    app_settings->packet_length = 512;
    app_settings->create_packet = false;

    // Parse arguments
    set_app_args("t:l:rh", long_options);

    while ((opt = getopt_long(argc, argv, get_short_args(), get_long_args(), &option_index)) != -1) {
        switch (opt) {
            case 't':
                app_settings->next_stats_delay_ms = atoi(optarg) * 1000;
                break;

            case 'l':
                app_settings->packet_length = atoi(optarg);
                break;

            case 'r':
                app_settings->create_packet = true;
                break;

            case 'h':
                print_help();
                rte_exit(0, "");
                break;

            default:
                if (is_dpdk_arg(opt)) {
                    parse_dpdk_args(opt, optarg, &app_settings->dpdk);
                } else {
                    printf("ERROR: Option %c not supported.\n", opt);
                    print_help();
                    return -1;
                }
                break;
        }
    }

    return 0;
}

void print_help()
{
    printf("Usage: ./generate [EAL options] -- [Application options]\n");

    print_dpdk_args_help();

    printf("\nApplication specific options:\n");
    printf("\t -t, --next-stats <time_in_s> = How many seconds to wait between stats display. Default value for this parameter is 5s.\n");
    printf("\t -l, --packet-length <packet size> = Length of generated packet. Note that this value should not be greater then mbuf size. Default value for this parameter is 512.\n");
    printf("\t -r, --create-packet = Flag to enable memcpy of dummy packet. If this flag is not active empty packet will be generated.\n");
    printf("\t -h, --help = Print this help.\n");
}
