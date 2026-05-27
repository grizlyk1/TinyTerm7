//
// tinyTerm7 -- A minimal Windows terminal emulator

#include "stdafx.h"
#include "tinyterm.h"

//define to display more debug messages
#define DBG_BOX

//can not use term_Send()
void term_partial_Reset(TERM *pt)
{
	assert(pt);
	pt->lx= pt->screen_lx; 
	pt->ly= pt->screen_ly;
	pt->top_x= pt->top_y= 0;

	pt->clr_attr = TERM::DEF_CLR_ATTR;

	pt->cursor_x= pt->cursor_y= 0;
	pt->bCursor = TRUE;
	
	pt->bEscape= pt->bUtf8= FALSE;
	pt->bSel= FALSE;
	pt->bInsert= FALSE;
	pt->bWrap= TRUE;
	pt->bKeyb= TRUE;
	pt->bAttrib= FALSE;

	pt->bAppMouse= TRUE;

	pt->tab_sz = TERM::DEF_TAB_SZ;

	pt->bAppTitle= FALSE;
	term_title_header(pt);
}
void term_Clear(TERM *pt)
{
	assert(pt);
	wmemset(pt->data_buf, L' ', pt->buffer_items() );
	wmemset(pt->attr_buf, TERM::DEF_CLR_ATTR, pt->buffer_items() );

	wmemset(pt->alt_data_buf, L' ', pt->alt_buffer_items() );
	wmemset(pt->alt_attr_buf, TERM::DEF_CLR_ATTR, pt->alt_buffer_items() );

	pt->init_rollback_buf();

	pt->bDisScrollBuf = FALSE;

	//the following options presist on terminal reset
	pt->bProgTab = FALSE;

	pt->bLogging= pt->bLogEsc = FALSE;

	pt->bOptCursorBS= TRUE;
	pt->bOptESC_do_CAN= TRUE;
	pt->bOptHideEsc= FALSE;
	//pt->bOptDisplayUTF8= TRUE;
	pt->OptDisplay_mode= TERM::Display_UTF8;
	pt->bOptKeybUTF8= TRUE;

	pt->mouse_mode= TERM::MOUSE_DIS;
	pt->bOptMouseXterm= FALSE;

	pt->bOptXOFF2= TRUE;
	pt->bOptBracket= FALSE;

	pt->title_idx=TERM::TITLE_BASE_SZ;
	pt->title[TERM::TITLE_BASE_SZ]= 0;

	pt->screen_lx= (TERM::DEF_LX <= pt->max_lx()? TERM::DEF_LX : pt->max_lx());
	pt->screen_ly= (TERM::DEF_LY <= pt->max_ly()? TERM::DEF_LY : pt->max_ly());

	term_partial_Reset(pt);
}
BOOL term_Construct(TERM *pt, unsigned max_lx, unsigned max_ly, unsigned total_lines)
{
	assert(pt);
	
	if(!max_lx)max_lx = TERM::DEF_MAX_LX; 
	if(!max_ly)max_ly = TERM::DEF_MAX_LY; 
	if(!total_lines)total_lines = TERM::DEF_TOTAL_LINES;

	pt->v_max_lx = max_lx; pt->v_max_ly = max_ly; pt->v_total_lines = total_lines;

	pt->mtx = CreateMutex(NULL, FALSE, L"term parse mutex");

	pt->data_buf = static_cast<TERM::tt7_data_t*>(malloc( pt->buffer_items()*sizeof(TERM::tt7_data_t) ));
	pt->attr_buf = static_cast<TERM::tt7_attr_t*>(malloc( pt->buffer_items()*sizeof(TERM::tt7_attr_t) ));

	pt->alt_data_buf = static_cast<TERM::tt7_data_t*>(malloc( pt->alt_buffer_items()*sizeof(TERM::tt7_data_t) ));
	pt->alt_attr_buf = static_cast<TERM::tt7_data_t*>(malloc( pt->alt_buffer_items()*sizeof(TERM::tt7_data_t) ));

	if ( !pt->data_buf || !pt->attr_buf || !pt->alt_data_buf || !pt->alt_attr_buf )
	#ifndef DBG_BOX
		{ return FALSE; }
	#else
		{ MessageBoxA(0,"Can not alloc buf data and attr","Error",MB_OK|MB_ICONERROR); return FALSE; }
	#endif
	
	term_Clear(pt);
	return TRUE;
}
void term_Destruct(TERM *pt)
{
	assert(pt);
	free(pt->data_buf); pt->data_buf= NULL; 
	free(pt->attr_buf); pt->attr_buf= NULL;

	free(pt->alt_data_buf); pt->alt_data_buf= NULL; 
	free(pt->alt_attr_buf); pt->alt_attr_buf= NULL;
}

/*
	return 
		1  if requested lx,ly parameters was OK and win was created 
		0  if requested lx,ly parameters was corrected, but win was created by is_force_set
		-1 is_force_set was not set or lx or ly parameters zero, win was not created 
	if is_force_set active window will be set even if input parameters was corrected and lx,ly not zero
*/
int term_set_active_window(TERM *pt, unsigned x, unsigned y, unsigned lx, unsigned ly, BOOL is_force_set)
{
	assert(pt);

	BOOL is_parameters_orig= TRUE;
	if( x > pt->screen_lx )return -1;
	if( y > pt->screen_ly )return -1;

	if( lx > pt->screen_lx ){ lx= pt->screen_lx; is_parameters_orig= FALSE; }
	if( ly > pt->screen_ly ){ ly= pt->screen_ly; is_parameters_orig= FALSE; }
	if( x > pt->screen_lx - lx ){ lx= pt->screen_lx - x; is_parameters_orig= FALSE; }
	if( y > pt->screen_ly - ly ){ ly= pt->screen_ly - y; is_parameters_orig= FALSE; }
	if(!lx || !ly)return -1;

	if( is_parameters_orig || is_force_set ){ 
	pt->top_x= x; pt->top_y= y;	pt->lx= lx;	pt->ly= ly; }

	if(is_parameters_orig) return 1;
	return (is_force_set? 0: -1);
}

//move screen data and head to idx 0 and init_rollback_buf()
void reset_rollback_buf(TERM *pt)
{ 
	assert(pt);

	unsigned src_line = pt->line_head;
	unsigned dst_line= 0;

	//move dy lines of screen data, if pt->line_head != 0
	//if(src_line)

	//screen copy does not use active window RECT

	//temporary screen buf, useful when (TERM::TOTAL_LINES < 2*pt->ly)
	unsigned wsz= pt->ly*pt->max_lx();
	TERM::tt7_data_t *wdata= static_cast<TERM::tt7_data_t*>(_alloca( wsz*sizeof(TERM::tt7_data_t) ));
	TERM::tt7_attr_t *wattr= static_cast<TERM::tt7_attr_t*>(_alloca( wsz*sizeof(TERM::tt7_attr_t) ));

	for( unsigned dy= pt->ly, tmp_line= 0; dy; --dy )
	{
		unsigned src_idx = src_line*pt->max_lx();
		unsigned tmp_idx = tmp_line*pt->max_lx();

		wmemcpy(wdata + tmp_idx, pt->data_buf + src_idx, pt->max_lx());
		wmemcpy(wattr + tmp_idx, pt->attr_buf + src_idx, pt->max_lx());

		src_line= pt->do_line_add1(src_line);
		++tmp_line;
	}

	for( unsigned dy= pt->ly, tmp_line= 0; dy; --dy )
	{
		unsigned tmp_idx = tmp_line*pt->max_lx();
		unsigned dst_idx = dst_line*pt->max_lx();

		wmemcpy(pt->data_buf + dst_idx, wdata + tmp_idx, pt->max_lx());
		wmemcpy(pt->attr_buf + dst_idx, wattr + tmp_idx, pt->max_lx());

		++tmp_line;
		dst_line= pt->do_line_add1(dst_line);
	}

	pt->init_rollback_buf();
}

//"line" is absolute line idx in memory buf, in range [0,TOTAL_LINES)
//"max_lx" is selected size (window size) of "line" line, in range (0,MAX_LX]
//"x" in [0,max_lx); "lx" in (0,max_lx];
#define tune_in_line_params(pt,line,x,lx,max_lx) \
	assert((pt));\
	/* assert( (line) >= TERM::TOTAL_LINES ); */\
	if((line) >= (pt)->total_lines())return;\
	if((max_lx) > (pt)->max_lx()) (max_lx) = (pt)->max_lx();\
	if((x) >= (max_lx))return;\
	if((lx) > (max_lx) - (x)) (lx) = (max_lx) - (x);\
	if(!(lx))return;
//
//add active win check, must be secon check after "tune_in_line_params"
//line is logical line as if active window is left upper corner of screen
//max_lx should be pt->lx for active window
#define tune_in_line_params_win(pt,line,x,lx,max_lx) \
	if((pt)->top_x >= (max_lx))return;\
	if((lx) > (max_lx) - (pt)->top_x) (lx) = (max_lx) - (pt)->top_x;\
	if(!(lx))return;\
	unsigned line_win = (pt)->do_line_index(line, (pt)->top_y);\
	unsigned win_x = ((x) + (pt)->top_x);
//

//line is logical line as if active window is left upper corner of screen
//max_lx should be pt->lx for active window
void clear_in_line(TERM *pt, unsigned line, unsigned x, unsigned lx, TERM::tt7_attr_t clr_attr, unsigned max_lx)
{
	tune_in_line_params(pt,line,x,lx,max_lx);
	tune_in_line_params_win(pt,line,x,lx,max_lx);

	unsigned	idx = line_win*pt->max_lx() + win_x;
				
	wmemset(pt->data_buf + idx, L' ', lx);
	wmemset(pt->attr_buf + idx, clr_attr, lx);
}
//line is logical line as if active window is left upper corner of screen
//max_lx should be pt->lx for active window
void delete_in_line(TERM *pt, unsigned line, unsigned x, unsigned lx, TERM::tt7_attr_t clr_attr, unsigned max_lx)
{
	tune_in_line_params(pt,line,x,lx,max_lx);

	//?is there tail to move
	//unsigned	tail = max_lx - (x + lx);
	//if(!tail){ clear_in_line(pt, line, x, lx, clr_attr, max_lx); return; }

	tune_in_line_params_win(pt,line,x,lx,max_lx);

	unsigned	tail = max_lx - (win_x + lx);
	unsigned	idx = line_win*pt->max_lx() + win_x;
	
	if(tail){
		wmemmove(pt->data_buf + idx, pt->data_buf + idx + lx, tail);
		wmemmove(pt->attr_buf + idx, pt->attr_buf + idx + lx, tail);
	}

	wmemset(pt->data_buf + idx + tail, L' ', lx);
	wmemset(pt->attr_buf + idx + tail, clr_attr, lx);
}
//line is logical line as if active window is left upper corner of screen
//max_lx should be pt->lx for active window
//optional str[lx]
void insert_in_line(
	TERM *pt, unsigned line, unsigned x, unsigned lx, TERM::tt7_attr_t clr_attr, unsigned max_lx, 
	const TERM::tt7_data_t * _In_opt_ str
	){

	tune_in_line_params(pt,line,x,lx,max_lx);
	tune_in_line_params_win(pt,line,x,lx,max_lx);

	unsigned	idx = line_win*pt->max_lx() + win_x;

	//?is there tail to move
	unsigned	tail = max_lx - (win_x + lx);
	if(tail){ 
		wmemmove(pt->data_buf + idx + lx, pt->data_buf + idx, tail);
		wmemmove(pt->attr_buf + idx + lx, pt->attr_buf + idx, tail);
	}

	if(str) wcsncpy(pt->data_buf + idx, str, lx);
	else wmemset(pt->data_buf + idx, L' ', lx);
	wmemset(pt->attr_buf + idx, clr_attr, lx);
}
//line is logical line as if active window is left upper corner of screen
//max_lx should be pt->lx for active window
//optional str[lx]
void print_in_line(
	TERM *pt, unsigned line, unsigned x, unsigned lx, TERM::tt7_attr_t clr_attr, unsigned max_lx, 
	const TERM::tt7_data_t * _In_opt_ str
	){

	tune_in_line_params(pt,line,x,lx,max_lx);
	tune_in_line_params_win(pt,line,x,lx,max_lx);

	unsigned	idx = win_x + line_win*pt->max_lx();
	
	if(str) wcsncpy(pt->data_buf + idx, str, lx);
	else wmemset(pt->data_buf + idx, L' ', lx);
	wmemset(pt->attr_buf + idx, clr_attr, lx);
}
//line is logical line as if active window is left upper corner of screen
//max_lx should be pt->lx for active window
//action:	
//	0 - reverse exising char attr 
//	1 - use exising bg attr | fg clr_attr 
//	2 - use exising fg attr | bg clr_attr 
//	3 - use clr_attr
void set_attr_in_line(
	TERM *pt, unsigned line, unsigned x, unsigned lx, unsigned action, TERM::tt7_attr_t clr_attr, unsigned max_lx
	){
	
	tune_in_line_params(pt,line,x,lx,max_lx);
	tune_in_line_params_win(pt,line,x,lx,max_lx);

	unsigned			idx = line_win*pt->max_lx() + win_x;
	TERM::tt7_attr_t	*wp= pt->attr_buf + idx;
				
	switch(action){
		case 0: //reverse exising char attr 
			for( unsigned dx=0; dx<lx; ++dx ){ 
				unsigned hi = (wp[dx] & 0x00FU) << 4; 
				unsigned lo = (wp[dx] >> 4) & 0x00FU; 
				wp[dx] = static_cast<TERM::tt7_attr_t>(hi|lo); 
			}
			break;
		case 1: //use exising bg attr | fg clr_attr
			for( unsigned dx=0; dx<lx; ++dx ){ 
				unsigned hi = (wp[dx] & 0x0F0U); 
				unsigned lo = (clr_attr) & 0x00FU; 
				wp[dx] = static_cast<TERM::tt7_attr_t>(hi|lo); 
			}
			break;
		case 2: //use exising fg attr | bg clr_attr
			for( unsigned dx=0; dx<lx; ++dx ){ 
				unsigned hi = (clr_attr & 0x0F0U); 
				unsigned lo = (wp[dx]) & 0x00FU; 
				wp[dx] = static_cast<TERM::tt7_attr_t>(hi|lo); 
			}
			break;
		case 3: //use clr_attr
		default:
			wmemset(pt->attr_buf + idx, clr_attr, lx);
	}
}

void term_setSize(TERM *pt, unsigned lx, unsigned ly)
{
	assert(pt);
	//no assert lx,ly - minimized window could send the term_Size
	if(!lx)return;
	if(!ly)return;

	pt->lx = (lx > pt->max_lx())? pt->max_lx(): lx;
	if( pt->lx <= pt->cursor_x ) pt->cursor_x= pt->lx-1;
	
	unsigned new_ly = (ly > pt->max_ly())? pt->max_ly(): ly;
	
	//check tail adjust
	//look up for easy reasons to do not 'init_rollback_buf()' on resize
	if( new_ly > pt->ly ) {
		unsigned new_tail = pt->do_line_index( pt->line_head, new_ly );

		//clear new screen bottom lines 
		unsigned clear_y_from = pt->line_head + pt->ly;
		//unsigned clear_y_to = new_tail; //pt->line_head + new_ly;
		
		//screen ly does not wrap in memory
		if( new_tail > pt->line_head ){ 
			//rollback buf also does not wrap in memory
			if( pt->line_tail <= pt->line_head ){
				//{ new_tail is never stored into pt->line_tail }
				//clear new screen bottom lines from (pt->line_head + pt->ly) to new_tail
			
			//rollback buf does wrap in memory
			}else{
				if( new_tail > pt->line_tail ) pt->line_tail= new_tail;
				//clear new screen bottom lines from (pt->line_head + pt->ly) to new_tail
			}
		
		//screen ly does wrap in memory
		}else{
			//rollback buf does wrap in memory
			if( pt->line_tail <= pt->line_head ){
				if( new_tail > pt->line_tail ) pt->line_tail= new_tail;
				//clear new screen bottom lines from (pt->line_head + pt->ly) to new_tail
			
			//rollback buf does not wrap in memory
			}else{
				pt->line_tail= new_tail; //{ new_tail is always stored into pt->line_tail }
				//clear new screen bottom lines from (pt->line_head + pt->ly) to new_tail
			}
		}

		//clear new screen bottom lines from (pt->line_head + pt->ly) to new_tail
		for( unsigned lines= (new_ly - pt->ly); lines; --lines){
			//clear line 
			clear_in_line(pt, clear_y_from, 0, pt->max_lx(), TERM::DEF_CLR_ATTR, pt->max_lx());
			//next y
			clear_y_from= pt->do_line_add1(clear_y_from);
		}
	}

	//can continue ly without init_rollback_buf();
	pt->ly= new_ly;
	if( pt->ly <= pt->cursor_y ) pt->cursor_y= pt->ly-1;

	//active win support
	pt->screen_lx= pt->lx; pt->screen_ly= pt->ly;
	pt->top_x = pt->top_y = 0;
}

//checkup params for win size edit functions
//y is logical line inside active window
#define tune_in_win_params(pt,n,y) \
	assert((pt));\
	assert((y) < (pt)->ly);\
	if((n) > (pt)->ly - (y))(n)= (pt)->ly - (y);\
	/**/\
	assert((pt)->lx <= (pt)->screen_lx);\
	assert((pt)->top_x <= (pt)->screen_lx - (pt)->lx);\
	unsigned win_x= 0 + (pt)->top_x;\
	/**/\
	assert((pt)->ly <= (pt)->screen_ly);\
	assert((pt)->top_y <= (pt)->screen_ly - (pt)->ly);\
	unsigned win_y= y + (pt)->top_y;\
	unsigned win_ly= (pt)->ly + (pt)->top_y;
//

//scroll up bottom lines in range [y, pt->ly-1) n times
//and insert n empty lines at bottom
//y is logical line inside active window
#define term_delete_lines scroll_forward_and_clear_lines
void scroll_forward_and_clear_lines(TERM *pt, unsigned const y, unsigned n)
{
	tune_in_win_params(pt,n,y);

	//find first dst line
	unsigned dst_line = pt->do_line_index(pt->line_head, win_y);
	//for( unsigned dy= n; dy; --dy ){ dst_line= pt->do_line_add1(dst_line); }

	//in order to not count wrong src index outside screen, 
	//do redundant (n >= pt->ly) condition check
	if(n < pt->ly){
	//find first src line
	unsigned src_line = pt->do_line_index(dst_line, n);
	//for( unsigned dy= n; dy; --dy ){ src_line= pt->do_line_add1(src_line); }

	//screen head will be untached, move dy lines of screen data
	for( unsigned dy= pt->ly - n - y; dy; --dy )
	{
		unsigned src_idx = win_x + src_line*pt->max_lx();
		unsigned dst_idx = win_x + dst_line*pt->max_lx();

		wmemcpy(pt->data_buf + dst_idx, pt->data_buf + src_idx, pt->lx);
		wmemcpy(pt->attr_buf + dst_idx, pt->attr_buf + src_idx, pt->lx);

		src_line= pt->do_line_add1(src_line);
		dst_line= pt->do_line_add1(dst_line);
	}}

	//clear tail of n deleted lines from dst_line in range [dst_line, pt->line_head + pt->ly)
	for( unsigned dy= n; dy; --dy )
	{
		unsigned idx = win_x + dst_line*pt->max_lx();

		wmemset(pt->data_buf + idx, L' ', pt->lx);
		wmemset(pt->attr_buf + idx, pt->clr_attr, pt->lx);

		dst_line= pt->do_line_add1(dst_line);
	}
}

//scroll down bottom lines in range [y, pt->ly-1) n times
//and insert n empty lines at y in range [y, y+n)
//y is logical line inside active window
#define term_insert_lines scroll_back_and_clear_lines
void scroll_back_and_clear_lines(TERM *pt, unsigned const y, unsigned n)
{
	tune_in_win_params(pt,n,y);

	//find first dst line
	unsigned dst_line = pt->do_line_index( pt->line_head, win_ly - 1);
	
	//find first src line
	unsigned src_line = dst_line;
	for( unsigned dy= n; dy; --dy ){ src_line= pt->do_line_sub1(src_line); }

	//screen head will be untached, move dy lines of screen data
	for( unsigned dy= pt->ly - n - y; dy; --dy )
	{
		unsigned src_idx = win_x + src_line*pt->max_lx();
		unsigned dst_idx = win_x + dst_line*pt->max_lx();

		wmemcpy(pt->data_buf + dst_idx, pt->data_buf + src_idx, pt->lx);
		wmemcpy(pt->attr_buf + dst_idx, pt->attr_buf + src_idx, pt->lx);

		src_line= pt->do_line_sub1(src_line);
		dst_line= pt->do_line_sub1(dst_line);
	}

	//clear n inserted lines from dst_line in range [dst_line, pt->line_head]
	//or in range [pt->line_head, pt->line_head + n)
	for( unsigned dy= n; dy; --dy )
	{
		unsigned idx = win_x + dst_line*pt->max_lx();

		wmemset(pt->data_buf + idx, L' ', pt->lx);
		wmemset(pt->attr_buf + idx, pt->clr_attr, pt->lx);

		dst_line= pt->do_line_sub1(dst_line);
	}

	//clear_in_line(pt, pt->line_head + 0, 0, TERM::MAX_LX, TERM::DEF_CLR_ATTR, TERM::MAX_LX);
	//clear_in_line(pt, src_line, 0, TERM::MAX_LX, TERM::DEF_CLR_ATTR, TERM::MAX_LX);
}

//clear n lines at y in range [y, y+n)
//y is logical line inside active window
void term_clear_lines(TERM *pt, unsigned const from_y, unsigned ly, TERM::tt7_attr_t clr_attr)
{
	tune_in_win_params(pt,ly,from_y);

	//find first clr line
	unsigned clr_line = pt->do_line_index( pt->line_head, win_y);
	
	//clear n lines from clr_line in range [clr_line, clr_line+n)
	for( unsigned dy= ly; dy; --dy )
	{
		unsigned idx = win_x + clr_line*pt->max_lx();

		wmemset(pt->data_buf + idx, L' ', pt->lx);
		wmemset(pt->attr_buf + idx, clr_attr, pt->lx);

		clr_line= pt->do_line_add1(clr_line);
	}
}

//set_attr n lines at y in range [y, y+n)
//y is logical line inside active window
void term_set_attr_lines(TERM *pt, unsigned const from_y, unsigned ly, unsigned action, TERM::tt7_attr_t clr_attr)
{
	tune_in_win_params(pt,ly,from_y);

	//find first attr line
	unsigned line= pt->do_line_index( pt->line_head, pt->cursor_y );

	for( unsigned dy= ly; dy; --dy )
	{ 
		//call 'set_attr_in_line' because need per char access for actions
		set_attr_in_line(pt, line, 0, pt->lx, action, pt->clr_attr, pt->lx); 

		line= pt->do_line_add1(line); 
	} 
}

//move top line into scroll roll buf
//move lines up 1 times
//and insert 1 empty line bottom of screen (tty stream behaviour)
// ordinary 'delete_lines' does not add deleted lines to scroll roll buf (window behaviour)
void term_scroll_forward1(TERM *pt)
{
	assert(pt);
	
	//move screen data if scroll disabled
	if( pt->bDisScrollBuf || pt->is_active_window() ){

		term_delete_lines(pt, 0, 1);

	//move screen head if scroll enabled
	//active window is not used 
	}else{
	unsigned new_head = pt->do_line_add1(pt->line_head);
	unsigned new_tail = pt->do_line_index(new_head, pt->ly);

	//check rollback buf tail
	if( pt->line_tail == new_tail )pt->line_tail = pt->do_line_add1(pt->line_tail);
	
	//check visual buf
	if( pt->line_scroll == new_tail )pt->line_scroll = pt->line_tail;
	else if( pt->line_scroll == pt->line_head )pt->line_scroll = new_head;

	pt->line_head = new_head;
	clear_in_line(pt, pt->do_line_sub1(new_tail), 0, pt->max_lx(), TERM::DEF_CLR_ATTR, pt->max_lx());

	//update scrollbar for new rollback buf size
	request_tiny_NC_redraw();
	}
}

//move lines down 1 times
//and insert 1 empty line top of screen 
void term_scroll_back1(TERM *pt){
	assert(pt);
	scroll_back_and_clear_lines(pt, 0, 1);
}

//print char as printable
void term_print_wchar(TERM *pt, TERM::tt7_data_t wch){
	assert(pt);

	TERM::tt7_data_t	wbuf[2]= { wch, 0 };

	unsigned line = pt->do_line_index(pt->line_head, pt->cursor_y);
	//unsigned lx = (pt->bWrap? pt->lx: TERM::MAX_LX);

	if (pt->bInsert ) insert_in_line(pt, line, pt->cursor_x, 1, pt->clr_attr, pt->lx, wbuf);
	else print_in_line(pt, line, pt->cursor_x, 1, pt->clr_attr, pt->lx, wbuf);
}

//print char attribute
void term_print_attr(TERM *pt, TERM::tt7_attr_t utf8_attr){
	assert(pt);

	//convert UTF8 to tt7_attr_t 
	//( >> 2 for UTF8; >>4 for plain 8 bit char pair )
	TERM::tt7_attr_t attr= pt->mk_utf8_attr(utf8_attr);

	unsigned line = pt->do_line_index(pt->line_head, pt->cursor_y);
	//unsigned lx = (pt->bWrap? pt->lx: TERM::MAX_LX);

	//action == 3 //use attr
	enum { ACTION= 3 };
	set_attr_in_line(pt, line, pt->cursor_x, 1, ACTION, attr, pt->lx);
}

//next tty cursor pos
void term_tty_cursor(TERM *pt){
	assert(pt);

	//rmam
	if( !pt->bWrap ){ 
		if( pt->cursor_x + 1 < pt->max_lx() ) ++pt->cursor_x;
	//smam
	}else{ 
		if( pt->cursor_x + 1 < pt->lx ) ++pt->cursor_x;
		else{ 
			pt->cursor_x=0; 
			if( pt->cursor_y + 1 < pt->ly ) ++pt->cursor_y;
			else term_scroll_forward1(pt);
	}}
}

//print printable char, do wrap and scroll if needed
void term_tty_wchar(TERM *pt, TERM::tt7_data_t wch){
	assert(pt);

	//print char as printable
	term_print_wchar(pt,wch);

	//next tty cursor pos
	term_tty_cursor(pt);
}

//print char attribute, do wrap and scroll if needed
void term_tty_attr(TERM *pt, TERM::tt7_attr_t attr){
	assert(pt);

	//print char attribute
	term_print_attr(pt,attr);

	//next tty cursor pos
	term_tty_cursor(pt);
}

void term_save_display(TERM *pt)
{
	assert(pt);

	//save others
	pt->alt_clr_attr = pt->clr_attr;
	pt->alt_cursor_x = pt->cursor_x;
	pt->alt_cursor_y = pt->cursor_y;

	pt->alt_title_idx = pt->title_idx;
	if(pt->title_idx) wmemcpy(pt->alt_title, pt->title, pt->title_idx); 
	pt->alt_title[pt->title_idx]= 0;

	//find first src line
	unsigned src_line = pt->line_head;
	//find first dst line
	unsigned dst_line = 0;

	//copy pt->ly lines of screen data
	for( unsigned dy= pt->screen_ly; dy; --dy )
	{
		unsigned src_idx = src_line*pt->max_lx();
		unsigned dst_idx = dst_line*pt->max_lx();

		wmemcpy(pt->alt_data_buf + dst_idx, pt->data_buf + src_idx, pt->max_lx());
		wmemcpy(pt->alt_attr_buf + dst_idx, pt->attr_buf + src_idx, pt->max_lx());

		src_line= pt->do_line_add1(src_line);
		++dst_line;
	}
}
void term_restore_display(TERM *pt)
{
	assert(pt);

	//restore others
	pt->clr_attr = pt->alt_clr_attr;
	pt->cursor_x = pt->alt_cursor_x;
	pt->cursor_y = pt->alt_cursor_y;

	pt->title_idx = pt->alt_title_idx;
	if(pt->title_idx) wmemcpy(pt->title, pt->alt_title, pt->title_idx); 
	pt->title[pt->title_idx]= 0;

	//find first src line
	unsigned src_line = 0;
	//find first dst line
	unsigned dst_line = pt->line_head;

	//copy pt->ly lines of screen data
	for( unsigned dy= pt->screen_ly; dy; --dy )
	{
		unsigned src_idx = src_line*pt->max_lx();
		unsigned dst_idx = dst_line*pt->max_lx();

		wmemcpy(pt->data_buf + dst_idx, pt->alt_data_buf + src_idx, pt->max_lx());
		wmemcpy(pt->attr_buf + dst_idx, pt->alt_attr_buf + src_idx, pt->max_lx());

		++src_line;
		dst_line= pt->do_line_add1(dst_line);
	}
}

void term_Reset(TERM *pt)
{
	assert(pt);
	term_partial_Reset(pt);

	term_clear_lines(pt, 0, pt->ly, pt->clr_attr);
	
	request_tiny_redraw(NEW_PAINT_RECT);
}

//scan incoming char p for UTF8 (can be partial UTF8) and store to wchar (*wp)
//input p[len], (*wp)
//return wlen for 'WCHAR wp [wlen == (wp - wp_in)]'
unsigned
	pre_parse_utf8(
		TERM *pt, 
		const unsigned char _In_ * _In_ p, 
		unsigned _In_ len, 
		WCHAR _Out_ * _In_ wp
		//, unsigned _In_ wlen  //to control wp dest size is enough
	){

	assert(pt);
	assert(p);
	assert(wp);
	if(!len)return 0;

	//unsigned wlen= 0;
	//const unsigned char *p= *pp;
	WCHAR *wp_in= wp;

	for(; len; --len){
		unsigned char c = *p++;
		//?continue prev utf8 seq
		if( pt->bUtf8 ){
			//cache 'utf8_sz(pt->utf8_code[0])'
			//also used as 'is_utf8_ok' for utf8_to_wchar
			unsigned utf8_sz = pt->utf8_sz( pt->utf8_code[0] ); 

			//store incoming utf8
			pt->utf8_code[pt->utf8_filled++] = c;

			//?continue good UTF8 char
			if( c >= 0x80U && c < 0xC0U ){ 
				//if( pt->utf8_idx < TERM::UTF8_SZ )break; 
				//?continue to next len
				if( pt->utf8_filled < utf8_sz )continue; 

			//invalid UTF8 char
			}else{
				//!is_utf8_ok
				utf8_sz= 0;
			}

			//anyway completed Utf8
			pt->bUtf8= FALSE;
			//ok, utf8 to wchar
			if( utf8_sz ){
				unsigned wlen = utf8_to_wchar(pt->utf8_code, pt->utf8_filled, wp, pt->utf8_filled);
				wp += wlen;
			//error, just copy from pt->utf8_code[pt->utf8_idx]
			}else{
				for( unsigned i= 0; i<pt->utf8_filled; ++i )
					{ *wp++ = unsigned(unsigned char( pt->utf8_code[i] )); }
			}

			//anyway continue to next len
			continue; 
		//if prev
		}

		//?start new utf8 seq
		if( c > 0xC1U && c < 0xFEU)
		{
			pt->bUtf8= TRUE;
			pt->utf8_code[0]= c;
			pt->utf8_filled= 1;
			//anyway continue to next len
			continue; 
		} 

		//just copy from c
		*wp++ = unsigned(c);
	//for
	}

	//here little ptrdiff_t values, unsigned is enough
	return (wp - wp_in);
}
//scan buf for UTF8 (can be partial UTF8 seq) and store to wchar
void term_Parse(TERM *pt, const char *buf, unsigned len)
{
	assert(pt);
	assert(buf);
	if(!len)return;

	if (pt->bLogging ){ assert( pt->fpLogFile ); fwrite( buf, 1, len, pt->fpLogFile); }

	unsigned wsz= len;
	//pt can hold up to 5 chars of prev incompleted UTF8 seq 
	//if new incoming char is incorrect UTF8 seq 
	// then prev collected seq will be returned without UTF8 conversion and added to buf[len] 
	//we add +16 to WBUF_SZ to ensure wbuf will be enough to receive all of them
	enum { WBUF_SZ_RESERVED = 16 };
	WCHAR	*wbuf= static_cast<WCHAR*>(_alloca( (wsz + WBUF_SZ_RESERVED)*sizeof(WCHAR) )); 

	//option, display as UTF8 or :As: mode
	//if( pt->bOptDisplayUTF8 )
	if( pt->OptDisplay_mode == TERM::Display_UTF8 || pt->bAttrib )
	{
		unsigned wlen = pre_parse_utf8(pt, (const unsigned char *)buf, len, wbuf );
		if(wlen)term_ParseW(pt, wbuf, wlen);

	//display as plain 8 bit chars
	}else{
		unsigned wlen = len;
		WCHAR	*wp = wbuf;
		const char *p = buf;
		//here always map to low byte 0x00-0xFF of UTF16LE
		//Windows codepage for 0xA0-0xFF exists only in paint function
		for( unsigned i= wlen; i; --i){ *wp++ = unsigned(*p++) & 0x0FFU; }
		if(wlen)term_ParseW(pt, wbuf, wlen);
	}
}

//const unsigned char *parse_EscapeW(TERM *pt, const unsigned char *sz, unsigned cnt);
static
const WCHAR *parse_EscapeW(TERM *pt, const WCHAR *wsz, unsigned wcnt);

void term_ParseW(TERM *pt, const WCHAR *wbuf, unsigned wlen)
{
	assert(pt);
	assert(wbuf);
	if(!wlen)return;

	const WCHAR *p= wbuf;
	const WCHAR *zz = p+wlen;

	if ( WaitForSingleObject(pt->mtx, INFINITE)!=WAIT_OBJECT_0 ) return;
	
	//logging moved to UTF8
	//if (pt->bLogging ) fwrite( buf, 1, len, pt->fpLogFile);

	//?continue ESC, can not occure in pt->bAppTitle mode
	if (pt->bEscape ) p = parse_EscapeW(pt, p, zz-p);

	while ( p < zz ) {
		WCHAR wc = *p++;

		//continue Title mode
		if (pt->bAppTitle){
			if( wc != 7){
				//write next char to Title
				if ( pt->title_idx < TERM::TITLE_SZ ) pt->title[pt->title_idx++] = wc;
				continue;
			}
			//Title EOL, back to screen
			pt->bAppTitle= FALSE;
			pt->title[pt->title_idx] = 0; //ensure
			term_title_header(pt);
			//request_tiny_NC_redraw();
			continue;
		}

		//continue screen mode
		switch ( wc ) {
		case 0x07:	//BEL, tiny_Beep() + drop 
		//case 0x0e:
		//case 0x0f: 	
		case 0x00:	//NUL, drop (use any form '\E\000' to print the \000 symbol)
			break;	
		case 0x7F:	//DEL, destructive smir: \E[P; rmir: 32, 8
			if(pt->cursor_x < pt->lx){ 
				if(pt->bInsert) delete_in_line(pt, pt->do_line_index(pt->line_head, pt->cursor_y), pt->cursor_x, 1, pt->clr_attr, pt->lx);
				else clear_in_line(pt, pt->do_line_index(pt->line_head, pt->cursor_y), pt->cursor_x, 1, pt->clr_attr, pt->lx);
			}
			break;
		case 0x08:	//BS, destructive smir: 8, \E[P; rmir: 8, 32, 8
			if(pt->cursor_x){ 
				//cursor motion
				--pt->cursor_x; 
				//option, destructive BS
				if( !pt->bOptCursorBS ){
					if(pt->bInsert) delete_in_line(pt, pt->do_line_index(pt->line_head, pt->cursor_y), pt->cursor_x, 1, pt->clr_attr, pt->lx);
					else clear_in_line(pt, pt->do_line_index(pt->line_head, pt->cursor_y), pt->cursor_x, 1, pt->clr_attr, pt->lx);
				}
			}
			break;
		case 0x09:	//TAB, cursor motion
			{ unsigned new_ts = pt->tab_sz - ( (pt->cursor_x + 1) % pt->tab_sz);
			if( pt->cursor_x + new_ts < pt->lx )pt->cursor_x += new_ts;	}
			break;
		case 0x0a:	//LF, in any V form: cursor motion + scroll up
		case 0x0b:	//VT 
			if( pt->cursor_y + 1 < pt->ly ) ++pt->cursor_y;
			else term_scroll_forward1(pt);
			break;
		case 0x0c:	//FF, destructive cursor up to 0 row of current col and clear screen:  \E[1;H, \E[2J
			pt->cursor_y= 0;
			term_clear_lines(pt, 0, pt->ly, pt->clr_attr);
			break;
		case 0x0d:	//CR, cursor motion
			if( pt->cursor_x ) pt->cursor_x= 0;
			break;
		case 0x11:	//XON, terminal output keyb+mouse events
			pt->bKeyb= TRUE;
			term_title_header(pt);
			break;
		//XOFF2 moved to \ES
		case 0x13:	//XOFF, terminal discard keyb+mouse events
			if(pt->bOptXOFF2)goto default_next_tty;
			pt->bKeyb= FALSE;
			term_title_header(pt);
			break;
		case 0x1b:	//ESC, start ESC seq
			pt->bEscape= TRUE;
			wmemset(pt->escape_wcode, 0, TERM::SEQ_SZ);
			pt->escape_wcode[0]= 27;
			pt->escape_filled= 1;
			p = parse_EscapeW(pt, p, zz-p); 
			break;
		default:
		default_next_tty:
			//continue Attrib mode
			if (pt->bAttrib){
				//write next attribute
				term_tty_attr(pt, static_cast<TERM::tt7_attr_t>(wc & 0xFFFFUL) );
				continue;
			}

			//consolas font does not contain glyphs for all codes in range 0-127
			//map 0-31 to 0xa0-0xbF, the mapping done in paint, but memory contain real data whithout mapping
			term_tty_wchar(pt, static_cast<TERM::tt7_data_t>(wc & 0xFFFFUL) );
		}
	}

	ReleaseMutex(pt->mtx);
	
	request_tiny_redraw(NEW_PAINT_RECT);
}

//print term title header: "TT7-------: "
static WCHAR *title_base_tpl= L"TT7%c%c%c%c%c%c%c: ";
void term_title_header(TERM *pt)
{
	assert(pt);

	_snwprintf( pt->title, TERM::TITLE_BASE_SZ, title_base_tpl, 
		(pt->bKeyb? 		L'-':L'k'),  
		(pt->mouse_mode? 	L'm':L'-'),
		(pt->bWrap? 		L'-':L'u'),
		(pt->bInsert? 		L'i':L'-'),
		(pt->bAttrib? 		L'b':L'-'),
		(pt->is_active_window()? 	
							L'a':L'-'),
		(pt->bAppTitle? 	L'T':L'-')
		);

	request_tiny_NC_redraw();
}

//lines>0 move line_scroll towards line_tail (backward, up)
void term_Scroll(TERM *pt, int lines)
{	
	assert(pt);
	//scroll is not allowed
	if( pt->bDisScrollBuf )return;

	//!lines used to reset scroll bar
	//if(!lines)return;
	BOOL is_redraw= lines? TRUE: FALSE;
	
	//pt->line_scroll= pt->do_line_offs( pt->line_scroll, lines);
	if(lines > 0){
		//already scrolled max backward
		//if( pt->line_scroll == pt->line_tail )return; 
		for(;
			lines
			&& (pt->line_scroll != pt->line_tail);
				--lines, 
				pt->line_scroll= pt->do_line_sub1(pt->line_scroll)
			){}
	}
	else if(lines < 0){
		//already scrolled max forward
		//if( pt->line_scroll == pt->line_head )return; 
		for(;
			lines
			&& (pt->line_scroll != pt->line_head);
				++lines, 
				pt->line_scroll= pt->do_line_add1(pt->line_scroll)
			){}
	}else {
		// (lines==0) used to reset scroll bar
		//invalidate NCarea, including scrollbar 
	}

	unsigned scroll_pos= pt->do_line_diff( pt->line_scroll, pt->line_tail );
	unsigned scroll_range= pt->do_line_diff( pt->do_line_index(pt->line_head, pt->ly), pt->line_tail );
	unsigned scroll_page= pt->ly; 
	tiny_Scroll( scroll_pos, scroll_range, scroll_page, is_redraw );
}
void term_Mouse(TERM *pt, int evt, int x, int y)
{
	assert(pt);

	switch( evt ) {
	case TERM::DOUBLECLK:
	case TERM::LEFTDOWN:
		//start new selection, set m1
		pt->bSel= FALSE;
		if( static_cast<unsigned>(y) >= pt->screen_ly ) y= pt->screen_ly-1; 
		if( static_cast<unsigned>(x) >= pt->screen_lx ) x= pt->screen_lx-1;
		pt->sel_line_m1= pt->do_line_index(pt->line_scroll, y);
		//pt->sel_y_m1= y;
		pt->sel_x_m1= x;
		break;
	case TERM::LEFTUP:
		//fixup new selection, set m2
		if(!pt->bSel)return;
		//here no break with intention
	case TERM::LEFTDRAG:
		//set new selection, track in m2
		pt->bSel= TRUE;
		if( static_cast<unsigned>(y) >= pt->screen_ly ) y= pt->screen_ly-1; 
		if( static_cast<unsigned>(x) >= pt->screen_lx ) x= pt->screen_lx-1;
		pt->sel_line_m2= pt->do_line_index(pt->line_scroll, y);
		pt->sel_x_m2= x;
		break;
	default:
		return;
	}

	request_tiny_redraw(NEW_PAINT_RECT);
}
void term_send_mouse_event(TERM *pt, unsigned evt, int evt_state, unsigned x, unsigned y)
{
	assert(pt);
	//no active window selected
	if( !pt->is_active_window() ){ 
		
		if( y >= pt->ly ) y= pt->ly-1;  
		if( x >= pt->lx ) x= pt->lx-1;

	}else{
		//check mouse pointer is outside of margins of active window
		//active window can not 'capture mouse'
		if( y < pt->top_y || (y >= pt->top_y + pt->ly) )return;
		if( x < pt->top_x || (x >= pt->top_x + pt->lx) )return;

		//move mouse pointer from screen base to active window cursor base
		x -= pt->top_x; 
		y -= pt->top_y;
	}

	//'bxy' format 'xy' limits
	if( y >= 256-32 ) y= 256-32-1; 
	if( x >= 256-32 ) x= 256-32-1; 

	//'bxy' chars 'xy' = 0x20+number+1
	enum { IDX_BUT=3, IDX_X, IDX_Y, MSG_SZ };
	char buf[MSG_SZ*2+1]= { 
		//TT7-compatible single, buf[0] to send len=MSG_SZ
		27, '[', 'M', 0x20U, x + 0x21U, y + 0x21U, 
		//xterm-compatible single, buf[MSG_SZ] to send len=MSG_SZ
		27, '[', 'M', 0x23U, x + 0x21U, y + 0x21U, 0 };
	unsigned msg_len= MSG_SZ;

	switch(evt){
		//evt_state value 1 if pressed; 
	case TERM::MOUSE_BUT_1:	
			if(evt_state){ 
				buf[IDX_BUT]=0x01U + 0x21U; 
				buf[MSG_SZ+IDX_BUT]= 0x00U + 0x20U;
				msg_len= 2*MSG_SZ; // pair
			}else { 
				buf[IDX_BUT]=0x01U + TERM::MOUSE_BUT_UP + 0x21U;
				buf[MSG_SZ+IDX_BUT]= 0x03U + 0x20U;
				msg_len= 2*MSG_SZ; // pair
			} 
			break;
	case TERM::MOUSE_BUT_2:	
			if(evt_state){ 
				buf[IDX_BUT]=0x02U + 0x21U; 
				buf[MSG_SZ+IDX_BUT]= 0x01U + 0x20U;
				msg_len= 2*MSG_SZ; // pair
			}else { 
				buf[IDX_BUT]=0x02U + TERM::MOUSE_BUT_UP + 0x21U;
				buf[MSG_SZ+IDX_BUT]= 0x03U + 0x20U;
				msg_len= 2*MSG_SZ; // pair
			} 
			break;
		//evt_state value is pressed button number to drag
	case TERM::MOUSE_MOVE:	
			{ 
			buf[IDX_BUT]= evt_state + TERM::MOUSE_MOVE + 0x21U; 
			buf[MSG_SZ+IDX_BUT]= evt_state + TERM::MOUSE_MOVE + 0x20U;
			msg_len= 2*MSG_SZ; // pair
			}
			break;
		//evt_state sign is rotation direction (0x60, 0x61 for xterm)
	case TERM::MOUSE_WHEEL: 
		buf[IDX_BUT]= ((evt_state >= 0)? 0x00U: 0x01U) + TERM::MOUSE_WHEEL + 0x21U; 
		buf[MSG_SZ+IDX_BUT]= ((evt_state >= 0)? 0x00U: 0x01U) + TERM::MOUSE_WHEEL + 0x20U; 
		msg_len= 2*MSG_SZ; // pair
		break;
	default: return;
	}

	//
	char *p=buf;

	//if Xterm mode and msg has pair, write only second msg
	//else write all msg
	if( pt->bOptMouseXterm && msg_len > MSG_SZ ){
		p=  buf+MSG_SZ;
	}
	//short msg format \e[Mbxy
	msg_len= MSG_SZ;

	//mouse mode_4 format \e[Mb
	if( pt->mouse_mode == TERM::MOUSE_MODE_4 ){
		msg_len= IDX_BUT+1;
	}

	term_Send(pt, p, msg_len);
}

void term_Send(TERM *pt, char *buf, unsigned len)
{
	assert(pt);
	assert(buf);
	if(!len)return;

	//keyb XOFF, discard events
	if(!pt->bKeyb)return;

	int res= write(wfPTY_stdin, buf, len);
	if( res != len ){
	#ifndef DBG_BOX
		{ exit(len); }
	#else
		{ MessageBoxA(0,"Can not write wfPTY_stdin","Error",MB_OK|MB_ICONERROR); exit(len); }
	#endif
	}
}
void term_SendW(TERM *pt, WCHAR *wbuf, unsigned wlen)
{
	assert(pt);
	assert(wbuf);
	if(!wlen)return;

	//worst case every incoming WCHAR will be converted to UTF8[6] (to 6 bytes)
	unsigned sz= wlen*6;
	char	*buf= static_cast<char*>(_alloca( (sz+2)*sizeof(char) ));

	//option, keyb send UTF8 encoded
	if( pt->bOptKeybUTF8 ){
		unsigned len= wchar_to_utf8(wbuf, wlen, buf, sz);
		if(len)term_Send(pt, buf, len);

	//keyb send plain 8 bit chars
	}else{
		char def_char= 0x1A; //SUB
		//CP_ACP (1251), CP_OEMCP (866), etc
		//WideCharToMultiByte(CP_ACP, 0, wbuf, wlen, buf, sz, &def_char, 0);
		//we input chars in the same codepage that display if OptDisplay_mode is not UTF8 (always CP_ACP except CP_OEMCP)
		unsigned const cp = (pt->OptDisplay_mode == TERM::Display_OEM? CP_OEMCP: CP_ACP);
		WideCharToMultiByte(cp, 0, wbuf, wlen, buf, sz, &def_char, 0);

		unsigned len= wlen;
		if(len)term_Send(pt, buf, len);
	}
}
void term_Paste(TERM *pt, char *buf, unsigned len)
{
	assert(pt);
	assert(buf);
	if(!len)return;
	
	//if (pt->bOptBracket ) term_Send(pt, "\033[200~", 6);
	term_Send(pt, buf, len);
	//if (pt->bOptBracket ) term_Send(pt, "\033[201~", 6);
}
void term_PasteW(TERM *pt, WCHAR *wbuf, unsigned wlen)
{
	assert(pt);
	assert(wbuf);
	if(!wlen)return;
	//if (pt->bOptBracket ) term_Send(pt, "\033[200~", 6);
	term_SendW(pt, wbuf, wlen);
	//if (pt->bOptBracket ) term_Send(pt, "\033[201~", 6);
}

//return size (based on pt->lx) of WCHAR wbuf to store selected str 
unsigned  term_CopyW_count_sz(TERM *pt)
{
	assert(pt);
	if(!pt->bSel){ return 0; }

	//count not less then needed lines*(pt->lx+\n)+\0
	return (abs( pt->do_line_diff( pt->sel_line_m1, pt->sel_line_m2 ) ) + 1)*(pt->lx + 1);
}
//return WCHAR wbuf[ term_CopyW_count_sz(pt) ] dynamic allocated by malloc
//external sould free the *wbufp by free
//copy selected str from pt data buf to the wbuf + trailing L'\0'
//return size of copied str
unsigned  term_CopyW(TERM *pt, WCHAR * _Out_ * _In_ wbufp)
{
	assert(pt);
	assert(wbufp);
	if(!pt->bSel){ *wbufp= 0; return 0; }

	unsigned wbuf_sz= term_CopyW_count_sz(pt);
	if(!wbuf_sz){ *wbufp= 0; return 0; }

	++wbuf_sz; //trailing L'\0'
	*wbufp= static_cast<WCHAR*>(malloc( wbuf_sz*sizeof(WCHAR) ));

	unsigned line1=0, line2=0;
	unsigned x1=0, x2=0;
	
	//m1
	line1= (pt)->sel_line_m1;
	x1= (pt)->sel_x_m1;
	//if( line1 >= TERM::TOTAL_LINES)line1= TERM::TOTAL_LINES -1;
	//if( x1 >= TERM::MAX_LX)x1= TERM::MAX_LX -1;
	//idx1= line1*TERM::MAX_LX + x1;

	//m2
	line2= (pt)->sel_line_m2;
	x2= (pt)->sel_x_m2;
	//if( line2 >= TERM::TOTAL_LINES)line2= TERM::TOTAL_LINES -1;
	//if( x2 >= TERM::MAX_LX)x2= TERM::MAX_LX -1;
	//idx2= line2*TERM::MAX_LX + x2;

	//m1 > m2
	if( pt->do_line_diff(line1, line2) > 0 )
	{ 
		unsigned 
		//tmp= idx1; idx1= idx2; idx2= tmp; 
		tmp= line1; line1= line2; line2= tmp;
		tmp= x1; x1= x2; x2= tmp;
	}

	unsigned idx1= line1*pt->max_lx() + x1;
	TERM::tt7_data_t *wp1 = pt->data_buf + idx1;

	unsigned wlen= 0;
	WCHAR *wp_dst= *wbufp;
	*wp_dst= 0;

	unsigned ly= line2 - line1;

	//single line selection
	if(!ly){
		unsigned idx2= line2*pt->max_lx() + x2;
		TERM::tt7_data_t *const wp2 = pt->data_buf + idx2;

		unsigned wlx= 0;
		for( ; wp1 <= wp2; ++wlx ){ *wp_dst++ = *wp1++; }
		//skip trailing spaces
		for( --wp_dst; wlx; --wlx ){ if(*wp_dst != L' ')break; --wp_dst; }
		*++wp_dst= 0;
		return wlx;
	}

	//multy line selection
	WCHAR *wpy= wp1;

	//first line
	if( x1 < pt->lx ){
		WCHAR *wpx= wpy;
		unsigned x;
		//copy up to pt->lx
		for( x= x1; x<pt->lx; ++x ){ *wp_dst++ = *wpx++; }
		//skip trailing spaces
		for( --wp_dst; x>x1; --x ){ if(*wp_dst != L' ')break; --wp_dst; }
		*++wp_dst= 0;
		wlen += x - x1;
	}

	//max src buf addr to check screen buf wrap
	WCHAR *const wpy_max = pt->data_buf + pt->total_lines() * pt->max_lx();

	//point to beginning of next lines 
	wpy -= x1;

	//middle lines
	if(ly > 1){
	for( ; ly>1; --ly){
		wpy+= pt->max_lx();
		//?wrap to line 0
		if( wpy >= wpy_max )wpy = pt->data_buf;
		*wp_dst++ = 10; //L'\n'
		++wlen;

		WCHAR *wpx= wpy;
		unsigned x;
		//copy up to pt->lx
		for( x= 0; x<pt->lx; ++x ){ *wp_dst++ = *wpx++; }
		//skip trailing spaces
		for( --wp_dst; x; --x ){ if(*wp_dst != L' ')break; --wp_dst; }
		*++wp_dst= 0;
		wlen += x;
	}}

	//last line
	if( x2 < pt->lx ){
		wpy+= pt->max_lx();
		//?wrap to line 0
		if( wpy >= wpy_max )wpy = pt->data_buf;
		*wp_dst++ = 10; //L'\n'
		++wlen;

		WCHAR *wpx= wpy;
		unsigned x;
		//copy up to x2
		for( x= 0; x<=x2; ++x ){ *wp_dst++ = *wpx++; }
		//skip trailing spaces
		for( --wp_dst; x; --x ){ if(*wp_dst != L' ')break; --wp_dst; }
		*++wp_dst= 0;
		wlen += x;
	}

	//and *wbufp
	if(!wlen){ free(*wbufp); *wbufp= 0; }
	return wlen;
}

void term_Logg(TERM *pt, char *fn)
{
	enum { log_buf_SZ= 80 };
	static char log_buf[log_buf_SZ];

	if (pt->bLogging ) {
		fclose(pt->fpLogFile);
		pt->fpLogFile= 0;
		pt->bLogging = FALSE;
	}
	else if ( fn!=NULL ) {
		if ( *fn==' ' ) fn++;
		pt->fpLogFile = fopen_utf8( fn, "ab");
		if (pt->fpLogFile != NULL ) {
			setvbuf( pt->fpLogFile, log_buf, _IOLBF, log_buf_SZ);
			pt->bLogging = TRUE;
		}
	}
}
void term_Log_esc(TERM *pt, char *fn)
{
	enum { log_buf_SZ= 80 };
	static char log_buf[log_buf_SZ];

	if (pt->bLogEsc ) {
		fclose(pt->fpLogEscFile);
		pt->fpLogEscFile= 0;
		pt->bLogEsc = FALSE;
	}
	else if ( fn!=NULL ) {
		if ( *fn==' ' ) fn++;
		pt->fpLogEscFile = fopen_utf8( fn, "ab");
		if (pt->fpLogEscFile != NULL ) {
			setvbuf( pt->fpLogEscFile, log_buf, _IOLBF, log_buf_SZ);
			pt->bLogEsc = TRUE;
		}
	}
}

//const unsigned char *parse_Escape(TERM *pt, const unsigned char *sz, unsigned cnt)
static
const WCHAR *parse_EscapeW(TERM *pt, const WCHAR *wsz, unsigned wcnt)
{
	assert(pt);
	if(!wcnt)return wsz;
	assert(wsz);

	//bEscape must be set ON externally
	if(!pt->bEscape)return wsz;

	//sizeof wchar_t is unknown, signedness is undefined 
	//using acsess 'unsigned(*wzz)' to clarify in conditions
	const wchar_t *wzz = wsz + wcnt;
	
	while ( wsz < wzz && pt->bEscape ) 
	{
		//seq too long, stop seq
		if (pt->escape_filled >= TERM::SEQ_SZ ){ pt->bEscape = FALSE; break; }

		//control code is subject to reparse, they can not be reprinted from pt->escape_wcode[] as wrong chars
		//if ( unsigned(*wsz) < 32 || unsigned(*wsz) ==  127 ) {
		if ( unsigned(*wsz) == 27 ) {
			//wrong char, but is subject to reparse, do not include the char into pt->escape_wcode[]
			pt->bEscape = FALSE;
			break;
		}
		//not control code is not the same
		WCHAR	wc;
		wc = pt->escape_wcode[pt->escape_filled++] = *wsz++;
		
		//idx will be limited by zero filled pt->escape_wcode in switch, not by pt->escape_filled
		unsigned idx=1;

		WCHAR	 wcs[2] = {0, 0};	//wchar + \0 
		unsigned line;				//line in memory buf

		//max number of parameters of %d;%d format
		enum { MAX_D_PARAMS= 10 };
		//parameters of %d;%d format
		unsigned d_param[MAX_D_PARAMS]= {};			//parameter value
		//unsigned d_found;							//number of found params
		BOOL	 is_d_explicit[MAX_D_PARAMS]= {};	//TRUE if parameter was set explicitly
		//BOOL	 is_unsupported= FALSE;				//unsupported seq
		BOOL	 is_prefixed= FALSE;				//\e[<prefix> form

		switch( pt->escape_wcode[idx] ){
		case 0:		//seq incomplete, wait next chars in pt->bEscape mode
			//never occure, already checked in while (wsz < wzz)
			continue;
		case L'^':
			++idx;
			//yet no extra data
			if( idx == pt->escape_filled )continue;

			//quoted print control char 0-31,127 as printable @A-Z[\\]^_? 
			{WCHAR wtmp= 0x100U;
			if( wc == L'?' ) wtmp= 127;
			else if( unsigned(wc) >= L'@' && unsigned(wc) <= L'_') wtmp= unsigned(wc) - 64;

			if( wtmp != 0x100U ){
				
				term_tty_wchar(pt, wtmp );
				pt->bEscape = FALSE;
				return wsz;
			}}
			//wrong char, stop seq
			pt->bEscape = FALSE;
			break;

		case L']':
			++idx;
			//yet no extra data
			if( idx == pt->escape_filled )continue;

			//check %d;%d format in pt->escape_wcode[ pt->escape_filled-1 ]
			if( isdigit( wc )
				){
				//collecting data next up to "action" letter suffix
				continue;
			}

			//check suffix
			if( wc != ';' ){
				//wrong char, stop seq
				pt->bEscape = FALSE;
				break;
			}
			
			//count [0] if exist in range [idx, pt->escape_filled - 1)
			for( ; idx < pt->escape_filled - 1; ++idx )
			{
				//digit or ';' delimiter, for both case set 'is_d_explicit'
				is_d_explicit[0] = TRUE;
				
				wc= pt->escape_wcode[idx];
				//is digit
				if( isdigit(wc) ){
					d_param[0] = d_param[0]*10 + (wc - 0x30U);
					continue;
				} 
				//else is ';' delimiter
				break; 
			}
			
			//check value before ';' 
			if(d_param[0] == 0){
				//\e]0; entering in Title mode
				pt->bAppTitle = TRUE;
				pt->title_idx= TERM::TITLE_BASE_SZ;
				
				term_title_header(pt);
				
				//now app will output to title string until \007, no control chars working in title
				pt->bEscape = FALSE;
				return wsz;
			}
			//wrong char, stop seq
			pt->bEscape = FALSE;
			break;

		case L'[':
			++idx;

			//check prefix <letter>
			switch(pt->escape_wcode[idx]){
				case 0:		//seq incomplete, wait next chars in pt->bEscape mode
					continue;
				case L'?':
					//collect data and skip unknown
					is_prefixed= TRUE;
					++idx;
					break;
			}

			//yet no extra data
			if( idx == pt->escape_filled )continue;

			//check %d;%d format in pt->escape_wcode[ pt->escape_filled-1 ]
			if( isdigit( wc )
				|| ( wc == ';' )
				){
				//collecting data next up to "action" letter suffix
				continue;
			}
			//count d_found if exist in range [idx, pt->escape_filled - 1)
			for( unsigned d_found=0; idx < pt->escape_filled - 1; ++idx )
			{
				if(d_found >= MAX_D_PARAMS)break;
				//digit or ';' delimiter, for both case set 'is_d_explicit'
				is_d_explicit[d_found] = TRUE;
				
				wc= pt->escape_wcode[idx];
				//is digit
				if( isdigit(wc) ){
					d_param[d_found] = d_param[d_found]*10 + (wc - 0x30U);
					continue;
				} 
				//else is ';' delimiter
				++d_found; 
			}
			idx = pt->escape_filled - 1;

			//check suffix <letter>
			if( is_prefixed ){
				//2 is idx of prefix
				switch( pt->escape_wcode[2] ){
				case L'?':
					break;
					#if 0
					//do not report well-known unsupported
					switch(d_param[0]){
					case 2004: 
					case 47:
						switch( pt->escape_wcode[idx] ){
						case L'h':
						case L'l':
							//drop this
							pt->bEscape = FALSE;
							return wsz;
						}
					}
					#endif
				}
				//wrong char, stop seq
				pt->bEscape = FALSE;
			}else
			switch(pt->escape_wcode[idx]){
				case 0:		//seq incomplete, wait next chars in pt->bEscape mode
					continue;
				case L'K':	//clear EOL, clear lines
					if( pt->cursor_x < pt->lx && pt->cursor_y < pt->ly ){
						line = pt->do_line_index( pt->line_head, pt->cursor_y );
						switch( d_param[0] ){
						case 1: //to left
							clear_in_line(pt, line, 0, (pt->cursor_x + 1), pt->clr_attr, pt->lx); 
							break;
						case 2: //both
							clear_in_line(pt, line, 0, pt->lx, pt->clr_attr, pt->lx);
							//clear lines
							{unsigned n= d_param[1]; 
							if( n >= 2 && pt->cursor_y + 1 < pt->ly ){ 
								--n;
								if( n >= pt->ly - pt->cursor_y - 1 )n = pt->ly - pt->cursor_y - 1; 
								if( n )term_clear_lines(pt, pt->cursor_y + 1, n, pt->clr_attr);
							}}
							break;
						default: //0 or else to right
							clear_in_line(pt, line, pt->cursor_x, (pt->lx - pt->cursor_x), pt->clr_attr, pt->lx);
					}}
					pt->bEscape = FALSE;
					return wsz;
				case L'J':	//clear EOS
					if ( d_param[0] == 4 ) goto lab_4J;
					if( pt->cursor_x < pt->lx && pt->cursor_y < pt->ly ){
						switch( d_param[0] ){
						case 1: //to left
							line = pt->do_line_index( pt->line_head, pt->cursor_y );
							clear_in_line(pt, line, 0, (pt->cursor_x + 1), pt->clr_attr, pt->lx);
							if(pt->cursor_y)term_clear_lines(pt, 0, (pt->cursor_y - 1), pt->clr_attr);
							break;
						case 2: //both
							term_clear_lines(pt, 0, pt->ly, pt->clr_attr);
							break;
						case 4: //cursor home + both
						lab_4J:
							pt->cursor_x= pt->cursor_y= 0;
							term_clear_lines(pt, 0, pt->ly, pt->clr_attr);
							break;
						default: //0 or else to right
							line = pt->do_line_index( pt->line_head, pt->cursor_y );
							clear_in_line(pt, line, pt->cursor_x, (pt->lx - pt->cursor_x), pt->clr_attr, pt->lx);
							term_clear_lines(pt, (pt->cursor_y + 1), (pt->ly - pt->cursor_y - 1), pt->clr_attr);
					}}
					pt->bEscape = FALSE;
					return wsz;
				case L'X':	//clear chars in line
					{ unsigned lx= (d_param[0]? d_param[0]: 1); 
					unsigned line= pt->do_line_index( pt->line_head, pt->cursor_y );
					clear_in_line(pt, line, pt->cursor_x, lx, pt->clr_attr, pt->lx); }
					pt->bEscape = FALSE;
					return wsz;

				case L'u':	//set attrs in line
					{ unsigned lx= (d_param[0]? d_param[0]: 1); 
					unsigned action= (d_param[1]>=3? 3: d_param[1]);
					unsigned line= pt->do_line_index( pt->line_head, pt->cursor_y );
					set_attr_in_line(pt, line, pt->cursor_x, lx, action, pt->clr_attr, pt->lx); }
					pt->bEscape = FALSE;
					return wsz;
				case L'U':	//set attrs for lines
					{unsigned n= (d_param[0]? d_param[0]: 1);
					unsigned action= (d_param[1]>=3? 3: d_param[1]);
					term_set_attr_lines(pt, pt->cursor_y, n, action, pt->clr_attr); }
					pt->bEscape = FALSE;
					return wsz;

				case L'H':	//set cursor pos
					//(is_d_explicit && !d_param) keep old data
					if( !is_d_explicit[0] )pt->cursor_y= 0;
					else if( d_param[0] && d_param[0] - 1 < pt->ly )pt->cursor_y= d_param[0] - 1;
					//
					if( !is_d_explicit[1] )pt->cursor_x= 0;
					else if( d_param[1] && d_param[1] - 1 < pt->lx )pt->cursor_x= d_param[1] - 1;
					pt->bEscape = FALSE;
					return wsz;
				case L'A':	//cursor up
					for( unsigned i= (is_d_explicit[0]? d_param[0]: 1); i && pt->cursor_y; --i )
						--pt->cursor_y; 
					pt->bEscape = FALSE; 
					return wsz;
				case L'B':	//cursor dn
					for( unsigned i= (is_d_explicit[0]? d_param[0]: 1); i && pt->cursor_y + 1 < pt->ly; --i )
						++pt->cursor_y; 
					pt->bEscape = FALSE; 
					return wsz;
				case L'C':	//cursor rt
					for( unsigned i= (is_d_explicit[0]? d_param[0]: 1); i && pt->cursor_x + 1 < pt->lx; --i )
						++pt->cursor_x; 
					pt->bEscape = FALSE; 
					return wsz;
				case L'D':	//cursor lt
					for( unsigned i= (is_d_explicit[0]? d_param[0]: 1); i && pt->cursor_x; --i )
						--pt->cursor_x; 
					pt->bEscape = FALSE; 
					return wsz;
				case L'Z':	//back tab, cursor motion
					if( pt->cursor_x < pt->tab_sz )pt->cursor_x = 0;
					else { 
						unsigned delta_ts = ((pt->cursor_x + 1) % pt->tab_sz);
						if(!delta_ts) delta_ts= pt->tab_sz;
						pt->cursor_x -= delta_ts;
					}
					pt->bEscape = FALSE;
					return wsz;

				case L'@':	//insert char
					if( pt->cursor_y < pt->ly && pt->cursor_x < pt->lx ){
						line = pt->do_line_index( pt->line_head, pt->cursor_y );
						insert_in_line(pt, line, pt->cursor_x, (is_d_explicit[0]? d_param[0]: 1), pt->clr_attr, pt->lx, 0);
					}
					pt->bEscape = FALSE;
					return wsz;
				case L'P':	//delete char
					if( pt->cursor_y < pt->ly && pt->cursor_x < pt->lx ){
						line = pt->do_line_index( pt->line_head, pt->cursor_y );
						delete_in_line(pt, line, pt->cursor_x, (is_d_explicit[0]? d_param[0]: 1), pt->clr_attr, pt->lx);
					}
					pt->bEscape = FALSE;
					return wsz;
				case L'L':	//insert line
					if( pt->cursor_y < pt->ly ){
						term_insert_lines(pt, pt->cursor_y, (is_d_explicit[0]? d_param[0]: 1));
					}
					pt->bEscape = FALSE;
					return wsz;
				case L'M':	//delete line
					if( pt->cursor_y < pt->ly ){
						term_delete_lines(pt, pt->cursor_y, (is_d_explicit[0]? d_param[0]: 1));
					}
					pt->bEscape = FALSE;
					return wsz;
				case L'S':	//scroll lines forward
					{ unsigned ly= (d_param[0]? d_param[0]: 1); 
					if(ly > pt->ly)ly= pt->ly;
					term_delete_lines(pt,0,ly); }
					pt->bEscape = FALSE;
					return wsz;
				case L'T':	//scroll lines backward
					{ unsigned ly= (d_param[0]? d_param[0]: 1); 
					if(ly > pt->ly)ly= pt->ly;
					term_insert_lines(pt,0,ly); }
					pt->bEscape = FALSE;
					return wsz;

				case L'h':	//set some modes
					pt->bEscape = FALSE;
					switch(d_param[0]){
					case 4:							//smir :im:
						pt->bInsert = TRUE;
						term_title_header(pt);
						return wsz;
					case 5:                         //:As:
						pt->bAttrib = TRUE;			
						term_title_header(pt);
						return wsz;
					case 7:                         //smam :SA:
						pt->bWrap = TRUE;			
						term_title_header(pt);
						return wsz;
					case 47:						//smcup :ti:
						//save screen, cursor pos and attr
						term_save_display(pt);
						return wsz;
					};
					//wrong mode, stop seq
					break;
				case L'l':	//reset some modes
					pt->bEscape = FALSE;
					switch(d_param[0]){
					case 4:							//rmir :ei:
						pt->bInsert = FALSE;
						term_title_header(pt);
						return wsz;
					case 5:                         //:Ae:
						pt->bAttrib = FALSE;			
						term_title_header(pt);
						return wsz;
					case 7:                         //rmam :RA:
						pt->bWrap = FALSE;			
						term_title_header(pt);
						return wsz;
					case 47:						//rmcup :te:
						//restore screen, cursor pos and attr
						term_restore_display(pt);
						return wsz;
					};
					//wrong mode, stop seq
					break;
				case L'n':
					pt->bEscape = FALSE;
					switch(d_param[0]){
					case 0:							//:is: partial terminal reset (manual set of tuning options are not affected)
						term_Reset(pt);
						return wsz;
					case 6:{						//:u7: request cursor pos, answer is :u6=\E[y;xR:
						enum { WBUF_SZ= 32 };
						WCHAR  wbuf[WBUF_SZ+2]; wbuf[WBUF_SZ]= 0;
						int wlen = _snwprintf(wbuf, WBUF_SZ, L"\033[%u;%uR", pt->cursor_y + 1, pt->cursor_x + 1);
						if(wlen > 0)term_SendW(pt,wbuf,wlen); 
						}return wsz;
					case 7:							//:sc: save cursor
						pt->cursor_saved_y = pt->cursor_y;
						pt->cursor_saved_x = pt->cursor_x;
						return wsz;
					case 8:							//:rc: restore cursor
						pt->cursor_y = pt->cursor_saved_y;
						pt->cursor_x = pt->cursor_saved_x;
						return wsz;
					case 9: {						//:wi: set active window size
						unsigned 
							y1= (d_param[1]? d_param[1]: 1),
							y2= (d_param[2]? d_param[2]: 1),
							x1= (d_param[3]? d_param[3]: 1),
							x2= (d_param[4]? d_param[4]: 1);
							
							if(    ( y1 >= pt->screen_ly || y2 >= pt->screen_ly || y1 > y2 )
								|| ( x1 >= pt->screen_lx || x2 >= pt->screen_lx || x1 > x2 )
								)break;
							
							pt->top_y= y1 - 1;
							pt->top_x= x1 - 1;
							pt->ly = 1 + y2 - y1;
							pt->lx = 1 + x2 - x1;
							
							pt->cursor_x = pt->cursor_y = 0;
						}return wsz;
					case 100:						//:M0:
						pt->mouse_mode = TERM::MOUSE_DIS;
						term_title_header(pt);
						return wsz;
					case 101:						//:M1:
						pt->mouse_mode = TERM::MOUSE_MODE_1;
						term_title_header(pt);
						return wsz;
					case 102:						//:M2:
						pt->mouse_mode = TERM::MOUSE_MODE_2;
						term_title_header(pt);
						return wsz;
					case 103:						//:M3:
						pt->mouse_mode = TERM::MOUSE_MODE_3;
						term_title_header(pt);
						return wsz;
					case 104:						//:M4:
						pt->mouse_mode = TERM::MOUSE_MODE_4;
						term_title_header(pt);
						return wsz;
					}
					//wrong mode, stop seq
					break;
				case L'g':
					pt->bEscape = FALSE;
					switch(d_param[0]){
					case 3:	//clear to default tabstop
						pt->tab_sz = TERM::DEF_TAB_SZ;
						return wsz;
					}
					//wrong mode, stop seq
					break;
					
				case L'm':	//set attr 
					if( !is_d_explicit[0] ){
						pt->clr_attr= TERM::DEF_CLR_ATTR;
					}else 
					for( unsigned i=0; is_d_explicit[i] && (i < MAX_D_PARAMS); ++i){
						switch(d_param[i]){
						case 0: pt->clr_attr= TERM::DEF_CLR_ATTR; break;																		//reset
						case 1:	pt->clr_attr= TERM::DEF_CLR_ATTR | 0x008U; break;																//fg bright
						 //emphased by color (not VGA)
						 case 2:	pt->clr_attr= (TERM::DEF_CLR_ATTR & 0x0F8U) | 4; break;
						 case 3:	pt->clr_attr= (TERM::DEF_CLR_ATTR & 0x0F8U) | 2; break;
						 case 4:	pt->clr_attr= (TERM::DEF_CLR_ATTR & 0x0F8U) | 6; break;
						 case 5:	pt->clr_attr= (TERM::DEF_CLR_ATTR & 0x0F8U) | 3; break;
						case 7:	pt->clr_attr= ((TERM::DEF_CLR_ATTR >> 4) & 0x0FU) | ((TERM::DEF_CLR_ATTR << 4) & 0x0F0U); break;				//reverse
						case 39: pt->clr_attr= (pt->clr_attr & 0x0F0U) | ( TERM::DEF_CLR_ATTR & 0x00FU); break;									//default fg
						case 49: pt->clr_attr= (pt->clr_attr & 0x00FU) | ( TERM::DEF_CLR_ATTR & 0x0F0U); break;									//default bg
						case 38:																												//fg by index
							if( i + 1 < MAX_D_PARAMS ){ ++i; if( d_param[i] == 5 ){
							if( i + 1 < MAX_D_PARAMS ){ ++i; pt->clr_attr= (pt->clr_attr & 0x0F0U) | (d_param[i] & 0x00FU); }}
							}break;
						case 48:																												//bg by index
							if( i + 1 < MAX_D_PARAMS ){ ++i; if( d_param[i] == 5 ){
							if( i + 1 < MAX_D_PARAMS ){ ++i; pt->clr_attr= (pt->clr_attr & 0x00FU) | ((d_param[i] << 4) & 0x0F0U); }}
							}break;
						default:
							if( d_param[i] >= 30  && d_param[i] <= 37  ){ pt->clr_attr= (pt->clr_attr & 0x0F0U) | (d_param[i] - 30); break; }
							if( d_param[i] >= 90  && d_param[i] <= 97  ){ pt->clr_attr= (pt->clr_attr & 0x0F0U) | (d_param[i] - 90 + 0x08U); break; }
							if( d_param[i] >= 40  && d_param[i] <= 47  ){ pt->clr_attr= (pt->clr_attr & 0x00FU) | ((d_param[i] - 40) << 4); break; }
							if( d_param[i] >= 100 && d_param[i] <= 107 ){ pt->clr_attr= (pt->clr_attr & 0x00FU) | ((d_param[i] - 100 + 0x80U) << 4); break; }
							pt->clr_attr= TERM::DEF_CLR_ATTR;
						}
					}
					pt->bEscape = FALSE;
					return wsz;
				default:
					//wrong char, stop seq
					pt->bEscape = FALSE;
			}
			//case L'['
			break;

		case L'M':	//RI, :sr: move cursor up or insert 1 empty line if was at top of screen
			if( pt->cursor_y ) --pt->cursor_y;
			else term_insert_lines(pt, 0, 1);
			pt->bEscape = FALSE;
			return wsz;
		case L'S':	//XOFF2 :XO: terminal will discard keyb and mouse events
			pt->bEscape = FALSE;
			if(!pt->bOptXOFF2)break;
			pt->bKeyb= FALSE;
			term_title_header(pt);
			return wsz;
		case L'H':	//:st: hts, set all tabstops gap by current cursor_x
			if(pt->bProgTab){ pt->tab_sz = pt->eval_tab_sz(); }
			pt->bEscape = FALSE;
			return wsz;
		case L'F':	//:ll: cursor last line
			pt->cursor_y = pt->ly - 1;
			pt->cursor_x = 0;
			pt->bEscape = FALSE;
			return wsz;
			
		default:
			//wrong char, stop seq
			pt->bEscape = FALSE;
		//switch(pt->escape_wcode[1] )
		}

	//while ( wsz < wzz && pt->bEscape ) 
	}

	//print unknown esc seq from pt->escape_wcode[] as ordinary chars
	if(!pt->bEscape){
		pt->escape_wcode[ pt->escape_filled ]= 0; //to ensure EOL

		//write to file delayed in escape_wcode buf
		if( pt->bLogEsc ){ 
			char buf[TERM::SEQ_SZ+2], *p= buf; memset(buf,0,sizeof(buf));
			//store low byte of UTF16LE
			for( WCHAR *wp= pt->escape_wcode; *wp; ++p, ++wp ){ *p = char(*wp); }
			assert(pt->fpLogEscFile);
			fwrite(buf, 1, pt->escape_filled, pt->fpLogEscFile);
			fflush(pt->fpLogEscFile);
		}

		//print out delayed in escape_wcode buf
		if( !pt->bOptHideEsc ){
		for( WCHAR *wp= pt->escape_wcode; *wp; ++wp){ term_tty_wchar( pt, *wp); }
		}
	} 
	return wsz;
}
