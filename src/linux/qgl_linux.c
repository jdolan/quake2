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
** QGL_WIN.C
**
** This file implements the operating system binding of GL to QGL function
** pointers.  When doing a port of Quake2 you must implement the following
** two functions:
**
** QGL_Init() - loads libraries, assigns function pointers, etc.
** QGL_Shutdown() - unloads libraries, NULLs function pointers
*/
#include <dlfcn.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "../qcommon/qcommon.h"
#include "glw_linux.h"

#define QGL_EXTERN

#define QGL_FUNC(type,name,params) type (APIENTRY * q##name) params;
#define QGL_EXT(type,name,params) type (APIENTRY * q##name) params;
#define QGL_GLX(type,name,params) type (APIENTRY * q##name) params;
#define QGL_GLX_EXT(type,name,params) type (APIENTRY * q##name) params;

#include "../ref_gl/qgl.h"

#undef QGL_GLX_EXT
#undef QGL_GLX
#undef QGL_EXT
#undef QGL_FUNC

/*
** QGL_Shutdown
**
** Unloads the specified DLL then nulls out all the proc pointers.
*/
void QGL_Shutdown( void )
{
	if ( glw_state.OpenGLLib )
		dlclose ( glw_state.OpenGLLib );

	glw_state.OpenGLLib = NULL;

#define QGL_FUNC(type,name,params) (q##name) = NULL;
#define QGL_EXT(type,name,params) (q##name) = NULL;
#define QGL_GLX(type,name,params) (q##name) = NULL;
#define QGL_GLX_EXT(type,name,params) (q##name) = NULL;

#include "../ref_gl/qgl.h"

#undef QGL_GLX_EXT
#undef QGL_GLX
#undef QGL_EXT
#undef QGL_FUNC
}

//#define GPA( a ) dlsym( glw_state.OpenGLLib, a )

/*
** QGL_Init
**
** This is responsible for binding our qgl function pointers to 
** the appropriate GL stuff.  In Windows this means doing a 
** LoadLibrary and a bunch of calls to GetProcAddress.  On other
** operating systems we need to do the right thing, whatever that
** might be.
** 
*/
qboolean QGL_Init( const char *dllname )
{
	if ( glw_state.OpenGLLib )
		QGL_Shutdown();

	if ( ( glw_state.OpenGLLib = dlopen( dllname, RTLD_LAZY ) ) == 0 )
	{
		char	fn[MAX_OSPATH];
		char	*path;

		// try basedir next
		path = Cvar_Get ("basedir", ".", CVAR_NOSET)->string;
		
		Com_sprintf(fn, MAX_OSPATH, "%s/%s", path, dllname );

		if ( ( glw_state.OpenGLLib = dlopen( fn, RTLD_LAZY ) ) == 0 ) {
			Com_Printf ( "%s\n", dlerror() );
			return false;
		}
		Com_Printf ("Using %s for OpenGL...", fn); 
	} else {
		Com_Printf ("Using %s for OpenGL...", dllname);
	}

#define QGL_FUNC(type,name,params) (q##name) = ( void * )dlsym( glw_state.OpenGLLib, #name ); \
	if( !(q##name) ) { Com_Printf( "QGL_Init: Failed to get address for %s\n", #name ); return false; }
#define QGL_EXT(type,name,params) (q##name) = NULL;
#define QGL_GLX(type,name,params) (q##name) = ( void * )dlsym( glw_state.OpenGLLib, #name ); \
	if( !(q##name) ) { Com_Printf( "QGL_Init: Failed to get address for %s\n", #name ); return false; }
#define QGL_GLX_EXT(type,name,params) (q##name) = NULL;

#include "../ref_gl/qgl.h"

#undef QGL_GLX_EXT
#undef QGL_GLX
#undef QGL_EXT
#undef QGL_FUNC

	return true;
}

void *qglGetProcAddress(const GLchar *procName)
{
	if (glw_state.OpenGLLib)
		return (void *)dlsym( glw_state.OpenGLLib, (const char *)procName );
	return NULL;
}

