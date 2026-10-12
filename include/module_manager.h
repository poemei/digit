#ifndef DIGIT_MODULE_MANAGER_H
#define DIGIT_MODULE_MANAGER_H
#include <stddef.h>
#define DIGIT_MODULE_MAX 32
#define DIGIT_MODULE_ID_MAX 64
#define DIGIT_MODULE_ABI_MAJOR 1U
#define DIGIT_MODULE_ABI_MINOR 0U
#define DIGIT_MODULE_MIN_TESTS 1U

typedef enum {
    DIGIT_MODULE_REGISTERED = 0,
    DIGIT_MODULE_QUALIFIED = 1,
    DIGIT_MODULE_ACTIVE = 2,
    DIGIT_MODULE_STOPPED = 3
} digit_module_state_t;
typedef enum {
    DIGIT_MODULE_OK = 0,
    DIGIT_MODULE_INVALID = 1,
    DIGIT_MODULE_DUPLICATE = 2,
    DIGIT_MODULE_FULL = 3,
    DIGIT_MODULE_NOT_FOUND = 4,
    DIGIT_MODULE_UNQUALIFIED = 5,
    DIGIT_MODULE_CALLBACK_FAILED = 6
} digit_module_result_t;
typedef struct {
    char id[DIGIT_MODULE_ID_MAX];
    unsigned int abi_major, abi_minor;
    unsigned int version_major, version_minor, version_patch;
} digit_module_descriptor_t;
typedef struct {
    unsigned int tests_executed, tests_passed, tests_failed;
    int negative_test_executed, negative_test_passed;
} digit_module_qualification_t;
typedef struct {
    int (*start)(void *context);
    void (*stop)(void *context);
    void *context;
} digit_module_callbacks_t;
typedef struct {
    digit_module_descriptor_t descriptor;
    digit_module_qualification_t qualification;
    digit_module_callbacks_t callbacks;
    digit_module_state_t state;
    int activation_authorized;
} digit_module_record_t;
typedef struct {
    digit_module_record_t modules[DIGIT_MODULE_MAX];
    size_t count;
} digit_module_manager_t;

void digit_module_manager_init(digit_module_manager_t *manager);
digit_module_result_t digit_module_register(digit_module_manager_t *manager,
    const digit_module_descriptor_t *descriptor, const digit_module_callbacks_t *callbacks);
digit_module_result_t digit_module_qualify(digit_module_manager_t *manager,
    const char *id, const digit_module_qualification_t *evidence);
digit_module_result_t digit_module_authorize(digit_module_manager_t *manager,
    const char *id, int operator_authorized);
digit_module_result_t digit_module_activate(digit_module_manager_t *manager, const char *id);
digit_module_result_t digit_module_stop(digit_module_manager_t *manager, const char *id);
void digit_module_manager_shutdown(digit_module_manager_t *manager);
const digit_module_record_t *digit_module_find(const digit_module_manager_t *manager, const char *id);
#endif
