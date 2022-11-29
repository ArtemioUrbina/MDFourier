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

#ifndef MDFOURIER_FREQ_H
#define MDFOURIER_FREQ_H

#include "mdfourier.h"

//#define roundFloat(number) RoundFloat(number, 4)
#define roundFloat(number) number

int areDoublesEqual(double a, double b);

int LoadAudioBlockStructure(FILE *file, parameters *config);
int LoadAudioNoSyncProfile(FILE *file, parameters *config);
int GetFirstSilenceIndex(parameters *config);
int GetFirstMonoIndex(parameters *config);
long int GetSecondSyncSilenceSampleOffset(double framerate, wav_hdr header, int frameAdjust, double silenceOffset, parameters *config);
int GetActiveAudioBlocks(parameters *config);
int GetTotalAudioBlocks(parameters *config);
long int GetLongestElementFrames(parameters *config);
long int GetSignalTotalFrames(parameters *config);
double GetFirstSyncDuration(double framerate, parameters *config);
double GetLastSyncDuration(double framerate, parameters *config);
double GetSignalTotalDuration(double framerate, parameters *config);
double GetFirstSilenceDuration(double framerate, parameters *config);
long int GetBlockFrames(parameters *config, int pos);
long int GetBlockCutFrames(parameters *config, int pos);
char *GetBlockName(parameters *config, int pos);
char *GetBlockDisplayName(parameters* config, int pos);
int GetBlockSubIndex(parameters *config, int pos);
int GetBlockType(parameters *config, int pos);
char *GetBlockColor(parameters *config, int pos);
int GetBlockElements(parameters *config, int pos);
char *GetTypeColor(parameters *config, int type);
char *GetTypeName(parameters *config, int type);
char *GetTypeDisplayName(parameters *config, int type);
void ReleaseAudioBlockStructure(parameters *config);
void PrintAudioBlocks(parameters *config);
void ReleasePCM(AudioSignal *Signal);
long int GetLastSyncFrameOffset(parameters *config);
long int GetBlockFrameOffset(int block, parameters *config);
long int GetElementFrameOffset(int block, parameters *config);
long int GetSampleSizeDifferenceByFrameRate(double framerate, long int frames, double samplerate, int AudioChannels, parameters *config);
double GetFirstElementFrameOffset(parameters* config);
int GetFirstSyncIndex(parameters *config);
int GetLastSyncIndex(parameters *config);
int GetLastSyncElementIndex(parameters *config);
int GetActiveBlockTypesNoRepeat(parameters *config);
int GetActiveBlockTypesNoRepeatArray(int **types, parameters *config);
int GetInternalSyncTotalLength(int pos, parameters *config);
int GetRemainingLengthFromElement(int pos, parameters *config);
int GetInternalSyncTone(int pos, parameters *config);
double GetInternalSyncLen(int pos, parameters *config);
char *GetInternalSyncSequentialName(int sequence, parameters *config);
char GetBlockChannel(parameters *config, int pos);
char GetBlockMaskType(parameters *config, int pos);
char GetTypeChannel(parameters *config, int type);
void CleanName(char *name, char *display);
int MatchesPreviousType(int pos, int type, parameters *config);
void CheckSilenceOverride(parameters *config);
int ConvertAudioTypeForProcessing(int type, parameters *config);
int getArrayIndexforType(int type, int *typeArray, int typeCount);
long int GetBlockFreqSize(AudioSignal *Signal, int block, char channel, parameters *config);

AudioSignal *CreateAudioSignal(parameters *config);
void CleanFrequency(Frequency *freq);
void CleanFrequenciesInBlock(AudioBlocks * AudioArray, parameters *config);
void ReleaseFFTW(AudioBlocks * AudioArray);
void CleanAndReleaseFFTW(AudioBlocks * AudioArray);
void ReleaseSamples(AudioBlocks * AudioArray);
void ReleaseFrequencies(AudioBlocks * AudioArray);
void ReleaseBlock(AudioBlocks *AudioArray);
void InitAudio(AudioSignal *Signal, parameters *config);
int InitFreqStruc(Frequency **freq, parameters *config);
int InitAudioBlock(AudioBlocks* block, char channel, char maskType, parameters *config);
int initInternalSync(AudioBlocks * AudioArray, int size);
void ReleaseAudio(AudioSignal *Signal, parameters *config);
void CleanMatched(AudioSignal *ReferenceSignal, AudioSignal *TestSignal, parameters *config);
int FillFrequencyStructures(AudioSignal *Signal, AudioBlocks *AudioArray, parameters *config);
int FillFrequencyStructuresInternal(AudioSignal *Signal, AudioBlocks *AudioArray, char channel, parameters *config);
void PrintFrequencies(AudioSignal *Signal, parameters *config);
void PrintFrequenciesWMagnitudes(AudioSignal *Signal, parameters *config);
void PrintFrequenciesBlock(AudioSignal *Signal, Frequency *freq, long int size, int type, parameters *config);
void PrintFrequenciesBlockMagnitude(AudioSignal *Signal, Frequency *freq, parameters *config);
void GlobalNormalize(AudioSignal *Signal, parameters *config);
void FindMaxMagnitude(AudioSignal *Signal, parameters *config);
void CalculateAmplitudes(AudioSignal *Signal, double ZeroDbMagReference, parameters *config);
void FindFloor(AudioSignal *Signal, parameters *config);
void FindStandAloneFloor(AudioSignal *Signal, parameters *config);
double GetLowerFrameRate(double framerateA, double framerateB);
void CompareFrameRates(AudioSignal *Signal1, AudioSignal *Signal2, parameters *config);
void CompareFrameRatesMDW(AudioSignal *Signal, double framerate, parameters *config);
double CalculatePCMMagnitude(double amplitude, double MaxMagnitude);

int IsHRefreshNoise(AudioSignal *Signal, double freq);
int IsHRefreshNoiseCrossTalk(AudioSignal *Signal, double freq);
int IsGridFrequencyNoise(AudioSignal *Signal, double freq);

void PrintComparedBlocks(AudioBlocks *ReferenceArray, AudioBlocks *ComparedArray, parameters *config);
void PrintThesholdDifferenceBlocks(AudioBlocks *ReferenceArray, AudioBlocks *ComparedArray, parameters *config, double threshold);

int CalculateTimeDurations(AudioSignal *Signal, parameters *config);
double CalculateWeightedError(double pError, parameters *config);
double RoundFloat(double x, int p);
long int RoundToNsamples(double src, int AudioChannels, int *discard, double *leftDecimals);
double GetDecimalValues(double value);
double SamplesToSeconds(double samplerate, long int samples, int AudioChannels);
double FramesToSeconds(double frames, double framerate);
double SecondsToFrames(double seconds, double framerate);
long int SecondsToSamples(double samplerate, double seconds, int AudioChannels, int *discard, double *leftDecimals);
double FramesToSamples(double frames, double samplerate, double framerate);
double SamplesToFrames(double samplerate, long int samples, double framerate, int AudioChannels);
long int SamplesToBytes(long int samples, int bytesPerSample);
long int SamplesForDisplay(long int samples, int AudioChannels);
int DetectWatermark(AudioSignal *Signal, parameters *config);
int DetectWatermarkIssue(char* msg, AudioSignal* Signal, parameters* config);

double CalculateMagnitude(fftw_complex *value, double factor);
double CalculateAmplitude(double magnitude, double MaxMagnitude);
double CalculateAmplitudeInternal(double magnitude, double MaxMagnitude);
double CalculatePhase(fftw_complex *value);
double CalculateFrequency(double boxindex, double boxsize);
double CalculateFrameRate(AudioSignal *Signal, parameters *config);
double CalculateFrameRateNS(AudioSignal *Signal, double Frames, parameters *config);
double CalculateFrameRateAndCheckSamplerate(AudioSignal *Signal, parameters *config);
double CalculateScanRate(AudioSignal *Signal);
double CalculateScanRateOriginalFramerate(AudioSignal *Signal);

double FindFrequencyBinSizeForBlock(AudioSignal *Signal, long int block);
long int GetZeroPadValues(long int *monoSignalSize, double *seconds, double samplerate);
long int GetBlockZeroPadValues(long int *monoSignalSize, double *seconds, double maxBlockSeconds, double samplerate);
void CalculateFrequencyBrackets(AudioSignal *signal, parameters *config);
double FindFrequencyBracket(double frequency, size_t size, int AudioChannels, double samplerate, parameters *config);
double FindFrequencyBracketForSync(double frequency, size_t size, int AudioChannels, double samplerate, parameters *config);
double FindFundamentalAmplitudeAverage(AudioSignal *Signal, parameters *config);

char GetTypeProfileName(int type);
int GetPulseSyncFreq(int role, parameters *config);
int getPulseCount(int role, parameters *config);
int getPulseFrameLen(int role, parameters *config);
double getMSPerFrameInternal(int role, parameters *config);
int GetLineCount(int role, parameters *config);
double GetMSPerFrame(AudioSignal *Signal, parameters *config);
double GetMSPerFrameRole(int role, parameters *config);
char *GetFileName(int role, parameters *config);
double CalculateClk(AudioSignal *Signal, parameters *config);
int CalculateCLKAmplitudes(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config);

long int GetSignalMaxInt(AudioSignal *Signal);
long int GetSignalMinInt(AudioSignal *Signal);

double GetSignalMinDBFS(AudioSignal *Signal);
void SetAmplitudeMatchByDuration(AudioSignal *reference, parameters *config);
void SetAmplitudeMatchByDurationMDW(AudioSignal *reference, parameters *config);
#endif
