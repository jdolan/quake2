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

// draw.c

#include "gl_local.h"

image_t		*draw_chars = NULL;

//extern	qboolean	scrap_dirty;
//void Scrap_Upload (void);

extern cvar_t *gl_fontshadow;

/*
===============
Draw_InitLocal
===============
*/
void Draw_InitLocal (void)
{

	if(draw_chars){  // invalidate it, will be freed next vid_restart
		draw_chars->type = -1;
		draw_chars->registration_sequence = -1;
	}

	if(gl_scale->value > 1.0){  // use high-res conchars

		draw_chars = GL_FindImage("pics/conchars-highres.tga", it_pic);

		if(!draw_chars)  // not available
			draw_chars = GL_FindImage("pics/conchars.pcx", it_pic);
	}
	else {  // ensure we're using the stock ones
		draw_chars = GL_FindImage("pics/conchars.pcx", it_pic);
	}

	// load console characters (don't bilerp characters)
	if (!draw_chars)
		Com_Error (ERR_FATAL, "Couldn't load conchars.pcx");

	GL_Bind( draw_chars->texnum );
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

const vec3_t color_table[8] = {
	{0, 0, 0},	// Black
	{1, 0, 0},	// Red
	{0, 1, 0},	// Green
	{1, 1, 0},	// Yellow
	{0, 0, 1},	// Blue
	{0, 1, 1},	// Cyan
	{1, 0, 1},	// Magenta
	{1, 1, 1}	// White
};

const vec4_t colorWhite	= { 1, 1, 1, 1 };

vec4_t colorBlack = { 0, 0, 0, 1 };

/*
================
Draw_Char

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/

#define DRAW_CHAR(x, y, frow, fcol) \
	qglTexCoord2f (fcol, frow); \
	qglVertex2i (x, y); \
	qglTexCoord2f (fcol + 0.0625f, frow); \
	qglVertex2i (x+8, y); \
	qglTexCoord2f (fcol + 0.0625f, frow + 0.0625f); \
	qglVertex2i (x+8, y+8); \
	qglTexCoord2f (fcol, frow + 0.0625f); \
	qglVertex2i (x, y+8)

void Draw_Char (int x, int y, int num, int color, float alpha)
{
	float			frow, fcol;
	vec4_t			fcolor;

	num &= 0xff;

	if ((num&127) == 32)
		return;		// space

	if (y <= -8)
		return;			// totally off screen

	frow = (num>>4)*0.0625f;
	fcol = (num&15)*0.0625f;

	qglDisable(GL_ALPHA_TEST);
	qglEnable(GL_BLEND);
	GL_TexEnv (GL_MODULATE);

	GL_Bind (draw_chars->texnum);

	qglBegin (GL_QUADS);
	
	if(gl_fontshadow->integer) {
		colorBlack[3] = alpha;
		qglColor4fv(colorBlack);
		DRAW_CHAR(x+1, y+1, frow, fcol);
	}

	VectorCopy(color_table[color&7], fcolor);
	fcolor[3] = alpha;

	qglColor4fv (fcolor);

	DRAW_CHAR(x, y, frow, fcol);
	qglEnd ();

	qglColor4fv(colorWhite);

	GL_TexEnv(GL_REPLACE);
	qglEnable(GL_ALPHA_TEST);
	qglDisable(GL_BLEND);
}

void Draw_String (int x, int y, const char *s, int color, float alpha, qboolean alt)
{
	vec4_t	fcolor;
	float	frow, fcol;
	byte	num;

	if (y <= -8)
		return;	// totally off screen

	qglDisable(GL_ALPHA_TEST);
	qglEnable(GL_BLEND);
	GL_TexEnv (GL_MODULATE);

	GL_Bind (draw_chars->texnum);
	qglBegin (GL_QUADS);

	if(gl_fontshadow->integer) {
		const char *string = s;
		int	tempX = x;
		
		colorBlack[3] = alpha;
		qglColor4fv(colorBlack);

		while (*s) {
			num = (alt) ? (*s ^ 0x80) : *s;
			if ((num&127) != 32) // not a space
			{
				frow = (num>>4)*0.0625f;
				fcol = (num&15)*0.0625f;
				DRAW_CHAR(x+1, y+1, frow, fcol);
			}
			x += 8;
			s++;
		}
		s = string;
		x = tempX;
	}

	VectorCopy( color_table[color&7], fcolor );
	fcolor[3] = alpha;

	qglColor4fv (fcolor);
	while (*s) {
		num = (alt) ? (*s ^ 0x80) : *s;
		if ((num&127) != 32) // not a space
		{
			frow = (num>>4)*0.0625f;
			fcol = (num&15)*0.0625f;
			DRAW_CHAR(x, y, frow, fcol);
		}
		x += 8;
		s++;
	}

	qglEnd ();
	qglColor4fv(colorWhite);

	GL_TexEnv(GL_REPLACE);
	qglEnable(GL_ALPHA_TEST);
	qglDisable(GL_BLEND);
}


void DrawCString (int x, int y, const short *s, float alpha, int enable)
{
	float		frow, fcol;
	byte		num;
	vec4_t		fcolor;
	int			currentColor = COLOR_WHITE;

	if (y <= -8)
		return;	// totally off screen

	qglDisable(GL_ALPHA_TEST);
	qglEnable(GL_BLEND);
	GL_TexEnv (GL_MODULATE);

	GL_Bind (draw_chars->texnum);
	qglBegin (GL_QUADS);

	if(gl_fontshadow->integer) {
		const short *string = s;
		int	tempX = x;
		
		colorBlack[3] = alpha;
		qglColor4fv(colorBlack);
		while (*s) {
			num = *s & 0xff;
			if ((num&127) != 32) // not a space
			{
				frow = (num>>4)*0.0625f;
				fcol = (num&15)*0.0625f;
				DRAW_CHAR(x+1, y+1, frow, fcol);
			}
			x += 8;
			s++;
		}
		s = string;
		x = tempX;
	}

	VectorCopy( color_table[currentColor], fcolor );
	fcolor[3] = alpha;
	qglColor4fv(fcolor);

	while (*s)
	{
		num = *s & 0xff;
		if ((num&127) != 32) // not a space
		{
			if ( enable && ( (*s>>8)&7 ) != currentColor) {
				currentColor = (*s>>8)&7;
				VectorCopy( color_table[currentColor], fcolor );
				qglColor4fv (fcolor);
			}

			frow = (num>>4)*0.0625f;
			fcol = (num&15)*0.0625f;
			DRAW_CHAR(x, y, frow, fcol);
		}
		x += 8;
		s++;
	}

	qglEnd ();
	qglColor4fv(colorWhite);

	GL_TexEnv(GL_REPLACE);
	qglEnable(GL_ALPHA_TEST);
	qglDisable(GL_BLEND);
}

/*
=============
Draw_FindPic
=============
*/
image_t	*Draw_FindPic (const char *name)
{
	image_t *gl;
	char	fullname[MAX_QPATH];

	if (name[0] != '/' && name[0] != '\\')
	{
		if(!strncmp("../", name, 3)) //ffs why aq2 uses this?
			Com_sprintf (fullname, sizeof(fullname), "%s.pcx", name+3);
		else
			Com_sprintf (fullname, sizeof(fullname), "pics/%s.pcx", name);

		gl = GL_FindImage (fullname, it_pic);
	}
	else
		gl = GL_FindImage (name+1, it_pic);

	return gl;
}

/*
=============
Draw_GetPicSize
=============
*/
void Draw_GetPicSize (int *w, int *h, const char *pic)
{
	image_t *gl;

	gl = Draw_FindPic (pic);
	if (!gl) {
		*w = *h = -1;
		return;
	}

	*w = gl->width;
	*h = gl->height;
}

/*
=============
Draw_ScaledPic
=============
*/
void Draw_ScaledPic (int x, int y, float scale, const char *pic, float red, float green, float blue, float alpha)
{
	image_t *gl;
	int yoff = 0, xoff = 0;
	int enabled = 0;

	gl = Draw_FindPic(pic);
	if (!gl) {
		Com_DPrintf ( "Can't find pic: %s\n", pic);
		return;
	}

	if (alpha < 1 || (gl->flags & (IT_TRANS|IT_PALETTED)) == IT_TRANS)
	{
		enabled = 1;
		qglEnable(GL_BLEND);
		qglDisable(GL_ALPHA_TEST);
	}
	else if ( ((gl_config.renderer == GL_RENDERER_MCD) || (gl_config.renderer & GL_RENDERER_RENDITION)) && !(gl->flags & IT_TRANS))
	{
		enabled = 2;
		qglDisable(GL_ALPHA_TEST);
	}

	GL_TexEnv(GL_MODULATE);
	qglColor4f(red, green, blue, alpha);

	GL_Bind (gl->texnum);

	qglBegin (GL_QUADS);

	if(scale != 1.0f) {
		xoff = (int)ceilf(((float)gl->width * scale - (float)gl->width)*0.5f);
		yoff = (int)ceilf(((float)gl->height * scale - (float)gl->height)*0.5f);
	}

	qglTexCoord2f (gl->sl, gl->tl);
	qglVertex2i (x-xoff, y-yoff);
	qglTexCoord2f (gl->sh, gl->tl);
	qglVertex2i (x+gl->width+xoff, y-yoff);
	qglTexCoord2f (gl->sh, gl->th);
	qglVertex2i (x+gl->width+xoff, y+gl->height+yoff);
	qglTexCoord2f (gl->sl, gl->th);
	qglVertex2i (x-xoff, y+gl->height+yoff);

	qglEnd ();

	GL_TexEnv(GL_REPLACE);
	if (enabled == 1)
	{
		qglEnable(GL_ALPHA_TEST);
		qglDisable(GL_BLEND);
	}
	else if (enabled == 2)
		qglEnable(GL_ALPHA_TEST);

	qglColor4fv(colorWhite);
}

/*
=============
Draw_StretchPic
=============
*/
void Draw_StretchPic (int x, int y, int w, int h, const char *pic, float alpha)
{
	image_t *gl;
	int enabled = 0;

	gl = Draw_FindPic(pic);
	if (!gl) {
		Com_DPrintf ( "Can't find pic: %s\n", pic);
		return;
	}

	if (alpha < 1 || (gl->flags & (IT_TRANS|IT_PALETTED)) == IT_TRANS)
	{
		enabled = 1;
		qglDisable(GL_ALPHA_TEST);
		qglEnable(GL_BLEND);
		GL_TexEnv(GL_MODULATE);
		qglColor4f(1,1,1,alpha);
	}
	else if ((gl_config.renderer == GL_RENDERER_MCD || gl_config.renderer & GL_RENDERER_RENDITION) && !(gl->flags & IT_TRANS))
	{
		enabled = 2;
		qglDisable(GL_ALPHA_TEST);
	}

	GL_Bind (gl->texnum);

	qglBegin (GL_QUADS);
	qglTexCoord2f (gl->sl, gl->tl);
	qglVertex2i (x, y);
	qglTexCoord2f (gl->sh, gl->tl);
	qglVertex2i (x+w, y);
	qglTexCoord2f (gl->sh, gl->th);
	qglVertex2i (x+w, y+h);
	qglTexCoord2f (gl->sl, gl->th);
	qglVertex2i (x, y+h);
	qglEnd ();

	if (enabled == 1)
	{
		GL_TexEnv (GL_REPLACE);
		qglEnable(GL_ALPHA_TEST);
		qglDisable(GL_BLEND);
		qglColor4fv(colorWhite);
	}
	else if (enabled == 2)
		qglEnable(GL_ALPHA_TEST);

}

/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, const char *pic, float alpha)
{
	image_t *gl;
	int enabled = 0;


	gl = Draw_FindPic (pic);
	if (!gl) {
		Com_DPrintf ( "Can't find pic: %s\n", pic);
		return;
	}

	if (alpha < 1 || (gl->flags & (IT_TRANS|IT_PALETTED)) == IT_TRANS)
	{
		enabled = 1;
		qglDisable(GL_ALPHA_TEST);
		qglEnable(GL_BLEND);
		GL_TexEnv(GL_MODULATE);
		qglColor4f(1,1,1,alpha);
	}
	else if ((gl_config.renderer == GL_RENDERER_MCD || gl_config.renderer & GL_RENDERER_RENDITION) && !(gl->flags & IT_TRANS))
	{
		enabled = 2;
		qglDisable(GL_ALPHA_TEST);
	}

	GL_Bind (gl->texnum);

	qglBegin (GL_QUADS);
	qglTexCoord2f (gl->sl, gl->tl);
	qglVertex2i (x, y);
	qglTexCoord2f (gl->sh, gl->tl);
	qglVertex2i (x+gl->width, y);
	qglTexCoord2f (gl->sh, gl->th);
	qglVertex2i (x+gl->width, y+gl->height);
	qglTexCoord2f (gl->sl, gl->th);
	qglVertex2i (x, y+gl->height);
	qglEnd ();
	
	if (enabled == 1)
	{
		GL_TexEnv (GL_REPLACE);
		qglEnable(GL_ALPHA_TEST);
		qglDisable(GL_BLEND);
		qglColor4fv(colorWhite);
	}
	else if (enabled == 2)
		qglEnable(GL_ALPHA_TEST);

}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h, const char *pic)
{
	image_t	*image;

	image = Draw_FindPic (pic);
	if (!image) {
		Com_DPrintf ( "Can't find pic: %s\n", pic);
		return;
	}

	if ((gl_config.renderer == GL_RENDERER_MCD || gl_config.renderer & GL_RENDERER_RENDITION) && !(image->flags & IT_TRANS))
		qglDisable(GL_ALPHA_TEST);

	GL_Bind (image->texnum);
	qglBegin (GL_QUADS);
	qglTexCoord2f (x*ONEDIV64, y*ONEDIV64);
	qglVertex2i (x, y);
	qglTexCoord2f ( (x+w)*ONEDIV64, y*ONEDIV64);
	qglVertex2i (x+w, y);
	qglTexCoord2f ( (x+w)*ONEDIV64, (y+h)*ONEDIV64);
	qglVertex2i (x+w, y+h);
	qglTexCoord2f ( x*ONEDIV64, (y+h)*ONEDIV64 );
	qglVertex2i (x, y+h);
	qglEnd ();

	if ((gl_config.renderer == GL_RENDERER_MCD || gl_config.renderer & GL_RENDERER_RENDITION) && !(image->flags & IT_TRANS))
		qglEnable(GL_ALPHA_TEST);
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
	qglDisable (GL_TEXTURE_2D);

	qglColor3ubv(( byte * )&d_8to24table[c & 255]);

	qglBegin (GL_QUADS);

	qglVertex2i (x,y);
	qglVertex2i (x+w, y);
	qglVertex2i (x+w, y+h);
	qglVertex2i (x, y+h);

	qglEnd ();
	qglColor3fv(colorWhite);
	qglEnable (GL_TEXTURE_2D);
}

//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	qglEnable(GL_BLEND);
	qglDisable (GL_TEXTURE_2D);
	qglColor4f (0, 0, 0, 0.75);
	qglBegin (GL_QUADS);

	qglVertex2i (0,0);
	qglVertex2i (vid.width, 0);
	qglVertex2i (vid.width, vid.height);
	qglVertex2i (0, vid.height);

	qglEnd ();
	qglColor4fv(colorWhite);
	qglEnable (GL_TEXTURE_2D);
	qglDisable(GL_BLEND);
}


//====================================================================


/*
=============
Draw_StretchRaw
=============
*/
extern uint32	r_rawpalette[256];

void Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data)
{
	int			i, j, trows;
	byte		*source;
	int			frac, fracstep;
	float		hscale;
	int			row;
	float		t;
	uint32		image32[256*256] = { 0 };
	uint32		*dest = image32;

	if (rows <= 256) {
		hscale = 1;
		trows = rows;
	}
	else {
		hscale = rows * ONEDIV256;
		trows = 256;
	}

	t = rows*hscale * ONEDIV256;
	fracstep = cols*0x10000/256;

	for (i=0 ; i<trows ; i++, dest+=256)
	{
		row = (int)(i*hscale);
		if (row > rows)
			break;
		source = data + cols*row;

		frac = fracstep >> 1;
		for (j=0 ; j<256 ; j+=4)
		{
			dest[j] = r_rawpalette[source[frac>>16]];
			frac += fracstep;
			dest[j+1] = r_rawpalette[source[frac>>16]];
			frac += fracstep;
			dest[j+2] = r_rawpalette[source[frac>>16]];
			frac += fracstep;
			dest[j+3] = r_rawpalette[source[frac>>16]];
			frac += fracstep;
		}
	}

	qglBindTexture( GL_TEXTURE_2D, 0 );
	qglTexImage2D (GL_TEXTURE_2D, 0, gl_tex_solid_format, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, image32);
	
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) 
		qglDisable(GL_ALPHA_TEST);

	qglBegin (GL_QUADS);
	qglTexCoord2f (0, 0);
	qglVertex2i (x, y);
	qglTexCoord2f (1, 0);
	qglVertex2i (x+w, y);
	qglTexCoord2f (1, t);
	qglVertex2i (x+w, y+h);
	qglTexCoord2f (0, t);
	qglVertex2i (x, y+h);
	qglEnd ();

	if ( ( gl_config.renderer == GL_RENDERER_MCD ) || ( gl_config.renderer & GL_RENDERER_RENDITION ) ) 
		qglEnable(GL_ALPHA_TEST);
}

