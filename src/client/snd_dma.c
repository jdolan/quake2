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
// snd_dma.c -- main control for any streaming sound output device

#include "client.h"
#include "snd_loc.h"

static void S_Play_f(void);
static void S_SoundList_f(void);
static void S_Update_(void);


// =======================================================================
// Internal sound data & structures
// =======================================================================

// only begin attenuating sound volumes when outside the FULLVOLUME range
#define		SOUND_FULLVOLUME	80

#define		SOUND_LOOPATTENUATE	0.003f

static int	s_registration_sequence;

channel_t   channels[MAX_CHANNELS];

qboolean	sound_started = false;

dma_t		dma;

static vec3_t		listener_origin;
static vec3_t		listener_forward;
static vec3_t		listener_right;
static vec3_t		listener_up;

static qboolean	s_registering;

int			soundtime;		// sample PAIRS
int   		paintedtime; 	// sample PAIRS

// during registration it is possible to have more sounds
// than could actually be referenced during gameplay,
// because we don't want to free anything until we are
// sure we won't need it.
#define		MAX_SFX		(MAX_SOUNDS*2)
static sfx_t		known_sfx[MAX_SFX];
static int			num_sfx;

#define SND_HASH_SIZE 32
static sfx_t *sfx_hash[SND_HASH_SIZE];

#define		MAX_PLAYSOUNDS	128
static playsound_t	s_playsounds[MAX_PLAYSOUNDS];
static playsound_t	s_freeplays;
playsound_t	s_pendingplays;

static int			s_beginofs;

cvar_t		*s_volume;
cvar_t		*s_testsound;
cvar_t		*s_loadas8bit;
cvar_t		*s_khz;
cvar_t		*s_show;
cvar_t		*s_mixahead;
cvar_t		*s_primary;
cvar_t		*s_swapstereo;
cvar_t		*s_ambient;
cvar_t		*s_oldresample;


int		s_rawend;
portable_samplepair_t	s_rawsamples[MAX_RAW_SAMPLES];

#ifdef USE_OPENAL
cvar_t *s_openal_device;
cvar_t *s_openal_dopplerFactor;
cvar_t *s_openal_dopplerVelocity;
cvar_t *s_openal_driver;
cvar_t *s_openal_errorCheck;
cvar_t *s_openal_maxDistance;
cvar_t *s_openal_rollOffFactor;
qboolean alSound = false;
#endif
qboolean snd_isActive = true;

// ====================================================================
// User-setable variables
// ====================================================================
void Snd_Activate (qboolean active)
{
	if (!sound_started)
		return;

	snd_isActive = active;

#ifdef USE_OPENAL
	if(alSound)
		ALSnd_Activate (active);
#endif
}

static void S_SoundInfo_f(void)
{
	if (!sound_started) {
		Com_Printf ("S_SoundInfo_f: sound system not started\n");
		return;
	}

#ifdef USE_OPENAL
	if(alSound) {
		ALSnd_SoundInfo();
		return;
	}
#endif
    Com_Printf("%5d stereo\n", dma.channels - 1);
    Com_Printf("%5d samples\n", dma.samples);
    Com_Printf("%5d samplepos\n", dma.samplepos);
    Com_Printf("%5d samplebits\n", dma.samplebits);
    Com_Printf("%5d submission_chunk\n", dma.submission_chunk);
    Com_Printf("%5d speed\n", dma.speed);
    Com_Printf("0x%x dma buffer\n", dma.buffer);
}

/*
================
S_Init
================
*/
void S_Init (void)
{
	cvar_t	*cv;

	Com_Printf("\n------- sound initialization -------\n");

	Cvar_Subsystem( CVAR_SYSTEM_SOUND );
	cv = Cvar_Get ("s_initsound", "1", CVAR_LATCHED);
	if (!cv->integer) {
		Com_Printf ("not initializing.\n");
		Com_Printf("------------------------------------\n");
		Cvar_Subsystem( CVAR_SYSTEM_GENERIC );
		return;
	}

	s_volume = Cvar_Get ("s_volume", "0.7", CVAR_ARCHIVE);
	s_khz = Cvar_Get ("s_khz", "22", CVAR_ARCHIVE|CVAR_LATCHED);
	s_loadas8bit = Cvar_Get ("s_loadas8bit", "0", CVAR_ARCHIVE|CVAR_LATCHED);
	s_mixahead = Cvar_Get ("s_mixahead", "0.2", CVAR_ARCHIVE);
	s_show = Cvar_Get ("s_show", "0", 0);
	s_testsound = Cvar_Get ("s_testsound", "0", 0);
	s_primary = Cvar_Get ("s_primary", "0", CVAR_ARCHIVE|CVAR_LATCHED);	// win32 specific

	s_swapstereo = Cvar_Get( "s_swapstereo", "0", CVAR_ARCHIVE );
	s_ambient = Cvar_Get ("s_ambient", "1", 0);
	s_oldresample = Cvar_Get ("s_oldresample", "0", CVAR_LATCHED);

#ifdef USE_OPENAL
	s_openal_device				= Cvar_Get("s_openal_device",				"",				CVAR_ARCHIVE|CVAR_LATCHED);
	s_openal_dopplerFactor		= Cvar_Get("s_openal_dopplerfactor",		"1",			CVAR_ARCHIVE);
	s_openal_dopplerVelocity	= Cvar_Get("s_openal_dopplervelocity",		"16384",		CVAR_ARCHIVE);
	s_openal_driver				= Cvar_Get("s_openal_driver",				AL_DRIVERNAME,	CVAR_ARCHIVE|CVAR_LATCHED);
	s_openal_errorCheck			= Cvar_Get("s_openal_errorcheck",			"0",			CVAR_ARCHIVE);
	s_openal_maxDistance		= Cvar_Get("s_openal_maxdistance",			"8192",			CVAR_ARCHIVE);
	s_openal_rollOffFactor		= Cvar_Get("s_openal_rollofffactor",		"1",			CVAR_ARCHIVE);
#endif

	Cvar_Subsystem( CVAR_SYSTEM_GENERIC );

#ifdef USE_OPENAL
	alSound = false;
	if (cv->integer == 2) {
		if(ALSnd_Init()) {
			alSound = true;
		} else {
			Com_Printf("Falling back to to normal sound\n");
		}
	}

	s_registration_sequence = 1;

	if (alSound || SNDDMA_Init())
#else
	if (SNDDMA_Init())
#endif
	{
		S_InitScaletable ();

		sound_started = true;
		num_sfx = 0;

		soundtime = 0;
		paintedtime = 0;

		Com_Printf ("sound sampling rate: %i\n", dma.speed);

		S_StopAllSounds ();

		Cmd_AddCommand("play", S_Play_f);
		Cmd_AddCommand("stopsound", S_StopAllSounds);
		Cmd_AddCommand("soundlist", S_SoundList_f);
		Cmd_AddCommand("soundinfo", S_SoundInfo_f);
	}

	Com_Printf("------------------------------------\n");
}


// =======================================================================
// Shutdown sound engine
// =======================================================================

void S_FreeAllSounds (void)
{
	int		i;
	sfx_t	*sfx;

	if (!sound_started)
		return;

#ifdef USE_OPENAL
	if (alSound)
		ALSnd_StopAllSounds();
#endif
	// free all sounds
	for (i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++)
	{
		if (!sfx->name[0])
			continue;
		if(sfx->cache)
		{
#ifdef USE_OPENAL
			if(alSound)
				ALSnd_DeleteBuffer(sfx->cache);
#endif
			Z_Free (sfx->cache);
		}
		if(sfx->truename)
			Z_Free(sfx->truename);
	}
	memset(known_sfx, 0, sizeof(known_sfx));
	memset(sfx_hash, 0, sizeof(sfx_hash));
	num_sfx = 0;
}

void S_Shutdown(void)
{
	if (!sound_started)
		return;

	Cmd_RemoveCommand("play");
	Cmd_RemoveCommand("stopsound");
	Cmd_RemoveCommand("soundlist");
	Cmd_RemoveCommand("soundinfo");

	S_FreeAllSounds();

#ifdef USE_OPENAL
	if(alSound)
	{
		ALSnd_Shutdown();
		alSound = false;
	}
	else
#endif
	{
		SNDDMA_Shutdown();
	}

	sound_started = false;
}


// =======================================================================
// Load a sound
// =======================================================================

/*
==================
S_FindName

==================
*/
static sfx_t *S_FindName (const char *name, qboolean create)
{
	int		i;
	unsigned int hash;
	sfx_t	*sfx;

	if (!name)
		Com_Error( ERR_FATAL, "S_FindName: NULL" );

	if (!name[0])
		Com_Error( ERR_DROP, "S_FindName: empty name" );

	hash = Com_HashKey(name, SND_HASH_SIZE);
	// see if already loaded
	for (sfx = sfx_hash[hash]; sfx; sfx = sfx->hashNext) {
		if (!strcmp(sfx->name, name)) {
			sfx->registration_sequence = s_registration_sequence;
			return sfx;
		}
	}

	if (!create)
		return NULL;

	// find a free sfx
	for (i = 0; i < num_sfx; i++) {
		if (!known_sfx[i].name[0])
			break;
	}

	if (i == num_sfx) {
		if (num_sfx == MAX_SFX)
			Com_Error (ERR_FATAL, "S_FindName: out of sfx_t");
		num_sfx++;
	}
	
	sfx = &known_sfx[i];
	//memset (sfx, 0, sizeof(*sfx));
	sfx->cache = NULL;
	sfx->truename = NULL;
	Q_strncpyz(sfx->name, name, sizeof(sfx->name));
	sfx->registration_sequence = s_registration_sequence;
	
	sfx->hashNext = sfx_hash[hash];
	sfx_hash[hash] = sfx;

	return sfx;
}


/*
==================
S_AliasName

==================
*/
static sfx_t *S_AliasName (const char *aliasname, const char *truename)
{
	sfx_t	*sfx;
	int		i;
	unsigned int	hash;

	// find a free sfx
	for (i = 0; i < num_sfx; i++) {
		if (!known_sfx[i].name[0])
			break;
	}

	if (i == num_sfx) {
		if (num_sfx == MAX_SFX)
			Com_Error (ERR_FATAL, "S_AliasName: out of sfx_t");
		num_sfx++;
	}
	
	sfx = &known_sfx[i];
	//memset (sfx, 0, sizeof(*sfx));
	sfx->cache = NULL;
	strcpy(sfx->name, aliasname);
	sfx->registration_sequence = s_registration_sequence;
	sfx->truename = CopyString (truename, TAG_CL_SFX);

	hash = Com_HashKey(aliasname, SND_HASH_SIZE);
	sfx->hashNext = sfx_hash[hash];
	sfx_hash[hash] = sfx;

	return sfx;
}


/*
=====================
S_BeginRegistration

=====================
*/
void S_BeginRegistration (void)
{
	s_registration_sequence++;
	s_registering = true;
}

/*
==================
S_RegisterSound

==================
*/
sfx_t *S_RegisterSound (const char *name)
{
	sfx_t	*sfx;

	if (!sound_started)
		return NULL;

	sfx = S_FindName (name, true);

	if (!s_registering)
		S_LoadSound(sfx);

	return sfx;
}

/*
=====================
S_EndRegistration

=====================
*/
void S_EndRegistration (void)
{
	int		i;
	sfx_t	*sfx, *entry, **back;
	int		size;
	unsigned int hash;

	// free any sounds not from this registration sequence
	for (i=0, sfx=known_sfx ; i < num_sfx ; i++,sfx++)
	{
		if (!sfx->name[0])
			continue;
		if (sfx->registration_sequence == s_registration_sequence) {
#ifdef USE_OPENAL
			if(sfx->cache && !alSound) { // make sure it is paged in
#else
			if(sfx->cache) { // make sure it is paged in
#endif
				size = sfx->cache->length * sfx->cache->width * sfx->cache->channels;
				Com_PageInMemory( (byte *)sfx->cache, size);
			}
			continue;
		}

		hash = Com_HashKey (sfx->name, SND_HASH_SIZE);
		// delete it from hash table
		for( back=&sfx_hash[hash], entry=sfx_hash[hash]; entry; back=&entry->hashNext, entry=entry->hashNext ) {
			if( entry == sfx ) {
				*back = entry->hashNext;
				break;
			}
		}

		if (sfx->cache)	{ // it is possible to have a leftover
#ifdef USE_OPENAL
			if(alSound)
				ALSnd_DeleteBuffer(sfx->cache);
#endif
			Z_Free (sfx->cache);	// from a server that didn't finish loading
		}
		if (sfx->truename)
			Z_Free (sfx->truename);

		memset (sfx, 0, sizeof(*sfx));
	}

	// load everything in
	for (i=0, sfx=known_sfx ; i < num_sfx ; i++,sfx++)
	{
		if (!sfx->name[0])
			continue;
		S_LoadSound (sfx);
	}

	s_registering = false;
}


//=============================================================================

/*
=================
S_PickChannel
=================
*/
static channel_t *S_PickChannel(int entnum, int entchannel)
{
    int			ch_idx;
    int			first_to_die = -1;
    int			life_left = 0x7fffffff;
	channel_t	*ch;

	if (entchannel < 0)
		Com_Error(ERR_DROP, "S_PickChannel: entchannel < 0");

// Check for replacement sound, or find the best one to replace
    for (ch_idx=0 ; ch_idx < MAX_CHANNELS ; ch_idx++)
    {
		if (entchannel != 0		// channel 0 never overrides
		&& channels[ch_idx].entnum == entnum
		&& channels[ch_idx].entchannel == entchannel)
		{	// always override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (channels[ch_idx].entnum == cl.playernum+1 && entnum != cl.playernum+1 && channels[ch_idx].sfx)
			continue;

		if (channels[ch_idx].end - paintedtime < life_left)
		{
			life_left = channels[ch_idx].end - paintedtime;
			first_to_die = ch_idx;
		}
   }

	if (first_to_die == -1)
		return NULL;

	ch = &channels[first_to_die];
	memset (ch, 0, sizeof(*ch));

    return ch;
}       

/*
=================
S_SpatializeOrigin

Used for spatializing channels and autosounds
=================
*/
static void S_SpatializeOrigin (const vec3_t origin, float master_vol, float dist_mult, int *left_vol, int *right_vol)
{
    vec_t		dot, dist;
    vec_t		lscale, rscale, vol;
    vec3_t		source_vec;

	if (cls.state != ca_active) {
		*left_vol = *right_vol = 255;
		return;
	}

// calculate stereo seperation and distance attenuation
	VectorSubtract(origin, listener_origin, source_vec);

	dist = VectorNormalize(source_vec);
	dist -= SOUND_FULLVOLUME;
	if (dist < 0)
		dist = 0;			// close enough to be at full volume
	else
		dist *= dist_mult;		// different attenuation levels
	
	if (dma.channels == 1 || !dist_mult)
	{ // no attenuation = no spatialization
		rscale = 1.0f;
		lscale = 1.0f;
	}
	else
	{
		dot = DotProduct(listener_right, source_vec);
		rscale = 0.5f * (1.0f + dot);
		lscale = 0.5f * (1.0f - dot);
	}

	// add in distance effect
	vol = master_vol * (1.0f - dist);
	*right_vol = (int) (vol * rscale);
	if (*right_vol < 0)
		*right_vol = 0;

	*left_vol = (int) (vol * lscale);
	if (*left_vol < 0)
		*left_vol = 0;
}

/*
=================
S_Spatialize
=================
*/
static void S_Spatialize(channel_t *ch)
{
	vec3_t		origin;

	// anything coming from the view entity will always be full volume
	if (ch->entnum == cl.playernum+1) {
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		return;
	}

	if (ch->fixed_origin)
		VectorCopy (ch->origin, origin);
	else
		CL_GetEntitySoundOrigin (ch->entnum, origin);

	S_SpatializeOrigin (origin, ch->master_vol, ch->dist_mult, &ch->leftvol, &ch->rightvol);
}           


/*
=================
S_AllocPlaysound
=================
*/
static playsound_t *S_AllocPlaysound (void)
{
	playsound_t	*ps;

	ps = s_freeplays.next;
	if (ps == &s_freeplays)
		return NULL;		// no free playsounds

	// unlink from freelist
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;
	
	return ps;
}


/*
=================
S_FreePlaysound
=================
*/
void S_FreePlaysound (playsound_t *ps)
{
	// unlink from channel
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;

	// add to free list
	ps->next = s_freeplays.next;
	s_freeplays.next->prev = ps;
	ps->prev = &s_freeplays;
	s_freeplays.next = ps;
}



/*
===============
S_IssuePlaysound

Take the next playsound and begin it on the channel
This is never called directly by S_Play*, but only
by the update loop.
===============
*/
void S_IssuePlaysound (playsound_t *ps)
{
	channel_t	*ch;
	sfxcache_t	*sc;

	if (s_show->integer)
		Com_Printf ("Issue %i\n", ps->begin);
	// pick a channel to play on
	ch = S_PickChannel(ps->entnum, ps->entchannel);
	if (!ch) {
		S_FreePlaysound(ps);
		return;
	}

	sc = S_LoadSound(ps->sfx);
	if (!sc || sc->length <= 0) {
		S_FreePlaysound(ps);
		return;
	}

	// spatialize
	if (ps->attenuation == ATTN_STATIC)
		ch->dist_mult = ps->attenuation * 0.001f;
	else
		ch->dist_mult = ps->attenuation * 0.0005f;
	ch->master_vol = ps->volume;
	ch->entnum = ps->entnum;
	ch->entchannel = ps->entchannel;
	ch->sfx = ps->sfx;
	VectorCopy (ps->origin, ch->origin);
	ch->fixed_origin = ps->fixed_origin;

	S_Spatialize(ch);

	ch->pos = 0;
    ch->end = paintedtime + sc->length;

	// free the playsound
	S_FreePlaysound(ps);
}

static struct sfx_s *S_RegisterSexedSound (int entnum, const char *base)
{
	int				n;
	char			*p;
	struct sfx_s	*sfx;
	char			model[MAX_QPATH];
	char			sexedFilename[MAX_QPATH];
	char			maleFilename[MAX_QPATH];

	// determine what model the client is using
	model[0] = 0;
	n = CS_PLAYERSKINS + entnum - 1;
	if (cl.configstrings[n][0])
	{
		p = strchr(cl.configstrings[n], '\\');
		if (p) {
			Q_strncpyz(model, p + 1, sizeof(model));
			p = strchr(model, '/');
			if (p)
				*p = 0;
		}
	}
	// if we can't figure it out, they're male
	if (!model[0])
		strcpy(model, "male");

	// see if we already know of the model specific sound
	Com_sprintf (sexedFilename, sizeof(sexedFilename), "#players/%s/%s", model, base+1);
	sfx = S_FindName (sexedFilename, false);

	if (!sfx) {
		// no, so see if it exists
		if (FS_LoadFile(sexedFilename + 1, NULL) > 0) {
			// yes, close the file and register it
			sfx = S_RegisterSound(sexedFilename);
		}
		else {
			// no, revert to the male sound in the pak0.pak
			Com_sprintf(maleFilename, sizeof(maleFilename), "player/%s/%s", "male", base+1);
			sfx = S_AliasName(sexedFilename, maleFilename);
		}
	}

	return sfx;
}


// =======================================================================
// Start a sound effect
// =======================================================================

/*
====================
S_StartSound

Validates the parms and ques the sound up
if pos is NULL, the sound will be dynamically sourced from the entity
Entchannel 0 will never override a playing sound
====================
*/
void S_StartSound(const vec3_t origin, int entnum, int entchannel, sfx_t *sfx, float fvol, float attenuation, float timeofs)
{
	sfxcache_t	*sc;
	playsound_t	*ps, *sort;
	int			start;

	if (!sound_started)
		return;

	if (!sfx)
		return;

	if (sfx->name[0] == '*') {
		if(entnum < 1 || entnum >= MAX_EDICTS ) {
			Com_Error( ERR_DROP, "S_StartSound: bad entnum: %d", entnum );
		}
		sfx = S_RegisterSexedSound(cl_entities[entnum].current.number, sfx->name);
	}

	// make sure the sound is loaded
	sc = S_LoadSound (sfx);
	if (!sc || sc->length <= 0)
		return;		// couldn't load the sound's data

	// make the playsound_t
	ps = S_AllocPlaysound ();
	if (!ps)
		return;

	if (origin) {
		VectorCopy (origin, ps->origin);
		ps->fixed_origin = true;
	}
	else
		ps->fixed_origin = false;

	ps->entnum = entnum;
	ps->entchannel = entchannel;
	ps->attenuation = attenuation;
	ps->sfx = sfx;

#ifdef USE_OPENAL
	if(alSound) {
		ps->volume = fvol*fvol;
		ps->begin = cl.time + (int)(timeofs * 1000);
	}
	else
#endif
	{
		ps->volume = fvol*255;
		// drift s_beginofs
		start = cl.serverTime * 0.001f * dma.speed + s_beginofs;
		if (start < paintedtime)
		{
			start = paintedtime;
			s_beginofs = start - (cl.serverTime * 0.001f * dma.speed);
		}
		else if (start > paintedtime + 0.3f * dma.speed)
		{
			start = paintedtime + 0.1f * dma.speed;
			s_beginofs = start - (cl.serverTime * 0.001f * dma.speed);
		}
		else
		{
			s_beginofs -= 10;
		}

		if (!timeofs)
			ps->begin = paintedtime;
		else
			ps->begin = start + timeofs * dma.speed;
	}

	// sort into the pending sound list
	for (sort = s_pendingplays.next;
		sort != &s_pendingplays && sort->begin < ps->begin ;
		sort = sort->next)
			;

	ps->next = sort;
	ps->prev = sort->prev;

	ps->next->prev = ps;
	ps->prev->next = ps;
}


/*
==================
S_StartLocalSound
==================
*/
void S_StartLocalSound (const char *sound)
{
	sfx_t	*sfx;

	if (!sound_started)
		return;
		
	sfx = S_RegisterSound (sound);
	if (!sfx) {
		Com_Printf ("S_StartLocalSound: can't cache %s\n", sound);
		return;
	}
	S_StartSound (NULL, cl.playernum+1, 0, sfx, 1, ATTN_NONE, 0);
}


/*
==================
S_ClearBuffer
==================
*/
static void S_ClearBuffer (void)
{
	int		clear;
		
	if (!sound_started)
		return;

#ifdef USE_OPENAL
	if (alSound)
		return;
#endif

	s_rawend = 0;

	if (dma.samplebits == 8)
		clear = 0x80;
	else
		clear = 0;

	SNDDMA_BeginPainting ();

	if (dma.buffer)
		Snd_Memset(dma.buffer, clear, dma.samples * dma.samplebits/8);

	SNDDMA_Submit ();
}

/*
==================
S_StopAllSounds
==================
*/
void S_StopAllSounds(void)
{
	int		i;

	if (!sound_started)
		return;

	// clear all the playsounds
	memset(s_playsounds, 0, sizeof(s_playsounds));
	s_freeplays.next = s_freeplays.prev = &s_freeplays;
	s_pendingplays.next = s_pendingplays.prev = &s_pendingplays;

	for (i=0 ; i<MAX_PLAYSOUNDS ; i++)
	{
		s_playsounds[i].prev = &s_freeplays;
		s_playsounds[i].next = s_freeplays.next;
		s_playsounds[i].prev->next = &s_playsounds[i];
		s_playsounds[i].next->prev = &s_playsounds[i];
	}

#ifdef USE_OPENAL
	if (alSound)
		ALSnd_StopAllSounds();
#endif
	// clear all the channels
	memset(channels, 0, sizeof(channels));

	S_ClearBuffer ();
}

/*
==================
S_AddLoopSounds

Entities with a ->sound field will generated looped sounds
that are automatically started, stopped, and merged together
as the entities are sent to the client
==================
*/
static void S_AddLoopSounds (void)
{
	int			i, j;
	int			sounds[MAX_EDICTS];
	int			left, right, left_total, right_total;
	channel_t	*ch;
	sfx_t		*sfx;
	sfxcache_t	*sc;
	int			num;
	entity_state_t	*ent;
	vec3_t		origin;

	if (cl_paused->integer || cls.state != ca_active || !cl.sound_prepped)
		return;

	for (i=0 ; i<cl.frame.num_entities ; i++)
	{
		num = (cl.frame.parse_entities + i) & PARSE_ENTITIES_MASK;
		ent = &cl_parse_entities[num];
		sounds[i] = ent->sound;
	}

	for (i=0 ; i<cl.frame.num_entities ; i++)
	{
		if (!sounds[i])
			continue;

		sfx = cl.sound_precache[sounds[i]];
		if (!sfx)
			continue;		// bad sound effect
		sc = sfx->cache;
		if (!sc || sc->length <= 0)
			continue;

		num = (cl.frame.parse_entities + i) & PARSE_ENTITIES_MASK;
		ent = &cl_parse_entities[num];

		CL_GetEntitySoundOrigin (ent->number, origin);
		// find the total contribution of all sounds of this type
		S_SpatializeOrigin (origin, 255.0f, SOUND_LOOPATTENUATE,
			&left_total, &right_total);
		for (j=i+1 ; j<cl.frame.num_entities ; j++)
		{
			if (sounds[j] != sounds[i])
				continue;
			sounds[j] = 0;	// don't check this again later

			num = (cl.frame.parse_entities + j) & PARSE_ENTITIES_MASK;
			ent = &cl_parse_entities[num];

			S_SpatializeOrigin (ent->origin, 255.0f, SOUND_LOOPATTENUATE, 
				&left, &right);
			left_total += left;
			right_total += right;
		}

		if (left_total == 0 && right_total == 0)
			continue;		// not audible

		// allocate a channel
		ch = S_PickChannel(0, 0);
		if (!ch)
			return;

		if (left_total > 255)
			left_total = 255;
		if (right_total > 255)
			right_total = 255;
		ch->leftvol = left_total;
		ch->rightvol = right_total;
		ch->autosound = true;	// remove next frame
		ch->sfx = sfx;
		ch->pos = paintedtime % sc->length;
		ch->end = paintedtime + sc->length - ch->pos;
	}
}

//=============================================================================

/*
============
S_RawSamples

Cinematic streaming and voice over network
============
*/
void S_RawSamples (int samples, int rate, int width, int nchannels, byte *data)
{
	int			snd_vol;
	unsigned	src, dst;
	unsigned	fracstep, samplefrac;

	if (!sound_started)
		return;

#ifdef USE_OPENAL
	if (alSound)
		return;
#endif
	if (s_rawend < paintedtime)
		s_rawend = paintedtime;


	snd_vol = (int)(s_volume->value * 256);
	if( snd_vol < 0 )
		snd_vol = 0;

	fracstep = ((unsigned)rate << 8) / (unsigned)dma.speed;
	samplefrac = 0;

	if( width == 2 ) {
		int16 *in = (int16 *)data;

		if( nchannels == 2 ) {
			for( src = 0; src < samples; samplefrac += fracstep, src = (samplefrac >> 8) ) {
				dst = s_rawend++ & (MAX_RAW_SAMPLES - 1);
				s_rawsamples[dst].left = LittleShort( in[src*2] ) * snd_vol;
				s_rawsamples[dst].right = LittleShort( in[src*2+1] ) * snd_vol;
			}
		} else  {
			for( src = 0; src < samples; samplefrac += fracstep, src = (samplefrac >> 8) ) {
				dst = s_rawend++ & (MAX_RAW_SAMPLES - 1);
				s_rawsamples[dst].left = s_rawsamples[dst].right = LittleShort( in[src] ) * snd_vol;
			}
		}
	} else {
		if( nchannels == 2 ) {
			char *in = (char *)data;

			for( src = 0; src < samples; samplefrac += fracstep, src = (samplefrac >> 8) ) {
				dst = s_rawend++ & (MAX_RAW_SAMPLES - 1);
				s_rawsamples[dst].left = in[src*2] << 8 * snd_vol;
				s_rawsamples[dst].right = in[src*2+1] << 8 * snd_vol;
			}
		} else {
			for( src = 0; src < samples; samplefrac += fracstep, src = (samplefrac >> 8) ) {
				dst = s_rawend++ & (MAX_RAW_SAMPLES - 1);
				s_rawsamples[dst].left = s_rawsamples[dst].right = (data[src] - 128) << 8 * snd_vol;
			}
		}
	}
}

//=============================================================================

/*
============
S_Update

Called once each time through the main loop
============
*/
void S_Update(const vec3_t origin, const vec3_t forward, const vec3_t right, const vec3_t up)
{
	int			i, total;
	channel_t	*ch;

	if (!sound_started)
		return;

	// if the laoding plaque is up, clear everything
	// out to make sure we aren't looping a dirty
	// dma buffer while loading
	if (cls.disable_screen)
	{
		S_ClearBuffer ();
		return;
	}

#ifdef USE_OPENAL
	if (alSound) {
		ALSnd_Update(origin, vec3_origin, forward, up);
		return;
	}
#endif
	// rebuild scale tables if volume is modified
	if (s_volume->modified)
		S_InitScaletable ();

	VectorCopy(origin, listener_origin);
	VectorCopy(forward, listener_forward);
	VectorCopy(right, listener_right);
	VectorCopy(up, listener_up);

	// update spatialization for dynamic sounds	
	ch = channels;
	for (i=0 ; i<MAX_CHANNELS; i++, ch++)
	{
		if (!ch->sfx)
			continue;
		if (ch->autosound)
		{	// autosounds are regenerated fresh each frame
			memset (ch, 0, sizeof(*ch));
			continue;
		}
		S_Spatialize(ch);         // respatialize channel
		if (!ch->leftvol && !ch->rightvol)
		{
			memset (ch, 0, sizeof(*ch));
			continue;
		}
	}

	// add loopsounds
	if (s_ambient->integer)
		S_AddLoopSounds ();

	//
	// debugging output
	//
	if (s_show->integer)
	{
		total = 0;
		ch = channels;
		for (i=0 ; i<MAX_CHANNELS; i++, ch++) {
			if (ch->sfx && (ch->leftvol || ch->rightvol) ) {
				Com_Printf ("%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name);
				total++;
			}
		}
		Com_Printf ("----(%i)---- painted: %i\n", total, paintedtime);
	}

// mix some sound
	S_Update_();
}

void GetSoundtime(void)
{
	int		samplepos;
	static	int		buffers;
	static	int		oldsamplepos;
	int		fullsamples;
	
#ifdef AVI_EXPORT
	if (Movie_GetSoundtime())
		return;
#endif

	fullsamples = dma.samples / dma.channels;

// it is possible to miscount buffers if it has wrapped twice between
// calls to S_Update.  Oh well.
	samplepos = SNDDMA_GetDMAPos();

	if (samplepos < oldsamplepos)
	{
		buffers++;					// buffer wrapped
		
		if (paintedtime > 0x40000000)
		{	// time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds ();
		}
	}
	oldsamplepos = samplepos;

	soundtime = buffers*fullsamples + samplepos/dma.channels;
}


static void S_Update_(void)
{
	unsigned        endtime;
	int				samps;

	if (!sound_started)
		return;

	SNDDMA_BeginPainting ();

	if (!dma.buffer)
		return;

// Updates DMA time
	GetSoundtime();

// check to make sure that we haven't overshot
	if (paintedtime < soundtime)
	{
		//Com_DPrintf ("S_Update_ : overflow\n");
		paintedtime = soundtime;
	}

// mix ahead of current position
	endtime = soundtime + s_mixahead->value * dma.speed;
//endtime = (soundtime + 4096) & ~4095;

	// mix to an even submission block size
	endtime = (endtime + dma.submission_chunk-1)
		& ~(dma.submission_chunk-1);
	samps = dma.samples >> (dma.channels-1);
	if (endtime - soundtime > samps)
		endtime = soundtime + samps;

	S_PaintChannels (endtime);

	SNDDMA_Submit ();
}

/*
===============================================================================

console functions

===============================================================================
*/

static void S_Play_f (void)
{
	int 	i;
	char	name[MAX_QPATH];
	sfx_t	*sfx;
	
	for (i = 1; i < Cmd_Argc(); i++)
	{
		if (!strrchr(Cmd_Argv(i), '.')) {
			Q_strncpyz(name, Cmd_Argv(i), sizeof(name)-4);
			strcat(name, ".wav");
		}
		else
			Q_strncpyz (name, Cmd_Argv(i), sizeof(name));

		sfx = S_RegisterSound(name);
		if (sfx)
			S_StartSound(NULL, cl.playernum+1, 0, sfx, 1, 1, 0);
	}
}

static void S_SoundList_f (void)
{
	int		i, count = 0;
	sfx_t	*sfx;
	sfxcache_t	*sc;
	int		size, total = 0;

	for (sfx=known_sfx, i=0 ; i<num_sfx ; i++, sfx++)
	{
		if (!sfx->registration_sequence)
			continue;
		sc = sfx->cache;
		if (sc)
		{
			size = sc->length * sc->width * sc->channels;
			total += size;
			if (sc->loopstart >= 0)
				Com_Printf ("L");
			else
				Com_Printf (" ");
			Com_Printf("(%2db) %6i : %s\n", sc->width*8,  size, sfx->name);
		}
		else
		{
			if (sfx->name[0] == '*')
				Com_Printf("  placeholder : %s\n", sfx->name);
			else
				Com_Printf("  not loaded  : %s\n", sfx->name);
		}
		count++;
	}
	Com_Printf ("%i sounds\n", count);
	Com_Printf ("Total resident: %i\n", total);
}
