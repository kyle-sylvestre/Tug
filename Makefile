DEBUG ?= 0
SAN ?= 0
IMGUI_DIR = ./third-party/imgui
OBJDIR =

CXX = g++
CXXFLAGS = -I./third-party -I./src -I./third-party/glfw/include
CXXFLAGS += -std=c++11 -g3 -gdwarf-2 -Wall -Wextra -Werror=format -Werror=shadow -pedantic -pthread
CXXFLAGS += -Wno-missing-field-initializers

ifeq ($(DEBUG), 1)
    CXXFLAGS += -DDEBUG -O0 
	OBJDIR = build_debug
else
    CXXFLAGS += -DNDEBUG -O3
	OBJDIR = build_release
endif


ifeq ($(SAN), 1)
	CXXFLAGS += -fno-omit-frame-pointer -fsanitize=undefined,address  #-fsanitize-undefined-trap-on-error
endif


GLFW = glfw3
EXE = $(OBJDIR)/tug

SOURCES = ./src/main.cpp\
          ./src/gdb.cpp\
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
LIBS += -L ./third-party/glfw/$(OBJDIR) -l $(GLFW)

ifeq ($(UNAME_S), Darwin) #APPLE
	SOURCES += ./third-party/sem_timedwait.cpp
	ECHO_MESSAGE = "Mac OS X"
	LIBS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
	LIBS += -L/usr/local/lib -L/opt/local/lib -L/opt/homebrew/lib
	LIBS += -lpthread

	CXXFLAGS += -I/usr/local/include -I/opt/local/include -I/opt/homebrew/include
else ifeq ($(OS), Windows_NT)
	ECHO_MESSAGE = "MinGW"
	CXXFLAGS += -D_GNU_SOURCE # ptty and dirent visibility
	LIBS += -lpthread -lgdi32 -lopengl32 -limm32
else
	# linux, BSD, etc.
	ECHO_MESSAGE = $(UNAME_S)
	LIBS += -lpthread -lm -ldl -lGL -lX11
endif



all: $(EXE)
	@echo build complete for $(ECHO_MESSAGE)
	
$(EXE): $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

$(EXE): | $(GLFW)

$(GLFW):
	$(MAKE) -C ./third-party/glfw DEBUG=$(DEBUG)

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
	rm -f $(EXE) $(OBJS)
	$(MAKE) -C ./third-party/glfw DEBUG=$(DEBUG) clean

