/* argparse.c: Argument parser for receive example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, January 2025
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
        {"packet-threshold", required_argument, 0, 't'},
        {"file-prefix", required_argument, 0, 'f'},
        {"dump", required_argument, 0, 'x'},
        {"help", no_argument, 0, 'h'},
        {0,0,0,0}
    };

    // Setup default values.
    int ret = set_default_dpdk_setts(&app_settings->dpdk);
    if (ret < 0)
        return -2;

    app_settings->max_packets_per_queue = 1;
    app_settings->check_packet_th = false;
    strcpy(app_settings->file_prefix, "received");
    app_settings->frequency = 5;
    app_settings->dump_packets = false;

    // Parse arguments
    set_app_args("ht:f:x:", long_options);

    while ((opt = getopt_long(argc, argv, get_short_args(), get_long_args(), &option_index)) != -1) {
        switch (opt) {
            case 't':
                app_settings->max_packets_per_queue = atoi(optarg);
                app_settings->check_packet_th = true;
                break;

            case 'f':
                if (strlen(optarg) + 1 > FILE_PREFIX_LEN) {
                    rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: Max file prefix length is %d.\n", FILE_PREFIX_LEN);
                    return -1;
                }
                strcpy(app_settings->file_prefix, optarg);
                break;

            case 'x':
                app_settings->frequency = atoi(optarg);
                app_settings->dump_packets = true;
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
    printf("Usage: ./receive [EAL options] -- [Application options]\n");

    print_dpdk_args_help();

    printf("\nApplication specific options:\n");
    printf("\t -t, --packet-threshold <packet_threshold> = Given number of packets will be written to the file. By default there is no limitation.\n");
    printf("\t -f, --file-prefix <file_prefix> = Name of the file where packets will be stored. Default 'output/packet'\n");
    printf("\t -x, --dump <frequency> = Print every <frequency>-th packet to the terminal. Disabled by default.\n");
    printf("\t -h, --help = Print this help.\n");
}
