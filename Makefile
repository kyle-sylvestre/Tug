
tug.elf: mega.cpp
	g++ -g -O0 mega.cpp -o tug.elf -I./src -I./third-party/dear-imgui -I./third-party -Wall -Wformat `pkg-config --cflags glfw3` -lpthread -lGL `pkg-config --static --libs glfw3`

clean:
	rm -f tug.elf
