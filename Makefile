sources := $(shell find . -name "*.cpp")
objects := $(patsubst %.cpp,%.o,$(sources))

CXXFLAGS = -I./third-party/dear-imgui -I./third-party -I./src
CXXFLAGS += -g -O0 -Wall -Wformat
CXXFLAGS += `pkg-config --cflags glfw3` -lpthread -lGL `pkg-config --static --libs glfw3`

%.o: %.cpp
	g++ $(CXXFLAGS) -c -o $@ $<

tug.elf: $(objects)
	g++ -o $@ $^ $(CXXFLAGS)
	
clean:
	rm -f tug.elf
