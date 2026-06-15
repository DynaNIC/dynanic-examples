/* table_filler.c: Tool for filling RTE rule tables.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Jan Privara <privara@dyna-nic.com>, January 2026
*
* A tool for testing capacity of RTE flow tables by generating and inserting rules to them.
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "common_dpdk.h"
#include <rte_flow.h>

#include "argparse.h"

#include <sys/time.h>
#include <signal.h>

/* Flag variable to determine when to stop packet processing. */
static volatile int __stop = 0;

/* Handle CTRL+C and kill signals. */
static void handle_sig(int sig)
{
    switch (sig) {
    case SIGINT:
    case SIGTERM:
        __stop = 1;
        break;
    }
}

/* Return whether to stop main loop or not. */
static int stop_loop(void)
{
    return __stop;
}

/* Init all necessary components. */
int init(
    int argc, char *argv[],
    struct application_settings *app_settings)
{
    // DPDK enviroment init.
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_eal_init() failed.\n");
        return ERROR_DONT_CLEANUP;
    }
    argc -= ret;
    argv += ret;

    // Parse custom command line arguments.
    ret = parse_custom_args(argc, argv, app_settings);
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: parse_custom_args() failed.\n");
        return ERROR_DONT_CLEANUP;
    }

    if (app_settings->key < 0 || app_settings->key > 5) {
        rte_exit(EXIT_FAILURE, "ERROR: parse_custom_args() Invalid key value - allowed values are 0-5\n");
        return ERROR_DONT_CLEANUP;
    }

    // RX port configuration.
    app_settings->dpdk.port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;
    app_settings->dpdk.port_conf.rxmode.offloads = 0;

    app_settings->dpdk.port_conf.rx_adv_conf.rss_conf.rss_key = NULL;
    app_settings->dpdk.port_conf.rx_adv_conf.rss_conf.rss_hf = RTE_ETH_RSS_IP;

    return 0;
}

static unsigned ipc = 1;

struct rte_flow* create_rule_key1(uint16_t portid,
                             unsigned group,
                             bool gen_rand,
                             struct rte_flow_error *error)
{
    struct rte_flow_attr attr = {
        .ingress = 1,
        .group = group
    };
    struct rte_flow_item pattern[2];
    struct rte_flow_action actions[2];
    struct rte_flow_item_ipv4 ipv4 = {0};
    struct rte_flow_item_ipv4 ipv4_mask = {0};
    struct rte_flow *flow = NULL;

    // Parameter for queue action.
    struct rte_flow_action_queue queue = { .index = 0 };

    // IP Pattern specification.
    ipv4.hdr.src_addr = gen_rand ? rand() : ipc++;
    ipv4_mask.hdr.src_addr = htonl(0xffffffff);

    // Match IP header.
    pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV4;
    pattern[0].spec = &ipv4;
    pattern[0].last = NULL;
    pattern[0].mask = &ipv4_mask;

    // End the pattern array.
    pattern[1].type = RTE_FLOW_ITEM_TYPE_END;

    // Create the queue action.
    actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
    actions[0].conf  = &queue;

    actions[1].type = RTE_FLOW_ACTION_TYPE_END;

    // Validate and create the flow rule.
    flow = rte_flow_create(portid, &attr, pattern, actions, error);

    return flow;
}

struct rte_flow* create_rule_key3(uint16_t portid,
                             unsigned group,
                             bool gen_rand,
                             struct rte_flow_error *error)
{
    struct rte_flow_attr attr = {
        .ingress = 1,
        .group = group
    };
    struct rte_flow_item pattern[3];
    struct rte_flow_action actions[2];
    struct rte_flow_item_ipv4 ipv4 = {0};
    struct rte_flow_item_ipv4 ipv4_mask = {0};
    struct rte_flow_item_udp  udp = {0};
    struct rte_flow_item_udp  udp_mask = {0};
    struct rte_flow *flow = NULL;

    // Parameter for queue action.
    struct rte_flow_action_queue queue = { .index = 0 };

    // IP Pattern specification.
    ipv4.hdr.src_addr = gen_rand ? rand() : ipc++;
    ipv4_mask.hdr.src_addr = htonl(0xffffffff);

    ipv4.hdr.dst_addr = 0;
    ipv4_mask.hdr.dst_addr = htonl(0xffffffff);

    ipv4.hdr.next_proto_id = 10;
    ipv4_mask.hdr.next_proto_id = 0xff;

    // Match IP header.
    pattern[0].type = RTE_FLOW_ITEM_TYPE_IPV4;
    pattern[0].spec = &ipv4;
    pattern[0].last = NULL;
    pattern[0].mask = &ipv4_mask;

    // UDP Pattern specification.
    udp.hdr.src_port = 0;
    udp_mask.hdr.src_port = 0xffff;

    udp.hdr.dst_port = 0;
    udp_mask.hdr.dst_port = 0xffff;

    // Match UDP header.
    pattern[1].type = RTE_FLOW_ITEM_TYPE_UDP;
    pattern[1].spec = &udp;
    pattern[1].last = NULL;
    pattern[1].mask = &udp_mask;

    // End the pattern array.
    pattern[2].type = RTE_FLOW_ITEM_TYPE_END;

    // Create the queue action.
    actions[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
    actions[0].conf  = &queue;

    actions[1].type = RTE_FLOW_ACTION_TYPE_END;

    // Validate and create the flow rule.
    flow = rte_flow_create(portid, &attr, pattern, actions, error);

    return flow;
}

int destroy_all_flow_rules(uint16_t portid)
{
    struct rte_flow_error error;
    int ret = 0;

    ret = rte_flow_flush(portid, &error);
    if (ret != 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_flow_flush() failed: %s (%d).\n", error.message, error.type);
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    struct application_settings app_settings;

    struct rte_flow *flow = NULL;
    struct rte_flow_error error;
    uint64_t rules_inserted = 0;
    uint64_t rules_failed = 0;

    struct timeval current_time_raw;
    uint64_t current_time;
    uint64_t measurement_time = 0;

    int ret = init(argc, argv, &app_settings);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "ERROR: init() failed.\n");
    }

    // Register signal for loop stopping.
    signal(SIGINT, &handle_sig);
    signal(SIGTERM, &handle_sig);

    // Create mbuf.
    struct rte_mempool *mbuf_pool;
    mbuf_pool = rte_pktmbuf_pool_create(
        "pcap_pool",
        app_settings.dpdk.mempool_size,
        app_settings.dpdk.mempool_cache_size,
        0,
        app_settings.dpdk.mbuf_size + RTE_PKTMBUF_HEADROOM,
        rte_socket_id()
    );
    if (mbuf_pool == NULL) {
        rte_exit(-rte_errno, "ERROR: rte_pktmbuf_pool_create() failed.\n");
    }

    // Setup device for RX.
    ret = device_setup(mbuf_pool, &app_settings.dpdk, true, false);
    if (ret != 0) {
        cleanup(app_settings.dpdk.ports);
        rte_exit(EXIT_FAILURE, "ERROR: device_setup() failed.\n");
    }

    srand(time(NULL));

    // Save start time for stats printing.
    gettimeofday(&current_time_raw, NULL);
    uint64_t start_time = current_time_raw.tv_sec * 1000 + current_time_raw.tv_usec / 1000;

    while (!stop_loop() && (app_settings.rules == 0 || rules_inserted < app_settings.rules)) {
        gettimeofday(&current_time_raw, NULL);
        current_time = current_time_raw.tv_sec * 1000 + current_time_raw.tv_usec / 1000;

        // Set the first measurement time to current time.
        if (measurement_time == 0)
            measurement_time = current_time;

        // Print stats every "next_stats_delay_ms" milliseconds.
        if (measurement_time <= current_time) {
            printf("Rules inserted: %lu, failed: %lu, time[ms]: %lu\n", rules_inserted, rules_failed, measurement_time - start_time);
            measurement_time = current_time + app_settings.next_stats_delay_ms;
        }

        // Create flow rule based on the key type specified in arguments.
        switch (app_settings.key) {
            case 1:
                flow = create_rule_key1(0, app_settings.group, app_settings.gen_rand, &error);
                break;
            case 3:
                flow = create_rule_key3(0, app_settings.group, app_settings.gen_rand, &error);
                break;
            default:
                cleanup(app_settings.dpdk.ports);
                rte_exit(EXIT_FAILURE, "ERROR: Key %u is not yet supported\n", app_settings.key);
        }

        if (!flow) {
            rules_failed++;
            if (app_settings.failure_exit) {
            rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: Creating rule failed: %s (%d).\n", error.message, error.type);
            break;
            }
        } else {
            rules_inserted++;
        }
    }

    // Print final stats.
    printf("Finished inserting rules. \n");
    gettimeofday(&current_time_raw, NULL);
    uint64_t stop_time = current_time_raw.tv_sec * 1000 + current_time_raw.tv_usec / 1000;
    printf("Rules inserted: %lu, failed: %lu, time[ms]: %lu\n", rules_inserted, rules_failed, stop_time - start_time);

    destroy_all_flow_rules(0);
    cleanup(app_settings.dpdk.ports);

    return 0;
}
