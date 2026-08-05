/* set.c: Functions for set handling.
* Copyright (C) 2026 DynaNIC Semiconductors, Ltd. - All Rights Reserved
* Author: Pavlina Patova <patova@dyna-nic.com>, August 2026
*
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Proprietary and confidential, additional license terms may apply.
*/

#include "set.h"

struct mark_set *set_create() {
    int initial_set_size = 10;

    struct mark_set *mark_set = (struct mark_set*)malloc(sizeof(struct mark_set));
    if (mark_set == NULL) {
        printf("ERROR: set_init() failed, can not setup mark_set.\n");
        return NULL;
    }

    mark_set->mark_stat = (struct mark_stat*)malloc(initial_set_size * sizeof(struct mark_stat));
    if (mark_set->mark_stat == NULL) {
        printf("ERROR: set_init() failed, can not setup mark_stat with size %d.\n", initial_set_size);
        free(mark_set);
        return NULL;
    }

    mark_set->max_size = initial_set_size;
    mark_set->current_size = 0;

    return mark_set;
}

static int set_add_stat_space(struct mark_set *mark_set) {
    int added_space = 5;

    int new_size = added_space + mark_set->max_size;

    struct mark_stat *tmp = NULL;

    tmp = (struct mark_stat*)realloc(mark_set->mark_stat, new_size * sizeof(struct mark_stat));
    if (tmp == NULL) {
        printf("ERROR: set_add_stat_space() failed, can not setup mark_stat with size %d.\n", new_size);
        return -1;
    }

    mark_set->mark_stat = tmp;
    mark_set->max_size = new_size;

    return 0;
}

int set_add(struct mark_set *mark_set, uint16_t mark_id, uint16_t pkt_cnt) {
    int ret = 0;

    if (mark_set == NULL) {
        printf("ERROR: set was not initialized.\n");
        return -1;
    }

    for (int i = 0; i < mark_set->max_size; i++) {
        struct mark_stat* stat = &(mark_set->mark_stat[i]);

        if (stat->mark_id == mark_id) {
            // Match found add packet count and end.
            stat->pkt_cnt += pkt_cnt;
            return 0;
        }
    }

    // Mark not found. Add it, but first check if there is enough space.
    if (mark_set->max_size == mark_set->current_size) {
        // printf("%d, %d, %d\n", mark_set->max_size, mark_set->current_size, mark_set->max_size == mark_set->current_size);
        ret = set_add_stat_space(mark_set);
        if (ret < 0) {
            return ret;
        }
    }

    mark_set->mark_stat[mark_set->current_size].mark_id = mark_id;
    mark_set->mark_stat[mark_set->current_size].pkt_cnt = pkt_cnt;
    mark_set->current_size++;

    return 0;
}

void print_set(struct mark_set *mark_set) {
    printf("{");
    for (int i = 0; i < mark_set->current_size; i++) {
        if (i != 0)
            printf(",");
        printf("\"ID_%d\": %d", mark_set->mark_stat[i].mark_id, mark_set->mark_stat[i].pkt_cnt);
    }
    printf("}");
}

void set_destroy(struct mark_set *mark_set) {
    if (mark_set == NULL)
        return;

    free(mark_set->mark_stat);
    free(mark_set);
}
