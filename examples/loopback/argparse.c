/* argparse.c: Argument parser for loopback example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, November 2024
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "argparse.h"

#include "common_args.h"

/* Parse custom arguments. */
int parse_custom_args(
    int argc,
    char *argv[],
    struct application_settings *app_settings)
{
    int opt;
    int option_index = 0;
    static struct option long_options[] = {
        {"next-stats", required_argument, 0, 't'},
        {"help", no_argument, 0, 'h'},
        {0,0,0,0}
    };

    // Setup default values.
    int ret = set_default_dpdk_setts(&app_settings->dpdk);
    if (ret < 0)
        return -2;

    app_settings->next_stats_delay_ms = 5000;

    // Parse arguments
    set_app_args("t:h", long_options);

    while ((opt = getopt_long(argc, argv, get_short_args(), get_long_args(), &option_index)) != -1) {
        switch (opt) {
            case 't':
                app_settings->next_stats_delay_ms = atoi(optarg) * 1000;
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
    printf("Usage: ./loopback [EAL options] -- [Application options]\n");

    print_dpdk_args_help();

    printf("\nApplication specific options:\n");
    printf("\t -t, --next-stats <time_in_s> = How many seconds to wait between stats display. Default value for this parameter is 5s.\n");
    printf("\t -h, --help = Print this help.\n");
}
