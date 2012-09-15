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

mp3.h
Remote controls for winamp/xmms/mpd
*/

#if defined(_WIN32) || defined(WITH_XMMS) || defined(WITH_MPD)

#define MP3_NOTRUNNING		-1
#define MP3_PLAYING			1
#define MP3_PAUSED			2
#define MP3_STOPPED			3

#define MP3_MAXSONGTITLE	128

#if defined(WITH_MPD)
#define MP3_PLAYERNAME_ALLCAPS		"MPD"
#define MP3_PLAYERNAME_LEADINGCAP	"MPD"
#define MP3_PLAYERNAME_NOCAPS		"mpd"
#elif defined(WITH_XMMS)
#define MP3_PLAYERNAME_ALLCAPS		"XMMS"
#define MP3_PLAYERNAME_LEADINGCAP	"XMMS"
#define MP3_PLAYERNAME_NOCAPS		"xmms"
#else
#define MP3_PLAYERNAME_ALLCAPS		"WINAMP"
#define MP3_PLAYERNAME_LEADINGCAP	"Winamp"
#define MP3_PLAYERNAME_NOCAPS		"winamp"
#endif

typedef struct
{
	int *num;
	char **name;

} mp3_tracks_t;

typedef struct
{
	int track;
	int total;
	char name[MP3_MAXSONGTITLE];

} mp3_track_t;

void MP3_Init(void);
void MP3_Shutdown(void);

#ifndef WITH_MPD

void MP3_Frame (void);
void MP3_GetPlaylistInfo (int *current, int *length);

#if defined(WITH_XMMS)
int MP3_GetPlaylistSongs(mp3_tracks_t *songList, char *filter);
#else
int MP3_GetPlaylist (char **buf);
int MP3_ParsePlaylist_EXTM3U(char *playlist_buf, unsigned int length, mp3_tracks_t *songList, const char *filter);
#endif
qboolean MP3_GetTrackTime(int *elapsed, int *total);
qboolean MP3_PlayTrack (int num);
char *MP3_Menu_SongTitle (void);

#endif

#endif
