/* argparse.c: Argument parser for transmit example.
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
        {"pcap-file", required_argument, 0, 'f'},
        {"help", no_argument, 0, 'h'},
        {"transmit-queue", required_argument, 0 , 'i'},
        {0,0,0,0}
    };

    // Setup default values.
    int ret = set_default_dpdk_setts(&app_settings->dpdk);
    if (ret < 0)
        return -2;

    strcpy(app_settings->pcap_file, "test.pcap");

    // -1 means not used
    app_settings->transmit_queue = -1;
    app_settings->transmit_port = -1;
    app_settings->transmit_single = false;

    // Parse arguments
    set_app_args("hf:i:", long_options);

    while ((opt = getopt_long(argc, argv, get_short_args(), get_long_args(), &option_index)) != -1) {
        switch (opt) {
            case 'f':
                if (strlen(optarg) + 1 > PCAP_FILE_LEN) {
                    rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: Max file name length is %d.\n", PCAP_FILE_LEN);
                    return -1;
                }
                strcpy(app_settings->pcap_file, optarg);
                break;

            case 'h':
                print_help();
                rte_exit(0, "");
                break;

            case 'i':
                if (sscanf(optarg, "%hu,%u", &app_settings->transmit_port, &app_settings->transmit_queue) != 2) {
                    rte_exit(EXIT_FAILURE, "ERROR: Parametr --transmit-queue must be in a format 'port,queue' (ex. 0,1) got %s\n", optarg);
                }
                printf("transmit-queue set for port %hu, queue: %u\n", app_settings->transmit_port, app_settings->transmit_queue);
                app_settings->transmit_single = true;

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

    if (app_settings->transmit_single &&
        app_settings->transmit_port > app_settings->dpdk.ports - 1) {
        printf("ERROR: Port ID %d is out of bounds. For %d active port, valid IDs are 0 to %d.\n",
            app_settings->transmit_port,
            app_settings->dpdk.ports,
            app_settings->dpdk.ports - 1
        );
        return -2;
    }

    return 0;
}

void print_help()
{
    printf("Usage: ./transmit [EAL options] -- [Application options]\n");

    print_dpdk_args_help();

    printf("\nApplication specific options:\n");
    printf("\t -f, --pcap-file <pcap-file> = Read data from PCAP file. Default value for this parameter is 'test.pcap'.\n");
    printf("\t -i, --transmit-queue <port,queue> = Send packets only to specific queue on specified port.\n");
    printf("\t -h, --help = Print this help.\n");
}
