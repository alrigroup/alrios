/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARAPISTOCK_DB_H
#define ARAPISTOCK_DB_H

#include <stdint.h>
#include <stddef.h>

#define MAX_ITEMS 512
#define MAX_MOVEMENTS 1024

typedef struct {
    char id[64];
    char sku[64];
    char company_id[64];
    char name[128];
    char category[64];
    int quantity;
    int min_quantity;
    double unit_cost;
    char location[64];
    char updated_at[32];
} StockItem;

typedef struct {
    uint64_t id;
    char item_id[64];
    char type[16]; // "IN", "OUT", "ADJUST"
    int qty_change;
    char reason[128];
    char user[64];
    char timestamp[32];
} StockMovement;

int arapistock_db_init(const char *data_dir);

// Inventory Items
char* arapistock_db_list_items_json(const char *caller_company, int is_master);
int arapistock_db_create_item(const char *sku, const char *company_id, const char *name, const char *category, int qty, int min_qty, double cost, const char *location);
int arapistock_db_record_movement(const char *item_id, const char *type, int qty_change, const char *reason, const char *user, const char *caller_company, int is_master);

// Summary & Valuation
char* arapistock_db_get_summary_json(const char *caller_company, int is_master);

void arapistock_db_close(void);

#endif /* ARAPISTOCK_DB_H */
