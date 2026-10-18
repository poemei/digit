/*
 * STN-LABZ
 * Module ABI 1.4
 *
 * abi.c
 *
 * Cross-application module lifecycle orchestration.
 */

#include <stdio.h>
#include <string.h>

#include "abi.h"


stnlabz_module_result_t
stnlabz_module_abi_prepare(
    stnlabz_module_registry_t *registry,
    const stnlabz_module_descriptor_t *descriptor
)
{
    stnlabz_module_result_t result;


    if (
        registry == NULL ||
        descriptor == NULL
    )
    {
        return
            STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    }


    result =
        stnlabz_module_registry_discover(
            registry,
            descriptor
        );


    if (
        result !=
        STNLABZ_MODULE_OK
    )
    {
        return result;
    }


    result =
        stnlabz_module_registry_verify(
            registry,
            descriptor->id
        );


    if (
        result !=
        STNLABZ_MODULE_OK
    )
    {
        return result;
    }


    return
        stnlabz_module_registry_qualify(
            registry,
            descriptor->id
        );
}


stnlabz_module_result_t
stnlabz_module_abi_authorize_and_activate(
    stnlabz_module_registry_t *registry,
    const char *module_id,
    const stnlabz_module_host_t *host
)
{
    const stnlabz_module_record_t *record;

    stnlabz_module_result_t result;


    if (
        registry == NULL ||
        module_id == NULL ||
        module_id[0] == '\0'
    )
    {
        return
            STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    }


    result =
        stnlabz_module_registry_authorize_activation(
            registry,
            module_id
        );


    if (
        result !=
        STNLABZ_MODULE_OK
    )
    {
        return result;
    }


    record =
        stnlabz_module_registry_find(
            registry,
            module_id
        );


    if (
        record == NULL
    )
    {
        return
            STNLABZ_MODULE_ERR_NOT_FOUND;
    }


    if (
        record->descriptor.start != NULL
    )
    {
        result =
            record
                ->descriptor
                .start(
                    host
                );


        if (
            result !=
            STNLABZ_MODULE_OK
        )
        {
            (void)
            stnlabz_module_registry_fail(
                registry,
                module_id
            );


            return
                STNLABZ_MODULE_ERR_START_FAILED;
        }
    }


    result =
        stnlabz_module_registry_activate(
            registry,
            module_id
        );


    if (
        result !=
        STNLABZ_MODULE_OK
    )
    {
        record =
            stnlabz_module_registry_find(
                registry,
                module_id
            );


        if (
            record != NULL &&
            record->descriptor.stop != NULL
        )
        {
            (void)
            record
                ->descriptor
                .stop();
        }


        (void)
        stnlabz_module_registry_fail(
            registry,
            module_id
        );


        return result;
    }


    return
        STNLABZ_MODULE_OK;
}


stnlabz_module_result_t
stnlabz_module_abi_stop(
    stnlabz_module_registry_t *registry,
    const char *module_id
)
{
    const stnlabz_module_record_t *record;

    stnlabz_module_result_t result;


    if (
        registry == NULL ||
        module_id == NULL ||
        module_id[0] == '\0'
    )
    {
        return
            STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    }


    record =
        stnlabz_module_registry_find(
            registry,
            module_id
        );


    if (
        record == NULL
    )
    {
        return
            STNLABZ_MODULE_ERR_NOT_FOUND;
    }


    if (
        record->state ==
        STNLABZ_MODULE_STATE_QUARANTINED
    )
    {
        return
            STNLABZ_MODULE_ERR_QUARANTINED;
    }


    if (
        record->state !=
        STNLABZ_MODULE_STATE_ACTIVE
    )
    {
        return
            STNLABZ_MODULE_ERR_INVALID_STATE;
    }


    if (
        record->descriptor.stop != NULL
    )
    {
        result =
            record
                ->descriptor
                .stop();


        if (
            result !=
            STNLABZ_MODULE_OK
        )
        {
            (void)
            stnlabz_module_registry_fail(
                registry,
                module_id
            );


            return
                STNLABZ_MODULE_ERR_STOP_FAILED;
        }
    }


    return
        stnlabz_module_registry_stop(
            registry,
            module_id
        );
}


stnlabz_module_result_t
stnlabz_module_abi_unregister(
    stnlabz_module_registry_t *registry,
    const char *module_id
)
{
    return
        stnlabz_module_registry_unregister(
            registry,
            module_id
        );
}


stnlabz_module_result_t
stnlabz_module_abi_prepare_replacement(
    stnlabz_module_registry_t *registry,
    const char *module_id
)
{
    const stnlabz_module_record_t *record;

    stnlabz_module_result_t result;


    if (
        registry == NULL ||
        module_id == NULL ||
        module_id[0] == '\0'
    )
    {
        return
            STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    }


    record =
        stnlabz_module_registry_find(
            registry,
            module_id
        );


    if (
        record == NULL
    )
    {
        return
            STNLABZ_MODULE_ERR_NOT_FOUND;
    }


    if (
        record->state ==
        STNLABZ_MODULE_STATE_ACTIVE
    )
    {
        result =
            stnlabz_module_abi_stop(
                registry,
                module_id
            );


        if (
            result !=
            STNLABZ_MODULE_OK
        )
        {
            return result;
        }
    }


    return
        stnlabz_module_abi_unregister(
            registry,
            module_id
        );
}


/*
 * ================================================================
 * SELF TEST
 * ================================================================
 */

static stnlabz_module_result_t
abi_test_qualify(
    stnlabz_module_qualification_result_t *result
)
{
    if (
        result == NULL
    )
    {
        return
            STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    }


    memset(
        result,
        0,
        sizeof(*result)
    );


    result->tests_executed =
        STNLABZ_MODULE_MIN_TESTS;

    result->tests_passed =
        STNLABZ_MODULE_MIN_TESTS;

    result->tests_failed =
        0;

    result->negative_test_executed =
        1;

    result->negative_test_passed =
        1;


    return
        STNLABZ_MODULE_OK;
}


static stnlabz_module_result_t
abi_test_start(
    const stnlabz_module_host_t *host
)
{
    (void)host;

    return
        STNLABZ_MODULE_OK;
}


static stnlabz_module_result_t
abi_test_stop(void)
{
    return
        STNLABZ_MODULE_OK;
}


static const stnlabz_module_descriptor_t
ABI_TEST_DESCRIPTOR =
{
    "abi-test",

    "ABI Test Module",

    1,
    0,
    0,

    STNLABZ_MODULE_API_MAJOR,
    STNLABZ_MODULE_API_MINOR,

    abi_test_qualify,
    abi_test_start,
    abi_test_stop
};


static int
abi_test_expect(
    const char *name,
    stnlabz_module_result_t actual,
    stnlabz_module_result_t expected
)
{
    printf(
        "%-32s : %-20s",
        name,
        stnlabz_module_result_string(
            actual
        )
    );


    if (
        actual != expected
    )
    {
        printf(
            " [EXPECTED %s]\n",
            stnlabz_module_result_string(
                expected
            )
        );

        return 0;
    }


    printf(
        " PASS\n"
    );


    return 1;
}


static int
abi_test_expect_state(
    const stnlabz_module_registry_t *registry,
    const char *module_id,
    stnlabz_module_state_t expected
)
{
    const stnlabz_module_record_t *record;


    record =
        stnlabz_module_registry_find(
            registry,
            module_id
        );


    if (
        record == NULL
    )
    {
        printf(
            "%-32s : NOT_FOUND FAIL\n",
            "STATE"
        );

        return 0;
    }


    printf(
        "%-32s : %-20s",
        "STATE",
        stnlabz_module_state_string(
            record->state
        )
    );


    if (
        record->state != expected
    )
    {
        printf(
            " [EXPECTED %s]\n",
            stnlabz_module_state_string(
                expected
            )
        );

        return 0;
    }


    printf(
        " PASS\n"
    );


    return 1;
}


int
stnlabz_module_abi_self_test(void)
{
    stnlabz_module_registry_t registry;

    stnlabz_module_result_t result;


    printf(
        "STN-LABZ Module ABI %u.%u Self Test\n",
        STNLABZ_MODULE_API_MAJOR,
        STNLABZ_MODULE_API_MINOR
    );


    printf(
        "============================================================\n"
    );


    stnlabz_module_registry_init(
        &registry
    );


    result =
        stnlabz_module_abi_prepare(
            &registry,
            &ABI_TEST_DESCRIPTOR
        );


    if (
        !abi_test_expect(
            "PREPARE",
            result,
            STNLABZ_MODULE_OK
        ) ||
        !abi_test_expect_state(
            &registry,
            ABI_TEST_DESCRIPTOR.id,
            STNLABZ_MODULE_STATE_QUALIFIED
        )
    )
    {
        return 1;
    }


    result =
        stnlabz_module_abi_authorize_and_activate(
            &registry,
            ABI_TEST_DESCRIPTOR.id,
            NULL
        );


    if (
        !abi_test_expect(
            "AUTHORIZE + ACTIVATE",
            result,
            STNLABZ_MODULE_OK
        ) ||
        !abi_test_expect_state(
            &registry,
            ABI_TEST_DESCRIPTOR.id,
            STNLABZ_MODULE_STATE_ACTIVE
        )
    )
    {
        return 1;
    }


    /*
     * ABI 1.4 lifecycle addition:
     *
     * ACTIVE -> STOPPED
     */

    result =
        stnlabz_module_abi_stop(
            &registry,
            ABI_TEST_DESCRIPTOR.id
        );


    if (
        !abi_test_expect(
            "STOP",
            result,
            STNLABZ_MODULE_OK
        ) ||
        !abi_test_expect_state(
            &registry,
            ABI_TEST_DESCRIPTOR.id,
            STNLABZ_MODULE_STATE_STOPPED
        )
    )
    {
        return 1;
    }


    /*
     * Same revision retains qualification but not
     * activation authority.
     */

    result =
        stnlabz_module_registry_activate(
            &registry,
            ABI_TEST_DESCRIPTOR.id
        );


    if (
        !abi_test_expect(
            "REACTIVATE WITHOUT AUTHORITY",
            result,
            STNLABZ_MODULE_ERR_NOT_AUTHORIZED
        )
    )
    {
        return 1;
    }


    result =
        stnlabz_module_abi_authorize_and_activate(
            &registry,
            ABI_TEST_DESCRIPTOR.id,
            NULL
        );


    if (
        !abi_test_expect(
            "REAUTHORIZE + REACTIVATE",
            result,
            STNLABZ_MODULE_OK
        ) ||
        !abi_test_expect_state(
            &registry,
            ABI_TEST_DESCRIPTOR.id,
            STNLABZ_MODULE_STATE_ACTIVE
        )
    )
    {
        return 1;
    }


    /*
     * True replacement boundary:
     *
     * ACTIVE -> STOPPED -> UNREGISTERED -> removed
     */

    result =
        stnlabz_module_abi_prepare_replacement(
            &registry,
            ABI_TEST_DESCRIPTOR.id
        );


    if (
        !abi_test_expect(
            "PREPARE REPLACEMENT",
            result,
            STNLABZ_MODULE_OK
        )
    )
    {
        return 1;
    }


    if (
        stnlabz_module_registry_find(
            &registry,
            ABI_TEST_DESCRIPTOR.id
        ) != NULL
    )
    {
        printf(
            "%-32s : PRESENT FAIL\n",
            "POST-UNREGISTER LOOKUP"
        );

        return 1;
    }


    printf(
        "%-32s : NOT_FOUND PASS\n",
        "POST-UNREGISTER LOOKUP"
    );


    /*
     * Replacement revision enters as a new discovery.
     *
     * This proves registry lifecycle readiness.
     * Dynamic loader replacement remains platform-owned.
     */

    result =
        stnlabz_module_abi_prepare(
            &registry,
            &ABI_TEST_DESCRIPTOR
        );


    if (
        !abi_test_expect(
            "REDISCOVER REPLACEMENT",
            result,
            STNLABZ_MODULE_OK
        ) ||
        !abi_test_expect_state(
            &registry,
            ABI_TEST_DESCRIPTOR.id,
            STNLABZ_MODULE_STATE_QUALIFIED
        )
    )
    {
        return 1;
    }


    printf(
        "============================================================\n"
    );


    printf(
        "MODULE ABI 1.4 LIFECYCLE SELF TEST PASS\n"
    );


    return 0;
}
