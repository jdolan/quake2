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
#include "../ui/ui_local.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define VC_LEANMEAN
#include <windows.h>
#endif
#define REF_OPENGL	0
#define REF_3DFX	1
#define REF_POWERVR	2
#define REF_VERITE	3

#define REF_OPENGLX	0
#define REF_MESA3DGLX 1

void VID_Restart_f (void);
extern cvar_t *vid_fullscreen;
extern cvar_t *vid_gamma;
extern cvar_t *scr_viewsize;

#ifdef GL_QUAKE
extern cvar_t *gl_mode;
extern cvar_t *gl_driver;
extern cvar_t *gl_picmip;
extern cvar_t *gl_finish;
#else
extern cvar_t *sw_mode;
extern cvar_t *sw_stipplealpha;
#endif
extern void M_ForceMenuOff( void );

void VID_MenuInit( void );
/*
====================================================================

MENU INTERACTION

====================================================================
*/
#define RMODES 14
static menuframework_s  s_video_menu;

static menulist_s		s_mode_list;
static menulist_s		s_ref_list;
static menuslider_s		s_tq_slider;
static menuslider_s		s_screensize_slider;
static menuslider_s		s_brightness_slider;
static menulist_s  		s_fs_box;
static menulist_s  		s_stipple_box;
static menulist_s  		s_finish_box;
static menuaction_s		s_defaults_action;
static menuaction_s		s_apply_action;

static void ScreenSizeCallback( void *s )
{
	menuslider_s *slider = ( menuslider_s * ) s;

	Cvar_SetValue( "viewsize", slider->curvalue * 10 );
}

static void BrightnessCallback( void *s )
{
#ifndef GL_QUAKE
	float gamma;
	menuslider_s *slider = ( menuslider_s * ) s;
#endif

	s_brightness_slider.curvalue = s_brightness_slider.curvalue;

#ifndef GL_QUAKE
	gamma = ( 0.8 - ( slider->curvalue/10.0 - 0.5 ) ) + 0.5;

	Cvar_SetValue( "vid_gamma", gamma );
#endif
}

static void ResetDefaults( void *unused )
{
	VID_MenuInit();
}

static void ApplyChanges( void *unused )
{
	float gamma;

	/*
	** invert sense so greater = brighter, and scale to a range of 0.5 to 1.3
	*/
	gamma = ( 0.8f - ( s_brightness_slider.curvalue/10.0f - 0.5f ) ) + 0.5f;

	Cvar_SetValue( "vid_gamma", gamma );
	Cvar_SetValue( "sw_stipplealpha", s_stipple_box.curvalue );
	Cvar_SetValue( "gl_picmip", 3 - s_tq_slider.curvalue );
	Cvar_SetValue( "vid_fullscreen", s_fs_box.curvalue );
	Cvar_SetValue( "gl_finish", s_finish_box.curvalue );

#ifndef GL_QUAKE
	if(s_mode_list.curvalue == RMODES)
		Cvar_SetValue( "sw_mode", -1 );
	else
		Cvar_SetValue( "sw_mode", s_mode_list.curvalue );
#else
	if(s_mode_list.curvalue == RMODES)
		Cvar_SetValue( "gl_mode", -1 );
	else
		Cvar_SetValue( "gl_mode", s_mode_list.curvalue );

	switch ( s_ref_list.curvalue )
	{
#ifdef _WIN32
	case REF_OPENGL:
		Cvar_Set( "gl_driver", "opengl32" );
		break;
	case REF_3DFX:
		Cvar_Set( "gl_driver", "3dfxgl" );
		break;
	case REF_POWERVR:
		Cvar_Set( "gl_driver", "pvrgl" );
		break;
#else
	case REF_OPENGLX :
		Cvar_Set( "gl_driver", "libGL.so" );
		break;

	case REF_MESA3DGLX :
		Cvar_Set( "gl_driver", "libMesaGL.so.2" );
		break;
#endif
	}
	if ( gl_driver->modified )
		VID_Restart_f ();
#endif


	M_ForceMenuOff();
}


/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (menuframework_s *self)
{
	int w, h;

	/*
	** draw the banner
	*/
	Draw_GetPicSize( &w, &h, "m_banner_video" );
	Draw_Pic( viddef.width / 2 - w / 2, viddef.height /2 - 110, "m_banner_video", 1 );

	/*
	** move cursor to a reasonable starting position
	*/
	Menu_AdjustCursor( self, 1 );

	/*
	** draw the menu
	*/
	Menu_Draw( self );
}

/*
** VID_MenuInit
*/
void VID_MenuInit( void )
{
	static const char *resolutions[] = 
	{
		"[320 240  ]",
		"[400 300  ]",
		"[512 384  ]",
		"[640 480  ]",
		"[800 600  ]",
		"[960 720  ]",
		"[1024 768 ]",
		"[1152 864 ]",
		"[1280 960 ]",
		"[1600 1200]",
		"[2048 1536]",
		"[1024 480 ]",
		"[1280 768 ]",
		"[1280 1024]",
		"[Custom   ]",
		0
	};
	static const char *refs[] =
	{
#ifdef _WIN32
		"[default OpenGL]",
		"[3Dfx OpenGL   ]",
		"[PowerVR OpenGL]",
#else
		"[OpenGL glX     ]",
		"[Mesa 3-D glX   ]",
#endif
		0
	};
	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0
	};

	s_screensize_slider.curvalue = scr_viewsize->integer/10;

#ifndef GL_QUAKE
	if(sw_mode->integer == -1)
		s_mode_list.curvalue = RMODES;
	else
		s_mode_list.curvalue = sw_mode->integer;
#else
	if(gl_mode->integer == -1)
		s_mode_list.curvalue = RMODES;
	else
		s_mode_list.curvalue = gl_mode->integer;

# ifdef _WIN32
	if ( strcmp( gl_driver->string, "3dfxgl" ) == 0 )
		s_ref_list.curvalue = REF_3DFX;
	else if ( strcmp( gl_driver->string, "pvrgl" ) == 0 )
		s_ref_list.curvalue = REF_POWERVR;
	else
		s_ref_list.curvalue = REF_OPENGL;
# else
	if ( strcmp( gl_driver->string, "libMesaGL.so.2" ) == 0 )
		s_ref_list.curvalue = REF_MESA3DGLX;
	else
		s_ref_list.curvalue = REF_OPENGLX;
# endif
#endif

	memset(&s_video_menu, 0, sizeof(s_video_menu));
	s_video_menu.x = viddef.width * 0.50;
	s_video_menu.nitems = 0;

	s_ref_list.generic.type = MTYPE_SPINCONTROL;
	s_ref_list.generic.name = "driver";
	s_ref_list.generic.x = 0;
	s_ref_list.generic.y = 0;
	s_ref_list.generic.callback = NULL;
	s_ref_list.itemnames = refs;

	s_mode_list.generic.type = MTYPE_SPINCONTROL;
	s_mode_list.generic.name = "video mode";
	s_mode_list.generic.x = 0;
	s_mode_list.generic.y = 10;
	s_mode_list.itemnames = resolutions;

	s_screensize_slider.generic.type	= MTYPE_SLIDER;
	s_screensize_slider.generic.x		= 0;
	s_screensize_slider.generic.y		= 20;
	s_screensize_slider.generic.name	= "screen size";
	s_screensize_slider.minvalue = 4;
	s_screensize_slider.maxvalue = 10;
	s_screensize_slider.generic.callback = ScreenSizeCallback;

	s_brightness_slider.generic.type	= MTYPE_SLIDER;
	s_brightness_slider.generic.x	= 0;
	s_brightness_slider.generic.y	= 30;
	s_brightness_slider.generic.name	= "brightness";
	s_brightness_slider.generic.callback = BrightnessCallback;
	s_brightness_slider.minvalue = 5;
	s_brightness_slider.maxvalue = 13;
	s_brightness_slider.curvalue = ( 1.3 - vid_gamma->value + 0.5 ) * 10;

	s_fs_box.generic.type = MTYPE_SPINCONTROL;
	s_fs_box.generic.x	= 0;
	s_fs_box.generic.y	= 40;
	s_fs_box.generic.name	= "fullscreen";
	s_fs_box.itemnames = yesno_names;
	s_fs_box.curvalue = vid_fullscreen->integer;

	s_defaults_action.generic.type = MTYPE_ACTION;
	s_defaults_action.generic.name = "reset to defaults";
	s_defaults_action.generic.x    = 0;
	s_defaults_action.generic.y    = 90;
	s_defaults_action.generic.callback = ResetDefaults;

	s_apply_action.generic.type = MTYPE_ACTION;
	s_apply_action.generic.name = "apply";
	s_apply_action.generic.x    = 0;
	s_apply_action.generic.y    = 100;
	s_apply_action.generic.callback = ApplyChanges;

#ifndef GL_QUAKE
	s_stipple_box.generic.type = MTYPE_SPINCONTROL;
	s_stipple_box.generic.x	= 0;
	s_stipple_box.generic.y	= 60;
	s_stipple_box.generic.name	= "stipple alpha";
	s_stipple_box.curvalue = sw_stipplealpha->value;
	s_stipple_box.itemnames = yesno_names;
#else
	s_tq_slider.generic.type	= MTYPE_SLIDER;
	s_tq_slider.generic.x		= 0;
	s_tq_slider.generic.y		= 60;
	s_tq_slider.generic.name	= "texture quality";
	s_tq_slider.minvalue = 0;
	s_tq_slider.maxvalue = 3;
	s_tq_slider.curvalue = 3-gl_picmip->integer;

	s_finish_box.generic.type = MTYPE_SPINCONTROL;
	s_finish_box.generic.x	= 0;
	s_finish_box.generic.y	= 80;
	s_finish_box.generic.name	= "sync every frame";
	s_finish_box.curvalue = gl_finish->integer;
	s_finish_box.itemnames = yesno_names;
#endif
	s_video_menu.draw = VID_MenuDraw;
	s_video_menu.key = NULL;

#ifndef GL_QUAKE
	Menu_AddItem( &s_video_menu, ( void * ) &s_mode_list );
	Menu_AddItem( &s_video_menu, ( void * ) &s_screensize_slider );
	Menu_AddItem( &s_video_menu, ( void * ) &s_brightness_slider );
	Menu_AddItem( &s_video_menu, ( void * ) &s_fs_box );
	Menu_AddItem( &s_video_menu, ( void * ) &s_stipple_box );
#else
	Menu_AddItem( &s_video_menu, ( void * ) &s_ref_list );
	Menu_AddItem( &s_video_menu, ( void * ) &s_mode_list );
	Menu_AddItem( &s_video_menu, ( void * ) &s_screensize_slider );
	Menu_AddItem( &s_video_menu, ( void * ) &s_brightness_slider );
	Menu_AddItem( &s_video_menu, ( void * ) &s_fs_box );
	Menu_AddItem( &s_video_menu, ( void * ) &s_tq_slider );
	Menu_AddItem( &s_video_menu, ( void * ) &s_finish_box );
#endif

	Menu_AddItem( &s_video_menu, ( void * ) &s_defaults_action );
	Menu_AddItem( &s_video_menu, ( void * ) &s_apply_action );

	Menu_Center( &s_video_menu );
	s_video_menu.x -= 8;
}

void M_Menu_Video_f( void ) {
	VID_MenuInit();

	M_PushMenu( &s_video_menu );
}
