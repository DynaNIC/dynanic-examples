/* set.h: Funtions and structures for keeping record of received marked packets.
    Each recorded mark_id has assigned the total number of packets with this mark id.
    The functions also handle automatic storage reallocation.
* Copyright (C) 2026 DynaNIC Semiconductors, Ltd. - All Rights Reserved
* Author: Pavlina Patova <patova@dyna-nic.com>, August 2026
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef __DYNANIC_FLAGGING_SET__
#define __DYNANIC_FLAGGING_SET__

struct mark_stat {
    uint16_t mark_id;
    uint16_t pkt_cnt;
};

struct mark_set {
    struct mark_stat* mark_stat;
    int max_size;
    int current_size;
};

struct mark_set *set_create();
int set_add(struct mark_set *mark_set, uint16_t mark_id, uint16_t pkt_cnt);
void print_set(struct mark_set *mark_set);
void set_destroy(struct mark_set *mark_set);

#endif // __DYNANIC_FLAGGING_SET__
