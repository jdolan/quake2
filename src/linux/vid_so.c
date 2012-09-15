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
// Main windowed and fullscreen graphics interface module. This module
// is used for both the software and OpenGL rendering versions of the
// Quake refresh engine.

#include <dlfcn.h> // ELF dl loader
#include <sys/stat.h>
#include <errno.h>

#include "../client/client.h"


// Console variables that we need to access from this module
cvar_t		*vid_gamma;
cvar_t		*vid_xpos;			// X coordinate of window position
cvar_t		*vid_ypos;			// Y coordinate of window position
cvar_t		*vid_fullscreen;

// Global variables used internally by this module
viddef_t	viddef;				// global video state; used by other modules

static qboolean vid_restart = false;
static qboolean vid_active = false;

//#define VID_NUM_MODES ( sizeof( vid_modes ) / sizeof( vid_modes[0] ) )


//==========================================================================

/*
============
VID_Restart_f

Console command to re-start the video mode and refresh DLL. We do this
simply by setting the modified flag for the vid_ref variable, which will
cause the entire video mode and refresh DLL to be reset on the next frame.
============
*/
void VID_Restart_f (void)
{
	vid_restart = true;
}

/*
** VID_NewWindow
*/
void VID_NewWindow ( int width, int height)
{
	viddef.width  = width;
	viddef.height = height;
}


/*
============
VID_CheckChanges

This function gets called once just before drawing each frame, and it's sole purpose in life
is to check to see if any of the video mode parameters have changed, and if they have to 
update the rendering DLL and/or video mode to match.
============
*/
void VID_CheckChanges (void)
{

	if ( vid_restart )
	{
		cl.force_refdef = true;		// can't use a paused refdef
		S_StopAllSounds();

		/*
		** refresh has changed
		*/
		vid_fullscreen->modified = true;
		cl.refresh_prepped = false;
		cls.disable_screen = true;

		VID_Shutdown();

		Com_Printf( "--------- [Loading Renderer] ---------\n" );

		vid_active = true;
		if ( R_Init( 0, 0 ) == -1 )
		{
			R_Shutdown();
			Com_Error (ERR_FATAL, "Couldn't initialize renderer!");
		}

		Com_Printf( "------------------------------------\n");

		vid_restart = false;
		cls.disable_screen = false;

		IN_Activate(false);
		IN_Activate(true);
	}
}

/*
============
VID_Init
============
*/
void VID_Init (void)
{
	/* Create the video variables so we know how to start the graphics drivers */
	// if DISPLAY is defined, try X
	vid_xpos = Cvar_Get ("vid_xpos", "3", CVAR_ARCHIVE);
	vid_ypos = Cvar_Get ("vid_ypos", "22", CVAR_ARCHIVE);
	vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_ARCHIVE);
	vid_gamma = Cvar_Get( "vid_gamma", "1.0", CVAR_ARCHIVE );

	/* Add some console commands that we want to handle */
	Cmd_AddCommand ("vid_restart", VID_Restart_f);

	/* Start the graphics mode and load refresh DLL */
	vid_restart = true;
	vid_active = false;
	VID_CheckChanges();
}

/*
============
VID_Shutdown
============
*/
void VID_Shutdown (void)
{
	if ( vid_active )
	{
		R_Shutdown ();
		vid_active = false;
	}
}

