//
// tinyTerm7 -- A minimal Windows terminal emulator

#include "stdafx.h"
#include "tinyterm.h"
#include "keytable.h"
//using std::vector;

#define KEYMAP_FNAME		(L"TinyTerm7.keymap")
#define KEYMAP_PATH_ENV		(L"USERPROFILE")
static WCHAR *keymap_fname= 0;
enum { KEYMAP_STR_SZ=32 }; //KEYMAP file string field maxsize

/* 
static config of keyboard
	how to auto expand init pure key in keytable_init init row
		do nothing, copy, do format \e[num;mod~ (num=ascii <letter>), do format \e[1;mod<letter>, 
	do format EXP_CTRL_TILDA { 
		skip pure and shift state, (done byTranslateMEssage)
		^code map to ctrl+shift, 
		esc and esc+shiftto ^[+letter and ^[+letter+shift 
		others by tilda method by expand_data value }
	do format EXP_DIGIT_TILDA { 
		skip pure and shift state, (done byTranslateMEssage)
		esc and esc+shiftto ^[+letter and ^[+letter+shift 
		others by tilda method by expand_data value } 
*/
enum { EXP_NO= 0, EXP_COPY, EXP_FORMAT_LETTER, EXP_FORMAT_TILDA, EXP_CTRL_TILDA, EXP_CTRL_SH_TILDA, EXP_CTRL_EN_TILDA, EXP_DIGIT_TILDA, EXP_METHODS };
//init records to compiled keytable
struct t_keytable_init{
	unsigned 	vk_num;
	unsigned 	expand_method;
	unsigned	expand_data;			//extra data to autoexpand if any, depend on EXP_METHOD
	unsigned	expand_data_2;
	const char 	*key_str;				//pure key
	const char 	*mod_str[KEY_MODS-1];	//modified key
};
//return 0 if error
//void init_keytable_row( t_keytable _Inout_ *keytable, t_keytable_init const _In_ *init_row );

//
t_keytable tiny_keytable[VK_KEYS];

//
void init_keytable_row( t_keytable * _Inout_ keytable, t_keytable_init const _In_ *init_row )
{
	assert(keytable);
	assert(init_row);

	unsigned num = init_row->vk_num;
	assert(num < VK_KEYS);
	assert(init_row->expand_method < EXP_METHODS);
	assert(init_row->key_str);

	std::vector<char>	*p_vk_str  = keytable[num].vk_str;
	unsigned			*p_vk_str_len = keytable[num].vk_str_len;

	//pure key len
	p_vk_str_len[0] = strlen(init_row->key_str);
	if( p_vk_str_len[0] ){
		//pure key data
		p_vk_str[0].assign( p_vk_str_len[0], 0 );
		memcpy( p_vk_str[0].data(), init_row->key_str, p_vk_str_len[0] );
	}
	
	//auto expansion
	switch( init_row->expand_method ){
	case EXP_NO: 
			for( unsigned m=1; m<KEY_MODS; ++m)			
			{ 
				p_vk_str_len[m] = strlen(init_row->mod_str[m-1]);
				if( p_vk_str_len[m] ){
				p_vk_str[m].assign( p_vk_str_len[m], 0 ); 
				memcpy( p_vk_str[m].data(), init_row->mod_str[m-1], p_vk_str_len[m] );
			}}
			break;

	case EXP_COPY: 
			if( *p_vk_str_len )
			for( unsigned m=1; m<KEY_MODS; ++m)			
			{ 
				keytable[num].vk_str[m].assign( *p_vk_str_len, 0 ); 
				memcpy( keytable[num].vk_str[m].data(), init_row->key_str, *p_vk_str_len );
				keytable[num].vk_str_len[m] = *p_vk_str_len;
			}
			break;

	case EXP_FORMAT_LETTER: //\e[1;mod<letter>
			for( unsigned m=1; m<KEY_MODS; ++m)			
			{ 
				enum{ BUF_SZ= 64 };
				char buf[BUF_SZ+16]; buf[BUF_SZ]=0;
				_snprintf(buf, BUF_SZ, "\033[1;%u%c", m+1, (init_row->expand_data & 0x0FFU));
				unsigned len= strlen(buf);

				keytable[num].vk_str[m].assign( len, 0 ); 
				memcpy( p_vk_str[m].data(), buf, len );
				keytable[num].vk_str_len[m] = len;
			}
			break;

	case EXP_FORMAT_TILDA: //\e[num;mod~
			for( unsigned m=1; m<KEY_MODS; ++m)			
			{ 
				enum{ BUF_SZ= 64 };
				char buf[BUF_SZ+16]; buf[BUF_SZ]=0;
				_snprintf(buf, BUF_SZ, "\033[%u;%u~", (init_row->expand_data & 0x0FFU), m+1);
				unsigned len= strlen(buf);

				keytable[num].vk_str[m].assign( len, 0 ); 
				memcpy( p_vk_str[m].data(), buf, len );
				keytable[num].vk_str_len[m] = len;
			}
			break;

	case EXP_CTRL_TILDA:
			//skip pure and shift
			for( unsigned m=2; m<KEY_MODS; ++m)			
			{ 
				enum{ BUF_SZ= 64 };
				char buf[BUF_SZ+16]; buf[BUF_SZ]=0;
				switch(m){
				case KEY_MOD_ALT:
					//alt ^[+<letter>
					_snprintf(buf, BUF_SZ, "\033%c", (init_row->expand_data_2 & 0x0FFU));
					break;
				case (KEY_MOD_ALT | KEY_MOD_SHIFT):
					//alt+shift ^[+<shift+letter>
					_snprintf(buf, BUF_SZ, "\033%c", (init_row->expand_data & 0x0FFU));
					break;
				case (KEY_MOD_CTRL | KEY_MOD_SHIFT):
					//map ^letter to ctrl+shift+letter
					_snprintf(buf, BUF_SZ, "%c", (init_row->expand_data & 0x0FFU) - 0x40U);
					break;
				default:
					//TILDA mode
					_snprintf(buf, BUF_SZ, "\033[%u;%u~", (init_row->expand_data & 0x0FFU), m+1);
				}
				unsigned len= strlen(buf);

				keytable[num].vk_str[m].assign( len, 0 ); 
				memcpy( p_vk_str[m].data(), buf, len );
				keytable[num].vk_str_len[m] = len;
			}
			break;

	case EXP_CTRL_EN_TILDA:
			//skip pure and shift
			for( unsigned m=2; m<KEY_MODS; ++m)			
			{ 
				enum{ BUF_SZ= 64 };
				char buf[BUF_SZ+16]; buf[BUF_SZ]=0;
				switch(m){
				case KEY_MOD_ALT:
					//alt ^[+<letter>
					_snprintf(buf, BUF_SZ, "\033%c", (init_row->expand_data_2 & 0x0FFU));
					break;
				case (KEY_MOD_ALT | KEY_MOD_SHIFT):
					//alt+shift ^[+<shift+letter>
					_snprintf(buf, BUF_SZ, "\033%c", (init_row->expand_data & 0x0FFU));
					break;
				case KEY_MOD_CTRL:
				case (KEY_MOD_CTRL | KEY_MOD_SHIFT):
					//map ^letter to ctrl+shift+letter
					_snprintf(buf, BUF_SZ, "%c", (init_row->expand_data & 0x0FFU) - 0x40U);
					break;
				default:
					//TILDA mode
					_snprintf(buf, BUF_SZ, "\033[%u;%u~", (init_row->expand_data & 0x0FFU), m+1);
				}
				unsigned len= strlen(buf);

				keytable[num].vk_str[m].assign( len, 0 ); 
				memcpy( p_vk_str[m].data(), buf, len );
				keytable[num].vk_str_len[m] = len;
			}
			break;

	case EXP_CTRL_SH_TILDA:
			//skip pure and shift
			for( unsigned m=2; m<KEY_MODS; ++m)			
			{ 
				enum{ BUF_SZ= 64 };
				char buf[BUF_SZ+16]; buf[BUF_SZ]=0;
				switch(m){
				case KEY_MOD_ALT:
					//alt ^[+<letter>
					_snprintf(buf, BUF_SZ, "\033%c", (init_row->expand_data & 0x0FFU));
					break;
				case (KEY_MOD_ALT | KEY_MOD_SHIFT):
					//alt+shift ^[+<shift+letter>
					_snprintf(buf, BUF_SZ, "\033%c", (init_row->expand_data_2 & 0x0FFU));
					break;
				case (KEY_MOD_CTRL | KEY_MOD_SHIFT):
					//map ^letter to ctrl+shift+letter
					_snprintf(buf, BUF_SZ, "%c", (init_row->expand_data & 0x0FFU) - 0x40U);
					break;
				default:
					//TILDA mode
					_snprintf(buf, BUF_SZ, "\033[%u;%u~", (init_row->expand_data & 0x0FFU), m+1);
				}
				unsigned len= strlen(buf);

				keytable[num].vk_str[m].assign( len, 0 ); 
				memcpy( p_vk_str[m].data(), buf, len );
				keytable[num].vk_str_len[m] = len;
			}
			break;

	case EXP_DIGIT_TILDA:
			//skip pure and shift
			for( unsigned m=2; m<KEY_MODS; ++m)			
			{ 
				enum{ BUF_SZ= 64 };
				char buf[BUF_SZ+16]; buf[BUF_SZ]=0;
				switch(m){
				case KEY_MOD_ALT:
					//alt ^[+<digit>
					_snprintf(buf, BUF_SZ, "\033%c", (init_row->expand_data & 0x0FFU));
					break;
				case (KEY_MOD_ALT | KEY_MOD_SHIFT):
					//alt+shift ^[+<shift+digit>
					_snprintf(buf, BUF_SZ, "\033%c", (init_row->expand_data_2 & 0x0FFU));
					break;
				default:
					//TILDA mode
					_snprintf(buf, BUF_SZ, "\033[%u;%u~", (init_row->expand_data & 0x0FFU), m+1); 
				}
				unsigned len= strlen(buf);

				keytable[num].vk_str[m].assign( len, 0 ); 
				memcpy( p_vk_str[m].data(), buf, len );
				keytable[num].vk_str_len[m] = len;
			}
			break;
	}
}

//
void init_keytable( t_keytable * _Inout_ keytable )
{
	assert(keytable);

	for(unsigned num=0; num<VK_KEYS; ++num){ 
		//keytable[num].vk_str[m].clear();
		memset( keytable[num].vk_str_len, 0, KEY_MODS*sizeof(unsigned) ); 
	}

	//	pure, shift, alt, shift+alt, ctrl, shift+ctrl, alt+ctrl, shift+alt+ctrl
	t_keytable_init	keytable_init[]={
		//{ VK_ESCAPE, EXP_FORMAT_TILDA, 033, "\030", {} },		//ESC	\CAN
		{ VK_ESCAPE, EXP_NO, 27, 0, "\030", { "\033[27;2~", "\033\033", "\033[27;4~", "\033", "\033[27;6~", "\033[27;7~", "\033[27;8~" } },		//ESC	\CAN
		{ VK_OEM_4, EXP_NO, '[', 0, "", { "", "\033[", "\033{", "\033[91;5~", "\033", "\033[91;7~", "\033[91;8~" } },		//^[  \ESC
		
		//{ VK_RETURN, EXP_FORMAT_TILDA, 13, 0, "\015", {} },
		{ VK_RETURN, EXP_NO, 13, 0, "\015", { "\033[13;2~", "\033\015", "\033[13;4~", "\012", "\033[13;6~", "\033\012", "\033[13;8~" } },
		{ VK_SPACE,  EXP_NO, 32, 0, "", { "", "\033\040", "\033[32;4~", "\033[32;5~", "\033[32;6~", "\033[32;7~", "\033[32;8~" } },

		//{ VK_TAB, EXP_FORMAT_TILDA, 9, 0, "\011", {} },
		{ VK_TAB, EXP_NO, 9, 0, "\011", { "\033[9;2~", "\033\011", "\033[9;4~", "\033[9;5~", "\033[9;6~", "\033[9;7~", "\033[9;8~" } },

		//{ VK_BACK,	EXP_FORMAT_TILDA, 8, 0, "\010", {} },
		{ VK_BACK,	EXP_NO, 8, 0, "\010", { "\033[8;2~", "\033\010", "\033[8;4~", "\033[8;5~", "\033[8;6~", "\033[8;7~", "\033[8;8~" } },

		//
		{ VK_OEM_3, EXP_DIGIT_TILDA, '`', '~', "", { "" } },
		{ VK_OEM_PLUS, EXP_DIGIT_TILDA, '=', '+',"", { "" } },
		{ VK_OEM_1, EXP_DIGIT_TILDA, ';', ':', "", { "" } },
		{ VK_OEM_7, EXP_DIGIT_TILDA, 39, '"', "", { "" } },
		{ VK_OEM_2, EXP_DIGIT_TILDA, '/', '?', "", { "" } },
		
		{ VK_OEM_COMMA, EXP_DIGIT_TILDA, ',', '<', "", { "" } },
		{ VK_OEM_PERIOD, EXP_DIGIT_TILDA, '.', '>', "", { "" } },

		{ '1', EXP_DIGIT_TILDA, '1', '!', "", { "" } },
		//{ '2', EXP_DIGIT_TILDA, '2', '@', "", { "" } },
		{ '3', EXP_DIGIT_TILDA, '3', '#', "", { "" } },
		{ '4', EXP_DIGIT_TILDA, '4', '$', "", { "" } },
		{ '5', EXP_DIGIT_TILDA, '5', '%', "", { "" } },
		//{ '6', EXP_DIGIT_TILDA, '6', '^', "", { "" } },
		{ '7', EXP_DIGIT_TILDA, '7', '&', "", { "" } },
		{ '8', EXP_DIGIT_TILDA, '8', '*', "", { "" } },
		{ '9', EXP_DIGIT_TILDA, '9', '(', "", { "" } },
		{ '0', EXP_DIGIT_TILDA, '0', ')', "", { "" } },

		//
		{ '2', EXP_CTRL_TILDA, '@', '2', "", { "" } },
		{ 'A', EXP_CTRL_EN_TILDA, 'A', 'a', "", { "" } },
		{ 'B', EXP_CTRL_EN_TILDA, 'B', 'b', "", { "" } },
		{ 'C', EXP_CTRL_TILDA, 'C', 'c', "", { "" } },
		{ 'D', EXP_CTRL_TILDA, 'D', 'd', "", { "" } },
		{ 'E', EXP_CTRL_EN_TILDA, 'E', 'e', "", { "" } },
		{ 'F', EXP_CTRL_EN_TILDA, 'F', 'f', "", { "" } },
		{ 'G', EXP_CTRL_EN_TILDA, 'G', 'g', "", { "" } },
		{ 'H', EXP_CTRL_TILDA, 'H', 'h', "", { "" } },
		{ 'I', EXP_CTRL_TILDA, 'I', 'i', "", { "" } },
		{ 'J', EXP_CTRL_TILDA, 'J', 'j', "", { "" } },
		{ 'K', EXP_CTRL_TILDA, 'K', 'k', "", { "" } },
		{ 'L', EXP_CTRL_TILDA, 'L', 'l', "", { "" } },
		{ 'M', EXP_CTRL_TILDA, 'M', 'm', "", { "" } },
		{ 'N', EXP_CTRL_EN_TILDA, 'N', 'n', "", { "" } },
		{ 'O', EXP_CTRL_TILDA, 'O', 'o', "", { "" } },
		{ 'P', EXP_CTRL_EN_TILDA, 'P', 'p', "", { "" } },
		{ 'Q', EXP_CTRL_TILDA, 'Q', 'q', "", { "" } },
		{ 'R', EXP_CTRL_EN_TILDA, 'R', 'r', "", { "" } },
		{ 'S', EXP_CTRL_TILDA, 'S', 's', "", { "" } },
		{ 'T', EXP_CTRL_EN_TILDA, 'T', 't', "", { "" } },
		{ 'U', EXP_CTRL_EN_TILDA, 'U', 'u', "", { "" } },
		{ 'V', EXP_CTRL_TILDA, 'V', 'v', "", { "" } },
		{ 'W', EXP_CTRL_EN_TILDA, 'W', 'w', "", { "" } },
		{ 'X', EXP_CTRL_TILDA, 'X', 'x', "", { "" } },
		{ 'Y', EXP_CTRL_EN_TILDA, 'Y', 'y', "", { "" } },
		{ 'Z', EXP_CTRL_TILDA, 'Z', 'z', "", { "" } },
		//{ VK_OEM_4, EXP_CTRL_SH_TILDA, '[', '{', "", { "" } },
		//{ VK_OEM_5, EXP_NO, '\\', 0, "", { "", "\033\\", "\033|", "\033[92;5~", "\034", "\033[92;7~", "\033[92;8~" } },		
		{ VK_OEM_5, EXP_CTRL_SH_TILDA, '\\', '|', "", { "" } },
		{ VK_OEM_6, EXP_CTRL_SH_TILDA, ']', '}', "", { "" } },
		{ '6', EXP_CTRL_TILDA, '^', '6', "", { "" } },
		{ VK_OEM_MINUS, EXP_CTRL_TILDA, '_', '-', "", { "" } },

		{ VK_UP,	EXP_FORMAT_LETTER, 'A', 0, "\033[A", {} },
		{ VK_DOWN,	EXP_FORMAT_LETTER, 'B', 0, "\033[B", {} },
		{ VK_RIGHT, EXP_FORMAT_LETTER, 'C', 0, "\033[C", {} },
		{ VK_LEFT,	EXP_FORMAT_LETTER, 'D', 0, "\033[D", {} },

		{ VK_INSERT, EXP_FORMAT_LETTER, '@', 0, "\033[@", {} },
		{ VK_DELETE, EXP_FORMAT_TILDA, 0177, 0, "\177", {} },

		{ VK_HOME,	EXP_FORMAT_LETTER, 'H', 0, "\033[H", {} },
		{ VK_END,	EXP_FORMAT_LETTER, 'Y', 0, "\033[Y", {} },
		{ VK_PRIOR, EXP_FORMAT_LETTER, 'V', 0, "\033[V", {} },
		{ VK_NEXT,	EXP_FORMAT_LETTER, 'U', 0, "\033[U", {} },

		{ VK_CLEAR, EXP_FORMAT_LETTER, 'G', 0, "\033[G", {} },		//VK_NUMPAD5 when numlock off

		{ VK_F10, EXP_FORMAT_TILDA, 0260, 0, "\260", {} },
		{ VK_F1,  EXP_FORMAT_TILDA, 0261, 0, "\261", {} },
		{ VK_F2,  EXP_FORMAT_TILDA, 0262, 0, "\262", {} },
		{ VK_F3,  EXP_FORMAT_TILDA, 0263, 0, "\263", {} },
		{ VK_F4,  EXP_FORMAT_TILDA, 0264, 0, "\264", {} },
		{ VK_F5,  EXP_FORMAT_TILDA, 0265, 0, "\265", {} },
		{ VK_F6,  EXP_FORMAT_TILDA, 0266, 0, "\266", {} },
		{ VK_F7,  EXP_FORMAT_TILDA, 0267, 0, "\267", {} },
		{ VK_F8,  EXP_FORMAT_TILDA, 0270, 0, "\270", {} },
		{ VK_F9,  EXP_FORMAT_TILDA, 0271, 0, "\271", {} },
		{ VK_F11, EXP_FORMAT_TILDA, 0272, 0, "\272", {} },
		{ VK_F12, EXP_FORMAT_TILDA, 0273, 0, "\273", {} },

		//all mods of VK_SNAPSHOT is linked in windows to print screen 
		//^SysRQ
		{ VK_SNAPSHOT, EXP_FORMAT_TILDA, 0275, 0, "", { "", "", "", "\275", "\033[189;6~", "\033[189;7~", "\033[189;8~" } }, 
		
		//	pure, shift, alt, shift+alt, ctrl, shift+ctrl, alt+ctrl, shift+alt+ctrl
		//'^S' '^Q'
		{ VK_PAUSE, EXP_NO, 0276, 0, "\023", { "\021", "\032", "\033[190;4~", "", "", "", "" } }, 
		//'^C' '^\\'; 
		{ VK_CANCEL, EXP_NO, 0276, 0, "", { "", "", "", "\003", "\032", "\034", "\033[190;8~" } },
	};

	//
	for( unsigned num= 0; num < sizeof(keytable_init)/sizeof(t_keytable_init); ++num){
		init_keytable_row( keytable, &keytable_init[num] );
	}

	//
	//(keytable[VK_RETURN].vk_str[KEY_MOD_CTRL].data())[0]= 10;
	//keytable[VK_RETURN].vk_str_len[KEY_MOD_CTRL]= 1;

	//alt+keypad is windows reserved "compose keys"
	//alt+shift+keypad check in WM_CHAR
	keytable[VK_UP].vk_str_len[KEY_MOD_ALT]= 0; 
	keytable[VK_DOWN].vk_str_len[KEY_MOD_ALT]= 0; 
	keytable[VK_RIGHT].vk_str_len[KEY_MOD_ALT]= 0; 
	keytable[VK_LEFT].vk_str_len[KEY_MOD_ALT]= 0; 

	keytable[VK_HOME].vk_str_len[KEY_MOD_ALT]= 0; 
	keytable[VK_END].vk_str_len[KEY_MOD_ALT]= 0; 
	keytable[VK_PRIOR].vk_str_len[KEY_MOD_ALT]= 0; 
	keytable[VK_NEXT].vk_str_len[KEY_MOD_ALT]= 0; 
	
	keytable[VK_CLEAR].vk_str_len[KEY_MOD_ALT]= 0; 
	keytable[VK_INSERT].vk_str_len[KEY_MOD_ALT]= 0; 

	//
	//create keymap file name
	{ WCHAR *keymap_path= _wgetenv(KEYMAP_PATH_ENV); //path
	while(keymap_path){ //if path exist
		//create full fname
		enum { STR_SZ = 2*MAX_PATH };
		WCHAR buf[STR_SZ+16];
		_snwprintf(buf, STR_SZ, L"%s\\%s", keymap_path, KEYMAP_FNAME); buf[STR_SZ]= 0;
		keymap_fname = static_cast<WCHAR*>(malloc( (wcslen(buf)+1)*sizeof(WCHAR) ));
		wcscpy(keymap_fname,buf);
		break;
	}}

}

void load_keymap( HWND hwnd, t_keytable _Inout_ *keytable, char is_explicit_load ){
	assert(keytable);

	if(!keymap_fname){
		{ MessageBoxA(hwnd,"Can not use keymap file","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ }
		return; 
	}
	
	FILE *fi = _wfopen(keymap_fname, L"rb"); 
	if(!fi){
		if(is_explicit_load){ 
			MessageBoxA(hwnd,"Can not read keymap file","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ 
		}
		return; 
	}

	//read lines
	for(;;){

		unsigned key;
		if( fscanf(fi, " %u", &key) != 1 ) { 
			if( feof(fi) )break;
			MessageBoxA(hwnd,"Can not read VK_KEY","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ break; 
		}
		if( key > 255 ) { MessageBoxA(hwnd,"VK_KEY > 255","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ break;}

		char buf[KEYMAP_STR_SZ+16];

		for( unsigned mod= 0; mod < KEY_MODS; ++mod)
		{
			//key mod: size 
			unsigned len;
			if( fscanf(fi, " , %u", &len) != 1 ) { MessageBoxA(hwnd,"Can not read size","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ goto break_load; }

			keytable[key].vk_str_len[mod] = len;
			if(!len) continue;

			//key mod: str
			if( fgetc(fi) != ' '){ MessageBoxA(hwnd,"Can not read str","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ goto break_load; }
			for( unsigned i= 0; i < len; ++i){
				if( feof(fi) ){ MessageBoxA(hwnd,"Can not read str","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ goto break_load; }
				buf[i] = fgetc(fi);
			}

			keytable[key].vk_str[mod].assign( len, 0 );
			memcpy(keytable[key].vk_str[mod].data(), buf, len );
		}

		//eol
		if( fscanf(fi, " %1[;]", buf ) != 1 ) { MessageBoxA(hwnd,"Can not read eol","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ break; }
	}
	break_load:

	fclose (fi);
}

void save_keymap( HWND hwnd, t_keytable const _In_ *keytable ){
	assert(keytable);

	if(!keymap_fname){
		{ MessageBoxA(hwnd,"Can not use keymap file","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ }
		return; 
	}
	
	FILE *fo = _wfopen(keymap_fname, L"wb"); 
	if(!fo){
		{ MessageBoxA(hwnd,"Can not write keymap file","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ }
		return; 
	}

	for(unsigned key=0; key<VK_KEYS; ++key){ 

		if( keytable[key].is_empty_record() )continue;

		//key:
		if( fprintf(fo,"%u", key ) <= 0 )
			{ MessageBoxA(hwnd,"Can not write keymap","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ }
		
		char buf[KEYMAP_STR_SZ+16];

		for( unsigned mod= 0; mod < KEY_MODS; ++mod)
		{
			//key mod: size 
			unsigned len = keytable[key].vk_str_len[mod];
			
			if( fprintf(fo," , %u", len) <= 0 )
				{ MessageBoxA(hwnd,"Can not write keymap","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ }
			if(!len) continue;

			//key mod: str
			memcpy(buf, keytable[key].vk_str[mod].data(), KEYMAP_STR_SZ); buf[len]=0;

			if( fprintf(fo," %s", buf) <= 0 )
				{ MessageBoxA(hwnd,"Can not write keymap","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ }
		}

		//eol
		if( fprintf(fo," ;\n")<= 0 )
			{ MessageBoxA(hwnd,"Can not write keymap","Error",MB_OK|MB_ICONERROR); /* exit(-1); */ }
	}

	fclose(fo);
}
