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
// cmodel.c -- model loading

#include "qcommon.h"

typedef struct
{
	cplane_t	*plane;
	int			children[2];		// negative numbers are leafs
} cnode_t;

typedef struct
{
	cplane_t	*plane;
	mapsurface_t	*surface;
} cbrushside_t;

typedef struct
{
	int			contents;
	int			cluster;
	int			area;
	uint16		firstleafbrush;
	uint16		numleafbrushes;
} cleaf_t;

typedef struct
{
	int			contents;
	int			numsides;
	int			firstbrushside;
	int			checkcount;		// to avoid repeated testings
} cbrush_t;

typedef struct
{
	int		numareaportals;
	int		firstareaportal;
	int		floodnum;			// if two areas have equal floodnums, they are connected
	int		floodvalid;
} carea_t;

static int			checkcount;

static char			map_name[MAX_QPATH];

static int			numbrushsides;
static cbrushside_t map_brushsides[MAX_MAP_BRUSHSIDES];

int					numtexinfo;
mapsurface_t		map_surfaces[MAX_MAP_TEXINFO];

static int			numplanes;
static cplane_t		map_planes[MAX_MAP_PLANES+6];		// extra for box hull

static int			numnodes;
static cnode_t		map_nodes[MAX_MAP_NODES+6];		// extra for box hull

static int			numleafs = 1;	// allow leaf funcs to be called without a map
static cleaf_t		map_leafs[MAX_MAP_LEAFS];		 // extra for box hull

static int			numleafbrushes;
static uint16		map_leafbrushes[MAX_MAP_LEAFBRUSHES];

static int			numcmodels;
static cmodel_t		map_cmodels[MAX_MAP_MODELS];

static int			numbrushes;
static cbrush_t		map_brushes[MAX_MAP_BRUSHES];

static int			numvisibility;
static byte			map_visibility[MAX_MAP_VISIBILITY];
static dvis_t		*map_vis = (dvis_t *)map_visibility;

static int			numentitychars;
static char			map_entitystring[MAX_MAP_ENTSTRING];

static int			numareas = 1;
static carea_t		map_areas[MAX_MAP_AREAS];

static int			numareaportals;
static dareaportal_t map_areaportals[MAX_MAP_AREAPORTALS];

static int			numclusters = 1;

static mapsurface_t	nullsurface;

static int			floodvalid;

static qboolean		portalopen[MAX_MAP_AREAPORTALS];


static cvar_t		*map_noareas;

static void	CM_InitBoxHull (void);
static void	FloodAreaConnections (void);


int		c_pointcontents;
int		c_traces, c_brush_traces;


/*
===============================================================================

					MAP LOADING

===============================================================================
*/

static byte	*cmod_base;

/*
=================
CMod_LoadSubmodels
=================
*/
static void CMod_LoadSubmodels (const lump_t *l)
{
	const dmodel_t	*in;
	cmodel_t	*out;
	int			i, j, count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "CMod_LoadSubmodels: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no models");
	if (count > MAX_MAP_MODELS)
		Com_Error (ERR_DROP, "Map has too many models");

	numcmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out = &map_cmodels[i];

		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}
		out->headnode = LittleLong (in->headnode);
	}
}


/*
=================
CMod_LoadSurfaces
=================
*/
static void CMod_LoadSurfaces (const lump_t *l)
{
	const texinfo_t	*in;
	mapsurface_t	*out;
	int			i, count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "CMod_LoadSurfaces: funny lump size");
	count = l->filelen / sizeof(*in);
	if (count < 1)
		Com_Error (ERR_DROP, "Map with no surfaces");
	if (count > MAX_MAP_TEXINFO)
		Com_Error (ERR_DROP, "Map has too many surfaces");

	numtexinfo = count;
	out = map_surfaces;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		Q_strncpyz (out->c.name, in->texture, sizeof(out->c.name));
		Q_strncpyz (out->rname, in->texture, sizeof(out->rname));
		out->c.flags = LittleLong (in->flags);
		out->c.value = LittleLong (in->value);
	}
}


/*
=================
CMod_LoadNodes

=================
*/
static void CMod_LoadNodes (const lump_t *l)
{
	const dnode_t		*in;
	int			child;
	cnode_t		*out;
	int			i, j, count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "CMod_LoadNodes: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map has no nodes");
	if (count > MAX_MAP_NODES)
		Com_Error (ERR_DROP, "Map has too many nodes");

	out = map_nodes;

	numnodes = count;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->plane = map_planes + LittleLong(in->planenum);
		for (j=0 ; j<2 ; j++)
		{
			child = LittleLong (in->children[j]);
			out->children[j] = child;
		}
	}

}

/*
=================
CMod_LoadBrushes

=================
*/
static void CMod_LoadBrushes (const lump_t *l)
{
	const dbrush_t	*in;
	cbrush_t	*out;
	int			i, count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "CMod_LoadBrushes: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count > MAX_MAP_BRUSHES)
		Com_Error (ERR_DROP, "Map has too many brushes");

	out = map_brushes;

	numbrushes = count;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->firstbrushside = LittleLong(in->firstside);
		out->numsides = LittleLong(in->numsides);
		out->contents = LittleLong(in->contents);
	}

}

/*
=================
CMod_LoadLeafs
=================
*/
static void CMod_LoadLeafs (const lump_t *l)
{
	int			i;
	cleaf_t		*out;
	const dleaf_t 	*in;
	int			count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "CMod_LoadLeafs: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no leafs");
	// need to save space for box planes
	if (count > MAX_MAP_LEAFS)
		Com_Error (ERR_DROP, "Map has too many leafs");

	out = map_leafs;	
	numleafs = count;
	numclusters = 0;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->contents = LittleLong (in->contents);
		out->cluster = LittleShort (in->cluster);
		out->area = LittleShort (in->area);
		out->firstleafbrush = LittleShort (in->firstleafbrush);
		out->numleafbrushes = LittleShort (in->numleafbrushes);

		if (out->cluster >= numclusters)
			numclusters = out->cluster + 1;
	}

	if (map_leafs[0].contents != CONTENTS_SOLID)
		Com_Error (ERR_DROP, "Map leaf 0 is not CONTENTS_SOLID");
}

/*
=================
CMod_LoadPlanes
=================
*/
static void CMod_LoadPlanes (const lump_t *l)
{
	int			i, j;
	cplane_t	*out;
	const dplane_t 	*in;
	int			count;
	int			bits;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "CMod_LoadPlanes: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no planes");
	// need to save space for box planes
	if (count > MAX_MAP_PLANES)
		Com_Error (ERR_DROP, "Map has too many planes");

	out = map_planes;	
	numplanes = count;

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
	}
}

/*
=================
CMod_LoadLeafBrushes
=================
*/
static void CMod_LoadLeafBrushes (const lump_t *l)
{
	int			i;
	uint16		*out;
	const uint16	 	*in;
	int			count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "CMod_LoadLeafBrushes: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no planes");
	// need to save space for box planes
	if (count > MAX_MAP_LEAFBRUSHES)
		Com_Error (ERR_DROP, "Map has too many leafbrushes");

	out = map_leafbrushes;
	numleafbrushes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
		*out = LittleShort (*in);
}

/*
=================
CMod_LoadBrushSides
=================
*/
static void CMod_LoadBrushSides (const lump_t *l)
{
	int			i, j;
	cbrushside_t	*out;
	const dbrushside_t 	*in;
	int			count;
	int			num;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "CMod_LoadBrushSides: funny lump size");
	count = l->filelen / sizeof(*in);

	// need to save space for box planes
	if (count > MAX_MAP_BRUSHSIDES)
		Com_Error (ERR_DROP, "Map has too many planes");

	out = map_brushsides;	
	numbrushsides = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		num = LittleShort (in->planenum);
		out->plane = &map_planes[num];
		j = LittleShort (in->texinfo);
		if (j >= numtexinfo)
			Com_Error (ERR_DROP, "Bad brushside texinfo");
		else if (j < 0)
			j = 0;
		out->surface = &map_surfaces[j];
	}
}

/*
=================
CMod_LoadAreas
=================
*/
static void CMod_LoadAreas (const lump_t *l)
{
	int			i;
	carea_t		*out;
	const darea_t 	*in;
	int			count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "CMod_LoadAreas: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count > MAX_MAP_AREAS)
		Com_Error (ERR_DROP, "Map has too many areas");

	out = map_areas;
	numareas = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->numareaportals = LittleLong (in->numareaportals);
		out->firstareaportal = LittleLong (in->firstareaportal);
		out->floodvalid = 0;
		out->floodnum = 0;
	}
}

/*
=================
CMod_LoadAreaPortals
=================
*/
static void CMod_LoadAreaPortals (const lump_t *l)
{
	int			i;
	dareaportal_t		*out;
	const dareaportal_t *in;
	int			count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "CMod_LoadAreaPortals: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count > MAX_MAP_AREAS)
		Com_Error (ERR_DROP, "Map has too many areas");

	out = map_areaportals;
	numareaportals = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->portalnum = LittleLong (in->portalnum);
		out->otherarea = LittleLong (in->otherarea);
	}
}

/*
=================
CMod_LoadVisibility
=================
*/
static void CMod_LoadVisibility (const lump_t *l)
{
#ifndef ENDIAN_LITTLE
	int		i;
#endif
	numvisibility = l->filelen;
	if (l->filelen > MAX_MAP_VISIBILITY)
		Com_Error (ERR_DROP, "Map has too large visibility lump");

	memcpy (map_visibility, cmod_base + l->fileofs, l->filelen);

#ifndef ENDIAN_LITTLE
	map_vis->numclusters = LittleLong (map_vis->numclusters);
	for (i=0 ; i<map_vis->numclusters ; i++)
	{
		map_vis->bitofs[i][0] = LittleLong (map_vis->bitofs[i][0]);
		map_vis->bitofs[i][1] = LittleLong (map_vis->bitofs[i][1]);
	}
#endif
}


/*
=================
CMod_LoadEntityString
=================
*/
static void CMod_LoadEntityString (const lump_t *l)
{
	numentitychars = l->filelen;
	if (l->filelen > MAX_MAP_ENTSTRING)
		Com_Error (ERR_DROP, "Map has too large entity lump");

	memcpy (map_entitystring, cmod_base + l->fileofs, l->filelen);
}



/*
==================
CM_LoadMap

Loads in the map and all submodels
==================
*/
cmodel_t *CM_LoadMap (const char *name, qboolean clientload, uint32 *checksum)
{
	unsigned		*buf;
	int				i, length;
	dheader_t		header;	
	static uint32 last_checksum;
	lump_t			*lump;

	map_noareas = Cvar_Get ("map_noareas", "0", 0);

	if (!strcmp (map_name, name) && (clientload || !Cvar_VariableIntValue("flushmap")) )
	{
		*checksum = last_checksum;
		if (!clientload)
		{
			memset (portalopen, 0, sizeof(portalopen));
			FloodAreaConnections ();
		}
		return &map_cmodels[0];		// still have the right version
	}

	// free old stuff
	numplanes = 0;
	numnodes = 0;
	numleafs = 0;
	numcmodels = 0;
	numvisibility = 0;
	numentitychars = 0;
	//map_entitystring[0] = 0;
	//map_name[0] = 0;

	//r1: fix for missing terminators on some badly compiled maps
	memset (map_entitystring, 0, sizeof(map_entitystring));
	memset (map_name, 0, sizeof(map_name));

	if (!name || !name[0])
	{
		numleafs = 1;
		numclusters = 1;
		numareas = 1;
		last_checksum = 0;
		*checksum = 0;
		return &map_cmodels[0];			// cinematic servers won't have anything at all
	}

	//
	// load the file
	//
	length = FS_LoadFile (name, (void **)&buf);
	if (!buf)
		Com_Error (ERR_DROP, "Couldn't load %s", name);

	last_checksum = LittleLong (Com_BlockChecksum (buf, length));
	*checksum = last_checksum;

	header = *(dheader_t *)buf;
	header.ident = LittleLong( header.ident );
	header.version = LittleLong( header.version );
	if (header.version != BSPVERSION)
	{
		FS_FreeFile (buf);
		Com_Error (ERR_DROP, "CM_LoadMap: %s has wrong version number (%i should be %i)"
		, name, header.version, BSPVERSION);
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
			FS_FreeFile (buf);
			Com_Error (ERR_DROP, "CM_LoadMap: lump %d offset %d of size %d is out of bounds\n%s is probably truncated or otherwise corrupted", i, lump->fileofs, lump->filelen, name);
		}
	}
	cmod_base = (byte *)buf;

	// load into heap
	CMod_LoadSurfaces (&header.lumps[LUMP_TEXINFO]);
	CMod_LoadLeafs (&header.lumps[LUMP_LEAFS]);
	CMod_LoadLeafBrushes (&header.lumps[LUMP_LEAFBRUSHES]);
	CMod_LoadPlanes (&header.lumps[LUMP_PLANES]);
	CMod_LoadBrushes (&header.lumps[LUMP_BRUSHES]);
	CMod_LoadBrushSides (&header.lumps[LUMP_BRUSHSIDES]);
	CMod_LoadSubmodels (&header.lumps[LUMP_MODELS]);
	CMod_LoadNodes (&header.lumps[LUMP_NODES]);
	CMod_LoadAreas (&header.lumps[LUMP_AREAS]);
	CMod_LoadAreaPortals (&header.lumps[LUMP_AREAPORTALS]);
	CMod_LoadVisibility (&header.lumps[LUMP_VISIBILITY]);
	CMod_LoadEntityString (&header.lumps[LUMP_ENTITIES]);

	FS_FreeFile (buf);

	CM_InitBoxHull ();

	memset (portalopen, 0, sizeof(portalopen));
	FloodAreaConnections ();

	Q_strncpyz (map_name, name, sizeof(map_name));

	return &map_cmodels[0];
}

/*
==================
CM_InlineModel
==================
*/
cmodel_t *CM_InlineModel (const char *name)
{
	int		num;

	if (!name || name[0] != '*')
		Com_Error (ERR_DROP, "CM_InlineModel: bad name");
	num = atoi (name+1);
	if (num < 1 || num >= numcmodels)
		Com_Error (ERR_DROP, "CM_InlineModel: bad number");

	return &map_cmodels[num];
}

int		CM_NumClusters (void)
{
	return numclusters;
}

int		CM_NumInlineModels (void)
{
	return numcmodels;
}

char	*CM_EntityString (void)
{
	return map_entitystring;
}

int		CM_LeafContents (int leafnum)
{
	if (leafnum < 0 || leafnum >= numleafs)
		Com_Error (ERR_DROP, "CM_LeafContents: bad number");
	return map_leafs[leafnum].contents;
}

int		CM_LeafCluster (int leafnum)
{
	if (leafnum < 0 || leafnum >= numleafs)
		Com_Error (ERR_DROP, "CM_LeafCluster: bad number");
	return map_leafs[leafnum].cluster;
}

int		CM_LeafArea (int leafnum)
{
	if (leafnum < 0 || leafnum >= numleafs)
		Com_Error (ERR_DROP, "CM_LeafArea: bad number");
	return map_leafs[leafnum].area;
}

//=======================================================================


static cplane_t	*box_planes;
static int		box_headnode;
static cbrush_t	*box_brush;
static cleaf_t	*box_leaf;

/*
===================
CM_InitBoxHull

Set up the planes and nodes so that the six floats of a bounding box
can just be stored out and get a proper clipping hull structure.
===================
*/
static void CM_InitBoxHull (void)
{
	int			i;
	int			side;
	cnode_t		*c;
	cplane_t	*p;
	cbrushside_t	*s;

	box_headnode = numnodes;
	box_planes = &map_planes[numplanes];
	if (numnodes+6 > MAX_MAP_NODES
		|| numbrushes+1 > MAX_MAP_BRUSHES
		|| numleafbrushes+1 > MAX_MAP_LEAFBRUSHES
		|| numbrushsides+6 > MAX_MAP_BRUSHSIDES
		|| numplanes+12 > MAX_MAP_PLANES)
		Com_Error (ERR_DROP, "Not enough room for box tree");

	box_brush = &map_brushes[numbrushes];
	box_brush->numsides = 6;
	box_brush->firstbrushside = numbrushsides;
	box_brush->contents = CONTENTS_MONSTER;

	box_leaf = &map_leafs[numleafs];
	box_leaf->contents = CONTENTS_MONSTER;
	box_leaf->firstleafbrush = numleafbrushes;
	box_leaf->numleafbrushes = 1;

	map_leafbrushes[numleafbrushes] = numbrushes;

	for (i=0 ; i<6 ; i++)
	{
		side = i&1;

		// brush sides
		s = &map_brushsides[numbrushsides+i];
		s->plane = 	map_planes + (numplanes+i*2+side);
		s->surface = &nullsurface;

		// nodes
		c = &map_nodes[box_headnode+i];
		c->plane = map_planes + (numplanes+i*2);
		c->children[side] = -1 - numleafs;
		if (i != 5)
			c->children[side^1] = box_headnode+i + 1;
		else
			c->children[side^1] = -1 - numleafs;

		// planes
		p = &box_planes[i*2];
		p->type = i>>1;
		p->signbits = 0;
		VectorClear (p->normal);
		p->normal[i>>1] = 1;

		p = &box_planes[i*2+1];
		p->type = 3 + (i>>1);
		p->signbits = 0;
		VectorClear (p->normal);
		p->normal[i>>1] = -1;
	}	
}


/*
===================
CM_HeadnodeForBox

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
int	CM_HeadnodeForBox (const vec3_t mins, const vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = -maxs[0];
	box_planes[2].dist = mins[0];
	box_planes[3].dist = -mins[0];
	box_planes[4].dist = maxs[1];
	box_planes[5].dist = -maxs[1];
	box_planes[6].dist = mins[1];
	box_planes[7].dist = -mins[1];
	box_planes[8].dist = maxs[2];
	box_planes[9].dist = -maxs[2];
	box_planes[10].dist = mins[2];
	box_planes[11].dist = -mins[2];

	return box_headnode;
}


/*
==================
CM_PointLeafnum_r

==================
*/
static int CM_PointLeafnum_r (const vec3_t p, int num)
{
	float		d;
	const cnode_t	*node;
	const cplane_t	*plane;

	while (num >= 0)
	{
		node = map_nodes + num;
		plane = node->plane;
		
		if (plane->type < 3)
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct (plane->normal, p) - plane->dist;
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	c_pointcontents++;		// optimize counter

	return -1 - num;
}

int CM_PointLeafnum (const vec3_t p)
{
	if (!numplanes)
		return 0;		// sound may call this without map loaded
	return CM_PointLeafnum_r (p, 0);
}



/*
=============
CM_BoxLeafnums

Fills in a list of all the leafs touched
=============
*/
static int		leaf_count, leaf_maxcount;
static int		*leaf_list;
static float	*leaf_mins, *leaf_maxs;
static int		leaf_topnode;

static void CM_BoxLeafnums_r (int nodenum)
{
	const cplane_t	*plane;
	const cnode_t	*node;
	int		s;

	while (1)
	{
		if (nodenum < 0)
		{
			if (leaf_count >= leaf_maxcount)
				return;

			leaf_list[leaf_count++] = -1 - nodenum;
			return;
		}
	
		node = &map_nodes[nodenum];
		plane = node->plane;
		//s = BoxOnPlaneSide (leaf_mins, leaf_maxs, plane);

		s = BOX_ON_PLANE_SIDE(leaf_mins, leaf_maxs, plane);
		if (s == 1)
			nodenum = node->children[0];
		else if (s == 2)
			nodenum = node->children[1];
		else
		{	// go down both
			if (leaf_topnode == -1)
				leaf_topnode = nodenum;
			CM_BoxLeafnums_r (node->children[0]);
			nodenum = node->children[1];
		}

	}
}

static int CM_BoxLeafnums_headnode (vec3_t mins, vec3_t maxs, int *list, int listsize, int headnode, int *topnode)
{
	leaf_list = list;
	leaf_count = 0;
	leaf_maxcount = listsize;
	leaf_mins = mins;
	leaf_maxs = maxs;

	leaf_topnode = -1;

	CM_BoxLeafnums_r (headnode);

	if (topnode)
		*topnode = leaf_topnode;

	return leaf_count;
}

int	CM_BoxLeafnums (vec3_t mins, vec3_t maxs, int *list, int listsize, int *topnode)
{
	return CM_BoxLeafnums_headnode (mins, maxs, list,
		listsize, map_cmodels[0].headnode, topnode);
}



/*
==================
CM_PointContents

==================
*/
int CM_PointContents (const vec3_t p, int headnode)
{
	int		l;

	if (!numnodes)	// map not loaded
		return 0;

	l = CM_PointLeafnum_r (p, headnode);

	return map_leafs[l].contents;
}

/*
==================
CM_TransformedPointContents

Handles offseting and rotation of the end points for moving and
rotating entities
==================
*/
int	CM_TransformedPointContents (const vec3_t p, int headnode, const vec3_t origin, const vec3_t angles)
{
	vec3_t		p_l;
	vec3_t		temp;
	vec3_t		forward, right, up;
	int			l;

	// subtract origin offset
	VectorSubtract (p, origin, p_l);

	// rotate start and end into the models frame of reference
	if (headnode != box_headnode && 
	(angles[0] || angles[1] || angles[2]) )
	{
		AngleVectors (angles, forward, right, up);

		VectorCopy (p_l, temp);
		p_l[0] = DotProduct (temp, forward);
		p_l[1] = -DotProduct (temp, right);
		p_l[2] = DotProduct (temp, up);
	}

	l = CM_PointLeafnum_r (p_l, headnode);

	return map_leafs[l].contents;
}


/*
===============================================================================

BOX TRACING

===============================================================================
*/

// 1/32 epsilon to keep floating point happy
#define	DIST_EPSILON	(0.03125f)

static vec3_t	trace_start, trace_end;
static vec3_t	trace_mins, trace_maxs;
static vec3_t	trace_extents;

static trace_t	trace_trace;
static int		trace_contents;
static qboolean	trace_ispoint;		// optimized case

/*
================
CM_ClipBoxToBrush
================
*/
static void CM_ClipBoxToBrush (const vec3_t mins, const vec3_t maxs, const vec3_t p1, const vec3_t p2,
					  trace_t *trace, const cbrush_t *brush)
{
	int			i, j;
	const cplane_t	*plane, *clipplane;
	float		dist;
	float		enterfrac, leavefrac;
	vec3_t		ofs;
	float		d1, d2;
	qboolean	getout, startout;
	float		f;
	const cbrushside_t	*side, *leadside;

	enterfrac = -1;
	leavefrac = 1;
	clipplane = NULL;

	if (!brush->numsides)
		return;

	c_brush_traces++;

	getout = false;
	startout = false;
	leadside = NULL;

	for (i=0 ; i<brush->numsides ; i++)
	{
		side = &map_brushsides[brush->firstbrushside+i];
		plane = side->plane;

		// FIXME: special case for axial

		if (!trace_ispoint)
		{	// general box case

			// push the plane out apropriately for mins/maxs

			// FIXME: use signbits into 8 way lookup for each mins/maxs
			for (j=0 ; j<3 ; j++)
			{
				if (plane->normal[j] < 0)
					ofs[j] = maxs[j];
				else
					ofs[j] = mins[j];
			}
			dist = DotProduct (ofs, plane->normal);
			dist = plane->dist - dist;
		}
		else
		{	// special point case
			dist = plane->dist;
		}

		d1 = DotProduct (p1, plane->normal) - dist;
		d2 = DotProduct (p2, plane->normal) - dist;

		if (d2 > 0.0f)
			getout = true;	// endpoint is not in solid
		if (d1 > 0.0f)
			startout = true;

		// if completely in front of face, no intersection
		if (d1 > 0.0f && d2 >= d1)
			return;

		if (d1 <= 0.0f && d2 <= 0.0f)
			continue;

		// crosses face
		if (d1 > d2)
		{	// enter
			f = (d1-DIST_EPSILON) / (d1-d2);
			if (f > enterfrac)
			{
				enterfrac = f;
				clipplane = plane;
				leadside = side;
			}
		}
		else
		{	// leave
			f = (d1+DIST_EPSILON) / (d1-d2);
			if (f < leavefrac)
				leavefrac = f;
		}
	}

	if (!startout)
	{	// original point was inside brush
		trace->startsolid = true;
		if (!getout)
			trace->allsolid = true;
		return;
	}
	if (enterfrac < leavefrac)
	{
		if (enterfrac > -1 && enterfrac < trace->fraction)
		{
			if (enterfrac < 0.0f)
				enterfrac = 0;
			trace->fraction = enterfrac;
			trace->plane = *clipplane;
			trace->surface = &(leadside->surface->c);
			trace->contents = brush->contents;
		}
	}
}

/*
================
CM_TestBoxInBrush
================
*/
static void CM_TestBoxInBrush (const vec3_t mins, const vec3_t maxs, const vec3_t p1,
					  trace_t *trace, const cbrush_t *brush)
{
	int			i, j;
	const cplane_t	*plane;
	float		dist;
	vec3_t		ofs;
	float		d1;
	const cbrushside_t	*side;

	if (!brush->numsides)
		return;

	for (i=0 ; i<brush->numsides ; i++)
	{
		side = &map_brushsides[brush->firstbrushside+i];
		plane = side->plane;

		// FIXME: special case for axial

		// general box case

		// push the plane out apropriately for mins/maxs

		// FIXME: use signbits into 8 way lookup for each mins/maxs
		for (j=0 ; j<3 ; j++)
		{
			if (plane->normal[j] < 0)
				ofs[j] = maxs[j];
			else
				ofs[j] = mins[j];
		}
		dist = DotProduct (ofs, plane->normal);
		dist = plane->dist - dist;

		d1 = DotProduct (p1, plane->normal) - dist;

		// if completely in front of face, no intersection
		if (d1 > 0.0f)
			return;

	}

	// inside this brush
	trace->startsolid = trace->allsolid = true;
	trace->fraction = 0;
	trace->contents = brush->contents;
}


/*
================
CM_TraceToLeaf
================
*/
static void CM_TraceToLeaf (int leafnum)
{
	int			k;
	int			brushnum;
	const cleaf_t		*leaf;
	cbrush_t	*b;

	leaf = &map_leafs[leafnum];
	if ( !(leaf->contents & trace_contents))
		return;
	// trace line against all brushes in the leaf
	for (k=0 ; k<leaf->numleafbrushes ; k++)
	{
		brushnum = map_leafbrushes[leaf->firstleafbrush+k];
		b = &map_brushes[brushnum];
		if (b->checkcount == checkcount)
			continue;	// already checked this brush in another leaf
		b->checkcount = checkcount;

		if ( !(b->contents & trace_contents))
			continue;
		CM_ClipBoxToBrush (trace_mins, trace_maxs, trace_start, trace_end, &trace_trace, b);
		if (!trace_trace.fraction)
			return;
	}

}


/*
================
CM_TestInLeaf
================
*/
static void CM_TestInLeaf (int leafnum)
{
	int			k;
	int			brushnum;
	const cleaf_t		*leaf;
	cbrush_t	*b;

	leaf = &map_leafs[leafnum];
	if ( !(leaf->contents & trace_contents))
		return;
	// trace line against all brushes in the leaf
	for (k=0 ; k<leaf->numleafbrushes ; k++)
	{
		brushnum = map_leafbrushes[leaf->firstleafbrush+k];
		b = &map_brushes[brushnum];
		if (b->checkcount == checkcount)
			continue;	// already checked this brush in another leaf
		b->checkcount = checkcount;

		if ( !(b->contents & trace_contents))
			continue;
		CM_TestBoxInBrush (trace_mins, trace_maxs, trace_start, &trace_trace, b);
		if (!trace_trace.fraction)
			return;
	}

}


/*
==================
CM_RecursiveHullCheck

==================
*/
static void CM_RecursiveHullCheck (int num, float p1f, float p2f, const vec3_t p1, const vec3_t p2)
{
	const cnode_t		*node;
	const cplane_t	*plane;
	float		t1, t2, offset;
	float		frac, frac2;
	float		idist;
	vec3_t		mid;
	int			side;
	float		midf;

	if (trace_trace.fraction <= p1f)
		return;		// already hit something nearer

	// if < 0, we are in a leaf node
	if (num < 0)
	{
		CM_TraceToLeaf (-1-num);
		return;
	}

	//
	// find the point distances to the seperating plane
	// and the offset for the size of the box
	//
	node = map_nodes + num;
	plane = node->plane;

	if (plane->type < 3)
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
		offset = trace_extents[plane->type];
	}
	else
	{
		t1 = DotProduct (plane->normal, p1) - plane->dist;
		t2 = DotProduct (plane->normal, p2) - plane->dist;
		if (trace_ispoint)
			offset = 0;
		else
			offset = (float)(fabs(trace_extents[0]*plane->normal[0]) +
				fabs(trace_extents[1]*plane->normal[1]) +
				fabs(trace_extents[2]*plane->normal[2]));
	}


	// see which sides we need to consider
	if (t1 >= offset && t2 >= offset)
	{
		CM_RecursiveHullCheck (node->children[0], p1f, p2f, p1, p2);
		return;
	}
	if (t1 < -offset && t2 < -offset)
	{
		CM_RecursiveHullCheck (node->children[1], p1f, p2f, p1, p2);
		return;
	}

	// put the crosspoint DIST_EPSILON pixels on the near side
	if (t1 < t2)
	{
		idist = 1.0f/(t1-t2);
		side = 1;
		frac2 = (t1 + offset + DIST_EPSILON)*idist;
		frac = (t1 - offset + DIST_EPSILON)*idist;
	}
	else if (t1 > t2)
	{
		idist = 1.0f/(t1-t2);
		side = 0;
		frac2 = (t1 - offset - DIST_EPSILON)*idist;
		frac = (t1 + offset + DIST_EPSILON)*idist;
	}
	else
	{
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	// move up to the node
	if (frac < 0.0f)
		frac = 0.0f;
	if (frac > 1.0f)
		frac = 1.0f;
		
	midf = p1f + (p2f - p1f)*frac;
	mid[0] = p1[0] + frac*(p2[0] - p1[0]);
	mid[1] = p1[1] + frac*(p2[1] - p1[1]);
	mid[2] = p1[2] + frac*(p2[2] - p1[2]);

	CM_RecursiveHullCheck (node->children[side], p1f, midf, p1, mid);

	// go past the node
	if (frac2 < 0.0f)
		frac2 = 0.0f;
	if (frac2 > 1.0f)
		frac2 = 1.0f;
		
	midf = p1f + (p2f - p1f)*frac2;
	mid[0] = p1[0] + frac2*(p2[0] - p1[0]);
	mid[1] = p1[1] + frac2*(p2[1] - p1[1]);
	mid[2] = p1[2] + frac2*(p2[2] - p1[2]);


	CM_RecursiveHullCheck (node->children[side^1], midf, p2f, mid, p2);
}



//======================================================================

/*
==================
CM_BoxTrace
==================
*/
trace_t		CM_BoxTrace (const vec3_t start, const vec3_t end,
						  const vec3_t mins, const vec3_t maxs,
						  int headnode, int brushmask)
{

	checkcount++;		// for multi-check avoidance

	c_traces++;			// for statistics, may be zeroed

	// fill in a default trace
	memset (&trace_trace, 0, sizeof(trace_trace));
	trace_trace.fraction = 1;
	trace_trace.surface = &(nullsurface.c);

	if (!numnodes)	// map not loaded
		return trace_trace;

	trace_contents = brushmask;
	VectorCopy (start, trace_start);
	VectorCopy (end, trace_end);
	VectorCopy (mins, trace_mins);
	VectorCopy (maxs, trace_maxs);

	//
	// check for position test special case
	//
	if (start[0] == end[0] && start[1] == end[1] && start[2] == end[2])
	{
		int		leafs[1024];
		int		i, numleafs, topnode;
		vec3_t	c1, c2;

		VectorAdd (start, mins, c1);
		VectorAdd (start, maxs, c2);

		c1[0] -= 1;
		c2[0] += 1;
		c1[1] -= 1;
		c2[1] += 1;
		c1[2] -= 1;
		c2[2] += 1;

		numleafs = CM_BoxLeafnums_headnode (c1, c2, leafs, 1024, headnode, &topnode);
		for (i=0 ; i<numleafs ; i++)
		{
			CM_TestInLeaf (leafs[i]);
			if (trace_trace.allsolid)
				break;
		}
		VectorCopy (start, trace_trace.endpos);
		return trace_trace;
	}

	//
	// check for point special case
	//
	if (mins[0] == 0 && mins[1] == 0 && mins[2] == 0
		&& maxs[0] == 0 && maxs[1] == 0 && maxs[2] == 0)
	{
		trace_ispoint = true;
		VectorClear (trace_extents);
	}
	else
	{
		trace_ispoint = false;
		trace_extents[0] = -mins[0] > maxs[0] ? -mins[0] : maxs[0];
		trace_extents[1] = -mins[1] > maxs[1] ? -mins[1] : maxs[1];
		trace_extents[2] = -mins[2] > maxs[2] ? -mins[2] : maxs[2];
	}

	//
	// general sweeping through world
	//
	CM_RecursiveHullCheck (headnode, 0, 1, start, end);

	if (trace_trace.fraction == 1.0f)
	{
		VectorCopy (end, trace_trace.endpos);
	}
	else
	{
		trace_trace.endpos[0] = start[0] + trace_trace.fraction * (end[0] - start[0]);
		trace_trace.endpos[1] = start[1] + trace_trace.fraction * (end[1] - start[1]);
		trace_trace.endpos[2] = start[2] + trace_trace.fraction * (end[2] - start[2]);
	}
	return trace_trace;
}


/*
==================
CM_TransformedBoxTrace

Handles offseting and rotation of the end points for moving and
rotating entities
==================
*/
//#ifdef _WIN32
//#pragma optimize( "", off )
//#endif


trace_t		CM_TransformedBoxTrace (const vec3_t start, const vec3_t end,
						  const vec3_t mins, const vec3_t maxs,
						  int headnode, int brushmask,
						  const vec3_t origin, const vec3_t angles)
{
	trace_t		trace;
	vec3_t		start_l, end_l;
	vec3_t		a;
	vec3_t		forward, right, up;
	vec3_t		temp;
	qboolean	rotated = false;

	// subtract origin offset
	VectorSubtract (start, origin, start_l);
	VectorSubtract (end, origin, end_l);

	// rotate start and end into the models frame of reference
	if (headnode != box_headnode && (angles[0] || angles[1] || angles[2]) )
	{
		rotated = true;

		AngleVectors (angles, forward, right, up);

		VectorCopy (start_l, temp);
		start_l[0] = DotProduct (temp, forward);
		start_l[1] = -DotProduct (temp, right);
		start_l[2] = DotProduct (temp, up);

		VectorCopy (end_l, temp);
		end_l[0] = DotProduct (temp, forward);
		end_l[1] = -DotProduct (temp, right);
		end_l[2] = DotProduct (temp, up);
	}

	// sweep the box through the model
	trace = CM_BoxTrace (start_l, end_l, mins, maxs, headnode, brushmask);

	if (rotated && trace.fraction != 1.0f)
	{
		// FIXME: figure out how to do this with existing angles
		VectorNegate (angles, a);
		AngleVectors (a, forward, right, up);

		VectorCopy (trace.plane.normal, temp);
		trace.plane.normal[0] = DotProduct (temp, forward);
		trace.plane.normal[1] = -DotProduct (temp, right);
		trace.plane.normal[2] = DotProduct (temp, up);
	}

	trace.endpos[0] = start[0] + trace.fraction * (end[0] - start[0]);
	trace.endpos[1] = start[1] + trace.fraction * (end[1] - start[1]);
	trace.endpos[2] = start[2] + trace.fraction * (end[2] - start[2]);

	return trace;
}

//#ifdef _WIN32
//#pragma optimize( "", on )
//#endif



/*
===============================================================================

PVS / PHS

===============================================================================
*/

/*
===================
CM_DecompressVis
===================
*/
static void CM_DecompressVis (byte *in, byte *out)
{
	int		c;
	byte	*out_p;
	int		row;

	row = (numclusters+7)>>3;	
	out_p = out;

	if (!in || !numvisibility)
	{	// no vis info, so make all visible
		while (row)
		{
			*out_p++ = 0xff;
			row--;
		}
		return;		
	}

	do
	{
		if (*in)
		{
			*out_p++ = *in++;
			continue;
		}
	
		c = in[1];
		in += 2;
		if ((out_p - out) + c > row)
		{
			c = row - (out_p - out);
			Com_DPrintf ("warning: Vis decompression overrun\n");
		}
		while (c)
		{
			*out_p++ = 0;
			c--;
		}
	} while (out_p - out < row);
}

static byte	pvsrow[MAX_MAP_LEAFS/8];
static byte	phsrow[MAX_MAP_LEAFS/8];

byte	*CM_ClusterPVS (int cluster)
{
	if (cluster == -1)
		memset (pvsrow, 0, (numclusters+7)>>3);
	else
		CM_DecompressVis (map_visibility + map_vis->bitofs[cluster][DVIS_PVS], pvsrow);
	return pvsrow;
}

byte	*CM_ClusterPHS (int cluster)
{
	if (cluster == -1)
		memset (phsrow, 0, (numclusters+7)>>3);
	else
		CM_DecompressVis (map_visibility + map_vis->bitofs[cluster][DVIS_PHS], phsrow);
	return phsrow;
}


/*
===============================================================================

AREAPORTALS

===============================================================================
*/

static void FloodArea_r (carea_t *area, int floodnum)
{
	int		i;
	const dareaportal_t	*p;

	if (area->floodvalid == floodvalid)
	{
		if (area->floodnum == floodnum)
			return;
		Com_Error (ERR_DROP, "FloodArea_r: reflooded");
	}

	area->floodnum = floodnum;
	area->floodvalid = floodvalid;
	p = &map_areaportals[area->firstareaportal];
	for (i=0 ; i<area->numareaportals ; i++, p++)
	{
		if (portalopen[p->portalnum])
			FloodArea_r (&map_areas[p->otherarea], floodnum);
	}
}

/*
====================
FloodAreaConnections


====================
*/
static void	FloodAreaConnections (void)
{
	int		i;
	carea_t	*area;
	int		floodnum = 0;

	// all current floods are now invalid
	floodvalid++;

	// area 0 is not used
	for (i=1 ; i<numareas ; i++)
	{
		area = &map_areas[i];
		if (area->floodvalid == floodvalid)
			continue;		// already flooded into
		floodnum++;
		FloodArea_r (area, floodnum);
	}

}

void CM_SetAreaPortalState (int portalnum, qboolean open)
{
	if (portalnum > numareaportals)
		Com_Error (ERR_DROP, "CM_SetAreaPortalState: areaportal > numareaportals");

	if (portalnum < 0)
		Com_Error (ERR_DROP, "CM_SetAreaPortalState: areaportal < 0");

	portalopen[portalnum] = open;
	FloodAreaConnections ();
}

qboolean CM_AreasConnected (int area1, int area2)
{
	if (map_noareas->integer)
		return true;

	if (area1 > numareas || area2 > numareas)
		Com_Error (ERR_DROP, "CM_AreasConnected: area > numareas");

	if (area1 < 0 || area2 < 0)
		Com_Error (ERR_DROP, "CM_AreasConnected: area < 0");

	if (map_areas[area1].floodnum == map_areas[area2].floodnum)
		return true;
	return false;
}


/*
=================
CM_WriteAreaBits

Writes a length byte followed by a bit vector of all the areas
that area in the same flood as the area parameter

This is used by the client refreshes to cull visibility
=================
*/
int CM_WriteAreaBits (byte *buffer, int area)
{
	int	i, floodnum, bytes;

	bytes = (numareas+7)>>3;

	if (map_noareas->integer || !area)
	{	// for debugging, send everything
		memset (buffer, 255, bytes);
	}
	else
	{
		memset (buffer, 0, bytes);

		floodnum = map_areas[area].floodnum;
		for (i=0 ; i<numareas ; i++)
		{
			if (map_areas[i].floodnum == floodnum)
				buffer[i>>3] |= 1<<(i&7);
		}
	}

	return bytes;
}


/*
===================
CM_WritePortalState

Writes the portal state to a savegame file
===================
*/
void CM_WritePortalState (fileHandle_t f)
{
	FS_Write( portalopen, sizeof(portalopen), f );
}

/*
===================
CM_ReadPortalState

Reads the portal state from a savegame file
and recalculates the area connections
===================
*/
void CM_ReadPortalState (fileHandle_t f)
{
	FS_Read( portalopen, sizeof(portalopen), f );
	FloodAreaConnections ();
}

/*
=============
CM_HeadnodeVisible

Returns true if any leaf under headnode has a cluster that
is potentially visible
=============
*/
qboolean CM_HeadnodeVisible (int nodenum, const byte *visbits)
{
	int		leafnum;
	int		cluster;
	const cnode_t	*node;

	if (nodenum < 0)
	{
		leafnum = -1-nodenum;
		cluster = map_leafs[leafnum].cluster;
		if (cluster == -1)
			return false;
		if (visbits[cluster>>3] & (1<<(cluster&7)))
			return true;
		return false;
	}

	node = &map_nodes[nodenum];
	if (CM_HeadnodeVisible(node->children[0], visbits))
		return true;
	return CM_HeadnodeVisible(node->children[1], visbits);
}

