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
// r_misc.c

#include "gl_local.h"

/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/ 
qboolean WriteJPG (const char *name, byte *buffer, int width, int height, int quality);
qboolean WriteTGA (const char *name, byte *buffer, int width, int height);

static const char *shotExt[2] = {"tga", "jpg"};
/* 
================== 
GL_ScreenShot_f
================== 
*/  
void GL_ScreenShot_f (void) 
{
	FILE	*f;
	byte	*buffer;
	int		i, picType, offset;
	char	picname[80], checkname[MAX_OSPATH];
	char	date[32], map[32] = "\0";
	time_t	clock;

	if(CL_Mapname()[0])
		Com_sprintf(map, sizeof(map), "_%s", CL_Mapname());

	if(!Q_stricmp( Cmd_Argv( 0 ), "screenshotjpg" )) {
		picType = 1;
		offset = 0;
	} else { //tga
		picType = 0;
		offset = 18;
	}

    // Create the scrnshots directory if it doesn't exist
    Com_sprintf (checkname, sizeof(checkname), "%s/scrnshot", FS_Gamedir());
    Sys_Mkdir (checkname);

	if(Cmd_Argc() == 1)
	{
		time( &clock );
		strftime( date, sizeof(date), "%Y-%m-%d_%H-%M", localtime(&clock));

		// Find a file name to save it to
		for (i=0 ; i<100 ; i++)
		{
			Com_sprintf (picname, sizeof(picname), "%s%s-%02i.%s", date, map, i, shotExt[picType]);
			Com_sprintf (checkname, sizeof(checkname), "%s/scrnshot/%s", FS_Gamedir(), picname);
			f = fopen (checkname, "rb");
			if (!f)
				break;	// file doesn't exist
			fclose (f);
		}
		if (i == 100)
		{
			Com_Printf ("GL_ScreenShot_f: Couldn't create a file\n"); 
			return;
		}
	}
	else
	{
		Com_sprintf (picname, sizeof(picname), "%s.%s", Cmd_Argv( 1 ), shotExt[picType]);
		Com_sprintf( checkname, sizeof(checkname), "%s/scrnshot/%s", FS_Gamedir(), picname );
	}

	buffer = Z_TagMalloc(vid.width * vid.height * 3 + offset, TAG_RENDER_SCRSHOT);
	qglReadPixels( 0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, buffer + offset );

	if( picType == 1 )
		i = WriteJPG(checkname, buffer, vid.width, vid.height, gl_screenshot_quality->integer);
	else
		i = WriteTGA(checkname, buffer, vid.width, vid.height);

	if(i)
		Com_Printf("Wrote %s\n", picname);

	Z_Free( buffer );
} 

/* 
================== 
GLAVI_ReadFrameData - Grabs a frame for exporting to AVI EXPORT
By Robert 'Heffo' Heffernan
================== 
*/
#ifdef AVI_EXPORT
void GLAVI_ReadFrameData (byte *buffer)
{
	if(!buffer)
		return;

	qglReadPixels(0, 0, vid.width, vid.height, GL_BGR_EXT, GL_UNSIGNED_BYTE, buffer);
}
#endif
/*
** GL_Strings_f
*/
void GL_Strings_f( void )
{
	Com_Printf ("GL_VENDOR: %s\n", gl_config.vendor_string );
	Com_Printf ("GL_RENDERER: %s\n", gl_config.renderer_string );
	Com_Printf ("GL_VERSION: %s\n", gl_config.version_string );
	Com_Printf ("GL_EXTENSIONS: %s\n", gl_config.extensions_string );
}

/*
** GL_SetDefaultState
*/
void GL_SetDefaultState( void )
{
	int i;

	qglClearColor (1, 0.2f, 0, 1);

	qglColor4fv(colorWhite);

	qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// properly disable multitexturing at startup
	for( i = gl_config.maxTextureUnits-1; i > 0; i-- ) {
		GL_SelectTexture( i );
		GL_TexEnv( GL_MODULATE );
		qglDisable( GL_BLEND );
		qglDisable( GL_TEXTURE_2D );
	}

	GL_SelectTexture( 0 );
	GL_TexEnv( GL_REPLACE );
	qglDisable (GL_BLEND);
	qglEnable(GL_TEXTURE_2D);

	qglEnable(GL_ALPHA_TEST);
	qglAlphaFunc(GL_GREATER, 0.666f);

	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);
	qglCullFace (GL_FRONT);
	if (gl_state.stencil)
		qglDisable (GL_STENCIL_TEST);

	qglDisable(GL_FOG);

	GL_TextureMode( gl_texturemode->string );
	GL_TextureBits();

	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if ( gl_ext_pointparameters->integer && qglPointParameterfEXT )
	{
		float attenuations[3];

		attenuations[0] = gl_particle_att_a->value;
		attenuations[1] = gl_particle_att_b->value;
		attenuations[2] = gl_particle_att_c->value;

		qglEnable( GL_POINT_SMOOTH );
		qglPointParameterfEXT( GL_POINT_SIZE_MIN_EXT, gl_particle_min_size->value );
		qglPointParameterfEXT( GL_POINT_SIZE_MAX_EXT, gl_particle_max_size->value );
		qglPointParameterfvEXT( GL_DISTANCE_ATTENUATION_EXT, attenuations );
	}

	gl_swapinterval->modified = true;
	GL_UpdateSwapInterval();
}

void GL_UpdateSwapInterval( void )
{
	if ( gl_swapinterval->modified )
	{
		gl_swapinterval->modified = false;

#ifdef _WIN32
		if ( !gl_state.stereo_enabled ) 
		{
			if ( qwglSwapIntervalEXT )
				qwglSwapIntervalEXT( gl_swapinterval->integer );
		}
#endif
	}
}
