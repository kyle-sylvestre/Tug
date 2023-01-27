// single translation unit build
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 1999309L

// squelch any existing glfw warnings
#ifdef __linux__
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

//#define _GLFW_WAYLAND
#define _GLFW_X11
#include <glfw/src/internal.h>

#include <glfw/src/context.c>
#include <glfw/src/egl_context.c>
#include <glfw/src/glx_context.c>
#include <glfw/src/init.c>
#include <glfw/src/input.c>
#include <glfw/src/linux_joystick.c>
#include <glfw/src/monitor.c>
#include <glfw/src/null_init.c>
#include <glfw/src/null_joystick.c>
#include <glfw/src/null_monitor.c>
#include <glfw/src/null_window.c>
#include <glfw/src/osmesa_context.c>
#include <glfw/src/platform.c>
#include <glfw/src/posix_module.c>
#include <glfw/src/posix_poll.c>
#include <glfw/src/posix_thread.c>
#include <glfw/src/posix_time.c>
#include <glfw/src/vulkan.c>
#include <glfw/src/window.c>
#include <glfw/src/x11_init.c>
#include <glfw/src/x11_monitor.c>
#include <glfw/src/x11_window.c>
#include <glfw/src/xkb_unicode.c>
