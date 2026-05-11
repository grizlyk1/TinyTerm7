//
// tinyTerm7 -- A minimal Windows terminal emulator

#include "stdafx.h"
#include "tinyterm.h"
#include "keytable.h"

//WS_TILEDWINDOW  | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CLIPCHILDREN
#define TERM_WS_STYLE (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VSCROLL)
//define to use LAYERED window style
//#define IS_LAYERED

//define to display more debug messages
#define DBG_BOX

//pty stub
HANDLE hPTY_stdin, hCH_stdin, hPTY_stdout, hCH_stdout;
int wfPTY_stdin, rfPTY_stdout;

HHOOK hhkLowLevelKybd;

#if 0
cmdline [-d] [command]
	-d	disable load config file on startup
#endif
static LPWSTR app_lpCmdLine;
static WCHAR def_lpCmdLine[]= L"";

#define CFG_FNAME			(L"TinyTerm7.cfg")
#define CFG_PATH_ENV		(L"USERPROFILE")
static WCHAR *cfg_fname= 0;
enum { CFG_STR_SZ=32 }; //CFG file string field maxsize

//if have no round
#if 0
static double 
	round(double d){

	double sf = 1;
	if(d<0){ d= -d; sf = -1; }
	return sf*floor( d + 0.5 );
}
#endif

#define count_fontHeight(font_sz)  ( -int( round((double(font_sz)*dpi)/72.0))  )
#define count_fontSize(fontHeight) ( -int( round((double(fontHeight)*72)/dpi)) )
int dpi = 96;
int fontSize = 16;
WCHAR fontFace[CFG_STR_SZ] = L"Consolas";
int fontStyle_Weight  = FW_NORMAL;
BOOL bfontStyle_italic  = FALSE;

int titleHeight;
enum { TITLE_BUF_SZ = 256 };
enum { TITLE_BUF_PROMPT_SZ = 16 }; 
enum { TITLE_BUF_STATUS_SZ = 200 };
WCHAR wndTitle[TITLE_BUF_SZ] = L"TinyTerm7";

//RGB(32,96,240)
const COLORREF COLORS[16+8] = {
	//norm
	RGB(0,0,0),			RGB(160,0,0),	RGB(0,160,0),	RGB(160,160,0),
	RGB(30,30,160),		RGB(160,0,160),	RGB(0,160,160),	RGB(160,160,160),
	//high(bold)
	RGB(120,120,120), 	RGB(240,0,0),	RGB(0,240,0),	RGB(240,240,0), 
	RGB(60,60,240),		RGB(240,0,240), RGB(0,240,240), RGB(240,240,240),
	//dim() 
	RGB(0,0,0),			RGB(120,0,0),	RGB(0,120,0),	RGB(120,120,0),
	RGB(30,30,120),		RGB(120,0,120), RGB(0,120,120), RGB(120,120,120)
};
static HINSTANCE hInst;
static HBRUSH dwBkBrush;
HWND hwndTerm;
RECT termRect;

static RECT wndRect;
static HFONT hTermFont;

enum { TTERM=0, Menu_SZ };
static HMENU hMainMenu, hMenu[Menu_SZ];
int menuX[Menu_SZ];
enum { CONTEX = TTERM };

TERM *pt;

static int iFontHeight, iFontWidth;
#ifdef IS_LAYERED
static int iTransparency = 255;
#endif

static BOOL bFocus=TRUE, bScrollbar=TRUE;

//prevent muliple HideCaret(hwndTerm), that hangs Caret
static BOOL is_HideCaret_done= 0;

//if wcnt == -1 then wbuf is \0-terminating wstring
//if cnt == 0 then return minimum buf size, buf is not used
//return number of bytes written to buf (including last \0 for wcnt == -1) or 0 if error
int wchar_to_utf8(WCHAR *wbuf, int wcnt, char *buf, int cnt)
{
	assert(wbuf);
	if(!wcnt)return 0;
	if(cnt)assert(buf);
	return WideCharToMultiByte(CP_UTF8, 0, wbuf, wcnt, buf, cnt, NULL, NULL);
}
//if cnt == -1 then buf is \0-terminating string
//if wcnt == 0 then return minimum wbuf size, wbuf is not used
//return number of bytes written to wbuf (including last \0 for cnt == -1) or 0 if error
int utf8_to_wchar(const char *buf, int cnt, WCHAR *wbuf, int wcnt)
{
	assert(buf);
	if(!cnt)return 0;
	if(wcnt)assert(wbuf);
	return MultiByteToWideChar(CP_UTF8, 0, buf, cnt, wbuf, wcnt);
}
FILE * fopen_utf8(const char *fn, const char *mode)
{
	assert(fn);
	assert(mode);
	WCHAR wfn[MAX_PATH], wmode[4];
	utf8_to_wchar(fn, strlen(fn)+1, wfn, MAX_PATH);
	utf8_to_wchar(mode, strlen(mode)+1, wmode, 4);
	return _wfopen(wfn, wmode);
}
int stat_utf8(const char *fn, struct _stat *buffer)
{
	assert(fn);
	assert(buffer);
	WCHAR wfn[MAX_PATH];
	utf8_to_wchar(fn, strlen(fn)+1, wfn, MAX_PATH);
	return _wstat(wfn, buffer);
}

WCHAR *fileDialog( HWND hwnd, WCHAR *szFilter, WCHAR *szDefExt, DWORD dwFlags )
{
	static WCHAR wname[MAX_PATH];
	BOOL ret = FALSE;
	OPENFILENAME ofn;

	wname[0]=0;
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFile = wname;
	ofn.nMaxFile = MAX_PATH-1;
	ofn.lpstrFilter = szFilter; // L"*.log" or L"*.*"
	ofn.lpstrDefExt = szDefExt; // L"log" without a dot
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = dwFlags | OFN_NOCHANGEDIR;
	ret = GetSaveFileName(&ofn);
	if ( ret ) {
		char fn[MAX_PATH];
		wchar_to_utf8(wname, wcslen(wname)+1, fn, MAX_PATH);
		FILE *fi= fopen(fn,"rb"); if( fi ){
			fclose(fi);
			if( MessageBoxA(hwnd,"log File exist, Append to existed log?","Confirmation",MB_YESNOCANCEL|MB_ICONQUESTION)
				!= IDYES) return NULL;
	}}
	//if ( dwFlags&OFN_OVERWRITEPROMPT ){
	//else
	//	ret = GetOpenFileName(&ofn);
	return ret ? wname : NULL;
}
char *getFolderName(WCHAR *wtitle)
{
	static BROWSEINFO bi;
	static char szFolder[MAX_PATH];
	static WCHAR wfolder[MAX_PATH];
	WCHAR szDispName[MAX_PATH];
	LPITEMIDLIST pidl;

	memset(&bi, 0, sizeof(BROWSEINFO));
	bi.hwndOwner = 0;
	bi.pidlRoot = NULL;
	bi.pszDisplayName = szDispName;
	bi.lpszTitle = wtitle;
	bi.ulFlags = BIF_RETURNONLYFSDIRS;
	bi.lpfn = NULL;
	bi.lParam = 0;
	pidl = SHBrowseForFolder(&bi);
	if ( pidl != NULL )
		if ( SHGetPathFromIDList(pidl, wfolder) )
		{
			wchar_to_utf8(wfolder, -1, szFolder, MAX_PATH);
			return szFolder;
		}
	return NULL;
}
BOOL tiny_fontDialog()
{
	LOGFONT lf;
	CHOOSEFONT cf;
	ZeroMemory(&cf, sizeof(cf));
	cf.lStructSize = sizeof (cf);
	cf.hwndOwner = hwndTerm;
	cf.lpLogFont = &lf;
	cf.Flags = CF_SCREENFONTS|CF_FIXEDPITCHONLY|CF_INITTOLOGFONTSTRUCT;
	if ( GetObject(hTermFont, sizeof(lf), &lf)==0 ) ZeroMemory(&lf, sizeof(lf));

	if ( !ChooseFont(&cf) ) return FALSE;
	
	DeleteObject(hTermFont);
	hTermFont = CreateFontIndirect(&lf);
	//SendMessage( hwndCmd, WM_SETFONT, (WPARAM)hTermFont, TRUE );

	//fontSize = lf.lfHeight; if ( fontSize<0 ) fontSize = -fontSize;
	fontSize= count_fontSize(lf.lfHeight);
	_snwprintf(fontFace, CFG_STR_SZ-1, L"%s", lf.lfFaceName); fontFace[CFG_STR_SZ-1]= 0;
	fontStyle_Weight = lf.lfWeight;
	bfontStyle_italic = lf.lfItalic? TRUE: FALSE;
	return TRUE;
}
//hwnd must be hwndTerm, returned hTermFont
void
	new_font(HWND hwnd){

	if(hTermFont)DeleteObject(hTermFont);

	//ount_fontHeight(16) == -21 
	hTermFont = CreateFont( count_fontHeight(fontSize), 0, 0, 0, 
						fontStyle_Weight, bfontStyle_italic, FALSE, FALSE, //FW_REGULAR, FALSE
						DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
						DEFAULT_QUALITY, FIXED_PITCH, fontFace);
	if (!hTermFont){ MessageBoxA(hwnd,"Can not CreateFont","Error",MB_OK|MB_ICONERROR); exit(-1); }
}

void menu_Check(DWORD id, BOOL op)
{
	CheckMenuItem(hMainMenu, id, MF_BYCOMMAND|(op?MF_CHECKED:MF_UNCHECKED));
}

//WM_TIMER process pending events with low rate
static volatile BOOL is_esc_pending= FALSE;
static unsigned	esc_pending_mod= KEY_MOD_PURE;

static volatile BOOL is_sysrq_pending= FALSE;
static unsigned	sysrq_pending_mod= KEY_MOD_PURE; 

static BOOL is_redraw_pending= FALSE;
static BOOL is_paint_in_progress= FALSE;

static BOOL is_NC_redraw_pending= FALSE;

//
void request_tiny_redraw( BOOL is_add_whole_screeen ){
	if(is_add_whole_screeen)GetClientRect(hwndTerm, &termRect);
	is_redraw_pending = TRUE;
}
void request_tiny_NC_redraw(){ is_NC_redraw_pending= TRUE; }

void tiny_Title()
{
	pt->title[pt->title_idx]= 0;
	SetWindowTextW(hwndTerm, pt->title);
}
//
void tiny_Scroll( unsigned scroll_pos, unsigned scroll_range, unsigned scroll_page, BOOL is_redraw )
{
	UNREFERENCED_PARAMETER(is_redraw);
	SCROLLINFO	si;
	
	sizeof (si);
	si.fMask = SIF_ALL;
	si.nPos = scroll_pos;
	si.nMin = 0;
	si.nMax = scroll_range;
	si.nPage = scroll_page;
	SetScrollInfo (hwndTerm, SB_VERT, &si, TRUE);

	if(is_redraw) request_tiny_redraw(NEW_PAINT_RECT);
}
//wnd_Size: adjust window size when fontface/fontsize or size_x/size_y changed
//set tiny window size by term size or fontsize 
//hwnd should be hwndTerm
void tiny_wnd_Size()
{
	HDC hdc;
	TEXTMETRIC tm;
	hdc = GetDC(hwndTerm);
	SelectObject(hdc, hTermFont);
	GetTextMetrics(hdc, &tm);
	ReleaseDC(hwndTerm, hdc);
	iFontHeight = tm.tmHeight;
	iFontWidth = tm.tmAveCharWidth;
	GetWindowRect( hwndTerm, &wndRect );
	int x = wndRect.left;
	int y = wndRect.top;
	wndRect.right = x + iFontWidth*pt->lx;
	wndRect.bottom = y + iFontHeight*pt->ly;
	wndRect.right += GetSystemMetrics(SM_CXVSCROLL); //hwndTerm has WS_VSCROLL attr
	AdjustWindowRect(&wndRect, TERM_WS_STYLE, FALSE); 
	MoveWindow( hwndTerm, x, y, wndRect.right-wndRect.left,
							wndRect.bottom-wndRect.top, TRUE );

	if ( IsWindowVisible(hwndTerm) ) {	//change term size only when visible
			//InvalidateRect(hwndTerm, NULL, FALSE);
			request_tiny_redraw(NEW_PAINT_RECT);
	}
}
void tiny_show_cursor(){
	//offs >= 0, cursor_y count by base pt->line_head, but displayed by base pt->line_scroll
	unsigned cursor_y_offs = pt->do_line_diff(pt->line_head, pt->line_scroll);

	//show cursor, if no scroll pt->scroll == pt->head 
	//if( (pt->head - pt->scroll) < pt->cursor_y ) in wrapped memory
	if( !pt->bCursor 
		|| !bFocus 
		//|| ( pt->scroll != pt->head ) 
		|| ( pt->cursor_y + cursor_y_offs >= pt->ly )
		){
		if( !is_HideCaret_done ){ HideCaret(hwndTerm); is_HideCaret_done= 1; }

	}else{
		SetCaretPos( ( pt->top_x + pt->cursor_x )*iFontWidth,
					 ( pt->top_y + pt->cursor_y + cursor_y_offs )*iFontHeight + iFontHeight*3/4);
		ShowCaret(hwndTerm); is_HideCaret_done= 0;
	}
}

//0-31, 127 for consolas
static 
WCHAR remapped_printable_wc[33] = {
		0x2736U, 0x263AU, 0x263BU, 0x2665U, 0x2666U, 0x2663U, 0x2660U, 0x2022U, //0-31
		0x25D8U, 0x25CBU, 0x25D9U, 0x2642U, 0x2640U, 0x266AU, 0x266BU, 0x263CU,
		0x25BAU, 0x25C4U, 0x2195U, 0x203CU, 0x00B6U, 0x00A7U, 0x25ACU, 0x21A8U,
		0x2191U, 0x2193U, 0x2192U, 0x2190U, 0x221FU, 0x2194U, 0x25B2U, 0x25BCU,
		0x2302U //127
};

void tiny_Paint(HDC hDC, RECT rcPaint)
{
	unsigned dx, dy=rcPaint.top; 

	SelectObject(hDC, hTermFont);
	
	//invalidated visual rectangle to repaint
	unsigned y_top= rcPaint.top/iFontHeight; //from y line on screen
	unsigned x_left= rcPaint.left/iFontWidth; // from x pos on screen
	if( y_top >= pt->screen_ly ) y_top= pt->screen_ly-1;
	if( x_left >= pt->screen_lx ) x_left= pt->screen_lx-1;
	
	unsigned ly_bottom= 1 + rcPaint.bottom/iFontHeight; // ly lines on screen
	unsigned lx_right= 1 + rcPaint.right/iFontWidth; // lx items in line on screen
	if( ly_bottom > pt->screen_ly ) ly_bottom= pt->screen_ly;
	if( lx_right > pt->screen_lx ) lx_right= pt->screen_lx;

	//
	struct t_sel{
		unsigned line;
		unsigned x;
	} ;
	t_sel sel[2];

	if( pt->bSel ){
		//sel_line_m1 < sel_line_m2
		sel[0]= {pt->sel_line_m1, pt->sel_x_m1}, sel[1]= { pt->sel_line_m2, pt->sel_x_m2};

		if( pt->sel_line_m1 == pt->sel_line_m2 ) {
			if( sel[1].x < sel[0].x ){ 
				unsigned tmp = sel[0].x; sel[0].x = sel[1].x; sel[1].x = tmp; 
			}
		//'do_is_line_less' sort 'sel_line_mX' in wrapped range [line_tail, line_head+ly)
		//}else if( pt->do_is_line_less(sel_line_m2, sel_line_m1) ){
		}else if( pt->do_line_diff(pt->sel_line_m2, pt->sel_line_m1) < 0 ){
			//sel_line_m1 > sel_line_m2
			t_sel tmp = sel[0]; sel[0] = sel[1]; sel[1] = tmp; 
	}}

	//internal paint loop vars
	//unsigned line = pt->line_scroll; //display visual buf (from base line_scroll)
	unsigned y_paint= 0;
	BOOL is_paint_lines= FALSE;
	BOOL initial_is_line_in_sel_range= FALSE;

	/*
		if selection active, how to diaplay the selection area in wrapped in memory buffer 
		always enum line by line in memory buf all the lines in range [line_tail, line_head + ly ) 
		and do paint only the lines in range [line_scroll, line_scroll + ly )
		and break loop in line_scroll + ly
			that means 
			if (pt->bSel) then { enum extra lines in range [line_tail, line_scroll) in order to keep track of selection state }
			else { only paint the visual lines in range [line_scroll, line_scroll + ly ) }
	*/
	for( 
		unsigned line_enum= (pt->bSel? pt->line_tail: pt->line_scroll), 
		line_visual_limit = pt->do_line_index(pt->line_scroll, pt->screen_ly); 
			line_enum != line_visual_limit; 
				line_enum = pt->do_line_add1(line_enum) 
		){

	//trace enum_line meet selection margine
	//this is not depend from paint region
	BOOL is_line_in_sel_range= initial_is_line_in_sel_range;
	//current line selection margine, default whole line selected
	unsigned sel_x1= 0, sel_x2 = pt->screen_lx-1;
		
	//fill selection margins struct
	if( pt->bSel ){
	if( line_enum == sel[0].line ){
		//find first
		is_line_in_sel_range= TRUE;
		//goto middle selection
		initial_is_line_in_sel_range= TRUE;
		//fill margins struct
		sel_x1= sel[0].x; 
	}
	//both sel[?] conditions checked together with intention
	if( line_enum == sel[1].line ){
		//find last
		is_line_in_sel_range= TRUE;
		//goto selection off
		initial_is_line_in_sel_range= FALSE;
		//fill margins struct
		sel_x2= sel[1].x;
	}}

	//end of extra lines range, can paint 
	if( line_enum == pt->line_scroll)is_paint_lines= TRUE;
	
	//for ( unsigned paint_line= y_top; paint_line < ly_bottom; ++paint_line )
	if( is_paint_lines ){
	if( y_paint >= y_top && y_paint < ly_bottom )
	{
		dx = 0;
		//unsigned line = pt->line_scroll + line; //display visual buf (from base line_scroll)
		TERM::tt7_data_t *p_data = pt->data_buf + line_enum*pt->max_lx();
		TERM::tt7_attr_t *p_attr = pt->attr_buf + line_enum*pt->max_lx();
		//line = pt->do_line_add1(line);

		//enum x 
		//j gathering the same attrs in single line
		//for( unsigned x= 0, j=0; x < pt->lx; ){
		//for( unsigned x= 0, j=0; x < lx_right; ){
		for( unsigned x= x_left, j=x_left; x < lx_right; ){

			//outside of paint rect
			//if(x < x_left || x >= lx_right ){ ++x; ++j; continue; }
			//if(x < x_left ){ ++x; ++j; continue; }

			//while x does not meet ctrl chars 0-31, 127
			//to improve visible width the codes printed char by char
			if( unsigned(p_data[x]) < 32 || p_data[x] == 127 ){
				++j;
			//invalidated LX part, need to repaint
			}else if( p_attr[j] == p_attr[x] ){
				++j;
				if( j < pt->screen_lx ) {
					//continue gathering x by the same attr 
					//while j does not meet ctrl chars 0-31, 127
					//to improve visible width the codes printed char by char
					if( unsigned(p_data[j]) < 32 || p_data[j] == 127 ){
						goto break_if_gathering;
					}
					//while j does not meet selection margin
					if( !is_line_in_sel_range ) continue;
					//[x,lx)
					if( j != sel_x1 && j != sel_x2 + 1 ) continue;
				break_if_gathering:;
				}
			}

			//xchange color and bk_color while x inside sel_range
			if( is_line_in_sel_range
				&& (x >= sel_x1 && x <= sel_x2)
				){
				//selected text
				SetTextColor(hDC, COLORS[(p_attr[x]>>4)&0x0f]);
				SetBkColor(hDC, COLORS[(p_attr[x])&0x0f]);
			}else{
				//normal text
				SetTextColor(hDC, COLORS[(p_attr[x])&0x0f]);
				SetBkColor(hDC, COLORS[(p_attr[x]>>4)&0x0f]);
			}

			unsigned wlen = j-x;
			//remap printable view of ctrl chars 0-31, 127
			if( wlen == 1 && unsigned(p_data[x]) < 32 || p_data[x] == 127 ){
				#if 0
				WCHAR wc[2]= { p_data[x], 0 };
				switch(p_data[x]){
					case 0:   wc[0]= 0xC0U; break;
					case 127: wc[0]= 0xC1U; break;
					default:  wc[0]= p_data[x] + 0xA0U;
				}
				#endif
				WCHAR wc[2]= { (p_data[x] == 127? remapped_printable_wc[32]: remapped_printable_wc[p_data[x]]), 0};
				TextOutW(hDC, dx, dy, wc, 1);
			//no remap ctrl
			}else{
				//option, display as UTF8
				//if( pt->bOptDisplayUTF8 )
				if( pt->OptDisplay_mode == TERM::Display_UTF8 )
					TextOutW(hDC, dx, dy, p_data+x, j);
				
				//option, display as plain 8 bit chars by codepage installed in Windows
				else{
					char *tmp_buf = static_cast<char*>(_alloca( (2*j + 2)*sizeof(char) ));
					//char def_char= 0x1A; //SUB
					//WideCharToMultiByte(?, 0, p_data+x,j, tmp_buf, 2*j, &def_char, 0);
					for( unsigned i= 0; i<j; ++i){ tmp_buf[i] = unsigned((p_data+x)[i]) & 0x0FFU; }

					WCHAR *wbuf = static_cast<WCHAR*>(_alloca( (2*j + 2)*sizeof(WCHAR) ));
					//CP_ACP (1251), CP_OEMCP (866), etc
					unsigned const cp = ( pt->OptDisplay_mode == TERM::Display_OEM? CP_OEMCP: CP_ACP);
					MultiByteToWideChar(cp, 0, tmp_buf, j, wbuf, 2*j);

					TextOutW(hDC, dx, dy, wbuf, j);
				}
			}
			//RECT text_rect = {0, 0, 0, 0};
			//DrawText(hDC, p_data+x, len, &text_rect, DT_CALCRECT|DT_NOPREFIX);
			//dx += text_rect.right;
			dx += wlen*iFontWidth;

			x=j;
		}

		dy += iFontHeight;
	}

	++y_paint;
	//is_paint_lines
	}
	
	//for enum_line
	}

	//
	//is_redraw_pending = FALSE;
	is_paint_in_progress= FALSE;
}

INT_PTR CALLBACK DAbout(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (msg)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

//
typedef struct tagDSize_Item{
	WCHAR *wstr; //L"80x25"
	unsigned lx; //80
	unsigned ly; //25
} t_DSize_Item;

t_DSize_Item
	DSize_Items[] = {
		{ L"80x25" , 80, 25 },
		{ L"120x30" , 120, 30 }
	};
enum { DSize_Item_SZ= sizeof(DSize_Items)/sizeof(t_DSize_Item) };

INT_PTR CALLBACK DSize(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	int lx, ly;
	BOOL res_lx, res_ly;

	//prev and correct selected lx,ly values; but still not stored into term
	//static and must be init in WM_INITDIALOG or in CBN_SELENDOK to keep data between callbacks
	static int def_lx;
	static int def_ly;

	UNREFERENCED_PARAMETER(lParam);
	switch (msg)
	{
	case WM_INITDIALOG:
		//FindWindowEx(hDlg,0,);
		def_lx= pt->lx; def_ly= pt->ly;
		SetDlgItemInt(hDlg,IDM_LX,pt->lx,FALSE);
		SetDlgItemInt(hDlg,IDM_LY,pt->ly,FALSE);

		SendDlgItemMessage(hDlg,IDM_LXY,CB_ADDSTRING,0,(LPARAM)DSize_Items[0].wstr);
		SendDlgItemMessage(hDlg,IDM_LXY,CB_ADDSTRING,0,(LPARAM)DSize_Items[1].wstr);
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		wmId = LOWORD(wParam);
		wmEvent = HIWORD(wParam);

		switch( wmId ){
			case IDM_LXY:
				//list item was selected in dropdown menu of IDM_LXY
				if (wmEvent != CBN_SELENDOK) break;
				switch( SendDlgItemMessage(hDlg,IDM_LXY,CB_GETCURSEL,0,0) ){
				case 0:
					def_lx= DSize_Items[0].lx; def_ly= DSize_Items[0].ly;
					break;
				case 1:
					def_lx= DSize_Items[1].lx; def_ly= DSize_Items[1].ly;
					break;
				default:
					return (INT_PTR)FALSE;
				}
				SetDlgItemInt(hDlg,IDM_LX,def_lx,FALSE);
				SetDlgItemInt(hDlg,IDM_LY,def_ly,FALSE);
				break;
			case IDOK:
				lx = GetDlgItemInt(hDlg,IDM_LX,&res_lx,FALSE);
				ly = GetDlgItemInt(hDlg,IDM_LY,&res_ly,FALSE);
				//if invalid return or too small/big size 
				if( !res_lx || lx < 1 ){ SetDlgItemInt(hDlg,IDM_LX,def_lx,FALSE); return (INT_PTR)FALSE; }
				if( !res_ly || ly < 1 ){ SetDlgItemInt(hDlg,IDM_LY,def_ly,FALSE); return (INT_PTR)FALSE; }
				
				term_setSize(pt, lx, ly); 
				tiny_wnd_Size();
				request_tiny_redraw(EXISTED_PAINT_RECT);
				//continue IDCANCEL
			case IDCANCEL:
				EndDialog(hDlg, LOWORD(wParam));
				return (INT_PTR)TRUE;
		}break;
	}
	return (INT_PTR)FALSE;
}

enum{ IS_DEFAULT_LOAD= 0, IS_EXPLICIT_LOAD= 1 };
//is_explicit_load can not fail; default load can fail
//return 0 if nothing to change
static char
	load_cfg(HWND hwnd, char is_explicit_load){
	
		if(!cfg_fname){
			{ MessageBoxA(hwnd,"Can not use config file","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ }
			return 0; 
		}

		FILE *fi= _wfopen(cfg_fname, L"rb"); 
		if(!fi){ 
			if(is_explicit_load){ 
				//MessageBoxW(hwnd,cfg_fname,L"Error",MB_OK|MB_ICONERROR); 
				MessageBoxA(hwnd,"Can not read config file","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ 
				}
			return 0; 
		}

		unsigned lx, ly, is_italic;
		unsigned opt[6];

		//size: lx, ly
		if( fwscanf(fi, L" size: %u %u ;", &lx, &ly) != 2)
			{ MessageBoxA(hwnd,"Can not read size","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ }

		//font: sz, name[CFG_STR_SZ], style_Weight, style_italic
		if( fwscanf(fi, L" font: %u %32s %u %u ;", &fontSize, &fontFace, &fontStyle_Weight, &is_italic) != 4)
			{ MessageBoxA(hwnd,"Can not read font","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ }

		//options: CursorBS, CANbyESC, HideEsc, DisplayUTF8, KeybUTF8, miceXterm
		if( fwscanf(fi, L" options: %u %u %u %u %u %u ;", opt+0, opt+1, opt+2, opt+3, opt+4, opt+5) != 6)
			{ MessageBoxA(hwnd,"Can not read options","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ }
		
		fclose(fi);

		bfontStyle_italic = is_italic;
		new_font(hwnd);
		term_setSize(pt, lx, ly);

		pt->bOptCursorBS=		(opt[0]? TRUE: FALSE);
		pt->bOptESC_do_CAN=		(opt[1]? TRUE: FALSE);
		pt->bOptHideEsc=		(opt[2]? TRUE: FALSE);
		switch(opt[3]){ 
			case TERM::Display_UTF8:
			case TERM::Display_OEM:
			case TERM::Display_ANSI:
				pt->OptDisplay_mode= opt[3];
			default:
				pt->OptDisplay_mode= TERM::Display_ANSI;
		}
		pt->bOptKeybUTF8=		(opt[4]? TRUE: FALSE);
		pt->bOptMouseXterm=		(opt[5]? TRUE: FALSE);

		return 1;
}
static void
	save_cfg(HWND hwnd)
{
		if(!cfg_fname){
			{ MessageBoxA(hwnd,"Can not use config file","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ }
			return; 
		}
					
		FILE *fo= _wfopen(cfg_fname, L"wb"); 
		if(!fo){
			{ MessageBoxA(hwnd,"Can not write config file","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ }
			return; 
		}

		//size: lx, ly
		if( fwprintf(fo,L" size: %u %u ;", pt->lx, pt->ly) <= 0)
			{ MessageBoxA(hwnd,"Can not write size","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ }

		//font: sz, name, style_Weight, style_italic
		if( fwprintf(fo,L" font: %u %s %u %u ;", fontSize, fontFace, fontStyle_Weight, (bfontStyle_italic? 1: 0) ) <= 0)
			{ MessageBoxA(hwnd,"Can not write font","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ }

		//options: CursorBS, CANbyESC, HideEsc, DisplayUTF8, KeybUTF8, miceXterm
		if( fwprintf(fo,L" options: %u %u %u %u %u %u ;", 
				pt->bOptCursorBS, pt->bOptESC_do_CAN, pt->bOptHideEsc, pt->OptDisplay_mode, pt->bOptKeybUTF8, pt->bOptMouseXterm 
				) <= 0)
			{ MessageBoxA(hwnd,"Can not write options","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ }

		fclose(fo);
}

//hwnd is hwndTerm
BOOL menu_Command( HWND hwnd, WPARAM wParam, LPARAM lParam )
{
	int wmId = LOWORD(wParam);
	int wmEvent = HIWORD(wParam);

	switch ( LOWORD(wParam) ) 
	{
	case IDM_LOADCFG:
		if( !load_cfg(hwnd,IS_EXPLICIT_LOAD) )break;
		//InvalidateRect(hwnd, NULL, FALSE);
		tiny_wnd_Size();
		request_tiny_redraw(EXISTED_PAINT_RECT);
		break;
	case IDM_SAVECFG:
		save_cfg(hwnd);
		break;

	case IDM_LOAD_KEYMAP:
		load_keymap(hwnd, tiny_keytable, IS_EXPLICIT_LOAD);
		break;
	case IDM_SAVE_KEYMAP:
		save_keymap(hwnd, tiny_keytable);
		break;

	case IDM_NEWWIN: {
		//run copy of own app
		WCHAR szPath[MAX_PATH];
		GetModuleFileName(0,szPath,MAX_PATH);

		STARTUPINFO si = { sizeof(si) };
		PROCESS_INFORMATION pi;
	
		// Create the new process instance
		if (!CreateProcess(
			szPath,         // Module name (self)
			def_lpCmdLine,  // Command line
			NULL,           // Process handle not inheritable
			NULL,           // Thread handle not inheritable
			FALSE,          // Set handle inheritance to FALSE
			0,              // No creation flags
			NULL,           // Use parent's environment block
			NULL,           // Use parent's starting directory 
			&si,            // Pointer to STARTUPINFO structure
			&pi)            // Pointer to PROCESS_INFORMATION structure
		) {
			{ MessageBoxA(hwnd,"Can not CreateProcess","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ }
			break;
		}
		// Close handles as they are no longer needed by the parent
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		}break;

	case IDM_REPAINT:
		InvalidateRect(hwnd, NULL, FALSE);
		break;

	case IDM_ABOUT:
		DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hwnd, DAbout);
		break;
	case IDM_FONT:
		if ( tiny_fontDialog() ) { tiny_wnd_Size(); request_tiny_redraw(EXISTED_PAINT_RECT); }
		break;
	case IDM_SIZE: //IDOK inside DSize
		DialogBox(hInst, MAKEINTRESOURCE(IDD_SIZE), hwnd, DSize);
		break;

	case IDM_M0: if(pt->mouse_mode != TERM::MOUSE_DIS   ){ pt->mouse_mode = TERM::MOUSE_DIS;    term_title_header(pt); } break;
	case IDM_M1: if(pt->mouse_mode != TERM::MOUSE_MODE_1){ pt->mouse_mode = TERM::MOUSE_MODE_1; term_title_header(pt); } break;
	case IDM_M2: if(pt->mouse_mode != TERM::MOUSE_MODE_2){ pt->mouse_mode = TERM::MOUSE_MODE_2; term_title_header(pt); } break;
	case IDM_M3: if(pt->mouse_mode != TERM::MOUSE_MODE_3){ pt->mouse_mode = TERM::MOUSE_MODE_3; term_title_header(pt); } break;
		//menu_Check( IDM_M0, pt->mouse_mode == TERM::MOUSE_DIS );
		//menu_Check( IDM_M1, pt->mouse_mode == TERM::MOUSE_MODE_1 );
		//menu_Check( IDM_M2, pt->mouse_mode == TERM::MOUSE_MODE_2 );
		//menu_Check( IDM_M3, pt->mouse_mode == TERM::MOUSE_MODE_3 );
	case IDM_MICE_XTERM:
		pt->bOptMouseXterm = ( pt->bOptMouseXterm? FALSE: TRUE );
		//menu_Check( IDM_MICE_XTERM, pt->bOptMouseXterm );
		break;

	case IDM_WRAP:
		pt->bWrap = ( pt->bWrap? FALSE: TRUE );
		term_title_header(pt); 
		//menu_Check( IDM_WRAP, pt->bWrap );
		break;
	case IDM_KEYBXON:
		if( !pt->bKeyb ){ 
			pt->bKeyb= TRUE;
			term_title_header(pt);
		}
		//menu_Check( IDM_KEYBXON, pt->bKeyb );
		break;
	case IDM_RESET_TERM: {
		//term_partial_Reset(pt);
		//can not reparse 
		//WCHAR wbuf[]= L"\033[m\033[4J";
		//term_ParseW(pt, wbuf, wcslen(wbuf));
		term_Reset(pt);
		}break;
	case IDM_KEYB_UTF8: 
		pt->bOptKeybUTF8 = ( pt->bOptKeybUTF8? FALSE: TRUE );
		//menu_Check( IDM_KEYB_UTF8, pt-> );
		break;		
	case IDM_DISP_UTF8: if(pt->OptDisplay_mode != TERM::Display_UTF8 ){ pt->OptDisplay_mode = TERM::Display_UTF8; GetClientRect(hwndTerm, &termRect); request_tiny_redraw(EXISTED_PAINT_RECT); } break;
	case IDM_DISP_OEM:  if(pt->OptDisplay_mode != TERM::Display_OEM  ){ pt->OptDisplay_mode = TERM::Display_OEM;  GetClientRect(hwndTerm, &termRect); request_tiny_redraw(EXISTED_PAINT_RECT); } break;
	case IDM_DISP_ANSI: if(pt->OptDisplay_mode != TERM::Display_ANSI ){ pt->OptDisplay_mode = TERM::Display_ANSI; GetClientRect(hwndTerm, &termRect); request_tiny_redraw(EXISTED_PAINT_RECT); } break;
		//pt->bOptDisplayUTF8 = ( pt->bOptDisplayUTF8? FALSE: TRUE );
		//menu_Check( IDM_DISP_UTF8, pt-> );
	case IDM_CURSOR_BS:
		pt->bOptCursorBS = ( pt->bOptCursorBS? FALSE: TRUE );
		//menu_Check( IDM_CURSOR_BS, pt-> );
		break;	
	case IDM_ESC_DO_CAN:
		pt->bOptESC_do_CAN = ( pt->bOptESC_do_CAN? FALSE: TRUE );
		(tiny_keytable[VK_ESCAPE].vk_str[KEY_MOD_PURE].data())[0] = ( pt->bOptESC_do_CAN? '\030': '\033' );
		//menu_Check( IDM_ESC_DO_CAN, pt->bOptESC_do_CAN);	
		break;
	case IDM_HIDE_ESC:
		pt->bOptHideEsc = ( pt->bOptHideEsc? FALSE: TRUE );
		//menu_Check( IDM_HIDE_ESC, pt-> );
		break;
	case IDM_LOG_ESC:
		if ( pt->bLogEsc ) 
			term_Log_esc( pt, NULL );
		else{
			WCHAR *wfn = fileDialog( hwnd, L"Log files (*.log)\0*.log\0All files (*.*)\0*.*\0\0", L"log",
				//do not use OFN_OVERWRITEPROMPT 
				OFN_PATHMUSTEXIST|OFN_NOREADONLYRETURN);
			if ( wfn!=NULL ) {
				char fn[MAX_PATH];
				wchar_to_utf8(wfn, wcslen(wfn)+1, fn, MAX_PATH);
				term_Log_esc(pt, fn);
			}
		}
		//menu_Check( IDM_LOG_ESC, pt->bLogEsc );
		break;
	case IDM_LOGG:
		if ( pt->bLogging ) 
			term_Logg( pt, NULL );
		else{
			WCHAR *wfn = fileDialog( hwnd, L"Log files (*.log)\0*.log\0All files (*.*)\0*.*\0\0", L"log",
				//do not use OFN_OVERWRITEPROMPT 
				OFN_PATHMUSTEXIST|OFN_NOREADONLYRETURN);
			if ( wfn!=NULL ) {
				char fn[MAX_PATH];
				wchar_to_utf8(wfn, wcslen(wfn)+1, fn, MAX_PATH);
				term_Logg(pt, fn);
			}
		}
		//menu_Check( IDM_LOGG, pt->bLogging );
		break;
	case IDM_SELBUF:
		pt->bSel= TRUE;
		pt->sel_x_m1= 0; pt->sel_line_m1= pt->line_tail;
		pt->sel_x_m2= pt->lx-1; pt->sel_line_m2= pt->do_line_index( pt->line_head, pt->ly-1);

		request_tiny_redraw(NEW_PAINT_RECT);
		break;
	case IDM_SELALL:
		pt->bSel= TRUE;
		pt->sel_x_m1= 0; pt->sel_line_m1= pt->line_scroll;
		pt->sel_x_m2= pt->lx-1; pt->sel_line_m2= pt->do_line_index( pt->line_scroll, pt->ly-1);
		
		request_tiny_redraw(NEW_PAINT_RECT);
		break;
	case IDM_CP_TITLE:
		if ( OpenClipboard(hwnd) ) {
			EmptyClipboard();
			WCHAR *wptr= pt->title;
			//wptr is auto
			unsigned wlen = pt->title_idx;
			if( wlen ){
			HANDLE hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (wlen+1)*sizeof(WCHAR));
			if ( hglbCopy!=NULL) {
				WCHAR *wbuf = static_cast<WCHAR*>( GlobalLock(hglbCopy) );
				wcsncpy(wbuf,wptr,wlen);
				wbuf[wlen]= 0;
				GlobalUnlock(hglbCopy);
				SetClipboardData(CF_UNICODETEXT, hglbCopy);
			}}
			//if(wptr){ free(wptr); }
			CloseClipboard();
		}
		break;
	case IDM_COPY:
		if ( OpenClipboard(hwnd) ) {
			EmptyClipboard();
			WCHAR *wptr=0;
			//wptr will be allocated by malloc
			unsigned wlen = term_CopyW(pt, &wptr);
			if( wlen ){
			HANDLE hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (wlen+1)*sizeof(WCHAR));
			if ( hglbCopy!=NULL) {
				WCHAR *wbuf = static_cast<WCHAR*>( GlobalLock(hglbCopy) );
				wcsncpy(wbuf,wptr,wlen);
				wbuf[wlen]= 0;
				GlobalUnlock(hglbCopy);
				SetClipboardData(CF_UNICODETEXT, hglbCopy);
			}}
			if(wptr){ free(wptr); }
			CloseClipboard();
		}
		break;
	case IDM_PASTE:
		if ( OpenClipboard(hwnd) ) {
			HANDLE hglb = GetClipboardData(CF_UNICODETEXT);
			WCHAR *ptr = static_cast<WCHAR*>( GlobalLock(hglb) );
			if (ptr != NULL) {
				int len =  wchar_to_utf8(ptr, -1, NULL, 0);
				char *p = (char *)malloc(len);
				if ( p!=NULL ) {
					wchar_to_utf8(ptr, -1, p, len);
					//no need paste trailing \0
					if(len)term_Paste(pt, p, len-1);
					free(p);
				}
				GlobalUnlock(hglb);
			}
			CloseClipboard();
		}
		break;
	case IDM_DELBUF:
		//pt->init_rollback_buf();
		reset_rollback_buf(pt);
		tiny_show_cursor();

		request_tiny_NC_redraw();
		request_tiny_redraw(NEW_PAINT_RECT);
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

//hwnd is hwndTerm
LRESULT CALLBACK MainWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	static WCHAR wm_chars[2]={0,0};	//for unicode character input
	//static
	BYTE	KeyState[256]; //for "shift, ctr, alt" keyb input state

	switch (msg) {
	case WM_CREATE:
		//tiny_new_font();
		//tiny_wnd_Size();
		SetTimer(hwnd, 1, 20, (TIMERPROC)NULL);	//redraw at 50Hz
		break;

		#if 0
	case WM_SIZE:
		//set term size by tiny window size
		if ( IsWindowVisible(hwnd) ) {	//change term size only when visible
			GetClientRect(hwnd, &termRect);
			term_setSize(pt, termRect.right/iFontWidth, 
						termRect.bottom/iFontHeight);
			request_tiny_redraw(EXISTED_PAINT_RECT);
		}
		//here no break with intention to WM_MOVE
		#endif
	case WM_MOVE:
		GetWindowRect(hwnd, &wndRect);
		break;

	case WM_PAINT: {
			PAINTSTRUCT ps;
			if ( BeginPaint(hwnd, &ps)!=NULL ) 
				tiny_Paint(ps.hdc, ps.rcPaint);
			EndPaint(hwnd, &ps);
		}
		break;
	case WM_TIMER:
		//keyb hook reporting
		if( is_esc_pending ){
			unsigned len = tiny_keytable[VK_ESCAPE].vk_str_len[esc_pending_mod];
			if(len) term_Send(pt, tiny_keytable[VK_ESCAPE].vk_str[esc_pending_mod].data(), len); 
			is_esc_pending= 0;
		}
		if(is_sysrq_pending){
			unsigned len = tiny_keytable[VK_SNAPSHOT].vk_str_len[sysrq_pending_mod];
			if(len) term_Send(pt, tiny_keytable[VK_SNAPSHOT].vk_str[sysrq_pending_mod].data(), len); 
			is_sysrq_pending= 0;
		}

		//repaint NC + C area
		if(is_NC_redraw_pending){ 
			term_Scroll(pt,0);
				//SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_DRAWFRAME);
				//RedrawWindow(hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
				//both calls RDW_INVALIDATE+RDW_VALIDATE is required
				//RedrawWindow(hwnd, NULL, NULL, RDW_NOFRAME | RDW_VALIDATE);
			tiny_Title();
			is_NC_redraw_pending= FALSE;
		}
		if ( is_redraw_pending && !is_paint_in_progress ) {
			is_paint_in_progress= TRUE;
			//repaint C area
			InvalidateRect(hwnd, &termRect, FALSE);
			termRect= {};
			is_redraw_pending= FALSE;
		}
		tiny_show_cursor();
		break;

	case WM_SETFOCUS:
		CreateCaret(hwnd, NULL, iFontWidth, iFontHeight/4);
		bFocus = TRUE;
		is_HideCaret_done= 1;
		break;
	case WM_KILLFOCUS:
		DestroyCaret();
		bFocus = FALSE;
		is_HideCaret_done= 1;
		break;
	#if 0
	case WM_IME_STARTCOMPOSITION:
		//moves the composition window to cursor pos on Win10
		break;
	#endif

	#if 0
	case WM_SYSCHAR:
		if( )

		//send all not used in WM_SYSKEYDOWN to DefWindowProc
		return DefWindowProc(hwnd,msg,wParam,lParam);
	#endif
	case WM_CHAR:
		if(! GetKeyboardState(KeyState) )
			{ MessageBoxA(hwnd,"Can not GetKeyboardState","Error",MB_OK|MB_ICONERROR); exit(-1); }

		//drop ctrl+
		if( KeyState[VK_CONTROL]&0x80U )break;
		//drop alt+
		if( (KeyState[VK_MENU]&0x80U) )break;
		//drop alt+shift+
		//if( (KeyState[VK_MENU]&0x80U) && KeyState[VK_SHIFT]&0x80U )break;

		if ( (wParam>>8)==0 )
		{
			char key = wParam&0x0ffU;
			if( unsigned(key) < 32 || key == 127 )break;
			term_Send(pt, &key, 1);

		}else{
			wm_chars[0] = wParam;
			term_SendW(pt, wm_chars, 1);
		}
		break; 
	case WM_KEYDOWN:
		//memset(KeyState,0,sizeof(KeyState));
		if( !GetKeyboardState(KeyState) )
			{ MessageBoxA(hwnd,"Can not GetKeyboardState","Error",MB_OK|MB_ICONERROR); exit(-1); }

		{ unsigned VK_key = (wParam & 0x0FFU);
		unsigned mod = 
			(KeyState[VK_SHIFT]&0x80U? KEY_MOD_SHIFT: 0)
			+ (KeyState[VK_MENU]&0x80U? KEY_MOD_ALT: 0)
			+ (KeyState[VK_CONTROL]&0x80U? KEY_MOD_CTRL: 0);
		
		//{ VK_NUM, expansion_method, key_str, { mod_str[7] } }
		//{ vk_str[8], vk_str_len[8] }
		unsigned len = tiny_keytable[VK_key].vk_str_len[mod];
		if(len) term_Send(pt, tiny_keytable[VK_key].vk_str[mod].data(), len); 
		
		//SB_BOTTOM
		switch(VK_key){
			//pressed mod keys do not scroll to command line
		case VK_SHIFT: case VK_MENU: case VK_CONTROL: break; 
		default:
			//do scroll to command line
			term_Scroll(pt,  pt->do_line_diff(pt->line_scroll, pt->line_head)); 
		}}
		break;

	case WM_SYSKEYDOWN:
		//memset(KeyState,0,sizeof(KeyState));
		if( !GetKeyboardState(KeyState) )
			{ MessageBoxA(hwnd,"Can not GetKeyboardState","Error",MB_OK|MB_ICONERROR); exit(-1); }

		{ unsigned VK_key = (wParam & 0x0FFU);
		unsigned mod = 
			(KeyState[VK_SHIFT]&0x80U? KEY_MOD_SHIFT: 0)
			+ (KeyState[VK_MENU]&0x80U? KEY_MOD_ALT: 0)
			+ (KeyState[VK_CONTROL]&0x80U? KEY_MOD_CTRL: 0);
		
		//{ VK_NUM, expansion_method, key_str, { mod_str[7] } }
		//{ vk_str[8], vk_str_len[8] }
		unsigned len = tiny_keytable[VK_key].vk_str_len[mod];
		if(len) { term_Send(pt, tiny_keytable[VK_key].vk_str[mod].data(), len); break; }}

		//send all not used in WM_SYSKEYDOWN to DefWindowProc
		return DefWindowProc(hwnd,msg,wParam,lParam);

	case WM_VSCROLL: 
		switch ( LOWORD (wParam) )
		{
		//request_tiny_redraw(NEW_PAINT_RECT); 
		case SB_TOP:		term_Scroll(pt,  pt->do_line_diff(pt->line_scroll, pt->line_tail)); break; 
		case SB_BOTTOM:		term_Scroll(pt,  pt->do_line_diff(pt->line_scroll, pt->line_head)); break;
		case SB_LINEUP:		term_Scroll(pt,  1); break;
		case SB_LINEDOWN:	term_Scroll(pt, -1); break;
		case SB_PAGEUP:		term_Scroll(pt, pt->ly-1); break;
		case SB_PAGEDOWN:	term_Scroll(pt, 1-pt->ly); break;
		case SB_THUMBTRACK: {
				SCROLLINFO si; 
				si.cbSize = sizeof (si);
				si.fMask = SIF_ALL;
				GetScrollInfo (hwnd, SB_VERT, &si);
				term_Scroll(pt, si.nPos-si.nTrackPos);
			}
		}
		break;

	case WM_MOUSEWHEEL:
		if( !pt->bAppMouse || !pt->mouse_mode || (wParam & MK_CONTROL) ){
			term_Scroll(pt,  GET_WHEEL_DELTA_WPARAM(wParam)/40);
			//tiny_Redraw(); 
		}else{
			POINT cwin;
			cwin.x = GET_X_LPARAM(lParam);
			cwin.y = GET_Y_LPARAM(lParam);
			ScreenToClient(hwnd, &cwin);

			//do not lock CTRL in AppMode state, because no "wheel up"
			term_send_mouse_event(pt, TERM::MOUSE_WHEEL, GET_WHEEL_DELTA_WPARAM(wParam), 
					//not client window x,y params
					//GET_X_LPARAM(lParam)/iFontWidth, (GET_Y_LPARAM(lParam)+2)/iFontHeight );
					cwin.x/iFontWidth, (cwin.y+2)/iFontHeight );
		}
		break;
	case WM_LBUTTONDBLCLK:
	case WM_LBUTTONDOWN: 
		SetCapture(hwnd);
		if( !pt->bAppMouse || !pt->mouse_mode || (wParam & MK_CONTROL) ){
			term_Mouse(pt, TERM::LEFTDOWN, GET_X_LPARAM(lParam)/iFontWidth,
							(GET_Y_LPARAM(lParam)+2)/iFontHeight);
		}else{
			term_send_mouse_event(pt, TERM::MOUSE_BUT_1, 1, 
							GET_X_LPARAM(lParam)/iFontWidth, (GET_Y_LPARAM(lParam)+2)/iFontHeight );
		}
		break;
	case WM_RBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
		if( !pt->bAppMouse || !pt->mouse_mode || (wParam & MK_CONTROL) ){
		}else{
			term_send_mouse_event(pt, TERM::MOUSE_BUT_2, 1, 
							GET_X_LPARAM(lParam)/iFontWidth, (GET_Y_LPARAM(lParam)+2)/iFontHeight );
		}
		break;
	case WM_MOUSEMOVE:
		if ( MK_LBUTTON&wParam ) {
		if( !pt->bAppMouse || !pt->mouse_mode || (wParam & MK_CONTROL) ){
			term_Mouse(pt, TERM::LEFTDRAG, GET_X_LPARAM(lParam)/iFontWidth,
							(GET_Y_LPARAM(lParam)+2)/iFontHeight);
		}
		else if( pt->mouse_mode > TERM::MOUSE_MODE_1 ){
			term_send_mouse_event(pt, TERM::MOUSE_MOVE, TERM::MOUSE_BUT_1, 
							GET_X_LPARAM(lParam)/iFontWidth, (GET_Y_LPARAM(lParam)+2)/iFontHeight );
		}}
		else if ( MK_RBUTTON&wParam ) {
		if( pt->mouse_mode > TERM::MOUSE_MODE_1 ){
			term_send_mouse_event(pt, TERM::MOUSE_MOVE, TERM::MOUSE_BUT_2, 
							GET_X_LPARAM(lParam)/iFontWidth, (GET_Y_LPARAM(lParam)+2)/iFontHeight );
		}}
		else if( pt->mouse_mode == TERM::MOUSE_MODE_3 ){
			term_send_mouse_event(pt, TERM::MOUSE_MOVE, TERM::MOUSE_BUT_NO, 
							GET_X_LPARAM(lParam)/iFontWidth, (GET_Y_LPARAM(lParam)+2)/iFontHeight );
		}
		break;
	case WM_LBUTTONUP:
		ReleaseCapture();
		if( !pt->bAppMouse || !pt->mouse_mode || (wParam & MK_CONTROL) ){
			term_Mouse(pt, TERM::LEFTUP, GET_X_LPARAM(lParam)/iFontWidth,
							(GET_Y_LPARAM(lParam)+2)/iFontHeight);
		}else{
			term_send_mouse_event(pt, TERM::MOUSE_BUT_1, 0, 
					GET_X_LPARAM(lParam)/iFontWidth, (GET_Y_LPARAM(lParam)+2)/iFontHeight );
		}
		break;
	case WM_RBUTTONUP:
		//menu_popup if mouse for tinyterm
		if( !pt->bAppMouse || !pt->mouse_mode || (wParam & MK_CONTROL) ) return DefWindowProc(hwnd,msg,wParam,lParam);
		//send mouse to app
		else{ 
			term_send_mouse_event(pt, TERM::MOUSE_BUT_2, 0, 
					GET_X_LPARAM(lParam)/iFontWidth, (GET_Y_LPARAM(lParam)+2)/iFontHeight );
		}
		break;
	case WM_CONTEXTMENU:

        //check context menu was requested on a scrollbar HTVSCROLL
		{ POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		// Convert screen coordinates to client coordinates
		//ScreenToClient(hwnd, &point);
		// Perform hit testing
		LRESULT hitTest = SendMessage(hwnd, WM_NCHITTEST, 0, MAKELPARAM(point.x, point.y));

        // Context menu was requested on a scrollbar
		if (hitTest == HTVSCROLL)return DefWindowProc(hwnd,msg,wParam,lParam);	}

		//menu_Popup(CONTEX, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		menu_Check( IDM_KEYBXON, pt->bKeyb );
		menu_Check( IDM_M0, pt->mouse_mode == TERM::MOUSE_DIS );
		menu_Check( IDM_M1, pt->mouse_mode == TERM::MOUSE_MODE_1 );
		menu_Check( IDM_M2, pt->mouse_mode == TERM::MOUSE_MODE_2 );
		menu_Check( IDM_M3, pt->mouse_mode == TERM::MOUSE_MODE_3 );
		menu_Check( IDM_MICE_XTERM, pt->bOptMouseXterm );
		menu_Check( IDM_LOGG, pt->bLogging );
		menu_Check( IDM_CURSOR_BS, pt->bOptCursorBS );
		menu_Check( IDM_ESC_DO_CAN, pt->bOptESC_do_CAN);
		menu_Check( IDM_HIDE_ESC, pt->bOptHideEsc );
		menu_Check( IDM_LOG_ESC, pt->bLogEsc );
		//menu_Check( IDM_DISP_UTF8, pt->bOptDisplayUTF8 );
		menu_Check( IDM_DISP_UTF8, pt->OptDisplay_mode == TERM::Display_UTF8 );
		menu_Check( IDM_DISP_OEM,  pt->OptDisplay_mode == TERM::Display_OEM );
		menu_Check( IDM_DISP_ANSI, pt->OptDisplay_mode == TERM::Display_ANSI );
		menu_Check( IDM_KEYB_UTF8, pt->bOptKeybUTF8 );
		menu_Check( IDM_WRAP, pt->bWrap );

		{ POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		//keyb shift+F10 used to call menu
		if( point.x == -1 && point.y == -1 ){ ClientToScreen(hwnd, &point); }
		TrackPopupMenu(hMenu[CONTEX], TPM_LEFTBUTTON, point.x, point.y, 0, hwnd, NULL);	}
		break;
	#if 0
	case WM_NCLBUTTONDOWN: {
		int y = GET_Y_LPARAM(lParam)-wndRect.top;
		if ( y>0 && y<titleHeight ) {
			int x = GET_X_LPARAM(lParam)-wndRect.left;
			if ( x>menuX[0] && x<menuX[3] ) return 0;
		}
		return DefWindowProc(hwnd,msg,wParam,lParam);
	}
	case WM_NCLBUTTONUP: {
		int y = GET_Y_LPARAM(lParam)-wndRect.top;
		if ( y>0 && y<titleHeight ) {
			int x = GET_X_LPARAM(lParam)-wndRect.left;
			for ( int i=0; i<3; i++ ) {
				if ( x>menuX[i] && x<menuX[i+1] ) {
					menu_Popup(i);
					return 0;
				}
			}
		}
		return DefWindowProc(hwnd,msg,wParam,lParam);
	}
	#endif

	case WM_COMMAND:
		if ( menu_Command(hwnd, wParam, lParam) ) return 1;
		break;
	case WM_SYSCOMMAND:
		//hook ALT+key
		if( wParam == SC_KEYMENU ){
			switch( (lParam&0x0FFFFU) ){
			//post to windows
			//ALT+SPACE
			case ' ':
				return DefWindowProc(hwnd,msg,wParam,lParam);
			}
			//drop others
			return 0;
		}
		//post to windows
		return DefWindowProc(hwnd,msg,wParam,lParam);

	case WM_CLOSE:
		if ( pt->bLogging ) term_Logg(pt, NULL);
		DestroyMenu(hMainMenu);
		DeleteObject(dwBkBrush);
		DestroyWindow(hwnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default: 
		return DefWindowProc(hwnd,msg,wParam,lParam);
	}
	return 0;
}

//pty stub pipe reader thread 
static uintptr_t thr_rd;
static unsigned __stdcall
	read_PTY_pipe( void * ){
	
	enum { SZ= 4096 };
	char buf[SZ+16];
	int rlen;
		
	for(;;){
		if( (rlen = read(rfPTY_stdout, buf, SZ)) <= 0)
		#ifndef DBG_BOX
			{ exit(rlen); }
		#else
			{ MessageBoxA(hwndTerm,"Can not read rfPTY_stdout","Error",MB_OK|MB_ICONERROR); exit(rlen); }
		#endif

		buf[rlen]= 0;
		//term_Parse(pt, buf, strlen(buf));
		term_Parse(pt, buf, rlen);
	}

	return 0;
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) 
{
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT *)lParam;

		//#define COND_SHIFT (GetAsyncKeyState(VK_SHIFT) & 0x8000)
		//#define COND_CTRL (GetAsyncKeyState(VK_CONTROL) & 0x8000)
		//#define COND_ALT (GetAsyncKeyState(VK_MENU) & 0x8000)
		
		if( p->vkCode == VK_ESCAPE || p->vkCode == VK_SNAPSHOT ){
		BYTE	KeyState[256]; //for "shift, ctr, alt" keyb input state

		if( GetKeyboardState(KeyState) ){

		#define COND_SHIFT (KeyState[VK_SHIFT]&0x80U)
		#define COND_CTRL (KeyState[VK_CONTROL]&0x80U)
		#define COND_ALT (KeyState[VK_MENU]&0x80U)

        // Check Esc is pressed
        if( p->vkCode == VK_ESCAPE )
		{ 
			if(!is_esc_pending)
			if(wParam ==  WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
			{
				unsigned mod= KEY_MOD_PURE;

				//while Shift is down
				mod|= COND_SHIFT? KEY_MOD_SHIFT: 0;
				//while Ctrl is down
				mod|= COND_CTRL? KEY_MOD_CTRL: 0;
				//while Alt is down
				//mod|= ( p->flags & LLKHF_ALTDOWN)? KEY_MOD_ALT: 0;
				mod|= COND_ALT? KEY_MOD_ALT: 0;
			
				esc_pending_mod= mod; 
				is_esc_pending= TRUE; 
			}
			return 1; //Block mod+Esc
		}

        // Check PrntScr is pressed
        if( p->vkCode == VK_SNAPSHOT )
		{ 
			unsigned mod= KEY_MOD_PURE;
			//while Ctrl is down
			mod|= COND_CTRL? KEY_MOD_CTRL: 0;

			//if( (mod & KEY_MOD_CTRL) )
			if( !(mod & KEY_MOD_CTRL) )goto break_no_hook;
					
			if(!is_sysrq_pending)
			if(wParam ==  WM_KEYDOWN || wParam == WM_SYSKEYDOWN){
				//while Shift is down
				mod|= COND_SHIFT? KEY_MOD_SHIFT: 0;
				//while Alt is down
				//mod|= ( p->flags & LLKHF_ALTDOWN)? KEY_MOD_ALT: 0;
				mod|= COND_ALT? KEY_MOD_ALT: 0;
			
				sysrq_pending_mod= mod; 
				is_sysrq_pending= TRUE; 
			}
			return 1; //Block ctrl+mod+PrntScr
		}
    }}}
	break_no_hook:
    return CallNextHookEx(hhkLowLevelKybd, nCode, wParam, lParam);
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
									_In_ LPWSTR lpCmdLine, _In_ INT nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	BOOL bDefCfg= FALSE;

	//parse cmdline
	//app_lpCmdLine = lpCmdLine;
	{ WCHAR *p= lpCmdLine;
	for(; *p; ++p){
		//if already bDefCfg loop just to skip trailing spaces
		if( (*p == L'-') && !bDefCfg ){
			if( p[1] != L'd' )break; //first found opt is not "-d"; and new windows will get "-..."
			//found opt is "-d"
			bDefCfg= TRUE; p+= 2; //no break; and new windows will not get "-d"
		}
		if(*p != L' ')break; //end skip trailing spaces
	} 
	app_lpCmdLine= p; }

	//create config file name
	{ WCHAR *cfg_path= _wgetenv(CFG_PATH_ENV); //path
	while(cfg_path){ //if path exist
		//create full fname
		enum { STR_SZ = 2*MAX_PATH };
		WCHAR buf[STR_SZ+16];
		_snwprintf(buf, STR_SZ, L"%s\\%s", cfg_path, CFG_FNAME); buf[STR_SZ]= 0;
		cfg_fname = static_cast<WCHAR*>(malloc( (wcslen(buf)+1)*sizeof(WCHAR) ));
		wcscpy(cfg_fname,buf);
		break;
	}}
	
	//init keytable
	init_keytable(tiny_keytable);
	//GetModuleHandle(NULL),GetCurrentThreadId()
	hhkLowLevelKybd = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0 ); 
	if(!hhkLowLevelKybd){ MessageBoxA(hwndTerm,"Can not SetWindowsHookEx\n not all keys will be allowed","Warning",MB_OK|MB_ICONERROR); }

	////RegisterClass
	WNDCLASSEX wc;
	wc.cbSize 		= sizeof(wc);
	wc.style 		= CS_DBLCLKS; // | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc 	= MainWndProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= 0;
	wc.hInstance 	= hInstance;
	wc.hIcon 		= LoadIcon(hInstance, MAKEINTRESOURCE(IDICON_TL1));
	wc.hIconSm 		= 0;
	wc.hCursor		= LoadCursor(NULL,IDC_ARROW);
	wc.hbrBackground = 0; //(HBRUSH)(COLOR_APPWORKSPACE+1); 
	wc.lpszMenuName = 0; //MAKEINTRESOURCE(IDMENU_MAIN); 
	wc.lpszClassName = L"TWnd";

	if ( !RegisterClassEx(&wc) ) return 0;

	////InitInstance
	hInst = hInstance;

	TERM term;
	pt = &term;
	if ( !term_Construct(pt) ) return 0;

	HDC sysDC = GetDC(0);
	dpi = GetDeviceCaps(sysDC, LOGPIXELSX);
	ReleaseDC(0, sysDC);
	dwBkBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
	
	hMainMenu = LoadMenu( hInst, MAKEINTRESOURCE(IDMENU_MAIN));
	for ( int i=0; i<Menu_SZ; ++i ) hMenu[i] = GetSubMenu(hMainMenu, i);

	//hTermFont
	new_font(hwndTerm);
	
	hwndTerm = CreateWindowEx( 
						#ifdef IS_LAYERED
						WS_EX_LAYERED |
						#endif
						0,
						L"TWnd", wndTitle,
						TERM_WS_STYLE,
						CW_USEDEFAULT, CW_USEDEFAULT,
						CW_USEDEFAULT, CW_USEDEFAULT,
						NULL, NULL, hInst, NULL );
	if (!hwndTerm){ MessageBoxA(hwndTerm,"Can not CreateWindow","Error",MB_OK|MB_ICONERROR); exit(-1); }
		
	ShowScrollBar(hwndTerm, SB_VERT, TRUE);

	//load config file
	if(!bDefCfg)load_cfg(hwndTerm, IS_DEFAULT_LOAD);
	if(!bDefCfg)load_keymap(hwndTerm, tiny_keytable, IS_DEFAULT_LOAD);
	tiny_wnd_Size();

	//first time reset scrollbar by lines==0
	term_Scroll(pt,0);

	#ifdef IS_LAYERED
	//WS_EX_LAYERED: 255 makes the window 100% alpha (completely opaque)
	SetLayeredWindowAttributes(hwndTerm, 0, iTransparency, LWA_ALPHA);
	#endif
	ShowWindow(hwndTerm, nCmdShow);
	UpdateWindow(hwndTerm);

	////pty stub
	//SECURITY_ATTRIBUTES sa;
	//HANDLE hPTY_stdin, hCH_stdin, hPTY_stdout, hCH_stdout;

	//create (&rd,&wr) IPC pipe handles
	if( !CreatePipe(&hCH_stdin, &hPTY_stdin, 0, 0) 
		|| !CreatePipe(&hPTY_stdout, &hCH_stdout, 0, 0) 
		){ MessageBoxA(hwndTerm,"Can not CreatePipe","Error",MB_OK|MB_ICONERROR); exit(-1); } 

	//mark pipe child side to inherit
	if ( !SetHandleInformation(hCH_stdin, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT) 
		|| !SetHandleInformation(hCH_stdout, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT) 
		){ MessageBoxA(hwndTerm,"Can not SetHandleInformation","Error",MB_OK|MB_ICONERROR); exit(-1); } 

	//create child process
	#define STUB_FNAME (L"PTYstub.exe")
	const unsigned			str_sz= (MAX_PATH + wcslen(app_lpCmdLine));
	WCHAR					*szCmdline= static_cast<WCHAR*>(malloc( (str_sz+1)*sizeof(WCHAR) ));
	_snwprintf(szCmdline, str_sz, L"%s %s", STUB_FNAME, app_lpCmdLine); szCmdline[str_sz]= 0;

	PROCESS_INFORMATION		pi; memset( &pi, 0, sizeof(pi) );
	STARTUPINFO				si; memset( &si, 0, sizeof(si) );
 
	//dup pipe child side to child stdin/stdout
	si.cb = sizeof(si); 
	si.dwFlags |= STARTF_USESTDHANDLES;
	si.hStdInput = hCH_stdin;
	si.hStdOutput = hCH_stdout;
	si.hStdError = hCH_stdout;
 
	if( !CreateProcess(NULL, 
			szCmdline,		// command line 
			NULL,			// process security attributes 
			NULL,			// primary thread security attributes 
			TRUE,			// handles are inherited 
			0,				// creation flags 
			NULL,			// use parent's environment 
			NULL,			// use parent's current directory 
			&si,			// STARTUPINFO pointer 
			&pi				// receives PROCESS_INFORMATION
		)){ MessageBoxA(hwndTerm,"Can not CreateProcess","Error",MB_OK|MB_ICONERROR); exit(-1); }   

	CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
	CloseHandle(hCH_stdin); hCH_stdin= INVALID_HANDLE_VALUE;
	CloseHandle(hCH_stdout); hCH_stdout= INVALID_HANDLE_VALUE;

	//Convert HANDLE to POSIX file descriptor
	wfPTY_stdin= _open_osfhandle((intptr_t)hPTY_stdin, 0);
	rfPTY_stdout= _open_osfhandle((intptr_t)hPTY_stdout, _O_RDONLY);

	//fork read pipe thread loop
	if( (thr_rd= _beginthreadex( 0, 0, read_PTY_pipe, 0, CREATE_SUSPENDED, 0 )) == 0)
		{ MessageBoxA(hwndTerm,"Can not _beginthreadex","Error",MB_OK|MB_ICONERROR); exit(-1); }
	if( ResumeThread((HANDLE)thr_rd) == -1 ){ MessageBoxA(hwndTerm,"Can not ResumeThread","Error",MB_OK|MB_ICONERROR); exit(-1); }

	////continue main msg loop
	//HACCEL haccel = LoadAccelerators(hInst, MAKEINTRESOURCE(IDACCEL_MAIN));

	MSG msg;
	BOOL bRet;
	while ( 0 != (bRet = GetMessage(&msg, NULL, 0, 0)) )
	{
		if (bRet == -1)break;
		//if ( IsWindow(hwndScriptDlg) && IsDialogMessage(hwndScriptDlg, &msg) ) continue;
		//if (!TranslateAccelerator(hwndTerm, haccel,  &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	
	term_Destruct(pt);

	//
	UnhookWindowsHookEx(hhkLowLevelKybd);

	return (bRet? -1: (int)msg.wParam);
}
