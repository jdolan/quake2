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

QUIT MENU

=======================================================================
*/
static menuframework_s	m_quitMenu;

const char *M_Quit_Key (menuframework_s *self, int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_MOUSE2:
	case 'n':
	case 'N':
		M_PopMenu ();
		break;

	case 'Y':
	case 'y':
		cls.key_dest = key_console;
		CL_Quit_f ();
		break;

	default:
		break;
	}

	return NULL;

}


void M_Quit_Draw (menuframework_s *self)
{
	int		w, h;

	Draw_GetPicSize (&w, &h, "quit");
	Draw_Pic ( (viddef.width-w)/2, (viddef.height-h)/2, "quit", 1 );
}

void QuitMenu_Init( void ) {
	memset( &m_quitMenu, 0, sizeof( m_quitMenu ) );

	m_quitMenu.draw = M_Quit_Draw;
	m_quitMenu.key = M_Quit_Key;
}

void M_Menu_Quit_f (void)
{
	QuitMenu_Init();
	M_PushMenu ( &m_quitMenu );
}

