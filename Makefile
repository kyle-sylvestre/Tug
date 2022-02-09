SRC := $(shell find . -name "*.cpp")
OBJ := $(patsubst %.cpp,%.o,$(SRC))

CXXFLAGS = -I./third-party/dear-imgui -I./third-party -I./src
CXXFLAGS += -g -O0 -Wall -Wformat
CXXFLAGS += -L./third-party/glfw -lglfw -lGL -lpthread

%.o: %.cpp
	g++ $(CXXFLAGS) -c -o $@ $<

tug.elf: $(OBJ)
	g++ -o $@ $^ $(CXXFLAGS)
	
clean:
	rm -f tug.elf
