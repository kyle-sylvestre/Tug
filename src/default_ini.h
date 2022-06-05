#pragma once

const char *const default_ini = R"(
[Tug]
Callstack=1
Locals=1
Registers=0
Watch=1
Source=1
Control=1
Breakpoints=0
WindowTheme=DarkBlue

; ImGui Begin
[Window][DockingWindow]
Size=1270,720
Collapsed=0

[Window][Debug##Default]
Pos=60,60
Size=400,400
Collapsed=0

[Window][Source]
Pos=0,19
Size=902,461
Collapsed=0
DockId=0x00000003,0

[Window][Control]
Pos=0,482
Size=902,238
Collapsed=0
DockId=0x00000004,0

[Window][Locals]
Pos=904,482
Size=376,238
Collapsed=0
DockId=0x00000006,0

[Window][Callstack]
Pos=904,252
Size=376,228
Collapsed=0
DockId=0x00000008,0

[Window][Watch]
Pos=904,19
Size=376,231
Collapsed=0
DockId=0x00000007,0

[Window][Registers]
Pos=904,248
Size=376,224
Collapsed=0
DockId=0x7FD18564,1

[Window][DockSpaceViewport_11111111]
Pos=0,19
Size=1280,701
Collapsed=0

[Table][0x948EDA1E,2]
RefScale=13
Column 0  Width=125
Column 1  Width=35

[Table][0xFB078A15,2]
RefScale=13
Column 0  Width=123
Column 1  Width=35

[Table][0x61EA1B4D,2]
RefScale=13
Column 0  Width=125
Column 1  Width=35

[Docking][Data]
DockSpace       ID=0x7FD18564 Pos=0,19 Size=1270,701 CentralNode=1 Selected=0xDA041833
DockSpace       ID=0x8B93E3BD Window=0xA787BDB4 Pos=0,19 Size=1280,701 Split=X
  DockNode      ID=0x00000001 Parent=0x8B93E3BD SizeRef=902,701 Split=Y Selected=0xDA041833
    DockNode    ID=0x00000003 Parent=0x00000001 SizeRef=902,461 CentralNode=1 Selected=0xDA041833
    DockNode    ID=0x00000004 Parent=0x00000001 SizeRef=902,238 Selected=0xCE6F6A26
  DockNode      ID=0x00000002 Parent=0x8B93E3BD SizeRef=376,701 Split=Y Selected=0x7BFCF530
    DockNode    ID=0x00000005 Parent=0x00000002 SizeRef=376,461 Split=Y Selected=0x7BFCF530
      DockNode  ID=0x00000007 Parent=0x00000005 SizeRef=376,231 Selected=0x7BFCF530
      DockNode  ID=0x00000008 Parent=0x00000005 SizeRef=376,228 Selected=0x2924BF46
    DockNode    ID=0x00000006 Parent=0x00000002 SizeRef=376,238 Selected=0xFEB5AC5E

)";
