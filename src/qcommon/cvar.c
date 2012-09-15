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
// cvar.c -- dynamic variable tracking

#include "qcommon.h"

cvar_t	*cvar_vars;

//#define		MAX_CVARS	512
//static cvar_t		cvar_indexes[MAX_CVARS];
//static unsigned int	cvar_numIndexes = 0;

#define CVAR_HASH_SIZE	256
static cvar_t *cvarHash[CVAR_HASH_SIZE];

static qboolean userCreated = false;
static int currentSubsystem = CVAR_SYSTEM_GENERIC;
int	silentSubsystem;


qboolean CL_CheatsOK(void);
void CL_RestartFilesystem( qboolean execAutoexec );
qboolean CL_IsDisconnected();

//static cvar_t *Cvar_Set2 (const char *var_name, const char *value, qboolean force);
static void Cvar_SetByVar(cvar_t *var, const char *value, qboolean force);

char *Cvar_CopyString (const char *in,  int tag);

void Cvar_Subsystem( int subsystem ) {
	currentSubsystem = subsystem & CVAR_SYSTEM_MASK;
}

/*
============
Cvar_InfoValidate
============
*/
static qboolean Cvar_InfoValidate (const char *s)
{
	while( *s ) {
		if( *s == '\\' || *s == '\"' || *s == ';' ) {
			return false;
		}
		s++;
	}
	return true;
}

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar (const char *var_name)
{
	cvar_t	*var;
	unsigned int hash;

	hash = Com_HashKey(var_name, CVAR_HASH_SIZE);
	for (var = cvarHash[hash]; var; var = var->hashNext)
		if (!Q_stricmp (var_name, var->name))
			return var;

	return NULL;
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue (const char *var_name)
{
	const cvar_t	*var;
	
	var = Cvar_FindVar(var_name);
	if (!var)
		return 0.0f;

	return var->value;
}

int Cvar_VariableIntValue (const char *var_name)
{
	const cvar_t	*var;
	
	var = Cvar_FindVar(var_name);
	if (!var)
		return 0;

	return var->integer;
}

/*
============
Cvar_VariableString
============
*/
char *Cvar_VariableString (const char *var_name)
{
	const cvar_t *var;
	
	var = Cvar_FindVar(var_name);
	if (!var)
		return "";

	return var->string;
}


/*
============
Cvar_CommandCompletion
============
*/
void Cvar_CommandCompletion ( const char *partial, void(*callback)(const char *name, const char *value) )
{
	const cvar_t		*cvar;
	int			len;
	
	len = strlen(partial);
	
	for ( cvar = cvar_vars ; cvar ; cvar = cvar->next ) {
		if (!Q_strnicmp (partial,cvar->name, len))
			callback( cvar->name, cvar->string );
	}
}


/*
============
Cvar_Get

If the variable already exists, the value will not be set
The flags will be or'ed in if the variable exists.
============
*/
cvar_t *Cvar_Get (const char *var_name, const char *var_value, int flags)
{
	cvar_t	*var;
	unsigned int hash;
	
	if( !var_name ) {
		Com_Error(ERR_FATAL, "Cvar_Get: NULL variable name");
	}

	if (flags & CVAR_INFOMASK) {
		if (!Cvar_InfoValidate(var_name)) {
			Com_Printf("Cvar_Get: Invalid info cvar name.\n");
			return NULL;
		}
	}

	hash = Com_HashKey(var_name, CVAR_HASH_SIZE);
	for (var = cvarHash[hash]; var; var = var->hashNext) {
		if (!Q_stricmp (var_name, var->name))
			break;
	}

	if (var)
	{
		if( !( flags & CVAR_USER_CREATED ) ) {
			if (var->flags & CVAR_USER_CREATED) {
				var->flags &= ~CVAR_USER_CREATED;
				var->flags |= currentSubsystem;
				Z_Free( var->resetString );
				var->resetString = Cvar_CopyString( var_value, TAG_CVAR );

				if (flags & CVAR_ROM) { //Users cant change these
					Cvar_SetByVar( var, var_value, true );
				}
			}
			if (flags & CVAR_SYSTEM_GAME) {
				if (var->flags & CVAR_SYSTEM_MASK) {
					flags &= ~CVAR_SYSTEM_GAME;
				}
			} else {
				if ((var->flags & CVAR_SYSTEM_MASK) && (flags & CVAR_SYSTEM_MASK)) {
					if((var->flags & CVAR_SYSTEM_MASK) != (flags & CVAR_SYSTEM_MASK))
						var->flags &= ~(var->flags & CVAR_SYSTEM_MASK);
				}
			}
		} else {
			flags &= ~CVAR_USER_CREATED;
		}

		var->flags |= flags;

		// if we have a latched string, take that value now
		if ( var->latched_string ) {
			Z_Free (var->string);
			var->string = var->latched_string;
			var->latched_string = NULL;
			var->value = (float)atof(var->string);
			var->integer = Q_rint(var->value);
			var->modified = true;

			if (var->flags & CVAR_USERINFO)
				userinfo_modified = true;	// transmit at next oportunity
		}

		return var;
	}

	if (!var_value)
		return NULL;

	if (flags & CVAR_INFOMASK) {
		if (!Cvar_InfoValidate (var_value)) {
			Com_Printf("Cvar_Get: Invalid info cvar value.\n");
			return NULL;
		}
	}

	var = Z_TagMalloc(sizeof(*var) + strlen( var_name ) + 1, TAG_CVAR);

	var->name = (char *)((byte *)var + sizeof(*var));
	strcpy(var->name, var_name);
	var->string = Cvar_CopyString(var_value, TAG_CVAR);
	var->resetString = Cvar_CopyString(var_value, TAG_CVAR);
	var->latched_string = NULL;
	var->value = (float)atof(var->string);
	var->integer = Q_rint(var->value);
	var->flags = flags|currentSubsystem;
	var->modified = true;
	var->OnChange = NULL;

	// link the variable in
	var->next = cvar_vars;
	cvar_vars = var;
	var->hashNext = cvarHash[hash];
	cvarHash[hash] = var;

	return var;
}

/*
============
Cvar_Set2
============
*/
static void Cvar_SetByVar(cvar_t *var, const char *value, qboolean force)
{
	char	*oldValue;

	if (!value)
		value = var->resetString;

	if (!strcmp(value,var->string))
	{	// not changed
		if (var->latched_string) {
			Z_Free (var->latched_string);
			var->latched_string = NULL;
		}
		return;
	}

	if (var->flags & CVAR_INFOMASK)
	{
		if (!Cvar_InfoValidate(value)) {
			Com_Printf("Invalid info cvar value.\n");
			return;
		}
	}

	if (!force)
	{
		if (var->flags & CVAR_ROM) {
			Com_Printf ("%s is read only.\n", var->name);
			return;
		}
		if (var->flags & CVAR_NOSET) {
			Com_Printf ("%s is write protected.\n", var->name);
			return;
		}
		if (var->flags & (CVAR_LATCH|CVAR_LATCHED))
		{
			if (var->latched_string)
			{
				if (!strcmp(value, var->latched_string))
					return;
				Z_Free (var->latched_string);
				var->latched_string = NULL;
			}

			if(var->flags & CVAR_LATCH) {
				if (Com_ServerState()) {
					Com_Printf ("%s will be changed for next game.\n", var->name);
					var->latched_string = Cvar_CopyString(value, TAG_CVAR);
					return;
				}
			} else {
				char *subsystemName;
				int	 subsystem;

				subsystem = var->flags & CVAR_SYSTEM_MASK;
				if(subsystem & silentSubsystem) {
					var->latched_string = Cvar_CopyString(value, TAG_CVAR);
					return;
				}

				switch (subsystem) {
				case CVAR_SYSTEM_GENERIC:
					subsystemName = "upon restarting desired subsystem";
					break;
				case CVAR_SYSTEM_VIDEO:
					subsystemName = "upon vid_restart";
					break;
				case CVAR_SYSTEM_SOUND:
					subsystemName = "upon snd_restart";
					break;
				case CVAR_SYSTEM_INPUT:
					subsystemName = "upon restarting input subsystem";
					break;
				case CVAR_SYSTEM_NET:
					subsystemName = "upon restarting network subsystem";
					break;
				case CVAR_SYSTEM_FILES:
					if (!strcmp(var->name, "game") ) {
						if(CL_IsDisconnected()) {
							Z_Free(var->string);
							var->string = Cvar_CopyString(value, TAG_CVAR);
							var->value = (float)atof(var->string);
							var->integer = Q_rint(var->value);
							CL_RestartFilesystem(true);
							return;
						}
						subsystemName = "for next game";
					} else {
						subsystemName = "upon restarting filesystem";
					}
					break;
				default:
					subsystemName = "upon restarting unknown";
					break;
				}
				Com_Printf ("%s will be changed %s.\n", var->name, subsystemName);
				var->latched_string = Cvar_CopyString(value, TAG_CVAR);
				return;
			}
		}

		if ((var->flags & CVAR_CHEAT) && !CL_CheatsOK()) {
			Com_Printf ("%s is cheat protected.\n", var->name);
			return;
		}
	}

	if (var->latched_string) {
		Z_Free (var->latched_string);
		var->latched_string = NULL;
	}

	var->modified = true;

	oldValue = var->string;
	
	var->string = Cvar_CopyString(value, TAG_CVAR);
	var->value = (float)atof(var->string);
	var->integer = Q_rint(var->value);

	if(!force && var->OnChange)
		var->OnChange(var, oldValue);

	if (var->flags & CVAR_USERINFO)
		userinfo_modified = true;	// transmit at next oportunity

	Z_Free (oldValue);	// free the old value string
}

static cvar_t *Cvar_SetEx (const char *var_name, const char *value, qboolean force)
{
	cvar_t	*var;

	var = Cvar_FindVar (var_name);
	if (!var)
	{	// create it
		return Cvar_Get (var_name, value, (userCreated) ? CVAR_USER_CREATED : 0);
	}

	Cvar_SetByVar( var, value, force );

	return var;
}
/*
============
Cvar_ForceSet
============
*/
cvar_t *Cvar_SetLatched (const char *var_name, const char *value)
{
	return Cvar_SetEx(var_name, value, false);
}

/*
============
Cvar_Set
============
*/
cvar_t *Cvar_Set (const char *var_name, const char *value)
{
	return Cvar_SetEx(var_name, value, true);
}

/*
============
Cvar_FullSet
============
*/
cvar_t *Cvar_FullSet (const char *var_name, const char *value, int flags)
{
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
	{	// create it
		return Cvar_Get (var_name, value, (userCreated) ? (flags|CVAR_USER_CREATED) : flags);
	}

	if (userCreated)
		Cvar_SetByVar( var, value, false );
	else
		Cvar_SetByVar( var, value, true );

	if (( var->flags | flags ) & CVAR_USERINFO)
		userinfo_modified = true;	// transmit at next oportunity

	var->flags &= ~CVAR_INFOMASK;
	var->flags |= flags;

	return var;
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue (const char *var_name, float value)
{
	char	val[32];

	if (value == (int)value)
		Com_sprintf(val, sizeof(val), "%i", (int)value);
	else
		Com_sprintf(val, sizeof(val), "%g", value);

	Cvar_SetEx(var_name, val, true);
}


void Cvar_SetDefault (const char *var_name)
{ 
	cvar_t	*var;

	var = Cvar_FindVar(var_name);
	if (var) {
		Cvar_SetByVar(var, var->resetString, true);
	}
}

/*
============
Cvar_SetCheatState

Any testing variables will be reset to the safe values
============
*/
void Cvar_SetCheatState( void ) {
	cvar_t	*var;

	if(CL_CheatsOK())
		return;

	// set all default vars to the safe value
	for ( var = cvar_vars; var; var = var->next ) {
		if (var->flags & CVAR_CHEAT)
			Cvar_SetByVar( var, var->resetString, true );
	}
}

/*
============
Cvar_GetLatchedVars

Any variables with latched values will now be updated
============
*/
void Cvar_GetLatchedVars (int flags)
{
	cvar_t	*var;

	for (var = cvar_vars ; var ; var = var->next)
	{
		if (!(var->flags & flags))
			continue;

		if (!var->latched_string)
			continue;

		Z_Free (var->string);
		var->string = var->latched_string;
		var->latched_string = NULL;
		var->value = (float)atof(var->string);
		var->integer = Q_rint(var->value);
		var->modified = true;
	}
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean Cvar_Command (const char *name, unsigned int hash)
{
	cvar_t			*v;

// check variables
	for (v = cvarHash[hash & (CVAR_HASH_SIZE-1)]; v; v = v->hashNext)
	{
		if (Q_stricmp(name, v->name))
			continue;
		
	// perform a variable print or set
		if (Cmd_Argc() == 1) {
			if (v->flags & (CVAR_ROM|CVAR_USER_CREATED))
				Com_Printf (S_ENABLE_COLOR "\"%s%s%s\" is \"%s%s%s\"\n", S_COLOR_CYAN, v->name, S_COLOR_WHITE, S_COLOR_CYAN, v->string, S_COLOR_WHITE);
			else
				Com_Printf (S_ENABLE_COLOR "\"%s%s%s\" is \"%s%s%s\" default: \"%s%s%s\"\n", S_COLOR_CYAN, v->name, S_COLOR_WHITE, S_COLOR_CYAN, v->string, S_COLOR_WHITE, S_COLOR_CYAN, v->resetString, S_COLOR_WHITE);

			if (v->latched_string)
				Com_Printf (S_ENABLE_COLOR "latched: \"%s%s%s\"\n", S_COLOR_CYAN, v->latched_string, S_COLOR_WHITE);

			return true;
		}

		Cvar_SetByVar(v, Cmd_Argv(1), false);
		return true;
	}

	return false;
}


/*
============
Cvar_Set_f

Allows setting and defining of arbitrary cvars from console
============
*/
static void Cvar_Set_f (void)
{
	int		c, flags;
	//cvar_t	*var;

	c = Cmd_Argc();
	if (c != 3 && c != 4) {
		Com_Printf ("usage: set <variable> <value> [u / s]\n");
		return;
	}

	if (c == 4)
	{
		if (!strcmp(Cmd_Argv(3), "u"))
			flags = CVAR_USERINFO;
		else if (!strcmp(Cmd_Argv(3), "s"))
			flags = CVAR_SERVERINFO;
		else
		{
			Com_Printf ("flags can only be 'u' or 's'\n");
			return;
		}
		userCreated = true;
		Cvar_FullSet (Cmd_Argv(1), Cmd_Argv(2), flags);
	}
	else
	{
		userCreated = true;
		Cvar_SetEx(Cmd_Argv(1), Cmd_Argv(2), false);
	}
	userCreated = false;
}

static void Cvar_Reset_f (void)
{
	cvar_t *var;

	if (Cmd_Argc() != 2) {
		Com_Printf ("Usage: %s <cvar>\n", Cmd_Argv(0));
		return;
	}

	var = Cvar_FindVar(Cmd_Argv(1)); 
	if (!var) { 
		Com_Printf("Cvar %s does not exist.\n", Cmd_Argv(1)); 
		return; 
	} 
	Cvar_SetByVar(var, var->resetString, false);
}

/*
=============
Cvar_Toggle_f
Toggles the given variable's value between 0 and 1
=============
*/
static void Cvar_Toggle_f (void) 
{ 
	cvar_t *var;
	int i;

	if (Cmd_Argc() < 2) { 
		Com_Printf("Usage: %s <cvar> [option1 option2 option3 ...]\n", Cmd_Argv(0)); 
		return; 
	}

	var = Cvar_FindVar(Cmd_Argv(1)); 
	if (!var) { 
		Com_Printf("Cvar %s does not exist.\n", Cmd_Argv(1)); 
		return; 
	} 

	if (Cmd_Argc() == 2)
	{
		if(!strcmp(var->string, "0"))
			Cvar_SetByVar(var, "1", false);
		else if(!strcmp(var->string, "1"))
			Cvar_SetByVar(var, "0", false);
		else
			Com_Printf("\"%s\" is \"%s\", can't toggle\n", var->name, var->string);
		return;
	}

	for (i = 2; i < Cmd_Argc()-1; i++)
	{
		if (!strcmp(var->string, Cmd_Argv(i))) {
			Cvar_SetByVar(var, Cmd_Argv(i+1), false);
			return;
		}
	}
	Cvar_SetByVar(var, Cmd_Argv(2), false);
}

static void Cvar_Increase_f (void)
{
	cvar_t	*var;
	char	val[32];
	float	value;

	if (Cmd_Argc() < 2) {
		Com_Printf ("Usage: %s <cvar> [value]\n", Cmd_Argv(0));
		return;
	}

	var = Cvar_FindVar(Cmd_Argv(1));
	if (!var) {
		Com_Printf ("Unknown cvar '%s'\n", Cmd_Argv(1));
		return;
	}
	
	if(!Q_IsNumeric(var->string)) {
		Com_Printf( "\"%s\" is \"%s\", can't increment\n", var->name, var->string );
		return;
	}

	if(Cmd_Argc() > 2) {
		if(!Q_IsNumeric(Cmd_Argv(2))) {
			Com_Printf( "\"%s\" is not numeric\n", Cmd_Argv(2));
			return;
		}
		value = (float)atof(Cmd_Argv(2));
	}
	else {
		value = 1.0f;
	}

	if(!Q_stricmp(Cmd_Argv(0), "dec"))
		value = var->value - value;
	else
		value = var->value + value;

	if (value == (int)value)
		Com_sprintf (val, sizeof(val), "%i", (int)value);
	else
		Com_sprintf (val, sizeof(val), "%g", value);

	Cvar_SetByVar(var, val, false);
}

static void Cvar_Random_f (void)
{
	cvar_t	*var;
	char	val[32];
	int		from, to;

	if (Cmd_Argc() != 4) {
		Com_Printf ("Usage: %s <cvar> <from> <to>\n", Cmd_Argv(0));
		return;
	}

	var = Cvar_FindVar(Cmd_Argv(1));
	if (!var) {
		Com_Printf ("Unknown cvar '%s'\n", Cmd_Argv(1));
		return;
	}
	
	if(!Q_IsNumeric(Cmd_Argv(2))) {
		Com_Printf( "\"%s\" is not numeric\n", Cmd_Argv(2));
		return;
	}

	if(!Q_IsNumeric(Cmd_Argv(3))) {
		Com_Printf( "\"%s\" is not numeric\n", Cmd_Argv(3));
		return;
	}

	from = atoi(Cmd_Argv(2));
	to = atoi(Cmd_Argv(3));

	Com_sprintf (val, sizeof(val), "%i", (int)brandom(from, to));

	Cvar_SetByVar(var, val, false);
}

/*
============
Cvar_WriteVariables

Appends lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/
void Cvar_WriteVariables (FILE *f)
{
	const cvar_t	*var;
	char	buffer[1024];

	for (var = cvar_vars ; var ; var = var->next)
	{
		if (var->flags & CVAR_ARCHIVE)
		{
			if((var->flags & CVAR_LATCHED) && var->latched_string)
				Com_sprintf (buffer, sizeof(buffer), "set %s \"%s\"\n", var->name, var->latched_string);
			else
				Com_sprintf (buffer, sizeof(buffer), "set %s \"%s\"\n", var->name, var->string);
			fprintf (f, "%s", buffer);
		}
	}
}

/*
============
Cvar_List_f

============
*/
static int CvarSort( const cvar_t **a, const cvar_t **b )
{
	return strcmp ((*a)->name, (*b)->name);
}

static void Cvar_List_f (void)
{
    cvar_t  *var;
    const char    *filter = NULL;
	int		i, total = 0, matching = 0;
	cvar_t	**sortedList;
	char	buffer[6];

    if (Cmd_Argc() > 1)
	    filter = Cmd_Argv(1);

	for (var = cvar_vars; var ; var = var->next, total++) {
		if (filter && !Com_WildCmp(filter, var->name) && !strstr(var->name, filter))
			continue;
		matching++;
	}

	if(!matching) {
		Com_Printf("%i cvars found (%i total cvars)\n", matching, total);
		return;
	}

	sortedList = Z_TagMalloc (matching * sizeof(cvar_t *), TAG_CVAR);

	for (matching = 0, var = cvar_vars; var ; var = var->next) {
		if (filter && !Com_WildCmp(filter, var->name) && !strstr(var->name, filter))
			continue;
		sortedList[matching++] = var;
	}

	qsort (sortedList, matching, sizeof(sortedList[0]), (int (*)(const void *, const void *))CvarSort);
	buffer[sizeof( buffer ) - 1] = 0;

	for (i = 0; i < matching; i++)
	{
		var = sortedList[i];

		memset( buffer, ' ', sizeof( buffer ) - 1 );

		if (var->flags & CVAR_CHEAT)
			buffer[0] = 'C';

		if (var->flags & CVAR_ARCHIVE)
			buffer[1] = 'A';

		if (var->flags & CVAR_USERINFO)
			buffer[2] = 'U';

		if (var->flags & CVAR_SERVERINFO)
			buffer[3] = 'S';

		if (var->flags & CVAR_ROM)
			buffer[4] = 'R';
		else if (var->flags & CVAR_NOSET)
			buffer[4] = 'N';
		else if (var->flags & (CVAR_LATCH|CVAR_LATCHED))
			buffer[4] = 'L';
		else if (var->flags & CVAR_USER_CREATED)
			buffer[4] = '?';

		Com_Printf("%s %s \"%s\"\n", buffer, var->name, var->string);
    }

	if (filter)
		Com_Printf("%i cvars found (%i total cvars)\n", matching, total);
	else
		Com_Printf("%i cvars\n", matching);

	Z_Free (sortedList);
}

qboolean userinfo_modified;


static char	*Cvar_BitInfo (int bit)
{
	static char	info[MAX_INFO_STRING];
	const cvar_t	*var;

	info[0] = 0;

	for (var = cvar_vars ; var ; var = var->next)
	{
		if (var->flags & bit)
			Info_SetValueForKey (info, var->name, var->string);
	}
	return info;
}

// returns an info string containing all the CVAR_USERINFO cvars
char	*Cvar_Userinfo (void)
{
	return Cvar_BitInfo (CVAR_USERINFO);
}

// returns an info string containing all the CVAR_SERVERINFO cvars
char	*Cvar_Serverinfo (void)
{
	return Cvar_BitInfo (CVAR_SERVERINFO);
}

/*
============
Cvar_Init

Reads in all archived cvars
============
*/
void Cvar_Init (void)
{
	Cmd_AddCommand ("set", Cvar_Set_f);
	Cmd_AddCommand ("toggle", Cvar_Toggle_f);
	Cmd_AddCommand ("inc", Cvar_Increase_f);
	Cmd_AddCommand ("dec", Cvar_Increase_f);
	Cmd_AddCommand ("cvarlist", Cvar_List_f);
	Cmd_AddCommand ("random", Cvar_Random_f);
	Cmd_AddCommand ("reset",	Cvar_Reset_f);
}

