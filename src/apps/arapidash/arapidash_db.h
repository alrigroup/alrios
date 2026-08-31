/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARAPIDASH_DB_H
#define ARAPIDASH_DB_H

#include <stdint.h>
#include <stddef.h>

int arapidash_db_init(const char *data_dir);

// Returns JSON of executive overview KPIs (scoped by company / is_master)
char* arapidash_db_get_overview_json(const char *caller_company, int is_master);

// Returns JSON timeseries chart data
char* arapidash_db_get_charts_json(const char *caller_company, int is_master);

// Returns JSON company breakdown
char* arapidash_db_get_breakdown_json(const char *caller_company, int is_master);

void arapidash_db_close(void);

#endif /* ARAPIDASH_DB_H */
