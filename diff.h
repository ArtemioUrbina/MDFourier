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

#ifndef MDFOURIER_DIFF_H
#define MDFOURIER_DIFF_H

#include "mdfourier.h"

AmplDifference *CreateAmplDifferences(parameters *config);
FreqDifference *CreateFreqDifferences(parameters *config);
PhaseDifference *CreatePhaseDifferences(parameters *config);
int CreateDifferenceArray(parameters *config);
int InsertFreqNotFound(int block, double freq, double amplitude, parameters *config);
int InsertAmplDifference(int block, Frequency ref, Frequency comp, parameters *config);
int InsertPhaseDifference(int block, Frequency ref, Frequency comp, parameters *config);
int IncrementCmpAmplDifference(int block, parameters *config);
int IncrementCmpFreqNotFound(int block, parameters *config);
int IncrementCompared(int block, parameters *config);
int IncrementPerfectMatch(int block, parameters *config);
void PrintDifferentFrequencies(int block, parameters *config);
void PrintDifferentAmplitudes(int block, parameters *config);
void PrintDifferenceArray(parameters *config);
void ReleaseDifferenceArray(parameters *config);

#endif
