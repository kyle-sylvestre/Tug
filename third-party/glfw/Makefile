DEBUG ?= 0
OBJDIR =

CC = gcc
CFLAGS = -I./include
CFLAGS += -std=c99 -g3 -gdwarf-2 -Wall -Werror=format -Wextra -pthread
CFLAGS += -Wno-missing-field-initializers -Wno-unused-parameter -Wno-sign-compare -Wno-cast-function-type

ifeq ($(DEBUG), 1)
    CFLAGS += -DDEBUG -O0 
	OBJDIR = ./build_debug
else
    CFLAGS += -DNDEBUG -O3
	OBJDIR = ./build_release
endif

OUT = $(OBJDIR)/libglfw3.a

SOURCES =	./src/context.c\
			./src/init.c\
			./src/input.c\
			./src/monitor.c\
			./src/platform.c\
			./src/vulkan.c\
			./src/window.c\
			./src/egl_context.c\
			./src/osmesa_context.c\
			./src/null_init.c\
			./src/null_monitor.c\
			./src/null_window.c\
			./src/null_joystick.c

UNAME_S = $(shell uname -s)
ifeq ($(UNAME_S), Darwin) #APPLE
	ECHO_MESSAGE = "Mac OS X"
	CFLAGS += -D_GLFW_COCOA
	CFLAGS += -I/usr/local/include -I/opt/local/include -I/opt/homebrew/include
	SOURCES += 	./src/cocoa_time.c\
				./src/cocoa_init.m\
				./src/cocoa_joystick.m\
				./src/cocoa_monitor.m\
				./src/cocoa_window.m\
				./src/nsgl_context.m\
				./src/posix_module.c\
				./src/posix_thread.c
				
else ifeq ($(OS), Windows_NT)
	ECHO_MESSAGE = "MinGW"
	CFLAGS += -D_GLFW_WIN32 -D_WIN32 # define _WIN32 for cygwin/msys2/mingw
	SOURCES += ./src/win32_module.c\
				./src/win32_time.c\
				./src/win32_thread.c\
				./src/win32_init.c\
				./src/win32_joystick.c\
				./src/win32_monitor.c\
				./src/win32_window.c\
				./src/wgl_context.c
else
	# linux, BSD, etc.
	CFLAGS += -D_GLFW_X11 -D_POSIX_C_SOURCE=200809L
	LIBS += -lpthread -lm -ldl -lGL -lX11
	SOURCES += ./src/posix_module.c\
				./src/posix_time.c\
				./src/posix_thread.c\
				./src/x11_init.c\
				./src/x11_monitor.c\
				./src/x11_window.c\
				./src/xkb_unicode.c\
				./src/glx_context.c\
				./src/linux_joystick.c\
				./src/posix_poll.c
endif

OBJS = $(addprefix $(OBJDIR)/, $(addsuffix .o, $(basename $(notdir $(SOURCES)))))

all: | $(OUT)
	
$(OUT): $(OBJS)
	ar rcs $@ $^

$(OBJDIR)/%.o:./src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<
	
$(OBJDIR)/%.o:./src/%.m
	$(CC) $(CFLAGS) -c -o $@ $<
	
$(OBJS): | $(OBJDIR)

$(OBJDIR):
	mkdir -p $@

clean:
	rm -f $(OUT) $(OBJS)


