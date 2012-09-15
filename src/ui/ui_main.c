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

/*
=======================================================================

MAIN MENU

=======================================================================
*/
#define	MAIN_ITEMS	5
#define NUM_CURSOR_FRAMES 15
//static int	m_main_cursor;

//static menuframework_s m_main;

static char *names[] =
{
	"m_main_game",
	"m_main_multiplayer",
	"m_main_options",
	"m_main_video",
	"m_main_quit",
	0
};

typedef struct mainMenu_s {
	menuframework_s	menu;
	menubitmap_s bitmaps[MAIN_ITEMS];
} mainMenu_t;

static mainMenu_t	m_main;

void M_Main_Draw (menuframework_s *self)
{
	int i;
	int w, h;
	int ystart;
	int	xoffset;
	int widest = -1;

	for ( i = 0; names[i] != 0; i++ )
	{
		Draw_GetPicSize( &w, &h, names[i] );

		if ( w > widest )
			widest = w;
	}

	ystart = ( viddef.height / 2 - 110 );
	xoffset = ( viddef.width - widest + 70 ) / 2;


	Draw_GetPicSize( &w, &h, "m_main_plaque" );
	Draw_Pic( xoffset - 30 - w, ystart, "m_main_plaque", 1 );

	Draw_Pic( xoffset - 30 - w, ystart + h + 5, "m_main_logo", 1 );

	Menu_Draw( self );
}

static void MainMenu_CursorDraw( void *self )
{
	menubitmap_s *b;
	b = (menubitmap_s *)self;
	M_DrawCursor( b->generic.x - 25, b->generic.y - 2, (int)(cls.realtime / 100)%NUM_CURSOR_FRAMES );
}

static void MainMenu_Callback( void *self )
{
	int index;
	menubitmap_s *b;
	b = (menubitmap_s *)self;

	index = b - m_main.bitmaps;
	switch( index ) {
	case 0:
		M_Menu_Game_f();
		break;
	case 1:
		M_Menu_Multiplayer_f();
		break;
	case 2:
		M_Menu_Options_f();
		break;
	case 3:
		M_Menu_Video_f();
		break;
	case 4:
		M_Menu_Quit_f();
		break;
	}
}


void MainMenu_Init( void ) {
	int i;
	int w, h;
	int ystart;
	int	xoffset;
	int widest = -1;

	for( i=0 ; names[i] ; i++ ) {
		Draw_GetPicSize( &w, &h, names[i] );

		if( w > widest )
			widest = w;
	}

	ystart = (viddef.height / 2 - 110);
	xoffset = (viddef.width - widest + 70) / 2;

	memset( &m_main, 0, sizeof( m_main ) );

	for( i=0 ; i<MAIN_ITEMS ; i++ ) {
		Draw_GetPicSize( &w, &h, names[i] );

		m_main.bitmaps[i].generic.type = MTYPE_BITMAP;
		m_main.bitmaps[i].generic.name = names[i];
		m_main.bitmaps[i].generic.x = xoffset;
		m_main.bitmaps[i].generic.y = ystart + i * 40 + 13;
		m_main.bitmaps[i].generic.width = w;
		m_main.bitmaps[i].generic.height = h;
		m_main.bitmaps[i].generic.cursordraw = MainMenu_CursorDraw;
		m_main.bitmaps[i].generic.callback = MainMenu_Callback;
	
		Menu_AddItem( &m_main.menu, (void *)&m_main.bitmaps[i] );
	}

	m_main.menu.draw = M_Main_Draw;
	m_main.menu.key = NULL;

}

void M_Menu_Main_f (void)
{
	MainMenu_Init();
	M_PushMenu( &m_main.menu );
}
