/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arapibus_db.h"
#include "aros_hal.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

static BusCompany g_companies[MAX_COMPANIES];
static int g_companies_count = 0;

static BusDepartment g_departments[MAX_DEPARTMENTS];
static int g_departments_count = 0;

static BusEmployee g_employees[MAX_EMPLOYEES];
static int g_employees_count = 0;

static void *g_bus_mutex = NULL;
static char g_db_file[512] = {0};

static void get_iso_now(char *out, size_t max_len) {
    time_t t = time(NULL);
    struct tm tm_buf;
    gmtime_r(&t, &tm_buf);
    strftime(out, max_len, "%Y-%m-%d %H:%M:%S UTC", &tm_buf);
}

int arapibus_db_init(const char *data_dir) {
    g_bus_mutex = ar_mutex_create();

    if (data_dir && data_dir[0]) {
        mkdir(data_dir, 0755);
        snprintf(g_db_file, sizeof(g_db_file), "%s/corporate.db", data_dir);
    }

    if (g_bus_mutex) ar_mutex_lock(g_bus_mutex);

    // Bootstrap default Companies if empty
    if (g_companies_count == 0) {
        arapibus_db_create_company("alrigroup", "ALRIGROUP Holding", "alrigroup", "alrigroup.com", 1, "00.000.000/0001-00");
        arapibus_db_create_company("detroitgg", "Detroit GG", "detroitgg", "detroitgg.alrigroup.com", 0, "11.111.111/0001-11");
    }

    // Bootstrap default Departments
    if (g_departments_count == 0) {
        arapibus_db_create_department("alrigroup", "Diretoria & Executivo", "DIR", "alexsanderalri", "Gestão Estratégica da Holding");
        arapibus_db_create_department("alrigroup", "Engenharia de Sistemas", "ENG", "alexsanderalri", "Desenvolvimento Soberano ALRIOS");
        arapibus_db_create_department("detroitgg", "Staff & Operações", "STAFF", "alexsanderalri", "Moderação e Suporte Detroit GG");
        arapibus_db_create_department("detroitgg", "Eventos & Mídia", "EVT", "alexsanderalri", "Produção de Conteúdo e Campanhas");
    }

    // Bootstrap default Key Employees
    if (g_employees_count == 0) {
        arapibus_db_create_employee("alrigroup", "DIR", "alexsanderalri", "Alex Sander Alri", "alex@alrigroup.com", "CEO Holding Master", 1);
        arapibus_db_create_employee("detroitgg", "STAFF", "alexsanderalri", "Alex Sander Alri", "alex@alrigroup.com", "Fundador & Gestor", 2);
    }

    if (g_bus_mutex) ar_mutex_unlock(g_bus_mutex);
    return 0;
}

int arapibus_db_get_caller_hierarchy(const char *username, const char *company_id) {
    if (!username) return 99;
    if (strcmp(username, "alexsanderalri") == 0 || strcmp(username, "admin") == 0) return 1;

    if (g_bus_mutex) ar_mutex_lock(g_bus_mutex);
    int level = 99;
    for (int i = 0; i < g_employees_count; i++) {
        if (strcmp(g_employees[i].username, username) == 0) {
            if (!company_id || company_id[0] == '\0' || strcmp(g_employees[i].company_id, company_id) == 0) {
                if (g_employees[i].hierarchy_level < level) {
                    level = g_employees[i].hierarchy_level;
                }
            }
        }
    }
    if (g_bus_mutex) ar_mutex_unlock(g_bus_mutex);
    return level;
}

// -----------------------------------------------------------------------------
// COMPANIES CRUD
// -----------------------------------------------------------------------------
int arapibus_db_create_company(const char *id, const char *name, const char *slug, const char *domain, int is_holding, const char *cnpj) {
    if (!id || !name) return -1;
    if (g_companies_count >= MAX_COMPANIES) return -1;

    for (int i = 0; i < g_companies_count; i++) {
        if (strcmp(g_companies[i].id, id) == 0) return 0; // already exists
    }

    BusCompany *c = &g_companies[g_companies_count++];
    memset(c, 0, sizeof(BusCompany));
    strncpy(c->id, id, sizeof(c->id) - 1);
    strncpy(c->name, name, sizeof(c->name) - 1);
    strncpy(c->slug, slug ? slug : id, sizeof(c->slug) - 1);
    strncpy(c->domain, domain ? domain : "", sizeof(c->domain) - 1);
    c->is_holding = is_holding;
    strncpy(c->cnpj, cnpj ? cnpj : "", sizeof(c->cnpj) - 1);
    return 0;
}

int arapibus_db_delete_company(const char *id) {
    if (!id) return -1;
    int found = -1;
    for (int i = 0; i < g_companies_count; i++) {
        if (strcmp(g_companies[i].id, id) == 0) {
            found = i;
            break;
        }
    }
    if (found < 0) return -1;

    for (int i = found; i < g_companies_count - 1; i++) {
        g_companies[i] = g_companies[i + 1];
    }
    g_companies_count--;
    return 0;
}

char* arapibus_db_list_companies_json(const char *caller_tenant, int is_master) {
    if (g_bus_mutex) ar_mutex_lock(g_bus_mutex);

    size_t alloc_size = 16384;
    char *buf = (char*)malloc(alloc_size);
    if (!buf) {
        if (g_bus_mutex) ar_mutex_unlock(g_bus_mutex);
        return strdup("[]");
    }

    size_t offset = 0;
    offset += snprintf(buf + offset, alloc_size - offset, "[\n");

    int matched = 0;
    for (int i = 0; i < g_companies_count; i++) {
        BusCompany *c = &g_companies[i];

        // If not master, only return own company
        if (!is_master && caller_tenant && caller_tenant[0]) {
            if (strcmp(c->id, caller_tenant) != 0) continue;
        }

        if (matched > 0) offset += snprintf(buf + offset, alloc_size - offset, ",\n");

        offset += snprintf(buf + offset, alloc_size - offset,
            "  {\n"
            "    \"id\": \"%s\",\n"
            "    \"name\": \"%s\",\n"
            "    \"slug\": \"%s\",\n"
            "    \"domain\": \"%s\",\n"
            "    \"is_holding\": %s,\n"
            "    \"cnpj\": \"%s\"\n"
            "  }",
            c->id, c->name, c->slug, c->domain, c->is_holding ? "true" : "false", c->cnpj);
        matched++;
    }

    offset += snprintf(buf + offset, alloc_size - offset, "\n]");
    if (g_bus_mutex) ar_mutex_unlock(g_bus_mutex);
    return buf;
}

// -----------------------------------------------------------------------------
// DEPARTMENTS CRUD
// -----------------------------------------------------------------------------
int arapibus_db_create_department(const char *company_id, const char *name, const char *code, const char *leader, const char *description) {
    if (!company_id || !name) return -1;
    if (g_departments_count >= MAX_DEPARTMENTS) return -1;

    BusDepartment *d = &g_departments[g_departments_count++];
    memset(d, 0, sizeof(BusDepartment));
    snprintf(d->id, sizeof(d->id), "dept_%d", g_departments_count);
    strncpy(d->company_id, company_id, sizeof(d->company_id) - 1);
    strncpy(d->name, name, sizeof(d->name) - 1);
    strncpy(d->code, code ? code : "DEP", sizeof(d->code) - 1);
    strncpy(d->leader, leader ? leader : "", sizeof(d->leader) - 1);
    strncpy(d->description, description ? description : "", sizeof(d->description) - 1);
    return 0;
}

int arapibus_db_delete_department(const char *dept_id, const char *company_id) {
    if (!dept_id) return -1;
    int found = -1;
    for (int i = 0; i < g_departments_count; i++) {
        if ((strcmp(g_departments[i].id, dept_id) == 0 || strcmp(g_departments[i].code, dept_id) == 0) &&
            (!company_id || strcmp(g_departments[i].company_id, company_id) == 0)) {
            found = i;
            break;
        }
    }
    if (found < 0) return -1;

    for (int i = found; i < g_departments_count - 1; i++) {
        g_departments[i] = g_departments[i + 1];
    }
    g_departments_count--;
    return 0;
}

char* arapibus_db_list_departments_json(const char *company_id, const char *caller_tenant, int is_master) {
    if (g_bus_mutex) ar_mutex_lock(g_bus_mutex);

    size_t alloc_size = 16384;
    char *buf = (char*)malloc(alloc_size);
    if (!buf) {
        if (g_bus_mutex) ar_mutex_unlock(g_bus_mutex);
        return strdup("[]");
    }

    size_t offset = 0;
    offset += snprintf(buf + offset, alloc_size - offset, "[\n");

    int matched = 0;
    for (int i = 0; i < g_departments_count; i++) {
        BusDepartment *d = &g_departments[i];

        if (company_id && company_id[0] && strcmp(d->company_id, company_id) != 0) continue;
        if (!is_master && caller_tenant && caller_tenant[0] && strcmp(d->company_id, caller_tenant) != 0) continue;

        if (matched > 0) offset += snprintf(buf + offset, alloc_size - offset, ",\n");

        offset += snprintf(buf + offset, alloc_size - offset,
            "  {\n"
            "    \"id\": \"%s\",\n"
            "    \"company_id\": \"%s\",\n"
            "    \"name\": \"%s\",\n"
            "    \"code\": \"%s\",\n"
            "    \"leader\": \"%s\",\n"
            "    \"description\": \"%s\"\n"
            "  }",
            d->id, d->company_id, d->name, d->code, d->leader, d->description);
        matched++;
    }

    offset += snprintf(buf + offset, alloc_size - offset, "\n]");
    if (g_bus_mutex) ar_mutex_unlock(g_bus_mutex);
    return buf;
}

// -----------------------------------------------------------------------------
// EMPLOYEES CRUD
// -----------------------------------------------------------------------------
int arapibus_db_create_employee(const char *company_id, const char *dept_id, const char *username, const char *name, const char *email, const char *role, int hierarchy_level) {
    if (!company_id || !username || !name) return -1;
    if (g_employees_count >= MAX_EMPLOYEES) return -1;

    for (int i = 0; i < g_employees_count; i++) {
        if (strcmp(g_employees[i].username, username) == 0 && strcmp(g_employees[i].company_id, company_id) == 0) {
            return 0; // already exists in this company
        }
    }

    BusEmployee *e = &g_employees[g_employees_count++];
    memset(e, 0, sizeof(BusEmployee));
    snprintf(e->id, sizeof(e->id), "emp_%d", g_employees_count);
    strncpy(e->company_id, company_id, sizeof(e->company_id) - 1);
    strncpy(e->department_id, dept_id ? dept_id : "DIR", sizeof(e->department_id) - 1);
    strncpy(e->username, username, sizeof(e->username) - 1);
    strncpy(e->name, name, sizeof(e->name) - 1);
    strncpy(e->email, email ? email : "", sizeof(e->email) - 1);
    strncpy(e->role, role ? role : "Colaborador", sizeof(e->role) - 1);
    e->hierarchy_level = hierarchy_level > 0 ? hierarchy_level : 4;
    get_iso_now(e->created_at, sizeof(e->created_at));
    return 0;
}

int arapibus_db_delete_employee(const char *username, const char *company_id) {
    if (!username) return -1;
    int found = -1;
    for (int i = 0; i < g_employees_count; i++) {
        if (strcmp(g_employees[i].username, username) == 0 &&
            (!company_id || strcmp(g_employees[i].company_id, company_id) == 0)) {
            found = i;
            break;
        }
    }
    if (found < 0) return -1;

    for (int i = found; i < g_employees_count - 1; i++) {
        g_employees[i] = g_employees[i + 1];
    }
    g_employees_count--;
    return 0;
}

char* arapibus_db_list_employees_json(const char *company_id, const char *caller_tenant, int is_master) {
    if (g_bus_mutex) ar_mutex_lock(g_bus_mutex);

    size_t alloc_size = 32768;
    char *buf = (char*)malloc(alloc_size);
    if (!buf) {
        if (g_bus_mutex) ar_mutex_unlock(g_bus_mutex);
        return strdup("[]");
    }

    size_t offset = 0;
    offset += snprintf(buf + offset, alloc_size - offset, "[\n");

    int matched = 0;
    for (int i = 0; i < g_employees_count; i++) {
        BusEmployee *e = &g_employees[i];

        if (company_id && company_id[0] && strcmp(e->company_id, company_id) != 0) continue;
        if (!is_master && caller_tenant && caller_tenant[0] && strcmp(e->company_id, caller_tenant) != 0) continue;

        if (matched > 0) offset += snprintf(buf + offset, alloc_size - offset, ",\n");

        offset += snprintf(buf + offset, alloc_size - offset,
            "  {\n"
            "    \"id\": \"%s\",\n"
            "    \"company_id\": \"%s\",\n"
            "    \"department_id\": \"%s\",\n"
            "    \"username\": \"%s\",\n"
            "    \"name\": \"%s\",\n"
            "    \"email\": \"%s\",\n"
            "    \"role\": \"%s\",\n"
            "    \"hierarchy_level\": %d,\n"
            "    \"created_at\": \"%s\"\n"
            "  }",
            e->id, e->company_id, e->department_id, e->username, e->name, e->email, e->role, e->hierarchy_level, e->created_at);
        matched++;
    }

    offset += snprintf(buf + offset, alloc_size - offset, "\n]");
    if (g_bus_mutex) ar_mutex_unlock(g_bus_mutex);
    return buf;
}

void arapibus_db_close(void) {
    if (g_bus_mutex) {
        ar_mutex_destroy(g_bus_mutex);
        g_bus_mutex = NULL;
    }
}
