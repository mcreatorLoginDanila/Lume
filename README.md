A light web?-browser on C++ without using chromium\electron
Uses C++ 17 x lua 5.4.7






---


COMPILE WITH

cl /EHsc /std:c++17 /O2 /I. main.cpp lib/lapi.c lib/lcode.c lib/lctype.c lib/ldebug.c lib/ldo.c lib/ldump.c lib/lfunc.c lib/lgc.c lib/llex.c lib/lmem.c lib/lobject.c lib/lopcodes.c lib/lparser.c lib/lstate.c lib/lstring.c lib/lstrlib.c lib/ltable.c lib/ltablib.c lib/ltm.c lib/lundump.c lib/lutf8lib.c lib/lvm.c lib/lzio.c lib/lauxlib.c lib/lbaselib.c lib/lcorolib.c lib/ldblib.c lib/linit.c lib/liolib.c lib/lmathlib.c lib/loadlib.c lib/loslib.c /link ws2_32.lib gdi32.lib user32.lib comctl32.lib shell32.lib
