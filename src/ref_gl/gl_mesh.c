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
// gl_mesh.c: triangle model functions

#include "gl_local.h"

/*
=============================================================

  ALIAS MODELS

=============================================================
*/

static vec4_t	shadelight;
static vec3_t	newScale, oldScale, move;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
static const float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anormtab.h"
;

static const float	*shadedots = r_avertexnormal_dots[0];
extern vec3_t lightspot;

#define RF_SHELL_MASK	         ( RF_SHELL_HALF_DAM | RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE )

/*============================
Cellshading from q2max
Discoloda's cellshading outline routine
=============================*/
#define OUTLINEDROPOFF 700.0f //distance away for it to stop
static void GL_DrawOutLine (const aliasMesh_t *mesh) 
{
	float	scale;
 
	scale = (float)Distance( r_newrefdef.vieworg, currententity->origin)*(r_newrefdef.fov_y/90.0f);
	scale = (OUTLINEDROPOFF-scale) / OUTLINEDROPOFF;

	if( scale <= 0 || scale >= 1)
		return;

	if( gl_celshading_width->value > 10)
		Cvar_Set("gl_celshading_width", "10");
	else if( gl_celshading_width->value < 0.5f)
		Cvar_Set("gl_celshading_width", "0.5");

	qglPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
	qglCullFace (GL_BACK);
	qglEnable(GL_BLEND);
	colorBlack[3] = scale;
	qglColor4fv(colorBlack);
	qglLineWidth(gl_celshading_width->value * scale);

	if(gl_state.compiledVertexArray) {
		qglLockArraysEXT(0, mesh->numVerts);
		qglDrawElements (GL_TRIANGLES, mesh->numTris*3, GL_UNSIGNED_INT, mesh->indices);
		qglUnlockArraysEXT ();
	} else {
		qglDrawElements (GL_TRIANGLES, mesh->numTris*3, GL_UNSIGNED_INT, mesh->indices);
	}

	qglLineWidth(1);
	qglDisable(GL_BLEND);
	qglCullFace(GL_FRONT);
	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
}

/*
=============
GL_DrawAliasFrameLerp

interpolates between two frames and origins
FIXME: batch lerp all vertexes
=============
*/
static void GL_DrawAliasFrameLerp (const aliasMesh_t *mesh, int oldFrame, int newFrame)
{
	aliasVertex_t	*ov, *v;
	int				i;
	float			l;
	image_t			*skin;
	vec_t			*colorArray, *vertexArray;

	vertexArray = r_arrays.vertices;
	colorArray = r_arrays.colors;

	v = mesh->vertexes + newFrame*mesh->numVerts;

	if( newFrame == oldFrame ) {
		for ( i = 0; i < mesh->numVerts; i++, v++ ) {
			vertexArray[0] = move[0] + v->point[0]*newScale[0];
			vertexArray[1] = move[1] + v->point[1]*newScale[1];
			vertexArray[2] = move[2] + v->point[2]*newScale[2];
			vertexArray += 3;

			l = shadedots[v->normalIndex];
			colorArray[0] = l * shadelight[0];
			colorArray[1] = l * shadelight[1];
			colorArray[2] = l * shadelight[2];
			colorArray[3] = shadelight[3];
			colorArray += 4;
		}
	} else {
		ov = mesh->vertexes + oldFrame*mesh->numVerts;
		for ( i = 0; i < mesh->numVerts; i++, v++, ov++ ) {
			vertexArray[0] = move[0] + ov->point[0]*oldScale[0] + v->point[0]*newScale[0];
			vertexArray[1] = move[1] + ov->point[1]*oldScale[1] + v->point[1]*newScale[1];
			vertexArray[2] = move[2] + ov->point[2]*oldScale[2] + v->point[2]*newScale[2];
			vertexArray += 3;

			l = shadedots[v->normalIndex];
			colorArray[0] = l * shadelight[0];
			colorArray[1] = l * shadelight[1];
			colorArray[2] = l * shadelight[2];
			colorArray[3] = shadelight[3];
			colorArray += 4;
		}
	}

	// select skin
	if (currententity->skin) {
		skin = currententity->skin;	// custom player skin
	} else {
		if ((unsigned)currententity->skinnum >= mesh->numSkins) {
			skin = mesh->skins[0].image;
		} else {
			skin = mesh->skins[currententity->skinnum].image;
			if (!skin)
				skin = mesh->skins[0].image;
		}
	}
	if (!skin)
		skin = r_notexture;	// fallback...

	// draw it
	GL_Bind(skin->texnum);

	qglEnableClientState (GL_COLOR_ARRAY);
	qglTexCoordPointer (2, GL_FLOAT, 0, mesh->stcoords);

	if(gl_state.compiledVertexArray) {
		qglLockArraysEXT(0, mesh->numVerts);
		qglDrawElements (GL_TRIANGLES, mesh->numTris*3, GL_UNSIGNED_INT, mesh->indices);
		qglUnlockArraysEXT ();
	} else {
		qglDrawElements (GL_TRIANGLES, mesh->numTris*3, GL_UNSIGNED_INT, mesh->indices);
	}

	qglDisableClientState(GL_COLOR_ARRAY);
}

static void GL_DrawAliasShellFrameLerp (const aliasMesh_t *mesh, int oldFrame, int newFrame )
{
	aliasVertex_t	*ov, *v;
	int				i;
	const vec_t		*normal;
	vec_t			*vertexArray, *texCoordArray;
	float			scale;

	vertexArray = r_arrays.vertices;

	if(gl_shelleffect->integer)
	{
		float time = (float)sin(r_newrefdef.time*0.3f);
		texCoordArray = r_arrays.tcoords[0];
		for(i = 0; i < mesh->numVerts; i++) {
			texCoordArray[0] = mesh->stcoords[i][0] - time;
			texCoordArray[1] = mesh->stcoords[i][1] - time;
			texCoordArray += 2;
		}
		scale = (currententity->flags & RF_WEAPONMODEL) ?  0.5f : POWERSUIT_SCALE;
	}
	else
	{
		scale = (currententity->flags & RF_WEAPONMODEL) ?  0.66f : POWERSUIT_SCALE;
	}

	v = mesh->vertexes + newFrame*mesh->numVerts;

	if( newFrame == oldFrame ) {
		for ( i = 0; i < mesh->numVerts; i++, v++ ) {
			normal = bytedirs[v->normalIndex % NUMVERTEXNORMALS];
			vertexArray[0] = move[0] + v->point[0]*newScale[0] + normal[0] * scale;
			vertexArray[1] = move[1] + v->point[1]*newScale[1] + normal[1] * scale;
			vertexArray[2] = move[2] + v->point[2]*newScale[2] + normal[2] * scale;
			vertexArray += 3;
		}
	} else {
		ov = mesh->vertexes + oldFrame*mesh->numVerts;
		for ( i = 0; i < mesh->numVerts; i++, v++, ov++ ) {
			normal = bytedirs[v->normalIndex % NUMVERTEXNORMALS];
			vertexArray[0] = move[0] + ov->point[0]*oldScale[0] + v->point[0]*newScale[0] + normal[0] * scale;
			vertexArray[1] = move[1] + ov->point[1]*oldScale[1] + v->point[1]*newScale[1] + normal[1] * scale;
			vertexArray[2] = move[2] + ov->point[2]*oldScale[2] + v->point[2]*newScale[2] + normal[2] * scale;
			vertexArray += 3;
		}
	}

	if(gl_state.compiledVertexArray) {
		qglLockArraysEXT(0, mesh->numVerts);
		qglDrawElements (GL_TRIANGLES, mesh->numTris*3, GL_UNSIGNED_INT, mesh->indices);
		qglUnlockArraysEXT ();
	} else {
		qglDrawElements (GL_TRIANGLES, mesh->numTris*3, GL_UNSIGNED_INT, mesh->indices);
	}
}
/*
=============
GL_DrawAliasShadow
=============
*/
static void GL_DrawAliasShadow (const aliasMesh_t *mesh)
{
	float	height;
	int		i;
	vec_t	*vertexArray;

	height = -(currententity->origin[2] - lightspot[2]) + 1.0f;
	
	if (gl_state.stencil && gl_shadows->integer == 2) {
		height -= 0.9f;
		qglEnable( GL_STENCIL_TEST );
		qglStencilFunc( GL_EQUAL, 128, 0xFF );
		qglStencilOp( GL_KEEP, GL_KEEP, GL_INCR );
	}

	vertexArray = r_arrays.vertices;
	for(i=0; i < mesh->numVerts; i++) {
		vertexArray[2] = height;
		vertexArray += 3;
	}
	
	qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	if(gl_state.compiledVertexArray) {
		qglLockArraysEXT(0, mesh->numVerts);
		qglDrawElements (GL_TRIANGLES, mesh->numTris*3, GL_UNSIGNED_INT, mesh->indices);
		qglUnlockArraysEXT ();
	} else {
		qglDrawElements (GL_TRIANGLES, mesh->numTris*3, GL_UNSIGNED_INT, mesh->indices);
	}

	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);

	if (gl_state.stencil && gl_shadows->integer == 2)
		qglDisable(GL_STENCIL_TEST);
}

static float r_ModelShadowMatrix[16];

static void R_SetupAliasShadowMatrix(float s, float c)
{
	float	*wM = r_WorldViewMatrix;
	
	r_ModelShadowMatrix[0]  = wM[0] * c + wM[4] * s;
	r_ModelShadowMatrix[1]  = wM[1] * c + wM[5] * s;
	r_ModelShadowMatrix[2]  = wM[2] * c + wM[6] * s;

	r_ModelShadowMatrix[4]  = wM[0] * -s + wM[4] * c;
	r_ModelShadowMatrix[5]  = wM[1] * -s + wM[5] * c;
	r_ModelShadowMatrix[6]  = wM[2] * -s + wM[6] * c;

	r_ModelShadowMatrix[8]  = wM[8 ];
	r_ModelShadowMatrix[9]  = wM[9 ];
	r_ModelShadowMatrix[10] = wM[10];
	
	r_ModelShadowMatrix[12] = r_ModelViewMatrix[12];
	r_ModelShadowMatrix[13] = r_ModelViewMatrix[13];
	r_ModelShadowMatrix[14] = r_ModelViewMatrix[14];
	r_ModelShadowMatrix[15] = 1.0f;

	r_ModelShadowMatrix[3] = r_ModelShadowMatrix[7] = r_ModelShadowMatrix[11] = 0.0f;
}

/*
** R_CullAliasModel
*/
static qboolean R_CullAliasModel( const aliasFrame_t *pframe, const aliasFrame_t *poldframe )
{
	int			i, f, aggregatemask = ~0, mask;
	vec3_t		mins, maxs, tmp, bbox[8];


	// compute axially aligned mins and maxs
	if ( pframe == poldframe ) {
		VectorCopy(pframe->mins, mins);
		VectorCopy(pframe->maxs, maxs);
	} else {
		mins[0] = min(pframe->mins[0], poldframe->mins[0]);
		mins[1] = min(pframe->mins[1], poldframe->mins[1]);
		mins[2] = min(pframe->mins[2], poldframe->mins[2]);

		maxs[0] = max(pframe->maxs[0], poldframe->maxs[0]);
		maxs[1] = max(pframe->maxs[1], poldframe->maxs[1]);
		maxs[2] = max(pframe->maxs[2], poldframe->maxs[2]);
	}

	// rotate the bounding box
	for ( i = 0; i < 8; i++ )
	{
		tmp[0] = ( i & 1 ) ? mins[0] : maxs[0];
		tmp[1] = ( i & 2 ) ? mins[1] : maxs[1];
		tmp[2] = ( i & 4 ) ? mins[2] : maxs[2];

		bbox[i][0] = currententity->axis[0][0]*tmp[0] + currententity->axis[1][0]*tmp[1] + currententity->axis[2][0]*tmp[2] + currententity->origin[0]; 
		bbox[i][1] = currententity->axis[0][1]*tmp[0] + currententity->axis[1][1]*tmp[1] + currententity->axis[2][1]*tmp[2] + currententity->origin[1]; 
		bbox[i][2] = currententity->axis[0][2]*tmp[0] + currententity->axis[1][2]*tmp[1] + currententity->axis[2][2]*tmp[2] + currententity->origin[2];

		mask = 0;
		for ( f = 0; f < 4; f++ ) {
			if (DotProduct(frustum[f].normal, bbox[i]) < frustum[f].dist)
				mask |= ( 1 << f );
		}

		aggregatemask &= mask;
	}

	if ( aggregatemask )
		return true;

	return false;
}

static void GL_SetShadeLight(void)
{
	int i;
	// get lighting information
	//
	// PMM - rewrote, reordered to handle new shells & mixing
	// PMM - 3.20 code .. replaced with original way of doing it to keep mod authors happy
	if ( currententity->flags & RF_SHELL_MASK )
	{
		if (currententity->flags & RF_SHELL_HALF_DAM)
			VectorSet(shadelight, 0.56f, 0.59f, 0.45f);
		else
			VectorClear (shadelight);

		if ( currententity->flags & RF_SHELL_DOUBLE )
			shadelight[0] = 0.9f, shadelight[1] = 0.7f;

		if ( currententity->flags & RF_SHELL_RED )
			shadelight[0] = 1.0f;
		if ( currententity->flags & RF_SHELL_GREEN )
			shadelight[1] = 1.0f;
		if ( currententity->flags & RF_SHELL_BLUE )
			shadelight[2] = 1.0f;
	}
	else if ( currententity->flags & RF_FULLBRIGHT )
	{
		shadelight[0] = shadelight[1] = shadelight[2] = 1.0f;
	}
	else
	{
		R_LightPoint (currententity->origin, shadelight);

		// player lighting hack for communication back to server
		// big hack!
		if ( currententity->flags & RF_WEAPONMODEL )
		{
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

		if ( currententity->flags & RF_MINLIGHT )
		{
			for (i = 0; i < 3; i++) {
				if (shadelight[i] > 0.1f)
					break;
			}
			if (i == 3)
				shadelight[0] = shadelight[1] = shadelight[2] = 0.1f;
		}

		if ( currententity->flags & RF_GLOW )
		{	// bonus items will pulse with time
			float	scale, min;

			scale = 0.1f * (float)sin(r_newrefdef.time*7);
			for (i=0 ; i<3 ; i++) {
				min = shadelight[i] * 0.8f;
				shadelight[i] += scale;
				if (shadelight[i] < min)
					shadelight[i] = min;
			}
		}
	}

	if ( r_newrefdef.rdflags & RDF_IRGOGGLES && currententity->flags & RF_IR_VISIBLE)
	{
		shadelight[0] = 1.0f;
		shadelight[1] = shadelight[2] = 0.0f;
	}
	shadedots = r_avertexnormal_dots[((int)(currententity->angles[1] * (SHADEDOT_QUANT / 360.0f))) & (SHADEDOT_QUANT - 1)];
}

/*
=================
R_DrawAliasModel

=================
*/
void R_DrawAliasModel (model_t *model)
{
	aliasModel_t	*paliashdr;
	aliasMesh_t		*mesh, *lastMesh;
	aliasFrame_t	*frame, *oldframe;
	vec3_t			delta, tAxis[3];
	vec_t			backlerp, frontlerp;
	int				oldframeIdx, newframeIdx;
	float			sy, cy, sr;

	paliashdr = model->aliasModel;

	newframeIdx = currententity->frame;
	if ((unsigned)newframeIdx >= paliashdr->numFrames) {
		Com_DPrintf("R_DrawAliasModel %s: no such frame %d\n", model->name, newframeIdx);
		newframeIdx = 0;
	}
	oldframeIdx = currententity->oldframe;
	if ((unsigned)oldframeIdx >= paliashdr->numFrames) {
		Com_DPrintf("R_DrawAliasModel %s: no such oldframe %d\n", model->name, oldframeIdx);
		oldframeIdx = 0;
	}

	frame = paliashdr->frames + newframeIdx;
	oldframe = paliashdr->frames + oldframeIdx;

	if (currententity->flags & RF_WEAPONMODEL)
	{
		if ( r_lefthand->integer == 2 )
			return;
	}
	else if ( R_CullAliasModel( frame, oldframe ) )
		return;


	GL_SetShadeLight();

	if (currententity->flags & RF_DEPTHHACK) // hack the depth range to prevent view model from poking into walls
		qglDepthRange( gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin) );

	if (currententity->flags & RF_CULLHACK)
		qglFrontFace( GL_CW );

	//Lets use negative ROLL angle as in vq2
	Q_sincos(DEG2RAD(currententity->angles[YAW]), &sy, &cy);
	VectorCopy(currententity->axis[0], tAxis[0]);
	sr = (float)sin(DEG2RAD(-currententity->angles[ROLL])) * 2.0f;
	tAxis[2][0] = currententity->axis[2][0] + sr*sy;
	tAxis[2][1] = currententity->axis[2][1] - sr*cy;
	tAxis[2][2] = currententity->axis[2][2];
	sr = (float)sin(DEG2RAD(currententity->angles[PITCH])) * sr;
	tAxis[1][0] = currententity->axis[1][0] + sr*cy;
	tAxis[1][1] = currententity->axis[1][1] + sr*sy;
	tAxis[1][2] = -currententity->axis[1][2];

	R_RotateForEntity(currententity->origin, tAxis);

	GL_TexEnv( GL_MODULATE );

	if ( currententity->flags & RF_TRANSLUCENT ) {
		qglEnable(GL_BLEND);
		shadelight[3] = currententity->alpha;
	} else {
		shadelight[3] = 1.0f;
	}

	backlerp = (r_lerpmodels->integer) ? currententity->backlerp : 0;
	frontlerp = 1.0f - backlerp;

	// move should be the delta back to the previous frame * backlerp
	VectorSubtract (currententity->oldorigin, currententity->origin, delta);
	move[0] = DotProduct(currententity->axis[0],delta) + oldframe->translate[0];
	move[1] = DotProduct(currententity->axis[1],delta) + oldframe->translate[1];
	move[2] = DotProduct(currententity->axis[2],delta) + oldframe->translate[2];

	move[0] = backlerp*move[0] + frontlerp*frame->translate[0];
	move[1] = backlerp*move[1] + frontlerp*frame->translate[1];
	move[2] = backlerp*move[2] + frontlerp*frame->translate[2];

	if (newframeIdx == oldframeIdx)
		VectorCopy(frame->scale, newScale);
	else
		VectorScale(frame->scale, frontlerp, newScale);

	VectorScale(oldframe->scale, backlerp, oldScale);

	if ( currententity->flags & RF_SHELL_MASK ) {
		if (gl_shelleffect->integer) {
			qglTexCoordPointer (2, GL_FLOAT, 0, r_arrays.tcoords);
			GL_Bind(r_shelltexture->texnum);
			qglBlendFunc (GL_SRC_ALPHA, GL_ONE);
			shadelight[3] = 1.0f;
		} else {
			qglDisable( GL_TEXTURE_2D );
			qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		qglColor4fv(shadelight);

		for (mesh = paliashdr->meshes, lastMesh = mesh + paliashdr->numMeshes; mesh != lastMesh; mesh++)
		{
			GL_DrawAliasShellFrameLerp(mesh, oldframeIdx, newframeIdx);
			c_alias_polys += mesh->numTris;
		}
		if (gl_shelleffect->integer) {
			qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		} else {
			qglEnable( GL_TEXTURE_2D );
			qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}
	} else {
		for (mesh = paliashdr->meshes, lastMesh = mesh + paliashdr->numMeshes; mesh != lastMesh; mesh++)
		{
			GL_DrawAliasFrameLerp(mesh, oldframeIdx, newframeIdx);
			c_alias_polys += mesh->numTris;

			if (currententity->flags & RF_TRANSLUCENT)
				continue;

			if (gl_celshading->integer)
				GL_DrawOutLine(mesh);

			if (gl_shadows->integer && !(currententity->flags & RF_WEAPONMODEL)
					&& lightspot[2] < currententity->origin[2])
			{
				if(mesh == paliashdr->meshes) {
					R_SetupAliasShadowMatrix(sy, cy);
				}
				qglLoadMatrixf(r_ModelShadowMatrix);

				qglDisable(GL_TEXTURE_2D);
				qglEnable(GL_BLEND);
				colorBlack[3] = 0.5f;
				qglColor4fv(colorBlack);

				GL_DrawAliasShadow (mesh);

				qglEnable (GL_TEXTURE_2D);
				qglDisable(GL_BLEND);

				if (mesh + 1 != lastMesh) {
					qglLoadMatrixf(r_ModelViewMatrix);
				}
			}
		}
	}

	if ( currententity->flags & RF_TRANSLUCENT )
		qglDisable(GL_BLEND);

	GL_TexEnv( GL_REPLACE );

	if (currententity->flags & RF_CULLHACK)
		qglFrontFace( GL_CCW );

	if (currententity->flags & RF_DEPTHHACK)
		qglDepthRange( gldepthmin, gldepthmax );

	qglColor4fv(colorWhite);
 }


