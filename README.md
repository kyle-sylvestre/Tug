# Tug
GDB frontend made with Dear Imgui



<img width="838" alt="preview" src="https://user-images.githubusercontent.com/25188464/155052886-23e46ed7-94f8-460e-8116-17953d54efee.png">

# Debug Tutorial
Option 1:
1. pass executable to tug.elf as a command line argument (this overrides the debug_exe_path)

Option 2:
1. open tug.ini
2. enter path to gdb after gdb_path=
3. enter path to debugged executable after debug_exe_path=
4. open tug.elf then hit the |> button

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

