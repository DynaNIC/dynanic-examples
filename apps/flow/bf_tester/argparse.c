/* argparse.c: Argument parser for the bf_tester application.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, November 2026
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "argparse.h"
#include "common_args.h"

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_ip.h>

/* Modes in measurement order; each one adds to the previous. */
static const struct bf_mode_info bf_mode_table[BF_MODE_MAX] = {
    [BF_MODE_SINGLE] = {
        .id = BF_MODE_SINGLE, .name = "single", .implemented = true,
        .desc = "baseline: one eth/ipv4/tcp rule -> RSS to hairpin queues",
    },
    [BF_MODE_PER_FLOW] = {
        .id = BF_MODE_PER_FLOW, .name = "per-flow", .implemented = true,
        .desc = "one exact 5-tuple rule per flow -> RSS to hairpin queues",
        .per_flow = true,
    },
    [BF_MODE_EDIT_MAC] = {
        .id = BF_MODE_EDIT_MAC, .name = "edit_mac", .implemented = true,
        .desc = "per-flow + set the source MAC to the port MAC",
        .per_flow = true, .set_mac = true,
    },
    [BF_MODE_INSERT] = {
        .id = BF_MODE_INSERT, .name = "insert", .implemented = true,
        .desc = "edit_mac + push VLAN 3",
        .per_flow = true, .set_mac = true, .push_vlan = true,
    },
    [BF_MODE_COUNT] = {
        .id = BF_MODE_COUNT, .name = "count", .implemented = false,
        .desc = "insert + counter per rule",
        .per_flow = true, .set_mac = true, .push_vlan = true, .count = true,
    },
    [BF_MODE_METER] = {
        .id = BF_MODE_METER, .name = "meter", .implemented = false,
        .desc = "count + meter per rule",
        .per_flow = true, .set_mac = true, .push_vlan = true, .count = true, .nb_meters = 1,
    },
    [BF_MODE_ALL] = {
        .id = BF_MODE_ALL, .name = "all", .implemented = false,
        .desc = "insert + counter and two meters per rule",
        .per_flow = true, .set_mac = true, .push_vlan = true, .count = true, .nb_meters = 2,
    },
};

/* Long-only options. */
enum {
    OPT_SRC_IP = 256,
    OPT_DST_IP,
    OPT_SRC_PORT,
    OPT_DST_PORT,
    OPT_TABLES,
    OPT_CHECK_MISS,
    OPT_INSERT_ONLY,
    OPT_NO_STATS,
    OPT_SYNC,
    OPT_EGRESS_VLAN,
    OPT_FLOW_QUEUE_SIZE,
    OPT_PUSH_BURST,
};

static int parse_u64(const char *s, uint64_t *out)
{
    char *end;

    errno = 0;
    unsigned long long v = strtoull(s, &end, 0);
    if (errno != 0 || end == s || *end != '\0')
        return -1;
    *out = v;
    return 0;
}

/*
 * Parse "BASE[,COUNT[,STEP]]" where BASE is an IPv4 address (is_ip) or
 * a number. COUNT and STEP default to 1. The last generated value must
 * still fit into the field (max_val).
 */
static int parse_field(const char *arg, bool is_ip, uint64_t max_val, struct bf_field_gen *f)
{
    char buf[64];
    char *tok, *save = NULL;
    uint64_t v;

    if (strlen(arg) >= sizeof(buf))
        return -1;
    strcpy(buf, arg);

    tok = strtok_r(buf, ",", &save);
    if (tok == NULL)
        return -1;
    if (is_ip) {
        struct in_addr a;
        if (inet_pton(AF_INET, tok, &a) != 1)
            return -1;
        f->base = ntohl(a.s_addr);
    } else {
        if (parse_u64(tok, &v) < 0 || v > max_val)
            return -1;
        f->base = (uint32_t)v;
    }

    f->count = 1;
    f->step = 1;

    tok = strtok_r(NULL, ",", &save);
    if (tok != NULL) {
        if (parse_u64(tok, &v) < 0 || v == 0 || v > max_val + 1)
            return -1;
        f->count = (uint32_t)v;

        tok = strtok_r(NULL, ",", &save);
        if (tok != NULL) {
            if (parse_u64(tok, &v) < 0 || v == 0 || v > max_val)
                return -1;
            f->step = (uint32_t)v;
        }
    }

    if (strtok_r(NULL, ",", &save) != NULL)
        return -1;

    /* Check the last value does not overflow the header field. */
    if ((uint64_t)f->base + (uint64_t)(f->count - 1) * f->step > max_val)
        return -1;

    return 0;
}

static const struct bf_mode_info *find_mode(const char *name)
{
    for (int i = 0; i < BF_MODE_MAX; i++)
        if (strcmp(bf_mode_table[i].name, name) == 0)
            return &bf_mode_table[i];
    return NULL;
}

int parse_custom_args(int argc, char *argv[], struct application_settings *app_settings)
{
    int opt;
    uint64_t v;
    static const struct option long_options[] = {
        {"mode",            required_argument, 0, 'M'},
        {"flows",           required_argument, 0, 'n'},
        {"src-ip",          required_argument, 0, OPT_SRC_IP},
        {"dst-ip",          required_argument, 0, OPT_DST_IP},
        {"src-port",        required_argument, 0, OPT_SRC_PORT},
        {"dst-port",        required_argument, 0, OPT_DST_PORT},
        {"tables",          required_argument, 0, OPT_TABLES},
        {"hp-queues",       required_argument, 0, 'H'},
        {"check-miss",      no_argument,       0, OPT_CHECK_MISS},
        {"insert-only",     no_argument,       0, OPT_INSERT_ONLY},
        {"no-stats",        no_argument,       0, OPT_NO_STATS},
        {"sync",            no_argument,       0, OPT_SYNC},
        {"egress-vlan",     no_argument,       0, OPT_EGRESS_VLAN},
        {"stats-interval",  required_argument, 0, 't'},
        {"flow-queue-size", required_argument, 0, OPT_FLOW_QUEUE_SIZE},
        {"push-burst",      required_argument, 0, OPT_PUSH_BURST},
        {"help",            no_argument,       0, 'h'},
        {0,0,0,0}
    };

    int ret = set_default_dpdk_setts(&app_settings->dpdk);
    if (ret < 0)
        return -2;

    /* Defaults: hairpin as in bf_tester/config.md, tuple bases as in the
     * Spirent stream block (only the counts need to be given). */
    app_settings->dpdk.queues_per_port = 1;
    app_settings->mode = &bf_mode_table[BF_MODE_SINGLE];
    app_settings->nb_hp_queues = 8;
    app_settings->nb_flows = 0;
    app_settings->nb_tables = 1;
    app_settings->src_ip   = (struct bf_field_gen){ RTE_IPV4(192, 85, 1, 2), 1, 1 };
    app_settings->dst_ip   = (struct bf_field_gen){ RTE_IPV4(192, 0, 0, 1), 1, 1 };
    app_settings->src_port = (struct bf_field_gen){ 1024, 1, 1 };
    app_settings->dst_port = (struct bf_field_gen){ 1024, 1, 1 };
    app_settings->check_miss = false;
    app_settings->flow_queue_size = 1024;
    app_settings->push_burst = 64;
    app_settings->stats_interval_ms = 1000;
    app_settings->insert_only = false;
    app_settings->no_stats = false;
    app_settings->sync = false;
    app_settings->egress_vlan = false;

    while ((opt = getopt_long(argc, argv, "M:n:H:t:h", long_options, NULL)) != -1) {
        switch (opt) {
        case 'M':
            app_settings->mode = find_mode(optarg);
            if (app_settings->mode == NULL) {
                printf("ERROR: Unknown mode '%s'. See --help.\n", optarg);
                return -1;
            }
            break;
        case 'n':
            if (parse_u64(optarg, &v) < 0 || v == 0)
                goto bad_value;
            app_settings->nb_flows = v;
            break;
        case OPT_SRC_IP:
            if (parse_field(optarg, true, UINT32_MAX, &app_settings->src_ip) < 0)
                goto bad_value;
            break;
        case OPT_DST_IP:
            if (parse_field(optarg, true, UINT32_MAX, &app_settings->dst_ip) < 0)
                goto bad_value;
            break;
        case OPT_SRC_PORT:
            if (parse_field(optarg, false, UINT16_MAX, &app_settings->src_port) < 0)
                goto bad_value;
            break;
        case OPT_DST_PORT:
            if (parse_field(optarg, false, UINT16_MAX, &app_settings->dst_port) < 0)
                goto bad_value;
            break;
        case OPT_TABLES:
            if (parse_u64(optarg, &v) < 0 || v == 0 || v > BF_MAX_TABLES_ARG || (v & (v - 1)) != 0)
                goto bad_value;
            app_settings->nb_tables = (uint16_t)v;
            break;
        case 'H':
            if (parse_u64(optarg, &v) < 0 || v == 0 || v > UINT16_MAX)
                goto bad_value;
            app_settings->nb_hp_queues = (uint16_t)v;
            break;
        case OPT_CHECK_MISS:
            app_settings->check_miss = true;
            break;
        case OPT_INSERT_ONLY:
            app_settings->insert_only = true;
            break;
        case OPT_NO_STATS:
            app_settings->no_stats = true;
            break;
        case OPT_SYNC:
            app_settings->sync = true;
            break;
        case OPT_EGRESS_VLAN:
            app_settings->egress_vlan = true;
            break;
        case 't':
            if (parse_u64(optarg, &v) < 0 || v == 0)
                goto bad_value;
            app_settings->stats_interval_ms = v * 1000;
            break;
        case OPT_FLOW_QUEUE_SIZE:
            if (parse_u64(optarg, &v) < 0 || v == 0 || v > UINT32_MAX)
                goto bad_value;
            app_settings->flow_queue_size = (uint32_t)v;
            break;
        case OPT_PUSH_BURST:
            if (parse_u64(optarg, &v) < 0 || v == 0 || v > UINT32_MAX)
                goto bad_value;
            app_settings->push_burst = (uint32_t)v;
            break;
        case 'h':
            print_help();
            rte_eal_cleanup();
            exit(EXIT_SUCCESS);
        default:
            print_help();
            return -1;
        }
    }

    if (!app_settings->mode->implemented) {
        printf("ERROR: Mode '%s' (%s) is not implemented yet.\n",
               app_settings->mode->name, app_settings->mode->desc);
        return -1;
    }
    if (app_settings->nb_tables > 1 && !app_settings->mode->per_flow) {
        printf("ERROR: --tables needs a per-flow mode.\n");
        return -1;
    }
    if (app_settings->no_stats && app_settings->check_miss) {
        printf("ERROR: --check-miss needs statistics, it cannot be used with --no-stats.\n");
        return -1;
    }
    if (app_settings->egress_vlan && !app_settings->mode->push_vlan) {
        printf("ERROR: --egress-vlan needs a mode that pushes a VLAN (insert and later).\n");
        return -1;
    }
    if (app_settings->egress_vlan && !app_settings->sync) {
        printf("ERROR: --egress-vlan is implemented only with --sync.\n");
        return -1;
    }
    if (app_settings->push_burst > app_settings->flow_queue_size) {
        printf("ERROR: --push-burst must not exceed --flow-queue-size.\n");
        return -1;
    }

    return 0;

bad_value:
    printf("ERROR: Invalid option value '%s'. See --help.\n", optarg);
    return -1;
}

void print_help(void)
{
    printf("Usage: ./bf_tester [EAL options] -- [Application options]\n");
    printf("\nThe async rule API needs HW steering on the NIC, e.g.: -a 0000:21:00.0,dv_flow_en=2\n");
    printf("Every worker lcore (-l minus the main lcore) inserts rules in parallel.\n");

    printf("\nMode (each mode includes everything from the previous one):\n");
    printf("\t -M, --mode <name>              = Measurement mode (default: single).\n");
    for (int i = 0; i < BF_MODE_MAX; i++)
        printf("\t       %-9s %s%s\n", bf_mode_table[i].name, bf_mode_table[i].desc,
               bf_mode_table[i].implemented ? "" : " [not implemented yet]");

    printf("\nFlows (all modes except single), each field is <base[,count[,step]]> like a Spirent\n");
    printf("modifier; all fields step with every flow and each wraps at its own count:\n");
    printf("\t     --src-ip <ip,...>          = Source IPv4 (default: 192.85.1.2).\n");
    printf("\t     --dst-ip <ip,...>          = Destination IPv4 (default: 192.0.0.1).\n");
    printf("\t     --src-port <port,...>      = TCP source port (default: 1024).\n");
    printf("\t     --dst-port <port,...>      = TCP destination port (default: 1024).\n");
    printf("\t -n, --flows <num>              = Number of rules (default: all distinct flows of the stream).\n");
    printf("\t     --tables <num>             = Spread the rules over this many tables/groups, power of two\n");
    printf("\t                                  up to %d, by the lowest bits of the source IP (default: 1).\n",
           BF_MAX_TABLES_ARG);

    printf("\nHairpin:\n");
    printf("\t -H, --hp-queues <num>          = Hairpin queues the traffic is spread over (default: 8).\n");

    printf("\nRun:\n");
    printf("\t     --check-miss               = Count packets that match no rule (they are dropped)\n");
    printf("\t                                  and report them every interval and at exit.\n");
    printf("\t     --insert-only              = Insert the rules, report the insertion rate and exit.\n");
    printf("\t -t, --stats-interval <secs>    = Seconds between stats lines (default: 1).\n");
    printf("\t     --no-stats                 = Do not read any statistics or counters while forwarding,\n");
    printf("\t                                  just wait for Ctrl+C (numbers come from Spirent).\n");

    printf("\nRule insertion:\n");
    printf("\t     --sync                     = Create rules synchronously with rte_flow_create() (as testpmd)\n");
    printf("\t                                  instead of the async template API. Use it for measurements,\n");
    printf("\t                                  async is only for future reference. Works with SW steering\n");
    printf("\t                                  (default dv_flow_en=1) and with HW steering (dv_flow_en=2).\n");
    printf("\t     --egress-vlan              = Push the VLAN in egress rules (on everything leaving the port)\n");
    printf("\t                                  instead of in the main ingress rules. Modes insert and later,\n");
    printf("\t                                  needs --sync.\n");
    printf("\t     --flow-queue-size <num>    = Async: rule operations in flight per lcore (default: 1024).\n");
    printf("\t     --push-burst <num>         = Async: rule operations sent to HW at once (default: 64).\n");
    printf("\t -h, --help                     = Print this help.\n");
}
