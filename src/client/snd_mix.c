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
// snd_mix.c -- portable code to mix sounds for snd_dma.c

#include "client.h"
#include "snd_loc.h"

#define	PAINTBUFFER_SIZE	2048
static portable_samplepair_t paintbuffer[PAINTBUFFER_SIZE];
static int		snd_scaletable[32][256], snd_vol;
int 	*snd_p, snd_linear_count;
int16	*snd_out;

void S_WriteLinearBlastStereo16 (void)
{
	int		i, val;

	for (i=0 ; i<snd_linear_count ; i+=2)
	{
		val = snd_p[i]>>8;
		snd_out[i] = bound (-32768, val, 0x7fff);

		val = snd_p[i+1]>>8;
		snd_out[i+1] = bound (-32768, val, 0x7fff);
	}
}

void S_WriteSwappedLinearBlastStereo16 (void)
{
	int		i, val;
	
	for (i=0 ; i<snd_linear_count ; i+=2)
	{
		val = snd_p[i+1]>>8;
		snd_out[i] = bound (-32768, val, 0x7fff);

		val = snd_p[i  ]>>8;
		snd_out[i+1] = bound (-32768, val, 0x7fff);
	}
}

static void S_TransferStereo16 (unsigned int *pbuf, int endtime)
{
	int		lpos, lpaintedtime;
	
	snd_p = (int *) paintbuffer;
	lpaintedtime = paintedtime;

	while (lpaintedtime < endtime)
	{
	// handle recirculating buffer issues
		lpos = lpaintedtime & ((dma.samples>>1)-1);

		snd_out = (int16 *) pbuf + (lpos<<1);

		snd_linear_count = (dma.samples>>1) - lpos;
		if (lpaintedtime + snd_linear_count > endtime)
			snd_linear_count = endtime - lpaintedtime;

		snd_linear_count <<= 1;

	// write a linear blast of samples
		if( s_swapstereo->integer )
			S_WriteSwappedLinearBlastStereo16 ();
		else
			S_WriteLinearBlastStereo16 ();

		snd_p += snd_linear_count;
		lpaintedtime += (snd_linear_count>>1);

#ifdef AVI_EXPORT
		Movie_TransferStereo16 ();
#endif
	}
}

/*
===================
S_TransferPaintBuffer

===================
*/
void S_TransferPaintBuffer(int endtime)
{
	int 	i, count, out_idx, out_mask;
	int 	*p, step, val;
	unsigned int *pbuf;

	pbuf = (unsigned int *)dma.buffer;

	if (s_testsound->integer) {
		// write a fixed sine wave
		count = (endtime - paintedtime);
		for (i=0 ; i<count ; i++)
			paintbuffer[i].left = paintbuffer[i].right = (int)((float)sin((paintedtime+i)*0.1f)*20000*256);
	}

	if (dma.samplebits == 16 && dma.channels == 2)
	{	// optimized case
		S_TransferStereo16 (pbuf, endtime);
	}
	else
	{	// general case
		p = (int *) paintbuffer;
		count = (endtime - paintedtime) * dma.channels;
		out_mask = dma.samples - 1; 
		out_idx = paintedtime * dma.channels & out_mask;
		step = 3 - dma.channels;

		if (dma.samplebits == 16)
		{
			int16 *out = (int16 *) pbuf;
			while (count--)
			{
				val = *p >> 8;
				p += step;
				out[out_idx] = bound (-32768, val, 0x7fff);
				out_idx = (out_idx + 1) & out_mask;
			}
		}
		else if (dma.samplebits == 8)
		{
			unsigned char *out = (unsigned char *) pbuf;
			while (count--)
			{
				val = *p >> 8;
				p += step;
				out[out_idx] = (bound (-32768, val, 0x7fff)>>8) + 128;
				out_idx = (out_idx + 1) & out_mask;
			}
		}
	}
}


/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/

static void S_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int endtime, int offset);
static void S_PaintChannelFrom16 (channel_t *ch, sfxcache_t *sc, int endtime, int offset);

void S_PaintChannels(int endtime)
{
	int 	i;
	int 	end;
	channel_t *ch;
	sfxcache_t	*sc;
	int		ltime, count;
	playsound_t	*ps;
	extern qboolean sound_started;

	if (!sound_started)
		return;

	snd_vol = s_volume->value*256;

	while (paintedtime < endtime)
	{
	// if paintbuffer is smaller than DMA buffer
		end = endtime;
		if (endtime - paintedtime > PAINTBUFFER_SIZE)
			end = paintedtime + PAINTBUFFER_SIZE;

		// start any playsounds
		while (1)
		{
			ps = s_pendingplays.next;
			if (ps == &s_pendingplays)
				break;	// no more pending sounds
			if (ps->begin <= paintedtime)
			{
				S_IssuePlaysound (ps);
				continue;
			}

			if (ps->begin < end)
				end = ps->begin;		// stop here
			break;
		}

	// clear the paint buffer
		if (s_rawend < paintedtime)
		{
			memset(paintbuffer, 0, (end - paintedtime) * sizeof(portable_samplepair_t));
		}
		else
		{	// copy from the streaming sound source
			int		s;
			int		stop;

			stop = (end < s_rawend) ? end : s_rawend;

			for (i=paintedtime ; i<stop ; i++)
			{
				s = i&(MAX_RAW_SAMPLES-1);
				paintbuffer[i-paintedtime] = s_rawsamples[s];
			}

			for ( ; i<end ; i++)
			{
				paintbuffer[i-paintedtime].left = 0;
				paintbuffer[i-paintedtime].right = 0;
			}
		}

	// paint in the channels.
		ch = channels;
		for (i=0; i<MAX_CHANNELS ; i++, ch++)
		{
			ltime = paintedtime;
		
			while (ltime < end)
			{
				if (!ch->sfx || (!ch->leftvol && !ch->rightvol) )
					break;

				// max painting is to the end of the buffer
				count = end - ltime;

				// might be stopped by running out of data
				if (ch->end < end)
					count = ch->end - ltime;
		
				sc = S_LoadSound (ch->sfx);
				if (!sc)
					break;

				if (count > 0 && ch->sfx)
				{	
					if (sc->width == 1)
						S_PaintChannelFrom8(ch, sc, count,  ltime - paintedtime);
					else
						S_PaintChannelFrom16(ch, sc, count, ltime - paintedtime);
	
					ltime += count;
				}

			// if at end of loop, restart
				if (ltime >= ch->end)
				{
					if (ch->autosound)
					{	// autolooping sounds always go back to start
						ch->pos = 0;
						ch->end = ltime + sc->length;
					}
					else if (sc->loopstart >= 0)
					{
						ch->pos = sc->loopstart;
						ch->end = ltime + sc->length - ch->pos;
					}
					else				
					{	// channel just stopped
						ch->sfx = NULL;
					}
				}
			}
															  
		}

	// transfer out according to DMA format
		S_TransferPaintBuffer(end);
		paintedtime = end;
	}
}

void S_InitScaletable (void)
{
	int		i, j, scale;

	if (s_volume->value > 2.0f)
		Cvar_Set ("s_volume", "2");
	else if (s_volume->value < 0)
		Cvar_Set  ("s_volume", "0");

	s_volume->modified = false;
	for (i = 0; i < 32; i++)
	{
		scale = i * 8 * 256 * s_volume->value;
		for (j = 0; j < 256; j++)
			snd_scaletable[i][j] = ((signed char)(j - 128)) * scale;
	}
}

static void S_PaintChannelFrom8 (channel_t *ch, sfxcache_t *sc, int count, int offset)
{
	int		i, j, *lscale, *rscale;
	byte *sfx;
	portable_samplepair_t	*samp;

	if (ch->leftvol > 255)
		ch->leftvol = 255;
	if (ch->rightvol > 255)
		ch->rightvol = 255;
		
	if ( !snd_vol ) {
		ch->pos += count;
		return;
	}

	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];
	samp = &paintbuffer[offset];

	if (sc->channels == 2) {
		sfx = sc->data + ch->pos * 2;
		for (i = 0; i < count; i++, samp++) {
			samp->left += lscale[*sfx++];
			samp->right += rscale[*sfx++];
		}
	} else {
		sfx = sc->data + ch->pos;
		for (i = 0; i < count; i++, samp++) {
			j = *sfx++;
			samp->left += lscale[j];
			samp->right += rscale[j];
		}
	}
	
	ch->pos += count;
}

static void S_PaintChannelFrom16 (channel_t *ch, sfxcache_t *sc, int count, int offset)
{
	int	i, j, leftvol, rightvol;
	int16 *sfx;
	portable_samplepair_t	*samp;

	if ( !snd_vol ) {
		ch->pos += count;
		return;
	}

	leftvol = ch->leftvol*snd_vol;
	rightvol = ch->rightvol*snd_vol;
	samp = &paintbuffer[offset];

	if (sc->channels == 2) {
		sfx = (int16 *)sc->data + ch->pos * 2;
		for (i = 0; i < count; i++, samp++) {
			samp->left += (*sfx++ * leftvol) >> 8;
			samp->right += (*sfx++ * rightvol) >> 8;
		}
	} else {
		sfx = (int16 *)sc->data + ch->pos;
		for (i = 0; i < count; i++, samp++) {
			j = *sfx++;
			samp->left += (j * leftvol) >> 8;
			samp->right += (j * rightvol) >> 8;
		}
	}

	ch->pos += count;
}
