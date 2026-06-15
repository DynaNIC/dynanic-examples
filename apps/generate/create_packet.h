/* create_packet.h: Packet creating for generate example.
* Copyright (C) DynaNIC Semiconductors, Ltd. - All Rights Reserved
* SPDX-License-Identifier: BSD-3-Clause
* Author: Pavlina Patova <patova@dyna-nic.com>, November 2024
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#ifndef __DYNANIC_GENERATE_CREATE_PACKET__
#define __DYNANIC_GENERATE_CREATE_PACKET__

#include <rte_ethdev.h>

int create_packet(char* new_packet, uint16_t packet_len);

#endif // __DYNANIC_GENERATE_CREATE_PACKET__
