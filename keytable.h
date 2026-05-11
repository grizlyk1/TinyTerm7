//
// tinyTerm7 -- A minimal Windows terminal emulator

#pragma once

#include "stdafx.h"
#include "resource.h"

//key modifier bits
enum { KEY_MOD_PURE= 0, KEY_MOD_SHIFT= 1, KEY_MOD_ALT= 2, KEY_MOD_CTRL= 4, KEY_MODS = 8 };
//	pure, shift, alt, shift+alt, ctrl, shift+ctrl, alt+ctrl, shift+alt+ctrl
//compiled keytable 
struct t_keytable{
	//char 		*vk_str[KEY_MODS];
	Tvector_char	vk_str[KEY_MODS];
	unsigned 		vk_str_len[KEY_MODS];

	//return TRUE if empty key record
	BOOL	is_empty_record()const { for(unsigned mod= 0; mod < KEY_MODS; ++mod){ if(vk_str_len[mod])return FALSE; } return TRUE; }
};

enum { VK_KEYS= 256 };
//current config of tiny_keytable
extern t_keytable tiny_keytable[VK_KEYS];

//create current config of tiny_keytable
void init_keytable( t_keytable _Inout_ *keytable );

//save non zero key records from tiny_keytable
void save_keymap( HWND hwnd, t_keytable const _In_ *keytable );
//load non zero key records to tiny_keytable
void load_keymap( HWND hwnd, t_keytable _Inout_ *keytable, char is_explicit_load );
