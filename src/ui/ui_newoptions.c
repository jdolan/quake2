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

#include "ui_local.h"


#define OPTS_CONSOLE	0
#define OPTS_CROSSHAIR	1
#define OPTS_MISC		2
#define OPTS_TOTAL		3

int curmenu = 0;

static menuframework_s	s_options_menu2[OPTS_TOTAL];
static menulist_s		s_options_list;

extern qboolean m_entersound;

static void UpdateMenuFunc( void *unused )
{
	curmenu = s_options_list.curvalue;
	m_entersound = true;
	M_PopMenu();
	M_PushMenu ( &s_options_menu2[curmenu] );
	m_entersound = false;

}

static void SetMenuItemValues( void );

/* ------------------------ */
/*		console menu		*/
/* ------------------------ */

static menulist_s		s_console_notfade_box;
static menuslider_s		s_console_notlines_slider;
static menuslider_s		s_console_alpha_slider;
static menuslider_s		s_console_drop_slider;
static menuslider_s		s_console_scroll_slider;
static menuaction_s		s_console_defaults_action;


static void ConsoleNotFadeFunc( void *unused )
{
	Cvar_SetValue( "con_notifyfade", s_console_notfade_box.curvalue );
}

static void ConsoleNotLinesFunc( void *unused )
{
	Cvar_SetValue( "con_notifylines", s_console_notlines_slider.curvalue );
}

static void ConsoleAlphaFunc( void *unused )
{
	Cvar_SetValue( "con_alpha", s_console_alpha_slider.curvalue / 10 );
}

static void ConsoleDropFunc( void *unused )
{
	Cvar_SetValue( "scr_conheight", s_console_drop_slider.curvalue / 10 );
}

static void ConsoleScrollFunc( void *unused )
{
	Cvar_SetValue( "con_scrlines", s_console_scroll_slider.curvalue );
}

void Cvar_SetDefault(const char *var_name);

static void ResetConsoleDefaultsFunc( void *unused )
{

	Cvar_SetDefault("con_notifyfade");
	Cvar_SetDefault("con_notifylines");
	Cvar_SetDefault("con_alpha");
	Cvar_SetDefault("scr_conheight");
	Cvar_SetDefault("con_scrlines");

	SetMenuItemValues();
}

/* ------------------------ */
/*		crosshair menu		*/
/* ------------------------ */

static menulist_s		s_crosshair_number_box;
static menuslider_s		s_crosshair_alpha_slider;
static menuslider_s		s_crosshair_pulse_slider;
static menuslider_s		s_crosshair_scale_slider;
static menuslider_s		s_crosshair_red_slider;
static menuslider_s		s_crosshair_green_slider;
static menuslider_s		s_crosshair_blue_slider;
static menuaction_s		s_crosshair_defaults_action;

static void CrosshairFunc2( void *unused )
{
	Cvar_SetValue( "crosshair", s_crosshair_number_box.curvalue );
}

static void CrosshairAlphaFunc( void *unused )
{
	Cvar_SetValue( "ch_alpha", s_crosshair_alpha_slider.curvalue / 10 );
}

static void CrosshairPulseFunc( void *unused )
{
	Cvar_SetValue( "ch_pulse", s_crosshair_pulse_slider.curvalue / 10 );
}

static void CrosshairScaleFunc( void *unused )
{
	Cvar_SetValue( "ch_scale", s_crosshair_scale_slider.curvalue / 10 );
}

static void CrosshairRedFunc( void *unused )
{
	Cvar_SetValue( "ch_red", s_crosshair_red_slider.curvalue / 10 );
}

static void CrosshairGreenFunc( void *unused )
{
	Cvar_SetValue( "ch_green", s_crosshair_green_slider.curvalue / 10 );
}

static void CrosshairBlueFunc( void *unused )
{
	Cvar_SetValue( "ch_blue", s_crosshair_blue_slider.curvalue / 10 );
}

static void ResetCrosshairDefaultsFunc( void *unused )
{

	Cvar_SetDefault("crosshair");
	Cvar_SetDefault("ch_alpha");
	Cvar_SetDefault("ch_pulse");
	Cvar_SetDefault("ch_scale");
	Cvar_SetDefault("ch_red");
	Cvar_SetDefault("ch_green");
	Cvar_SetDefault("ch_blue");

	SetMenuItemValues();
}

/* ------------------------ */
/*		misc menu			*/
/* ------------------------ */

static menulist_s	s_misc_showclock_box;
static menulist_s	s_misc_showfps_box;
static menulist_s	s_misc_showchat_box;
static menulist_s	s_misc_showtime_box;
static menulist_s	s_misc_timestamp_box;
static menuslider_s	s_misc_hudalpha_slider;
static menuaction_s	s_misc_defaults_action;

static void ShowCLOCKFunc( void *unused )
{
	Cvar_SetValue( "cl_clock", s_misc_showclock_box.curvalue );
}

static void ShowFPSFunc( void *unused )
{
	Cvar_SetValue( "cl_fps", s_misc_showfps_box.curvalue );
}

static void ShowCHATFunc( void *unused )
{
	Cvar_SetValue( "cl_chathud", s_misc_showchat_box.curvalue );
}

static void ShowTIMEFunc( void *unused )
{
	Cvar_SetValue( "cl_maptime", s_misc_showtime_box.curvalue );
}

static void TimeStampFunc( void *unused )
{
	Cvar_SetValue( "cl_timestamps", s_misc_timestamp_box.curvalue );
}

static void HudAlphaFunc( void *unused )
{
	Cvar_SetValue( "cl_hudalpha", s_misc_hudalpha_slider.curvalue / 10 );
}

static void ResetMiscDefaultsFunc( void *unused )
{

	Cvar_SetDefault("cl_clock");
	Cvar_SetDefault("cl_fps");
	Cvar_SetDefault("cl_chathud");
	Cvar_SetDefault("cl_maptime");
	Cvar_SetDefault("cl_timestamps");
	Cvar_SetDefault("cl_hudalpha");

	SetMenuItemValues();
}

/* ------------------------ */
/*		in all menus		*/
/* ------------------------ */

float ClampCvar( float min, float max, float value );

static void SetMenuItemValues( void )
{
	s_options_list.curvalue = curmenu;


	/* -------------------------------- */
	/*			console menu			*/


	Cvar_SetValue( "con_notifyfade", ClampCvar( 0, 1, con_notifyfade->value ) );
	s_console_notfade_box.curvalue		= con_notifyfade->value;

	s_console_notlines_slider.curvalue	= con_notifylines->value;
	s_console_alpha_slider.curvalue		= con_alpha->value * 10;
	s_console_drop_slider.curvalue		= scr_conheight->value * 10;
	s_console_scroll_slider.curvalue	= con_scrlines->value;

	/* -------------------------------- */
	/*			crosshair menu			*/

	Cvar_SetValue( "crosshair", ClampCvar( 0, 8, crosshair->value ) );
	s_crosshair_number_box.curvalue			= crosshair->value;

	s_crosshair_alpha_slider.curvalue		= ch_alpha->value*10;
	s_crosshair_pulse_slider.curvalue		= ch_pulse->value*10;
	s_crosshair_scale_slider.curvalue		= ch_scale->value*10;

	s_crosshair_red_slider.curvalue			= ch_red->value*10;
	s_crosshair_green_slider.curvalue		= ch_green->value*10;
	s_crosshair_blue_slider.curvalue		= ch_blue->value*10;

	/* -------------------------------- */
	/*			misc menu				*/

	Cvar_SetValue( "cl_clock", ClampCvar( 0, 2, cl_clock->value ) );
	s_misc_showclock_box.curvalue				= cl_clock->value;

	Cvar_SetValue( "cl_fps", ClampCvar( 0, 2, cl_fps->value ) );
	s_misc_showfps_box.curvalue					= cl_fps->value;

	Cvar_SetValue( "cl_chathud", ClampCvar( 0, 2, cl_chathud->value ) );
	s_misc_showchat_box.curvalue				= cl_chathud->value;

	Cvar_SetValue( "cl_maptime", ClampCvar( 0, 2, cl_maptime->value ) );
	s_misc_showtime_box.curvalue				= cl_maptime->value;

	Cvar_SetValue( "cl_timestamps", ClampCvar( 0, 2, cl_timestamps->value ) );
	s_misc_timestamp_box.curvalue				= cl_timestamps->value;

	Cvar_SetValue( "cl_hudalpha", ClampCvar( 0, 1, cl_hudalpha->value ) );
	s_misc_hudalpha_slider.curvalue				= cl_hudalpha->value*10;

}
/* ------------------------ */

void NewOptions_MenuDraw (menuframework_s *self)
{
	M_Banner( "m_banner_options" );
	Menu_AdjustCursor( self, 1 );
	Menu_Draw( self );
}

void Options_MenuInit2( void )
{
	static const char *menu_items[] =
	{
		"[ console  >",
		"< crosshair >",
		"< misc.     ]",
		0
	};

	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0
	};

	static const char *drawthings_names[] =
	{
		"no",
		"color 1",
		"color 2",
		0
	};

	static const char *crosshair_names[] =
	{
		"none",
		"one",
		"two",
		"three",
		"four",
		"five",
		"six",
		"seven",
		"eight",
		0
	};

	static const char *timestamp_names[] =
	{
		"off",
		"chat text only",
		"all messages",
		0
	};

	int y, i;

	memset(&s_options_menu2, 0, sizeof(&s_options_menu2));
	/*
	** configure controls menu and menu items
	*/
	for (i=0 ; i<OPTS_TOTAL ; i++)
	{
		s_options_menu2[i].x = viddef.width / 2;
		s_options_menu2[i].y = viddef.height / 2 - 58;
		s_options_menu2[i].nitems = 0;
	}

	s_options_list.generic.type			= MTYPE_SPINCONTROL;
	s_options_list.generic.x			= 0;
	s_options_list.generic.y			= 0;
	s_options_list.generic.name			= "";
	s_options_list.generic.callback		= UpdateMenuFunc;
	s_options_list.itemnames			= menu_items;
	s_options_list.generic.statusbar	= "Select Your Menu";


	/* ------------------------------------ */
	/*				CONSOLE					*/
	/* ------------------------------------ */

	s_console_notfade_box.generic.type		= MTYPE_SPINCONTROL;
	s_console_notfade_box.generic.x			= 0;
	s_console_notfade_box.generic.y			= y = 20;
	s_console_notfade_box.generic.name		= "notify fading";
	s_console_notfade_box.generic.callback	= ConsoleNotFadeFunc;
	s_console_notfade_box.itemnames			= yesno_names;
	s_console_notfade_box.generic.statusbar	= "Notifyline Fading";

	s_console_notlines_slider.generic.type		= MTYPE_SLIDER;
	s_console_notlines_slider.generic.x			= 0;
	s_console_notlines_slider.generic.y			= y += 10;
	s_console_notlines_slider.generic.name		= "notify lines";
	s_console_notlines_slider.generic.callback	= ConsoleNotLinesFunc;
	s_console_notlines_slider.minvalue			= 1;
	s_console_notlines_slider.maxvalue			= 8;
	s_console_notlines_slider.generic.statusbar	= "Maximum Notifylines";

	s_console_alpha_slider.generic.type			= MTYPE_SLIDER;
	s_console_alpha_slider.generic.x			= 0;
	s_console_alpha_slider.generic.y			= y += 20;
	s_console_alpha_slider.generic.name			= "alpha";
	s_console_alpha_slider.generic.callback		= ConsoleAlphaFunc;
	s_console_alpha_slider.minvalue				= 0;
	s_console_alpha_slider.maxvalue				= 10;
	s_console_alpha_slider.generic.statusbar	= "Console Alpha";

	s_console_drop_slider.generic.type		= MTYPE_SLIDER;
	s_console_drop_slider.generic.x			= 0;
	s_console_drop_slider.generic.y			= y += 10;
	s_console_drop_slider.generic.name		= "drop height";
	s_console_drop_slider.generic.callback	= ConsoleDropFunc;
	s_console_drop_slider.minvalue			= 1;
	s_console_drop_slider.maxvalue			= 10;
	s_console_drop_slider.generic.statusbar	= "Console Drop Height";

	s_console_scroll_slider.generic.type		= MTYPE_SLIDER;
	s_console_scroll_slider.generic.x			= 0;
	s_console_scroll_slider.generic.y			= y += 10;
	s_console_scroll_slider.generic.name		= "scroll lines";
	s_console_scroll_slider.generic.callback	= ConsoleScrollFunc;
	s_console_scroll_slider.minvalue			= 1;
	s_console_scroll_slider.maxvalue			= 10;
	s_console_scroll_slider.generic.statusbar	= "Scrolling Lines with PGUP/DN or MWHEELUP/DN";

	s_console_defaults_action.generic.type		= MTYPE_ACTION;
	s_console_defaults_action.generic.x		= 0;
	s_console_defaults_action.generic.y		= y = 150;
	s_console_defaults_action.generic.name		= "reset defaults";
	s_console_defaults_action.generic.callback	= ResetConsoleDefaultsFunc;
	s_console_defaults_action.generic.statusbar= "Reset Console Changes";

	/* ------------------------------------ */
	/*				CROSSHAIR				*/
	/* ------------------------------------ */

	s_crosshair_number_box.generic.type			= MTYPE_SPINCONTROL;
	s_crosshair_number_box.generic.x			= 0;
	s_crosshair_number_box.generic.y			= y = 20;
	s_crosshair_number_box.generic.name			= "crosshair";
	s_crosshair_number_box.generic.callback		= CrosshairFunc2;
	s_crosshair_number_box.itemnames			= crosshair_names;
	s_crosshair_number_box.generic.statusbar	= "Crosshair Number";

	s_crosshair_alpha_slider.generic.type		= MTYPE_SLIDER;
	s_crosshair_alpha_slider.generic.x			= 0;
	s_crosshair_alpha_slider.generic.y			= y += 20;
	s_crosshair_alpha_slider.generic.name		= "alpha";
	s_crosshair_alpha_slider.generic.callback	= CrosshairAlphaFunc;
	s_crosshair_alpha_slider.minvalue			= 0;
	s_crosshair_alpha_slider.maxvalue			= 10;
	s_crosshair_alpha_slider.generic.statusbar	= "Crosshair Alpha";

	s_crosshair_pulse_slider.generic.type		= MTYPE_SLIDER;
	s_crosshair_pulse_slider.generic.x			= 0;
	s_crosshair_pulse_slider.generic.y			= y += 10;
	s_crosshair_pulse_slider.generic.name		= "pulse";
	s_crosshair_pulse_slider.generic.callback	= CrosshairPulseFunc;
	s_crosshair_pulse_slider.minvalue			= 0;
	s_crosshair_pulse_slider.maxvalue			= 40;
	s_crosshair_pulse_slider.generic.statusbar	= "Crosshair Alpha Pulsing";

	s_crosshair_scale_slider.generic.type		= MTYPE_SLIDER;
	s_crosshair_scale_slider.generic.x			= 0;
	s_crosshair_scale_slider.generic.y			= y += 10;
	s_crosshair_scale_slider.generic.name		= "scale";
	s_crosshair_scale_slider.generic.callback	= CrosshairScaleFunc;
	s_crosshair_scale_slider.minvalue			= 10;
	s_crosshair_scale_slider.maxvalue			= 80;
	s_crosshair_scale_slider.generic.statusbar	= "Crosshair Scale";

	s_crosshair_red_slider.generic.type			= MTYPE_SLIDER;
	s_crosshair_red_slider.generic.x			= 0;
	s_crosshair_red_slider.generic.y			= y += 20;
	s_crosshair_red_slider.generic.name			= "red";
	s_crosshair_red_slider.generic.callback		= CrosshairRedFunc;
	s_crosshair_red_slider.minvalue				= 0;
	s_crosshair_red_slider.maxvalue				= 10;
	s_crosshair_red_slider.generic.statusbar	= "Crosshair Red Amount";

	s_crosshair_green_slider.generic.type		= MTYPE_SLIDER;
	s_crosshair_green_slider.generic.x			= 0;
	s_crosshair_green_slider.generic.y			= y += 10;
	s_crosshair_green_slider.generic.name		= "green";
	s_crosshair_green_slider.generic.callback	= CrosshairGreenFunc;
	s_crosshair_green_slider.minvalue			= 0;
	s_crosshair_green_slider.maxvalue			= 10;
	s_crosshair_green_slider.generic.statusbar	= "Crosshair Green Amount";

	s_crosshair_blue_slider.generic.type		= MTYPE_SLIDER;
	s_crosshair_blue_slider.generic.x			= 0;
	s_crosshair_blue_slider.generic.y			= y += 10;
	s_crosshair_blue_slider.generic.name		= "blue";
	s_crosshair_blue_slider.generic.callback	= CrosshairBlueFunc;
	s_crosshair_blue_slider.minvalue			= 0;
	s_crosshair_blue_slider.maxvalue			= 10;
	s_crosshair_blue_slider.generic.statusbar	= "Crosshair Blue Amount";

	s_crosshair_defaults_action.generic.type	= MTYPE_ACTION;
	s_crosshair_defaults_action.generic.x		= 0;
	s_crosshair_defaults_action.generic.y		= y = 150;
	s_crosshair_defaults_action.generic.name	= "reset defaults";
	s_crosshair_defaults_action.generic.callback= ResetCrosshairDefaultsFunc;
	s_crosshair_defaults_action.generic.statusbar= "Reset Crosshair Changes";

	/* ------------------------------------ */
	/*				MISC					*/
	/* ------------------------------------ */

	s_misc_showclock_box.generic.type		= MTYPE_SPINCONTROL;
	s_misc_showclock_box.generic.x			= 0;
	s_misc_showclock_box.generic.y			= y = 20;
	s_misc_showclock_box.generic.name		= "clock";
	s_misc_showclock_box.generic.callback	= ShowCLOCKFunc;
	s_misc_showclock_box.itemnames			= drawthings_names;
	s_misc_showclock_box.generic.statusbar	= "Clock";

	s_misc_showfps_box.generic.type			= MTYPE_SPINCONTROL;
	s_misc_showfps_box.generic.x			= 0;
	s_misc_showfps_box.generic.y			= y += 10;
	s_misc_showfps_box.generic.name			= "fps display";
	s_misc_showfps_box.generic.callback		= ShowFPSFunc;
	s_misc_showfps_box.itemnames			= drawthings_names;
	s_misc_showfps_box.generic.statusbar	= "Framerate Display";

	s_misc_showchat_box.generic.type		= MTYPE_SPINCONTROL;
	s_misc_showchat_box.generic.x			= 0;
	s_misc_showchat_box.generic.y			= y += 10;
	s_misc_showchat_box.generic.name		= "chat hud";
	s_misc_showchat_box.generic.callback	= ShowCHATFunc;
	s_misc_showchat_box.itemnames			= drawthings_names;
	s_misc_showchat_box.generic.statusbar	= "Chat HUD";

	s_misc_showtime_box.generic.type		= MTYPE_SPINCONTROL;
	s_misc_showtime_box.generic.x			= 0;
	s_misc_showtime_box.generic.y			= y += 10;
	s_misc_showtime_box.generic.name		= "map timer";
	s_misc_showtime_box.generic.callback	= ShowTIMEFunc;
	s_misc_showtime_box.itemnames			= drawthings_names;
	s_misc_showtime_box.generic.statusbar	= "Map Timer";

	s_misc_timestamp_box.generic.type		= MTYPE_SPINCONTROL;
	s_misc_timestamp_box.generic.x			= 0;
	s_misc_timestamp_box.generic.y			= y += 10;
	s_misc_timestamp_box.generic.name		= "timestamp";
	s_misc_timestamp_box.generic.callback	= TimeStampFunc;
	s_misc_timestamp_box.itemnames			= timestamp_names;
	s_misc_timestamp_box.generic.statusbar	= "Message Timestamping";

	s_misc_hudalpha_slider.generic.type		= MTYPE_SLIDER;
	s_misc_hudalpha_slider.generic.x		= 0;
	s_misc_hudalpha_slider.generic.y		= y += 10;
	s_misc_hudalpha_slider.generic.name		= "hud alpha";
	s_misc_hudalpha_slider.generic.callback	= HudAlphaFunc;
	s_misc_hudalpha_slider.minvalue			= 0;
	s_misc_hudalpha_slider.maxvalue			= 10;
	s_misc_hudalpha_slider.generic.statusbar= "Hud Alpha";

	s_misc_defaults_action.generic.type		= MTYPE_ACTION;
	s_misc_defaults_action.generic.x		= 0;
	s_misc_defaults_action.generic.y		= y = 150;
	s_misc_defaults_action.generic.name		= "reset defaults";
	s_misc_defaults_action.generic.callback	= ResetMiscDefaultsFunc;
	s_misc_defaults_action.generic.statusbar= "Reset Misc Changes";


	/* ------------------------------------ */
	/*				ADD ITEMS				*/
	/* ------------------------------------ */

	SetMenuItemValues();

	for (i=0 ; i<OPTS_TOTAL ; i++) {
		s_options_menu2[i].draw = NewOptions_MenuDraw;
		s_options_menu2[i].key = NULL;
		Menu_AddItem( &s_options_menu2[i],			(void *) &s_options_list );
	}

	Menu_AddItem( &s_options_menu2[OPTS_CROSSHAIR],	(void *) &s_crosshair_number_box );
	Menu_AddItem( &s_options_menu2[OPTS_CROSSHAIR],	(void *) &s_crosshair_alpha_slider );
	Menu_AddItem( &s_options_menu2[OPTS_CROSSHAIR],	(void *) &s_crosshair_pulse_slider );
	Menu_AddItem( &s_options_menu2[OPTS_CROSSHAIR],	(void *) &s_crosshair_scale_slider );
	Menu_AddItem( &s_options_menu2[OPTS_CROSSHAIR],	(void *) &s_crosshair_red_slider );
	Menu_AddItem( &s_options_menu2[OPTS_CROSSHAIR],	(void *) &s_crosshair_green_slider );
	Menu_AddItem( &s_options_menu2[OPTS_CROSSHAIR],	(void *) &s_crosshair_blue_slider );
	Menu_AddItem( &s_options_menu2[OPTS_CROSSHAIR],	(void *) &s_crosshair_defaults_action );

	Menu_AddItem( &s_options_menu2[OPTS_CONSOLE],	(void *) &s_console_notfade_box);
	Menu_AddItem( &s_options_menu2[OPTS_CONSOLE],	(void *) &s_console_notlines_slider);
	Menu_AddItem( &s_options_menu2[OPTS_CONSOLE],	(void *) &s_console_alpha_slider);
	Menu_AddItem( &s_options_menu2[OPTS_CONSOLE],	(void *) &s_console_drop_slider);
	Menu_AddItem( &s_options_menu2[OPTS_CONSOLE],	(void *) &s_console_scroll_slider);
	Menu_AddItem( &s_options_menu2[OPTS_CONSOLE],	(void *) &s_console_defaults_action);

	Menu_AddItem( &s_options_menu2[OPTS_MISC],		(void *) &s_misc_showclock_box );
	Menu_AddItem( &s_options_menu2[OPTS_MISC],		(void *) &s_misc_showfps_box );
	Menu_AddItem( &s_options_menu2[OPTS_MISC],		(void *) &s_misc_showchat_box );
	Menu_AddItem( &s_options_menu2[OPTS_MISC],		(void *) &s_misc_showtime_box );
	Menu_AddItem( &s_options_menu2[OPTS_MISC],		(void *) &s_misc_timestamp_box );
	Menu_AddItem( &s_options_menu2[OPTS_MISC],		(void *) &s_misc_hudalpha_slider );
	Menu_AddItem( &s_options_menu2[OPTS_MISC],		(void *) &s_misc_defaults_action );

}


void M_Menu_NewOptions_f (void)
{
	Options_MenuInit2();
	M_PushMenu ( &s_options_menu2[curmenu] );
}
