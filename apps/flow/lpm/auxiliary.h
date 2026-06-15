/* auxiliary.h: Common functions used in the lpm example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, April 2025
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_LPM_AUX__
#define __DYNANIC_LPM_AUX__

#include <rte_ethdev.h>

#define QUEUES 3

void cleanup();

int init(
    int argc,
    char *argv[]
);

int port_init(
	uint16_t port,
	struct rte_mempool *mbuf_pool,
	uint16_t nb_rx_queues,
	struct rte_eth_conf *port_conf
);

void print_simple_stats();

int rx_device_setup(
	struct rte_mempool *rx_mbuf_pool,
	struct rte_eth_conf port_conf,
	uint16_t queues
);

#endif //__DYNANIC_LPM_AUX__
