#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <va/va.h>
#include <va/va_backend.h>

/*
 * Prototype libva driver shim for the vaapi-cpu-bridge project.
 *
 * This is NOT a complete production VA-API driver. It is meant to demonstrate
 * the pattern used to preserve the real GPU-backed VA-API driver while injecting
 * software AV1 capability reporting for Chromium/Chrome when the browser asks
 * for decode capability information.
 *
 * Build with:
 *   gcc -shared -fPIC -o build/libva_cpu_bridge.so src/libva_cpu_bridge.c -ldl -lva
 *
 * Then put the resulting .so in the libva driver path and set:
 *   LIBVA_DRIVER_NAME=cpu_bridge
 *
 * The real driver is still loaded separately and used for non-AV1 paths.
 */

static void *g_real_driver = NULL;
static VAStatus (*g_real_vaInitialize)(VADisplay, int *, int *);
static VAStatus (*g_real_vaTerminate)(VADisplay);
static VAStatus (*g_real_vaQueryConfigProfiles)(VADisplay, VAProfile *, int *);
static VAStatus (*g_real_vaQueryConfigEntrypoints)(VADisplay, VAProfile, VAEntrypoint *, int *);

static void load_real_driver_if_needed(void) {
    if (g_real_driver != NULL) {
        return;
    }

    const char *path = getenv("VAAPI_REAL_DRIVER");
    if (path == NULL || path[0] == '\0') {
        /* Common Mesa / VA-API path for ARM systems */
        path = "/usr/lib/arm-linux-gnueabihf/dri/libva_mesa.so";
    }

    fprintf(stderr, "[vaapi-cpu-bridge] Loading real driver: %s\n", path);
    g_real_driver = dlopen(path, RTLD_LAZY | RTLD_GLOBAL);
    if (g_real_driver == NULL) {
        fprintf(stderr, "[vaapi-cpu-bridge] Failed to load real driver: %s\n", dlerror());
        return;
    }

    g_real_vaInitialize = (VAStatus (*)(VADisplay, int *, int *))dlsym(g_real_driver, "vaInitialize");
    g_real_vaTerminate = (VAStatus (*)(VADisplay))dlsym(g_real_driver, "vaTerminate");
    g_real_vaQueryConfigProfiles = (VAStatus (*)(VADisplay, VAProfile *, int *))dlsym(g_real_driver, "vaQueryConfigProfiles");
    g_real_vaQueryConfigEntrypoints = (VAStatus (*)(VADisplay, VAProfile, VAEntrypoint *, int *))dlsym(g_real_driver, "vaQueryConfigEntrypoints");
}

VAStatus vaInitialize(VADisplay dpy, int *major_version, int *minor_version) {
    load_real_driver_if_needed();
    if (g_real_vaInitialize == NULL) {
        fprintf(stderr, "[vaapi-cpu-bridge] Real vaInitialize not found\n");
        return VA_STATUS_ERROR_UNKNOWN;
    }
    return g_real_vaInitialize(dpy, major_version, minor_version);
}

VAStatus vaTerminate(VADisplay dpy) {
    load_real_driver_if_needed();
    if (g_real_vaTerminate == NULL) {
        return VA_STATUS_SUCCESS;
    }
    return g_real_vaTerminate(dpy);
}

VAStatus vaQueryConfigProfiles(VADisplay dpy, VAProfile *profile_list, int *num_profiles) {
    load_real_driver_if_needed();
    if (g_real_vaQueryConfigProfiles == NULL) {
        fprintf(stderr, "[vaapi-cpu-bridge] Real vaQueryConfigProfiles not found\n");
        return VA_STATUS_ERROR_UNKNOWN;
    }

    VAStatus status = g_real_vaQueryConfigProfiles(dpy, profile_list, num_profiles);
    if (status != VA_STATUS_SUCCESS) {
        return status;
    }

    /* Inject AV1 profile if it is absent. */
    int already_present = 0;
    for (int i = 0; i < *num_profiles; i++) {
        if (profile_list[i] == VAProfileAV1Profile0) {
            already_present = 1;
            break;
        }
    }

    if (!already_present && (*num_profiles < 64)) {
        profile_list[*num_profiles] = VAProfileAV1Profile0;
        (*num_profiles)++;
        fprintf(stderr, "[vaapi-cpu-bridge] Injected AV1 profile: VAProfileAV1Profile0\n");
    }

    return VA_STATUS_SUCCESS;
}

VAStatus vaQueryConfigEntrypoints(VADisplay dpy, VAProfile profile, VAEntrypoint *entrypoint_list, int *num_entrypoints) {
    load_real_driver_if_needed();

    if (profile == VAProfileAV1Profile0) {
        if (entrypoint_list != NULL && *num_entrypoints < 8) {
            entrypoint_list[0] = VAEntrypointVLD;
            *num_entrypoints = 1;
            fprintf(stderr, "[vaapi-cpu-bridge] Injected AV1 entrypoint: VAEntrypointVLD\n");
            return VA_STATUS_SUCCESS;
        }
    }

    if (g_real_vaQueryConfigEntrypoints == NULL) {
        fprintf(stderr, "[vaapi-cpu-bridge] Real vaQueryConfigEntrypoints not found\n");
        return VA_STATUS_ERROR_UNKNOWN;
    }

    return g_real_vaQueryConfigEntrypoints(dpy, profile, entrypoint_list, num_entrypoints);
}

/* The following symbols are commonly used by libva driver plugins as an init/term entrypoint. */
int vaDriverInit(void) {
    fprintf(stderr, "[vaapi-cpu-bridge] vaDriverInit called\n");
    load_real_driver_if_needed();
    return 0;
}

void vaDriverTerm(void) {
    fprintf(stderr, "[vaapi-cpu-bridge] vaDriverTerm called\n");
    if (g_real_driver != NULL) {
        dlclose(g_real_driver);
        g_real_driver = NULL;
    }
}

const char *vaQueryVendorString(VADisplay dpy) {
    (void)dpy;
    return "vaapi-cpu-bridge (AV1 capability shim)";
}
