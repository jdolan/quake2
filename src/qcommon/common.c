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
// common.c -- misc functions used in client and server
#include "qcommon.h"
#include <setjmp.h>

#define MAX_NUM_ARGVS	50


static int	com_argc;
static char	*com_argv[MAX_NUM_ARGVS+1];

int		realtime;

static jmp_buf abortframe;		// an ERR_DROP occured, exit the entire frame


cvar_t	nullCvar;

cvar_t	*host_speeds;
cvar_t	*developer = &nullCvar;
cvar_t	*timescale;
cvar_t	*fixedtime;
cvar_t	*logfile_active = &nullCvar;	// 1 = buffer log, 2 = flush after each print
cvar_t	*showtrace;
cvar_t	*dedicated = &nullCvar;

static FILE	*logfile;

static int			server_state;

// host_speeds times
unsigned int time_before_game;
unsigned int time_after_game;
unsigned int time_before_ref;
unsigned int time_after_ref;

/*
============================================================================

CLIENT / SERVER interactions

============================================================================
*/

static int	rd_target;
static char	*rd_buffer;
static int	rd_buffersize;
static void	(*rd_flush)(int target, char *buffer);

void Com_BeginRedirect (int target, char *buffer, int buffersize, void (*flush))
{
	if (!target || !buffer || !buffersize || !flush)
		return;
	rd_target = target;
	rd_buffer = buffer;
	rd_buffersize = buffersize;
	rd_flush = flush;

	*rd_buffer = 0;
}

void Com_EndRedirect (void)
{
	if(rd_flush && rd_buffer)
		rd_flush(rd_target, rd_buffer);

	rd_target = 0;
	rd_buffer = NULL;
	rd_buffersize = 0;
	rd_flush = NULL;
}

/*
=============
Com_Printf

Both client and server can use this, and it will output
to the apropriate place.
=============
*/
void Com_Printf (const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG], *s, *text;

	va_start (argptr,fmt);
	vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
	va_end (argptr);

	if (rd_target)
	{
		if ((strlen (msg) + strlen(rd_buffer)) > (rd_buffersize - 1)) {
			rd_flush(rd_target, rd_buffer);
			*rd_buffer = 0;
		}
		strcat (rd_buffer, msg);
		return;
	}

	Con_Print (msg);

	text = msg;
	//Remove color triggers
	switch (msg[0]) {
	case 1:
	case 2:
		text++;
	default:
		for (s = text; *s; s++)
			*s &= 127;
		break;
	case COLOR_ONE:
		text++;
		if (Q_IsColorString(text))
			text += 2;

		for (s = text; *s; s++)
			*s &= 127;
		break;
	case COLOR_ENABLE:
		text++;
		for (s = text; *s;) {
			if(Q_IsColorString(s)) {
				memmove( s, s + 2, MAXPRINTMSG - 2 - (s - msg));
				continue;
			}
			*s++ &= 127;
		}
		break;
	}

	// also echo to debugging console
	Sys_ConsoleOutput (text);

	// logfile
	if (logfile_active->integer) {
		if (!logfile) {
			logfile = fopen(va("%s/qconsole.log", FS_Gamedir()), (logfile_active->integer > 2) ? "a" : "w");
		}
		if (logfile) {
			fprintf(logfile, "%s", text);
			if (logfile_active->integer > 1)
				fflush(logfile);		// force it to save every time
		}
	}
}


/*
================
Com_DPrintf

A Com_Printf that only shows up if the "developer" cvar is set
================
*/
void _Com_DPrintf (const char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
		
	if (!developer->integer)
		return;			// don't confuse non-developers with techie stuff...

	va_start (argptr,fmt);
	vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
	va_end (argptr);
	
	Com_Printf ("%s", msg);
}


/*
=============
Com_Error

Both client and server can use this, and it will
do the apropriate things.
=============
*/
void Com_Error (int code, const char *fmt, ...)
{
	va_list		argptr;
	static char		msg[MAXPRINTMSG];
	static	qboolean	recursive = false;

	if (recursive)
		Sys_Error ("recursive error after: %s", msg);
	recursive = true;

	va_start (argptr,fmt);
	vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
	va_end (argptr);
	
	if (code == ERR_DISCONNECT)
	{
		CL_Drop ();
		recursive = false;
		longjmp (abortframe, -1);
	}
	else if (code == ERR_DROP)
	{
		Com_Printf ("********************\nERROR: %s\n********************\n", msg);
		SV_Shutdown (va("Server crashed: %s\n", msg), false);
		CL_Drop ();
		recursive = false;
		longjmp (abortframe, -1);
	}
	else
	{
		SV_Shutdown (va("Server fatal crashed: %s\n", msg), false);
		CL_Shutdown ();
	}

	if (logfile)
	{
		fprintf (logfile, "Fatal Error\n*****************************\n"
						  "%s\n*****************************\n", msg);
		fclose (logfile);
		logfile = NULL;
	}

	Sys_Error ("%s", msg);
}


/*
=============
Com_Quit

Both client and server can use this, and it will
do the apropriate things.
=============
*/
void Com_Quit (void)
{
	SV_Shutdown ("Server quit\n", false);
	CL_Shutdown ();

	if (logfile) {
		fclose (logfile);
		logfile = NULL;
	}

	Sys_Quit ();
}


/*
==================
Com_ServerState
==================
*/
int Com_ServerState (void)
{
	return server_state;
}

/*
==================
Com_SetServerState
==================
*/
void Com_SetServerState (int state)
{
	server_state = state;
}

/*
================
COM_CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
int COM_CheckParm (const char *parm)
{
	int		i;
	
	for (i = 1 ; i < com_argc ; i++) {
		if (!strcmp(parm, com_argv[i]))
			return i;
	}
		
	return 0;
}

int COM_Argc (void)
{
	return com_argc;
}

char *COM_Argv (int arg)
{
	if ((unsigned)arg >= com_argc || !com_argv[arg])
		return "";
	return com_argv[arg];
}

void COM_ClearArgv (int arg)
{
	if ((unsigned)arg >= com_argc || !com_argv[arg])
		return;
	com_argv[arg] = "";
}


/*
================
COM_InitArgv
================
*/
void COM_InitArgv (int argc, char **argv)
{
	int		i;

	if (argc > MAX_NUM_ARGVS)
		Com_Error (ERR_FATAL, "argc > MAX_NUM_ARGVS");
	com_argc = argc;
	for (i=0 ; i<argc ; i++)
	{
		if (!argv[i] || strlen(argv[i]) >= MAX_TOKEN_CHARS )
			com_argv[i] = "";
		else
			com_argv[i] = argv[i];
	}
}

/*
================
COM_AddParm

Adds the given string at the end of the current argument list
================
*/
void COM_AddParm (char *parm)
{
	if (com_argc == MAX_NUM_ARGVS)
		Com_Error (ERR_FATAL, "COM_AddParm: MAX_NUM_ARGVS");
	com_argv[com_argc++] = parm;
}



#if 0
/// just for debugging
int	memsearch (byte *start, int count, int search)
{
	int		i;
	
	for (i=0 ; i<count ; i++)
		if (start[i] == search)
			return i;
	return -1;
}
#endif

void Info_Print (const char *s)
{
	char	key[512];
	char	value[512];
	char	*o;
	int		l;

	if (*s == '\\')
		s++;
	while (*s)
	{
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		if (l < 20)
		{
			memset (o, ' ', 20-l);
			key[20] = 0;
		}
		else
			*o = 0;
		Com_Printf ("%s", key);

		if (!*s)
		{
			Com_Printf ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;
		Com_Printf ("%s\n", value);
	}
}

int SortStrcmp( const void *p1, const void *p2 ) {
	const char *s1 = *(const char **)p1;
	const char *s2 = *(const char **)p2;

	return strcmp( s1, s2 );
}

/*
==============================================================================

						ZONE MEMORY ALLOCATION

just cleared malloc with counters now...

==============================================================================
*/

#define	Z_MAGIC		0x1d1d
#define	Z_TAIL		0x5b7b

typedef struct zhead_s {
	int16	magic;
	int16	tag;			// for group free
	uint32	size;
	struct zhead_s	*prev, *next;
} zhead_t;

static zhead_t	z_chain = {0};

#define	TAG_GAME	765	// clear when unloading the dll
#define	TAG_LEVEL	766	// clear when loading a new level

#pragma pack(push,1)
typedef struct zstatic_s {
	zhead_t	z;
	char	data[2]; /* !!make sure 'tail' field is aligned properly */
	uint16	tail;
} zstatic_t;
#pragma pack(pop)

static zstatic_t		z_static[] = {
	{ { Z_MAGIC, TAG_STATIC, sizeof( zstatic_t ) }, { '0', '\0' }, Z_TAIL },
	{ { Z_MAGIC, TAG_STATIC, sizeof( zstatic_t ) }, { '1', '\0' }, Z_TAIL },
	{ { Z_MAGIC, TAG_STATIC, sizeof( zstatic_t ) }, { '2', '\0' }, Z_TAIL },
	{ { Z_MAGIC, TAG_STATIC, sizeof( zstatic_t ) }, { '3', '\0' }, Z_TAIL },
	{ { Z_MAGIC, TAG_STATIC, sizeof( zstatic_t ) }, { '4', '\0' }, Z_TAIL },
	{ { Z_MAGIC, TAG_STATIC, sizeof( zstatic_t ) }, { '5', '\0' }, Z_TAIL },
	{ { Z_MAGIC, TAG_STATIC, sizeof( zstatic_t ) }, { '6', '\0' }, Z_TAIL },
	{ { Z_MAGIC, TAG_STATIC, sizeof( zstatic_t ) }, { '7', '\0' }, Z_TAIL },
	{ { Z_MAGIC, TAG_STATIC, sizeof( zstatic_t ) }, { '8', '\0' }, Z_TAIL },
	{ { Z_MAGIC, TAG_STATIC, sizeof( zstatic_t ) }, { '9', '\0' }, Z_TAIL },
	{ { Z_MAGIC, TAG_STATIC, sizeof( zstatic_t ) }, { '\0', '\0'}, Z_TAIL }
};

typedef struct zstats_s {
	const char	*name;
	uint32		count, bytes;
} zstats_t;

static zstats_t z_stats[TAG_MAX_TAGS] =
{
	{"Not tagged", 0, 0},
	{"Static", 0, 0},
	{"Temp", 0, 0},
	{"Commands", 0, 0},
	{"Aliases", 0, 0},
	{"Triggers", 0, 0},
	{"Macros", 0, 0},
	{"Cvars", 0, 0},
	{"FS_Files", 0, 0},
	{"FS_Paks", 0, 0},
	{"FS_Filelist", 0, 0},
	{"FS_Searchpath", 0, 0},
	{"CLIENTS", 0, 0},
	{"CL_ENTS", 0, 0},

	{"Keybindings", 0, 0},
	{"CL_SFX", 0, 0},
	{"CL_Soundcache", 0, 0},
	{"CL_LoadPCX", 0, 0},
	{"CL_CINEMA", 0, 0},
	{"CL_Loc", 0, 0},
	{"CL_Ignore", 0, 0},
	{"CL_Drawstring", 0, 0},
	{"CL_Download", 0, 0},

	{"X86CACHE", 0, 0},
	{"SYS_Clipboard", 0, 0},
	{"MENU", 0, 0},
	{"MP3_List", 0, 0},
	{"AVI_Framebuffer", 0, 0},

	{"R_Models", 0, 0},
	{"R_Images", 0, 0},
	{"R_Resample", 0, 0},
	{"R_Screenshot", 0, 0},

	{"DLL_GAME", 0, 0},
	{"DLL_LEVEL", 0, 0}
};

void Z_Check( void )
{
	zhead_t	*z;

	for( z = z_chain.next; z != &z_chain; z = z->next ) {
		if( z->magic != Z_MAGIC ) {
			Com_Error( ERR_FATAL, "Z_Check: bad magic" );
        }

		if( *( uint16 * )( ( byte * )z + z->size - sizeof( uint16 ) ) != Z_TAIL ) {
			Com_Error( ERR_FATAL, "Z_Check: bad tail with tag %i", z->tag );
        }

		if( z->tag == TAG_NOT_TAGGED ) {
			Com_Error( ERR_FATAL, "Z_Check: bad tag" );
        }
	}
	Com_Printf("Z_Check: all ok\n");
}

/*
========================
Z_Free
========================
*/
#ifndef NDEBUG
void _Z_Free (void *ptr, const char *filename, int fileline)
#else
void Z_Free (void *ptr)
#endif
{
	zhead_t	*z;
	zstats_t *s;

#ifndef NDEBUG
	if (!ptr)
		Com_Error (ERR_FATAL, "Z_Free: null pointer (%s:%i)", filename, fileline);
#endif

	z = ((zhead_t *)ptr) - 1;

	if (z->magic != Z_MAGIC) {
#ifndef NDEBUG
		Com_Error (ERR_FATAL, "Z_Free: bad magic (%s:%i)", filename, fileline);
#else
		Com_Error (ERR_FATAL, "Z_Free: bad magic");
#endif
	}

	s = &z_stats[(unsigned)z->tag < TAG_MAX_TAGS ? z->tag : TAG_NOT_TAGGED];
#ifndef NDEBUG
	if (s->count == 0 || s->bytes < z->size)
		Com_Error(ERR_FATAL, "Z_Free: counters are screwed after free of %d bytes at %p tagged %d:%s (%s:%i)", z->size, z, z->tag, s->name, filename, fileline);
#endif
	s->count--;
	s->bytes -= z->size;

	if( *( uint16 * )( ( byte * )z + z->size - sizeof( uint16 ) ) != Z_TAIL ) {
		Com_Printf("Z_Free: bad tail, tagged %d:%s\n", z->tag, s->name);
    }

	if (z->tag != TAG_STATIC) {
		z->prev->next = z->next;
		z->next->prev = z->prev;

		free( z );
	}
}

/*
========================
Z_Stats_f
========================
*/

void Z_Stats_f (void)
{
	int i, totalBytes = 0, totalCount = 0;
	zstats_t *s;

	for (i = 0, s = z_stats; i < TAG_MAX_TAGS; i++, s++) {
		if(!s->count)
			continue;

		totalBytes += s->bytes;
		totalCount += s->count;
		Com_Printf ("%14.14s: %8i bytes %5i blocks\n", s->name, s->bytes, s->count);
	}

	Com_Printf (" RUNNING_TOTAL: %8i bytes %5i blocks\n", totalBytes, totalCount);
}

/*
========================
Z_FreeTags
========================
*/
#ifndef NDEBUG
void _Z_FreeTags (int tag, const char *filename, int fileline)
#else
void Z_FreeTags (int tag)
#endif
{
	zhead_t	*z, *next;

	for (z=z_chain.next ; z && z != &z_chain ; z=next)
	{
		next = z->next;
		if (z->tag == tag)
#ifndef NDEBUG
			_Z_Free((void *)(z+1), filename, fileline);
#else
			Z_Free((void *)(z+1));
#endif
	}
}

/*
========================
Z_TagMalloc
========================
*/
#ifndef NDEBUG
void *_Z_TagMalloc (int size, int tag, const char *filename, int fileline)
#else
void *Z_TagMalloc (int size, int tag)
#endif
{
	zhead_t	*z;
	zstats_t *s;

	s = &z_stats[(unsigned)tag < TAG_MAX_TAGS ? tag : TAG_NOT_TAGGED];

#ifndef NDEBUG
	if (size <= 0)
		Com_Error (ERR_FATAL, "Z_TagMalloc: tried to allocate %i bytes, tagged %i%s (%s:%i)!", size, tag, s->name, filename, fileline);
#endif

	size += sizeof(zhead_t) + sizeof( uint16 );
	z = malloc(size);
	if (!z) {
#ifndef NDEBUG
		Com_Error (ERR_FATAL, "Z_TagMalloc: failed on allocation of %i bytes, tagged %i%s (%s:%i)!", size, tag, s->name, filename, fileline);
#else
		Com_Error (ERR_FATAL, "Z_TagMalloc: failed on allocation of %i bytes, tagged %i%s", size, tag, s->name);
#endif
	}

	//memset (z, 0, size);
	s->count++;
	s->bytes += size;

	z->magic = Z_MAGIC;
	z->tag = tag;
	z->size = size;

	z->next = z_chain.next;
	z->prev = &z_chain;
	z_chain.next->prev = z;
	z_chain.next = z;

	*( uint16 * )( ( byte * )z + size - sizeof( uint16 ) ) = Z_TAIL;

	return (void *)(z+1);
}

#ifndef NDEBUG
char *_CopyString (const char *in, int tag, const char *filename, int fileline)
#else
char *CopyString (const char *in, int tag)
#endif
{
	char	*out;
	int		len;

#ifndef NDEBUG
	if (!in)
		Com_Error (ERR_FATAL, "CopyString: in == NULL, tagged %i:%s (%s:%i)!", tag, z_stats[(unsigned)tag < TAG_MAX_TAGS ? tag : TAG_NOT_TAGGED].name, filename, fileline);
#endif

	len = strlen(in);
	
#ifndef NDEBUG
	out = _Z_TagMalloc (len+1, tag, filename, fileline);
#else
	out = Z_TagMalloc (len+1, tag);
#endif
	strcpy (out, in);

	return out;
}

char *Cvar_CopyString( const char *in, int tag ) {
	char	*out;
	int		len;
    zstatic_t *z;

#ifndef NDEBUG
	if (!in)
		Com_Error (ERR_FATAL, "CopyString: in == NULL, tagged %i:%s!", tag, z_stats[(unsigned)tag < TAG_MAX_TAGS ? tag : TAG_NOT_TAGGED].name);
#endif

	if( !in[0] ) {
        z = &z_static[10];
        z_stats[TAG_STATIC].count++;
        z_stats[TAG_STATIC].bytes += z->z.size;
		return z->data;
	}

	if( !in[1] && Q_isdigit( in[0] ) ) {
        z = &z_static[ in[0] - '0' ];
        z_stats[TAG_STATIC].count++;
        z_stats[TAG_STATIC].bytes += z->z.size;
		return z->data;
	}

	len = strlen(in);
	
#ifndef NDEBUG
	out = _Z_TagMalloc (len+1, tag, "Cvar_CopyString", 1);
#else
	out = Z_TagMalloc (len+1, tag);
#endif

	strcpy( out, in );

	return out;
}

void *Z_TagMallocGame (int size, int tag)
{
	byte		*b;

	if (size <= 0) {
		Com_Printf ("Z_TagMallocGame: Game DLL tried to allocate %i bytes, tag %i!\n", size, tag);
		return NULL;
	}

	if (tag == TAG_GAME)
		tag = TAG_DLL_GAME;
	else if (tag == TAG_LEVEL)
		tag = TAG_DLL_LEVEL;
	else
		tag = tag + TAG_MAX_TAGS;

#ifndef NDEBUG
	b = _Z_TagMalloc (size, tag, "Game DLL", 0);
#else
	b = Z_TagMalloc (size, tag);
#endif
	memset (b, 0, size);

	return (void *)b;
}

void Z_FreeGame (void *ptr)
{
	if (!ptr)
		Com_Error (ERR_FATAL, "Z_FreeGame: null pointer from Game DLL");

#ifndef NDEBUG
	_Z_Free (ptr, "Game DLL", 1);
#else
	Z_Free (ptr);
#endif
}

void Z_FreeTagsGame (int tag)
{
	if (tag == TAG_GAME)
		tag = TAG_DLL_GAME;
	else if(tag == TAG_LEVEL)
		tag = TAG_DLL_LEVEL;
	else
		tag = tag + TAG_MAX_TAGS;


#ifndef NDEBUG
	_Z_FreeTags(tag, "Game DLL", 2);
#else
	Z_FreeTags(tag);
#endif
}

static void Z_FreeAll(void)
{
	zhead_t	*z, *next;

	for (z=z_chain.next ; z && z != &z_chain ; z=next)
	{
		next = z->next;
#ifndef NDEBUG
		_Z_Free((void *)(z+1), "FreeAll", 0);
#else
		Z_Free((void *)(z+1));
#endif
	}
}

/*
========================
Z_Malloc
========================
*/
/*void *Z_Malloc (int size)
{
	return Z_TagMalloc (size, 0);
}*/


//============================================================================


/*
====================
COM_BlockSequenceCheckByte

For proxy protecting

// THIS IS MASSIVELY BROKEN!  CHALLENGE MAY BE NEGATIVE
// DON'T USE THIS FUNCTION!!!!!

====================
*/
/*
byte	COM_BlockSequenceCheckByte (byte *base, int length, int sequence, int challenge)
{
	Sys_Error("COM_BlockSequenceCheckByte called\n");

#if 0
	int		checksum;
	byte	buf[68];
	byte	*p;
	float temp;
	byte c;

	temp = bytedirs[(sequence*0.33333333) % NUMVERTEXNORMALS][sequence % 3];
	temp = LittleFloat(temp);
	p = ((byte *)&temp);

	if (length > 60)
		length = 60;
	memcpy (buf, base, length);

	buf[length] = (sequence & 0xff) ^ p[0];
	buf[length+1] = p[1];
	buf[length+2] = ((sequence>>8) & 0xff) ^ p[2];
	buf[length+3] = p[3];

	temp = bytedirs[((sequence+challenge)*0.33333333) % NUMVERTEXNORMALS][(sequence+challenge) % 3];
	temp = LittleFloat(temp);
	p = ((byte *)&temp);

	buf[length+4] = (sequence & 0xff) ^ p[3];
	buf[length+5] = (challenge & 0xff) ^ p[2];
	buf[length+6] = ((sequence>>8) & 0xff) ^ p[1];
	buf[length+7] = ((challenge >> 7) & 0xff) ^ p[0];

	length += 8;

	checksum = LittleLong(Com_BlockChecksum (buf, length));

	checksum &= 0xff;

	return checksum;
#endif
	return 0;
}
*/
static const byte chktbl[1024] = {
0x84, 0x47, 0x51, 0xc1, 0x93, 0x22, 0x21, 0x24, 0x2f, 0x66, 0x60, 0x4d, 0xb0, 0x7c, 0xda,
0x88, 0x54, 0x15, 0x2b, 0xc6, 0x6c, 0x89, 0xc5, 0x9d, 0x48, 0xee, 0xe6, 0x8a, 0xb5, 0xf4,
0xcb, 0xfb, 0xf1, 0x0c, 0x2e, 0xa0, 0xd7, 0xc9, 0x1f, 0xd6, 0x06, 0x9a, 0x09, 0x41, 0x54,
0x67, 0x46, 0xc7, 0x74, 0xe3, 0xc8, 0xb6, 0x5d, 0xa6, 0x36, 0xc4, 0xab, 0x2c, 0x7e, 0x85,
0xa8, 0xa4, 0xa6, 0x4d, 0x96, 0x19, 0x19, 0x9a, 0xcc, 0xd8, 0xac, 0x39, 0x5e, 0x3c, 0xf2,
0xf5, 0x5a, 0x72, 0xe5, 0xa9, 0xd1, 0xb3, 0x23, 0x82, 0x6f, 0x29, 0xcb, 0xd1, 0xcc, 0x71,
0xfb, 0xea, 0x92, 0xeb, 0x1c, 0xca, 0x4c, 0x70, 0xfe, 0x4d, 0xc9, 0x67, 0x43, 0x47, 0x94,
0xb9, 0x47, 0xbc, 0x3f, 0x01, 0xab, 0x7b, 0xa6, 0xe2, 0x76, 0xef, 0x5a, 0x7a, 0x29, 0x0b,
0x51, 0x54, 0x67, 0xd8, 0x1c, 0x14, 0x3e, 0x29, 0xec, 0xe9, 0x2d, 0x48, 0x67, 0xff, 0xed,
0x54, 0x4f, 0x48, 0xc0, 0xaa, 0x61, 0xf7, 0x78, 0x12, 0x03, 0x7a, 0x9e, 0x8b, 0xcf, 0x83,
0x7b, 0xae, 0xca, 0x7b, 0xd9, 0xe9, 0x53, 0x2a, 0xeb, 0xd2, 0xd8, 0xcd, 0xa3, 0x10, 0x25,
0x78, 0x5a, 0xb5, 0x23, 0x06, 0x93, 0xb7, 0x84, 0xd2, 0xbd, 0x96, 0x75, 0xa5, 0x5e, 0xcf,
0x4e, 0xe9, 0x50, 0xa1, 0xe6, 0x9d, 0xb1, 0xe3, 0x85, 0x66, 0x28, 0x4e, 0x43, 0xdc, 0x6e,
0xbb, 0x33, 0x9e, 0xf3, 0x0d, 0x00, 0xc1, 0xcf, 0x67, 0x34, 0x06, 0x7c, 0x71, 0xe3, 0x63,
0xb7, 0xb7, 0xdf, 0x92, 0xc4, 0xc2, 0x25, 0x5c, 0xff, 0xc3, 0x6e, 0xfc, 0xaa, 0x1e, 0x2a,
0x48, 0x11, 0x1c, 0x36, 0x68, 0x78, 0x86, 0x79, 0x30, 0xc3, 0xd6, 0xde, 0xbc, 0x3a, 0x2a,
0x6d, 0x1e, 0x46, 0xdd, 0xe0, 0x80, 0x1e, 0x44, 0x3b, 0x6f, 0xaf, 0x31, 0xda, 0xa2, 0xbd,
0x77, 0x06, 0x56, 0xc0, 0xb7, 0x92, 0x4b, 0x37, 0xc0, 0xfc, 0xc2, 0xd5, 0xfb, 0xa8, 0xda,
0xf5, 0x57, 0xa8, 0x18, 0xc0, 0xdf, 0xe7, 0xaa, 0x2a, 0xe0, 0x7c, 0x6f, 0x77, 0xb1, 0x26,
0xba, 0xf9, 0x2e, 0x1d, 0x16, 0xcb, 0xb8, 0xa2, 0x44, 0xd5, 0x2f, 0x1a, 0x79, 0x74, 0x87,
0x4b, 0x00, 0xc9, 0x4a, 0x3a, 0x65, 0x8f, 0xe6, 0x5d, 0xe5, 0x0a, 0x77, 0xd8, 0x1a, 0x14,
0x41, 0x75, 0xb1, 0xe2, 0x50, 0x2c, 0x93, 0x38, 0x2b, 0x6d, 0xf3, 0xf6, 0xdb, 0x1f, 0xcd,
0xff, 0x14, 0x70, 0xe7, 0x16, 0xe8, 0x3d, 0xf0, 0xe3, 0xbc, 0x5e, 0xb6, 0x3f, 0xcc, 0x81,
0x24, 0x67, 0xf3, 0x97, 0x3b, 0xfe, 0x3a, 0x96, 0x85, 0xdf, 0xe4, 0x6e, 0x3c, 0x85, 0x05,
0x0e, 0xa3, 0x2b, 0x07, 0xc8, 0xbf, 0xe5, 0x13, 0x82, 0x62, 0x08, 0x61, 0x69, 0x4b, 0x47,
0x62, 0x73, 0x44, 0x64, 0x8e, 0xe2, 0x91, 0xa6, 0x9a, 0xb7, 0xe9, 0x04, 0xb6, 0x54, 0x0c,
0xc5, 0xa9, 0x47, 0xa6, 0xc9, 0x08, 0xfe, 0x4e, 0xa6, 0xcc, 0x8a, 0x5b, 0x90, 0x6f, 0x2b,
0x3f, 0xb6, 0x0a, 0x96, 0xc0, 0x78, 0x58, 0x3c, 0x76, 0x6d, 0x94, 0x1a, 0xe4, 0x4e, 0xb8,
0x38, 0xbb, 0xf5, 0xeb, 0x29, 0xd8, 0xb0, 0xf3, 0x15, 0x1e, 0x99, 0x96, 0x3c, 0x5d, 0x63,
0xd5, 0xb1, 0xad, 0x52, 0xb8, 0x55, 0x70, 0x75, 0x3e, 0x1a, 0xd5, 0xda, 0xf6, 0x7a, 0x48,
0x7d, 0x44, 0x41, 0xf9, 0x11, 0xce, 0xd7, 0xca, 0xa5, 0x3d, 0x7a, 0x79, 0x7e, 0x7d, 0x25,
0x1b, 0x77, 0xbc, 0xf7, 0xc7, 0x0f, 0x84, 0x95, 0x10, 0x92, 0x67, 0x15, 0x11, 0x5a, 0x5e,
0x41, 0x66, 0x0f, 0x38, 0x03, 0xb2, 0xf1, 0x5d, 0xf8, 0xab, 0xc0, 0x02, 0x76, 0x84, 0x28,
0xf4, 0x9d, 0x56, 0x46, 0x60, 0x20, 0xdb, 0x68, 0xa7, 0xbb, 0xee, 0xac, 0x15, 0x01, 0x2f,
0x20, 0x09, 0xdb, 0xc0, 0x16, 0xa1, 0x89, 0xf9, 0x94, 0x59, 0x00, 0xc1, 0x76, 0xbf, 0xc1,
0x4d, 0x5d, 0x2d, 0xa9, 0x85, 0x2c, 0xd6, 0xd3, 0x14, 0xcc, 0x02, 0xc3, 0xc2, 0xfa, 0x6b,
0xb7, 0xa6, 0xef, 0xdd, 0x12, 0x26, 0xa4, 0x63, 0xe3, 0x62, 0xbd, 0x56, 0x8a, 0x52, 0x2b,
0xb9, 0xdf, 0x09, 0xbc, 0x0e, 0x97, 0xa9, 0xb0, 0x82, 0x46, 0x08, 0xd5, 0x1a, 0x8e, 0x1b,
0xa7, 0x90, 0x98, 0xb9, 0xbb, 0x3c, 0x17, 0x9a, 0xf2, 0x82, 0xba, 0x64, 0x0a, 0x7f, 0xca,
0x5a, 0x8c, 0x7c, 0xd3, 0x79, 0x09, 0x5b, 0x26, 0xbb, 0xbd, 0x25, 0xdf, 0x3d, 0x6f, 0x9a,
0x8f, 0xee, 0x21, 0x66, 0xb0, 0x8d, 0x84, 0x4c, 0x91, 0x45, 0xd4, 0x77, 0x4f, 0xb3, 0x8c,
0xbc, 0xa8, 0x99, 0xaa, 0x19, 0x53, 0x7c, 0x02, 0x87, 0xbb, 0x0b, 0x7c, 0x1a, 0x2d, 0xdf,
0x48, 0x44, 0x06, 0xd6, 0x7d, 0x0c, 0x2d, 0x35, 0x76, 0xae, 0xc4, 0x5f, 0x71, 0x85, 0x97,
0xc4, 0x3d, 0xef, 0x52, 0xbe, 0x00, 0xe4, 0xcd, 0x49, 0xd1, 0xd1, 0x1c, 0x3c, 0xd0, 0x1c,
0x42, 0xaf, 0xd4, 0xbd, 0x58, 0x34, 0x07, 0x32, 0xee, 0xb9, 0xb5, 0xea, 0xff, 0xd7, 0x8c,
0x0d, 0x2e, 0x2f, 0xaf, 0x87, 0xbb, 0xe6, 0x52, 0x71, 0x22, 0xf5, 0x25, 0x17, 0xa1, 0x82,
0x04, 0xc2, 0x4a, 0xbd, 0x57, 0xc6, 0xab, 0xc8, 0x35, 0x0c, 0x3c, 0xd9, 0xc2, 0x43, 0xdb,
0x27, 0x92, 0xcf, 0xb8, 0x25, 0x60, 0xfa, 0x21, 0x3b, 0x04, 0x52, 0xc8, 0x96, 0xba, 0x74,
0xe3, 0x67, 0x3e, 0x8e, 0x8d, 0x61, 0x90, 0x92, 0x59, 0xb6, 0x1a, 0x1c, 0x5e, 0x21, 0xc1,
0x65, 0xe5, 0xa6, 0x34, 0x05, 0x6f, 0xc5, 0x60, 0xb1, 0x83, 0xc1, 0xd5, 0xd5, 0xed, 0xd9,
0xc7, 0x11, 0x7b, 0x49, 0x7a, 0xf9, 0xf9, 0x84, 0x47, 0x9b, 0xe2, 0xa5, 0x82, 0xe0, 0xc2,
0x88, 0xd0, 0xb2, 0x58, 0x88, 0x7f, 0x45, 0x09, 0x67, 0x74, 0x61, 0xbf, 0xe6, 0x40, 0xe2,
0x9d, 0xc2, 0x47, 0x05, 0x89, 0xed, 0xcb, 0xbb, 0xb7, 0x27, 0xe7, 0xdc, 0x7a, 0xfd, 0xbf,
0xa8, 0xd0, 0xaa, 0x10, 0x39, 0x3c, 0x20, 0xf0, 0xd3, 0x6e, 0xb1, 0x72, 0xf8, 0xe6, 0x0f,
0xef, 0x37, 0xe5, 0x09, 0x33, 0x5a, 0x83, 0x43, 0x80, 0x4f, 0x65, 0x2f, 0x7c, 0x8c, 0x6a,
0xa0, 0x82, 0x0c, 0xd4, 0xd4, 0xfa, 0x81, 0x60, 0x3d, 0xdf, 0x06, 0xf1, 0x5f, 0x08, 0x0d,
0x6d, 0x43, 0xf2, 0xe3, 0x11, 0x7d, 0x80, 0x32, 0xc5, 0xfb, 0xc5, 0xd9, 0x27, 0xec, 0xc6,
0x4e, 0x65, 0x27, 0x76, 0x87, 0xa6, 0xee, 0xee, 0xd7, 0x8b, 0xd1, 0xa0, 0x5c, 0xb0, 0x42,
0x13, 0x0e, 0x95, 0x4a, 0xf2, 0x06, 0xc6, 0x43, 0x33, 0xf4, 0xc7, 0xf8, 0xe7, 0x1f, 0xdd,
0xe4, 0x46, 0x4a, 0x70, 0x39, 0x6c, 0xd0, 0xed, 0xca, 0xbe, 0x60, 0x3b, 0xd1, 0x7b, 0x57,
0x48, 0xe5, 0x3a, 0x79, 0xc1, 0x69, 0x33, 0x53, 0x1b, 0x80, 0xb8, 0x91, 0x7d, 0xb4, 0xf6,
0x17, 0x1a, 0x1d, 0x5a, 0x32, 0xd6, 0xcc, 0x71, 0x29, 0x3f, 0x28, 0xbb, 0xf3, 0x5e, 0x71,
0xb8, 0x43, 0xaf, 0xf8, 0xb9, 0x64, 0xef, 0xc4, 0xa5, 0x6c, 0x08, 0x53, 0xc7, 0x00, 0x10,
0x39, 0x4f, 0xdd, 0xe4, 0xb6, 0x19, 0x27, 0xfb, 0xb8, 0xf5, 0x32, 0x73, 0xe5, 0xcb, 0x32,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00
};

/*
====================
COM_BlockSequenceCRCByte

For proxy protecting
====================
*/
byte	COM_BlockSequenceCRCByte (byte *base, int length, int sequence)
{
	int			n, x;
	const byte	*p;
	byte		chkb[60 + 4];
	uint16		crc;
	byte		r;

	if (sequence < 0)
		Sys_Error("sequence < 0, this shouldn't happen\n");

	p = chktbl + (sequence % (sizeof(chktbl) - 4));

	if (length > 60)
		length = 60;
	memcpy (chkb, base, length);

	chkb[length] = p[0];
	chkb[length+1] = p[1];
	chkb[length+2] = p[2];
	chkb[length+3] = p[3];

	length += 4;

	crc = CRC_Block(chkb, length);

	for (x=0, n=0; n<length; n++)
		x += chkb[n];

	r = (crc ^ x) & 0xff;

	return r;
}

//========================================================

void Key_Init (void);
void SCR_EndLoadingPlaque (void);


static void Com_Time_m( char *buffer, int bufferSize )
{
	time_t clock;

	time( &clock );
	strftime( buffer, bufferSize-1, "%H-%M", localtime(&clock));
}

static void Com_Date_m( char *buffer, int bufferSize )
{
	time_t clock;

	time( &clock );
	strftime( buffer, bufferSize-1, "%Y-%m-%d", localtime(&clock));
}

static void OnChange_Timescale (cvar_t *self, const char *oldValue)
{
	if(self->value > 400)
		Cvar_Set(self->name, "400");
}

#ifndef NDEBUG

/*
=============
Com_Error_f

Just throw a fatal error to
test error shutdown procedures
=============
*/
static void Com_Error_f( void ) {
	Com_Error( ERR_FATAL, "%s", Cmd_Argv( 1 ) );
}

static void Com_ErrorDrop_f( void ) {
	Com_Error( ERR_DROP, "%s", Cmd_Argv( 1 ) );
}

static void Com_Freeze_f( void ) {
	int seconds;
	unsigned int time;

	if( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: %s <seconds>\n", Cmd_Argv( 0 ) );
		return;
	}

	seconds = atoi( Cmd_Argv( 1 ) );
	if( seconds < 1 ) {
		return;
	}

	time = Sys_Milliseconds() + seconds * 1000;
	while( Sys_Milliseconds() < time )
		;

}

static void Com_Crash_f( void ) {
	*( uint32 * )0 = 0x123456;
}
#endif

void Cmd_Echo_f (void);
qboolean ComInitialized = false;

/*
=================
Qcommon_Init
=================
*/
void Qcommon_Init (int argc, char **argv)
{
	static const char	*apVersion = APPLICATION " v" VERSION " " CPUSTRING " " __DATE__ " " BUILDSTRING;

	if (setjmp (abortframe) )
		Sys_Error ("Error during initialization");

	z_chain.next = z_chain.prev = &z_chain;

	// prepare enough of the subsystems to handle
	// cvar and command buffer management
	COM_InitArgv (argc, argv);

	Swap_Init ();
	Cbuf_Init ();

	Cmd_Init ();
	Cvar_Init ();

	Key_Init ();

	// we need to add the early commands twice, because
	// a basedir or cddir needs to be set before execing
	// config files, but we want other parms to override
	// the settings of the config files
	Cbuf_AddEarlyCommands (false);
	Cbuf_Execute();

	FS_InitFilesystem ();

	Cbuf_AddText ("exec default.cfg\n");
	Cbuf_Execute();
	Cbuf_AddText ("exec aprconfig.cfg\n");
	Cbuf_Execute();
	FS_ExecConfig("autoexec.cfg");
	Cbuf_Execute();

	Cbuf_AddEarlyCommands (true);
	Cbuf_Execute();

	Cmd_AddCommand ("echo", Cmd_Echo_f);
	// init commands and vars
    Cmd_AddCommand ("z_stats", Z_Stats_f);
	Cmd_AddCommand( "z_check", Z_Check );

	host_speeds = Cvar_Get ("host_speeds", "0", 0);
	developer = Cvar_Get ("developer", "0", 0);
	timescale = Cvar_Get ("timescale", "1", CVAR_CHEAT);
	fixedtime = Cvar_Get ("fixedtime", "0", CVAR_CHEAT);
	logfile_active = Cvar_Get ("logfile", "0", 0);
	showtrace = Cvar_Get ("showtrace", "0", 0);
#ifdef DEDICATED_ONLY
	dedicated = Cvar_Get ("dedicated", "1", CVAR_ROM);
#else
	dedicated = Cvar_Get ("dedicated", "0", CVAR_NOSET);
#endif

	timescale->OnChange = OnChange_Timescale;
	OnChange_Timescale(timescale, timescale->resetString);

	Cvar_Get ("version", apVersion, CVAR_SERVERINFO|CVAR_ROM);

	Cmd_AddMacro( "date", Com_Date_m );
	Cmd_AddMacro( "time", Com_Time_m );

	if (dedicated->integer)
		Cmd_AddCommand ("quit", Com_Quit);

#ifndef NDEBUG
	Cmd_AddCommand( "error", Com_Error_f );
	Cmd_AddCommand( "errordrop", Com_ErrorDrop_f );
	Cmd_AddCommand( "freeze", Com_Freeze_f );
	Cmd_AddCommand( "crash", Com_Crash_f );
#endif

	Sys_Init ();

	srand(Sys_Milliseconds());

	NET_Init ();
	Netchan_Init ();

	SV_Init ();
	CL_Init ();

	ComInitialized = true;

	Cbuf_InsertFromDefer(); //Execute commands which was initialized after loading autoexec (ignore, highlight etc)
	FS_ExecConfig ("postinit.cfg");
	Cbuf_Execute();

	// add + commands from command line
	if (!Cbuf_AddLateCommands ())
	{	// if the user didn't give any commands, run default action
		if (!dedicated->integer)
			Cbuf_AddText ("toggleconsole\n");
		else
			Cbuf_AddText ("dedicated_start\n");
		Cbuf_Execute();
	}
	else
	{	// the user asked for something explicit
		// so drop the loading plaque
		SCR_EndLoadingPlaque();
	}

	Com_Printf ("====== " APPLICATION " Initialized ======\n\n");	
}

/*
=================
Qcommon_Frame
=================
*/
#ifdef GL_QUAKE
cvar_t *cl_avidemo;
#endif

void Qcommon_Frame (int msec)
{

	if (setjmp (abortframe) )
		return;			// an ERR_DROP was thrown

#ifdef GL_QUAKE
	if(cl_avidemo->integer)
	{
		msec = (int)(1000 / cl_avidemo->integer * timescale->value);
	}
	else
#endif
	{
		if (fixedtime->integer)
			msec = fixedtime->integer;

		msec = (int)(msec * timescale->value);
	}

	if (msec < 1)
		msec = 1;

	if (showtrace->integer)
	{
		extern	int c_traces, c_brush_traces;
		extern	int	c_pointcontents;

		Com_Printf ("%4i traces  %4i points\n", c_traces, c_pointcontents);
		c_traces = 0;
		c_brush_traces = 0;
		c_pointcontents = 0;
	}

	if (dedicated->integer)
	{
		char	*s;
		do
		{
			s = Sys_ConsoleInput ();
			if (s)
				Cbuf_AddText (va("%s\n",s));
		} while (s);
		Cbuf_Execute ();
	}

	if (!host_speeds->integer)
	{
		SV_Frame (msec);
		CL_Frame (msec);
	}
	else
	{
		unsigned int time_before = 0, time_between = 0, time_after = 0;
		unsigned int all, sv, gm, cl, rf;

		time_before = Sys_Milliseconds ();

		SV_Frame (msec);

		time_between = Sys_Milliseconds ();

		CL_Frame (msec);

		time_after = Sys_Milliseconds ();

		all = time_after - time_before;
		sv = time_between - time_before;
		cl = time_after - time_between;
		gm = time_after_game - time_before_game;
		rf = time_after_ref - time_before_ref;
		sv -= gm;
		cl -= rf;
		Com_Printf ("all:%3i sv:%3i gm:%3i cl:%3i rf:%3i\n", all, sv, gm, cl, rf);
	}	
}

/*
=================
Qcommon_Shutdown
=================
*/
void Qcommon_Shutdown (void)
{
	Z_FreeAll();
}

int ZLibDecompress (byte *in, int inlen, byte *out, int outlen, int wbits)
{
	z_stream zs;
	int result;

	memset (&zs, 0, sizeof(zs));

	zs.next_in = in;
	zs.avail_in = 0;

	zs.next_out = out;
	zs.avail_out = outlen;

	result = inflateInit2(&zs, wbits);
	if (result != Z_OK) {
		Com_Error (ERR_DROP, "ZLib data error! Error %d on inflateInit.\nMessage: %s", result, zs.msg);
		return 0;
	}

	zs.avail_in = inlen;

	result = inflate(&zs, Z_FINISH);
	if (result != Z_STREAM_END) {
		Com_Error (ERR_DROP, "ZLib data error! Error %d on inflate.\nMessage: %s", result, zs.msg);
		zs.total_out = 0;
	}

	result = inflateEnd(&zs);
	if (result != Z_OK) {
		Com_Error (ERR_DROP, "ZLib data error! Error %d on inflateEnd.\nMessage: %s", result, zs.msg);
		return 0;
	}

	return zs.total_out;
}

int ZLibCompressChunk(byte *in, int len_in, byte *out, int len_out, int method, int wbits)
{
	z_stream zs;
	int result;

	zs.next_in = in;
	zs.avail_in = len_in;
	zs.total_in = 0;

	zs.next_out = out;
	zs.avail_out = len_out;
	zs.total_out = 0;

	zs.msg = NULL;
	zs.state = NULL;
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.opaque = NULL;

	zs.data_type = Z_BINARY;
	zs.adler = 0;
	zs.reserved = 0;

	result = deflateInit2 (&zs, method, Z_DEFLATED, wbits, 9, Z_DEFAULT_STRATEGY);
	if (result != Z_OK)
		return -1;

	result = deflate(&zs, Z_FINISH);
	if (result != Z_STREAM_END)
		return -1;

	result = deflateEnd(&zs);
	if (result != Z_OK)
		return -1;

	return zs.total_out;
}


