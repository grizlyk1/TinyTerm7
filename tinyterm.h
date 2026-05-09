//
// tinyTerm7 -- A minimal Windows terminal emulator

#pragma once

#include "stdafx.h"
#include "resource.h"

//pty stub
extern HANDLE hPTY_stdin, hCH_stdin, hPTY_stdout, hCH_stdout;
extern int wfPTY_stdin, rfPTY_stdout; //stdc file descriptors to wr/rd

//
struct TERM {
	// UTF8 is format to transfer or storage data, not to eval data 
	// UTF16LE used to eval
	typedef WCHAR	tt7_data_t;		//char array item
	typedef WCHAR	tt7_attr_t;		//attr array item (unsigned short int)

	//default terminal screen size
	enum { DEF_LX= 120 };				//size of terminal screen 
	enum { DEF_LY= 30 };

	enum { DEF_MAX_LX= 256 };			//max lx size of terminal line (scan line)
	enum { DEF_MAX_LY= 256 };			//max ly size of screen (for check only)
	enum { DEF_TOTAL_LINES= 1024 };		//total lines in memory buf
	enum { DEF_BUFFER_ITEMS= DEF_TOTAL_LINES*DEF_MAX_LX };	//total items in memory buf

	//actual terminal screen size (noone of the sizes can be zero)
	unsigned
		lx, ly			
		, v_max_lx, v_max_ly
		, v_total_lines
		;
	unsigned max_lx()const { return v_max_lx; }
	unsigned max_ly()const { return v_max_ly; }
	unsigned total_lines()const { return v_total_lines; }
	unsigned buffer_items()const { return v_total_lines * v_max_lx; }

	/*
	new V0.2 video memory layout 
		memory array is fixed TOTAL_LINES number of lines of fixed MAX_LX line size 
		start of memory array
			->line_tail						|
											|
											|
			->line_scroll				|	|
			->line_head				|	|	|		
			->line_scroll+ly		|	|
			->line_head+ly			|
		end of memory array

		vars line_XXXX are indexes of lines suitable for any memory buf (char, attr etc)
		to unwrap memory map lines to linear memory by add TOTAL_LINES to wrapped values (to values less then base line)

		scroll direction convention
			backward, up	- decrement line_scroll, scroll_offs >0
			forward, dn		- increment line_scroll, scroll_offs <0
	*/
	unsigned
		//y positions from zero
		  line_head		//top of active window, (lx, ly) is size of active window
		, line_tail		//last existed line of rollback buffer, so (line_head - line_tail) is size in lines of rollback buf 
		, line_scroll	//top of scrolled view, must be between [line_tail;line_head]
		;

		//xterm compatible alt buffer fullscreen mode, disable scrollback buf
	BOOL bDisScrollBuf;			

	/*
	because of video memory is ring buf, class provide some functions to generic access to buf lines:
		to do "inc", "dec" any line number relatively given line
		to "index" any line number relatively given base line
		to count "size" of current rollback buf 
	function init_rollback_buf() will do { line_head= line_tail= line_scroll= 0; }
	*/
	void		init_rollback_buf(){ line_head= line_tail= line_scroll= 0; }
	unsigned	do_line_add1(unsigned line)const { 
		assert( line < total_lines() );
		return ((line+1 < total_lines())? line+1: 0); 
		}
	unsigned	do_line_sub1(unsigned line)const { 
		assert( line < total_lines() );
		//if(!line) return TOTAL_LINES-1;
		//return ((line-1 < TOTAL_LINES)? line-1: (TOTAL_LINES-1)); 
		return (line? line-1: total_lines()-1);
		}
	unsigned	do_line_index(unsigned line_base, unsigned idx)const { 
		//if(line_base >= TOTAL_LINES) line_base= TOTAL_LINES-1; 
		assert( line_base < total_lines() );
		//if(idx >= TOTAL_LINES) idx= idx%TOTAL_LINES; 
		//if(idx >= TOTAL_LINES) return line_base; 
		assert( idx < total_lines() );
		//can not change "TOTAL_LINES - y_base" to "y_base + idx" in condition check
		return ((idx < total_lines() - line_base)? line_base + idx: 0 + idx - (total_lines() - line_base)); 
		}
	//signed idx version of do_index
	unsigned	do_line_offs(unsigned line_base, int offs)const { 
		assert( line_base < total_lines() );
		assert( unsigned( abs(offs) ) < total_lines() );
		if( !offs )return line_base;
		//can not change "TOTAL_LINES - y_base" to "y_base + offs" in condition check
		if( offs > 0 )return (unsigned(offs) < total_lines() - line_base)? line_base + offs: 0 + offs - (total_lines() - line_base);
		//offs < 0
		return (unsigned(-offs) <= line_base)? line_base + offs: total_lines() - (-offs - line_base);
		}
	//found distance between lines in range [line_tail, line_head_ly)
	//less(line_1, line_2) is the same as (do_line_diff(line_1, line_2) < 0)
	int			do_line_diff(unsigned line_1, unsigned line_2){
		assert( line_1 < total_lines() );
		assert( line_2 < total_lines() );
		//to unwrap wrapped memory in range[line_tail, line_head+ly)
		//map lines to linear memory by add TOTAL_LINES to values less 'line_tail'
		if(line_1 < line_tail)line_1 += total_lines();
		if(line_2 < line_tail)line_2 += total_lines();
		//unsigned l_head = (line_head < line_tail ? line_head+TOTAL_LINES: line_head);
		return line_1 - line_2;
		}

	enum { DEF_CLR_ATTR= 0x07U };

	tt7_data_t	*data_buf;
	tt7_attr_t	*attr_buf, clr_attr, saved_attr;

	/*
		active window suport
			add new screen_lx, screen_ly instead of old lx, ly
			new (top_x,top_y) + old (lx,ly) is location of active window inside screen
		
		all output going to active window in local coordinates of the active window 
		function is_fulscreen() test active window in fullscreen mode and rollback buffer is in work
		function is_active_window() answer is active window in action
		any active window will be reset to fullscreen in reset_terminal()

		(screen_lx, screen_ly) will be set by term_Construct() and term_Size() function
		term_set_active_window() is only function using new screen_lx, screen_ly margine instead of old lx, ly
		all other functions using old lx, ly margine but add top_x, top_y to beginnig of head pointer to correct output data
	*/
	unsigned screen_lx, screen_ly;			
	unsigned top_x, top_y;
	BOOL is_fullscreen(){ return (top_x == 0 && top_y == 0 && lx == screen_lx && ly == screen_ly); }
	BOOL is_active_window(){ return !is_fullscreen(); }

	BOOL bCursor;
	//cursor positions from zero
	unsigned	cursor_x, cursor_y, cursor_saved_x, cursor_saved_y;

	enum { DEF_TAB_SZ= 8 };

	BOOL bProgTab;								//enable programmed tab size, in range [1;8]
	unsigned	tab_sz;							// 1 + (cursor_x % 8)
	unsigned	eval_tab_sz()const { return 1 + (cursor_x % DEF_TAB_SZ); } //set tab_sz by current X of cursor position

	//selection marks
	BOOL bSel;
	unsigned	sel_x_m1, sel_line_m1;
	unsigned	sel_x_m2, sel_line_m2;
	//	y most top is (line_tail) of roolback buf
	//	y most bottom is (line_head+ly-1) of screen
	//unsigned	sel_y_most_top()const { return line_tail; }
	//unsigned	sel_y_most_bottom()const { return (line_head + ly - 1); }
		
	BOOL bInsert;				//smir/rmir "insert mode"
	BOOL bWrap;					//smam/rmam "auto wrap line mode"

	/*
	keyb can be locked by error data output
	so could be done automatic set XON by timer, 1 second delay for example, 
	but new incoming XON should reset the timer 
	*/
	BOOL bKeyb;					//XON/XOFF keyboard+mouse output enable/discard

	/*
		TT7 do not keep pressed state of buttons, every event is only single button pressed/unpressed
		app can track pressed state of buttons itself

		mouse event reported about index of own pressed button 0,1,2 or index 3 if no buttons pressed
		mouse event reported about index of button in low bits
		MOUSE_BUTTON_MASK defines used number of mouse buttons (2 buttons for 0x03 mask)

		mouse event reported about mutual exclusive event type in high bits
			hi		low
			0x00	event button HOLD index, can be with index 3 (xterm button HOLD index)
			0x10	event button UP index, can not be with index 3 (incompatible with xterm)
			0x20	mouse move, hold button index or index 3
			0x40	mouse wheel, wheel direction index 0 or 1

		to be xterm compat
			on event button DN mouse reported single: 0x00+but_idx
			on event button UP mouse reported pair: 0x10+but_idx and 0x00+3
	*/
	enum { MOUSE_BUT_MASK = 0x03 };
	enum { MOUSE_BUT_1= 0, MOUSE_BUT_2= 1, MOUSE_BUT_3= 2, MOUSE_BUT_NO= 3 };
	enum { MOUSE_BUT_HOLD= 0x00, MOUSE_BUT_UP= 0x10, MOUSE_MOVE= 0x20, MOUSE_WHEEL= 0x40 };

	//tracking detail mode3 implies modes 1&2 etc
	enum { MOUSE_DIS=0, MOUSE_MODE_1= 1, MOUSE_MODE_2= 2, MOUSE_MODE_3= 3 };
	unsigned	mouse_mode;
	BOOL bAppMouse;				//if (!bAppMouse || !mouse_mode || ctrl_pressed), mouse event directed to terminal else to app

	enum mouseEvents {DOUBLECLK, RIGHTCLK, LEFTDOWN, LEFTDRAG, LEFTUP, MIDDLEUP};

	//options 
	BOOL bOptHideEsc;			//drop unknown ESC seq to display (not always working properly, because of unknown size of ESC seq)
	BOOL bOptCursorBS;			//display as non-destructive BS, some apps uses BS to left cursor motion instead of :le=\E[D:
	BOOL bOptESC_do_CAN;		//ESC key emits CAN code \030 else ESC code \033
	//mutual exclusive Display mode: UTF8, OEM, ANSI
	enum { Display_UTF8=0, Display_OEM= 1, Display_ANSI= 2 };
	unsigned OptDisplay_mode;	//BOOL bOptDisplayUTF8;
								//display as UTF8 encoded 8 bit chars, else as plain 8 bit chars mapped to low byte 0x00-0xFF of UTF16LE
								//	codepage for 0xA0-0xFF can be properly installed in host Windows
								//	keyb output is never affected by the option (UTF8 enceded + control chars in range [0xA0-0xC1, 0xFE, 0xFF])
								//	the option is intended to easy display existed not UTF8 windows text files in range [0xA0-0xFF])
	BOOL bOptKeybUTF8;			//keyb send to app UTF8 encoded 8 bit chars, else plain 8 bit chars 
								//	codepage for 0xA0-0xFF can be properly installed in host Windows
								//	display output is never affected by the option 
								//	the option is intended to easy change existed not UTF8 windows text files in range [0xA0-0xFF])
	BOOL bOptMouseXterm;		//more xterm compatible mouse for xterm apps runnig under TT7

	BOOL bOptXOFF2;				//use instead of keyb as XOFF new escaped XOFF2 code :XO=\ES:, else old single byte ascii code 0x13 (^S)
								//the option does not change XOFF code posted from terminal to PC
	
	BOOL bOptBracket;			//mark terminal pasted blocks in esc bracket for best editor undo, 
								//but the rich editors should use own mice instead of raw terminal paste tools

	enum { TITLE_SZ= 256 };			//max term title size in items
	enum { TITLE_BASE_SZ = 11 };	//"TT7------: "
	unsigned	title_idx;
	tt7_data_t	title[TITLE_SZ+16];
	BOOL bAppTitle;					//app display to Title line at pos 'title_idx' until \007 or ESC seq,
									//data stored in memory only for title_idx in range [TITLE_BASE_SZ, TITLE_SZ)
									//else app display to screen

	//save alt buf, alt clr_attr, alt cursor_x_y, { alt_title, alt_title_idx }
	enum { ALT_DEF_BUFFER_ITEMS= (DEF_MAX_LY+16)*DEF_MAX_LX };	//total items in save memory buf
	unsigned alt_buffer_items()const { return (v_max_ly+16) * v_max_lx; }

	tt7_data_t	*alt_data_buf;
	tt7_attr_t	*alt_attr_buf, alt_clr_attr;
	unsigned	alt_cursor_x, alt_cursor_y;
	unsigned	alt_title_idx;
	tt7_data_t	alt_title[TITLE_SZ+16];

	enum { SEQ_SZ= 64 };			//max seq size in items
	BOOL bEscape;					//ESC seq parse in progress
	WCHAR escape_wcode[SEQ_SZ + 2];	//must be zero filled before every first enter parse_EscapeW
	unsigned escape_filled;			//cur buf items filled

	enum { UTF8_SZ= 6};
	BOOL bUtf8;						//UTF8 seq parse in progress
	char utf8_code[UTF8_SZ + 2];
	unsigned utf8_filled;			//cur buf items filled
									//expected buf size in range [2, UTF8_SZ] coded in utf8_code[0]
	//unsigned saved_utf8_sz;
	unsigned utf8_sz(char first_utf8)const { 
		if( !(first_utf8 & 0x20U) )return 2;
		if( !(first_utf8 & 0x10U) )return 3;
		if( !(first_utf8 & 0x08U) )return 4;
		if( !(first_utf8 & 0x04U) )return 5;
		if( !(first_utf8 & 0x02U) )return 6;
		return 0;	//error
	}

	BOOL bLogEsc;				//write unknown ESC seq to log file
	FILE *fpLogEscFile;

	BOOL bLogging;				//enable log to file all data incoming to display 
	FILE *fpLogFile;

	HANDLE mtx;					//term parse mutex
};

/****************term.c****************/
//void host_callback( void *term, char *buf, int len);
BOOL term_Construct(TERM *pt, unsigned max_lx, unsigned max_ly, unsigned total_lines );
BOOL inline term_Construct(TERM *pt){ return term_Construct(pt, TERM::DEF_MAX_LX, TERM::DEF_MAX_LY, TERM::DEF_TOTAL_LINES ); }
void term_Destruct(TERM *pt);

void term_partial_Reset(TERM *pt);
void reset_rollback_buf(TERM *pt);
void term_setSize(TERM *pt, unsigned lx, unsigned ly);

int term_set_active_window(TERM *pt, unsigned x, unsigned y, unsigned lx, unsigned ly, BOOL is_force_set);
void term_Reset(TERM *pt);

void term_Parse(TERM *pt, const char *buf, unsigned len);
void term_ParseW(TERM *pt, const WCHAR *wbuf, unsigned wlen);

void term_title_header(TERM *pt);
void term_Scroll(TERM* pt, int lines);
void term_Mouse(TERM *pt, int evt, int x, int y);
void term_send_mouse_event(TERM *pt, unsigned evt, int evt_state, unsigned x, unsigned y );

//BOOL term_Echo(TERM *pt);
void term_Logg(TERM *pt, char *fn);
void term_Log_esc(TERM *pt, char *fn);
//void term_Print(TERM *pt, const char *fmt, ...);
//void term_Disp(TERM *pt, const char *buf);

void term_Send(TERM *pt, char *buf, unsigned len);
void term_SendW(TERM *pt, WCHAR *wbuf, unsigned wlen);
void term_Paste(TERM *pt, char *buf, unsigned len);
void term_PasteW(TERM *pt, WCHAR *wbuf, unsigned wlen);

//unsigned term_Copy(TERM *pt, char **buf);
unsigned term_CopyW(TERM *pt, WCHAR **wbufp);
//int  term_Srch(TERM *pt, char *sstr);

/****************tiny.c****************/
extern HWND hwndTerm;
extern RECT termRect;
enum{ EXISTED_PAINT_RECT= 0, NEW_PAINT_RECT };
void request_tiny_redraw( BOOL is_add_whole_screeen );
void request_tiny_NC_redraw();

void tiny_show_cursor();

int utf8_to_wchar(const char *buf, int cnt, WCHAR *wbuf, int wcnt);
int wchar_to_utf8(WCHAR *wbuf, int wcnt, char *buf, int cnt);
int stat_utf8(const char *fn, struct _stat *buffer);
FILE *fopen_utf8(const char *fn, const char *mode);

void tiny_wnd_Size(); //resize window when font or term size changes, hwnd should be hwndTerm
void tiny_Title(WCHAR *wbuf, int wcnt); //set terminal window caption
//is_redraw if caller think window need to repaint
void tiny_Scroll( unsigned scroll_pos, unsigned scroll_range, unsigned scroll_page, BOOL is_redraw );
