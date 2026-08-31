/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARAPIBUS_DB_H
#define ARAPIBUS_DB_H

#include <stdint.h>
#include <stddef.h>

#define MAX_COMPANIES 32
#define MAX_DEPARTMENTS 64
#define MAX_EMPLOYEES 512

typedef struct {
    char id[64];
    char name[128];
    char slug[64];
    char domain[128];
    int is_holding;
    char cnpj[32];
} BusCompany;

typedef struct {
    char id[64];
    char company_id[64];
    char name[128];
    char code[32];
    char leader[64];
    char description[256];
} BusDepartment;

typedef struct {
    char id[64];
    char company_id[64];
    char department_id[64];
    char username[64];
    char name[128];
    char email[128];
    char role[64];
    int hierarchy_level;
    char created_at[32];
} BusEmployee;

int arapibus_db_init(const char *data_dir);
int arapibus_db_get_caller_hierarchy(const char *username, const char *company_id);

// Companies CRUD
char* arapibus_db_list_companies_json(const char *caller_tenant, int is_master);
int arapibus_db_create_company(const char *id, const char *name, const char *slug, const char *domain, int is_holding, const char *cnpj);
int arapibus_db_delete_company(const char *id);

// Departments CRUD
char* arapibus_db_list_departments_json(const char *company_id, const char *caller_tenant, int is_master);
int arapibus_db_create_department(const char *company_id, const char *name, const char *code, const char *leader, const char *description);
int arapibus_db_delete_department(const char *dept_id, const char *company_id);

// Employees CRUD
char* arapibus_db_list_employees_json(const char *company_id, const char *caller_tenant, int is_master);
int arapibus_db_create_employee(const char *company_id, const char *dept_id, const char *username, const char *name, const char *email, const char *role, int hierarchy_level);
int arapibus_db_delete_employee(const char *username, const char *company_id);

void arapibus_db_close(void);

#endif /* ARAPIBUS_DB_H */
