/* auxiliary.c: Common functions used in the lpm example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, April 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "auxiliary.h"

/* Free allocated resources. */
void cleanup() {
    /* Stop device on each started port and end DPDK enviroment. */
    rte_eth_dev_stop(0);
    rte_eal_cleanup();
}

/* Init all necessary components. */
int init(
    int argc,
    char *argv[]
) {
    int ret;

    /* DPDK enviroment init. */
    ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_eal_init() failed.\n");
        return ret;
    }
    argc -= ret;
    argv += ret;

    return 0;
}

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
    struct rte_eth_conf port_conf,
    uint16_t queues
) {
    uint16_t portid = 0;
    uint16_t queueid = 0;
    int ret;
    uint64_t descriptors = 1024;

    rte_log(RTE_LOG_INFO, RTE_LOGTYPE_EAL, "INFO: Setup nb_queues: %" PRIu16 " for port %" PRIu16 "\n", queues, portid);

    ret = port_init(portid, rx_mbuf_pool, queues, &port_conf);
    if (ret != 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: port_init(%u) failed.\n", portid);
        return ret;
    }

    for (int i = 0; i < queues; i++) {

        ret = rte_eth_rx_queue_setup(
            portid,
            i,
            descriptors,
            rte_eth_dev_socket_id(portid),
            NULL,
            rx_mbuf_pool
        );
        if (ret < 0) {
            rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_eth_rx_queue_setup() for queue %d and port %d failed\n", i, portid);
            return ret;
        }
    }

    /* Ports start and enable promiscuous mode. */
    ret = rte_eth_dev_start(portid);
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_EAL, "ERROR: rte_eth_dev_start() failed for port %" PRIu16 "\n", portid);
        return ret;
    }

    rte_eth_promiscuous_enable(0);

    rte_log(RTE_LOG_INFO , RTE_LOGTYPE_EAL, "INFO: Start port: %" PRIu16 "\n", portid);

    return 0;
}
