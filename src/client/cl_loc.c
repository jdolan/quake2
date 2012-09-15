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
#include "client.h"

typedef struct cl_location_s
{
	struct cl_location_s	*next;
	vec3_t					location;
	char					name[1];
} cl_location_t;

static cl_location_t	*cl_locations = NULL;

static cvar_t	*cl_drawlocs;
static cvar_t	*loc_dir;
static char locDir[MAX_QPATH];

void CL_FreeLocs(void)
{
	cl_location_t	*loc, *next;

	for(loc = cl_locations; loc; loc = next) {
		next = loc->next;
		Z_Free(loc);
	}
	cl_locations = NULL;
}

static void CL_AddLoc (const vec3_t location, const char *name)
{
	cl_location_t	*loc;
	int length;

	length = strlen(name);

	loc = Z_TagMalloc (sizeof(cl_location_t) + length, TAG_CL_LOC);
	strcpy(loc->name, name);
	VectorCopy(location, loc->location);
	loc->next = cl_locations;
	cl_locations = loc;
}

void CL_LoadLoc(void)
{

	char fileName[MAX_OSPATH], *buffer = NULL;
	int line = 0, fileLen = 0, count = 0;
	char *s, *p;
	vec3_t	origin;

	CL_FreeLocs();

	if (!cls.mapname[0])
		return;

	Com_sprintf(fileName, sizeof(fileName), "%s/%s.loc", locDir, cls.mapname);

	fileLen = FS_LoadFile( fileName, (void **)&buffer );
	if (!buffer) {
		Com_DPrintf ("CL_LoadLoc: %s not found\n", fileName);
		return;
	}

	s = buffer;

	while( *s ) {
		p = strchr( s, '\n' );
		if( p )
			*p = 0;

		Cmd_TokenizeString( s, false );
		line++;

		if(Cmd_Argc() < 4)
		{
			if(Cmd_Argc() > 0)
				Com_Printf( "CL_LoadLoc: line %i uncompleted\n", line );
		}
		else
		{
			origin[0] = (float)atof(Cmd_Argv(0)) * 0.125f;
			origin[1] = (float)atof(Cmd_Argv(1)) * 0.125f;
			origin[2] = (float)atof(Cmd_Argv(2)) * 0.125f;
			CL_AddLoc(origin, Cmd_ArgsFrom(3));
			count++;
		}

		if( !p )
			break;

		s = p + 1;
	}

	Com_Printf("Loaded %i locations from '%s'\n", count, fileName);

	FS_FreeFile( buffer );
}


static cl_location_t *CL_Loc_Get (const vec3_t org)
{
	unsigned int	length, bestlength = 0xFFFFFFFF;
	cl_location_t	*loc, *best = cl_locations;

	for(loc = cl_locations; loc; loc = loc->next)
	{
		length = (unsigned int)Distance(loc->location, org);
		if (length < bestlength) {
			best = loc;
			bestlength = length;
		}
	}

	return best;
}


void CL_AddViewLocs(void)
{
	cl_location_t *loc, *nearestLoc;
	entity_t ent;
	unsigned int dist;

	if (!cl_drawlocs->integer || !cl_locations)
		return;

	memset( &ent, 0, sizeof(ent) );
	ent.skin = NULL;
	ent.model = NULL;
#ifdef GL_QUAKE
	AxisClear(ent.axis);
#endif

	nearestLoc = CL_Loc_Get(cl.refdef.vieworg);

	for(loc = cl_locations; loc; loc = loc->next)
	{
		dist = (int)Distance(loc->location, cl.refdef.vieworg);
		if (dist > 16000)
			continue;

		VectorCopy(loc->location, ent.origin);

		if (loc == nearestLoc)
			ent.origin[2] += (float)sin(cl.time * 0.01f) * 10.0f;

		V_AddEntity(&ent);
	}
}

/*
=============================================
			LOC COMMANDS
=============================================
*/
static void CL_LocList_f(void)
{
	const cl_location_t	*loc;
	int i;

	if (!cl_locations) {
		Com_Printf("No locations found\n");
		return;
	}

	if (cls.state != ca_active) {
		Com_Printf("Must be in level to use this command\n");
		return;
	}

	for(loc = cl_locations, i = 1; loc; loc = loc->next, i++)
		Com_Printf("Location: %2i. at (%d, %d, %d) = %s\n", i, (int)loc->location[0], (int)loc->location[1], (int)loc->location[2], loc->name);

}

static void CL_LocAdd_f(void)
{
	if(Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <label>\n", Cmd_Argv(0));
		return;
	}

	if (cls.state != ca_active) {
		Com_Printf("Must be in level to use this command\n");
		return;
	}

	CL_AddLoc(cl.refdef.vieworg, Cmd_Args());
	Com_Printf("Location '%s' added at (%d, %d, %d).\n", Cmd_Args(), (int)cl.refdef.vieworg[0]*8, (int)cl.refdef.vieworg[1]*8, (int)cl.refdef.vieworg[2]*8);
}

static void CL_LocDel_f(void)
{
	cl_location_t	*loc, *entry, **back;

	if (!cl_locations) {
		Com_Printf("No locations found\n");
		return;
	}

	if (cls.state != ca_active) {
		Com_Printf("Must be in level to use this command\n");
		return;
	}

	entry = CL_Loc_Get(cl.refdef.vieworg);
	back = &cl_locations;
	while(1)
	{
		loc = *back;
		if (!loc) {
			Com_Printf ("Cant find location.\n");
			return;
		}
		if(loc == entry) {
			*back = loc->next;
			Com_Printf ("Removed location (%d, %d, %d) = %s\n", (int)loc->location[0], (int)loc->location[1], (int)loc->location[2], loc->name);
			Z_Free (loc);
			return;
		}
		back = &loc->next;
	}
}

static void CL_LocSave_f (void)
{
	const cl_location_t	*loc;
	FILE *f;
	char fileName[MAX_OSPATH];
	int count = 0;

	if (!cl_locations) {
		Com_Printf("No locations what to write\n");
		return;
	}

	if (cls.state != ca_active) {
		Com_Printf("Must be in level to use this command\n");
		return;
	}

	if(Cmd_Argc() == 2) {
		Com_sprintf (fileName, sizeof(fileName), "%s/%s/%s.loc", FS_Gamedir(), locDir, Cmd_Argv(1));
	} else {
		Com_sprintf (fileName, sizeof(fileName), "%s/%s/%s.loc", FS_Gamedir(), locDir, cls.mapname);
	}

	FS_CreatePath(fileName);

	f = fopen(fileName, "wb");
	if (!f) {
		Com_Printf("Warning: Unable to open %s for writing.\n", fileName);
		return;
	}

	for(loc = cl_locations; loc; loc = loc->next) {
		fprintf (f, "%i %i %i %s\n", (int)(loc->location[0]*8), (int)(loc->location[1]*8), (int)(loc->location[2]*8), loc->name);
		count++;
	}

	fclose(f);
	Com_Printf("%i locations saved to '%s'.\n", count, fileName);
}

static void CL_LocHere_m( char *buffer, int bufferSize )
{
	const cl_location_t	*loc;

	if (!cl_locations || cls.state != ca_active) {
		Q_strncpyz ( buffer, "%L", bufferSize );
		return;
	}

	loc = CL_Loc_Get(cl.refdef.vieworg);
	Q_strncpyz(buffer, loc->name, bufferSize);

}

static void CL_LocThere_m( char *buffer, int bufferSize )
{
	const cl_location_t	*loc;
	trace_t		tr;
	vec3_t		end;

	if (!cl_locations || cls.state != ca_active) {
		Q_strncpyz(buffer, "%S", bufferSize);
		return;
	}

	end[0] = cl.refdef.vieworg[0] + cl.v_forward[0] * 65556.0f + cl.v_right[0];
	end[1] = cl.refdef.vieworg[1] + cl.v_forward[1] * 65556.0f + cl.v_right[1];
	end[2] = cl.refdef.vieworg[2] + cl.v_forward[2] * 65556.0f + cl.v_right[2];

	tr = CM_BoxTrace(cl.refdef.vieworg, end, vec3_origin, vec3_origin, 0, MASK_SOLID);

	if (tr.fraction != 1.0f)
		loc = CL_Loc_Get(tr.endpos);
	else
		loc = CL_Loc_Get(end);

	Q_strncpyz(buffer, loc->name, bufferSize);

}

static void OnChange_LocDir(cvar_t *self, const char *oldValue)
{
	Q_strncpyz(locDir, self->string, sizeof(locDir));
	COM_FixPath(locDir);

	CL_LoadLoc();
}

/*
==============
LOC_Init
==============
*/
void CL_InitLocs( void )
{
	cl_drawlocs = Cvar_Get("cl_drawlocs", "0", 0);
	loc_dir = Cvar_Get("loc_dir", "locs", 0);
	loc_dir->OnChange = OnChange_LocDir;
	OnChange_LocDir(loc_dir, loc_dir->resetString);

	Cmd_AddCommand ("loc_add", CL_LocAdd_f);
	Cmd_AddCommand ("loc_list", CL_LocList_f);
	Cmd_AddCommand ("loc_save", CL_LocSave_f);
	Cmd_AddCommand ("loc_del", CL_LocDel_f);

	Cmd_AddMacro( "loc_here", CL_LocHere_m );
	Cmd_AddMacro( "loc_there", CL_LocThere_m );
}

