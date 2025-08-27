#ifndef APPLICATION_MANAGER_H
#define APPLICATION_MANAGER_H

#include <stdatomic.h>
#include <stdbool.h>

#include "../../common/common_types.h"
#include "../capture/include/v4l2_device_api.h"
#include "../frontend/include/frontend_manager.h"

typedef struct {
    V4l2DeviceContext *video_device;
    FrontendContext *frontend_context;
    FrameDimensions frame_dimensions;
    _Atomic bool *running_flag;
    unsigned char *rgb_frame_buffer;
    unsigned char *rgb_flipped_buffer;
    unsigned char *gray_frame_buffer;
    unsigned char *gray_rgba_buffer;
    _Atomic bool *gray_view;
} ApplicationContext;

bool Application_init(ApplicationContext *context,
                      const char *video_device_path);
void Application_run(ApplicationContext *context);
void Application_cleanup(ApplicationContext *context);

#endif  // APPLICATION_MANAGER_H
