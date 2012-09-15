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
//Decals, from egl, orginally from qfusion?
#include "gl_local.h"

#define DF_SHADE		0x00000400	// 1024
#define DF_NOTIMESCALE	0x00000800	// 2048
#define INSTANT_DECAL	-10000.0

#define MAX_DECALS				256
#define MAX_DECAL_VERTS			64
#define MAX_DECAL_FRAGMENTS		64

#define DECAL_BHOLE	1
#define	DECAL_BLOOD	2

typedef struct cdecal_t
{
	struct cdecal_t	*prev, *next;
	float		time;

	int			numverts;
	vec3_t		verts[MAX_DECAL_VERTS];
	vec2_t		stcoords[MAX_DECAL_VERTS];
	mnode_t		*node;

	vec3_t		direction;

	vec4_t		color;
	vec3_t		org;

	int			type;
	int			flags;

} cdecal_t;

typedef struct
{
	int			firstvert;
	int			numverts;
	mnode_t		*node;
	msurface_t	*surf;
} fragment_t;

static int maxDecals;
static cdecal_t	*decals;
static cdecal_t	active_decals, *free_decals;

static int R_GetClippedFragments (vec3_t origin, float radius, mat3_t axis, int maxfverts, vec3_t *fverts, int maxfragments, fragment_t *fragments);
//void R_DrawDecal (int numverts, vec3_t *verts, vec2_t *stcoords, vec4_t color, int type, int flags);

static cvar_t	*gl_decals;
static cvar_t	*gl_decals_time;
static cvar_t	*gl_decals_max;

static qboolean loadedDecalImages;

image_t		*r_bholetexture;

void GL_InitDecalImages (void)
{
	r_bholetexture = GL_FindImage("pics/bullethole.png", it_sprite);
	if(!r_bholetexture)
		r_bholetexture = r_notexture;

	loadedDecalImages = true;
}

void GL_FreeUnusedDecalImages(void)
{
	if (!gl_decals->integer) {
		loadedDecalImages = false;
		return;
	}

	if(!loadedDecalImages) {
		GL_InitDecalImages();
		return;
	}

	r_bholetexture->registration_sequence = registration_sequence;
}

static void GL_FreeDecals(void)
{
	if (decals)
		Z_Free(decals);

	maxDecals = 0;
	decals = NULL;
}

static void GL_AllocDecals(void)
{
	maxDecals = gl_decals_max->integer;
	decals = Z_TagMalloc(maxDecals * sizeof(cdecal_t), TAG_RENDER_SCRSHOT);
	GL_ClearDecals();
}

static void OnChange_Decals(cvar_t *self, const char *oldValue)
{
	if (!self->integer)
		return;

	if (!decals) {
		GL_AllocDecals();
	}
	if(!loadedDecalImages)
		GL_InitDecalImages();

}

static void OnChange_DecalsMax(cvar_t *self, const char *oldValue)
{
	if (self->integer < 256)
		Cvar_Set(self->name, "256");
	else if (self->integer > 4096)
		Cvar_Set(self->name, "4096");

	if (maxDecals == self->integer)
		return;

	GL_FreeDecals();
	OnChange_Decals(gl_decals, gl_decals->resetString);
}

void GL_InitDecals (void)
{
	gl_decals = Cvar_Get( "gl_decals", "1", CVAR_ARCHIVE );
	gl_decals_time = Cvar_Get( "gl_decals_time", "30", CVAR_ARCHIVE );
	gl_decals_max = Cvar_Get( "gl_decals_max", "256", CVAR_ARCHIVE );

	gl_decals->OnChange = OnChange_Decals;
	gl_decals_max->OnChange = OnChange_DecalsMax;

	loadedDecalImages = false;

	OnChange_DecalsMax(gl_decals_max, gl_decals_max->resetString);
}

void GL_ShutDownDecals (void)
{
	
	if(!loadedDecalImages)
		return;

	GL_FreeDecals();

	gl_decals->OnChange = NULL;
	gl_decals_max->OnChange = NULL;
}

/*
=================
CG_ClearDecals
=================
*/
void GL_ClearDecals (void)
{
	int i;

	if (!decals)
		return;

	memset ( decals, 0, maxDecals * sizeof(cdecal_t) );

	// link decals
	free_decals = decals;
	active_decals.prev = &active_decals;
	active_decals.next = &active_decals;
	for ( i = 0; i < maxDecals - 1; i++ )
		decals[i].next = &decals[i+1];
}

/*
=================
CG_AllocDecal

Returns either a free decal or the oldest one
=================
*/
static cdecal_t *GL_AllocDecal (void)
{
	cdecal_t *dl;

	if ( free_decals ) {	// take a free decal if possible
		dl = free_decals;
		free_decals = dl->next;
	} else {				// grab the oldest one otherwise
		dl = active_decals.prev;
		dl->prev->next = dl->next;
		dl->next->prev = dl->prev;
	}

	// put the decal at the start of the list
	dl->prev = &active_decals;
	dl->next = active_decals.next;
	dl->next->prev = dl;
	dl->prev->next = dl;

	return dl;
}

/*
=================
CG_FreeDecal
=================
*/
static void GL_FreeDecal ( cdecal_t *dl )
{
	if (!dl->prev)
		return;

	// remove from linked active list
	dl->prev->next = dl->next;
	dl->next->prev = dl->prev;

	// insert into linked free list
	dl->next = free_decals;
	free_decals = dl;
}


/*
===============
makeDecal
===============
*/
void R_AddDecal	(vec3_t origin, vec3_t dir, float red, float green, float blue, float alpha,
				 float size, int type, int flags, float angle)
{
	int			i, j, numfragments;
	vec3_t		verts[MAX_DECAL_VERTS], shade;
	fragment_t	*fr, fragments[MAX_DECAL_FRAGMENTS];
	mat3_t		axis;
	cdecal_t	*d;

	if (!gl_decals->integer)
		return;

	// invalid decal
	if (size <= 0 || VectorCompare (dir, vec3_origin))
		return;

	// calculate orientation matrix
	VectorNormalize2 (dir, axis[0]);
	PerpendicularVector (axis[1], axis[0]);
	RotatePointAroundVector (axis[2], axis[0], axis[1], angle);
	CrossProduct (axis[0], axis[2], axis[1]);

	numfragments = R_GetClippedFragments (origin, size, axis, // clip it
		MAX_DECAL_VERTS, verts, MAX_DECAL_FRAGMENTS, fragments);

	// no valid fragments
	if (!numfragments)
		return;

	size = 0.5f / size;
	VectorScale (axis[1], size, axis[1]);
	VectorScale (axis[2], size, axis[2]);

	for (i=0, fr=fragments ; i<numfragments ; i++, fr++)
	{
		if (fr->numverts > MAX_DECAL_VERTS)
			fr->numverts = MAX_DECAL_VERTS;
		else if (fr->numverts < 1)
			continue;

		d = GL_AllocDecal ();

		d->time = r_newrefdef.time;

		d->numverts = fr->numverts;
		d->node = fr->node;

		VectorCopy(fr->surf->plane->normal, d->direction);
		// reverse direction
		if (!(fr->surf->flags & SURF_PLANEBACK))
			VectorNegate(d->direction, d->direction);

		Vector4Set(d->color, red, green, blue, alpha);
		VectorCopy (origin, d->org);

		if (flags&DF_SHADE) {
			R_LightPoint (origin, shade);

			for (j=0 ; j<3 ; j++)
				d->color[j] = (d->color[j] * shade[j] * 0.6f) + (d->color[j] * 0.4f);
		}

		d->type = type;
		d->flags = flags;

		for (j = 0; j < fr->numverts; j++) {
			vec3_t v;

			VectorCopy (verts[fr->firstvert+j], d->verts[j]);
			VectorSubtract (d->verts[j], origin, v);
			d->stcoords[j][0] = DotProduct (v, axis[1]) + 0.5f;
			d->stcoords[j][1] = DotProduct (v, axis[2]) + 0.5f;
		}
	}
}


/*
===============
CL_AddDecals
===============
*/
extern int r_visframecount;

void R_AddDecals (void)
{
	cdecal_t	*dl, *next,	*active;
	float		mindist, time;
	vec3_t		v;
	vec4_t		color;

	if (!gl_decals->integer)
		return;

	active = &active_decals;
	if (active->next == active)
		return;

	mindist = DotProduct(r_origin, viewAxis[0]) + 4.0f; 

	qglEnable(GL_POLYGON_OFFSET_FILL);
	qglPolygonOffset(-1, -2);

	qglDepthMask(GL_FALSE);
	qglEnable(GL_BLEND);
	GL_TexEnv(GL_MODULATE);

	GL_Bind(r_bholetexture->texnum);

	for (dl = active->next; dl != active; dl = next)
	{
		next = dl->next;

		if (dl->time + gl_decals_time->value <= r_newrefdef.time ) {
			GL_FreeDecal ( dl );
			continue;
		}
		
		if (dl->node == NULL || dl->node->visframe != r_visframecount)
			continue;

		// do not render if the decal is behind the view
		if ( DotProduct(dl->org, viewAxis[0]) < mindist)
			continue;

		// do not render if the view origin is behind the decal
		VectorSubtract(dl->org, r_origin, v);
		if (DotProduct(dl->direction, v) < 0)
			continue;

		Vector4Copy (dl->color, color);

		time = dl->time + gl_decals_time->value - r_newrefdef.time;

		if (time < 1.5f)
			color[3] *= time / 1.5f;

		//Draw it
		qglColor4fv (color);

		qglTexCoordPointer( 2, GL_FLOAT, 0, dl->stcoords);
		qglVertexPointer( 3, GL_FLOAT, 0, dl->verts );
		qglDrawArrays( GL_TRIANGLE_FAN, 0, dl->numverts );
	}

	GL_TexEnv(GL_REPLACE);
	qglDisable(GL_BLEND);
	qglColor4fv(colorWhite);
	qglDepthMask(GL_TRUE);
	qglDisable(GL_POLYGON_OFFSET_FILL);
	qglVertexPointer( 3, GL_FLOAT, 0, r_arrays.vertices );
}


#define	ON_EPSILON			0.1			// point on plane side epsilon
#define BACKFACE_EPSILON	0.01

static int numFragmentVerts;
static int maxFragmentVerts;
static vec3_t *fragmentVerts;

static int numClippedFragments;
static int maxClippedFragments;
static fragment_t *clippedFragments;

static int		fragmentFrame;
static cplane_t fragmentPlanes[6];

/*
=================
R_ClipPoly
=================
*/

static void R_ClipPoly (int nump, vec4_t vecs, int stage, fragment_t *fr)
{
	cplane_t *plane;
	qboolean	front, back;
	vec4_t	newv[MAX_DECAL_VERTS];
	float	*v, d, dists[MAX_DECAL_VERTS];
	int		newc, i, j, sides[MAX_DECAL_VERTS];

	if (nump > MAX_DECAL_VERTS-2)
		Com_Printf ("R_ClipPoly: MAX_DECAL_VERTS");

	if (stage == 6)
	{	// fully clipped
		if (nump > 2)
		{
			fr->numverts = nump;
			fr->firstvert = numFragmentVerts;

			if (numFragmentVerts+nump >= maxFragmentVerts)
				nump = maxFragmentVerts - numFragmentVerts;

			for (i=0, v=vecs ; i<nump ; i++, v+=4)
				VectorCopy (v, fragmentVerts[numFragmentVerts+i]);

			numFragmentVerts += nump;
		}

		return;
	}

	front = back = false;
	plane = &fragmentPlanes[stage];
	for (i=0, v=vecs ; i<nump ; i++ , v+= 4)
	{
		d = PlaneDiff (v, plane);
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
		{
			sides[i] = SIDE_ON;
		}

		dists[i] = d;
	}

	if (!front)
		return;

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy (vecs, (vecs+(i*4)) );
	newc = 0;

	for (i=0, v=vecs ; i<nump ; i++, v+=4)
	{
		switch (sides[i])
		{
		case SIDE_FRONT:
			VectorCopy (v, newv[newc]);
			newc++;
			break;
		case SIDE_BACK:
			break;
		case SIDE_ON:
			VectorCopy (v, newv[newc]);
			newc++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);
		for (j=0 ; j<3 ; j++)
			newv[newc][j] = v[j] + d * (v[j+4] - v[j]);
		newc++;
	}

	// continue
	R_ClipPoly (newc, newv[0], stage+1, fr);
}

/*
=================
R_PlanarSurfClipFragment
=================
*/

static void R_PlanarSurfClipFragment (mnode_t *node, msurface_t *surf, vec3_t normal)
{
	int			i;
	float		*v, *v2, *v3;
	fragment_t	*fr;
	vec4_t		verts[MAX_DECAL_VERTS];

	// bogus face
	if (surf->numedges < 3)
		return;

	// greater than 60 degrees
	if (surf->flags & SURF_PLANEBACK)
	{
		if (-DotProduct (normal, surf->plane->normal) < 0.5)
			return;
	}
	else
	{
		if (DotProduct (normal, surf->plane->normal) < 0.5)
			return;
	}

	v = surf->polys->verts[0];
	// copy vertex data and clip to each triangle
	for (i=0; i<surf->polys->numverts-2 ; i++)
	{
		fr = &clippedFragments[numClippedFragments];
		fr->numverts = 0;
		fr->node = node;
		fr->surf = surf;

		v2 = surf->polys->verts[0] + (i+1) * VERTEXSIZE;
		v3 = surf->polys->verts[0] + (i+2) * VERTEXSIZE;

		VectorCopy (v , verts[0]);
		VectorCopy (v2, verts[1]);
		VectorCopy (v3, verts[2]);
		R_ClipPoly (3, verts[0], 0, fr);

		if (fr->numverts)
		{
			numClippedFragments++;

			if ((numFragmentVerts >= maxFragmentVerts) || (numClippedFragments >= maxClippedFragments))
			{
				return;
			}
		}
	}
}

/*
=================
R_RecursiveFragmentNode
=================
*/

static void R_RecursiveFragmentNode (mnode_t *node, vec3_t origin, float radius, vec3_t normal)
{
	float dist;
	cplane_t *plane;

mark0:
	if ((numFragmentVerts >= maxFragmentVerts) || (numClippedFragments >= maxClippedFragments))
		return;			// already reached the limit somewhere else

	if (node->contents == CONTENTS_SOLID)
		return;

	if (node->contents != CONTENTS_NODE)
	{
		// leaf
		int c;
		mleaf_t *leaf;
		msurface_t *surf, **mark;

		leaf = (mleaf_t *)node;
		if (!(c = leaf->nummarksurfaces))
			return;

		mark = leaf->firstmarksurface;
		do
		{
			if ((numFragmentVerts >= maxFragmentVerts) || (numClippedFragments >= maxClippedFragments))
				return;

			surf = *mark++;
			if (surf->fragmentframe == fragmentFrame)
				continue;

			surf->fragmentframe = fragmentFrame;
			if (!(surf->texinfo->flags & (SURF_SKY|SURF_WARP|SURF_TRANS33|SURF_TRANS66|SURF_FLOWING|SURF_NODRAW)))
			{
				R_PlanarSurfClipFragment (node, surf, normal);
			}
		} while (--c);

		return;
	}

	plane = node->plane;
	dist = PlaneDiff (origin, plane);

	if (dist > radius)
	{
		node = node->children[0];
		goto mark0;
	}
	if (dist < -radius)
	{
		node = node->children[1];
		goto mark0;
	}

	R_RecursiveFragmentNode (node->children[0], origin, radius, normal);
	R_RecursiveFragmentNode (node->children[1], origin, radius, normal);
}

/*
=================
R_GetClippedFragments
=================
*/

static int R_GetClippedFragments (vec3_t origin, float radius, mat3_t axis, int maxfverts, vec3_t *fverts, int maxfragments, fragment_t *fragments)
{
	int i;
	float d;

	fragmentFrame++;

	// initialize fragments
	numFragmentVerts = 0;
	maxFragmentVerts = maxfverts;
	fragmentVerts = fverts;

	numClippedFragments = 0;
	maxClippedFragments = maxfragments;
	clippedFragments = fragments;

	// calculate clipping planes
	for (i=0 ; i<3; i++)
	{
		d = DotProduct (origin, axis[i]);

		VectorCopy (axis[i], fragmentPlanes[i*2].normal);
		fragmentPlanes[i*2].dist = d - radius;
		fragmentPlanes[i*2].type = PlaneTypeForNormal (fragmentPlanes[i*2].normal);

		VectorNegate (axis[i], fragmentPlanes[i*2+1].normal);
		fragmentPlanes[i*2+1].dist = -d - radius;
		fragmentPlanes[i*2+1].type = PlaneTypeForNormal (fragmentPlanes[i*2+1].normal);
	}

	R_RecursiveFragmentNode (r_worldmodel->nodes, origin, radius, axis[0]);

	return numClippedFragments;
}
