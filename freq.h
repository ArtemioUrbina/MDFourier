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

#define roundFloat(number) RoundFloat(number, 4)
#define readLine(buffer, file) if(fgets(buffer, LINE_BUFFER_SIZE, file) == NULL) { logmsg("Invalid mdfblocks.mfn file\n");	return 0;	}

int LoadProfile(parameters *config);
int LoadAudioBlockStructure(FILE *file, parameters *config);
int LoadAudioNoSyncProfile(FILE *file, parameters *config);
int GetFirstSilenceIndex(parameters *config);
int GetFirstMonoIndex(parameters *config);
long int GetLastSilenceByteOffset(double framerate, wav_hdr header, int frameAdjust, double silenceOffset, parameters *config);
int GetActiveAudioBlocks(parameters *config);
int GetTotalAudioBlocks(parameters *config);
long int GetLongestElementFrames(parameters *config);
long int GetSignalTotalFrames(parameters *config);
double GetLastSyncDuration(double framerate, parameters *config);
double GetSignalTotalDuration(double framerate, parameters *config);
long int GetBlockFrames(parameters *config, int pos);
char *GetBlockName(parameters *config, int pos);
int GetBlockSubIndex(parameters *config, int pos);
int GetBlockType(parameters *config, int pos);
char *GetBlockColor(parameters *config, int pos);
char *GetTypeColor(parameters *config, int type);
char *GetTypeName(parameters *config, int type);
void ReleaseAudioBlockStructure(parameters *config);
void PrintAudioBlocks(parameters *config);
long int GetLastSyncFrameOffset(wav_hdr header, parameters *config);
long int GetBlockFrameOffset(int block, parameters *config);
long int GetByteSizeDifferenceByFrameRate(double framerate, long int frames, long int samplerate, parameters *config);
int GetFirstSyncIndex(parameters *config);
int GetLastSyncIndex(parameters *config);
int GetLastSyncElementIndex(parameters *config);
int GetActiveBlockTypes(parameters *config);
int GetInternalSyncTotalLength(int pos, parameters *config);
int GetInternalSyncTone(int pos, parameters *config);
double GetInternalSyncLen(int pos, parameters *config);
char GetBlockChannel(parameters *config, int pos);
char GetTypeChannel(parameters *config, int type);

AudioSignal *CreateAudioSignal(parameters *config);
void CleanAudio(AudioSignal *Signal, parameters *config);
void ReleaseAudio(AudioSignal *Signal, parameters *config);
void CleanMatched(AudioSignal *ReferenceSignal, AudioSignal *TestSignal, parameters *config);
void FillFrequencyStructures(AudioSignal *Signal, AudioBlocks *AudioArray, parameters *config);
void PrintFrequencies(AudioSignal *Signal, parameters *config);
void PrintFrequenciesWMagnitudes(AudioSignal *Signal, parameters *config);
void PrintFrequenciesBlock(AudioSignal *Signal, Frequency *freq, int type, parameters *config);
void PrintFrequenciesBlockMagnitude(AudioSignal *Signal, Frequency *freq, int type, parameters *config);
void GlobalNormalize(AudioSignal *Signal, parameters *config);
void FindMaxMagnitude(AudioSignal *Signal, parameters *config);
void CalculateAmplitudes(AudioSignal *Signal, double ZeroDbMagReference, parameters *config);
void FindFloor(AudioSignal *Signal, parameters *config);
void FindStandAloneFloor(AudioSignal *Signal, parameters *config);
double GetLowerFrameRate(double framerateA, double framerateB);
void CompareFrameRates(double framerate1, double framerate2, parameters *config);

int IsHRefreshNoise(AudioSignal *Signal, double freq);
int IsGridFrequencyNoise(AudioSignal *Signal, double freq);

void PrintComparedBlocks(AudioBlocks *ReferenceArray, AudioBlocks *ComparedArray, parameters *config, AudioSignal *Signal);

double CalculateWeightedError(double pError, parameters *config);
double RoundFloat(double x, int p);
long int RoundTo4bytes(double src, int *leftover, int *discard, double *leftDecimals);
double GetDecimalValues(double value);
double BytesToSeconds(long int samplerate, long int bytes);
double FramesToSeconds(double frames, double framerate);
long int SecondsToBytes(long int samplerate, double seconds, int *leftover, int *discard, double *leftDecimals);
double BytesToFrames(long int samplerate, long int bytes, double framerate);

double CalculateMagnitude(fftw_complex value, long int size);
double CalculateAmplitude(double magnitude, double MaxMagnitude);
double CalculatePhase(fftw_complex value);
double CalculateFrequency(double boxindex, double boxsize, int HertzAligned);
double CalculateFrameRate(AudioSignal *Signal, parameters *config);
double CalculateFrameRateNS(AudioSignal *Signal, double Frames, parameters *config);
double CalculateScanRate(AudioSignal *Signal);

double FindFrequencyBinSizeForBlock(AudioSignal *Signal, long int block);
long int GetZeroPadValues(long int *monoSignalSize, double *seconds, long int samplerate);
void CalcuateFrequencyBrackets(AudioSignal *signal, parameters *config);
double FindFrequencyBracket(double frequency, size_t size, long samplerate);

double FindDifferenceAverage(parameters *config);
void SubstractDifferenceAverage(parameters *config, double average);
int FindDifferenceTypeTotals(int type, long int *cntAmplBlkDiff, long int *cmpAmplBlkDiff, parameters *config);
int FindMissingTypeTotals(int type, long int *cntFreqBlkDiff, long int *cmpFreqBlkDiff, parameters *config);

int GetPulseSyncFreq(int role, parameters *config);
int getPulseCount(int role, parameters *config);
int getPulseFrameLen(int role, parameters *config);
double getMSPerFrameInternal(int role, parameters *config);
int GetLineCount(int role, parameters *config);
double GetMSPerFrame(AudioSignal *Signal, parameters *config);
double GetMSPerFrameRole(int role, parameters *config);

#endif