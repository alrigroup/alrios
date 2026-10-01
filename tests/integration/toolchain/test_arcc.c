/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define REQUIRE(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "requirement failed: %s\n", #condition); \
        return 1; \
    } \
} while (0)

static int command_exit_code(const char *command) {
    int status = system(command);
    if (status == -1 || !WIFEXITED(status)) {
        return 255;
    }
    return WEXITSTATUS(status);
}

int main(void) {
    const char *bad_source = "/tmp/alrios_arcc_banned.c";
    const char *good_source = "/tmp/alrios_arcc_allowed.c";
    int status;
    FILE *file = fopen(bad_source, "w");

    REQUIRE(file != NULL);
    REQUIRE(fputs("#include <string.h>\nint main(void) { char b[4]; strcpy(b, \"x\"); return 0; }\n", file) >= 0);
    REQUIRE(fclose(file) == 0);
    status = command_exit_code("./arcore/arcc /tmp/alrios_arcc_banned.c -o /tmp/alrios_arcc_banned 2>/dev/null");
    REQUIRE(status != 0);

    file = fopen(good_source, "w");
    REQUIRE(file != NULL);
    REQUIRE(fputs("int main(void) { return 0; }\n", file) >= 0);
    REQUIRE(fclose(file) == 0);
    status = command_exit_code("./arcore/arcc /tmp/alrios_arcc_allowed.c -o /tmp/alrios_arcc_allowed");
    REQUIRE(status == 0);

    REQUIRE(unlink(bad_source) == 0);
    REQUIRE(unlink(good_source) == 0);
    REQUIRE(unlink("/tmp/alrios_arcc_allowed") == 0);
    (void)unlink("/tmp/alrios_arcc_banned");
    (void)printf("TEST_ARCC: PASS\n");
    return 0;
}
