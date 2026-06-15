/* part3.h: Define flow rules for flow example part3.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, December 2024
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_FLOW_RULE_PART3__
#define __DYNANIC_FLOW_RULE_PART3__

#include <rte_flow.h>

struct rte_flow* create_part3_example(uint16_t portid, struct rte_flow_error *error);
struct rte_flow* create_tag_jump(uint16_t portid, struct rte_flow_error *error);
struct rte_flow* create_match_tag(uint16_t portid, struct rte_flow_error *error);

#endif // __DYNANIC_FLOW_RULE_PART3__
