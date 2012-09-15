/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "ui_local.h"

#define	MAX_MENU_DEPTH	8

static menuframework_s	*m_layers[MAX_MENU_DEPTH];
static int	m_menudepth;
static menuframework_s *m_active;

int m_mouse[2] = {0, 0};
int m_mouseold[2] = {0, 0};

qboolean	m_entersound;		// play after drawing a frame, so caching
								// won't disrupt the sound

void M_Banner( char *name )
{
	int w, h;

	Draw_GetPicSize (&w, &h, name );
	Draw_Pic( viddef.width / 2 - w / 2, viddef.height / 2 - 110, name, 1 );
}

void M_PushMenu ( menuframework_s *menu )
{
	int		i;

	if (Cvar_VariableIntValue ("maxclients") == 1 
		&& Com_ServerState () && !cl_paused->integer)
		Cvar_Set ("paused", "1");

	// if this menu is already present, drop back to that level
	// to avoid stacking menus by hotkeys
	for( i=0 ; i<m_menudepth ; i++ ) {
		if( m_layers[i] == menu ) {
			break;
		}
	}

	if (i == m_menudepth)
	{
		if (m_menudepth >= MAX_MENU_DEPTH)
			Com_Error (ERR_FATAL, "M_PushMenu: MAX_MENU_DEPTH");
		m_layers[m_menudepth++] = menu;
	}
	else {
		m_menudepth = i+1;
	}

	m_active = menu;

	m_entersound = true;

	cls.key_dest = key_menu;
}

void M_ForceMenuOff (void)
{
	cls.key_dest = key_game;
	m_menudepth = 0;
	m_active = NULL;
	Key_ClearStates ();
	if(cl_paused->integer)
		Cvar_Set ("paused", "0");
}

void M_PopMenu (void)
{
	if (m_menudepth < 1)
		Com_Error (ERR_FATAL, "M_PopMenu: depth < 1");

	if( !m_entersound )
		S_StartLocalSound( menu_out_sound );

	m_menudepth--;

	if (!m_menudepth) {
		M_ForceMenuOff ();
		return;
	}

	m_active = m_layers[m_menudepth - 1];
}
int Menu_ClickHit ( menuframework_s *menu, int x, int y );
qboolean bselected = false;
const char *Default_MenuKey( menuframework_s *m, int key )
{
	const char *sound = NULL;
	menucommon_s *item = NULL;
	int index;

	if ( m )
	{
		if (key == K_MOUSE1) {
			index = Menu_ClickHit(m, m_mouse[0], m_mouse[1]);
			if( index != -1 && m_active->cursor != index) {
				m_active->cursor = index;
			}
		}

		if ( ( item = Menu_ItemAtCursor( m ) ) != 0 )
		{
			if ( item->type == MTYPE_FIELD )
			{
				if ( Field_Key( ( menufield_s * ) item, key ) )
					return NULL;
			}
			else if ( item->type == MTYPE_LIST )
			{
				if ( List_Key( ( menulist_s * ) item, key ) )
					return NULL;
			}
		}
	}

	// Little hack
	if( item && (item->type == MTYPE_SLIDER || item->type == MTYPE_SPINCONTROL) ) {
		if( key == K_MOUSE1 ) {
			key = K_RIGHTARROW;
		} else if( key == K_MOUSE2 ) {
			key = K_LEFTARROW;
		}
	}

	switch ( key )
	{
	case K_MOUSE2:
	case K_ESCAPE:
		M_PopMenu();
		return menu_out_sound;
	case K_KP_UPARROW:
	case K_UPARROW:
		if ( m )
		{
			m->cursor--;
			Menu_AdjustCursor( m, -1 );
			sound = menu_move_sound;
		}
		break;
	case K_TAB:
		if ( m )
		{
			m->cursor++;
			Menu_AdjustCursor( m, 1 );
			sound = menu_move_sound;
		}
		break;
	case K_KP_DOWNARROW:
	case K_DOWNARROW:
		if ( m )
		{
			m->cursor++;
			Menu_AdjustCursor( m, 1 );
			sound = menu_move_sound;
		}
		break;
	case K_KP_LEFTARROW:
	case K_LEFTARROW:
		if ( m )
		{
			Menu_SlideItem( m, -1 );
			sound = menu_move_sound;
		}
		break;
	case K_KP_RIGHTARROW:
	case K_RIGHTARROW:
		if ( m )
		{
			Menu_SlideItem( m, 1 );
			sound = menu_move_sound;
		}
		break;

	case K_MOUSE1:
	//case K_MOUSE2:
	case K_MOUSE3:
#ifdef JOYSTICK
	case K_JOY1:
	case K_JOY2:
	case K_JOY3:
	case K_JOY4:
	case K_AUX1:
	case K_AUX2:
	case K_AUX3:
	case K_AUX4:
	case K_AUX5:
	case K_AUX6:
	case K_AUX7:
	case K_AUX8:
	case K_AUX9:
	case K_AUX10:
	case K_AUX11:
	case K_AUX12:
	case K_AUX13:
	case K_AUX14:
	case K_AUX15:
	case K_AUX16:
	case K_AUX17:
	case K_AUX18:
	case K_AUX19:
	case K_AUX20:
	case K_AUX21:
	case K_AUX22:
	case K_AUX23:
	case K_AUX24:
	case K_AUX25:
	case K_AUX26:
	case K_AUX27:
	case K_AUX28:
	case K_AUX29:
	case K_AUX30:
	case K_AUX31:
	case K_AUX32:
#endif
		
	case K_KP_ENTER:
	case K_ENTER:
		if ( m )
			Menu_SelectItem( m );
		sound = menu_move_sound;
		break;
	}

	return sound;
}

//=============================================================================

/*
================
M_DrawCharacter

Draws one solid graphics character
cx and cy are in 320*240 coordinates, and will be centered on
higher res screens.
================
*/
void M_DrawCharacter (int cx, int cy, int num)
{
	Draw_Char ( cx + ((viddef.width - 320)>>1), cy + ((viddef.height - 240)>>1), num, COLOR_WHITE, 1);
}

void M_Print (int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawCharacter (cx, cy, (*str)+128);
		str++;
		cx += 8;
	}
}

void M_PrintWhite (int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawCharacter (cx, cy, *str);
		str++;
		cx += 8;
	}
}

void M_DrawPic (int x, int y, char *pic)
{
	Draw_Pic (x + ((viddef.width - 320)>>1), y + ((viddef.height - 240)>>1), pic, 1);
}


/*
=============
M_DrawCursor

Draws an animating cursor with the point at
x,y.  The pic will extend to the left of x,
and both above and below y.
=============
*/
#define NUM_CURSOR_FRAMES 15
void M_DrawCursor( int x, int y, int f )
{
	char	cursorname[80];
	static qboolean cached;

	if ( !cached )
	{
		int i;

		for ( i = 0; i < NUM_CURSOR_FRAMES; i++ )
		{
			Com_sprintf( cursorname, sizeof( cursorname ), "m_cursor%d", i );

			Draw_FindPic( cursorname );
		}
		cached = true;
	}

	Com_sprintf( cursorname, sizeof(cursorname), "m_cursor%d", f );
	Draw_Pic( x, y, cursorname, 1 );
}

void M_DrawTextBox (int x, int y, int width, int lines)
{
	int		cx, cy;
	int		n;

	// draw left side
	cx = x;
	cy = y;
	M_DrawCharacter (cx, cy, 1);
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawCharacter (cx, cy, 4);
	}
	M_DrawCharacter (cx, cy+8, 7);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		M_DrawCharacter (cx, cy, 2);
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			M_DrawCharacter (cx, cy, 5);
		}
		M_DrawCharacter (cx, cy+8, 8);
		width -= 1;
		cx += 8;
	}

	// draw right side
	cy = y;
	M_DrawCharacter (cx, cy, 3);
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawCharacter (cx, cy, 6);
	}
	M_DrawCharacter (cx, cy+8, 9);
}

//=============================================================================
/* Menu Subsystem */


/*
=================
M_Init
=================
*/
void M_Init (void)
{
	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
	Cmd_AddCommand ("menu_game", M_Menu_Game_f);
		Cmd_AddCommand ("menu_loadgame", M_Menu_LoadGame_f);
		Cmd_AddCommand ("menu_savegame", M_Menu_SaveGame_f);
		Cmd_AddCommand ("menu_joinserver", M_Menu_JoinServer_f);
			Cmd_AddCommand ("menu_addressbook", M_Menu_AddressBook_f);
		Cmd_AddCommand ("menu_startserver", M_Menu_StartServer_f);
			Cmd_AddCommand ("menu_dmoptions", M_Menu_DMOptions_f);
		Cmd_AddCommand ("menu_playerconfig", M_Menu_PlayerConfig_f);
			Cmd_AddCommand ("menu_downloadoptions", M_Menu_DownloadOptions_f);
		Cmd_AddCommand ("menu_credits", M_Menu_Credits_f );
	Cmd_AddCommand ("menu_multiplayer", M_Menu_Multiplayer_f );
	Cmd_AddCommand ("menu_video", M_Menu_Video_f);
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
		Cmd_AddCommand ("menu_newoptions", M_Menu_NewOptions_f);
		Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);
	Cmd_AddCommand ("menu_demos", M_Menu_Demos_f);
#if defined(_WIN32) || defined(WITH_XMMS)
	Cmd_AddCommand ("menu_" MP3_PLAYERNAME_NOCAPS, M_Menu_MP3_f);
#endif
}

void List_MoveB ( menulist_s *l, int moy, int my);

void M_MouseMove( int mx, int my )
{
	menucommon_s *item = NULL;

	m_mouse[0] += mx;
	m_mouse[1] += my;

	clamp( m_mouse[0], 0, viddef.width - 8 );
	clamp( m_mouse[1], 0, viddef.height - 8 );

	if(bselected) {
		if (my) {
			if((item = Menu_ItemAtCursor( m_active ) ) != 0)
				List_MoveB( (menulist_s * ) item, m_mouse[1], my);
		}
	}
}

/*
=================
M_Draw
=================
*/
void M_Draw (void)
{
	extern cvar_t *gl_scale;
	int index;
	static int prev;

	if (cls.key_dest != key_menu)
		return;

	if( !m_active )
		return;

	// repaint everything next frame
	SCR_DirtyScreen ();

	// dim everything behind it down
	if (cl.cinematictime > 0)
		Draw_Fill (0,0,viddef.width, viddef.height, 0);
	else
		Draw_FadeScreen ();

	if(!bselected) {
		index = Menu_HitTest( m_active, m_mouse[0], m_mouse[1] );
		if( prev != index ) {
			if( index != -1 ) {
				m_active->cursor = index;
			}
		}
		prev = index;
	}

	if( m_active->draw )
		m_active->draw( m_active );
	else
		Menu_Draw( m_active );

	Draw_ScaledPic(m_mouse[0], m_mouse[1], gl_scale->value, "ch1", 1, 1, 1, 1);

	// delay playing the enter sound until after the
	// menu has been drawn, to avoid delay while
	// caching images
	if (m_entersound)
	{
		S_StartLocalSound( menu_in_sound );
		m_entersound = false;
	}
}

/*
=================
M_Keydown
=================
*/
void M_Keydown (int key, qboolean down)
{
	const char *s;

	if( !m_active )
		return;

	if(key == K_MOUSE1 && !down && bselected)
		bselected = false;

	if(!down || bselected)
		return;

	if( m_active->key )
		s = m_active->key( m_active, key );
	else
		s = Default_MenuKey( m_active, key );

	if( s )
		S_StartLocalSound( s );
}

