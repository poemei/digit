/*
 * STN-LABZ Module ABI 1.4
 * Linux dynamic module loader.
 */

#include <dlfcn.h>
#include <stddef.h>
#include <string.h>

#include "module_loader.h"

static size_t stnlabz_bounded_length(const char *value, size_t maximum)
{
    size_t length = 0;

    if (value == NULL)
    {
        return maximum;
    }

    while (length < maximum && value[length] != '\0')
    {
        ++length;
    }

    return length;
}

static int stnlabz_module_loader_descriptor_valid(
    const stnlabz_module_descriptor_t *descriptor
)
{
    size_t id_length;
    size_t name_length;

    if (descriptor == NULL)
    {
        return 0;
    }

    id_length = stnlabz_bounded_length(descriptor->id, sizeof(descriptor->id));
    name_length = stnlabz_bounded_length(descriptor->name, sizeof(descriptor->name));

    if (id_length == 0 || id_length >= sizeof(descriptor->id))
    {
        return 0;
    }

    if (name_length == 0 || name_length >= sizeof(descriptor->name))
    {
        return 0;
    }

    if (descriptor->qualify == NULL)
    {
        return 0;
    }

    return 1;
}

void stnlabz_module_loader_init(stnlabz_module_loader_t *loader)
{
    if (loader != NULL)
    {
        memset(loader, 0, sizeof(*loader));
    }
}

const stnlabz_loaded_module_t *
stnlabz_module_loader_find(
    const stnlabz_module_loader_t *loader,
    const char *module_id
)
{
    size_t index;

    if (loader == NULL || module_id == NULL)
    {
        return NULL;
    }

    for (index = 0; index < loader->count; ++index)
    {
        if (strcmp(loader->modules[index].module_id, module_id) == 0)
        {
            return &loader->modules[index];
        }
    }

    return NULL;
}

stnlabz_module_loader_result_t
stnlabz_module_loader_load(
    stnlabz_module_loader_t *loader,
    const char *expected_module_id,
    const char *module_path,
    const stnlabz_module_descriptor_t **descriptor_out
)
{
    void *handle;
    void *export_address;
    stnlabz_module_get_descriptor_fn get_descriptor;
    const stnlabz_module_descriptor_t *descriptor;
    stnlabz_loaded_module_t *loaded;
    size_t id_length;
    size_t path_length;

    if (loader == NULL || expected_module_id == NULL || module_path == NULL ||
        descriptor_out == NULL || expected_module_id[0] == '\0' ||
        module_path[0] == '\0')
    {
        return STNLABZ_MODULE_LOADER_ERR_INVALID_ARGUMENT;
    }

    *descriptor_out = NULL;

    if (stnlabz_module_loader_find(loader, expected_module_id) != NULL)
    {
        return STNLABZ_MODULE_LOADER_ERR_ALREADY_LOADED;
    }

    if (loader->count >= STNLABZ_MODULE_LOADER_MAX)
    {
        return STNLABZ_MODULE_LOADER_ERR_FULL;
    }

    id_length = strlen(expected_module_id);
    path_length = strlen(module_path);

    if (id_length == 0 || id_length >= STNLABZ_MODULE_ID_MAX ||
        path_length == 0 || path_length >= STNLABZ_MODULE_LOADER_PATH_MAX)
    {
        return STNLABZ_MODULE_LOADER_ERR_INVALID_ARGUMENT;
    }

    handle = dlopen(module_path, RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL)
    {
        return STNLABZ_MODULE_LOADER_ERR_LOAD_FAILED;
    }

    export_address = dlsym(handle, STNLABZ_MODULE_DESCRIPTOR_EXPORT);
    if (export_address == NULL)
    {
        dlclose(handle);
        return STNLABZ_MODULE_LOADER_ERR_EXPORT_MISSING;
    }

    memcpy(&get_descriptor, &export_address, sizeof(get_descriptor));
    descriptor = get_descriptor();

    if (!stnlabz_module_loader_descriptor_valid(descriptor))
    {
        dlclose(handle);
        return STNLABZ_MODULE_LOADER_ERR_DESCRIPTOR_INVALID;
    }

    if (strcmp(descriptor->id, expected_module_id) != 0)
    {
        dlclose(handle);
        return STNLABZ_MODULE_LOADER_ERR_ID_MISMATCH;
    }

    loaded = &loader->modules[loader->count];
    memset(loaded, 0, sizeof(*loaded));
    loaded->handle = handle;
    memcpy(loaded->module_id, expected_module_id, id_length + 1);
    memcpy(loaded->dll_path, module_path, path_length + 1);
    loaded->descriptor = descriptor;
    loader->count++;
    *descriptor_out = descriptor;

    return STNLABZ_MODULE_LOADER_OK;
}

stnlabz_module_loader_result_t
stnlabz_module_loader_unload(
    stnlabz_module_loader_t *loader,
    const char *module_id
)
{
    size_t index;

    if (loader == NULL || module_id == NULL || module_id[0] == '\0')
    {
        return STNLABZ_MODULE_LOADER_ERR_INVALID_ARGUMENT;
    }

    for (index = 0; index < loader->count; ++index)
    {
        if (strcmp(loader->modules[index].module_id, module_id) == 0)
        {
            size_t move_index;

            if (loader->modules[index].handle != NULL)
            {
                (void)dlclose(loader->modules[index].handle);
            }

            for (move_index = index; move_index + 1 < loader->count; ++move_index)
            {
                loader->modules[move_index] = loader->modules[move_index + 1];
            }

            memset(&loader->modules[loader->count - 1], 0,
                   sizeof(loader->modules[loader->count - 1]));
            loader->count--;
            return STNLABZ_MODULE_LOADER_OK;
        }
    }

    return STNLABZ_MODULE_LOADER_ERR_NOT_FOUND;
}

void stnlabz_module_loader_unload_all(stnlabz_module_loader_t *loader)
{
    if (loader == NULL)
    {
        return;
    }

    while (loader->count > 0)
    {
        size_t index = loader->count - 1;

        if (loader->modules[index].handle != NULL)
        {
            (void)dlclose(loader->modules[index].handle);
        }

        memset(&loader->modules[index], 0, sizeof(loader->modules[index]));
        loader->count--;
    }
}

const char *
stnlabz_module_loader_result_string(stnlabz_module_loader_result_t result)
{
    switch (result)
    {
        case STNLABZ_MODULE_LOADER_OK: return "OK";
        case STNLABZ_MODULE_LOADER_ERR_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case STNLABZ_MODULE_LOADER_ERR_FULL: return "FULL";
        case STNLABZ_MODULE_LOADER_ERR_ALREADY_LOADED: return "ALREADY_LOADED";
        case STNLABZ_MODULE_LOADER_ERR_LOAD_FAILED: return "LOAD_FAILED";
        case STNLABZ_MODULE_LOADER_ERR_EXPORT_MISSING: return "EXPORT_MISSING";
        case STNLABZ_MODULE_LOADER_ERR_DESCRIPTOR_INVALID: return "DESCRIPTOR_INVALID";
        case STNLABZ_MODULE_LOADER_ERR_ID_MISMATCH: return "ID_MISMATCH";
        case STNLABZ_MODULE_LOADER_ERR_NOT_FOUND: return "NOT_FOUND";
        default: return "UNKNOWN";
    }
}
