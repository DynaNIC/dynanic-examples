/* common_dpdk.c: Common argument functions used in the DynaNIC examples.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Jan Privara <privara@dyna-nic.com>, November 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "common_args.h"

#include <string.h>

#define MIN_MEMPOOL_SIZE 8196

#define MAX_LONG_ARGS 50
#define MAX_SHORT_ARGS_LEN 50

static struct option dpdk_long_args[MAX_LONG_ARGS] =
{
    {"mempool-size",       required_argument, 0, 'm'},
    {"mempool-cache-size", required_argument, 0, 'c'},
    {"burst-size",         required_argument, 0, 'b'},
    {"descriptors",        required_argument, 0, 'd'},
    {"mbuf-size",          required_argument, 0, 's'},
    {"ports",              required_argument, 0, 'p'},
    {"queues-per-port",    required_argument, 0, 'q'},
    {0,0,0,0}
};

char dpdk_short_args[MAX_SHORT_ARGS_LEN] = "m:c:b:d:s:p:q:";

int set_default_dpdk_setts(struct dpdk_settings *dpdk)
{
    dpdk->mempool_cache_size = 512;
    dpdk->burst_size = 32;
    dpdk->descriptors = 1024;
    dpdk->mbuf_size = 1518;

    dpdk->ports = 1;

    struct rte_eth_dev_info dev_info;
    int ret = rte_eth_dev_info_get(0, &dev_info);
    if (ret != 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_eth_dev_info_get() faild for port %u: %s\n",
                0,
                strerror(-ret)
        );

        return ret;
    }

    dpdk->queues_per_port = dev_info.max_tx_queues;

    /* NOTE: optimal mempool size can be counted as:
        nports * nb_rx_queue * nb_rxd +
        nports * nb_lcores * MAX_PKT_BURST +
        nports * n_tx_queue * nb_txd +
        nb_lcores * MEMPOOL_CACHE_SIZE

        But min should be 8192.

        This info is taken from l3fwd example source code.
    */
    dpdk->mempool_size = RTE_MAX(
            dpdk->ports * dpdk->queues_per_port * dpdk->descriptors + \
            dpdk->ports * 0 * 0 + \
            dpdk->ports * 16 * dpdk->burst_size + \
            16 * dpdk->mempool_cache_size, MIN_MEMPOOL_SIZE);

    // Clear port configuration.
    memset(&dpdk->port_conf, 0, (sizeof(struct rte_eth_conf)));

    return 0;
}

unsigned long_args_count(struct option *long_args)
{
    unsigned cnt = 0;

    while(long_args[cnt].name != 0) {
        cnt++;
    }

    return cnt;
}

void set_app_args(char *short_args, struct option *long_args)
{
    // copy in application short options
    if (strlen(short_args) + strlen(dpdk_short_args) >= MAX_SHORT_ARGS_LEN) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: short options overflow\n");
        return;
    }

    strcat(dpdk_short_args, short_args);

    // copy in application long options
    unsigned dpdk_long_args_cnt = long_args_count(dpdk_long_args);
    unsigned app_long_args_cnt = long_args_count(long_args);

    if (app_long_args_cnt + dpdk_long_args_cnt >= MAX_LONG_ARGS) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: long options overflow\n");
        return;
    }

    for (unsigned i = 0; i < app_long_args_cnt; i++) {
        unsigned idx = dpdk_long_args_cnt + i;
        dpdk_long_args[idx] = long_args[i];

        dpdk_long_args[idx+1].name = 0;
        dpdk_long_args[idx+1].flag = 0;
        dpdk_long_args[idx+1].has_arg = 0;
        dpdk_long_args[idx+1].val = 0;
    }
}

char * get_short_args()
{
    return dpdk_short_args;
}

struct option * get_long_args()
{
    return dpdk_long_args;
}

void parse_dpdk_args(int opt, char *optarg, struct dpdk_settings *dpdk)
{
    switch (opt) {
        case 'm':
            dpdk->mempool_size = atoi(optarg);
            break;

        case 'c':
            dpdk->mempool_cache_size = atoi(optarg);
            break;

        case 'b':
            dpdk->burst_size = atoi(optarg);
            break;

        case 'd':
            dpdk->descriptors = atoi(optarg);
            break;

        case 's':
            dpdk->mbuf_size = atoi(optarg);
            break;

        case 'p':
            dpdk->ports = atoi(optarg);
            break;

        case 'q':
            dpdk->queues_per_port = atoi(optarg);
            break;

        default:
            break;
    }
}

bool is_dpdk_arg(int opt)
{
    // options used in parse_dpdk_args
    char dpdk_args[] = "mcbdspq";

    if (strchr(dpdk_args, (char)opt) != NULL) {
        return true;
    }

    return false;
}

void print_dpdk_args_help()
{
    printf("Useful EAL options:\n");
    printf("\t -a <PCI-address> = Allow NIC to be used with DPDK.\n");
    printf("\t --log-level=<log-level> = Logging level of the DPDK application (debug, info, error, ...).\n");
    printf("\t -l <lcore-range> = Threads that will be used to process packets.\n");

    printf("\nApplication DPDK options:\n");
    printf("\t -m, --mempool-size <mempool_size> = Determine base value for the mempool size.\n");
    printf("\t -c, --mempool-cache-size <mempool_cache_size> = Determine mempool cache size to use when creating real mempool struct. Default value for this parameter is 512.\n");
    printf("\t -b, --burst-size <burst_size> = Maximum packets that can be fetched by one call of rte_eth_rx_burst function. Default value for this parameter is 32.\n");
    printf("\t -d, --descriptors <number_of_descriptors> = The number of receive descriptors to allocate for the receive ring. Default value for this parameter is 1024.\n");
    printf("\t -s, --mbuf-size <mbuf_size> = Maximum size of one packet. Default value for this parameter is 1518 + RTE_PKTMBUF_HEADROOM.\n");
    printf("\t -p, --ports <number of used ports> = How many ports should be used. Default value for this parameter is 1.\n");
    printf("\t -q, --queues-per-port <number of used ports> = How many queues should inicialized per port. Default value for this parameter is the maximum for the given card.\n");
}
