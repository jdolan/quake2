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
// models.c -- model loading and caching

#include "gl_local.h"

static model_t	*loadmodel;

static void Mod_LoadAliasMD2Model (model_t *mod, byte *rawdata, int length);
static void Mod_LoadAliasMD3Model (model_t *mod, byte *rawdata, int length);
static void Mod_LoadSpriteModel (model_t *mod, byte *rawdata, int length);
static void Mod_LoadBrushModel (model_t *mod, byte *rawdata, int length);

#define Mod_Malloc(size)	Z_TagMalloc( size, TAG_RENDER_MODEL )


static byte	mod_novis[MAX_MAP_LEAFS/8];

#define	MAX_MOD_KNOWN	512
static model_t	mod_known[MAX_MOD_KNOWN];
static int		mod_numknown;

#define MODELS_HASH_SIZE 32
static model_t *mod_hash[MODELS_HASH_SIZE];

// the inline * models from the current map are kept seperate
static model_t		mod_inline[MAX_MOD_KNOWN];

int		registration_sequence;

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (const vec3_t p, const bspModel_t *model)
{
	mnode_t	*node;
	float	d;
	cplane_t	*plane;
	
	if (!model || !model->nodes)
		Com_Error (ERR_DROP, "Mod_PointInLeaf: bad model");

	node = model->nodes;
	while (1)
	{
		if (node->contents != CONTENTS_NODE)
			return (mleaf_t *)node;
		plane = node->plane;
		if ( plane->type < 3 )
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct (p, plane->normal) - plane->dist;

		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}
	
	return NULL;	// never reached
}


/*
===================
Mod_DecompressVis
===================
*/
static byte *Mod_DecompressVis (const byte *in, const bspModel_t *model)
{
	static byte	decompressed[MAX_MAP_LEAFS/8];
	int		c;
	byte	*out;
	int		row;

	row = (model->vis->numclusters+7)>>3;	
	out = decompressed;

	if (!in)
	{	// no vis info, so make all visible
		while (row) {
			*out++ = 0xff;
			row--;
		}
		return decompressed;		
	}

	do
	{
		if (*in) {
			*out++ = *in++;
			continue;
		}
	
		c = in[1];
		in += 2;
		while (c) {
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);
	
	return decompressed;
}

/*
==============
Mod_ClusterPVS
==============
*/
byte *Mod_ClusterPVS (int cluster, const bspModel_t *model)
{
	if (cluster == -1 || !model->vis)
		return mod_novis;
	return Mod_DecompressVis ( (byte *)model->vis + model->vis->bitofs[cluster][DVIS_PVS], model);
}


//===============================================================================

/*
================
Mod_Modellist_f
================
*/
void Mod_Modellist_f (void)
{
	int		i;
	model_t	*mod;
	int		total = 0;

	Com_Printf ("Loaded models:\n");
	for (i=0, mod=mod_known ; i < mod_numknown ; i++, mod++)
	{
		if (!mod->name[0])
			continue;
		Com_Printf ("%8i : %s\n",mod->extradatasize, mod->name);
		total += mod->extradatasize;
	}
	Com_Printf ("Total resident: %i\n", total);
}

/*
===============
Mod_Init
===============
*/
void Mod_Init (void)
{
	memset (mod_novis, 0xff, sizeof(mod_novis));
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
static model_t *Mod_ForName (const char *name, qboolean crash)
{
	model_t		*mod;
	byte		*rawdata;
	int			i, length;
	unsigned int hash;
	
	if (!name || !name[0])
		Com_Error (ERR_DROP, "Mod_ForName: NULL name");
		
	//
	// inline models are grabbed only from worldmodel
	//
	if (name[0] == '*')
	{
		i = atoi(name+1);
		if (i < 1 || !r_worldmodel || i >= r_worldmodel->numSubmodels)
			Com_Error (ERR_DROP, "Mod_ForName: bad inline model number");
		return &mod_inline[i];
	}

	//
	// search the currently loaded models
	//
	hash = Com_HashKey(name, MODELS_HASH_SIZE);
	for (mod=mod_hash[hash]; mod; mod = mod->hashNext)
	{
		if (!strcmp (mod->name, name) )
			return mod;
	}
	
	//
	// find a free model slot spot
	//
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
		if (!mod->name[0])
			break;	// free spot

	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
			Com_Error (ERR_DROP, "Mod_ForName: mod_numknown == MAX_MOD_KNOWN");
		mod_numknown++;
	}
	Q_strncpyz (mod->name, name, sizeof(mod->name));

	//
	// load the file
	//
	length = FS_LoadFile(mod->name, (void **)&rawdata);
	if (!rawdata)
	{
		if (crash)
			Com_Error (ERR_DROP, "Mod_ForName: %s not found", mod->name);
		memset (mod->name, 0, sizeof(mod->name));
		return NULL;
	}

	if (length < 4) {
		if (crash)
			Com_Error (ERR_DROP, "Mod_ForName: %s has invalid length", mod->name);
		memset (mod->name, 0, sizeof(mod->name));
		return NULL;
	}

	loadmodel = mod;

	//
	// fill it in
	//


	// call the apropriate loader
	switch (LittleLong(*(int32 *)rawdata))
	{
	case IDALIASHEADER:
		Mod_LoadAliasMD2Model( mod, rawdata, length );
		break;
	case IDSPRITEHEADER:
		Mod_LoadSpriteModel( mod, rawdata, length );
		break;
	case IDBSPHEADER:
		Mod_LoadBrushModel( mod, rawdata, length );
		break;
	case IDMD3HEADER:
		Mod_LoadAliasMD3Model( mod, rawdata, length );
		break;
	default:
		Com_Error (ERR_DROP,"Mod_ForName: unknown fileid for %s", mod->name);
		break;
	}

	mod->hashNext = mod_hash[hash];
	mod_hash[hash] = mod;

	FS_FreeFile((void *)rawdata);

	return mod;
}

/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

static byte	*mod_base;


/*
=================
Mod_LoadLighting
=================
*/
static void Mod_LoadLighting (const lump_t *l)
{
	if (l->filelen < 1) {
		loadmodel->bspModel->lightdata = NULL;
		return;
	}

	loadmodel->bspModel->lightdata = Hunk_Alloc (l->filelen);
	memcpy (loadmodel->bspModel->lightdata, mod_base + l->fileofs, l->filelen);

	if (gl_stainmaps->integer) {
		loadmodel->bspModel->staindata = Hunk_Alloc (l->filelen);	// Stainmaps
		memcpy (loadmodel->bspModel->staindata, mod_base + l->fileofs, l->filelen);	// Stainmaps
	} else {
		loadmodel->bspModel->staindata = loadmodel->bspModel->lightdata;
	}

}


/*
=================
Mod_LoadVisibility
=================
*/
static void Mod_LoadVisibility (const lump_t *l)
{
#ifndef ENDIAN_LITTLE
	int		i;
#endif
	if (l->filelen < 1) {
		loadmodel->bspModel->vis = NULL;
		return;
	}
	loadmodel->bspModel->vis = Hunk_Alloc ( l->filelen);	
	memcpy (loadmodel->bspModel->vis, mod_base + l->fileofs, l->filelen);
#ifndef ENDIAN_LITTLE
	loadmodel->bspModel->vis->numclusters = LittleLong (loadmodel->bspModel->vis->numclusters);
	for (i=0 ; i<loadmodel->bspModel->vis->numclusters ; i++)
	{
		loadmodel->bspModel->vis->bitofs[i][0] = LittleLong (loadmodel->bspModel->vis->bitofs[i][0]);
		loadmodel->bspModel->vis->bitofs[i][1] = LittleLong (loadmodel->bspModel->vis->bitofs[i][1]);
	}
#endif
}


/*
=================
Mod_LoadVertexes
=================
*/
static void Mod_LoadVertexes (const lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	if (l->filelen % sizeof(*in)) {
		Com_Error (ERR_DROP, "Mod_LoadBmodel: funny lump size in %s", loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc( count*sizeof(*out));	

	loadmodel->bspModel->vertexes = out;
	loadmodel->bspModel->numvertexes = count;

	in = (dvertex_t *)(mod_base + l->fileofs);

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = LittleFloat (in->point[0]);
		out->position[1] = LittleFloat (in->point[1]);
		out->position[2] = LittleFloat (in->point[2]);
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
static void Mod_LoadSubmodels (const lump_t *l)
{
	dmodel_t		*in;
	bspSubmodel_t	*out;
	int				i, count;
	uint32			firstFace, numFaces, headnode;

	if (l->filelen % sizeof(*in)) {
		Com_Error (ERR_DROP, "Mod_LoadSubmodels: funny lump size in %s", loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->bspModel->submodels = out;
	loadmodel->bspModel->numSubmodels = count;

	in = (dmodel_t *)(mod_base + l->fileofs);
	for ( i=0 ; i<count ; i++, in++, out++)
	{
		// spread the mins / maxs by a pixel
		out->mins[0] = LittleFloat (in->mins[0]) - 1;
		out->mins[1] = LittleFloat (in->mins[1]) - 1;
		out->mins[2] = LittleFloat (in->mins[2]) - 1;
		out->maxs[0] = LittleFloat (in->maxs[0]) + 1;
		out->maxs[1] = LittleFloat (in->maxs[1]) + 1;
		out->maxs[2] = LittleFloat (in->maxs[2]) + 1;

		out->radius = RadiusFromBounds (out->mins, out->maxs);

		headnode = LittleLong( in->headnode );
		if( headnode >= loadmodel->bspModel->numnodes ) {
			/* FIXME: headnode may be garbage for some models */
			Com_DPrintf( "LoadSubmodels: bad headnode for model %d\n", i );
			out->headnode = NULL;
		} else {
			out->headnode = loadmodel->bspModel->nodes + headnode;
		}

		firstFace = LittleLong (in->firstface);
		numFaces = LittleLong (in->numfaces);
		if( firstFace + numFaces > loadmodel->bspModel->numsurfaces ) {
			Com_Error(ERR_DROP, "LoadSubmodels: bad faces\n" );
			return;
		}
		out->firstFace = loadmodel->bspModel->surfaces + firstFace;
		out->numFaces = numFaces;
	}
}

/*
=================
Mod_LoadEdges
=================
*/
static void Mod_LoadEdges (const lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	if (l->filelen % sizeof(*in)) {
		Com_Error (ERR_DROP, "Mod_LoadEdges: funny lump size in %s", loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc (count * sizeof(*out));	

	loadmodel->bspModel->edges = out;
	loadmodel->bspModel->numedges = count;

	in = (dedge_t *)(mod_base + l->fileofs);
	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->v[0] = LittleShort(in->v[0]);
		out->v[1] = LittleShort(in->v[1]);
	}
}

/*
=================
Mod_LoadTexinfo
=================
*/
static void Mod_LoadTexinfo (const lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out, *step;
	int 	i, count, next;
	char	name[MAX_QPATH];

	if (l->filelen % sizeof(*in)) {
		Com_Error (ERR_DROP, "Mod_LoadTexinfo: funny lump size in %s", loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->bspModel->texinfo = out;
	loadmodel->bspModel->numtexinfo = count;

	in = (texinfo_t *)(mod_base + l->fileofs);

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->vecs[0][0] = LittleFloat (in->vecs[0][0]);
		out->vecs[0][1] = LittleFloat (in->vecs[0][1]);
		out->vecs[0][2] = LittleFloat (in->vecs[0][2]);
		out->vecs[0][3] = LittleFloat (in->vecs[0][3]);

		out->vecs[1][0] = LittleFloat (in->vecs[1][0]);
		out->vecs[1][1] = LittleFloat (in->vecs[1][1]);
		out->vecs[1][2] = LittleFloat (in->vecs[1][2]);
		out->vecs[1][3] = LittleFloat (in->vecs[1][3]);

		out->flags = LittleLong (in->flags);
		next = LittleLong (in->nexttexinfo);
		if (next > 0)
			out->next = loadmodel->bspModel->texinfo + next;
		else
		    out->next = NULL;
		
		Com_sprintf (name, sizeof(name), "textures/%s.wal", in->texture);

		out->image = GL_FindImage (name, it_wall);
		if (!out->image) {
			Com_Printf ("Couldn't load %s\n", name);
			out->image = r_notexture;
		}
	}

	// count animation frames
	for (i=0 ; i<count ; i++)
	{
		out = &loadmodel->bspModel->texinfo[i];
		out->numframes = 1;
		for (step = out->next ; step && step != out ; step=step->next)
			out->numframes++;
	}
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
static void CalcSurfaceExtents (msurface_t *s)
{
	float	mins[2], maxs[2], val;
	int		i,j, e, bmins[2], bmaxs[2];
	mvertex_t	*v;
	mtexinfo_t	*tex;

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;
	
	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->bspModel->surfedges[s->firstedge+i];
		if (e >= 0)
			v = &loadmodel->bspModel->vertexes[loadmodel->bspModel->edges[e].v[0]];
		else
			v = &loadmodel->bspModel->vertexes[loadmodel->bspModel->edges[-e].v[1]];
		
		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->vecs[j][0] + 
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{	
		bmins[i] = (int)floor(mins[i] / 16);
		bmaxs[i] = (int)ceil(maxs[i] / 16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
	}
}


void GL_BuildPolygonFromSurface(msurface_t *fa, bspModel_t *bspModel);
void GL_CreateSurfaceLightmap (msurface_t *surf);
void GL_EndBuildingLightmaps (void);
void GL_BeginBuildingLightmaps (void);

/*
=================
Mod_LoadFaces
=================
*/
static void Mod_LoadFaces (const lump_t *l)
{
	dface_t		*in;
	msurface_t 	*out;
	int			i, count, surfnum, planenum, side, ti;

	if (l->filelen % sizeof(*in)) {
		Com_Error (ERR_DROP, "Mod_LoadFaces: funny lump size in %s", loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->bspModel->surfaces = out;
	loadmodel->bspModel->numsurfaces = count;

	in = (dface_t *)(mod_base + l->fileofs);
	//currentmodel = loadmodel;

	//GL_BeginBuildingLightmaps (loadmodel);
	GL_BeginBuildingLightmaps ();

	for ( surfnum=0 ; surfnum<count ; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);		
		out->flags = 0;
		out->polys = NULL;

		out->texturechain = NULL;
		out->lightmapchain = NULL;
		out->dlight_s = 0;
		out->dlight_t = 0;
		out->dlightframe = 0;
		out->dlightbits = 0;

		out->visframe = 0;

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;			

		out->plane = loadmodel->bspModel->planes + planenum;

		ti = LittleShort (in->texinfo);
		if ((unsigned)ti >= loadmodel->bspModel->numtexinfo) {
			Com_Error (ERR_DROP, "Mod_LoadFaces: bad texinfo number");
		}
		out->texinfo = loadmodel->bspModel->texinfo + ti;

		CalcSurfaceExtents (out);
				
	// lighting info

		for (i=0 ; i<MAXLIGHTMAPS ; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);
		if (i == -1)
		{
			out->samples = NULL;
			out->stain_samples = NULL;
		}
		else
		{
			out->samples = loadmodel->bspModel->lightdata + i;
			if (gl_stainmaps->integer) {
				out->stain_samples = loadmodel->bspModel->staindata + i;
			} else {
				out->stain_samples = out->samples;
			}
		}
		
		// set the drawing flags
		if (out->texinfo->flags & SURF_WARP)
			out->flags |= SURF_DRAWTURB;

		// create lightmaps and polygons
		if ( !(out->texinfo->flags & (SURF_SKY|SURF_TRANS33|SURF_TRANS66|SURF_WARP) ) )
			GL_CreateSurfaceLightmap (out);

		GL_BuildPolygonFromSurface(out, loadmodel->bspModel);

	}

	GL_EndBuildingLightmaps ();
}


/*
=================
Mod_SetParent
=================
*/
static void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;

	if (node->contents != CONTENTS_NODE)
		return;

	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
static void Mod_LoadNodes (const lump_t *l)
{
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	if (l->filelen % sizeof(*in)) {
		Com_Error (ERR_DROP, "Mod_LoadNodes: funny lump size in %s", loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->bspModel->nodes = out;
	loadmodel->bspModel->numnodes = count;

	in = (dnode_t *)(mod_base + l->fileofs);

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->minmaxs[0] = LittleShort (in->mins[0]);
		out->minmaxs[1] = LittleShort (in->mins[1]);
		out->minmaxs[2] = LittleShort (in->mins[2]);

		out->minmaxs[3] = LittleShort (in->maxs[0]);
		out->minmaxs[4] = LittleShort (in->maxs[1]);
		out->minmaxs[5] = LittleShort (in->maxs[2]);
	
		p = LittleLong(in->planenum);
		out->plane = loadmodel->bspModel->planes + p;

		out->firstsurface = LittleShort (in->firstface);
		out->numsurfaces = LittleShort (in->numfaces);
		out->contents = CONTENTS_NODE;	// differentiate from leafs

		out->parent = NULL;
		out->visframe = 0;

		for (j=0 ; j<2 ; j++)
		{
			p = LittleLong (in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->bspModel->nodes + p;
			else
				out->children[j] = (mnode_t *)(loadmodel->bspModel->leafs + (-1 - p));
		}
	}
	
	Mod_SetParent (loadmodel->bspModel->nodes, NULL);	// sets nodes and leafs
}

/*
=================
Mod_LoadLeafs
=================
*/
static void Mod_LoadLeafs (const lump_t *l)
{
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count;

	if (l->filelen % sizeof(*in)) {
		Com_Error (ERR_DROP, "Mod_LoadLeafs: funny lump size in %s", loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->bspModel->leafs = out;
	loadmodel->bspModel->numleafs = count;

	in = (dleaf_t *)(mod_base + l->fileofs);

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->minmaxs[0] = LittleShort (in->mins[0]);
		out->minmaxs[1] = LittleShort (in->mins[1]);
		out->minmaxs[2] = LittleShort (in->mins[2]);

		out->minmaxs[3] = LittleShort (in->maxs[0]);
		out->minmaxs[4] = LittleShort (in->maxs[1]);
		out->minmaxs[5] = LittleShort (in->maxs[2]);

		out->contents = LittleLong(in->contents);

		out->cluster = LittleShort(in->cluster);
		out->area = LittleShort(in->area);

		out->firstmarksurface = loadmodel->bspModel->marksurfaces +
			LittleShort(in->firstleafface);
		out->nummarksurfaces = LittleShort(in->numleaffaces);

		out->parent = NULL;
		out->visframe = 0;
		// gl underwater warp
		if (out->contents & (CONTENTS_WATER|CONTENTS_SLIME|CONTENTS_LAVA/*|CONTENTS_THINWATER*/) )
		{
			for (j=0; j<out->nummarksurfaces; j++)
			{
				if ((out->firstmarksurface[j]->texinfo->flags & (SURF_SKY|SURF_TRANS33|SURF_TRANS66|SURF_WARP)))
					continue;

				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
			}
		}
	}	
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
static void Mod_LoadMarksurfaces (const lump_t *l)
{	
	int		i, j, count;
	short		*in;
	msurface_t **out;
	
	if (l->filelen % sizeof(*in)) {
		Com_Error (ERR_DROP, "Mod_LoadMarksurfaces: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->bspModel->marksurfaces = out;
	loadmodel->bspModel->nummarksurfaces = count;

	in = (short *)(mod_base + l->fileofs);

	for ( i=0 ; i<count ; i++)
	{
		j = LittleShort(in[i]);
		if (j < 0 ||  j >= loadmodel->bspModel->numsurfaces)
			Com_Error (ERR_DROP, "Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->bspModel->surfaces + j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
static void Mod_LoadSurfedges (const lump_t *l)
{	
	int		i, count;
	int		*in, *out;
	
	if (l->filelen % sizeof(*in)) {
		Com_Error (ERR_DROP, "Mod_LoadSurfedges: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	if (count < 1 || count >= MAX_MAP_SURFEDGES)
		Com_Error (ERR_DROP, "Mod_LoadSurfedges: bad surfedges count in %s: %i", loadmodel->name, count);

	out = Hunk_Alloc ( count*sizeof(*out));	

	loadmodel->bspModel->surfedges = out;
	loadmodel->bspModel->numsurfedges = count;

	in = (int *)(mod_base + l->fileofs);

	for (i = 0; i < count; i++) {
		out[i] = LittleLong(in[i]);
	}
}


/*
=================
Mod_LoadPlanes
=================
*/
static void Mod_LoadPlanes (const lump_t *l)
{
	int			i, j, count, bits;
	cplane_t	*out;
	dplane_t 	*in;

	if (l->filelen % sizeof(*in)) {
		Com_Error (ERR_DROP, "MOD_LoadPlanes: funny lump size in %s", loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc ( count*sizeof(*out));	
	
	loadmodel->bspModel->planes = out;
	loadmodel->bspModel->numplanes = count;

	in = (dplane_t *)(mod_base + l->fileofs);

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1<<j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
		out->pad[0] = out->pad[1] = 0;
	}
}

/*
=================
Mod_LoadBrushModel
=================
*/
static void Mod_LoadBrushModel (model_t *mod, byte *rawdata, int length)
{
	int				i;
	dheader_t		header;
	lump_t			*lump;
	
	if (loadmodel != mod_known)
		Com_Error (ERR_DROP, "Mod_LoadBrushModel: Loaded a brush model after the world");

	if( length < sizeof( dheader_t ) ) {
		Com_Error(ERR_DROP, "Mod_LoadBrushModel: %s has length < header length\n", mod->name );
	}

	header = *(dheader_t *)rawdata;
	header.ident = LittleLong( header.ident );
	header.version = LittleLong( header.version );
	if (header.version != BSPVERSION) {
		Com_Error (ERR_DROP, "Mod_LoadBrushModel: %s has wrong version number (%i should be %i)", mod->name, header.version, BSPVERSION);
	}

	// byte swap and validate lumps
	for (lump = header.lumps, i = 0; i < HEADER_LUMPS; i++, lump++)
	{
		lump->fileofs = LittleLong( lump->fileofs );
		lump->filelen = LittleLong( lump->filelen );
		//for some reason there are unused lumps with invalid values
		if (i == LUMP_POP)
			continue;

		if (lump->fileofs < 0 || lump->filelen < 0 || lump->fileofs + lump->filelen > length) {
			Com_Error (ERR_DROP, "Mod_LoadBrushModel: lump %d offset %d of size %d is out of bounds\n%s is probably truncated or otherwise corrupted", i, lump->fileofs, lump->filelen, mod->name);
		}
	}

	mod_base = rawdata;

	mod->type = mod_brush;
	mod->extradata = Hunk_Begin(0x1000000);
	mod->bspModel = (bspModel_t *)Hunk_Alloc(sizeof(bspModel_t));

// load into heap
	
	Mod_LoadVertexes (&header.lumps[LUMP_VERTEXES]);
	Mod_LoadEdges (&header.lumps[LUMP_EDGES]);
	Mod_LoadSurfedges (&header.lumps[LUMP_SURFEDGES]);
	Mod_LoadLighting (&header.lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes (&header.lumps[LUMP_PLANES]);
	Mod_LoadTexinfo (&header.lumps[LUMP_TEXINFO]);
	Mod_LoadFaces (&header.lumps[LUMP_FACES]);
	Mod_LoadMarksurfaces (&header.lumps[LUMP_LEAFFACES]);
	Mod_LoadVisibility (&header.lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs (&header.lumps[LUMP_LEAFS]);
	Mod_LoadNodes (&header.lumps[LUMP_NODES]);
	Mod_LoadSubmodels (&header.lumps[LUMP_MODELS]);
	//mod->numframes = 2;		// regular and alternate animation
	
//
// set up the submodels
//
	for (i=0 ; i<mod->bspModel->numSubmodels ; i++)
	{
		model_t	*starmod;

		starmod = &mod_inline[i];
		//*starmod = *loadmodel;
		starmod->type = mod_brush;
		starmod->subModel = &mod->bspModel->submodels[i];
	}

	mod->extradatasize = Hunk_End();
}

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

/*
=================
Mod_LoadAliasMD2Model
=================
*/
//#define Mod_Malloc(size) Z_TagMalloc(size, TAG_RENDER_IMAGE)
//#define Mod_Malloc(size) Hunk_Alloc(size)

static void Mod_LoadAliasMD2Model (model_t *mod, byte *rawdata, int length)
{
	int					i, j, k;
	dmdl_t				header;
	dstvert_t			*pinst;
	dtriangle_t			*pintri;
	daliasframe_t		*pinframe;
	index_t				*poutindex;
	aliasModel_t		*poutmodel;
	aliasMesh_t			*poutmesh;
	vec2_t				*poutcoord;
	aliasFrame_t		*poutframe;
	aliasVertex_t		*poutvertex;
	aliasSkin_t			*poutskin;
	int					numVerts, numIndexes, numSkins, bufsize;
	uint16				indremap[MD2_MAX_TRIANGLES*3];
	uint16				vertIndices[MD2_MAX_TRIANGLES*3];
	uint16				tcIndices[MD2_MAX_TRIANGLES*3];
	index_t				finalIndices[MD2_MAX_TRIANGLES*3];
	byte				*buf;
	char				*pinskin;
	byte				*rawend;
	vec_t				scaleS, scaleT;


	if( length < sizeof( dmdl_t ) ) {
		Com_Error( ERR_DROP, "Mod_LoadAliasMD2Model: %s has length < header length\n", mod->name );
	}

	/* byte swap the header */
	header = *( dmdl_t * )rawdata;
	for( i = 0; i < sizeof( header )/4; i++ ) {
		(( uint32 * )&header)[i] = LittleLong( (( uint32 * )&header)[i] );
	}

	if( header.version != ALIAS_VERSION )
		Com_Error( ERR_DROP, "Mod_LoadAliasMD2Model: %s has wrong version number (%d should be %d)\n",
				 mod->name, header.version, ALIAS_VERSION );

	if (header.skinwidth <= 0 || header.skinheight <= 0)
		Com_Error( ERR_DROP, "Mod_LoadAliasMD2Model: %s has bad skin dimensions: %d x %d\n", mod->name, header.skinwidth, header.skinheight);

	if( header.num_frames < 1 )
		Com_Error( ERR_DROP, "Mod_LoadAliasMD2Model: %s bad number of frames: %d\n", mod->name, header.num_frames );
	else if( header.num_frames > MD2_MAX_FRAMES )
		Com_Error( ERR_DROP, "Mod_LoadAliasMD2Model: %s has too many frames: %d > %d\n", mod->name, MD2_MAX_FRAMES, header.num_frames );

	if( header.num_tris < 1 )
		Com_Error( ERR_DROP, "Mod_LoadAliasMD2Model: %s bad number of triangles: %d\n", mod->name, header.num_tris );
	else if( header.num_tris > MD2_MAX_TRIANGLES )
		Com_Error( ERR_DROP, "Mod_LoadAliasMD2Model: %s has too many triangles: %d > %d\n", mod->name , header.num_tris, MD2_MAX_TRIANGLES);

	if( header.num_xyz < 1 )
		Com_Error( ERR_DROP, "Mod_LoadAliasMD2Model: %s has bad number of vertices: %d\n", mod->name, header.num_xyz);
	else if( header.num_xyz > MD2_MAX_VERTS )
		Com_Error( ERR_DROP, "Mod_LoadAliasMD2Model: %s has too many vertices: %d > %d\n", mod->name, header.num_xyz, MD2_MAX_VERTS );

	if((unsigned)header.num_skins > MD2_MAX_SKINS )
		Com_Error( ERR_DROP, "Mod_LoadAliasMD2Model: %s has too many skins: %d > %d\n", mod->name, header.num_skins, MD2_MAX_SKINS );

	rawend = rawdata + length;
	if (header.ofs_tris < 1 || rawdata + header.ofs_tris + sizeof(dtriangle_t) * header.num_tris > rawend ) {
		Com_Error( ERR_DROP, "Mod_LoadAliasMD2Model: %s has bad triangles offset\n", mod->name );
	}

	if ((header.ofs_skins < 1 && header.ofs_skins != -1) || rawdata + header.ofs_skins + MAX_SKINNAME * header.num_skins > rawend) {
		Com_Error( ERR_DROP, "Mod_LoadAliasMD2Model: %s has bad skins offset\n", mod->name );
	}

	if (header.ofs_frames < 1 || rawdata + header.ofs_frames + header.num_frames * header.framesize > rawend) {
		Com_Error( ERR_DROP, "Mod_LoadAliasMD2Model: %s has bad frame offset\n", mod->name );
	}

//
// load triangle lists
//
	pintri = ( dtriangle_t * )( rawdata + header.ofs_tris );
	for( i = 0, k = 0; i < header.num_tris; i++, k += 3 ) {
		vertIndices[k+0] = ( index_t )LittleShort( pintri[i].index_xyz[0] );
		vertIndices[k+1] = ( index_t )LittleShort( pintri[i].index_xyz[1] );
		vertIndices[k+2] = ( index_t )LittleShort( pintri[i].index_xyz[2] );

		tcIndices[k+0] = ( index_t )LittleShort( pintri[i].index_st[0] );
		tcIndices[k+1] = ( index_t )LittleShort( pintri[i].index_st[1] );
		tcIndices[k+2] = ( index_t )LittleShort( pintri[i].index_st[2] );
	}

//
// build list of unique vertexes
//
	numIndexes = header.num_tris * 3;
	for( i = 0; i < numIndexes; i++ ) {
		indremap[i] = 0xFFFF;
	}

	numVerts = 0;
	pinst = ( dstvert_t * )( rawdata + header.ofs_st );
	for( i = 0; i < numIndexes; i++ ) {
		if( indremap[i] != 0xFFFF )
			continue;

		// remap duplicates
		for( j = i + 1; j < numIndexes; j++ ) {
			if(vertIndices[j] == vertIndices[i] &&
				(pinst[tcIndices[j]].s == pinst[tcIndices[i]].s
				&& pinst[tcIndices[j]].t == pinst[tcIndices[i]].t) ) {
				indremap[j] = i;
				finalIndices[j] = numVerts;
			}
		}
		// add unique vertex
		indremap[i] = i;
		finalIndices[i] = numVerts++;
	}

	Com_DPrintf( "%s: remapped %i verts to %i (%i tris)\n", mod->name, header.num_xyz, numVerts, header.num_tris );

	numSkins = header.num_skins;
	if (numSkins < 1) { //Some models dont have skins and uses the null skin
		numSkins = 1;
	}

	bufsize = ( sizeof(aliasModel_t) + sizeof(aliasMesh_t) +
		numIndexes * sizeof(index_t) + //indexes
		numVerts * sizeof(vec2_t) + //stcoords
		header.num_frames * (sizeof(aliasFrame_t) + numVerts * sizeof(aliasVertex_t)) + //frames
		numSkins * sizeof(aliasSkin_t)); //skins

	mod->type = mod_alias;

	buf = mod->extradata = Mod_Malloc(bufsize);
	mod->extradatasize = bufsize;

	poutmodel = mod->aliasModel = (aliasModel_t *)buf; buf += sizeof(aliasModel_t);
	poutmodel->numMeshes = 1;
	poutmodel->numFrames = header.num_frames;

	poutmesh = poutmodel->meshes = (aliasMesh_t *)buf; buf += sizeof(aliasMesh_t);
	poutmesh->numSkins = header.num_skins;
	poutmesh->numVerts = numVerts;
	poutmesh->numTris = header.num_tris;

	poutindex = poutmesh->indices = (index_t *)buf; buf += numIndexes * sizeof(index_t);

	for ( i = 0; i < numIndexes; i++)
		poutindex[i] = finalIndices[i];
//
// load base s and t vertices
//
	scaleS = 1.0f / header.skinwidth;
	scaleT = 1.0f / header.skinheight;
	poutcoord = poutmesh->stcoords = (vec2_t *)buf; buf += numVerts * sizeof(vec2_t);
	for( i = 0; i < numIndexes; i++ ) {
		if( indremap[i] == i ) {
			poutcoord[poutindex[i]][0] = ((float)LittleShort( pinst[tcIndices[i]].s ) + 0.5f) * scaleS;
			poutcoord[poutindex[i]][1] = ((float)LittleShort( pinst[tcIndices[i]].t ) + 0.5f) * scaleT;
		}
	}

//
// load the frames
//
	poutframe = poutmodel->frames = (aliasFrame_t *)buf;
	buf += poutmodel->numFrames * sizeof(aliasFrame_t);
	poutvertex = poutmesh->vertexes = (aliasVertex_t *)buf;
	buf += poutmodel->numFrames * poutmesh->numVerts * sizeof(aliasVertex_t);

	for( i = 0; i < poutmodel->numFrames; i++, poutframe++, poutvertex += numVerts ) {
		pinframe = ( daliasframe_t * )( rawdata + header.ofs_frames + i * header.framesize );

		poutframe->scale[0] = LittleFloat( pinframe->scale[0] );
		poutframe->scale[1] = LittleFloat( pinframe->scale[1] );
		poutframe->scale[2] = LittleFloat( pinframe->scale[2] );
		poutframe->translate[0] = LittleFloat( pinframe->translate[0] );
		poutframe->translate[1] = LittleFloat( pinframe->translate[1] );
		poutframe->translate[2] = LittleFloat( pinframe->translate[2] );

		for( j = 0; j < numIndexes; j++ ) {		// verts are all 8 bit, so no swapping needed
			if( indremap[j] == j ) {
				poutvertex[poutindex[j]].point[0] = (short)pinframe->verts[vertIndices[j]].v[0];
				poutvertex[poutindex[j]].point[1] = (short)pinframe->verts[vertIndices[j]].v[1];
				poutvertex[poutindex[j]].point[2] = (short)pinframe->verts[vertIndices[j]].v[2];
				poutvertex[poutindex[j]].normalIndex = pinframe->verts[vertIndices[j]].lightnormalindex;
			}
		}

		VectorCopy( poutframe->translate, poutframe->mins );
		VectorMA( poutframe->translate, 255, poutframe->scale, poutframe->maxs );
		poutframe->radius = RadiusFromBounds( poutframe->mins, poutframe->maxs );
	}

	// register all skins
	poutskin = poutmesh->skins = (aliasSkin_t *)buf;
	memset(poutskin, 0, numSkins * sizeof(aliasSkin_t));
	if (header.ofs_skins != -1 && header.num_skins) {
		pinskin = ( char * )rawdata + header.ofs_skins;
		for( i = 0; i < header.num_skins; i++, poutskin++, pinskin += MD2_MAX_SKINNAME) {
			poutskin->image = GL_FindImage(pinskin, it_skin );
		}
	}
}


/*
=================
Mod_StripLODSuffix
=================
*/
/*void Mod_StripLODSuffix( char *name )
{
	int len, lodnum;

	len = strlen( name );
	if( len <= 2 )
		return;

	lodnum = atoi( &name[len - 1] );
	if( lodnum < MD3_ALIAS_MAX_LODS ) {
		if( name[len-2] == '_' )
			name[len-2] = 0;
	}
}*/

/*
=================
Mod_LoadAliasMD3Model
=================
*/

/*static vec_t Quat_Normalize( quat_t q )
{
	vec_t length;

	length = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
	if( length != 0.0f ) {
		vec_t ilength = 1.0f / (float)sqrt( length );
		q[0] *= ilength;
		q[1] *= ilength;
		q[2] *= ilength;
		q[3] *= ilength;
	}

	return length;
}

static void Matrix_Quat( vec3_t m[3], quat_t q )
{
	vec_t tr, s;

	tr = m[0][0] + m[1][1] + m[2][2];
	if( tr > 0.00001f ) {
		s = (float)sqrt( tr + 1.0f );
		q[3] = s * 0.5f; s = 0.5f / s;
		q[0] = (m[2][1] - m[1][2]) * s;
		q[1] = (m[0][2] - m[2][0]) * s;
		q[2] = (m[1][0] - m[0][1]) * s;
	} else {
		int i = 0, j, k;

		if (m[1][1] > m[0][0]) i = 1;
		if (m[2][2] > m[i][i]) i = 2;
		j = (i + 1) % 3;
		k = (i + 2) % 3;

		s = (float)sqrt( m[i][i] - (m[j][j] + m[k][k]) + 1.0f );

		q[i] = s * 0.5f;
		if(s != 0.0f)
			s = 0.5f / s;
		q[j] = (m[j][i] + m[i][j]) * s;
		q[k] = (m[k][i] + m[i][k]) * s;
		q[3] = (m[k][j] - m[j][k]) * s;
	}

	Quat_Normalize( q );
}*/

static void Mod_LoadAliasMD3Model ( model_t *mod, byte *rawdata, int length )
{
	int					version, i, j, l;
	int					bufsize;
	dmd3header_t		*header;
	dmd3frame_t			*pinframe;
//	dmd3tag_t			*pintag;
	dmd3mesh_t			*pinmesh;
	dmd3skin_t			*pinskin;
	dmd3coord_t			*pincoord;
	dmd3vertex_t		*pinvert;
	index_t				*pinindex, *poutindex;
	aliasVertex_t		*poutvert;
	vec2_t				*poutcoord;
	aliasSkin_t			*poutskin;
	aliasMesh_t			*poutmesh;
//	aliasTag_t			*pouttag;
	aliasFrame_t		*poutframe;
	aliasModel_t		*poutmodel;
	byte				*buf, *rawend;
	int					numFrames, numTags, numMeshes;
	int					numTris[MD3_MAX_MESHES], numSkins[MD3_MAX_MESHES], numVerts[MD3_MAX_MESHES];
	float				s[2], c[2];
	vec3_t				normal;

	if( length < sizeof( dmd3header_t ) ) {
		Com_Error( ERR_DROP, "Mod_LoadAliasMD3Model: %s has length < header length\n", mod->name );
	}

	header = ( dmd3header_t * )rawdata;
	version = LittleLong( header->version );

	if ( version != MD3_ALIAS_VERSION ) {
		Com_Error (ERR_DROP, "Mod_LoadAliasMD3Model: %s has wrong version number (%d should be %d)\n",
				 mod->name, version, MD3_ALIAS_VERSION);
	}

	// byte swap the header fields and sanity check
	numFrames = LittleLong ( header->num_frames );
	if ( numFrames < 1 )
		Com_Error (ERR_DROP, "Mod_LoadAliasMD3Model: %s has bad number of frames: %d\n", mod->name, numFrames );
	else if ( numFrames > MD3_MAX_FRAMES )
		Com_Error (ERR_DROP, "Mod_LoadAliasMD3Model: %s has too many frames: %d > %d\n", mod->name, numFrames, MD3_MAX_FRAMES );

	numTags = LittleLong ( header->num_tags );
	/*if ( numTags < 0 ) 
		Com_Error (ERR_DROP, "Mod_LoadAliasMD3Model: %s has bad number of tags: %d\n", mod->name, numTags );
	else if ( numTags > MD3_MAX_TAGS )
		Com_Error (ERR_DROP, "Mod_LoadAliasMD3Model: %s has too many tags: %d > %d\n", mod->name, numTags, MD3_MAX_TAGS );*/

	numMeshes = LittleLong ( header->num_meshes );
	if ( numMeshes < 1 )
		Com_Error (ERR_DROP, "Mod_LoadAliasMD3Model: %s has bad number of meshes: %d\n", mod->name, numMeshes );
	else if ( numMeshes > MD3_MAX_MESHES )
		Com_Error (ERR_DROP, "Mod_LoadAliasMD3Model: %s has too many meshes: %d > %d\n", mod->name, numMeshes, MD3_MAX_MESHES );

	rawend = rawdata + length;
	if (header->ofs_frames < 1 || rawdata + header->ofs_frames + header->num_frames * sizeof(dmd3frame_t) > rawend) {
		Com_Error( ERR_DROP, "Mod_LoadAliasMD3Model: %s has bad frame offset\n", mod->name );
	}

	bufsize = sizeof(aliasModel_t) + numFrames * (sizeof( aliasFrame_t ) /*+ sizeof( aliasTag_t ) * numTags*/) + 
		numMeshes * sizeof( aliasMesh_t );

	pinmesh = ( dmd3mesh_t * )( rawdata + LittleLong( header->ofs_meshes ) );
	for( i = 0; i < numMeshes; i++ ) {
		if( ( byte * )( pinmesh + 1 ) > rawend ) {
			Com_Error( ERR_DROP, "Mod_LoadAliasMD3Model: %s has bad offset for mesh %d\n", mod->name, i );
		}

		if( strncmp( (const char *)pinmesh->id, "IDP3", 4) )
			Com_Error( ERR_DROP, "Mod_LoadAliasMD3Model: %s has wrong id for mesh %s (%s should be %s)\n",
					 mod->name, pinmesh->name, LittleLong( pinmesh->id ), IDMD3HEADER );

		numSkins[i] = LittleLong( pinmesh->num_skins );
		if( numSkins[i] < 1 )
			Com_Error( ERR_DROP, "Mod_LoadAliasMD3Model: %s has no skins for mesh %d\n", mod->name, i );
		else if( numSkins[i] > MD3_MAX_SHADERS )
			Com_Error( ERR_DROP, "Mod_LoadAliasMD3Model: %s has too many skins (%d > %d) for mesh %d\n", i, mod->name, numSkins[i], MD3_MAX_SHADERS, i);

		numTris[i] = LittleLong( pinmesh->num_tris );
		if( numTris[i] < 1 )
			Com_Error( ERR_DROP, "Mod_LoadAliasMD3Model: %s has no elements for mesh %d\n", mod->name, i );
		else if( numTris[i] > MD3_MAX_TRIANGLES )
			Com_Error( ERR_DROP, "Mod_LoadAliasMD3Model: %s has too many triangles (%d > %d) for mesh %d\n", mod->name, numTris[i], MD3_MAX_TRIANGLES, i);
		
		numVerts[i] = LittleLong( pinmesh->num_verts );
		if( numVerts[i] < 1 )
			Com_Error( ERR_DROP, "Mod_LoadAliasMD3Model: %s has no vertices for mesh %d\n", mod->name, i );
		else if( numVerts[i] > MD3_MAX_VERTS )
			Com_Error( ERR_DROP, "Mod_LoadAliasMD3Model: %s has too many vertices (%d > %d) for mesh %d\n", mod->name, numVerts[i], MD3_MAX_VERTS, i);

		bufsize += sizeof(aliasSkin_t *) * numSkins[i] + numTris[i] * sizeof(index_t) * 3 +
			numVerts[i] * (sizeof(vec2_t) + sizeof(aliasVertex_t) * numFrames);

		pinmesh = ( dmd3mesh_t * )( ( byte * )pinmesh + LittleLong( pinmesh->meshsize ) );
	}

	mod->type = mod_alias;

	//bufsize = (bufsize+31)&~31;
	buf = mod->extradata = Mod_Malloc(bufsize);
	mod->extradatasize = bufsize;

	poutmodel = mod->aliasModel = (aliasModel_t *)buf; buf += sizeof(aliasModel_t);

	poutmodel->numFrames = numFrames;
	//poutmodel->numTags = numTags;
	poutmodel->numMeshes = numMeshes;

//
// load the frames
//
	pinframe = ( dmd3frame_t * )( rawdata + LittleLong( header->ofs_frames ) );
	poutframe = poutmodel->frames = ( aliasFrame_t * )buf; buf += sizeof( aliasFrame_t ) * poutmodel->numFrames;
	for( i = 0; i < poutmodel->numFrames; i++, pinframe++, poutframe++ ) {
		poutframe->scale[0] = poutframe->scale[1] = poutframe->scale[2] = MD3_XYZ_SCALE;
		poutframe->translate[0] = LittleFloat( pinframe->translate[0] );
		poutframe->translate[1] = LittleFloat( pinframe->translate[1] );
		poutframe->translate[2] = LittleFloat( pinframe->translate[2] );
		// never trust the modeler utility and recalculate bbox and radius
		ClearBounds( poutframe->mins, poutframe->maxs );
	}
	
//
// load the tags
//
/*	pintag = ( dmd3tag_t * )( ( byte * )header + LittleLong( header->ofs_tags ) );
	pouttag = poutmodel->tags = ( aliasTag_t * )buf; buf += sizeof( aliasTag_t ) * poutmodel->numFrames * poutmodel->numTags;
	for( i = 0; i < poutmodel->numFrames; i++ ) {
		for( l = 0; l < poutmodel->numTags; l++, pintag++, pouttag++ ) {
			for ( j = 0; j < 3; j++ ) {
				vec3_t axis[3];

				axis[0][j] = LittleFloat( pintag->axis[0][j] );
				axis[1][j] = LittleFloat( pintag->axis[1][j] );
				axis[2][j] = LittleFloat( pintag->axis[2][j] );
				Matrix_Quat( axis, pouttag->quat );
				//Quat_Normalize( pouttag->quat );
				pouttag->origin[j] = LittleFloat( pintag->origin[j] );
			}

			Q_strncpyz( pouttag->name, pintag->name, MD3_MAX_PATH );
		}
	}*/

//
// load the meshes
//
	pinmesh = ( dmd3mesh_t * )( rawdata + LittleLong( header->ofs_meshes ) );
	poutmesh = poutmodel->meshes = ( aliasMesh_t * )buf; buf += poutmodel->numMeshes * sizeof( aliasMesh_t );
	for( i = 0; i < poutmodel->numMeshes; i++, poutmesh++ )
	{
		//Q_strncpyz( poutmesh->name, pinmesh->name, MD3_MAX_PATH );
		//Mod_StripLODSuffix( poutmesh->name );

		poutmesh->numTris = numTris[i];
		poutmesh->numSkins = numSkins[i];
		poutmesh->numVerts = numVerts[i];
	//
	// load the skins
	//
		pinskin = ( dmd3skin_t * )( ( byte * )pinmesh + LittleLong( pinmesh->ofs_skins ) );
		if( ( byte * )( pinskin + poutmesh->numSkins ) > rawend ) {
			Com_Error( ERR_DROP, "Mod_LoadAliasMD3Model: %s has bad skin offset for mesh %d\n", mod->name, i );
		}
		poutskin = poutmesh->skins = ( aliasSkin_t * )buf; buf += sizeof(aliasSkin_t *) * poutmesh->numSkins;
		for( j = 0; j < poutmesh->numSkins; j++, pinskin++, poutskin++ )
			poutskin->image = GL_FindImage(pinskin->name, it_skin );

	//
	// load the indexes
	//
		pinindex = ( index_t * )( ( byte * )pinmesh + LittleLong( pinmesh->ofs_indexes ) );
		if( ( byte * )( pinindex + poutmesh->numTris * 3 ) > rawend ) {
			Com_Error( ERR_DROP, "Mod_LoadAliasMD3Model: %s has bad indices offset for mesh %d\n", mod->name, i );
		}
		poutindex = poutmesh->indices = ( index_t * )buf; buf += poutmesh->numTris * sizeof(index_t) * 3;
		for( j = 0; j < poutmesh->numTris; j++, pinindex += 3, poutindex += 3 ) {
			poutindex[0] = (index_t)LittleLong( pinindex[0] );
			poutindex[1] = (index_t)LittleLong( pinindex[1] );
			poutindex[2] = (index_t)LittleLong( pinindex[2] );
		}

	//
	// load the texture coordinates
	//
		pincoord = ( dmd3coord_t * )( ( byte * )pinmesh + LittleLong( pinmesh->ofs_tcs ) );
		if( ( byte * )( pincoord + poutmesh->numVerts ) > rawend ) {
			Com_Error( ERR_DROP, "Mod_LoadAliasMD3Model: %s has bad tcoords offset for mesh %d\n", mod->name, i );
		}
		poutcoord = poutmesh->stcoords = ( vec2_t * )buf; buf += poutmesh->numVerts * sizeof(vec2_t);
		for( j = 0; j < poutmesh->numVerts; j++, pincoord++ ) {
			poutcoord[j][0] = LittleFloat( pincoord->st[0] );
			poutcoord[j][1] = LittleFloat( pincoord->st[1] );
		}

	//
	// load the vertexes and normals
	//
		pinvert = ( dmd3vertex_t * )( ( byte * )pinmesh + LittleLong( pinmesh->ofs_verts ) );
		if( ( byte * )( pinvert + poutmesh->numVerts * poutmodel->numFrames ) > rawend ) {
			Com_Error( ERR_DROP, "Mod_LoadAliasMD3Model: %s has bad vertices offset for mesh %d\n", mod->name, i );
		}
		poutvert = poutmesh->vertexes = ( aliasVertex_t * )buf; buf += poutmesh->numVerts * sizeof(aliasVertex_t) * poutmodel->numFrames;
		for( l = 0, poutframe = poutmodel->frames; l < poutmodel->numFrames; l++, poutframe++ ) {

			for( j = 0; j < poutmesh->numVerts; j++, pinvert++, poutvert++ ) {
				poutvert->point[0] = LittleShort( pinvert->point[0] );
				poutvert->point[1] = LittleShort( pinvert->point[1] );
				poutvert->point[2] = LittleShort( pinvert->point[2] );

				Q_sincos((float)pinvert->norm[0] / 255.0f, &s[0], &c[0]);	
				Q_sincos((float)pinvert->norm[1] / 255.0f, &s[1], &c[1]);
				VectorSet( normal, s[0] * c[1], s[0] * s[1], c[0] );
				poutvert->normalIndex = DirToByte( normal );

				normal[0] = (float)pinvert->point[0];
				normal[1] = (float)pinvert->point[1];
				normal[2] = (float)pinvert->point[2];
				AddPointToBounds( normal, poutframe->mins, poutframe->maxs );
			}
		}

		pinmesh = ( dmd3mesh_t * )( ( byte * )pinmesh + LittleLong( pinmesh->meshsize ) );
	}

//
// calculate model bounds
//
	poutframe = poutmodel->frames;
	for( i = 0; i < poutmodel->numFrames; i++, poutframe++ ) {
		VectorMA( poutframe->translate, MD3_XYZ_SCALE, poutframe->mins, poutframe->mins );
		VectorMA( poutframe->translate, MD3_XYZ_SCALE, poutframe->maxs, poutframe->maxs );
		poutframe->radius = RadiusFromBounds( poutframe->mins, poutframe->maxs );

		//AddPointToBounds( poutframe->mins, mod->mins, mod->maxs );
		//AddPointToBounds( poutframe->maxs, mod->mins, mod->maxs );
//		mod->radius = max( mod->radius, poutframe->radius );
	}
}

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/

/*
=================
Mod_LoadSpriteModel
=================
*/
static void Mod_LoadSpriteModel (model_t *mod, byte *rawdata, int length)
{
	dsprite_t		*sprin;
	dsprframe_t		*sprinframe;
	spriteModel_t	*sprout;
	spriteFrame_t	*sproutframe;
	int				i, numFrames, bufsize;
	char			skinName[SPRITE_MAX_NAME];

	if( length < sizeof( dsprite_t ) ) {
		Com_Error(ERR_DROP, "Mod_LoadSpriteModel: %s has length < header length\n", mod->name );
	}

	sprin = (dsprite_t *)rawdata;

	if (LittleLong(sprin->version) != SPRITE_VERSION)
		Com_Error (ERR_DROP, "Mod_LoadSpriteModel: %s has wrong version number (%d should be %d)\n",
				 mod->name, LittleLong(sprin->version), SPRITE_VERSION);

	numFrames = LittleLong (sprin->numframes);
	if (numFrames < 1)
		Com_Error (ERR_DROP, "Mod_LoadSpriteModel: %s has bad number of frames: %d\n", mod->name, numFrames);
	else if (numFrames > SPRITE_MAX_FRAMES)
		Com_Error (ERR_DROP, "Mod_LoadSpriteModel: %s has too many frames: %d > %d\n", mod->name, numFrames, SPRITE_MAX_FRAMES);

	sprinframe = sprin->frames;
	if ( ( byte * )(sprinframe + numFrames) > rawdata + length) {
		Com_Printf("Mod_LoadSpriteModel: WARNING: %s frames exeeds the filelenght with %d\n", mod->name, (int)(( byte * )(sprinframe + numFrames) - (rawdata + length)));
		numFrames = (length - sizeof( dsprite_t )) / sizeof( dsprframe_t );
		numFrames += 1; // dsprite_t got 1 frame
	}

	bufsize = sizeof(spriteModel_t) + sizeof(spriteFrame_t) * numFrames;

	mod->type = mod_sprite;
	mod->extradata = Mod_Malloc(bufsize);
	mod->extradatasize = bufsize;

	sprout = mod->sModel = (spriteModel_t *)mod->extradata;
	sprout->numFrames = numFrames;

	sprout->frames = sproutframe = (spriteFrame_t *)((byte *)sprout + sizeof(spriteModel_t));

	// byte swap everything
	for (i = 0; i < numFrames; i++, sprinframe++, sproutframe++)
	{
		sproutframe->width = LittleLong( sprinframe->width );
		sproutframe->height = LittleLong( sprinframe->height );
		if( sproutframe->width < 1 || sproutframe->height < 1 ) {
			Com_DPrintf( "Mod_LoadSpriteModel: %s has bad image dimensions for frame #%d: %d x %d\n",
				mod->name, sproutframe->width, sproutframe->height, i );
			sproutframe->width = 1;
			sproutframe->height = 1;
		}
		sproutframe->origin_x = LittleLong( sprinframe->origin_x );
		sproutframe->origin_y = LittleLong( sprinframe->origin_y );
		//Q_strncpyz(sproutframe->name, sprinframe->name, sizeof(sproutframe->name));

		Q_strncpyz(skinName, sprinframe->name, sizeof(skinName));
		sproutframe->image = GL_FindImage(skinName, it_sprite);
		if (!sproutframe->image) {
			Com_DPrintf( "Mod_LoadSpriteModel: %s: Couldn't find image '%s' for frame #%d\n", mod->name, skinName, i );
			sproutframe->image = r_notexture;
		}
	}
}

//=============================================================================

static void Mod_RemoveHash (model_t *mod)
{
	model_t	*entry, **back;
	unsigned int  hash;

	hash = Com_HashKey(mod->name, MODELS_HASH_SIZE);
	for(back=&mod_hash[hash], entry=mod_hash[hash]; entry; back=&entry->hashNext, entry=entry->hashNext ) {
		if( entry == mod ) {
			*back = entry->hashNext;
			break;
		}
	}
}
/*
@@@@@@@@@@@@@@@@@@@@@
R_BeginRegistration

Specifies the model that will be used as the world
@@@@@@@@@@@@@@@@@@@@@
*/
void R_BeginRegistration (const char *map)
{
	char	fullname[MAX_QPATH];
	cvar_t	*flushmap;
	model_t	*model;

	registration_sequence++;

	gl_state.registering = true;

	Com_sprintf (fullname, sizeof(fullname), "maps/%s.bsp", map);

	// explicitly free the old map if different
	// this guarantees that mod_known[0] is the world map
	flushmap = Cvar_Get ("flushmap", "0", 0);
	if ( mod_known[0].name[0] && (strcmp(mod_known[0].name, fullname) || flushmap->integer)) {
		Mod_Free (&mod_known[0]);
	}

	model = Mod_ForName(fullname, true);
	if(model)
		r_worldmodel = model->bspModel;
	else
		r_worldmodel = NULL;

	r_framecount = 1;
	r_oldviewcluster = r_viewcluster = -1;		// force markleafs

	GL_ClearDecals (); //Decals
}


/*
@@@@@@@@@@@@@@@@@@@@@
R_RegisterModel

@@@@@@@@@@@@@@@@@@@@@
*/
struct model_s *R_RegisterModel (const char *name)
{
	model_t			*mod;
	int				i, j;
	spriteFrame_t	*sFrame;
	aliasMesh_t		*mesh;

	i = strlen(name);
	if (gl_replacemd2->integer && i > 4 && !strcmp(name + i - 4, ".md2"))
	{
		char	s[MAX_QPATH];

		Q_strncpyz(s, name, sizeof(s));
		s[strlen(s) - 1] = '3';
		mod = Mod_ForName(s, false);
		if (!mod)
			mod = Mod_ForName (name, false);
	}
	else
		mod = Mod_ForName (name, false);

	if (mod)
	{
		mod->registration_sequence = registration_sequence;

		// register any images used by the models
		switch (mod->type) {
		case mod_sprite:
			if(!mod->sModel) {
				Com_Error( ERR_DROP, "R_RegisterModel: NULL sprite model: %s", name );
				return NULL;
			}
			for (i = 0, sFrame = mod->sModel->frames; i < mod->sModel->numFrames; i++, sFrame++) {
				if (sFrame->image)
					sFrame->image->registration_sequence = registration_sequence;
			}
			break;
		case mod_alias:
			if(!mod->aliasModel) {
				Com_Error( ERR_DROP, "R_RegisterModel: NULL alias model: %s", name );
				return NULL;
			}
			for (i = 0, mesh = mod->aliasModel->meshes; i < mod->aliasModel->numMeshes; i++, mesh++)
			{
				for (j = 0; j < mesh->numSkins; j++) {
					if (mesh->skins[j].image)
						mesh->skins[j].image->registration_sequence = registration_sequence;
				}
			}

			//mod->numframes = pheader->numframes;
			break;
		case mod_brush:
			if(!r_worldmodel) {
				Com_Error( ERR_DROP, "R_RegisterModel: NULL brush model: %s", name );
				return NULL;
			}
			for (i = 0; i < r_worldmodel->numtexinfo; i++) {
				r_worldmodel->texinfo[i].image->registration_sequence = registration_sequence;
			}
			break;
		default:
			Com_Error( ERR_DROP, "R_RegisterModel: bad model type: %d (%s)", mod->type, name );
			return NULL;
		}
	}
	return mod;
}


/*
@@@@@@@@@@@@@@@@@@@@@
R_EndRegistration

@@@@@@@@@@@@@@@@@@@@@
*/
void R_EndRegistration (void)
{
	int		i;
	model_t	*mod;

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (!mod->name[0])
			continue;
		if (mod->registration_sequence != registration_sequence)
		{	// don't need this model
			Mod_Free (mod);
		}
	}

	gl_state.registering = false;
	GL_FreeUnusedImages ();
}


//=============================================================================


/*
================
Mod_Free
================
*/
void Mod_Free (model_t *mod)
{
	Mod_RemoveHash(mod);
	if(mod->extradata) {
		if (mod->type == mod_brush)
			Hunk_Free(mod->extradata);
		else
			Z_Free(mod->extradata);
	}
	memset (mod, 0, sizeof(*mod));
}

/*
================
Mod_FreeAll
================
*/
void R_ShutdownModels (void)
{
	int		i;
	model_t	*mod;

	for (i = 0, mod = mod_known; i < mod_numknown; i++, mod++)
	{
		if (mod->extradata) {
			if (mod->type == mod_brush)
				Hunk_Free(mod->extradata);
			else
				Z_Free(mod->extradata);
		}
	}

	r_worldmodel = NULL;
	mod_numknown = 0;
	memset(mod_known, 0, sizeof(mod_known));
	memset(mod_hash, 0, sizeof(mod_hash));
	memset(mod_inline, 0, sizeof(mod_inline));
}
