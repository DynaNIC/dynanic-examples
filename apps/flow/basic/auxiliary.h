/* auxiliary.h: Header file declaring helper function prototypes for basic RTE Flow
*               rule creation. Provides function declarations for configuring
*               hardware-offloaded flow rules based on destination IPv4 address parity
*               (QUEUE action for even IPs, DROP action for odd IPs).
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, August 2026
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_BASIC_AUX__
#define __DYNANIC_BASIC_AUX__

#include <rte_ethdev.h>

#include <getopt.h>

void create_even_rule(uint16_t portid, uint32_t q_id, struct rte_flow_error *error);
void create_odd_rule(uint16_t portid, uint32_t q_id, struct rte_flow_error *error);

#endif //__DYNANIC_BASIC_AUX__
