/* 
 * MDFourier
 * A Fourier Transform analysis tool to compare game console audio
 * http://junkerhq.net/MDFourier/
 *
 * Copyright (C)2019-2020 Artemio Urbina
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
 * 
 */
#ifndef MDFLOADFILE_H
#define MDFLOADFILE_H

int LoadFile(AudioSignal **Signal, char *fileName, int role, parameters *config);
int LoadWAVFile(FILE *file, AudioSignal *Signal, parameters *config);
int DetectSync(AudioSignal *Signal, parameters *config);
int AdjustSignalValues(AudioSignal *Signal, parameters *config);

/* Functions that deal with samples */
int MoveSampleBlockInternal(AudioSignal *Signal, long int element, long int pos, long int signalStartOffset, parameters *config);
int MoveSampleBlockExternal(AudioSignal *Signal, long int element, long int pos, long int signalStartOffset, parameters *config);
int ProcessInternalSync(AudioSignal *Signal, long int element, long int pos, int *syncinternal, long int *advanceBytes, int knownLength, parameters *config);
int CopySamplesForTimeDomainPlotInternalSync(AudioBlocks *AudioArray, double *samples, size_t size, int slotForSamples, double *window, int AudioChannels, parameters *config);

#endif
