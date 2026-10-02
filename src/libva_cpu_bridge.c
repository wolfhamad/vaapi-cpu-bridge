#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <va/va.h>
#include <va/va_backend.h>

/*
 * Prototype libva driver shim for the vaapi-cpu-bridge project.
 *
 * This driver loads the real VA-API driver and injects software AV1
 * capability reporting while proxying all other calls to the GPU driver.
 *
 * Build with:
 *   bash scripts/build-libva-shim.sh
 *
 * Then use with:
 *   export LIBVA_DRIVERS_PATH=$(pwd)/build
 *   export LIBVA_DRIVER_NAME=cpu_bridge
 *   export VAAPI_REAL_DRIVER=/usr/lib/arm-linux-gnueabihf/dri/libva_mesa.so
 */

/* Global state */
static void *g_real_driver_handle = NULL;
static VADriverContextP g_real_ctx = NULL;

/* Real driver function pointers */
static VAStatus (*g_real_vaQueryConfigProfiles)(VADriverContextP, VAProfile *, int *);
static VAStatus (*g_real_vaQueryConfigEntrypoints)(VADriverContextP, VAProfile, VAEntrypoint *, int *);

/*
 * Load the real VA-API driver and extract the vtable.
 */
static VAStatus load_real_driver(void) {
    if (g_real_driver_handle != NULL) {
        return VA_STATUS_SUCCESS;
    }

    const char *real_driver_path = getenv("VAAPI_REAL_DRIVER");
    if (!real_driver_path || real_driver_path[0] == '\0') {
        real_driver_path = "/usr/lib/arm-linux-gnueabihf/dri/libva_mesa.so";
    }

    fprintf(stderr, "[vaapi-cpu-bridge] Loading real driver: %s\n", real_driver_path);
    
    g_real_driver_handle = dlopen(real_driver_path, RTLD_LAZY | RTLD_GLOBAL);
    if (!g_real_driver_handle) {
        fprintf(stderr, "[vaapi-cpu-bridge] Failed to load real driver: %s\n", dlerror());
        return VA_STATUS_ERROR_UNKNOWN;
    }

    fprintf(stderr, "[vaapi-cpu-bridge] Real driver loaded successfully\n");
    return VA_STATUS_SUCCESS;
}

/*
 * Hook: vaQueryConfigProfiles
 * Inject VAProfileAV1Profile0 alongside hardware profiles.
 */
static VAStatus vaapi_QueryConfigProfiles(VADriverContextP ctx, VAProfile *profile_list, int *num_profiles) {
    if (!g_real_vaQueryConfigProfiles || !g_real_ctx) {
        fprintf(stderr, "[vaapi-cpu-bridge] Real vaQueryConfigProfiles not available\n");
        return VA_STATUS_ERROR_UNKNOWN;
    }

    /* Call the real driver */
    VAStatus status = g_real_vaQueryConfigProfiles(g_real_ctx, profile_list, num_profiles);
    if (status != VA_STATUS_SUCCESS) {
        return status;
    }

    /* Check if AV1 is already present */
    int av1_found = 0;
    for (int i = 0; i < *num_profiles; i++) {
        if (profile_list[i] == VAProfileAV1Profile0) {
            av1_found = 1;
            break;
        }
    }

    /* Inject AV1 if not present and there's space */
    if (!av1_found && *num_profiles < 32) {
        profile_list[*num_profiles] = VAProfileAV1Profile0;
        (*num_profiles)++;
        fprintf(stderr, "[vaapi-cpu-bridge] Injected VAProfileAV1Profile0\n");
    }

    return VA_STATUS_SUCCESS;
}

/*
 * Hook: vaQueryConfigEntrypoints
 * Provide VLD entrypoint for AV1 decode.
 */
static VAStatus vaapi_QueryConfigEntrypoints(VADriverContextP ctx, VAProfile profile,
                                               VAEntrypoint *entrypoint_list, int *num_entrypoints) {
    if (profile == VAProfileAV1Profile0) {
        /* For AV1, we provide a software decode entrypoint */
        if (*num_entrypoints > 0 && entrypoint_list) {
            entrypoint_list[0] = VAEntrypointVLD;
            *num_entrypoints = 1;
            fprintf(stderr, "[vaapi-cpu-bridge] Provided AV1 VLD entrypoint\n");
        }
        return VA_STATUS_SUCCESS;
    }

    /* For other profiles, proxy to the real driver */
    if (!g_real_vaQueryConfigEntrypoints || !g_real_ctx) {
        fprintf(stderr, "[vaapi-cpu-bridge] Real vaQueryConfigEntrypoints not available\n");
        return VA_STATUS_ERROR_UNKNOWN;
    }

    return g_real_vaQueryConfigEntrypoints(g_real_ctx, profile, entrypoint_list, num_entrypoints);
}

/*
 * libva driver entry point: __vaDriverInit_1_0
 * This is the mandatory function that libva calls to initialize the driver.
 */
VAStatus __vaDriverInit_1_0(VADriverContextP ctx) {
    fprintf(stderr, "[vaapi-cpu-bridge] __vaDriverInit_1_0 called\n");

    if (!ctx) {
        fprintf(stderr, "[vaapi-cpu-bridge] Invalid context\n");
        return VA_STATUS_ERROR_INVALID_DISPLAY;
    }

    /* Load the real driver first */
    VAStatus status = load_real_driver();
    if (status != VA_STATUS_SUCCESS) {
        fprintf(stderr, "[vaapi-cpu-bridge] Failed to load real driver\n");
        return status;
    }

    /* Try to get the real driver's init function */
    typedef VAStatus (*vaDriverInit_func)(VADriverContextP);
    vaDriverInit_func real_init = (vaDriverInit_func)dlsym(g_real_driver_handle, "__vaDriverInit_1_0");
    
    if (!real_init) {
        fprintf(stderr, "[vaapi-cpu-bridge] Real driver's __vaDriverInit_1_0 not found\n");
        return VA_STATUS_ERROR_UNKNOWN;
    }

    /* Create a temporary context to initialize the real driver */
    g_real_ctx = (VADriverContextP)malloc(sizeof(struct VADriverContext));
    if (!g_real_ctx) {
        fprintf(stderr, "[vaapi-cpu-bridge] Failed to allocate context\n");
        return VA_STATUS_ERROR_UNKNOWN;
    }

    memcpy(g_real_ctx, ctx, sizeof(struct VADriverContext));

    /* Initialize the real driver */
    status = real_init(g_real_ctx);
    if (status != VA_STATUS_SUCCESS) {
        fprintf(stderr, "[vaapi-cpu-bridge] Real driver initialization failed\n");
        free(g_real_ctx);
        g_real_ctx = NULL;
        return status;
    }

    /* Extract function pointers from the real driver's vtable */
    if (g_real_ctx->vtable) {
        g_real_vaQueryConfigProfiles = g_real_ctx->vtable->vaQueryConfigProfiles;
        g_real_vaQueryConfigEntrypoints = g_real_ctx->vtable->vaQueryConfigEntrypoints;
    }

    /* Copy the real driver's vtable to our context */
    if (g_real_ctx->vtable) {
        memcpy(ctx->vtable, g_real_ctx->vtable, sizeof(struct VADriverVTable));
    }

    /* Override only the functions we want to intercept */
    ctx->vtable->vaQueryConfigProfiles = vaapi_QueryConfigProfiles;
    ctx->vtable->vaQueryConfigEntrypoints = vaapi_QueryConfigEntrypoints;

    fprintf(stderr, "[vaapi-cpu-bridge] Driver initialized successfully\n");
    return VA_STATUS_SUCCESS;
}
