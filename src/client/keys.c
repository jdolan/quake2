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

/*

key up events are sent even if in console mode

*/

int			anykeydown;

qkey_t		keys[MAX_KEYS];

static qboolean	key_overstrikeMode;

typedef struct
{
	const char	*name;
	int		keynum;
} keyname_t;

static keyname_t keynames[] =
{
	{"TAB", K_TAB},
	{"ENTER", K_ENTER},
	{"ESCAPE", K_ESCAPE},
	{"SPACE", K_SPACE},
	{"BACKSPACE", K_BACKSPACE},
	{"UPARROW", K_UPARROW},
	{"DOWNARROW", K_DOWNARROW},
	{"LEFTARROW", K_LEFTARROW},
	{"RIGHTARROW", K_RIGHTARROW},

	{"ALT", K_ALT},
	{"CTRL", K_CTRL},
	{"SHIFT", K_SHIFT},

	{"CAPSLOCK",		K_CAPSLOCK },
	
	{"F1", K_F1},
	{"F2", K_F2},
	{"F3", K_F3},
	{"F4", K_F4},
	{"F5", K_F5},
	{"F6", K_F6},
	{"F7", K_F7},
	{"F8", K_F8},
	{"F9", K_F9},
	{"F10", K_F10},
	{"F11", K_F11},
	{"F12", K_F12},

	{"INS", K_INS},
	{"DEL", K_DEL},
	{"PGDN", K_PGDN},
	{"PGUP", K_PGUP},
	{"HOME", K_HOME},
	{"END", K_END},

	{"MOUSE1", K_MOUSE1},
	{"MOUSE2", K_MOUSE2},
	{"MOUSE3", K_MOUSE3},
	{"MOUSE4", K_MOUSE4},
	{"MOUSE5", K_MOUSE5},
	{"MOUSE6", K_MOUSE6},
	{"MOUSE7", K_MOUSE7},
	{"MOUSE8", K_MOUSE8},

#ifdef JOYSTICK
	{"JOY1", K_JOY1},
	{"JOY2", K_JOY2},
	{"JOY3", K_JOY3},
	{"JOY4", K_JOY4},

	{"AUX1", K_AUX1},
	{"AUX2", K_AUX2},
	{"AUX3", K_AUX3},
	{"AUX4", K_AUX4},
	{"AUX5", K_AUX5},
	{"AUX6", K_AUX6},
	{"AUX7", K_AUX7},
	{"AUX8", K_AUX8},
	{"AUX9", K_AUX9},
	{"AUX10", K_AUX10},
	{"AUX11", K_AUX11},
	{"AUX12", K_AUX12},
	{"AUX13", K_AUX13},
	{"AUX14", K_AUX14},
	{"AUX15", K_AUX15},
	{"AUX16", K_AUX16},
	{"AUX17", K_AUX17},
	{"AUX18", K_AUX18},
	{"AUX19", K_AUX19},
	{"AUX20", K_AUX20},
	{"AUX21", K_AUX21},
	{"AUX22", K_AUX22},
	{"AUX23", K_AUX23},
	{"AUX24", K_AUX24},
	{"AUX25", K_AUX25},
	{"AUX26", K_AUX26},
	{"AUX27", K_AUX27},
	{"AUX28", K_AUX28},
	{"AUX29", K_AUX29},
	{"AUX30", K_AUX30},
	{"AUX31", K_AUX31},
	{"AUX32", K_AUX32},
#endif

	{"KP_HOME",			K_KP_HOME },
	{"KP_UPARROW",		K_KP_UPARROW },
	{"KP_PGUP",			K_KP_PGUP },
	{"KP_LEFTARROW",	K_KP_LEFTARROW },
	{"KP_5",			K_KP_5 },
	{"KP_RIGHTARROW",	K_KP_RIGHTARROW },
	{"KP_END",			K_KP_END },
	{"KP_DOWNARROW",	K_KP_DOWNARROW },
	{"KP_PGDN",			K_KP_PGDN },
	{"KP_ENTER",		K_KP_ENTER },
	{"KP_INS",			K_KP_INS },
	{"KP_DEL",			K_KP_DEL },
	{"KP_SLASH",		K_KP_SLASH },
	{"KP_MINUS",		K_KP_MINUS },
	{"KP_PLUS",			K_KP_PLUS },

	{"MWHEELUP", K_MWHEELUP },
	{"MWHEELDOWN", K_MWHEELDOWN },

	{"PAUSE", K_PAUSE},

	{"SEMICOLON", ';'},	// because a raw semicolon seperates commands

	{NULL,0}
};

/*
===================
Key_GetOverstrikeMode
===================
*/
qboolean Key_GetOverstrikeMode( void ) {
	return key_overstrikeMode;
}

/*
===================
Key_SetOverstrikeMode
===================
*/
void Key_SetOverstrikeMode( qboolean state ) {
	key_overstrikeMode = state;
}

/*
===================
Key_IsDown
===================
*/
qboolean Key_IsDown( int key )
{
	if( (unsigned)key >= MAX_KEYS )
		return false;

	return keys[key].down;
}

/*
===================
Key_StringToKeynum

Returns a key number to be used to index keybindings[] by looking at
the given string.  Single ascii characters return themselves, while
the K_* names are matched up.

0x11 will be interpreted as raw hex, which will allow new controlers
to be configured even if they don't have defined names.
===================
*/
static int Key_StringToKeynum (const char *str)
{
	keyname_t	*kn;
	
	if (!str || !str[0])
		return -1;
	if (!str[1])
		return (int)(unsigned char)str[0];

	for (kn=keynames ; kn->name ; kn++)
	{
		if (!Q_stricmp(str,kn->name))
			return kn->keynum;
	}

	// check for hex code
	if ( str[0] == '0' && str[1] == 'x' && strlen( str ) == 4) {
		int		n1, n2;
		
		n1 = str[2];
		if ( n1 >= '0' && n1 <= '9' ) {
			n1 -= '0';
		} else if ( n1 >= 'a' && n1 <= 'f' ) {
			n1 = n1 - 'a' + 10;
		} else {
			n1 = 0;
		}

		n2 = str[3];
		if ( n2 >= '0' && n2 <= '9' ) {
			n2 -= '0';
		} else if ( n2 >= 'a' && n2 <= 'f' ) {
			n2 = n2 - 'a' + 10;
		} else {
			n2 = 0;
		}

		return n1 * 16 + n2;
	}
	return -1;
}

/*
===================
Key_KeynumToString

Returns a string (either a single ascii char, or a K_* name) for the
given keynum.
FIXME: handle quote special (general escape sequence?)
===================
*/
const char *Key_KeynumToString (int keynum)
{
	keyname_t	*kn;	
	static	char	tinystr[5];
	int			i, j;
	
	if (keynum == -1)
		return "<KEY NOT FOUND>";
	
	if ( (unsigned)keynum >= MAX_KEYS )
		return "<OUT OF RANGE>";

	// check for printable ascii (don't use quote)
	if ( keynum > 32 && keynum < 127 && keynum != '"' && keynum != ';' ) {
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}

	for (kn=keynames ; kn->name ; kn++) {
		if (keynum == kn->keynum)
			return kn->name;
	}

	// make a hex string
	i = keynum >> 4;
	j = keynum & 15;

	tinystr[0] = '0';
	tinystr[1] = 'x';
	tinystr[2] = i > 9 ? i - 10 + 'a' : i + '0';
	tinystr[3] = j > 9 ? j - 10 + 'a' : j + '0';
	tinystr[4] = 0;

	return tinystr;
}


/*
===================
Key_SetBinding
===================
*/
void Key_SetBinding (int keynum, const char *binding)
{
	if( (unsigned)keynum >= MAX_KEYS )
		return;

	// free old bindings
	if (keys[keynum].binding) {
		Z_Free(keys[keynum].binding);
	}
	// allocate memory for new binding
	keys[keynum].binding = CopyString(binding, TAG_CL_KEYBIND);
}

/*
===================
Key_Unbind_f
===================
*/
static void Key_Unbind_f (void)
{
	int		b;

	if (Cmd_Argc() != 2) {
		Com_Printf ("unbind <key> : remove commands from a key\n");
		return;
	}

	b = Key_StringToKeynum (Cmd_Argv(1));
	if (b == -1) {
		Com_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	Key_SetBinding (b, "");
}

static void Key_Unbindall_f (void)
{
	int	i;

	for (i=0 ; i<MAX_KEYS ; i++) {
		if (keys[i].binding)
			Key_SetBinding (i, "");
	}
}


/*
===================
Key_Bind_f
===================
*/
static void Key_Bind_f (void)
{
	int			c, b;
	
	c = Cmd_Argc();

	if (c < 2) {
		Com_Printf ("bind <key> [command] : attach a command to a key\n");
		return;
	}
	b = Key_StringToKeynum (Cmd_Argv(1));
	if (b == -1) {
		Com_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	if (c == 2) {
		if (keys[b].binding)
			Com_Printf ("\"%s\" = \"%s\"\n", Cmd_Argv(1), keys[b].binding );
		else
			Com_Printf ("\"%s\" is not bound\n", Cmd_Argv(1) );
		return;
	}
	
	Key_SetBinding (b, Cmd_ArgsFrom(2));
}

/*
============
Key_WriteBindings

Writes lines containing "bind key value"
============
*/
void Key_WriteBindings (FILE *f)
{
	int		i;

	for (i = 0; i < MAX_KEYS; i++) {
		if (keys[i].binding && keys[i].binding[0])
			fprintf (f, "bind %s \"%s\"\n", Key_KeynumToString(i), keys[i].binding);
	}
}


/*
============
Key_Bindlist_f

============
*/
static void Key_Bindlist_f (void)
{
	int		i;

	for (i = 0; i < MAX_KEYS; i++) {
		if (keys[i].binding && keys[i].binding[0])
			Com_Printf ("%s \"%s\"\n", Key_KeynumToString(i), keys[i].binding);
	}
}


/*
===================
Key_Init
===================
*/
void Key_Init (void)
{
	int		i;
	
// init ascii characters in console mode
	for (i = 32; i < 128; i++)
		keys[i].consolekey = true;
	keys[K_ENTER].consolekey = true;
	keys[K_KP_ENTER].consolekey = true;
	keys[K_TAB].consolekey = true;
	keys[K_LEFTARROW].consolekey = true;
	keys[K_KP_LEFTARROW].consolekey = true;
	keys[K_RIGHTARROW].consolekey = true;
	keys[K_KP_RIGHTARROW].consolekey = true;
	keys[K_UPARROW].consolekey = true;
	keys[K_KP_UPARROW].consolekey = true;
	keys[K_DOWNARROW].consolekey = true;
	keys[K_KP_DOWNARROW].consolekey = true;
	keys[K_BACKSPACE].consolekey = true;
	keys[K_HOME].consolekey = true;
	keys[K_KP_HOME].consolekey = true;
	keys[K_END].consolekey = true;
	keys[K_KP_END].consolekey = true;
	keys[K_PGUP].consolekey = true;
	keys[K_KP_PGUP].consolekey = true;
	keys[K_PGDN].consolekey = true;
	keys[K_KP_PGDN].consolekey = true;
	keys[K_SHIFT].consolekey = true;
	keys[K_INS].consolekey = true;
	keys[K_KP_INS].consolekey = true;
	keys[K_KP_DEL].consolekey = true;
	keys[K_KP_SLASH].consolekey = true;
	keys[K_KP_PLUS].consolekey = true;
	keys[K_KP_MINUS].consolekey = true;
	keys[K_KP_5].consolekey = true;

	keys[K_MWHEELUP].consolekey = true;
	keys[K_MWHEELDOWN].consolekey = true;
	keys[K_DEL].consolekey = true;

	keys[K_CTRL].consolekey = true;

	keys['`'].consolekey = false;
	keys['~'].consolekey = false;

	for (i = 0; i < 256; i++)
		keys[i].keyshift = i;
	for (i = 'a'; i <= 'z'; i++)
		keys[i].keyshift = i - 'a' + 'A';
	keys['1'].keyshift = '!';
	keys['2'].keyshift = '@';
	keys['3'].keyshift = '#';
	keys['4'].keyshift = '$';
	keys['5'].keyshift = '%';
	keys['6'].keyshift = '^';
	keys['7'].keyshift = '&';
	keys['8'].keyshift = '*';
	keys['9'].keyshift = '(';
	keys['0'].keyshift = ')';
	keys['-'].keyshift = '_';
	keys['='].keyshift = '+';
	keys[','].keyshift = '<';
	keys['.'].keyshift = '>';
	keys['/'].keyshift = '?';
	keys[';'].keyshift = ':';
	keys['\''].keyshift = '"';
	keys['['].keyshift = '{';
	keys[']'].keyshift = '}';
	keys['`'].keyshift = '~';
	keys['\\'].keyshift = '|';

	keys[K_ESCAPE].menubound = true;
	for (i = 0; i < 12; i++)
		keys[K_F1+i].menubound = true;

// register our functions
	Cmd_AddCommand ("bind",Key_Bind_f);
	Cmd_AddCommand ("unbind",Key_Unbind_f);
	Cmd_AddCommand ("unbindall",Key_Unbindall_f);
	Cmd_AddCommand ("bindlist",Key_Bindlist_f);
}

/*
===================
Key_Event

Called by the system between frames for both key up and key down events
Should NOT be called during an interrupt!
===================
*/
void Key_Event (int key, qboolean down, unsigned time)
{
	char	*kb;
	char	cmd[1024];

	// update auto-repeat status
	if (down)
	{
		keys[key].repeats++;
		if (keys[key].repeats > 1)
		{
			if ((cls.key_dest == key_game ||
				(cls.key_dest == key_menu && key != K_UPARROW && key != K_DOWNARROW))
				&& key != K_BACKSPACE && key != K_PAUSE 
				&& key != K_PGUP && key != K_KP_PGUP 
				&& key != K_PGDN && key != K_KP_PGDN)
				return;	// ignore most autorepeats
		}
	}
	else
	{
		keys[key].repeats = 0;
	}

	// console key is hardcoded, so the user can never unbind it
	if (key == '`' || key == '~')
	{
		if (down)
			Con_ToggleConsole_f ();

		return;
	}

	// menu key is hardcoded, so the user can never unbind it
	if (key == K_ESCAPE)
	{
		if (!down)
			return;

		if (cl.frame.playerstate.stats[STAT_LAYOUTS] && cls.key_dest == key_game)
		{	// put away help computer / inventory
			Cbuf_AddText ("cmd putaway\n");
			return;
		}
		switch (cls.key_dest)
		{
		case key_message:
			Key_Message (key);
			break;
		case key_menu:
			M_Keydown (key, true);
			break;
		case key_game:
		case key_console:
			M_Menu_Main_f ();
			break;
		default:
			Com_Error (ERR_FATAL, "Bad cls.key_dest");
		}
		return;
	}

//
// key up events only generate commands if the game key binding is
// a button command (leading + sign).  These will occur even in console mode,
// to keep the character from continuing an action started before a console
// switch.  Button commands include the kenum as a parameter, so multiple
// downs can be matched with ups
//
	// Track if any key is down for BUTTON_ANY
	keys[key].down = down;
	if (down)
	{
		if (keys[key].repeats == 1)
			anykeydown++;
	}
	else
	{
		anykeydown--;
		if (anykeydown < 0)
			anykeydown = 0;

		if(key == K_MOUSE1 && cls.key_dest == key_menu)
			M_Keydown (key, down);

		kb = keys[key].binding;
		if (kb && kb[0] == '+')
		{
			Com_sprintf (cmd, sizeof(cmd), "-%s %i %i\n", kb+1, key, time);
			Cbuf_AddText (cmd);
		}
		if (keys[key].keyshift != key)
		{
			kb = keys[keys[key].keyshift].binding;
			if (kb && kb[0] == '+')
			{
				Com_sprintf (cmd, sizeof(cmd), "-%s %i %i\n", kb+1, key, time);
				Cbuf_AddText (cmd);
			}
		}

		return; // other systems only care about key down events
	}

#if defined(__linux__) || defined(__FreeBSD__)
	if (key == K_ENTER && keys[K_ALT].down)
	{
		Key_ClearStates();
		if (Cvar_VariableIntValue("vid_fullscreen") == 0)
		{
			Com_Printf("Switching to fullscreen rendering\n");
			Cvar_Set("vid_fullscreen", "1");
		}
		else
		{
			Com_Printf("Switching to windowed rendering\n");
			Cvar_Set("vid_fullscreen", "0");
		}
		//Cbuf_ExecuteText( EXEC_APPEND, "vid_restart\n");
		return;
	}
#endif
//
// if not a consolekey, send to the interpreter no matter what mode is
//
	if ( (cls.key_dest == key_menu && keys[key].menubound)
	|| (cls.key_dest == key_console && !keys[key].consolekey)
	|| (cls.key_dest == key_game && ( cls.state == ca_active || !keys[key].consolekey ) ) )
	{
		kb = keys[key].binding;
		if (kb)
		{
			if (kb[0] == '+')
			{	// button commands add keynum and time as a parm
				Com_sprintf (cmd, sizeof(cmd), "%s %i %i\n", kb, key, time);
				Cbuf_AddText (cmd);
			}
			else
			{
				Cbuf_AddText (kb);
				Cbuf_AddText ("\n");
			}
		}
		return;
	}

	if (keys[K_SHIFT].down)
		key = keys[key].keyshift;

	switch (cls.key_dest)
	{
	case key_message:
		Key_Message (key);
		break;
	case key_menu:
		M_Keydown (key, true);
		break;

	case key_game:
	case key_console:
		Key_Console (key);
		break;
	default:
		Com_Error (ERR_FATAL, "Bad cls.key_dest");
	}
}

/*
===================
Key_ClearStates
===================
*/
void Key_ClearStates (void)
{
	int		i;

	anykeydown = 0;

	for (i = 0; i < MAX_KEYS; i++)
	{
		//if ( keys[i].down || keys[i].repeats )
		if ( keys[i].down )
			Key_Event( i, false, 0 );
		keys[i].down = false;
		keys[i].repeats = 0;
	}
}

