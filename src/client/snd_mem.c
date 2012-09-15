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
// snd_mem.c: sound caching

#include "client.h"
#include "snd_loc.h"

extern cvar_t *s_oldresample;

/*
================
ResampleSfx
================
*/
static void ResampleSfxOrg(sfxcache_t *sc, byte *data)
{
	int		i, outcount, fracstep, samplefrac, srcsample, inwidth;
	float	stepscale;

	stepscale = (float)sc->speed / dma.speed; // this is usually 0.5, 1, or 2

	outcount = sc->length / stepscale;
	sc->length = outcount;
	if (sc->loopstart != -1)
		sc->loopstart = sc->loopstart / stepscale;

	sc->speed = dma.speed;
	
	inwidth = sc->width;
	if (s_loadas8bit->integer == 2)
		sc->width = 1;

	// resample / decimate to the current source rate
	if (stepscale == 1 && inwidth == sc->width)
	{
#ifndef ENDIAN_LITTLE
		if (sc->width == 2) {
			for (i = 0; i < outcount; i++)
				((int16 *)sc->data)[i] = LittleShort (((in16 *)data)[i]);
		} else { // 8bit
			memcpy( sc->data, data, outcount );
		}
#else
		memcpy( sc->data, data, outcount * sc->width );
#endif
	}
	else
	{
	// general case
		samplefrac = 0;
		fracstep = stepscale*256;
		if (inwidth == 2) {
			int16 *in = (int16 *)data;
			if (sc->width == 2) {
				int16 *out = (int16 *)sc->data;
				for (i = 0; i < outcount; i++) {
					srcsample = samplefrac >> 8;
					samplefrac += fracstep;
					*out++ = LittleShort( in[srcsample] );	
				}
			} else {
				byte *out = sc->data;
				for (i = 0; i < outcount; i++) {
					srcsample = samplefrac >> 8;
					samplefrac += fracstep;
					*out++ = (byte)(LittleShort( in[srcsample] ) >> 8) + 128;
				}
			}
		} else {
			byte *in = data, *out = sc->data;
			for (i = 0; i < outcount; i++) {
				srcsample = samplefrac >> 8;
				samplefrac += fracstep;
				*out++	= in[srcsample];
			}
		}
	}
}

static void ResampleSfx(sfxcache_t *sc, byte *data)
{
	int i, srclength, outcount, fracstep, chancount;
	int	samplefrac, srcsample, srcnextsample;

	// this is usually 0.5 (128), 1 (256), or 2 (512)
	fracstep = ((double) sc->speed / (double) dma.speed) * 256.0;

	chancount = sc->channels - 1;
	srclength = sc->length * sc->channels;
	outcount = (double) sc->length * (double) dma.speed / (double) sc->speed;

	sc->length = outcount;
	if (sc->loopstart != -1)
		sc->loopstart = (double) sc->loopstart * (double) dma.speed / (double) sc->speed;

	sc->speed = dma.speed;

// resample / decimate to the current source rate
	if (fracstep == 256)
	{
#ifndef ENDIAN_LITTLE
		if (sc->width == 2) {
			for (i = 0; i < srclength; i++)
				((int16 *)sc->data)[i] = LittleShort (((in16 *)data)[i]);
		} else { // 8bit
			memcpy( sc->data, data, srclength );
		}
#else
		memcpy( sc->data, data, srclength * sc->width );
#endif
	}
	else
	{
		int j, a, b, sample;

// general case
		samplefrac = 0;
		srcsample = 0;
		srcnextsample = sc->channels;
		outcount *= sc->channels;

#define RESAMPLE_AND_ADVANCE	\
				sample = (((b - a) * (samplefrac & 255)) >> 8) + a; \
				if (j == chancount) \
				{ \
					samplefrac += fracstep; \
					srcsample = (samplefrac >> 8) << chancount; \
					srcnextsample = srcsample + sc->channels; \
				}

		if (sc->width == 2)
		{
			int16 *out = (int16 *)sc->data, *in = (int16 *)data;
			for (i = 0, j = 0; i < outcount; i++, j = i & chancount) {
				a = LittleShort (in[srcsample + j]);
				b = ((srcnextsample < srclength) ? LittleShort (in[srcnextsample + j]) : 0);
				RESAMPLE_AND_ADVANCE;
				*out++ = (int16)sample;
			}
		}
		else
		{
			byte *out = sc->data, *in = data;
			for (i = 0, j = 0; i < outcount; i++, j = i & chancount) {
				a = (int)in[srcsample + j];
				b = ((srcnextsample < srclength) ? (int)in[srcnextsample + j] : 128);
				RESAMPLE_AND_ADVANCE;
				*out++ = (byte)sample;
			}
		}
	}
}

/*
===============================================================================

WAV loading

===============================================================================
*/
typedef struct wavinfo_s {
	int			rate;
	int			width;
	int			channels;
	int			loopstart;
	int			samples;
	int			dataofs;		// chunk starts this many bytes from file start
} wavinfo_t;

static byte		*data_p;
static byte 	*iff_end;
static byte 	*last_chunk;
static byte 	*iff_data;
static int32	iff_chunk_len;

static int16 GetLittleShort(void)
{
	int16 val;

    if( data_p + 2 > iff_end ) {
        return -1;
    }

	val = data_p[0];
	val |= data_p[1] << 8;
	data_p += 2;
	return val;
}

static int32 GetLittleLong(void)
{
	int32 val;

    if( data_p + 4 > iff_end ) {
        return -1;
    }

	val = data_p[0];
	val |= data_p[1] << 8;
	val |= data_p[2] << 16;
	val |= data_p[3] << 24;
	data_p += 4;
	return val;
}

static void FindNextChunk(const char *name)
{
	while (1)
	{
		data_p = last_chunk;

		data_p += 4;

		if (data_p >= iff_end)
		{	// didn't find the chunk
			data_p = NULL;
			return;
		}

		iff_chunk_len = GetLittleLong();
		if (iff_chunk_len < 0) {
			data_p = NULL;
			return;
		}

		data_p -= 8;
		last_chunk = data_p + 8 + ( (iff_chunk_len + 1) & ~1 );
		if (!strncmp((char *)data_p, name, 4))
			return;
	}
}

static void FindChunk(const char *name)
{
	last_chunk = iff_data;
	FindNextChunk (name);
}

/*
============
GetWavinfo
============
*/
static qboolean GetWavinfo (const char *name, wavinfo_t *info, byte *wav, int wavlength)
{
	int			format, samples, maxLen;

	memset (info, 0, sizeof(*info));

	if (!wav)
		return false;

	iff_data = wav;
	iff_end = wav + wavlength;

// find "RIFF" chunk
	FindChunk("RIFF");
	if (!data_p) {
		Com_Printf("GetWavinfo: missing/invalid RIFF chunk (%s).\n", name);
		return false;
	}
	if (strncmp((char *)data_p+8, "WAVE", 4)) {
		Com_Printf("GetWavinfo: missing/invalid WAVE chunk (%s).\n", name);
		return false;
	}

// get "fmt " chunk
	iff_data = data_p + 12;

	FindChunk("fmt ");
	if (!data_p) {
		Com_Printf("GetWavinfo: missing/invalid fmt chunk (%s).\n", name);
		return false;
	}

	data_p += 8;
	format = GetLittleShort();
	if (format != 1) {
		Com_Printf("GetWavinfo: non-Microsoft PCM format (%s).\n", name);
		return false;
	}

	info->channels = GetLittleShort();
	if (info->channels != 1 && info->channels != 2) {
		Com_Printf("GetWavinfo: %s has bad number of channels (%i).\n", name, info->channels);
        return false;
	}

	info->rate = GetLittleLong();
    if( info->rate <= 0 ) {
		Com_Printf("GetWavinfo: %s has bad rate (%i).\n", name, info->rate);
        return false;
    }

	data_p += 4+2;
	info->width = GetLittleShort();
    switch( info->width ) {
    case 8:
		info->width = 1;
		break;
    case 16:
        info->width = 2;
        break;
    default:
		Com_Printf( "GetWavinfo: %s has bad width (%i).\n", name );
        return false;
    }


// get cue chunk
	FindChunk("cue ");
	if (data_p)
	{
		data_p += 32;
		info->loopstart = GetLittleLong();

	// if the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk ("LIST");
		if (data_p && !strncmp((char *)data_p + 28, "mark", 4))
		{	// this is not a proper parse, but it works with cooledit...
			data_p += 24;
			samples = GetLittleLong();	// samples in loop
			info->samples = info->loopstart + samples;
		}
	}
	else
		info->loopstart = -1;

// find data chunk
	FindChunk("data");
	if (!data_p) {
		Com_Printf("GetWavinfo: missing/invalid data chunk (%s).\n", name);
		return false;
	}

	data_p += 4;
	samples = GetLittleLong () / info->width;
    if (!samples) {
		Com_DPrintf( "GetWavinfo: %s has zero length\n", name );
        //return false;
    }

	if (info->samples)
	{
		if (samples < info->samples) {
			Com_Printf("GetWavinfo: %s has a bad loop length", name);
			return false;
		}
	}
	else
		info->samples = samples;

	info->dataofs = (int)(data_p - wav);

	maxLen = wavlength - info->dataofs;
	if (info->samples * info->width > maxLen) {
		Com_Printf("GetWavinfo: %s is malformed, samples exceeds the filelenght (%d > %d)\n", name, info->samples * info->width, maxLen);
		info->samples = maxLen / info->width;
	}

	return true;
}


/*
==============
S_LoadSound
==============
*/
sfxcache_t *S_LoadSound (sfx_t *s)
{
    char	namebuffer[MAX_QPATH], *name;
	byte	*data;
	wavinfo_t	info;
	sfxcache_t	*sc;
	int		len, size;

	if (s->name[0] == '*')
		return NULL;

// see if still in memory
	sc = s->cache;
	if (sc)
		return sc;

// load it in
	name = (s->truename) ? s->truename : s->name;

	if (name[0] == '#')
		Q_strncpyz (namebuffer, &name[1], sizeof(namebuffer));
	else
		Com_sprintf (namebuffer, sizeof(namebuffer), "sound/%s", name);

	size = FS_LoadFile (namebuffer, (void **)&data);
	if (!data) {
		Com_DPrintf ("Couldn't load %s\n", namebuffer);
		return NULL;
	}

	if (!GetWavinfo( name, &info, data, size )) {
        FS_FreeFile (data);
        return NULL;
    }

#ifdef USE_OPENAL
	if(alSound) {
		len = 0;
	} else {
#endif
		// calculate resampled length
		len = (int)((double)info.samples * (double)dma.speed / (double)info.rate);
		len = len * info.width;
#ifdef USE_OPENAL
	}
#endif

	sc = s->cache = Z_TagMalloc (len + sizeof(sfxcache_t), TAG_CL_SOUNDCACHE);

	sc->length = info.samples / info.channels;
	sc->loopstart = info.loopstart;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->channels = info.channels;

#ifdef USE_OPENAL
	sc->alBufferNum = 0;
	if(alSound)
	{
		ALSnd_CreateBuffer (sc, info.width, info.channels, data + info.dataofs, info.samples * info.width * info.channels, info.rate);
	}
	else
#endif
	{
		if (s_oldresample->integer && sc->channels == 1)
			ResampleSfxOrg(sc, data + info.dataofs);
		else
			ResampleSfx(sc, data + info.dataofs);
	}
	FS_FreeFile (data);

	return sc;
}

