#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>

#include "application/application_manager.h"
#include "common/constants.h"

int main(int argc, char* argv[]) {
    (void)argv;
    ApplicationContext app_context = {0};
    app_context.gray_view = calloc(1, sizeof(atomic_bool));

    if (!app_context.gray_view) {
        fputs("Failed to allocate gray_view flag\n", stderr);
        Application_cleanup(&app_context);
        return EXIT_FAILURE;
    }

    atomic_init(app_context.gray_view, (bool)(argc > 1));

    if (!Application_init(&app_context, VIDEO_DEVICE_PATH)) {
        fputs("Application initialization failed.\n", stderr);
        return EXIT_FAILURE;
    }

    Application_run(&app_context);
    Application_cleanup(&app_context);
    return EXIT_SUCCESS;
}
