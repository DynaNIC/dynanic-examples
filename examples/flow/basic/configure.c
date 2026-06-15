/* configure.c: Functions for configuring dpdk in flow example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, December 2024
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "configure.h"

/* Network port init. */
int port_init(
    uint16_t port,
    struct rte_mempool *mbuf_pool,
    uint16_t nb_rx_queues,
    struct rte_eth_conf *port_conf
) {
    int ret;
    const uint16_t tx_queues = 0;

    if (!rte_eth_dev_is_valid_port(port))
        return -1;


    ret = rte_eth_dev_configure(port, nb_rx_queues, tx_queues, port_conf);
    if (ret != 0)
        return ret;

    return 0;
}

int rx_device_setup(
    struct rte_mempool *rx_mbuf_pool,
    struct rte_eth_conf* port_conf,
    uint16_t queues,
    struct application_settings *app_settings,
    uint16_t portid
) {
    struct rte_eth_dev_info dev_info;
    int i;
    unsigned lcore_id;
    uint16_t queueid;
    int ret;

    ret = port_init(portid, rx_mbuf_pool, queues, port_conf);
    if (ret != 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: port_init(%u) failed.\n", portid);
        return ret;
    }

    for (queueid = 0; queueid < queues; queueid++) {
        ret = rte_eth_rx_queue_setup(
            portid,
            queueid,
            app_settings->descriptors,
            rte_eth_dev_socket_id(portid),
            NULL,
            rx_mbuf_pool
        );
        if (ret < 0) {
            rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_eth_rx_queue_setup() for queue %d and port %d failed\n", queueid, portid);
            return ret;
        }
    }

    ret = rte_eth_dev_start(portid);
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_eth_dev_start() failed for port %" PRIu16 "\n", i);
        return ret;
    }

    rte_eth_promiscuous_enable(portid);

    if (!app_settings->measurement) {
        rte_log(RTE_LOG_INFO , RTE_LOGTYPE_EAL, "INFO: Start port: %" PRIu16 "\n", portid);
    }

    return 0;
}
