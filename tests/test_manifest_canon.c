/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#include <assert.h>
#include <stdio.h>
#include <string.h>

int alrios_manifest_canonicalize(const char *in_json, char *out_canon,
                                 size_t max_out);

static void expect_canonical(const char *input, const char *expected)
{
    char output[1024];
    assert(alrios_manifest_canonicalize(input, output, sizeof(output)) == 0);
    assert(strcmp(output, expected) == 0);
}

static void test_sorting_and_whitespace(void)
{
    expect_canonical(
        " { \"resources\" : { \"max_threads\" : 8, \"cpu_weight\":100 },"
        "\"app_id\" : \"com.test\", \"enabled\" : true } \n",
        "{\"app_id\":\"com.test\",\"enabled\":true,\"resources\":"
        "{\"cpu_weight\":100,\"max_threads\":8}}");
    expect_canonical("{\"z\":[3, {\"b\":2,\"a\":1}],\"a\":null}",
                     "{\"a\":null,\"z\":[3,{\"a\":1,\"b\":2}]}");
}

static void test_strings_and_unicode(void)
{
    expect_canonical("{\"escaped\":\"a\\/b\\u000a\",\"unicode\":\"\\u00e9\"}",
                     "{\"escaped\":\"a/b\\n\",\"unicode\":\"é\"}");
    expect_canonical("{\"\\u0062\":1,\"a\":2}",
                     "{\"a\":2,\"b\":1}");
    expect_canonical("{\"zero\":-0}", "{\"zero\":0}");
}

static void test_fail_closed(void)
{
    char output[32] = "stale";
    assert(alrios_manifest_canonicalize(NULL, output, sizeof(output)) != 0);
    assert(alrios_manifest_canonicalize("{}", NULL, sizeof(output)) != 0);
    assert(alrios_manifest_canonicalize("{}", output, 0U) != 0);

    assert(alrios_manifest_canonicalize("{\"a\":1,\"a\":2}", output,
                                        sizeof(output)) != 0);
    assert(strcmp(output, "stale") == 0);
    assert(alrios_manifest_canonicalize("{\"a\":01}", output,
                                        sizeof(output)) != 0);
    assert(alrios_manifest_canonicalize("{\"a\":1.5}", output,
                                        sizeof(output)) != 0);
    assert(alrios_manifest_canonicalize("{\"a\":\"\\uD800\"}", output,
                                        sizeof(output)) != 0);
    assert(alrios_manifest_canonicalize("{} trailing", output,
                                        sizeof(output)) != 0);
    assert(alrios_manifest_canonicalize("{\"long\":true}", output, 4U) != 0);
    assert(strcmp(output, "stale") == 0);
}

int main(void)
{
    expect_canonical("{\"app_id\":\"com.test\"}",
                     "{\"app_id\":\"com.test\"}");
    test_sorting_and_whitespace();
    test_strings_and_unicode();
    test_fail_closed();
    printf("TASK-022 (Manifest Canonicalization): PASS\n");
    return 0;
}
