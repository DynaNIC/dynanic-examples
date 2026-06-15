/* argparse.c: Argument parser for table filler.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Jan Privara <privara@dyna-nic.com>, January 2026
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
        {"group", required_argument, 0, 'g'},
        {"key", required_argument, 0, 'k'},
        {"rules", required_argument, 0, 'n'},
        {"gen-iter", no_argument, 0, 'i'},
        {"failure-exit", no_argument, 0, 'e'},
        {"next-stats", required_argument, 0, 't'},
        {"help", no_argument, 0, 'h'},
        {0,0,0,0}
    };

    // Setup default values.
    int ret = set_default_dpdk_setts(&app_settings->dpdk);
    if (ret < 0)
        return -2;

    app_settings->group = 0;
    app_settings->key = 0;
    app_settings->rules = 0;
    app_settings->gen_rand = true;
    app_settings->next_stats_delay_ms = 1000;
    app_settings->failure_exit = false;

    // Parse arguments
    set_app_args("g:k:n:iet:h", long_options);

    while ((opt = getopt_long(argc, argv, get_short_args(), get_long_args(), &option_index)) != -1) {
        switch (opt) {
            case 'g':
                app_settings->group = atoi(optarg);
                break;
            case 'k':
                app_settings->key = atoi(optarg);
                break;
            case 'n':
                app_settings->rules = atoi(optarg);
                break;
            case 'i':
                app_settings->gen_rand = false;
                break;
            case 'e':
                app_settings->failure_exit = true;
                break;
            case 't':
                app_settings->next_stats_delay_ms = atoi(optarg);
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
    printf("Usage: ./table_filler [EAL options] -- [Application options]\n");

    print_dpdk_args_help();

    printf("\nApplication specific options:\n");
    printf("\t -g, --group <group_idx> = Group ID. Default value is 0.\n");
    printf("\t -k, --key <key_type> = Key ID / type. Default value is 0.\n");
    printf("\t -n, --rules <max_rules> = Maximum number of records to be inserted, 0 = unlimited. Default value is 0.\n");
    printf("\t -i, --gen-iter = Iterative rules generation mode. Otherwise random mode. Off by default.\n");
    printf("\t -e, --failure-exit = Exit on first rule insertion failure. Off by default.\n");
    printf("\t -t, --next-stats <time_in_s> = How many seconds to wait between stats display. Default value for this parameter is 1 s.\n");
    printf("\t -h, --help = Print this help.\n");
}
