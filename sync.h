/* 
 * MDFourier
 * A Fourier Transform analysis tool to compare different 
 * Sega Genesis/Mega Drive audio hardware revisions.
 *
 * Copyright (C)2019 Artemio Urbina
 *
 * This file is part of the 240p Test Suite
 *
 * You can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA	02111-1307	USA
 *
 * Requires the FFTW library: 
 *	  http://www.fftw.org/
 */

#ifndef MDFOURIER_SYNC_H
#define MDFOURIER_SYNC_H

typedef struct pulses_st {
	double	hertz;
	double	magnitude;
	double	amplitude;
	long int bytes;
} Pulses;

long int DetectPulse(char *AllSamples, wav_hdr header, parameters *config);
long int DetectEndPulse(char *AllSamples, long int startpulse, wav_hdr header, parameters *config);
long int DetectPulseInternal(char *Samples, wav_hdr header, int factor, long int offset, int *maxdetected, parameters *config);
double ProcessChunkForSyncPulse(int16_t *samples, size_t size, long samplerate, Pulses *pulse, char channel, double target, parameters *config);
long int DetectPulseTrainSequence(Pulses *pulseArray, double targetFrequency, long int TotalMS, int factor, int *maxdetected, parameters *config);

long int DetectSignalStart(char *AllSamples, wav_hdr header, parameters *config);
long int DetectSignalStartInternal(char *Samples, wav_hdr header, int factor, long int offset, int *maxdetected, parameters *config);
#endif