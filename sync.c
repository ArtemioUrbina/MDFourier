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

#include "mdfourier.h"
#include "sync.h"
#include "log.h"
#include "freq.h"

/*
	There are the number of subdivisions to use. 
	The higher, the less precise in frequency but more in position and vice versa 
*/

#define SYNC_LPF		24000   // Sync Low pass filter
#define	FACTOR_LFEXPL	4
#define	FACTOR_EXPLORE	8
#define	FACTOR_DETECT	8

// Cut off for harmonic search
#define HARMONIC_TSHLD 6000

long int DetectPulse(double *AllSamples, wav_hdr header, int role, parameters *config)
{
	int			maxdetected = 0, AudioChannels = 0;
	long int	sampleOffset = 0, searchOffset = 0;
	double		seconds = 0;

	if(config->debugSync)
		logmsgFileOnly("\nStarting Detect start pulse\n");

	AudioChannels = header.fmt.NumOfChan;

	sampleOffset = DetectPulseInternal(AllSamples, header, FACTOR_EXPLORE, 0, &maxdetected, role, AudioChannels, config);
	if(sampleOffset == -1)
	{
		if(config->debugSync)
			logmsgFileOnly("First round start pulse failed\n");

		return DetectPulseSecondTry(AllSamples, header, role, config);
	}

	// Tight detect, in case of longer files
	// start search close to the sync pulses in order to reduce
	// false amplitude detection
	seconds = SamplesToSeconds(header.fmt.SamplesPerSec, sampleOffset, AudioChannels);
	if(seconds > 0.1)
		searchOffset = sampleOffset - SecondsToSamples(header.fmt.SamplesPerSec, 0.1, AudioChannels, header.fmt.bitsPerSample/8, NULL, NULL, NULL);
	else
		searchOffset = sampleOffset/2;
	searchOffset = DetectPulseInternal(AllSamples, header, FACTOR_EXPLORE, searchOffset, &maxdetected, role, AudioChannels, config);
	if(searchOffset == -1)
	{
		if(config->debugSync)
			logmsgFileOnly("First round start pulse failed\n");

		return DetectPulseSecondTry(AllSamples, header, role, config);
	}

	if(config->debugSync && searchOffset != sampleOffset)
		logmsg("WARNING: Adjusted sync offset start from %ld to %ld\n", sampleOffset/AudioChannels, searchOffset/AudioChannels);

	sampleOffset = AdjustPulseSampleStart(AllSamples, header, sampleOffset, role, AudioChannels, config);
	if(sampleOffset != -1)
		return sampleOffset;

	if(config->debugSync)
		logmsgFileOnly("Start pulse return value %ld\n", SamplesForDisplay(sampleOffset, AudioChannels));

	return sampleOffset;
}

/* only difference is that it auto detects the start first, helps in some cases with long silence and high noise floor */
long int DetectPulseSecondTry(double *AllSamples, wav_hdr header, int role, parameters *config)
{
	int			maxdetected = 0, AudioChannels = 0, bytesPerSample = 0;
	long int	sampleOffset = 0;

	if(config->debugSync)
		logmsgFileOnly("\nStarting Detect start pulse\n");

	AudioChannels = header.fmt.NumOfChan;
	bytesPerSample = header.fmt.bitsPerSample/8;

	sampleOffset = DetectSignalStart(AllSamples, header, 0, 0, 0, NULL, NULL, config);
	if(sampleOffset > 0)
	{
		long int MS_Samples = 0;
			
		MS_Samples = SecondsToSamples(header.fmt.SamplesPerSec, 0.015, AudioChannels, bytesPerSample, NULL, NULL, NULL);
		if(sampleOffset >= MS_Samples)
			sampleOffset -= MS_Samples;
	}
	else
		sampleOffset = 0;

	sampleOffset = DetectPulseInternal(AllSamples, header, FACTOR_LFEXPL, sampleOffset, &maxdetected, role, AudioChannels, config);
	if(sampleOffset == -1)
	{
		if(config->debugSync)
			logmsgFileOnly("First round start pulse failed\n");

		return -1;
	}

	sampleOffset = AdjustPulseSampleStart(AllSamples, header, sampleOffset, role, AudioChannels, config);
	if(sampleOffset != -1)
		return sampleOffset;

	if(config->debugSync)
		logmsgFileOnly("Start pulse return value %ld\n", SamplesForDisplay(sampleOffset, AudioChannels));

	return sampleOffset;
}

/*
 positions relative to the expected one
 Start with common sense ones, then search all around the place
 2.1 and above were added for PAL MD at 60 detection. Yes, that is 2.1
*/

#define END_SYNC_MAX_TRIES		64
#define END_SYNC_VALUES			{ 0.50, 0.25, 0.0, 1.25, 1.50,\
								0.9, 0.8, 0.7, 0.6, 1.6, 1.7, 1.8, 1.9, \
								0.4, 0.3, 0.1, 1.1, 1.3, 1.4,\
								1.0, -1,0, 2.0, -2.0,\
								2.1, 2.2, 2.3, 2.4, 2.5, 2.5, 2.7, 2.8, 2.9, 3.0,\
								-2.1, -2.2, -2.3, -2.4, -2.5, -2.5, -2.7, -2.8, -2.9, -3.0,\
								3.1, 3.2, 3.3, 3.4, 3.5, 3.5, 3.7, 3.8, 3.9, 4.0,\
								-3.1, -3.2, -3.3, -3.4, -3.5, -3.5, -3.7, -3.8, -3.9, -4.0 }

long int DetectEndPulse(double *AllSamples, long int startpulse, wav_hdr header, int role, parameters *config)
{
	int			maxdetected = 0, frameAdjust = 0, tries = 0, maxtries = END_SYNC_MAX_TRIES;
	int			factor = 0, AudioChannels = 0;
	long int 	sampleOffset = 0;
	double		silenceOffset[END_SYNC_MAX_TRIES] = END_SYNC_VALUES;

	AudioChannels = header.fmt.NumOfChan;
	if(GetPulseSyncFreq(role, config) < HARMONIC_TSHLD)
		factor = FACTOR_LFEXPL;
	else
		factor = FACTOR_EXPLORE;
	/* Try a clean detection */
	sampleOffset = GetSecondSyncSilenceSampleOffset(GetMSPerFrameRole(role, config), header, 0, 1, config);
	if(!sampleOffset)
	{
		logmsg("ERROR: Invalid profile, Closing sync has no pre-silence block\n");
		return -1;
	}
	sampleOffset += startpulse;
	if(config->debugSync)
		logmsgFileOnly("\nStarting CLEAN Detect end pulse with sample offset %ld\n", SamplesForDisplay(sampleOffset, AudioChannels));
	sampleOffset = DetectPulseInternal(AllSamples, header, factor, sampleOffset, &maxdetected, role, AudioChannels, config);
	if(sampleOffset != -1)
	{
		sampleOffset = AdjustPulseSampleStart(AllSamples, header, sampleOffset, role, AudioChannels, config);
		if(sampleOffset != -1)
			return sampleOffset;
	}

	
	/* We try to figure out position of the pulses */
	if(config->debugSync)
		logmsgFileOnly("End pulse CLEAN detection failed started search at %ld samples\n", SamplesForDisplay(sampleOffset, AudioChannels));

	do
	{
		/* Use defaults to calculate real frame rate */
		sampleOffset = GetSecondSyncSilenceSampleOffset(GetMSPerFrameRole(role, config), header, frameAdjust, silenceOffset[tries], config) + startpulse;
	
		if(config->debugSync)
			logmsgFileOnly("\nStarting Detect end pulse with sample offset %ld [%g silence]\n\tMaxDetected %d frameAdjust: %d\n",
				SamplesForDisplay(sampleOffset, AudioChannels), silenceOffset[tries], maxdetected, frameAdjust);
	
		frameAdjust = 0;
		maxdetected = 0;

		sampleOffset = DetectPulseInternal(AllSamples, header, factor, sampleOffset, &maxdetected, role, AudioChannels, config);
		if(sampleOffset == -1 && !maxdetected)
		{
			if(config->debugSync)
				logmsgFileOnly("End pulse failed try %d, started search at %ld samples [%g silence]\n",
				tries+1, SamplesForDisplay(sampleOffset, AudioChannels), silenceOffset[tries]);
		}

		tries ++;
	}while(sampleOffset == -1 && tries < maxtries);

	if(tries == maxtries)
		return -1;

	sampleOffset = AdjustPulseSampleStart(AllSamples, header, sampleOffset, role, AudioChannels, config);
	if(config->debugSync)
		logmsgFileOnly("End pulse return value %ld\n", SamplesForDisplay(sampleOffset, AudioChannels));

	return sampleOffset;
}


#define LOGCASE(x, y) { x; if(config->debugSync) logmsgFileOnly("Case #%d\n", y); }
double findAverageAmplitudeForTarget(Pulses *pulseArray, double targetFrequency, double *targetFrequencyHarmonic, long int TotalMS, long int start, int factor, int AudioChannels, parameters *config)
{
	long	count = 0, i = 0;
	double	averageAmplitude = 0, standardDeviation = 0, useAmplitude = 0, percent = 0;

	if(config->debugSync)
		logmsgFileOnly("Searching for in range: %ld-%ld samples\n", 
				SamplesForDisplay(pulseArray[start].samples, AudioChannels),
				SamplesForDisplay(pulseArray[TotalMS-1].samples, AudioChannels));
	for(i = start; i < TotalMS; i++)
	{
		if(pulseArray[i].hertz == targetFrequency 
			|| pulseArray[i].hertz == targetFrequencyHarmonic[0]
			|| pulseArray[i].hertz == targetFrequencyHarmonic[1])
		{
			averageAmplitude += fabs(pulseArray[i].amplitude);
			count ++;
		}
	}

	if(!count)
	{
		logmsgFileOnly("WARNING! Average Amplitude values for sync not found in range (NULL from digital/emu)\n");
		return 0;
	}

	averageAmplitude /= count;

	// Calculate Standard Deviatioon
	count = 0;
	for(i = start; i < TotalMS; i++)
	{
		if(pulseArray[i].hertz == targetFrequency || pulseArray[i].hertz == targetFrequencyHarmonic[0]
			|| pulseArray[i].hertz == targetFrequencyHarmonic[1])
		{
			standardDeviation += pow(fabs(pulseArray[i].amplitude) - averageAmplitude, 2);
			count ++;
		}
	}

	if(!count)
	{
		logmsgFileOnly("WARNING! Standard Deviation values for sync not found in range (NULL from digital/emu)\n");
		return 0;
	}

	standardDeviation = sqrt(standardDeviation/(count-1));

	percent = (double)count/(double)(TotalMS-start)*100;
	
	if(factor == FACTOR_EXPLORE) { // first round detection
		if(averageAmplitude <= 40) { //this is the regular case for the above
			if(averageAmplitude >= 10) {
				if(standardDeviation < averageAmplitude) {
					if(percent > 5)
						LOGCASE(useAmplitude = -1*averageAmplitude, 0)
					else
						LOGCASE(useAmplitude = -1*(averageAmplitude + standardDeviation/2), 1)
				} else 
					LOGCASE(useAmplitude = -1*(averageAmplitude + standardDeviation/4), 2)
			} else {
				LOGCASE(useAmplitude = -1*(averageAmplitude + 2*standardDeviation), 3)
			}
			
		} else {
			// these are special cases, too much difference
			if(standardDeviation < averageAmplitude)
				LOGCASE(useAmplitude = -1*(averageAmplitude - standardDeviation), 4)
			else
				LOGCASE(useAmplitude = -1*(averageAmplitude - standardDeviation/2), 5)
		}
	}
	else if(factor == FACTOR_DETECT) { // second round
		if(percent > 55) { // Too much noise 
			if(percent > 90) {  // ridiculous noise
				if(percent == 100)
					LOGCASE(useAmplitude = -1*(averageAmplitude + standardDeviation/4), 6)
				else
					LOGCASE(useAmplitude = -1*averageAmplitude, 7)
			} else if(averageAmplitude <= 40) {
				if(standardDeviation < averageAmplitude)
					LOGCASE(useAmplitude = -1*averageAmplitude, 8)
				else
					LOGCASE(useAmplitude = -1*(averageAmplitude + standardDeviation/4), 9)
			} else {
				// these are special cases, too much difference
				if(standardDeviation < averageAmplitude)
					LOGCASE(useAmplitude = -1*(averageAmplitude - standardDeviation), 10)
				else
					LOGCASE(useAmplitude = -1*(averageAmplitude - standardDeviation/2), 11)
			}
		} else { // lower that 55%, pulses hopefully
			if(standardDeviation < averageAmplitude || (fabs(standardDeviation) + fabs(averageAmplitude) <= 20))  // this is the ideal case
				LOGCASE(useAmplitude = -1*(averageAmplitude + standardDeviation), 12)
			else  // This is trying to adjust it
				LOGCASE(useAmplitude = -1*(averageAmplitude + standardDeviation/4), 13)
		}
	}

	if(count && useAmplitude == 0)
	{
		if(config->debugSync)
			logmsgFileOnly("Signal is too perfect, matching to -1dbfs\n");
		useAmplitude = -1;
	}

	if(config->debugSync)
		logmsgFileOnly("AVG: %g STD: %g Use: %g count %ld total %d %% %g\n",
			averageAmplitude, standardDeviation, useAmplitude,
			TotalMS-start, count, (double)count/(double)(TotalMS-start)*100);

	if(config->debugSync)
		logmsgFileOnly("Searching for Average amplitude in block: F %g Total Start sample: %ld milliseconds to check: %ld\n", 
			targetFrequency, 
			SamplesForDisplay(pulseArray[start].samples, AudioChannels),
			TotalMS);


	return useAmplitude;
}

long int DetectPulseTrainSequence(Pulses *pulseArray, double targetFrequency, double *targetFrequencyHarmonic, long int TotalMS, int factor, int *maxdetected, long int start, int role, int AudioChannels, parameters *config)
{
	long	i = 0, sequence_start = 0;
	int		frame_pulse_count = 0, frame_silence_count = 0, 
			pulse_count = 0, silence_count = 0, lastcountedWasPulse = 0, lookingfor = 0;
	double	averageAmplitude = 0;

	*maxdetected = 0;

	lookingfor = getPulseFrameLen(role, config)*factor-factor/2;
	//smoothAmplitudes(pulseArray, targetFrequency, TotalMS, start);
	averageAmplitude = findAverageAmplitudeForTarget(pulseArray, targetFrequency, targetFrequencyHarmonic, TotalMS, start, factor, AudioChannels, config);
	if(averageAmplitude == 0)
		return -1;

	if(config->debugSync)
		logmsgFileOnly("== Searching for %g/%g/%g Average Amplitude %g looking for %d (%d*%d)\n", 
			targetFrequency, targetFrequencyHarmonic[0], targetFrequencyHarmonic[1], averageAmplitude, lookingfor,
			getPulseFrameLen(role, config), factor);

	for(i = start; i < TotalMS; i++)
	{
		int checkSilence = 0;

		if(pulseArray[i].hertz)
		{
			if(config->syncTolerance)
				targetFrequency = pulseArray[i].hertz;
			if(pulseArray[i].amplitude >= averageAmplitude
				&& (pulseArray[i].hertz == targetFrequency || pulseArray[i].hertz == targetFrequencyHarmonic[0] || pulseArray[i].hertz == targetFrequencyHarmonic[1]))
			{
				int linefeedNeeded = 1;

				frame_pulse_count++;
				lastcountedWasPulse = 1;
				if(config->debugSync)
					logmsgFileOnly("[i:%ld] Sample:%7ld [%5gHz %0.2f dBFS] <<Pulse Frame counted %d>>", 
						i, SamplesForDisplay(pulseArray[i].samples, AudioChannels),
						pulseArray[i].hertz, pulseArray[i].amplitude, 
						frame_pulse_count);
	
				if(!sequence_start)
				{
					if(config->debugSync)
						logmsgFileOnly(" This starts the sequence\n");

					sequence_start = pulseArray[i].samples;
					frame_silence_count = 0;
					linefeedNeeded = 0;
				}
	
				if(frame_silence_count >= lookingfor*3/5)  /* allow silence to have some stray noise */
				{
					silence_count++;
	
					if(config->debugSync)
						logmsgFileOnly("Closed a silence cycle %d\n", silence_count);
					/*
					if(silence_count > getPulseCount(role, config))
					{
						if(config->debugSync)
							logmsgFileOnly("Resets the sequence\n");
						sequence_start = 0;
						silence_count = 0;
					}
					*/
					linefeedNeeded = 0;
					frame_silence_count = 0;
				}
	
				//if(frame_silence_count)
					//frame_silence_count = 0;

				if(config->debugSync && linefeedNeeded)
					logmsgFileOnly("\n");
			}
			else
				checkSilence = 1;
		}
		else
			checkSilence = 1;

		// 1 tick tolerance within a pulse
		if(checkSilence && pulseArray[i].amplitude < averageAmplitude)
		{
			if(frame_pulse_count >= lookingfor/3 && (lastcountedWasPulse && (pulseArray[i].hertz == targetFrequency 
				|| pulseArray[i].hertz == targetFrequencyHarmonic[0] || pulseArray[i].hertz == targetFrequencyHarmonic[1])))
			{
				if(config->debugSync)
					logmsgFileOnly("[i:%ld] Sample:%7ld [%5gHz %0.2f dBFS] Silence Frame found due to AVG but skipped due to Hz%d\n", 
						i, SamplesForDisplay(pulseArray[i].samples, AudioChannels),
						pulseArray[i].hertz, pulseArray[i].amplitude, 
						frame_silence_count);
				lastcountedWasPulse = 0;
				checkSilence = 0;
			}
		}

		if(checkSilence)
		{
			if(pulseArray[i].amplitude < averageAmplitude)
			{
				if(config->debugSync)
					logmsgFileOnly("[i:%ld] sample:%7ld [%5gHz %0.2f dBFS] %s %d\n", 
						i, SamplesForDisplay(pulseArray[i].samples, AudioChannels),
						pulseArray[i].hertz, pulseArray[i].amplitude, 
						sequence_start ? " Silence Frame counted" : "--", frame_silence_count);

				frame_silence_count ++;
			}
			else
			{
				int linefeedNeeded = 1;

				if(config->debugSync)
					logmsgFileOnly("[i:%ld] sample:%7ld [%5gHz %0.2f dBFS] Non Frame skipped %d LC:%d", 
						i, SamplesForDisplay(pulseArray[i].samples, AudioChannels),
						pulseArray[i].hertz, pulseArray[i].amplitude, 
						frame_silence_count, lastcountedWasPulse);

				if(lastcountedWasPulse == 0)
				{
					if(config->debugSync)
						logmsgFileOnly(" Counting as silence\n");
					frame_silence_count++;
					linefeedNeeded = 0;
				}

				if(lastcountedWasPulse == 1 && frame_pulse_count >= lookingfor)
				{
					if(config->debugSync)
						logmsgFileOnly(" Counting as silence due to pulse count\n");
					frame_silence_count++;
					linefeedNeeded = 0;
				}
				if(config->debugSync && linefeedNeeded)
					logmsgFileOnly("\n");
			}

			if(frame_pulse_count >= lookingfor)
			{
				pulse_count++;

				if(config->debugSync)
					logmsgFileOnly("Closed pulse #%d cycle, silence count %d pulse count %d\n", pulse_count, silence_count, frame_pulse_count);
				
				if(config->syncTolerance)
					silence_count = pulse_count - 1;

				if(pulse_count == getPulseCount(role, config) && silence_count >= pulse_count/2) /* silence_count == pulse_count - 1 */
				{
					if(config->debugSync)
						logmsgFileOnly("Completed the sequence %ld\n", SamplesForDisplay(sequence_start, AudioChannels));
					return sequence_start;
				}
				lastcountedWasPulse = 0;
			}

			if(frame_pulse_count > 0)
			{
				if(!pulse_count && sequence_start)
				{
					if(config->debugSync)
						logmsgFileOnly("Resets the sequence (no pulse count)\n");
					sequence_start = 0;
				}

				if(!lastcountedWasPulse)  // Allow 1 frame of silence/invalid frequency in between
				{
					frame_pulse_count = 0;
					lastcountedWasPulse = 0;
				}
			}

			if(frame_silence_count > lookingfor*2)
			{
				if(pulse_count)
				{
					if(config->debugSync)
						logmsgFileOnly("Resets the sequence (silence too long > %d)\n", lookingfor * 2);
					sequence_start = 0;
					pulse_count = 0;
				}
			}
		}
	}

	if(config->debugSync)
		logmsgFileOnly("Failed\n");
	*maxdetected = pulse_count;
	return -1;
}

#define SORT_NAME PulsesByMagnitude
#define SORT_TYPE Pulses
#define SORT_CMP(x, y)  ((x).magnitude > (y).magnitude ? -1 : ((x).magnitude == (y).magnitude ? 0 : 1))
#include "sort.h"  // https://github.com/swenson/sort/

long int AdjustPulseSampleStart(double *Samples, wav_hdr header, long int offset, int role, int AudioChannels, parameters *config)
{
	int			samplesNeeded = 0, frequency = 0, minDiffPos = -1, bytesPerSample = 0;
	long int	startSearch = 0, endSearch = 0, pos = 0, count = 0, foundPos = -1, totalSamples = 0;
	double		*buffer = NULL;
	Pulses		*pulseArray = NULL;
	double		targetFrequency = 0, minDiff = 1000;

	frequency = GetPulseSyncFreq(role, config);
	samplesNeeded = header.fmt.SamplesPerSec/frequency*AudioChannels;

	targetFrequency = FindFrequencyBracketForSync(frequency,
						samplesNeeded, AudioChannels, header.fmt.SamplesPerSec, config);
	if(config->debugSync)
		logmsgFileOnly("\nSearcing at %ld, looking for %ghz samples needed: %d\n",
				SamplesForDisplay(offset, AudioChannels), targetFrequency, 
				SamplesForDisplay(samplesNeeded, AudioChannels));
	buffer = (double*)malloc(samplesNeeded*sizeof(double));
	if(!buffer)
	{
		logmsgFileOnly("\tSync Adjust malloc failed\n");
		return(foundPos);
	}

	if(offset >= 2*samplesNeeded)
		startSearch = offset-2*samplesNeeded;
	else
		startSearch = 0;
	endSearch = offset+samplesNeeded;

	pulseArray = (Pulses*)malloc(sizeof(Pulses)*(endSearch-startSearch));
	if(!pulseArray)
	{
		free(buffer);
		logmsgFileOnly("\tPulse malloc failed!\n");
		return(foundPos);
	}
	memset(pulseArray, 0, sizeof(Pulses)*(endSearch-startSearch));

	bytesPerSample = header.fmt.bitsPerSample/8;
	totalSamples = header.data.DataSize/bytesPerSample;

	for(pos = startSearch; pos < endSearch; pos += AudioChannels)
	{
		memset(buffer, 0, samplesNeeded*sizeof(double));
		if(pos + samplesNeeded > totalSamples)
		{
			//logmsg("\tUnexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
			break;
		}

		pulseArray[count].samples = pos;
		memcpy(buffer, Samples + pos, samplesNeeded*sizeof(double));
		ProcessChunkForSyncPulse(buffer, samplesNeeded, 
			header.fmt.SamplesPerSec, &pulseArray[count], 
			CHANNEL_LEFT, AudioChannels, config);
		count ++;
	}

	PulsesByMagnitude_tim_sort(pulseArray, count);
	for(pos = 0; pos < count; pos++)
	{
		if(targetFrequency == pulseArray[pos].hertz)
		{
			double diff = 0;
	
			diff = fabs(0 - pulseArray[pos].phase);
			if(config->debugSync)
				logmsgFileOnly("Sample: %ld %gHz Mag %g Phase %g diff: %g\n",
					SamplesForDisplay(pulseArray[pos].samples, AudioChannels),
					pulseArray[pos].hertz, pulseArray[pos].magnitude, pulseArray[pos].phase, diff);
			if(diff < minDiff)
			{
				minDiff = diff;
				minDiffPos = pos;
			}
		}
	}

	if(minDiffPos != -1)
	{
		foundPos = pulseArray[minDiffPos].samples;
		if(config->debugSync)
			logmsgFileOnly("FOUND: Item-%ld Sample: %ld %gHz Mag %g Phase %g diff: %g\n",
				minDiffPos, 
				SamplesForDisplay(pulseArray[minDiffPos].samples, AudioChannels),
				pulseArray[minDiffPos].hertz, pulseArray[minDiffPos].magnitude, pulseArray[minDiffPos].phase, minDiff);
	}

	free(buffer);
	free(pulseArray);

	return foundPos;
}

// Searches using 1ms/factor blocks
long int DetectPulseInternal(double *Samples, wav_hdr header, int factor, long int offset, int *maxdetected, int role, int AudioChannels, parameters *config)
{
	int					bytesPerSample = 0;
	long int			i = 0, TotalMS = 0, totalSamples = 0;
	double				*sampleBuffer = NULL;
	long int		 	sampleBufferSize = 0, pos = 0, startPos = 0;
	Pulses				*pulseArray = NULL;
	double				targetFrequency = 0, targetFrequencyHarmonic[2] = { NO_FREQ, NO_FREQ }, origFrequency = 0, MaxMagnitude = 0;

	bytesPerSample = header.fmt.bitsPerSample/8;
	/* Not a real ms, just approximate */
	sampleBufferSize = SecondsToSamples(header.fmt.SamplesPerSec, 1.0/((double)factor*1000.0), AudioChannels, bytesPerSample, NULL, NULL, NULL);
	if(sampleBufferSize < 4){
		logmsg("ERROR: Invalid parameters for sync detection\n");
		return -1;
	}
	sampleBuffer = (double*)malloc(sampleBufferSize*sizeof(double));
	if(!sampleBuffer)
	{
		logmsgFileOnly("\tERROR: malloc failed for sample buffer during DetectPulseInternal\n");
		return -1;
	}

	totalSamples = header.data.DataSize/bytesPerSample;
	// calculate how many sampleBufferSize units fit in the available samples from the file
	TotalMS = totalSamples/sampleBufferSize-1;
	pos = offset;
	if(offset)
	{
		double syncLen = 0;

		i = offset;
		startPos = i;

		/* check for the time duration in ms*factor of the sync pulses */
		syncLen = GetLastSyncDuration(GetMSPerFrameRole(role, config), config)*1000*factor;
		if(factor == FACTOR_EXPLORE)  /* are we exploring? */
			syncLen *= 2.0;  /* widen so that the silence offset is compensated for */
		else
			syncLen *= 1.2;  /* widen so that the silence offset is compensated for */
		//if(i+syncLen > TotalMS)
		TotalMS = i+syncLen;

		if(config->debugSync)
			logmsgFileOnly("Started detecting with offet %ld. Changed to:\n\tSamplesBufferSize: %ld, Samples:%ld-%ld/ms:%ld-%ld]\n\tms len: %g Bytes: %g Factor: %d\n", 
				offset/AudioChannels, sampleBufferSize, i*factor, TotalMS*factor, i, TotalMS,
				syncLen, syncLen/factor, factor);
	}
	else
	{
		double expectedlen = 0, seconds = 0, syncLenSeconds = 0, syncLen = 0, silenceLen = 0, silenceLenSeconds = 0;

		seconds = GetSignalTotalDuration(GetMSPerFrameRole(role, config), config);
		expectedlen = SecondsToSamples(header.fmt.SamplesPerSec, seconds, header.fmt.NumOfChan, bytesPerSample, NULL, NULL, NULL);
		expectedlen = floor(expectedlen/sampleBufferSize) - 1;

		syncLenSeconds = GetFirstSyncDuration(GetMSPerFrameRole(role, config), config);
		syncLen = SecondsToSamples(header.fmt.SamplesPerSec, syncLenSeconds, header.fmt.NumOfChan, bytesPerSample, NULL, NULL, NULL);
		syncLen = floor(syncLen/sampleBufferSize) - 1;

		silenceLenSeconds = GetFirstSilenceDuration(GetMSPerFrameRole(role, config), config);
		silenceLen = SecondsToSamples(header.fmt.SamplesPerSec, silenceLenSeconds, header.fmt.NumOfChan, bytesPerSample, NULL, NULL, NULL);
		silenceLen = floor(silenceLen/sampleBufferSize) - 1;

		// check if it is long enough
		if(expectedlen + syncLen + silenceLen / 2 < TotalMS)
			TotalMS = TotalMS - expectedlen + syncLen + silenceLen/2;

		if(expectedlen*1.5 < TotalMS)  // long file
			config->trimmingNeeded = 1;
	}

	pulseArray = (Pulses*)malloc(sizeof(Pulses)*TotalMS);
	if(!pulseArray)
	{
		logmsgFileOnly("\tPulse malloc failed!\n");
		return -1;
	}
	memset(pulseArray, 0, sizeof(Pulses)*TotalMS);

	origFrequency = GetPulseSyncFreq(role, config);
	targetFrequency = FindFrequencyBracketForSync(origFrequency,
						sampleBufferSize, AudioChannels, header.fmt.SamplesPerSec, config);
	if(origFrequency < HARMONIC_TSHLD)  //default behavior for around 8khz, harmonic is NO_FREQ
	{
		targetFrequencyHarmonic[0] = FindFrequencyBracketForSync(targetFrequency*2, 	
						sampleBufferSize, AudioChannels, header.fmt.SamplesPerSec, config);
		targetFrequencyHarmonic[1] = FindFrequencyBracketForSync(targetFrequency*3,
						sampleBufferSize, AudioChannels, header.fmt.SamplesPerSec, config);
		if(config->debugSync)
			logmsgFileOnly("\n - Using %gHz and harmonics %g/%gHz for sync detection\n", 
				targetFrequency, targetFrequencyHarmonic[0], targetFrequencyHarmonic[1]);
	}
		
	if(config->debugSync)
	{
		logmsgFileOnly("\nDefined Sync %g/%g/%g Adjusted to %g/%g/%g\n", 
				origFrequency, 
				origFrequency >= HARMONIC_TSHLD ? origFrequency : targetFrequency*2, 
				origFrequency >= HARMONIC_TSHLD ? origFrequency : targetFrequency*3, 
				targetFrequency, targetFrequencyHarmonic[0], targetFrequencyHarmonic[1]);
		logmsgFileOnly("Start ms %ld Total MS: %ld (%ld)\n",
			 i, TotalMS-1, totalSamples/sampleBufferSize - 1);
	}

	while(i < TotalMS)
	{
		if(pos + sampleBufferSize > totalSamples)
		{
			//logmsg("\tUnexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
			break;
		}

		memset(sampleBuffer, 0, sampleBufferSize*sizeof(double));
		memcpy(sampleBuffer, Samples + pos, sampleBufferSize*sizeof(double));
		pulseArray[i].samples = pos;

		pos += sampleBufferSize;

		/* We use left channel by default, we don't know about channel imbalances yet */
		ProcessChunkForSyncPulse(sampleBuffer, sampleBufferSize, 
			header.fmt.SamplesPerSec, &pulseArray[i], 
			CHANNEL_LEFT, AudioChannels, config);

		if(pulseArray[i].magnitude > MaxMagnitude)
			MaxMagnitude = pulseArray[i].magnitude;
		i++;
	}


	for(i = startPos; i < TotalMS; i++)
	{
		if(pulseArray[i].hertz)  /* this can be zero if samples were zeroed */
			pulseArray[i].amplitude = CalculateAmplitude(pulseArray[i].magnitude, MaxMagnitude);
		else
			pulseArray[i].amplitude = NO_AMPLITUDE;
	}

	/*
	if(config->debugSync)
	{
		logmsgFileOnly("===== Searching for %gHz =======\n", targetFrequency);
		for(i = startPos; i < 40; i++)
		{
			//if(pulseArray[i].hertz == targetFrequency)
				logmsgFileOnly("B: %ld Hz: %g A: %g M: %g\n", 
					pulseArray[i].samples, 
					pulseArray[i].hertz, 
					pulseArray[i].amplitude,
					pulseArray[i].magnitude);
		}
		logmsgFileOnly("========  End listing =========\n");
	}
	*/

	offset = DetectPulseTrainSequence(pulseArray, targetFrequency, targetFrequencyHarmonic, TotalMS, factor, maxdetected, startPos, role, AudioChannels, config);

	free(pulseArray);
	free(sampleBuffer);

	return offset;
}

double ProcessChunkForSyncPulse(double *samples, size_t size, long samplerate, Pulses *pulse, char channel, int AudioChannels, parameters *config)
{
	fftw_plan		p = NULL;
	long		  	stereoSignalSize = 0;	
	long		  	i = 0, monoSignalSize = 0; 
	double		  	*signal = NULL;
	fftw_complex  	*spectrum = NULL;
	double		 	seconds = 0, boxsize = 0;
	double			maxHertz = 0, maxMag = 0, maxPhase = 0;

	stereoSignalSize = (long)size;
	monoSignalSize = stereoSignalSize/AudioChannels;	 /* is 1/2 16 bit values */
	seconds = (double)size/((double)samplerate*AudioChannels);
	boxsize = seconds;

	signal = (double*)malloc(sizeof(double)*(monoSignalSize+1));
	if(!signal)
	{
		logmsgFileOnly("Not enough memory\n");
		return(0);
	}
	spectrum = (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*(monoSignalSize/2+1));
	if(!spectrum)
	{
		logmsgFileOnly("Not enough memory\n");
		return(0);
	}

	if(!config->sync_plan)
	{
 		fftw_import_wisdom_from_filename("wisdom.fftw");

		config->sync_plan = fftw_plan_dft_r2c_1d(monoSignalSize, signal, spectrum, FFTW_MEASURE);
		if(!config->sync_plan)
		{
			logmsgFileOnly("FFTW failed to create FFTW_MEASURE plan\n");
			free(signal);
			signal = NULL;
			fftw_free(spectrum);
			spectrum = NULL;
			return 0;
		}
	}

	p = fftw_plan_dft_r2c_1d(monoSignalSize, signal, spectrum, FFTW_MEASURE);
	if(!p)
	{
		logmsgFileOnly("FFTW failed to create FFTW_MEASURE plan\n");

		free(signal);
		signal = NULL;

		fftw_free(spectrum);
		spectrum = NULL;
		return 0;
	}

	memset(signal, 0, sizeof(double)*(monoSignalSize+1));
	memset(spectrum, 0, sizeof(fftw_complex)*(monoSignalSize/2+1));

	for(i = 0; i < monoSignalSize; i++)
	{
		if(channel == CHANNEL_LEFT)
			signal[i] = (double)samples[i*AudioChannels];
		if(channel == CHANNEL_RIGHT)
			signal[i] = (double)samples[i*AudioChannels+1];
		if(channel == CHANNEL_STEREO)
			signal[i] = ((double)samples[i*AudioChannels]+(double)samples[i*AudioChannels+1])/2.0;
	}

	fftw_execute(p); 
	fftw_destroy_plan(p);
	p = NULL;

	for(i = 1; i < monoSignalSize/2+1; i++)
	{
		double magnitude;

		magnitude = CalculateMagnitude(spectrum[i], size);
		if(magnitude > maxMag)
		{
			double hertz = 0;

			hertz = CalculateFrequency(i, boxsize);
			if(hertz < SYNC_LPF)  // LPF
			{
				maxHertz = hertz;
				maxMag = magnitude;
				maxPhase = CalculatePhase(spectrum[i]);
			}
		}
	}

	fftw_free(spectrum);
	spectrum = NULL;

	free(signal);
	signal = NULL;

	pulse->hertz = maxHertz;
	pulse->magnitude = maxMag;
	pulse->phase = maxPhase;

	return(maxHertz);
}

long int DetectSignalStart(double *AllSamples, wav_hdr header, long int offset, int syncKnow, long int expectedSyncLen, long int *endPulse, int *toleranceIssue, parameters *config)
{
	int			maxdetected = 0, AudioChannels = 0;
	long int	position = 0;

	if(config->debugSync)
		logmsgFileOnly("\nStarting Detect Signal\n");

	AudioChannels = header.fmt.NumOfChan;
	position = DetectSignalStartInternal(AllSamples, header, FACTOR_DETECT, offset, syncKnow, expectedSyncLen, &maxdetected, endPulse, AudioChannels, toleranceIssue, config);
	if(position == -1)
	{
		if(config->debugSync)
			logmsgFileOnly("Detect signal failed %ld\n", SamplesForDisplay(offset, AudioChannels));

		return -1;
	}

	if(config->debugSync)
		logmsgFileOnly("Detect signal return value %ld\n", SamplesForDisplay(position, AudioChannels));
	return position;
}

// amount of full length pulses to use
#define MIN_LEN 4
long int DetectSignalStartInternal(double *Samples, wav_hdr header, int factor, long int offset, int syncKnown, long int expectedSyncLen, int *maxdetected, long int *endPulse, int AudioChannels, int *toleranceIssue, parameters *config)
{
	int					bytesPerSample;
	long int			i = 0, TotalMS = 0, start = 0, totalSamples = 0;
	double				*sampleBuffer = NULL;
	long int		 	sampleBufferSize = 0;
	long int			pos = 0;
	double				MaxMagnitude = 0;
	Pulses				*pulseArray;
	double 				total = 0;
	long int 			count = 0, length = 0, tolerance = 0, toleranceIssueOffset = -1, MaxTolerance = 4;
	double 				targetFrequency = 0, targetFrequencyHarmonic[2] = { NO_FREQ, NO_FREQ }, averageAmplitude = 0;

	bytesPerSample = header.fmt.bitsPerSample/8;
	/* Not a real ms, just approximate */
	sampleBufferSize = SecondsToSamples(header.fmt.SamplesPerSec, 1.0/((double)factor*1000.0), AudioChannels, bytesPerSample, NULL, NULL, NULL);
	if(sampleBufferSize < 4){
		logmsg("ERROR: Invalid parameters for sync detection\n");
		return -1;
	}
	sampleBuffer = (double*)malloc(sampleBufferSize*sizeof(double));
	if(!sampleBuffer)
	{
		logmsgFileOnly("\tERROR: malloc failed for sample buffer during DetectPulseInternal\n");
		return -1;
	}

	totalSamples = header.data.DataSize/bytesPerSample;
	// calculate how many sampleBufferSize units fit in the available samples from the file
	TotalMS = totalSamples/sampleBufferSize-1;
	pulseArray = (Pulses*)malloc(sizeof(Pulses)*TotalMS);
	if(!pulseArray)
	{
		logmsgFileOnly("\tPulse malloc failed!\n");
		return(0);
	}
	memset(pulseArray, 0, sizeof(Pulses)*TotalMS);

	if(config->verbose)
		logmsg(" - Starting Internal Sync detection at %ld samples\n", SamplesForDisplay(offset, AudioChannels));

	pos = offset;
	if(offset)
	{
		i = offset/sampleBufferSize;
		start = i;
	}
	else
		TotalMS /= 6;

	while(i < TotalMS)
	{
		if(pos + sampleBufferSize > totalSamples)
		{
			//logmsg("\tUnexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
			break;
		}

		memset(sampleBuffer, 0, sampleBufferSize*sizeof(double));
		memcpy(sampleBuffer, Samples + pos, sampleBufferSize*sizeof(double));
		pulseArray[i].samples = pos;

		pos += sampleBufferSize;

		/* We use left channel by default, we don't know about channel imbalances yet */
		ProcessChunkForSyncPulse(sampleBuffer, sampleBufferSize, 
			header.fmt.SamplesPerSec, &pulseArray[i], 
			CHANNEL_LEFT, AudioChannels, config);

		if(pulseArray[i].magnitude > MaxMagnitude)
			MaxMagnitude = pulseArray[i].magnitude;
		i++;
	}

	for(i = start; i < TotalMS; i++)
	{
		if(pulseArray[i].hertz)  /* we can get this empty due to zeroes in samples */
			pulseArray[i].amplitude = CalculateAmplitude(pulseArray[i].magnitude, MaxMagnitude);
		else
			pulseArray[i].amplitude = NO_AMPLITUDE;
	}

	/*
	if(config->debugSync)
	{
		logmsgFileOnly("===== Searching for %gHz  =======\n", targetFrequency);
		for(i = 0; i < TotalMS; i++)
		{
				logmsgFileOnly("S: %ld Hz: %g A: %g\n", 
					pulseArray[i].samples, 
					pulseArray[i].hertz, 
					pulseArray[i].amplitude);
		}
		logmsgFileOnly("========  End listing =========\n");
	}
	*/

	/* if not found, compare all */
	offset = -1;
	if(endPulse) 
		*endPulse = -1;

	if(syncKnown)
	{
		targetFrequency = FindFrequencyBracketForSync(syncKnown, 
					sampleBufferSize, AudioChannels, header.fmt.SamplesPerSec, config);
		/*
		targetFrequencyHarmonic[0] = FindFrequencyBracketForSync(syncKnown*2, 
					millisecondSize/2, AudioChannels, header.fmt.SamplesPerSec, config);
		targetFrequencyHarmonic[1] = FindFrequencyBracketForSync(syncKnown*3, 
					millisecondSize/2, AudioChannels, header.fmt.SamplesPerSec, config);
		*/
		averageAmplitude = findAverageAmplitudeForTarget(pulseArray, targetFrequency, targetFrequencyHarmonic, TotalMS, start, factor, AudioChannels, config);
	}

	for(i = start; i < TotalMS; i++)
	{
		if(syncKnown)
		{
			if(pulseArray[i].amplitude > averageAmplitude &&
				pulseArray[i].hertz == targetFrequency) /* ||
				pulseArray[i].hertz == targetFrequencyHarmonic[0] ||
				pulseArray[i].hertz == targetFrequencyHarmonic[1]))  // harmonic */
			{
				if(offset == -1)
					offset = pulseArray[i].samples;
				length++;

				if(length == MIN_LEN && config->verbose)
					logmsg(" - Start detected at %ld samples\n",
						SamplesForDisplay(offset, AudioChannels));
			}
			else
			{
				if(offset != -1 && length > MIN_LEN)
				{
					int endProcess = 0;

					// If we are way smaller than expected...
					if(labs(expectedSyncLen - (pulseArray[i].samples-offset)) > (expectedSyncLen)/5)
					{
						tolerance ++;

						if(config->verbose && tolerance <= MaxTolerance)
							logmsg(" - Tolerance %ld/%ld (pulse active samples %ld from expected %ld) at %ld samples\n",
								tolerance, MaxTolerance, SamplesForDisplay(pulseArray[i].samples-offset, AudioChannels),
								SamplesForDisplay(expectedSyncLen, AudioChannels), SamplesForDisplay(pulseArray[i].samples, AudioChannels));

						if(toleranceIssueOffset == -1)
							toleranceIssueOffset = pulseArray[i].samples;
						if(tolerance > MaxTolerance)
							endProcess = 1;
					}
					else
						endProcess = 1;
					
					if(endProcess)
					{
						long int pulseend = 0;

						pulseend = pulseArray[i].samples;
						if(toleranceIssueOffset != -1)
						{
							// If we had errors, check if they are consecutive at the end
							if(pulseArray[i].samples - toleranceIssueOffset == (tolerance > MaxTolerance ? MaxTolerance : tolerance) * sampleBufferSize)
								pulseend = toleranceIssueOffset;
						}
						if(endPulse)
							*endPulse = pulseend;
						if(config->verbose)
							logmsg(" - EndPulse Set %ld\n", SamplesForDisplay(pulseend, AudioChannels));
						break;
					}
				}
				else
				{
					length = 0;
					offset = -1;
				}
			}
		}
		else
		{
			if(pulseArray[i].hertz)
			{
				double average = 0;

				total += pulseArray[i].amplitude;
				count ++;
				average = total/count;
	
				if(pulseArray[i].amplitude * 1.5 > average)
				{
					offset = pulseArray[i].samples;
					/*
					if(config->debugSync)
						logmsgFileOnly("Offset Set\n");
					*/
					break;
				}
			}
		}
	}

	if(tolerance)
	{
		logmsg(" - WARNING: Internal sync tone has anomalies at %g seconds (%ld samples)\n", 
				SamplesToSeconds(header.fmt.SamplesPerSec, toleranceIssueOffset, AudioChannels),
				SamplesForDisplay(toleranceIssueOffset, AudioChannels));
		if(toleranceIssue)
			*toleranceIssue = 1;
	}

	free(pulseArray);
	free(sampleBuffer);

	return offset;
}
