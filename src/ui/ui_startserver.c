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
#ifdef _WIN32
#include <io.h>
#endif
/*
=============================================================================

START SERVER MENU

=============================================================================
*/
#define MAX_MENU_MAPS	128
static menuframework_s s_startserver_menu;
static char *mapnames[MAX_MENU_MAPS];
//static int	  nummaps;
static menulist_s	maplist;
static int			map_count = 0;

static menuaction_s	s_startserver_start_action;
static menuaction_s	s_startserver_dmoptions_action;
static menufield_s	s_timelimit_field;
static menufield_s	s_fraglimit_field;
static menufield_s	s_maxclients_field;
static menufield_s	s_hostname_field;
//static menulist_s	s_startmap_list;
static menulist_s	s_rules_box;

static void Maps_Free( void ) {
	int i;

	if(!map_count)
		return;

	for( i=0 ; i<map_count; i++ ) {
		Z_Free( mapnames[i] );
		mapnames[i] = NULL;
	}
	map_count = 0;
	maplist.itemnames = NULL;
}

static void Maps_Scan( void)
{
	int		numFiles;
	char	findname[1024];
	char	**list;
	int		i;

	Maps_Free();

	Com_sprintf(findname, sizeof(findname), "%s/maps/*.bsp", FS_Gamedir());
	list = FS_ListFiles( findname, &numFiles, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM );
	if( !list ) {
		return;
	}

	for( i = 0; i < numFiles - 1; i++ ) {
		if( map_count < MAX_MENU_MAPS ) {
			list[i][strlen(list[i]) - 4] = 0;
			if (strrchr( list[i], '/' ))
				mapnames[map_count] = CopyString( strrchr( list[i], '/' ) + 1, TAG_MENU);
			else
				mapnames[map_count] = CopyString( list[i], TAG_MENU);

			map_count++;
		}
		Z_Free( list[i] );
	}
	Z_Free( list );

}

static void Build_MapList(void)
{
	Maps_Scan();
	maplist.curvalue = 0;
	maplist.prestep = 0;
	maplist.itemnames = (const char **)mapnames;
	maplist.count = map_count;
}

void DMOptionsFunc( void *self )
{
	if (s_rules_box.curvalue == 1)
		return;
	M_Menu_DMOptions_f();
}

void RulesChangeFunc ( void *self )
{
	// DM
	if (s_rules_box.curvalue == 0)
	{
		s_maxclients_field.generic.statusbar = NULL;
		s_startserver_dmoptions_action.generic.statusbar = NULL;
	}
	else if(s_rules_box.curvalue == 1)		// coop				// PGM
	{
		s_maxclients_field.generic.statusbar = "4 maximum for cooperative";
		if (atoi(s_maxclients_field.buffer) > 4)
			strcpy( s_maxclients_field.buffer, "4" );
		s_startserver_dmoptions_action.generic.statusbar = "N/A for cooperative";
	}
//=====
//PGM
	// ROGUE GAMES
	else if(Developer_searchpath() == 2)
	{
		if (s_rules_box.curvalue == 2)			// tag	
		{
			s_maxclients_field.generic.statusbar = NULL;
			s_startserver_dmoptions_action.generic.statusbar = NULL;
		}
	}
//PGM
//=====
}

float ClampCvar( float min, float max, float value );

void StartServerActionFunc( void *self )
{
	char	startmap[1024];
	int		timelimit;
	int		fraglimit;
	int		maxclients;
	char	*spot;

//	strcpy( startmap, strchr( mapnames[maplist.curvalue], '\n' ) + 1 );
	strcpy( startmap, mapnames[maplist.curvalue]);

	maxclients  = atoi( s_maxclients_field.buffer );
	timelimit	= atoi( s_timelimit_field.buffer );
	fraglimit	= atoi( s_fraglimit_field.buffer );

	Cvar_SetLatched( "maxclients", va("%i", (int)ClampCvar( 0, maxclients, maxclients )) );
	Cvar_SetValue ("timelimit", ClampCvar( 0, timelimit, timelimit ) );
	Cvar_SetValue ("fraglimit", ClampCvar( 0, fraglimit, fraglimit ) );
	Cvar_Set("hostname", s_hostname_field.buffer );
//	Cvar_SetValue ("deathmatch", !s_rules_box.curvalue );
//	Cvar_SetValue ("coop", s_rules_box.curvalue );

//PGM
	if((s_rules_box.curvalue < 2) || (Developer_searchpath() != 2))
	{
		Cvar_SetLatched ("deathmatch", s_rules_box.curvalue ? "0" : "1");
		Cvar_SetLatched ("coop", s_rules_box.curvalue ? "1" : "0" );
		Cvar_SetValue ("gamerules", 0 );
	}
	else
	{
		Cvar_SetLatched ("deathmatch", "1");	// deathmatch is always true for rogue games, right?
		Cvar_SetLatched ("coop", "0");			// FIXME - this might need to depend on which game we're running
		Cvar_SetValue ("gamerules", s_rules_box.curvalue);
	}
//PGM

	spot = NULL;
	if (s_rules_box.curvalue == 1)		// PGM
	{
 		if(Q_stricmp(startmap, "bunk1") == 0)
  			spot = "start";
 		else if(Q_stricmp(startmap, "mintro") == 0)
  			spot = "start";
 		else if(Q_stricmp(startmap, "fact1") == 0)
  			spot = "start";
 		else if(Q_stricmp(startmap, "power1") == 0)
  			spot = "pstart";
 		else if(Q_stricmp(startmap, "biggun") == 0)
  			spot = "bstart";
 		else if(Q_stricmp(startmap, "hangar1") == 0)
  			spot = "unitstart";
 		else if(Q_stricmp(startmap, "city1") == 0)
  			spot = "unitstart";
 		else if(Q_stricmp(startmap, "boss1") == 0)
			spot = "bosstart";
	}

	if (spot)
	{
		if (Com_ServerState())
			Cbuf_AddText ("disconnect\n");
		Cbuf_AddText (va("gamemap \"*%s$%s\"\n", startmap, spot));
	}
	else
	{
		Cbuf_AddText (va("map %s\n", startmap));
	}

	M_ForceMenuOff ();
}

const char *StartServer_MenuKey( menuframework_s *self, int key )
{
	if ( key == K_ESCAPE )
	{
		Maps_Free();
	}

	return Default_MenuKey( self, key );
}

void StartServer_MenuInit( void )
{
	static const char *dm_coop_names[] =
	{
		"deathmatch",
		"cooperative",
		0
	};
//=======
//PGM
	static const char *dm_coop_names_rogue[] =
	{
		"deathmatch",
		"cooperative",
		"tag",
//		"deathball",
		0
	};

	int x = 0, y = 0;

	memset(&s_startserver_menu, 0, sizeof(s_startserver_menu));
	// initialize the menu stuff
	//s_startserver_menu.x = viddef.width * 0.50;
	s_startserver_menu.x = 0;
	s_startserver_menu.y = 0;
	s_startserver_menu.nitems = 0;

	/*s_startmap_list.generic.type = MTYPE_SPINCONTROL;
	s_startmap_list.generic.x	= 0;
	s_startmap_list.generic.y	= 0;
	s_startmap_list.generic.name	= "initial map";
	s_startmap_list.itemnames = (const char **)mapnames;*/

	maplist.generic.type		= MTYPE_LIST;
	maplist.generic.flags		= QMF_LEFT_JUSTIFY;
	maplist.generic.name		= NULL;
	maplist.generic.callback	= NULL;
	maplist.generic.x			= 20;
	maplist.generic.y			= 30;
	//maplist.width				= viddef.width - 40;
	maplist.width				= (viddef.width - 40) / 2;
	maplist.height				= viddef.height - 110;
	MenuList_Init(&maplist);

	x = maplist.width + 20 + ((viddef.width - maplist.width - 20)/ 2);
	y = 30;

	Build_MapList();

	s_rules_box.generic.type = MTYPE_SPINCONTROL;
	s_rules_box.generic.flags	= QMF_LEFT_JUSTIFY;
	s_rules_box.generic.cursor_offset = 24;
	s_rules_box.generic.x	= x;
	s_rules_box.generic.y	= y+20;
	s_rules_box.generic.name	= "rules";
	
//PGM - rogue games only available with rogue DLL.
	if(Developer_searchpath() == 2)
		s_rules_box.itemnames = dm_coop_names_rogue;
	else
		s_rules_box.itemnames = dm_coop_names;
//PGM

	if (Cvar_VariableIntValue("coop"))
		s_rules_box.curvalue = 1;
	else
		s_rules_box.curvalue = 0;
	s_rules_box.generic.callback = RulesChangeFunc;

	s_timelimit_field.generic.type = MTYPE_FIELD;
	s_timelimit_field.generic.name = "time limit";
	s_timelimit_field.generic.flags = QMF_NUMBERSONLY;
	s_timelimit_field.generic.x	= x;
	s_timelimit_field.generic.y	= y+36;
	s_timelimit_field.generic.statusbar = "0 = no limit";
	s_timelimit_field.length = 3;
	s_timelimit_field.visible_length = 3;
	strcpy( s_timelimit_field.buffer, Cvar_VariableString("timelimit") );

	s_fraglimit_field.generic.type = MTYPE_FIELD;
	s_fraglimit_field.generic.name = "frag limit";
	s_fraglimit_field.generic.flags = QMF_NUMBERSONLY;
	s_fraglimit_field.generic.x	= x;
	s_fraglimit_field.generic.y	= y+54;
	s_fraglimit_field.generic.statusbar = "0 = no limit";
	s_fraglimit_field.length = 3;
	s_fraglimit_field.visible_length = 3;
	strcpy( s_fraglimit_field.buffer, Cvar_VariableString("fraglimit") );

	/*
	** maxclients determines the maximum number of players that can join
	** the game.  If maxclients is only "1" then we should default the menu
	** option to 8 players, otherwise use whatever its current value is. 
	** Clamping will be done when the server is actually started.
	*/
	s_maxclients_field.generic.type = MTYPE_FIELD;
	s_maxclients_field.generic.name = "max players";
	s_maxclients_field.generic.flags = QMF_NUMBERSONLY;
	s_maxclients_field.generic.x	= x;
	s_maxclients_field.generic.y	= y+72;
	s_maxclients_field.generic.statusbar = NULL;
	s_maxclients_field.length = 3;
	s_maxclients_field.visible_length = 3;
	if (Cvar_VariableIntValue( "maxclients" ) == 1)
		strcpy( s_maxclients_field.buffer, "8" );
	else 
		strcpy( s_maxclients_field.buffer, Cvar_VariableString("maxclients") );

	s_hostname_field.generic.type = MTYPE_FIELD;
	s_hostname_field.generic.name = "hostname";
	s_hostname_field.generic.flags = 0;
	s_hostname_field.generic.x	= x;
	s_hostname_field.generic.y	= y+90;
	s_hostname_field.generic.statusbar = NULL;
	s_hostname_field.length = 12;
	s_hostname_field.visible_length = 12;
	strcpy( s_hostname_field.buffer, Cvar_VariableString("hostname") );

	s_startserver_dmoptions_action.generic.type = MTYPE_ACTION;
	s_startserver_dmoptions_action.generic.name	= " deathmatch flags";
	s_startserver_dmoptions_action.generic.flags= QMF_LEFT_JUSTIFY;
	s_startserver_dmoptions_action.generic.x	= x+24;
	s_startserver_dmoptions_action.generic.y	= y+108;
	s_startserver_dmoptions_action.generic.statusbar = NULL;
	s_startserver_dmoptions_action.generic.callback = DMOptionsFunc;

	s_startserver_start_action.generic.type = MTYPE_ACTION;
	s_startserver_start_action.generic.name	= " begin";
	s_startserver_start_action.generic.flags= QMF_LEFT_JUSTIFY;
	s_startserver_start_action.generic.x	= x+24;
	s_startserver_start_action.generic.y	= y+128;
	s_startserver_start_action.generic.callback = StartServerActionFunc;

	s_startserver_menu.draw = NULL;
	s_startserver_menu.key = StartServer_MenuKey;

//	Menu_AddItem( &s_startserver_menu, &s_startmap_list );
	Menu_AddItem( &s_startserver_menu, &maplist );
	Menu_AddItem( &s_startserver_menu, &s_rules_box );
	Menu_AddItem( &s_startserver_menu, &s_timelimit_field );
	Menu_AddItem( &s_startserver_menu, &s_fraglimit_field );
	Menu_AddItem( &s_startserver_menu, &s_maxclients_field );
	Menu_AddItem( &s_startserver_menu, &s_hostname_field );
	Menu_AddItem( &s_startserver_menu, &s_startserver_dmoptions_action );
	Menu_AddItem( &s_startserver_menu, &s_startserver_start_action );

	//Menu_Center( &s_startserver_menu );

	// call this now to set proper inital state
	RulesChangeFunc ( NULL );
}

void M_Menu_StartServer_f (void)
{
	StartServer_MenuInit();
	M_PushMenu( &s_startserver_menu );
}

