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
// cl_main.c  -- client main loop

#include "client.h"

cvar_t	*freelook;

cvar_t	*adr0;
cvar_t	*adr1;
cvar_t	*adr2;
cvar_t	*adr3;
cvar_t	*adr4;
cvar_t	*adr5;
cvar_t	*adr6;
cvar_t	*adr7;
cvar_t	*adr8;

cvar_t	*cl_stereo_separation;
cvar_t	*cl_stereo;

cvar_t	*rcon_client_password;
cvar_t	*rcon_address;

cvar_t	*cl_noskins;
//cvar_t	*cl_autoskins;
cvar_t	*cl_footsteps;
cvar_t	*cl_timeout;
cvar_t	*cl_predict;
cvar_t	*r_maxfps;
cvar_t	*cl_maxfps;
cvar_t	*cl_async;
cvar_t	*cl_gun;
cvar_t	*cl_gunalpha;
cvar_t	*cl_gun_x, *cl_gun_y, *cl_gun_z;

cvar_t	*cl_add_particles;
cvar_t	*cl_add_lights;
cvar_t	*cl_add_entities;
cvar_t	*cl_add_blend;

cvar_t	*cl_shownet;
cvar_t	*cl_showmiss;
cvar_t	*cl_showclamp;

cvar_t	*cl_paused;
cvar_t	*cl_timedemo;

cvar_t	*lookspring;
cvar_t	*lookstrafe;
cvar_t	*sensitivity;

cvar_t	*m_pitch;
cvar_t	*m_yaw;
cvar_t	*m_forward;
cvar_t	*m_side;

cvar_t	*cl_lightlevel;

// userinfo
cvar_t	*info_password;
cvar_t	*info_spectator;
cvar_t	*info_name;
cvar_t	*info_skin;
cvar_t	*info_rate;
cvar_t	*info_fov;
cvar_t	*info_msg;
cvar_t	*info_hand;
cvar_t	*info_gender;
cvar_t	*gender_auto;

cvar_t	*cl_vwep;

void CL_WriteConfig_f (void);
void CL_InitParse( void );
//Added cvars  -Maniac
cvar_t	*cl_hudalpha;

cvar_t	*cl_protocol;
cvar_t	*cl_instantpacket;

client_static_t	cls;
client_state_t	cl;

centity_t		cl_entities[MAX_EDICTS];

entity_state_t	cl_parse_entities[MAX_PARSE_ENTITIES];

extern	cvar_t *allow_download;
extern	cvar_t *allow_download_players;
extern	cvar_t *allow_download_models;
extern	cvar_t *allow_download_sounds;
extern	cvar_t *allow_download_maps;

//======================================================================

typedef enum {
	REQ_FREE,
	REQ_STATUS,
	REQ_INFO,
	REQ_PING,
	REQ_RCON
} requestType_t;

typedef struct {
	requestType_t type;
	netadr_t adr;
	unsigned int time;
} request_t;

#define MAX_REQUESTS	32
#define REQUEST_MASK	( MAX_REQUESTS - 1 )

static request_t	clientRequests[MAX_REQUESTS];
static request_t	broadcastRequest;
static int			currentRequest;

static request_t *CL_AddRequest( const netadr_t *adr, requestType_t type ) {
	request_t *r;

	if( adr->type == NA_BROADCAST ) {
		r = &broadcastRequest;
		r->time = cls.realtime + 3000;
	} else {
		r = &clientRequests[currentRequest++ & REQUEST_MASK];
		r->time = cls.realtime + 6000;
	}

	r->adr = *adr;
	r->type = type;

	return r;
}

/*
===================
Cmd_ForwardToServer

adds the current command line as a clc_stringcmd to the client message.
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
void Cmd_ForwardToServer (void)
{
	char	*cmd;

	if(cls.demoplaying)
		return;

	cmd = Cmd_Argv(0);
	if (cls.state < ca_active || *cmd == '-' || *cmd == '+') {
		Com_Printf ("Unknown command \"%s\"\n", cmd);
		return;
	}

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, cmd);
	if (Cmd_Argc() > 1)
	{
		SZ_Print (&cls.netchan.message, " ");
		SZ_Print (&cls.netchan.message, Cmd_Args());
	}
}

/*void CL_Setenv_f( void ) {
	int argc = Cmd_Argc();

	if ( argc > 2 ) {
		char buffer[1000];
		int i;

		strcpy( buffer, Cmd_Argv(1) );
		strcat( buffer, "=" );
		for ( i = 2; i < argc; i++ ) {
			strcat( buffer, Cmd_Argv( i ) );
			strcat( buffer, " " );
		}
		putenv( buffer );
	} else if ( argc == 2 ) {
		char *env = getenv( Cmd_Argv(1) );

		if ( env ) {
			Com_Printf( "%s=%s\n", Cmd_Argv(1), env );
		} else {
			Com_Printf( "%s undefined\n", Cmd_Argv(1));
		}
	}
}*/

/*
==================
CL_ForwardToServer_f
==================
*/
void CL_ForwardToServer_f (void)
{
	if(cls.demoplaying)
		return;

	//if (cls.state != ca_connected && cls.state != ca_active)
	if (cls.state < ca_connected) {
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}
	
	// don't forward the first argument
	if (Cmd_Argc() > 1) {
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, Cmd_Args());
	}
}


/*
==================
CL_Pause_f
==================
*/
void CL_Pause_f (void)
{
	// never pause in multiplayer
	if (Cvar_VariableIntValue("maxclients") > 1 || !Com_ServerState ()) {
		Cvar_Set("paused", "0");
		return;
	}

	Cvar_SetValue ("paused", !cl_paused->integer);
}

/*
==================
CL_Quit_f
==================
*/
void CL_Quit_f (void)
{
	CL_Disconnect();
	Com_Quit();
}

/*
================
CL_Drop

Called after an ERR_DROP was thrown
================
*/
void CL_Drop (void)
{
	if (cls.state <= ca_disconnected)
		return;

	CL_Disconnect ();

	// drop loading plaque unless this is the initial game start
	if (cls.disable_servercount != -1)
		SCR_EndLoadingPlaque ();	// get rid of loading plaque

	cls.serverProtocol = 0;
}


/*
=======================
CL_SendConnectPacket

We have gotten a challenge from the server, so try and
connect.
======================
*/
static void CL_SendConnectPacket (int useProtocol)
{
	netadr_t	adr = { 0 };
	int		port;

	if (!NET_StringToAdr (cls.servername, &adr)) {
		Com_Printf ("Bad server address: %s\n", cls.servername);
		cls.connect_time = 0;
		return;
	}
	if (adr.port == 0)
		adr.port = BigShort (PORT_SERVER);

	port = Cvar_VariableIntValue("qport");
	userinfo_modified = false;

	if (cl.attractloop)
		cls.serverProtocol = PROTOCOL_VERSION_DEFAULT;
	else if (!cls.serverProtocol)
		cls.serverProtocol = useProtocol;

	if (cls.serverProtocol > PROTOCOL_VERSION_DEFAULT)
		port &= 0xFF;

	cls.quakePort = port;

	if (cls.serverProtocol == PROTOCOL_VERSION_R1Q2)
		Netchan_OutOfBandPrint (NS_CLIENT, &adr, "connect %i %i %i \"%s\" %u %u\n", cls.serverProtocol, port, cls.challenge, Cvar_Userinfo(), net_maxmsglen->integer, PROTOCOL_VERSION_R1Q2_CURRENT );
	else
		Netchan_OutOfBandPrint (NS_CLIENT, &adr, "connect %i %i %i \"%s\"\n", cls.serverProtocol, port, cls.challenge, Cvar_Userinfo());
}

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
static void CL_CheckForResend (void)
{
	netadr_t	adr;

	// if the local server is running and we aren't
	// then connect
	if (cls.state == ca_disconnected && Com_ServerState() )
	{
		cls.state = ca_connecting;
		strcpy(cls.servername, "localhost");
		// we don't need a challenge on the localhost
		CL_SendConnectPacket (PROTOCOL_VERSION_DEFAULT);
		return;
//		cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
	}

	// resend if we haven't gotten a reply yet
	if (cls.state != ca_connecting)
		return;

	if (cls.realtime - cls.connect_time < 3000)
		return;

	if (!NET_StringToAdr (cls.servername, &adr))
	{
		Com_Printf ("Bad server address\n");
		cls.state = ca_disconnected;
		return;
	}
	if (adr.port == 0)
		adr.port = BigShort (PORT_SERVER);

	cls.connect_time = cls.realtime;	// for retransmit requests

	Com_Printf ("Connecting to %s...\n", cls.servername);

	Netchan_OutOfBandPrint (NS_CLIENT, &adr, "getchallenge\n");
}


/*
================
CL_Connect_f

================
*/
static void CL_Connect_f (void)
{
	char	*server, *p;
	netadr_t	adr;

	if (Cmd_Argc() != 2) {
		Com_Printf ("usage: connect <server>\n");
		return;	
	}
	
	if (Com_ServerState ())
	{	// if running a local server, kill it and reissue
		SV_Shutdown ("Server has shut down\n", false);
	}

	server = Cmd_Argv (1);

	// quake2://protocol support
	if (!Q_strnicmp(server, "quake2://", 9))
		server += 9;

	p = strchr (server, '/');
	if (p)
		p[0] = 0;

	NET_Config(NET_CLIENT);		// allow remote

	if (!NET_StringToAdr (server, &adr))
	{
		Com_Printf ("Bad server address: %s\n", server);
		return;
	}

	if (adr.port == 0)
		adr.port = BigShort (PORT_SERVER);

	CL_Disconnect ();

	//reset protocol attempt if we're connecting to a different server
	if (!NET_CompareAdr (&adr, &cls.netchan.remote_address))
	{
		Com_DPrintf ("Resetting protocol attempt since %s is not ", NET_AdrToString (&adr));
		Com_DPrintf ("%s.\n", NET_AdrToString (&cls.netchan.remote_address));
		cls.serverProtocol = 0;
	}

	cls.state = ca_connecting;
	Q_strncpyz (cls.servername, server, sizeof(cls.servername));
	strcpy (cls.lastservername, cls.servername);
	cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
}


/*
=====================
CL_Rcon_f

  Send the rest of the command line over as
  an unconnected command.
=====================
*/
static void CL_Rcon_f (void)
{
	char	message[1024], *s;
	netadr_t	to = {0};

	if( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: %s <command>\n", Cmd_Argv( 0 ) );
		return;
	}

	if (!rcon_client_password->string[0]) {
		Com_Printf ("You must set 'rcon_password' before issuing an rcon command.\n");
		return;
	}

	s = Cmd_ArgsFrom(1);
	if ((strlen(s) + strlen(rcon_client_password->string) + 16) >= sizeof(message)) {
		Com_Printf ("Length of password + command exceeds maximum allowed length.\n");
		return;
	}

	message[0] = message[1] = message[2] = message[3] = -1;
	message[4] = 0;

	NET_Config(NET_CLIENT);		// allow remote

	strcat (message, "rcon ");
	if (rcon_client_password->string[0]) {
		strcat (message, rcon_client_password->string);
		strcat (message, " ");
	}

	strcat (message, s);

	if (cls.state >= ca_connected)
		to = cls.netchan.remote_address;
	else
	{
		if (!rcon_address->string[0])
		{
			Com_Printf ("You must either be connected,\n"
						"or set the 'rcon_address' cvar\n"
						"to issue rcon commands\n");

			return;
		}
		NET_StringToAdr (rcon_address->string, &to);
		if (to.port == 0)
			to.port = BigShort (PORT_SERVER);
	}
	
	CL_AddRequest( &to, REQ_RCON );
	NET_SendPacket (NS_CLIENT, strlen(message)+1, message, &to);
}


/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState (void)
{
	S_StopAllSounds ();
	CL_ClearEffects ();
	CL_ClearTEnts ();

// wipe the entire cl structure
	memset (&cl, 0, sizeof(cl));
	memset (cl_entities, 0, sizeof(cl_entities));

	cl.maxclients = MAX_CLIENTS;
	SZ_Clear (&cls.netchan.message);

	SZ_Init (&cl.demoBuff, cl.demoFrame, sizeof(cl.demoFrame));
	cl.demoBuff.allowoverflow = true;
}

/*
=====================
CL_Disconnect

Goes from a connected state to full screen console state
Sends a disconnect message to the server
This is also called on Com_Error, so it shouldn't cause any errors
=====================
*/
void CL_StopDemoFile( void );

void CL_Disconnect (void)
{
	byte	final[32];

	if (cls.state <= ca_disconnected)
		return;

	if (cl_timedemo->integer)
	{
		unsigned int time;
		
		time = Sys_Milliseconds () - cl.timedemo_start;
		if (time > 0)
			Com_Printf ("%i frames, %3.1f seconds: %3.1f fps\n", cl.timedemo_frames,
			time/1000.0, cl.timedemo_frames*1000.0 / time);
	}

	VectorClear (cl.refdef.blend);
	R_CinematicSetPalette(NULL);

	M_ForceMenuOff ();

	cls.connect_time = 0;

	SCR_StopCinematic ();

	if (cls.demorecording)
		CL_Stop_f ();

	if( cls.demoplaying )
		CL_StopDemoFile();

	// send a disconnect message to the server
	final[0] = clc_stringcmd;
	strcpy ((char *)final+1, "disconnect");
	Netchan_Transmit (&cls.netchan, 11, final);
	Netchan_Transmit (&cls.netchan, 11, final);
	Netchan_Transmit (&cls.netchan, 11, final);

	CL_ClearState ();

	// stop download
	if (cls.download) {
		fclose(cls.download);
		cls.download = NULL;
	}

#ifdef USE_CURL
	CL_CancelHTTPDownloads (true);
	cls.downloadReferer[0] = 0;
#endif

	cls.downloadname[0] = 0;
	cls.downloadposition = 0;

	cls.servername[0] = '\0';
	cls.state = ca_disconnected;

#ifdef AVI_EXPORT
	AVI_StopExport();
#endif
#ifdef GL_QUAKE
	Cvar_Set("cl_avidemo", "0");
#endif
}

void CL_Disconnect_f (void)
{
	if (cls.state > ca_disconnected)
	{
		cls.serverProtocol = 0;
		cls.key_dest = key_console;
		Com_Error (ERR_DROP, "Disconnected from server");
	}
}

/*
================
CL_ServerStatus_f
================
*/
#define PRINT_STATUS 1
#define PRINT_SCORES 2

void CL_SendUIStatusRequests(netadr_t *adr)
{
	CL_AddRequest( adr, REQ_PING );
	Netchan_OutOfBandPrint( NS_CLIENT, adr, "status" );
}

static void CL_ServerStatus_f( void )
{
	char	*server;
	netadr_t	adr;

	if( Cmd_Argc() != 2 ) {
		if (cls.state < ca_connected)
		{
			Com_Printf ("Not connected to a server.\n"
						"Usage: %s [server]\n", Cmd_Argv( 0 ) );
			return;	
		}
		adr = cls.netchan.remote_address;
	}
	else {
		NET_Config(NET_CLIENT);		// allow remote

		server = Cmd_Argv( 1 );
		if( !NET_StringToAdr( server, &adr ) )
		{
			Com_Printf( "Bad address: %s\n", server );
			return;
		}
	}

	if( !adr.port )
		adr.port = BigShort( PORT_SERVER );

	CL_AddRequest( &adr, REQ_STATUS );
	Netchan_OutOfBandPrint( NS_CLIENT, &adr, "status" );
}

/*
====================
CL_ServerStatusResponse
====================
*/
static int SortPlayers( const playerStatus_t *p1, const playerStatus_t *p2 )
{

	if( p1->score > p2->score )
		return -1;

	if( p1->score < p2->score )
		return 1;

	return strcmp(p1->name, p2->name);
}


static qboolean CL_ServerStatusResponse( const char *status, const netadr_t *from, serverStatus_t *dest )
{
	char *s;
	playerStatus_t *player;
	int length;

	memset( dest, 0, sizeof( *dest ) );

	s = strchr( status, '\n' );
	if( !s )
		return false;

	length = s - status;
	if( length > sizeof(dest->infostring) - 1 )
		return false;

    strcpy( dest->address, NET_AdrToString( from ) );
    strncpy( dest->infostring, status, length );
    
    // HACK: check if this is a status response
	if( !strstr( dest->infostring, "\\hostname\\" ) )
		return false;

	s++;
    if( *s < 32 )
        return true;
 
	do {
		player = &dest->players[dest->numPlayers];
		player->score = atoi( COM_Parse( &s ) );
		player->ping = atoi( COM_Parse( &s ) );
		if( !s )
			break;

		Q_strncpyz(player->name, COM_Parse( &s ), sizeof( player->name ));

		if( ++dest->numPlayers == MAX_PLAYERSTATUS )
			break;

	} while( s );

	qsort( dest->players, dest->numPlayers, sizeof( dest->players[0] ), (int (*)(const void *, const void *))SortPlayers );

	return true;

}

void CL_DumpServerInfo( const serverStatus_t *status )
{
	const playerStatus_t *player;
	int	i;

	// print serverinfo
	Info_Print( status->infostring );

	// print player list
	Com_Printf( "\nName            Score Ping\n"
				  "--------------- ----- ----\n" );
	if (status->numPlayers) {
		for( i = 0, player = status->players; i < status->numPlayers; i++, player++ ) {
			Com_Printf( "%-15s %5i %4i\n", player->name, player->score, player->ping );
		}
	} else {
		Com_Printf("No players\n");
	}
}

/*
====================
CL_ParsePrintMessage
====================
*/
static void CL_ParsePrintMessage( sizebuf_t *msg, netadr_t	*remoteAdr )
{
	request_t *r;
	requestType_t type = REQ_FREE;
    serverStatus_t serverStatus;
    char *string;
    int i;

    string = MSG_ReadString(msg);

	i = currentRequest - MAX_REQUESTS;
	if( i < 0 )
		i = 0;

	for( ; i < currentRequest; i++ )
	{
		r = &clientRequests[i & REQUEST_MASK];
		if( !r->type )
			continue;

		if(r->time < cls.realtime) {
			r->type = REQ_FREE;
			continue;
		}

		if( !NET_CompareBaseAdr( &net_from, &r->adr ) )
			continue;

		type = r->type;
		r->type = REQ_FREE;
		break;
	}

	if (!type) {
		if (cls.state > ca_disconnected && cls.state < ca_active && NET_CompareBaseAdr( &net_from, remoteAdr )) {
			Com_Printf( "%s", string );
			return;
		}

		if (broadcastRequest.time >= cls.realtime)
			type = broadcastRequest.type;

		if (!type) {
			if (NET_IsLocalAddress( &net_from ))
				Com_Printf( "%s", string );
			else
				Com_DPrintf( "Dropped unrequested packet from %s\n", NET_AdrToString(&net_from) );
			return;
		}
	}

	switch( type ) {
	case REQ_STATUS:
		if( CL_ServerStatusResponse( string, &net_from, &serverStatus ) ) {
		    CL_DumpServerInfo( &serverStatus );
        }
		break;
	case REQ_INFO:
		break;
	case REQ_PING:
		if( CL_ServerStatusResponse( string, &net_from, &serverStatus ) ) {
			M_AddToServerList( &serverStatus );
		}
		break;
	case REQ_RCON:
		Com_Printf( "%s", string );
		CL_AddRequest( &net_from, REQ_RCON );
		break;
	default:
		if (cls.state < ca_active)
			Com_Printf ("%s: %s", NET_AdrToString(&net_from), string);
		break;
	}
}

/*
=================
CL_Changing_f

Just sent as a hint to the client that they should
drop to full console
=================
*/
void CL_Changing_f (void)
{
	//ZOID
	//if we are downloading, we don't change!  This so we don't suddenly stop downloading a map
	if (cls.download)
		return;

	if(!cl.attractloop) {
		Cmd_ExecTrigger( "#cl_changelevel" );
		CL_StopAutoRecord();
	}

	SCR_BeginLoadingPlaque ();
	cls.state = ca_connected;	// not active anymore, but not disconnected
	Com_Printf ("\nChanging map...\n");
}


/*
=================
CL_Reconnect_f

The server is changing levels
=================
*/
void CL_Reconnect_f (void)
{
	//ZOID
	//if we are downloading, we don't change!  This so we don't suddenly stop downloading a map
	if (cls.download)
		return;

	S_StopAllSounds ();
	if (cls.state == ca_connected)
	{
		Com_Printf ("reconnecting...\n");
		//cls.state = ca_connected;
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		return;
	}

	if (cls.servername[0])
	{
		if (cls.state >= ca_connected)
		{
			strcpy (cls.lastservername, cls.servername);
			CL_Disconnect();
			cls.connect_time = cls.realtime - 1500;
		}
		else
			cls.connect_time = -99999; // fire immediately

		cls.state = ca_connecting;
		//Com_Printf ("reconnecting...\n");
	}

	if (cls.lastservername[0])
	{
		cls.connect_time = -99999;
		cls.state = ca_connecting;
		Com_Printf ("reconnecting...\n");
		strcpy (cls.servername, cls.lastservername);
	}
	else
	{
		Com_Printf ("No server to reconnect to.\n");
	}
}

/*
=================
CL_ParseStatusMessage

Handle a reply from a ping
=================
*/
static void CL_ParseStatusMessage (sizebuf_t *msg)
{
	char	*s;

	s = MSG_ReadString(msg);

	if(strrchr(s, '\n'))
		Com_Printf ("%s", s);
	else
		Com_Printf ("%s\n", s);
	//M_AddToServerList (net_from, s);
}


/*
=================
CL_PingServers_f
=================
*/
void CL_PingServers_f (void)
{
	int			i;
	netadr_t	adr;
	char		name[32], *adrstring;

	NET_Config (NET_CLIENT);		// allow remote

	// send a broadcast packet
	Com_Printf ("pinging broadcast...\n");

	adr.type = NA_BROADCAST;
	adr.port = BigShort(PORT_SERVER);
	Netchan_OutOfBandPrint (NS_CLIENT, &adr, va("info %i", PROTOCOL_VERSION_DEFAULT));

	// send a packet to each address book entry
	for (i=0 ; i<MAX_LOCAL_SERVERS ; i++)
	{
		Com_sprintf (name, sizeof(name), "adr%i", i);
		adrstring = Cvar_VariableString (name);
		if (!adrstring || !adrstring[0])
			continue;

		Com_Printf ("pinging %s...\n", adrstring);
		if (!NET_StringToAdr (adrstring, &adr))
		{
			Com_Printf ("Bad address: %s\n", adrstring);
			continue;
		}
		if (!adr.port)
			adr.port = BigShort(PORT_SERVER);
		Netchan_OutOfBandPrint (NS_CLIENT, &adr, va("info %i", PROTOCOL_VERSION_DEFAULT));
	}
}


/*
=================
CL_Skins_f

Load or download any custom player skins and models
=================
*/
void CL_Skins_f (void)
{
	int		i;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (!cl.configstrings[CS_PLAYERSKINS+i][0])
			continue;
		Com_Printf ("client %i: %s\n", i, cl.configstrings[CS_PLAYERSKINS+i]); 
		SCR_UpdateScreen ();
		Sys_SendKeyEvents ();	// pump message loop
		CL_ParseClientinfo (i);
	}
}


/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc
=================
*/
static void CL_ConnectionlessPacket (sizebuf_t *msg)
{
	char		*s, *c;
	int			i;
	netadr_t	remoteAdr;
	
	MSG_BeginReading (msg);
	MSG_ReadLong (msg);	// skip the -1

	s = MSG_ReadStringLine (msg);

	Cmd_TokenizeString (s, false);

	c = Cmd_Argv(0);

	//Com_Printf ("%s: %s\n", NET_AdrToString (&net_from), c);

	// server responding to a status broadcast
	if (!strcmp(c, "info")) {
		CL_ParseStatusMessage(msg);
		return;
	}
	
	NET_StringToAdr (cls.servername, &remoteAdr);

	// print command from somewhere
	if (!strcmp(c, "print")) {
		CL_ParsePrintMessage(msg, &remoteAdr);
		return;
	}

	// server connection
	if (!strcmp(c, "client_connect"))
	{
		int		try_to_use_anticheat = 0;
		char	*p;

		if ( cls.state < ca_connecting ) {
			Com_DPrintf( "Connect received while not connecting.  Ignored.\n" );
			return;
		}
		if ( !NET_CompareBaseAdr( &net_from, &remoteAdr ) ) {
            Com_DPrintf( "Connect from different address.  Ignored.\n" );
            return;
        }
		if ( cls.state > ca_connecting ) {
			Com_DPrintf( "Dup connect received.  Ignored.\n" );
			return;
		}

		Netchan_Setup (NS_CLIENT, &cls.netchan, &net_from, cls.serverProtocol, cls.quakePort);

		for (i = 1; i < Cmd_Argc(); i++)
		{
			p = Cmd_Argv(i);
			if (!strncmp(p, "ac=", 3))  {
				p += 3;
				if (*p)
					try_to_use_anticheat = atoi(p);
			}
#ifdef USE_CURL
			else if (!strncmp(p, "dlserver=", 9)) {
				p += 9;
				if (strlen(p) > 2) {
					Com_sprintf (cls.downloadReferer, sizeof(cls.downloadReferer), "quake2://%s", NET_AdrToString(&cls.netchan.remote_address));
					CL_SetHTTPServer(p);
					if (cls.downloadServer[0])
						Com_Printf ("HTTP downloading enabled, URL: %s\n", cls.downloadServer);
				}
			}
#endif
		}

		if (try_to_use_anticheat) {
#ifdef ANTICHEAT
			MSG_WriteByte (&cls.netchan.message, clc_nop);
			Netchan_Transmit (&cls.netchan, 0, NULL);
			S_StopAllSounds ();
			Com_Printf("\1\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");
			Com_Printf ("\1Loading anticheat, this may take a few moments...\n");
			Com_Printf("\1\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");
			SCR_UpdateScreen ();
			SCR_UpdateScreen ();
			SCR_UpdateScreen ();
			if (!Sys_GetAntiCheatAPI ())
				Com_Printf ("anticheat failed to load, trying to connect without it.\n");
#else
			if (try_to_use_anticheat > 1)
					Com_Printf("Anticheat required by server, but no anticheat support linked in.\n" );
#endif
		}

		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		cls.state = ca_connected;
		cl.sendPacketNow = true;
		return;
	}

	// remote command from gui front end
	if (!strcmp(c, "cmd"))
	{
		if (!NET_IsLocalAddress(&net_from)) {
			Com_DPrintf ("Command packet from remote host.  Ignored.\n");
			return;
		}
		Sys_AppActivate ();
		s = MSG_ReadString(msg);
		Cbuf_AddText(s);
		Cbuf_AddText("\n");
		return;
	}

	// challenge from the server we are connecting to
	if (!strcmp(c, "challenge"))
	{
		int		j, protocol = PROTOCOL_VERSION_DEFAULT;
		char	*p;

        if ( cls.state < ca_connecting ) {
            Com_DPrintf( "Challenge received while not connecting.  Ignored.\n" );
            return;
        }
        if ( !NET_CompareBaseAdr( &net_from, &remoteAdr ) ) {
            Com_DPrintf( "Challenge from different address.  Ignored.\n" );
            return;
        }
        if ( cls.state > ca_connecting ) {
            Com_DPrintf( "Dup challenge received.  Ignored.\n" );
            return;
        }

		cls.challenge = atoi(Cmd_Argv(1));

		// available protocol versions now in challenge, until few months we still default to brute.
		for (i = 2; i < Cmd_Argc(); i++)
		{
			p = Cmd_Argv(i);
			if (!strncmp(p, "p=", 2))
			{
				for (p += 2; *p; p++)
				{
					j = atoi(p);
					if (j == cl_protocol->integer)
						protocol = j;
					p = strchr(p, ',');
					if (!p)
						break;
				}
			}
		}
		//r1: reset the timer so we don't send dup. getchallenges
		cls.connect_time = cls.realtime;

		CL_SendConnectPacket( protocol );
		return;
	}

	// ping from somewhere
	/*if (!strcmp(c, "ping")) {
		Netchan_OutOfBandPrint (NS_CLIENT, &net_from, "ack");
		return;
	}*/

	// echo request from server
	/*if (!strcmp(c, "echo")) {
		Netchan_OutOfBandPrint (NS_CLIENT, &net_from, "%s", Cmd_Argv(1) );
		return;
	}*/

	Com_DPrintf ("Unknown connectionless packet command: %s\n", c);
}


/*
=================
CL_DumpPackets

A vain attempt to help bad TCP stacks that cause problems
when they overflow
=================
*/
/*void CL_DumpPackets (void)
{
	while (NET_GetPacket (NS_CLIENT, &net_from, &net_message))
	{
		Com_Printf ("dumnping a packet\n");
	}
}*/

/*
=================
CL_ReadPackets
=================
*/
void CL_ReadPackets (void)
{
	int ret;

	while ((ret = NET_GetPacket (NS_CLIENT, &net_from, &net_message)) != 0)
	{
		// remote command packet
		if (ret == 1 && *(int *)net_message.data == -1) {
			CL_ConnectionlessPacket (&net_message);
			continue;
		}

		if (cls.state < ca_connected)
			continue;		// dump it if not connected

		if (ret == 1 && net_message.cursize < 8) {
			Com_DPrintf ("%s: Runt packet\n", NET_AdrToString(&net_from));
			continue;
		}

		// packet from server
		if (!NET_CompareAdr (&net_from, &cls.netchan.remote_address)) {
			Com_DPrintf ("%s:sequenced packet without connection\n",NET_AdrToString(&net_from));
			continue;
		}

		if( ret == -1 ) {
			Com_Error( ERR_DISCONNECT, "Connection reset by peer" );
		}

		if (!Netchan_Process(&cls.netchan, &net_message))
			continue;		// wasn't accepted for some reason

		CL_ParseServerMessage(&net_message);

		CL_AddNetgraph();

		//
		// we don't know if it is ok to save a demo message until
		// after we have parsed the frame
		//
		if (cls.demorecording && !cls.demowaiting && cls.serverProtocol == PROTOCOL_VERSION_DEFAULT)
			CL_WriteDemoMessageFull (&net_message);

		SCR_AddLagometerPacketInfo();
	}

	// check timeout
	if (cls.state >= ca_connected && cls.realtime - cls.netchan.last_received > (unsigned int)(cl_timeout->integer*1000) && !cls.demoplaying)
	{
		if (++cl.timeoutcount > 5)	// timeoutcount saves debugger
		{
			Com_Printf("\nServer connection timed out.\n");
			CL_Disconnect();
			return;
		}
	}
	else
		cl.timeoutcount = 0;
	
}


//=============================================================================

/*
==============
CL_FixUpGender_f
==============
*/
void CL_FixUpGender(void)
{
	char *p;
	char sk[80];

	if (gender_auto->integer) {

		if (info_gender->modified) {
			// was set directly, don't override the user
			info_gender->modified = false;
			return;
		}

		Q_strncpyz(sk, info_skin->string, sizeof(sk));
		if ((p = strchr(sk, '/')) != NULL)
			*p = 0;
		if (Q_stricmp(sk, "male") == 0 || Q_stricmp(sk, "cyborg") == 0)
			Cvar_Set ("gender", "male");
		else if (Q_stricmp(sk, "female") == 0 || Q_stricmp(sk, "crackhor") == 0)
			Cvar_Set ("gender", "female");
		else
			Cvar_Set ("gender", "none");
		info_gender->modified = false;
	}
}

/*
==============
CL_Userinfo_f
==============
*/
void CL_Userinfo_f (void)
{
	Com_Printf ("User info settings:\n");
	Info_Print (Cvar_Userinfo());
}

/*
=================
CL_Snd_Restart_f

Restart the sound subsystem so it can pick up
new parameters and flush all sounds
=================
*/
void CL_Snd_Restart_f (void)
{
	S_Shutdown ();
	S_Init ();
	memset (cl.sound_precache, 0, sizeof(cl.sound_precache));
	CL_RegisterSounds ();
}

static int precache_check; // for autodownload of precache items
static int precache_spawncount;
static int precache_tex;
static int precache_model_skin;

static byte *precache_model; // used for skin checking in alias models

//#define PLAYER_MULT 5

// ENV_CNT is map load, ENV_CNT+1 is first env map
#define ENV_CNT (CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
#define TEXTURE_CNT (ENV_CNT+13)

static const char *env_suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};

qboolean isvalidchar (int c)
{
	if (!isalnum(c) && c != '_' && c != '-')
		return false;
	return true;
}

void CL_ResetPrecacheCheck (void)
{
	precache_check = CS_MODELS;
	precache_model = 0;
	precache_model_skin = -1;
}

void CL_RequestNextDownload (void)
{
	int			i, PLAYER_MULT;
	char		*sexedSounds[MAX_SOUNDS];
	uint32		map_checksum;		// for detecting cheater maps
	char		fn[MAX_OSPATH];
	dmdl_t		*pheader;
	dsprite_t	*spriteheader;

	if (cls.state != ca_connected)
		return;


	PLAYER_MULT = 0;

	for (i = CS_SOUNDS; i < CS_SOUNDS + MAX_SOUNDS; i++)
	{
		if (cl.configstrings[i][0] == '*')
			sexedSounds[PLAYER_MULT++] = cl.configstrings[i] + 1;
	}

	PLAYER_MULT += 5;

	if (!allow_download->integer && precache_check < ENV_CNT)
		precache_check = ENV_CNT;

//ZOID
	if (precache_check == CS_MODELS)
	{ // confirm map
		precache_check = CS_MODELS+2; // 0 isn't used
		if (allow_download_maps->integer)
		{
			if (strlen(cl.configstrings[CS_MODELS+1]) >= MAX_QPATH-1)
				Com_Error (ERR_DROP, "Bad map configstring '%s'", cl.configstrings[CS_MODELS+1]);

			if (!CL_CheckOrDownloadFile(cl.configstrings[CS_MODELS+1]))
				return; // started a download
		}
	}


redoSkins:;

	if (precache_check >= CS_MODELS && precache_check < CS_MODELS+MAX_MODELS)
	{
		if (allow_download_models->integer)
		{
			char *skinname;

			while (precache_check < CS_MODELS+MAX_MODELS &&
				cl.configstrings[precache_check][0])
			{
				//its a brush/alias model, we don't do those
				if (cl.configstrings[precache_check][0] == '*' || cl.configstrings[precache_check][0] == '#')
				{
					precache_check++;
					continue;
				}

				//new model, try downloading it
				if (precache_model_skin == -1)
				{
					if (!CL_CheckOrDownloadFile(cl.configstrings[precache_check]))
					{
						precache_check++;
						return; // started a download
					}
					precache_check++;
				}
				else
				{
					//model is ok, now we are checking for skins in the model
					if (!precache_model)
					{
						//load model into buffer
						precache_model_skin = 1;
						FS_LoadFile (cl.configstrings[precache_check], (void **)&precache_model);
						if (!precache_model)
						{
							//shouldn't happen?
							precache_model_skin = 0;
							precache_check++;
							continue; // couldn't load it
						}
						
						//is it an alias model
						if (LittleLong(*(uint32 *)precache_model) != IDALIASHEADER)
						{
							//is it a sprite
							if (LittleLong(*(uint32 *)precache_model) != IDSPRITEHEADER)
							{
								//no, free and move onto next model
								FS_FreeFile(precache_model);
								precache_model = NULL;
								precache_model_skin = 0;
								precache_check++;
								continue;
							}
							else
							{
								//get sprite header
								spriteheader = (dsprite_t *)precache_model;
								if (LittleLong (spriteheader->version) != SPRITE_VERSION)
								{
									//this is unknown version! free and move onto next.
									FS_FreeFile(precache_model);
									precache_model = NULL;
									precache_check++;
									precache_model_skin = 0;
									continue; // couldn't load it
								}
							}
						}
						else
						{
							//get model header
							pheader = (dmdl_t *)precache_model;
							if (LittleLong (pheader->version) != ALIAS_VERSION)
							{
								//unknown version! free and move onto next
								FS_FreeFile(precache_model);
								precache_model = NULL;
								precache_check++;
								precache_model_skin = 0;
								continue; // couldn't load it
							}
						}
					}

					//if its an alias model
					if (LittleLong(*(uint32 *)precache_model) == IDALIASHEADER)
					{
						pheader = (dmdl_t *)precache_model;

						//iterate through number of skins
						while (precache_model_skin - 1 < LittleLong(pheader->num_skins))
						{
							skinname = (char *)precache_model +
								LittleLong(pheader->ofs_skins) + 
								(precache_model_skin - 1)*MAX_SKINNAME;

							//r1: spam warning for models that are broken
							if (strchr (skinname, '\\'))
							{
								Com_Printf ("Warning, model %s with incorrectly linked skin: %s\n", cl.configstrings[precache_check], skinname);
							}
							else if (strlen(skinname) > MAX_SKINNAME-1)
							{
								Com_Error (ERR_DROP, "Model %s has too long a skin path: %s", cl.configstrings[precache_check], skinname);
							}

							//check if this skin exists
							if (!CL_CheckOrDownloadFile(skinname))
							{
								precache_model_skin++;
								return; // started a download
							}
							precache_model_skin++;
						}
					}
					else
					{
						//its a sprite
						spriteheader = (dsprite_t *)precache_model;

						//iterate through skins
						while (precache_model_skin - 1 < LittleLong(spriteheader->numframes))
						{
							skinname = spriteheader->frames[(precache_model_skin - 1)].name;

							//r1: spam warning for models that are broken
							if (strchr (skinname, '\\'))
							{
								Com_Printf ("Warning, sprite %s with incorrectly linked skin: %s\n", cl.configstrings[precache_check], skinname);
							}
							else if (strlen(skinname) > MAX_SKINNAME-1)
							{
								Com_Error (ERR_DROP, "Sprite %s has too long a skin path: %s", cl.configstrings[precache_check], skinname);
							}

							//check if this skin exists
							if (!CL_CheckOrDownloadFile(skinname))
							{
								precache_model_skin++;
								return; // started a download
							}
							precache_model_skin++;
						}
					}

					//we're done checking the model and all skins, free
					if (precache_model)
					{ 
						FS_FreeFile(precache_model);
						precache_model = NULL;
					}

					precache_model_skin = 0;
					precache_check++;
				}
			}
		}
		if (precache_model_skin == -1)
		{
			precache_check = CS_MODELS + 2;
			precache_model_skin = 0;

	//pending downloads (models), let's wait here before we can check skins.
#ifdef USE_CURL
		if (CL_PendingHTTPDownloads ())
			return;
#endif

			goto redoSkins;
		}
		precache_check = CS_SOUNDS;
	}

	if (precache_check >= CS_SOUNDS && precache_check < CS_SOUNDS+MAX_SOUNDS) { 
		if (allow_download_sounds->integer) {
			if (precache_check == CS_SOUNDS)
				precache_check++; // zero is blank
			while (precache_check < CS_SOUNDS+MAX_SOUNDS &&
				cl.configstrings[precache_check][0]) {
				if (cl.configstrings[precache_check][0] == '*') {
					precache_check++;
					continue;
				}
				Com_sprintf(fn, sizeof(fn), "sound/%s", cl.configstrings[precache_check++]);
				if (!CL_CheckOrDownloadFile(fn))
					return; // started a download
			}
		}
		precache_check = CS_IMAGES;
	}

	if (precache_check >= CS_IMAGES && precache_check < CS_IMAGES+MAX_IMAGES)
	{
		if (precache_check == CS_IMAGES)
			precache_check++; // zero is blank

		while (precache_check < CS_IMAGES+MAX_IMAGES && cl.configstrings[precache_check][0])
		{
			Com_sprintf(fn, sizeof(fn), "pics/%s.pcx", cl.configstrings[precache_check]);
			if (FS_LoadFile (fn, NULL) == -1)
			{
				//Com_sprintf(fn, sizeof(fn), "pics/%s.pcx", cl.configstrings[precache_check]);
				if (!CL_CheckOrDownloadFile(fn))
				{
					precache_check++;
					return; // started a download
				}
			}
			precache_check++;
		}
		precache_check = CS_PLAYERSKINS;
	}

	// skins are special, since a player has three things to download:
	// model, weapon model and skin
	// so precache_check is now *3
	if (precache_check >= CS_PLAYERSKINS && precache_check < CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
	{
		if (allow_download_players->integer)
		{
			while (precache_check < CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
			{
				int i, n, j, length;
				char model[MAX_QPATH], skin[MAX_QPATH], *p;

				i = (precache_check - CS_PLAYERSKINS)/PLAYER_MULT;
				n = (precache_check - CS_PLAYERSKINS)%PLAYER_MULT;

				if (i >= cl.maxclients)
				{
					precache_check = ENV_CNT;
					continue;
				}

				if (!cl.configstrings[CS_PLAYERSKINS+i][0])
				{
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
					continue;
				}

				if (n && cls.failed_download)
				{
					cls.failed_download = false;
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
					continue;
				}

				if ((p = strchr(cl.configstrings[CS_PLAYERSKINS+i], '\\')) != NULL)
				{
					p++;
				}
				else
				{
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
					continue;
				}

				Q_strncpyz(model, p, sizeof(model));

				p = strchr(model, '/');

				if (!p)
					p = strchr(model, '\\');

				if (p)
				{
					*p++ = 0;
					if (!*p || !model[0])
					{
						precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
						continue;
					}
					else
					{
						Q_strncpyz (skin, p, sizeof(skin));
					}
				}
				else
				{
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
					continue;
				}
					//*skin = 0;

				length = (int)strlen (model);
				for (j = 0; j < length; j++)
				{
					if (!isvalidchar(model[j]))
					{
						Com_Printf ("Bad character '%c' in playermodel '%s'\n", model[j], model);
						precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
						goto skipplayer;
					}
				}

				length = (int)strlen (skin);
				for (j = 0; j < length; j++)
				{
					if (!isvalidchar(skin[j]))
					{
						Com_Printf ("Bad character '%c' in playerskin '%s'\n", skin[j], skin);
						precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
						goto skipplayer;
					}
				}

				switch (n) 
				{
					case -1:
						continue;
					case 0: // model
						cls.failed_download = false;
						Com_sprintf(fn, sizeof(fn), "players/%s/tris.md2", model);
						if (!CL_CheckOrDownloadFile(fn)) {
							precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 1;
							return; // started a download
						}
						//n++;
						/*FALL THROUGH*/

					case 1: // weapon model
						Com_sprintf(fn, sizeof(fn), "players/%s/weapon.md2", model);
						if (!CL_CheckOrDownloadFile(fn)) {
							precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 2;
							return; // started a download
						}
						//n++;
						/*FALL THROUGH*/

					case 2: // weapon skin
						Com_sprintf(fn, sizeof(fn), "players/%s/weapon.pcx", model);
						if (!CL_CheckOrDownloadFile(fn)) {
							precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 3;
							return; // started a download
						}
						//n++;
						/*FALL THROUGH*/

					case 3: // skin
						Com_sprintf(fn, sizeof(fn), "players/%s/%s.pcx", model, skin);
						if (!CL_CheckOrDownloadFile(fn)) {
							precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 4;
							return; // started a download
						}
						//n++;
						/*FALL THROUGH*/

					case 4: // skin_i
						Com_sprintf(fn, sizeof(fn), "players/%s/%s_i.pcx", model, skin);
						if (!CL_CheckOrDownloadFile(fn)) {
							precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 5;
							return; // started a download
						}
						n = 5;

					default:
						while (n < PLAYER_MULT)
						{
							Com_sprintf(fn, sizeof(fn), "players/%s/%s", model, sexedSounds[n-5]);
							n++;
							if (!CL_CheckOrDownloadFile(fn)) {
								precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + n;
								return; // started a download
							}
						}
				}
				
				// move on to next model
				precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;

skipplayer:;
			}
		}
		// precache phase completed
		precache_check = ENV_CNT;
	}

#ifdef USE_CURL
	//map might still be downloading?
	if (CL_PendingHTTPDownloads ())
		return;
#endif

	if (precache_check == ENV_CNT)
	{
		precache_check = ENV_CNT + 1;
		
		CL_LoadLoc();

		CM_LoadMap (cl.configstrings[CS_MODELS+1], true, &map_checksum);

		if (map_checksum && map_checksum != (uint32)strtoul(cl.configstrings[CS_MAPCHECKSUM], NULL, 10))
		{
			Com_Error (ERR_DROP, "Local map version differs from server: 0x%.8x != 0x%.8x",
				map_checksum, atoi(cl.configstrings[CS_MAPCHECKSUM]));
			return;
		}
	}

	if (precache_check > ENV_CNT && precache_check < TEXTURE_CNT)
	{
		if (allow_download->integer && allow_download_maps->integer)
		{
			while (precache_check < TEXTURE_CNT)
			{
				int n = precache_check++ - ENV_CNT - 1;

				if (n & 1)
					Com_sprintf(fn, sizeof(fn), "env/%s%s.pcx", 
						cl.configstrings[CS_SKY], env_suf[n/2]);
				else
					Com_sprintf(fn, sizeof(fn), "env/%s%s.tga", 
						cl.configstrings[CS_SKY], env_suf[n/2]);
				if (!CL_CheckOrDownloadFile(fn))
					return; // started a download
			}
		}
		precache_check = TEXTURE_CNT;
	}

	if (precache_check == TEXTURE_CNT) {
		precache_check = TEXTURE_CNT+1;
		precache_tex = 0;
	}

	// confirm existance of textures, download any that don't exist
	if (precache_check == TEXTURE_CNT+1) {
		// from qcommon/cmodel.c
		extern int			numtexinfo;
		extern mapsurface_t	map_surfaces[];

		if (allow_download->integer && allow_download_maps->integer) {
			while (precache_tex < numtexinfo) {
				//char fn[MAX_OSPATH];

				sprintf(fn, "textures/%s.wal", map_surfaces[precache_tex++].rname);
				if (!CL_CheckOrDownloadFile(fn))
					return; // started a download
			}
		}
		precache_check = TEXTURE_CNT+999;
	}

	//pending downloads (possibly textures), let's wait here.
#ifdef USE_CURL
	if (CL_PendingHTTPDownloads ())
		return;
#endif

//ZOID
	CL_RegisterSounds ();
	CL_PrepRefresh ();

	Cvar_SetCheatState();

	if (cls.serverProtocol > PROTOCOL_VERSION_DEFAULT) {
		MSG_WriteByte (&cls.netchan.message, clc_setting);
		MSG_WriteShort(&cls.netchan.message, CLSET_NOGUN);
		MSG_WriteShort(&cls.netchan.message, cl_gun->integer ? 0 : 1);
	}

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message, va("begin %i\n", precache_spawncount) );

}

/*
=================
CL_Precache_f

The server will send this command right
before allowing the client into the server
=================
*/
void CL_Precache_f (void)
{
	//Yet another hack to let old demos work
	//the old precache sequence
	if (Cmd_Argc() < 2 || cls.demoplaying) {
		uint32 map_checksum = 0;	// for detecting cheater maps

		CM_LoadMap (cl.configstrings[CS_MODELS+1], true, &map_checksum);
		CL_RegisterSounds ();
		CL_PrepRefresh ();
		cls.state = ca_connected;
		return;
	}

	precache_spawncount = atoi(Cmd_Argv(1));
	CL_ResetPrecacheCheck ();
	CL_RequestNextDownload();
}


static void OnChange_MaxFps(cvar_t *self, const char *oldValue)
{
	if (self->integer < 5)
		Cvar_Set(self->name, "5");
}

static void OnChange_Protocol(cvar_t *self, const char *oldValue)
{
	//force reparsing of cl_protocol
	if (cls.state == ca_disconnected)
		cls.serverProtocol = 0;

	if (!self->integer)
		return;

	if( self->integer != PROTOCOL_VERSION_DEFAULT &&
		self->integer != PROTOCOL_VERSION_R1Q2 )
	{
		Com_Printf("Protocol %i is not supported. Current supported protocols: 34(vq2) and %i(r1q2)\n", self->integer, PROTOCOL_VERSION_R1Q2);
		Cvar_SetValue(self->name, PROTOCOL_VERSION_R1Q2);
	}
}

static void OnChange_Gun(cvar_t *self, const char *oldValue)
{
	if (cls.state >= ca_connected && cls.serverProtocol > PROTOCOL_VERSION_DEFAULT) {
		MSG_WriteByte (&cls.netchan.message, clc_setting);
		MSG_WriteShort(&cls.netchan.message, CLSET_NOGUN);
		MSG_WriteShort(&cls.netchan.message, self->integer ? 0 : 1);
	}
}

#ifdef USE_CURL
void OnChange_http_max_connections (cvar_t *self, const char *oldValue)
{
	if (self->integer > 4)
		Cvar_Set (self->name, "4");
	else if (self->integer < 1)
		Cvar_Set (self->name, "1");

	if (self->integer > 2)
		Com_Printf ("WARNING: Changing the maximum connections higher than 2 violates the HTTP specification recommendations. Doing so may result in you being blocked from the remote system and offers no performance benefits unless you are on a very high latency link (ie, satellite)\n");
}
#endif

static void OnChange_Name(cvar_t *self, const char *oldValue)
{
	if (strlen(self->string) >= 16)
		self->string[15] = 0;
	else if (!*self->string)
		Cvar_Set (self->name, "unnamed");
}

static void OnChange_StereoSeparation (cvar_t *self, const char *oldValue)
{
	// range check cl_camera_separation so we don't inadvertently fry someone's brain
	if ( self->value > 1.0f )
		Cvar_Set( self->name, "1" );
	else if ( self->value < 0.0f )
		Cvar_Set( self->name, "0" );
}

#ifdef GL_QUAKE
void R_BeginAviDemo( int format );
void R_WriteAviFrame( void );
void R_StopAviDemo( void );

cvar_t *cl_avidemo = &nullCvar;
static cvar_t *cl_forceavidemo;
static cvar_t *cl_avidemoformat;

static void OnChange_AviDemo(cvar_t *self, const char *oldValue)
{
#ifdef AVI_EXPORT
	if(CL_AviRecording()) {
		if(self->integer)
			Cvar_Set(self->name, "0");
		return;
	}
#endif

	if(self->integer > 0)
	{
		if(!atoi(oldValue))
			R_BeginAviDemo(cl_avidemoformat->integer);
	}
	else
	{
		if(atoi(oldValue))
			R_StopAviDemo();
		if(self->integer < 0)
			Cvar_Set(self->name, "0");
	}
}
#endif

char *CL_Mapname (void)
{
	return cls.mapname;
}

static void CL_Mapname_m ( char *buffer, int bufferSize )
{
	if(cls.mapname[0])
		Q_strncpyz(buffer, cls.mapname, bufferSize);
	else
		Q_strncpyz(buffer, "nomap", bufferSize );
}

static void CL_Server_m(char *buffer, int bufferSize)
{
	char *s;

	if(cls.state < ca_connecting) {
        Q_strncpyz( buffer, "noserver", bufferSize );
        return;
    }

	s = NET_AdrToString(&cls.netchan.remote_address);
	if(*s)
		Q_strncpyz(buffer, s, bufferSize);
}

static void CL_Health_m(char *buffer, int bufferSize)
{
	if( cls.state != ca_active )
		return;

	Com_sprintf(buffer, bufferSize, "%i", (int)cl.frame.playerstate.stats[STAT_HEALTH]);
}

static void CL_Ammo_m(char *buffer, int bufferSize)
{
	if( cls.state != ca_active )
		return;

	Com_sprintf(buffer, bufferSize, "%i", (int)cl.frame.playerstate.stats[STAT_AMMO]);
}

static void CL_WeaponModel_m(char *buffer, int bufferSize)
{
	int num;

	if( cls.state != ca_active )
		return;

	num = cl.frame.playerstate.gunindex + CS_MODELS;
	Q_strncpyz(buffer, cl.configstrings[num], bufferSize);
}

static void CL_Ups_m( char *buffer, int bufferSize ) {
	vec3_t vel;
	int ups;
	player_state_t *ps;

	if( cls.state != ca_active )
		return;

	if( !cls.demoplaying && cl_predict->integer ) {
		VectorCopy( cl.predicted_velocity, vel );
	} else {
		ps = &cl.frame.playerstate;
		
		vel[0] = ps->pmove.velocity[0] * 0.125f;
		vel[1] = ps->pmove.velocity[1] * 0.125f;
		vel[2] = ps->pmove.velocity[2] * 0.125f;
	}

	ups = VectorLength( vel );
	Com_sprintf( buffer, bufferSize, "%d", ups );
}

/*
============
CL_LocalConnect
============
*/
void CL_LocalConnect( void ) {
    if ( FS_NeedRestart() ) {
		if (cls.state > ca_connecting)
			cls.state = ca_connecting;

        CL_RestartFilesystem(true);
    }
}

/*
====================
CL_RestartFilesystem
 
Flush caches and restart the VFS.
====================
*/
void VID_Restart_f (void);
extern int silentSubsystem;
qboolean CL_IsDisconnected(void)
{
	return (cls.state <= ca_disconnected);
}

void CL_RestartFilesystem( qboolean execAutoexec )
{
    connstate_t	cls_state;

    if ( cls.state == ca_uninitialized ) {
        FS_Restart();
		if (execAutoexec) {
			FS_ExecConfig("autoexec.cfg");
			Cbuf_Execute();
		}
        return;
    }

    Com_DPrintf( "CL_RestartFilesystem()\n" );

    /* temporary switch to loading state */
    cls_state = cls.state;
    if ( cls.state == ca_active ) {
//        cls.state = ca_loading;
    }

	S_Shutdown();
	VID_Shutdown();

    FS_Restart();
	
	if (execAutoexec) {
		silentSubsystem = CVAR_SYSTEM_VIDEO|CVAR_SYSTEM_SOUND;
		FS_ExecConfig("autoexec.cfg");
		Cbuf_Execute();
		silentSubsystem = 0;
	}

#ifndef VID_INITFIRST
	S_Init();	
	VID_Restart_f();
	VID_CheckChanges();
#else
	VID_Restart_f();
	VID_CheckChanges();
	S_Init();
#endif

    if ( cls_state == ca_disconnected ) {
        //UI_OpenMenu( UIMENU_MAIN );
    } else if ( cls_state > ca_connected ) {
		memset(cl.sound_precache, 0, sizeof(cl.sound_precache));
		CL_RegisterSounds();
        CL_PrepRefresh();
    }

    /* switch back to original state */
    cls.state = cls_state;
}

/*
=================
CL_InitLocal
=================
*/
void CL_InitLocal (void)
{
	cls.state = ca_disconnected;
	cls.realtime = Sys_Milliseconds ();

	CL_InitInput ();

	adr0 = Cvar_Get( "adr0", "tastyspleen.net", CVAR_ARCHIVE );
	adr1 = Cvar_Get( "adr1", "", CVAR_ARCHIVE );
	adr2 = Cvar_Get( "adr2", "", CVAR_ARCHIVE );
	adr3 = Cvar_Get( "adr3", "", CVAR_ARCHIVE );
	adr4 = Cvar_Get( "adr4", "", CVAR_ARCHIVE );
	adr5 = Cvar_Get( "adr5", "", CVAR_ARCHIVE );
	adr6 = Cvar_Get( "adr6", "", CVAR_ARCHIVE );
	adr7 = Cvar_Get( "adr7", "", CVAR_ARCHIVE );
	adr8 = Cvar_Get( "adr8", "", CVAR_ARCHIVE );

// register our variables
	cl_stereo_separation = Cvar_Get( "cl_stereo_separation", "0.4", CVAR_ARCHIVE );
	cl_stereo = Cvar_Get( "cl_stereo", "0", 0 );

	cl_add_blend = Cvar_Get ("cl_blend", "1", 0);
	cl_add_lights = Cvar_Get ("cl_lights", "1", 0);
	cl_add_particles = Cvar_Get ("cl_particles", "1", 0);
	cl_add_entities = Cvar_Get ("cl_entities", "1", 0);
	cl_gun = Cvar_Get ("cl_gun", "2", 0);
	cl_gun_x = Cvar_Get("cl_gun_x", "0", 0);
	cl_gun_y = Cvar_Get("cl_gun_y", "0", 0);
	cl_gun_z = Cvar_Get("cl_gun_z", "0", 0);
	cl_footsteps = Cvar_Get ("cl_footsteps", "1", 0);
	cl_noskins = Cvar_Get ("cl_noskins", "0", 0);
//	cl_autoskins = Cvar_Get ("cl_autoskins", "0", 0);
	cl_predict = Cvar_Get ("cl_predict", "1", 0);
	r_maxfps = Cvar_Get ("r_maxfps", "250", 0);
	cl_maxfps = Cvar_Get ("cl_maxfps", "125", 0);
	cl_async = Cvar_Get ("cl_async", "0", CVAR_ARCHIVE);

	cl_upspeed = Cvar_Get ("cl_upspeed", "200", 0);
	cl_forwardspeed = Cvar_Get ("cl_forwardspeed", "200", 0);
	cl_sidespeed = Cvar_Get ("cl_sidespeed", "200", 0);
	cl_yawspeed = Cvar_Get ("cl_yawspeed", "140", 0);
	cl_pitchspeed = Cvar_Get ("cl_pitchspeed", "150", 0);
	cl_anglespeedkey = Cvar_Get ("cl_anglespeedkey", "1.5", 0);

	cl_run = Cvar_Get ("cl_run", "1", CVAR_ARCHIVE);
	freelook = Cvar_Get( "freelook", "1", CVAR_ARCHIVE );
	lookspring = Cvar_Get ("lookspring", "0", CVAR_ARCHIVE);
	lookstrafe = Cvar_Get ("lookstrafe", "0", CVAR_ARCHIVE);
	sensitivity = Cvar_Get ("sensitivity", "3", CVAR_ARCHIVE);

	m_pitch = Cvar_Get ("m_pitch", "0.022", CVAR_ARCHIVE);
	m_yaw = Cvar_Get ("m_yaw", "0.022", 0);
	m_forward = Cvar_Get ("m_forward", "1", 0);
	m_side = Cvar_Get ("m_side", "1", 0);

	cl_shownet = Cvar_Get ("cl_shownet", "0", 0);
	cl_showmiss = Cvar_Get ("cl_showmiss", "0", 0);
	cl_showclamp = Cvar_Get ("showclamp", "0", 0);
	cl_timeout = Cvar_Get ("cl_timeout", "120", 0);
	cl_paused = Cvar_Get ("paused", "0", CVAR_CHEAT);
	cl_timedemo = Cvar_Get ("timedemo", "0", CVAR_CHEAT);

	rcon_client_password = Cvar_Get ("rcon_password", "", 0);
	rcon_address = Cvar_Get ("rcon_address", "", 0);

	cl_lightlevel = Cvar_Get ("r_lightlevel", "0", 0);

	// userinfo
	info_password = Cvar_Get ("password", "", CVAR_USERINFO);
	info_spectator = Cvar_Get ("spectator", "0", CVAR_USERINFO);
	info_name = Cvar_Get ("name", "unnamed", CVAR_USERINFO | CVAR_ARCHIVE);
	info_skin = Cvar_Get ("skin", "male/grunt", CVAR_USERINFO | CVAR_ARCHIVE);
	info_rate = Cvar_Get ("rate", "10000", CVAR_USERINFO | CVAR_ARCHIVE);	// FIXME
	info_msg = Cvar_Get ("msg", "1", CVAR_USERINFO | CVAR_ARCHIVE);
	info_hand = Cvar_Get ("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	info_fov = Cvar_Get ("fov", "90", CVAR_USERINFO | CVAR_ARCHIVE);
	info_gender = Cvar_Get ("gender", "male", CVAR_USERINFO | CVAR_ARCHIVE);
	gender_auto = Cvar_Get ("gender_auto", "1", CVAR_ARCHIVE);
	info_gender->modified = false; // clear this so we know when user sets it manually

	cl_vwep = Cvar_Get ("cl_vwep", "1", CVAR_ARCHIVE);


	CL_InitDemos();
	CL_InitLocs();
	CL_InitParse();
#ifdef AVI_EXPORT
	CL_InitAVIExport(); //AVI EXPORT
#endif
	cl_hudalpha = Cvar_Get ("cl_hudalpha",  "1", CVAR_ARCHIVE);
	cl_gunalpha = Cvar_Get("cl_gunalpha", "1", 0);
	Cmd_AddCommand ("serverstatus", CL_ServerStatus_f);
	Cmd_AddCommand ("cfg_save", CL_WriteConfig_f);

	cl_protocol = Cvar_Get ("cl_protocol", "35", CVAR_ARCHIVE);
	cl_protocol->OnChange = OnChange_Protocol;
	OnChange_Protocol(cl_protocol, cl_protocol->resetString);
	cl_gun->OnChange = OnChange_Gun;
	
	cl_instantpacket = Cvar_Get ("cl_instantpacket", "1", 0);

#ifdef USE_CURL
	cl_http_proxy = Cvar_Get ("cl_http_proxy", "", 0);
	cl_http_filelists = Cvar_Get ("cl_http_filelists", "1", 0);
	cl_http_downloads = Cvar_Get ("cl_http_downloads", "1", 0);
	cl_http_max_connections = Cvar_Get ("cl_http_max_connections", "2", 0);
	cl_http_max_connections->OnChange = OnChange_http_max_connections;
	OnChange_http_max_connections(cl_http_max_connections, cl_http_max_connections->resetString);
#endif

#ifdef GL_QUAKE
	cl_avidemo = Cvar_Get("cl_avidemo", "0", CVAR_CHEAT);
	cl_avidemoformat = Cvar_Get("cl_avidemoformat", "0", 0);
	cl_forceavidemo = Cvar_Get("cl_forceavidemo", "0", 0);
	cl_avidemo->OnChange = OnChange_AviDemo;
	OnChange_AviDemo(cl_avidemo, cl_avidemo->resetString);
#endif

	r_maxfps->OnChange = OnChange_MaxFps;
	OnChange_MaxFps(r_maxfps, r_maxfps->resetString);
	cl_maxfps->OnChange = OnChange_MaxFps;
	OnChange_MaxFps(cl_maxfps, cl_maxfps->resetString);
	info_name->OnChange = OnChange_Name;
	OnChange_Name(info_name, info_name->resetString);
	cl_stereo_separation->OnChange = OnChange_StereoSeparation;
	Cmd_AddMacro( "mapname", CL_Mapname_m );
	Cmd_AddMacro( "serverip", CL_Server_m );
	Cmd_AddMacro( "cl_ups", CL_Ups_m );
	Cmd_AddMacro( "cl_health", CL_Health_m );
	Cmd_AddMacro( "cl_ammo", CL_Ammo_m );
	Cmd_AddMacro( "cl_weaponmodel", CL_WeaponModel_m );


	// register our commands
	Cmd_AddCommand ("cmd", CL_ForwardToServer_f);
	Cmd_AddCommand ("pause", CL_Pause_f);
	Cmd_AddCommand ("pingservers", CL_PingServers_f);
	Cmd_AddCommand ("skins", CL_Skins_f);

	Cmd_AddCommand ("userinfo", CL_Userinfo_f);
	Cmd_AddCommand ("snd_restart", CL_Snd_Restart_f);

	Cmd_AddCommand ("changing", CL_Changing_f);
	Cmd_AddCommand ("disconnect", CL_Disconnect_f);

	Cmd_AddCommand ("quit", CL_Quit_f);

	Cmd_AddCommand ("connect", CL_Connect_f);
	Cmd_AddCommand ("reconnect", CL_Reconnect_f);

	Cmd_AddCommand ("rcon", CL_Rcon_f);

//	Cmd_AddCommand ("setenv", CL_Setenv_f );

	Cmd_AddCommand ("precache", CL_Precache_f);

	Cmd_AddCommand ("download", CL_Download_f);

	// forward to server commands
	// the only thing this does is allow command completion to work -
	// all unknown commands are automatically forwarded to the server
	Cmd_AddCommand ("wave", NULL);
	Cmd_AddCommand ("inven", NULL);
	Cmd_AddCommand ("kill", NULL);
	Cmd_AddCommand ("use", NULL);
	Cmd_AddCommand ("drop", NULL);
	Cmd_AddCommand ("say", NULL);
	Cmd_AddCommand ("say_team", NULL);
	Cmd_AddCommand ("info", NULL);
	Cmd_AddCommand ("prog", NULL);
	Cmd_AddCommand ("give", NULL);
	Cmd_AddCommand ("god", NULL);
	Cmd_AddCommand ("notarget", NULL);
	Cmd_AddCommand ("noclip", NULL);
	Cmd_AddCommand ("invuse", NULL);
	Cmd_AddCommand ("invprev", NULL);
	Cmd_AddCommand ("invnext", NULL);
	Cmd_AddCommand ("invdrop", NULL);
	Cmd_AddCommand ("weapnext", NULL);
	Cmd_AddCommand ("weapprev", NULL);
}



/*
===============
CL_WriteConfiguration

Writes key bindings and archived cvars to a config file
===============
*/
void CL_WriteConfiguration (void)
{
	FILE	*f;
	char	path[MAX_QPATH];

	if (cls.state == ca_uninitialized)
		return;

	Com_sprintf (path, sizeof(path),"%s/aprconfig.cfg",FS_Gamedir());
	f = fopen (path, "w");
	if (!f) {
		Com_Printf ("Couldn't write aprconfig.cfg.\n");
		return;
	}

	fprintf (f, "// generated by Quake2, do not modify\n");
	Key_WriteBindings (f);
	Cvar_WriteVariables (f);
	fclose (f);
}

/*
===============
CL_WriteConfig_f

===============
*/
void CL_WriteConfig_f (void)
{
	FILE	*f;
	char	path[MAX_QPATH];

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: %s <filename>\n", Cmd_Argv(0));
        return;
    }

	Com_sprintf (path, sizeof(path),"%s/%s.cfg", FS_Gamedir(), Cmd_Argv(1));
	f = fopen (path, "w");
	if (!f) {
		Com_Printf ("Couldn't write %s.\n", Cmd_Argv(1));
		return;
	}

	fprintf (f, "// *** BINDINGS ***\n");
	Key_WriteBindings (f);

	fprintf (f, "\n// *** VARIABLES ***\n");
	Cvar_WriteVariables (f);
	
	fprintf (f, "\n// *** ALIASES ***\n");
	Cmd_WriteAliases (f);

	fclose (f);
	Com_Printf ("Config saved to file %s\n", Cmd_Argv(1));
}

/*
==================
CL_FixCvarCheats

==================
*/
qboolean CL_CheatsOK(void)
{
	if( cls.state < ca_connected || cl.attractloop || Com_ServerState() == ss_demo)
		return true;

	// single player can cheat
	if( cl.maxclients == 1 )
		return true;

	// developer option
	if( Com_ServerState() == ss_game && Cvar_VariableIntValue( "cheats" ) )
		return true;

	return false;
}

//============================================================================

/*
==================
CL_Frame

==================
*/
void CL_DemoFrame( int msec );
void CL_UpdateCmd( int msec );

void CL_Frame (int msec)
{
	static int	extratime, packet_delta, misc_delta = 1000;
	qboolean	packet_frame = true;

	if (dedicated->integer)
		return;

	extratime += msec;
	packet_delta += msec;

	if (!cl_timedemo->integer)
	{
		if( cl_async->integer )
		{
			if (cls.state == ca_connected) {
				if (packet_delta < 100) {
					packet_frame = false;
#ifdef USE_CURL
					CL_RunHTTPDownloads (); //we run full speed when connecting
#endif
				}
			} else {
				if (packet_delta < 1000/cl_maxfps->integer)
					packet_frame = false;	// packetrate is too high
			}

			if (!packet_frame && extratime < 1000/r_maxfps->integer)
				return;	// framerate is too high
		}
		else
		{
			if (cls.state == ca_connected) {
				if (extratime < 100)
					return;			// don't flood packets out while connecting
			} else {
				if (extratime < 1000/cl_maxfps->integer)
					return;			// framerate is too high
			}
		}
	}

	cls.realtime = curtime;
	cls.frametime = extratime * 0.001f;
	// don't extrapolate too far ahead
	if (cls.frametime > 0.2f)
		cls.frametime = 0.2f;

	CL_DemoFrame(extratime);

	//if in the debugger last frame, don't timeout
	if (msec > 5000)
		cls.netchan.last_received = Sys_Milliseconds ();

	// process packets from server
	CL_ReadPackets();

	// get new key events
	Sys_SendKeyEvents();

	// allow mice or other external controllers to add commands
	IN_Commands();

	if (cl.sendPacketNow || userinfo_modified) {
		if (cls.state < ca_connected)
			userinfo_modified = false;
		else
			packet_frame = true;
	}

	CL_UpdateCmd(extratime);
	if (packet_frame) {
		// process console commands
		Cbuf_Execute();

		// send intentions now
		CL_SendCmd();

		// resend a connection request if necessary
		if(!cls.demoplaying)
			CL_CheckForResend();
#ifdef USE_CURL
		//we run less often in game
		CL_RunHTTPDownloads();
#endif
		packet_delta = 0;
	}

	misc_delta += extratime;
	extratime = 0;

	//Render the display
	// predict all unacknowledged movements
	CL_PredictMovement();

	// allow rendering DLL change
	VID_CheckChanges();
	if (!cl.refresh_prepped && cls.state == ca_active)
		CL_PrepRefresh();

	// update the screen
	if (host_speeds->integer) {
		time_before_ref = Sys_Milliseconds();
		SCR_UpdateScreen();
		time_after_ref = Sys_Milliseconds();
	} else {
		SCR_UpdateScreen();
	}

	// update audio
	S_Update(cl.refdef.vieworg, cl.v_forward, cl.v_right, cl.v_up);

	if(misc_delta > 99) { // don't need to do this stuff much.
		// let the mouse activate or deactivate
		IN_Frame();
#ifdef CD_AUDIO
		CDAudio_Update();
#endif
#if defined(_WIN32) || defined(WITH_XMMS)
		MP3_Frame();
#endif
		if (cls.spamTime && cls.spamTime < cls.realtime) {
			Cbuf_AddText ("say \"" APPLICATION " v" VERSION " " CPUSTRING " " BUILDSTRING "\"\n");
			cls.lastSpamTime = cls.realtime;
			cls.spamTime = 0;
		}
		misc_delta = 0;
	}

#ifdef GL_QUAKE
#ifdef AVI_EXPORT
	if (CL_AviRecording()) {
		AVI_ProcessFrame (); //Avi export
	}
	else
#endif
	if(cl_avidemo->integer) {
		if((cls.state == ca_active && cl.attractloop) || (cls.state != ca_active && cl_forceavidemo->integer))
			R_WriteAviFrame();
	}
#endif
	// advance local effects for next frame
	CL_RunDLights();
	CL_RunLightStyles();
	SCR_RunCinematic();
	SCR_RunConsole();

	//cls.framecount++;
}

//============================================================================

/*
====================
CL_Init
====================
*/
void CL_Init (void)
{
	if (dedicated->integer)
		return;		// nothing running on the client

	// all archived variables will now be loaded

	Con_Init ();

#ifndef VID_INITFIRST
	S_Init ();	
	VID_Init ();
	Con_CheckResize();
#else
	VID_Init ();
	Con_CheckResize();
	S_Init ();	// sound must be initialized after window is created
#endif
	
	V_Init ();
	
	SZ_Init (&net_message, net_message_buffer, sizeof(net_message_buffer));

	M_Init ();	
	
	SCR_Init ();
	cls.disable_screen = true;	// don't draw yet

#ifdef CD_AUDIO
	CDAudio_Init ();
#endif
	CL_InitLocal ();

	IN_Init ();

#if defined(_WIN32) || defined(WITH_XMMS) || defined(WITH_MPD)
	MP3_Init();
#endif

#ifdef USE_CURL
	CL_InitHTTPDownloads ();
#endif

	//FS_ExecAutoexec ();
	//Cbuf_Execute ();
}


/*
===============
CL_Shutdown

FIXME: this is a callback from Sys_Quit and Com_Error.  It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void CL_Shutdown(void)
{
	static qboolean isdown = false;
	
	if (isdown)
	{
		//printf ("recursive shutdown\n");
		return;
	}
	isdown = true;

#ifdef USE_CURL
	CL_HTTP_Cleanup (true);
#endif
#ifdef AVI_EXPORT
	CL_ShutdownAVIExport();
#endif
	CL_FreeLocs ();
	CL_WriteConfiguration (); 

#ifdef CD_AUDIO
	CDAudio_Shutdown ();
#endif
	S_Shutdown();
#if defined(_WIN32) || defined(WITH_XMMS) || defined(WITH_MPD)
	if (!dedicated->integer)
		MP3_Shutdown();
#endif

	IN_Shutdown ();
	VID_Shutdown();
}

