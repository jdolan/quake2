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

MULTIPLAYER MENU

=======================================================================
*/
menuframework_s	s_multiplayer_menu;
static menuaction_s		s_join_network_server_action;
static menuaction_s		s_start_network_server_action;
static menuaction_s		s_player_setup_action;

static void Multiplayer_MenuDraw ( menuframework_s *self )
{
	M_Banner( "m_banner_multiplayer" );

	Menu_AdjustCursor( self, 1 );
	Menu_Draw( self );
}

static void PlayerSetupFunc( void *unused )
{
	M_Menu_PlayerConfig_f();
}

static void JoinNetworkServerFunc( void *unused )
{
	M_Menu_JoinServer_f();
}

static void StartNetworkServerFunc( void *unused )
{
	M_Menu_StartServer_f ();
}

void Multiplayer_MenuInit( void )
{
	memset(&s_multiplayer_menu, 0, sizeof(s_multiplayer_menu));
	s_multiplayer_menu.x = viddef.width * 0.50 - 64;
	s_multiplayer_menu.nitems = 0;

	s_join_network_server_action.generic.type	= MTYPE_ACTION;
	s_join_network_server_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_join_network_server_action.generic.x		= 0;
	s_join_network_server_action.generic.y		= 0;
	s_join_network_server_action.generic.name	= " join network server";
	s_join_network_server_action.generic.callback = JoinNetworkServerFunc;

	s_start_network_server_action.generic.type	= MTYPE_ACTION;
	s_start_network_server_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_start_network_server_action.generic.x		= 0;
	s_start_network_server_action.generic.y		= 10;
	s_start_network_server_action.generic.name	= " start network server";
	s_start_network_server_action.generic.callback = StartNetworkServerFunc;

	s_player_setup_action.generic.type	= MTYPE_ACTION;
	s_player_setup_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_player_setup_action.generic.x		= 0;
	s_player_setup_action.generic.y		= 20;
	s_player_setup_action.generic.name	= " player setup";
	s_player_setup_action.generic.callback = PlayerSetupFunc;

	s_multiplayer_menu.draw = Multiplayer_MenuDraw;
	s_multiplayer_menu.key = NULL;

	Menu_AddItem( &s_multiplayer_menu, ( void * ) &s_join_network_server_action );
	Menu_AddItem( &s_multiplayer_menu, ( void * ) &s_start_network_server_action );
	Menu_AddItem( &s_multiplayer_menu, ( void * ) &s_player_setup_action );

	Menu_SetStatusBar( &s_multiplayer_menu, NULL );

	Menu_Center( &s_multiplayer_menu );
}


void M_Menu_Multiplayer_f( void )
{
	Multiplayer_MenuInit();
	M_PushMenu( &s_multiplayer_menu );
}
