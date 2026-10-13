/*
 * STN-LABZ
 * Module ABI 1.4
 *
 * module_registry.c
 *
 * Core-controlled module lifecycle registry.
 */

#include <string.h>

#include "module_registry.h"


static int
stnlabz_module_text_valid(
    const char *text,
    size_t capacity
)
{
    size_t length;


    if (
        text == NULL ||
        capacity == 0
    )
    {
        return 0;
    }


    length =
        strlen(
            text
        );


    if (
        length == 0 ||
        length >= capacity
    )
    {
        return 0;
    }


    return 1;
}


static stnlabz_module_record_t *
stnlabz_module_registry_find_mutable(
    stnlabz_module_registry_t *registry,
    const char *module_id
)
{
    size_t index;


    if (
        registry == NULL ||
        module_id == NULL
    )
    {
        return NULL;
    }


    for (
        index = 0;
        index < registry->count;
        ++index
    )
    {
        if (
            strcmp(
                registry
                    ->modules[index]
                    .descriptor
                    .id,
                module_id
            ) == 0
        )
        {
            return
                &registry->modules[index];
        }
    }


    return NULL;
}



void
stnlabz_module_registry_init(
    stnlabz_module_registry_t *registry
)
{
    if (
        registry == NULL
    )
    {
        return;
    }


    memset(
        registry,
        0,
        sizeof(*registry)
    );
}


stnlabz_module_result_t
stnlabz_module_registry_discover(
    stnlabz_module_registry_t *registry,
    const stnlabz_module_descriptor_t *descriptor
)
{
    stnlabz_module_record_t *record;


    if (
        registry == NULL ||
        descriptor == NULL
    )
    {
        return
            STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    }


    if (
        !stnlabz_module_text_valid(
            descriptor->id,
            sizeof(descriptor->id)
        ) ||
        !stnlabz_module_text_valid(
            descriptor->name,
            sizeof(descriptor->name)
        ) ||
        descriptor->qualify == NULL
    )
    {
        return
            STNLABZ_MODULE_ERR_INVALID_IDENTITY;
    }


    if (
        stnlabz_module_registry_find_mutable(
            registry,
            descriptor->id
        ) != NULL
    )
    {
        return
            STNLABZ_MODULE_ERR_DUPLICATE;
    }


    if (
        registry->count >=
        STNLABZ_MODULE_REGISTRY_MAX
    )
    {
        return
            STNLABZ_MODULE_ERR_REGISTRY_FULL;
    }


    record =
        &registry->modules[
            registry->count
        ];


    memset(
        record,
        0,
        sizeof(*record)
    );


    record->descriptor =
        *descriptor;


    record->state =
        STNLABZ_MODULE_STATE_DISCOVERED;


    record->activation_authorized =
        0;


    


    registry->count++;


    return
        STNLABZ_MODULE_OK;
}


stnlabz_module_result_t
stnlabz_module_registry_verify(
    stnlabz_module_registry_t *registry,
    const char *module_id
)
{
    stnlabz_module_record_t *record;


    record =
        stnlabz_module_registry_find_mutable(
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
        STNLABZ_MODULE_STATE_DISCOVERED
    )
    {
        return
            STNLABZ_MODULE_ERR_INVALID_STATE;
    }


    if (
        record
            ->descriptor
            .required_core_api_major !=
            STNLABZ_MODULE_API_MAJOR ||
        record
            ->descriptor
            .required_core_api_minor >
            STNLABZ_MODULE_API_MINOR
    )
    {
        record->state =
            STNLABZ_MODULE_STATE_FAILED;


        


        return
            STNLABZ_MODULE_ERR_INCOMPATIBLE;
    }


    record->state =
        STNLABZ_MODULE_STATE_UNVERIFIED;


    


    return
        STNLABZ_MODULE_OK;
}


stnlabz_module_result_t
stnlabz_module_registry_qualify(
    stnlabz_module_registry_t *registry,
    const char *module_id
)
{
    stnlabz_module_record_t *record;

    stnlabz_module_result_t module_result;

    stnlabz_module_qualification_result_t report;


    record =
        stnlabz_module_registry_find_mutable(
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
        STNLABZ_MODULE_STATE_UNVERIFIED
    )
    {
        return
            STNLABZ_MODULE_ERR_INVALID_STATE;
    }


    record->state =
        STNLABZ_MODULE_STATE_TESTING;


    


    memset(
        &report,
        0,
        sizeof(report)
    );


    module_result =
        record
            ->descriptor
            .qualify(
                &report
            );


    record->qualification =
        report;


    if (
        module_result !=
            STNLABZ_MODULE_OK ||
        report.tests_executed <
            STNLABZ_MODULE_MIN_TESTS ||
        report.tests_passed !=
            report.tests_executed ||
        report.tests_failed != 0 ||
        report.tests_passed +
            report.tests_failed !=
            report.tests_executed ||
        !report.negative_test_executed ||
        !report.negative_test_passed
    )
    {
        record->state =
            STNLABZ_MODULE_STATE_FAILED;


        record->activation_authorized =
            0;


        


        return
            STNLABZ_MODULE_ERR_QUALIFICATION;
    }


    record->state =
        STNLABZ_MODULE_STATE_QUALIFIED;


    


    return
        STNLABZ_MODULE_OK;
}


stnlabz_module_result_t
stnlabz_module_registry_restore_qualification(
    stnlabz_module_registry_t *registry,
    const char *module_id,
    const stnlabz_module_qualification_result_t *qualification
)
{
    stnlabz_module_record_t *record;


    if (
        registry == NULL ||
        module_id == NULL ||
        qualification == NULL
    )
    {
        return
            STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    }


    record =
        stnlabz_module_registry_find_mutable(
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
        STNLABZ_MODULE_STATE_UNVERIFIED
    )
    {
        return
            STNLABZ_MODULE_ERR_INVALID_STATE;
    }


    if (
        qualification->tests_executed <
            STNLABZ_MODULE_MIN_TESTS ||
        qualification->tests_passed !=
            qualification->tests_executed ||
        qualification->tests_failed != 0 ||
        qualification->tests_passed +
            qualification->tests_failed !=
            qualification->tests_executed ||
        !qualification->negative_test_executed ||
        !qualification->negative_test_passed
    )
    {
        return
            STNLABZ_MODULE_ERR_QUALIFICATION;
    }


    record->qualification =
        *qualification;


    record->activation_authorized =
        0;


    record->state =
        STNLABZ_MODULE_STATE_QUALIFIED;


    


    return
        STNLABZ_MODULE_OK;
}


stnlabz_module_result_t
stnlabz_module_registry_authorize_activation(
    stnlabz_module_registry_t *registry,
    const char *module_id
)
{
    stnlabz_module_record_t *record;


    record =
        stnlabz_module_registry_find_mutable(
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


    /*
     * ABI 1.4:
     *
     * A STOPPED module retains qualification and may
     * be explicitly authorized for reactivation.
     */

    if (
        record->state !=
            STNLABZ_MODULE_STATE_QUALIFIED &&
        record->state !=
            STNLABZ_MODULE_STATE_STOPPED
    )
    {
        return
            STNLABZ_MODULE_ERR_NOT_QUALIFIED;
    }


    record->activation_authorized =
        1;


    


    return
        STNLABZ_MODULE_OK;
}


stnlabz_module_result_t
stnlabz_module_registry_activate(
    stnlabz_module_registry_t *registry,
    const char *module_id
)
{
    stnlabz_module_record_t *record;


    record =
        stnlabz_module_registry_find_mutable(
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
            STNLABZ_MODULE_STATE_QUALIFIED &&
        record->state !=
            STNLABZ_MODULE_STATE_STOPPED
    )
    {
        return
            STNLABZ_MODULE_ERR_NOT_QUALIFIED;
    }


    if (
        !record->activation_authorized
    )
    {
        return
            STNLABZ_MODULE_ERR_NOT_AUTHORIZED;
    }


    record->state =
        STNLABZ_MODULE_STATE_ACTIVE;


    


    return
        STNLABZ_MODULE_OK;
}


stnlabz_module_result_t
stnlabz_module_registry_stop(
    stnlabz_module_registry_t *registry,
    const char *module_id
)
{
    stnlabz_module_record_t *record;


    record =
        stnlabz_module_registry_find_mutable(
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


    /*
     * Stopping does not destroy qualification.
     *
     * Activation authority does not survive stop.
     */

    record->activation_authorized =
        0;


    record->state =
        STNLABZ_MODULE_STATE_STOPPED;


    


    return
        STNLABZ_MODULE_OK;
}


stnlabz_module_result_t
stnlabz_module_registry_unregister(
    stnlabz_module_registry_t *registry,
    const char *module_id
)
{
    size_t index;


    if (
        registry == NULL ||
        module_id == NULL ||
        module_id[0] == '\0'
    )
    {
        return
            STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    }


    for (
        index = 0;
        index < registry->count;
        ++index
    )
    {
        stnlabz_module_record_t *record;

        size_t move_index;


        record =
            &registry->modules[index];


        if (
            strcmp(
                record->descriptor.id,
                module_id
            ) != 0
        )
        {
            continue;
        }


        /*
         * Executing code cannot disappear from the
         * registry.
         */

        if (
            record->state ==
                STNLABZ_MODULE_STATE_ACTIVE ||
            record->state ==
                STNLABZ_MODULE_STATE_TESTING ||
            record->state ==
                STNLABZ_MODULE_STATE_UNVERIFIED ||
            record->state ==
                STNLABZ_MODULE_STATE_DISCOVERED
        )
        {
            return
                STNLABZ_MODULE_ERR_INVALID_STATE;
        }


        record->activation_authorized =
            0;


        record->state =
            STNLABZ_MODULE_STATE_UNREGISTERED;


        


        /* Remove the live registry record. */

        for (
            move_index = index;
            move_index + 1 <
                registry->count;
            ++move_index
        )
        {
            registry->modules[
                move_index
            ] =
                registry->modules[
                    move_index + 1
                ];
        }


        memset(
            &registry->modules[
                registry->count - 1
            ],
            0,
            sizeof(
                registry->modules[
                    registry->count - 1
                ]
            )
        );


        registry->count--;


        return
            STNLABZ_MODULE_OK;
    }


    return
        STNLABZ_MODULE_ERR_NOT_FOUND;
}


stnlabz_module_result_t
stnlabz_module_registry_fail(
    stnlabz_module_registry_t *registry,
    const char *module_id
)
{
    stnlabz_module_record_t *record;


    record =
        stnlabz_module_registry_find_mutable(
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


    record->state =
        STNLABZ_MODULE_STATE_FAILED;


    record->activation_authorized =
        0;


    return STNLABZ_MODULE_OK;
}


stnlabz_module_result_t
stnlabz_module_registry_quarantine(
    stnlabz_module_registry_t *registry,
    const char *module_id
)
{
    stnlabz_module_record_t *record;


    record =
        stnlabz_module_registry_find_mutable(
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


    record->state =
        STNLABZ_MODULE_STATE_QUARANTINED;


    record->activation_authorized =
        0;


    return STNLABZ_MODULE_OK;
}


const stnlabz_module_record_t *
stnlabz_module_registry_find(
    const stnlabz_module_registry_t *registry,
    const char *module_id
)
{
    size_t index;


    if (
        registry == NULL ||
        module_id == NULL
    )
    {
        return NULL;
    }


    for (
        index = 0;
        index < registry->count;
        ++index
    )
    {
        if (
            strcmp(
                registry
                    ->modules[index]
                    .descriptor
                    .id,
                module_id
            ) == 0
        )
        {
            return
                &registry->modules[index];
        }
    }


    return NULL;
}
