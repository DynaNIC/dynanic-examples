/* common_dpdk.c: Common functions used in the DynaNIC examples.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Jan Privara <privara@dyna-nic.com>, November 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "common_dpdk.h"

#include <string.h>

struct worker_job_data_t
{
    int worker_id;
    int start_index;
    int end_index;
    int queues_per_lcore;
    int remaining_queues;
    void *app_setts;
    struct rte_mempool *mbuf_pool;
};

static struct worker_job_data_t workers_d;

int get_worker_cores_cnt()
{
    return rte_lcore_count() - 1;
}

int check_worker_cores_cnt() {
    if (get_worker_cores_cnt() < 1) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: No cores defined for forwarding.\n");
        return -1;
    }

    return 0;
}

int get_queues_per_lcore(struct dpdk_settings *dpdk_setts)
{
    return (dpdk_setts->queues_per_port * dpdk_setts->ports) / (get_worker_cores_cnt());
}

int get_remaining_queues(struct dpdk_settings *dpdk_setts)
{
    return (dpdk_setts->queues_per_port * dpdk_setts->ports) % (get_worker_cores_cnt());
}

void workers_job_init(struct dpdk_settings *dpdk_setts, void *app_setts, struct rte_mempool *mbuf_pool)
{
    workers_d.worker_id = 0;
    workers_d.start_index = 0;
    workers_d.end_index = 0;
    workers_d.queues_per_lcore = get_queues_per_lcore(dpdk_setts);
    workers_d.remaining_queues = get_remaining_queues(dpdk_setts);
    workers_d.app_setts = app_setts;
    workers_d.mbuf_pool = mbuf_pool;
}

bool workers_job_assign(unsigned int lcore_id, struct lcore_arguments *lcore_args)
{
    workers_d.end_index += workers_d.worker_id < workers_d.remaining_queues ?
                            workers_d.queues_per_lcore + 1 :
                            workers_d.queues_per_lcore;

    if (workers_d.end_index - workers_d.start_index == 0) {
        return false;
    }

    rte_log(RTE_LOG_INFO, RTE_LOGTYPE_EAL, "Queues for lcore %d: %d (%d to %d)\n",
        lcore_id, workers_d.end_index - workers_d.start_index,
        workers_d.start_index, workers_d.end_index - 1);

    lcore_args->app_settings = workers_d.app_setts;
    lcore_args->mbuf_pool = workers_d.mbuf_pool;
    lcore_args->worker_id = workers_d.worker_id;
    lcore_args->start_index = workers_d.start_index;
    lcore_args->end_index = workers_d.end_index;

    workers_d.start_index = workers_d.end_index;
    workers_d.worker_id++;

    return true;
}

int port_init(
    uint16_t port,
    uint16_t rx_queues,
    uint16_t tx_queues,
    struct rte_eth_conf *port_conf
) {
    if (!rte_eth_dev_is_valid_port(port))
        return -1;

    int ret = rte_eth_dev_configure(port, rx_queues, tx_queues, port_conf);
    if (ret != 0)
        return ret;

    return 0;
}

int device_setup(
    struct rte_mempool *mbuf_pool,
    struct dpdk_settings *dpdk_settings,
    bool rx_en,
    bool tx_en
) {
    int ret;

    uint16_t rx_queues = rx_en ? dpdk_settings->queues_per_port : 0;
    uint16_t tx_queues = tx_en ? dpdk_settings->queues_per_port : 0;

    for (uint16_t port_id = 0; port_id < dpdk_settings->ports; port_id++) {
        struct rte_eth_dev_info dev_info;
        struct rte_eth_txconf txconf;

        // Get informations about device.
        ret = rte_eth_dev_info_get(port_id, &dev_info);
        if (ret != 0) {
            rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_eth_dev_info_get() faild for port %u: %s\n",
                    port_id,
                    strerror(-ret)
            );

            return ret;
        }

        if (dev_info.max_tx_queues < dpdk_settings->queues_per_port) {
            rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: dev_info.max_tx_queues(%d) < queues_per_port(%d) for Port %d\n", dev_info.max_tx_queues, dpdk_settings->queues_per_port, port_id);

            return -1;
        }

        rte_log(RTE_LOG_INFO, RTE_LOGTYPE_EAL, "INFO: Setup nb_queues: %" PRIu16 " for port %" PRIu16 "\n", dpdk_settings->queues_per_port, port_id);

        // Init port
        ret = port_init(port_id, rx_queues, tx_queues, &dpdk_settings->port_conf);
        if (ret != 0) {
            rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: port_init(%u) failed.\n", port_id);
            return ret;
        }

        txconf = dev_info.default_txconf;
        txconf.offloads = dpdk_settings->port_conf.txmode.offloads;

        // Start all queues.
        for (uint16_t queue_id = 0; queue_id < dpdk_settings->queues_per_port; queue_id++) {
            if (rx_en) {
                // RX queue setup.
                ret = rte_eth_rx_queue_setup(
                    port_id,
                    queue_id,
                    dpdk_settings->descriptors,
                    rte_eth_dev_socket_id(port_id),
                    NULL,
                    mbuf_pool
                );
                if (ret < 0) {
                    rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_eth_rx_queue_setup() for queue %d and port %d failed\n", queue_id, port_id);
                    return ret;
                }
            }

            if (tx_en) {
                // TX queue setup.
                ret = rte_eth_tx_queue_setup(
                    port_id,
                    queue_id,
                    dpdk_settings->descriptors,
                    rte_eth_dev_socket_id(port_id),
                    &txconf
                );
                if (ret < 0) {
                    rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_eth_tx_queue_setup() for queue %d and port %d failed.\n", queue_id, port_id);
                    return ret;
                }
            }
        }

        // Start port
        ret = rte_eth_dev_start(port_id);
        if (ret < 0) {
            rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_eth_dev_start() failed for port %" PRIu16 "\n", port_id);
            return ret;
        }

        rte_log(RTE_LOG_INFO , RTE_LOGTYPE_EAL, "INFO: Start port: %" PRIu16 "\n", port_id);
    }

    return 0;
}

void cleanup(uint16_t started_ports)
{
    // Stop device on each started port and end DPDK enviroment.
    for (int i = 0; i < started_ports; i++) {
        rte_eth_dev_stop(i);
    }

    rte_eal_cleanup();
}
