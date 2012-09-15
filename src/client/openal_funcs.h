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

#ifndef AL_FUNC
#define AL_FUNC(type, func)
#define UNDEF_AL_FUNC
#endif

AL_FUNC (LPALGETSTRING, alGetString)
AL_FUNC (LPALGETERROR, alGetError)
AL_FUNC (LPALLISTENERF, alListenerf)
AL_FUNC (LPALLISTENERFV, alListenerfv)
AL_FUNC (LPALGENSOURCES, alGenSources)
AL_FUNC (LPALDELETESOURCES, alDeleteSources)
AL_FUNC (LPALISSOURCE, alIsSource)
AL_FUNC (LPALSOURCEF, alSourcef)
AL_FUNC (LPALSOURCE3F, alSource3f)
AL_FUNC (LPALSOURCEFV, alSourcefv)
AL_FUNC (LPALSOURCEI, alSourcei)
AL_FUNC (LPALGETSOURCEI, alGetSourcei)
AL_FUNC (LPALSOURCEPLAY, alSourcePlay)
AL_FUNC (LPALSOURCESTOP, alSourceStop)
AL_FUNC (LPALSOURCEQUEUEBUFFERS, alSourceQueueBuffers)
AL_FUNC (LPALSOURCEUNQUEUEBUFFERS, alSourceUnqueueBuffers)

AL_FUNC (LPALGENBUFFERS, alGenBuffers)
AL_FUNC (LPALDELETEBUFFERS, alDeleteBuffers)
AL_FUNC (LPALISBUFFER, alIsBuffer)
AL_FUNC (LPALBUFFERDATA, alBufferData)
AL_FUNC (LPALDOPPLERFACTOR, alDopplerFactor)
AL_FUNC (LPALDOPPLERVELOCITY, alDopplerVelocity)
AL_FUNC (LPALDISTANCEMODEL, alDistanceModel)

AL_FUNC (LPALCCREATECONTEXT, alcCreateContext)
AL_FUNC (LPALCDESTROYCONTEXT, alcDestroyContext)
AL_FUNC (LPALCMAKECONTEXTCURRENT, alcMakeContextCurrent)

AL_FUNC (LPALCOPENDEVICE, alcOpenDevice)
AL_FUNC (LPALCCLOSEDEVICE, alcCloseDevice)
AL_FUNC (LPALCGETSTRING, alcGetString)

//These isnt used right now
/*AL_FUNC (LPALENABLE, alEnable)
AL_FUNC (LPALDISABLE, alDisable)
AL_FUNC (LPALISENABLED, alIsEnabled)

AL_FUNC (LPALGETBOOLEANV, alGetBooleanv)
AL_FUNC (LPALGETINTEGERV, alGetIntegerv)
AL_FUNC (LPALGETFLOATV, alGetFloatv)
AL_FUNC (LPALGETDOUBLEV, alGetDoublev)
AL_FUNC (LPALGETBOOLEAN, alGetBoolean)
AL_FUNC (LPALGETINTEGER, alGetInteger)
AL_FUNC (LPALGETFLOAT, alGetFloat)
AL_FUNC (LPALGETDOUBLE, alGetDouble)

AL_FUNC (LPALISEXTENSIONPRESENT, alIsExtensionPresent)
AL_FUNC (LPALGETPROCADDRESS, alGetProcAddress)
AL_FUNC (LPALGETENUMVALUE, alGetEnumValue)

AL_FUNC (LPALLISTENER3F, alListener3f)
AL_FUNC (LPALLISTENERI, alListeneri)
AL_FUNC (LPALLISTENER3I, alListener3i)
AL_FUNC (LPALLISTENERIV, alListeneriv)
AL_FUNC (LPALGETLISTENERF, alGetListenerf)
AL_FUNC (LPALGETLISTENER3F, alGetListener3f)
AL_FUNC (LPALGETLISTENERFV, alGetListenerfv)
AL_FUNC (LPALGETLISTENERI, alGetListeneri)
AL_FUNC (LPALGETLISTENER3I, alGetListener3i)
AL_FUNC (LPALGETLISTENERIV, alGetListeneriv)

AL_FUNC (LPALSOURCE3I, alSource3i)
AL_FUNC (LPALSOURCEIV, alSourceiv)

AL_FUNC (LPALGETSOURCEF, alGetSourcef)
AL_FUNC (LPALGETSOURCE3F, alGetSource3f)
AL_FUNC (LPALGETSOURCEFV, alGetSourcefv)
AL_FUNC (LPALGETSOURCE3I, alGetSource3i)
AL_FUNC (LPALGETSOURCEIV, alGetSourceiv)
AL_FUNC (LPALSOURCEPLAYV, alSourcePlayv)
AL_FUNC (LPALSOURCESTOPV, alSourceStopv)
AL_FUNC (LPALSOURCEREWINDV, alSourceRewindv)
AL_FUNC (LPALSOURCEPAUSEV, alSourcePausev)

AL_FUNC (LPALSOURCEREWIND, alSourceRewind)
AL_FUNC (LPALSOURCEPAUSE, alSourcePause)

AL_FUNC (LPALBUFFERF, alBufferf)
AL_FUNC (LPALBUFFER3F, alBuffer3f)
AL_FUNC (LPALBUFFERFV, alBufferfv)
AL_FUNC (LPALBUFFERI, alBufferi)
AL_FUNC (LPALBUFFER3I, alBuffer3i)
AL_FUNC (LPALBUFFERIV, alBufferiv)
AL_FUNC (LPALGETBUFFERF, alGetBufferf)
AL_FUNC (LPALGETBUFFER3F, alGetBuffer3f)
AL_FUNC (LPALGETBUFFERFV, alGetBufferfv)
AL_FUNC (LPALGETBUFFERI, alGetBufferi)
AL_FUNC (LPALGETBUFFER3I, alGetBuffer3i)
AL_FUNC (LPALGETBUFFERIV, alGetBufferiv)

AL_FUNC (LPALSPEEDOFSOUND, alSpeedOfSound)

AL_FUNC (LPALCPROCESSCONTEXT, alcProcessContext)
AL_FUNC (LPALCSUSPENDCONTEXT, alcSuspendContext)

AL_FUNC (LPALCGETCURRENTCONTEXT, alcGetCurrentContext)
AL_FUNC (LPALCGETCONTEXTSDEVICE, alcGetContextsDevice)


AL_FUNC (LPALCGETERROR, alcGetError)

AL_FUNC (LPALCISEXTENSIONPRESENT, alcIsExtensionPresent)
AL_FUNC (LPALCGETPROCADDRESS, alcGetProcAddress)

AL_FUNC (LPALCGETENUMVALUE, alcGetEnumValue)
AL_FUNC (LPALCGETINTEGERV, alcGetIntegerv)

AL_FUNC (LPALCCAPTUREOPENDEVICE, alcCaptureOpenDevice)
AL_FUNC (LPALCCAPTURECLOSEDEVICE, alcCaptureCloseDevice)
AL_FUNC (LPALCCAPTURESTART, alcCaptureStart)
AL_FUNC (LPALCCAPTURESTOP, alcCaptureStop)
AL_FUNC (LPALCCAPTURESAMPLES, alcCaptureSamples)*/

#ifdef UNDEF_AL_FUNC
#undef AL_FUNC
#undef UNDEF_AL_FUNC
#endif
