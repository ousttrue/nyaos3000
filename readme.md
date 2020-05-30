What is NYAOS 3000
==================

NYAOS 3000 is the code name of the Nihongo Yet Another Open Shell 3.xx, which is the next generation of NYAOS 2.xx Series.

* It is powered by Lua.
    * Build-in command: `lua_e "LUA-CODE"` which extends NYAOS.
    * About detail, see "What powered by Lua").
* Support Windows XP SP3 or later, and OS/2 Warp 4.5
* It does not require CMD.EXE. It runs standalone.

Caution
--------

* The rule of loading `_nya` changed. NYAOS.EXE comes to load all of `_nya` on the directories where nyaos.exe exists , which %HOME% or %USERPROFILE% points, and on the current directory.
* On OS/2 Warp, emx.dll (0.9 or later) are required.
