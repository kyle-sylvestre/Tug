SRC := $(shell find . -name "*.cpp")
OBJ := $(patsubst %.cpp,%.o,$(SRC))

CXXFLAGS = -I./third-party -I./src
CXXFLAGS += -std=c++11 -g -O0 -Wall -Wformat -fsanitize=undefined #-fsanitize-undefined-trap-on-error
CXXFLAGS += -L./third-party/glfw -lglfw -lGL -lpthread

%.o: %.cpp
	g++ $(CXXFLAGS) -c -o $@ $<

tug.elf: $(OBJ)
	g++ -o $@ $^ $(CXXFLAGS)
	
clean:
	rm -f ./tug.elf $(OBJ) 
