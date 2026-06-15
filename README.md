# TinyTerm7
Win32 light terminal to work with PTY device of cygwin; intended to provide remote connection with the same look and feel as local minix 3 console

TinyTerm7
	light Windows terminal emulator 
	to work with PTY device of mingw or cygwin

based on source code of "tinyTerm" 
	"A minimal serail/telnet/ssh/sftp terminal emulator" by Yongchao Fan, 2018-2019 
	The "tinyTerm" is free software distributed under GNU GPL 3.0
	https://github.com/yongchaofan/tinyTerm

1. how to make TinyTerm7 files
	
  "TinyTerm7" connects to cygwin PTY device via "PTYstub" stream filter

	read mak.cmd, set correct path and then run mak.cmd to build TinyTerm7 files
	
	note, TinyTerm7 file is only 32 bit x86 target, building as x64 target is not tested,
	that means incorrect for x64 target data types cast was used everywhere,
	build TinyTerm7 as 32 bit x86 target to stay in tested properties
	
2. file PTYstub
  
  PTYstub is part of tinyTerm7 software, separated from file TinyTerm7 because of different 
  design tools for win32 API and cygwin API

	PTYstub is stream filter between PTY cygwin device and ordinary stdin/stdout pipes 
	(like pipe created by '|' in 'ls / | grep v*' between "ls" and "grep")

	PTY device is part cygwin software, implemented "termios.h" compatible "virtual" terminal interface; 
	the termios interface is intended to be "local end" device in connection to real harware terminals 
	and is needed to support unified user terminal login session in cygwin environment 

	in comparison with ordinary pipe, termios PTY device has extra functionality needed only to 
	harware terminal and to support login session; all the PTY termios interface special functions 
	are implemented by cygwin itself inside the PTY device
	
	when telnet makes network connection, the link also uses remote PTY device to remote console login 

	so many interactive cygwin console apps (like 'bash') is written to always use the termios interface,
	they will not work with ordinary stdin/stdout pipes

3. file TinyTerm7
	
  TinyTerm7 is ordinary win32 GUI app and connectied to PTYstub via ordinary stdin/stdout pipes 
	provides own win32 GUI window as PTY device in cygwin environment 

	TinyTerm7 generates app input from system win32 keyboard and display app output chars to 
	own win32 window as "remote end" of termios PTY device in cygwin environment, 
	termcap/terminfo records (selected by environment like "TERM=xterm") declares access to the 
	TinyTerm7 terminal functions

	TinyTerm7 acting as software implemented terminal of TERM type to user access 
	to cygwin environment 

	TinyTerm7 does not contain any protocols other than "connect to PTY device", 
	and generic app for PTY device in cygwin environment is bash interpreter;
	any other remote protocols can be used by external tools running in cygwin environment
	connected to the PTY device (the tools are called from bash prompt, like "telnet")

4. external repos
   
   because of "github 2fa protection" i probably will not be able to access own data on github since june 2026, 
   check sourceforge.net or similar open storage for the same project name TinyTerm7 for possible project updates.
   
===
