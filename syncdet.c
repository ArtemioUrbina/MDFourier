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

#include "mdfourier.h"
#include "syncdet.h"
#include "log.h"
#include "freq.h"

int syncDetRefit = 0;	// last pass used in SyncDetFindTrain

/* 
	Values tuned from the previois sync detection and test cases
*/

#define SYNCDET_STEP_MS			0.5		// only to find the pulses
#define SYNCDET_WINDOW_MS		1.0
#define SYNCDET_SWEEP_DB		3.0
#define SYNCDET_FIRST_DB		3.0
#define SYNCDET_HYSTERESIS_DB	6.0		// off threshold below on
#define SYNCDET_FLOOR_DB		12.0
#define SYNCDET_FLOOR_PERCENT	5.0
#define SYNCDET_MIN_OFF_MS		2.0		// shorter gaps are dropouts
#define SYNCDET_GRID_MS			2.0
#define SYNCDET_PERIOD_MIN		0.9		// in frames
#define SYNCDET_PERIOD_MAX		4.2
#define SYNCDET_END_WINDOW		0.005	// +-0.5% around the closing sync
#define SYNCDET_WIDE_START		0.9
#define SYNCDET_RISE			0.25
#define SYNCDET_ONSET			0.10
#define SYNCDET_ONSET_SIGMA		4.0
#define SYNCDET_ONSET_PEAK		0.02
#define SYNCDET_FREQ_RANGE		0.45
#define SYNCDET_FREQ_STEP		2.0
#define SYNCDET_FREQ_RERUN		0.01
#define SYNCDET_NOMINAL_TOL		0.001
#define SYNCDET_FRACTION_TOL	0.25

#define SYNCDET_IDLE			0
#define SYNCDET_RISING			1
#define SYNCDET_ON				2
#define SYNCDET_FALLING			3

int DetectSyncTrains(AudioSignal *Signal, parameters *config)
{
	SyncDetResult	result;
	int				AudioChannels = 0;
	double			frequency = 0;
	
	AudioChannels = Signal->AudioChannels;
	frequency = GetPulseSyncFreq(Signal->role, config);

	memset(&result, 0, sizeof(SyncDetResult));
	if(!SyncDetFind(Signal, frequency, 1, &result, config))
		return 0;
	
	Signal->startOffset = (long int)floor(result.startOnset + 0.5)*AudioChannels;
	Signal->endOffset = (long int)floor(result.endOnset + 0.5)*AudioChannels;
	Signal->syncFrames = result.frames;
	Signal->syncSamplesPerFrame = result.samplesPerFrame;
	
	if (result.missingPulse)
		logmsg(" - WARNING: Sync pulse trains have %d and %d of %d pulses\n", 
			result.matched[0], result.matched[1], getPulseCount(Signal->role, config));
	if(result.frames != result.profileFrames)
		logmsg(" - Sync: closing pulse train is %ld frames %s than the profile places it\n",
			labs(result.frames - result.profileFrames), result.frames > result.profileFrames ? "later" : "earlier");
	if(result.ambiguous)
		logmsg(" - WARNING: The frame count between sync pulse trains is uncertain (+-%.1f frames), consider using -m\n", 
			result.innerFramesError);

	if(config->verbose)
	{
		if(fabs(result.pulseHz/frequency - 1.0) > SYNCDET_FREQ_RERUN)
			logmsg(" - Sync: pulses measured at %g Hz, the profile defines %g Hz\n", result.pulseHz, frequency);
		logmsg(" - Sync: start %ld end %ld frames %ld (profile %ld) %.8g Hz +-%.2f ppm residual %.3f ms\n",
			SamplesForDisplay(Signal->startOffset, AudioChannels), SamplesForDisplay(Signal->endOffset, AudioChannels), 
			result.frames, result.profileFrames, Signal->SampleRate/result.samplesPerFrame,
			result.ppm, result.residualMS);
	}
	return 1;
}

// Some pulses are a slightly different frequency than what teh profile says, we measure and search again
int SyncDetFind(AudioSignal *Signal, double frequency, int remeasure, SyncDetResult *result, parameters *config)
{
	SyncDetLevel	level;
	SyncDetPulse	*pulses = NULL;
	SyncDetTrain	start, end;
	int				pulseCount = 0, startCount = 0, riseCount = 0, periodFrames = 0, tries = 0, degreesOfFreedom = 0;
	double			pulseMS = 0, msPerFrame = 0, frameSteps = 0, onDB = 0;
	double			riseFrame[2*SYNCDET_MAX_PULSES], riseSample[2*SYNCDET_MAX_PULSES], onsetSample[2*SYNCDET_MAX_PULSES];
	double			measuredHz = 0, nominalSamples = 0, samplesPerFrame = 0, frameSpread = 0, frameSampleSpread = 0, trainDistance = 0;
	double			meanFrame[2], meanSample[2], residual = 0, innerSigma = 0, periodVariance = 0, firstFrameSample = 0, meanAllFrames = 0, meanAllSamples = 0;
	long int		profileFrames = 0, trainFrames = 0;
	
	memset(&level, 0, sizeof(SyncDetLevel));
	memset(&start, 0, sizeof(SyncDetTrain));
	memset(&end, 0, sizeof(SyncDetTrain));
	
	pulseCount = getPulseCount(Signal->role, config);
	if (pulseCount < 3 || pulseCount > SYNCDET_MAX_PULSES)
	{
		logmsg(" - Sync: %d pulses per train is out of range for the sync detector\n", pulseCount);
		return 0;
	} 
	pulseMS = getPulseFrameLen(Signal->role, config);
	msPerFrame = GetMSPerFrameRole(Signal->role, config);
	profileFrames = GetLastSyncFrameOffset(config);
	if(!profileFrames || !msPerFrame)
		return 0;

	if(!SyncDetBandLevel(Signal, frequency, SYNCDET_WINDOW_MS, SYNCDET_STEP_MS, &level))
		return 0;
	frameSteps = msPerFrame/level.stepMS;

	if(config->debugSync)
		logmsgFileOnly("\nSync detector: %g Hz, %ld steps of %gms, floor %0.2f dB, max %0.2f dB\n", 
			frequency, level.count, level.stepMS, level.floor, level.max);
	
	/* Strict first, then the closing train anywhere after 90%, then allowing a missing pulse,
	   last refitting the period */
	result->wideSearch = 0;
	result->missingPulse = 0;
	result->refitPeriod = 0;
	if (!SyncDetSweep(&level, pulseCount, frameSteps, profileFrames, pulseMS, pulseCount, 0, &pulses, &start, &end, &onDB, &tries))
	{
		if(SyncDetSweep(&level, pulseCount, frameSteps, profileFrames, pulseMS, pulseCount, 1, &pulses, &start, &end, &onDB, &tries))
			result->wideSearch = 1;
		else if(SyncDetSweep(&level, pulseCount, frameSteps, profileFrames, pulseMS, pulseCount - 1, 0, &pulses, &start, &end, &onDB, &tries))
			result->missingPulse = 1;
		else if(SyncDetSweep(&level, pulseCount, frameSteps, profileFrames, pulseMS, pulseCount - 1, 1, &pulses, &start, &end, &onDB, &tries))
			result->wideSearch = result->missingPulse = 1;
		else
		{
			syncDetRefit = 1;
			if(SyncDetSweep(&level, pulseCount, frameSteps, profileFrames, pulseMS, pulseCount, 0, &pulses, &start, &end, &onDB, &tries))
				result->refitPeriod = 1;
			else if(SyncDetSweep(&level, pulseCount, frameSteps, profileFrames, pulseMS, pulseCount, 1, &pulses, &start, &end, &onDB, &tries))
				result->refitPeriod = result->wideSearch = 1;
			syncDetRefit = 0;
		}
		if(!result->refitPeriod && !result->wideSearch && !result->missingPulse)
		{
			if(config->debugSync)
				logmsgFileOnly("Sync detector: no pulse trains found after %d thresholds\n", tries); 
			SyncDetFreeLevel(&level);
			return 0;
		}
	}
	if(start.count < pulseCount || end.count < pulseCount)
		result->missingPulse = 1;
	else
		result->missingPulse = 0;

	if(remeasure)
	{
		measuredHz = SyncDetMeasureFrequency(Signal, &start, level.step, frequency, pulseMS);
		if(measuredHz > 0 && fabs(measuredHz/frequency - 1.0) > SYNCDET_FREQ_RERUN)
		{
			if(config->debugSync)
				logmsgFileOnly("Sync detector: pulses at %g Hz instead of %g Hz, searching again\n", measuredHz, frequency);
			free(pulses);
			SyncDetFreeLevel(&level);
			return(SyncDetFind(Signal, measuredHz, 0, result, config));
		}
	}
	result->pulseHz = frequency;

	periodFrames = (int)floor(start.period/frameSteps + 0.5);
	if(periodFrames < 1)
		periodFrames = 1;
	for(int trainIndex = 0; trainIndex < 2; trainIndex++)
	{
		SyncDetTrain *train = trainIndex == 0 ? &start : &end;

		for(int pulseIndex = 0; pulseIndex < train->count; pulseIndex++)
		{
			riseSample[riseCount] = SyncDetRise(Signal, train->pulse[pulseIndex], &level, frequency);
			onsetSample[riseCount] = SyncDetOnset(Signal, train->pulse[pulseIndex]->rise*level.step, frequency);
			if(onsetSample[riseCount] < 0)
				onsetSample[riseCount] = riseSample[riseCount];
			riseFrame[riseCount] = train->position[pulseIndex]*periodFrames;
			riseCount ++;
		}
	}
	startCount = start.count;
	
	// Frame period from inside the trains, first pulses are off the grid so they are left out
	for(int trainIndex = 0; trainIndex < 2; trainIndex++) 
	{ 
		int first = trainIndex == 0 ? 1 : startCount + 1, last = trainIndex == 0 ? startCount : riseCount, used = 0;

		meanFrame[trainIndex] = meanSample[trainIndex] = 0;
		for(int i = first; i < last; i++)
		{
			meanFrame[trainIndex] += riseFrame[i];
			meanSample[trainIndex] += riseSample[i];
			used ++;
		}
		if(used)
		{
			meanFrame[trainIndex] /= used;
			meanSample[trainIndex] /= used;
		}
	}
	frameSpread = frameSampleSpread = 0;
	for(int i = 0; i < riseCount; i++)
	{
		int trainIndex = i < startCount ? 0 : 1;
		
		if(i == 0 || i == startCount)
			continue;
		frameSpread += (riseFrame[i] - meanFrame[trainIndex])*(riseFrame[i] - meanFrame[trainIndex]);
		frameSampleSpread += (riseFrame[i] - meanFrame[trainIndex])*(riseSample[i] - meanSample[trainIndex]);
	}
	if(frameSpread <= 0)
	{
		free(pulses);
		SyncDetFreeLevel(&level);
		return 0;
	}
	samplesPerFrame = frameSampleSpread/frameSpread;
	residual = 0;
	for(int i = 0; i < riseCount; i++)
	{
		int		trainIndex = i < startCount ? 0 : 1;
		double	distance = 0;

		if(i == 0 || i == startCount)
			continue;
		distance = riseSample[i] - (meanSample[trainIndex] + samplesPerFrame*(riseFrame[i] - meanFrame[trainIndex]));
		residual += distance*distance;
	}
	degreesOfFreedom = riseCount - 2 - 3;
	if(degreesOfFreedom < 1)
		degreesOfFreedom = 1;
	innerSigma = sqrt(residual/degreesOfFreedom)/sqrt(frameSpread);
	
	/* Frames between trains: the profile's if they match, counted with the profile
	rate if it is close (the Sega CD waits for the CD track), or the inner period */
	nominalSamples = msPerFrame*Signal->SampleRate/1000.0;
	trainDistance = (riseSample[startCount] - riseFrame[startCount]*samplesPerFrame) - (riseSample[0] - riseFrame[0]*samplesPerFrame);
	result->innerFrames = trainDistance/samplesPerFrame;
	result->innerFramesError = trainDistance*innerSigma/(samplesPerFrame*samplesPerFrame);
	result->ambiguous = 0;
	if (fabs(result->innerFrames - profileFrames) <= fmax(1.0, 3.0*result->innerFramesError))
		trainFrames = profileFrames;
	else if(fabs(samplesPerFrame/nominalSamples - 1.0) < SYNCDET_NOMINAL_TOL && fabs(trainDistance/nominalSamples - floor(trainDistance/nominalSamples + 0.5)) < SYNCDET_FRACTION_TOL)
		trainFrames = (long int)floor(trainDistance/nominalSamples + 0.5);	
	else
	{
		trainFrames = (long int)floor(result->innerFrames + 0.5);
		if (3.0*result->innerFramesError > 0.5 || fabs(result->innerFrames - trainFrames) > 0.25) 
			result->ambiguous = 1;
	}
	for(int i = startCount; i < riseCount; i++)
		riseFrame[i] += trainFrames;

	// Frame rate from all the rises
	meanAllFrames = meanAllSamples = 0;
	for(int i = 0; i < riseCount; i++)
	{
		meanAllFrames += riseFrame[i];
		meanAllSamples += riseSample[i];
	}
	meanAllFrames /= riseCount;
	meanAllSamples /= riseCount;
	frameSpread = frameSampleSpread = 0;
	for(int i = 0; i < riseCount; i++)
	{
		frameSpread += (riseFrame[i] - meanAllFrames)*(riseFrame[i] - meanAllFrames);
		frameSampleSpread += (riseFrame[i] - meanAllFrames)*(riseSample[i] - meanAllSamples);
	}
	samplesPerFrame = frameSampleSpread/frameSpread;
	firstFrameSample = meanAllSamples - samplesPerFrame*meanAllFrames;
	residual = 0;
	for(int i = 0; i < riseCount; i++)
	{
		double distance = riseSample[i] - (firstFrameSample + samplesPerFrame*riseFrame[i]);
		residual += distance*distance;
	}
	periodVariance = residual/(riseCount > 2 ? riseCount - 2 : 1)/frameSpread;
	
	result->samplesPerFrame = samplesPerFrame;
	result->ppm = sqrt(periodVariance)/samplesPerFrame*1000000.0;
	result->residualMS = sqrt(residual/riseCount)/Signal->SampleRate*1000.0;
	result->frames = trainFrames;
	result->profileFrames = profileFrames;
	result->startOnset = onsetSample[0];
	result->endOnset = onsetSample[startCount];
	result->matched[0] = start.count;
	result->matched[1] = end.count;
	result->tries = tries;
	result->onDB = onDB;
	result->floorDB = level.floor;

	if(config->debugSync)
	{
		logmsgFileOnly("Sync detector: on %0.2f dB after %d thresholds, %d+%d pulses, period %d frames%s%s%s\n",
			onDB, tries, start.count, end.count, periodFrames, result->missingPulse ? ", one missing" : "", result->wideSearch ? ", wide search" : "",
			result->refitPeriod ? ", period refit" : "");
		for(int i = 0; i < riseCount; i++)
			logmsgFileOnly("\tframe %6ld rise %12.2f onset %12.2f fit %+0.4fms\n", (long int)riseFrame[i], riseSample[i], onsetSample[i],
				(riseSample[i] - (firstFrameSample + samplesPerFrame*riseFrame[i]))/Signal->SampleRate*1000.0);
		logmsgFileOnly("Sync detector: inner %0.2f +-%0.2f frames, profile %ld, used %ld\n", 
			result->innerFrames, result->innerFramesError, profileFrames, trainFrames);
	}

	free(pulses);
	SyncDetFreeLevel(&level);
	return 1;
}

int SyncDetBandLevel(AudioSignal *Signal, double frequency, double windowMS, double stepMS, SyncDetLevel *level)
{
	double		*cosTable = NULL, *sinTable = NULL, *sorted = NULL;
	long int	totalFrames = 0;

	level->step = (long int)floor(stepMS*Signal->SampleRate/1000.0 + 0.5);
	if(level->step < 1)
		level->step = 1;
	level->window = (long int)floor(windowMS*Signal->SampleRate/1000.0 + 0.5);
	if(level->window < 1)
		level->window = 1;
	level->stepMS = level->step*1000.0/Signal->SampleRate;
	
	totalFrames = Signal->numSamples/Signal->AudioChannels;
	if(totalFrames <= level->window)
		return 0;
	level->count = (totalFrames - level->window)/level->step + 1;

	if(!SyncDetMakeTables(level->window, frequency, Signal->SampleRate, &cosTable, &sinTable))
		return 0;

	level->dB = (double*)malloc(sizeof(double)*level->count);
	if(!level->dB)
	{
		logmsg("ERROR: Not enough memory for the sync level\n");
		free(cosTable);
		free(sinTable);
		return 0;
	}

	level->max = -1000;
	for(long int i = 0; i < level->count; i++)
	{
		double amplitude = SyncDetEnvelope(Signal->Samples, Signal->AudioChannels, i*level->step, level->window, cosTable, sinTable);
		
		level->dB[i] = 20.0*log10(amplitude + 1e-10);
		if(level->dB[i] > level->max)
			level->max = level->dB[i];
	}
	free(cosTable);
	free(sinTable);
	
	// the percentile sorts
	sorted = (double*)malloc(sizeof(double)*level->count);
	if(!sorted)
	{
		logmsg("ERROR: Not enough memory for the sync level\n");
		SyncDetFreeLevel(level);
		return 0;
	}
	memcpy(sorted, level->dB, sizeof(double)*level->count);
	level->floor = SyncDetPercentile(sorted, level->count, SYNCDET_FLOOR_PERCENT);
	free(sorted);
	return 1;
}

void SyncDetFreeLevel(SyncDetLevel *level)
{
	if(level->dB)
		free(level->dB); 
	level->dB = NULL;
	level->count = 0;
}

int SyncDetMakeTables(long int window, double frequency, double sampleRate, double **cosTable, double **sinTable)
{
	*cosTable = (double*)malloc(sizeof(double)*window);
	*sinTable = (double*)malloc(sizeof(double)*window);
	if(!*cosTable || !*sinTable)	
	{
		logmsg("ERROR: Not enough memory for the sync tables\n");
		if(*cosTable)
			free(*cosTable);
		if (*sinTable)
			free(*sinTable);
		*cosTable = *sinTable = NULL;
		return 0;
	}
	for(long int sample = 0; sample < window; sample++)
	{
		(*cosTable)[sample] = cos(2.0*M_PI*frequency*sample/sampleRate);
		(*sinTable)[sample] = sin(2.0*M_PI*frequency*sample/sampleRate);
	}
	return 1;
}

double SyncDetEnvelope(double *Samples, int AudioChannels, long int position, long int window, double *cosTable, double *sinTable)
{
	double power = 0;

	for(int channel = 0; channel < AudioChannels; channel++)
	{
		double	real = 0, imaginary = 0;
		double	*channelSamples = Samples + position*AudioChannels + channel;

		for(long int sample = 0; sample < window; sample++) 
		{
			real += channelSamples[sample*AudioChannels]*cosTable[sample];
			imaginary -= channelSamples[sample*AudioChannels]*sinTable[sample];
		}
		real /= window;
		imaginary /= window;
		power += real*real + imaginary*imaginary;
	}
	return(sqrt(power));
}

int SyncDetEnvelopeSpan(AudioSignal *Signal, long int start, long int count, long int window, double frequency, double *amplitude)
{ 
	double		*cosTable = NULL, *sinTable = NULL;
	long int	totalFrames = 0;

	if(!SyncDetMakeTables(window, frequency, Signal->SampleRate, &cosTable, &sinTable))
		return 0;

	totalFrames = Signal->numSamples/Signal->AudioChannels;
	for(long int i = 0; i <= count; i++)
	{
		long int position = start + i;

		if(position < 0 || position + window > totalFrames)
			amplitude[i] = 0;
		else
			amplitude[i] = SyncDetEnvelope(Signal->Samples, Signal->AudioChannels, position, window, cosTable, sinTable);
	}
	free(cosTable);
	free(sinTable);
	return 1;
}

int SyncDetEdges(SyncDetLevel *level, double onDB, double offDB, double minOnMS, double minOffMS, SyncDetPulse **pulses)
{
	int			state = SYNCDET_IDLE, found = 0, allocated = 0;
	long int	rise = 0, fall = 0, steps = 0, minOn = 0, minOff = 0;

	minOn = (long int)(minOnMS/level->stepMS);
	minOff = (long int)(minOffMS/level->stepMS);
	*pulses = NULL;

	for(long int i = 0; i < level->count; i++)
	{
		double value = level->dB[i];

		switch(state)
		{
			case SYNCDET_IDLE:
				if(value >= onDB)
				{
					state = SYNCDET_RISING;
					rise = i;
					steps = 1;
				}
				break;
			case SYNCDET_RISING:
				if(value < offDB)
					state = SYNCDET_IDLE;	// glitch
				else
				{
					steps ++;
					if(steps >= minOn)
						state = SYNCDET_ON;
				}
				break;
			case SYNCDET_ON:
				if(value < offDB)
				{
					state = SYNCDET_FALLING;
					fall = i;
					steps = 1;
				}
				break;
			case SYNCDET_FALLING:
				if (value >= onDB)
					state = SYNCDET_ON;		// dropout
				else
				{
					steps ++;
					if(steps >= minOff)
					{
						if(found == allocated)
						{
							SyncDetPulse *resized = NULL;

							allocated = allocated ? allocated*2 : 256;
							resized = (SyncDetPulse*)realloc(*pulses, sizeof(SyncDetPulse)*allocated);
							if(!resized)
							{
								logmsg("ERROR: Not enough memory for sync pulses\n");
								free(*pulses);
								*pulses = NULL;
								return 0;
							}
							*pulses = resized;
						}
						(*pulses)[found].rise = rise;
						(*pulses)[found].fall = fall;
						(*pulses)[found].peak = level->dB[rise];
						for(long int step = rise; step < fall; step++)
						{
							if (level->dB[step] > (*pulses)[found].peak)
								(*pulses)[found].peak = level->dB[step];
						}
						found ++;
						state = SYNCDET_IDLE;
					}
				}
				break;
		}
	}
	return found;
}

// Least squares period from the matched rises
double SyncDetFitPeriod(SyncDetTrain *train, double period)
{
	double	meanPosition = 0, meanRise = 0, positionSpread = 0, positionRiseSpread = 0;

	if(train->count < 3)
		return period;
	for(int i = 0; i < train->count; i++)
	{
		meanPosition += train->position[i];
		meanRise += train->pulse[i]->rise;
	}
	meanPosition /= train->count;
	meanRise /= train->count;
	for(int i = 0; i < train->count; i++)
	{
		positionSpread += (train->position[i] - meanPosition)*(train->position[i] - meanPosition);
		positionRiseSpread += (train->position[i] - meanPosition)*(train->pulse[i]->rise - meanRise);
	}	
	
	if(positionSpread > 0)
		period = positionRiseSpread/positionSpread;
	return period;
}

// Grid positions from firstRise, the nearest rise within tolerance at each
void SyncDetMatchGrid(SyncDetPulse *pulses, int foundPulses, double firstRise, double period, int gridSize, double tolerance, SyncDetTrain *train)
{
	train->count = 0;
	for(int position = 0; position < gridSize; position++)
	{
		double		expected = firstRise + position*period;
		int			low = 0, high = foundPulses - 1, nearest = 0;
		
		// nearest rise
		while(low < high)
		{	
			int middle = (low + high)/2;
			
			if(pulses[middle].rise < expected)
				low = middle + 1;
			else
				high = middle;
		}
		nearest = low;
		
		if(low > 0 && fabs(pulses[low - 1].rise - expected) < fabs(pulses[low].rise - expected))
			nearest = low - 1;
		if(fabs(pulses[nearest].rise - expected) <= tolerance)
		{
			train->pulse[train->count] = &pulses[nearest];
			train->position[train->count] = position;
			train->count ++;
		}
	}
}

// before < 0 is no limit
int SyncDetFindTrain(SyncDetPulse *pulses, int foundPulses, int gridSize, double periodMin, double periodMax, double tolerance, 
					double after, double before, int minMatch, SyncDetTrain *train)
{
	for(int candidate = 0; candidate < foundPulses; candidate++)
	{
		double	firstRise = pulses[candidate].rise, period = 0, spacing[SYNCDET_MAX_PULSES], shortest = 0;
		int		spaces = 0, matchedCount = 0;
		
		if(firstRise < after || (before >= 0 && firstRise > before))
			continue;
		for(int i = candidate + 1; i < candidate + gridSize && i < foundPulses; i++)
			spacing[spaces++] = pulses[i].rise - pulses[i - 1].rise;
		if(!spaces)
			break;

		// with a pulse missing one gap is two periods
		if(minMatch < gridSize)
		{
			int singleSpaces = 0;

			shortest = spacing[0];
			for(int i = 1; i < spaces; i++)
				if(spacing[i] < shortest)
					shortest = spacing[i];
			for(int i = 0; i < spaces; i++) 
				if(spacing[i] < 1.5*shortest)
					spacing[singleSpaces++] = spacing[i];
			spaces = singleSpaces;
		}
		period = SyncDetPercentile(spacing, spaces, 50.0);
		if (period < periodMin || period > periodMax)
			continue; 

		SyncDetMatchGrid(pulses, foundPulses, firstRise, period, gridSize, tolerance, train);
		// the median of quantized gaps can be off in emulators
		// fit the period from the rises it got and try again. Last pass only
		if(syncDetRefit && train->count > 2 && train->count < gridSize && train->position[0] == 0)
		{
			SyncDetTrain	refit;

			SyncDetMatchGrid(pulses, foundPulses, firstRise, SyncDetFitPeriod(train, period), gridSize, tolerance, &refit);
			if(refit.count > train->count)
				*train = refit;
		}
		matchedCount = train->count;
		if(matchedCount >= minMatch && train->position[0] == 0)
		{
			period = SyncDetFitPeriod(train, period);
			train->period = period;
			return 1;
		}
	}
	train->count = 0;
	return 0;
}

// Found from a later point the train can start one pulse late, we move it back
void SyncDetExtendTrain(SyncDetPulse *pulses, int foundPulses, int gridSize, double tolerance, SyncDetTrain *train)
{
	while(train->count > 0)
	{	
		double	expected = train->pulse[0]->rise - train->period;
		int		earlier = -1;
		
		for(int i = 0; i < foundPulses; i++)
		{
			if(fabs(pulses[i].rise - expected) <= tolerance)
			{
				earlier = i;
				break;
			}
		}
		if(earlier < 0)
			return;

		for(int i = train->count; i > 0; i--)
		{
			train->pulse[i] = train->pulse[i - 1];
			train->position[i] = train->position[i - 1] + 1;
		}
		train->pulse[0] = &pulses[earlier];
		train->position[0] = 0;
		train->count ++;
		while(train->count > 0 && train->position[train->count - 1] > gridSize - 1)
			train->count --;
	}
} 

int SyncDetSweep(SyncDetLevel *level, int gridSize, double frameSteps, double profileFrames, double pulseMS, int minMatch, int wide,
					SyncDetPulse **pulses, SyncDetTrain *start, SyncDetTrain *end, double *onDB, int *tries)
{
	double	threshold = 0, tolerance = 0, periodMin = 0, periodMax = 0, interval = 0; 
	int		foundPulses = 0;
	
	tolerance = SYNCDET_GRID_MS/level->stepMS;
	periodMin = SYNCDET_PERIOD_MIN*frameSteps;
	periodMax = SYNCDET_PERIOD_MAX*frameSteps;
	interval = profileFrames*frameSteps;
	*tries = 0;
	*pulses = NULL;
	
	for(threshold = level->max - SYNCDET_FIRST_DB; threshold > level->floor + SYNCDET_FLOOR_DB; threshold -= SYNCDET_SWEEP_DB)
	{
		double searchAfter = 0;
		
		(*tries) ++;
		foundPulses = SyncDetEdges(level, threshold, threshold - SYNCDET_HYSTERESIS_DB, pulseMS*0.5, SYNCDET_MIN_OFF_MS, pulses);
		while(foundPulses && SyncDetFindTrain(*pulses, foundPulses, gridSize, periodMin, periodMax, tolerance, searchAfter, -1, minMatch, start))
		{
			double firstRise = start->pulse[0]->rise;
			int found = 0;

			if(wide)
				found = SyncDetFindTrain(*pulses, foundPulses, gridSize, periodMin, periodMax, tolerance, 
							firstRise + interval*SYNCDET_WIDE_START, -1, minMatch, end); 
			else
				found = SyncDetFindTrain(*pulses, foundPulses, gridSize, periodMin, periodMax, tolerance,
							firstRise + interval*(1.0 - SYNCDET_END_WINDOW), firstRise + interval*(1.0 + SYNCDET_END_WINDOW), minMatch, end);
			if(found)
			{	
				SyncDetExtendTrain(*pulses, foundPulses, gridSize, tolerance, start);
				SyncDetExtendTrain(*pulses, foundPulses, gridSize, tolerance, end);
				*onDB = threshold;
				return 1; 
			}
			// no closing train, try the next one
			searchAfter = firstRise + 1;
		}
		if (*pulses)
			free(*pulses);
		*pulses = NULL;
	}
	return 0;
}

double SyncDetMeasureFrequency(AudioSignal *Signal, SyncDetTrain *train, long int step, double frequency, double pulseMS)
{
	long int	length = 0, totalFrames = 0;
	double		bestHz = 0, bestMagnitude = 0, belowMagnitude = 0, aboveMagnitude = 0, *hann = NULL, *mixedSamples = NULL;
	int			usedPulses = 0;
	
	length = (long int)(pulseMS*0.5*Signal->SampleRate/1000.0);
	totalFrames = Signal->numSamples/Signal->AudioChannels;
	if(length < 8 || train->count < 2)
		return 0;

	hann = (double*)malloc(sizeof(double)*length);
	mixedSamples = (double*)malloc(sizeof(double)*length*train->count);
	if (!hann || !mixedSamples)
	{
		logmsg("ERROR: Not enough memory for the sync frequency\n");
		if(hann)
			free(hann);
		if (mixedSamples)
			free(mixedSamples);
		return 0;
	}
	for(long int i = 0; i < length; i++)
		hann[i] = 0.5 - 0.5*cos(2.0*M_PI*i/(length - 1));

	for(int pulseIndex = 1; pulseIndex < train->count; pulseIndex++)
	{
		long int firstSample = train->pulse[pulseIndex]->rise*step + length/2;

		if(firstSample + length > totalFrames)
			continue;	
		for(long int i = 0; i < length; i++)
		{
			double magnitudeSum = 0;

			for(int channel = 0; channel < Signal->AudioChannels; channel++)
				magnitudeSum += Signal->Samples[(firstSample + i)*Signal->AudioChannels + channel];
			mixedSamples[usedPulses*length + i] = hann[i]*magnitudeSum/Signal->AudioChannels;
		}
		usedPulses ++;
	}
	if(!usedPulses)
	{
		free(hann);
		free(mixedSamples);
		return 0;	
	}
	
	// Scan, then refine the peak with a parabola
	for(int pass = 0; pass < 3; pass++)
	{
		double	fromHz = frequency*(1.0 - SYNCDET_FREQ_RANGE), toHz = frequency*(1.0 + SYNCDET_FREQ_RANGE);

		if (pass > 0)
			fromHz = toHz = bestHz + (pass == 1 ? -SYNCDET_FREQ_STEP : SYNCDET_FREQ_STEP);	
		for(double hertz = fromHz; hertz <= toHz + 1e-9; hertz += SYNCDET_FREQ_STEP) 
		{
			double	magnitudeSum = 0, omega = 2.0*M_PI*hertz/Signal->SampleRate;

			for(int pulseIndex = 0; pulseIndex < usedPulses; pulseIndex++)
			{
				double	real = 0, imaginary = 0, phasorReal = 1, phasorImaginary = 0, stepReal = cos(omega), stepImaginary = -sin(omega);
				
				for(long int i = 0; i < length; i++)
				{
					double value = mixedSamples[pulseIndex*length + i], rotated = 0;
					
					real += value*phasorReal; 
					imaginary += value*phasorImaginary;
					rotated = phasorReal*stepReal - phasorImaginary*stepImaginary;
					phasorImaginary = phasorReal*stepImaginary + phasorImaginary*stepReal;
					phasorReal = rotated;
				}
				magnitudeSum += sqrt(real*real + imaginary*imaginary);
			}
			if(pass == 0 && magnitudeSum > bestMagnitude)
			{
				bestMagnitude = magnitudeSum;
				bestHz = hertz;
			}
			if(pass == 1)
				belowMagnitude = magnitudeSum;
			if(pass == 2)
				aboveMagnitude = magnitudeSum;
		}
	}
	free(hann);
	free(mixedSamples);

	if(belowMagnitude + aboveMagnitude - 2*bestMagnitude < 0)
		bestHz += SYNCDET_FREQ_STEP*0.5*(belowMagnitude - aboveMagnitude)/(belowMagnitude + aboveMagnitude - 2*bestMagnitude);
	return bestHz;
}

// The window delays the rise by half its length
double SyncDetRise(AudioSignal *Signal, SyncDetPulse *pulse, SyncDetLevel *level, double frequency)
{
	long int	coarseRise = 0, spanStart = 0, spanCount = 0, window = 0, step = 0;
	double		*amplitude = NULL, peak = 0, beforePulse = 0, riseLevel = 0, rise = 0;

	window = level->window;
	step = level->step;
	coarseRise = pulse->rise*step;
	spanStart = coarseRise - 4*step - window;
	spanCount = 8*step + 3*window;
	rise = coarseRise + window/2.0;
	
	amplitude = (double*)malloc(sizeof(double)*(spanCount + 1));
	if(!amplitude)
		return rise;

	if(!SyncDetEnvelopeSpan(Signal, spanStart, spanCount, window, frequency, amplitude))
	{
		free(amplitude);
		return rise;
	}	

	peak = pow(10.0, pulse->peak/20.0); 
	beforePulse = amplitude[0];
	for(long int i = 1; i < 2*step && i <= spanCount; i++)
		if(amplitude[i] < beforePulse)
			beforePulse = amplitude[i];

	riseLevel = beforePulse + SYNCDET_RISE*(peak - beforePulse);
	if(amplitude[0] >= riseLevel)		// already up
	{
		free(amplitude);
		return rise;
	}
	
	for(long int i = 1; i <= spanCount; i++)
	{
		if(amplitude[i] >= riseLevel)
		{
			rise = spanStart + i - 1 + (riseLevel - amplitude[i - 1])/(amplitude[i] - amplitude[i - 1]) + window/2.0;
			break;
		}
	}
	free(amplitude);
	return rise;
}

// One cycle window, from 10% of the peak back to where it leaves the floor
double SyncDetOnset(AudioSignal *Signal, long int coarseRise, double frequency)
{
	long int	window = 0, beforeSamples = 0, afterSamples = 0, spanStart = 0, spanCount = 0, quietSamples = 0, i = 0;
	double		*amplitude = NULL, *sorted = NULL, floorLevel = 0, deviation = 0, peak = 0, threshold = 0, onset = -1;

	window = (long int)floor(Signal->SampleRate/frequency + 0.5);	
	if(window < 2)
		window = 2;

	beforeSamples = (long int)(0.006*Signal->SampleRate);
	afterSamples = (long int)(0.006*Signal->SampleRate);
	quietSamples = (long int)(0.002*Signal->SampleRate);
	spanStart = coarseRise - beforeSamples;
	spanCount = beforeSamples + afterSamples;
	
	if(spanStart < 0)
		return -1;

	amplitude = (double*)malloc(sizeof(double)*(spanCount + 1));
	sorted = (double*)malloc(sizeof(double)*quietSamples);
	if(!amplitude || !sorted)		
	{
		if(amplitude)
			free(amplitude);
		if (sorted)
			free(sorted);
		return -1;
	}

	if(!SyncDetEnvelopeSpan(Signal, spanStart, spanCount, window, frequency, amplitude))
	{
		free(amplitude);
		free(sorted);
		return -1;
	}
	
	for(i = 0; i < quietSamples; i++)
	{
		sorted[i] = amplitude[i];
		floorLevel += amplitude[i];
	}
	floorLevel /= quietSamples; 

	for(i = 0; i < quietSamples; i++)
		deviation += (amplitude[i] - floorLevel)*(amplitude[i] - floorLevel);
	deviation = sqrt(deviation/quietSamples);
	floorLevel = SyncDetPercentile(sorted, quietSamples, 50.0);
	
	for(i = beforeSamples - window; i <= spanCount; i++)
		if(amplitude[i] > peak)
			peak = amplitude[i];
	
	for(i = 0; i <= spanCount; i++)
		if(amplitude[i] >= floorLevel + SYNCDET_ONSET*(peak - floorLevel))
			break;

	if (i <= spanCount)
	{
		threshold = floorLevel + fmax(SYNCDET_ONSET_SIGMA*deviation, SYNCDET_ONSET_PEAK*(peak - floorLevel));
		while(i > 0 && amplitude[i - 1] > threshold)
			i --;
		onset = spanStart + i + window;
	}
	free(amplitude);
	free(sorted);
	return onset;
}

int SyncDetCompareDoubles(const void *a, const void *b)
{
	double first = *(const double*)a, second = *(const double*)b;

	return (first > second) - (first < second);
}

// Like numpy, sorts in place
double SyncDetPercentile(double *values, long int count, double percent)
{
	double		position = 0, fraction = 0;
	long int	index = 0;
	
	if(count <= 0)
		return 0;
	if(count == 1)
		return values[0];

	qsort(values, count, sizeof(double), SyncDetCompareDoubles);
	position = percent/100.0*(count - 1);
	index = (long int)floor(position);
	fraction = position - index;

	if(index >= count - 1)
		return values[count - 1];

	return(values[index] + fraction*(values[index + 1] - values[index]));
}
