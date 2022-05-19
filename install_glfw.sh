# compile GLFW, if your distro is new enough you can skip compiling with "sudo apt install libglfw3-dev" and linking the -lglfw shared library
git clone https://github.com/glfw/glfw.git
cd glfw
sudo apt install cmake
sudo apt install libx11-dev
sudo apt install libxrandr-dev
sudo apt install libxinerama-dev
sudo apt install libxcursor-dev
sudo apt install libxi-dev
sudo apt install libgl-dev
cmake . -D GLFW_BUILD_EXAMPLES=0 -D GLFW_BUILD_TESTS=0 -D GLFW_BUILD_DOCS=0
make clean
make -j
sudo make install
