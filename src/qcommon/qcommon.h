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

// qcommon.h -- definitions common between client and server, but not game.dll

#include "../game/q_shared.h"


#define APPLICATION "Quake II (Quetoo.org)"

#define	VERSION		"1.213"


#define	BASEDIRNAME	"baseq2"

//============================================================================

typedef struct sizebuf_s
{
	qboolean	allowoverflow;	// if false, do a Com_Error
	qboolean	overflowed;		// set to true if the buffer size failed
	byte	*data;
	int		maxsize;
	int		cursize;
	int		readcount;
} sizebuf_t;

void SZ_Init (sizebuf_t *buf, byte *data, int length);
void SZ_Clear (sizebuf_t *buf);
void *SZ_GetSpace (sizebuf_t *buf, int length);

void SZ_Write (sizebuf_t *buf, const void *data, int length);
void SZ_Print (sizebuf_t *buf, const char *data);	// strcats onto the sizebuf

//============================================================================

struct usercmd_s;
struct entity_state_s;

void MSG_WriteChar (sizebuf_t *sb, int c);
void MSG_WriteByte (sizebuf_t *sb, int c);
void MSG_WriteShort (sizebuf_t *sb, int c);
void MSG_WriteLong (sizebuf_t *sb, int c);
void MSG_WriteFloat (sizebuf_t *sb, float f);
void MSG_WriteString (sizebuf_t *sb, const char *s);
void MSG_WriteCoord (sizebuf_t *sb, float f);
void MSG_WritePos (sizebuf_t *sb, const vec3_t pos);
void MSG_WriteAngle (sizebuf_t *sb, float f);
void MSG_WriteAngle16 (sizebuf_t *sb, float f);
void MSG_WriteDeltaUsercmd (sizebuf_t *sb, const struct usercmd_s *from, const struct usercmd_s *cmd, int protocol);
void MSG_WriteDeltaEntity (const struct entity_state_s *from, const struct entity_state_s *to, sizebuf_t *msg, qboolean force, qboolean newentity);
void MSG_WriteDir (sizebuf_t *sb, const vec3_t vector);

void MSG_WriteDeltaPlayerstate_Default (const player_state_t *from, const player_state_t *to, sizebuf_t *msg);


void	MSG_BeginReading (sizebuf_t *sb);

int		MSG_ReadChar (sizebuf_t *sb);
int		MSG_ReadByte (sizebuf_t *sb);
int		MSG_ReadShort (sizebuf_t *sb);
int		MSG_ReadLong (sizebuf_t *sb);
float	MSG_ReadFloat (sizebuf_t *sb);
char	*MSG_ReadString (sizebuf_t *sb);
char	*MSG_ReadStringLine (sizebuf_t *sb);

float	MSG_ReadCoord (sizebuf_t *sb);
void	MSG_ReadPos (sizebuf_t *sb, vec3_t pos);
float	MSG_ReadAngle (sizebuf_t *sb);
float	MSG_ReadAngle16 (sizebuf_t *sb);
void	MSG_ReadDeltaUsercmd (sizebuf_t *sb, const struct usercmd_s *from, struct usercmd_s *cmd);

void	MSG_ReadDir (sizebuf_t *sb, vec3_t vector);

void	MSG_ReadData (sizebuf_t *sb, void *buffer, int size);

void MSG_ParseDeltaEntity ( sizebuf_t *msg, const entity_state_t *from, entity_state_t *to, int number, int bits, int protocol );

void MSG_ParseDeltaPlayerstate_Default ( sizebuf_t *msg, const player_state_t *from, player_state_t *to, int flags );
void MSG_ParseDeltaPlayerstate_Enhanced( sizebuf_t *msg, const player_state_t *from, player_state_t *to, int flags, int extraflags );


int ZLibCompressChunk(byte *in, int len_in, byte *out, int len_out, int method, int wbits);
int ZLibDecompress (byte *in, int inlen, byte /*@out@*/*out, int outlen, int wbits);

//============================================================================


int	COM_Argc (void);
char *COM_Argv (int arg);	// range and null checked
void COM_ClearArgv (int arg);
int COM_CheckParm (const char *parm);
void COM_AddParm (char *parm);

void COM_Init (void);
void COM_InitArgv (int argc, char **argv);


//============================================================================

void Info_Print (const char *s);


/* crc.h */

void CRC_Init(uint16 *crcvalue);
void CRC_ProcessByte(uint16 *crcvalue, byte data);
uint16 CRC_Value(uint16 crcvalue);
uint16 CRC_Block (byte *start, int count);



/*
==============================================================

PROTOCOL

==============================================================
*/

// protocol.h -- communications protocols

#define	PROTOCOL_VERSION_OLD		26
#define	PROTOCOL_VERSION_DEFAULT	34
#define PROTOCOL_VERSION_R1Q2		35

#define PROTOCOL_VERSION_R1Q2_MINIMUM	1902
#define PROTOCOL_VERSION_R1Q2_STRAFE	1903
#define PROTOCOL_VERSION_R1Q2_UCMD		1904	// b7387
#define PROTOCOL_VERSION_R1Q2_SOLID		1905
#define PROTOCOL_VERSION_R1Q2_CURRENT	1905

//=========================================

#define	PORT_MASTER	27900
#define	PORT_CLIENT	27901
#define	PORT_SERVER	27910

//=========================================

#define	UPDATE_BACKUP	16	// copies of entity_state_t to keep buffered must be power of two
#define	UPDATE_MASK		(UPDATE_BACKUP-1)

#define NET_NONE		0
#define NET_CLIENT		1
#define NET_SERVER		2


#define SVCMD_BITS				5
#define SVCMD_MASK				( ( 1 << SVCMD_BITS ) - 1 )

#define	FRAMENUM_BITS			27
#define FRAMENUM_MASK			( ( 1 << FRAMENUM_BITS ) - 1 )

#define SURPRESSCOUNT_BITS		4
#define SURPRESSCOUNT_MASK		( ( 1 << SURPRESSCOUNT_BITS ) - 1 )

//==================
// the svc_strings[] array in cl_parse.c should mirror this
//==================

//
// server to client
//
enum svc_ops_e
{
	svc_bad,

	// these ops are known to the game dll
	svc_muzzleflash,
	svc_muzzleflash2,
	svc_temp_entity,
	svc_layout,
	svc_inventory,

	// the rest are private to the client and server
	svc_nop,
	svc_disconnect,
	svc_reconnect,
	svc_sound,					// <see code>
	svc_print,					// [byte] id [string] null terminated string
	svc_stufftext,				// [string] stuffed into client's console buffer, should be \n terminated
	svc_serverdata,				// [long] protocol ...
	svc_configstring,			// [short] [string]
	svc_spawnbaseline,		
	svc_centerprint,			// [string] to put in center of the screen
	svc_download,				// [short] size [size bytes]
	svc_playerinfo,				// variable
	svc_packetentities,			// [...]
	svc_deltapacketentities,	// [...]
	svc_frame,

	// ********** r1q2 specific ***********
	svc_zpacket,
	svc_zdownload,
	svc_playerupdate,
	svc_setting,
	// ********** end r1q2 specific *******

	svc_max_enttypes
};

typedef enum {
	ss_dead,			// no map loaded
	ss_loading,			// spawning level edicts
	ss_game,			// actively running
	ss_cinematic,
	ss_demo,
	ss_pic
} server_state_t;

typedef enum
{
	CLSET_NOGUN,
	CLSET_NOBLEND,
	CLSET_RECORDING,
	CLSET_PLAYERUPDATE_REQUESTS,
	CLSET_MAX
} clientsetting_t;

typedef enum
{
	SVSET_PLAYERUPDATES,
	SVSET_MAX
} serversetting_t;

//==============================================

//
// client to server
//
enum clc_ops_e
{
	clc_bad,
	clc_nop, 		
	clc_move,				// [[usercmd_t]
	clc_userinfo,			// [[userinfo string]
	clc_stringcmd,			// [string] message
	clc_setting				// [setting][value] R1Q2 settings support.
};

//==============================================

// plyer_state_t communication

#define	PS_M_TYPE			(1<<0)
#define	PS_M_ORIGIN			(1<<1)
#define	PS_M_VELOCITY		(1<<2)
#define	PS_M_TIME			(1<<3)
#define	PS_M_FLAGS			(1<<4)
#define	PS_M_GRAVITY		(1<<5)
#define	PS_M_DELTA_ANGLES	(1<<6)

#define	PS_VIEWOFFSET		(1<<7)
#define	PS_VIEWANGLES		(1<<8)
#define	PS_KICKANGLES		(1<<9)
#define	PS_BLEND			(1<<10)
#define	PS_FOV				(1<<11)
#define	PS_WEAPONINDEX		(1<<12)
#define	PS_WEAPONFRAME		(1<<13)
#define	PS_RDFLAGS			(1<<14)
#define	PS_BBOX				(1<<15)

#define PS_BITS				16
#define PS_MASK				( ( 1 << PS_BITS ) - 1 )

/* r1q2 protocol specific extra flags */
#define	EPS_GUNOFFSET		(1<<0)
#define	EPS_GUNANGLES		(1<<1)
#define	EPS_VELOCITY2		(1<<2)
#define	EPS_ORIGIN2			(1<<3)
#define	EPS_VIEWANGLE2		(1<<4)
#define	EPS_STATS			(1<<5)
//==============================================

// user_cmd_t communication

// ms and light always sent, the others are optional
#define	CM_ANGLE1 	(1<<0)
#define	CM_ANGLE2 	(1<<1)
#define	CM_ANGLE3 	(1<<2)
#define	CM_FORWARD	(1<<3)
#define	CM_SIDE		(1<<4)
#define	CM_UP		(1<<5)
#define	CM_BUTTONS	(1<<6)
#define	CM_IMPULSE	(1<<7)

 // r1q2 button byte hacks
#define BUTTON_MASK (BUTTON_ATTACK|BUTTON_USE|BUTTON_ANY)
#define BUTTON_FORWARD  4
#define BUTTON_SIDE     8
#define BUTTON_UP       16
#define BUTTON_ANGLE1   32
#define BUTTON_ANGLE2   64

#define	BUTTON_UCMD_DBLFORWARD	4
#define BUTTON_UCMD_DBLSIDE		8
#define	BUTTON_UCMD_DBLUP		16
#define BUTTON_UCMD_DBL_ANGLE1	32
#define BUTTON_UCMD_DBL_ANGLE2	64

//==============================================

// a sound without an ent or pos will be a local only sound
#define	SND_VOLUME		(1<<0)		// a byte
#define	SND_ATTENUATION	(1<<1)		// a byte
#define	SND_POS			(1<<2)		// three coordinates
#define	SND_ENT			(1<<3)		// a short 0-2: channel, 3-12: entity
#define	SND_OFFSET		(1<<4)		// a byte, msec offset from frame start

#define DEFAULT_SOUND_PACKET_VOLUME	1.0f
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0f

//==============================================

// entity_state_t communication

// try to pack the common update flags into the first byte
#define	U_ORIGIN1	(1<<0)
#define	U_ORIGIN2	(1<<1)
#define	U_ANGLE2	(1<<2)
#define	U_ANGLE3	(1<<3)
#define	U_FRAME8	(1<<4)		// frame is a byte
#define	U_EVENT		(1<<5)
#define	U_REMOVE	(1<<6)		// REMOVE this entity, don't add it
#define	U_MOREBITS1	(1<<7)		// read one additional byte

// second byte
#define	U_NUMBER16	(1<<8)		// NUMBER8 is implicit if not set
#define	U_ORIGIN3	(1<<9)
#define	U_ANGLE1	(1<<10)
#define	U_MODEL		(1<<11)
#define U_RENDERFX8	(1<<12)		// fullbright, etc
#define	U_EFFECTS8	(1<<14)		// autorotate, trails, etc
#define	U_MOREBITS2	(1<<15)		// read one additional byte

// third byte
#define	U_SKIN8		(1<<16)
#define	U_FRAME16	(1<<17)		// frame is a short
#define	U_RENDERFX16 (1<<18)	// 8 + 16 = 32
#define	U_EFFECTS16	(1<<19)		// 8 + 16 = 32
#define	U_MODEL2	(1<<20)		// weapons, flags, etc
#define	U_MODEL3	(1<<21)
#define	U_MODEL4	(1<<22)
#define	U_MOREBITS3	(1<<23)		// read one additional byte

// fourth byte
#define	U_OLDORIGIN	(1<<24)		// FIXME: get rid of this
#define	U_SKIN16	(1<<25)
#define	U_SOUND		(1<<26)
#define	U_SOLID		(1<<27)


/*
==============================================================

CMD

Command text buffering and command execution

==============================================================
*/

/*

Any number of commands can be added in a frame, from several different sources.
Most commands come from either keybindings or console line input, but remote
servers can also send across commands and entire text files can be execed.

The + command line options are also added to the command buffer.

The game starts with a Cbuf_AddText ("exec quake.rc\n"); Cbuf_Execute ();

*/

void Cbuf_Init (void);
// allocates an initial text buffer that will grow as needed

void Cbuf_AddText (const char *text);
// as new commands are generated from the console or keybindings,
// the text is added to the end of the command buffer.

void Cbuf_InsertText (const char *text);
// when a command wants to issue other commands immediately, the text is
// inserted at the beginning of the buffer, before any remaining unexecuted
// commands.

void Cbuf_ExecuteText (int exec_when, const char *text);
// this can be used in place of either Cbuf_AddText or Cbuf_InsertText

void Cbuf_AddEarlyCommands (qboolean clear);
// adds all the +set commands from the command line

qboolean Cbuf_AddLateCommands (void);
// adds all the remaining + commands from the command line
// Returns true if any late commands were added, which
// will keep the demoloop from immediately starting

void Cbuf_Execute (void);
// Pulls off \n terminated lines of text from the command buffer and sends
// them through Cmd_ExecuteString.  Stops when the buffer is empty.
// Normally called once per frame, but may be explicitly invoked.
// Do not call inside a command function!

void Cbuf_CopyToDefer (void);
void Cbuf_InsertFromDefer (void);
// These two functions are used to defer any pending commands while a map
// is being loaded

//===========================================================================

/*

Command execution takes a null terminated string, breaks it into tokens,
then searches for a command or variable that matches the first token.

*/

typedef void ( *xcommand_t ) ( void );
typedef void ( *xmacro_t )( char *, int );

void	Cmd_Init (void);

void	Cmd_AddCommand (const char *cmd_name, xcommand_t function);
// called by the init functions of other parts of the program to
// register commands and functions to call for them.
// The cmd_name is referenced later, so it should not be in temp memory
// if function is NULL, the command will be forwarded to the server
// as a clc_stringcmd instead of executed locally
void	Cmd_RemoveCommand (const char *cmd_name);

void	Cmd_CommandCompletion( const char *partial, void(*callback)(const char *name, const char *value) );

//qboolean Cmd_Exists (char *cmd_name);
// used by the cvar code to check for cvar / command name overlap

//char 	*Cmd_CompleteCommand (const char *partial);
// attempts to match a partial command for automatic command line completion
// returns NULL if nothing fits

int		Cmd_Argc (void);
char	*Cmd_Argv (int arg);
char	*Cmd_Args (void);
// The functions that execute commands get their parameters with these
// functions. Cmd_Argv () will return an empty string, not a NULL
// if arg > argc, so string operations are always safe.

const char *Cmd_MacroExpandString (const char *text);

void	Cmd_TokenizeString (const char *text, qboolean macroExpand);
// Takes a null terminated string.  Does not need to be /n terminated.
// breaks the string up into arg tokens.

void	Cmd_ExecuteString (const char *text);
// Parses a single line of text into arguments and tries to execute it
// as if it was typed at the console

void	Cmd_ForwardToServer (void);
// adds the current command line as a clc_stringcmd to the client message.
// things like godmode, noclip, etc, are commands directed to the server,
// so when they are typed in at the console, they will need to be forwarded.

void Cmd_WriteAliases (FILE *f);

char *Cmd_ArgsFrom (int arg);

void Cmd_AddMacro( const char *name, xmacro_t function );
xmacro_t Cmd_FindMacroFunction( const char *name );
void Cmd_ExecTrigger( const char *string );

/*
==============================================================

CVAR

==============================================================
*/

/*

cvar_t variables are used to hold scalar or string variables that can be changed or displayed at the console or prog code as well as accessed directly
in C code.

The user can access cvars from the console in three ways:
r_draworder			prints the current value
r_draworder 0		sets the current value to 0
set r_draworder 0	as above, but creates the cvar if not present
Cvars are restricted from having the same names as commands to keep this
interface from being ambiguous.
*/

#define CVAR_LATCHED	32	// save changes until restarting its subsystem
#define CVAR_CHEAT		128	// will be reset to default unless cheats are enabled
#define CVAR_USER_CREATED 256 // user own cvars created with set
#define CVAR_ROM		512 //user cant change it even from command line

#define CVAR_INFOMASK		(CVAR_USERINFO|CVAR_SERVERINFO)
#define CVAR_EXTENDED_MASK	(~31)

/* bits 12 - 14, enum */
#define	CVAR_SYSTEM_GENERIC		0x00000000
#define	CVAR_SYSTEM_GAME		0x00001000
#define	CVAR_SYSTEM_VIDEO		0x00002000
#define	CVAR_SYSTEM_SOUND		0x00003000
#define	CVAR_SYSTEM_INPUT		0x00004000
#define	CVAR_SYSTEM_NET			0x00005000
#define	CVAR_SYSTEM_FILES		0x00006000
#define CVAR_SYSTEM_RESERVED	0x00007000
#define	CVAR_SYSTEM_MASK		0x00007000

extern	cvar_t	*cvar_vars;

cvar_t *Cvar_Get (const char *var_name, const char *value, int flags);
// creates the variable if it doesn't exist, or returns the existing one
// if it exists, the value will not be changed, but flags will be ORed in
// that allows variables to be unarchived without needing bitflags

cvar_t 	*Cvar_Set (const char *var_name, const char *value);
// will create the variable if it doesn't exist

cvar_t *Cvar_SetLatched (const char *var_name, const char *value);
// will set the variable even if NOSET or LATCH

cvar_t 	*Cvar_FullSet (const char *var_name, const char *value, int flags);

void	Cvar_SetValue (const char *var_name, float value);
// expands value to a string and calls Cvar_Set

float	Cvar_VariableValue (const char *var_name);
int		Cvar_VariableIntValue (const char *var_name);
// returns 0 if not defined or non numeric

char	*Cvar_VariableString (const char *var_name);
// returns an empty string if not defined

void Cvar_CommandCompletion ( const char *partial, void(*callback)(const char *name, const char *value) );
//char 	*Cvar_CompleteVariable (const char *partial);
// attempts to match a partial variable name for command line completion
// returns NULL if nothing fits
void	Cvar_SetCheatState( void );

void	Cvar_GetLatchedVars (int flags);
// any CVAR_LATCHED variables that have been set will now take effect

qboolean Cvar_Command (const char *name, unsigned int hash);
// called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
// command.  Returns true if the command was a variable reference that
// was handled. (print or change)

void 	Cvar_WriteVariables (FILE *f);
// appends lines containing "set variable value" for all variables
// with the archive flag set to true.

void	Cvar_Init (void);

char	*Cvar_Userinfo (void);
// returns an info string containing all the CVAR_USERINFO cvars

char	*Cvar_Serverinfo (void);
// returns an info string containing all the CVAR_SERVERINFO cvars

extern	qboolean	userinfo_modified;
// this is set each time a CVAR_USERINFO variable is changed
// so that the client knows to send it to the server

void Cvar_Subsystem( int subsystem );

/*
==============================================================

NET

==============================================================
*/

// net.h -- quake's interface to the networking layer

#define	PORT_ANY	-1

//#define	MAX_MSGLEN		1400		// max length of a message
#define	MAX_MSGLEN		4096
#define	PACKET_HEADER	10			// two ints and a short
#define	MAX_USABLEMSG	(MAX_MSGLEN - PACKET_HEADER)

//typedef enum {NA_LOOPBACK, NA_BROADCAST, NA_IP, NA_IPX, NA_BROADCAST_IPX} netadrtype_t;
typedef enum {NA_LOOPBACK, NA_BROADCAST, NA_IP} netadrtype_t;

typedef enum {NS_CLIENT, NS_SERVER, NS_COUNT} netsrc_t;

typedef struct netadr_s
{
	netadrtype_t	type;
	byte			ip[4];
	uint16			port;
} netadr_t;

void		NET_Init (void);
void		NET_Shutdown (void);

void		NET_Config (int flag);

int			NET_GetPacket (netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message);
int			NET_SendPacket (netsrc_t sock, int length, const void *data, const netadr_t *to);

qboolean	NET_CompareAdr (const netadr_t *a, const netadr_t *b);
qboolean	NET_CompareBaseAdr (const netadr_t *a, const netadr_t *b);

char		*NET_AdrToString (const netadr_t *a);
qboolean	NET_StringToAdr (const char *s, netadr_t *a);
void		NET_Sleep (int msec);

qboolean	NET_IsLANAddress (const netadr_t *adr);
#define		NET_IsLocalAddress( adr )	( (adr)->type == NA_LOOPBACK || ((adr)->ip[0] == 127 && (adr)->ip[1] == 0 && (adr)->ip[2] == 0 && (adr)->ip[3] == 1))

//============================================================================

#define	OLD_AVG		0.99		// total = oldtotal*OLD_AVG + new*(1-OLD_AVG)

#define	MAX_LATENT	32

typedef struct
{
	//qboolean	fatal_error;
	qboolean	got_reliable;

	netsrc_t	sock;

	int			dropped;			// between last packet and previous

	unsigned int last_received;		// for timeouts
	unsigned int last_sent;			// for retransmits

	netadr_t	remote_address;
	uint16		qport;				// qport value to write when transmitting
	int			protocol;

// sequencing variables
	int			incoming_sequence;
	int			incoming_acknowledged;
	int			incoming_reliable_acknowledged;	// single bit

	int			incoming_reliable_sequence;		// single bit, maintained local

	int			outgoing_sequence;
	int			reliable_sequence;			// single bit
	int			last_reliable_sequence;		// sequence number of last send

// reliable staging and holding areas
	sizebuf_t	message;		// writing buffer to send to server
	byte		message_buf[MAX_USABLEMSG];		// leave space for header

// message is copied to this buffer when it is first transfered
	int			reliable_length;
	byte		reliable_buf[MAX_USABLEMSG];	// unacked reliable message
} netchan_t;

extern	netadr_t	net_from;
extern	sizebuf_t	net_message;
extern	byte		net_message_buffer[MAX_MSGLEN];
extern	cvar_t		*net_maxmsglen;

void Netchan_Init (void);
void Netchan_Setup (netsrc_t sock, netchan_t *chan, netadr_t *adr, int protocol, int qport);

//qboolean Netchan_NeedReliable (netchan_t *chan);
int Netchan_Transmit (netchan_t *chan, int length, const byte *data);
void Netchan_OutOfBand (int net_socket, const netadr_t *adr, int length, const byte *data);
void Netchan_OutOfBandPrint (int net_socket, const netadr_t *adr, const char *format, ...);
qboolean Netchan_Process (netchan_t *chan, sizebuf_t *msg);

//qboolean Netchan_CanReliable (netchan_t *chan);


/*
==============================================================

CMODEL

==============================================================
*/


#include "../qcommon/qfiles.h"

cmodel_t	*CM_LoadMap (const char *name, qboolean clientload, uint32 *checksum);
cmodel_t	*CM_InlineModel (const char *name);	// *1, *2, etc

int			CM_NumClusters (void);
int			CM_NumInlineModels (void);
char		*CM_EntityString (void);

// creates a clipping hull for an arbitrary box
int			CM_HeadnodeForBox (const vec3_t mins, const vec3_t maxs);


// returns an ORed contents mask
int			CM_PointContents (const vec3_t p, int headnode);
int			CM_TransformedPointContents (const vec3_t p, int headnode, const vec3_t origin, const vec3_t angles);

trace_t		CM_BoxTrace (const vec3_t start, const vec3_t end,
						  const vec3_t mins, const vec3_t maxs,
						  int headnode, int brushmask);
trace_t		CM_TransformedBoxTrace (const vec3_t start, const vec3_t end,
						  const vec3_t mins, const vec3_t maxs,
						  int headnode, int brushmask,
						  const vec3_t origin, const vec3_t angles);

byte		*CM_ClusterPVS (int cluster);
byte		*CM_ClusterPHS (int cluster);

int			CM_PointLeafnum (const vec3_t p);

// call with topnode set to the headnode, returns with topnode
// set to the first node that splits the box
int			CM_BoxLeafnums (vec3_t mins, vec3_t maxs, int *list,
							int listsize, int *topnode);

int			CM_LeafContents (int leafnum);
int			CM_LeafCluster (int leafnum);
int			CM_LeafArea (int leafnum);

void		CM_SetAreaPortalState (int portalnum, qboolean open);
qboolean	CM_AreasConnected (int area1, int area2);

int			CM_WriteAreaBits (byte *buffer, int area);
qboolean	CM_HeadnodeVisible (int headnode, const byte *visbits);

void		CM_WritePortalState (fileHandle_t f);
void		CM_ReadPortalState (fileHandle_t f);

/*
==============================================================

PLAYER MOVEMENT CODE

Common between server and client so prediction matches

==============================================================
*/

typedef struct {
	qboolean	airaccelerate;
	qboolean	strafeHack;
	float		speedMultiplier;
} pmoveParams_t;

void Pmove ( pmove_t *pmove, pmoveParams_t *params );

/*
==============================================================

FILESYSTEM

==============================================================
*/
#define MAX_LISTED_FILES	4096

/* bits 0 - 1, enum */
#define		FS_MODE_APPEND			0x00000000
#define		FS_MODE_READ			0x00000001
#define		FS_MODE_WRITE			0x00000002
#define		FS_MODE_RDWR			0x00000003
#define		FS_MODE_MASK			0x00000003

/* bits 0 - 1, enum */
#define		FS_SEARCHDIRS_NO			0x00000000
#define		FS_SEARCHDIRS_YES			0x00000001
#define		FS_SEARCHDIRS_ONLY			0x00000002
#define		FS_SEARCHDIRS_RESERVED		0x00000003
#define		FS_SEARCHDIRS_MASK			0x00000003

/* bit 2, enum */
#define FS_FLUSH_NONE			0x00000000
#define FS_FLUSH_SYNC			0x00000004
#define	FS_FLUSH_MASK			0x00000004

/* bits 3 - 4, enum */
#define	FS_TYPE_ANY			0x00000000
#define	FS_TYPE_REAL		0x00000008
#define	FS_TYPE_PAK			0x00000010
#define	FS_TYPE_RESERVED	0x00000018
#define	FS_TYPE_MASK		0x00000018

/* bits 5 - 6, enum */
#define	FS_PATH_ANY			0x00000000
#define	FS_PATH_INIT		0x00000020
#define	FS_PATH_BASE		0x00000040
#define	FS_PATH_GAME		0x00000060
#define	FS_PATH_MASK		0x00000060

/* bits 7 - 10, flag */
#define	FS_SEARCH_BYFILTER		0x00000080
#define	FS_SEARCH_SAVEPATH		0x00000100
#define	FS_SEARCH_EXTRAINFO		0x00000200
#define	FS_SEARCH_RESERVED		0x00000400

/* bits 7 - 8, flag */
#define	FS_FLAG_RAW				0x00000080
#define	FS_FLAG_CACHE			0x00000100

void	FS_InitFilesystem (void);
char	*FS_Gamedir (void);
char	*FS_NextPath (const char *prevpath);
//void	FS_ExecAutoexec (void);
void	FS_ExecConfig (const char *filename);
qboolean FS_ExistsInGameDir (const char *filename);

int		FS_FOpenFile (const char *filename, fileHandle_t *f, uint32 mode);
void	FS_FCloseFile (fileHandle_t f);
// note: this can't be called from another DLL, due to MS libc issues

int		FS_LoadFileEx (const char *path, void **buffer, uint32 flags);
int		FS_LoadFile (const char *path, void **buffer);
void	FS_FreeFile (void *buffer);
// a null buffer will just return the file length without loading
// a -1 length is not present

int		FS_Read (void *buffer, int len, fileHandle_t hFile);
int		FS_Write (const void *buffer, int len, fileHandle_t hFile);
// properly handles partial reads

void	FS_FPrintf( fileHandle_t f, const char *format, ... );

qboolean FS_CopyFile (const char *src, const char *dst);
qboolean FS_RemoveFile (const char *filename);
qboolean FS_RenameFile (const char *src, const char *dst);

void	FS_CreatePath ( const char *path );

char **FS_ListFiles( const char *findname, int *numfiles, unsigned musthave, unsigned canthave );

qboolean FS_NeedRestart ( void );
void FS_Restart( void );
char **FS_FindMaps( void );

/*
==============================================================

MISC

==============================================================
*/


#define	ERR_FATAL	0		// exit the entire game with a popup window
#define	ERR_DROP	1		// print to console and disconnect from game
#define	ERR_QUIT	2		// not an error, just a normal exit

#define	EXEC_NOW	0		// don't return until completed
#define	EXEC_INSERT	1		// insert at current position, but don't run yet
#define	EXEC_APPEND	2		// add to end of the command buffer

#define	PRINT_ALL		0
#define PRINT_DEVELOPER	1	// only print when "developer 1"

void		Com_BeginRedirect (int target, char *buffer, int buffersize, void (*flush));
void		Com_EndRedirect (void);
void 		Com_Printf (const char *fmt, ...);
void 		_Com_DPrintf (const char *fmt, ...);
void 		Com_Error (int code, const char *fmt, ...);
void 		Com_Quit (void);

//r1: use variadic macros where possible to avoid overhead of evaluations and va
/*#if  (__STDC_VERSION__ >= 199901L || _MSC_VER >= 1400 || __GNUC__ >= 3 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95))
// #define Com_DPrintf(...) do{ if(developer->integer) Com_Printf(__VA_ARGS__); }while(0)
 #define Com_DPrintf(...) if (developer->integer) { Com_Printf(__VA_ARGS__); }
#endif*/
// #define Com_DPrintf if(developer->integer) Com_Printf
#define Com_DPrintf (!developer->integer) ? (void)0 : Com_Printf

int			Com_ServerState (void);		// this should have just been a cvar...
void		Com_SetServerState (int state);

uint32		Com_BlockChecksum (void *buffer, int length);
byte		COM_BlockSequenceCRCByte (byte *base, int length, int sequence);

int SortStrcmp( const void *p1, const void *p2 );

extern	cvar_t	nullCvar;
extern	cvar_t	*developer;
extern	cvar_t	*dedicated;
extern	cvar_t	*host_speeds;

// host_speeds times
extern unsigned int time_before_game;
extern unsigned int time_after_game;
extern unsigned int time_before_ref;
extern unsigned int time_after_ref;

typedef enum memtag_e
{
	TAG_NOT_TAGGED,
	TAG_STATIC,
	TAG_TEMP,
	TAG_CMD,
	TAG_ALIAS,
	TAG_TRIGGER,
	TAG_MACRO,
	TAG_CVAR,
	TAG_FS_LOADFILE,
	TAG_FS_LOADPAK,
	TAG_FS_FILELIST,
	TAG_FS_SEARCHPATH,
	TAG_CLIENTS,
	TAG_CL_ENTS,

	TAG_CL_KEYBIND,
	TAG_CL_SFX,
	TAG_CL_SOUNDCACHE,
	TAG_CL_LOADPCX,
	TAG_CL_CINEMA,
	TAG_CL_LOC,
	TAG_CL_IGNORE,
	TAG_CL_DRAWSTRING,
	TAG_CL_DOWNLOAD,

	TAG_X86,
	TAG_CLIPBOARD,
	TAG_MENU,
	TAG_MP3LIST,
	TAG_AVIEXPORT,

	TAG_RENDER_MODEL,
	TAG_RENDER_IMAGE,
	TAG_RENDER_IMGRESAMPLE,
	TAG_RENDER_SCRSHOT,

	TAG_DLL_GAME,
	TAG_DLL_LEVEL,
	TAG_MAX_TAGS
} memtag_t;

#ifndef NDEBUG
void _Z_Free (void *ptr, const char *filename, int fileline);
void *_Z_TagMalloc (int size, int tag, const char *filename, int fileline);
void _Z_FreeTags (int tag, const char *filename, int fileline);
char *_CopyString (const char *in, int tag, const char *filename, int fileline);

#define Z_Free(ptr) _Z_Free(ptr,__FILE__,__LINE__)
#define Z_TagMalloc(size,tag) _Z_TagMalloc(size,tag,__FILE__,__LINE__)
#define Z_FreeTags(tag) _Z_FreeTags(tag,__FILE__,__LINE__)
#define CopyString(in,tag) _CopyString(in,tag,__FILE__,__LINE__)
#else
//void *Z_Malloc (int size);			// returns 0 filled memory
void Z_Free (void *ptr);
void *Z_TagMalloc (int size, int tag);
void Z_FreeTags (int tag);
char *CopyString (const char *in, int tag);
#endif

void Z_FreeGame (void *ptr);
void *Z_TagMallocGame (int size, int tag);
void Z_FreeTagsGame (int tag);

void Qcommon_Init (int argc, char **argv);
void Qcommon_Frame (int msec);
void Qcommon_Shutdown (void);

// this is in the client code, but can be used for debugging from server
void SCR_DebugGraph (float value, int color);


/*
==============================================================

NON-PORTABLE SYSTEM SERVICES

==============================================================
*/

void	Sys_Init (void);

void	Sys_AppActivate (void);

void	Sys_UnloadGame (void);
void	*Sys_GetGameAPI (void *parms);
// loads the game dll and calls the api init function

char	*Sys_ConsoleInput (void);
void	Sys_ConsoleOutput (const char *string);
void	Sys_SendKeyEvents (void);
void	Sys_Error (const char *error, ...);
void	Sys_Quit (void);
char	*Sys_GetClipboardData( void );
void	Sys_CopyProtect (void);

/*
==============================================================

CLIENT / SERVER SYSTEMS

==============================================================
*/

void CL_Init (void);
void CL_Drop (void);
void CL_Shutdown (void);
void CL_Frame (int msec);
void Con_Print (const char *text);
void SCR_BeginLoadingPlaque (void);

void SV_Init (void);
void SV_Shutdown (char *finalmsg, qboolean reconnect);
void SV_Frame (int msec);

char *CL_Mapname (void);

#define Q_COLOR_ESCAPE	'^'
#define Q_IsColorString(p)	( p && *(p) == Q_COLOR_ESCAPE && *((p)+1) >= '0' && *((p)+1) <= '7' )

#define COLOR_ENABLE	'\x03'
#define COLOR_ONE		'\x04'
#define Q_IsColorEnabled(p)  ( p && *(p) == COLOR_ENABLE)
#define Q_IsOneColorString(p)( p && *(p) == COLOR_ONE)
#define S_ENABLE_COLOR	"\x03"
#define S_ONE_COLOR		"\x04"

#define COLOR_BLACK		0
#define COLOR_RED		1
#define COLOR_GREEN		2
#define COLOR_YELLOW	3
#define COLOR_BLUE		4
#define COLOR_CYAN		5
#define COLOR_MAGENTA	6
#define COLOR_WHITE		7
#define ColorIndex(c)	( ( (c) - '0' ) & 7 )

#define S_COLOR_BLACK	"^0"
#define S_COLOR_RED		"^1"
#define S_COLOR_GREEN	"^2"
#define S_COLOR_YELLOW	"^3"
#define S_COLOR_BLUE	"^4"
#define S_COLOR_CYAN	"^5"
#define S_COLOR_MAGENTA	"^6"
#define S_COLOR_WHITE	"^7"


