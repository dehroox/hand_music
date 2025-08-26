#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>

#include "application/application_manager.h"
#include "common/constants.h"

int main(void) {
    ApplicationContext app_context = {0};

    if (!Application_init(&app_context, VIDEO_DEVICE_PATH)) {
        fputs("Application initialization failed.\n", stderr);
        return EXIT_FAILURE;
    }

    Application_run(&app_context);

    Application_cleanup(&app_context);

    return EXIT_SUCCESS;
}
