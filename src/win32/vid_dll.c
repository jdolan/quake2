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
//#include <assert.h>
#include <float.h>

#include "..\client\client.h"
#ifdef CD_AUDIO
#include <mmsystem.h>
#endif
#include "winquake.h"
//#include "zmouse.h"


cvar_t *win_noalttab;

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL (WM_MOUSELAST+1)  // message that will be supported by the OS 
#endif

static UINT MSH_MOUSEWHEEL;

// Console variables that we need to access from this module
cvar_t		*vid_gamma;
cvar_t		*vid_xpos;			// X coordinate of window position
cvar_t		*vid_ypos;			// Y coordinate of window position
cvar_t		*vid_fullscreen;

cvar_t		*vid_displayfrequency;
cvar_t		*vid_restore_on_switch;

// Global variables used internally by this module
viddef_t	viddef;				// global video state; used by other modules

HWND        cl_hwnd;            // Main window handle for life of program

//#define VID_NUM_MODES ( sizeof( vid_modes ) / sizeof( vid_modes[0] ) )

LONG WINAPI MainWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

static qboolean s_alttab_disabled = false;

extern	unsigned	sys_msg_time;

static qboolean vid_restart = false;
static qboolean vid_active = false;
// WIN32 helper functions
extern qboolean s_win95;

static void WIN_DisableAltTab( void )
{
	if ( s_alttab_disabled )
		return;

	if ( s_win95 )
	{
		BOOL old;
		SystemParametersInfo( SPI_SCREENSAVERRUNNING, 1, &old, 0 );
	}
	else
	{
		RegisterHotKey( 0, 0, MOD_ALT, VK_TAB );
		//RegisterHotKey( 0, 1, MOD_ALT, VK_RETURN );
	}
	s_alttab_disabled = true;
}


static void WIN_EnableAltTab( void )
{
	if (!s_alttab_disabled)
		return;

	if ( s_win95 )
	{
		BOOL old;
		SystemParametersInfo( SPI_SCREENSAVERRUNNING, 0, &old, 0 );
	}
	else
	{
		UnregisterHotKey( 0, 0 );
		//UnregisterHotKey( 0, 1 );
	}

	s_alttab_disabled = false;
}

//==========================================================================
static const byte scantokey[128] = 
					{ 
//  0           1       2       3       4       5       6       7 
//  8           9       A       B       C       D       E       F 
	0  ,    27,     '1',    '2',    '3',    '4',    '5',    '6', 
	'7',    '8',    '9',    '0',    '-',    '=',    K_BACKSPACE, 9, // 0 
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i', 
	'o',    'p',    '[',    ']',    13 ,    K_CTRL,'a',  's',      // 1 
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';', 
	'\'' ,    '`',    K_SHIFT,'\\',  'z',    'x',    'c',    'v',      // 2 
	'b',    'n',    'm',    ',',    '.',    '/',    K_SHIFT,'*', 
	K_ALT,' ',   K_CAPSLOCK  ,    K_F1, K_F2, K_F3, K_F4, K_F5,   // 3 
	K_F6, K_F7, K_F8, K_F9, K_F10,  K_PAUSE,    0  , K_HOME, 
	K_UPARROW,K_PGUP,K_KP_MINUS,K_LEFTARROW,K_KP_5,K_RIGHTARROW, K_KP_PLUS,K_END, //4 
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,0,0,             0,              K_F11, 
	K_F12,0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 5
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 6 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0         // 7 
}; 


/*
=======
MapKey

Map from windows to quake keynums
=======
*/
int MapKey (int key)
{
	int is_extended;
	int modified = ( key >> 16 ) & 255;

	if ( modified > 127)
		return 0;

	is_extended = (key >> 24) & 1;

	key = scantokey[modified];

	if ( !is_extended )
	{
		switch ( key )
		{
		case K_HOME:
			return K_KP_HOME;
		case K_UPARROW:
			return K_KP_UPARROW;
		case K_PGUP:
			return K_KP_PGUP;
		case K_LEFTARROW:
			return K_KP_LEFTARROW;
		case K_RIGHTARROW:
			return K_KP_RIGHTARROW;
		case K_END:
			return K_KP_END;
		case K_DOWNARROW:
			return K_KP_DOWNARROW;
		case K_PGDN:
			return K_KP_PGDN;
		case K_INS:
			return K_KP_INS;
		case K_DEL:
			return K_KP_DEL;
		default:
			return key;
		}
	}
	else
	{
		switch ( key )
		{
		case 0x0D:
			return K_KP_ENTER;
		case 0x2F:
			return K_KP_SLASH;
		case 0xAF:
			return K_KP_PLUS;
		default:
			return key;
		}
	}
}


static void VID_AppActivate(BOOL fActive, BOOL minimize)
{
	Minimized = minimize;

	Key_ClearStates();

	// we don't want to act like we're active if we're minimized
	if (fActive && !Minimized)
		ActiveApp = true;
	else
		ActiveApp = false;

	// minimize/restore mouse-capture on demand
	IN_Activate (ActiveApp);
#ifdef CD_AUDIO
	CDAudio_Activate (ActiveApp);
#endif
	S_Activate (ActiveApp);

	if ( win_noalttab->integer )
	{
		if(!ActiveApp)
			WIN_EnableAltTab();
		else
			WIN_DisableAltTab();
	}

	if ( vid_active )
		R_AppActivate( ActiveApp );
}

/*
====================
MainWndProc

main window procedure
====================
*/

#ifndef WM_XBUTTONDOWN 
   #define WM_XBUTTONDOWN      0x020B 
   #define WM_XBUTTONUP		   0x020C 
#endif 
#ifndef MK_XBUTTON1 
   #define MK_XBUTTON1         0x0020 
   #define MK_XBUTTON2         0x0040 
#endif 


LONG WINAPI MainWndProc (
    HWND    hWnd,
    UINT    uMsg,
    WPARAM  wParam,
    LPARAM  lParam)
{

	if ( uMsg == MSH_MOUSEWHEEL )
	{
		if(DIMouse)
			return DefWindowProc (hWnd, uMsg, wParam, lParam);

		if ( ( ( int ) wParam ) > 0 )
		{
			Key_Event( K_MWHEELUP, true, sys_msg_time );
			Key_Event( K_MWHEELUP, false, sys_msg_time );
		}
		else
		{
			Key_Event( K_MWHEELDOWN, true, sys_msg_time );
			Key_Event( K_MWHEELDOWN, false, sys_msg_time );
		}
        return DefWindowProc (hWnd, uMsg, wParam, lParam);
	}

	switch (uMsg)
	{
	case WM_MOUSEWHEEL:
		/*
		** this chunk of code theoretically only works under NT4 and Win98
		** since this message doesn't exist under Win95
		*/
		if(DIMouse)
			break;

		if ( ( short ) HIWORD( wParam ) > 0 )
		{
			Key_Event( K_MWHEELUP, true, sys_msg_time );
			Key_Event( K_MWHEELUP, false, sys_msg_time );
		}
		else
		{
			Key_Event( K_MWHEELDOWN, true, sys_msg_time );
			Key_Event( K_MWHEELDOWN, false, sys_msg_time );
		}
		break;

	case WM_HOTKEY:
		return 0;

	case WM_CREATE:
		cl_hwnd = hWnd;
		MSH_MOUSEWHEEL = RegisterWindowMessage("MSWHEEL_ROLLMSG");
        break;

	case WM_PAINT:
		SCR_DirtyScreen ();	// force entire screen to update next frame
        break;

	case WM_DESTROY:
		// let sound and input know about this?
		cl_hwnd = NULL;
        break;

	case WM_ACTIVATE:
		// KJB: Watch this for problems in fullscreen modes with Alt-tabbing.
		VID_AppActivate((BOOL)(LOWORD(wParam) != WA_INACTIVE), (BOOL)HIWORD(wParam) );
        break;

	case WM_MOVE:
		if (!vid_fullscreen->integer)
		{
			int		xPos, yPos, style;
			RECT	r;

			xPos = (short) LOWORD(lParam);    // horizontal position 
			yPos = (short) HIWORD(lParam);    // vertical position 

			r.left = r.top = 0;
			r.right = r.bottom = 1;

			style = GetWindowLong( hWnd, GWL_STYLE );
			AdjustWindowRect( &r, style, FALSE );

			Cvar_SetValue( "vid_xpos", xPos + r.left);
			Cvar_SetValue( "vid_ypos", yPos + r.top);
			vid_xpos->modified = false;
			vid_ypos->modified = false;
			if (ActiveApp)
				IN_Activate (true);
		}
        break;

// this is complicated because Win32 seems to pack multiple mouse events into
// one update sometimes, so we always check all states and look for events
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEMOVE:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
		if(!DIMouse) {
			int	temp = 0;

			if (wParam & MK_LBUTTON)
				temp |= 1;

			if (wParam & MK_RBUTTON)
				temp |= 2;

			if (wParam & MK_MBUTTON)
				temp |= 4;

			if (wParam & MK_XBUTTON1) 
				temp |= 8;

			if (wParam & MK_XBUTTON2) 
				temp |= 16;

			IN_MouseEvent (temp, 5);
		}
		break;

	case WM_SYSCOMMAND:
		switch (wParam) {
		case SC_SCREENSAVE:
		case SC_MONITORPOWER:
		case SC_KEYMENU:
			return 0;
		case SC_CLOSE:
			Cbuf_AddText("quit\n");
			return 0;
		case SC_MAXIMIZE:
			Cvar_Set("vid_fullscreen", "1");
			return 0;
		}
		break;

	case WM_QUIT:
	case WM_CLOSE:
		Cbuf_AddText("quit\n");
		return 0;

	case WM_SYSKEYDOWN:
		if ( wParam == 13 )
		{
			if ( vid_fullscreen ) {
				Cvar_SetValue( "vid_fullscreen", !vid_fullscreen->integer );
			}
			return 0;
		}
		// fall through
	case WM_KEYDOWN:
		Key_Event( MapKey( lParam ), true, sys_msg_time);
		break;

	case WM_SYSKEYUP:
	case WM_KEYUP:
		Key_Event( MapKey( lParam ), false, sys_msg_time);
		break;

#ifdef CD_AUDIO
	case MM_MCINOTIFY:
		{
			LONG lRet = 0;
			LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
			lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
		}
		break;
#endif
    }
	// pass all unhandled messages to DefWindowProc
    // return 0 if handled message, 1 if not
    return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

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

void VID_Front_f( void )
{
	SetWindowLong( cl_hwnd, GWL_EXSTYLE, WS_EX_TOPMOST );
	SetForegroundWindow( cl_hwnd );
}

/*
** VID_UpdateWindowPosAndSize
*/
void VID_UpdateWindowPosAndSize( void )
{

	if (!vid_xpos->modified && !vid_ypos->modified)
		return;

	if (!vid_fullscreen->integer)
	{
		RECT	r;
		int		style, w, h;

		r.left   = 0;
		r.top    = 0;
		r.right  = viddef.width;
		r.bottom = viddef.height;

		style = GetWindowLong( cl_hwnd, GWL_STYLE );
		if(AdjustWindowRect( &r, style, FALSE ))
		{
			w = r.right - r.left;
			h = r.bottom - r.top;

			MoveWindow( cl_hwnd, vid_xpos->integer, vid_ypos->integer, w, h, TRUE );
		}
	}

	vid_xpos->modified = false;
	vid_ypos->modified = false;
}

/*
** VID_NewWindow
*/
void VID_NewWindow ( int width, int height)
{
	viddef.width  = width;
	viddef.height = height;

	cl.force_refdef = true;		// can't use a paused refdef
}

void VID_Minimize_f (void)
{
	if(cl_hwnd)
		ShowWindow (cl_hwnd, SW_MINIMIZE);
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

		Com_Printf( "-------- [Loading Renderer] --------\n" );

		//Need to update lached cvar's
		vid_displayfrequency = Cvar_Get ( "vid_displayfrequency", "0", CVAR_ARCHIVE|CVAR_LATCHED );

		vid_active = true;

		if ( R_Init( global_hInstance, MainWndProc ) == -1 )
		{
			R_Shutdown();
			Com_Error (ERR_FATAL, "Couldn't initialize renderer!");
		}

		Com_Printf( "------------------------------------\n");

		vid_restart = false;
		cls.disable_screen = false;
		VID_UpdateWindowPosAndSize();
	}
}


static void OnChange_VID_AltTab (cvar_t *self, const char *oldValue)
{
	if(self->integer)
		WIN_DisableAltTab();
	else
		WIN_EnableAltTab();
}

static void OnChange_VID_XY (cvar_t *self, const char *oldValue)
{
	// update our window position
	VID_UpdateWindowPosAndSize();
}

/*
============
VID_Init
============
*/
void VID_Init (void)
{
	Cvar_Subsystem( CVAR_SYSTEM_VIDEO );
	/* Create the video variables so we know how to start the graphics drivers */
	vid_xpos = Cvar_Get ("vid_xpos", "3", CVAR_ARCHIVE);
	vid_ypos = Cvar_Get ("vid_ypos", "22", CVAR_ARCHIVE);
	vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_ARCHIVE);
	vid_gamma = Cvar_Get( "vid_gamma", "1.0", CVAR_ARCHIVE );
	win_noalttab = Cvar_Get( "win_noalttab", "0", CVAR_ARCHIVE );

	/* Add some console commands that we want to handle */
	Cmd_AddCommand ("vid_restart", VID_Restart_f);
	Cmd_AddCommand ("vid_front", VID_Front_f);
	Cmd_AddCommand ("vid_minimize", VID_Minimize_f);

	vid_displayfrequency = Cvar_Get ( "vid_displayfrequency", "0", CVAR_ARCHIVE|CVAR_LATCHED );
	vid_restore_on_switch = Cvar_Get ("vid_flip_on_switch", "0", 0);

	vid_xpos->OnChange = OnChange_VID_XY;
	vid_ypos->OnChange = OnChange_VID_XY;
	win_noalttab->OnChange = OnChange_VID_AltTab;
	OnChange_VID_AltTab(win_noalttab, win_noalttab->resetString);
	/*
	** this is a gross hack but necessary to clamp the mode for 3Dfx
	*/

	Cvar_Subsystem( CVAR_SYSTEM_GENERIC );

	/* Disable the 3Dfx splash screen */
	putenv("FX_GLIDE_NO_SPLASH=0");
		
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


