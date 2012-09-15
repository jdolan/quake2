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
// GL_RSURF.C: surface-related refresh code

#include "gl_local.h"

static vec3_t	modelorg;		// relative to viewpoint

static msurface_t	*r_alpha_surfaces;

#define DYNAMIC_LIGHT_WIDTH  128
#define DYNAMIC_LIGHT_HEIGHT 128
#define LIGHTMAP_BYTES 4
#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128
#define	MAX_LIGHTMAPS	128

int		c_visible_lightmaps;
int		c_visible_textures;

#define GL_LIGHTMAP_FORMAT GL_RGBA

typedef struct
{
	int internal_format;
	int	current_lightmap_texture;

	msurface_t	*lightmap_surfaces[MAX_LIGHTMAPS];

	int			allocated[BLOCK_WIDTH];

	// the lightmap texture data needs to be kept in
	// main memory so texsubimage can update properly
	byte		lightmap_buffer[4*BLOCK_WIDTH*BLOCK_HEIGHT];
} gllightmapstate_t;

static gllightmapstate_t gl_lms;


static void		LM_InitBlock( void );
static void		LM_UploadBlock( qboolean dynamic );
static qboolean	LM_AllocBlock (int w, int h, int *x, int *y);

extern void R_SetCacheState( msurface_t *surf );
extern void R_BuildLightMap (const msurface_t *surf, byte *dest, int stride);

/*
=============================================================

	BRUSH MODELS

=============================================================
*/

/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
image_t *R_TextureAnimation (mtexinfo_t *tex)
{
	int		c;

	if (!tex->next)
		return tex->image;

	c = currententity->frame % tex->numframes;
	while (c)
	{
		tex = tex->next;
		c--;
	}

	return tex->image;
}

/*
================
DrawGLPoly
================
*/
static void DrawGLPoly (const glpoly_t *p, float scroll)
{
	int		i;
	const float	*v;

	v = p->verts[0];
	qglBegin (GL_POLYGON);
	for (i = 0; i < p->numverts; i++, v+= VERTEXSIZE) {
		qglTexCoord2f (v[3] + scroll, v[4]);
		qglVertex3fv (v);
	}
	qglEnd ();
}

/*
================
R_DrawTriangleOutlines
================
*/
static void R_DrawTriangleOutlines (void)
{
	int			i, j;
	const glpoly_t	*p;

	if (!gl_showtris->integer)
		return;

	qglDisable (GL_TEXTURE_2D);
	qglDisable (GL_DEPTH_TEST);

	for (i=0 ; i<MAX_LIGHTMAPS ; i++)
	{
		msurface_t *surf;

		for ( surf = gl_lms.lightmap_surfaces[i]; surf != 0; surf = surf->lightmapchain )
		{
			p = surf->polys;
			for (j=2 ; j<p->numverts ; j++ )
			{
				qglBegin (GL_LINE_STRIP);
				qglVertex3fv (p->verts[0]);
				qglVertex3fv (p->verts[j-1]);
				qglVertex3fv (p->verts[j]);
				qglVertex3fv (p->verts[0]);
				qglEnd ();
			}
		}
	}

	qglEnable (GL_DEPTH_TEST);
	qglEnable (GL_TEXTURE_2D);
}

/*
================
DrawGLPolyChain
================
*/
static void DrawGLPolyChain( const glpoly_t *p, float soffset, float toffset )
{
	const float *v;
	int i;

	v = p->verts[0];
	qglBegin (GL_POLYGON);
	for (i = 0; i < p->numverts; i++, v+= VERTEXSIZE) {
		qglTexCoord2f (v[5] - soffset, v[6] - toffset );
		qglVertex3fv (v);
	}
	qglEnd ();
}

/*
================================
	R_BlendLightMaps

This routine takes all the given light mapped surfaces in the world and
blends them into the framebuffer.
================================
*/
static void R_BlendLightmaps (qboolean isWorldModel)
{
	int			i;
	msurface_t	*surf, *newdrawsurf = 0;

	// don't bother if we're set to fullbright
	if (r_fullbright->integer)
		return;
	if (!r_worldmodel->lightdata)
		return;

	// don't bother writing Z
	qglDepthMask( GL_FALSE );

	// set the appropriate blending mode unless we're only looking at the lightmaps.
	if (!gl_lightmap->integer)
	{
		qglEnable(GL_BLEND);

		if ( gl_saturatelighting->integer )
			qglBlendFunc( GL_ONE, GL_ONE );
		else
			qglBlendFunc (GL_ZERO, GL_SRC_COLOR );
	}

	if (isWorldModel)
		c_visible_lightmaps = 0;

	// render static lightmaps first
	for ( i = 1; i < MAX_LIGHTMAPS; i++ )
	{
		if ( gl_lms.lightmap_surfaces[i] )
		{
			if (isWorldModel)
				c_visible_lightmaps++;
			GL_Bind( gl_state.lightmap_textures + i);

			for ( surf = gl_lms.lightmap_surfaces[i]; surf != 0; surf = surf->lightmapchain )
			{
				if ( surf->polys )
					DrawGLPolyChain( surf->polys, 0, 0 );
			}
		}
	}

	// render dynamic lightmaps
	if ( gl_dynamic->integer )
	{
		LM_InitBlock();

		GL_Bind( gl_state.lightmap_textures+0 );

		if (isWorldModel)
			c_visible_lightmaps++;

		newdrawsurf = gl_lms.lightmap_surfaces[0];

		for ( surf = gl_lms.lightmap_surfaces[0]; surf != 0; surf = surf->lightmapchain )
		{
			int		smax, tmax;
			byte	*base;

			smax = (surf->extents[0]>>4)+1;
			tmax = (surf->extents[1]>>4)+1;

			if ( LM_AllocBlock( smax, tmax, &surf->dlight_s, &surf->dlight_t ) )
			{
				base = gl_lms.lightmap_buffer;
				base += ( surf->dlight_t * BLOCK_WIDTH + surf->dlight_s ) * LIGHTMAP_BYTES;

				R_BuildLightMap (surf, base, BLOCK_WIDTH*LIGHTMAP_BYTES);
			}
			else
			{
				msurface_t *drawsurf;

				// upload what we have so far
				LM_UploadBlock( true );

				// draw all surfaces that use this lightmap
				for ( drawsurf = newdrawsurf; drawsurf != surf; drawsurf = drawsurf->lightmapchain )
				{
					if ( drawsurf->polys )
						DrawGLPolyChain( drawsurf->polys, 
							            ( drawsurf->light_s - drawsurf->dlight_s ) * ONEDIV128, 
										( drawsurf->light_t - drawsurf->dlight_t ) * ONEDIV128 );
				}

				newdrawsurf = drawsurf;

				// clear the block
				LM_InitBlock();

				// try uploading the block now
				if ( !LM_AllocBlock( smax, tmax, &surf->dlight_s, &surf->dlight_t ) )
				{
					Com_Error( ERR_FATAL, "Consecutive calls to LM_AllocBlock(%d,%d) failed (dynamic)\n", smax, tmax );
				}

				base = gl_lms.lightmap_buffer;
				base += ( surf->dlight_t * BLOCK_WIDTH + surf->dlight_s ) * LIGHTMAP_BYTES;

				R_BuildLightMap (surf, base, BLOCK_WIDTH*LIGHTMAP_BYTES);
			}
		}

		// draw remainder of dynamic lightmaps that haven't been uploaded yet
		if ( newdrawsurf )
			LM_UploadBlock( true );

		for ( surf = newdrawsurf; surf != 0; surf = surf->lightmapchain )
		{
			if ( surf->polys )
				DrawGLPolyChain( surf->polys, ( surf->light_s - surf->dlight_s ) * ONEDIV128, ( surf->light_t - surf->dlight_t ) * ONEDIV128);
		}
	}

	// restore state
	qglDisable(GL_BLEND);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDepthMask( GL_TRUE );
}

/*
================
R_RenderBrushPoly
================
*/
static void R_RenderBrushPoly (msurface_t *fa)
{
	int			maps;
	image_t		*image;
	qboolean is_dynamic = false;
	float   scroll = 0;

	c_brush_polys++;

	image = R_TextureAnimation (fa->texinfo);

	if (fa->flags & SURF_DRAWTURB)
	{	
		GL_Bind( image->texnum );

		// warp texture, no lightmaps
		GL_TexEnv( GL_MODULATE );
		qglColor4f( gl_state.inverse_intensity, 
			        gl_state.inverse_intensity,
					gl_state.inverse_intensity,
					1.0F );
		EmitWaterPolys( fa->polys, (fa->texinfo->flags & SURF_FLOWING) );
		qglColor4fv(colorWhite);
		GL_TexEnv( GL_REPLACE );

		return;
	}
	else
	{
		GL_Bind( image->texnum );
		GL_TexEnv( GL_REPLACE );
	}

	if(fa->texinfo->flags & SURF_FLOWING)
	{
		scroll = -64.0f * ( (r_newrefdef.time / 40.0f) - (int)(r_newrefdef.time / 40.0f) );
		if (scroll == 0.0f)
			scroll = -64.0f;
	}

	DrawGLPoly (fa->polys, scroll);

	/*
	** check for lightmap modification
	*/
	for ( maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++ )
	{
		if ( r_newrefdef.lightstyles[fa->styles[maps]].white != fa->cached_light[maps] )
			goto dynamic;
	}

	// dynamic this frame or dynamic previously
	if ( ( fa->dlightframe == r_framecount ) )
	{
dynamic:
		if ( gl_dynamic->integer )
		{
			if (!( fa->texinfo->flags & (SURF_SKY|SURF_TRANS33|SURF_TRANS66|SURF_WARP ) ) )
				is_dynamic = true;
		}
	}

	if ( is_dynamic )
	{
		if ( ( fa->styles[maps] >= 32 || fa->styles[maps] == 0 ) && ( fa->dlightframe != r_framecount ) )
		{
			unsigned	temp[34*34];
			int			smax, tmax;

			smax = (fa->extents[0]>>4)+1;
			tmax = (fa->extents[1]>>4)+1;

			R_BuildLightMap( fa, (void *)temp, smax*4 );
			R_SetCacheState( fa );

			GL_Bind( gl_state.lightmap_textures + fa->lightmaptexturenum );

			qglTexSubImage2D( GL_TEXTURE_2D, 0,
							  fa->light_s, fa->light_t, 
							  smax, tmax, 
							  GL_LIGHTMAP_FORMAT, 
							  GL_UNSIGNED_BYTE, temp );

			fa->lightmapchain = gl_lms.lightmap_surfaces[fa->lightmaptexturenum];
			gl_lms.lightmap_surfaces[fa->lightmaptexturenum] = fa;
		}
		else
		{
			fa->lightmapchain = gl_lms.lightmap_surfaces[0];
			gl_lms.lightmap_surfaces[0] = fa;
		}
	}
	else
	{
		fa->lightmapchain = gl_lms.lightmap_surfaces[fa->lightmaptexturenum];
		gl_lms.lightmap_surfaces[fa->lightmaptexturenum] = fa;
	}
}

/*
================
R_DrawAlphaSurfaces

Draw water surfaces and windows.
The BSP tree is waled front to back, so unwinding the chain
of alpha_surfaces will draw back to front, giving proper ordering.
================
*/
void R_DrawAlphaSurfaces (void)
{
	const msurface_t	*s;
	float		scroll;
	vec4_t		intens;

	// go back to the world matrix
    //qglLoadMatrixf (r_WorldViewMatrix);

	qglEnable(GL_BLEND);
	GL_TexEnv( GL_MODULATE );

	// the textures are prescaled up for a better lighting range,
	// so scale it back down
	intens[0] = intens[1] = intens[2] = gl_state.inverse_intensity;
	intens[3] = 1.0f;

	scroll = -64 * ( (r_newrefdef.time / 40.0) - (int)(r_newrefdef.time / 40.0) );
	if (scroll == 0)
		scroll = -64;

	for (s = r_alpha_surfaces; s; s = s->texturechain)
	{
		GL_Bind(s->texinfo->image->texnum);
		c_brush_polys++;

		if (s->texinfo->flags & SURF_TRANS33)
			intens[3] = 0.33f;
		else if (s->texinfo->flags & SURF_TRANS66)
			intens[3] = 0.66f;
		else
			intens[3] = 1.0f;

		qglColor4fv(intens);

		if (s->flags & SURF_DRAWTURB)
			EmitWaterPolys( s->polys, (s->texinfo->flags & SURF_FLOWING) );
		else
			DrawGLPoly( s->polys, (s->texinfo->flags & SURF_FLOWING) ? scroll : 0 );
	}

	GL_TexEnv( GL_REPLACE );
	qglColor4fv(colorWhite);
	qglDisable(GL_BLEND);

	r_alpha_surfaces = NULL;
}

/*
================
DrawTextureChains
================
*/
static void DrawTextureChains (void)
{
	int		i;
	msurface_t	*s;
	image_t		*image;

	c_visible_textures = 0;

	if ( !gl_state.multiTexture )
	{
		for ( i = 0, image=gltextures ; i<numgltextures ; i++,image++)
		{
			if (!image->registration_sequence)
				continue;
			s = image->texturechain;
			if (!s)
				continue;

			c_visible_textures++;
			for (; s; s = s->texturechain)
				R_RenderBrushPoly (s);

			image->texturechain = NULL;
		}
	}
	else
	{
		for ( i = 0, image=gltextures ; i<numgltextures ; i++,image++)
		{
			if (!image->registration_sequence)
				continue;
			s = image->texturechain;
			if (!s)
				continue;

			c_visible_textures++;
			for (; s; s = s->texturechain) {
				if ( !( s->flags & SURF_DRAWTURB ) )
					R_RenderBrushPoly (s);
			}
		}

		GL_EnableMultitexture( false );
		for ( i = 0, image=gltextures ; i<numgltextures ; i++,image++)
		{
			if (!image->registration_sequence)
				continue;
			s = image->texturechain;
			if (!s)
				continue;

			for (; s; s = s->texturechain) {
				if ( s->flags & SURF_DRAWTURB )
					R_RenderBrushPoly (s);
			}

			image->texturechain = NULL;
		}
	}

	GL_TexEnv( GL_REPLACE );
}

void GL_DrawPoly(int nv, msurface_t *surf)
{
	float	*v, r, g, b;
	int i;

	GL_SelectTexture(1);
	qglDisable(GL_TEXTURE_2D);
	GL_SelectTexture(0);
	qglDisable(GL_TEXTURE_2D);

	if (gl_eff_world_bg_type->integer)
	{
		if (gl_eff_world_bg_color_r->value < 256)
			r = gl_eff_world_bg_color_r->value/255;
		else
			r = (float)random()*255/255;

		if (gl_eff_world_bg_color_g->value < 256)
			g = gl_eff_world_bg_color_g->value/255;
		else
			g = (float)random()*255/255;

		if (gl_eff_world_bg_color_b->value < 256)
			b = gl_eff_world_bg_color_b->value/255;
		else
			b = (float)random()*255/255;


		v = surf->polys->verts[0];
		qglColor3f (r, g, b);
		qglBegin (GL_POLYGON);
		for (i=0 ; i < nv; i++, v += VERTEXSIZE)
		{
			qglVertex3fv (v);
		}
		qglEnd ();
	}

	if (gl_eff_world_wireframe->integer)
	{
		qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		if (gl_eff_world_wireframe->value != WIREFRAME_LINE)
		{
			qglEnable (GL_LINE_STIPPLE);
			if (gl_eff_world_wireframe->value == WIREFRAME_DASH)
				qglLineStipple (1, 0x00FF);
			else if (gl_eff_world_wireframe->value == WIREFRAME_DOT)
				qglLineStipple (1, 0x0101);
			else if (gl_eff_world_wireframe->value == WIREFRAME_DOT_DASH)
				qglLineStipple (1, 0x1C47);
		}

		if (gl_eff_world_lines_color_r->value < 256)
			r = gl_eff_world_lines_color_r->value/255;
		else
			r = (float)random()*255/255;

		if (gl_eff_world_lines_color_g->value < 256)
			g = gl_eff_world_lines_color_g->value/255;
		else
			g = (float)random()*255/255;

		if (gl_eff_world_lines_color_b->value < 256)
			b = gl_eff_world_lines_color_b->value/255;
		else
			b = (float)random()*255/255;


		v = surf->polys->verts[0];
		qglColor3f (r, g, b);
		qglBegin (GL_POLYGON);
		for (i=0 ; i < nv; i++, v += VERTEXSIZE)
		{
			qglVertex3fv (v);
		}
		qglEnd ();

		if (gl_eff_world_wireframe->value != WIREFRAME_LINE)
			qglDisable (GL_LINE_STIPPLE);
		qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	}

	qglColor3fv(colorWhite);
	GL_SelectTexture(1);
	qglEnable(GL_TEXTURE_2D);
	GL_SelectTexture(0);
	qglEnable(GL_TEXTURE_2D);

}

extern void EmitCausticPolys (const glpoly_t *p);

static void GL_RenderLightmappedPoly( msurface_t *surf )
{
	int		i, nv = surf->polys->numverts;
	int		map;
	float	*v;
	image_t *image = R_TextureAnimation( surf->texinfo );
	qboolean is_dynamic = false;
	unsigned lmtex = surf->lightmaptexturenum;
	float scroll = 0;

	for ( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++ )
	{
		if ( r_newrefdef.lightstyles[surf->styles[map]].white != surf->cached_light[map] )
			goto dynamic;
	}

	// dynamic this frame or dynamic previously
	if ( ( surf->dlightframe == r_framecount ) )
	{
dynamic:
		if ( gl_dynamic->integer )
		{
			if ( !(surf->texinfo->flags & (SURF_SKY|SURF_TRANS33|SURF_TRANS66|SURF_WARP ) ) )
				is_dynamic = true;
		}
	}

	if ( is_dynamic )
	{
		unsigned	temp[128*128];
		int			smax, tmax;

		smax = (surf->extents[0]>>4)+1;
		tmax = (surf->extents[1]>>4)+1;

		R_BuildLightMap( surf, (void *)temp, smax*4 );

		if ( ( surf->styles[map] >= 32 || surf->styles[map] == 0 ) && ( surf->dlightframe != r_framecount ) )
		{
			R_SetCacheState( surf );

			lmtex = surf->lightmaptexturenum;
		}
		else
		{
			lmtex = 0;
		}

		GL_MBind(1, gl_state.lightmap_textures + lmtex);

		qglTexSubImage2D( GL_TEXTURE_2D, 0,
						  surf->light_s, surf->light_t, 
						  smax, tmax, 
						  GL_LIGHTMAP_FORMAT, 
						  GL_UNSIGNED_BYTE, temp );

	}

	c_brush_polys++;

	GL_MBind(0, image->texnum);
	GL_MBind(1, gl_state.lightmap_textures + lmtex);

	if (surf->texinfo->flags & SURF_FLOWING)
	{
		scroll = -64.0f * ( (r_newrefdef.time / 40.0f) - (int)(r_newrefdef.time / 40.0f) );
		if(scroll == 0.0f)
			scroll = -64.0f;
	}

	if(gl_eff_world_bg_type->integer) {
		GL_DrawPoly(nv,surf);
	} else {
		v = surf->polys->verts[0];

		qglBegin (GL_POLYGON);
		for (i=0 ; i< nv; i++, v+= VERTEXSIZE)
		{
			qglMTexCoord2fSGIS(QGL_TEXTURE0, v[3] + scroll, v[4]);
			qglMTexCoord2fSGIS(QGL_TEXTURE1, v[5], v[6]);
			qglVertex3fv (v);
		}
		qglEnd ();

		if (gl_watercaustics->integer && (surf->flags & SURF_UNDERWATER) && !(image->flags & IT_TRANS))
			EmitCausticPolys(surf->polys);

		if(gl_eff_world_wireframe->integer)
			GL_DrawPoly(nv,surf);
	}

}

/*
=================
R_DrawInlineBModel
=================
*/
static void R_DrawInlineBModel (bspSubmodel_t *subModel)
{
	int			i, k;
	cplane_t	*pplane;
	float		dot;
	msurface_t	*psurf;
	dlight_t	*lt;

	if(!subModel)
		Com_Error(ERR_FATAL, "subModel == NULL");

	// calculate dynamic lighting for bmodel
	if (!gl_flashblend->integer && subModel->headnode)
	{
		lt = r_newrefdef.dlights;
		if (currententity->angles[0] || currententity->angles[1] || currententity->angles[2])
		{
			vec3_t temp;

			for (k=0 ; k<r_newrefdef.num_dlights ; k++, lt++)
			{
				VectorSubtract (lt->origin, currententity->origin, temp);
				lt->origin[0] = DotProduct (temp, currententity->axis[0]);
				lt->origin[1] = DotProduct (temp, currententity->axis[1]);
				lt->origin[2] = DotProduct (temp, currententity->axis[2]);
				R_MarkLights( lt, 1<<k, subModel->headnode );
				VectorAdd (temp, currententity->origin, lt->origin);
			}
		}
		else
		{
			for (k=0 ; k<r_newrefdef.num_dlights ; k++, lt++)
			{
				VectorSubtract (lt->origin, currententity->origin, lt->origin);
				R_MarkLights( lt, 1<<k, subModel->headnode );
				VectorAdd (lt->origin, currententity->origin, lt->origin);
			}
		}
	}

	psurf = subModel->firstFace;

	if ( currententity->flags & RF_TRANSLUCENT )
	{
		qglEnable(GL_BLEND);
		qglColor4f (1,1,1,0.25);
		GL_TexEnv( GL_MODULATE );
	}

	// draw texture
	for (i = 0; i < subModel->numFaces; i++, psurf++)
	{
	// find which side of the node we are on
		pplane = psurf->plane;

		if ( pplane->type < 3 )
			dot = modelorg[pplane->type] - pplane->dist;
		else
			dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

	// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			if (psurf->texinfo->flags & (SURF_TRANS33|SURF_TRANS66) )
			{
				// add to the translucent chain
				psurf->texturechain = r_alpha_surfaces;
				r_alpha_surfaces = psurf;
			}
			else if ( qglMTexCoord2fSGIS && !( psurf->flags & SURF_DRAWTURB ) )
			{
				GL_RenderLightmappedPoly( psurf );
			}
			else
			{
				GL_EnableMultitexture( false );
				R_RenderBrushPoly( psurf );
				GL_EnableMultitexture( true );
			}
		}
	}

	if ( !(currententity->flags & RF_TRANSLUCENT) )
	{
		if ( !qglMTexCoord2fSGIS )
			R_BlendLightmaps(false);
	}
	else
	{
		qglDisable(GL_BLEND);
		qglColor4fv(colorWhite);
		GL_TexEnv( GL_REPLACE );
	}
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel (bspSubmodel_t *subModel)
{
	vec3_t		mins, maxs;
	qboolean	rotated;

	if (subModel->numFaces == 0)
		return;

	gl_state.currentTextures[0] = gl_state.currentTextures[1] = -1;

	if (currententity->angles[0] || currententity->angles[1] || currententity->angles[2])
	{
		rotated = true;
		mins[0] = currententity->origin[0] - subModel->radius;
		mins[1] = currententity->origin[1] - subModel->radius;
		mins[2] = currententity->origin[2] - subModel->radius;
		maxs[0] = currententity->origin[0] + subModel->radius;
		maxs[1] = currententity->origin[1] + subModel->radius;
		maxs[2] = currententity->origin[2] + subModel->radius;
	}
	else
	{
		rotated = false;
		VectorAdd (currententity->origin, subModel->mins, mins);
		VectorAdd (currententity->origin, subModel->maxs, maxs);
	}

	if (R_CullBox (mins, maxs))
		return;

	memset (gl_lms.lightmap_surfaces, 0, sizeof(gl_lms.lightmap_surfaces));

	VectorSubtract (r_newrefdef.vieworg, currententity->origin, modelorg);
	if (rotated)
	{
		vec3_t	temp;

		VectorCopy(modelorg, temp);
		modelorg[0] = DotProduct(temp, currententity->axis[0]);
		modelorg[1] = DotProduct(temp, currententity->axis[1]);
		modelorg[2] = DotProduct(temp, currententity->axis[2]);

		R_RotateForEntity(currententity->origin, currententity->axis);
	}
	else {
		R_TranslateForEntity(currententity->origin);
	}

	GL_EnableMultitexture( true );
	GL_SelectTexture(1);

	if ( gl_lightmap->integer )
		GL_TexEnv( GL_REPLACE );
	else
		GL_TexEnv( GL_MODULATE );

	R_DrawInlineBModel(subModel);

	GL_EnableMultitexture( false );
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

/*
================
R_RecursiveWorldNode
================
*/
static void R_RecursiveWorldNode (mnode_t *node, int clipflags)
{
	int			c, side, sidebit;
	cplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	float		dot;
	image_t		*image;

	if (node->contents == CONTENTS_SOLID)
		return;		// solid

	if (node->visframe != r_visframecount)
		return;

	if( clipflags ) {
		int			i, clipped;
		cplane_t	*clipplane;
		for (i=0,clipplane=frustum ; i<4 ; i++,clipplane++) {
			if(!(clipflags & (1<<i)))
				continue;
			clipped = BOX_ON_PLANE_SIDE(node->minmaxs, node->minmaxs+3, clipplane);
			if( clipped == 2 )
				return;
			else if( clipped == 1 )
				clipflags &= ~(1<<i);	// node is entirely on screen
		}	
	}
	
	// if a leaf node, draw stuff
	if (node->contents != CONTENTS_NODE)
	{
		pleaf = (mleaf_t *)node;

		// check for door connected areas
		if (r_newrefdef.areabits)
		{
			if (! (r_newrefdef.areabits[pleaf->area>>3] & (1<<(pleaf->area&7)) ) )
				return;		// not visible
		}

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}

		return;
	}

// node is just a decision point, so go down the appropriate sides
// find which side of the node we are on
	plane = node->plane;

	if ( plane->type < 3 )
		dot = modelorg[plane->type] - plane->dist;
	else
		dot = DotProduct (modelorg, plane->normal) - plane->dist;

	if (dot >= 0)
	{
		side = sidebit = 0;
	}
	else
	{
		side = 1;
		sidebit = SURF_PLANEBACK;
	}

	// recurse down the children, front side first
	R_RecursiveWorldNode (node->children[side], clipflags);

	// draw stuff
	for ( c = node->numsurfaces, surf = r_worldmodel->surfaces + node->firstsurface; c ; c--, surf++)
	{
		if (surf->visframe != r_framecount)
			continue;

		if ( (surf->flags & SURF_PLANEBACK) != sidebit )
			continue;		// wrong side

		if (surf->texinfo->flags & SURF_SKY)
		{	// just adds to visible sky bounds
			R_AddSkySurface (surf);
		}
		else if (surf->texinfo->flags & (SURF_TRANS33|SURF_TRANS66))
		{	// add to the translucent chain
			surf->texturechain = r_alpha_surfaces;
			r_alpha_surfaces = surf;
		}
		else
		{
			if ( qglMTexCoord2fSGIS && !( surf->flags & SURF_DRAWTURB ) )
			{
				GL_RenderLightmappedPoly( surf );
			}
			else
			{
				// the polygon is visible, so add it to the texture sorted chain
				// FIXME: this is a hack for animation
				image = R_TextureAnimation (surf->texinfo);
				surf->texturechain = image->texturechain;
				image->texturechain = surf;
			}
		}
	}

	// recurse down the back side
	R_RecursiveWorldNode (node->children[!side], clipflags);

}


/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld (void)
{
	entity_t	ent;

	if (!r_drawworld->integer)
		return;

	if ( r_newrefdef.rdflags & RDF_NOWORLDMODEL )
		return;


	VectorCopy (r_newrefdef.vieworg, modelorg);

	// auto cycle the world frame for texture animation
	memset (&ent, 0, sizeof(ent));
	ent.frame = (int)(r_newrefdef.time*2);
	currententity = &ent;

	gl_state.currentTextures[0] = gl_state.currentTextures[1] = -1;

	memset (gl_lms.lightmap_surfaces, 0, sizeof(gl_lms.lightmap_surfaces));

	R_ClearSkyBox ();

	if ( qglMTexCoord2fSGIS )
	{
		GL_EnableMultitexture( true );

		GL_SelectTexture(1);

		if ( gl_lightmap->integer )
			GL_TexEnv( GL_REPLACE );
		else 
			GL_TexEnv( GL_MODULATE );

		R_RecursiveWorldNode (r_worldmodel->nodes, (r_nocull->integer ? 0 : 15));

		GL_EnableMultitexture( false );
	}
	else
	{
		R_RecursiveWorldNode (r_worldmodel->nodes, (r_nocull->integer ? 0 : 15));
	}

	// theoretically nothing should happen in the next two functions if multitexture is enabled
	DrawTextureChains ();
	R_BlendLightmaps(true);

	if(gl_eff_world_bg_type->integer && qglMTexCoord2fSGIS)
	{
		vec3_t	color;

		GL_EnableMultitexture( true );
		GL_SelectTexture(1);
		qglDisable(GL_TEXTURE_2D);
		GL_SelectTexture(0);
		qglDisable(GL_TEXTURE_2D);
		if (gl_eff_world_bg_color_r->value < 256)
			color[0] = gl_eff_world_bg_color_r->value/255;
		else
			color[0] = (float)random()*255/255;

		if (gl_eff_world_bg_color_g->value < 256)
			color[1] = gl_eff_world_bg_color_g->value/255;
		else
			color[1] = (float)random()*255/255;

		if (gl_eff_world_bg_color_b->value < 256)
			color[2] = gl_eff_world_bg_color_b->value/255;
		else
			color[2] = (float)random()*255/255;

		qglColor3fv(color);
		R_DrawSkyBox ();
		qglColor3fv(colorWhite);
		GL_SelectTexture(1);
		qglEnable(GL_TEXTURE_2D);
		GL_SelectTexture(0);
		qglEnable(GL_TEXTURE_2D);
		GL_EnableMultitexture( false );
	} else {
		R_DrawSkyBox ();
	}

	R_DrawTriangleOutlines ();
}


/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current
cluster
===============
*/
void R_MarkLeaves (void)
{
	byte	*vis, fatvis[MAX_MAP_LEAFS/8];
	mnode_t	*node;
	mleaf_t	*leaf;
	int		i, c, cluster;

	if (r_oldviewcluster == r_viewcluster && r_oldviewcluster2 == r_viewcluster2 && !r_novis->integer && r_viewcluster != -1)
		return;

	// development aid to let you run around and see exactly where the pvs ends
	if (gl_lockpvs->integer)
		return;

	r_visframecount++;
	r_oldviewcluster = r_viewcluster;
	r_oldviewcluster2 = r_viewcluster2;

	if (r_novis->integer || r_viewcluster == -1 || !r_worldmodel->vis)
	{
		// mark everything
		for (i=0 ; i<r_worldmodel->numleafs ; i++)
			r_worldmodel->leafs[i].visframe = r_visframecount;
		for (i=0 ; i<r_worldmodel->numnodes ; i++)
			r_worldmodel->nodes[i].visframe = r_visframecount;
		return;
	}

	vis = Mod_ClusterPVS (r_viewcluster, r_worldmodel);
	// may have to combine two clusters because of solid water boundaries
	if (r_viewcluster2 != r_viewcluster)
	{
		memcpy (fatvis, vis, (r_worldmodel->numleafs+7)/8);
		vis = Mod_ClusterPVS (r_viewcluster2, r_worldmodel);
		c = (r_worldmodel->numleafs+31)/32;
		for (i=0 ; i<c ; i++)
			((int *)fatvis)[i] |= ((int *)vis)[i];
		vis = fatvis;
	}
	
	for (i=0,leaf=r_worldmodel->leafs ; i<r_worldmodel->numleafs ; i++, leaf++)
	{
		cluster = leaf->cluster;
		if (cluster == -1)
			continue;
		if (vis[cluster>>3] & (1<<(cluster&7)))
		{
			node = (mnode_t *)leaf;
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}

}



/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

static void LM_InitBlock( void )
{
	memset( gl_lms.allocated, 0, sizeof( gl_lms.allocated ) );
}

static void LM_UploadBlock( qboolean dynamic )
{
	int texture;
	int height = 0;

	if ( dynamic )
		texture = 0;
	else
		texture = gl_lms.current_lightmap_texture;

	GL_Bind( gl_state.lightmap_textures + texture );
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if ( dynamic )
	{
		int i;

		for ( i = 0; i < BLOCK_WIDTH; i++ ) {
			if ( gl_lms.allocated[i] > height )
				height = gl_lms.allocated[i];
		}

		qglTexSubImage2D( GL_TEXTURE_2D, 
						  0,
						  0, 0,
						  BLOCK_WIDTH, height,
						  GL_LIGHTMAP_FORMAT,
						  GL_UNSIGNED_BYTE,
						  gl_lms.lightmap_buffer );
	}
	else
	{
		qglTexImage2D( GL_TEXTURE_2D, 
					   0, 
					   gl_lms.internal_format,
					   BLOCK_WIDTH, BLOCK_HEIGHT, 
					   0, 
					   GL_LIGHTMAP_FORMAT, 
					   GL_UNSIGNED_BYTE, 
					   gl_lms.lightmap_buffer );
		if ( ++gl_lms.current_lightmap_texture == MAX_LIGHTMAPS )
			Com_Error( ERR_DROP, "LM_UploadBlock() - MAX_LIGHTMAPS exceeded\n" );
	}
}

// returns a texture number and the position inside it
static qboolean LM_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;

	best = BLOCK_HEIGHT;

	for (i=0 ; i<BLOCK_WIDTH-w ; i++)
	{
		best2 = 0;

		for (j=0 ; j<w ; j++)
		{
			if (gl_lms.allocated[i+j] >= best)
				break;
			if (gl_lms.allocated[i+j] > best2)
				best2 = gl_lms.allocated[i+j];
		}
		if (j == w)
		{	// this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > BLOCK_HEIGHT)
		return false;

	for (i=0 ; i<w ; i++)
		gl_lms.allocated[*x + i] = best + h;

	return true;
}

/*
================
GL_BuildPolygonFromSurface
================
*/
void GL_BuildPolygonFromSurface(msurface_t *fa, bspModel_t *bspModel)
{
	int			i, lindex, lnumverts;
	medge_t		*pedges, *r_pedge;
	float		*vec;
	float		s, t;
	glpoly_t	*poly;

	// reconstruct the polygon
	pedges = bspModel->edges;
	lnumverts = fa->numedges;

	// draw texture
	poly = Hunk_Alloc (sizeof(glpoly_t) + (lnumverts-4) * VERTEXSIZE*sizeof(float));

	poly->numverts = lnumverts;
	fa->polys = poly;

	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = bspModel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = bspModel->vertexes[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = bspModel->vertexes[r_pedge->v[1]].position;
		}
		
		if (!(fa->flags & SURF_DRAWTURB))
		{
			s = poly->verts[i][5] = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
			s /= fa->texinfo->image->width;
			t = poly->verts[i][6] = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
			t /= fa->texinfo->image->height;
		}
		else
		{
			s = DotProduct (vec, fa->texinfo->vecs[0]);
			t = DotProduct (vec, fa->texinfo->vecs[1]);
		}

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		// lightmap texture coordinates
		s = poly->verts[i][5] - fa->texturemins[0] + fa->light_s*16 + 8;
		s /= BLOCK_WIDTH*16;

		t = poly->verts[i][6] - fa->texturemins[1] + fa->light_t*16 + 8;
		t /= BLOCK_HEIGHT*16;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap (msurface_t *surf)
{
	int		smax, tmax;
	byte	*base;

//	if (surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
//		return;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	if ( !LM_AllocBlock( smax, tmax, &surf->light_s, &surf->light_t ) )
	{
		LM_UploadBlock( false );
		LM_InitBlock();
		if ( !LM_AllocBlock( smax, tmax, &surf->light_s, &surf->light_t ) )
			Com_Error( ERR_FATAL, "Consecutive calls to LM_AllocBlock(%d,%d) failed\n", smax, tmax );
	}

	surf->lightmaptexturenum = gl_lms.current_lightmap_texture;

	base = gl_lms.lightmap_buffer;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * LIGHTMAP_BYTES;

	R_SetCacheState( surf );
	R_BuildLightMap (surf, base, BLOCK_WIDTH*LIGHTMAP_BYTES);
}


/*
==================
GL_BeginBuildingLightmaps

==================
*/
void GL_BeginBuildingLightmaps (void) //(model_t *m)
{
	static lightstyle_t	lightstyles[MAX_LIGHTSTYLES];
	int				i;
	unsigned		dummy[128*128];

	memset( gl_lms.allocated, 0, sizeof(gl_lms.allocated) );

	r_framecount = 1;		// no dlightcache

	GL_EnableMultitexture( true );

	GL_SelectTexture(1);

	/*
	** setup the base lightstyles so the lightmaps won't have to be regenerated
	** the first time they're seen
	*/
	for (i=0 ; i<MAX_LIGHTSTYLES ; i++)
	{
		lightstyles[i].rgb[0] = lightstyles[i].rgb[1] = lightstyles[i].rgb[2] = 1;
		lightstyles[i].white = 3;
	}
	r_newrefdef.lightstyles = lightstyles;

	if (!gl_state.lightmap_textures)
		gl_state.lightmap_textures	= TEXNUM_LIGHTMAPS;


	gl_lms.current_lightmap_texture = 1;

	gl_lms.internal_format = gl_tex_solid_format;

	/*
	** initialize the dynamic lightmap texture
	*/
	GL_Bind( gl_state.lightmap_textures + 0 );
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qglTexImage2D( GL_TEXTURE_2D, 
				   0, 
				   gl_lms.internal_format,
				   BLOCK_WIDTH, BLOCK_HEIGHT, 
				   0, 
				   GL_LIGHTMAP_FORMAT, 
				   GL_UNSIGNED_BYTE, 
				   dummy );
}

/*
=======================
GL_EndBuildingLightmaps
=======================
*/
void GL_EndBuildingLightmaps (void)
{
	LM_UploadBlock( false );
	GL_EnableMultitexture( false );
}

