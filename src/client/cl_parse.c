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
// cl_parse.c  -- parse a message received from the server

#include "client.h"

cvar_t	*cl_timestamps;
cvar_t	*cl_timestampsformat;
cvar_t	*cl_highlight;
cvar_t	*cl_highlightmode;
cvar_t	*cl_highlightcolor;
cvar_t	*cl_highlightnames;
cvar_t	*cl_textcolors;
cvar_t	*cl_mychatcolor;
cvar_t	*cl_chat_notify;

cvar_t	*cl_autoscreenshot;

extern cvar_t *info_name;

void CL_VersionReply (const char *s);
qboolean CL_ChatParse (const char *s, int *clinu, int *skip2, int *mm2);
void CL_HighlightNames( char *s );
int CL_Highlight ( const char *s, int *color );
void CL_ParseAutoScreenshot (const char *s);
char *CL_Timestamp( qboolean chat );
qboolean CL_IgnoreText(const char *s);
qboolean CL_IgnoreNick(const char *s);

void CL_Reconnect_f (void);

const char *svc_strings[256] =
{
	"svc_bad",

	"svc_muzzleflash",
	"svc_muzzleflash2",
	"svc_temp_entity",
	"svc_layout",
	"svc_inventory",

	"svc_nop",
	"svc_disconnect",
	"svc_reconnect",
	"svc_sound",
	"svc_print",
	"svc_stufftext",
	"svc_serverdata",
	"svc_configstring",
	"svc_spawnbaseline",	
	"svc_centerprint",
	"svc_download",
	"svc_playerinfo",
	"svc_packetentities",
	"svc_deltapacketentities",
	"svc_frame",
	"svc_zpacket",
	"svc_zdownload"
};

//=============================================================================

static void CL_DownloadFileName(char *dest, int destlen, const char *fn)
{
	//if (strncmp(fn, "players", 7) == 0)
	//	Com_sprintf (dest, destlen, "%s/%s", BASEDIRNAME, fn);
	//else
		Com_sprintf (dest, destlen, "%s/%s", FS_Gamedir(), fn);
}

static void CL_FinishDownload (void)
{
	char	oldn[MAX_OSPATH], newn[MAX_OSPATH];

	fclose (cls.download);

	// rename the temp file to it's final name
	CL_DownloadFileName(oldn, sizeof(oldn), cls.downloadtempname);
	CL_DownloadFileName(newn, sizeof(newn), cls.downloadname);

	if (!FS_RenameFile(oldn, newn))
		Com_Printf ("failed to rename.\n");

	cls.failed_download = false;
	cls.downloadname[0] = 0;
	cls.downloadposition = 0;
	cls.download = NULL;
	cls.downloadpercent = 0;
}

/*
===============
CL_CheckOrDownloadFile

Returns true if the file exists, otherwise it attempts
to start a download from the server.
===============
*/
qboolean	CL_CheckOrDownloadFile (const char *filename)
{
	FILE *fp;
	char	name[MAX_OSPATH];
	static char lastfilename[MAX_OSPATH] = "\0";


	Q_strncpyz(name, filename, sizeof(name));
	COM_FixPath(name);
	filename = name;

	//r1: don't attempt same file many times
	if (!strcmp (filename, lastfilename))
		return true;

	strcpy (lastfilename, filename);

	if (strstr (filename, ".."))
	{
		Com_Printf ("Refusing to download a path with .. (%s)\n", filename);
		return true;
	}

	if (strchr (filename, ' '))
	{
		Com_Printf ("Refusing to check a path containing spaces (%s)\n", filename);
		return true;
	}

	if (strchr (filename, ':'))
	{
		Com_Printf ("Refusing to check a path containing a colon (%s)\n", filename);
		return true;
	}

	if (FS_LoadFile (filename, NULL) != -1)
	{	// it exists, no need to download
		return true;
	}

#ifdef USE_CURL
	if (CL_QueueHTTPDownload (filename))
	{
		//we return true so that the precache check keeps feeding us more files.
		//since we have multiple HTTP connections we want to minimize latency
		//and be constantly sending requests, not one at a time.
		return true;
	}
#endif

	strcpy (cls.downloadname, filename);

	// download to a temp name, and only rename
	// to the real name when done, so if interrupted
	// a runt file wont be left
	COM_StripExtension (cls.downloadname, cls.downloadtempname);
	strcat (cls.downloadtempname, ".tmp");

//ZOID
	// check to see if we already have a tmp for this file, if so, try to resume
	// open the file if not opened yet
	CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);

//	FS_CreatePath (name);

	fp = fopen (name, "r+b");
	if (fp)
	{ // it exists
		int len;
		fseek(fp, 0, SEEK_END);
		len = ftell(fp);

		cls.download = fp;

		// give the server an offset to start the download
		Com_Printf ("Resuming %s\n", cls.downloadname);

		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		
		if (cls.serverProtocol == PROTOCOL_VERSION_R1Q2)
			MSG_WriteString (&cls.netchan.message, va("download \"%s\" %i udp-zlib", cls.downloadname, len));
		else
			MSG_WriteString (&cls.netchan.message, va("download \"%s\" %i", cls.downloadname, len));
	}
	else
	{
		Com_Printf ("Downloading %s\n", cls.downloadname);

		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);

		if (cls.serverProtocol == PROTOCOL_VERSION_R1Q2)
			MSG_WriteString (&cls.netchan.message, va("download \"%s\" 0 udp-zlib", cls.downloadname));
		else
			MSG_WriteString (&cls.netchan.message, va("download \"%s\"", cls.downloadname));
	}

	//cls.downloadnumber++;

	return false;
}

/*
===============
CL_Download_f

Request a download from the server
===============
*/
void	CL_Download_f (void)
{
	char filename[MAX_OSPATH];

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: download <filename>\n");
		return;
	}

	if (cls.state <= ca_connecting)
	{
		Com_Printf ("Not connected.\n");
		return;
	}

	Q_strncpyz(filename, Cmd_Argv(1), sizeof(filename));
	COM_FixPath(filename);

	if (FS_LoadFile (filename, NULL) != -1)
	{	// it exists, no need to download
		Com_Printf("File already exists.\n");
		return;
	}

	CL_CheckOrDownloadFile (filename);
}

/*
======================
CL_RegisterSounds
======================
*/
void CL_RegisterSounds (void)
{
	int		i;

	S_BeginRegistration ();
	CL_RegisterTEntSounds ();
	for (i=1 ; i<MAX_SOUNDS ; i++)
	{
		if (!cl.configstrings[CS_SOUNDS+i][0])
			break;
		cl.sound_precache[i] = S_RegisterSound (cl.configstrings[CS_SOUNDS+i]);
		Sys_SendKeyEvents ();	// pump message loop
	}
	S_EndRegistration ();
}


/*
=====================
CL_ParseDownload

A download message has been received from the server
=====================
*/
void CL_ParseDownload (sizebuf_t *msg, qboolean dataIsCompressed)
{
	int		size, percent;
	char	name[MAX_OSPATH];
	size_t	sz;
//	int		r;

	// read the data
	size = MSG_ReadShort (msg);
	percent = MSG_ReadByte (msg);
	if (size < 0)
	{
		if (size == -1)
			Com_Printf ("Server does not have this file.\n");
		else
			Com_Printf ("Bad download data from server.\n");

		cls.downloadtempname[0] = 0;
		cls.downloadname[0] = 0;
		cls.failed_download = true;

		if (cls.download)
		{
			// if here, we tried to resume a file but the server said no
			fclose (cls.download);
			cls.download = NULL;
		}
		CL_RequestNextDownload ();
		return;
	}

	// open the file if not opened yet
	if (!cls.download)
	{
		if (!cls.downloadtempname[0])
		{
			Com_Printf ("Received download packet without request. Ignored.\n");
			msg->readcount += size;
			return;
		}

		CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);

		FS_CreatePath (name);

		cls.download = fopen (name, "wb");
		if (!cls.download)
		{
			msg->readcount += size;
			Com_Printf ("Failed to open %s\n", cls.downloadtempname);
			CL_RequestNextDownload ();
			return;
		}
	}

	if (dataIsCompressed)
	{
		uint16		uncompressedLen;
		byte		uncompressed[0xFFFF];

		uncompressedLen = MSG_ReadShort (msg);

		if (!uncompressedLen)
			Com_Error (ERR_DROP, "uncompressedLen == 0");

		ZLibDecompress (msg->data + msg->readcount, size, uncompressed, uncompressedLen, -15);
		sz = fwrite (uncompressed, 1, uncompressedLen, cls.download);
		Com_DPrintf ("svc_zdownload(%s): %d -> %d\n", cls.downloadname, size, uncompressedLen);
	}
	else
	{
		sz = fwrite (msg->data + msg->readcount, 1, size, cls.download);
	}

	//fwrite (net_message.data + net_message.readcount, 1, size, cls.download);
	msg->readcount += size;

	if (percent != 100)
	{
		// request next block
// change display routines by zoid

		cls.downloadpercent = percent;

		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "nextdl");
	}
	else
	{
		CL_FinishDownload();

		// get another file if needed
		CL_RequestNextDownload ();
	}
}


/*
=====================================================================

  SERVER CONNECTING MESSAGES

=====================================================================
*/

/*
==================
CL_ParseServerData
==================
*/
qboolean CL_ParseServerData (sizebuf_t *msg)
{
	char	*str;
	int		i;
	
	Com_DPrintf ("Serverdata packet received.\n");
//
// wipe the client_state_t struct
//
	CL_ClearState ();
	cls.state = ca_connected;

// parse protocol version number
	i = MSG_ReadLong (msg);
	cls.serverProtocol = i;

	cl.servercount = MSG_ReadLong (msg);
	cl.attractloop = MSG_ReadByte (msg);

	if (cl.attractloop) {
		//cls.serverProtocol = PROTOCOL_VERSION_DEFAULT;
	}
	else if (i != PROTOCOL_VERSION_DEFAULT && i != PROTOCOL_VERSION_R1Q2) {
		Com_Error (ERR_DROP, "Server is using unknown protocol %d.", i);
	}

	// game directory
	str = MSG_ReadString (msg);
	Q_strncpyz (cl.gamedir, str, sizeof(cl.gamedir));
	str = cl.gamedir;

	// set gamedir
	if (!Com_ServerState()) {
		Cvar_SetLatched("game", str);
		if( FS_NeedRestart() ) {
			CL_RestartFilesystem(true);
		}
	}

	// parse player entity number
	cl.playernum = MSG_ReadShort (msg);

	// get the full level name
	str = MSG_ReadString (msg);

	cl.pmp.strafeHack = false;
	cl.pmp.speedMultiplier = 1;
	cl.pmp.airaccelerate = 0;
	cls.protocolVersion = 0;

	if (cls.serverProtocol == PROTOCOL_VERSION_R1Q2)
	{
		i = MSG_ReadByte(msg);
		if( i ) {
			Com_Printf("'Enhanced' R1Q2 servers are not supported, falling back to protocol 34.\n" );
			CL_Disconnect();		
			cls.serverProtocol = PROTOCOL_VERSION_DEFAULT;
			CL_Reconnect_f ();
			return false;
		}
		i = MSG_ReadShort(msg);
		if (i < PROTOCOL_VERSION_R1Q2_MINIMUM || i > PROTOCOL_VERSION_R1Q2_CURRENT)
		{
			if (cl.attractloop)
			{
				if ( i < PROTOCOL_VERSION_R1Q2_MINIMUM )
					Com_Printf("This demo was recorded with an earlier version of the R1Q2 protocol. It may not play back properly.\n");
				else
					Com_Printf("This demo was recorded with a later version of the R1Q2 protocol. It may not play back properly.\n");
			}
			else
			{
				if( i < PROTOCOL_VERSION_R1Q2_MINIMUM ) {
					Com_Printf("Server uses OLDER minor R1Q2 protocol version than minimum supported (%i < %i), falling back to protocol 34.\n", i, PROTOCOL_VERSION_R1Q2_MINIMUM);
					CL_Disconnect();
					cls.serverProtocol = PROTOCOL_VERSION_DEFAULT;
					CL_Reconnect_f ();
					return false;
				}
				Com_Printf("Server uses NEWER minor R1Q2 protocol version (%i > %i), some features will be unavailable.\n", i, PROTOCOL_VERSION_R1Q2_CURRENT);
			}
		}

		if (i >= PROTOCOL_VERSION_R1Q2_STRAFE) {
			MSG_ReadByte(msg);
			cl.pmp.strafeHack = MSG_ReadByte(msg);
		}

		cl.pmp.speedMultiplier = 2;
		cls.protocolVersion = i;
	}


	if (cl.playernum == -1)
	{	// playing a cinematic or showing a pic, not a level
		SCR_PlayCinematic (str);
	}
	else
	{
		// seperate the printfs so the server message can have a color
		Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
		Com_Printf ("%c%s\n", 2, str);

		// need to prep refresh at next oportunity
		cl.refresh_prepped = false;

        if((unsigned)cl.playernum >= MAX_CLIENTS) {
            cl.playernum = ( MAX_CLIENTS - 1 );
        }
	}

	return true;
}

/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline (sizebuf_t *msg)
{
	entity_state_t	*es;
	unsigned int	bits;
	int				newnum;

	newnum = CL_ParseEntityBits(msg, &bits);

	es = &cl_entities[newnum].baseline;
	MSG_ParseDeltaEntity(msg, NULL, es, newnum, bits, cls.protocolVersion);
}

void CL_ParseZPacket (sizebuf_t *msg)
{
	byte buff_in[MAX_MSGLEN];
	byte buff_out[0xFFFF];
	sizebuf_t sb;
	uint16 compressed_len = MSG_ReadShort (msg);
	uint16 uncompressed_len = MSG_ReadShort (msg);
	
	if (uncompressed_len <= 0)
		Com_Error (ERR_DROP, "CL_ParseZPacket: uncompressed_len <= 0");

	if (compressed_len <= 0)
		Com_Error (ERR_DROP, "CL_ParseZPacket: compressed_len <= 0");

	MSG_ReadData (msg, buff_in, compressed_len);

	SZ_Init (&sb, buff_out, uncompressed_len);
	sb.cursize = ZLibDecompress (buff_in, compressed_len, buff_out, uncompressed_len, -15);

	CL_ParseServerMessage(&sb);

	Com_DPrintf ("Got a ZPacket, %d->%d\n", uncompressed_len + 4, compressed_len);
}

/*
================
CL_LoadClientinfo

================
*/
void CL_LoadClientinfo (clientinfo_t *ci, char *s)
{
	int			i = 0;
	char		*t;
	char		model_name[MAX_QPATH];
	char		skin_name[MAX_QPATH];
	char		model_filename[MAX_QPATH];
	char		skin_filename[MAX_QPATH];
	char		weapon_filename[MAX_QPATH];

	Q_strncpyz (ci->cinfo, s, sizeof(ci->cinfo));

	// isolate the player's name
	Q_strncpyz (ci->name, s, sizeof(ci->name));

	t = strchr(s, '\\');
	if (t) {
		if (t - s >= sizeof(ci->name)-1) {
			i = -1;
		} else {
			ci->name[t-s] = 0;
			s = t+1;
		}
	}
	t = s;
	while (*t) {
		if (*t <= 32) {
			i = -1;
			break;
		}
		t++;
	}

	if (cl_noskins->integer || *s == 0 || i == -1)
	{
		strcpy (model_filename, "players/male/tris.md2");
		strcpy (weapon_filename, "players/male/weapon.md2");
		strcpy (skin_filename, "players/male/grunt.pcx");
		strcpy (ci->iconname, "/players/male/grunt_i.pcx");
		ci->model = R_RegisterModel (model_filename);
		memset(ci->weaponmodel, 0, sizeof(ci->weaponmodel));
		ci->weaponmodel[0] = R_RegisterModel (weapon_filename);
		ci->skin = R_RegisterSkin (skin_filename);
		ci->icon = Draw_FindPic (ci->iconname);
	}
	else
	{
		// isolate the model name
		Q_strncpyz (model_name, s, sizeof(model_name));
		t = strchr(model_name, '/');
		if (!t)
			t = strchr(model_name, '\\');
		
		if (!t) {
			memcpy (model_name, "male\0grunt\0\0\0\0\0\0", 16);
			s = "male\\grunt\0";
		}
		else {
			*t = 0;
		}

		// isolate the skin name
		Q_strncpyz(skin_name, s + strlen(model_name) + 1, sizeof(skin_name));

		// model file
		Com_sprintf (model_filename, sizeof(model_filename), "players/%s/tris.md2", model_name);
		ci->model = R_RegisterModel (model_filename);
		if (!ci->model)
		{
			strcpy(model_name, "male");
			strcpy(model_filename, "players/male/tris.md2");
			ci->model = R_RegisterModel (model_filename);
		}

		// skin file
		Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/%s.pcx", model_name, skin_name);
		ci->skin = R_RegisterSkin (skin_filename);

		// if we don't have the skin and the model wasn't male,
		// see if the male has it (this is for CTF's skins)
 		if (!ci->skin && Q_stricmp(model_name, "male"))
		{
			// change model to male
			strcpy(model_name, "male");
			strcpy(model_filename, "players/male/tris.md2");
			ci->model = R_RegisterModel (model_filename);

			// see if the skin exists for the male model
			Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/%s.pcx", model_name, skin_name);
			ci->skin = R_RegisterSkin (skin_filename);
		}

		// if we still don't have a skin, it means that the male model didn't have
		// it, so default to grunt
		if (!ci->skin) {
			// see if the skin exists for the male model
			Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/grunt.pcx", model_name);
			ci->skin = R_RegisterSkin (skin_filename);
		}

		// weapon file
		for (i = 0; i < num_cl_weaponmodels; i++) {
			Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/%s/%s", model_name, cl_weaponmodels[i]);
			ci->weaponmodel[i] = R_RegisterModel(weapon_filename);
			if (!ci->weaponmodel[i] && strcmp(model_name, "cyborg") == 0) {
				// try male
				Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/male/%s", cl_weaponmodels[i]);
				ci->weaponmodel[i] = R_RegisterModel(weapon_filename);
			}
			if (!cl_vwep->integer)
				break; // only one when vwep is off
		}

		// icon file
		Com_sprintf (ci->iconname, sizeof(ci->iconname), "/players/%s/%s_i.pcx", model_name, skin_name);
		ci->icon = Draw_FindPic (ci->iconname);
	}

	// must have loaded all data types to be valud
	if (!ci->skin || !ci->icon || !ci->model || !ci->weaponmodel[0])
	{
		ci->skin = NULL;
		ci->icon = NULL;
		ci->model = NULL;
		ci->weaponmodel[0] = NULL;
		return;
	}
}

/*
================
CL_ParseClientinfo

Load the skin, icon, and model for a client
================
*/
void CL_ParseClientinfo (int player)
{
	char			*s;
	clientinfo_t	*ci;

	s = cl.configstrings[player+CS_PLAYERSKINS];

	ci = &cl.clientinfo[player];

	CL_LoadClientinfo (ci, s);
}


/*
================
CL_ParseConfigString
================
*/
void CL_ParseConfigString (sizebuf_t *msg)
{
	int		i, length;
	char	*s;
	char	olds[MAX_QPATH];

	i = MSG_ReadShort (msg);
	if ((unsigned)i >= MAX_CONFIGSTRINGS)
		Com_Error (ERR_DROP, "configstring > MAX_CONFIGSTRINGS");

	Q_strncpyz (olds, cl.configstrings[i], sizeof(olds));

	s = MSG_ReadString(msg);

	length = strlen(s);

	if (i != CS_NAME && i < CS_GENERAL)
	{
		if (i >= CS_STATUSBAR && i < CS_AIRACCEL)
		{
			Q_strncpyz(cl.configstrings[i], s, MAX_QPATH * (CS_AIRACCEL - i));
		}
		else
		{
			if (length >= MAX_QPATH)
				Com_Printf ("WARNING: Configstring %d of length %d exceeds MAX_QPATH.\n", i, length);
			Q_strncpyz(cl.configstrings[i], s, MAX_QPATH);
		}
	}
	else
	{
		Q_strncpyz(cl.configstrings[i], s, MAX_QPATH * (MAX_CONFIGSTRINGS - i));
	}

	// do something apropriate
	if(i == CS_AIRACCEL)
	{
		cl.pmp.airaccelerate = atoi(cl.configstrings[CS_AIRACCEL]) ? true : false;
	}
	else if (i >= CS_LIGHTS && i < CS_LIGHTS+MAX_LIGHTSTYLES)
	{
		CL_SetLightstyle (i - CS_LIGHTS);
	}
	else if (i == CS_CDTRACK)
	{
#ifdef CD_AUDIO
		if (cl.refresh_prepped)
			CDAudio_Play (atoi(cl.configstrings[CS_CDTRACK]), true);
#endif
	}
	else if (i >= CS_MODELS && i < CS_MODELS+MAX_MODELS)
	{
		if( i == CS_MODELS + 1 ) {
			if( length > 9 ) {
				Q_strncpyz( cls.mapname, s + 5, sizeof( cls.mapname ) ); // skip "maps/"
				cls.mapname[strlen( cls.mapname ) - 4] = 0; // cut off ".bsp"
			}

		}
		if (cl.refresh_prepped)
		{
			cl.model_draw[i-CS_MODELS] = R_RegisterModel (cl.configstrings[i]);
			if (cl.configstrings[i][0] == '*')
				cl.model_clip[i-CS_MODELS] = CM_InlineModel (cl.configstrings[i]);
			else
				cl.model_clip[i-CS_MODELS] = NULL;
		}
	}
	else if (i >= CS_SOUNDS && i < CS_SOUNDS+MAX_MODELS)
	{
		if (cl.refresh_prepped)
			cl.sound_precache[i-CS_SOUNDS] = S_RegisterSound (cl.configstrings[i]);
	}
	else if (i >= CS_IMAGES && i < CS_IMAGES+MAX_MODELS)
	{
		if (cl.refresh_prepped)
			cl.image_precache[i-CS_IMAGES] = Draw_FindPic (cl.configstrings[i]);
	}
	else if (i == CS_MAXCLIENTS)
	{
		cl.maxclients = atoi(cl.configstrings[CS_MAXCLIENTS]);
		clamp(cl.maxclients, 0, MAX_CLIENTS);
	}
	else if (i >= CS_PLAYERSKINS && i < CS_PLAYERSKINS+MAX_CLIENTS)
	{
		if (cl.refresh_prepped && strcmp(olds, s))
			CL_ParseClientinfo (i-CS_PLAYERSKINS);
	}
}


/*
=====================================================================

ACTION MESSAGES

=====================================================================
*/

/*
==================
CL_ParseStartSoundPacket
==================
*/
void CL_ParseStartSoundPacket(sizebuf_t *msg)
{
    vec3_t  pos_v;
	float	*pos;
    int 	channel, ent;
    int 	sound_num, flags;
    float 	volume, attenuation, ofs;

	flags = MSG_ReadByte (msg);
	sound_num = MSG_ReadByte (msg);
	if( sound_num == -1 ) {
		Com_Error( ERR_DROP, "CL_ParseStartSoundPacket: read past end of message" );
	}

    if (flags & SND_VOLUME)
		volume = MSG_ReadByte(msg) * ONEDIV255;
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;
	
    if (flags & SND_ATTENUATION)
		attenuation = MSG_ReadByte(msg) * ONEDIV64;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;	

    if (flags & SND_OFFSET)
		ofs = MSG_ReadByte(msg) / 1000.0f;
	else
		ofs = 0;

	if (flags & SND_ENT)
	{	// entity reletive
		channel = MSG_ReadShort(msg); 
		ent = channel>>3;
		if ((unsigned)ent >= MAX_EDICTS)
			Com_Error (ERR_DROP,"CL_ParseStartSoundPacket: ent = %i", ent);

		channel &= 7;
	}
	else
	{
		ent = 0;
		channel = 0;
	}

	if (flags & SND_POS)
	{	// positioned in space
		MSG_ReadPos (msg, pos_v);
 
		pos = pos_v;
	}
	else	// use entity number
		pos = NULL;

	if (!cl.sound_precache[sound_num])
		return;

	S_StartSound (pos, ent, channel, cl.sound_precache[sound_num], volume, attenuation, ofs);
}       


void SHOWNET(sizebuf_t *msg, const char *s)
{
	if (cl_shownet->integer >= 2)
		Com_Printf ("%3i:%s\n", msg->readcount-1, s);
}

static void CL_ParsePrint( char *s, int i )
{
	char text[MAX_STRING_CHARS], *timestamp = NULL;
	const char *string;
	int color = -1, highlight = 0, skip = 0, client = 0, mm2 = 0;
	int c, l = 0;

	string = s;
	while((c = *string++ & 127) && l < sizeof(text)-1) {
		if(c > 31)
			text[l++] = tolower(c);
	}
	text[l] = 0;

	if (i != PRINT_CHAT)
	{
		CL_HighlightNames(s);

		Com_Printf("%s%s", CL_Timestamp(false), s);

		if(!cl.attractloop) {
			if (i == PRINT_HIGH)
				CL_ParseAutoScreenshot(text);		
		
			Cmd_ExecTrigger(text); //Triggers
		}
		return;
	}

	if(CL_ChatParse(s, &client, &skip, &mm2))
	{
		clamp(skip, 0, l);
		if(!strcmp(cl.clientinfo[client].name, info_name->string)) //Own chat text
		{
			if(cl_mychatcolor->integer && cl_textcolors->integer)
				color = cl_mychatcolor->integer;
		}
		else
		{
			if(CL_IgnoreNick(cl.clientinfo[client].name)) //do not show ignored msg
				return;
			
			if(CL_IgnoreText(text))
				return;

			if(cl_highlightmode->integer)
				highlight = CL_Highlight(text, &color);
			else
				highlight = CL_Highlight(text+skip, &color);
		}
		if(!cl.attractloop)
			CL_VersionReply(text+skip);
	}
	else if (!cl.attractloop)
	{
		if(CL_IgnoreText(text))
			return;
		
		highlight = CL_Highlight(text, &color);
		CL_VersionReply(text);
	}

	if(highlight & 1)
		S_StartLocalSound ("misc/talk1.wav");
	else
		S_StartLocalSound ("misc/talk.wav");

	if (!cl_chat_notify->integer)
		Con_SkipNotify( true );

	timestamp = CL_Timestamp(true);
	if(color > -1) {
		clamp (color, 0, 7);
		SCR_AddToChatHUD(s, color, mm2); //Chathud
		Com_Printf (S_ONE_COLOR "^%i%s%s", color, timestamp, s);
	} else {
		SCR_AddToChatHUD(s, 7, mm2); //Chathud
		Com_Printf("\1%s%s", timestamp, s);
	}

	Con_SkipNotify( false );
}

void CL_ParseCenterPrint(const char *s)
{
	char text[MAX_STRING_CHARS];
	const char *string;
	int c, l = 0;

	SCR_CenterPrint (s);

	string = s;
	while((c = *string++ & 127) && l < sizeof(text)-1) {
		if(c > 31)
			text[l++] = c;
	}
	text[l] = 0;

	if (!strcmp(text, "ACTION!")) //Hack to show roundtime in aq2 mod
		cls.roundtime = cl.time;

	if(!cl.attractloop)
		Cmd_ExecTrigger(text); //Triggers
}

static void CL_ParseSetting (void)
{
	uint32	setting, value;

	setting = MSG_ReadLong (&net_message);
	value = MSG_ReadLong (&net_message);

	if (setting >= SVSET_MAX)
		return;

	cl.settings[setting] = value;
}

/*
=====================
CL_ParseServerMessage
=====================
*/
#define WRITEDEMOMSG \
	if(cls.demorecording && cls.serverProtocol > PROTOCOL_VERSION_DEFAULT) \
		CL_WriteDemoMessage (msg->data + oldReadCount, msg->readcount - oldReadCount, false);

void CL_ParseServerMessage (sizebuf_t *msg)
{
	int			cmd, extrabits = 0;
	char		*s;
	int			i, oldReadCount;


    if (cls.doscreenshot == 2)
	{
		cls.doscreenshot = 0;
		if (cl_autoscreenshot->integer == 1)
			  Cbuf_AddText("screenshot\n");
		else if (cl_autoscreenshot->integer == 2)
			  Cbuf_AddText("screenshotjpg\n");
    }

//
// if recording demos, copy the message out
//
	if (cl_shownet->integer == 1)
		Com_Printf ("%i ", msg->cursize);
	else if (cl_shownet->integer >= 2)
		Com_Printf ("------------------\n");

//
// parse the message
//
	while (1)
	{
		if (msg->readcount > msg->cursize) {
			Com_Error (ERR_DROP,"CL_ParseServerMessage: Bad server message (%d>%d)", msg->readcount, msg->cursize);
			break;
		}

		oldReadCount = msg->readcount;

		cmd = MSG_ReadByte(msg);
		if (cmd == -1) {
			SHOWNET(msg, "END OF MESSAGE");
			break;
		}

		//r1: more hacky bit stealing in the name of bandwidth
		extrabits = cmd >> SVCMD_BITS;
		cmd &= SVCMD_MASK;

		if (cl_shownet->integer > 1) {
			if (!svc_strings[cmd])
				Com_Printf ("%3i:BAD CMD %i\n", msg->readcount-1, cmd);
			else
				SHOWNET(msg, svc_strings[cmd]);
		}
	
	// other commands
		switch (cmd)
		{
		case svc_frame:
			CL_ParseFrame (msg, extrabits);
			//CL_ParseFrame ();
			break;

		case svc_muzzleflash:
			CL_ParseMuzzleFlash (msg);
			WRITEDEMOMSG
			break;

		case svc_muzzleflash2:
			CL_ParseMuzzleFlash2 (msg);
			WRITEDEMOMSG
			break;

		case svc_temp_entity:
			CL_ParseTEnt (msg);
			WRITEDEMOMSG
			break;

		case svc_sound:
			CL_ParseStartSoundPacket(msg);
			WRITEDEMOMSG
			break;

		case svc_print:
			i = MSG_ReadByte( msg );
			s = MSG_ReadString( msg );
			WRITEDEMOMSG
			CL_ParsePrint(s, i);
			break;
			
		case svc_centerprint:
			s = MSG_ReadString (msg);
			CL_ParseCenterPrint(s);
			WRITEDEMOMSG
			break;
			
		case svc_stufftext:
			s = MSG_ReadString (msg);
			if (Q_stristr( s, "loc_save" ) || Q_stristr( s, "cfg_save" ) ) {
				Com_DPrintf( "ignored stufftext: %s\n", s );
			} else if (cl.attractloop && 
				Q_stricmp( s, "precache\n" ) && Q_stricmp( s, "changing\n" ) &&
				Q_stricmp( s, "reconnect\n" ) && Q_strnicmp( s, "play ", 5 ) ) {
				Com_DPrintf ("WARNING: Demo tried to execute command '%s', ignored.\n", s);
			} else {
				Com_DPrintf ("stufftext: %s\n", s);
				Cbuf_AddText(s);
				WRITEDEMOMSG
			}
			break;
			
		case svc_serverdata:
			Cbuf_Execute ();		// make sure any stuffed commands are done
			if (!CL_ParseServerData(msg))
				return;
			WRITEDEMOMSG
			break;
			
		case svc_configstring:
			CL_ParseConfigString (msg);
			WRITEDEMOMSG
			break;
			
		case svc_spawnbaseline:
			CL_ParseBaseline (msg);
			WRITEDEMOMSG
			break;

		case svc_download:
			CL_ParseDownload (msg, false);
			WRITEDEMOMSG
			break;

		case svc_inventory:
			CL_ParseInventory (msg);
			WRITEDEMOMSG
			break;

		case svc_layout:
			s = MSG_ReadString (msg);
			WRITEDEMOMSG
			Q_strncpyz (cl.layout, s, sizeof(cl.layout));
			break;

		// ************** r1q2 specific BEGIN ****************
		case svc_zpacket:
			CL_ParseZPacket(msg);
			break;

		case svc_zdownload:
			CL_ParseDownload(msg, true);
			break;

		case svc_playerupdate:
			break;

		case svc_setting:
			CL_ParseSetting ();
			break;
		// ************** r1q2 specific END ******************

		case svc_playerinfo:
		case svc_packetentities:
		case svc_deltapacketentities:
			Com_Error (ERR_DROP, "Out of place frame data");
			break;

		case svc_nop:
			break;
			
		case svc_reconnect:
			Com_Printf ("Server disconnected, reconnecting\n");
			if (cls.download) {
				//ZOID, close download
				fclose (cls.download);
				cls.download = NULL;
			}

			cls.downloadname[0] = 0;

			cls.state = ca_connecting;
			cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately

			CL_StopAutoRecord();
            SCR_ClearChatHUD_f();
			break;

		case svc_disconnect:
			WRITEDEMOMSG
			Com_Error (ERR_DISCONNECT, "Server disconnected\n");

			SCR_ClearChatHUD_f();
			break;

		default:
			Com_Error (ERR_DROP,"CL_ParseServerMessage: Unknown command byte %d (0x%.2x)", cmd, cmd);
			break;
		}
	}

	//flush this frame
	if(cls.demorecording && cls.serverProtocol > PROTOCOL_VERSION_DEFAULT)
		CL_WriteDemoMessage (NULL, 0, true);
}

//  reply to !version
void CL_VersionReply (const char *s)
{
	if (!strstr(s, "!version"))
		return;
	
	if (cls.lastSpamTime == 0 || cls.realtime > cls.lastSpamTime + 150000)
	{
		cls.spamTime = cls.realtime + (unsigned int)(random() * 1500);
	}
}

qboolean CL_ChatParse (const char *s, int *clinu, int *skip2, int *mm2) //Seperates nick & msg from chat msg
{
	int i, client = -1, skip = 0, nLen;
	const char *tmp, *start = NULL;

	for(i = 0; i < cl.maxclients; i++)
	{ 
		if(cl.clientinfo[i].name[0])
		{
			tmp = strstr(s, cl.clientinfo[i].name);

			if(tmp && (!start || tmp < start || (tmp == start && strlen(cl.clientinfo[i].name) > strlen(cl.clientinfo[client].name))) )
			{ 
				client = i; 
				start = tmp; 
			}
		}
	}

	if(!start)
		return false;

	nLen = strlen(cl.clientinfo[client].name);

	skip = start - s;
	if(skip > 0) {
		if(s[skip - 1] == '(' && s[skip + nLen] == ')') //Hack and only work on aq2?
			*mm2 = 1;
	}

	// skip the name 
	start += nLen;

	// walk to a space (after the colon) 
	while(*start && *start != ' ')
		start++;

	if(!*start)
		return false;

	skip = start - s;

	*skip2 = skip;
	*clinu = client;

	return true;
}

void CL_HighlightNames( char *s )
{
	char *t;
	int i, j, nro = 0, temp = 0, len;
	int ord[MAX_CLIENTS] = { 0 };

	if(!cl_highlightnames->integer)
		return;
	
	for(i = 0; i < cl.maxclients; i++) {
		if(cl.clientinfo[i].name[0] && cl.clientinfo[i].name[1])
			ord[nro++] = i;
	}

	//Put nick list to order by lenght, longest first
	for(i = 0; i < nro; i++) {
		for(j = i+1; j < nro; j++) {
			if(strlen(cl.clientinfo[ord[j]].name) > strlen(cl.clientinfo[ord[i]].name) )
			{
				temp = ord[i];
				ord[i] = ord[j];
				ord[j] = temp;
			}
		}
	}
	
	// highlight peoples names
	for(i = 0; i < nro; i++)
	{
		char *tmp = strstr( s, cl.clientinfo[ord[i]].name );

		if(tmp)
		{
			len = strlen(cl.clientinfo[ord[i]].name);
			for(t = tmp; t < tmp + len; t++)
				*t |= 128;
		}
	}
}

typedef struct highlight_s
{
	struct highlight_s *next;
	char *text;
	int color;
} highlight_t;

static highlight_t *cl_highlights = NULL;


static void CL_Highlight_f( void )
{
	highlight_t *entry;
	const char *s;
	int color = -1, tLen;

	if (Cmd_Argc() != 2 && Cmd_Argc() != 3)
	{
		Com_Printf("Usage: %s <text> [color]\n", Cmd_Argv(0));
		Com_Printf("Current text highlights:\n");
		Com_Printf("------------------------\n");
		for(entry = cl_highlights; entry; entry = entry->next)
			Com_Printf("%s\n", entry->text);

		return;
	}
	
	s = Cmd_Argv(1);
	tLen = strlen(s);
	if(tLen < 3) {
		Com_Printf("Text is too short.\n");
		return;
	}

	if(Cmd_Argc() > 2)
		color = atoi(Cmd_Argv(2))&7;

	for(entry = cl_highlights; entry; entry = entry->next) {
		if(!strcmp(entry->text, s))
		{
			if(entry->color != color) //Update the color
				entry->color = color;
			//else
				//Com_Printf("Same highlight already exists\n");
			return;
		}
	}

	entry = Z_TagMalloc(sizeof(highlight_t) + tLen+1, TAG_CL_IGNORE);
	entry->text = (char *)((byte *)entry + sizeof(highlight_t));
	strcpy(entry->text, s);
	entry->color = color;
	entry->next = cl_highlights;
	cl_highlights = entry;
}

static void CL_UnHighlight_f( void )
{
	highlight_t *entry, *next, **back;
	int count = 0;
	const char *s;

	if(!cl_highlights) {
		Com_Printf("No text highlights\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: %s <text/all>\n", Cmd_Argv(0));
		Com_Printf("Current text highlights:\n");
		Com_Printf("------------------------\n");
		for(entry = cl_highlights; entry; entry = entry->next)
			Com_Printf("%s\n", entry->text);

		return;
	}

	s = Cmd_Argv(1);
	if(!Q_stricmp(s, "all"))
	{
		for(entry = cl_highlights; entry; entry = next)
		{
			next = entry->next;
			Z_Free(entry);
			count++;
		}
		cl_highlights = NULL;
		if(count)
			Com_Printf("Removed %i highlights\n", count);
		return;
	}

	back = &cl_highlights;
	for(;;)
	{
		entry = *back;
		if(!entry)
		{
			Com_Printf ("Highlight '%s' not found.\n", s);
			return;
		}
		if(!Q_stricmp(s, entry->text))
		{
			*back = entry->next;
			Com_Printf ("Removed highlight '%s'\n", entry->text);
			Z_Free(entry);
			return;
		}
		back = &entry->next;
	}
}


int CL_Highlight (const char *s, int *color)
{
	highlight_t *entry;
	const char *text;

	if(!cl_highlight->integer)
		return 0;

	for (entry = cl_highlights; entry; entry = entry->next) 
	{ 
		text = Cmd_MacroExpandString(entry->text);
		if(text && Com_WildCmp(text, s)) {
			if(cl_highlight->integer & 2 && cl_textcolors->integer)
			{
				if(entry->color > -1)
					*color = entry->color;
				else
					*color = cl_highlightcolor->integer;
			}
			return cl_highlight->integer;
		}
	}

	return 0;
}

char *CL_Timestamp( qboolean chat )
{
    static char timebuf[32];
    time_t clock;

	if(!cl_timestamps->integer || (!chat && cl_timestamps->integer < 2))
		return "";

	time( &clock );
	strftime( timebuf, sizeof(timebuf), cl_timestampsformat->string, localtime( &clock ) );
	Q_strncatz(timebuf, " ", sizeof(timebuf));
	return timebuf;
}

typedef struct ignore_s
{
	struct ignore_s *next;
	char *text;
} ignore_t;


void CL_PrintIgnores(ignore_t *list, qboolean num)
{
	int count = 0;
	ignore_t *entry;

	for(entry = list; entry; entry = entry->next)
	{
		if(num)
			Com_Printf("%2i %s\n", ++count, entry->text);
		else
			Com_Printf("%s\n", entry->text);
	}
}

void CL_AddIgnore(ignore_t **ignoreList, const char *text)
{
	ignore_t *entry;

	entry = Z_TagMalloc(sizeof(ignore_t) + strlen(text)+1, TAG_CL_IGNORE);
	entry->text = (char *)((byte *)entry + sizeof(ignore_t));
	strcpy(entry->text, text);
	entry->next = *ignoreList;
	*ignoreList = entry;
}

void CL_RemoveIgnoreAll(ignore_t **ignoreList)
{
	ignore_t *entry, *next;
	int count = 0;

	for(entry = *ignoreList; entry; entry = next)
	{
		next = entry->next;
		Z_Free(entry);
		count++;
	}
	*ignoreList = NULL;

	if(count)
		Com_Printf("Removed %i ignores\n", count);
}

void CL_RemoveIgnoreNum(ignore_t **ignoreList, int num)
{
	ignore_t *entry, **back;
	int count = 0;

	back = ignoreList;
	for(;;)
	{
		entry = *back;
		if(!entry)
		{
			Com_Printf ("Ignore '%i' not found.\n", num);
			return;
		}
		if(++count == num)
		{
			*back = entry->next;
			Com_Printf ("Removed ignore '%s'\n", entry->text);
			Z_Free(entry);
			return;
		}
		back = &entry->next;
	}
}

void CL_RemoveIgnoreText(ignore_t **ignoreList, const char *s)
{
	ignore_t *entry, **back;

	back = ignoreList;
	for(;;)
	{
		entry = *back;
		if(!entry)
		{
			Com_Printf ("Ignore '%s' not found.\n", s);
			return;
		}
		if(!strcmp(entry->text, s))
		{
			*back = entry->next;
			Com_Printf ("Removed ignore '%s'\n", entry->text);
			Z_Free(entry);
			return;
		}
		back = &entry->next;
	}
}

static ignore_t *cl_textIgnores = NULL;
cvar_t *cl_ignoremode;

static void CL_IgnoreText_f(void) 
{
	ignore_t *list;
	char *text;

	if (Cmd_Argc() < 2)
	{
		Com_Printf("Usage: %s <text>\n", Cmd_Argv(0));
		if(!cl_textIgnores)
			return;

		Com_Printf("Current text ignores:\n");
		Com_Printf("---------------------\n");
		CL_PrintIgnores(cl_textIgnores, false);

		if(!cl_ignoremode->integer)
			Com_Printf("Ignores is now disabled, use cl_ignoremode to enable.\n");
		else if(cl_ignoremode->integer == 2)
			Com_Printf("You have set inverse mode, so text which not match any of these is ignored.\n");
		return; 
	}

	text = Cmd_ArgsFrom(1);
	if(strlen(text) < 3)
	{
		Com_Printf("Text is too short.\n");
		return;
	}

	for (list = cl_textIgnores; list; list = list->next) 
	{ 
		if(!strcmp(list->text, text))
		{
			//Com_Printf("Exactly same ignore text exists.\n");
			return;
		}
	}

	CL_AddIgnore(&cl_textIgnores, text);
}

static void CL_UnIgnoreText_f( void )
{
	if(!cl_textIgnores)
	{
		Com_Printf("Text ignore list is empty\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: %s <text/all>\n", Cmd_Argv(0));
		Com_Printf("Current text ignores:\n");
		Com_Printf("---------------------\n");
		CL_PrintIgnores(cl_textIgnores, false);

		if(!cl_ignoremode->integer)
			Com_Printf("Ignores is now disabled, use cl_ignoremode cvar to enable.\n");
		else if(cl_ignoremode->integer == 2)
			Com_Printf("You have set inverse mode, so text which not match any of these is ignored.\n");
		return;
	}

	if(!Q_stricmp(Cmd_Argv(1), "all"))
	{
		CL_RemoveIgnoreAll(&cl_textIgnores);
		return;
	}

	CL_RemoveIgnoreText(&cl_textIgnores, Cmd_Argv(1));
}

qboolean CL_IgnoreText(const char *s)
{
	ignore_t *list;
	const char *text;
	qboolean ignore = false;

	if(!cl_ignoremode->integer)
		return false;

	if(cl_ignoremode->integer == 2)
		ignore = true;

	for (list = cl_textIgnores; list; list = list->next) 
	{ 
		text = Cmd_MacroExpandString(list->text);
		if(text && Com_WildCmp(text, s))
			return !ignore;
	}

	return ignore;
}


//Ignorenick and Unignorenick
static ignore_t *cl_nickIgnores = NULL;

static void CL_IgnoreNick_f(void) 
{
	int num, i;
	char *nick;

	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: \"%s <id>\" to ignore player name.\n", Cmd_Argv(0));
		Com_Printf("Current list of players in server:\n");
		Com_Printf("id  Name             Ignored\n");
		Com_Printf("--  ---------------  -------\n");
		for (i=0;i<cl.maxclients;i++) 
		{ 
			if (cl.clientinfo[i].name[0] && strcmp(cl.clientinfo[i].name, info_name->string)) 
			{ 
				Com_Printf("%2i  %-15s", i, cl.clientinfo[i].name);

				if(CL_IgnoreNick(cl.clientinfo[i].name))
					Com_Printf("    yes");

				Com_Printf("\n");
			} 
		}
		return; 
	}

	num = atoi(Cmd_Argv(1));
	if((unsigned)num >= cl.maxclients) {
		Com_Printf("Bad player id\n");
		return;
	}

	nick = cl.clientinfo[num].name;
	if (!*nick) { 
		Com_Printf("Cant find player with id '%i'\n", num);
		return;
	}

	if(!strcmp(nick, info_name->string)) {
		Com_Printf("You cant ignore yourself!\n");
		return;
	}

	if(CL_IgnoreNick(nick)) {
		Com_Printf("Player [%s] is already ignored.\n", nick);
		return;
	}

	CL_AddIgnore(&cl_nickIgnores, nick);
	Com_Printf("Player [%s] is now ignored!\n", nick);
}

static void CL_UnignoreNick_f(void)
{
	if(!cl_nickIgnores) {
		Com_Printf("Ignorelist is empty.\n");
		return;
	}

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <id/all>\n", Cmd_Argv(0));
		Com_Printf("Current ignores:\n");
		Com_Printf("id  Name\n");
		Com_Printf("--  ---------------\n");
		CL_PrintIgnores(cl_nickIgnores, true);
		return; 
	}

	if(!Q_stricmp(Cmd_Argv(1), "all")) {
		CL_RemoveIgnoreAll(&cl_nickIgnores);
		return;
	}

	CL_RemoveIgnoreNum(&cl_nickIgnores, atoi(Cmd_Argv(1)));
}

qboolean CL_IgnoreNick(const char *s)
{
	ignore_t *list;

	for (list = cl_nickIgnores; list; list = list->next) { 
		if(!strcmp(s, list->text))
			return true;
	}
	return false;
}

static cvar_t	*cl_customlimitmsg;

void CL_ParseAutoScreenshot (const char *s)
{
	if(!cl_autoscreenshot->integer)
		return;

	if (strstr(s, "timelimit hit") ||
		strstr(s, "capturelimit hit") ||
		strstr(s, "fraglimit hit") ||
		strstr(s, cl_customlimitmsg->string) ) {
		SCR_ClearChatHUD_f();
		cls.doscreenshot = 1;
	}
}

static void OnChange_Color (cvar_t *self, const char *oldValue)
{
	if(self->integer < 0)
		Cvar_Set(self->name, "0");
	else if(self->integer > 7)
		Cvar_Set(self->name, "7");
}

void CL_InitParse( void )
{
	cl_timestamps		= Cvar_Get( "cl_timestamps","0", CVAR_ARCHIVE );
	cl_timestampsformat = Cvar_Get( "cl_timestampsformat", "[%H:%M:%S]", 0 );

	cl_ignoremode		= Cvar_Get( "cl_ignoremode", "1", 0 );
	cl_highlight		= Cvar_Get( "cl_highlight",  "0", CVAR_ARCHIVE );
	cl_highlightmode	= Cvar_Get( "cl_highlightmode", "0", CVAR_ARCHIVE );
	cl_highlightcolor	= Cvar_Get( "cl_highlightcolor", "0", CVAR_ARCHIVE );
	cl_highlightnames	= Cvar_Get( "cl_highlightnames",  "0", CVAR_ARCHIVE );

	cl_textcolors		= Cvar_Get( "cl_textcolors", "0", CVAR_ARCHIVE );
	cl_mychatcolor		= Cvar_Get( "cl_mychatcolor", "0", CVAR_ARCHIVE );
	cl_chat_notify		= Cvar_Get( "cl_chat_notify", "1", 0 );

	cl_highlightcolor->OnChange = OnChange_Color;
	cl_mychatcolor->OnChange = OnChange_Color;
	OnChange_Color(cl_highlightcolor, cl_highlightcolor->resetString);
	OnChange_Color(cl_mychatcolor, cl_mychatcolor->resetString);

	//hacky shit from q2ace
	cl_autoscreenshot = Cvar_Get( "cl_autoscreenshot", "0", 0 );
	cl_customlimitmsg = Cvar_Get( "cl_customlimitmsg", "timelimit hit", 0 );

	Cmd_AddCommand( "highlight", CL_Highlight_f );
	Cmd_AddCommand( "unhighlight", CL_UnHighlight_f );

	Cmd_AddCommand( "ignoretext", CL_IgnoreText_f );
	Cmd_AddCommand( "unignoretext", CL_UnIgnoreText_f );

	Cmd_AddCommand( "ignorenick", CL_IgnoreNick_f );
	Cmd_AddCommand( "unignorenick", CL_UnignoreNick_f );
}
