/*
 * MDFourier
 * A Fourier Transform analysis tool to compare game console audio
 * http://junkerhq.net/MDFourier/
 *
 * Copyright (C)2019-2026 Artemio Urbina
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

#ifndef MDFOURIER_SYNCDET_H
#define MDFOURIER_SYNCDET_H

#define SYNCDET_MAX_PULSES	32

typedef struct syncdet_pulse_st {
	long int	rise;		// first step at or above the on threshold
	long int	fall;		// first step below the off threshold
	double		peak;		// loudest level of the pulse, in dB
} SyncDetPulse;

typedef struct syncdet_train_st {
	int				count;							// pulses matched to the grid
	SyncDetPulse	*pulse[SYNCDET_MAX_PULSES];
	int				position[SYNCDET_MAX_PULSES];	// grid position of each pulse
	double			period;							// in steps
} SyncDetTrain;

typedef struct syncdet_level_st {
	double		*dB;		// band level, one value per step
	long int	count;
	long int	step;		// in samples (per channel)
	long int	window;		// in samples (per channel)
	double		stepMS;
	double		floor;		// 5th percentile of the level, in dB
	double		max;
} SyncDetLevel;

typedef struct syncdet_result_st {
	double		startOnset;			// in samples per channel, fractional
	double		endOnset;
	double		samplesPerFrame;	// from the fit over all rises
	long int	frames;				// between the first pulses of both trains
	long int	profileFrames;
	double		pulseHz;
	double		ppm;				// uncertainty of the frame rate
	double		residualMS;			// RMS of the rises around the fit
	double		innerFrames;		// frames between trains from the spacing inside them
	double		innerFramesError;
	int			matched[2];
	int			missingPulse;
	int			wideSearch;
	int			ambiguous;
	int			tries;
	double		onDB;
	double		floorDB;
} SyncDetResult;

int DetectSyncTrains(AudioSignal *Signal, parameters *config);
int SyncDetFind(AudioSignal *Signal, double frequency, int remeasure, SyncDetResult *result, parameters *config);

int SyncDetBandLevel(AudioSignal *Signal, double frequency, double windowMS, double stepMS, SyncDetLevel *level);
void SyncDetFreeLevel(SyncDetLevel *level);
double SyncDetEnvelope(double *Samples, int AudioChannels, long int position, long int window, double *cosTable, double *sinTable);
int SyncDetEnvelopeSpan(AudioSignal *Signal, long int start, long int count, long int window, double frequency, double *amplitude);
int SyncDetMakeTables(long int window, double frequency, double sampleRate, double **cosTable, double **sinTable);

int SyncDetEdges(SyncDetLevel *level, double onDB, double offDB, double minOnMS, double minOffMS, SyncDetPulse **pulses);
int SyncDetFindTrain(SyncDetPulse *pulses, int foundPulses, int gridSize, double periodMin, double periodMax, double tolerance,
					double after, double before, int minMatch, SyncDetTrain *train);
void SyncDetExtendTrain(SyncDetPulse *pulses, int foundPulses, int gridSize, double tolerance, SyncDetTrain *train);
int SyncDetSweep(SyncDetLevel *level, int gridSize, double frameSteps, double profileFrames, double pulseMS, int minMatch, int wide,
					SyncDetPulse **pulses, SyncDetTrain *start, SyncDetTrain *end, double *onDB, int *tries);

double SyncDetMeasureFrequency(AudioSignal *Signal, SyncDetTrain *train, long int step, double frequency, double pulseMS);
double SyncDetRise(AudioSignal *Signal, SyncDetPulse *pulse, SyncDetLevel *level, double frequency);
double SyncDetOnset(AudioSignal *Signal, long int coarse, double frequency);

double SyncDetPercentile(double *values, long int count, double percent);
#endif
