//termios PTY stub for win32 app

//gcc -mwindows -static -o PTYstub PTYstub.c
#include <windows.h>
//#define _In_
//#define _In_opt_
//#define _Out_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
//#include <process.h>
#include <pthread.h>
#include <pty.h>

//define to display more debug messages
#define DBG_BOX
//#define DBG_BOX_PIPE

/*
Daemonization Process: To fully detach and run as a daemon, 
perform a double-fork, call setsid(), change working directory to root (/), 
umask to 0, and close stdin/stdout/stderr.
*/

#if 0
//when secondary part of PTY is closed, parent process will be terminated by unknown signal 
//(SIGHUP or else, but not CHILD)
static
void     ch_sig( int signum ){
	//exit(-1);
	printf("signal\n");
}
#endif

//pty file descriptor
static
int     pri_fd= 0;

//pty stub pipe reader thread 
//uintptr_t thr_rd;
static
pthread_t thr_rd;

//unsigned __stdcall 
static
void*
	read_PTY_pipe( void * ){

	enum { SZ= 4096 };
	char buf[SZ+16];
	int rlen, wlen;
		
	for(;;){
		if( (rlen= read(pri_fd, buf, SZ)) <= 0 )
		#ifndef DBG_BOX_PIPE
			{ exit(rlen); }
		#else
			{ MessageBoxA(0,"Can not stub read pri_fd","Error",MB_OK|MB_ICONERROR); exit(rlen); }
		#endif

	 	if( (wlen= write( STDOUT_FILENO, buf, rlen)) != rlen )
		#ifndef DBG_BOX_PIPE
			{ exit(wlen); }
		#else
			{ MessageBoxA(0,"Can not stub write STDOUT","Error",MB_OK|MB_ICONERROR); exit(wlen); }
		#endif
	}

	return 0;
}

//
int 
	main(int argc, char **argv){

	#if 0
		fprintf(stdout,"argc %u argv[1] %s argv[2] %s\n", argc, argv[1], (argc >=3? argv[2]: "???") );
		fflush(stdout);
		sleep(3);
	#endif

	int
		fork_pid;

	#if 0
	fork_pid = fork();
	if( fork_pid == -1){ perror("fork"); exit(-1); }
	if( fork_pid ){ 
		//sleep(3); 
		exit(0);
	}

	setsid();
	fclose(stdin);
	fclose(stdout);
	fclose(stderr);
	#endif

	fork_pid= forkpty( &pri_fd, 0, 0, 0 );
	if(fork_pid == -1)
		#ifndef DBG_BOX
			{ /*perror("forkpty");*/ exit(-1); }
		#else
			{ MessageBoxA(0,"Can not stub forkpty","Error",MB_OK|MB_ICONERROR); exit(-1); }
		#endif

	//child ttyp
	if( !fork_pid ) { 
		//for(;;){ sleep(1); }
		//printf("pty: %u\n", pri_fd);
		//exit( system("/bin/bash") );

		if(argc == 1){
			execl ("/bin/sh", "sh", "--verbose", "-c", "ls -alF; sh", (char *)0);
		}else{
			execvp( argv[1], argv+1 );
		}

		#ifndef DBG_BOX
			{ /*perror("execl");*/ exit(-1); }
		#else
			{ MessageBoxA(0,"Can not stub exec","Error",MB_OK|MB_ICONERROR); exit(-1); }
		#endif
	}

	//parent pty
		//printf("pty: %u\n", pri_fd);
		//int
		//	ch_status;
		//waitpid(fork_pid, &ch_status, 0);

		#if 0
		struct sigaction 
			sa = { 0 };
		sa.sa_handler= ch_sig;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags= SA_NOCLDSTOP;

		sigaction( SIGCHLD, &sa, 0 );
		#endif

	//fork read pipe thread loop
	int err;
	//if( (thr_rd= _beginthreadex( 0, 0, read_PTY_pipe, 0, CREATE_SUSPENDED, 0 )) == 0)
	if( 0 != (err = pthread_create( &thr_rd, 0, read_PTY_pipe, 0 )) )
		#ifndef DBG_BOX
			{ /*perror("pthread_create");*/ exit(err); }
		#else
			{ MessageBoxA(0,"Can not pthread_create","Error",MB_OK|MB_ICONERROR); exit(err); }
		#endif
	//if( ResumeThread((HANDLE)thr_rd) == -1 ){ MessageBoxA(0,"Can not ResumeThread","Error",MB_OK|MB_ICONERROR); exit(-1); }

	//continue write pipe thread loop
	enum { SZ= 4096 };
	char buf[SZ+16];
	int rlen, wlen;
		
	for(;;){
		if( (rlen= read( STDIN_FILENO, buf, SZ)) <= 0 )
		#ifndef DBG_BOX_PIPE
			{ exit(rlen); }
		#else
			{ MessageBoxA(0,"Can not stub read STDIN","Error",MB_OK|MB_ICONERROR); exit(rlen); }
		#endif

	 	if( (wlen= write( pri_fd, buf, rlen)) != rlen )
		#ifndef DBG_BOX_PIPE
			{ exit(wlen); }
		#else
			{ MessageBoxA(0,"Can not stub write pri_fd","Error",MB_OK|MB_ICONERROR); exit(wlen); }
		#endif
	}

	return 0;
}
