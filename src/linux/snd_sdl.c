/*
	snd_sdl.c

	Sound code taken from SDLQuake and modified to work with Quake2
	Robert Bäuml 2001-12-25

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id: snd_sdl.c,v 1.2 2002/02/09 20:29:38 relnev Exp $
*/

#include "SDL.h"

#include "../client/client.h"
#include "../client/snd_loc.h"

static qboolean snd_inited = false;

cvar_t *sndbits;
cvar_t *sndspeed;
cvar_t *sndchannels;

void Snd_Memset (void* dest, const int val, const size_t count)
{
	memset(dest,val,count);
}

static void sdl_audio_callback (void *unused, Uint8 * stream, int len)
{
	if (snd_inited) {
		dma.buffer = stream;
		dma.samplepos += len / (dma.samplebits / 4);
		// Check for samplepos overflow?
		S_PaintChannels (dma.samplepos);
	}
}

qboolean SNDDMA_Init (void)
{
	SDL_AudioSpec desired, obtained;
	
	if(snd_inited)
		return true;

	if (!sndbits) {
		sndbits = Cvar_Get("sndbits", "16", CVAR_ARCHIVE);
		sndspeed = Cvar_Get("sndspeed", "0", CVAR_ARCHIVE);
		sndchannels = Cvar_Get("sndchannels", "2", CVAR_ARCHIVE);
		//snddevice = Cvar_Get("snddevice", "/dev/dsp", CVAR_ARCHIVE);
	}

	if (!SDL_WasInit(SDL_INIT_EVERYTHING)) {
		if (SDL_Init(SDL_INIT_AUDIO) < 0) {
			Com_Printf ("Couldn't init SDL audio: %s\n", SDL_GetError ());
			return false;
		}
	} else if (!SDL_WasInit(SDL_INIT_AUDIO)) {
		if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
			Com_Printf ("Couldn't init SDL audio: %s\n", SDL_GetError ());
			return false;
		}
	}
	

	/* Set up the desired format */
	switch (sndbits->integer) {
		case 8:
			desired.format = AUDIO_U8;
			break;
		default:
			Com_Printf ("Unknown number of audio bits: %i, trying with 16\n", sndbits->integer);
		case 16:
			desired.format = AUDIO_S16SYS;
			break;
	}
	
	if(sndspeed->integer)
		desired.freq = sndspeed->integer;
	else
		desired.freq = 22050;

    // just pick a sane default.
    if (desired.freq <= 11025)
        desired.samples = 256;
    else if (desired.freq <= 22050)
        desired.samples = 512;
    else if (desired.freq <= 44100)
        desired.samples = 1024;
    else
        desired.samples = 2048;  // (*shrug*)
	
	desired.channels = sndchannels->integer;
	if (desired.channels < 1 || desired.channels > 2)
		desired.channels = 2;

	desired.callback = sdl_audio_callback;
	
	/* Open the audio device */
	if (SDL_OpenAudio (&desired, &obtained) < 0) {
		Com_Printf ("SDL_OpenAudio() failed: %s\n", SDL_GetError ());
		if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_AUDIO)
			SDL_Quit();
		else
			SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return false;
	}

	/* Fill the audio DMA information block */
	dma.samplebits = (obtained.format & 0xFF);
	dma.speed = obtained.freq;
	dma.channels = obtained.channels;
	dma.samples = obtained.samples * dma.channels;
	dma.samplepos = 0;
	dma.submission_chunk = 1;
	dma.buffer = NULL;

    Com_Printf("Starting SDL audio callback...\n");
    SDL_PauseAudio(0);  // start callback.

    Com_Printf("SDL audio initialized.\n");

	snd_inited = true;
	return true;
}

int SNDDMA_GetDMAPos (void)
{
	if(!snd_inited)
		return 0;

	return dma.samplepos;
}

void SNDDMA_Shutdown (void)
{
	if (!snd_inited)
		return;

	SDL_PauseAudio(1);
	SDL_CloseAudio ();
	snd_inited = false;

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_AUDIO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

/*

	SNDDMA_Submit

	Send sound to device if buffer isn't really the dma buffer

*/
void SNDDMA_Submit (void)
{
}


void SNDDMA_BeginPainting(void)
{    
}
