/* argparse.c: Argument parser for flow example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, December 2024
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "argparse.h"

/* Parse custum arguments. */
int parse_custom_args(
    int argc,
    char *argv[],
    struct application_settings *app_settings
) {
    int opt;
    int option_index = 0;
    static struct option long_options[] = {
        {"mbuf-cnt-per-queue", required_argument, 0, 'm'},
        {"mempool-cache-size", required_argument, 0, 'c'},
        {"burst-size", required_argument, 0, 'b'},
        {"descriptors", required_argument, 0, 'd'},
        {"mbuf-size", required_argument, 0, 'p'},
        {"measurement", no_argument, 0, 'e'},
        {"help", no_argument, 0, 'h'},

        {"disable-rte-flow", no_argument, 0, 'r'},
        {"example", required_argument, 0, 'x'},
    };
    uint8_t tmp = 0;

    /* Setup default options. */
    app_settings->measurement = false;
    app_settings->burst_size = 32;
    app_settings->descriptors = 1024;
    app_settings->mempool_cache_size = 512;
    app_settings->mbuf_cnt_per_queue = 4096;
    app_settings->mbuf_size = 1518;
    app_settings->started_ports = 0;

    app_settings->disable_rte_flow = false;
    app_settings->example_part_number = 1;

    while ((opt = getopt_long(argc, argv, "m:c:b:d:p:ehrx:" , long_options, &option_index)) != -1) {
        switch (opt) {
            case 'm':
                app_settings->mbuf_cnt_per_queue = atoi(optarg);
                break;

            case 'c':
                app_settings->mempool_cache_size = atoi(optarg);
                break;

            case 'b':
                app_settings->burst_size = atoi(optarg);
                break;

            case 'd':
                app_settings->descriptors = atoi(optarg);
                break;

            case 'p':
                app_settings->mbuf_size = atoi(optarg);
                break;

            case 'e':
                app_settings->measurement = true;
                break;

            case 'h':
                print_help();
                return -1;
                break;

            case 'r':
                app_settings->disable_rte_flow = true;
                break;

            case 'x':
                tmp = atoi(optarg);
                if (tmp > 3 || tmp < 1) {
                    rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: Avalable examples are 1 - 3. %d not supported.\n", tmp);
                    return - 1;
                }
                app_settings->example_part_number = tmp;
                break;

            default:
                rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: Option %s not supported.\n", opt);
                print_help();
                return -1;
                break;
        }
    }

    return 0;
}

void print_help() {
    printf("Usage: ./read [EAL-options] -- [Application-specific-options]\n");
    printf("Useful EAL-options:\n");
    printf("\t -a <PCI-address> = Allow NIC to be used with DPDK.\n");
    printf("\t --log-level=<log-level> = Logging level of the DPDK application (debug, info, error, ...).\n");
    printf("\t -l <lcore-range> = How many lcores to use. For example -l 0-4 will enable app to use one main core and four worker cores. This program also allocates as many RX queues as worker lcores. Therefore if you allocace 4 worker cores, program will be ran with 4 RX queues.\n");

    printf("Application-specific-options:\n");
    printf("\t -m, --mbuf-cnt-per-queue <mbuf-cnt-per-queue> = Determine base value for the mempool size. To get real size is this value multiplied by number of queues. Default value of this parameter is 4096\n");
    printf("\t -c, --mempool-cache-size <mempool_cache_size> = Determine mempool cache size to use when creating real mempool struct. Default value for this parameter is 512.\n");
    printf("\t -b, --burst-size <burst_size> = Maximum packets that can be fetched by one call of rte_eth_rx_burst function. Default value for this parameter is 32.\n");
    printf("\t -d, --descriptors <number_of_descriptors> = The number of receive descriptors to allocate for the receive ring. This value should not be grater than burst size. Default value for this parameter is 1024.\n");
    printf("\t -p, --mbuf-size <mbuf_size> = Maximum size of one packet. Size of the headroom is then added to selected size. Therefore this parameter determines mbuf size without packet headroom. Final size of the mbuf is couted like mbuf_size + 128. Default value for this parameter is 1518.\n");
    printf("\t -e, --measurement = Flag to enable measurement mode where all informations about traffic are printed only on the app end.\n");
    printf("\t -h, --help = Print this help.\n");

    printf("\t -r, --disable-rte-flow = Flag to disable RTE Flow.\n");
    printf("\t -x, --example <1|2|3>= Determines what RTE Flow rules to use. Defaul value is 1.\n");
}
