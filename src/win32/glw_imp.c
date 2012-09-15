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
/*
** GLW_IMP.C
**
** This file contains ALL Win32 specific stuff having to do with the
** OpenGL refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_Shutdown
** GLimp_SwitchFullscreen
**
*/

#include <assert.h>
#include "../ref_gl/gl_local.h"
#include "glw_win.h"
#include "winquake.h"

//static qboolean GLimp_SwitchFullscreen( int width, int height );
qboolean GLimp_InitGL (void);

glwstate_t glw_state;

extern cvar_t *vid_fullscreen;
extern cvar_t *vid_ref;
extern cvar_t *vid_xpos;
extern cvar_t *vid_ypos;
extern cvar_t *vid_displayfrequency;
extern cvar_t *vid_restore_on_switch;

static DEVMODE		originalDesktopMode;
static DEVMODE		fullScreenMode;

static qboolean VerifyDriver( void )
{
	char buffer[1024];

	Q_strncpyz( buffer, (const char *)qglGetString( GL_RENDERER ), sizeof(buffer) );
	Q_strlwr( buffer );
	if ( strcmp( buffer, "gdi generic" ) == 0 )
		if ( !glw_state.mcd_accelerated )
			return false;
	return true;
}

extern char qglLastError[256];

static void PrintWinError(const char *function, qboolean isFatal)
{ 
	LPVOID lpMsgBuf;
	DWORD dw = GetLastError(); 

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf,
		0, NULL );

	Com_sprintf(qglLastError, sizeof(qglLastError), "%s failed with error %d: %s", function, dw, (char *)lpMsgBuf);
	LocalFree(lpMsgBuf);

	if(isFatal)
		Com_Error(ERR_FATAL, "%s", qglLastError);
	else
		Com_Printf("%s\n", qglLastError);
}

/*
** VID_CreateWindow
*/
#include "resource.h"

#define	WINDOW_CLASS_NAME APPLICATION"WndClass"

static qboolean s_classRegistered = false;
extern int		vid_scaled_width, vid_scaled_height;

static qboolean VID_CreateWindow( int width, int height, qboolean fullscreen )
{
	RECT			r;
	int				stylebits;
	int				x, y, w, h, exstyle;

	/* Register the frame class */
	if (!s_classRegistered)
	{
		WNDCLASS wc;

		memset( &wc, 0, sizeof( wc ) );
		wc.style         = 0;
		wc.lpfnWndProc   = (WNDPROC)glw_state.wndproc;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra    = 0;
		wc.hInstance     = glw_state.hInstance;
		wc.hIcon         = LoadIcon(glw_state.hInstance, MAKEINTRESOURCE(IDI_ICON1));
		wc.hCursor       = LoadCursor (NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)GetStockObject(GRAY_BRUSH);
		wc.lpszMenuName  = 0;
		wc.lpszClassName = WINDOW_CLASS_NAME;

		if (!RegisterClass(&wc)) {
			PrintWinError("VID_CreateWindow: RegisterClass", true);
			return false;
		}

		s_classRegistered = true;
	}

	if (fullscreen) {
		exstyle = WS_EX_TOPMOST;
		stylebits = WS_POPUP|WS_VISIBLE;

		w = width;
		h = height;
		x = 0;
		y = 0;
	}
	else {
		exstyle = 0;
		stylebits = WINDOW_STYLE;

		r.left = 0;
		r.top = 0;
		r.right  = width;
		r.bottom = height;

		AdjustWindowRect (&r, stylebits, FALSE);

		w = r.right - r.left;
		h = r.bottom - r.top;
		x = vid_xpos->integer;
		y = vid_ypos->integer;
	}

	glw_state.hWnd = CreateWindowEx (
		 exstyle, 
		 WINDOW_CLASS_NAME,
		 APPLICATION,
		 stylebits,
		 x, y, w, h,
		 NULL,
		 NULL,
		 glw_state.hInstance,
		 NULL);

	if (!glw_state.hWnd) {
		PrintWinError("VID_CreateWindow: CreateWindowEx", true);
		return false;
	}
	
	ShowWindow( glw_state.hWnd, SW_SHOW );
	UpdateWindow( glw_state.hWnd );

	// init all the gl stuff for the window
	if (!GLimp_InitGL())
	{
		Com_Printf ( "VID_CreateWindow() - GLimp_InitGL failed\n");

		if (glw_state.hGLRC) {
			qwglDeleteContext( glw_state.hGLRC );
			glw_state.hGLRC = NULL;
		}
		if (glw_state.hDC) {
			ReleaseDC( glw_state.hWnd, glw_state.hDC );
			glw_state.hDC = NULL;
		}

		ShowWindow( glw_state.hWnd, SW_HIDE );
		DestroyWindow( glw_state.hWnd );
		glw_state.hWnd = NULL;
		return false;
	}

	SetForegroundWindow( glw_state.hWnd );
	SetFocus( glw_state.hWnd );

	vid_scaled_width = (int)ceilf((float)width / gl_scale->value);
	vid_scaled_height = (int)ceilf((float)height / gl_scale->value);

	//round to powers of 8/2 to avoid blackbars
	width = (vid_scaled_width+7)&~7;
	height = (vid_scaled_height+1)&~1;

	// let the sound and input subsystems know about the new window
	VID_NewWindow( width, height );
	//VID_NewWindow( width / gl_scale->value, height / gl_scale->value);

	return true;
}


static qboolean GLimp_SetFSMode( int *width, int *height )
{
	DEVMODE		dm;
	HRESULT		hr;

	EnumDisplaySettings (NULL, ENUM_CURRENT_SETTINGS, &originalDesktopMode);

	memset( &dm, 0, sizeof( dm ) );
	dm.dmSize		= sizeof( dm );
	dm.dmPelsWidth  = *width;
	dm.dmPelsHeight = *height;
	dm.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT;

	if (vid_displayfrequency->integer > 30)
	{
		dm.dmFields |= DM_DISPLAYFREQUENCY;
		dm.dmDisplayFrequency = vid_displayfrequency->integer;
		Com_Printf("...using display frequency %i\n", dm.dmDisplayFrequency);
	}
	if ( gl_bitdepth->integer > 0 )
	{
		if(glw_state.allowdisplaydepthchange)
		{
			dm.dmFields |= DM_BITSPERPEL;
			dm.dmBitsPerPel = gl_bitdepth->integer;
			Com_Printf("...using gl_bitdepth of %d\n", gl_bitdepth->integer);
		}
		else
		{
			Com_Printf("WARNING:...changing depth not supported on Win95 < pre-OSR 2.x\n");
		}
	}
	else
	{
		Com_Printf("...using desktop display depth of %d\n", originalDesktopMode.dmBitsPerPel);
	}

	Com_Printf ( "...calling CDS: " );
	hr = ChangeDisplaySettings(&dm, CDS_FULLSCREEN);
	if (hr != DISP_CHANGE_SUCCESSFUL)
	{
		DEVMODE		devmode;
		int			modeNum;
		
		Com_Printf ( "failed\n" );

		switch ((int)hr) {
		case DISP_CHANGE_BADFLAGS: //Shouldnt hapent
			Com_Printf("An invalid set of flags was passed in.\n");
			return false;
		case DISP_CHANGE_BADPARAM: //Shouldnt hapent
			Com_Printf("An invalid parameter was passed in.\n");
			return false;
		case DISP_CHANGE_RESTART:
			Com_Printf("Windows need to restart for that mode.\n");
			return false;
		case DISP_CHANGE_FAILED:
		case DISP_CHANGE_BADMODE:
			if(hr == DISP_CHANGE_FAILED)
				Com_Printf("The display driver failed the specified graphics mode.\n");
			else
				Com_Printf("The graphics mode is not supported.\n");

			if(dm.dmFields & (DM_DISPLAYFREQUENCY|DM_BITSPERPEL)) {
				DWORD fields = dm.dmFields;

				dm.dmFields &= ~(DM_DISPLAYFREQUENCY|DM_BITSPERPEL);

				//Lets try without users params
				if(ChangeDisplaySettings(&dm, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL)
				{
					if((fields & DM_DISPLAYFREQUENCY) && (fields & DM_BITSPERPEL))
						Com_Printf("WARNING: vid_displayfrequency & gl_bitdepth is bad value(s), ignored\n");
					else
						Com_Printf("WARNING: %s is bad value, ignored\n", (fields & DM_DISPLAYFREQUENCY) ? "vid_displayfrequency" : "gl_bitdepth");

					return true;
				}
				dm.dmFields = fields; //it wasnt because of that
			}
			break;
		default:
			break;
		}

		// the exact mode failed, so scan EnumDisplaySettings for the next largest mode
		Com_Printf("...trying next higher resolution: " );
		
		// we could do a better matching job here...
		for ( modeNum = 0 ; ; modeNum++ ) {
			if ( !EnumDisplaySettings( NULL, modeNum, &devmode ) ) {
				modeNum = -1;
				break;
			}
			if ( devmode.dmPelsWidth >= *width
				&& devmode.dmPelsHeight >= *height
				&& devmode.dmBitsPerPel >= 15 ) {
				break;
			}
		}

		if (modeNum == -1 || ChangeDisplaySettings(&devmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
		{
			Com_Printf("failed\n");
			return false;
		}

		*width = devmode.dmPelsWidth;
		*height = devmode.dmPelsHeight;
		Com_Printf ("ok, %ix%i\n", *width, *height);
	}
	else {
		Com_Printf ( "ok\n" );
	}

	return true;
}

/*
** GLimp_SetMode
*/
rserr_t GLimp_SetMode( int *pwidth, int *pheight, int mode, qboolean fullscreen )
{
	int width, height;
	const char *win_fs[] = { "W", "FS" };

	Com_Printf ( "Initializing OpenGL display\n");

	Com_Printf ("...setting mode %d:", mode );

	if ( !R_GetModeInfo( &width, &height, mode ) )
	{
		Com_Printf ( " invalid mode\n" );
		return rserr_invalid_mode;
	}

	Com_Printf ( " %d %d %s\n", width, height, win_fs[fullscreen] );

	// destroy the existing window
	if (glw_state.hWnd)
		GLimp_Shutdown ();

	// do a CDS if needed
	if ( fullscreen )
	{
		if (GLimp_SetFSMode(&width, &height))
		{
			*pwidth = width;
			*pheight = height;
			if (!VID_CreateWindow (width, height, true)) {
				Com_Printf("...restoring display settings\n");
				ChangeDisplaySettings( 0, 0 );
				gl_state.fullscreen = false;
				return rserr_invalid_mode;
			}

			EnumDisplaySettings (NULL, ENUM_CURRENT_SETTINGS, &fullScreenMode);

			gl_state.fullscreen = true;
			return rserr_ok;
		}
	}

	*pwidth = width;
	*pheight = height;
	Com_Printf ( "...setting windowed mode\n" );

	if (gl_state.fullscreen)
	{
		ChangeDisplaySettings( 0, 0 );
		gl_state.fullscreen = false;
	}

	if (!VID_CreateWindow (width, height, false))
		return rserr_invalid_mode;

	return (fullscreen) ? rserr_invalid_fullscreen : rserr_ok;
}

/*
** GLimp_Shutdown
**
** This routine does all OS specific shutdown procedures for the OpenGL
** subsystem.  Under OpenGL this means NULLing out the current DC and
** HGLRC, deleting the rendering context, and releasing the DC acquired
** for the window.  The state structure is also nulled out.
**
*/
void GLimp_Shutdown( void )
{
	if ( qwglMakeCurrent && !qwglMakeCurrent( NULL, NULL ) )
		Com_Printf ( "GLimp_Shutdown() - wglMakeCurrent failed\n");
	if ( glw_state.hGLRC )
	{
		if (  qwglDeleteContext && !qwglDeleteContext( glw_state.hGLRC ) )
			Com_Printf ( "GLimp_Shutdown() - wglDeleteContext failed\n");
		glw_state.hGLRC = NULL;
	}
	if (glw_state.hDC)
	{
		if ( !ReleaseDC( glw_state.hWnd, glw_state.hDC ) )
			Com_Printf ( "GLimp_Shutdown() - ReleaseDC failed\n" );
		glw_state.hDC   = NULL;
	}
	if (glw_state.hWnd)
	{
		ShowWindow (glw_state.hWnd, SW_HIDE);
		DestroyWindow (	glw_state.hWnd );
		glw_state.hWnd = NULL;
	}

	UnregisterClass (WINDOW_CLASS_NAME, glw_state.hInstance);
	s_classRegistered = false;

	if ( gl_state.fullscreen )
	{
		ChangeDisplaySettings( 0, 0 );
		gl_state.fullscreen = false;
	}
}


/*
** GLimp_Init
**
** This routine is responsible for initializing the OS specific portions
** of OpenGL.  Under Win32 this means dealing with the pixelformats and
** doing the wgl interface stuff.
*/
qboolean GLimp_Init( void *hinstance, void *wndproc )
{
#define OSR2_BUILD_NUMBER 1111

	OSVERSIONINFO	vinfo;

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	glw_state.allowdisplaydepthchange = false;

	if ( GetVersionEx( &vinfo) )
	{
		if ( vinfo.dwMajorVersion > 4 )
		{
			glw_state.allowdisplaydepthchange = true;
		}
		else if ( vinfo.dwMajorVersion == 4 )
		{
			if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT )
			{
				glw_state.allowdisplaydepthchange = true;
			}
			else if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS )
			{
				if ( LOWORD( vinfo.dwBuildNumber ) >= OSR2_BUILD_NUMBER )
				{
					glw_state.allowdisplaydepthchange = true;
				}
			}
		}
	}
	else
	{
		PrintWinError("GLimp_Init() - GetVersionEx", false);
		return false;
	}

	glw_state.hInstance = ( HINSTANCE ) hinstance;
	glw_state.wndproc = wndproc;

	return true;
}

qboolean GLimp_InitGL (void)
{
    PIXELFORMATDESCRIPTOR pfd = 
	{
		sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
		1,								// version number
		PFD_DRAW_TO_WINDOW |			// support window
		PFD_SUPPORT_OPENGL |			// support OpenGL
		PFD_DOUBLEBUFFER,				// double buffered
		PFD_TYPE_RGBA,					// RGBA type
		24,								// 24-bit color depth
		0, 0, 0, 0, 0, 0,				// color bits ignored
		0,								// no alpha buffer
		0,								// shift bit ignored
		0,								// no accumulation buffer
		0, 0, 0, 0, 					// accum bits ignored
		24,								// 32-bit z-buffer
		8,								// no stencil buffer
		0,								// no auxiliary buffer
		PFD_MAIN_PLANE,					// main layer
		0,								// reserved
		0, 0, 0							// layer masks ignored
    };
    int  pixelformat;
	cvar_t *stereo;
	
	stereo = Cvar_Get( "cl_stereo", "0", 0 );

	/*
	** set PFD_STEREO if necessary
	*/
	if (stereo->integer)
	{
		Com_Printf ( "...attempting to use stereo\n" );
		pfd.dwFlags |= PFD_STEREO;
		gl_state.stereo_enabled = true;
	}
	else
	{
		gl_state.stereo_enabled = false;
	}

	/*
	** figure out if we're running on a minidriver or not
	*/
	if (Q_stristr( gl_driver->string, "opengl32" ) != 0 )
		glw_state.minidriver = false;
	else
		glw_state.minidriver = true;

	/*
	** Get a DC for the specified window
	*/
	if ( glw_state.hDC != NULL )
		Com_Printf ( "GLimp_Init() - non-NULL DC exists\n" );

    if ( ( glw_state.hDC = GetDC( glw_state.hWnd ) ) == NULL )
	{
		PrintWinError("GLimp_Init() - GetDC", false);
		return false;
	}

	if ( glw_state.minidriver )
	{
		if ( (pixelformat = qwglChoosePixelFormat( glw_state.hDC, &pfd)) == 0 )
		{
			PrintWinError("GLimp_Init() - wglChoosePixelFormat", false);
			return false;
		}
		if ( qwglSetPixelFormat( glw_state.hDC, pixelformat, &pfd) == FALSE )
		{
			PrintWinError("GLimp_Init() - wglSetPixelFormat", false);
			return false;
		}
		qwglDescribePixelFormat(glw_state.hDC, pixelformat, sizeof( pfd ), &pfd );
	}
	else
	{
		if ((pixelformat = ChoosePixelFormat(glw_state.hDC, &pfd)) == 0)
		{
			PrintWinError("GLimp_Init() - ChoosePixelFormat", false);
			return false;
		}
		if (SetPixelFormat( glw_state.hDC, pixelformat, &pfd) == FALSE)
		{
			PrintWinError("GLimp_Init() - SetPixelFormat", false);
			return false;
		}
		DescribePixelFormat( glw_state.hDC, pixelformat, sizeof( pfd ), &pfd );

		if ( !( pfd.dwFlags & PFD_GENERIC_ACCELERATED ) )
		{
			extern cvar_t *gl_allow_software;

			if ( gl_allow_software->integer )
				glw_state.mcd_accelerated = true;
			else
				glw_state.mcd_accelerated = false;
		}
		else
		{
			glw_state.mcd_accelerated = true;
		}
	}

	/*
	** report if stereo is desired but unavailable
	*/
	if ( !( pfd.dwFlags & PFD_STEREO ) && ( stereo->value != 0 ) ) 
	{
		Com_Printf("...failed to select stereo pixel format\n");
		Cvar_Set("cl_stereo", "0");
		gl_state.stereo_enabled = false;
	}

	/*
	** startup the OpenGL subsystem by creating a context and making
	** it current
	*/
	if ((glw_state.hGLRC = qwglCreateContext(glw_state.hDC)) == 0)
	{
		PrintWinError("GLimp_Init() - wglCreateContext", false);
		return false;
	}

    if (!qwglMakeCurrent(glw_state.hDC, glw_state.hGLRC))
	{
		PrintWinError("GLimp_Init() - wglMakeCurrent", false);
		return false;
	}

	if (!VerifyDriver())
	{
		Com_Error(ERR_FATAL, "GLimp_Init() - no hardware acceleration detected");
		return false;
	}

	/*
	** print out PFD specifics 
	*/
	Com_Printf("GL PFD: Color(%dbits) Depth(%dbits) Stencil(%dbits)\n", (int)pfd.cColorBits, (int)pfd.cDepthBits, (int)pfd.cStencilBits);
	{
		char	buffer[1024];

		Q_strncpyz( buffer, (const char *)qglGetString( GL_RENDERER ), sizeof(buffer));
		Q_strlwr( buffer );

		gl_state.stencil = false;
		if (strstr(buffer, "voodoo3"))
		{
			Com_Printf("... Voodoo3 has no stencil buffer\n");
		}
		else
		{
			if (pfd.cStencilBits > 0)
			{
				Com_Printf("... Using stencil buffer\n");
				gl_state.stencil = true;	// Stencil shadows -MrG
			}
		}
	}

	return true;
}

/*
** GLimp_BeginFrame
*/
void GLimp_BeginFrame( float camera_separation )
{
	if (gl_state.stereo_enabled && camera_separation < 0)
		qglDrawBuffer( GL_BACK_LEFT );
	else if (gl_state.stereo_enabled && camera_separation > 0)
		qglDrawBuffer( GL_BACK_RIGHT );
	else
		qglDrawBuffer( GL_BACK );
}

/*
** GLimp_EndFrame
** 
** Responsible for doing a swapbuffers and possibly for other stuff
** as yet to be determined.  Probably better not to make this a GLimp
** function and instead do a call to GLimp_SwapBuffers.
*/
void GLimp_EndFrame (void)
{

	assert( qglGetError() == GL_NO_ERROR );

	if ( Q_stricmp( gl_drawbuffer->string, "GL_FRONT" ) != 0 )
	{
		if(glw_state.minidriver)
		{
			if ( !qwglSwapBuffers( glw_state.hDC ) )
				PrintWinError("GLimp_EndFrame() - wglSwapBuffers()", true);
		}
		else
		{
			SwapBuffers( glw_state.hDC );
		}
	}
}

static void RestoreDesktopSettings (void)
{
	if (ChangeDisplaySettings( &originalDesktopMode, CDS_FULLSCREEN ) != DISP_CHANGE_SUCCESSFUL )
		PrintWinError("Couldn't restore desktop display settings - ChangeDisplaySettings", true);
}

static void RestoreQ2Settings (void)
{
	if (ChangeDisplaySettings( &fullScreenMode, CDS_FULLSCREEN ) != DISP_CHANGE_SUCCESSFUL )
		PrintWinError("Couldn't restore Quake 2 display settings - ChangeDisplaySettings", true);
}

/*
** GLimp_AppActivate
*/
void GLimp_AppActivate( qboolean active )
{
	static qboolean usingDesktopSettings = false;

	if ( active )
	{
		SetForegroundWindow( glw_state.hWnd );
		ShowWindow( glw_state.hWnd, SW_SHOW );

		if (usingDesktopSettings)
		{
			RestoreQ2Settings ();
			usingDesktopSettings = false;
		}
	}
	else
	{
		if ( vid_fullscreen->integer )
		{
			ShowWindow( glw_state.hWnd, SW_MINIMIZE );

			if(vid_restore_on_switch->integer)
			{
				RestoreDesktopSettings();
				usingDesktopSettings = true;
			}
		}
	}
}

