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
// cmd.c -- Quake script command processing module

#include "qcommon.h"

static int	cmd_wait = 0;

static int	alias_count = 0;		// for detecting runaway loops
extern qboolean ComInitialized;

//=============================================================================

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
============
*/
static void Cmd_Wait_f (void)
{
	if(!ComInitialized)
		return;

	if(Cmd_Argc() > 1)
		cmd_wait = atoi(Cmd_Argv(1));
	else
		cmd_wait = 1;
}


/*
=============================================================================

						COMMAND BUFFER

=============================================================================
*/
#define	MAX_CMD_BUFFER	65536
#define	MAX_CMD_LINE	1024
static sizebuf_t	cmd_text;

static byte		cmd_text_buf[MAX_CMD_BUFFER];
static byte		defer_text_buf[MAX_CMD_BUFFER];
static int		deferCurSize = 0;

/*
============
Cbuf_Init
============
*/
void Cbuf_Init (void)
{
	SZ_Init(&cmd_text, cmd_text_buf, MAX_CMD_BUFFER);
}

/*
============
Cbuf_AddText

Adds command text at the end of the buffer
============
*/
void Cbuf_AddText (const char *text)
{
	int		l;
	
	if (!text[0])
		return;

	l = strlen (text);

	if (cmd_text.cursize + l >= cmd_text.maxsize)
	{
		Com_Printf ("Cbuf_AddText: overflow\n");
		return;
	}
	memcpy(&cmd_text.data[cmd_text.cursize], text, l);
	cmd_text.cursize += l;
}


/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
============
*/
void Cbuf_InsertText (const char *text)
{
	int		len;
	int		i;

	len = strlen( text ) + 1;
	if ( len + cmd_text.cursize >= cmd_text.maxsize ) {
		Com_Printf( "Cbuf_InsertText overflowed\n" );
		return;
	}

	// move the existing command text
	for ( i = cmd_text.cursize - 1 ; i >= 0 ; i-- ) {
		cmd_text.data[ i + len ] = cmd_text.data[ i ];
	}

	// copy the new text in
	memcpy( cmd_text.data, text, len - 1 );

	// add a \n
	cmd_text.data[ len - 1 ] = '\n';

	cmd_text.cursize += len;
}


/*
============
Cbuf_CopyToDefer
============
*/
void Cbuf_CopyToDefer (void)
{
	if (!cmd_text.cursize)
		return;

	memcpy (defer_text_buf, cmd_text_buf, cmd_text.cursize);
	defer_text_buf[cmd_text.cursize] = 0;
	deferCurSize = cmd_text.cursize;
	cmd_text.cursize = 0;
}

void Cbuf_AddTextToDefer (const char *text)
{
	int		l;
	
	if (!text[0])
		return;

	l = strlen(text) + 1;
	if (deferCurSize + l >= sizeof(defer_text_buf)) {
		Com_Printf ("Cbuf_AddTextToDefer: overflow\n");
		return;
	}
	memcpy(&defer_text_buf[deferCurSize], text, l - 1);

	deferCurSize += l;
	defer_text_buf[deferCurSize-1] = '\n';
}

/*
============
Cbuf_InsertFromDefer
============
*/
void Cbuf_InsertFromDefer (void)
{
	if (deferCurSize)
		Cbuf_InsertText ((char *)defer_text_buf);

	defer_text_buf[0] = 0;
	deferCurSize = 0;
}


/*
============
Cbuf_ExecuteText
============
*/
void Cbuf_ExecuteText (int exec_when, const char *text)
{
	switch (exec_when)
	{
	case EXEC_NOW:
		Cmd_ExecuteString (text);
		break;
	case EXEC_INSERT:
		Cbuf_InsertText (text);
		break;
	case EXEC_APPEND:
		Cbuf_AddText (text);
		break;
	default:
		Com_Error (ERR_FATAL, "Cbuf_ExecuteText: bad exec_when");
	}
}

/*
============
Cbuf_Execute
============
*/
void Cbuf_Execute (void)
{
	int		i;
	char	*text;
	char	line[MAX_CMD_LINE];
	int		quotes;

	alias_count = 0;		// don't allow infinite alias loops

	while (cmd_text.cursize)
	{
		if (cmd_wait > 0) {
			// skip out while text still remains in buffer, leaving it for next frame
			cmd_wait--;
			break;
		}

		// find a \n or ; line break
		text = (char *)cmd_text.data;

		quotes = 0;
		for (i = 0; i < cmd_text.cursize; i++)
		{
			if (text[i] == '"')
				quotes++;
			if ( !(quotes&1) &&  text[i] == ';')
				break;	// don't break if inside a quoted string
			if (text[i] == '\n')
				break;
		}
			
		if( i > MAX_CMD_LINE - 1)
			i = MAX_CMD_LINE - 1;		

		memcpy (line, text, i);
		line[i] = 0;

		// delete the text from the command buffer and move remaining commands down
		// this is necessary because commands (exec, alias) can insert data at the
		// beginning of the text buffer

		if (i == cmd_text.cursize)
			cmd_text.cursize = 0;
		else
		{
			i++;
			cmd_text.cursize -= i;
			if (cmd_text.cursize)
				memmove (text, text+i, cmd_text.cursize);
		}

		// execute the command line
		Cmd_ExecuteString (line);
	}
}

/*
===============
Cbuf_AddEarlyCommands

Adds command line parameters as script statements
Commands lead with a +, and continue until another +

Set commands are added early, so they are guaranteed to be set before
the client and server initialize for the first time.

Other commands are added late, after all initialization is complete.
===============
*/
void Cbuf_AddEarlyCommands (qboolean clear)
{
	int		i;
	char	*s;

	for (i = 0; i < COM_Argc(); i++)
	{
		s = COM_Argv(i);
		if (strcmp (s, "+set"))
			continue;
		Cbuf_AddText (va("set %s %s\n", COM_Argv(i+1), COM_Argv(i+2)));
		if (clear)
		{
			COM_ClearArgv(i);
			COM_ClearArgv(i+1);
			COM_ClearArgv(i+2);
		}
		i+=2;
	}
}

/*
=================
Cbuf_AddLateCommands

Adds command line parameters as script statements
Commands lead with a + and continue until another + or -
quake +vid_ref gl +map amlev1

Returns true if any late commands were added, which
will keep the demoloop from immediately starting
=================
*/
qboolean Cbuf_AddLateCommands (void)
{
	int		i, j, s = 0, argc;
	char	*text, *build, c;	
	qboolean	ret;

	// build the combined string to parse from
	argc = COM_Argc();
	for (i = 1; i < argc; i++)
		s += strlen (COM_Argv(i)) + 1;

	if (!s)
		return false;
		
	text = Z_TagMalloc ((s+1)*2, TAG_TEMP);
	text[0] = 0;
	for (i = 1; i < argc; i++)
	{
		strcat (text,COM_Argv(i));
		if (i != argc-1)
			strcat (text, " ");
	}
	
	// pull out the commands
	//build = Z_TagMalloc (s+1, TAG_CMDBUFF);
	build = text + s+1;
	build[0] = 0;
	
	for (i = 0; i < s-1; i++)
	{
		if (text[i] == '+')
		{
			i++;

			for (j=i ; (text[j] != '+') && (text[j] != 0) ; j++)
				;

			c = text[j];
			text[j] = 0;
			
			strcat (build, text+i);
			strcat (build, "\n");
			text[j] = c;
			i = j-1;
		}
	}

	ret = (build[0] != 0);
	if (ret)
		Cbuf_AddText (build);
	
	Z_Free (text);
	//Z_Free (build);

	return ret;
}


/*
==============================================================================

						SCRIPT COMMANDS

==============================================================================
*/


/*
===============
Cmd_Exec_f
===============
*/
static void Cmd_Exec_f (void)
{
	char	*f, config[MAX_QPATH];
	int		len;

	if (Cmd_Argc () != 2)
	{
		Com_Printf ("exec <filename> : execute a script file\n");
		return;
	}

	Q_strncpyz(config, Cmd_Argv(1), sizeof(config));
	len = FS_LoadFile (config, (void **)&f);
	if (!f || !len)
	{
		if(!strchr(config, '.')) {
			COM_DefaultExtension(config, sizeof(config), ".cfg");
			len = FS_LoadFile (config, (void **)&f);
		}

		if (!f || !len)
		{
			Com_Printf ("couldn't exec %s\n", config);
			return;
		}
	}
	Com_Printf ("execing %s\n", config);
	
	Cbuf_InsertText (f);

	FS_FreeFile (f);
}

/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
void Cmd_Echo_f (void)
{
	Com_Printf (S_ENABLE_COLOR "%s\n", Cmd_ArgsFrom(1));
}


/*
==============================================================================

						SCRIPT COMMANDS

==============================================================================
*/

#define	MAX_ALIAS_NAME	32
#define	ALIAS_LOOP_COUNT	16

typedef struct cmdalias_s
{
	struct cmdalias_s	*next;
	struct cmdalias_s	*hashNext;

	char	*value;
	qboolean params;
	char	name[1];
} cmdalias_t;

#define ALIAS_HASH_SIZE	64
static cmdalias_t	*cmd_aliasHash[ALIAS_HASH_SIZE];

static cmdalias_t	*cmd_alias;

static int AliasSort( const cmdalias_t **a, const cmdalias_t **b )
{
	return strcmp((*a)->name, (*b)->name);
}

static void Cmd_Aliaslist_f (void)
{
	cmdalias_t	*a, **sortedList;
    char    *filter = NULL;
	int		i, total = 0, matching = 0;

    if (Cmd_Argc() > 1)
		filter = Cmd_Argv(1);

	for (a = cmd_alias; a; a = a->next, total++) {
		if (filter && !Com_WildCmp(filter, a->name) && !strstr(a->name, filter))
			continue;
		matching++;
	}

	if (!matching) {
		Com_Printf("%i alias found (%i total alias)\n", matching, total);
		return;
	}

	sortedList = Z_TagMalloc(matching * sizeof(cmdalias_t *), TAG_ALIAS);
	
	for (matching = 0, a = cmd_alias; a; a = a->next) {
		if (filter && !Com_WildCmp(filter, a->name) && !strstr(a->name, filter))
			continue;
		sortedList[matching++] = a;
	}

	qsort(sortedList, matching, sizeof(sortedList[0]), (int (*)(const void *, const void *))AliasSort);

	for (i = 0; i < matching; i++) {
		a = sortedList[i];
		Com_Printf ("%s : \"%s\"\n", a->name, a->value);
	}

	if (filter)
		Com_Printf("%i alias found (%i total alias)\n", matching, total);
	else
		Com_Printf("%i alias\n", matching);

	Z_Free(sortedList);
}

/*
===============
Cmd_AliasFind
===============
*/
cmdalias_t *Cmd_AliasFind( const char *name )
{
	unsigned int hash;
	cmdalias_t *alias;

	hash = Com_HashKey(name, ALIAS_HASH_SIZE);
	for( alias=cmd_aliasHash[hash]; alias ; alias=alias->hashNext ) {
		if( !Q_stricmp( name, alias->name ) )
			return alias;
	}

	return NULL;
}

/*
===============
Cmd_Alias_f

Creates a new command that executes a command string (possibly ; seperated)
===============
*/
static void Cmd_Alias_f (void)
{
	cmdalias_t	*a;
	const char	*s;
	unsigned int hash;
	int			 len;

	if (Cmd_Argc() == 1) {
		Com_Printf ("usage: %s <name> <command>\n", Cmd_Argv(0));
		return;
	}

	s = Cmd_Argv(1);
	len = strlen(s);
	if (len >= MAX_ALIAS_NAME) {
		Com_Printf ("Alias name is too long\n");
		return;
	}

	if (Cmd_Argc() == 2) {
		a = Cmd_AliasFind(s);
		if (a)
			Com_Printf( "\"%s\" = \"%s\"\n", a->name, a->value );
		else
			Com_Printf( "\"%s\" is undefined\n", s );

		return;		
	}

	// if the alias already exists, reuse it
	a = Cmd_AliasFind(s);
	if( a )
	{
		Z_Free (a->value);
	}
	else
	{
		a = Z_TagMalloc (sizeof(cmdalias_t) + len, TAG_ALIAS);
		strcpy(a->name, s);
		a->next = cmd_alias;
		cmd_alias = a;

		hash = Com_HashKey(s, ALIAS_HASH_SIZE);
		a->hashNext = cmd_aliasHash[hash];
		cmd_aliasHash[hash] = a;
	}

// copy the rest of the command line
	a->value = CopyString(Cmd_ArgsFrom(2), TAG_ALIAS);

	//check if alias takes params
	a->params = false;
	for(s = a->value; *s; s++)
	{
		if(*s != '$')
			continue;

		s++;
		if(*s == '{')
			s++;

		if(*s < '1' || *s > '9')
			continue;

		if(s[1] && s[1] == '-')
			s++;

		if(s[1] <= ' ' || s[1] == '}' || s[1] == '$') {
			a->params = true;
			break;
		}
	}

}

static void Cmd_UnAlias_f (void)
{
	char		*s;
	unsigned int hash;
	cmdalias_t	*a, *entry, **back;

	if (Cmd_Argc() == 1) {
		Com_Printf ("Usage: %s <name>\n", Cmd_Argv(0));
		return;
	}

	s = Cmd_Argv(1);
	if (strlen(s) >= MAX_ALIAS_NAME) {
		Com_Printf ("Alias name is too long\n");
		return;
	}

	hash = Com_HashKey(s, ALIAS_HASH_SIZE);
	back = &cmd_aliasHash[hash];
	for(entry = *back; entry; back=&entry->hashNext, entry=entry->hashNext ) {
		if(!Q_stricmp(s, entry->name)) {
			*back = entry->hashNext;
			break;
		}
	}
	if(!entry) {
		Com_Printf ("Cmd_Unalias_f: \"%s\" not added\n", s);
		return;
	}

	back = &cmd_alias;
	for(a = *back; a; back = &a->next, a = a->next) {
		if(a == entry) {
			*back = a->next;
			Z_Free(a->value);
			Z_Free(a);
			Com_Printf ("Alias \"%s\" removed\n", s);
			break;
		}
	}
}

void Alias_CommandCompletion( const char *partial, void(*callback)(const char *name, const char *value) )
{
	int len;
	const cmdalias_t *a;

	len = strlen(partial);
	if(!len)
		return;
	
	for (a=cmd_alias ; a ; a=a->next) {
		if (!Q_strnicmp (partial, a->name, len))
			callback( a->name, NULL );
	}
}

/*
===================
WriteAliases
===================
*/
void Cmd_WriteAliases (FILE *f)
{
	const cmdalias_t	*a;

	for (a = cmd_alias; a; a = a->next)
		fprintf (f, "alias %s \"%s\"\n", a->name, a->value);
}


/*
=============================================================================

					MESSAGE TRIGGERS

=============================================================================
*/

typedef struct cmd_trigger_s {
	char		*match;
	char		*command;

	struct cmd_trigger_s	*next;
} cmd_trigger_t;

static cmd_trigger_t	*cmd_triggers = NULL;

/*
============
Cmd_Trigger_f
============
*/
static void Cmd_Trigger_f( void )
{
	cmd_trigger_t *trigger;
	const char *command, *match;
	int cmdLen, matchLen;

	if(Cmd_Argc() == 1)
	{
		Com_Printf("Usage: %s <command> <match>\n", Cmd_Argv(0));
		if(!cmd_triggers) {
			Com_Printf("No current message triggers\n");
			return;
		}

		Com_Printf("Current message triggers:\n");
		for(trigger = cmd_triggers; trigger; trigger = trigger->next) {
			Com_Printf( "\"%s\" = \"%s\"\n", trigger->command, trigger->match );
		}
		return;
	}

	if(Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <command> <match>\n", Cmd_Argv(0));
		return;
	}

	command = Cmd_Argv(1);
	match = Cmd_ArgsFrom(2);

	// don't create the same trigger twice
	for( trigger=cmd_triggers; trigger; trigger=trigger->next ) {
		if(!strcmp(trigger->command, command) && !strcmp(trigger->match, match))
		{
			//Com_Printf( "Exactly same trigger allready exists\n" );
			return;
		}
	}

	cmdLen = strlen(command) + 1;
	matchLen = strlen(match) + 1;
	if(matchLen < 4) {
		Com_Printf("match is too short\n");
		return;
	}

	trigger = Z_TagMalloc( sizeof( cmd_trigger_t ) + cmdLen + matchLen, TAG_TRIGGER );
	trigger->command = (char *)((byte *)trigger + sizeof( cmd_trigger_t ));
	trigger->match = trigger->command + cmdLen;
	strcpy(trigger->command, command);
	strcpy(trigger->match, match);
	trigger->next = cmd_triggers;
	cmd_triggers = trigger;
}

static void Cmd_UnTrigger_f( void )
{
	cmd_trigger_t *trigger, *next, **back;
	const char *command, *match;
	int count = 0;

	if(!cmd_triggers) {
		Com_Printf("No current message triggers\n");
		return;
	}

	if(Cmd_Argc() == 1)
	{
		Com_Printf("Usage: %s <command> <match>\n", Cmd_Argv(0));
		Com_Printf( "Current message triggers:\n" );
		for(trigger = cmd_triggers; trigger; trigger = trigger->next) {
			Com_Printf( "\"%s\" = \"%s\"\n", trigger->command, trigger->match );
		}
		return;
	}

	if(Cmd_Argc() == 2)
	{
		if(!Q_stricmp(Cmd_Argv(1), "all"))
		{
			for(trigger = cmd_triggers; trigger; trigger = next)
			{
				next = trigger->next;
				Z_Free(trigger);
				count++;
			}
			cmd_triggers = NULL;

			if(count)
				Com_Printf("Removed all (%i) triggers\n", count);
			return;
		}

		Com_Printf("Usage: %s <command> <match>\n", Cmd_Argv(0));
		return;
	}


	command = Cmd_Argv(1);
	match = Cmd_ArgsFrom(2);

	back = &cmd_triggers;
	for(;;)
	{
		trigger = *back;
		if(!trigger)
		{
			Com_Printf ("Cant find trigger \"%s\" = \"%s\".\n", command, match);
			return;
		}
		if(!strcmp(trigger->command, command) && !strcmp(trigger->match, match))
		{
			*back = trigger->next;
			Com_Printf ("Removed trigger \"%s\" = \"%s\"\n", trigger->command, trigger->match );
			Z_Free (trigger);
			return;
		}
		back = &trigger->next;
	}
}

/*
============
Cmd_ExecTrigger
============
*/
void Cmd_ExecTrigger( const char *string )
{
	const cmd_trigger_t *trigger;
	const char *text;

	// execute matching triggers
	for(trigger = cmd_triggers; trigger; trigger = trigger->next)
	{
		text = Cmd_MacroExpandString(trigger->match);
		if(text && Com_WildCmp(text, string)) {
			Cbuf_AddText(trigger->command);
			Cbuf_AddText("\n");
		}
	}
}

/*
=============================================================================

					MACRO EXECUTION

=============================================================================
*/

typedef struct cmd_macro_s {
	struct cmd_macro_s	*next;

	const char		*name;
	xmacro_t		function;
} cmd_macro_t;

static cmd_macro_t	*cmd_macros;

/*
============
Cmd_MacroFind
============
*/
static cmd_macro_t *Cmd_MacroFind( const char *name )
{
	cmd_macro_t *macro;

	for( macro = cmd_macros; macro; macro = macro->next ) {
		if( !Q_stricmp( macro->name, name ) )
			return macro;
	}

	return NULL;
}

/*
============
Cmd_AddMacro
============
*/
void Cmd_AddMacro( const char *name, void (*function)( char *, int ) )
{
	cmd_macro_t	*macro;
	
// fail if the macro already exists
	if( Cmd_MacroFind( name ) ) {
		Com_Printf( "Cmd_AddMacro: %s already defined\n", name );
		return;
	}

	macro = Z_TagMalloc( sizeof( cmd_macro_t ), TAG_MACRO );
	macro->name = name;
	macro->function = function;
	macro->next = cmd_macros;
	cmd_macros = macro;
}

xmacro_t Cmd_FindMacroFunction( const char *name ) {
	cmd_macro_t *macro;

	macro = Cmd_MacroFind( name );
	if( !macro ) {
		return NULL;
	}

	return macro->function;
}

static char *Cmd_MacroString( const char *name )
{
	const cmd_macro_t	*macro;
	static char macroString[128];
	
	macroString[0] = 0;
	macro = Cmd_MacroFind(name);
	if(macro)
		macro->function(macroString, sizeof(macroString));

	return macroString;
}

/*
============
Cmd_MacroList_f
============
*/
static void Cmd_MacroList_f( void ) {
	const cmd_macro_t	*macro;
	int					total = 0;

	for( macro = cmd_macros; macro; macro = macro->next, total++ ) {
		Com_Printf( "%-14s - \"%s\"\n", macro->name, Cmd_MacroString(macro->name) );
	}
	Com_Printf( "%i macros\n", total );
}

/*
=============================================================================

					COMMAND EXECUTION

=============================================================================
*/

typedef struct cmd_function_s
{
	struct cmd_function_s	*next;
	struct cmd_function_s	*hashNext;

	const char				*name;
	xcommand_t				function;
} cmd_function_t;


static	int			cmd_argc;
static	char		*cmd_argv[MAX_STRING_TOKENS];
static	char		cmd_tokenized[MAX_STRING_CHARS*2];
static	char		cmd_args[MAX_STRING_CHARS];

static	cmd_function_t	*cmd_functions;		// possible commands to execute

#define CMD_HASH_SIZE	128
static cmd_function_t	*cmd_hash[CMD_HASH_SIZE];

/*
============
Cmd_Argc
============
*/
int Cmd_Argc (void)
{
	return cmd_argc;
}

/*
============
Cmd_Argv
============
*/
char *Cmd_Argv (int arg)
{
	if ( (unsigned)arg >= cmd_argc )
		return "";
	return cmd_argv[arg];	
}

/*
============
Cmd_Args

Returns a single string containing argv(1) to argv(argc()-1)
============
*/
char *Cmd_Args (void)
{
	return cmd_args;
}

char *Cmd_ArgsFrom (int arg)
{
	static char argsFrom[2048]; //Same size as cmd_tokenized
	int i;

	argsFrom[0] = 0;

	if (arg < 0)
		arg = 0;
	for (i = arg; i < cmd_argc; i++)
	{
		if (argsFrom[0]) {
			strcat(argsFrom, " ");
		}
		strcat(argsFrom, cmd_argv[i]);
	}

	return argsFrom;
}


/*
======================
Cmd_MacroExpandString
======================
*/
static qboolean aliasHack = false;

const char *Cmd_MacroExpandString (const char *text)
{
	int				i, j, count, len;
	qboolean		inquote = false, rescan, badtoken;
	static	char	expanded[MAX_STRING_CHARS];
	char			temporary[MAX_STRING_CHARS], *token;
	const char		*scan, *start;

	scan = text;
	len = strlen(scan);
	if (len >= MAX_STRING_CHARS) {
		Com_Printf ("Cmd_MacroExpandString: Line exceeded %i chars, discarded.\n", MAX_STRING_CHARS);
		return NULL;
	}

	count = 0;

	for (i=0 ; i<len ; i++)
	{
		if (scan[i] == '"')
			inquote ^= 1;
		if (inquote)
			continue;	// don't expand inside quotes
		if (scan[i] != '$')
			continue;
		// scan out the complete macro
		start = scan+i+1;

		// skip whitespace
		while (*start && *start <= ' ') {
			start++;
		}

		if (!*start)
			break;

		token = temporary;
		if(*start == '{') //${var} scripting
		{
			start++;
			if( *start == '$' )
				start++;

			badtoken = true;
			while( *start > ' ' ) {
				if( *start == '}' ) {
					start++;
					badtoken = false;
					break;
				}
				*token++ = *start++;
			}
			if(badtoken)
				continue;
		}
		else
		{
			while( *start > ' ' ) {
				if( *start == '$' ) { //$var$$var scripting
					start++;
					break;
				}
				*token++ = *start++;
			}
		}
		*token = 0;

		rescan = false;
		if(aliasHack)
		{
			if(*temporary < '1' || *temporary > '9')
				continue;

			if(!temporary[1])
				token = Cmd_Argv(*temporary - '0');
			else if(temporary[1] == '-' && !temporary[2])
				token = Cmd_ArgsFrom(*temporary - '0');
			else
				continue;
		}
		else
		{
			token = Cvar_VariableString ( temporary );
			if(!*token)
				token = Cmd_MacroString( temporary );
			else
				rescan = true;
		}

		j = strlen(token);

		len += j - (start - scan - i);
		if (len >= MAX_STRING_CHARS) {
			Com_Printf ("Expanded line exceeded %i chars, discarded.\n", MAX_STRING_CHARS);
			return NULL;
		}
		if (++count == 100) {
			Com_Printf ("Macro expansion loop, discarded.\n");
			return NULL;
		}

		strncpy (temporary, scan, i);
		strcpy (temporary+i, token);
		strcpy (temporary+i+j, start);

		strcpy (expanded, temporary);
		scan = expanded;
		i--;
		if(!rescan)
			i += j;
	}

	if (inquote) {
		Com_Printf ("Line has unmatched quote, discarded.\n");
		return NULL;
	}

	return scan;
}


/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
$Cvars will be expanded unless they are in a quoted token
============
*/
void Cmd_TokenizeString (const char *text, qboolean macroExpand)
{
	char *textOut;

// clear the args from the last string
	cmd_argc = 0;
	cmd_args[0] = 0;
	
	// macro expand the text
	if (macroExpand) {
		text = Cmd_MacroExpandString (text);
		if (!text)
			return;
	} else {
		if (!text)
			return;
		if (strlen(text) >= MAX_STRING_CHARS) { //Macroexpand do this
			Com_Printf ("Cmd_TokenizeString: Line exceeded %i chars, discarded.\n", MAX_STRING_CHARS);
			return;
		}
	}

	textOut = cmd_tokenized;

	do {
		// skip whitespace up to a /n
		while (*text <= ' ') {
			if (!*text || *text == '\n')
				return;
			text++;
		}

		if (text[0] == '/' && text[1] == '/')
			return;			// skip // comments

		// set cmd_args to everything after the first arg
		if (cmd_argc == 1)
		{
			int		l;

			strcpy(cmd_args, text);
			// strip off any trailing whitespace
			for (l = strlen(cmd_args) - 1; l >= 0 ; l--) {
				if (cmd_args[l] > ' ')
					break;

				cmd_args[l] = 0;
			}
		}
		
		cmd_argv[cmd_argc++] = textOut;

		if ( *text == '"' )
		{	// handle quoted strings
			text++;

			while (*text && *text != '"') {
				*textOut++ = *text++;
			}
			*textOut++ = 0;

			if (!*text++)
				return;		// all tokens parsed
		}
		else
		{	// parse a regular word
			while (*text > ' ') {
				*textOut++ = *text++;
			}
			*textOut++ = 0;
		}
	} while (cmd_argc < MAX_STRING_TOKENS);

}

/*
============
Cmd_Find
============
*/
cmd_function_t *Cmd_Find( const char *name )
{
	cmd_function_t *cmd;
	unsigned int	hash;

	hash = Com_HashKey(name, CMD_HASH_SIZE);
	for( cmd=cmd_hash[hash]; cmd; cmd=cmd->hashNext ) {
		if( !Q_stricmp( cmd->name, name ) )
			return cmd;
	}

	return NULL;
}

/*
============
Cmd_AddCommand
============
*/
cvar_t *Cvar_FindVar (const char *var_name);

void Cmd_AddCommand (const char *cmd_name, xcommand_t function)
{
	cmd_function_t	*cmd;
	unsigned int	hash;
	
// fail if the command is a variable name
	if (Cvar_FindVar( cmd_name )) {
		Com_Printf ("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		//return; //Cmd's are priority 1
	}
	
	// fail if the command already exists
	hash = Com_HashKey(cmd_name, CMD_HASH_SIZE);
	for( cmd=cmd_hash[hash]; cmd; cmd=cmd->hashNext ) {
		if(!Q_stricmp(cmd->name, cmd_name)) {
			Com_Printf ("Cmd_AddCommand: %s already defined\n", cmd_name);
			return;
		}
	}

	cmd = Z_TagMalloc (sizeof(cmd_function_t), TAG_CMD);
	cmd->name = cmd_name;
	cmd->function = function;
	cmd->next = cmd_functions;
	cmd_functions = cmd;

	cmd->hashNext = cmd_hash[hash];
	cmd_hash[hash] = cmd;
}

/*
============
Cmd_RemoveCommand
============
*/
void Cmd_RemoveCommand (const char *cmd_name)
{
	cmd_function_t	*cmd, *entry, **back;
	unsigned int	hash;

	hash = Com_HashKey(cmd_name, CMD_HASH_SIZE);
	back = &cmd_hash[hash];
	for(entry = *back; entry; back=&entry->hashNext, entry=entry->hashNext ) {
		if(!Q_stricmp(cmd_name, entry->name)) {
			*back = entry->hashNext;
			break;
		}
	}
	if(!entry) {
		Com_Printf ("Cmd_RemoveCommand: \"%s\" not added\n", cmd_name);
		return;
	}

	back = &cmd_functions;
	for(cmd = *back; cmd; back = &cmd->next, cmd = cmd->next) {
		if(cmd == entry) {
			*back = cmd->next;
			Z_Free (cmd);
			break;
		}
	}
}


/*
============
Cmd_CommandCompletion
============
*/
void Cmd_CommandCompletion( const char *partial, void(*callback)(const char *name, const char *value) )
{
	int len;
	const cmd_function_t	*cmd;

	len = strlen(partial);
	if(!len)
		return;
	
	for (cmd=cmd_functions ; cmd ; cmd=cmd->next) {
		if (!Q_strnicmp (partial, cmd->name, len))
			callback( cmd->name, NULL );
	}
}

/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
FIXME: lookupnoadd the token to speed search?
============
*/
void	Cmd_ExecuteString (const char *text)
{	
	const cmd_function_t	*cmd;
	const cmdalias_t		*a;
	unsigned int			hash;

	Cmd_TokenizeString (text, true);
			
	// execute the command line
	if (!cmd_argc)
		return;		// no tokens

	hash = Com_HashValue(cmd_argv[0]);
	// check functions
	for (cmd = cmd_hash[hash & (CMD_HASH_SIZE-1)]; cmd; cmd=cmd->hashNext ) {
		if ( Q_stricmp(cmd_argv[0], cmd->name) )
			continue;

		if (!cmd->function) // forward to server command
			Cmd_ForwardToServer();
		else
			cmd->function();

		return;
	}

	for (a = cmd_aliasHash[hash & (ALIAS_HASH_SIZE-1)]; a; a=a->hashNext ) {
		if ( Q_stricmp(cmd_argv[0], a->name) )
			continue;

		if (++alias_count == ALIAS_LOOP_COUNT) {
			Com_Printf ("ALIAS_LOOP_COUNT\n");
			return;
		}
		if (a->params) {
			aliasHack = true;
			text = Cmd_MacroExpandString(a->value);
			aliasHack = false;
			if(text)
				Cbuf_InsertText(text);
		} else {
			Cbuf_InsertText(a->value);
		}
		return;
	}
	
	// check cvars
	if (Cvar_Command(cmd_argv[0], hash))
		return;

	if (!ComInitialized) { //could command which isnt initialized yet
		if (Q_stricmp(cmd_argv[0], "vid_restart") &&
			Q_stricmp(cmd_argv[0], "snd_restart")) //Hacky, but no need extra restart
			Cbuf_AddTextToDefer(text);
		return;
	}

	// send it as a server command if we are connected
	Cmd_ForwardToServer();
}

/*
============
Cmd_List_f
============
*/
static int CmdSort( const cmd_function_t **a, const cmd_function_t **b )
{
	return strcmp ((*a)->name, (*b)->name);
}

static void Cmd_List_f (void)
{
	cmd_function_t  *cmd;
    const char *filter = NULL;
	int		i, total = 0, matching = 0;
	cmd_function_t	**sortedList;

    if (Cmd_Argc() > 1)
	    filter = Cmd_Argv(1);

	for (cmd = cmd_functions; cmd; cmd = cmd->next, total++) {
        if (filter && !Com_WildCmp(filter, cmd->name) && !strstr(cmd->name, filter))
			continue;
		matching++;
	}

	if(!matching) {
		Com_Printf("%i cmds found (%i total cmds)\n", matching, total);
		return;
	}

	sortedList = Z_TagMalloc (matching * sizeof(cmd_function_t *), TAG_CMD);
	
	for (matching = 0, cmd = cmd_functions; cmd; cmd = cmd->next) {
        if (filter && !Com_WildCmp(filter, cmd->name) && !strstr(cmd->name, filter))
			continue;
		sortedList[matching++] = cmd;
	}

	qsort(sortedList, matching, sizeof(sortedList[0]), (int (*)(const void *, const void *))CmdSort);

	for (i = 0; i < matching; i++)
	{
		cmd = sortedList[i];
        Com_Printf("%s\n", cmd->name);
    }

    if (filter)
		Com_Printf("%i cmds found (%i total cmds)\n", matching, total);
    else
		Com_Printf("%i cmds\n", matching);

	Z_Free (sortedList);
}


/*
============
Cmd_If_f
============
*/
static void Cmd_If_f( void )
{
	char command[MAX_STRING_CHARS];
	const char *a, *b, *op;
	qboolean numeric, istrue;
	int i;

	if( Cmd_Argc() < 5 ) {
		Com_Printf( "Usage: if <expr> <op> <expr> then <command> [else <command>]\n" );
		return;
	}

	a = Cmd_Argv(1);
	op = Cmd_Argv(2);
	b = Cmd_Argv(3);

#define CHECK_NUMERIC	if( !numeric ) { Com_Printf( "Can't use '%s' with non-numeric expression(s)\n", op );	return; }

	numeric = (Q_IsNumeric(a) && Q_IsNumeric(b));
	if(!strcmp(op, "=="))
	{
		istrue = numeric ? ((float)atof(a) == (float)atof(b)) : (!strcmp(a, b));
	}
	else if(!strcmp(op, "!=") || !strcmp(op, "<>"))
	{
		istrue = numeric ? ((float)atof(a) != (float)atof(b)) : (strcmp(a, b));
	}
	else if(!strcmp(op, "<"))
	{
		CHECK_NUMERIC;
		istrue = ((float)atof(a) < (float)atof(b));
	}
	else if(!strcmp(op, "<="))
	{
		CHECK_NUMERIC;
		istrue = ((float)atof(a) <= (float)atof(b));
	}
	else if(!strcmp(op, ">"))
	{
		CHECK_NUMERIC;
		istrue = ((float)atof(a) > (float)atof(b));
	}
	else if(!strcmp(op, ">="))
	{
		CHECK_NUMERIC;
		istrue = ((float)atof(a) >= (float)atof(b));
	}
	else if(!Q_stricmp(op, "isin"))
	{
		istrue = (strstr(b, a) != NULL);
	}
	else if(!Q_stricmp(op, "!isin"))
	{
		istrue = (strstr(b, a) == NULL);
	}
	else if(!Q_stricmp(op, "isini"))
	{
		istrue = (Q_stristr(b, a) != NULL);
	}
	else if(!Q_stricmp(op, "!isini"))
	{
		istrue = (Q_stristr(b, a) == NULL);
	}
	else if(!Q_stricmp(op, "eq"))
	{
		istrue = (!Q_stricmp(a, b));
	}
	else if(!Q_stricmp(op, "ne"))
	{
		istrue = (Q_stricmp(a, b));
	}
	else
	{
		Com_Printf( "Unknown operator '%s'\n", op );
		Com_Printf( "Valid are: ==, != or <>, <, <=, >, >=, isin, !isin, isini, !isini, eq, ne\n" );
		return;
	}

	i = 4;
	if(!Q_stricmp( Cmd_Argv( i ), "then" ))
		i++;

	command[0] = 0;
	for( ; i<Cmd_Argc() ; i++ ) {
		if( !Q_stricmp(Cmd_Argv(i), "else") ) {
			break;
		}
		if( istrue ) {
			if( command[0] ) {
				Q_strncatz(command, " ", sizeof(command));
			}
			Q_strncatz(command, Cmd_Argv(i), sizeof(command));
		}
	}

	if( istrue ) {
		Cbuf_InsertText( command );
		return;
	}

	i++;
	if(i >= Cmd_Argc())
		return;

	Cbuf_InsertText( Cmd_ArgsFrom(i) );
}

/*
============
Cmd_Init
============
*/
void Cmd_Init (void)
{
	// register our commands
	Cmd_AddCommand ("if", Cmd_If_f);
	Cmd_AddCommand ("trigger", Cmd_Trigger_f );
	Cmd_AddCommand ("untrigger", Cmd_UnTrigger_f);
	Cmd_AddCommand ("macrolist", Cmd_MacroList_f );

	Cmd_AddCommand ("cmdlist",Cmd_List_f);
	Cmd_AddCommand ("exec",Cmd_Exec_f);
	//Cmd_AddCommand ("echo",Cmd_Echo_f);
	Cmd_AddCommand ("alias",Cmd_Alias_f);
	Cmd_AddCommand ("unalias", Cmd_UnAlias_f);
	Cmd_AddCommand ("aliaslist",Cmd_Aliaslist_f);
	Cmd_AddCommand ("wait", Cmd_Wait_f);
}

