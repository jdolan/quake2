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

/*
=======================================================================

CONTROLS MENU

=======================================================================
*/

#ifdef JOYSTICK
extern cvar_t *in_joystick;
#endif

static menuframework_s	s_options_menu;
static menuaction_s		s_options_defaults_action;
static menuaction_s		s_options_customize_options_action;
static menuaction_s		s_options_new_options_action;
static menuaction_s		s_options_demos_action;
#if defined(_WIN32) || defined(WITH_XMMS)
static menuaction_s		s_options_mp3_action;
#endif
static menuslider_s		s_options_sensitivity_slider;
static menulist_s		s_options_freelook_box;
//static menulist_s		s_options_noalttab_box;
static menulist_s		s_options_alwaysrun_box;
static menulist_s		s_options_invertmouse_box;
static menulist_s		s_options_lookspring_box;
static menulist_s		s_options_lookstrafe_box;
static menulist_s		s_options_crosshair_box;
static menuslider_s		s_options_sfxvolume_slider;
#ifdef JOYSTICK
static menulist_s		s_options_joystick_box;
#endif
#ifdef CD_AUDIO
static menulist_s		s_options_cdvolume_box;
#endif
static menulist_s		s_options_quality_list;
static menulist_s		s_options_compatibility_list;
static menulist_s		s_options_console_action;

static void CrosshairFunc( void *unused )
{
	Cvar_SetValue( "crosshair", s_options_crosshair_box.curvalue );
}

#ifdef JOYSTICK
static void JoystickFunc( void *unused )
{
	Cvar_SetValue( "in_joystick", s_options_joystick_box.curvalue );
}
#endif

static void CustomizeControlsFunc( void *unused )
{
	M_Menu_Keys_f();
}

static void NewOptionsFunc( void *unused )
{
	M_Menu_NewOptions_f();
}

static void DemosFunc( void *unused )
{
	M_Menu_Demos_f();
}

#if defined(_WIN32) || defined(WITH_XMMS)
static void MP3Func( void *unused )
{
	M_Menu_MP3_f();
}
#endif

static void AlwaysRunFunc( void *unused )
{
	Cvar_SetValue( "cl_run", s_options_alwaysrun_box.curvalue );
}

static void FreeLookFunc( void *unused )
{
	Cvar_SetValue( "freelook", s_options_freelook_box.curvalue );
}

static void MouseSpeedFunc( void *unused )
{
	Cvar_SetValue( "sensitivity", s_options_sensitivity_slider.curvalue / 2.0F );
}

float ClampCvar( float min, float max, float value )
{
	if ( value < min ) return min;
	if ( value > max ) return max;
	return value;
}

static void ControlsSetMenuItemValues( void )
{
	s_options_sfxvolume_slider.curvalue		= Cvar_VariableValue( "s_volume" ) * 10;
#ifdef CD_AUDIO
	s_options_cdvolume_box.curvalue 		= !Cvar_VariableValue("cd_nocd");
#endif

	switch((int)Cvar_VariableValue("s_khz"))
	{
		case 48: s_options_quality_list.curvalue = 3; break;
		case 44: s_options_quality_list.curvalue = 2; break;
		case 22: s_options_quality_list.curvalue = 1; break;
		default: s_options_quality_list.curvalue = 0; break;
	}

	//s_options_quality_list.curvalue			= !Cvar_VariableValue( "s_loadas8bit" );
	s_options_sensitivity_slider.curvalue	= ( sensitivity->value ) * 2;

	Cvar_SetValue( "cl_run", ClampCvar( 0, 1, cl_run->value ) );
	s_options_alwaysrun_box.curvalue		= cl_run->value;

	s_options_invertmouse_box.curvalue		= m_pitch->value < 0;

	Cvar_SetValue( "lookspring", ClampCvar( 0, 1, lookspring->value ) );
	s_options_lookspring_box.curvalue		= lookspring->value;

	Cvar_SetValue( "lookstrafe", ClampCvar( 0, 1, lookstrafe->value ) );
	s_options_lookstrafe_box.curvalue		= lookstrafe->value;

	Cvar_SetValue( "freelook", ClampCvar( 0, 1, freelook->value ) );
	s_options_freelook_box.curvalue			= freelook->value;

	Cvar_SetValue( "crosshair", ClampCvar( 0, 3, crosshair->value ) );
	s_options_crosshair_box.curvalue		= crosshair->value;

#ifdef JOYSTICK
	Cvar_SetValue( "in_joystick", ClampCvar( 0, 1, in_joystick->value ) );
	s_options_joystick_box.curvalue		= in_joystick->value;
#endif

//	s_options_noalttab_box.curvalue			= win_noalttab->value;
}

static void ControlsResetDefaultsFunc( void *unused )
{
	Cbuf_AddText ("exec default.cfg\n");
	Cbuf_Execute();

	ControlsSetMenuItemValues();
}

static void InvertMouseFunc( void *unused )
{
	if ( s_options_invertmouse_box.curvalue == 0 )
	{
		Cvar_SetValue( "m_pitch", (float)fabs( m_pitch->value ) );
	}
	else
	{
		Cvar_SetValue( "m_pitch", -(float)fabs( m_pitch->value ) );
	}
}

static void LookspringFunc( void *unused )
{
	Cvar_SetValue( "lookspring", s_options_lookspring_box.curvalue );
}

static void LookstrafeFunc( void *unused )
{
	Cvar_SetValue( "lookstrafe", s_options_lookstrafe_box.curvalue );
}

static void UpdateVolumeFunc( void *unused )
{
	Cvar_SetValue( "s_volume", s_options_sfxvolume_slider.curvalue / 10 );
}

#ifdef CD_AUDIO
static void UpdateCDVolumeFunc( void *unused )
{
	Cvar_SetValue( "cd_nocd", !s_options_cdvolume_box.curvalue );
}
#endif

extern void Key_ClearTyping( void );

void ConsoleFunc( void *unused )
{
	/*
	** the proper way to do this is probably to have ToggleConsole_f accept a parameter
	*/
	if ( cl.attractloop )
	{
		Cbuf_AddText ("killserver\n");
		return;
	}

	Key_ClearTyping ();
	Con_ClearNotify ();

	M_ForceMenuOff ();
	cls.key_dest = key_console;
}

static void UpdateSoundQualityFunc( void *unused )
{
	switch (s_options_quality_list.curvalue)
	{
	case 0:
		Cvar_SetValue( "s_khz", 11 );
		Cvar_SetValue( "s_loadas8bit", 1 );
		break;
	case 1:
		Cvar_SetValue( "s_khz", 22 );
		Cvar_SetValue( "s_loadas8bit", 0 );
		break;
	case 2:
		Cvar_SetValue( "s_khz", 44 );
		Cvar_SetValue( "s_loadas8bit", 0 );
		break;
	case 3:
		Cvar_SetValue( "s_khz", 48 );
		Cvar_SetValue( "s_loadas8bit", 0 );
		break;
	default:
		Cvar_SetValue( "s_khz", 22 );
		Cvar_SetValue( "s_loadas8bit", 0 );
		break;
	}
	
	Cvar_SetValue( "s_primary", s_options_compatibility_list.curvalue );

	M_DrawTextBox( 8, 120 - 48, 36, 3 );
	M_Print( 16 + 16, 120 - 48 + 8,  "Restarting the sound system. This" );
	M_Print( 16 + 16, 120 - 48 + 16, "could take up to a minute, so" );
	M_Print( 16 + 16, 120 - 48 + 24, "please be patient." );

	// the text box won't show up unless we do a buffer swap
	R_EndFrame();

	CL_Snd_Restart_f();
}

static void Options_MenuDraw ( menuframework_s *self )
{
	M_Banner( "m_banner_options" );
	Menu_AdjustCursor( self, 1 );
	Menu_Draw( self );
}

void Options_MenuInit( void )
{
	int squality = 0, y = 0;

#ifdef CD_AUDIO
	static const char *cd_music_items[] =
	{
		"disabled",
		"enabled",
		0
	};
#endif
	static const char *quality_items[] =
	{
		"Low (11KHz/8-bit)",
		"Normal (22KHz/16-bit)",
		"High (44KHz/16-bit)",
		"Extreme (48KHz/16-bit)",
		0
	};

	static const char *compatibility_items[] =
	{
		"max compatibility", "max performance", 0
	};

	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0
	};

	static const char *crosshair_names[] =
	{
		"none",
		"cross",
		"dot",
		"angle",
		0
	};

	squality = Cvar_VariableIntValue("s_khz");

	switch (squality)	{
		case 11:
			squality = 0;
			break;
		case 22:
			squality = 1;
			break;
		case 44:
			squality = 2;
			break;
		case 48:
			squality = 3;
			break;
		default:
			squality = 1;
			break;
	}

	/*
	** configure controls menu and menu items
	*/
	memset(&s_options_menu, 0, sizeof(s_options_menu));
	s_options_menu.x = viddef.width / 2;
	s_options_menu.y = viddef.height / 2 - 58;
	s_options_menu.nitems = 0;

	s_options_sfxvolume_slider.generic.type	= MTYPE_SLIDER;
	s_options_sfxvolume_slider.generic.x	= 0;
	s_options_sfxvolume_slider.generic.y	= 0;
	s_options_sfxvolume_slider.generic.name	= "effects volume";
	s_options_sfxvolume_slider.generic.callback	= UpdateVolumeFunc;
	s_options_sfxvolume_slider.minvalue		= 0;
	s_options_sfxvolume_slider.maxvalue		= 10;
	s_options_sfxvolume_slider.curvalue		= Cvar_VariableValue( "s_volume" ) * 10;
#ifdef CD_AUDIO
	s_options_cdvolume_box.generic.type	= MTYPE_SPINCONTROL;
	s_options_cdvolume_box.generic.x		= 0;
	s_options_cdvolume_box.generic.y		= y += 10;
	s_options_cdvolume_box.generic.name	= "CD music";
	s_options_cdvolume_box.generic.callback	= UpdateCDVolumeFunc;
	s_options_cdvolume_box.itemnames		= cd_music_items;
	s_options_cdvolume_box.curvalue 		= !Cvar_VariableValue("cd_nocd");
#endif
	s_options_quality_list.generic.type	= MTYPE_SPINCONTROL;
	s_options_quality_list.generic.x		= 0;
	s_options_quality_list.generic.y		= y += 10;
	s_options_quality_list.generic.name		= "sound quality";
	s_options_quality_list.generic.callback = UpdateSoundQualityFunc;
	s_options_quality_list.itemnames		= quality_items;
	s_options_quality_list.curvalue			= squality;

	s_options_compatibility_list.generic.type	= MTYPE_SPINCONTROL;
	s_options_compatibility_list.generic.x		= 0;
	s_options_compatibility_list.generic.y		= y += 10;
	s_options_compatibility_list.generic.name	= "sound compatibility";
	s_options_compatibility_list.generic.callback = UpdateSoundQualityFunc;
	s_options_compatibility_list.itemnames		= compatibility_items;
	s_options_compatibility_list.curvalue		= Cvar_VariableIntValue( "s_primary" );

	s_options_sensitivity_slider.generic.type	= MTYPE_SLIDER;
	s_options_sensitivity_slider.generic.x		= 0;
	s_options_sensitivity_slider.generic.y		= y += 20;
	s_options_sensitivity_slider.generic.name	= "mouse speed";
	s_options_sensitivity_slider.generic.callback = MouseSpeedFunc;
	s_options_sensitivity_slider.minvalue		= 2;
	s_options_sensitivity_slider.maxvalue		= 22;

	s_options_alwaysrun_box.generic.type = MTYPE_SPINCONTROL;
	s_options_alwaysrun_box.generic.x	= 0;
	s_options_alwaysrun_box.generic.y	= y += 10;
	s_options_alwaysrun_box.generic.name	= "always run";
	s_options_alwaysrun_box.generic.callback = AlwaysRunFunc;
	s_options_alwaysrun_box.itemnames = yesno_names;

	s_options_invertmouse_box.generic.type = MTYPE_SPINCONTROL;
	s_options_invertmouse_box.generic.x	= 0;
	s_options_invertmouse_box.generic.y	= y += 10;
	s_options_invertmouse_box.generic.name	= "invert mouse";
	s_options_invertmouse_box.generic.callback = InvertMouseFunc;
	s_options_invertmouse_box.itemnames = yesno_names;

	s_options_lookspring_box.generic.type = MTYPE_SPINCONTROL;
	s_options_lookspring_box.generic.x	= 0;
	s_options_lookspring_box.generic.y	= y += 10;
	s_options_lookspring_box.generic.name	= "lookspring";
	s_options_lookspring_box.generic.callback = LookspringFunc;
	s_options_lookspring_box.itemnames = yesno_names;

	s_options_lookstrafe_box.generic.type = MTYPE_SPINCONTROL;
	s_options_lookstrafe_box.generic.x	= 0;
	s_options_lookstrafe_box.generic.y	= y += 10;
	s_options_lookstrafe_box.generic.name	= "lookstrafe";
	s_options_lookstrafe_box.generic.callback = LookstrafeFunc;
	s_options_lookstrafe_box.itemnames = yesno_names;

	s_options_freelook_box.generic.type = MTYPE_SPINCONTROL;
	s_options_freelook_box.generic.x	= 0;
	s_options_freelook_box.generic.y	= y += 10;
	s_options_freelook_box.generic.name	= "free look";
	s_options_freelook_box.generic.callback = FreeLookFunc;
	s_options_freelook_box.itemnames = yesno_names;

	s_options_crosshair_box.generic.type = MTYPE_SPINCONTROL;
	s_options_crosshair_box.generic.x	= 0;
	s_options_crosshair_box.generic.y	= y += 10;
	s_options_crosshair_box.generic.name	= "crosshair";
	s_options_crosshair_box.generic.callback = CrosshairFunc;
	s_options_crosshair_box.itemnames = crosshair_names;

#ifdef JOYSTICK
	s_options_joystick_box.generic.type = MTYPE_SPINCONTROL;
	s_options_joystick_box.generic.x	= 0;
	s_options_joystick_box.generic.y	= y += 10;
	s_options_joystick_box.generic.name	= "use joystick";
	s_options_joystick_box.generic.callback = JoystickFunc;
	s_options_joystick_box.itemnames = yesno_names;
#endif

	//Added new options -Maniac
	s_options_new_options_action.generic.type	= MTYPE_ACTION;
	s_options_new_options_action.generic.x		= 0;
	s_options_new_options_action.generic.y		= y += 20;
	s_options_new_options_action.generic.name	= "new options";
	s_options_new_options_action.generic.callback = NewOptionsFunc;

	s_options_demos_action.generic.type	= MTYPE_ACTION;
	s_options_demos_action.generic.x		= 0;
	s_options_demos_action.generic.y		= y += 10;
	s_options_demos_action.generic.name	= "Demos";
	s_options_demos_action.generic.callback = DemosFunc;

#if defined(_WIN32) || defined(WITH_XMMS)
	s_options_mp3_action.generic.type	= MTYPE_ACTION;
	s_options_mp3_action.generic.x		= 0;
	s_options_mp3_action.generic.y		= y += 10;
	s_options_mp3_action.generic.callback = MP3Func;
	s_options_mp3_action.generic.name	= MP3_PLAYERNAME_LEADINGCAP;
#endif

	s_options_customize_options_action.generic.type	= MTYPE_ACTION;
	s_options_customize_options_action.generic.x		= 0;
	s_options_customize_options_action.generic.y		= y += 20;
	s_options_customize_options_action.generic.name	= "customize controls";
	s_options_customize_options_action.generic.callback = CustomizeControlsFunc;

	s_options_defaults_action.generic.type	= MTYPE_ACTION;
	s_options_defaults_action.generic.x		= 0;
	s_options_defaults_action.generic.y		= y += 10;
	s_options_defaults_action.generic.name	= "reset defaults";
	s_options_defaults_action.generic.callback = ControlsResetDefaultsFunc;

	s_options_console_action.generic.type	= MTYPE_ACTION;
	s_options_console_action.generic.x		= 0;
	s_options_console_action.generic.y		= y += 10;
	s_options_console_action.generic.name	= "go to console";
	s_options_console_action.generic.callback = ConsoleFunc;

	ControlsSetMenuItemValues();

	s_options_menu.draw = Options_MenuDraw;
	s_options_menu.key = NULL;

	Menu_AddItem( &s_options_menu, ( void * ) &s_options_sfxvolume_slider );
#ifdef CD_AUDIO
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_cdvolume_box );
#endif
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_quality_list );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_compatibility_list );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_sensitivity_slider );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_alwaysrun_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_invertmouse_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_lookspring_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_lookstrafe_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_freelook_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_crosshair_box );
#ifdef JOYSTICK
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_joystick_box );
#endif
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_new_options_action );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_demos_action );
#if defined(_WIN32) || defined(WITH_XMMS)
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_mp3_action );
#endif
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_customize_options_action );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_defaults_action );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_console_action );
}


void M_Menu_Options_f (void)
{
	Options_MenuInit();
	M_PushMenu ( &s_options_menu );
}

