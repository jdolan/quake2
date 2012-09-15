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
#if defined(_WIN32) || defined(WITH_XMMS)

#include "ui_local.h"

/*
=======================================================================

MP3 (winamp/xmms) MENU

=======================================================================
*/
#define MP3MENU_TITLE "-[ " MP3_PLAYERNAME_LEADINGCAP " Menu ]-"

typedef struct m_tracks_s {
	menuframework_s	menu;
	menulist_s		list;
	menufield_s		filter;

	mp3_tracks_t	track;
} m_tracks_t;

static m_tracks_t	m_tracks;
static int			track_count = 0;
static mp3_track_t	currentsong = { -1, -1, "\0"};

static void Update_Current (void)
{
	char *songtitle;
	int track = -1, total = 0;

	MP3_GetPlaylistInfo (&track, NULL);
	if(track != currentsong.track)
	{
		if(MP3_GetTrackTime(NULL, &total))
		{
			songtitle = MP3_Menu_SongTitle();
			if (songtitle)
			{
				Q_strncpyz(currentsong.name, songtitle, sizeof(currentsong.name));
				currentsong.track = track;
				currentsong.total = total;
			}
		}
	}
}

static void MP3_MenuDraw( menuframework_s *self ) {

	char song_print[MP3_MAXSONGTITLE];

	DrawString((viddef.width - (strlen(MP3MENU_TITLE)*8))>>1, 10, MP3MENU_TITLE);

	Menu_Draw( self );

	Update_Current();
	if(currentsong.track == -1) {
		DrawAltString( 20, 20, MP3_PLAYERNAME_LEADINGCAP " not running");
		return;
	}

	Com_sprintf (song_print, sizeof(song_print), "Current: %s [%i:%02i]\n", currentsong.name, currentsong.total / 60, currentsong.total % 60);

	DrawAltString (20, 20, song_print);
}

static void Tracks_Free( void )
{
	int i;

	if(!track_count)
		return;

	for( i=0 ; i<track_count; i++ )
		Z_Free( m_tracks.track.name[i] );

	Z_Free (m_tracks.track.name);
	Z_Free (m_tracks.track.num);
	m_tracks.track.name = NULL;
	m_tracks.track.num = NULL;
	track_count = 0;
	m_tracks.list.itemnames = NULL;
}

static void Tracks_Scan( void)
{
#ifdef _WIN32
	long length;	
	char *playlist_buf = NULL;

	Tracks_Free();

	if ((length = MP3_GetPlaylist(&playlist_buf)) == -1)
		return;

	track_count = MP3_ParsePlaylist_EXTM3U(playlist_buf, length, &m_tracks.track, m_tracks.filter.buffer);
	Z_Free(playlist_buf);
#endif
#ifdef WITH_XMMS
	Tracks_Free();

	track_count = MP3_GetPlaylistSongs(&m_tracks.track, m_tracks.filter.buffer);
#endif
}

int Current_Track(void)
{
	int i;

	Update_Current();

	for( i=0 ; i<track_count; i++ )
		if(m_tracks.track.num[i] - 1 == currentsong.track)
			return i;

	return 0;
}
static void Build_Tracklist (void)
{
	int maxItems = m_tracks.list.maxItems;

	Tracks_Scan();
	m_tracks.list.itemnames = (const char **)m_tracks.track.name;
	m_tracks.list.count = track_count;
	m_tracks.list.curvalue = Current_Track();
	if(m_tracks.list.count > maxItems)
	{
		m_tracks.list.prestep = m_tracks.list.curvalue - (maxItems/2);
		if(m_tracks.list.prestep < 0)
			m_tracks.list.prestep = 0;
		else if(m_tracks.list.prestep > m_tracks.list.count - maxItems)
			m_tracks.list.prestep = m_tracks.list.count - maxItems;
	}
	else
		m_tracks.list.prestep = 0;
}
static void Tracks_Filter( void *s ) {
	Build_Tracklist();
}

void Select_Track ( void *s)
{
	if(!m_tracks.list.count)
		return;

	MP3_PlayTrack(m_tracks.track.num[m_tracks.list.curvalue]);
}

const char *MP3_MenuKey( menuframework_s *self, int key )
{
	switch( key ) {
	case K_ESCAPE:
		Tracks_Free();
		M_PopMenu();
		return NULL;
	}

	return Default_MenuKey( self, key );
}

void MP3_MenuInit( void ) {
	memset( &m_tracks.menu, 0, sizeof( m_tracks.menu ) );

	m_tracks.menu.x = 0;
	m_tracks.menu.y = 0;

	m_tracks.list.generic.type		= MTYPE_LIST;
	m_tracks.list.generic.flags		= QMF_LEFT_JUSTIFY;
	m_tracks.list.generic.name		= NULL;
	m_tracks.list.generic.callback  = Select_Track;
	m_tracks.list.generic.x			= 20;
	m_tracks.list.generic.y			= 30;
	m_tracks.list.width				= viddef.width - 40;
	m_tracks.list.height			= viddef.height - 110;
	MenuList_Init(&m_tracks.list);

	m_tracks.filter.generic.type	= MTYPE_FIELD;
	m_tracks.filter.generic.name	= "Search";
	m_tracks.filter.generic.callback = Tracks_Filter;
	m_tracks.filter.generic.x		= (viddef.width/2)-100;
	m_tracks.filter.generic.y		= 30 + m_tracks.list.height + 5;
	m_tracks.filter.length			= 30;
	m_tracks.filter.visible_length	= 15;
	memset(m_tracks.filter.buffer, 0, sizeof(m_tracks.filter.buffer));
	m_tracks.filter.cursor			= 0;

	m_tracks.menu.draw = MP3_MenuDraw;
	m_tracks.menu.key = MP3_MenuKey;
	Menu_AddItem( &m_tracks.menu, (void *)&m_tracks.list );
	Menu_AddItem( &m_tracks.menu, (void *)&m_tracks.filter );

	Menu_SetStatusBar( &m_tracks.menu, NULL );
	m_tracks.menu.cursor = 1; //cursor default to search box
	Menu_AdjustCursor( &m_tracks.menu, 1);

	Build_Tracklist();
}



void M_Menu_MP3_f( void ) {
	MP3_MenuInit();
	M_PushMenu( &m_tracks.menu );
}
#endif

