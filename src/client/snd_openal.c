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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

//
// snd_openal.c
//

#include "client.h"

#ifdef USE_OPENAL
#include "snd_loc.h"

#if defined(_WIN32)
# define WIN32_LEAN_AND_MEAN 1
# include <windows.h>

static HINSTANCE			snd_alLibrary = NULL;

# define AL_LOADLIB(a)		LoadLibrary (a)
# define AL_GPA(a)			GetProcAddress (snd_alLibrary, a)
# define AL_FREELIB(a)		FreeLibrary (a)
#else

# include <dlfcn.h>
# include <unistd.h>
# include <sys/types.h>

static void					*snd_alLibrary = NULL;

# define AL_LOADLIB(a)		dlopen (a, RTLD_LAZY|RTLD_GLOBAL)
# define AL_GPA(a)			dlsym (snd_alLibrary, a)
# define AL_FREELIB(a)		dlclose (a)
#endif

#define AL_NO_PROTOTYPES
#include "../include/al.h"
//#include <AL/al.h>

#define ALC_NO_PROTOTYPES
#include "../include/alc.h"
//#include <AL/alc.h>

static ALCdevice			*al_hDevice = NULL;
static ALCcontext			*al_hALC = NULL;

typedef struct audioAL_s {
	// Static information (never changes after initialization)
	const char	*extensionString;
	const char	*rendererString;
	const char	*vendorString;
	const char	*versionString;

	const char	*deviceName;

	int			numChannels;

	// Dynamic information
	int			frameCount;
} audioAL_t;

static audioAL_t			snd_audioAL;
static openal_channel_t		snd_alOutChannels[MAX_CHANNELS];

extern	qboolean snd_isActive;
extern	cvar_t *s_ambient;

extern 	cvar_t *s_openal_device;
extern 	cvar_t *s_openal_dopplerFactor;
extern 	cvar_t *s_openal_dopplerVelocity;
extern 	cvar_t *s_openal_driver;
extern 	cvar_t *s_openal_errorCheck;
extern 	cvar_t *s_openal_maxDistance;
extern 	cvar_t *s_openal_rollOffFactor;

#define AL_FUNC(type, func) \
static type q##func;
#include "openal_funcs.h"
#undef AL_FUNC

void ALSnd_RawShutdown (void);
/*
===========
ALSnd_CheckForError

Return true if there was an error.
===========
*/
static const char *GetALErrorString (ALenum error)
{
	switch (error) {
	case AL_NO_ERROR:			return "AL_NO_ERROR";
	case AL_INVALID_NAME:		return "AL_INVALID_NAME";
	case AL_INVALID_ENUM:		return "AL_INVALID_ENUM";
	case AL_INVALID_VALUE:		return "AL_INVALID_VALUE";
	case AL_INVALID_OPERATION:	return "AL_INVALID_OPERATION";
	case AL_OUT_OF_MEMORY:		return "AL_OUT_OF_MEMORY";
	}

	return "unknown";
}

static qboolean ALSnd_CheckForError (const char *from)
{
	ALenum	error;

	error = qalGetError ();
	if (error != AL_NO_ERROR) {
		Com_Printf ("AL_ERROR: alGetError (): '%s' (0x%x) %s\n", GetALErrorString (error), error, from ? from : "");
		return false;
	}

	return true;
}


/*
==================
ALSnd_Activate
==================
*/
void ALSnd_Activate (qboolean active)
{
	if (active)
		qalListenerf (AL_GAIN, s_volume->value);
	else
		qalListenerf (AL_GAIN, 0.0f);
}

/*
===============================================================================

	BUFFER MANAGEMENT

===============================================================================
*/

/*
==================
ALSnd_BufferFormat
==================
*/
static int ALSnd_BufferFormat (int width, int channels)
{
	if (width == 2)
		return (channels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;

	return (channels == 2) ? AL_FORMAT_STEREO8 : AL_FORMAT_MONO8;
}


/*
==================
ALSnd_CreateBuffer
==================
*/
void ALSnd_CreateBuffer (sfxcache_t *sc, int width, int channels, byte *data, int size, int frequency)
{
	int alFormat;

	if (!sc)
		return;

	// Find the format
	alFormat = ALSnd_BufferFormat (width, channels);

	// Upload
	qalGenBuffers (1, &sc->alBufferNum);
	qalBufferData (sc->alBufferNum, alFormat, data, size, frequency);

	// Check
	if (!qalIsBuffer (sc->alBufferNum))
		Com_Error (ERR_DROP, "ALSnd_CreateBuffer: created buffer is not valid!");
}


/*
==================
ALSnd_DeleteBuffer
==================
*/
void ALSnd_DeleteBuffer (sfxcache_t *sc)
{
	if (sc && sc->alBufferNum)
		qalDeleteBuffers (1, &sc->alBufferNum);
}

/*
===============================================================================

	SOUND PLAYING

===============================================================================
*/

/*
==================
ALSnd_StartChannel
==================
*/
static void ALSnd_StartChannel (openal_channel_t *ch, sfx_t *sfx)
{
	ch->sfx = sfx;

	qalSourcei (ch->sourceNum, AL_BUFFER, sfx->cache->alBufferNum);
	qalSourcei (ch->sourceNum, AL_LOOPING, ch->alLooping);
	if (ch->psType == PSND_LOCAL) {
		qalSourcei (ch->sourceNum, AL_SOURCE_RELATIVE, AL_TRUE);
		qalSourcefv (ch->sourceNum, AL_POSITION, vec3_origin);
		//qalSourcefv (ch->sourceNum, AL_VELOCITY, vec3_origin);
		qalSourcef (ch->sourceNum, AL_ROLLOFF_FACTOR, 0);
	}
	else
		qalSourcei (ch->sourceNum, AL_SOURCE_RELATIVE, AL_FALSE);

	qalSourcePlay (ch->sourceNum);
}


/*
==================
ALSnd_StopSound
==================
*/
static void ALSnd_StopSound (openal_channel_t *ch)
{
	if(ch->sfx)
		qalSourceStop (ch->sourceNum);
	ch->sfx = NULL;
	qalSourcei (ch->sourceNum, AL_BUFFER, 0);
}


/*
==================
ALSnd_StopAllSounds
==================
*/
void ALSnd_StopAllSounds (void)
{
	int		i;

	// Stop all sources
	for (i=0 ; i<snd_audioAL.numChannels ; i++) {
		if (snd_alOutChannels[i].sfx)
			ALSnd_StopSound (&snd_alOutChannels[i]);
	}

	// Stop raw streaming
	ALSnd_RawShutdown ();

	// Reset frame count
	snd_audioAL.frameCount = 0;
}

/*
===============================================================================

	SPATIALIZATION

===============================================================================
*/

/*
==================
ALSnd_SpatializeChannel

Updates volume, distance, rolloff, origin, and velocity for a channel
If it's a local sound only the volume is updated
==================
*/
void CL_GetEntitySoundVelocity (int ent, vec3_t velocity);

static void ALSnd_SpatializeChannel (openal_channel_t *ch)
{
	vec3_t	position;

	// Channel volume
	qalSourcef (ch->sourceNum, AL_GAIN, ch->volume);

	// Local sound
	if (ch->psType == PSND_LOCAL)
		return;

	// Distance, rolloff
	qalSourcef(ch->sourceNum, AL_REFERENCE_DISTANCE, 240.0f * ch->distanceMult);
	qalSourcef (ch->sourceNum, AL_MAX_DISTANCE, s_openal_maxDistance->value);
	qalSourcef (ch->sourceNum, AL_ROLLOFF_FACTOR, s_openal_rollOffFactor->value);

	// Fixed origin
	if (ch->psType == PSND_FIXED) {
		qalSource3f (ch->sourceNum, AL_POSITION, ch->origin[1], ch->origin[2], -ch->origin[0]);
		//qalSource3f (ch->sourceNum, AL_VELOCITY, 0, 0, 0);
		return;
	}

	// Entity origin
	if (ch->alLooping) {
		CL_GetEntitySoundOrigin (ch->alLoopEntNum, position);
		//CL_GetEntitySoundVelocity (ch->alLoopEntNum, velocity);
	}
	else {
		CL_GetEntitySoundOrigin (ch->entNum, position);
		//CL_GetEntitySoundVelocity (ch->entNum, velocity);
	}

	qalSource3f (ch->sourceNum, AL_POSITION, position[1], position[2], -position[0]);
	//qalSource3f (ch->sourceNum, AL_VELOCITY, velocity[1], velocity[2], -velocity[0]);
}

/*
===============================================================================

	CHANNELS

===============================================================================
*/

/*
=================
ALSnd_PickChannel
=================
*/
static openal_channel_t *ALSnd_PickChannel (int entNum, int entChannel)
{
	int			i;
	int			firstToDie;
	int			oldest;
	openal_channel_t	*ch;
	unsigned	sourceNum;

	firstToDie = -1;
	oldest = cl.time;

	// Check for replacement sound, or find the best one to replace;
	for (i=0, ch=snd_alOutChannels ; i<snd_audioAL.numChannels ; ch++, i++) {
		// Never take over raw stream channels
		if (ch->alRawStream)
			continue;

		// Free channel
		if (!ch->sfx) {
			firstToDie = i;
			break;
		}

		// Channel 0 never overrides
		if (entChannel != 0 && ch->entNum == entNum && ch->entChannel == entChannel) {
			// Always override sound from same entity
			firstToDie = i;
			break;
		}

		// Don't let monster sounds override player sounds
		if (ch->entNum == cl.playernum+1 && entNum != cl.playernum+1)
			continue;

		// Replace the oldest sound
		if (ch->startTime < oldest) {
			oldest = ch->startTime;
			firstToDie = i;
		}
   }

	if (firstToDie == -1)
		return NULL;

	ch = &snd_alOutChannels[firstToDie];
	// Stop the source
	ALSnd_StopSound (ch);
	sourceNum = ch->sourceNum;
	memset (ch, 0, sizeof (*ch));

	ch->entNum = entNum;
	ch->entChannel = entChannel;
	ch->startTime = cl.time;
	ch->sourceNum = sourceNum;

	return ch;
}

/*
===============================================================================

	PLAYSOUNDS

===============================================================================
*/

/*
===============
ALSnd_IssuePlaysounds
===============
*/
void S_FreePlaysound (playsound_t *ps);

static void ALSnd_IssuePlaysounds (void)
{
	playsound_t	*ps;
	openal_channel_t	*ch;

	for ( ; ; ) {
		ps = s_pendingplays.next;
		if (ps == &s_pendingplays)
			break;		// No more pending playSounds
		if (ps->begin > cl.time)
			break;		// No more pending playSounds this frame

		// Pick a channel to play on
		ch = ALSnd_PickChannel (ps->entnum, ps->entchannel);
		if (!ch) {
			S_FreePlaysound (ps);
			return;
		}
		// Spatialize
		ch->alLooping = false;
		ch->alRawStream = false;
		ch->volume = ps->volume;
		VectorCopy (ps->origin, ch->origin);

		// Convert to a local sound if it's not supposed to attenuation
		if (ps->attenuation == ATTN_NONE)
			ch->psType = PSND_LOCAL;
		else if (ps->fixed_origin)
			ch->psType = PSND_FIXED;
		else
			ch->psType = PSND_ENTITY;

		ch->distanceMult = 1.0f / ps->attenuation;

		ALSnd_SpatializeChannel (ch);
		ALSnd_StartChannel (ch, ps->sfx);

		// Free the playsound
		S_FreePlaysound(ps);
	}
}

/*
===============================================================================

	RAW SAMPLING

	Cinematic streaming and voice over network

===============================================================================
*/

/*
===========
ALSnd_RawStart
===========
*/
openal_channel_t *ALSnd_RawStart (void)
{
	openal_channel_t	*ch;

	ch = ALSnd_PickChannel (0, 0);
	if (!ch)
		Com_Error (ERR_FATAL, "ALSnd_RawStart: failed to allocate a source!");

	// Fill source values
	ch->psType = PSND_LOCAL;
	ch->alLooping = false;
	ch->alRawStream = true;
	ch->volume = 1;
	ch->sfx = NULL;

	// Spatialize
	qalSourcef (ch->sourceNum, AL_GAIN, 1);
	qalSourcei (ch->sourceNum, AL_BUFFER, 0);
	qalSourcei (ch->sourceNum, AL_LOOPING, AL_FALSE);

	// Local sound
	qalSourcei (ch->sourceNum, AL_SOURCE_RELATIVE, AL_TRUE);
	qalSourcefv (ch->sourceNum, AL_POSITION, vec3_origin);
	//qalSourcefv (ch->sourceNum, AL_VELOCITY, vec3_origin);
	qalSourcef (ch->sourceNum, AL_ROLLOFF_FACTOR, 0);

	return ch;
}


/*
============
ALSnd_RawSamples

Cinematic streaming and voice over network
============
*/
void ALSnd_RawSamples (struct openal_channel_s *rawChannel, int samples, int rate, int width, int channels, byte *data)
{
	ALuint	buffer;
	ALuint	format;

	if (!rawChannel || !rawChannel->alRawStream)
		return;

	// Find the format
	format = ALSnd_BufferFormat (width, channels);

	// Generate a buffer
	qalGenBuffers (1, &buffer);
	qalBufferData (buffer, format, data, (samples * width * channels), rate);

	// Place in queue
	qalSourceQueueBuffers (rawChannel->sourceNum, 1, &buffer);
}


/*
===========
ALSnd_RawStop
===========
*/
void ALSnd_RawStop (openal_channel_t *rawChannel)
{
	if (!rawChannel || !rawChannel->alRawStream)
		return;

	qalSourceStop (rawChannel->sourceNum);
	rawChannel->alRawPlaying = false;
	rawChannel->alRawStream = false;
}


/*
===========
ALSnd_RawShutdown
===========
*/
void ALSnd_RawShutdown (void)
{
	openal_channel_t	*ch;
	int			i;

	// Stop all raw streaming channels
	for (i=0, ch=snd_alOutChannels ; i<snd_audioAL.numChannels ; ch++, i++) {
		if (!ch->alRawStream)
			continue;

		ALSnd_RawStop (ch);
	}
}


/*
===========
ALSnd_RawUpdate
===========
*/
static void ALSnd_RawUpdate (openal_channel_t *rawChannel)
{
	int		processed;
	ALuint	buffer;
	ALint	state;

	if (!rawChannel || !rawChannel->alRawStream)
		return;

	// Delete processed buffers
	qalGetSourcei (rawChannel->sourceNum, AL_BUFFERS_PROCESSED, &processed);
	while (processed--) {
		qalSourceUnqueueBuffers (rawChannel->sourceNum, 1, &buffer);
		qalDeleteBuffers (1, &buffer);
	}

	// Start the queued buffers
	qalGetSourcei (rawChannel->sourceNum, AL_BUFFERS_QUEUED, &processed);
	qalGetSourcei (rawChannel->sourceNum, AL_SOURCE_STATE, &state);
	if (state == AL_STOPPED) {
		if (processed) {
			qalSourcePlay (rawChannel->sourceNum);
			rawChannel->alRawPlaying = true;
		}
		else if (!rawChannel->alRawPlaying)
			ALSnd_RawStop (rawChannel);
	}
}

/*
===============================================================================

	CHANNEL MIXING

===============================================================================
*/

/*
===========
ALSnd_AddLoopSounds
===========
*/
static void ALSnd_AddLoopSounds (void)
{
	int					i, j;
	openal_channel_t	*ch;
	sfx_t				*sfx;
	entity_state_t		*ent;

	if (cl_paused->integer || cls.state != ca_active || !cl.sound_prepped)
		return;

	// Add looping entity sounds
	for (i=0 ; i<cl.frame.num_entities ; i++)
	{
		ent = &cl_parse_entities[(cl.frame.parse_entities + i) & PARSE_ENTITIES_MASK];
		if (!ent->sound)
			continue;

		sfx = cl.sound_precache[ent->sound];
		if (!sfx || !sfx->cache)
			continue;		// Bad sound effect

		// Update if already active
		for (j = 0, ch = snd_alOutChannels; j < snd_audioAL.numChannels; ch++, j++) {
			if (ch->sfx != sfx)
				continue;

			if (ch->alRawStream)
				continue;
			if (!ch->alLooping)
				continue;
			if (ch->alLoopEntNum != ent->number)
				continue;
			if (ch->alLoopFrame + 1 != snd_audioAL.frameCount)
				continue;

			ch->alLoopFrame = snd_audioAL.frameCount;
			break;
		}

		// Already active, and simply updated
		if (j != snd_audioAL.numChannels)
			continue;

		// Pick a channel to start the effect
		ch = ALSnd_PickChannel (0, 0);
		if (!ch)
			return;

		ch->alLooping = true;
		ch->alLoopEntNum = ent->number;
		ch->alLoopFrame = snd_audioAL.frameCount;
		ch->alRawStream = false;
		ch->volume = 1;
		ch->psType = PSND_ENTITY;
		ch->distanceMult = 0.3f;

		ALSnd_SpatializeChannel (ch);
		ALSnd_StartChannel (ch, sfx);
	}
}


/*
===========
ALSnd_Update
===========
*/
void ALSnd_Update (const vec3_t position, const vec3_t velocity, const vec3_t at, const vec3_t up)
{
	openal_channel_t	*ch;
	int			state;
	int			total;
	int			i;
	ALfloat		origin[3];
	//ALfloat		vel[3];
	ALfloat		orient[6];

	snd_audioAL.frameCount++;

	// Update our position, velocity, and orientation
	VectorSet(origin, position[1], position[2], -position[0]);
	//VectorSet(velocity, velocity[1], velocity[2], -velocity[0]);

	if(cls.state != ca_active || cl.cinematictime > 0) {
		orient[0] = 0.0f;
		orient[1] = 0.0f;
		orient[2] = -1.0f;
		orient[3] = 0.0f;
		orient[4] = 1.0f;
		orient[5] = 0.0f;
	}
	else
	{
		orient[0] = at[1];
		orient[1] = -at[2];
		orient[2] = -at[0];
		orient[3] = up[1];
		orient[4] = -up[2];
		orient[5] = -up[0];
	}

	qalListenerfv (AL_POSITION, origin);
	//qalListenerfv (AL_VELOCITY, vel);
	qalListenerfv (AL_ORIENTATION, orient);

	// Update doppler
	if (s_openal_dopplerFactor->modified) {
		s_openal_dopplerFactor->modified = false;
		qalDopplerFactor (s_openal_dopplerFactor->value);
	}
	if (s_openal_dopplerVelocity->modified) {
		s_openal_dopplerVelocity->modified = false;
		qalDopplerVelocity (s_openal_dopplerVelocity->value);
	}

	// Update listener volume
	qalListenerf (AL_GAIN, snd_isActive ? s_volume->value : 0);

	// Distance model
	qalDistanceModel (AL_INVERSE_DISTANCE_CLAMPED);

	// Add loop sounds
	if (s_ambient->integer)
		ALSnd_AddLoopSounds ();

	// Add play sounds
	ALSnd_IssuePlaysounds ();

	// Update channel spatialization
	total = 0;
	for (i=0, ch=snd_alOutChannels ; i<snd_audioAL.numChannels ; ch++, i++) {
		// Update streaming channels
		if (ch->alRawStream) {
			ALSnd_RawUpdate (ch);
		}
		else
		{
			if (!ch->sfx)
				continue;

			// Stop inactive channels
			if (ch->alLooping) {
				if (ch->alLoopFrame != snd_audioAL.frameCount) {
					ALSnd_StopSound (ch);
					continue;
				}
				else if (!snd_isActive) {
					ch->alLoopFrame = snd_audioAL.frameCount - 1;
					ALSnd_StopSound (ch);
					continue;
				}
			}
			else {
				qalGetSourcei (ch->sourceNum, AL_SOURCE_STATE, &state);
				if (state == AL_STOPPED) {
					ALSnd_StopSound (ch);
					continue;
				}
			}
			// Spatialize
			ALSnd_SpatializeChannel (ch);
		}

		// Debug output
		if (s_show->integer && ch->volume) {
			Com_Printf ("%3i %s\n", i+1, ch->sfx->name);
			total++;
		}
	}

	// Debug output
	if (s_show->integer)
		Com_Printf ("----(%i)----\n", total);

	// Check for errors
	if (s_openal_errorCheck->integer)
		ALSnd_CheckForError ("ALSnd_Update");
}

/*
==============================================================================

	INIT / SHUTDOWN
 
==============================================================================
*/
static void QAL_Shutdown(void)
{
	// Release the library
	if (snd_alLibrary) {
		Com_Printf ("...releasing the OpenAL library\n");
		AL_FREELIB (snd_alLibrary);
	}
	snd_alLibrary = NULL;

	// Reset QAL bindings
#define AL_FUNC(type, func) q##func = NULL;
#include "openal_funcs.h"
#undef AL_FUNC
}

static qboolean QAL_Init (const char *libName)
{
#ifdef _WIN32
	char	name[MAX_OSPATH];

	Q_strncpyz(name, libName, sizeof(name));
	COM_DefaultExtension(name, sizeof(name), ".dll");
	libName = name;
#endif
	Com_Printf ("...LoadLibrary (\"%s\"): ", libName);
	if (!(snd_alLibrary = AL_LOADLIB (libName))) {
		Com_Printf ("failed!\n");
		return false;
	}
	Com_Printf ("ok\n");

	// Create the QAL bindings
	#define AL_FUNC(type, func) \
	if(!(q##func = (type)AL_GPA(#func))) { \
		Com_Printf("Failed to get address for %s\n", #func); \
		QAL_Shutdown(); \
		return false; \
	}
	#include "openal_funcs.h"
	#undef AL_FUNC
	return true;
}

/*
===========
ALSnd_Init
===========
*/
qboolean ALSnd_Init (void)
{
	char	*device;
	int		i;

	Com_Printf ("Initializing OpenAL\n");

	// Load our OpenAL library
	if(!QAL_Init(s_openal_driver->string)) {
		if(strcmp(s_openal_driver->string, AL_DRIVERNAME)){
			Com_Printf ("ALSnd_Init: trying \"%s\"\n", AL_DRIVERNAME);
			if(!QAL_Init(AL_DRIVERNAME))
				return false;
		} else {
			return false;
		}
	}

	// Open the AL device
	device = s_openal_device->string[0] ?  s_openal_device->string : NULL;
	if (device)
		Com_Printf("...opening device (%s): ", device);
	else
		Com_Printf("...opening device: ");

	al_hDevice = qalcOpenDevice (device);
	if (!al_hDevice) {
		Com_Printf ("failed!\n");
		QAL_Shutdown();
		return false;
	}
	Com_Printf("ok\n");

	// Create the context and make it current
	Com_Printf ("...creating context: ");
	al_hALC = qalcCreateContext (al_hDevice, NULL);
	if (!al_hALC) {
		Com_Printf ("failed!\n");
		ALSnd_Shutdown ();
		return false;
	}
	Com_Printf("ok\n");

	Com_Printf ("...making current: ");
	if (!qalcMakeContextCurrent (al_hALC)) {
		Com_Printf ("failed!\n");
		ALSnd_Shutdown ();
		return false;
	}
	Com_Printf("ok\n");

	// Generate sources
	snd_audioAL.numChannels = 0;
	for (i=0 ; i<MAX_CHANNELS ; i++) {
		qalGenSources (1, &snd_alOutChannels[i].sourceNum);
		if (qalGetError () != AL_NO_ERROR)
			break;
		snd_audioAL.numChannels++;
	}
	if (!snd_audioAL.numChannels) {
		Com_Printf ("...generating sources failed!\n");
		ALSnd_Shutdown ();
		return false;
	}
	Com_Printf ("...generated %i sources\n", snd_audioAL.numChannels);

	// Doppler
	//Com_Printf ("...setting doppler\n");
	qalDopplerFactor (s_openal_dopplerFactor->value);
	qalDopplerVelocity (s_openal_dopplerVelocity->value);

	s_openal_dopplerFactor->modified = false;
	s_openal_dopplerVelocity->modified = false;

	// Query some info
	snd_audioAL.extensionString = qalGetString (AL_EXTENSIONS);
	snd_audioAL.rendererString = qalGetString (AL_RENDERER);
	snd_audioAL.vendorString = qalGetString (AL_VENDOR);
	snd_audioAL.versionString = qalGetString (AL_VERSION);
	snd_audioAL.deviceName = qalcGetString (al_hDevice, ALC_DEVICE_SPECIFIER);

	//Com_Printf ("Initialization successful\n");
	Com_Printf ("AL_VENDOR: %s\n", snd_audioAL.vendorString);
	Com_Printf ("AL_RENDERER: %s\n", snd_audioAL.rendererString);
	Com_Printf ("AL_VERSION: %s\n", snd_audioAL.versionString);
	Com_Printf ("AL_EXTENSIONS: %s\n", snd_audioAL.extensionString);
	Com_Printf ("ALC_DEVICE_SPECIFIER: %s\n", snd_audioAL.deviceName);
	return true;
}
void ALSnd_SoundInfo(void)
{
	Com_Printf ("AL_VENDOR: %s\n", snd_audioAL.vendorString);
	Com_Printf ("AL_RENDERER: %s\n", snd_audioAL.rendererString);
	Com_Printf ("AL_VERSION: %s\n", snd_audioAL.versionString);
	Com_Printf ("AL_EXTENSIONS: %s\n", snd_audioAL.extensionString);
	Com_Printf ("ALC_DEVICE_SPECIFIER: %s\n", snd_audioAL.deviceName);
}

/*
===========
ALSnd_Shutdown
===========
*/
void ALSnd_Shutdown (void)
{
	int		i;

	Com_Printf ("Shutting down OpenAL\n");

	// Free sources
	for (i=0 ; i<snd_audioAL.numChannels ; i++) {
		qalDeleteSources (1, &snd_alOutChannels[i].sourceNum);
	}

	// Release the context
	if (al_hALC) {
		Com_Printf ("...releasing the context\n");
#ifndef _WIN32
		//Hack, this versio hangs when givin NULL as parameter, known bug
		if(!strcmp(snd_audioAL.vendorString, "J. Valenzuela") && 
			!strcmp(snd_audioAL.versionString, "0.0.8")) {
			Com_Printf("Your OpenAL lib has problem with alcMakeContextCurrent(NULL), skipping...\n");
		}
		else
#endif
		{
			qalcMakeContextCurrent (NULL);
		}

		Com_Printf ("...destroying the context\n");
		qalcDestroyContext (al_hALC);
	}
	al_hALC = NULL;

	// Close the device
	if (al_hDevice) {
		Com_Printf ("...closing the device\n");
		qalcCloseDevice (al_hDevice);
	}
	al_hDevice = NULL;

	QAL_Shutdown();

	memset(snd_alOutChannels, 0, sizeof(snd_alOutChannels));
	memset(&snd_audioAL, 0, sizeof(snd_audioAL));
}
#endif
