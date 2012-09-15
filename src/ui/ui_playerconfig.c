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

PLAYER CONFIG MENU

=============================================================================
*/
static menuframework_s	s_player_config_menu;
static menufield_s		s_player_name_field;
static menulist_s		s_player_model_box;
static menulist_s		s_player_skin_box;
static menulist_s		s_player_handedness_box;
static menulist_s		s_player_rate_box;
static menuseparator_s	s_player_skin_title;
static menuseparator_s	s_player_model_title;
static menuseparator_s	s_player_hand_title;
static menuseparator_s	s_player_rate_title;
static menuaction_s		s_player_download_action;

#define MAX_DISPLAYNAME 16
#define MAX_PLAYERMODELS 1024

typedef struct
{
	int		nskins;
	char	**skindisplaynames;
	char	displayname[MAX_DISPLAYNAME];
	char	directory[MAX_QPATH];
} playermodelinfo_s;

static playermodelinfo_s s_pmi[MAX_PLAYERMODELS];
static char *s_pmnames[MAX_PLAYERMODELS];
static int s_numplayermodels;

static int rate_tbl[] = { 2500, 3200, 5000, 10000, 25000, 0 };
static const char *rate_names[] = { "28.8 Modem", "33.6 Modem", "Single ISDN",
	"Dual ISDN/Cable", "T1/LAN", "User defined", 0 };

void DownloadOptionsFunc( void *self )
{
	M_Menu_DownloadOptions_f();
}

static void HandednessCallback( void *unused )
{
	Cvar_SetValue( "hand", s_player_handedness_box.curvalue );
}

static void RateCallback( void *unused )
{
	if (s_player_rate_box.curvalue != sizeof(rate_tbl) / sizeof(*rate_tbl) - 1)
		Cvar_SetValue( "rate", rate_tbl[s_player_rate_box.curvalue] );
}

static void ModelCallback( void *unused )
{
	s_player_skin_box.itemnames = (const char **)s_pmi[s_player_model_box.curvalue].skindisplaynames;
	s_player_skin_box.curvalue = 0;
}

static void FreeFileList( char **list, int n )
{
	int i;

	for ( i = 0; i < n; i++ )
	{
		if ( list[i] )
		{
			Z_Free( list[i] );
			list[i] = NULL;
		}
	}
	Z_Free( list );
}

static qboolean IconOfSkinExists( const char *skin, char **pcxfiles, int npcxfiles )
{
	int i;
	char scratch[1024];

	Q_strncpyz( scratch, skin, sizeof(scratch) );
	*strrchr( scratch, '.' ) = 0;
	Q_strncatz( scratch, "_i.pcx", sizeof(scratch) );

	for ( i = 0; i < npcxfiles; i++ )
	{
		if ( strcmp( pcxfiles[i], scratch ) == 0 )
			return true;
	}

	return false;
}

static qboolean PlayerConfig_ScanDirectories( void )
{
	char findname[1024];
	char scratch[1024];
	int ndirs = 0, npms = 0;
	char **dirnames = NULL;
	char *path = NULL;
	int i;

	s_numplayermodels = 0;

	/*
	** get a list of directories
	*/
	do 
	{
		path = FS_NextPath( path );

		if (path == NULL)
			break;

		Com_sprintf( findname, sizeof(findname), "%s/players/*.*", path );

		if ( ( dirnames = FS_ListFiles( findname, &ndirs, SFF_SUBDIR, 0 ) ) != 0 )
			break;
	} while ( path );

	if ( !dirnames )
		return false;

	/*
	** go through the subdirectories
	*/
	npms = ndirs;
	if ( npms > MAX_PLAYERMODELS )
		npms = MAX_PLAYERMODELS;

	for ( i = 0; i < npms; i++ )
	{
		int k, s;
		char *a, *b, *c;
		char **pcxnames;
		char **skinnames;
		int npcxfiles;
		int nskins = 0;

		if ( dirnames[i] == 0 )
			continue;

		// verify the existence of tris.md2
		strcpy( scratch, dirnames[i] );
		strcat( scratch, "/tris.md2" );
		if ( !Sys_FindFirst( scratch, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM ) )
		{
			Sys_FindClose();
			continue;
		}
		Sys_FindClose();

		// verify the existence of at least one pcx skin
		strcpy( scratch, dirnames[i] );
		strcat( scratch, "/*.pcx" );
		pcxnames = FS_ListFiles( scratch, &npcxfiles, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM );
		if ( !pcxnames ) {
			continue;
		}

		// count valid skins, which consist of a skin with a matching "_i" icon
		for ( k = 0; k < npcxfiles-1; k++ )
		{
			if ( !strstr( pcxnames[k], "_i.pcx" ) )
			{
				if ( IconOfSkinExists( pcxnames[k], pcxnames, npcxfiles - 1 ) )
				{
					nskins++;
				}
			}
		}
		if ( !nskins )
		{
			FreeFileList( pcxnames, npcxfiles );
			continue;
		}

		skinnames = Z_TagMalloc( sizeof( char * ) * ( nskins + 1 ), TAG_MENU );
		memset( skinnames, 0, sizeof( char * ) * ( nskins + 1 ) );

		// copy the valid skins
		for ( s = 0, k = 0; k < npcxfiles-1; k++ )
		{
			if ( !strstr( pcxnames[k], "_i.pcx" ) )
			{
				if ( IconOfSkinExists( pcxnames[k], pcxnames, npcxfiles - 1 ) )
				{
					a = strrchr( pcxnames[k], '/' );
					b = strrchr( pcxnames[k], '\\' );

					if ( a > b )
						c = a;
					else
						c = b;

					strcpy( scratch, c + 1 );

					if ( strrchr( scratch, '.' ) )
						*strrchr( scratch, '.' ) = 0;

					skinnames[s] = CopyString( scratch, TAG_MENU );
					s++;
				}
			}
		}

		// at this point we have a valid player model
		s_pmi[s_numplayermodels].nskins = nskins;
		s_pmi[s_numplayermodels].skindisplaynames = skinnames;

		// make short name for the model
		a = strrchr( dirnames[i], '/' );
		b = strrchr( dirnames[i], '\\' );

		if ( a > b )
			c = a;
		else
			c = b;

		strncpy( s_pmi[s_numplayermodels].displayname, c + 1, MAX_DISPLAYNAME-1 );
		strcpy( s_pmi[s_numplayermodels].directory, c + 1 );

		FreeFileList( pcxnames, npcxfiles );

		s_numplayermodels++;
	}

	FreeFileList( dirnames, ndirs );

	return true;
}

static int pmicmpfnc( const void *_a, const void *_b )
{
	const playermodelinfo_s *a = ( const playermodelinfo_s * ) _a;
	const playermodelinfo_s *b = ( const playermodelinfo_s * ) _b;

	/*
	** sort by male, female, then alphabetical
	*/
	if ( strcmp( a->directory, "male" ) == 0 )
		return -1;
	else if ( strcmp( b->directory, "male" ) == 0 )
		return 1;

	if ( strcmp( a->directory, "female" ) == 0 )
		return -1;
	else if ( strcmp( b->directory, "female" ) == 0 )
		return 1;

	return strcmp( a->directory, b->directory );
}

void PlayerConfig_MenuDraw( menuframework_s *self )
{
	refdef_t refdef;
	char scratch[MAX_QPATH];

	memset( &refdef, 0, sizeof( refdef ) );

	refdef.x = viddef.width / 2;
	refdef.y = viddef.height / 2 - 72;
	refdef.width = 144;
	refdef.height = 168;
	refdef.fov_x = 40;
	refdef.fov_y = CalcFov( refdef.fov_x, refdef.width, refdef.height );
	refdef.time = cls.realtime*0.001;

	if ( s_pmi[s_player_model_box.curvalue].skindisplaynames )
	{
		static int yaw = 0, nextUpdate = 0;
		//int maxframe = 29;
		entity_t entity;

		memset( &entity, 0, sizeof( entity ) );

		Com_sprintf( scratch, sizeof( scratch ), "players/%s/tris.md2", s_pmi[s_player_model_box.curvalue].directory );
		entity.model = R_RegisterModel( scratch );
		Com_sprintf( scratch, sizeof( scratch ), "players/%s/%s.pcx", s_pmi[s_player_model_box.curvalue].directory, s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] );
		entity.skin = R_RegisterSkin( scratch );
		entity.flags = RF_FULLBRIGHT;
		VectorSet(entity.origin, 80, 0, 0);
		VectorCopy( entity.origin, entity.oldorigin );
		entity.frame = 0;
		entity.oldframe = 0;
		entity.backlerp = 0.0;
		entity.angles[1] = yaw;

		if (nextUpdate <= cls.realtime) {
			nextUpdate = cls.realtime + 2;
			yaw++;
			if ( yaw > 360 )
				yaw -= 360;
		}

#ifdef GL_QUAKE
		AnglesToAxis(entity.angles, entity.axis);
#endif
		refdef.areabits = 0;
		refdef.num_entities = 1;
		refdef.entities = &entity;
		refdef.lightstyles = 0;
		refdef.rdflags = RDF_NOWORLDMODEL;

		Menu_Draw( self );

		M_DrawTextBox( ( refdef.x ) * ( 320.0F / viddef.width ) - 8, ( viddef.height / 2 ) * ( 240.0F / viddef.height) - 77, refdef.width / 8, refdef.height / 8 );
		refdef.height += 4;
#ifdef GL_QUAKE
		Draw_Fill( refdef.x, refdef.y, refdef.width, refdef.height, 7 );
#endif

		R_RenderFrame( &refdef );

		Com_sprintf( scratch, sizeof( scratch ), "/players/%s/%s_i.pcx", 
			s_pmi[s_player_model_box.curvalue].directory,
			s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] );
		Draw_Pic( s_player_config_menu.x - 40, refdef.y, scratch, 1 );
	}
}

const char *PlayerConfig_MenuKey (menuframework_s *self, int key)
{
	int i;

	if ( key == K_ESCAPE )
	{
		char scratch[1024];

		Cvar_Set( "name", s_player_name_field.buffer );

		Com_sprintf( scratch, sizeof( scratch ), "%s/%s", 
			s_pmi[s_player_model_box.curvalue].directory, 
			s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] );

		Cvar_Set( "skin", scratch );

		for ( i = 0; i < s_numplayermodels; i++ )
		{
			int j;

			for ( j = 0; j < s_pmi[i].nskins; j++ )
			{
				if ( s_pmi[i].skindisplaynames[j] )
					Z_Free( s_pmi[i].skindisplaynames[j] );
				s_pmi[i].skindisplaynames[j] = NULL;
			}
			Z_Free( s_pmi[i].skindisplaynames );
			s_pmi[i].skindisplaynames = NULL;
			s_pmi[i].nskins = 0;
		}
	}
	return Default_MenuKey( self, key );
}

qboolean PlayerConfig_MenuInit( void )
{
	extern cvar_t *info_name;
	extern cvar_t *info_skin;
	extern cvar_t *info_hand;
	char currentdirectory[1024];
	char currentskin[1024];
	int i = 0;
	int currentdirectoryindex = 0;
	int currentskinindex = 0;
	static const char *handedness[] = { "right", "left", "center", 0 };

	PlayerConfig_ScanDirectories();

	if (s_numplayermodels == 0)
		return false;

	if ( info_hand->integer < 0 || info_hand->integer > 2 )
		Cvar_SetValue( "hand", 0 );

	Q_strncpyz( currentdirectory, info_skin->string, sizeof(currentdirectory) );

	if ( strchr( currentdirectory, '/' ) )
	{
		strcpy( currentskin, strchr( currentdirectory, '/' ) + 1 );
		*strchr( currentdirectory, '/' ) = 0;
	}
	else if ( strchr( currentdirectory, '\\' ) )
	{
		strcpy( currentskin, strchr( currentdirectory, '\\' ) + 1 );
		*strchr( currentdirectory, '\\' ) = 0;
	}
	else
	{
		strcpy( currentdirectory, "male" );
		strcpy( currentskin, "grunt" );
	}

	qsort( s_pmi, s_numplayermodels, sizeof( s_pmi[0] ), pmicmpfnc );

	memset( s_pmnames, 0, sizeof( s_pmnames ) );
	for ( i = 0; i < s_numplayermodels; i++ )
	{
		s_pmnames[i] = s_pmi[i].displayname;
		if ( Q_stricmp( s_pmi[i].directory, currentdirectory ) == 0 )
		{
			int j;

			currentdirectoryindex = i;

			for ( j = 0; j < s_pmi[i].nskins; j++ )
			{
				if ( Q_stricmp( s_pmi[i].skindisplaynames[j], currentskin ) == 0 )
				{
					currentskinindex = j;
					break;
				}
			}
		}
	}

	memset(&s_player_config_menu, 0, sizeof(s_player_config_menu));
	s_player_config_menu.x = viddef.width / 2 - 95; 
	s_player_config_menu.y = viddef.height / 2 - 97;
	s_player_config_menu.nitems = 0;

	s_player_name_field.generic.type = MTYPE_FIELD;
	s_player_name_field.generic.name = "name";
	s_player_name_field.generic.callback = 0;
	s_player_name_field.generic.x		= 0;
	s_player_name_field.generic.y		= 0;
	s_player_name_field.length	= 20;
	s_player_name_field.visible_length = 20;
	Q_strncpyz( s_player_name_field.buffer, info_name->string, sizeof(s_player_name_field.buffer));
	s_player_name_field.cursor = strlen( s_player_name_field.buffer );

	s_player_model_title.generic.type = MTYPE_SEPARATOR;
	s_player_model_title.generic.name = "model";
	s_player_model_title.generic.x    = -8;
	s_player_model_title.generic.y	 = 60;

	s_player_model_box.generic.type = MTYPE_SPINCONTROL;
	s_player_model_box.generic.x	= -56;
	s_player_model_box.generic.y	= 70;
	s_player_model_box.generic.callback = ModelCallback;
	s_player_model_box.generic.cursor_offset = -48;
	s_player_model_box.curvalue = currentdirectoryindex;
	s_player_model_box.itemnames = (const char **)s_pmnames;

	s_player_skin_title.generic.type = MTYPE_SEPARATOR;
	s_player_skin_title.generic.name = "skin";
	s_player_skin_title.generic.x    = -16;
	s_player_skin_title.generic.y	 = 84;

	s_player_skin_box.generic.type = MTYPE_SPINCONTROL;
	s_player_skin_box.generic.x	= -56;
	s_player_skin_box.generic.y	= 94;
	s_player_skin_box.generic.name	= 0;
	s_player_skin_box.generic.callback = 0;
	s_player_skin_box.generic.cursor_offset = -48;
	s_player_skin_box.curvalue = currentskinindex;
	s_player_skin_box.itemnames = (const char **)s_pmi[currentdirectoryindex].skindisplaynames;

	s_player_hand_title.generic.type = MTYPE_SEPARATOR;
	s_player_hand_title.generic.name = "handedness";
	s_player_hand_title.generic.x    = 32;
	s_player_hand_title.generic.y	 = 108;

	s_player_handedness_box.generic.type = MTYPE_SPINCONTROL;
	s_player_handedness_box.generic.x	= -56;
	s_player_handedness_box.generic.y	= 118;
	s_player_handedness_box.generic.name	= 0;
	s_player_handedness_box.generic.cursor_offset = -48;
	s_player_handedness_box.generic.callback = HandednessCallback;
	s_player_handedness_box.curvalue = Cvar_VariableIntValue( "hand" );
	s_player_handedness_box.itemnames = handedness;

	for (i = 0; i < sizeof(rate_tbl) / sizeof(*rate_tbl) - 1; i++)
		if (Cvar_VariableIntValue("rate") == rate_tbl[i])
			break;

	s_player_rate_title.generic.type = MTYPE_SEPARATOR;
	s_player_rate_title.generic.name = "connect speed";
	s_player_rate_title.generic.x    = 56;
	s_player_rate_title.generic.y	 = 156;

	s_player_rate_box.generic.type = MTYPE_SPINCONTROL;
	s_player_rate_box.generic.x	= -56;
	s_player_rate_box.generic.y	= 166;
	s_player_rate_box.generic.name	= 0;
	s_player_rate_box.generic.cursor_offset = -48;
	s_player_rate_box.generic.callback = RateCallback;
	s_player_rate_box.curvalue = i;
	s_player_rate_box.itemnames = rate_names;

	s_player_download_action.generic.type = MTYPE_ACTION;
	s_player_download_action.generic.name	= "download options";
	s_player_download_action.generic.flags= QMF_LEFT_JUSTIFY;
	s_player_download_action.generic.x	= -24;
	s_player_download_action.generic.y	= 186;
	s_player_download_action.generic.statusbar = NULL;
	s_player_download_action.generic.callback = DownloadOptionsFunc;

	s_player_config_menu.draw = PlayerConfig_MenuDraw;
	s_player_config_menu.key = PlayerConfig_MenuKey;

	Menu_AddItem( &s_player_config_menu, &s_player_name_field );
	Menu_AddItem( &s_player_config_menu, &s_player_model_title );
	Menu_AddItem( &s_player_config_menu, &s_player_model_box );
	if ( s_player_skin_box.itemnames )
	{
		Menu_AddItem( &s_player_config_menu, &s_player_skin_title );
		Menu_AddItem( &s_player_config_menu, &s_player_skin_box );
	}
	Menu_AddItem( &s_player_config_menu, &s_player_hand_title );
	Menu_AddItem( &s_player_config_menu, &s_player_handedness_box );
	Menu_AddItem( &s_player_config_menu, &s_player_rate_title );
	Menu_AddItem( &s_player_config_menu, &s_player_rate_box );
	Menu_AddItem( &s_player_config_menu, &s_player_download_action );

	return true;
}

extern int s_multiplayer_menu;

void M_Menu_PlayerConfig_f (void)
{
	if (!PlayerConfig_MenuInit())
	{
		Menu_SetStatusBar( (menuframework_s *)&s_multiplayer_menu, "No valid player models found" );
		return;
	}
	Menu_SetStatusBar( (menuframework_s *)&s_multiplayer_menu, NULL );
	M_PushMenu( &s_player_config_menu );
}

