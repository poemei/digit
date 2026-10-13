#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "logging.h"
#include "module_manager.h"

static digit_module_result_t recorded(const char *event, const char *id,
    digit_module_result_t result)
{
    char detail[160];
    /* No admission or lifecycle decision may succeed without its audit trail. */
    if (!digit_log_is_open()) return DIGIT_MODULE_CALLBACK_FAILED;
    (void)snprintf(detail, sizeof detail, "id=%s result=%d",
                   id ? id : "unknown", (int)result);
    if (!digit_log_event("MODULE", event, detail) || !digit_log_sync())
        return DIGIT_MODULE_CALLBACK_FAILED;
    return result;
}

/* Admission gate: reject malformed evidence before changing module state. */
static int valid_qualification(const digit_module_qualification_t *q)
{
    return q && q->tests_executed >= DIGIT_MODULE_MIN_TESTS &&
        q->tests_failed == 0 &&
        q->tests_passed == q->tests_executed &&
        q->negative_test_executed == 1 &&
        q->negative_test_passed == 1;
}

static int valid_id(const char *s)
{
    size_t i;
    if (!s) return 0;
    for (i = 0; i < DIGIT_MODULE_ID_MAX; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (!c) return i > 0;
        if (!(islower(c) || isdigit(c) || c == '_' || c == '-')) return 0;
    }
    return 0;
}
static digit_module_record_t *find_mut(digit_module_manager_t *m, const char *id)
{
    size_t i;
    if (!m || !valid_id(id)) return NULL;
    for (i = 0; i < m->count; ++i)
        if (strcmp(m->modules[i].descriptor.id, id) == 0) return &m->modules[i];
    return NULL;
}
const digit_module_record_t *digit_module_find(const digit_module_manager_t *m, const char *id)
{
    size_t i;
    if (!m || !valid_id(id)) return NULL;
    for (i = 0; i < m->count; ++i)
        if (strcmp(m->modules[i].descriptor.id, id) == 0) return &m->modules[i];
    return NULL;
}
void digit_module_manager_init(digit_module_manager_t *m)
{
    if (m) memset(m, 0, sizeof(*m));
    if (digit_log_is_open()) (void)digit_log_event("MODULE", "MANAGER_INIT", "registered=0");
}
digit_module_result_t digit_module_register(digit_module_manager_t *m,
    const digit_module_descriptor_t *d, const digit_module_callbacks_t *cb)
{
    digit_module_record_t *r;
    if (!m || !d || !cb || !valid_id(d->id) || !cb->start || !cb->stop ||
        d->abi_major != DIGIT_MODULE_ABI_MAJOR ||
        d->abi_minor > DIGIT_MODULE_ABI_MINOR) return recorded("REGISTER", d ? d->id : NULL, DIGIT_MODULE_INVALID);
    if (find_mut(m, d->id)) return recorded("REGISTER", d ? d->id : NULL, DIGIT_MODULE_DUPLICATE);
    if (m->count >= DIGIT_MODULE_MAX) return recorded("REGISTER", d ? d->id : NULL, DIGIT_MODULE_FULL);
    if (recorded("REGISTER", d->id, DIGIT_MODULE_OK) != DIGIT_MODULE_OK)
        return DIGIT_MODULE_CALLBACK_FAILED;
    r = &m->modules[m->count++];
    memset(r, 0, sizeof(*r));
    r->descriptor = *d;
    r->callbacks = *cb;
    r->state = DIGIT_MODULE_REGISTERED;
    return DIGIT_MODULE_OK;
}
digit_module_result_t digit_module_qualify(digit_module_manager_t *m,
    const char *id, const digit_module_qualification_t *q)
{
    digit_module_record_t *r = find_mut(m, id);
    if (!r) return recorded("QUALIFY", id, DIGIT_MODULE_NOT_FOUND);
    if (r->state != DIGIT_MODULE_REGISTERED || !valid_qualification(q))
        return recorded("QUALIFY", id, DIGIT_MODULE_UNQUALIFIED);
    if (recorded("QUALIFY", id, DIGIT_MODULE_OK) != DIGIT_MODULE_OK)
        return DIGIT_MODULE_CALLBACK_FAILED;
    r->qualification = *q;
    r->state = DIGIT_MODULE_QUALIFIED;
    return DIGIT_MODULE_OK;
}
digit_module_result_t digit_module_authorize(digit_module_manager_t *m,
    const char *id, int operator_authorized)
{
    digit_module_record_t *r = find_mut(m, id);
    if (!r) return recorded("AUTHORIZE", id, DIGIT_MODULE_NOT_FOUND);
    if (r->state != DIGIT_MODULE_QUALIFIED && r->state != DIGIT_MODULE_STOPPED)
        return recorded("AUTHORIZE", id, DIGIT_MODULE_UNQUALIFIED);
    if (operator_authorized != 1) {
        r->activation_authorized = 0;
        return recorded("AUTHORIZE", id, DIGIT_MODULE_UNQUALIFIED);
    }
    if (!valid_qualification(&r->qualification))
        return recorded("AUTHORIZE", id, DIGIT_MODULE_UNQUALIFIED);
    if (recorded("AUTHORIZE", id, DIGIT_MODULE_OK) != DIGIT_MODULE_OK)
        return DIGIT_MODULE_CALLBACK_FAILED;
    r->activation_authorized = 1;
    return DIGIT_MODULE_OK;
}
digit_module_result_t digit_module_activate(digit_module_manager_t *m, const char *id)
{
    digit_module_record_t *r = find_mut(m, id);
    if (!r) return recorded("ACTIVATE", id, DIGIT_MODULE_NOT_FOUND);
    if ((r->state != DIGIT_MODULE_QUALIFIED && r->state != DIGIT_MODULE_STOPPED) ||
        !valid_qualification(&r->qualification) || !r->activation_authorized)
        return recorded("ACTIVATE", id, DIGIT_MODULE_UNQUALIFIED);
    if (recorded("ACTIVATE_ATTEMPT", id, DIGIT_MODULE_OK) != DIGIT_MODULE_OK)
        return DIGIT_MODULE_CALLBACK_FAILED;
    if (!r->callbacks.start(r->callbacks.context)) return recorded("ACTIVATE", id, DIGIT_MODULE_CALLBACK_FAILED);
    r->state = DIGIT_MODULE_ACTIVE;
    return recorded("ACTIVATE", id, DIGIT_MODULE_OK);
}
digit_module_result_t digit_module_stop(digit_module_manager_t *m, const char *id)
{
    digit_module_record_t *r = find_mut(m, id);
    if (!r) return recorded("STOP", id, DIGIT_MODULE_NOT_FOUND);
    if (r->state != DIGIT_MODULE_ACTIVE) return recorded("STOP", id, DIGIT_MODULE_INVALID);
    if (recorded("STOP_ATTEMPT", id, DIGIT_MODULE_OK) != DIGIT_MODULE_OK)
        return DIGIT_MODULE_CALLBACK_FAILED;
    r->callbacks.stop(r->callbacks.context);
    r->state = DIGIT_MODULE_STOPPED;
    r->activation_authorized = 0;
    return recorded("STOP", id, DIGIT_MODULE_OK);
}
void digit_module_manager_shutdown(digit_module_manager_t *m)
{
    size_t i;
    if (!m) return;
    for (i = m->count; i > 0; --i) {
        digit_module_record_t *r = &m->modules[i-1];
        if (r->state == DIGIT_MODULE_ACTIVE) {
            r->callbacks.stop(r->callbacks.context);
            r->state = DIGIT_MODULE_STOPPED;
            (void)recorded("SHUTDOWN_STOP", r->descriptor.id, DIGIT_MODULE_OK);
        }
        r->activation_authorized = 0;
    }
    (void)recorded("MANAGER_SHUTDOWN", NULL, DIGIT_MODULE_OK);
}
