/* bf_tester.c: Hairpin forwarding with large HW-steering rule sets (BlueField / mlx5).
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, November 2026
*
* Traffic received on the port is steered by rte_flow rules to hairpin queues
* and sent back out of the same port entirely in HW. Equivalent testpmd setup:
*
*   dpdk-testpmd -a <pci> ... -- --hairpinq=8 --hairpin-mode=0x12 --hairpin-map=0:16:0:16:8
*   flow create 0 ingress group 0 pattern end actions jump group 1 / end
*   flow create 0 ingress group 1 pattern eth / ipv4 / tcp / end
*        actions count / rss types ipv4-tcp end queues <hairpin queues> end / end
*
* Modes (see --help): single is the setup above without the counter,
* per-flow puts one exact-match 5-tuple rule per flow into group 1 (millions
* of rules inserted in parallel with the async flow API), the further modes
* add packet editing, counters and meters to the per-flow rules.
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "argparse.h"
#include "flow_rules.h"
#include "stats.h"

#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

#define PORT_ID 0

static volatile int g_stop = 0;

static void handle_sig(int sig)
{
    (void)sig;
    g_stop = 1;
}

static void print_tuple(const char *label, const struct application_settings *s, uint64_t idx)
{
    uint32_t sip, dip;
    uint16_t sp, dp;
    char a[INET_ADDRSTRLEN], b[INET_ADDRSTRLEN];
    struct in_addr in;

    bf_flow_tuple(s, idx, &sip, &dip, &sp, &dp);
    in.s_addr = htonl(sip);
    inet_ntop(AF_INET, &in, a, sizeof(a));
    in.s_addr = htonl(dip);
    inet_ntop(AF_INET, &in, b, sizeof(b));
    printf("  %s #%" PRIu64 ": %s:%u -> %s:%u tcp\n", label, idx, a, sp, b, dp);
}

/* Resolve the number of flows and check the generator gives distinct tuples. */
static int resolve_flows(struct application_settings *s)
{
    if (!s->mode->per_flow)
        return 0;

    uint64_t distinct = bf_flow_distinct_tuples(s);
    if (s->nb_flows == 0)
        s->nb_flows = distinct;
    if (s->nb_flows > distinct) {
        printf("ERROR: %" PRIu64 " flows requested but the fields give only %" PRIu64
               " distinct flows (the stream repeats after lcm of the counts).\n",
               s->nb_flows, distinct);
        return -1;
    }
    if (s->nb_flows > UINT32_MAX) {
        printf("ERROR: at most %u flows are supported.\n", UINT32_MAX);
        return -1;
    }
    return 0;
}

static void print_config(const struct application_settings *s, uint16_t nb_std)
{
    printf("\n=== bf_tester ===\n");
    printf("Mode:            %s (%s)\n", s->mode->name, s->mode->desc);
    printf("Queues:          %u standard (0-%u), %u hairpin (%u-%u)\n",
           nb_std, nb_std - 1, s->nb_hp_queues, nb_std, nb_std + s->nb_hp_queues - 1);
    printf("Rule API:        %s\n", s->sync ? "sync (rte_flow_create)" : "async (template API, future reference only, not tuned)");
    printf("Statistics:      %s\n", s->no_stats ? "off" : "on");
    printf("Miss check:      %s\n", s->check_miss ? "on" : "off");
    if (s->mode->per_flow) {
        printf("Flows:           %" PRIu64 "\n", s->nb_flows);
        print_tuple("first", s, 0);
        print_tuple("last ", s, s->nb_flows - 1);
    }
    printf("Lcores:          %u (1 main + %u inserting workers)\n\n",
           rte_lcore_count(), rte_lcore_count() - 1);
}

/* Report the port and flow setup: common part first, then the rule API specifics. */
static void print_flow_setup(const struct bf_flow_ctx *ctx, const struct application_settings *s,
                             uint16_t nb_std)
{
    struct rte_eth_dev_info info;
    const uint16_t hp_last = nb_std + s->nb_hp_queues - 1;

    printf("\n=== Flow setup ===\n");
    if (rte_eth_dev_info_get(PORT_ID, &info) == 0)
        printf("Port:            %u (%s), max rxq %u, max txq %u\n",
               PORT_ID, info.driver_name, info.max_rx_queues, info.max_tx_queues);
    if (ctx->hws)
        printf("Steering:        HW (dv_flow_en=2), counters %u used of max %u\n",
               ctx->nb_counters, ctx->max_counters);
    else
        printf("Steering:        SW (dv_flow_en=1)\n");
    /* "group 1" or "groups 1-N" for the matching rules. */
    char groups[32];
    if (ctx->nb_tables == 1)
        snprintf(groups, sizeof(groups), "group 1");
    else
        snprintf(groups, sizeof(groups), "groups 1-%u", ctx->nb_tables);

    if (ctx->nb_tables == 1)
        printf("Rules:           group 0  all -> jump group 1\n");
    else
        printf("Rules:           group 0  src IP & %u == t -> jump group 1+t (%u rules), rest -> group 1\n",
               ctx->nb_tables - 1, ctx->nb_tables);
    /* What the main rules do with a packet, in HW order. */
    char acts[160];
    int len = 0;
    if (s->mode->count)
        len += snprintf(acts + len, sizeof(acts) - len, "count, ");
    if (s->mode->push_vlan && !s->egress_vlan)
        len += snprintf(acts + len, sizeof(acts) - len, "push VLAN %u, ", BF_INSERT_VLAN_ID);
    if (s->mode->set_mac) {
        char mac[RTE_ETHER_ADDR_FMT_SIZE];
        rte_ether_format_addr(mac, sizeof(mac), &ctx->port_mac);
        len += snprintf(acts + len, sizeof(acts) - len, "src MAC := %s, ", mac);
    }
    snprintf(acts + len, sizeof(acts) - len, "RSS to hairpin queues %u-%u", nb_std, hp_last);

    if (s->mode->per_flow)
        printf("                 %s  %" PRIu64 " x tcp 5-tuple -> %s (inserted below)\n",
               groups, s->nb_flows, acts);
    else
        printf("                 %s  eth/ipv4/tcp -> %s\n", groups, acts);
    printf("                 %s  miss -> %s\n", groups, s->check_miss ? "count + drop" : "drop");
    if (s->egress_vlan) {
        printf("                 egress group 0  all -> jump egress group 1\n");
        printf("                 egress group 1  all -> push VLAN %u\n", BF_INSERT_VLAN_ID);
    }
    if (ctx->nb_tables > 1) {
        printf("                 rules per group:");
        for (uint16_t t = 0; t < ctx->nb_tables; t++)
            printf(" %" PRIu64, ctx->table_flows[t]);
        printf("\n");
    }

    if (s->sync) {
        printf("Sync API:\n");
        printf("  Insertion:     rte_flow_create(), %u worker lcore(s) %s\n", ctx->nb_flow_queues,
               ctx->sync_serialize ? "serialized (mlx5 counter cache is not thread-safe)"
                                   : "in parallel");
    } else {
        printf("Async API:\n");
        printf("  Flow queues:   %u (one per worker lcore) x %u rule operations in flight,"
               " push burst %u\n", ctx->nb_cfg_queues, s->flow_queue_size, s->push_burst);
        /* Fill of the fullest main table. */
        double fill = 0;
        for (uint16_t t = 0; t < ctx->nb_tables; t++) {
            uint64_t rules = s->mode->per_flow ? ctx->table_flows[t] : 1;
            fill = RTE_MAX(fill, 100.0 * rules / ctx->table_size[t]);
        }
        printf("  Tables:        %s main %u x %" PRIu64 " slots (max %.0f %% full),"
               " each with a 1-rule miss table, group 0 jump table(s)\n",
               groups, ctx->nb_tables, ctx->table_size[0], fill);
    }
    printf("\n");
}

/* Configure the port with standard and self-peered hairpin queues. */
static int port_setup(uint16_t port, const struct application_settings *s,
                      struct rte_mempool *mp)
{
    struct rte_eth_dev_info info;
    struct rte_eth_hairpin_cap cap;
    struct rte_eth_conf conf = s->dpdk.port_conf;
    const uint16_t nb_std = s->dpdk.queues_per_port;
    const uint16_t nb_hp = s->nb_hp_queues;
    const uint16_t total = nb_std + nb_hp;
    int ret;

    ret = rte_eth_dev_info_get(port, &info);
    if (ret != 0) {
        printf("ERROR: rte_eth_dev_info_get(): %s\n", rte_strerror(-ret));
        return ret;
    }
    if (total > info.max_rx_queues || total > info.max_tx_queues) {
        printf("ERROR: %u queues requested, port supports rx %u / tx %u.\n",
               total, info.max_rx_queues, info.max_tx_queues);
        return -EINVAL;
    }

    ret = rte_eth_dev_hairpin_capability_get(port, &cap);
    if (ret != 0) {
        printf("ERROR: port %u does not support hairpin: %s\n", port, rte_strerror(-ret));
        return ret;
    }
    if (nb_hp > cap.max_nb_queues) {
        printf("ERROR: %u hairpin queues requested, port supports %u.\n", nb_hp, cap.max_nb_queues);
        return -EINVAL;
    }

    if (nb_std > 1) {
        conf.rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;
        conf.rx_adv_conf.rss_conf.rss_hf = RTE_ETH_RSS_IP & info.flow_type_rss_offloads;
    }

    ret = rte_eth_dev_configure(port, total, total, &conf);
    if (ret != 0) {
        printf("ERROR: rte_eth_dev_configure(): %s\n", rte_strerror(-ret));
        return ret;
    }

    for (uint16_t q = 0; q < nb_std; q++) {
        ret = rte_eth_rx_queue_setup(port, q, s->dpdk.descriptors,
                                     rte_eth_dev_socket_id(port), NULL, mp);
        if (ret == 0)
            ret = rte_eth_tx_queue_setup(port, q, s->dpdk.descriptors,
                                         rte_eth_dev_socket_id(port), NULL);
        if (ret != 0) {
            printf("ERROR: standard queue %u setup: %s\n", q, rte_strerror(-ret));
            return ret;
        }
    }

    /* Hairpin Rx queue N is peered with hairpin Tx queue N of the same port
     * (testpmd --hairpin-mode=0x12: manual bind, explicit Tx rules). */
    for (uint16_t q = nb_std; q < total; q++) {
        struct rte_eth_hairpin_conf hp = {
            .peer_count = 1,
            .manual_bind = 1,
            .tx_explicit = 1,
            .peers[0] = { .port = port, .queue = q },
        };

        ret = rte_eth_rx_hairpin_queue_setup(port, q, 0, &hp);    /* 0 = PMD max */
        if (ret == 0)
            ret = rte_eth_tx_hairpin_queue_setup(port, q, 0, &hp);
        if (ret != 0) {
            printf("ERROR: hairpin queue %u setup: %s\n", q, rte_strerror(-ret));
            return ret;
        }
    }

    return 0;
}

/* Sum per-flow counters and report flows that saw no traffic. */
static void query_flow_counters(struct bf_flow_ctx *ctx)
{
    const struct application_settings *s = ctx->set;
    uint64_t hits_total = 0, bytes_total = 0, zero = 0, errors = 0, missing = 0;
    const uint64_t hz = rte_get_timer_hz();
    const uint64_t t0 = rte_get_timer_cycles();

    printf("\nQuerying %" PRIu64 " flow counters...\n", ctx->nb_flows);
    for (uint64_t i = 0; i < ctx->nb_flows; i++) {
        uint64_t hits, bytes;

        if (ctx->flows[i] == NULL) {
            missing++;
            continue;
        }
        if (bf_flow_counter_read(ctx, ctx->flows[i], &hits, &bytes) != 0) {
            errors++;
            continue;
        }
        hits_total += hits;
        bytes_total += bytes;
        if (hits == 0) {
            if (zero < 5)
                print_tuple("no hits", s, i);
            zero++;
        }
    }
    printf("Flow counters (%.2f s): packets %" PRIu64 ", bytes %" PRIu64
           ", flows without hits %" PRIu64 ", not inserted %" PRIu64 ", query errors %" PRIu64 "\n",
           (double)(rte_get_timer_cycles() - t0) / hz,
           hits_total, bytes_total, zero, missing, errors);
}

/* Packets dropped by the miss rules of all groups, BF_STATS_NA without --check-miss. */
static uint64_t miss_or_na(struct bf_flow_ctx *ctx)
{
    uint64_t pkts;

    if (!ctx->set->check_miss || bf_flow_miss_read(ctx, &pkts) != 0)
        return BF_STATS_NA;
    return pkts;
}

static uint64_t counter_or_na(struct bf_flow_ctx *ctx, struct rte_flow *flow)
{
    uint64_t hits;

    if (flow == NULL || bf_flow_counter_read(ctx, flow, &hits, NULL) != 0)
        return BF_STATS_NA;
    return hits;
}

static void traffic_loop(struct bf_flow_ctx *ctx, const struct application_settings *s)
{
    struct bf_stats st;
    struct rte_flow *match = s->mode->count && !s->mode->per_flow ? ctx->single_rule : NULL;

    if (s->no_stats) {
        /* Nothing is read from the NIC; the main lcore just sleeps. */
        printf("\nForwarding via hairpin, statistics disabled. Press Ctrl+C to stop.\n");
        while (!g_stop)
            usleep(100000);
        return;
    }

    if (bf_stats_init(&st, PORT_ID,
                      counter_or_na(ctx, match), miss_or_na(ctx)) != 0) {
        printf("ERROR: cannot initialise statistics.\n");
        return;
    }

    printf("\nForwarding via hairpin. Press Ctrl+C to stop.\n");
    uint64_t next = rte_get_timer_cycles() + s->stats_interval_ms * rte_get_timer_hz() / 1000;
    while (!g_stop) {
        usleep(10000);
        if (rte_get_timer_cycles() < next)
            continue;
        next += s->stats_interval_ms * rte_get_timer_hz() / 1000;
        bf_stats_print(&st, counter_or_na(ctx, match), miss_or_na(ctx));
    }

    bf_stats_summary(&st, counter_or_na(ctx, match), miss_or_na(ctx));
}

int main(int argc, char *argv[])
{
    struct application_settings s;
    struct bf_flow_ctx ctx;
    struct bf_bulk_result res;
    bool port_started = false;
    bool hairpin_bound = false;
    bool ctx_ready = false;
    int exit_code = EXIT_FAILURE;
    int ret;

    /* 1. EAL and application arguments. */
    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "ERROR: rte_eal_init() failed.\n");
    argc -= ret;
    argv += ret;

    if (parse_custom_args(argc, argv, &s) < 0)
        rte_exit(EXIT_FAILURE, "ERROR: failed to parse arguments.\n");
    if (resolve_flows(&s) < 0)
        rte_exit(EXIT_FAILURE, "ERROR: invalid flow generator settings.\n");

    const uint16_t nb_std = s.dpdk.queues_per_port;
    print_config(&s, nb_std);

    if (rte_eth_dev_count_avail() < 1)
        rte_exit(EXIT_FAILURE, "ERROR: no DPDK port available (use -a <pci>,dv_flow_en=2).\n");
    if (rte_eth_dev_count_avail() > 1)
        printf("NOTE: %u ports available, only port %u is used.\n", rte_eth_dev_count_avail(), PORT_ID);

    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);

    /* Mbufs are only needed for the standard queues; hairpin uses NIC memory. */
    struct rte_mempool *mp = rte_pktmbuf_pool_create("bf_tester_pool", s.dpdk.mempool_size,
                                                     s.dpdk.mempool_cache_size, 0,
                                                     s.dpdk.mbuf_size + RTE_PKTMBUF_HEADROOM,
                                                     rte_eth_dev_socket_id(PORT_ID));
    if (mp == NULL)
        rte_exit(EXIT_FAILURE, "ERROR: rte_pktmbuf_pool_create(): %s\n", rte_strerror(rte_errno));

    /* 2. Port with standard and hairpin queues (port still stopped). */
    if (port_setup(PORT_ID, &s, mp) != 0)
        goto out;

    /* 3. Flow engine; must be configured while the port is stopped. */
    uint16_t nb_flow_queues = RTE_MAX(1u, rte_lcore_count() - 1);
    if (bf_flow_engine_configure(&ctx, PORT_ID, &s, nb_std, nb_flow_queues) != 0)
        goto out;
    ctx_ready = true;

    /* 4. Start the port and connect the hairpin Rx queues to the Tx queues. */
    ret = rte_eth_dev_start(PORT_ID);
    if (ret != 0) {
        printf("ERROR: rte_eth_dev_start(): %s\n", rte_strerror(-ret));
        goto out;
    }
    port_started = true;

    /* Spirent sends to a foreign destination MAC. */
    ret = rte_eth_promiscuous_enable(PORT_ID);
    if (ret != 0)
        printf("WARNING: cannot enable promiscuous mode: %s\n", rte_strerror(-ret));

    ret = rte_eth_hairpin_bind(PORT_ID, PORT_ID);
    if (ret != 0) {
        printf("ERROR: rte_eth_hairpin_bind(): %s\n", rte_strerror(-ret));
        goto out;
    }
    hairpin_bound = true;

    /* 5. Rules: tables (async only), fixed rules, then the per-flow rules. */
    if (bf_flow_tables_create(&ctx) != 0 || bf_flow_fixed_rules_insert(&ctx) != 0)
        goto out;
    print_flow_setup(&ctx, &s, nb_std);

    if (s.mode->per_flow) {
        if (bf_flow_bulk_insert(&ctx, &g_stop, &res) != 0)
            goto out;
    }

    /* 6. Forward until Ctrl+C; the HW does the work, the main lcore only reports. */
    if (!s.insert_only && !g_stop)
        traffic_loop(&ctx, &s);

    /* Modes with per-rule counters: report flows that saw no traffic. */
    if (s.mode->per_flow && s.mode->count && !s.insert_only && !s.no_stats)
        query_flow_counters(&ctx);

    /* Per-flow rules are not destroyed one by one; closing the port frees them. */

    exit_code = EXIT_SUCCESS;

out:
    /* 7. Cleanup in reverse order. */
    if (ctx_ready)
        bf_flow_teardown(&ctx);
    if (hairpin_bound)
        rte_eth_hairpin_unbind(PORT_ID, PORT_ID);
    if (port_started)
        rte_eth_dev_stop(PORT_ID);
    rte_eth_dev_close(PORT_ID);
    rte_eal_cleanup();
    return exit_code;
}
