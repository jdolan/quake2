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

#include "client.h"

cvar_t	*cl_clan;
cvar_t	*cl_autorecord;

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length
====================
*/
void CL_WriteDemoMessageFull( sizebuf_t *msg )
{
	int		len, swlen;

	// the first eight bytes are just packet sequencing stuff
	len = msg->cursize - 8;
	swlen = LittleLong( len );

	if(swlen > 0) // skip bad packets
	{
		FS_Write( &swlen, 4, cls.demofile);
		FS_Write( msg->data + 8, len, cls.demofile);
	}
}

void CL_WriteDemoMessage (byte *buff, int len, qboolean forceFlush)
{
	if (forceFlush)
	{
		if (!cls.demowaiting)
		{
			int	swlen;

			if (cl.demoBuff.overflowed)
			{
				Com_DPrintf ("Dropped a demo frame, maximum message size exceeded: %d > %d\n", cl.demoBuff.cursize, cl.demoBuff.maxsize);

				//we write a message regardless to keep in sync time-wise.
				SZ_Clear (&cl.demoBuff);
				MSG_WriteByte (&cl.demoBuff, svc_nop);
			}

			swlen = LittleLong(cl.demoBuff.cursize);
			FS_Write (&swlen, 4, cls.demofile);
			FS_Write (cl.demoFrame, cl.demoBuff.cursize, cls.demofile);
		}
		SZ_Clear (&cl.demoBuff);
	}

	if (len)
		SZ_Write (&cl.demoBuff, buff, len);
}

/*
====================
CL_CloseDemoFile
====================
*/
void CL_CloseDemoFile( void )
{
	int len;

	if (!cls.demofile)
		return;

	// finish up
	len = -1;
	FS_Write(&len, 4, cls.demofile);

	FS_FCloseFile( cls.demofile );
	 
	cls.demofile = 0;

	// inform server we are done with extra data
	if (cls.serverProtocol == PROTOCOL_VERSION_R1Q2)
	{
		MSG_WriteByte  (&cls.netchan.message, clc_setting);
		MSG_WriteShort (&cls.netchan.message, CLSET_RECORDING);
		MSG_WriteShort (&cls.netchan.message, 0);
	}
}


/*
====================
CL_Stop_f

stop recording a demo
====================
*/
void CL_Stop_f( void )
{
	if( !cls.demorecording )
	{
		Com_Printf( "Not recording a demo.\n" );
		return;
	}

	CL_CloseDemoFile();
	cls.demorecording = false;
	Com_Printf( "Stopped demo.\n" );
}

void CL_StopAutoRecord (void)
{
	if(cl_autorecord->integer && cls.demorecording)
		CL_Stop_f();
}

void CL_StartRecording(char *name)
{
	byte	buf_data[1390];
	sizebuf_t	buf;
	int		i, len;
	entity_state_t	*ent;
	char *string;

	FS_CreatePath( name );
	FS_FOpenFile( name, &cls.demofile, FS_MODE_WRITE );
	if( !cls.demofile ) {
		Com_Printf( "ERROR: Couldn't open demo file %s.\n", name );
		return;
	}

	Com_Printf( "Recording demo to %s.\n", name );

	cls.demorecording = true;

	// don't start saving messages until a non-delta compressed message is received
	cls.demowaiting = true;

	// inform server we need to receive more data
	if (cls.serverProtocol == PROTOCOL_VERSION_R1Q2)
	{
		MSG_WriteByte  (&cls.netchan.message, clc_setting);
		MSG_WriteShort (&cls.netchan.message, CLSET_RECORDING);
		MSG_WriteShort (&cls.netchan.message, 1);
	}

	//
	// write out messages to hold the startup information
	//
	SZ_Init( &buf, buf_data, sizeof( buf_data ) );

	// send the serverdata
	MSG_WriteByte( &buf, svc_serverdata );
	MSG_WriteLong( &buf, PROTOCOL_VERSION_DEFAULT );
	MSG_WriteLong( &buf, 0x10000 + cl.servercount );
	MSG_WriteByte( &buf, 1 );	// demos are always attract loops
	MSG_WriteString( &buf, cl.gamedir );
	MSG_WriteShort( &buf, cl.playernum );
	MSG_WriteString( &buf, cl.configstrings[CS_NAME] );

	// configstrings
	for( i=0 ; i<MAX_CONFIGSTRINGS ; i++ )
	{
		string = cl.configstrings[i];
		if( !string[0] )
			continue;
		
		if( buf.cursize + strlen( string ) + 32 > buf.maxsize )
		{	// write it out
			len = LittleLong (buf.cursize);
			FS_Write(&len, 4, cls.demofile);
			FS_Write(buf.data, buf.cursize, cls.demofile);
			buf.cursize = 0;
		}

		MSG_WriteByte( &buf, svc_configstring );
		MSG_WriteShort( &buf, i );
		MSG_WriteString( &buf, string );
	}

	// baselines
	for( i=1; i<MAX_EDICTS ; i++ )
	{
		ent = &cl_entities[i].baseline;
		if( !ent->modelindex )
			continue;

		if( buf.cursize + 64 > buf.maxsize )
		{	// write it out
			len = LittleLong (buf.cursize);
			FS_Write(&len, 4, cls.demofile);
			FS_Write(buf.data, buf.cursize, cls.demofile);
			buf.cursize = 0;
		}

		MSG_WriteByte( &buf, svc_spawnbaseline );
		MSG_WriteDeltaEntity(NULL, ent, &buf, true, false);
	}

	MSG_WriteByte( &buf, svc_stufftext );
	MSG_WriteString( &buf, "precache\n" );

	// write it to the demo file
	len = LittleLong (buf.cursize);
	FS_Write(&len, 4, cls.demofile);
	FS_Write(buf.data, buf.cursize, cls.demofile);

	// the rest of the demo file will be individual frames
}

/*
====================
CL_Record_f

record <demoname>

Begins recording a demo from the current position
====================
*/
void CL_Record_f( void )
{
	int		i, c;
    char 	name[MAX_OSPATH], timebuf[32], *ext;
    time_t	clock;

	c = Cmd_Argc();
	if( c != 1 && c != 2) {
		Com_Printf( "Usage: %s [demoname]\n", Cmd_Argv(0) );
		return;
	}

	if( cls.demorecording ) {
		Com_Printf( "Already recording.\n" );
		return;
	}

	if( cls.state != ca_active ) {
		Com_Printf( "You must be in a level to record.\n" );
		return;
	}

	if (cl.attractloop) {
		Com_Printf ("Unable to record from a demo stream due to insufficient deltas.\n");
		return;
	}

	ext = "dm2";
	//
	// open the demo file
	//
    if (c == 1)
    {
		time( &clock );
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d", localtime(&clock));

        Com_sprintf (name, sizeof(name), "demos/%s_%s_%s.%s", timebuf, cl_clan->string, cls.mapname, ext);
        for (i=2; i<100; i++)
        {
			if (FS_LoadFileEx( name, NULL, FS_TYPE_REAL|FS_PATH_GAME) == -1)
				break;

			Com_sprintf (name, sizeof(name), "demos/%s_%s_%s_%i%i.%s", timebuf, cl_clan->string, cls.mapname, (int)(i/10)%10, i%10, ext);
		}
		if (i == 100)
		{
			Com_Printf ("ERROR: Too many demos with same name.\n");
			return;
		}
    }
	else
	{
		Com_sprintf( name, sizeof( name ), "demos/%s.%s", Cmd_Argv( 1 ), ext);
	}

	CL_StartRecording(name);
}

//----------------------------------------------------
//		AUTO RECORD
//----------------------------------------------------
void CL_StartAutoRecord(void)
{
	char	timebuf[32], name[MAX_OSPATH];
	time_t	clock;

	if (cls.state != ca_active)
		return;

	if (!cl_autorecord->integer || cls.demorecording || cl.attractloop)
		return;

	time( &clock );
	strftime( timebuf, sizeof(timebuf), "%Y-%m-%d_%H-%M-%S", localtime(&clock));

	if(strlen(cl_clan->string) > 2)
		Com_sprintf(name, sizeof(name), "demos/%s_%s_%s.dm2", timebuf, cl_clan->string, cls.mapname);
	else
		Com_sprintf(name, sizeof(name), "demos/%s_%s.dm2", timebuf, cls.mapname);	

	CL_StartRecording(name);
}

static void OnChange_AutoRecord(cvar_t *self, const char *oldValue)
{
	CL_StartAutoRecord();
}

/*
==============
CL_Demo_List_f
==============
*/
void CL_Demo_List_f ( void )
{

	char	findname[MAX_OSPATH], *tmp;
	char	**dirnames;
	int		ndirs;

	Com_sprintf(findname, sizeof(findname), "%s/demos/*%s*.dm2", FS_Gamedir(), Cmd_Argv( 1 )) ;

	tmp = findname;
	while ((tmp = strchr(tmp, '\\')))
		*tmp = '/';

	Com_Printf( "Directory of %s\n", findname );
	Com_Printf( "----\n" );

	if( ( dirnames = FS_ListFiles( findname, &ndirs, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM ) ) != 0 )
	{
		int i;

		for( i = 0; i < ndirs-1; i++ )
		{
			if( strrchr( dirnames[i], '/' ) )
				Com_Printf( "%i %s\n", i, strrchr( dirnames[i], '/' ) + 1 );
			else
				Com_Printf( "%i %s\n", i, dirnames[i] );

			Z_Free( dirnames[i] );
		}
		Z_Free( dirnames );
	}
}

/*
==============
CL_Demo_Play_f
==============
*/
void CL_Demo_Play_f ( void )
{

	char	findname[MAX_OSPATH], *tmp;
	char	**dirnames;
	int		ndirs, find, found = 0;
	char	buf[MAX_OSPATH];

	if(Cmd_Argc() == 1)
	{
		Com_Printf("Usage: %s <id> [search card]\n", Cmd_Argv(0)) ;
		return;
	}

	find = atoi(Cmd_Argv (1));

	Com_sprintf (findname, sizeof(findname), "%s/demos/*%s*.dm2", FS_Gamedir(), Cmd_Argv( 2 ));

	tmp = findname;
	while ((tmp = strchr(tmp, '\\')))
		*tmp = '/';

	Com_Printf( "Directory of %s\n", findname );
	Com_Printf( "----\n" );

	if( ( dirnames = FS_ListFiles( findname, &ndirs, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM ) ) != 0 )
	{
		int i;

		for( i = 0; i < ndirs-1; i++ )
		{
			if (i == find)
			{
				if( strrchr( dirnames[i], '/' ) )
				{
					Com_Printf( "%i %s\n", i, strrchr( dirnames[i], '/' ) + 1 );
					found = 1;
					Com_sprintf(buf, sizeof(buf), "demo \"%s\"\n", strrchr( dirnames[i], '/' ) + 1);
				}	
				else
					Com_Printf( "%i %s\n", i, dirnames[i] );
			}

			Z_Free( dirnames[i] );
		}
		Z_Free( dirnames );
	}

	if (found)		
		Cbuf_AddText(buf);
}


/*
=================
CL_DemoCompleted
=================
*/
void CL_StopDemoFile( void ) {
	if( cls.demofile ) {
		FS_FCloseFile( cls.demofile );
		cls.demofile = 0;
	}
	cls.demoplaying = false;
}

void CL_DemoCompleted( void ) {
	if (cl_timedemo && cl_timedemo->integer) {
		unsigned int time;
		
		time = Sys_Milliseconds() - cls.timeDemoStart;
		if ( time > 0 ) {
			Com_Printf ("%i frames, %3.1f seconds: %3.1f fps\n", cls.timeDemoFrames,
			time/1000.0f, cls.timeDemoFrames*1000.0f / time);
		}
	}
	CL_Disconnect();
}

/*
=================
CL_ReadDemoMessage
=================
*/
void CL_ReadDemoMessage( void ) {
	int			r;
	sizebuf_t	buf;
	byte		bufData[MAX_MSGLEN];

	if ( !cls.demofile ) {
		CL_DemoCompleted ();
		return;
	}

	// init the message
	SZ_Init( &buf, bufData, sizeof( bufData ) );
	buf.allowoverflow = true;

	// get the length
	r = FS_Read (&buf.cursize, 4, cls.demofile);
	if ( r != 4 ) {
		CL_DemoCompleted ();
		return;
	}
	buf.cursize = LittleLong( buf.cursize );
	if ( buf.cursize == -1 ) {
		CL_DemoCompleted ();
		return;
	}
	if ( buf.cursize > buf.maxsize ) {
		Com_Error (ERR_DROP, "CL_ReadDemoMessage: demoMsglen > MAX_MSGLEN");
	}
	r = FS_Read( buf.data, buf.cursize, cls.demofile );
	if ( r != buf.cursize ) {
		Com_Printf( "Demo file was truncated.\n");
		CL_DemoCompleted ();
		return;
	}

	buf.readcount = 0;
	CL_ParseServerMessage( &buf );
}

/*
====================
CL_PlayDemo_f

demo <demoname>
====================
*/
qboolean skipFirst = false;

void CL_PlayDemo_f( void ) {
	char		name[MAX_OSPATH];
	char		*arg;
	int			len;
	fileHandle_t demofile;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <name>\n", Cmd_Argv(0));
		return;
	}

	// open the demo file
	arg = Cmd_Argv(1);

	Com_sprintf( name, sizeof( name ), "demos/%s", arg);
	COM_DefaultExtension(name, sizeof(name), ".dm2");

	len = FS_FOpenFile (name, &demofile, FS_MODE_READ);
	if (!demofile) {
		Com_Printf( "couldn't open %s\n", name);
		return;
	}

	// make sure a local server is killed
	if( Com_ServerState() ) {
		SV_Shutdown( "Server quit\n", false );
	}

	CL_Disconnect();

//	Con_Close();

	cls.state = ca_connected;
	cls.demofile = demofile;
	cls.demoplaying = true;
	Q_strncpyz( cls.servername, COM_SkipPath(name), sizeof( cls.servername ) );

	if( cl_timedemo->integer ) {
		cls.timeDemoFrames = 0;
		cls.timeDemoStart = Sys_Milliseconds();
	}

	// read demo messages until connected
	do {
		CL_ReadDemoMessage();
	} while( cls.state == ca_connected );

	Cbuf_Execute();

	// don't get the first snapshot this frame, to prevent the long
	// time from the gamestate load from messing causing a time skip
	//clc.firstDemoFrameSkipped = qfalse;
	//cl.serverTime = -1;
	skipFirst = true;
}

cvar_t *cl_demotimescale;

void CL_DemoFrame( int msec ) {
	static float frac;
	int dt;

	if( !cls.demoplaying ) {
		cl.time += msec;
		return;
	}

	if( cls.state < ca_connected ) {
		return;
	}

	if( cls.state != ca_active ) {
		CL_ReadDemoMessage();
		return;
	}

	if( cl_timedemo->integer ) {
		CL_ReadDemoMessage();
		cl.time = cl.serverTime;
		cls.timeDemoFrames++;
		return;
	}

	if( cl_demotimescale->value < 0 ) {
		Cvar_Set( "cl_demotimescale", "0" );
	} else if( cl_demotimescale->value > 1000 ) {
		Cvar_Set( "cl_demotimescale", "1000" );
	}

	if( cl_demotimescale->value ) {
		frac += msec * cl_demotimescale->value;
		dt = frac;
		frac -= dt;

		cl.time += dt;
	}

	// Skip the first frame
	if( skipFirst) {
		//cl.serverTime = cl.time;
		skipFirst = false;
		return;
	}

	while( cl.serverTime < cl.time ) {
		CL_ReadDemoMessage();
	}

}

/*
====================
CL_InitDemos
====================
*/
void CL_InitDemos( void )
{
	cl_demotimescale = Cvar_Get( "cl_demotimescale", "1", 0 );
	cl_clan = Cvar_Get ("cl_clan", "", 0);
	Cmd_AddCommand( "record", CL_Record_f );
	Cmd_AddCommand( "stop", CL_Stop_f );

	Cmd_AddCommand ("demolist", CL_Demo_List_f );
	Cmd_AddCommand ("demoplay", CL_Demo_Play_f );

	Cmd_AddCommand ("demo", CL_PlayDemo_f);

	cl_autorecord = Cvar_Get("cl_autorecord", "0", 0);
	cl_autorecord->OnChange = OnChange_AutoRecord;
}
