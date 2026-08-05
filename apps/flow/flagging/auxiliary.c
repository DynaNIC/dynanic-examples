/* auxiliary.c: Provides functions to configure hardware offload flow rules targeting IPv4 traffic
*               with MARK and FLAG offload actions, along with runtime setting initialization.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, August 2026
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "auxiliary.h"

/* --------------------------------------------- */
/* MARK packets with DST IP 128.0.0.1 using ID 5 */
/* --------------------------------------------- */
void create_mark_rule(uint16_t portid, uint32_t q_id, struct rte_flow_error *error) {
    /* Pattern specification. */
    struct rte_flow_item_ipv4 ipv4 = {0};
    struct rte_flow_item_ipv4 ipv4_mask = {0};
    ipv4.hdr.dst_addr = inet_addr("128.0.0.1");
    ipv4_mask.hdr.dst_addr = inet_addr("255.255.255.255");

    /* Match IP header. */
    struct rte_flow_item pattern[] = {
        {
            .type = RTE_FLOW_ITEM_TYPE_IPV4,
            .spec = &ipv4,
            .last = NULL,
            .mask = &ipv4_mask,
        },
        {
            .type = RTE_FLOW_ITEM_TYPE_END,
        },
    };

    /* Create the queue and mark action. */
    struct rte_flow_action_queue queue = { .index = q_id };
    struct rte_flow_action_mark mark = { .id = 5 };
    struct rte_flow_action actions[] = {
        {
            .type = RTE_FLOW_ACTION_TYPE_MARK,
            .conf  = &mark,
        },
        {
            .type = RTE_FLOW_ACTION_TYPE_QUEUE,
            .conf  = &queue,
        },
        {
            .type = RTE_FLOW_ACTION_TYPE_END,
        }
    };

    struct rte_flow_attr attr = { .ingress = 1, .priority = 0 };
    struct rte_flow *flow = NULL;

    /* Validate and create the flow rule. */
    flow = rte_flow_create(portid, &attr, pattern, actions, error);
}

/* ---------------------------------- */
/* FLAG packets with DST IP 128.0.0.1 */
/* ---------------------------------- */
void create_flag_rule(uint16_t portid, uint32_t q_id, struct rte_flow_error *error) {
    /* Pattern specification. */
    struct rte_flow_item_ipv4 ipv4 = {0};
    struct rte_flow_item_ipv4 ipv4_mask = {0};
    ipv4.hdr.dst_addr = inet_addr("128.0.0.1");
    ipv4_mask.hdr.dst_addr = inet_addr("255.255.255.255");

    /* Match IP header. */
    struct rte_flow_item pattern[] = {
        {
            .type = RTE_FLOW_ITEM_TYPE_IPV4,
            .spec = &ipv4,
            .last = NULL,
            .mask = &ipv4_mask,
        },
        {
            .type = RTE_FLOW_ITEM_TYPE_END,
        },
    };

    /* Create the queue and flag action. */
    struct rte_flow_action_queue queue = { .index = q_id };
    struct rte_flow_action actions[] = {
        {
            .type = RTE_FLOW_ACTION_TYPE_FLAG,
            .conf  = NULL,
        },
        {
            .type = RTE_FLOW_ACTION_TYPE_QUEUE,
            .conf  = &queue,
        },
        {
            .type = RTE_FLOW_ACTION_TYPE_END,
        }
    };

    struct rte_flow_attr attr = { .ingress = 1, .priority = 0 };
    struct rte_flow *flow = NULL;

    /* Validate and create the flow rule. */
    flow = rte_flow_create(portid, &attr, pattern, actions, error);
}

static void print_help() {
    printf("Application-specific-options:\n");
    printf("\t -m, --use-mark = If true use mark else use flag.\n");
    printf("\t -p, --port-id <port-id> = Select port on which rules should be created.\n");
    printf("\t -i, --queue-id <q-id> = Select queue on which rules should be created.\n");
    printf("\t -q, --rxq <q-id> = How many queues will be started.\n");
    printf("\t -d, --debug = Print debug messages.\n");
}

/* Parse custum arguments. */
int parse_custom_args(
    int argc,
    char *argv[],
    struct application_settings *app_settings
) {
    int opt;
    int option_index = 0;
    static struct option long_options[] = {
        {"use-mark", no_argument, 0, 'm'},
        {"rxq", required_argument, 0, 'q'},
        {"queue-id", required_argument, 0, 'i'},
        {"port-id", required_argument, 0, 'p'},
        {"debug", no_argument, 0, 'd'},
    };

    /* Setup default options. */
    app_settings->use_mark = false;
    app_settings->q_id = 0;
    app_settings->rx_queues = 1;
    app_settings->port_id = 0;
    app_settings->debug = false;

    while ((opt = getopt_long(argc, argv, "mq:p:i:dh" , long_options, &option_index)) != -1) {
        switch (opt) {
            case 'm':
                app_settings->use_mark = true;
                break;

            case 'i':
                app_settings->q_id = atoi(optarg);
                break;

            case 'q':
                app_settings->rx_queues = atoi(optarg);
                break;

            case 'p':
                app_settings->port_id = atoi(optarg);
                break;

            case 'd':
                app_settings->debug = true;
                break;

            case 'h':
                print_help();
                return -1;
                break;

            default:
                rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: Option %c not supported.\n", optopt);
                print_help();
                return -1;
                break;
        }
    }

    if (app_settings->use_mark) {
        rte_log(RTE_LOG_INFO, RTE_LOGTYPE_EAL, "INFO: Mark action will be used.\n");
    } else {
        rte_log(RTE_LOG_INFO, RTE_LOGTYPE_EAL, "INFO: Flag action will be used.\n");
    }

    return 0;
}
