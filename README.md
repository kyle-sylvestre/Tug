# Tug
GDB frontend made with Dear Imgui

![image](https://user-images.githubusercontent.com/25188464/160298425-a5267c22-89fc-4d60-b93a-cd6dd9098924.png)

*Tugboat captain is the GDB archer fish mascot https://sourceware.org/gdb/mascot/*</br>
*Jamie Guinan's original archer fish logo and the vector versions by Andreas Arnez are licensed under CC BY-SA 3.0 US.*
*https://creativecommons.org/licenses/by-sa/3.0/us/*

![image](https://user-images.githubusercontent.com/25188464/160457519-15b65af3-0046-4c78-8fda-0b56a3ae7664.png)
# Building the Project
1. download pre-compiled glfw: https://www.glfw.org/download.html</br>
   OR compile glfw: https://www.glfw.org/docs/latest/compile.html</br>
2. run make in root git directory</br>

# Debuging an Executable
command line arguments:</br>
tug --exe [path to exe here] --gdb [path to gdb here]

NOTE: if you are debugging an executable in another directory, use gdb dir command to search for symbols in another path

# Source Window
-CTRL-F: open text search mode, N = next match, SHIFT-N = previous match, ESC to exit 

-CTRL-G: open goto line window, ENTER to jump to input line, ESC to exit

-hover over any word to query its value, right click it to create a new watch within the control window

-add/remove breakpoint by clicking the empty column to the left of the line number

# Control Window
-top row: program execution buttons

  "---" = jump to next executed line inside source window
  
  "|>"  = start/continue program
  
  "||"  = pause program
  
  "-->" = step into
  
  "/\\>" = step over
  
  "</\\" = step out
  
# GDB Console Command Line
-repeat last command on hitting enter on an empty line (GDB emulation)

-cycle command history by pressing up/down arrow while clicked on the box

-hit tab while typing to see all the autocompletions, hit tab/shift tab to cycle through them

# Resources
-GDB Machine Interpreter: https://sourceware.org/gdb/download/onlinedocs/gdb/GDB_002fMI.html#GDB_002fMI

