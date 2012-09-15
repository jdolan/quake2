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
=============================================================================

ADDRESS BOOK MENU

=============================================================================
*/
static menuframework_s	s_addressbook_menu;
static menufield_s		s_addressbook_fields[MAX_LOCAL_SERVERS];

static const char *AddressBook_MenuKey( menuframework_s *self, int key )
{
	int index;
	char buffer[20];

	if ( key == K_ESCAPE )
	{
		for ( index = 0; index < MAX_LOCAL_SERVERS; index++ )
		{
			Com_sprintf( buffer, sizeof( buffer ), "adr%d", index );
			Cvar_Set( buffer, s_addressbook_fields[index].buffer );
		}
	}
	return Default_MenuKey( self, key );
}

static void AddressBook_MenuDraw(menuframework_s *self)
{
	M_Banner( "m_banner_addressbook" );
	Menu_Draw( self );
}

static void AddressBook_MenuInit( void )
{
	int i;

	memset(&s_addressbook_menu, 0, sizeof(s_addressbook_menu));

	s_addressbook_menu.x = viddef.width / 2 - 142;
	s_addressbook_menu.y = viddef.height / 2 - 58;
	s_addressbook_menu.nitems = 0;

	for ( i = 0; i < MAX_LOCAL_SERVERS; i++ )
	{
		cvar_t *adr;
		char buffer[20];

		Com_sprintf( buffer, sizeof( buffer ), "adr%d", i );

		adr = Cvar_Get( buffer, "", CVAR_ARCHIVE );

		s_addressbook_fields[i].generic.type = MTYPE_FIELD;
		s_addressbook_fields[i].generic.name = 0;
		s_addressbook_fields[i].generic.callback = 0;
		s_addressbook_fields[i].generic.x		= 0;
		s_addressbook_fields[i].generic.y		= i * 18 + 0;
		s_addressbook_fields[i].generic.localdata[0] = i;
		s_addressbook_fields[i].cursor			= 0;
		s_addressbook_fields[i].length			= 60;
		s_addressbook_fields[i].visible_length	= 30;

		strcpy( s_addressbook_fields[i].buffer, adr->string );

		Menu_AddItem( &s_addressbook_menu, &s_addressbook_fields[i] );
	}

	s_addressbook_menu.draw = AddressBook_MenuDraw;
	s_addressbook_menu.key = AddressBook_MenuKey;
}

void M_Menu_AddressBook_f(void)
{
	AddressBook_MenuInit();
	M_PushMenu( &s_addressbook_menu );
}

