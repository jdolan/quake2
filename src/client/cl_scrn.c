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
// cl_scrn.c -- master for refresh, status bar, console, chat, notify, etc

/*

  full screen console
  put up loading plaque
  blanked background with loading plaque
  blanked background with menu
  cinematics
  full screen image for quit and victory

  end of unit intermissions

  */

#include "client.h"


float		scr_con_current;	// aproaches scr_conlines at scr_conspeed
float		scr_conlines;		// 0.0 to 1.0 lines of console to display

qboolean	scr_initialized;		// ready to draw

int			scr_draw_loading;

vrect_t		scr_vrect;		// position of render window on screen


cvar_t		*scr_viewsize;
cvar_t		*scr_conspeed;
cvar_t		*scr_centertime;
cvar_t		*scr_showturtle;
cvar_t		*scr_showpause;
cvar_t		*scr_printspeed;

cvar_t		*scr_netgraph;
cvar_t		*scr_timegraph;
cvar_t		*scr_debuggraph;
cvar_t		*scr_graphheight;
cvar_t		*scr_graphscale;
cvar_t		*scr_graphshift;
cvar_t		*scr_drawall;

cvar_t		*scr_conheight;

typedef struct
{
	int		x1, y1, x2, y2;
} dirty_t;

dirty_t		scr_dirty, scr_old_dirty[2];

char		crosshair_pic[8];
int			crosshair_width, crosshair_height;

void SCR_TimeRefresh_f (void);
void SCR_Loading_f (void);

/*
===============================================================================

BAR GRAPHS

===============================================================================
*/

/*
==============
CL_AddNetgraph

A new packet was just parsed
==============
*/
void CL_AddNetgraph (void)
{
	int		i, in, ping;

	// if using the debuggraph for something else, don't
	// add the net lines
	if (scr_debuggraph->integer || scr_timegraph->integer)
		return;

	for (i=0 ; i<cls.netchan.dropped ; i++)
		SCR_DebugGraph (30, 0x40);

	for (i=0 ; i<cl.surpressCount ; i++)
		SCR_DebugGraph (30, 0xdf);

	// see what the latency was on this packet
	in = cls.netchan.incoming_acknowledged & CMD_MASK;
	ping = cls.realtime - cl.history[in].realtime;
	ping /= 30;
	if (ping > 30)
		ping = 30;
	SCR_DebugGraph (ping, 0xd0);
}


typedef struct
{
	float	value;
	int		color;
} graphsamp_t;

static	int			current = 0;
static	graphsamp_t	values[1024];

/*
==============
SCR_DebugGraph
==============
*/
void SCR_DebugGraph (float value, int color)
{
	values[current&1023].value = value;
	values[current&1023].color = color;
	current++;
}

/*
==============
SCR_DrawDebugGraph
==============
*/
static void SCR_DrawDebugGraph (void)
{
	int		a, x, y, w, i, h;
	float	v;
	int		color;

	//
	// draw the graph
	//
	w = scr_vrect.width;

	x = scr_vrect.x;
	y = scr_vrect.y+scr_vrect.height;

	//Draw_Fill (x, y-scr_graphheight->value, w, scr_graphheight->value, 8);

	for (a=0 ; a<w ; a++)
	{
		i = (current-1-a+1024) & 1023;
		v = values[i].value;
		color = values[i].color;
		v = v*scr_graphscale->value + scr_graphshift->value;
		
		if (v < 0)
			v += scr_graphheight->value * (1+(int)(-v/scr_graphheight->value));
		h = (int)v % scr_graphheight->integer;
		Draw_Fill (x+w-1-a, y - h, 1,	h, color);
	}
}

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

static char			scr_centerstring[1024];
static unsigned int	scr_centertime_off, scr_centertime_start, scr_center_lines;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint (const char *str)
{
	char	line[1024], *s, *s2;
	int		i, j, l, longestLine, maxChars;

	while (*str == '\n')
		str++;

	Q_strncpyz(line, str, sizeof(line));
	s = line;
	for (i = strlen(s) - 1; i > 0 && (s[i] == '\n' || s[i] == ' '); i--)
		s[i] = 0;

	if (!line[0])
		return;

	scr_centertime_start = cls.realtime;
	scr_centertime_off = cls.realtime + (unsigned int)(scr_centertime->value * 1000);

	if (!strcmp(scr_centerstring, line))
		return;

	strcpy(scr_centerstring, line);

	s = scr_centerstring;
	// count the number of lines for centering
	longestLine = l = 0;
	scr_center_lines = 1;
	do
	{
		if (*s == '\n') {
			scr_center_lines++;
			if (l > longestLine)
				longestLine = l;

			l = 0;
		} else {
			l++;
		}
		s++;
	} while (*s);
	
	if (l > longestLine)
		longestLine = l;

	maxChars = (viddef.width >> 3) - 2;
	if (maxChars > longestLine)
		maxChars = longestLine;

	if (maxChars >= sizeof(line))
		maxChars = sizeof(line) - 1;
	else if (maxChars < 40)
		maxChars = 40;

	// echo it to the console
	s2 = line + sizeof(line) - 1;
	*s2 = 0;

	for (i = maxChars - 2; i > 0; i--)
		*--s2 = '\36';

	Com_Printf("\n\35%s\37\n\n", s2);

	s = scr_centerstring;
	do	
	{
		// scan the width of the line
		for (l = 0; l < maxChars; l++) {
			if (s[l] == '\n' || !s[l])
				break;
		}

		j = (maxChars - l)/2;
		for (i = 0; i < j; i++)
			line[i] = ' ';

		for (j = 0; j < l; j++) {
			line[i++] = s[j];
		}

		line[i] = '\n';
		line[i+1] = 0;

		Com_Printf ("%s", line);

		s += l;
		if (!*s)
			break;

		if (l != maxChars)
			s++;		// skip the \n
	}
	while (*s);

	s2 = line + sizeof(line) - 1;
	*s2 = 0;

	for (i = maxChars - 2; i > 0; i--)
		*--s2 = '\36';

	Com_Printf("\n\35%s\37\n\n", s2);
	//Com_Printf("\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	Con_ClearNotify();
}


static void SCR_DrawCenterString (void)
{
	char	*start, saved_byte;
	int		l, x, y, maxChars;
	float	alpha = 1.0f;

// the finale prints the characters one at a time
	start = scr_centerstring;

	if (scr_center_lines <= 4)
		y = (int)(viddef.height*0.35f);
	else
		y = 48;

//	alpha = (scr_centertime_off - cls.realtime)*0.001f;
	alpha = SCR_FadeAlpha(scr_centertime_start, scr_centertime_off - scr_centertime_start, 1000);
	maxChars = (viddef.width >> 3) - 2;

	do	
	{
		// scan the width of the line
		for (l = 0; l < maxChars; l++) {
			if (start[l] == '\n' || !start[l])
				break;
		}
		x = (viddef.width - l*8)*0.5f;
		SCR_AddDirtyPoint (x, y);

		saved_byte = start[l];
		start[l] = 0;
		Draw_String(x, y, start, COLOR_WHITE, alpha, false);
		start[l] = saved_byte;

		y += 8;
		SCR_AddDirtyPoint (x, y);

		start += l;
		if (!*start)
			break;

		start++;		// skip the \n
	}
	while (*start);
}

void SCR_CheckDrawCenterString (void)
{
	if (scr_centertime_off > cls.realtime)
		SCR_DrawCenterString();
}

//=============================================================================

/*
=================
SCR_CalcVrect

Sets scr_vrect, the coordinates of the rendered window
=================
*/
static void SCR_CalcVrect (void)
{
	int		size;

	size = scr_viewsize->integer;

	scr_vrect.width = viddef.width*size/100;
	scr_vrect.width &= ~7;

	scr_vrect.height = viddef.height*size/100;
	scr_vrect.height &= ~1;

	scr_vrect.x = (viddef.width - scr_vrect.width)/2;
	scr_vrect.y = (viddef.height - scr_vrect.height)/2;
}


/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
static void SCR_SizeUp_f (void)
{
	if (scr_viewsize->integer >= 90)
		Cvar_Set("viewsize", "100");
	else
		Cvar_SetValue ("viewsize", scr_viewsize->integer+10);	
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
static void SCR_SizeDown_f (void)
{
	if (scr_viewsize->integer <= 50)
		Cvar_Set("viewsize", "40");
	else
		Cvar_SetValue ("viewsize", scr_viewsize->integer-10);
}

/*
=================
SCR_Sky_f

Set a specific sky and rotation speed
=================
*/
char currentSky[64];

static void SCR_Sky_f (void)
{
	float	rotate = 0.0f;
	vec3_t	axis;

	if (Cmd_Argc() < 2) {
		Com_Printf ("Usage: sky <basename> <rotate> <axis x y z>\n");
		Com_Printf ("Current: %s Map default: %s\n", currentSky, cl.configstrings[CS_SKY]);
		return;
	}
	if (Cmd_Argc() > 2)
		rotate = (float)atof(Cmd_Argv(2));

	if (Cmd_Argc() == 6)
		VectorSet(axis, (float)atof(Cmd_Argv(3)), (float)atof(Cmd_Argv(4)), (float)atof(Cmd_Argv(5)));
	else
		VectorSet(axis, 0, 0, 1);

	Q_strncpyz(currentSky, Cmd_Argv(1), sizeof(currentSky));
	R_SetSky (currentSky, rotate, axis);
}

//============================================================================

static void OnChange_Conheight (cvar_t *self, const char *oldValue)
{
	if(self->value < 0.1f)
		Cvar_Set(self->name, "0.1");
	else if(self->value > 1.0f)
		Cvar_Set(self->name, "1");
}

static void OnChange_Viewsize (cvar_t *self, const char *oldValue)
{
	// bound viewsize
	if (self->integer < 40)
		Cvar_Set(self->name, "40");
	if (self->integer > 100)
		Cvar_Set(self->name, "100");
}

static void CL_EchoCenterPrint_f(void)
{
	if(cls.state != ca_active)
		return;

	if(Cmd_Argc() < 1) {
		Com_Printf("usage: %s <text>\n", Cmd_Argv(0));
		return;
	}

	SCR_CenterPrint(Cmd_ArgsFrom(1));
}

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{
	scr_viewsize = Cvar_Get ("viewsize", "100", CVAR_ARCHIVE);
	scr_conspeed = Cvar_Get ("scr_conspeed", "3", 0);
	scr_showturtle = Cvar_Get ("scr_showturtle", "0", 0);
	scr_showpause = Cvar_Get ("scr_showpause", "1", 0);
	scr_centertime = Cvar_Get ("scr_centertime", "2.5", 0);
	scr_printspeed = Cvar_Get ("scr_printspeed", "8", 0);
	scr_netgraph = Cvar_Get ("netgraph", "0", 0);
	scr_timegraph = Cvar_Get ("timegraph", "0", 0);
	scr_debuggraph = Cvar_Get ("debuggraph", "0", 0);
	scr_graphheight = Cvar_Get ("graphheight", "32", 0);
	scr_graphscale = Cvar_Get ("graphscale", "1", 0);
	scr_graphshift = Cvar_Get ("graphshift", "0", 0);
	scr_drawall = Cvar_Get ("scr_drawall", "0", 0);

	scr_conheight = Cvar_Get ("scr_conheight", "0.5", CVAR_ARCHIVE);

	scr_conheight->OnChange = OnChange_Conheight;
	OnChange_Conheight(scr_conheight, scr_conheight->resetString);
	scr_viewsize->OnChange = OnChange_Viewsize;
	OnChange_Viewsize(scr_viewsize, scr_viewsize->resetString);

	SCR_InitDraw();

//
// register our commands
//
	Cmd_AddCommand ("timerefresh",SCR_TimeRefresh_f);
	Cmd_AddCommand ("loading",SCR_Loading_f);
	Cmd_AddCommand ("sizeup",SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f);
	Cmd_AddCommand ("sky",SCR_Sky_f);
	Cmd_AddCommand ("centerprint", CL_EchoCenterPrint_f);

	scr_initialized = true;
}


/*
==============
SCR_DrawNet
==============
*/
void SCR_DrawNet (void)
{
	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged < CMD_BACKUP-1)
		return;

	Draw_Pic (scr_vrect.x+64, scr_vrect.y, "net", cl_hudalpha->value);
}

/*
==============
SCR_DrawPause
==============
*/
void SCR_DrawPause (void)
{
	int		w = 0, h = 0;

	if (!scr_showpause->integer)		// turn off for screenshots
		return;

	if (!cl_paused->integer)
		return;

	Draw_GetPicSize (&w, &h, "pause");
	Draw_Pic ((viddef.width-w)/2, viddef.height/2 + 8, "pause", 1.0f);
}

/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading (void)
{
	int		w = 0, h = 0;
		
	if (!scr_draw_loading)
		return;

	scr_draw_loading = false;
	Draw_GetPicSize (&w, &h, "loading");
	Draw_Pic ((viddef.width-w)/2, (viddef.height-h)/2, "loading", 1.0f);
}

//=============================================================================

/*
==================
SCR_RunConsole

Scroll it up or down
==================
*/
void SCR_RunConsole (void)
{
// decide on the height of the console
	if (cls.key_dest == key_console)
		scr_conlines = scr_conheight->value;	// user controllable
	else
		scr_conlines = 0;				// none visible
	
	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= scr_conspeed->value*cls.frametime;
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;

	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += scr_conspeed->value*cls.frametime;
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

}

/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole (void)
{
	Con_CheckResize ();
	
	if (cls.state <= ca_connecting)
	{	// forced full screen console
		Con_DrawConsole (1.0f, false);
		return;
	}

	if (cls.state != ca_active || !cl.refresh_prepped)
	{	// connected, but can't render
		Draw_Fill (0, 0, viddef.width, viddef.height, 0);
		Con_DrawConsole (scr_conheight->value, false);
		return;
	}

	if (scr_con_current)
	{
		Con_DrawConsole (scr_con_current, true);
	}
	else
	{
		if (cls.key_dest == key_game || cls.key_dest == key_message)
			Con_DrawNotify ();	// only draw notify in game
	}
}

//=============================================================================

/*
================
SCR_BeginLoadingPlaque
================
*/
void SCR_BeginLoadingPlaque (void)
{
	S_StopAllSounds ();
	cl.sound_prepped = false;		// don't play ambients
#ifdef CD_AUDIO
	CDAudio_Stop ();
#endif
	if (cls.disable_screen)
		return;
	if (developer->integer)
		return;
	if (cls.state == ca_disconnected)
		return;	// if at console, don't bring up the plaque
	if (cls.key_dest == key_console)
		return;
	if (cl.cinematictime > 0)
		scr_draw_loading = 2;	// clear to balack first
	else
		scr_draw_loading = 1;
	SCR_UpdateScreen ();
	cls.disable_screen = Sys_Milliseconds();
	cls.disable_servercount = cl.servercount;
}

/*
================
SCR_EndLoadingPlaque
================
*/
void SCR_EndLoadingPlaque (void)
{
	cls.disable_screen = 0;
	Con_ClearNotify ();
}

/*
================
SCR_Loading_f
================
*/
void SCR_Loading_f (void)
{
	SCR_BeginLoadingPlaque ();
}

/*
================
SCR_TimeRefresh_f
================
*/
void SCR_TimeRefresh_f (void)
{
	int		i;
	unsigned int start, stop;
	float	time;

	if ( cls.state != ca_active )
		return;

	start = Sys_Milliseconds();

	if (Cmd_Argc() == 2)
	{	// run without page flipping
		R_BeginFrame( 0 );
		for (i = 0; i < 128; i++) {
			cl.refdef.viewangles[1] = i/128.0f*360.0f;
			R_RenderFrame (&cl.refdef);
		}
		R_EndFrame();
	}
	else
	{
		for (i = 0; i < 128; i++) {
			cl.refdef.viewangles[1] = i/128.0f*360.0f;

			R_BeginFrame( 0 );
			R_RenderFrame (&cl.refdef);
			R_EndFrame();
		}
	}

	stop = Sys_Milliseconds();
	time = (stop-start)* 0.001f;
	Com_Printf ("%f seconds (%f fps)\n", time, 128/time);
}

/*
=================
SCR_AddDirtyPoint
=================
*/
void SCR_AddDirtyPoint (int x, int y)
{
	if (x < scr_dirty.x1)
		scr_dirty.x1 = x;
	if (x > scr_dirty.x2)
		scr_dirty.x2 = x;
	if (y < scr_dirty.y1)
		scr_dirty.y1 = y;
	if (y > scr_dirty.y2)
		scr_dirty.y2 = y;
}

void SCR_DirtyScreen (void)
{
	SCR_AddDirtyPoint (0, 0);
	SCR_AddDirtyPoint (viddef.width-1, viddef.height-1);
}

/*
==============
SCR_TileClear

Clear any parts of the tiled background that were drawn on last frame
==============
*/
void SCR_TileClear (void)
{
	int		i;
	int		top, bottom, left, right;
	dirty_t	clear;

	if (scr_drawall->integer)
		SCR_DirtyScreen ();	// for power vr or broken page flippers...

	if (scr_viewsize->integer == 100)
		return;		// full screen rendering

	if (scr_con_current == 1.0f)
		return;		// full screen console

	if (cl.cinematictime > 0)
		return;		// full screen cinematic

	// erase rect will be the union of the past three frames
	// so tripple buffering works properly
	clear = scr_dirty;
	for (i=0 ; i<2 ; i++)
	{
		if (scr_old_dirty[i].x1 < clear.x1)
			clear.x1 = scr_old_dirty[i].x1;
		if (scr_old_dirty[i].x2 > clear.x2)
			clear.x2 = scr_old_dirty[i].x2;
		if (scr_old_dirty[i].y1 < clear.y1)
			clear.y1 = scr_old_dirty[i].y1;
		if (scr_old_dirty[i].y2 > clear.y2)
			clear.y2 = scr_old_dirty[i].y2;
	}

	scr_old_dirty[1] = scr_old_dirty[0];
	scr_old_dirty[0] = scr_dirty;

	scr_dirty.x1 = 9999;
	scr_dirty.x2 = -9999;
	scr_dirty.y1 = 9999;
	scr_dirty.y2 = -9999;

	// don't bother with anything convered by the console)
	top = scr_con_current*viddef.height;
	if (top >= clear.y1)
		clear.y1 = top;

	if (clear.y2 <= clear.y1)
		return;		// nothing disturbed

	top = scr_vrect.y;
	bottom = top + scr_vrect.height-1;
	left = scr_vrect.x;
	right = left + scr_vrect.width-1;

	if (clear.y1 < top)
	{	// clear above view screen
		i = clear.y2 < top-1 ? clear.y2 : top-1;
		Draw_TileClear (clear.x1 , clear.y1,
			clear.x2 - clear.x1 + 1, i - clear.y1+1, "backtile");
		clear.y1 = top;
	}
	if (clear.y2 > bottom)
	{	// clear below view screen
		i = clear.y1 > bottom+1 ? clear.y1 : bottom+1;
		Draw_TileClear (clear.x1, i,
			clear.x2-clear.x1+1, clear.y2-i+1, "backtile");
		clear.y2 = bottom;
	}
	if (clear.x1 < left)
	{	// clear left of view screen
		i = clear.x2 < left-1 ? clear.x2 : left-1;
		Draw_TileClear (clear.x1, clear.y1,
			i-clear.x1+1, clear.y2 - clear.y1 + 1, "backtile");
		clear.x1 = left;
	}
	if (clear.x2 > right)
	{	// clear left of view screen
		i = clear.x1 > right+1 ? clear.x1 : right+1;
		Draw_TileClear (i, clear.y1,
			clear.x2-i+1, clear.y2 - clear.y1 + 1, "backtile");
		clear.x2 = right;
	}

}


//===============================================================


#define STAT_MINUS		10	// num frame for '-' stats digit
static const char	*sb_nums[2][11] = 
{
	{"num_0", "num_1", "num_2", "num_3", "num_4", "num_5",
	"num_6", "num_7", "num_8", "num_9", "num_minus"},
	{"anum_0", "anum_1", "anum_2", "anum_3", "anum_4", "anum_5",
	"anum_6", "anum_7", "anum_8", "anum_9", "anum_minus"}
};

#define	ICON_WIDTH	24
#define	ICON_HEIGHT	24
#define	CHAR_WIDTH	16
#define	ICON_SPACE	8



/*
================
SizeHUDString

Allow embedded \n in the string
================
*/
#if 0
static void SizeHUDString (const char *string, int *w, int *h)
{
	int		lines = 1, width = 0, current = 0;

	while (*string)
	{
		if (*string == '\n')
		{
			lines++;
			current = 0;
		}
		else
		{
			current++;
			if (current > width)
				width = current;
		}
		string++;
	}

	*w = width * 8;
	*h = lines * 8;
}
#endif

static void DrawHUDString (const char *string, int x, int y, int centerwidth, qboolean alt)
{
	int		margin;
	char	line[1024];
	int		width;

	margin = x;

	while (*string)
	{
		// scan out one line of text from the string
		width = 0;
		while (*string && *string != '\n' && width < 1023)
			line[width++] = *string++;
		line[width] = 0;

		if (centerwidth)
			x = margin + (centerwidth - width*8)/2;
		else
			x = margin;
			
		Draw_String (x, y, line, 7, 1, alt);

		if (*string)
		{
			string++;	// skip the \n
			y += 8;
		}
	}
}


/*
==============
SCR_DrawField
==============
*/
void SCR_DrawField (int x, int y, int color, int width, int value)
{
	char	num[16], *ptr;
	int		l;
	int		frame;

	if (width < 1)
		return;

	// draw number string
	if (width > 5)
		width = 5;

	color &= 1;

	SCR_AddDirtyPoint (x, y);
	SCR_AddDirtyPoint (x+width*CHAR_WIDTH+2, y+23);

	Com_sprintf (num, sizeof(num), "%i", value);
	l = strlen(num);
	if (l > width)
		l = width;
	x += 2 + CHAR_WIDTH*(width - l);

	ptr = num;
	while (*ptr && l)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		Draw_Pic (x,y,sb_nums[color][frame], cl_hudalpha->value); //hud nums
		x += CHAR_WIDTH;
		ptr++;
		l--;
	}
}

/*
===============
SCR_TouchPics

Allows rendering code to cache all needed sbar graphics
===============
*/
void SCR_TouchPics (void)
{
	int		i, j;

	for (i=0 ; i<2 ; i++) {
		for (j=0 ; j<11 ; j++)
			Draw_FindPic (sb_nums[i][j]);
	}

	if (crosshair->integer)
	{
		if (crosshair->integer < 0)
			Cvar_Set(crosshair->name, "1");
		else if (crosshair->integer > 50)
			Cvar_Set(crosshair->name, "50");

		Com_sprintf (crosshair_pic, sizeof(crosshair_pic), "ch%i", crosshair->integer);
		Draw_GetPicSize (&crosshair_width, &crosshair_height, crosshair_pic);
		if (!crosshair_width)
			crosshair_pic[0] = 0;
	}
}

/*
================
SCR_ExecuteLayoutString 

================
*/
void SCR_ExecuteLayoutString (char *s)
{
	int		x = 0, y = 0;
	int		value;
	const char	*token;
	int		width = 3, index;
	clientinfo_t	*ci;

	if (!cl.refresh_prepped)
		return;

	if (!s[0])
		return;

	while (s)
	{
		token = COM_Parse (&s);

		switch (token[0]) {
		case 'x':
			if(token[2])
				break;
			if (token[1] == 'v')
			{
				token = COM_Parse (&s);
				x = viddef.width/2 - 160 + atoi(token);
				continue;
			}
			else if (token[1] == 'r')
			{
				token = COM_Parse (&s);
				x = viddef.width + atoi(token);
				continue;
			}
			else if (token[1] == 'l')
			{
				token = COM_Parse (&s);
				x = atoi(token);
				continue;
			}
			break;
		case 'y':
			if(token[2])
				break;
			if (token[1] == 'v')
			{
				token = COM_Parse (&s);
				y = viddef.height/2 - 120 + atoi(token);

				if (cls.doscreenshot == 1)
					cls.doscreenshot = 2;

				continue;
			}
			else if (token[1] == 'b')
			{
				token = COM_Parse (&s);
				y = viddef.height + atoi(token);
				continue;
			}
			else if (token[1] == 't')
			{
				token = COM_Parse (&s);
				y = atoi(token);
				continue;
			}
			break;
		case 'c':
			if (!strcmp(token, "client"))
			{	// draw a deathmatch client block
				int		score, ping, time;

				token = COM_Parse (&s);
				x = viddef.width/2 - 160 + atoi(token);
				token = COM_Parse (&s);
				y = viddef.height/2 - 120 + atoi(token);
				SCR_AddDirtyPoint (x, y);
				SCR_AddDirtyPoint (x+159, y+31);

				token = COM_Parse (&s);
				value = atoi(token);

				if ((unsigned)value >= MAX_CLIENTS)
					Com_Error (ERR_DROP, "Bad client index %d in block 'client' whilst parsing layout string", value);

				ci = &cl.clientinfo[value];

				token = COM_Parse (&s);
				score = atoi(token);

				token = COM_Parse (&s);
				ping = atoi(token);

				token = COM_Parse (&s);
				time = atoi(token);

				DrawAltString (x+32, y, ci->name);
				DrawString (x+32, y+8,  "Score: ");
				DrawAltString (x+32+7*8, y+8,  va("%i", score));
				DrawString (x+32, y+16, va("Ping:  %i", ping));
				DrawString (x+32, y+24, va("Time:  %i", time));

				if (!ci->icon)
					ci = &cl.baseclientinfo;
				Draw_Pic (x, y, ci->iconname, cl_hudalpha->value); //model pic
				continue;
			}
			else if (token[1] == 't' && token[2] == 'f' && !token[3])
			{	// draw a ctf client block
				int		score, ping;
				char	block[80];

				token = COM_Parse (&s);
				x = viddef.width/2 - 160 + atoi(token);
				token = COM_Parse (&s);
				y = viddef.height/2 - 120 + atoi(token);
				SCR_AddDirtyPoint (x, y);
				SCR_AddDirtyPoint (x+159, y+31);

				token = COM_Parse (&s);

				value = atoi(token);
				
				if ((unsigned)value >= MAX_CLIENTS)
					Com_Error (ERR_DROP, "Bad client index %d in block 'ctf' whilst parsing layout string", value);

				ci = &cl.clientinfo[value];

				token = COM_Parse (&s);
				score = atoi(token);

				token = COM_Parse (&s);
				ping = atoi(token);
				if (ping > 999)
					ping = 999;

				Com_sprintf(block, sizeof(block), "%3d %3d %-12.12s", score, ping, ci->name);

				if (value == cl.playernum)
					DrawAltString (x, y, block);
				else
					DrawString (x, y, block);
				continue;
			}
			else if (!strncmp(token, "cstring", 7))
			{
				if (!token[7])
				{
					token = COM_Parse (&s);
					DrawHUDString (token, x, y, 320, false);
					continue;
				}
				else if (token[7] == '2' && !token[8])
				{
					token = COM_Parse (&s);
					DrawHUDString (token, x, y, 320, true);
					continue;
				}
			}
			break;
		case 's':
			if (!strncmp(token, "string", 6))
			{
				if (!token[6])
				{
					token = COM_Parse (&s);
					DrawString (x, y, token);
					continue;
				}
				else if (token[6] == '2' && token[7] == 0)
				{
					token = COM_Parse (&s);
					DrawAltString (x, y, token);
					continue;
				}
			}
			else if (!strcmp(token, "stat_string"))
			{
				token = COM_Parse (&s);
				index = atoi(token);

				if ((unsigned)index >= MAX_STATS)
					Com_Error (ERR_DROP, "Bad stats index %d in block 'stat_string' whilst parsing layout string", index);

				index = cl.frame.playerstate.stats[index];

				if ((unsigned)index >= MAX_CONFIGSTRINGS)
					Com_Error (ERR_DROP, "Bad stat_string index %d whilst parsing layout string", index);

				DrawString (x, y, cl.configstrings[index]);
				continue;
			}
			break;
		case 'p':
			if (token[1] == 'i' && token[2] == 'c')
			{
				if (!token[3])
				{	// draw a pic from a stat number
					token = COM_Parse (&s);

					index = atoi(token);

					if ((unsigned)index >= MAX_STATS)
						Com_Error (ERR_DROP, "Bad stats index %d in block 'pic' whilst parsing layout string", index);

					value = cl.frame.playerstate.stats[index];

					if ((unsigned)value >= MAX_IMAGES)
						Com_Error (ERR_DROP, "Bad picture index %d in block 'pic' whilst parsing layout string", value);

					if (cl.configstrings[CS_IMAGES+value][0])
					{
						SCR_AddDirtyPoint (x, y);
						SCR_AddDirtyPoint (x+23, y+23);
						Draw_Pic (x, y, cl.configstrings[CS_IMAGES+value], cl_hudalpha->value); //hud icons
					}
				}
				else if (token[3] == 'n' && !token[4])
				{	// draw a pic from a name
					token = COM_Parse (&s);
					SCR_AddDirtyPoint (x, y);
					SCR_AddDirtyPoint (x+23, y+23);
					Draw_Pic (x, y, token, cl_hudalpha->value); //tag
				}
				continue;
			}
			break;
		case 'n':
			if (token[1] == 'u' && token[2] == 'm' && !token[3])
			{	// draw a number
				int index;

				token = COM_Parse (&s);
				width = atoi(token);
				token = COM_Parse (&s);

				index = atoi(token);

				if ((unsigned)index >= MAX_STATS)
					Com_Error (ERR_DROP, "Bad stats index %d in block 'num' whilst parsing layout string", index);

				value = cl.frame.playerstate.stats[index];
				SCR_DrawField (x, y, 0, width, value);
				continue;
			}
			break;
		case 'i':
			if (token[1] == 'f' && !token[2])
			{	// draw a number
				token = COM_Parse (&s);
				index = atoi(token);

				if ((unsigned)index >= MAX_STATS)
					Com_Error (ERR_DROP, "Bad stats index %d in block 'if' whilst parsing layout string", index);

				value = cl.frame.playerstate.stats[index];
				if (!value)
				{	// skip to endif
					/*while (s && strcmp(token, "endif") )
					{
						token = COM_Parse (&s);
					}*/
					//hack for speed
					s = strstr (s, " endif");
					if (s)
						s += 6;
				}
				continue;
			}
			break;
		case 0:
			break;
		default:
			if (token[1] == 'n' && token[2] == 'u' && token[3] == 'm' && !token[4])
			{
				int		color;

				switch (token[0])
				{
				case 'a':
					width = 3;
					value = cl.frame.playerstate.stats[STAT_AMMO];
					if (value > 5)
						color = 0;	// green
					else if (value >= 0)
						color = (cl.frame.serverframe>>2) & 1;		// flash
					else
						continue;	// negative number = don't show

					if (cl.frame.playerstate.stats[STAT_FLASHES] & 4)
						Draw_Pic (x, y, "field_3", cl_hudalpha->value); //ammo flash

					SCR_DrawField (x, y, color, width, value);
					continue;

				case 'h':
					// health number
					width = 3;
					value = cl.frame.playerstate.stats[STAT_HEALTH];
					if (value > 25)
						color = 0;	// green
					else if (value > 0)
						color = (cl.frame.serverframe>>2) & 1;		// flash
					else
						color = 1;

					if (cl.frame.playerstate.stats[STAT_FLASHES] & 1)
						Draw_Pic (x, y, "field_3", cl_hudalpha->value); //health flash

					SCR_DrawField (x, y, color, width, value);
					continue;

				case 'r':
					width = 3;
					value = cl.frame.playerstate.stats[STAT_ARMOR];
					if (value < 1)
						continue;

					color = 0;	// green

					if (cl.frame.playerstate.stats[STAT_FLASHES] & 2)
						Draw_Pic (x, y, "field_3", cl_hudalpha->value);

					SCR_DrawField (x, y, color, width, value);
					continue;
				}
			}
			break;
		}
	}
}


/*
================
SCR_DrawStats

The status bar is a small layout program that
is based on the stats array
================
*/
void SCR_DrawStats (void)
{
	SCR_ExecuteLayoutString (cl.configstrings[CS_STATUSBAR]);
}


/*
================
SCR_DrawLayout

================
*/
//#define	STAT_LAYOUTS		13
#if 0
static void SCR_DrawLayout (void)
{
	if (!cl.frame.playerstate.stats[STAT_LAYOUTS])
		return;
	SCR_ExecuteLayoutString (cl.layout);
}
#endif
//=======================================================

/*
==================
SCR_DrawScreenFrame
==================
*/
static void SCR_DrawScreenFrame( float separation )
{
	R_BeginFrame( separation );

	if (scr_draw_loading == 2)
	{	//  loading plaque over black screen
		int		w, h;

		R_CinematicSetPalette(NULL);
		scr_draw_loading = false;
		Draw_GetPicSize (&w, &h, "loading");
		Draw_Pic ((viddef.width-w)* 0.5f, (viddef.height-h)* 0.5f, "loading", 1);

		return;
	} 
	// if a cinematic is supposed to be running, handle menus
	// and console specially
	if (cl.cinematictime > 0)
	{
		if (cls.key_dest == key_menu)
		{
			if (cl.cinematicpalette_active)
			{
				R_CinematicSetPalette(NULL);
				cl.cinematicpalette_active = false;
			}
			M_Draw ();
		}
		else if (cls.key_dest == key_console)
		{
			if (cl.cinematicpalette_active)
			{
				R_CinematicSetPalette(NULL);
				cl.cinematicpalette_active = false;
			}
			SCR_DrawConsole ();
		}
		else
		{
			SCR_DrawCinematic();
		}
		return;
	}

	// make sure the game palette is active
	if (cl.cinematicpalette_active)
	{
		R_CinematicSetPalette(NULL);
		cl.cinematicpalette_active = false;
	}

	// do 3D refresh drawing, and then update the screen
	SCR_CalcVrect ();

	// clear any dirty part of the background
	SCR_TileClear ();

	if (cls.state == ca_active)
	{

		V_RenderView ( separation );

		if (scr_timegraph->integer)
			SCR_DebugGraph (cls.frametime*300, 0);

		if (scr_debuggraph->integer || scr_timegraph->integer || scr_netgraph->integer)
			SCR_DrawDebugGraph ();

		SCR_Draw2D();
	}

	SCR_DrawPause ();

	SCR_DrawConsole ();

	SCR_DrawLoading ();

	M_Draw ();
}

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void SCR_UpdateScreen (void)
{
	// if the screen is disabled (loading plaque is up, or vid mode changing)
	// do nothing at all
	if (cls.disable_screen)
	{
		if (Sys_Milliseconds() - cls.disable_screen > 120000)
		{
			cls.disable_screen = 0;
			Com_Printf ("Loading plaque timed out.\n");
		}
		return;
	}

	if (!scr_initialized)
		return;				// not initialized yet

	if ( cl_stereo->value )
	{
		SCR_DrawScreenFrame( -cl_stereo_separation->value / 2 );
		SCR_DrawScreenFrame( cl_stereo_separation->value / 2 );
	}
	else
	{
		SCR_DrawScreenFrame( 0 );
	}

	R_EndFrame();
}
