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
// r_light.c

#include "gl_local.h"

int	r_dlightframecount;

extern cvar_t *gl_dynamic;

extern qboolean usingmodifiedlightmaps;

#define	DLIGHT_CUTOFF	0

/*
=============================================================================

DYNAMIC LIGHTS BLEND RENDERING

=============================================================================
*/
static void R_RenderDlight (const dlight_t *light)
{
	int		i;
	float	a, b, rad;
	vec3_t	v;

	rad = light->intensity * 0.35f;

	VectorSubtract (light->origin, r_origin, v);

	qglBegin (GL_TRIANGLE_FAN);
	qglColor3f (light->color[0]*0.2f, light->color[1]*0.2f, light->color[2]*0.2f);

	v[0] = light->origin[0] - viewAxis[0][0]*rad;
	v[1] = light->origin[1] - viewAxis[0][1]*rad;
	v[2] = light->origin[2] - viewAxis[0][2]*rad;

	qglVertex3fv (v);
	qglColor3fv(colorBlack);
	for (i=16 ; i>=0 ; i--)
	{
		//a = (float)i/16.0f * M_TWOPI;
		a = (float)i*0.3926990816987241548f;
		b = (float)cos(a) * rad;
		a = (float)sin(a) * rad;
		v[0] = light->origin[0] - viewAxis[1][0] * b + viewAxis[2][0] * a;
		v[1] = light->origin[1] - viewAxis[1][1] * b + viewAxis[2][1] * a;
		v[2] = light->origin[2] - viewAxis[1][2] * b + viewAxis[2][2] * a;
		qglVertex3fv (v);
	}
	qglEnd ();
}

/*
=============
R_RenderDlights
=============
*/
void R_RenderDlights (void)
{
	int		i;
	dlight_t	*l;

	if (!gl_flashblend->integer)
		return;

	r_dlightframecount = r_framecount + 1;	// because the count hasn't advanced yet for this frame

	qglDepthMask (GL_FALSE);
	qglDisable (GL_TEXTURE_2D);
	//qglShadeModel (GL_SMOOTH);
	qglEnable(GL_BLEND);
	qglBlendFunc (GL_ONE, GL_ONE);

	l = r_newrefdef.dlights;
	for (i = 0; i < r_newrefdef.num_dlights; i++, l++)
		R_RenderDlight (l);

	qglColor3fv(colorWhite);
	qglDisable(GL_BLEND);
	qglEnable (GL_TEXTURE_2D);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDepthMask (GL_TRUE);
}


/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

/*
=============
R_MarkLights
=============
*/

void R_MarkLights (const dlight_t *light, int bit, const mnode_t *node)
{
	cplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int			i;

	if (node->contents != CONTENTS_NODE)
		return;

	splitplane = node->plane;

	if ( splitplane->type < 3 )
		dist = light->origin[splitplane->type] - splitplane->dist;
	else
		dist = DotProduct (light->origin, splitplane->normal) - splitplane->dist;


	if (dist > light->intensity - DLIGHT_CUTOFF)
	{
		R_MarkLights (light, bit, node->children[0]);
		return;
	}
	if (dist < -light->intensity + DLIGHT_CUTOFF)
	{
		R_MarkLights (light, bit, node->children[1]);
		return;
	}

	// mark the polygons
	surf = r_worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->dlightframe != r_dlightframecount)
		{
			surf->dlightbits = bit;
			surf->dlightframe = r_dlightframecount;
		}
		else
		{
			surf->dlightbits |= bit;
		}
	}

	R_MarkLights (light, bit, node->children[0]);
	R_MarkLights (light, bit, node->children[1]);
}

/*
=============
R_PushDlights
=============
*/
void R_PushDlights (void)
{
	int		i;
	dlight_t	*l;

	if (gl_flashblend->integer)
		return;

	r_dlightframecount = r_framecount + 1;	// because the count hasn't advanced yet for this frame
	l = r_newrefdef.dlights;
	for (i=0 ; i<r_newrefdef.num_dlights ; i++, l++)
		R_MarkLights ( l, 1<<i, r_worldmodel->nodes );
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

static vec3_t	pointcolor;
static cplane_t	*lightplane;		// used as shadow plane
vec3_t			lightspot;

static int RecursiveLightPoint (const mnode_t *node, const vec3_t start, const vec3_t end)
{
	float		front, back, frac;
	int			side;
	cplane_t	*plane;
	vec3_t		mid;
	msurface_t	*surf;
	int			ds, dt, i, r;
	mtexinfo_t	*tex;

	if (node->contents != CONTENTS_NODE)
		return -1;		// didn't hit anything
	
// calculate mid point

	plane = node->plane;
	if (plane->type < 3) {
		front = start[plane->type] - plane->dist;
		back = end[plane->type] - plane->dist;
	} else {
		front = DotProduct(start, plane->normal) - plane->dist;
		back = DotProduct(end, plane->normal) - plane->dist;
	}

	side = front < 0;
	
	if ( (back < 0) == side)
		return RecursiveLightPoint (node->children[side], start, end);
	
	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;
	
// go down front side	
	r = RecursiveLightPoint(node->children[side], start, mid);
	if (r >= 0)
		return r;		// hit something
		
	if ( (back < 0) == side )
		return -1;		// didn't hit anuthing
		
// check for impact on this node
	VectorCopy(mid, lightspot);
	lightplane = plane;

	surf = r_worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags&(SURF_DRAWTURB|SURF_DRAWSKY)) 
			continue;	// no lightmaps

		tex = surf->texinfo;
		
		ds = (int)(DotProduct(mid, tex->vecs[0]) + tex->vecs[0][3]) - surf->texturemins[0];
		if (ds < 0 || ds > surf->extents[0])
			continue;

		dt = (int)(DotProduct(mid, tex->vecs[1]) + tex->vecs[1][3]) - surf->texturemins[1];
		if (dt < 0 || dt > surf->extents[1])
			continue;

		if (surf->samples)
		{
			byte	*lightmap;
			int		maps;

			lightmap = surf->stain_samples + 3*((dt>>4) * ((surf->extents[0]>>4)+1) + (ds>>4));

			r = 3*((surf->extents[0]>>4)+1)*((surf->extents[1]>>4)+1);
			VectorClear(pointcolor);

			frac = gl_modulate->value * ONEDIV255;
			for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
			{
				pointcolor[0] += lightmap[0] * frac*r_newrefdef.lightstyles[surf->styles[maps]].rgb[0];
				pointcolor[1] += lightmap[1] * frac*r_newrefdef.lightstyles[surf->styles[maps]].rgb[1];
				pointcolor[2] += lightmap[2] * frac*r_newrefdef.lightstyles[surf->styles[maps]].rgb[2];
				lightmap += r;
			}

			return 1;
		}
		return 0;
	}

// go down back side
	return RecursiveLightPoint (node->children[!side], mid, end);
}

/*
===============
R_LightPoint
===============
*/
void R_LightPoint (const vec3_t p, vec3_t color)
{
	vec3_t		end;
	float		r, add;
	int			lnum;
	dlight_t	*dl;
	
	if (!r_worldmodel->lightdata) {
		color[0] = color[1] = color[2] = 1.0f;
		return;
	}
	
	VectorSet(end, p[0], p[1], p[2] - 2048);
	
	r = RecursiveLightPoint(r_worldmodel->nodes, p, end);
	if (r == -1)
		VectorClear (color);
	else
		VectorCopy (pointcolor, color);

	// add dynamic lights
	dl = r_newrefdef.dlights;
	for (lnum = 0; lnum < r_newrefdef.num_dlights; lnum++, dl++)
	{
		add = dl->intensity - (float)Distance(currententity->origin, dl->origin);
		if (add > 0.0f) {
			add *= ONEDIV256;
			VectorMA (color, add, dl->color, color);
		}
	}

	VectorScale(color, gl_modulate->value, color);

	if (usingmodifiedlightmaps)
	{
		float max = (color[0] + color[1] + color[2]) / 3;

		color[0] = max + (color[0] - max) * gl_coloredlightmaps->value;
		color[1] = max + (color[1] - max) * gl_coloredlightmaps->value;
		color[2] = max + (color[2] - max) * gl_coloredlightmaps->value;
	}

	if(gl_minlight_entities->value){
		
		// clamp light to a reasonable minimum
		float sum = color[0] + color[1] + color[2];

		if(sum < 0.1)  // too dark to bother scaling, just set it
			VectorSet(color, 0.4, 0.4, 0.4);
		else if(sum < 1.2)
			VectorScale(color, (1.2 / sum), color);
	}
}


//===================================================================

static float s_blocklights[34*34*3];
/*
===============
R_StainNode
===============
*/
static void R_StainNode (const stain_t *st, const mnode_t *node) { 
	msurface_t *surf;
	float		dist;
	int			c;
	
	if (node->contents != CONTENTS_NODE)
		return;

	if (node->plane->type < 3)
		dist = st->origin[node->plane->type] - node->plane->dist;
	else
		dist = DotProduct (st->origin, node->plane->normal) - node->plane->dist;

	if (dist > st->size) {
		R_StainNode (st, node->children[0]);
		return;
	}
	if (dist < -st->size) {
		R_StainNode (st, node->children[1]);
		return;
	}

	for (c = node->numsurfaces, surf = r_worldmodel->surfaces + node->firstsurface; c ; c--, surf++) {
		int			i;
		mtexinfo_t	*tex;
		int			sd, td;
		float		fdist, frad, fminlight;
		vec3_t		impact, local;
		int			s, t;
		int			smax, tmax;
		byte		*pfBL;
		float		fsacc, ftacc;
		long		col;

		smax = (surf->extents[0]>>4)+1;
		tmax = (surf->extents[1]>>4)+1;
		tex = surf->texinfo;

		if ( (tex->flags & (SURF_SKY|SURF_TRANS33|SURF_TRANS66|SURF_WARP) ) )
			continue;

		frad = st->size;
		fdist = DotProduct (st->origin, surf->plane->normal) - surf->plane->dist;
		if(surf->flags & SURF_PLANEBACK)
			fdist *= -1;

		frad -= (float)fabs(fdist);

		fminlight = DLIGHT_CUTOFF;
		if (frad < fminlight)
			continue;

		fminlight = frad - fminlight;

		impact[0] = st->origin[0] - surf->plane->normal[0]*fdist;
		impact[1] = st->origin[1] - surf->plane->normal[1]*fdist;
		impact[2] = st->origin[2] - surf->plane->normal[2]*fdist;

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3] - surf->texturemins[0];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3] - surf->texturemins[1];

		if (!surf->samples)
			return;

		pfBL = surf->samples;
		surf->cached_light[0]=0;

		for (t = 0, ftacc = 0 ; t<tmax ; t++, ftacc += 16)
		{
			td = local[1] - ftacc;
			if ( td < 0 )
				td = -td;
			for ( s=0, fsacc = 0 ; s<smax ; s++, fsacc += 16, pfBL += 3)
			{
				sd = (int)Q_ftol( local[0] - fsacc );

				if ( sd < 0 )
					sd = -sd;

				if (sd > td)
					fdist = sd + (td>>1);
				else
					fdist = td + (sd>>1);

				if ( fdist < fminlight ) {
					int test;
					for(i = 0; i < 3; i++) {
						test = pfBL[i] + (( frad - fdist ) * st->color[i]);
						if(test < 255 && test > 0) {
							col=pfBL[i]*st->color[i];
							clamp(col, 0, 255);
							pfBL[i] = (byte)col;
						}
					}
				}
			}
		}
	}

	R_StainNode (st, node->children[0]);
	R_StainNode (st, node->children[1]);
}

/*
=====================
R_AddStain

=====================
*/
static const vec3_t stainColors[4] = {
  {1.0f, 0.8f, 0.8f}, //Blood
  {0.89f, 0.89f, 0.89f}, //Normal shot
  {1.1f, 1.1f, 0.0f}, //Blaster
  {0.8f, 0.8f, 0.8f} //Explosion
};

void R_AddStain (const vec3_t org, int color, float size)
{
	stain_t	s;

	if (!gl_stainmaps->integer)
		return;

	VectorCopy (org, s.origin);
	VectorCopy (stainColors[color], s.color);
	s.size = size;

	R_StainNode(&s, r_worldmodel->nodes);
}

/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights (const msurface_t *surf)
{
	int			lnum;
	int			sd, td;
	float		fdist, frad, fminlight;
	vec3_t		impact, local, dlorigin;
	int			s, t;
	int			smax, tmax;
	mtexinfo_t	*tex;
	dlight_t	*dl;
	float		*pfBL;
	float		fsacc, ftacc;
	qboolean	rotated = false;
	vec3_t		temp;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	if (currententity->angles[0] || currententity->angles[1] || currententity->angles[2])
		rotated = true;

	for (lnum = 0; lnum < r_newrefdef.num_dlights; lnum++)
	{
		if ( !(surf->dlightbits & (1<<lnum) ) )
			continue;		// not lit by this light

		dl = &r_newrefdef.dlights[lnum];
		frad = dl->intensity;
		VectorSubtract (dl->origin, currententity->origin, dlorigin);

		if (rotated)
		{
			VectorCopy (dlorigin, temp);
			dlorigin[0] = DotProduct (temp, currententity->axis[0]);
			dlorigin[1] = DotProduct (temp, currententity->axis[1]);
			dlorigin[2] = DotProduct (temp, currententity->axis[2]);
		}

		if (surf->plane->type < 3)
			fdist = dlorigin[surf->plane->type] - surf->plane->dist;
		else
			fdist = DotProduct (dlorigin, surf->plane->normal) - surf->plane->dist;

		frad -= (float)fabs(fdist);
		// rad is now the highest intensity on the plane
		
		fminlight = DLIGHT_CUTOFF;
		if (frad < fminlight)
			continue;
		fminlight = frad - fminlight;

		if (surf->plane->type < 3)
		{
			VectorCopy (dlorigin, impact);
			impact[surf->plane->type] -= fdist;
		}
		else 
		{
			VectorMA (dlorigin, -fdist, surf->plane->normal, impact);
		}

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3] - surf->texturemins[0];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3] - surf->texturemins[1];

		pfBL = s_blocklights;
		for (t = 0, ftacc = 0 ; t<tmax ; t++, ftacc += 16)
		{
			td = local[1] - ftacc;
			if ( td < 0 )
				td = -td;

			for ( s=0, fsacc = 0 ; s<smax ; s++, fsacc += 16, pfBL += 3)
			{
				sd = Q_ftol( local[0] - fsacc );

				if ( sd < 0 )
					sd = -sd;

				if (sd > td)
					fdist = sd + (td>>1);
				else
					fdist = td + (sd>>1);

				if (fdist < fminlight)
				{
					pfBL[0] += ( frad - fdist ) * dl->color[0];
					pfBL[1] += ( frad - fdist ) * dl->color[1];
					pfBL[2] += ( frad - fdist ) * dl->color[2];
				}

			}
		}
	}
}


/*
** R_SetCacheState
*/
void R_SetCacheState( msurface_t *surf )
{
	int maps;

	for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
	{
		surf->cached_light[maps] = r_newrefdef.lightstyles[surf->styles[maps]].white;
	}
}

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the floating format in blocklights
===============
*/
void R_BuildLightMap (const msurface_t *surf, byte *dest, int stride)
{
	int			smax, tmax;
	int			r, g, b, a, max;
	int			i, j, size, nummaps;
	byte		*lightmap;
	float		scale[4], *bl;

	if ( surf->texinfo->flags & (SURF_SKY|SURF_TRANS33|SURF_TRANS66|SURF_WARP) )
		Com_Error (ERR_DROP, "R_BuildLightMap called for non-lit surface");

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax;
	if (size > (sizeof(s_blocklights)>>4) )
		Com_Error (ERR_DROP, "Bad s_blocklights size");

	// set to full bright if no light data
	if (!surf->samples)
	{
		for (i = 0; i < size*3; i++)
			s_blocklights[i] = 255;

		goto store;
	}

	// count the # of maps
	for ( nummaps = 0; nummaps < MAXLIGHTMAPS && surf->styles[nummaps] != 255;	nummaps++)
		;

	lightmap = surf->samples;

	// add all the lightmaps
	if ( nummaps == 1 )
	{
		bl = s_blocklights;

		scale[0] = gl_modulate->value*r_newrefdef.lightstyles[surf->styles[0]].rgb[0];
		scale[1] = gl_modulate->value*r_newrefdef.lightstyles[surf->styles[0]].rgb[1];
		scale[2] = gl_modulate->value*r_newrefdef.lightstyles[surf->styles[0]].rgb[2];

		if ( scale[0] != 1.0F || scale[1] != 1.0F || scale[2] != 1.0F )
		{
			for (i = 0; i < size; i++, bl += 3, lightmap += 3 ) {
				bl[0] = lightmap[0] * scale[0];
				bl[1] = lightmap[1] * scale[1];
				bl[2] = lightmap[2] * scale[2];
			}
		}
		else
		{
			for (i = 0; i < size; i++, bl += 3, lightmap += 3 ) {
				bl[0] = lightmap[0];
				bl[1] = lightmap[1];
				bl[2] = lightmap[2];
			}
		}
	}
	else
	{
		int maps;

		memset( s_blocklights, 0, sizeof( s_blocklights[0] ) * size * 3 );

		for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
		{
			bl = s_blocklights;

			scale[0] = gl_modulate->value*r_newrefdef.lightstyles[surf->styles[maps]].rgb[0];
			scale[1] = gl_modulate->value*r_newrefdef.lightstyles[surf->styles[maps]].rgb[1];
			scale[2] = gl_modulate->value*r_newrefdef.lightstyles[surf->styles[maps]].rgb[2];

			if ( scale[0] != 1.0F || scale[1] != 1.0F || scale[2] != 1.0F )
			{
				for (i = 0; i < size; i++, bl += 3, lightmap += 3 ) {
					bl[0] += lightmap[0] * scale[0];
					bl[1] += lightmap[1] * scale[1];
					bl[2] += lightmap[2] * scale[2];
				}
			}
			else
			{
				for (i = 0; i < size; i++, bl += 3, lightmap += 3 ) {
					bl[0] += lightmap[0];
					bl[1] += lightmap[1];
					bl[2] += lightmap[2];
				}
			}
		}
	}

// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights (surf);

// put into texture format
store:
	stride -= (smax<<2);
	bl = s_blocklights;

	//monolightmap = gl_monolightmap->string[0];

	for (i=0 ; i<tmax ; i++, dest += stride)
	{
		for (j=0 ; j<smax ; j++)
		{
			
			r = Q_ftol( bl[0] );
			g = Q_ftol( bl[1] );
			b = Q_ftol( bl[2] );

			// catch negative lights
			if (r < 0)
				r = 0;
			if (g < 0)
				g = 0;
			if (b < 0)
				b = 0;

			// determine the brightest of the three color components
			if (r > g)
				max = r;
			else
				max = g;
			if (b > max)
				max = b;

			/*
			** alpha is ONLY used for the mono lightmap case.  For this reason
			** we set it to the brightest of the color components so that 
			** things don't get too dim.
			*/
			a = max;

			/*
			** rescale all the color components if the intensity of the greatest
			** channel exceeds 1.0
			*/
			if (max > 255)
			{
				float t = 255.0F / max;

				r = r*t;
				g = g*t;
				b = b*t;
				a = a*t;
			}

			if (!usingmodifiedlightmaps)
			{
				dest[0] = r;
				dest[1] = g;
				dest[2] = b;
			}
			else
			{
				//max = r*0.289f + g*0.587f + b*0.114f;
				max = (r + g + b) / 3;

				dest[0] = max + (r - max) * gl_coloredlightmaps->value;
				dest[1] = max + (g - max) * gl_coloredlightmaps->value;
				dest[2] = max + (b - max) * gl_coloredlightmaps->value;
			}
			dest[3] = a;


			bl += 3;
			dest += 4;
		}
	}
}

