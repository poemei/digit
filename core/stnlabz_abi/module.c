/*
 * STN-LABZ
 * Module ABI 1.4
 *
 * module.c
 *
 * Common module lifecycle and result strings.
 */

#include "module.h"


const char *
stnlabz_module_state_string(
    stnlabz_module_state_t state
)
{
    switch (
        state
    )
    {
        case STNLABZ_MODULE_STATE_DISCOVERED:

            return "DISCOVERED";


        case STNLABZ_MODULE_STATE_UNVERIFIED:

            return "UNVERIFIED";


        case STNLABZ_MODULE_STATE_TESTING:

            return "TESTING";


        case STNLABZ_MODULE_STATE_QUALIFIED:

            return "QUALIFIED";


        case STNLABZ_MODULE_STATE_ACTIVE:

            return "ACTIVE";


        case STNLABZ_MODULE_STATE_STOPPED:

            return "STOPPED";


        case STNLABZ_MODULE_STATE_FAILED:

            return "FAILED";


        case STNLABZ_MODULE_STATE_QUARANTINED:

            return "QUARANTINED";


        case STNLABZ_MODULE_STATE_UNREGISTERED:

            return "UNREGISTERED";


        default:

            return "UNKNOWN";
    }
}


const char *
stnlabz_module_result_string(
    stnlabz_module_result_t result
)
{
    switch (
        result
    )
    {
        case STNLABZ_MODULE_OK:

            return "OK";


        case STNLABZ_MODULE_ERR_INVALID_ARGUMENT:

            return "INVALID_ARGUMENT";


        case STNLABZ_MODULE_ERR_INVALID_IDENTITY:

            return "INVALID_IDENTITY";


        case STNLABZ_MODULE_ERR_DUPLICATE:

            return "DUPLICATE";


        case STNLABZ_MODULE_ERR_REGISTRY_FULL:

            return "REGISTRY_FULL";


        case STNLABZ_MODULE_ERR_NOT_FOUND:

            return "NOT_FOUND";


        case STNLABZ_MODULE_ERR_INCOMPATIBLE:

            return "INCOMPATIBLE";


        case STNLABZ_MODULE_ERR_INVALID_STATE:

            return "INVALID_STATE";


        case STNLABZ_MODULE_ERR_QUALIFICATION:

            return "QUALIFICATION_FAILED";


        case STNLABZ_MODULE_ERR_NOT_QUALIFIED:

            return "NOT_QUALIFIED";


        case STNLABZ_MODULE_ERR_NOT_AUTHORIZED:

            return "NOT_AUTHORIZED";


        case STNLABZ_MODULE_ERR_QUARANTINED:

            return "QUARANTINED";


        case STNLABZ_MODULE_ERR_AUDIT_FULL:

            return "AUDIT_FULL";


        case STNLABZ_MODULE_ERR_START_FAILED:

            return "START_FAILED";


        case STNLABZ_MODULE_ERR_STOP_FAILED:

            return "STOP_FAILED";


        default:

            return "UNKNOWN";
    }
}
