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

"Nullsoft", "Winamp", and "Winamp3" are trademarks of Nullsoft, Inc.
*/

#include "../client/client.h"
#include "winquake.h"

typedef struct qwinamp_s
{
	HWND	 hWnd;
	qboolean isOK;
} qwinamp_t;

static qwinamp_t mywinamp;

static cvar_t	*cl_winampmessages;
static cvar_t	*cl_winamp_dir;
static qboolean	updateSong = false;

static void MP3_GetWinAmp (void)
{

	mywinamp.hWnd = FindWindow( "Winamp v1.x", NULL);
	if (mywinamp.hWnd)
	{
		mywinamp.isOK = true;
		Com_Printf ("Winamp Integration Enabled\n");
		updateSong = true;
	}
	else
	{
		// Winamp not running, or we couldn't find it
		mywinamp.hWnd = NULL;
		mywinamp.isOK = false;
		Com_Printf ("Winamp Integration Disabled\n");
	}
}

static qboolean MP3_Status(void)
{
	if (mywinamp.isOK)
		return true;

	mywinamp.hWnd = FindWindow( "Winamp v1.x", NULL);
	if (mywinamp.hWnd)
	{
		mywinamp.isOK = true;
		return true;
	}
	mywinamp.hWnd = NULL;
	Com_Printf ("Winamp Integration Disabled\n");
	return false;

}

int MP3_GetStatus(void) {
	int ret;

	if (!MP3_Status())
		return MP3_NOTRUNNING;
	ret = SendMessage(mywinamp.hWnd, WM_USER, 0, 104);
	switch (ret) {
		case 3 : return MP3_PAUSED; break;
		case 1 : return MP3_PLAYING; break;
		case 0 : 
		default : return MP3_STOPPED; break;
	}
}

/*
===================
MP3_SetVolume

Updates Winamp's volume to give value
===================
*/
void MP3_SetVolume_f (void)
{
	int vol, percent;

	if (!MP3_Status())
		return;
	
	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: winampvolume <value>\n" );
		return;
    }

	percent = atoi(Cmd_Args());

	vol = (percent * 0.01f) * 255;
	clamp(vol, 0, 255);

	SendMessage(mywinamp.hWnd, WM_USER, vol, 122);
	if (cl_winampmessages->integer)
		Com_Printf ("Winamp volume set to %i%%\n", percent);
}

/*
===================
MP3_ToggleShuffle
Toggles suffle mode
===================
*/
void MP3_ToggleShuffle_f (void)
{
	int ret;

	if (!MP3_Status())
		return;

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40023, 0);

	ret = SendMessage(mywinamp.hWnd, WM_USER, 0, 250);
	if (ret == 1)
		Com_Printf ("Winamp Shuffle is ON\n");
	else
		Com_Printf ("Winamp Shuffle OFF\n");
}

/*
===================
MP3_ToggleRepeat
Toggles repeat mode
===================
*/
void MP3_ToggleRepeat_f (void)
{
	int ret;

	if (!MP3_Status())
		return;

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40022, 0);

	ret = SendMessage(mywinamp.hWnd, WM_USER, 0, 251);
	if (ret == 1)
		Com_Printf ("Winamp Repeat is ON\n");
	else
		Com_Printf ("Winamp Repeat is OFF\n");
}

/*
===================
MP3_VolumeUp
Increase winamp volume by 1%
===================
*/
void MP3_VolumeUp_f (void)
{
	if (!MP3_Status())
		return;

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40058, 0);
	if (cl_winampmessages->integer)
		Com_Printf ("Winamp Volume Increased by 1%%\n");
}

void MP3_VolumeDown_f (void)
{
	if (!MP3_Status())
		return;

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40059, 0);
	if (cl_winampmessages->integer)
		Com_Printf ("Winamp Volume Decreased by 1%%\n");
}
/*
===================
MP3_GetPlaylistInfo
===================
*/
void MP3_GetPlaylistInfo (int *current, int *length)
{
	if (!mywinamp.isOK)
		return;

	if (length)
		*length = SendMessage (mywinamp.hWnd, WM_USER, 0, 124);

	if (current)
		*current = SendMessage (mywinamp.hWnd, WM_USER, 0, 125);
}

qboolean MP3_GetTrackTime(int *elapsed, int *total)
{
	int ret1 = 0, ret2 = 0;

	if (!mywinamp.isOK)
		return false;
	if (elapsed && (ret1 = SendMessage(mywinamp.hWnd, WM_USER, 0, 105)) == -1)
		return false;
	if (total && (ret2 = SendMessage(mywinamp.hWnd, WM_USER, 1, 105)) == -1)
		return false;

	if(elapsed)
		*elapsed = ret1 / 1000;

	if(total)
		*total = ret2;

	return true;
}
/*
===================
MP3_PlayTrack
Start playing given track
===================
*/
qboolean MP3_PlayTrack (int num)
{
	int length, ret;

	num -= 1;
	if (num < 0)
		return false;

	MP3_GetPlaylistInfo(NULL, &length);
	if(num > length)
	{
		Com_Printf("Winamp: playlist got only %i tracks\n", length);
        return false;
	}

	updateSong = true;
	ret = SendMessage(mywinamp.hWnd, WM_USER, 1, 104); //status of playback. ret 1 is playing. ret 3 is paused.
	if(ret == 3) //in paused case it just resume it so we need to stop it
		SendMessage(mywinamp.hWnd, WM_COMMAND, 40047, 0);

	SendMessage(mywinamp.hWnd, WM_USER, num, 121);	//Sets position in playlist
	SendMessage(mywinamp.hWnd, WM_COMMAND, 40045, 0);	//Play it

	return true;
}
/*
===================
MP3_Play
===================
*/
void MP3_Play_f (void)
{
	int track;

	if (!MP3_Status())
		return;

	if (Cmd_Argc() == 2)
	{
		track = atoi(Cmd_Argv(1));
        
		if(!MP3_PlayTrack (track))
			return;

		if (cl_winampmessages->integer)
			Com_Printf ("Winamp Play track %i\n", track);

		return;
    }
	updateSong = true;
	SendMessage(mywinamp.hWnd, WM_COMMAND, 40045, 0);
	if (cl_winampmessages->integer)
		Com_Printf ("Winamp Play\n");
}

/*
===================
MP3_Play
===================
*/
void MP3_Stop_f (void)
{
	if (!MP3_Status())
		return;

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40047, 0);
	if (cl_winampmessages->integer)
		Com_Printf ("Winamp Stop\n");
}

/*
===================
MP3_Play
===================
*/
void MP3_Pause_f (void)
{
	int status;

	status = MP3_GetStatus();
	if (status == MP3_NOTRUNNING)
		return;

	if(status == MP3_STOPPED)
	{
		Com_Printf("Winamp is stopped\n");
		return;
	}

	SendMessage(mywinamp.hWnd, WM_COMMAND, 40046, 0);
	if (cl_winampmessages->integer)
		Com_Printf ("Winamp is %s\n", (status == MP3_PAUSED) ? "playing" : "paused");
}

/*
===================
MP3_NextTrack
===================
*/
void MP3_NextTrack_f (void)
{
	if (!MP3_Status())
		return;

	updateSong = true;
	SendMessage(mywinamp.hWnd, WM_COMMAND, 40048, 0);
	if (cl_winampmessages->integer)
		Com_Printf ("Winamp Next Track\n");
}

/*
===================
MP3_PreviousTrack
===================
*/
void MP3_PreviousTrack_f (void)
{
	if (!MP3_Status())
		return;

	updateSong = true;
	SendMessage(mywinamp.hWnd, WM_COMMAND, 40044, 0);
	if (cl_winampmessages->integer)
		Com_Printf ("Winamp Previous Track\n");
}

/*
===================
MP3_SongTitle

Returns current song title
===================
*/
static char *MP3_SongTitle (qboolean tracknum)
{ 
   static char title[MP3_MAXSONGTITLE]; 
   char			*s; 

   title[0] = 0;

   GetWindowText(mywinamp.hWnd, title, sizeof(title)); 
   
   s = strrchr(title, '-');
   if (s && s > title) 
      *(s - 1) = 0;

   if(!tracknum)
   {
		for (s = title; *s && isdigit((unsigned char)*s); s++)
			;
		if (*s == '.' && s[1] == ' ' && s[2])
			memmove(title, s + 2, strlen(s + 2) + 1);
   }

	COM_MakePrintable (title);

	return title; 
}

char *MP3_Menu_SongTitle (void)
{
   return MP3_SongTitle (true); 
}

/*
===================
MP3_Title
===================
*/
void MP3_Title_f (void)
{
	char *songtitle;
	int	 total;

	if (!MP3_Status())
		return;

	if(!MP3_GetTrackTime(NULL, &total))
		return;

	songtitle = MP3_SongTitle(true);
	if (!*songtitle)
		return;

	Com_Printf (S_ENABLE_COLOR "%sWinamp Title: %s%s %s[%i:%02i]\n",
		S_COLOR_CYAN, S_COLOR_YELLOW, songtitle, S_COLOR_CYAN, total / 60, total % 60);
}

static void MP3_SongTitle_m ( char *buffer, int bufferSize )
{
	char *songtitle;
	int	 total;

	//if (!MP3_Status())
	//	return;

	if (!mywinamp.isOK)
		return;

	if(!MP3_GetTrackTime(NULL, &total))
		return;

	songtitle = MP3_SongTitle(false);
	if (!*songtitle)
		return;

	Com_sprintf(buffer, bufferSize, "%s [%i:%02i]", songtitle, total / 60, total % 60);
}

/*
===================
MP3_SongInfo
===================
*/
void MP3_SongInfo_f (void)
{
	char *songtitle;
	int total, elapsed, remaining, samplerate, bitrate;

	if (!MP3_Status())
		return;

	if(!MP3_GetTrackTime(&elapsed, &total))
		return;

	songtitle = MP3_SongTitle(true);
	if (!*songtitle)
		return;

	remaining = total - elapsed;

	samplerate = SendMessage(mywinamp.hWnd, WM_USER, 0, 126);
	bitrate = SendMessage(mywinamp.hWnd, WM_USER, 1, 126);

	Com_Printf ("WinAmp current song info:\n");
	Com_Printf ("Name: %s Length: %i:%02i\n", songtitle, total / 60, total % 60);
	Com_Printf ("Elapsed: %i:%02i Remaining: %i:%02i Bitrate: %ikbps Samplerate: %ikHz\n",
				elapsed / 60, elapsed % 60, remaining / 60, remaining % 60, bitrate, samplerate);
}

static int FS_filelength (FILE *f)
{
	int		pos;
	int		end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

/*
===================
MP3_GetPlaylist

===================
*/
int MP3_GetPlaylist (char **buf)
{
	FILE			*file;
	char			path[512];
	int				pathlength;
	long			filelength;

	if (!MP3_Status())
		return -1;

	SendMessage (mywinamp.hWnd, WM_USER, 0, 120);
	Q_strncpyz (path, cl_winamp_dir->string, sizeof (path));
	pathlength = strlen(path);

	if (pathlength && (path[pathlength - 1] == '\\' || path[pathlength - 1] == '/'))
		path[pathlength - 1] = 0;

	Q_strncatz(path, "/winamp.m3u", sizeof(path));
	file = fopen (path, "rb");
	if (!file) {
		Com_Printf("Cant find winamp in \"%s\", use cl_winamp_dir to change dir\n", path);
		return -1;
	}
	filelength = FS_filelength (file);

	*buf = Z_TagMalloc (filelength, TAG_MP3LIST);
	if (filelength != fread (*buf, 1,  filelength, file))
	{
		Z_Free (*buf);
		fclose (file);
		return -1;
	}

	fclose (file);

	return filelength;
}


int MP3_ParsePlaylist_EXTM3U(char *playlist_buf, unsigned int length, mp3_tracks_t *songList, const char *filter)
{
	int i, skip = 0, playlist_size = 0;
	char *s, *t, *buf, *line;
	int trackNum = 0, songCount = 0, trackNum2 = 0, trackNumLen = 0, j;
	qboolean counted = false;

	for(i = 0; i < 2; i++)
	{
		buf = playlist_buf;
		trackNum = 0;
		for (;;) {
			for (s = line = buf; s - playlist_buf < length && *s && *s != '\n' && *s != '\r'; s++)
				;
			if (s - playlist_buf >= length)
				break;
			*s = 0;
			buf = s + 2;
			if (skip || !strncmp(line, "#EXTM3U", 7)) {
				skip = 0;
				continue;
			}
			if (!strncmp(line, "#EXTINF:", 8)) {
				if (!(s = strchr(line, ',')) || ++s - playlist_buf >= length) 
					break;
				
				skip = 1;
				goto print;
			}
		
			for (s = line + strlen(line); s > line && *s != '\\' && *s != '/'; s--)
				;
			if (s != line)
				s++;
		
			if ((t = strrchr(s, '.')) && t - playlist_buf < length)
				*t = 0;		
		
			for (t = s + strlen(s) - 1; t > s && *t == ' '; t--)
				*t = 0;

	print:
			trackNum++;

			if(!Q_stristr(s, filter))
				continue;

			if(!counted) {
				trackNum2 = trackNum;
				songCount++;
				continue;
			}

			if (strlen(s) >= MP3_MAXSONGTITLE-1)
				s[MP3_MAXSONGTITLE-1] = 0;

			COM_MakePrintable(s);
			songList->num[playlist_size] = trackNum;
			songList->name[playlist_size++] = CopyString(va("%*i. %s", trackNumLen, trackNum, s), TAG_MP3LIST);

			if(playlist_size >= songCount)
				break;
		}
		if(!counted)
		{
			if(!songCount)
				return 0;

			for(j = 1; ; j++)
			{
				if(trackNum2 < (int)pow(10, j))
				{
					trackNumLen = j;
					break;
				}
			}
			songList->name = Z_TagMalloc (sizeof(char *) * songCount, TAG_MP3LIST);
			songList->num = Z_TagMalloc (sizeof(int) * songCount, TAG_MP3LIST);
			counted = true;
		}
	}
	return playlist_size;
}

/*
===================
MP3_PrintPlaylist

===================
*/
void MP3_PrintPlaylist_f (void)
{
	char *playlist_buf;
	unsigned int length;
	int i, playlist_size, current;
	char *filter;
	mp3_tracks_t songList;

	if (!MP3_Status())
		return;

	if (Cmd_Argc() < 2)
	{
		Com_Printf("Usage: %s <song name>\n", Cmd_Argv(0));
		return;
    }

	if ((length = MP3_GetPlaylist(&playlist_buf)) == -1)
		return;

	filter = Cmd_Args();

	MP3_GetPlaylistInfo (&current, NULL);

	playlist_size = MP3_ParsePlaylist_EXTM3U(playlist_buf, length, &songList, filter);
	Z_Free (playlist_buf);

	if(!playlist_size)
	{
		Com_Printf("Cant find any tracks with %s\n", filter);
		return;
	}
	
	for (i = 0; i < playlist_size; i++) {
		Com_Printf("%s%s\n", songList.num[i] == current+1 ? "\x02" : "", songList.name[i]);
		Z_Free(songList.name[i]);
	}
	Z_Free (songList.name);
	Z_Free (songList.num);
}

/*
===================
MP3_Restart
===================
*/
void MP3_Restart_f (void)
{
	MP3_GetWinAmp();
}

/*
===================
MP3_Init
===================
*/
void MP3_Init (void)
{
	cl_winampmessages = Cvar_Get("cl_winampmessages", "1", CVAR_ARCHIVE);
	cl_winamp_dir = Cvar_Get("cl_winamp_dir", "C:/Program Files/Winamp", CVAR_ARCHIVE);

	Cmd_AddCommand ( "winampnext", MP3_NextTrack_f );
	Cmd_AddCommand ( "winamppause", MP3_Pause_f );
	Cmd_AddCommand ( "winampplay", MP3_Play_f );
	Cmd_AddCommand ( "winampprev", MP3_PreviousTrack_f );
	Cmd_AddCommand ( "winampstop", MP3_Stop_f );
	Cmd_AddCommand ( "winampvolup", MP3_VolumeUp_f );
	Cmd_AddCommand ( "winampvoldown", MP3_VolumeDown_f );
	Cmd_AddCommand ( "winamprestart", MP3_Restart_f );
	Cmd_AddCommand ( "winampshuffle", MP3_ToggleShuffle_f );
	Cmd_AddCommand ( "winamprepeat", MP3_ToggleRepeat_f );
	Cmd_AddCommand ( "winampvolume", MP3_SetVolume_f );
	Cmd_AddCommand ( "winamptitle", MP3_Title_f );
	Cmd_AddCommand ( "winampsonginfo", MP3_SongInfo_f );
	Cmd_AddCommand ( "winampsearch", MP3_PrintPlaylist_f );
	Cmd_AddMacro( "cursong", MP3_SongTitle_m );

	// Get WinAmp
	MP3_GetWinAmp();
}

/*
===================
MP3_Shutdown
===================
*/
void MP3_Shutdown (void)
{
	if(!cl_winampmessages)
		return;

	Cmd_RemoveCommand ( "winampnext" );
	Cmd_RemoveCommand ( "winamppause" );
	Cmd_RemoveCommand ( "winampplay" );
	Cmd_RemoveCommand ( "winampprev" );
	Cmd_RemoveCommand ( "winampstop" );
	Cmd_RemoveCommand ( "winampvolup" );
	Cmd_RemoveCommand ( "winampvoldown" );
	Cmd_RemoveCommand ( "winamprestart" );
	Cmd_RemoveCommand ( "winampshuffle" );
	Cmd_RemoveCommand ( "winamprepeat" );
	Cmd_RemoveCommand ( "winampvolume" );
	Cmd_RemoveCommand ( "winamptitle" );
	Cmd_RemoveCommand ( "winampsonginfo" );
	Cmd_RemoveCommand ( "winampsearch" );
}

/*
===================
MP3_Frame
===================
*/
void MP3_Frame (void)
{
	char *songtitle;
	static int curTrack = -1;
	int	total, track = -1;

	if (!mywinamp.isOK || !cl_winampmessages->integer)
		return;

	if(!((int)(cls.realtime>>8)&8) && !updateSong)
		return;

	MP3_GetPlaylistInfo (&track, NULL);
	if(track  == curTrack)
		return;

	if(!MP3_GetTrackTime(NULL, &total))
		return;

	songtitle = MP3_SongTitle(true);
	if (!*songtitle)
		return;

	curTrack = track;
	Com_Printf (S_ENABLE_COLOR "%sWinamp Title: %s%s %s[%i:%02i]\n",
		S_COLOR_CYAN, S_COLOR_YELLOW, songtitle, S_COLOR_CYAN, total / 60, total % 60);

	updateSong = false;
}

