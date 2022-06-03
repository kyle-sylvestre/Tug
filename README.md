# Tug
GDB frontend made with Dear Imgui

![image](https://user-images.githubusercontent.com/25188464/160298425-a5267c22-89fc-4d60-b93a-cd6dd9098924.png)

*Tugboat captain is the GDB archer fish mascot https://sourceware.org/gdb/mascot/*</br>
*Jamie Guinan's original archer fish logo and the vector versions by Andreas Arnez are licensed under CC BY-SA 3.0 US.*
*https://creativecommons.org/licenses/by-sa/3.0/us/*

![image](https://user-images.githubusercontent.com/25188464/171760180-31b82e33-e3db-4731-ad72-208ed8fcd104.png)

# Building the Project
1. Install GLFW3 from your package system</br>
   **Debian:** sudo apt-get install libglfw3 libglfw3-dev</br>
   **Arch:** sudo pacman -S glfw </br>
   **Fedora:** sudo dnf install glfw glfw-devel </br>

2. If unable to find package, compile from git repo </br>
    https://github.com/glfw/glfw </br>
    https://www.glfw.org/docs/latest/compile.html </br>
    **NOTE**: linker flag -lglfw is the shared library, -lglfw3 is the static library </br>
    
3. Run command "make DEBUG=0", output in generated build_release directory

# Debuging an Executable
have either gdb or gdb-multiarch installed

1. run program from command line</br>
    tug --exe [program to debug filename] --gdb [gdb filename]</br>

OR 

1. run tug</br>
2. click "Debug Program" menu button</br>
3. fill in gdb filename and debug filename, args are optional</br>
4. click "Start" button</br>

# Source Window
* CTRL-F: open text search mode, N = next match, SHIFT-N = previous match, ESC to exit 
* CTRL-G: open goto line window, ENTER to jump to input line, ESC to exit
* hover over any word to query its value, right click it to create a new watch within the control window
* add/remove breakpoint by clicking the empty column to the left of the line number

# Control Window
program execution buttons</br>
* "---" = jump to next executed line inside source window
* "|>"  = start/continue program
* "||"  = pause program
* "-->" = step into
* "/\\>" = step over
* "</\\" = step out
  
# GDB Console Command Line
* repeat last command on hitting enter on an empty line (GDB emulation)
* cycle command history by pressing up/down arrow while clicked on the box
* hit tab while typing to see all the autocompletions, hit tab/shift tab to cycle through them

# Resources
GDB Machine Interpreter:</br>
https://sourceware.org/gdb/download/onlinedocs/gdb/GDB_002fMI.html#GDB_002fMI</br>

GLFW:</br>
https://www.glfw.org/download.html</br>
https://www.glfw.org/docs/latest/compile.html</br>


