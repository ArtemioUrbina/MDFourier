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
 * Compile with: 
 *	  gcc -Wall -std=gnu99 -o mdfourier mdfourier.c -lfftw3 -lm
 */

#ifndef MDFOURIER_FREQ_H
#define MDFOURIER_FREQ_H

#include "mdfourier.h"

int LoadAudioBlockStructure(parameters *config);
int GetSilenceIndex(parameters *config);
int GetTotalAudioBlocks(parameters *config);
int GetTotalAudioBlocksWithSilence(parameters *config);
double GetLongestBlock(parameters *config);
double GetTotalBlockDuration(parameters *config);
double GetBlockDuration(parameters *config, int pos);
char *GetBlockName(parameters *config, int pos);
int GetBlockSubIndex(parameters *config, int pos);
int GetBlockType(parameters *config, int pos);
double GetFramerateAdjust(parameters *config);
void ReleaseAudioBlockStructure(parameters *config);

AudioSignal *CreateAudioSignal(parameters *config);
void CleanAudio(AudioSignal *Signal, parameters *config);
void ReleaseAudio(AudioSignal *Signal, parameters *config);
void CleanMatched(AudioSignal *ReferenceSignal, AudioSignal *TestSignal, parameters *config);
void FillFrequencyStructures(AudioBlocks *AudioArray, parameters *config);
void PrintFrequencies(AudioSignal *Signal, parameters *config);
void GlobalNormalize(AudioSignal *Signal, parameters *config);
void LocalNormalize(AudioBlocks *AudioArray, parameters *config);
void CompressFrequencies(AudioBlocks *AudioArray, parameters *config);
void FindFloor(AudioSignal *Signal, parameters *config);
int IsCRTNoise(double freq);

void PrintComparedBlocks(AudioBlocks *ReferenceArray, AudioBlocks *ComparedArray, parameters *config, AudioSignal *Signal);
void InsertMessageInBuffer(msgbuff *message, parameters *config);

#endif