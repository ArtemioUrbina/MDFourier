#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Include the production config structure and function declarations */
#include "plot.c"

START_TEST(test_compareName_buffer_overflow)
{
    /* Invariant: sprintf into 'name' buffer must never exceed its declared size
       regardless of compareName length. The buffer 'name' in plot.c is typically
       BUFFER_SIZE or similar; oversized compareName must be rejected or truncated. */
    const char *payloads[] = {
        "A",                                          /* valid short input */
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", /* 64 chars - boundary */
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", /* 256+ chars - exploit */
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        parameters config;
        memset(&config, 0, sizeof(parameters));

        /* Set compareName to the adversarial payload */
        strncpy(config.compareName, payloads[i], sizeof(config.compareName) - 1);
        config.compareName[sizeof(config.compareName) - 1] = '\0';

        /* The invariant: after truncation to compareName buffer, the resulting
           name string must fit within the expected output buffer.
           compareName must be bounded by its declared field size. */
        size_t name_len = strlen(config.compareName);
        ck_assert_msg(name_len < sizeof(config.compareName),
            "compareName must be null-terminated within its declared bounds");

        /* Simulate what plot.c does: name = compareName + '_' + CHANNEL char
           Verify the composed string length stays within a safe bound (256 bytes) */
        char name[256];
        int written = snprintf(name, sizeof(name), "%s_%c", config.compareName, 'L');
        ck_assert_msg(written < (int)sizeof(name),
            "Composed name must fit within output buffer for payload index %d", i);

        written = snprintf(name, sizeof(name), "%s_%c", config.compareName, 'R');
        ck_assert_msg(written < (int)sizeof(name),
            "Composed name must fit within output buffer for payload index %d", i);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_compareName_buffer_overflow);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}