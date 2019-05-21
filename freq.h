/* 
 * MDFourier
 * A Fourier Transform analysis tool to compare different 
 * Sega Genesis/Mega Drive audio hardware revisions, and
 * other hardware in the future
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
 * 
 */

#ifndef MDFOURIER_FREQ_H
#define MDFOURIER_FREQ_H

#include "mdfourier.h"

#define LINE_BUFFER_SIZE	1024
#define PARAM_BUFFER_SIZE	512

#define readLine(buffer, file) if(fgets(buffer, LINE_BUFFER_SIZE, file) == NULL) { logmsg("Invalid mdfblocks.mfn file\n");	return 0;	}

double FramesToSeconds(double frames, double framerate);
long int SecondsToBytes(long int samplerate, double seconds, int *leftover, int *discard);

int LoadAudioBlockStructure(parameters *config);
int GetFirstSilenceIndex(parameters *config);
double GetBlockTimeOffset(int block, parameters *config);
long int GetLastSilenceByteOffset(double framerate, wav_hdr header, int frameAdjust, parameters *config);
int GetActiveAudioBlocks(parameters *config);
int GetTotalAudioBlocks(parameters *config);
long int GetLongestElementFrames(parameters *config);
long int GetSignalTotalFrames(parameters *config);
double GetSignalTotalDuration(double framerate, parameters *config);
long int GetBlockFrames(parameters *config, int pos);
char *GetBlockName(parameters *config, int pos);
int GetBlockSubIndex(parameters *config, int pos);
int GetBlockType(parameters *config, int pos);
char *GetBlockColor(parameters *config, int pos);
char *GetTypeColor(parameters *config, int type);
double GetPlatformMSPerFrame(parameters *config);
void ReleaseAudioBlockStructure(parameters *config);
void PrintAudioBlocks(parameters *config);
long int GetLastSyncFrameOffset(wav_hdr header, parameters *config);
long int GetBlockFrameOffset(int block, parameters *config);
double GetPulseSyncFreq(parameters *config);
long int GetByteSizeDifferenceByFrameRate(double framerate, long int frames, long int samplerate, parameters *config);
int GetFirstSyncIndex(parameters *config);
int GetLastSyncIndex(parameters *config);
int GetActiveBlockTypes(parameters *config);

AudioSignal *CreateAudioSignal(parameters *config);
void CleanAudio(AudioSignal *Signal, parameters *config);
void ReleaseAudio(AudioSignal *Signal, parameters *config);
void CleanMatched(AudioSignal *ReferenceSignal, AudioSignal *TestSignal, parameters *config);
void FillFrequencyStructures(AudioBlocks *AudioArray, parameters *config);
void PrintFrequencies(AudioSignal *Signal, parameters *config);
void GlobalNormalize(AudioSignal *Signal, parameters *config);
void FindFloor(AudioSignal *Signal, parameters *config);
int IsCRTNoise(double freq);
double GetLowerFrameRate(double framerateA, double framerateB);

void PrintComparedBlocks(AudioBlocks *ReferenceArray, AudioBlocks *ComparedArray, parameters *config, AudioSignal *Signal);

double CalculateWeightedError(double pError, parameters *config);
double RoundFloat(double x, int p);
long int RoundTo4bytes(double src, int *leftover, int *discard);
double GetDecimalValues(double value);
double BytesToSeconds(long int samplerate, long int bytes);

double CalculateMagnitude(fftw_complex value, long int size);
double CalculateAmplitude(double magnitude, double MaxMagnitude);
double CalculatePhase(fftw_complex value);
double CalculateFrequency(double boxindex, double boxsize, int HertzAligned);

long int GetZeroPadValues(long int *monoSignalSize, double *seconds, long int samplerate);

#endif