# get precompiled GLFW if available, else compile GLFW from source
sudo apt update
apt-cache show libglfw3
exit_code=$?
if [ $exit_code = 0 ]
then
    sudo apt -y install libglfw3
else
    sudo apt -y install git
    git clone https://github.com/glfw/glfw.git
    exit_code=$?
    if [ $exit_code = 0 ]
    then    
        cd glfw
        sudo apt -y install cmake libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev
        cmake . -DBUILD_SHARED_LIBS=1\
                -DGLFW_BUILD_EXAMPLES=0\
                -DGLFW_BUILD_TESTS=0\
                -DGLFW_BUILD_DOCS=0 
        make clean
        make -j
        sudo make install
        cd ..
        rm -rf glfw
    fi
fi 

