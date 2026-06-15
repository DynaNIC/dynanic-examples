/* configure.h: Functions for configuring dpdk in flow example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, December 2024
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_FLOW_CONFIGURE__
#define __DYNANIC_FLOW_CONFIGURE__

#include <rte_ethdev.h>

#include "argparse.h"

int port_init(
    uint16_t port,
    struct rte_mempool *mbuf_pool,
    uint16_t nb_rx_queues,
    struct rte_eth_conf *port_conf
);

int rx_device_setup(
    struct rte_mempool *rx_mbuf_pool,
    struct rte_eth_conf *port_conf,
    uint16_t queues_pool,
    struct application_settings *app_settings,
    uint16_t portid
);

#endif // __DYNANIC_FLOW_CONFIGURE__
