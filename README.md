# Tug
GDB frontend made with Dear ImGui

![image](https://user-images.githubusercontent.com/25188464/160298425-a5267c22-89fc-4d60-b93a-cd6dd9098924.png)

*Tugboat captain is the GDB archer fish mascot https://sourceware.org/gdb/mascot/*</br>
*Jamie Guinan's original archer fish logo and the vector versions by Andreas Arnez are licensed under CC BY-SA 3.0 US.*
*https://creativecommons.org/licenses/by-sa/3.0/us/*

![image](https://user-images.githubusercontent.com/25188464/171760180-31b82e33-e3db-4731-ad72-208ed8fcd104.png)

# Building the Project

1. Run command "make DEBUG=0 -j", output executable is ./build_release/tug

# Debugging an Executable

**NOTE**: Tug defaults to the gdb filename returned by the command "which gdb" </br>

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


