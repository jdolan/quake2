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
// console.c

#include "client.h"

#define		MAX_FIELD_TEXT	256
#define		HISTORY_LINES	32
#define		HISTORY_MASK	(HISTORY_LINES - 1)

typedef struct
{
	char	text[HISTORY_LINES][MAX_FIELD_TEXT];
	int		cursorPos;
	int		editLine;
	int		historyLine;
} inputField_t;

#define	CON_TIMES		8
#define CON_TIMES_MASK	(CON_TIMES - 1)
#define	CON_TEXTSIZE	65536

typedef struct console_s
{
	qboolean	initialized;

	short 	text[CON_TEXTSIZE];
	int		current;		// line where next message will be printed
	int		x;				// offset in current line for next print
	int		display;		// bottom of console displays this line

	//int		ormask;			// high bit mask for colored characters

	int 	linewidth;		// characters across screen
	int		totallines;		// total lines in console scrollback

	float	cursorspeed;

	int		vislines;

	unsigned int		times[CON_TIMES];	// cls.realtime time the line was generated
								// for transparent notify lines
	qboolean	skipNotify;
} console_t;

static console_t	con;

static cvar_t		*con_notifytime;
cvar_t		*con_notifylines;	//Notifylines
cvar_t		*con_notifyfade;	//Notifyfade
cvar_t		*con_alpha;			//transparent console
cvar_t		*con_scrlines;
static cvar_t		*con_mwheel;		//disable mwheel scrolling
static cvar_t		*con_cmdcomplete;

static inputField_t con_inputLines;
static inputField_t chat_inputLines;

static qboolean	chat_team = false;

#ifndef GL_QUAKE
void DrawString (int x, int y, const char *s)
{
	while (*s)
	{
		Draw_Char (x, y, *s, COLOR_WHITE, 1);
		x+=8;
		s++;
	}
}

void DrawAltString (int x, int y, const char *s)
{
	while (*s)
	{
		Draw_Char (x, y, *s ^ 0x80, COLOR_WHITE, 1);
		x+=8;
		s++;
	}
}

void DrawString2 (int x, int y, const short *s, float alpha)
{
	while (*s)
	{
		Draw_Char (x, y, *s & 0xff, COLOR_WHITE, alpha);
		x+=8;
		s++;
	}
}
#endif

void Draw_StringLen (int x, int y, short *str, int len, float alpha)
{
	short saved_byte;

	if (len < 0) {
		DrawString2 (x, y, str, alpha);
		return;
	}

	saved_byte = str[len];
	str[len] = 0;
	DrawString2 (x, y, str, alpha);
	str[len] = saved_byte;
}

void Key_ClearTyping (void)
{
	memset(con_inputLines.text[con_inputLines.editLine], 0, sizeof(con_inputLines.text[con_inputLines.editLine])); // clear any typing
	con_inputLines.cursorPos = 0;
	con_inputLines.historyLine = con_inputLines.editLine;
}

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void)
{
	SCR_EndLoadingPlaque ();	// get rid of loading plaque


	/*if (cl.attractloop)
	{
		Cbuf_AddText ("killserver\n");
		return;
	}*/

	if (cls.state == ca_disconnected)
	{	// start the demo loop again
		//Cbuf_AddText ("d1\n");
		cls.key_dest = key_console;
		return;
	}

	Key_ClearTyping ();
	Con_ClearNotify ();

	if (cls.key_dest == key_console)
	{
		M_ForceMenuOff ();
		if(cl_paused->integer)
			Cvar_Set ("paused", "0");
	}
	else
	{
		M_ForceMenuOff ();
		cls.key_dest = key_console;	

		if (Cvar_VariableIntValue ("maxclients") == 1 && Com_ServerState () && !cl.attractloop && !cl_paused->integer)
			Cvar_Set ("paused", "1");
	}
}

/*
================
Con_ToggleChat_f
================
*/
void Con_ToggleChat_f (void)
{
	Key_ClearTyping ();

	if (cls.key_dest == key_console)
	{
		if (cls.state == ca_active)
		{
			M_ForceMenuOff ();
			cls.key_dest = key_game;
		}
	}
	else
		cls.key_dest = key_console;
	
	Con_ClearNotify ();
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void)
{
	int		i;

	for ( i = 0 ; i < CON_TEXTSIZE ; i++ ) {
		con.text[i] = (COLOR_WHITE<<8) | ' ';
	}
}

						
/*
================
Con_Dump_f

Save the console contents out to a file
================
*/
static void Con_Dump_f (void)
{
	int		l, x, i;
	short	*line;
	FILE	*f;
	char	buffer[1024], name[MAX_OSPATH], *s;

	if (Cmd_Argc() != 2) {
		Com_Printf ("usage: %s <filename>\n", Cmd_Argv(0));
		return;
	}

	s = Cmd_Argv(1);
	if (strstr(s, "..") || strchr(s, '/') || strchr(s, '\\') ) {
		Com_Printf ("Con_Dump_f: Illegal filename.\n");
		return;
	}

	Com_sprintf (name, sizeof(name), "%s/%s.txt", FS_Gamedir(), s);

	FS_CreatePath (name);
	f = fopen (name, "w");
	if (!f) {
		Com_Printf ("Con_Dump_f: Couldn't open %s for writing.\n", name );
		return;
	}

	// skip empty lines
	for (l = con.current - con.totallines + 1 ; l <= con.current ; l++)
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		for (x=0 ; x<con.linewidth ; x++)
			if ( (line[x] & 0xff) != ' ')
				break;
		if (x != con.linewidth)
			break;
	}

	// write the remaining lines
	buffer[con.linewidth] = 0;
	for ( ; l <= con.current ; l++)
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		for(i=0; i<con.linewidth; i++)
			buffer[i] = line[i] & 0xff;
		for (x=con.linewidth-1 ; x>=0 ; x--)
		{
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}
		for (x = 0; buffer[x]; x++)
			buffer[x] &= 0x7f;

		fprintf (f, "%s\n", buffer);
	}

	fclose (f);

	Com_Printf ("Dumped console text to %s.\n", name);
}

						
/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify (void)
{
	int		i;
	
	for ( i = 0 ; i < CON_TIMES ; i++ ) {
		con.times[i] = 0;
	}
}

						
/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f (void)
{
	if (cls.state != ca_active)
		return;

	chat_team = false;
	cls.key_dest = key_message;
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void)
{
	if (cls.state != ca_active)
		return;

	chat_team = true;
	cls.key_dest = key_message;
}

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	short	tbuf[CON_TEXTSIZE];


	width = (viddef.width >> 3) - 2;

	if (width == con.linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = 38;
		con.linewidth = width;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		for(i=0; i<CON_TEXTSIZE; i++)
			con.text[i] = (COLOR_WHITE<<8) | ' ';
	}
	else
	{
		oldwidth = con.linewidth;
		con.linewidth = width;
		oldtotallines = con.totallines;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		numlines = oldtotallines;

		if (con.totallines < numlines)
			numlines = con.totallines;

		numchars = oldwidth;
	
		if (con.linewidth < numchars)
			numchars = con.linewidth;

		memcpy (tbuf, con.text, CON_TEXTSIZE * sizeof(short));
		for(i=0; i<CON_TEXTSIZE; i++)
			con.text[i] = (COLOR_WHITE<<8) | ' ';

		for (i=0 ; i<numlines ; i++)
		{
			for (j=0 ; j<numchars ; j++)
			{
				con.text[(con.totallines - 1 - i) * con.linewidth + j] =
						tbuf[((con.current - i + oldtotallines) %
							  oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con.current = con.totallines - 1;
	con.display = con.current;
}

static void OnChange_scrlines (cvar_t *self, const char *oldValue)
{
	if (self->integer < 1)
		Cvar_Set(self->name, "1");
}

static void OnChange_notifyLines (cvar_t *self, const char *oldValue)
{
	if(self->integer < 0)
		Cvar_Set(self->name, "0");
	else if (self->integer > CON_TIMES)
		Cvar_SetValue(self->name, CON_TIMES);
}

/*
================
Con_Init
================
*/
void Con_Init (void)
{
	con.linewidth = -1;
//	con.ormask = 0;

	Con_CheckResize ();
	
	Com_Printf ("Console initialized.\n");

//
// register our commands
//
	con_notifytime = Cvar_Get ("con_notifytime", "3", 0);

	con_notifylines = Cvar_Get("con_notifylines","4", CVAR_ARCHIVE); //Notifylines -Maniac
	con_notifyfade = Cvar_Get("con_notifyfade","0", CVAR_ARCHIVE); //Notify fade
	con_scrlines = Cvar_Get("con_scrlines", "2", CVAR_ARCHIVE);
	con_mwheel = Cvar_Get("con_mwheel", "1", 0);
	con_cmdcomplete = Cvar_Get("con_cmdcomplete", "2", 0);
	con_alpha = Cvar_Get ("con_alpha", "0.6", CVAR_ARCHIVE);

	con_notifylines->OnChange = OnChange_notifyLines;
	OnChange_notifyLines(con_notifylines, con_notifylines->resetString);

	con_scrlines->OnChange = OnChange_scrlines;
	OnChange_scrlines(con_scrlines, con_scrlines->resetString);

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("togglechat", Con_ToggleChat_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
	Cmd_AddCommand ("condump", Con_Dump_f);
	con.initialized = true;
}

/*
================
Con_SkipNotify
================
*/
void Con_SkipNotify( qboolean skip ) {
	con.skipNotify = skip;
}

/*
===============
Con_Linefeed
===============
*/
static void Con_Linefeed (void)
{
	int i;

	con.x = 0;
	if (con.display == con.current)
		con.display++;
	con.current++;
	for(i=0; i<con.linewidth; i++)
		con.text[(con.current%con.totallines)*con.linewidth+i] = (COLOR_WHITE<<8) | ' ';

	// mark time for transparent overlay
	if (con.current >= 0)
		con.times[con.current & CON_TIMES_MASK] = (con.skipNotify) ? 0 : cls.realtime;
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void Con_Print (const char *txt)
{
	int		y, c, l;
	static qboolean	cr = false;
	int		mask, color;
	qboolean colors = false;

	if (!con.initialized)
		return;

	color = COLOR_WHITE;
	mask = 0;

	switch (*txt) {
	case 1:
	case 2:
		mask = 128; // go to colored text
		txt++;
		break;
	case COLOR_ONE:
		txt++;
		if(Q_IsColorString( txt )) {
			color = ColorIndex(*(txt+1));
			txt+=2;
		}
		break;
	case COLOR_ENABLE:
		txt++;
		colors = true;
		break;
	}

	while ( (c = *txt) != 0 )
	{
		if ( colors && Q_IsColorString( txt ) ) {
			color = ColorIndex( *(txt+1) );
			txt += 2;
			continue;
		}

		if (con.x) {
			// count word length
			for (l=0 ; l< con.linewidth ; l++) {
				if ( txt[l] <= ' ')
					break;
			}

			// word wrap
			if (l != con.linewidth && (con.x + l > con.linewidth) )
				Con_Linefeed();
		}
		else {
			if (cr) {
				con.current--;
				cr = false;
			}
			Con_Linefeed();
		}

		switch (c)
		{
		case '\r':
			cr = true;
		case '\n':
			color = COLOR_WHITE;
			con.x = 0;
			break;
		default:	// display character and advance
			y = con.current % con.totallines;
			con.text[y*con.linewidth+con.x] = c | mask | (color << 8);
			con.x++;
			if (con.x >= con.linewidth)
				con.x = 0;

			break;
		}
		txt++;
	}
}


/*
==============
Con_CenteredPrint
==============
*/
void Con_CenteredPrint (const char *text)
{
	int		l;
	char	buffer[1024];

	l = strlen(text);
	l = (con.linewidth-l)/2;
	if (l < 0)
		l = 0;
	memset (buffer, ' ', l);
	strcpy (buffer+l, text);
	strcat (buffer, "\n");
	Con_Print (buffer);
}

/*
==============================================================================

DRAWING

==============================================================================
*/
void Draw_Input( const char *text, int x, int y, int curPos )
{
	int cursorPos = curPos;
	int i, len = strlen(text);

	// prestep if horizontally scrolling
	if (cursorPos >= con.linewidth - x/8 + 2)
	{
		cursorPos = con.linewidth - 1 - x/8 + 2;
		text += curPos - cursorPos;
	}

	for( i=0 ; i<con.linewidth && text[i]; i++ )
		Draw_Char( x + (i<<3), y, text[i], COLOR_WHITE, 1);
	
	// add the cursor frame
	if ((int)(cls.realtime>>8)&1)
	{
		if (curPos == len || Key_GetOverstrikeMode())
			Draw_Char ( x+cursorPos*8, y, 11, COLOR_WHITE, 1);
		else
			Draw_Char ( x+cursorPos*8, y+4, '_', COLOR_WHITE, 1);
	}
}
/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
void Con_DrawInput (void)
{
	int		x = 8, y = con.vislines - 22;

	if (cls.key_dest == key_menu)
		return;
	if (cls.key_dest != key_console && cls.state == ca_active)
		return;		// don't draw anything (always draw if not active)

	// draw command prompt
	Draw_Char( x, y, ']', COLOR_WHITE, 1);
	x += 8;

	// draw it
	Draw_Input(con_inputLines.text[con_inputLines.editLine], x, y, con_inputLines.cursorPos);
}

/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
	int		i, v = 0, skip;
	unsigned int time;
	short	*text;
	float	alpha = 1.0f;


	i = con.current - con_notifylines->integer + 1;
	if(i < 0)
		i = 0;
	for (; i <= con.current ; i++)
	{
		time = con.times[i & CON_TIMES_MASK];
		if (time == 0)
			continue;

		if (cls.realtime - time > con_notifytime->value*1000)
			continue;
		text = con.text + (i % con.totallines)*con.linewidth;
		
		if (con_notifyfade->value) {
			alpha = SCR_FadeAlpha( time, con_notifytime->value * 1000, con_notifyfade->value * 1000 );
			if( !alpha )
				continue;
			//alpha = (time + con_notifytime->value*1000 - cls.realtime)*0.001f;
		}

		Draw_StringLen (8, v, text, con.linewidth, alpha);

		v += 8;
	}


	if (cls.key_dest == key_message)
	{
		if (chat_team) {
			DrawString (8, v, "say_team:");
			skip = 11;
		}
		else {
			DrawString (8, v, "say:");
			skip = 6;
		}
		Draw_Input(chat_inputLines.text[chat_inputLines.editLine], skip*8, v, chat_inputLines.cursorPos);
		v += 8;
	}
	
	if (v) {
		SCR_AddDirtyPoint (0,0);
		SCR_AddDirtyPoint (viddef.width-1, v);
	}
}

/*
================
Con_DrawConsole

Draws the console with the solid background
================
*/
void Con_DrawConsole (float frac, qboolean ingame)
{
	int				i, j, x, y, n;
	int				rows, row, lines;
	short			*text;
	char			*text2, dlbar[1024];

	lines = viddef.height * frac;
	if (lines <= 0)
		return;

	if (lines > viddef.height)
		lines = viddef.height;

	// draw the background
	Draw_StretchPic (0, lines-viddef.height, viddef.width, viddef.height, "conback", ingame ? con_alpha->value : 1);
	SCR_AddDirtyPoint (0,0);
	SCR_AddDirtyPoint (viddef.width-1,lines-1);

	text2 = APPLICATION " v" VERSION;
	DrawAltString(viddef.width-4-(strlen(text2)<<3), lines-12, text2);


// draw the text
	con.vislines = lines;
	
	rows = (lines-22)>>3;		// rows of text to draw

	y = lines - 30;

// draw from the bottom up
	if (con.display != con.current)
	{
	// draw arrows to show the buffer is backscrolled
		for (x = 0; x < con.linewidth; x += 4)
			Draw_Char ( (x+1)<<3, y, '^', COLOR_WHITE, 1);
	
		y -= 8;
		rows--;
	}
	
	row = con.display;
	for (i = 0; i < rows; i++, y -= 8, row--)
	{
		if (row < 0)
			break;
		if (con.current - row >= con.totallines)
			break;		// past scrollback wrap point
			
		text = con.text + (row % con.totallines)*con.linewidth;

		Draw_StringLen (8, y, text, con.linewidth, 1);
	}

//ZOID
	// draw the download bar
	// figure out width
	if (cls.downloadname[0] && (cls.download || cls.downloadposition))
	{
		if ((text2 = strrchr(cls.downloadname, '/')) != NULL)
			text2++;
		else
			text2 = cls.downloadname;

		x = con.linewidth - ((con.linewidth * 7) / 40);
		y = x - strlen(text2) - 40;
		i = con.linewidth/3;
		if (strlen(text2) > i) {
			y = x - i - 11;
			strncpy(dlbar, text2, i);
			dlbar[i] = 0;
			strcat(dlbar, "...");
		} else
			strcpy(dlbar, text2);
		strcat(dlbar, ": ");
		i = strlen(dlbar);
		dlbar[i++] = '\x80';

		// where's the dot go?
		if (cls.downloadpercent == 0)
			n = 0;
		else
			n = y * cls.downloadpercent / 100;
			
		for (j = 0; j < y; j++)
			if (j == n)
				dlbar[i++] = '\x83';
			else
				dlbar[i++] = '\x81';
		dlbar[i++] = '\x82';
		dlbar[i] = 0;

		if (cls.download)
			cls.downloadposition = ftell(cls.download);

		sprintf (dlbar + i, " %02d%% (%.02f KB)", cls.downloadpercent, (float)cls.downloadposition / 1024.0);

		// draw it
		DrawString(8, con.vislines-12, dlbar);
	}
//ZOID

// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();
}

/*
==============================================================================

			LINE TYPING INTO THE CONSOLE

==============================================================================
*/

void IF_Init( inputField_t *field )
{
	memset( field->text[field->editLine], 0, sizeof(field->text[field->editLine]) );
	field->cursorPos = 0;
}

typedef struct compMatches_s
{
	const char *name;
	const char *value;
} compMatches_t;

#define MAX_MATCHES 1024
static compMatches_t compMatches[MAX_MATCHES];
static char shortestMatch[MAX_TOKEN_CHARS];
static int matchCount;
static char *completionString;

#define LIST_CVARS    1
#define LIST_COMMANDS 2
#define LIST_ALIASES  4

static int MatchShort (const compMatches_t *a, const compMatches_t *b)
{
	return strcmp(a->name, b->name);
}

static void AddMatches (const char *name, const char *value)
{
	int i;

	if(matchCount >= MAX_MATCHES)
		return;

	compMatches[matchCount].name = name;
	compMatches[matchCount].value = value;
	matchCount++;

	if ( matchCount == 1 ) {
		Q_strncpyz( shortestMatch, name, sizeof( shortestMatch ) );
		return;
	}
	
	if(con_cmdcomplete->integer <= 1) //want first match
	{
		if( !Q_stricmp(name, completionString) ) //exact match
			Q_strncpyz( shortestMatch, name, sizeof( shortestMatch ) );
		return;
	}

	// cut shortestMatch to the amount common with s
	for ( i = 0; i < MAX_TOKEN_CHARS; i++ ) {
		if ( !name[i] || !shortestMatch[i]) {
			if(shortestMatch[i])
				shortestMatch[i] = 0;

			break;
		}
		if ( tolower(shortestMatch[i]) != tolower(name[i]) ) {
			shortestMatch[i] = 0;
			break;
		}
	}
}
void Alias_CommandCompletion( const char *partial, void(*callback)(const char *name, const char *value) );

void CompleteCommand (void)
{
	int		i, offset = 0, listOptions, len;
	compMatches_t *match;

	completionString = con_inputLines.text[con_inputLines.editLine];
	if (*completionString == '\\' || *completionString == '/')
		completionString++;

	if( *completionString == '\0' )
		return;

	matchCount = 0;
	shortestMatch[0] = 0;

	if(Key_IsDown(K_CTRL) && cls.state == ca_active) //Hacky nick completion
	{
		qboolean skipWhite = false;

		len = strlen(completionString);
		if(!len)
			return;

		if(len > 15) {
			if(completionString[len-16] != ' ')
				skipWhite = true;

			completionString += len - 15;
		}
			
		do
		{
			if(skipWhite) {
				while(*completionString && *completionString != ' ')
					completionString++;
			}

			while(*completionString && *completionString == ' ')
				completionString++;

			len = strlen(completionString);
			if(!len)
				return;

			for(i = 0; i < cl.maxclients; i++) {
				if(cl.clientinfo[i].name[0] && !Q_strnicmp(cl.clientinfo[i].name, completionString, len)) {
					AddMatches(cl.clientinfo[i].name, NULL);
				}
			}
			skipWhite = true;
		} while(!matchCount);
		offset = 1;
		listOptions = 0;
	}
	else if (!Q_strnicmp(completionString, "set ", 4)) { //Hacky
		offset = 1;
		completionString += 4;
		listOptions = LIST_CVARS;
	}
	else if (!Q_strnicmp(completionString, "reset ", 6)) {
		offset = 1;
		completionString += 6;
		listOptions = LIST_CVARS;
	}
	else if (!Q_strnicmp(completionString, "alias ", 6)) {
		offset = 1;
		completionString += 6;
		listOptions = LIST_ALIASES;
	}
	else if (!Q_strnicmp(completionString, "unalias ", 8)) {
		offset = 1;
		completionString += 8;
		listOptions = LIST_ALIASES;
	}
	else {
		listOptions = LIST_CVARS|LIST_COMMANDS|LIST_ALIASES;
	}

	if ( strlen( completionString ) == 0 ) {
		return;
	}

	if(listOptions & LIST_COMMANDS)
		Cmd_CommandCompletion(completionString, AddMatches);
	
	if(listOptions & LIST_ALIASES)
		Alias_CommandCompletion(completionString, AddMatches);

	if(listOptions & LIST_CVARS)
		Cvar_CommandCompletion(completionString, AddMatches);

	if ( matchCount == 0 )
		return;	// no matches

	if(offset) {
		*completionString = '\0';
		Q_strncatz(con_inputLines.text[con_inputLines.editLine], shortestMatch, MAX_FIELD_TEXT - 1);
	}
	else
	{
		IF_Init(&con_inputLines);
		con_inputLines.text[con_inputLines.editLine][0] = '/';
		strcpy (con_inputLines.text[con_inputLines.editLine]+1, shortestMatch);
	}
	con_inputLines.cursorPos = strlen(con_inputLines.text[con_inputLines.editLine]);

	if ( matchCount == 1 || con_cmdcomplete->integer <= 1) {
		con_inputLines.text[con_inputLines.editLine][con_inputLines.cursorPos] = ' ';
		con_inputLines.text[con_inputLines.editLine][con_inputLines.cursorPos+1] = '\0';
		con_inputLines.cursorPos++;
		if(matchCount == 1 || con_cmdcomplete->integer < 1)
			return;
	}

	//Print partial matches
	Com_Printf("]%s\n", con_inputLines.text[con_inputLines.editLine]);

	qsort(compMatches, matchCount, sizeof(compMatches[0]), (int (*)(const void *, const void *))MatchShort);

	for(i = 0, match = compMatches; i < matchCount; i++, match++)
	{
		if(con_cmdcomplete->integer == 3 && match->value)
			Com_Printf("   %s = %s\n", match->name, match->value);
		else
			Com_Printf("   %s\n", match->name);

		match->name = NULL;
		match->value = NULL;
	}

}

void IF_CharEvent( inputField_t *field, int key )
{
	int len;

	if (key < 32 || key > 127)
		return;	// non printable

	if (field->cursorPos >= MAX_FIELD_TEXT-1)
		return;

	len = strlen(field->text[field->editLine]);
	if (!Key_GetOverstrikeMode()) {
		if(len >= MAX_FIELD_TEXT-1)
			return;
		memmove( field->text[field->editLine] + field->cursorPos + 1, field->text[field->editLine] + field->cursorPos, len - field->cursorPos + 1 );
	}


	field->text[field->editLine][field->cursorPos] = key;
	field->cursorPos++;

	if (field->cursorPos == len + 1)
		field->text[field->editLine][field->cursorPos] = 0;
}


void IF_KeyEvent( inputField_t *field, int key )
{
	if ( ( toupper( key ) == 'V' && Key_IsDown(K_CTRL) ) ||
		 ( (key == K_INS || key == K_KP_INS) && Key_IsDown(K_SHIFT) ) )
	{
		char *cbd;
		
		if ( (cbd = Sys_GetClipboardData()) != 0 )
		{
			int i;

			//strtok( cbd, "\n\r\b" );

			for( i=0; cbd[i]; i++ )
			{
				if(cbd[i] == '\n' || cbd[i] == '\r')
					IF_CharEvent (field, ' ');
				else
					IF_CharEvent (field, cbd[i]);
			}
			Z_Free( cbd );
		}

		return;
	}

	if ( key == K_UPARROW || key == K_KP_UPARROW || (key == 'p' && Key_IsDown(K_CTRL)) )
	{
		int tmp = field->historyLine;
		do
		{
			field->historyLine = (field->historyLine - 1) & HISTORY_MASK;
		} while (field->historyLine != field->editLine && !field->text[field->historyLine][0]);

		if (field->historyLine == field->editLine)
		{
			field->historyLine = tmp;
			return;
		}

		IF_Init(field);

		strcpy(field->text[field->editLine], field->text[field->historyLine]);
		field->cursorPos = strlen(field->text[field->editLine]);
		return;
	}

	if ( key == K_DOWNARROW || key == K_KP_DOWNARROW || (key == 'n' && Key_IsDown(K_CTRL)) )
	{
		if (field->historyLine == field->editLine)
			return;
		do
		{
			field->historyLine = (field->historyLine + 1) & HISTORY_MASK;
		} while (field->historyLine != field->editLine && !field->text[field->historyLine][0]);

		IF_Init(field);

		if (field->historyLine != field->editLine)
		{
			strcpy(field->text[field->editLine], field->text[field->historyLine]);
			field->cursorPos = strlen(field->text[field->editLine]);
		}
		return;
	}

	if (key == K_HOME || key == K_KP_HOME )
	{
		field->cursorPos = 0;
		return;
	}

	if (key == K_END || key == K_KP_END )
	{
		field->cursorPos = strlen(field->text[field->editLine]);
		return;
	}

	if ( key == K_DEL )
	{
		if (field->text[field->editLine][field->cursorPos])
			memmove( field->text[field->editLine] + field->cursorPos, field->text[field->editLine] + field->cursorPos + 1, sizeof( field->text[field->editLine] ) - field->cursorPos - 1);

		return;
	}

	if ( key == K_BACKSPACE )
	{
		if (field->cursorPos > 0)
		{
			memmove(field->text[field->editLine] + field->cursorPos - 1, field->text[field->editLine] + field->cursorPos, sizeof( field->text[field->editLine] ) - field->cursorPos);
			field->cursorPos--;
		}
		return;
	}

	if ( key == K_LEFTARROW || key == K_KP_LEFTARROW || (key == 'h' && Key_IsDown(K_CTRL)) )
	{
		if(field->cursorPos > 0)
			field->cursorPos--;

		return;
	}

	if ( key == K_RIGHTARROW )
	{
		if (field->text[field->editLine][field->cursorPos])
			field->cursorPos++;

		return;
	}

	if ( key == K_INS )
	{ // toggle insert mode
		Key_SetOverstrikeMode( Key_GetOverstrikeMode() ^ 1 );
		return;
	}

	IF_CharEvent ( field, key);
}

/*
====================
Key_Console

Interactive line editing and console scrollback
====================
*/
void Key_Console (int key)
{
	switch ( key )
	{
	case K_KP_SLASH:
		key = '/';
		break;
	case K_KP_MINUS:
		key = '-';
		break;
	case K_KP_PLUS:
		key = '+';
		break;
	case K_KP_HOME:
		key = '7';
		break;
	case K_KP_UPARROW:
		key = '8';
		break;
	case K_KP_PGUP:
		key = '9';
		break;
	case K_KP_LEFTARROW:
		key = '4';
		break;
	case K_KP_5:
		key = '5';
		break;
	case K_KP_RIGHTARROW:
		key = '6';
		break;
	case K_KP_END:
		key = '1';
		break;
	case K_KP_DOWNARROW:
		key = '2';
		break;
	case K_KP_PGDN:
		key = '3';
		break;
	case K_KP_INS:
		key = '0';
		break;
	case K_KP_DEL:
		key = '.';
		break;
	}

	if ( key == 'l' && Key_IsDown(K_CTRL) ) 
	{
		Con_Clear_f();
		return;
	}

	if ( key == K_ENTER || key == K_KP_ENTER )
	{	// backslash text are commands, else chat
		if (con_inputLines.text[con_inputLines.editLine][0] == '\\' || con_inputLines.text[con_inputLines.editLine][0] == '/')
			Cbuf_AddText (con_inputLines.text[con_inputLines.editLine]+1);	// skip the >
		else
			Cbuf_AddText (con_inputLines.text[con_inputLines.editLine]);	// valid command

		Cbuf_AddText ("\n");

		Com_Printf ("]%s\n", con_inputLines.text[con_inputLines.editLine]);

		if(strcmp(con_inputLines.text[con_inputLines.editLine], con_inputLines.text[(con_inputLines.editLine - 1) & HISTORY_MASK])) {
			con_inputLines.editLine = (con_inputLines.editLine + 1) & HISTORY_MASK;
		}
		con_inputLines.historyLine = con_inputLines.editLine;

		IF_Init(&con_inputLines);

		if (cls.state == ca_disconnected)
			SCR_UpdateScreen ();	// force an update, because the command
									// may take some time
		return;
	}

	if (key == K_TAB)
	{	// command completion
		CompleteCommand();
		return;
	}

	if (key == K_PGUP || key == K_KP_PGUP || key == K_MWHEELUP)
	{
		if(!con_mwheel->integer && key == K_MWHEELUP)
			return;

		con.display -= con_scrlines->integer;
		if (con.display < con.current - con.totallines + 1)
			con.display = con.current - con.totallines + 1;
		return;
	}

	if (key == K_PGDN || key == K_KP_PGDN || key == K_MWHEELDOWN)
	{
		if(!con_mwheel->integer && key == K_MWHEELDOWN)
			return;

		con.display += con_scrlines->integer;
		if (con.display > con.current)
			con.display = con.current;
		return;
	}

	if (key == K_HOME || key == K_KP_HOME )
	{
		if (Key_IsDown(K_CTRL) || !con_inputLines.text[con_inputLines.editLine][0])
		{
			int row, i;
			short *text;
			qboolean findText = false;

			//Find first line with text
			for (row = con.current - con.totallines + 1; row < con.current; row++)
			{				
				text = con.text + (row % con.totallines)*con.linewidth;
				for(i=0; i<con.linewidth; i++)
				{
					if(text[i] != ((COLOR_WHITE<<8) | ' ')) {
						findText = true;
						break;
					}
				}
				if(findText)
					break;
			}
			
			if(findText && row < con.display - ((con.vislines-30)>>3)+5)
			{
				row += ((con.vislines-30)>>3)-5;
				if(row < con.current)
					con.display = row;
			}
			return;
		}
	}

	if(key == K_UPARROW || key == K_DOWNARROW)
	{
		//console buffer find
		char *s = con_inputLines.text[con_inputLines.editLine];
		if(Key_IsDown(K_CTRL) && *s && *s != ' ')
		{
			int row, i, j, len;
			short *text;
			qboolean findText = false;

			if(key == K_DOWNARROW && con.display == con.current)
				return;

			len = strlen(s);
			row = con.display;
			for (;;)
			{
				if(key == K_UPARROW) {
					if(--row < con.current - con.totallines+1)
						break;
				}
				else if(++row > con.current)
					break;

				text = con.text + (row % con.totallines)*con.linewidth;
				for(i=0; i<con.linewidth-len; i++) {
					for(j = 0; j < len; j++)
					{
						if(toupper(text[i+j] & 127) != toupper(s[j]))
							break;
					}
					if(j==len) {
						findText = true;
						break;
					}
				}
				if(findText)
					break;
			}
			
			if(findText && row <= con.current)
				con.display = row;

			return;
		}
	}

	if (key == K_END || key == K_KP_END )
	{
		if (Key_IsDown(K_CTRL) || !con_inputLines.text[con_inputLines.editLine][0])
		{
			con.display = con.current;
			return;
		}
	}

	IF_KeyEvent (&con_inputLines, key);

}

/*
====================
Key_Message

Interactive line editing
====================
*/

void Key_Message (int key)
{
	switch ( key )
	{
	case K_KP_SLASH:
		key = '/';
		break;
	case K_KP_MINUS:
		key = '-';
		break;
	case K_KP_PLUS:
		key = '+';
		break;
	case K_KP_HOME:
		key = '7';
		break;
	case K_KP_UPARROW:
		key = '8';
		break;
	case K_KP_PGUP:
		key = '9';
		break;
	case K_KP_LEFTARROW:
		key = '4';
		break;
	case K_KP_5:
		key = '5';
		break;
	case K_KP_RIGHTARROW:
		key = '6';
		break;
	case K_KP_END:
		key = '1';
		break;
	case K_KP_DOWNARROW:
		key = '2';
		break;
	case K_KP_PGDN:
		key = '3';
		break;
	case K_KP_INS:
		key = '0';
		break;
	case K_KP_DEL:
		key = '.';
		break;
	}

	if ( key == K_ENTER || key == K_KP_ENTER )
	{
		if(!chat_inputLines.text[chat_inputLines.editLine][0]) {
			cls.key_dest = key_game;
			return;
		}

		if (chat_team)
			Cbuf_AddText ("say_team \"");
		else
			Cbuf_AddText ("say \"");

		Cbuf_AddText(chat_inputLines.text[chat_inputLines.editLine]);
		Cbuf_AddText("\"\n");

		if(strcmp(chat_inputLines.text[chat_inputLines.editLine], chat_inputLines.text[(chat_inputLines.editLine - 1) & HISTORY_MASK])) {
			chat_inputLines.editLine = (chat_inputLines.editLine + 1) & HISTORY_MASK;
		}
		chat_inputLines.historyLine = chat_inputLines.editLine;

		IF_Init(&chat_inputLines);
		cls.key_dest = key_game;
		return;
	}

	if (key == K_ESCAPE)
	{
		chat_inputLines.historyLine = chat_inputLines.editLine;
		IF_Init(&chat_inputLines);
		cls.key_dest = key_game;
		return;
	}

	IF_KeyEvent (&chat_inputLines, key);
}
