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
// r_main.c
#include "gl_local.h"

viddef_t	vid;

unsigned int QGL_TEXTURE0, QGL_TEXTURE1;

bspModel_t	*r_worldmodel;

double		gldepthmin, gldepthmax;
int			vid_scaled_width, vid_scaled_height;
qboolean usingmodifiedlightmaps;

glconfig_t gl_config;
glstate_t  gl_state;

image_t		*r_notexture;		// use for bad textures
image_t		*r_whitetexture;
image_t		*r_particletexture;	// little dot for particles
image_t		*r_caustictexture;	//Water caustic texture
image_t		*r_shelltexture;

entity_t	*currententity;
//model_t		*currentmodel;

cplane_t	frustum[4];

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

int			c_brush_polys, c_alias_polys;

void GL_Strings_f( void );

//
// view origin
//
float			r_ModelViewMatrix[16];
static float	r_ProjectionMatrix[16];
float			r_WorldViewMatrix[16];

vec3_t	viewAxis[3];
vec3_t	r_origin;

//float	r_world_matrix[16];
//float	r_base_world_matrix[16];

//
// screen size info
//
refdef_t	r_newrefdef;

int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

cvar_t	*r_norefresh;
cvar_t	*r_drawentities;
cvar_t	*r_drawworld;
cvar_t	*r_speeds;
cvar_t	*r_fullbright;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_lerpmodels;
cvar_t	*r_lefthand;

cvar_t	*r_lightlevel;	// FIXME: This is a HACK to get the client's light level

//cvar_t	*gl_nosubimage;
cvar_t	*gl_allow_software;

cvar_t	*gl_vertex_arrays;

cvar_t	*gl_particle_min_size;
cvar_t	*gl_particle_max_size;
cvar_t	*gl_particle_size;
cvar_t	*gl_particle_att_a;
cvar_t	*gl_particle_att_b;
cvar_t	*gl_particle_att_c;
cvar_t	*gl_particle_scale;

cvar_t	*gl_ext_swapinterval;
cvar_t	*gl_ext_multitexture;
cvar_t	*gl_ext_pointparameters;
cvar_t	*gl_ext_compiled_vertex_array;

cvar_t	*gl_bitdepth;
cvar_t	*gl_drawbuffer;
cvar_t  *gl_driver;
cvar_t	*gl_lightmap;
cvar_t	*gl_shadows;
cvar_t	*gl_mode;
cvar_t	*gl_dynamic;
//cvar_t  *gl_monolightmap;
cvar_t	*gl_modulate;
cvar_t	*gl_nobind;
cvar_t	*gl_round_down;
cvar_t	*gl_picmip;
cvar_t	*gl_skymip;
cvar_t	*gl_showtris;
cvar_t	*gl_ztrick;
cvar_t	*gl_finish;
cvar_t	*gl_clear;
cvar_t	*gl_cull;
cvar_t	*gl_polyblend;
cvar_t	*gl_flashblend;
//cvar_t	*gl_playermip;
cvar_t	*gl_swapinterval;
cvar_t	*gl_texturemode;
cvar_t	*gl_texturebits;
cvar_t	*gl_lockpvs;

cvar_t	*gl_3dlabs_broken;

cvar_t	*gl_eff_world_wireframe;
cvar_t	*gl_eff_world_bg_type;
cvar_t	*gl_eff_world_bg_color_r;
cvar_t	*gl_eff_world_bg_color_g;
cvar_t	*gl_eff_world_bg_color_b;
cvar_t	*gl_eff_world_lines_color_r;
cvar_t	*gl_eff_world_lines_color_g;
cvar_t	*gl_eff_world_lines_color_b;

extern cvar_t	*vid_fullscreen;
extern cvar_t	*vid_gamma;
void VID_Restart_f (void);

//Added cvar's -Maniac
cvar_t *skydistance; // DMP - skybox size change
cvar_t *gl_screenshot_quality;

cvar_t 	*gl_replacewal;
cvar_t  *gl_replacepcx;
cvar_t	*gl_replacemd2;
cvar_t	*gl_stainmaps;				// stainmaps
cvar_t	*gl_motionblur;				// motionblur
cvar_t	*gl_waterwaves;
cvar_t	*gl_fontshadow;
cvar_t	*gl_particle;
cvar_t	*gl_sgis_mipmap;
cvar_t	*gl_ext_texture_compression;
cvar_t	*gl_celshading;				//celshading
cvar_t	*gl_celshading_width;
cvar_t	*gl_scale;
cvar_t	*gl_watercaustics;
cvar_t	*gl_fog;
cvar_t	*gl_fog_density;
cvar_t	*gl_coloredlightmaps;
cvar_t	*r_customwidth;
cvar_t	*r_customheight;
cvar_t	*gl_gammapics;
cvar_t	*gl_shelleffect;
cvar_t	*gl_minlight_entities;

cvar_t	*gl_ext_texture_filter_anisotropic;
cvar_t	*gl_ext_max_anisotropy;

cvar_t	*gl_multisample;

/*cvar_t	*gl_colorbits;
cvar_t	*gl_alphabits;
cvar_t	*gl_depthbits;
cvar_t	*gl_stencilbits;*/

static void R_ModeList_f( void );

// vertex arrays
vArrays_t r_arrays;

/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (const vec3_t mins, const vec3_t maxs)
{
	int		i;
	cplane_t *p;

	if (r_nocull->integer)
		return false;

	for (i=0,p=frustum ; i<4; i++,p++)
	{
		switch (p->signbits)
		{
		case 0:
			if (p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 1:
			if (p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 2:
			if (p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 3:
			if (p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*maxs[2] < p->dist)
				return true;
			break;
		case 4:
			if (p->normal[0]*maxs[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		case 5:
			if (p->normal[0]*mins[0] + p->normal[1]*maxs[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		case 6:
			if (p->normal[0]*maxs[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		case 7:
			if (p->normal[0]*mins[0] + p->normal[1]*mins[1] + p->normal[2]*mins[2] < p->dist)
				return true;
			break;
		default:
			return false;
		}
	}

	return false;
}


static qboolean isModelMatrix = false;

static void R_LoadModelIdentity (void)
{
	if(!isModelMatrix)
		return;

	qglLoadMatrixf(r_WorldViewMatrix);
	isModelMatrix = false;
}

void R_TranslateForEntity (const vec3_t origin)
{
	float	*wM = r_WorldViewMatrix;

	r_ModelViewMatrix[0]  = wM[0];
	r_ModelViewMatrix[1]  = wM[1];
	r_ModelViewMatrix[2]  = wM[2];

	r_ModelViewMatrix[4]  = wM[4];
	r_ModelViewMatrix[5]  = wM[5];
	r_ModelViewMatrix[6]  = wM[6];

	r_ModelViewMatrix[8]  = wM[8];
	r_ModelViewMatrix[9]  = wM[9];
	r_ModelViewMatrix[10] = wM[10];

	r_ModelViewMatrix[12] = wM[0] * origin[0] + wM[4] * origin[1] + wM[8 ] * origin[2] + wM[12];
	r_ModelViewMatrix[13] = wM[1] * origin[0] + wM[5] * origin[1] + wM[9 ] * origin[2] + wM[13];
	r_ModelViewMatrix[14] = wM[2] * origin[0] + wM[6] * origin[1] + wM[10] * origin[2] + wM[14];

	qglLoadMatrixf(r_ModelViewMatrix);
	isModelMatrix = true;
}

void R_RotateForEntity (const vec3_t origin, vec3_t axis[3])
{
	float	*wM = r_WorldViewMatrix;

	r_ModelViewMatrix[0]  = wM[0] * axis[0][0] + wM[4] * axis[0][1] + wM[8 ] * axis[0][2];
	r_ModelViewMatrix[1]  = wM[1] * axis[0][0] + wM[5] * axis[0][1] + wM[9 ] * axis[0][2];
	r_ModelViewMatrix[2]  = wM[2] * axis[0][0] + wM[6] * axis[0][1] + wM[10] * axis[0][2];

	r_ModelViewMatrix[4]  = wM[0] * axis[1][0] + wM[4] * axis[1][1] + wM[8 ] * axis[1][2];
	r_ModelViewMatrix[5]  = wM[1] * axis[1][0] + wM[5] * axis[1][1] + wM[9 ] * axis[1][2];
	r_ModelViewMatrix[6]  = wM[2] * axis[1][0] + wM[6] * axis[1][1] + wM[10] * axis[1][2];

	r_ModelViewMatrix[8]  = wM[0] * axis[2][0] + wM[4] * axis[2][1] + wM[8 ] * axis[2][2];
	r_ModelViewMatrix[9]  = wM[1] * axis[2][0] + wM[5] * axis[2][1] + wM[9 ] * axis[2][2];
	r_ModelViewMatrix[10] = wM[2] * axis[2][0] + wM[6] * axis[2][1] + wM[10] * axis[2][2];

	r_ModelViewMatrix[12] = wM[0] * origin[0] + wM[4] * origin[1] + wM[8 ] * origin[2] + wM[12];
	r_ModelViewMatrix[13] = wM[1] * origin[0] + wM[5] * origin[1] + wM[9 ] * origin[2] + wM[13];
	r_ModelViewMatrix[14] = wM[2] * origin[0] + wM[6] * origin[1] + wM[10] * origin[2] + wM[14];

	qglLoadMatrixf(r_ModelViewMatrix);
	isModelMatrix = true;
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/


/*
=================
R_DrawSpriteModel

=================
*/
static void R_DrawSpriteModel (model_t *model)
{
	spriteFrame_t	*frame;
	qboolean		alpha = false;
	vec3_t			point;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache

	R_LoadModelIdentity();

	frame = &model->sModel->frames[currententity->frame % model->sModel->numFrames];

	// normal sprite
	if (currententity->flags & RF_TRANSLUCENT && currententity->alpha < 1.0F) {
		alpha = true;
		qglEnable(GL_BLEND);
		qglColor4f( 1, 1, 1, currententity->alpha );
	}
	else
		qglEnable(GL_ALPHA_TEST);

	GL_Bind(frame->image->texnum);

	GL_TexEnv( GL_MODULATE );

	qglBegin (GL_QUADS);

	qglTexCoord2i (0, 1);
	VectorMA (currententity->origin, -frame->origin_y, viewAxis[2], point);
	VectorMA (point, frame->origin_x, viewAxis[1], point);
	qglVertex3fv (point);

	qglTexCoord2i (0, 0);
	VectorMA (currententity->origin, frame->height - frame->origin_y, viewAxis[2], point);
	VectorMA (point, frame->origin_x, viewAxis[1], point);
	qglVertex3fv (point);

	qglTexCoord2i (1, 0);
	VectorMA (currententity->origin, frame->height - frame->origin_y, viewAxis[2], point);
	VectorMA (point, frame->origin_x - frame->width, viewAxis[1], point);
	qglVertex3fv (point);

	qglTexCoord2i (1, 1);
	VectorMA (currententity->origin, -frame->origin_y, viewAxis[2], point);
	VectorMA (point, frame->origin_x - frame->width, viewAxis[1], point);
	qglVertex3fv (point);
	
	qglEnd ();

	GL_TexEnv( GL_REPLACE );

	if (alpha) {
		qglDisable(GL_BLEND);
		qglColor4fv(colorWhite);
	}
	else
		qglDisable(GL_ALPHA_TEST);

}

//==================================================================================

/*
=============
R_DrawNullModel
=============
*/
static void R_DrawNullModel (void)
{
	vec3_t	shadelight;
	int		i;

	if ( currententity->flags & RF_FULLBRIGHT )
		shadelight[0] = shadelight[1] = shadelight[2] = 1.0F;
	else
		R_LightPoint (currententity->origin, shadelight);

	R_LoadModelIdentity();

	qglDisable (GL_TEXTURE_2D);
	qglColor3fv (shadelight);

	qglBegin (GL_TRIANGLE_FAN);

	qglVertex3f (currententity->origin[0], currententity->origin[1], currententity->origin[2]-16);
	for (i=0 ; i<=4 ; i++)
		qglVertex3f ( currententity->origin[0] + 16*(float)cos(i*M_PI_DIV_2),
					currententity->origin[1] + 16*(float)sin(i*M_PI_DIV_2), 
					currententity->origin[2]);
	qglEnd ();

	qglBegin (GL_TRIANGLE_FAN);
	qglVertex3f (currententity->origin[0], currententity->origin[1], currententity->origin[2]+16);
	for (i=4 ; i>=0 ; i--)
		qglVertex3f ( currententity->origin[0] + 16*(float)cos(i*M_PI_DIV_2),
					currententity->origin[1] + 16*(float)sin(i*M_PI_DIV_2), 
					currententity->origin[2]);

	qglEnd ();

	qglColor3fv(colorWhite);
	qglEnable (GL_TEXTURE_2D);
}

/*
=============
R_DrawEntitiesOnList
=============
*/
static void R_DrawEntitiesOnList (void)
{
	model_t	*model;
	entity_t *lastE, *transE = NULL;

	if (!r_drawentities->integer)
		return;

	lastE = r_newrefdef.entities + r_newrefdef.num_entities;
	// draw non-transparent first
	for (currententity = r_newrefdef.entities; currententity != lastE; currententity++)
	{
		if (currententity->flags & RF_TRANSLUCENT) {
			if(!transE)
				transE = currententity;
			continue;	// solid
		}

		if ( currententity->flags & RF_BEAM ) {
			R_DrawBeam ();
			continue;
		}

		model = currententity->model;
		if (!model) {
			R_DrawNullModel ();
			continue;
		}

		switch (model->type) {
		case mod_alias:
			R_DrawAliasModel( model );
			break;
		case mod_brush:
			R_DrawBrushModel( model->subModel );
			break;
		case mod_sprite:
			R_DrawSpriteModel( model );
			break;
		default:
			Com_Error (ERR_DROP, "Bad modeltype");
			break;
		}
	}

	if(!transE) {
		R_LoadModelIdentity();
		return;
	}

	// draw transparent entities, we could sort these if it ever becomes a problem...
	qglDepthMask (GL_FALSE);		// no z writes
	for (currententity = transE; currententity != lastE; currententity++)
	{
		if (!(currententity->flags & RF_TRANSLUCENT))
			continue;	// solid

		if ( currententity->flags & RF_BEAM ) {
			R_DrawBeam ();
			continue;
		}

		model = currententity->model;
		if (!model) {
			R_DrawNullModel ();
			continue;
		}

		switch (model->type) {
		case mod_alias:
			R_DrawAliasModel( model );
			break;
		case mod_brush:
			R_DrawBrushModel( model->subModel );
			break;
		case mod_sprite:
			R_DrawSpriteModel( model );
			break;
		default:
			Com_Error (ERR_DROP, "Bad modeltype");
			break;
		}
	}
	qglDepthMask (GL_TRUE);		// back to writing

	R_LoadModelIdentity();
}

static int defaultTcoords[TESS_MAX_VERTICES*2];
static qboolean defaultTCInitialized = false;
static qboolean defaultInxInitialized = false;

static void GL_InitArrays (void)
{
	int i, currentVert, *dst_idx, *dst_tc;

	if(!defaultInxInitialized) {
		dst_idx = r_arrays.indices;

		currentVert = 0;
		i = TESS_MAX_INDICES/6;
		while(i--) {
			dst_idx[0] = currentVert;
			dst_idx[1] = currentVert + 1;
			dst_idx[2] = currentVert + 2;
			dst_idx[3] = currentVert;
			dst_idx[4] = currentVert + 2;
			dst_idx[5] = currentVert + 3;
			dst_idx += 6;
			currentVert += 4;
		}
		defaultInxInitialized = true;
	}

	if (!defaultTCInitialized) {
		dst_tc = defaultTcoords;

		i = TESS_MAX_VERTICES/4;
		while(i--) {
			dst_tc[0] = dst_tc[1] = dst_tc[3] = dst_tc[6] = 0;
			dst_tc[2] = dst_tc[4] = dst_tc[5] = dst_tc[7] = 1;
			dst_tc += 8;
		}
		defaultTCInitialized = true;
	}

	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, r_arrays.colors );
	qglVertexPointer( 3, GL_FLOAT, 0, r_arrays.vertices );
	qglEnableClientState( GL_VERTEX_ARRAY );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
}

static void GL_DrawParticles(void)
{
	const particle_t *p;
	int				i, j, numParticles;
	float			scale, tscale;
	vec3_t			up, right;
	byte			color[4];
	uint32			*dst_color, *src_color;
    vec_t			*dst_vert;

    GL_Bind(r_particletexture->texnum);
	qglDepthMask( GL_FALSE );		// no z buffering
	qglEnable( GL_BLEND );
	GL_TexEnv( GL_MODULATE );
	if (gl_particle->integer)
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE);

	qglEnableClientState( GL_COLOR_ARRAY );
	qglTexCoordPointer( 2, GL_INT, 0, defaultTcoords );

	tscale = 1.5f * gl_particle_scale->value;

	numParticles = r_newrefdef.num_particles;
	j = TESS_MAX_VERTICES / 4;
	p = r_newrefdef.particles;
	
	src_color = (uint32 *)color;

	do {
		if (numParticles < j)
			j = numParticles;

		dst_vert = r_arrays.vertices;
		dst_color = ( uint32 * )r_arrays.colors;

		for (i = 0; i < j; i++, p++) {
			// hack a scale up to keep particles from disapearing
			scale = ( p->origin[0] - r_origin[0] ) * viewAxis[0][0] + 
					( p->origin[1] - r_origin[1] ) * viewAxis[0][1] +
					( p->origin[2] - r_origin[2] ) * viewAxis[0][2];

			scale = (scale < 20) ? tscale : tscale + scale * 0.004f;

			VectorScale(viewAxis[2], scale, up);
			VectorScale(viewAxis[1], -scale, right);
			
			dst_vert[0] = p->origin[0] - 0.5f * (up[0]+right[0]);
			dst_vert[1] = p->origin[1] - 0.5f * (up[1]+right[1]);
			dst_vert[2] = p->origin[2] - 0.5f * (up[2]+right[2]);

			dst_vert[3] = dst_vert[0] + up[0];
			dst_vert[4] = dst_vert[1] + up[1]; 
			dst_vert[5] = dst_vert[2] + up[2];

			dst_vert[6] = dst_vert[3] + right[0]; 
			dst_vert[7]	= dst_vert[4] + right[1];
			dst_vert[8] = dst_vert[5] + right[2];

			dst_vert[9]  = dst_vert[0] + right[0]; 
			dst_vert[10] = dst_vert[1] + right[1]; 
			dst_vert[11] = dst_vert[2] + right[2];
			dst_vert += 12;

			*src_color = d_8to24table[p->color & 0xFF];
			color[3] = (byte)(p->alpha * 255);
			dst_color[0] = dst_color[1] = dst_color[2] = dst_color[3] = *src_color;
			dst_color += 4;
		}

		if(gl_state.compiledVertexArray) {
			qglLockArraysEXT( 0, i*4 );
			qglDrawElements( GL_TRIANGLES, i*6, GL_UNSIGNED_INT, r_arrays.indices );
			qglUnlockArraysEXT ();
		} else {
			qglDrawElements( GL_TRIANGLES, i*6, GL_UNSIGNED_INT, r_arrays.indices );
		}
		numParticles -= i;
	} while (numParticles > 0);

	qglDisable(GL_BLEND);
	qglColor4fv(colorWhite);
	qglDepthMask( GL_TRUE );		// back to normal Z buffering
	GL_TexEnv( GL_REPLACE );
	if (gl_particle->integer)
		qglBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	qglDisableClientState( GL_COLOR_ARRAY );
}

/*
===============
R_DrawParticles
===============
*/
static void R_DrawParticles (void)
{
	if ( gl_ext_pointparameters->integer && qglPointParameterfEXT )
	{
		int i;
		byte color[4];
		const particle_t *p;

		qglDepthMask( GL_FALSE );
		qglEnable(GL_BLEND);
		qglDisable( GL_TEXTURE_2D );

		qglPointSize( gl_particle_size->value );

		qglBegin( GL_POINTS );
		for ( i = 0, p = r_newrefdef.particles; i < r_newrefdef.num_particles; i++, p++ )
		{
			*(uint32 *)color = d_8to24table[p->color & 0xFF];
			color[3] = (byte)(p->alpha*255);

			qglColor4ubv( color );
			qglVertex3fv( p->origin );
		}
		qglEnd();

		qglDisable(GL_BLEND);
		qglColor4fv(colorWhite);
		qglDepthMask( GL_TRUE );
		qglEnable( GL_TEXTURE_2D );
	}
	else
	{
		GL_DrawParticles();
	}
}

/*
============
R_PolyBlend
============
*/
static void R_PolyBlend (void)
{
	if (!gl_polyblend->integer || r_newrefdef.blend[3] < 0.01f)
		return;

	qglDisable(GL_ALPHA_TEST);
	qglEnable(GL_BLEND);
	GL_TexEnv(GL_MODULATE);

	VectorScale(r_newrefdef.blend, 0.5, r_newrefdef.blend);
	qglColor4fv(r_newrefdef.blend);

	GL_Bind (r_whitetexture->texnum);

	qglBegin (GL_QUADS);
	qglVertex2i (0, 0);
	qglVertex2i (vid.width, 0);
	qglVertex2i (vid.width, vid.height);
	qglVertex2i (0, vid.height);
	qglEnd ();

	GL_TexEnv (GL_REPLACE);
	qglEnable(GL_ALPHA_TEST);
	qglDisable(GL_BLEND);
	qglColor4fv(colorWhite);
}

//=======================================================================

static void SetPlaneSignbits(cplane_t *out)
{
	out->signbits = 0;

	if (out->normal[0] < 0)
		out->signbits |= 1;
	if (out->normal[1] < 0)
		out->signbits |= 2;
	if (out->normal[2] < 0)
		out->signbits |= 4;
}

static void R_SetFrustum (void)
{
	int		i;
	float	xs, xc, ang;

	ang = r_newrefdef.fov_x / 180.0f * M_PI * 0.5f;
	Q_sincos(ang, &xs, &xc);

	VectorScale( viewAxis[0], xs, frustum[1].normal );
	VectorMA( frustum[1].normal, xc, viewAxis[1], frustum[0].normal );
	VectorMA( frustum[1].normal, -xc, viewAxis[1], frustum[1].normal );

	ang = r_newrefdef.fov_y / 180.0f * M_PI * 0.5f;
	Q_sincos(ang, &xs, &xc);

	VectorScale( viewAxis[0], xs, frustum[3].normal );
	VectorMA( frustum[3].normal, xc, viewAxis[2], frustum[2].normal );
	VectorMA( frustum[3].normal, -xc, viewAxis[2], frustum[3].normal );

	for (i = 0; i < 4; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		SetPlaneSignbits (&frustum[i]);
	}
}

//=======================================================================

/*
===============
R_SetupFrame
===============
*/
static void R_SetupFrame (void)
{

	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy (r_newrefdef.vieworg, r_origin);
	AnglesToAxis(r_newrefdef.viewangles, viewAxis);

	// current viewcluster
	if ( !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) )
	{
		mleaf_t	*leaf;
		vec3_t	temp;

		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		leaf = Mod_PointInLeaf (r_origin, r_worldmodel);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		VectorCopy (r_origin, temp);
		// check above and below so crossing solid water doesn't draw wrong
		if (!leaf->contents)
			temp[2] -= 16; // look down a bit
		else
			temp[2] += 16;

		leaf = Mod_PointInLeaf (temp, r_worldmodel);
		if ( !(leaf->contents & CONTENTS_SOLID) && (leaf->cluster != r_viewcluster2) )
			r_viewcluster2 = leaf->cluster;
	}
	else // clear out the portion of the screen that the NOWORLDMODEL defines
	{
		qglEnable( GL_SCISSOR_TEST );
		qglScissor( r_newrefdef.x, vid.height - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width, r_newrefdef.height );
		qglClear( GL_DEPTH_BUFFER_BIT );
		qglDisable( GL_SCISSOR_TEST );
	}
}


float skybox_farz = 4096;

/*
static float R_SetFarClip (void) {

	float	farDist, dirDist, worldDist = 0;
	vec_t *mins, *maxs;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return 4096.0f;

	dirDist = DotProduct(r_origin, viewAxis[0]);
	farDist = dirDist + 256.0f;

	mins = &r_worldmodel->nodes[0].minmaxs[0];
	maxs = &r_worldmodel->nodes[0].minmaxs[3];
	worldDist += (viewAxis[0][0] < 0 ? mins[0] : maxs[0]) * viewAxis[0][0] + 
				 (viewAxis[0][1] < 0 ? mins[1] : maxs[1]) * viewAxis[0][1] +
				 (viewAxis[0][2] < 0 ? mins[2] : maxs[2]) * viewAxis[0][2];

	if (farDist < worldDist)
		farDist = worldDist;

	return farDist - dirDist + 256.0f;
}*/

static void R_InitMatrices (void)
{
	memset(r_ProjectionMatrix, 0, sizeof(r_ProjectionMatrix));
	memset(r_WorldViewMatrix, 0, sizeof(r_WorldViewMatrix));
	memset(r_ModelViewMatrix, 0, sizeof(r_ModelViewMatrix));

	//These doesnt change
	r_ProjectionMatrix[11] = -1.0f;
	r_WorldViewMatrix[15] = 1.0f;
	r_ModelViewMatrix[15] = 1.0f;
}

static void R_SetupProjection( void )
{
	float	xmin, xmax, ymin, ymax;
	float	width, height, depth;
	float	zNear, zFar;

	// set up projection matrix
	zNear	= 4.0f;
	zFar	= skybox_farz;

	ymax = zNear * (float)tan( r_newrefdef.fov_y * M_PI_DIV_360 );
	ymin = -ymax;

	//xmax = ymax * ((float)r_newrefdef.width / r_newrefdef.height);
	xmax = zNear * (float)tan( r_newrefdef.fov_x * M_PI_DIV_360 );
	xmin = -xmax;

	if(gl_state.camera_separation) {
		xmin += -( 2.0f * gl_state.camera_separation ) / zNear;
		xmax += -( 2.0f * gl_state.camera_separation ) / zNear;
	}

	width = xmax - xmin;
	height = ymax - ymin;
	depth = zFar - zNear;

	r_ProjectionMatrix[0] = 2.0f * zNear / width;

	r_ProjectionMatrix[5] = 2.0f * zNear / height;

	r_ProjectionMatrix[8] = ( xmax + xmin ) / width;	// normally 0
	r_ProjectionMatrix[9] = ( ymax + ymin ) / height;	// normally 0
	r_ProjectionMatrix[10] = -( zFar + zNear ) / depth;

	r_ProjectionMatrix[14] = -2.0f * zFar * zNear / depth;
}

/*
=============
R_SetupModelviewMatrix
=============
*/
static void R_SetupModelviewMatrix( void )
{
	r_WorldViewMatrix[ 0] = -viewAxis[1][0];
	r_WorldViewMatrix[ 1] = viewAxis[2][0];
	r_WorldViewMatrix[ 2] = -viewAxis[0][0];

	r_WorldViewMatrix[ 4] = -viewAxis[1][1];
	r_WorldViewMatrix[ 5] = viewAxis[2][1];
	r_WorldViewMatrix[ 6] = -viewAxis[0][1];

	r_WorldViewMatrix[ 8] = -viewAxis[1][2];
	r_WorldViewMatrix[ 9] = viewAxis[2][2];
	r_WorldViewMatrix[10] = -viewAxis[0][2];

	r_WorldViewMatrix[12] = DotProduct(r_origin, viewAxis[1]);
	r_WorldViewMatrix[13] = -DotProduct(r_origin, viewAxis[2]);
	r_WorldViewMatrix[14] = DotProduct(r_origin, viewAxis[0]);
}

/*
=============
R_SetupGL
=============
*/
static void R_SetupGL (void)
{
	// set up viewport
	//qglScissor( r_newrefdef.x, vid.height - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width, r_newrefdef.height );
	qglViewport( r_newrefdef.x, vid.height - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width, r_newrefdef.height );

	// set up projection matrix
	R_SetupProjection();
	qglMatrixMode(GL_PROJECTION);
    qglLoadMatrixf( r_ProjectionMatrix );

	// set up the world view matrix
	R_SetupModelviewMatrix();
	qglMatrixMode(GL_MODELVIEW);
	qglLoadMatrixf( r_WorldViewMatrix );

	// set drawing parms
	if (gl_cull->integer)
		qglEnable(GL_CULL_FACE);

	qglDisable(GL_ALPHA_TEST);
	qglEnable(GL_DEPTH_TEST);
}

/*
=============
R_Clear
=============
*/
static void R_Clear (void)
{
	int	bits = 0;

	if (gl_clear->integer)
		bits |= GL_COLOR_BUFFER_BIT;

	if (gl_state.stencil && gl_shadows->integer == 2) {
		qglClearStencil(128);
		bits |= GL_STENCIL_BUFFER_BIT;
	}

	if (gl_ztrick->integer && r_worldmodel != NULL)
	{
		static int trickframe = 0;

		if (bits)
			qglClear (bits);

		trickframe++;
		if (trickframe & 1) {
			gldepthmin = 0;
			gldepthmax = 0.49999;
			qglDepthFunc (GL_LEQUAL);
		} else {
			gldepthmin = 1;
			gldepthmax = 0.5;
			qglDepthFunc (GL_GEQUAL);
		}
	}
	else
	{
		bits |= GL_DEPTH_BUFFER_BIT;
		qglClear(bits);

		gldepthmin = 0;
		gldepthmax = 1;
		qglDepthFunc (GL_LEQUAL);
	}

	qglDepthRange (gldepthmin, gldepthmax);
}

/*
================
R_RenderView

r_newrefdef must be set before the first call
================
*/
static void R_RenderView (refdef_t *fd)
{
	if (r_norefresh->integer)
		return;

	r_newrefdef = *fd;

	if (!r_worldmodel && !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) )
		Com_Error (ERR_DROP, "R_RenderView: NULL worldmodel");

	if (gl_scale->value > 1.0f) {
		r_newrefdef.width *= gl_scale->value;
		r_newrefdef.height *= gl_scale->value;
		r_newrefdef.x *= gl_scale->value;
		r_newrefdef.y *= gl_scale->value;
	}

	c_brush_polys = c_alias_polys = 0;

	qglColorPointer(  4, GL_FLOAT, 0, r_arrays.colors);

	R_PushDlights();

	if (gl_finish->integer)
		qglFinish();

	R_SetupFrame();

	R_SetFrustum ();

	R_SetupGL();

	R_MarkLeaves();	// done here so we know if we're in water

	R_DrawWorld();

	R_AddDecals(); //Decals

	R_DrawEntitiesOnList();
	
	R_RenderDlights();

	GL_InitArrays();

	if (r_newrefdef.num_particles)
		R_DrawParticles();

	R_DrawAlphaSurfaces();

//	R_PolyBlend();

	if (r_speeds->integer)
	{
		Com_Printf ("%4i wpoly %4i epoly %i tex %i lmaps %i parts\n",
			c_brush_polys, 
			c_alias_polys, 
			c_visible_textures, 
			c_visible_lightmaps,
			r_newrefdef.num_particles); 
	}

	if(gl_fog->integer)
	{
		static const float fogcolor[4] = {0.09f,0.1f,0.12f,1.0f};

		qglFogi		(GL_FOG_MODE, GL_EXP2);
		qglFogf		(GL_FOG_DENSITY, gl_fog_density->value*2/4096);
		qglFogfv	(GL_FOG_COLOR, fogcolor);
		qglEnable	(GL_FOG);
	}
}

static void R_MotionBlur(void)
{
	static unsigned int blurtex = 0;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	if (!blurtex)
	{
		qglGenTextures(1,&blurtex);
		qglBindTexture(GL_TEXTURE_RECTANGLE_NV,blurtex);
		qglCopyTexImage2D(GL_TEXTURE_RECTANGLE_NV,0,GL_RGB,r_newrefdef.x,r_newrefdef.y,r_newrefdef.width,r_newrefdef.height,0);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		return;
	}

    qglMatrixMode(GL_PROJECTION); 
    qglLoadIdentity (); 
    qglOrtho  (0, r_newrefdef.width, r_newrefdef.height, 0, -99999, 99999); 
    qglMatrixMode(GL_MODELVIEW); 
    qglLoadIdentity (); 

	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);

	qglDisable(GL_ALPHA_TEST);
	qglEnable(GL_BLEND);
	GL_TexEnv(GL_MODULATE);
	qglEnable(GL_TEXTURE_RECTANGLE_NV);
	qglColor4f (1,1,1,0.5f);
	
	qglBegin(GL_QUADS);
	qglTexCoord2i(0, r_newrefdef.height);
	qglVertex2i(0,0);
	qglTexCoord2i(r_newrefdef.width, r_newrefdef.height);
	qglVertex2i(r_newrefdef.width, 0);
	qglTexCoord2i(r_newrefdef.width, 0);
	qglVertex2i(r_newrefdef.width, r_newrefdef.height);
	qglTexCoord2i(0,0);
	qglVertex2i(0, r_newrefdef.height);
	qglEnd();

	qglDisable(GL_TEXTURE_RECTANGLE_NV);
	GL_TexEnv( GL_REPLACE );
	qglDisable(GL_BLEND);
	qglEnable(GL_ALPHA_TEST);
	qglColor4fv(colorWhite);

	qglBindTexture(GL_TEXTURE_RECTANGLE_NV,blurtex);
	qglCopyTexImage2D(GL_TEXTURE_RECTANGLE_NV,0,GL_RGB,0,0,r_newrefdef.width,r_newrefdef.height,0);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

static void	R_SetGL2D (void)
{
	// set 2D virtual screen size
	qglViewport (0,0, vid.width, vid.height);

	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();

	qglOrtho(0, vid_scaled_width, vid_scaled_height, 0, -99999, 99999);
//	qglOrtho  (0, vid.width, vid.height, 0, -99999, 99999);

	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();

	qglDisable(GL_DEPTH_TEST);
	qglDisable(GL_CULL_FACE);

	qglEnable(GL_ALPHA_TEST);
}

/*
====================
R_SetLightLevel

====================
*/
static void R_SetLightLevel (void)
{
	vec3_t		shadelight;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	// save off light value for server to look at (BIG HACK!)

	R_LightPoint (r_newrefdef.vieworg, shadelight);

	// pick the greatest component, which should be the same
	// as the mono value returned by software
	if (shadelight[0] > shadelight[1])
	{
		if (shadelight[0] > shadelight[2])
			r_lightlevel->value = 150.0f*shadelight[0];
		else
			r_lightlevel->value = 150.0f*shadelight[2];
	}
	else
	{
		if (shadelight[1] > shadelight[2])
			r_lightlevel->value = 150.0f*shadelight[1];
		else
			r_lightlevel->value = 150.0f*shadelight[2];
	}

}

/*
=====================
R_RenderFrame
=====================
*/
void R_RenderFrame (refdef_t *fd)
{
	R_RenderView( fd );
	R_SetLightLevel ();

	if (gl_motionblur->integer && gl_state.tex_rectangle)
		R_MotionBlur();

	R_SetGL2D ();

	R_PolyBlend();
}


static void OnChange_Scale(cvar_t *self, const char *oldValue)
{
	int width, height;

	if( self->value < 1.0f ) {
		Cvar_Set( self->name, "1" );
		if((float)atof(oldValue) == 1.0f)
			return;
	}

	vid_scaled_width = (int)ceilf((float)vid.width / self->value);
	vid_scaled_height = (int)ceilf((float)vid.height / self->value);

	//round to powers of 8/2 to avoid blackbars
	width = (vid_scaled_width+7)&~7;
	height = (vid_scaled_height+1)&~1;

	// reload the appropriate conchars
	Draw_InitLocal();

	// lie to client about new scaled window size
	VID_NewWindow( width, height );
}

static void OnChange_WaterWaves(cvar_t *self, const char *oldValue)
{
	if (self->value < 0)
		Cvar_Set(self->name, "0");
	else if (self->value > 4)
		Cvar_Set(self->name, "4");
}

// texturemode stuff
static void OnChange_TexMode(cvar_t *self, const char *oldValue)
{
	GL_TextureMode( self->string );
	self->modified = false;
}

static void OnChange_Skydistace(cvar_t *self, const char *oldValue)
{
	GLdouble boxsize;

	boxsize = self->integer;
	boxsize -= 252 * ceil(boxsize / 2300);
	skybox_farz = 1.0;
	while (skybox_farz < boxsize)  // make this value a power-of-2
	{
		skybox_farz *= 2.0;
		if (skybox_farz >= 65536.0)  // don't make it larger than this
			break;
  	}
	skybox_farz *= 2.0;	// double since boxsize is distance from camera to edge of skybox
	if (skybox_farz >= 65536.0)  // don't make it larger than this
		skybox_farz = 65536.0;
}

static void OnChange_Fog(cvar_t *self, const char *oldValue)
{
	qglDisable(GL_FOG);
}

static void OnChangeCustomWidth(cvar_t *self, const char *oldValue)
{
	if(gl_state.prev_mode == -1)
		gl_state.prev_mode = 3;

	if(self->integer && self->integer < 320)
		Cvar_Set(self->name, "320");
}

static void OnChangeCustomHeight(cvar_t *self, const char *oldValue)
{
	if(gl_state.prev_mode == -1)
		gl_state.prev_mode = 3;

	if(self->integer && self->integer < 240)
		Cvar_Set(self->name, "240");
}

static void OnChangeMaxAnisotropy(cvar_t *self, const char *oldValue)
{
	int		i;
	image_t	*image;

	if(!gl_config.anisotropic)
		return;

	if(self->value > gl_config.maxAnisotropic)
	{
		Cvar_SetValue(self->name, gl_config.maxAnisotropic);
		if(atof(oldValue) == gl_config.maxAnisotropic)
			return;
	}
	else if (self->value < 0)
	{
		Cvar_Set(self->name, "0");
		if(atof(oldValue) == 0)
			return;
	}

	for (i=0, image=gltextures; i<numgltextures ; i++, image++)
	{
		if (image->type != it_pic && image->type != it_sky )
		{
			GL_Bind (image->texnum);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, self->value);
		}
	}
}

static void OnChange_SwapInterval(cvar_t *self, const char *oldValue) {
	VID_Restart_f();
}

static void OnChange_Multisample(cvar_t *self, const char *oldValue) {
	VID_Restart_f();
}

static void OnChange_ShellEffect(cvar_t *self, const char *oldValue)
{
	if (!self->integer)
		return;

	if (!r_shelltexture) {
		r_shelltexture = GL_FindImage("pics/shell.tga", it_pic);
		if(!r_shelltexture)
			r_shelltexture = r_particletexture;
	}
}

static void OnChange_Caustics(cvar_t *self, const char *oldValue)
{
	if (!self->integer)
		return;

	if (!r_caustictexture) {
		r_caustictexture = GL_FindImage("pics/caustic.png", it_wall);
		if(!r_caustictexture)
			r_caustictexture = r_notexture;
	}
}

extern float r_turbsin[256];

static void GL_Register( void )
{
	int j;
	static qboolean render_initialized = false;

	if(!render_initialized) {
		for ( j = 0; j < 256; j++ ) {
			r_turbsin[j] *= 0.5f;
		}
	}

	Cvar_Subsystem( CVAR_SYSTEM_VIDEO );

	r_lefthand = Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	r_norefresh = Cvar_Get ("r_norefresh", "0", 0);
	r_fullbright = Cvar_Get ("r_fullbright", "0", CVAR_CHEAT);
	r_drawentities = Cvar_Get ("r_drawentities", "1", 0);
	r_drawworld = Cvar_Get ("r_drawworld", "1", CVAR_CHEAT);
	r_novis = Cvar_Get ("r_novis", "0", 0);
	r_nocull = Cvar_Get ("r_nocull", "0", 0);
	r_lerpmodels = Cvar_Get ("r_lerpmodels", "1", 0);
	r_speeds = Cvar_Get ("r_speeds", "0", 0);

	r_lightlevel = Cvar_Get ("r_lightlevel", "0", 0);

	//gl_nosubimage = Cvar_Get( "gl_nosubimage", "0", 0 );
	gl_allow_software = Cvar_Get( "gl_allow_software", "0", CVAR_LATCHED );

	gl_particle_min_size = Cvar_Get( "gl_particle_min_size", "2", CVAR_ARCHIVE );
	gl_particle_max_size = Cvar_Get( "gl_particle_max_size", "40", CVAR_ARCHIVE );
	gl_particle_size = Cvar_Get( "gl_particle_size", "40", CVAR_ARCHIVE );
	gl_particle_att_a = Cvar_Get( "gl_particle_att_a", "0.01", CVAR_ARCHIVE );
	gl_particle_att_b = Cvar_Get( "gl_particle_att_b", "0.0", CVAR_ARCHIVE );
	gl_particle_att_c = Cvar_Get( "gl_particle_att_c", "0.01", CVAR_ARCHIVE );

	gl_modulate = Cvar_Get ("gl_modulate", "1.666", CVAR_ARCHIVE );
	gl_bitdepth = Cvar_Get( "gl_bitdepth", "0", CVAR_ARCHIVE|CVAR_LATCHED );
	gl_mode = Cvar_Get( "gl_mode", "-1", CVAR_ARCHIVE );

	gl_lightmap = Cvar_Get ("gl_lightmap", "0", CVAR_CHEAT);
	gl_shadows = Cvar_Get ("gl_shadows", "2", CVAR_ARCHIVE );
	gl_dynamic = Cvar_Get ("gl_dynamic", "1", 0);
	gl_nobind = Cvar_Get ("gl_nobind", "0", 0);
	gl_round_down = Cvar_Get ("gl_round_down", "1", CVAR_ARCHIVE|CVAR_LATCHED);
	gl_picmip = Cvar_Get ("gl_picmip", "0", CVAR_ARCHIVE|CVAR_LATCHED);
	gl_skymip = Cvar_Get ("gl_skymip", "0", CVAR_ARCHIVE|CVAR_LATCHED);
	gl_showtris = Cvar_Get ("gl_showtris", "0", 0);
	gl_ztrick = Cvar_Get ("gl_ztrick", "0", 0);
	gl_finish = Cvar_Get ("gl_finish", "0", CVAR_ARCHIVE);
	gl_clear = Cvar_Get ("gl_clear", "0", 0);
	gl_cull = Cvar_Get ("gl_cull", "1", 0);
	gl_polyblend = Cvar_Get ("gl_polyblend", "1", 0);
	gl_flashblend = Cvar_Get ("gl_flashblend", "0", 0);
//	gl_playermip = Cvar_Get ("gl_playermip", "0", 0);
//	gl_monolightmap = Cvar_Get( "gl_monolightmap", "0", 0 );
	gl_driver = Cvar_Get( "gl_driver", GL_DRIVERNAME, CVAR_ARCHIVE|CVAR_LATCHED);	
	gl_texturemode = Cvar_Get( "gl_texturemode", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE );
	gl_texturebits = Cvar_Get( "gl_texturebits", "0", CVAR_ARCHIVE|CVAR_LATCHED);
	gl_lockpvs = Cvar_Get( "gl_lockpvs", "0", CVAR_CHEAT );

	gl_vertex_arrays = Cvar_Get( "gl_vertex_arrays", "1", CVAR_ARCHIVE );

	gl_ext_swapinterval = Cvar_Get( "gl_ext_swapinterval", "1", CVAR_ARCHIVE|CVAR_LATCHED );
	gl_ext_multitexture = Cvar_Get( "gl_ext_multitexture", "1", CVAR_ARCHIVE|CVAR_LATCHED );
	gl_ext_pointparameters = Cvar_Get( "gl_ext_pointparameters", "0", CVAR_ARCHIVE|CVAR_LATCHED );
	gl_ext_compiled_vertex_array = Cvar_Get( "gl_ext_compiled_vertex_array", "1", CVAR_ARCHIVE|CVAR_LATCHED );

	gl_drawbuffer = Cvar_Get( "gl_drawbuffer", "GL_BACK", 0 );
	gl_swapinterval = Cvar_Get( "gl_swapinterval", "1", CVAR_ARCHIVE );
	gl_swapinterval->OnChange = OnChange_SwapInterval;

	gl_3dlabs_broken = Cvar_Get( "gl_3dlabs_broken", "1", CVAR_ARCHIVE|CVAR_LATCHED );

//	vid_fullscreen = Cvar_Get( "vid_fullscreen", "0", CVAR_ARCHIVE );
//	vid_gamma = Cvar_Get( "vid_gamma", "1.0", CVAR_ARCHIVE );

	//Added cvar's -Maniac
	skydistance = Cvar_Get("skydistance", "4096", CVAR_ARCHIVE ); // DMP - skybox size change

	gl_replacewal = Cvar_Get( "gl_replacewal", "1", CVAR_ARCHIVE|CVAR_LATCHED);
	gl_replacepcx = Cvar_Get( "gl_replacepcx", "1", CVAR_ARCHIVE|CVAR_LATCHED);
	gl_replacemd2 = Cvar_Get( "gl_replacemd2", "0", CVAR_ARCHIVE|CVAR_LATCHED);
	gl_fontshadow = Cvar_Get( "gl_fontshadow", "0", CVAR_ARCHIVE);

	gl_stainmaps = Cvar_Get( "gl_stainmaps", "1", CVAR_ARCHIVE|CVAR_LATCHED );
	gl_sgis_mipmap = Cvar_Get( "gl_sgis_mipmap", "1", CVAR_LATCHED );
	gl_ext_texture_compression = Cvar_Get( "gl_ext_texture_compression", "0", CVAR_ARCHIVE|CVAR_LATCHED );
	gl_celshading = Cvar_Get ( "gl_celshading", "0", CVAR_ARCHIVE ); //Celshading
	gl_celshading_width = Cvar_Get ( "gl_celshading_width", "4", CVAR_ARCHIVE );
	gl_scale = Cvar_Get ("gl_scale", "2.0", CVAR_ARCHIVE);

	gl_watercaustics = Cvar_Get ("gl_watercaustics", "1", 0);
	gl_watercaustics->OnChange = OnChange_Caustics;

    gl_screenshot_quality = Cvar_Get( "gl_screenshot_quality", "85", CVAR_ARCHIVE );
	gl_motionblur = Cvar_Get( "gl_motionblur", "0", 0 );	// motionblur
	gl_waterwaves = Cvar_Get( "gl_waterwaves", "1", CVAR_ARCHIVE );	// waterwave
	gl_particle = Cvar_Get ( "gl_particle", "1", CVAR_ARCHIVE );
	gl_particle_scale = Cvar_Get( "gl_particle_scale", "2", 0 );

	gl_gammapics = Cvar_Get( "gl_gammapics", "1", CVAR_ARCHIVE|CVAR_LATCHED );
	gl_fog = Cvar_Get ("gl_fog", "0", 0);
	gl_fog_density = Cvar_Get ("gl_fog_density", "1", 0);
	gl_coloredlightmaps = Cvar_Get( "gl_coloredlightmaps", "1", CVAR_ARCHIVE|CVAR_LATCHED);

	gl_eff_world_wireframe = Cvar_Get( "gl_eff_world_wireframe", "0", CVAR_CHEAT );
	gl_eff_world_bg_type = Cvar_Get( "gl_eff_world_bg_type", "0", CVAR_CHEAT );
	gl_eff_world_bg_color_r = Cvar_Get( "gl_eff_world_bg_color_r", "255", 0 );
	gl_eff_world_bg_color_g = Cvar_Get( "gl_eff_world_bg_color_g", "255", 0 );
	gl_eff_world_bg_color_b = Cvar_Get( "gl_eff_world_bg_color_b", "255", 0 );
	gl_eff_world_lines_color_r = Cvar_Get( "gl_eff_world_lines_color_r", "0", 0 );
	gl_eff_world_lines_color_g = Cvar_Get( "gl_eff_world_lines_color_g", "255", 0 );
	gl_eff_world_lines_color_b = Cvar_Get( "gl_eff_world_lines_color_b", "0", 0 );

	gl_shelleffect = Cvar_Get ("gl_shelleffect", "1", CVAR_ARCHIVE);
	gl_shelleffect->OnChange = OnChange_ShellEffect;

	gl_minlight_entities = Cvar_Get("gl_minlight_entities", "1", 0);

	/*gl_colorbits =	Cvar_Get ("gl_colorbits", "0", 0);
	gl_alphabits =		Cvar_Get ("gl_alphabits", "", 0);
	gl_depthbits =		Cvar_Get ("gl_depthbits", "", 0);
	gl_stencilbits =	Cvar_Get ("gl_stencilbits", "0", 0);*/

	gl_ext_texture_filter_anisotropic = Cvar_Get("gl_ext_texture_filter_anisotropic", "1", CVAR_ARCHIVE|CVAR_LATCHED);
	gl_ext_max_anisotropy = Cvar_Get("gl_ext_max_anisotropy", "999", CVAR_ARCHIVE);
	gl_ext_max_anisotropy->OnChange = OnChangeMaxAnisotropy;

	gl_texturemode->OnChange = OnChange_TexMode;

	gl_multisample = Cvar_Get("gl_multisample", "0", CVAR_ARCHIVE);
	gl_multisample->OnChange = OnChange_Multisample;

	r_customwidth = Cvar_Get ("r_customwidth",  "0", CVAR_ARCHIVE);
	r_customheight = Cvar_Get ("r_customheight", "0", CVAR_ARCHIVE);
	r_customwidth->OnChange = OnChangeCustomWidth;
	r_customheight->OnChange = OnChangeCustomHeight;
	OnChangeCustomWidth(r_customwidth, r_customwidth->resetString);
	OnChangeCustomHeight(r_customheight, r_customheight->resetString);

	if( gl_scale->value < 1.0f )
		Cvar_Set( "gl_scale", "1.0" );

	gl_waterwaves->OnChange = OnChange_WaterWaves;
	gl_scale->OnChange = OnChange_Scale;
	skydistance->OnChange = OnChange_Skydistace;
	gl_fog->OnChange = OnChange_Fog;
	OnChange_WaterWaves(gl_waterwaves, gl_waterwaves->resetString);
	OnChange_Skydistace(skydistance, skydistance->resetString);
	//End

	Cmd_AddCommand( "modelist", R_ModeList_f );
	Cmd_AddCommand( "imagelist", GL_ImageList_f );
	Cmd_AddCommand( "screenshot", GL_ScreenShot_f );
    Cmd_AddCommand( "screenshotjpg", GL_ScreenShot_f );
	Cmd_AddCommand( "modellist", Mod_Modellist_f );
	Cmd_AddCommand( "gl_strings", GL_Strings_f );

	Cvar_Subsystem( CVAR_SYSTEM_GENERIC );
	render_initialized = true;
}

void GL_Unregister (void)
{
	Cmd_RemoveCommand( "modelist" );
	Cmd_RemoveCommand( "modellist" );
	Cmd_RemoveCommand( "imagelist" );
	Cmd_RemoveCommand( "gl_strings" );

	Cmd_RemoveCommand( "screenshot" );
	Cmd_RemoveCommand( "screenshotjpg" );

	gl_watercaustics->OnChange = NULL;
	gl_shelleffect->OnChange = NULL;
	gl_ext_max_anisotropy->OnChange = NULL;
	gl_texturemode->OnChange = NULL;
	r_customwidth->OnChange = NULL;
	r_customheight->OnChange = NULL;
	gl_waterwaves->OnChange = NULL;
	gl_scale->OnChange = NULL;
	skydistance->OnChange = NULL;
	gl_fog->OnChange = NULL;
}

/*
================
R_GetModeInfo
================
*/
typedef struct vidmode_s
{
	const char *description;
	int         width, height;
	int         mode;
} vidmode_t;

static const vidmode_t r_vidModes[] =
{
	{ "Mode 0: 320x240",	 320,  240,	0  },
	{ "Mode 1: 400x300",	 400,  300,	1  },
	{ "Mode 2: 512x384",	 512,  384,	2  },
	{ "Mode 3: 640x480",	 640,  480,	3  },
	{ "Mode 4: 800x600",	 800,  600,	4  },
	{ "Mode 5: 960x720",	 960,  720,	5  },
	{ "Mode 6: 1024x768",	1024,  768,	6  },
	{ "Mode 7: 1152x864",	1152,  864,	7  },
	{ "Mode 8: 1280x960",	1280,  960,	8  },
	{ "Mode 9: 1600x1200",	1600, 1200,	9  },
	{ "Mode 10: 2048x1536",	2048, 1536,	10 },
	{ "Mode 11: 1024x480",	1024,  480,	11 },
	{ "Mode 12: 1280x768",	1280,  768,	12 },
	{ "Mode 13: 1280x1024",	1280, 1024,	13 }
};

static const int s_numVidModes = ( sizeof( r_vidModes ) / sizeof( r_vidModes[0] ) );

qboolean R_GetModeInfo( int *width, int *height, int mode )
{
	if ( mode < -1 || mode >= s_numVidModes )
		return false;

	if ( mode == -1 ) {
		*width = r_customwidth->integer;
		*height = r_customheight->integer;
		//*windowAspect = r_customaspect->value;
		return true;
	}
	*width  = r_vidModes[mode].width;
	*height = r_vidModes[mode].height;

	return true;
}

static void R_ModeList_f( void )
{
	int i;

	Com_Printf("%sMode -1: Custom resolution (%dx%d)\n", -1 == gl_mode->integer ? "\x02" : "", r_customwidth->integer, r_customheight->integer);
	for ( i = 0; i < s_numVidModes; i++ ) {
		Com_Printf("%s%s\n", i == gl_mode->integer ? "\x02" : "", r_vidModes[i].description);
	}
}

/*
==================
R_SetMode
==================
*/
qboolean R_SetMode (void)
{
	rserr_t err;
	qboolean fullscreen;

#ifdef _WIN32
	if ( vid_fullscreen->modified && !gl_config.allow_cds )
	{
		Com_Printf ( "R_SetMode: CDS not allowed with this driver.\n" );
		Cvar_SetValue( "vid_fullscreen", !vid_fullscreen->integer );
		vid_fullscreen->modified = false;
	}
#endif
	fullscreen = vid_fullscreen->integer;
	vid_fullscreen->modified = false;
	gl_mode->modified = false;

	if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_mode->integer, fullscreen ) ) == rserr_ok )
	{
		gl_state.prev_mode = gl_mode->integer;
	}
	else
	{
		if ( err == rserr_invalid_fullscreen )
		{
			Cvar_Set( "vid_fullscreen", "0");
			vid_fullscreen->modified = false;
			Com_Printf ( "R_SetMode: fullscreen unavailable in this mode (%i).\n", gl_mode->integer );
			//This isnt nesecery, GLimp_SetMode go windowed mode automaticly on fs fail
			//if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_mode->integer, false ) ) == rserr_ok ) {
				return true;
			//}
		}
		else if ( err == rserr_invalid_mode )
		{
			Com_Printf ( "R_SetMode: invalid mode (%i).\n", gl_mode->integer );
			if(gl_mode->integer != gl_state.prev_mode) {
				Cvar_SetValue( "gl_mode", gl_state.prev_mode );
				gl_mode->modified = false;
				return R_SetMode();
			}
		}

		// try setting it back to something safe
		if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_state.prev_mode, false ) ) != rserr_ok )
		{
			Com_Printf ( "R_SetMode: could not revert to safe mode.\n" );
			return false;
		}
	}

	OnChange_Scale(gl_scale, "1.0");

	return true;
}

/*
===============
R_Init
===============
*/
char qglLastError[256];

static const char *GetQGLErrorString (unsigned int error)
{
	switch (error) {
	case GL_INVALID_ENUM:		return "INVALID ENUM";
	case GL_INVALID_OPERATION:	return "INVALID OPERATION";
	case GL_INVALID_VALUE:		return "INVALID VALUE";
	case GL_NO_ERROR:			return "NO ERROR";
	case GL_OUT_OF_MEMORY:		return "OUT OF MEMORY";
	case GL_STACK_OVERFLOW:		return "STACK OVERFLOW";
	case GL_STACK_UNDERFLOW:	return "STACK UNDERFLOW";
	}

	return "unknown";
}

static void GL_SetupRenderer( void )
{
	char renderer_buffer[MAX_STRING_CHARS];
	char vendor_buffer[MAX_STRING_CHARS];

	Q_strncpyz( renderer_buffer, gl_config.renderer_string, sizeof(renderer_buffer) );
	Q_strlwr( renderer_buffer );

	Q_strncpyz( vendor_buffer, gl_config.vendor_string, sizeof(vendor_buffer) );
	Q_strlwr( vendor_buffer );

	if ( strstr( renderer_buffer, "voodoo" ) )
	{
		if ( !strstr( renderer_buffer, "rush" ) )
			gl_config.renderer = GL_RENDERER_VOODOO;
		else
			gl_config.renderer = GL_RENDERER_VOODOO_RUSH;
	}
	else if ( strstr( vendor_buffer, "sgi" ) )
		gl_config.renderer = GL_RENDERER_SGI;
	else if ( strstr( renderer_buffer, "permedia" ) )
		gl_config.renderer = GL_RENDERER_PERMEDIA2;
	else if ( strstr( renderer_buffer, "glint" ) )
		gl_config.renderer = GL_RENDERER_GLINT_MX;
	else if ( strstr( renderer_buffer, "glzicd" ) )
		gl_config.renderer = GL_RENDERER_REALIZM;
	else if ( strstr( renderer_buffer, "gdi" ) )
		gl_config.renderer = GL_RENDERER_MCD;
	else if ( strstr( renderer_buffer, "pcx2" ) )
		gl_config.renderer = GL_RENDERER_PCX2;
	else if ( strstr( renderer_buffer, "verite" ) )
		gl_config.renderer = GL_RENDERER_RENDITION;
	else
		gl_config.renderer = GL_RENDERER_OTHER;


	// power vr can't have anything stay in the framebuffer, so
	// the screen needs to redraw the tiled background every frame
	if ( gl_config.renderer & GL_RENDERER_POWERVR ) 
		Cvar_Set( "scr_drawall", "1" );
	else
		Cvar_Set( "scr_drawall", "0" );

	// MCD has buffering issues
	if ( gl_config.renderer == GL_RENDERER_MCD )
		Cvar_Set( "gl_finish", "1" );

	gl_config.allow_cds = true;
	if ( (gl_config.renderer & GL_RENDERER_3DLABS) && gl_3dlabs_broken->integer)
		gl_config.allow_cds = false;

#ifdef _WIN32
	if ( gl_config.allow_cds )
		Com_Printf ( "...allowing CDS\n" );
	else
		Com_Printf ( "...disabling CDS\n" );
#endif
}

static void GL_SetupExtensions ( const char *extensions )
{
	gl_state.compiledVertexArray = false;
	if ( strstr( extensions, "GL_EXT_compiled_vertex_array" ) || 
		 strstr( extensions, "GL_SGI_compiled_vertex_array" ) )
	{
		if (gl_ext_compiled_vertex_array->integer) {
			qglLockArraysEXT = qglGetProcAddress( "glLockArraysEXT" );
			if(qglLockArraysEXT)
				qglUnlockArraysEXT = qglGetProcAddress( "glUnlockArraysEXT" );

			if(qglUnlockArraysEXT) {
				Com_Printf ( "...enabling GL_EXT_compiled_vertex_array\n" );
				gl_state.compiledVertexArray = true;
			} else {
				Com_Printf ("...GL_EXT_compiled_vertex_array not properly supported!\n");
				qglLockArraysEXT = NULL;
				qglUnlockArraysEXT = NULL;
			}
		} else {
			Com_Printf ( "...ignoring GL_EXT_compiled_vertex_array\n" );
		}
	} else {
		Com_Printf ( "...GL_EXT_compiled_vertex_array not found\n" );
	}

#ifdef _WIN32
	if ( strstr( extensions, "WGL_EXT_swap_control" ) ) {
		qwglSwapIntervalEXT = ( BOOL (WINAPI *)(int)) qglGetProcAddress( "wglSwapIntervalEXT" );
		if(qwglSwapIntervalEXT)
			Com_Printf ( "...enabling WGL_EXT_swap_control\n" );
		else
			Com_Printf ("...WGL_EXT_swap_control not properly supported!\n");
	} else {
		Com_Printf ( "...WGL_EXT_swap_control not found\n" );
	}
#endif

	if ( strstr( extensions, "GL_EXT_point_parameters" ) ) {
		if ( gl_ext_pointparameters->integer ) {
			qglPointParameterfEXT = ( void (APIENTRY *)( GLenum, GLfloat ) ) qglGetProcAddress( "glPointParameterfEXT" );
			
			if (qglPointParameterfEXT)
				qglPointParameterfvEXT = ( void (APIENTRY *)( GLenum, const GLfloat * ) ) qglGetProcAddress( "glPointParameterfvEXT" );
			
			if (qglPointParameterfvEXT) {
				Com_Printf ( "...using GL_EXT_point_parameters\n" );
			} else {
				Com_Printf ("...GL_EXT_point_parameters not properly supported!\n");
				qglPointParameterfEXT = NULL;
				qglPointParameterfvEXT = NULL;
			}
		} else {
			Com_Printf ( "...ignoring GL_EXT_point_parameters\n" );
		}
	} else {
		Com_Printf ( "...GL_EXT_point_parameters not found\n" );
	}

	gl_state.multiTexture = false;
	QGL_TEXTURE0 = QGL_TEXTURE1 = 0;
	if ( strstr( extensions, "GL_ARB_multitexture" ) )
	{
		if ( gl_ext_multitexture->integer ) {
			qglActiveTextureARB = qglGetProcAddress( "glActiveTextureARB" );
			if(qglActiveTextureARB)
				qglClientActiveTextureARB = qglGetProcAddress( "glClientActiveTextureARB" );

			if (qglClientActiveTextureARB) {
				Com_Printf ( "...using GL_ARB_multitexture\n" );
				QGL_TEXTURE0 = GL_TEXTURE0_ARB;
				QGL_TEXTURE1 = GL_TEXTURE1_ARB;
				gl_state.multiTexture = true;
				qglMTexCoord2fSGIS = qglGetProcAddress( "glMultiTexCoord2fARB" );
			} else {
				Com_Printf ("...GL_ARB_multitexture not properly supported!\n");
				qglActiveTextureARB = NULL;
				qglClientActiveTextureARB = NULL;
				qglMTexCoord2fSGIS = NULL;
			}
		} else {
			Com_Printf ( "...ignoring GL_ARB_multitexture\n" );
		}
	} else {
		Com_Printf ( "...GL_ARB_multitexture not found\n" );
	}

	if (!qglActiveTextureARB)
	{
		if ( strstr( extensions, "GL_SGIS_multitexture" ) )
		{
			if ( gl_ext_multitexture->integer ) {
				qglSelectTextureSGIS = qglGetProcAddress( "glSelectTextureSGIS" );
				if (qglSelectTextureSGIS) {
					Com_Printf ( "...using GL_SGIS_multitexture\n" );
					QGL_TEXTURE0 = GL_TEXTURE0_SGIS;
					QGL_TEXTURE1 = GL_TEXTURE1_SGIS;
					gl_state.multiTexture = true;
					qglMTexCoord2fSGIS = qglGetProcAddress( "glMTexCoord2fSGIS" );
				} else {
					Com_Printf ("...GL_SGIS_multitexture not properly supported!\n");
					qglMTexCoord2fSGIS = NULL;
					qglSelectTextureSGIS = NULL;
				}
			} else {
				Com_Printf ( "...ignoring GL_SGIS_multitexture\n" );
			}
		} else {
			Com_Printf ( "...GL_SGIS_multitexture not found\n" );
		}
	}

	if (strstr( extensions, "GL_NV_texture_rectangle" )) {
		Com_Printf ( "...using GL_NV_texture_rectangle\n");
		gl_state.tex_rectangle = true;
	} else if (strstr( extensions, "GL_EXT_texture_rectangle" )) {
		Com_Printf ( "...using GL_EXT_texture_rectangle\n");
		gl_state.tex_rectangle = true;
	} else {
		Com_Printf ( "...GL_NV_texture_rectangle not found\n");
		gl_state.tex_rectangle = false;
	}

	gl_state.sgis_mipmap = false;
	if ( strstr( extensions, "GL_SGIS_generate_mipmap" ) ) {
		if(gl_sgis_mipmap->integer) {
			Com_Printf ( "...using GL_SGIS_generate_mipmap\n");
			gl_state.sgis_mipmap = true;
		} else {
			Com_Printf ( "...ignoring GL_SGIS_generate_mipmap\n");
		}
	} else {
		Com_Printf ( "...GL_SGIS_generate_mipmap not found\n");
		gl_state.sgis_mipmap = false;
	}

	// ARB Texture Compression
	gl_state.texture_compression = false;
	if ( strstr( extensions, "GL_ARB_texture_compression" ) ) {
		if(gl_ext_texture_compression->integer) {
			Com_Printf ( "...using GL_ARB_texture_compression\n");
			gl_state.texture_compression = true;
			qglHint(GL_TEXTURE_COMPRESSION_HINT_ARB, GL_NICEST);
		} else {
			Com_Printf ( "...ignoring GL_ARB_texture_compression\n");
		}
	} else {
		Com_Printf ( "...GL_ARB_texture_compression not found\n");	
	}

	// Anisotropic
	gl_config.anisotropic = false;
	if ( strstr( extensions, "GL_EXT_texture_filter_anisotropic" ) ) {
		if(gl_ext_texture_filter_anisotropic->integer) {
			qglGetFloatv (GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_config.maxAnisotropic);
			if (gl_config.maxAnisotropic < 1) {
				Com_Printf ("...GL_EXT_texture_filter_anisotropic not properly supported!\n");
				gl_config.maxAnisotropic = 0;
			} else {
				Com_Printf ("...enabling GL_EXT_texture_filter_anisotropic\n");
				Com_Printf ("Maximum Anisotropy: %2.1f\n", gl_config.maxAnisotropic);
				gl_config.anisotropic = true;

				if(gl_ext_max_anisotropy->value > gl_config.maxAnisotropic)
					Cvar_SetValue("gl_ext_max_anisotropy", gl_config.maxAnisotropic);
				else if(gl_ext_max_anisotropy->value < 0)
					Cvar_Set("gl_ext_max_anisotropy", "0");
			}
		} else {
			Com_Printf ("...ignoring GL_EXT_texture_filter_anisotropic\n");
		}
	} else {
		Com_Printf ("...GL_EXT_texture_filter_anisotropic not found\n");
	}
}

void GL_Update3DfxGamma (void)
{
	char envbuffer[32];
	float g;

	if (!(gl_config.renderer & GL_RENDERER_VOODOO))
		return;

	g = 2.00f * ( 0.8f - ( vid_gamma->value - 0.5f ) ) + 1.0F;
	Com_sprintf( envbuffer, sizeof(envbuffer), "SSTV2_GAMMA=%g", g );
	putenv( envbuffer );
	Com_sprintf( envbuffer, sizeof(envbuffer), "SST_GAMMA=%g", g );
	putenv( envbuffer );
}

int R_Init( void *hinstance, void *hWnd )
{	
	unsigned int		err;

	//Com_Printf ("ref_gl version: "REF_VERSION"\n");

	GL_Register();

	gl_config.allow_cds = true;

	// initialize our QGL dynamic bindings
	if ( !QGL_Init( gl_driver->string ) )
	{
		QGL_Shutdown();
		if(strcmp(gl_driver->string, GL_DRIVERNAME)) {
			Com_Printf ("R_Init: Could not load \"%s\", trying \"%s\"\n", gl_driver->string, GL_DRIVERNAME );
			if( !QGL_Init( GL_DRIVERNAME ) ) {
				QGL_Shutdown();
				GL_Unregister();
			    Com_Error (ERR_FATAL, "R_Init: Could not load \"%s\" or \"%s\"", gl_driver->string, GL_DRIVERNAME);
				return -1;
			}
			Cvar_Set("gl_driver", GL_DRIVERNAME);
		} else {
			Com_Error (ERR_FATAL, "R_Init: Could not load \"%s\"", gl_driver->string );
			return -1;
		}
	}

	// initialize OS-specific parts of OpenGL
	if ( !GLimp_Init( hinstance, hWnd ) )
	{
		QGL_Shutdown();
		GL_Unregister();
		Com_Error (ERR_FATAL, "R_Init: GLimp_Init Failed!" );
		return -1;
	}

	// set our "safe" modes
	gl_state.prev_mode = 3;

	qglLastError[0] = 0;
	// create the window and set up the context
	if ( !R_SetMode() ) {
		QGL_Shutdown();
		GL_Unregister();
		Com_Error (ERR_FATAL, "R_Init: Could not set video mode%s%s", qglLastError[0] ? ": " : "!", qglLastError);
		return -1;
	}

	// get our various GL strings
	gl_config.vendor_string = (const char *)qglGetString (GL_VENDOR);
	Com_Printf ("GL_VENDOR: %s\n", gl_config.vendor_string );
	gl_config.renderer_string = (const char *)qglGetString (GL_RENDERER);
	Com_Printf ("GL_RENDERER: %s\n", gl_config.renderer_string );
	gl_config.version_string = (const char *)qglGetString (GL_VERSION);
	Com_Printf ("GL_VERSION: %s\n", gl_config.version_string );
	gl_config.extensions_string = (const char *)qglGetString (GL_EXTENSIONS);
	//Com_Printf ("GL_EXTENSIONS: %s\n", gl_config.extensions_string );


	GL_SetupRenderer();

	GL_SetupExtensions(gl_config.extensions_string);

	qglGetIntegerv (GL_MAX_TEXTURE_SIZE, &gl_config.maxTextureSize);
	if (gl_config.maxTextureSize < 256)
		gl_config.maxTextureSize = 256;

	if( !gl_state.multiTexture ) {
		gl_config.maxTextureUnits = 1;
	} else {
		qglGetIntegerv( GL_MAX_TEXTURE_UNITS, &gl_config.maxTextureUnits );
		if( gl_config.maxTextureUnits < 2 )
			Com_Error( ERR_DROP, "R_Init: glConfig.maxTextureUnits = %i, broken driver, contact your video card vendor", gl_config.maxTextureUnits );
		else if( gl_config.maxTextureUnits > MAX_TEXTURE_UNITS )
			gl_config.maxTextureUnits = MAX_TEXTURE_UNITS;
	}

	//Com_Printf ( "Max Texture Units: %i\n", gl_config.maxTextureUnits);

	Com_Printf ( "Maximum Texture Size: %ix%i\n", gl_config.maxTextureSize, gl_config.maxTextureSize);

	GL_Update3DfxGamma(); 	// update 3Dfx gamma irrespective of underlying DLL

	GL_SetDefaultState();

	/* draw our stereo patterns */
#if 0 // commented out until H3D pays us the money they owe us
	GL_DrawStereoPattern();
#endif

	if(gl_coloredlightmaps->modified) {
		if (gl_coloredlightmaps->value < 0.0f)
			Cvar_Set(gl_coloredlightmaps->name, "0");
		else if (gl_coloredlightmaps->value > 1.0f)
			Cvar_Set(gl_coloredlightmaps->name, "1");

		usingmodifiedlightmaps = (gl_coloredlightmaps->value < 1.0f) ? true : false;

		gl_coloredlightmaps->modified = false;
	}

	vid_scaled_width = (int)ceilf((float)vid.width / gl_scale->value);
	vid_scaled_height = (int)ceilf((float)vid.height / gl_scale->value);

	GL_InitImages ();

	Mod_Init ();
	Draw_InitLocal ();

	GL_InitDecals();

	R_InitMatrices();
	defaultTCInitialized = defaultInxInitialized = false;
	GL_InitArrays();

	err = qglGetError();
	if ( err != GL_NO_ERROR )
		Com_Printf ("glGetError() = '%s' (0x%x)\n", GetQGLErrorString(err), err);

	return 0;
}

/*
===============
R_Shutdown
===============
*/
void R_Shutdown (void)
{	
	R_ShutdownModels();

	GL_ShutdownImages();

	GL_ShutDownDecals();

	// shut down OS specific OpenGL stuff like contexts, etc.
	GLimp_Shutdown();

	// shutdown our QGL subsystem
	QGL_Shutdown();

	GL_Unregister();

	memset( &gl_config, 0, sizeof( gl_config ) );
	memset( &gl_state, 0, sizeof( gl_state ) );
}



/*
===============
R_BeginFrame
===============
*/
void R_BeginFrame( float camera_separation )
{

	gl_state.camera_separation = camera_separation;

	/*
	** change modes if necessary
	*/
	if ( gl_mode->modified || vid_fullscreen->modified )
	{	// FIXME: only restart if CDS is required
		VID_Restart_f ();
	}

	// update 3Dfx gamma -- it is expected that a user will do a vid_restart after tweaking this value
	if ( vid_gamma->modified ) {
		vid_gamma->modified = false;
		GL_Update3DfxGamma();
	}

	GLimp_BeginFrame( camera_separation );

	// go into 2D mode
	R_SetGL2D();

	if ( gl_drawbuffer->modified )
	{
		gl_drawbuffer->modified = false;

		if ( gl_state.camera_separation == 0 || !gl_state.stereo_enabled )
		{
			if ( Q_stricmp( gl_drawbuffer->string, "GL_FRONT" ) == 0 )
				qglDrawBuffer( GL_FRONT );
			else
				qglDrawBuffer( GL_BACK );
		}
	}

	// swapinterval stuff
	GL_UpdateSwapInterval();

	// clear screen if desired
	R_Clear();
}

void R_EndFrame (void)
{
	GLimp_EndFrame();
}
/*
=============
R_SetPalette
=============
*/
uint32 r_rawpalette[256];

void R_CinematicSetPalette ( const unsigned char *palette)
{
	int		i;

	byte *rp = ( byte * ) r_rawpalette;

	if ( palette )
	{
		for ( i = 0; i < 256; i++ )
		{
			rp[i*4+0] = palette[i*3+0];
			rp[i*4+1] = palette[i*3+1];
			rp[i*4+2] = palette[i*3+2];
			rp[i*4+3] = 0xff;
		}
	}
	else
	{
		for ( i = 0; i < 256; i++ )
		{
			rp[i*4+0] = d_8to24table[i] & 0xff;
			rp[i*4+1] = ( d_8to24table[i] >> 8 ) & 0xff;
			rp[i*4+2] = ( d_8to24table[i] >> 16 ) & 0xff;
			rp[i*4+3] = 0xff;
		}
	}

	//GL_SetTexturePalette( r_rawpalette );

	qglClearColor (0, 0, 0, 0);
	qglClear (GL_COLOR_BUFFER_BIT);
	qglClearColor( 1, 0.2f, 0, 1 );
}

/*
** R_DrawBeam
*/
void R_DrawBeam (void)
{
#define NUM_BEAM_SEGS 6

	int		i;
	byte	color[4];

	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t	start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	VectorCopy(currententity->oldorigin, oldorigin);

	VectorCopy(currententity->origin, origin);

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if ( VectorNormalize( normalized_direction ) == 0.0f )
		return;

	PerpendicularVector( perpvec, normalized_direction );
	VectorScale( perpvec, currententity->frame * 0.5f, perpvec );

	for ( i = 0; i < NUM_BEAM_SEGS; i++ )
	{
		RotatePointAroundVector( start_points[i], normalized_direction, perpvec, (360.0f/NUM_BEAM_SEGS)*i );
		VectorAdd( start_points[i], origin, start_points[i] );
		VectorAdd( start_points[i], direction, end_points[i] );
	}

	R_LoadModelIdentity();

	qglDisable( GL_TEXTURE_2D );
	qglEnable(GL_BLEND);
	qglDepthMask( GL_FALSE );

	*( uint32 * )color = d_8to24table[currententity->skinnum & 0xFF];
	color[3] = (byte)(currententity->alpha * 255);

	qglColor4ubv( color );

	qglBegin( GL_TRIANGLE_STRIP );
	for ( i = 0; i < NUM_BEAM_SEGS; i++ )
	{
		qglVertex3fv( start_points[i] );
		qglVertex3fv( end_points[i] );
		qglVertex3fv( start_points[(i+1)%NUM_BEAM_SEGS] );
		qglVertex3fv( end_points[(i+1)%NUM_BEAM_SEGS] );
	}
	qglEnd();

	qglEnable( GL_TEXTURE_2D );
	qglDisable(GL_BLEND);
	qglDepthMask( GL_TRUE );
	qglColor4fv(colorWhite);
}

