/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/supervisor/context.h"
#include "alrios/supervisor/loader.h"
#include "alrios/supervisor/policy.h"
#include "alrios/supervisor/guest_router.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                      __FILE__, __LINE__, #cond); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void test_context_create_destroy(void) {
    ar_supervisor_context_t *ctx = ar_supervisor_context_create();
    CHECK(ctx != NULL);
    CHECK(ctx->app_count == 0);
    CHECK(ctx->hotapp_count == 0);
    CHECK(ctx->autostart_count == 0);
    CHECK(ctx->router == NULL);
    CHECK(ctx->policy == NULL);
    CHECK(ctx->app_mutex != NULL);
    CHECK(ctx->hotreload_mutex != NULL);

    /* Safe teardown */
    ar_supervisor_context_destroy(ctx);

    /* Verify destroy handles NULL gracefully */
    ar_supervisor_context_destroy(NULL);
}

static void test_multiple_contexts_concurrency_and_isolation(void) {
    ar_supervisor_context_t *ctx1 = ar_supervisor_context_create();
    ar_supervisor_context_t *ctx2 = ar_supervisor_context_create();
    ar_supervisor_context_t *ctx3 = ar_supervisor_context_create();

    CHECK(ctx1 != NULL);
    CHECK(ctx2 != NULL);
    CHECK(ctx3 != NULL);
    CHECK(ctx1 != ctx2);
    CHECK(ctx2 != ctx3);
    CHECK(ctx1 != ctx3);

    ar_app_manifest_t m1;
    memset(&m1, 0, sizeof(m1));
    snprintf(m1.name, sizeof(m1.name), "service-alpha");
    snprintf(m1.version, sizeof(m1.version), "1.0.0");
    m1.runtime = AR_RUNTIME_NATIVE;

    ar_app_manifest_t m2;
    memset(&m2, 0, sizeof(m2));
    snprintf(m2.name, sizeof(m2.name), "service-beta");
    snprintf(m2.version, sizeof(m2.version), "2.0.0");
    m2.runtime = AR_RUNTIME_NATIVE;

    ar_app_manifest_t m3;
    memset(&m3, 0, sizeof(m3));
    snprintf(m3.name, sizeof(m3.name), "service-gamma");
    snprintf(m3.version, sizeof(m3.version), "3.0.0");
    m3.runtime = AR_RUNTIME_NATIVE;

    /* Register m1 into ctx1 */
    loader_app_t *reg1 = ar_supervisor_register_app(ctx1, &m1, "/apps/alpha", "/pkgs/alpha.arapp");
    CHECK(reg1 != NULL);
    CHECK(ctx1->app_count == 1);
    CHECK(ar_supervisor_find_app(ctx1, "service-alpha") != NULL);

    /* Verify ctx2 and ctx3 remain isolated with 0 apps */
    CHECK(ctx2->app_count == 0);
    CHECK(ctx3->app_count == 0);
    CHECK(ar_supervisor_find_app(ctx2, "service-alpha") == NULL);
    CHECK(ar_supervisor_find_app(ctx3, "service-alpha") == NULL);

    /* Register m2 into ctx2 */
    loader_app_t *reg2 = ar_supervisor_register_app(ctx2, &m2, "/apps/beta", "/pkgs/beta.arapp");
    CHECK(reg2 != NULL);
    CHECK(ctx2->app_count == 1);
    CHECK(ar_supervisor_find_app(ctx2, "service-beta") != NULL);

    /* Verify ctx1 and ctx3 do NOT have service-beta */
    CHECK(ar_supervisor_find_app(ctx1, "service-beta") == NULL);
    CHECK(ar_supervisor_find_app(ctx3, "service-beta") == NULL);

    /* Register m3 into ctx3 */
    loader_app_t *reg3 = ar_supervisor_register_app(ctx3, &m3, "/apps/gamma", "/pkgs/gamma.arapp");
    CHECK(reg3 != NULL);
    CHECK(ctx3->app_count == 1);
    CHECK(ar_supervisor_find_app(ctx3, "service-gamma") != NULL);

    /* Update runtime process in ctx1 */
    CHECK(ar_supervisor_update_app_process(ctx1, "service-alpha", 4242, "/run/alpha") == 0);
    loader_app_t *alpha = ar_supervisor_find_app(ctx1, "service-alpha");
    CHECK(alpha != NULL);
    CHECK(alpha->pid == 4242);
    CHECK(alpha->state == APP_RUNNING);
    CHECK(strcmp(alpha->dir, "/run/alpha") == 0);

    /* Verify ctx2 beta is still stopped with pid 0 */
    loader_app_t *beta = ar_supervisor_find_app(ctx2, "service-beta");
    CHECK(beta != NULL);
    CHECK(beta->pid == 0);
    CHECK(beta->state == APP_STOPPED);

    /* Verify list_apps reflects isolated state */
    char list1[1024] = {0};
    char list2[1024] = {0};
    CHECK(ar_supervisor_list_apps(ctx1, list1, sizeof(list1)) > 0);
    CHECK(ar_supervisor_list_apps(ctx2, list2, sizeof(list2)) > 0);
    CHECK(strstr(list1, "service-alpha") != NULL);
    CHECK(strstr(list1, "service-beta") == NULL);
    CHECK(strstr(list2, "service-beta") != NULL);
    CHECK(strstr(list2, "service-alpha") == NULL);

    /* Destroy ctx1 and verify ctx2 and ctx3 survive intact */
    ar_supervisor_context_destroy(ctx1);

    loader_app_t *beta_after = ar_supervisor_find_app(ctx2, "service-beta");
    CHECK(beta_after != NULL);
    CHECK(strcmp(beta_after->name, "service-beta") == 0);

    loader_app_t *gamma_after = ar_supervisor_find_app(ctx3, "service-gamma");
    CHECK(gamma_after != NULL);
    CHECK(strcmp(gamma_after->name, "service-gamma") == 0);

    ar_supervisor_context_destroy(ctx2);
    ar_supervisor_context_destroy(ctx3);
}

static void test_context_with_config(void) {
    ar_supervisor_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    snprintf(cfg.base_dir, sizeof(cfg.base_dir), "/tmp/alrios_custom_base");
    snprintf(cfg.apps_dir, sizeof(cfg.apps_dir), "/tmp/alrios_custom_apps");
    snprintf(cfg.run_dir, sizeof(cfg.run_dir), "/tmp/alrios_custom_run");

    ar_supervisor_context_t *ctx = NULL;
    int rc = ar_supervisor_context_create_with_config(&cfg, &ctx);
    CHECK(rc == 0);
    CHECK(ctx != NULL);

    char base[1024];
    char apps[1024];
    char run[1024];

    ar_supervisor_get_base_dir(ctx, base, sizeof(base));
    ar_supervisor_get_apps_dir(ctx, apps, sizeof(apps));
    ar_supervisor_get_run_dir(ctx, run, sizeof(run));

    CHECK(strcmp(base, "/tmp/alrios_custom_base") == 0);
    CHECK(strcmp(apps, "/tmp/alrios_custom_apps") == 0);
    CHECK(strcmp(run, "/tmp/alrios_custom_run") == 0);

    ar_supervisor_context_destroy(ctx);
}

static void test_hotreload_slots_isolation(void) {
    ar_supervisor_context_t *ctx1 = ar_supervisor_context_create();
    ar_supervisor_context_t *ctx2 = ar_supervisor_context_create();
    CHECK(ctx1 != NULL && ctx2 != NULL);

    CHECK(ar_supervisor_hotreload_init(ctx1) == 0);
    CHECK(ar_supervisor_hotreload_init(ctx2) == 0);

    char status1[512] = {0};
    char status2[512] = {0};

    CHECK(ar_supervisor_hotreload_status(ctx1, "web-app", status1, sizeof(status1)) == 0);
    CHECK(ar_supervisor_hotreload_status(ctx2, "web-app", status2, sizeof(status2)) == 0);

    CHECK(strstr(status1, "No active hot-reload session") != NULL);
    CHECK(strstr(status2, "No active hot-reload session") != NULL);

    ar_supervisor_context_destroy(ctx1);
    ar_supervisor_context_destroy(ctx2);
}

static void test_router_isolation(void) {
    ar_supervisor_context_t *ctx1 = ar_supervisor_context_create();
    ar_supervisor_context_t *ctx2 = ar_supervisor_context_create();
    CHECK(ctx1 != NULL && ctx2 != NULL);

    ar_guest_router_config_t config;
    memset(&config, 0, sizeof(config));
    config.max_guests = 16;
    config.max_queue_depth = 32;
    config.max_payload_size = 4096;
    config.io_timeout_ms = 50;

    int rc = ar_supervisor_init_router(ctx1, &config);
    if (rc == 0) {
        CHECK(ar_supervisor_get_router(ctx1) != NULL);
        CHECK(ar_supervisor_get_router(ctx2) == NULL);
    }

    ar_supervisor_context_destroy(ctx1);
    ar_supervisor_context_destroy(ctx2);
}

int main(void) {
    test_context_create_destroy();
    test_multiple_contexts_concurrency_and_isolation();
    test_context_with_config();
    test_hotreload_slots_isolation();
    test_router_isolation();

    (void)puts("test_supervisor_context: PASS");
    return EXIT_SUCCESS;
}
