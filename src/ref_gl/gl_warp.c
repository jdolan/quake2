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
// gl_warp.c -- sky and water polygons

#include "gl_local.h"

static char		skyname[MAX_QPATH];
static float	skyrotate;
static vec3_t	skyaxis;
static image_t	*sky_images[6];


// speed up sin calculations - Ed
float	r_turbsin[] =
{
	#include "warpsin.h"
};
//#define TURBSCALE (256.0 / M_TWOPI)
#define TURBSCALE	40.74366543152520595686852575586922507168212759145447757326835836930F
#define TURBSIN(f, s) r_turbsin[((int)(((f)*(s) + r_newrefdef.time) * TURBSCALE) & 255)]

/*
=============
EmitWaterPolys

Does a water warp
=============
*/
void EmitWaterPolys (const glpoly_t *p, qboolean flowing)
{
	const float	*v;
	vec_t		st[2], scroll;
	int			i, nv;

	if (flowing)
		scroll = -64 * ( (r_newrefdef.time*0.5f) - (int)(r_newrefdef.time*0.5f) );
	else
		scroll = 0;

	v = p->verts[0];
	nv = p->numverts;

	qglBegin (GL_TRIANGLE_FAN);
	if (!flowing && gl_waterwaves->value) {
		vec3_t		wv;   // Water waves

		for (i = 0; i < nv; i++, v += VERTEXSIZE) {
			st[0] = (v[3] + TURBSIN(v[4], 0.125f) + scroll) * ONEDIV64;
			st[1] = (v[4] + TURBSIN(v[3], 0.125f)) * ONEDIV64;
			qglTexCoord2fv(st);

			wv[0] = v[0];
			wv[1] = v[1];
			wv[2] = v[2] + gl_waterwaves->value *(float)sin(v[0]*0.025f+r_newrefdef.time)*(float)sin(v[2]*0.05f+r_newrefdef.time)
					+ gl_waterwaves->value *(float)sin(v[1]*0.025f+r_newrefdef.time*2)*(float)sin(v[2]*0.05f+r_newrefdef.time);
			qglVertex3fv(wv);
		}
	} else {
		for (i = 0; i < nv; i++, v += VERTEXSIZE) {
			st[0] = (v[3] + TURBSIN(v[4], 0.125f) + scroll) * ONEDIV64;
			st[1] = (v[4] + TURBSIN(v[3], 0.125f)) * ONEDIV64;
			qglTexCoord2fv (st);
			qglVertex3fv (v);
		}
	}
	qglEnd ();
}


//===================================================================


static const vec3_t skyclip[6] = {
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1} 
};

// 1 = s, 2 = t, 3 = 2048
static const int st_to_vec[6][3] =
{
	{3,-1,2},
	{-3,1,2},

	{1,3,2},
	{-1,-3,2},

	{-2,-1,3},		// 0 degrees yaw, look straight up
	{2,-1,-3}		// look straight down
};

// s = [0]/[2], t = [1]/[2]
static const int vec_to_st[6][3] =
{
	{-2,3,1},
	{2,3,-1},

	{1,3,2},
	{-1,3,-2},

	{-2,-1,3},
	{-2,1,-3}
};

static float	skymins[2][6], skymaxs[2][6];
static float	sky_min, sky_max;

static void DrawSkyPolygon (int nump, vec3_t vecs)
{
	int		i,j;
	vec3_t	v = {0,0,0}, av;
	float	s, t, dv;
	int		axis;
	float	*vp;

	// decide which face it maps to
	for (i=0, vp=vecs ; i<nump ; i++, vp+=3)
		VectorAdd (vp, v, v);

	VectorSet(av, (float)fabs(v[0]), (float)fabs(v[1]), (float)fabs(v[2]));
	if (av[0] > av[1] && av[0] > av[2])
	{
		if (v[0] < 0)
			axis = 1;
		else
			axis = 0;
	}
	else if (av[1] > av[2] && av[1] > av[0])
	{
		if (v[1] < 0)
			axis = 3;
		else
			axis = 2;
	}
	else
	{
		if (v[2] < 0)
			axis = 5;
		else
			axis = 4;
	}

	// project new texture coords
	for (i=0 ; i<nump ; i++, vecs+=3)
	{
		j = vec_to_st[axis][2];
		if (j > 0)
			dv = vecs[j - 1];
		else
			dv = -vecs[-j - 1];

		if (dv < 0.001f)
			continue;	// don't divide by zero

		j = vec_to_st[axis][0];
		if (j < 0)
			s = -vecs[-j -1] / dv;
		else
			s = vecs[j-1] / dv;

		j = vec_to_st[axis][1];
		if (j < 0)
			t = -vecs[-j -1] / dv;
		else
			t = vecs[j-1] / dv;

		if (s < skymins[0][axis])
			skymins[0][axis] = s;
		if (t < skymins[1][axis])
			skymins[1][axis] = t;
		if (s > skymaxs[0][axis])
			skymaxs[0][axis] = s;
		if (t > skymaxs[1][axis])
			skymaxs[1][axis] = t;
	}
}

#define	ON_EPSILON		0.1f		// point on plane side epsilon
#define	MAX_CLIP_VERTS	64

static void ClipSkyPolygon (int nump, vec3_t vecs, int stage)
{
	const float	*norm;
	float	*v, d;
	qboolean	front, back;
	float	dists[MAX_CLIP_VERTS];
	int		sides[MAX_CLIP_VERTS];
	vec3_t	newv[2][MAX_CLIP_VERTS];
	int		newc[2], i;

	if (nump > MAX_CLIP_VERTS-2)
		Com_Error (ERR_DROP, "ClipSkyPolygon: MAX_CLIP_VERTS");

	if (stage == 6)
	{	// fully clipped, so draw it
		DrawSkyPolygon (nump, vecs);
		return;
	}

	front = back = false;
	norm = skyclip[stage];
	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		d = DotProduct (v, norm);
		if (d > ON_EPSILON)
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if (d < -ON_EPSILON)
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = d;
	}

	if (!front || !back)
	{	// not clipped
		ClipSkyPolygon (nump, vecs, stage+1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy (vecs, (vecs+(i*3)) );
	newc[0] = newc[1] = 0;

	for (i=0, v = vecs ; i<nump ; i++, v+=3)
	{
		switch (sides[i])
		{
		case SIDE_FRONT:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);
		
		newv[0][newc[0]][0] = newv[1][newc[1]][0] = v[0] + d*(v[3] - v[0]);
		newv[0][newc[0]][1] = newv[1][newc[1]][1] = v[1] + d*(v[4] - v[1]);
		newv[0][newc[0]][2] = newv[1][newc[1]][2] = v[2] + d*(v[5] - v[2]);

		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon (newc[0], newv[0][0], stage+1);
	ClipSkyPolygon (newc[1], newv[1][0], stage+1);
}

/*
=================
R_AddSkySurface
=================
*/
void R_AddSkySurface (const msurface_t *fa)
{
	int			i;
	vec3_t		verts[MAX_CLIP_VERTS];
	const glpoly_t	*p;

	// calculate vertex values for sky box
	for (i = 0, p = fa->polys; i < p->numverts; i++)
		VectorSubtract (p->verts[i], r_origin, verts[i]);

	ClipSkyPolygon (p->numverts, verts[0], 0);
}


/*
==============
R_ClearSkyBox
==============
*/
void R_ClearSkyBox (void)
{
	int		i;

	for (i = 0; i < 6; i++) {
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}
}

extern float skybox_farz;

static void MakeSkyVec (float s, float t, int axis, vec_t *v, vec_t *st)
{
	vec3_t		b;
	int			j, k;

	b[2] = (skybox_farz / 2);
	b[0] = s * b[2];
	b[1] = t * b[2];

	for (j = 0; j < 3; j++)
	{
		k = st_to_vec[axis][j];
		if (k < 0)
			v[j] = -b[-k - 1];
		else
			v[j] = b[k - 1];
	}

	// avoid bilerp seam
	s = (s+1.0f)*0.5f;
	t = (t+1.0f)*0.5f;

	if (s < sky_min)
		st[0] = sky_min;
	else if (s > sky_max)
		st[0] = sky_max;
	else
		st[0] = s;

	if (t < sky_min)
		st[1] = 1.0f - sky_min;
	else if (t > sky_max)
		st[1] = 1.0f - sky_max;
	else
		st[1] = 1.0f - t;
}

/*
==============
R_DrawSkyBox
==============
*/
static void R_RotateMatrix (float dstM[16], const float srcM[16], vec_t angle, vec_t x, vec_t y, vec_t z )
{
	vec_t	c, s, mc, t1, t2, t3, t4, t5;

	dstM[3] = dstM[7] = dstM[11] = dstM[12] = dstM[13] = dstM[14] = 0.0f;
	dstM[15] = 1.0f;

	t1 = (float)sqrt(x*x + y*y + z*z);
	if (t1 == 0.0f) {
		dstM[0] = srcM[0], dstM[1] = srcM[1], dstM[2]  = srcM[2];
		dstM[4] = srcM[4], dstM[5] = srcM[5], dstM[6]  = srcM[6];
		dstM[8] = srcM[8], dstM[9] = srcM[9], dstM[10] = srcM[10];
		return;
	}

	t1 = 1.0f / t1;
	x *= t1;
	y *= t1;
	z *= t1;

	Q_sincos(DEG2RAD(angle), &s, &c);
	mc = 1.0f - c;

	t1 = y * x * mc;
	t2 = z * s;
	t3 = t1 + t2;
	t4 = t1 - t2;
	
	t1 = x * z * mc;
	t2 = y * s;
	t5 = t1 + t2;
	t2 = t1 - t2;
	t1 = (x * x * mc) + c;
	dstM[0]  = srcM[0] * t1 + srcM[4] * t3 + srcM[8 ] * t2;
	dstM[1]  = srcM[1] * t1 + srcM[5] * t3 + srcM[9 ] * t2;
	dstM[2]  = srcM[2] * t1 + srcM[6] * t3 + srcM[10] * t2;

	t1 = y * z * mc;
	t2 = x * s;
	t3 = t1 + t2;
	t2 = t1 - t2;
	t1 = (y * y * mc) + c;
	dstM[4]  = srcM[0] * t4 + srcM[4] * t1 + srcM[8 ] * t3;
	dstM[5]  = srcM[1] * t4 + srcM[5] * t1 + srcM[9 ] * t3;
	dstM[6]  = srcM[2] * t4 + srcM[6] * t1 + srcM[10] * t3;

	t1 = (z * z * mc) + c;
	dstM[8]  = srcM[0] * t5 + srcM[4] * t2 + srcM[8 ] * t1;
	dstM[9]  = srcM[1] * t5 + srcM[5] * t2 + srcM[9 ] * t1;
	dstM[10] = srcM[2] * t5 + srcM[6] * t2 + srcM[10] * t1;
}

static const int	skytexorder[6] = {0,2,1,3,4,5};
void R_DrawSkyBox (void)
{
	int		i;
	float skyM[16];
	vec_t	*v, *st;

	if (skyrotate)
	{	// check for no sky at all
		for (i = 0; i < 6; i++) {
			if (skymins[0][i] < skymaxs[0][i] && skymins[1][i] < skymaxs[1][i])
				break;
		}
		if (i == 6)
			return;		// nothing visible

		R_RotateMatrix(skyM, r_WorldViewMatrix, r_newrefdef.time * skyrotate, skyaxis[0], skyaxis[1], skyaxis[2]);
	} else {
		skyM[0]  = r_WorldViewMatrix[0];
		skyM[1]  = r_WorldViewMatrix[1];
		skyM[2]  = r_WorldViewMatrix[2];
		skyM[4]  = r_WorldViewMatrix[4];
		skyM[5]  = r_WorldViewMatrix[5];
		skyM[6]  = r_WorldViewMatrix[6];
		skyM[8]  = r_WorldViewMatrix[8];
		skyM[9]  = r_WorldViewMatrix[9];
		skyM[10] = r_WorldViewMatrix[10];
		skyM[3] = skyM[7] = skyM[11] = skyM[12] = skyM[13] = skyM[14] = 0.0f;
		skyM[15] = 1.0f;
	}

	qglLoadMatrixf( skyM );

	qglTexCoordPointer( 2, GL_FLOAT, 0, r_arrays.tcoords );
	for (i = 0; i < 6; i++)
	{
		if (skyrotate)
		{	// hack, forces full sky to draw when rotating
			skymins[0][i] =	skymins[1][i] = -1;
			skymaxs[0][i] =	skymaxs[1][i] = 1;
		}
		else if (skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i])
			continue;

		GL_Bind (sky_images[skytexorder[i]]->texnum);

		st = r_arrays.tcoords[0];
		v = r_arrays.vertices;
		MakeSkyVec (skymins[0][i], skymins[1][i], i, v, st);
		MakeSkyVec (skymins[0][i], skymaxs[1][i], i, v+=3, st+=2);
		MakeSkyVec (skymaxs[0][i], skymaxs[1][i], i, v+=3, st+=2);
		MakeSkyVec (skymaxs[0][i], skymins[1][i], i, v+=3, st+=2);
		if(gl_state.compiledVertexArray) {
			qglLockArraysEXT( 0, 4 );
			qglDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_INT, r_arrays.indices );
			qglUnlockArraysEXT ();
		} else {
			qglDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_INT, r_arrays.indices );
		}
	}

	qglLoadMatrixf(r_WorldViewMatrix);
}

/*
============
R_SetSky
============
*/
// 3dstudio environment map names
static const char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};

void R_SetSky (const char *name, float rotate, vec3_t axis)
{
	int		i;
	char	pathname[MAX_QPATH];

	Q_strncpyz (skyname, name, sizeof(skyname));
	skyrotate = rotate;
	VectorCopy (axis, skyaxis);

	// chop down rotating skies for less memory
	if (gl_skymip->integer || skyrotate)
		gl_picmip->integer++;

	for (i=0 ; i<6 ; i++)
	{
		Com_sprintf (pathname, sizeof(pathname), "env/%s%s.tga", skyname, suf[i]);

		sky_images[i] = GL_FindImage (pathname, it_sky);
		if (!sky_images[i])
			sky_images[i] = r_notexture;
	}
	if (gl_skymip->integer || skyrotate)
	{	// take less memory
		gl_picmip->integer--;
		sky_min = 0.00390625f;
		sky_max = 0.99609375f;
	}
	else	
	{
		sky_min = 0.001953125f;
		sky_max = 0.998046875f;
	}
}

//Water caustics
void EmitCausticPolys (const glpoly_t *p)
{
	int			i, nv;
	float		txm, tym;
	const float	*v;

	txm = (float)cos(r_newrefdef.time*0.3f) * 0.3f;
	tym = (float)sin(r_newrefdef.time*-0.3f) * 0.6f;

	GL_SelectTexture(1);
	qglDisable(GL_TEXTURE_2D);
	GL_SelectTexture(0);
	qglEnable(GL_BLEND);

    qglBlendFunc(GL_ZERO, GL_SRC_COLOR);

	qglColor4f (1, 1, 1, 0.275f);

	GL_Bind(r_caustictexture->texnum);
	
	v = p->verts[0];
	nv = p->numverts;

	qglBegin (GL_POLYGON);

	for (i = 0; i < nv; i++, v += VERTEXSIZE) {
		qglTexCoord2f (v[3]+txm, v[4]+tym);
		qglVertex3fv (v);
	}

	qglEnd();

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	qglColor4fv(colorWhite);
	qglDisable(GL_BLEND);
	GL_SelectTexture(1);
	qglEnable(GL_TEXTURE_2D);
	GL_SelectTexture(0);
}
