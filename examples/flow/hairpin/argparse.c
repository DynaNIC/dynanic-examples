/* argparse.c: Argument parser for hairpin example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Denis Kurka <kurka@dyna-nic.com>, May 2026
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "argparse.h"
#include "common_args.h"

#include <rte_eal.h>
#include <stdio.h>
#include <stdlib.h>

int parse_custom_args(int argc, char *argv[], struct application_settings *app_settings)
{
    int opt;
    int option_index = 0;
    static struct option long_options[] = {
        {"std-queues", required_argument, 0, 's'},
        {"hp-queues",  required_argument, 0, 'q'},
        {"desc",       required_argument, 0, 'd'},
        {"packets",    required_argument, 0, 'p'},
        {"next-stats", required_argument, 0, 't'},
        {"help",       no_argument,       0, 'h'},
        {0,0,0,0}
    };

    int ret = set_default_dpdk_setts(&app_settings->dpdk);
    if (ret < 0) return -2;

    // Set application defaults
    app_settings->next_stats_delay_ms = 5000;
    app_settings->nb_std_queues = 1;
    app_settings->nb_hp_queues = 1;
    app_settings->nb_desc = 1024;
    app_settings->packets_to_send = 64;

    set_app_args("s:q:d:p:t:h", long_options);

    while ((opt = getopt_long(argc, argv, get_short_args(), get_long_args(), &option_index)) != -1) {
        switch (opt) {
            case 's':
                app_settings->nb_std_queues = atoi(optarg);
                break;
            case 'q':
                app_settings->nb_hp_queues = atoi(optarg);
                break;
            case 'd':
                app_settings->nb_desc = atoi(optarg);
                break;
            case 'p':
                app_settings->packets_to_send = atoi(optarg);
                break;
            case 't':
                app_settings->next_stats_delay_ms = atoi(optarg) * 1000;
                break;
            case 'h':
                print_help();
                rte_exit(0, "");
                break;
            default:
                printf("ERROR: Option %c not supported.\n", opt);
                print_help();
                return -1;
        }
    }
    return 0;
}

void print_help(void)
{
    printf("Usage: ./hairpin [EAL options] -- [Application options]\n");
    printf("\nApplication specific options:\n");
    printf("\t -s, --std-queues <num>  = Number of SW queues (default: 1).\n");
    printf("\t -q, --hp-queues <num>   = Number of Hairpin queues (default: 1).\n");
    printf("\t -d, --desc <num>        = Number of descriptors (default: 1024).\n");
    printf("\t -p, --packets <num>     = Total number of packets to send (default: 64).\n");
    printf("\t -t, --next-stats <secs> = Seconds between stats display (default: 5s).\n");
    printf("\t -h, --help              = Print this help.\n");
}