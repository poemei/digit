#include <stdatomic.h>
#include <string.h>

#include "module_host.h"
#include "digit_transport.h"

#define DIGIT_HOST_SERVICE_MAX 32U
#define DIGIT_HOST_COMMAND_MAX 32U

typedef struct {
    char name[STNLABZ_MODULE_SERVICE_NAME_MAX];
    stnlabz_module_service_handler_fn handler;
    void *context;
    unsigned int in_flight;
    int withdrawing;
} digit_service_entry_t;

typedef struct {
    char name[STNLABZ_MODULE_COMMAND_NAME_MAX];
    stnlabz_module_command_handler_fn handler;
    void *context;
} digit_command_entry_t;

static digit_service_entry_t services[DIGIT_HOST_SERVICE_MAX];
static digit_command_entry_t commands[DIGIT_HOST_COMMAND_MAX];
static atomic_flag service_lock = ATOMIC_FLAG_INIT;
static atomic_flag command_lock = ATOMIC_FLAG_INIT;

static size_t bounded_length(const char *text, size_t maximum)
{
    size_t length = 0U;
    if (!text) return maximum;
    while (length < maximum && text[length]) ++length;
    return length;
}

static void lock(atomic_flag *flag)
{
    while (atomic_flag_test_and_set_explicit(flag, memory_order_acquire)) {}
}

static void unlock(atomic_flag *flag)
{
    atomic_flag_clear_explicit(flag, memory_order_release);
}

static int register_service(const char *name,
                            stnlabz_module_service_handler_fn handler,
                            void *context)
{
    size_t i;
    size_t free_slot = DIGIT_HOST_SERVICE_MAX;
    size_t length;

    if (!name || !handler) return 0;
    length = bounded_length(name, STNLABZ_MODULE_SERVICE_NAME_MAX);
    if (!length || length >= STNLABZ_MODULE_SERVICE_NAME_MAX) return 0;

    lock(&service_lock);
    for (i = 0; i < DIGIT_HOST_SERVICE_MAX; ++i) {
        if (services[i].handler && !strcmp(services[i].name, name)) {
            unlock(&service_lock);
            return 0;
        }
        if (!services[i].handler && free_slot == DIGIT_HOST_SERVICE_MAX)
            free_slot = i;
    }
    if (free_slot == DIGIT_HOST_SERVICE_MAX) {
        unlock(&service_lock);
        return 0;
    }

    memcpy(services[free_slot].name, name, length + 1U);
    services[free_slot].handler = handler;
    services[free_slot].context = context;
    unlock(&service_lock);
    return 1;
}

static int unregister_service(const char *name, void *context)
{
    digit_service_entry_t *entry = NULL;
    size_t i;

    if (!name) return 0;
    lock(&service_lock);
    for (i = 0; i < DIGIT_HOST_SERVICE_MAX; ++i) {
        if (services[i].handler &&
            !services[i].withdrawing &&
            services[i].context == context &&
            !strcmp(services[i].name, name)) {
            services[i].withdrawing = 1;
            entry = &services[i];
            break;
        }
    }
    unlock(&service_lock);
    if (!entry) return 0;

    for (;;) {
        lock(&service_lock);
        if (!entry->in_flight) {
            memset(entry, 0, sizeof *entry);
            unlock(&service_lock);
            return 1;
        }
        unlock(&service_lock);
    }
}

static stnlabz_module_result_t invoke_service(
    const char *name,
    const void *request,
    size_t request_size,
    void *response,
    size_t response_size,
    size_t *response_used)
{
    digit_service_entry_t *entry = NULL;
    stnlabz_module_service_handler_fn handler = NULL;
    void *context = NULL;
    stnlabz_module_result_t result;
    size_t i;

    if (response_used) *response_used = 0U;
    if (!name || (request_size && !request) || (response_size && !response))
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;

    lock(&service_lock);
    for (i = 0; i < DIGIT_HOST_SERVICE_MAX; ++i) {
        if (services[i].handler &&
            !services[i].withdrawing &&
            !strcmp(services[i].name, name)) {
            entry = &services[i];
            entry->in_flight++;
            handler = entry->handler;
            context = entry->context;
            break;
        }
    }
    unlock(&service_lock);

    if (!handler) return STNLABZ_MODULE_ERR_NOT_FOUND;

    result = handler(request, request_size, response, response_size,
                     response_used, context);

    lock(&service_lock);
    entry->in_flight--;
    unlock(&service_lock);
    return result;
}

static int send_message(const char *message)
{
    digit_transport_channel_request_t request;
    size_t length;

    if (!message) return 0;
    length = bounded_length(message, sizeof request.message);
    if (!length || length >= sizeof request.message) return 0;
    memset(&request, 0, sizeof request);
    memcpy(request.message, message, length + 1U);

    return invoke_service(DIGIT_TRANSPORT_CHANNEL_SEND_SERVICE,
                          &request, sizeof request,
                          NULL, 0U, NULL) == STNLABZ_MODULE_OK;
}

static int send_private_message(const char *target, const char *message)
{
    digit_transport_private_request_t request;
    size_t target_length;
    size_t message_length;

    if (!target || !message) return 0;
    target_length = bounded_length(target, sizeof request.target);
    message_length = bounded_length(message, sizeof request.message);
    if (!target_length || target_length >= sizeof request.target ||
        !message_length || message_length >= sizeof request.message)
        return 0;

    memset(&request, 0, sizeof request);
    memcpy(request.target, target, target_length + 1U);
    memcpy(request.message, message, message_length + 1U);

    return invoke_service(DIGIT_TRANSPORT_PRIVATE_SEND_SERVICE,
                          &request, sizeof request,
                          NULL, 0U, NULL) == STNLABZ_MODULE_OK;
}

static int register_command(const char *name,
                            stnlabz_module_command_handler_fn handler,
                            void *context)
{
    size_t i;
    size_t free_slot = DIGIT_HOST_COMMAND_MAX;
    size_t length;

    if (!name || !handler) return 0;
    length = bounded_length(name, STNLABZ_MODULE_COMMAND_NAME_MAX);
    if (!length || length >= STNLABZ_MODULE_COMMAND_NAME_MAX) return 0;

    lock(&command_lock);
    for (i = 0; i < DIGIT_HOST_COMMAND_MAX; ++i) {
        if (commands[i].handler && !strcmp(commands[i].name, name)) {
            unlock(&command_lock);
            return 0;
        }
        if (!commands[i].handler && free_slot == DIGIT_HOST_COMMAND_MAX)
            free_slot = i;
    }
    if (free_slot == DIGIT_HOST_COMMAND_MAX) {
        unlock(&command_lock);
        return 0;
    }

    memcpy(commands[free_slot].name, name, length + 1U);
    commands[free_slot].handler = handler;
    commands[free_slot].context = context;
    unlock(&command_lock);
    return 1;
}

static int unregister_command(const char *name, void *context)
{
    size_t i;

    if (!name) return 0;
    lock(&command_lock);
    for (i = 0; i < DIGIT_HOST_COMMAND_MAX; ++i) {
        if (commands[i].handler &&
            commands[i].context == context &&
            !strcmp(commands[i].name, name)) {
            memset(&commands[i], 0, sizeof commands[i]);
            unlock(&command_lock);
            return 1;
        }
    }
    unlock(&command_lock);
    return 0;
}

static const stnlabz_module_host_t host = {
    .send_message = send_message,
    .send_private_message = send_private_message,
    .register_command = register_command,
    .unregister_command = unregister_command,
    .register_service = register_service,
    .unregister_service = unregister_service,
    .invoke_service = invoke_service
};

const stnlabz_module_host_t *digit_module_host(void)
{
    return &host;
}
