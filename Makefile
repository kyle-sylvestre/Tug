DEBUG ?= 0
SAN ?= 0
IMGUI_DIR = ./third-party/imgui
OBJDIR =

CXX = g++
CXXFLAGS = -I./third-party -I./src
CXXFLAGS += -g3 -gdwarf-2 -std=c++11 -Wall -Werror=format -Wextra -Werror=shadow -pedantic -pthread

ifeq ($(DEBUG), 1)
    CXXFLAGS += -DDEBUG -O0 
	OBJDIR = ./build_debug
else
    CXXFLAGS += -DNDEBUG -O3
	OBJDIR = ./build_release
endif


ifeq ($(SAN), 1)
	CXXFLAGS += -fno-omit-frame-pointer -fsanitize=undefined,address  #-fsanitize-undefined-trap-on-error
endif


EXE = $(OBJDIR)/tug
PVS_LOG = $(addprefix $(OBJDIR)/, pvs.log)

SOURCES = ./src/main.cpp\
          ./src/gdb.cpp\
          ./third-party/dlmalloc.cpp\
          $(IMGUI_DIR)/imgui.cpp\
          $(IMGUI_DIR)/imgui_demo.cpp\
          $(IMGUI_DIR)/imgui_draw.cpp\
          $(IMGUI_DIR)/imgui_impl_glfw.cpp\
          $(IMGUI_DIR)/imgui_impl_opengl2.cpp\
          $(IMGUI_DIR)/imgui_tables.cpp\
          $(IMGUI_DIR)/imgui_widgets.cpp

OBJS = $(addprefix $(OBJDIR)/, $(addsuffix .o, $(basename $(notdir $(SOURCES)))))
UNAME_S = $(shell uname -s)

## from example_glfw_opengl2 makefile
##---------------------------------------------------------------------
## BUILD FLAGS PER PLATFORM
##---------------------------------------------------------------------

ifeq ($(UNAME_S), Linux) #LINUX
	ECHO_MESSAGE = "Linux"
	LIBS += -lpthread -lGL -lglfw -lm -ldl	# -lglfw3=static, -lglfw=dynamic
endif

ifeq ($(UNAME_S), Darwin) #APPLE
	ECHO_MESSAGE = "Mac OS X"
	LIBS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
	LIBS += -L/usr/local/lib -L/opt/local/lib -L/opt/homebrew/lib
	LIBS += -lpthread -lglfw

	CXXFLAGS += -I/usr/local/include -I/opt/local/include -I/opt/homebrew/include
endif

# TODO: mingw doesn't support spawn.h, need replacement for posix_spawnp
#ifeq ($(OS), Windows_NT)
#	ECHO_MESSAGE = "MinGW"
#	LIBS += -lpthread -lglfw3 -lgdi32 -lopengl32 -limm32
#endif

all: $(EXE)
	@echo build complete for $(ECHO_MESSAGE)
	
$(EXE): $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

$(OBJDIR)/%.o:./src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/%.o:./third-party/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/%.o:$(IMGUI_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
	
$(OBJS): | $(OBJDIR)

$(OBJDIR):
	mkdir -p $@

clean:
	rm -f $(EXE) $(OBJS) $(PVS_LOG)


