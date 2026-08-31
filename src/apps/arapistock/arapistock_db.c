/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arapistock_db.h"
#include "aros_hal.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

static StockItem g_items[MAX_ITEMS];
static int g_items_count = 0;
static StockMovement g_movements[MAX_MOVEMENTS];
static int g_movements_count = 0;
static uint64_t g_next_movement_id = 1;

static void *g_stock_mutex = NULL;

static void get_iso_now(char *out, size_t max_len) {
    time_t t = time(NULL);
    struct tm tm_buf;
    gmtime_r(&t, &tm_buf);
    strftime(out, max_len, "%Y-%m-%d %H:%M:%S", &tm_buf);
}

static void escape_json_str(const char *in, char *out, size_t max_out) {
    if (!in || !out || max_out < 2) {
        if (out && max_out > 0) out[0] = '\0';
        return;
    }
    size_t j = 0;
    for (size_t i = 0; in[i] && j < max_out - 2; i++) {
        if (in[i] == '\"' || in[i] == '\\') {
            out[j++] = '\\';
        } else if (in[i] == '\n') {
            out[j++] = ' ';
            continue;
        } else if (in[i] == '\r') {
            continue;
        }
        out[j++] = in[i];
    }
    out[j] = '\0';
}

int arapistock_db_init(const char *data_dir) {
    g_stock_mutex = ar_mutex_create();
    if (data_dir && data_dir[0]) {
        mkdir(data_dir, 0755);
    }

    // Bootstrap initial assets & inventory (arapistock_db_create_item will lock mutex internally)
    if (g_items_count == 0) {
        arapistock_db_create_item("SRV-RACK-01", "alrigroup", "Servidor Dell PowerEdge R750 64-Core", "Infraestrutura", 4, 1, 38500.0, "Datacenter Rack A1");
        arapistock_db_create_item("SW-CISCO-48", "alrigroup", "Switch Cisco Catalyst 48P 10GbE", "Redes", 6, 2, 12800.0, "Datacenter Rack B2");
        arapistock_db_create_item("MIC-SHURE-SM7B", "detroitgg", "Microfone Shure SM7B Studio", "Equipamentos Stream", 12, 4, 3200.0, "Estúdio Detroit Sala 01");
        arapistock_db_create_item("HEADSET-LOGI-PRO", "detroitgg", "Headset Logitech G PRO X Wireless", "Periféricos", 25, 5, 1150.0, "Almoxarifado Detroit");
    }

    return 0;
}

int arapistock_db_create_item(const char *sku, const char *company_id, const char *name, const char *category, int qty, int min_qty, double cost, const char *location) {
    if (!sku || !company_id || !name) return -1;
    if (g_stock_mutex) ar_mutex_lock(g_stock_mutex);

    if (g_items_count >= MAX_ITEMS) {
        if (g_stock_mutex) ar_mutex_unlock(g_stock_mutex);
        return -1;
    }

    StockItem *item = &g_items[g_items_count++];
    memset(item, 0, sizeof(StockItem));
    snprintf(item->id, sizeof(item->id), "stk_%03d", g_items_count);
    strncpy(item->sku, sku, sizeof(item->sku) - 1);
    strncpy(item->company_id, company_id, sizeof(item->company_id) - 1);
    strncpy(item->name, name, sizeof(item->name) - 1);
    strncpy(item->category, category ? category : "Geral", sizeof(item->category) - 1);
    item->quantity = qty >= 0 ? qty : 0;
    item->min_quantity = min_qty >= 0 ? min_qty : 1;
    item->unit_cost = cost >= 0.0 ? cost : 0.0;
    strncpy(item->location, location ? location : "Estoque Principal", sizeof(item->location) - 1);
    get_iso_now(item->updated_at, sizeof(item->updated_at));

    if (g_stock_mutex) ar_mutex_unlock(g_stock_mutex);
    return 0;
}

int arapistock_db_record_movement(const char *item_id, const char *type, int qty_change, const char *reason, const char *user, const char *caller_company, int is_master) {
    if (!item_id || !type || qty_change == 0) return -1;
    if (g_stock_mutex) ar_mutex_lock(g_stock_mutex);

    StockItem *target = NULL;
    for (int i = 0; i < g_items_count; i++) {
        if (strcmp(g_items[i].id, item_id) == 0 || strcmp(g_items[i].sku, item_id) == 0) {
            if (!is_master && caller_company && caller_company[0] && strcmp(g_items[i].company_id, caller_company) != 0) {
                if (g_stock_mutex) ar_mutex_unlock(g_stock_mutex);
                return -2; // 403 Forbidden
            }
            target = &g_items[i];
            break;
        }
    }

    if (!target) {
        if (g_stock_mutex) ar_mutex_unlock(g_stock_mutex);
        return -1; // 404 Not Found
    }

    if (strcasecmp(type, "OUT") == 0) {
        if (target->quantity < qty_change) {
            if (g_stock_mutex) ar_mutex_unlock(g_stock_mutex);
            return -3; // 400 Insufficient stock
        }
        target->quantity -= qty_change;
    } else {
        target->quantity += qty_change;
    }
    get_iso_now(target->updated_at, sizeof(target->updated_at));

    // Record audit trail
    if (g_movements_count < MAX_MOVEMENTS) {
        StockMovement *m = &g_movements[g_movements_count++];
        memset(m, 0, sizeof(StockMovement));
        m->id = g_next_movement_id++;
        strncpy(m->item_id, target->id, sizeof(m->item_id) - 1);
        strncpy(m->type, type, sizeof(m->type) - 1);
        m->qty_change = qty_change;
        strncpy(m->reason, reason ? reason : "Movimentação operacional", sizeof(m->reason) - 1);
        strncpy(m->user, user ? user : "system", sizeof(m->user) - 1);
        get_iso_now(m->timestamp, sizeof(m->timestamp));
    }

    if (g_stock_mutex) ar_mutex_unlock(g_stock_mutex);
    return 0;
}

char* arapistock_db_list_items_json(const char *caller_company, int is_master) {
    if (g_stock_mutex) ar_mutex_lock(g_stock_mutex);

    size_t alloc_size = 32768;
    char *buf = (char*)malloc(alloc_size);
    if (!buf) {
        if (g_stock_mutex) ar_mutex_unlock(g_stock_mutex);
        return strdup("[]");
    }

    size_t offset = 0;
    offset += snprintf(buf + offset, alloc_size - offset, "[\n");

    int matched = 0;
    for (int i = 0; i < g_items_count; i++) {
        StockItem *item = &g_items[i];

        if (!is_master && caller_company && caller_company[0]) {
            if (strcmp(item->company_id, caller_company) != 0 && strcmp(item->company_id, "global") != 0) {
                continue;
            }
        }

        if (matched > 0) offset += snprintf(buf + offset, alloc_size - offset, ",\n");

        char esc_name[256];
        escape_json_str(item->name, esc_name, sizeof(esc_name));

        offset += snprintf(buf + offset, alloc_size - offset,
            "  {\n"
            "    \"id\": \"%s\",\n"
            "    \"sku\": \"%s\",\n"
            "    \"company_id\": \"%s\",\n"
            "    \"name\": \"%s\",\n"
            "    \"category\": \"%s\",\n"
            "    \"quantity\": %d,\n"
            "    \"min_quantity\": %d,\n"
            "    \"unit_cost\": %.2f,\n"
            "    \"total_value\": %.2f,\n"
            "    \"location\": \"%s\",\n"
            "    \"updated_at\": \"%s\"\n"
            "  }",
            item->id, item->sku, item->company_id, esc_name, item->category,
            item->quantity, item->min_quantity, item->unit_cost,
            item->quantity * item->unit_cost, item->location, item->updated_at);
        matched++;
    }

    offset += snprintf(buf + offset, alloc_size - offset, "\n]");
    if (g_stock_mutex) ar_mutex_unlock(g_stock_mutex);
    return buf;
}

char* arapistock_db_get_summary_json(const char *caller_company, int is_master) {
    if (g_stock_mutex) ar_mutex_lock(g_stock_mutex);

    int total_skus = 0;
    int total_units = 0;
    double total_inventory_value = 0.0;
    int low_stock_alerts = 0;

    for (int i = 0; i < g_items_count; i++) {
        StockItem *item = &g_items[i];

        if (!is_master && caller_company && caller_company[0]) {
            if (strcmp(item->company_id, caller_company) != 0 && strcmp(item->company_id, "global") != 0) {
                continue;
            }
        }

        total_skus++;
        total_units += item->quantity;
        total_inventory_value += (item->quantity * item->unit_cost);
        if (item->quantity <= item->min_quantity) {
            low_stock_alerts++;
        }
    }

    char *buf = (char*)malloc(1024);
    if (!buf) {
        if (g_stock_mutex) ar_mutex_unlock(g_stock_mutex);
        return strdup("{}");
    }

    snprintf(buf, 1024,
        "{\n"
        "  \"company_id\": \"%s\",\n"
        "  \"total_skus\": %d,\n"
        "  \"total_units\": %d,\n"
        "  \"total_inventory_value\": %.2f,\n"
        "  \"low_stock_alerts\": %d\n"
        "}\n",
        (is_master || !caller_company || !caller_company[0]) ? "alrigroup" : caller_company,
        total_skus,
        total_units,
        total_inventory_value,
        low_stock_alerts);

    if (g_stock_mutex) ar_mutex_unlock(g_stock_mutex);
    return buf;
}

void arapistock_db_close(void) {
    if (g_stock_mutex) {
        ar_mutex_destroy(g_stock_mutex);
        g_stock_mutex = NULL;
    }
}
