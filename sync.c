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

#define	FACTOR_LFEXPL	4
#define	FACTOR_EXPLORE	8
#define	FACTOR_DETECT	8

// Cut off for harmonic search
#define HARMONIC_TSHLD 6000

long int DetectPulse(char *AllSamples, wav_hdr header, int role, parameters *config)
{
	int			maxdetected = 0, AudioChannels = 0;
	long int	offset = 0;

	if(config->debugSync)
		logmsgFileOnly("\nStarting Detect start pulse\n");

	AudioChannels = header.fmt.NumOfChan;

	offset = DetectPulseInternal(AllSamples, header, FACTOR_EXPLORE, 0, &maxdetected, role, AudioChannels, config);
	if(offset == -1)
	{
		if(config->debugSync)
			logmsgFileOnly("First round start pulse failed\n", offset);

		return DetectPulseSecondTry(AllSamples, header, role, config);
	}

	offset = AdjustPulseSampleStart(AllSamples, header, offset, role, AudioChannels, config);
	if(offset != -1)
		return offset;

	if(config->debugSync)
		logmsgFileOnly("Start pulse return value %ld\n", offset);

	return offset;
}

/* only difference is that it auto detects the start first, helps in some cases with long silence and high noise floor */
long int DetectPulseSecondTry(char *AllSamples, wav_hdr header, int role, parameters *config)
{
	int			maxdetected = 0, AudioChannels = 0;
	long int	offset = 0;

	if(config->debugSync)
		logmsgFileOnly("\nStarting Detect start pulse\n");

	AudioChannels = header.fmt.NumOfChan;

	offset = DetectSignalStart(AllSamples, header, 0, 0, 0, NULL, NULL, config);
	if(offset > 0)
	{
		long int MSBytes = 0;
			
		MSBytes = SecondsToBytes(header.fmt.SamplesPerSec, 0.015, AudioChannels, NULL, NULL, NULL);
		if(offset >= MSBytes)
			offset -= MSBytes;
	}
	else
		offset = 0;

	offset = DetectPulseInternal(AllSamples, header, FACTOR_LFEXPL, offset, &maxdetected, role, AudioChannels, config);
	if(offset == -1)
	{
		if(config->debugSync)
			logmsgFileOnly("First round start pulse failed\n", offset);

		return -1;
	}

	offset = AdjustPulseSampleStart(AllSamples, header, offset, role, AudioChannels, config);
	if(offset != -1)
		return offset;

	if(config->debugSync)
		logmsgFileOnly("Start pulse return value %ld\n", offset);

	return offset;
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

long int DetectEndPulse(char *AllSamples, long int startpulse, wav_hdr header, int role, parameters *config)
{
	int			maxdetected = 0, frameAdjust = 0, tries = 0, maxtries = END_SYNC_MAX_TRIES;
	int			factor = 0, AudioChannels = 0;
	long int 	offset = 0;
	double		silenceOffset[END_SYNC_MAX_TRIES] = END_SYNC_VALUES;


	if(GetPulseSyncFreq(role, config) < HARMONIC_TSHLD)
		factor = FACTOR_LFEXPL;
	else
		factor = FACTOR_EXPLORE;
	/* Try a clean detection */
	offset = GetSecondSyncSilenceByteOffset(GetMSPerFrameRole(role, config), header, 0, 1, config) + startpulse;
	if(config->debugSync)
		logmsgFileOnly("\nStarting CLEAN Detect end pulse with offset %ld\n", offset);
	AudioChannels = header.fmt.NumOfChan;
	offset = DetectPulseInternal(AllSamples, header, factor, offset, &maxdetected, role, AudioChannels, config);
	if(offset != -1)
	{
		offset = AdjustPulseSampleStart(AllSamples, header, offset, role, AudioChannels, config);
		if(offset != -1)
			return offset;
	}

	
	/* We try to figure out position of the pulses */
	if(config->debugSync)
		logmsgFileOnly("End pulse CLEAN detection failed started search at %ld bytes\n", offset);

	do
	{
		/* Use defaults to calculate real frame rate */
		offset = GetSecondSyncSilenceByteOffset(GetMSPerFrameRole(role, config), header, frameAdjust, silenceOffset[tries], config) + startpulse;
	
		if(config->debugSync)
			logmsgFileOnly("\nStarting Detect end pulse with offset %ld [%g silence]\n\tMaxDetected %d frameAdjust: %d\n",
				offset, silenceOffset[tries], maxdetected, frameAdjust);
	
		frameAdjust = 0;
		maxdetected = 0;

		offset = DetectPulseInternal(AllSamples, header, factor, offset, &maxdetected, role, AudioChannels, config);
		if(offset == -1 && !maxdetected)
		{
			if(config->debugSync)
				logmsgFileOnly("End pulse failed try %d, started search at %ld bytes [%g silence]\n", tries+1, offset, silenceOffset[tries]);
		}

		tries ++;
	}while(offset == -1 && tries < maxtries);

	if(tries == maxtries)
		return -1;

	offset = AdjustPulseSampleStart(AllSamples, header, offset, role, AudioChannels, config);
	if(config->debugSync)
		logmsgFileOnly("End pulse return value %ld\n", offset);

	return offset;
}


#define LOGCASE(x, y) { x; if(config->debugSync) logmsgFileOnly("Case #%d\n", y); }
double findAverageAmplitudeForTarget(Pulses *pulseArray, double targetFrequency, double *targetFrequencyHarmonic, long int TotalMS, long int start, int factor, parameters *config)
{
	long	count = 0, i = 0;
	double	averageAmplitude = 0, standardDeviation = 0, useAmplitude = 0, percent = 0;

	if(config->debugSync)
		logmsgFileOnly("Searching for in range: %ld-%ld bytes\n", pulseArray[start].bytes, pulseArray[TotalMS-1].bytes);
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
		logmsgFileOnly("Searching for Average amplitude in block: F %g Total Start byte: %ld milliseconds to check: %ld\n", 
			targetFrequency, 
			pulseArray[start].bytes,
			TotalMS);


	return useAmplitude;
}

long int DetectPulseTrainSequence(Pulses *pulseArray, double targetFrequency, double *targetFrequencyHarmonic, long int TotalMS, int factor, int *maxdetected, long int start, int role, parameters *config)
{
	long	i, sequence_start = 0;
	int		frame_pulse_count = 0, frame_silence_count = 0, 
			pulse_count = 0, silence_count = 0, lastcountedWasPulse = 0, lookingfor = 0;
	double	averageAmplitude = 0;

	*maxdetected = 0;

	lookingfor = getPulseFrameLen(role, config)*factor-factor/2;
	//smoothAmplitudes(pulseArray, targetFrequency, TotalMS, start);
	averageAmplitude = findAverageAmplitudeForTarget(pulseArray, targetFrequency, targetFrequencyHarmonic, TotalMS, start, factor, config);
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
					logmsgFileOnly("[i:%ld] byte:%7ld [%5gHz %0.2f dBFS] <<Pulse Frame counted %d>>", 
						i, pulseArray[i].bytes, pulseArray[i].hertz, pulseArray[i].amplitude, 
						frame_pulse_count);
	
				if(!sequence_start)
				{
					if(config->debugSync)
						logmsgFileOnly(" This starts the sequence\n");

					sequence_start = pulseArray[i].bytes;
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
					logmsgFileOnly("[i:%ld] byte:%7ld [%5gHz %0.2f dBFS] Silence Frame found due to AVG but skipped due to Hz%d\n", 
						i, pulseArray[i].bytes, pulseArray[i].hertz, pulseArray[i].amplitude, 
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
					logmsgFileOnly("[i:%ld] byte:%7ld [%5gHz %0.2f dBFS] Silence Frame counted %d\n", 
						i, pulseArray[i].bytes, pulseArray[i].hertz, pulseArray[i].amplitude, 
						frame_silence_count);

				frame_silence_count ++;
			}
			else
			{
				int linefeedNeeded = 1;

				if(config->debugSync)
					logmsgFileOnly("[i:%ld] byte:%7ld [%5gHz %0.2f dBFS] Non Frame skipped %d LC:%d", 
						i, pulseArray[i].bytes, pulseArray[i].hertz, pulseArray[i].amplitude, 
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
						logmsgFileOnly("Completed the sequence %ld\n", sequence_start);
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
						logmsgFileOnly("Resets the sequence (silence too long)\n");
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

long int AdjustPulseSampleStart(char *Samples, wav_hdr header, long int offset, int role, int AudioChannels, parameters *config)
{
	int			samplesNeeded = 0, frequency = 0, bytesNeeded = 0, minDiffPos = -1;
	long int	startSearch = 0, endSearch = 0, pos = 0, count = 0, foundPos = -1;
	char		*buffer = NULL;
	Pulses		*pulseArray = NULL;
	double		targetFrequency = 0, minDiff = 1000;

	frequency = GetPulseSyncFreq(role, config);
	samplesNeeded = header.fmt.SamplesPerSec/frequency;
	bytesNeeded = samplesNeeded*AudioChannels*2;

	targetFrequency = FindFrequencyBracketForSync(frequency,
						bytesNeeded/2, AudioChannels, header.fmt.SamplesPerSec, config);
	if(config->debugSync)
		logmsgFileOnly("\nGot %ld, looking for %g bytes %d\n", offset, targetFrequency, bytesNeeded);
	buffer = (char*)malloc(bytesNeeded);
	if(!buffer)
	{
		logmsgFileOnly("\tSync Adjust malloc failed\n");
		return(foundPos);
	}

	if(offset >= 2*bytesNeeded)
		startSearch = offset-2*bytesNeeded;
	else
		startSearch = 0;
	endSearch = offset+bytesNeeded;

	pulseArray = (Pulses*)malloc(sizeof(Pulses)*(endSearch-startSearch));
	if(!pulseArray)
	{
		free(buffer);
		logmsgFileOnly("\tPulse malloc failed!\n");
		return(foundPos);
	}
	memset(pulseArray, 0, sizeof(Pulses)*(endSearch-startSearch));

	for(pos = startSearch; pos < endSearch; pos += 2*AudioChannels)  // bytes
	{
		memset(buffer, 0, bytesNeeded);
		if(pos + bytesNeeded > header.data.DataSize)
		{
			//logmsg("\tUnexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
			break;
		}

		pulseArray[count].bytes = pos;
		memcpy(buffer, Samples + pos, bytesNeeded);
		ProcessChunkForSyncPulse((int16_t*)buffer, bytesNeeded/2, 
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
				logmsgFileOnly("Bytes: %ld %gHz Mag %g Phase %g diff: %g\n",
					pulseArray[pos].bytes, pulseArray[pos].hertz, pulseArray[pos].magnitude, pulseArray[pos].phase, diff);
			if(diff < minDiff)
			{
				minDiff = diff;
				minDiffPos = pos;
			}
		}
	}

	if(minDiffPos != -1)
	{
		foundPos = pulseArray[minDiffPos].bytes;
		if(config->debugSync)
			logmsgFileOnly("FOUND: %ld Bytes: %ld %gHz Mag %g Phase %g diff: %g\n",
				minDiffPos, pulseArray[minDiffPos].bytes, pulseArray[minDiffPos].hertz, pulseArray[minDiffPos].magnitude, pulseArray[minDiffPos].phase, minDiff);
	}

	free(buffer);
	free(pulseArray);

	return foundPos;
}

long int DetectPulseInternal(char *Samples, wav_hdr header, int factor, long int offset, int *maxdetected, int role, int AudioChannels, parameters *config)
{
	long int			i = 0, TotalMS = 0;
	long int			loadedBlockSize = 0;
	char				*buffer;
	size_t			 	buffersize = 0;
	long int			pos = 0, millisecondSize = 0, startPos = 0;
	Pulses				*pulseArray;
	double				targetFrequency = 0, targetFrequencyHarmonic[2] = { NO_FREQ, NO_FREQ }, origFrequency = 0, MaxMagnitude = 0;

	/* Not a real ms, just approximate */
	millisecondSize = RoundToNbytes(floor((((double)header.fmt.SamplesPerSec*2.0*AudioChannels)/1000.0)/(double)factor), AudioChannels, NULL, NULL, NULL);
	buffersize = millisecondSize*sizeof(char); 
	buffer = (char*)malloc(buffersize);
	if(!buffer)
	{
		logmsgFileOnly("\tmalloc failed\n");
		return(0);
	}

	TotalMS = header.data.DataSize / buffersize - 1;
	pos = offset;
	if(offset)
	{
		double msLen = 0;

		i = offset/buffersize;
		startPos = i;

		/* check for the duration of the sync pulses */
		msLen = GetLastSyncDuration(GetMSPerFrameRole(role, config), config)*1000;
		if(factor == FACTOR_EXPLORE)  /* are we exploring? */
			msLen *= 2.0;  /* widen so that the silence offset is compensated for */
		else
			msLen *= 1.2;  /* widen so that the silence offset is compensated for */
		TotalMS = i + floor(msLen*factor);

		if(config->debugSync)
			logmsgFileOnly("changed to:\n\tMS: %ld, BuffSize: %ld, Bytes:%ld-%ld/ms:%ld-%ld]\n\tms len: %g Bytes: %g Buffer Size: %d Factor: %d\n", 
				millisecondSize, buffersize, i*buffersize, TotalMS*buffersize, i, TotalMS,
				msLen, floor(msLen*factor), (int)buffersize, factor);
	}
	else
		TotalMS /= 4;

	pulseArray = (Pulses*)malloc(sizeof(Pulses)*TotalMS);
	if(!pulseArray)
	{
		logmsgFileOnly("\tPulse malloc failed!\n");
		return(0);
	}
	memset(pulseArray, 0, sizeof(Pulses)*TotalMS);

	origFrequency = GetPulseSyncFreq(role, config);
	targetFrequency = FindFrequencyBracketForSync(origFrequency,
						millisecondSize/2, AudioChannels, header.fmt.SamplesPerSec, config);
	if(origFrequency < HARMONIC_TSHLD)  //default behavior for around 8khz, harmonic is NO_FREQ
	{
		targetFrequencyHarmonic[0] = FindFrequencyBracketForSync(targetFrequency*2, 	
						millisecondSize/2, AudioChannels, header.fmt.SamplesPerSec, config);
		targetFrequencyHarmonic[1] = FindFrequencyBracketForSync(targetFrequency*3,
						millisecondSize/2, AudioChannels, header.fmt.SamplesPerSec, config);
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
			 i, TotalMS-1, header.data.DataSize / buffersize - 1);
	}

	while(i < TotalMS)
	{
		loadedBlockSize = millisecondSize;

		memset(buffer, 0, buffersize);
		if(pos + loadedBlockSize > header.data.DataSize)
		{
			//logmsg("\tUnexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
			break;
		}

		pulseArray[i].bytes = pos;
		memcpy(buffer, Samples + pos, loadedBlockSize);

#ifdef SAVE_CHUNKS
		if(1)
		{	
			char FName[4096];
			AudioSignal s;

			s.header = header;
			sprintf(FName,	 "%06ld_SYNC_chunk.wav", i);

			SaveWAVEChunk(FName, &s, buffer, 0, loadedBlockSize, config); 
		}
#endif

		pos += loadedBlockSize;

		/* We use left channel by default, we don't know about channel imbalances yet */
		ProcessChunkForSyncPulse((int16_t*)buffer, loadedBlockSize/2, 
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
		for(i = startPos; i < TotalMS; i++)
		{
			//if(pulseArray[i].hertz == targetFrequency)
				logmsgFileOnly("B: %ld Hz: %g A: %g M: %g\n", 
					pulseArray[i].bytes, 
					pulseArray[i].hertz, 
					pulseArray[i].amplitude,
					pulseArray[i].magnitude);
		}
		logmsgFileOnly("========  End listing =========\n");
	}
*/
	offset = DetectPulseTrainSequence(pulseArray, targetFrequency, targetFrequencyHarmonic, TotalMS, factor, maxdetected, startPos, role, config);

	free(pulseArray);
	free(buffer);

	return offset;
}

double ProcessChunkForSyncPulse(int16_t *samples, size_t size, long samplerate, Pulses *pulse, char channel, int AudioChannels, parameters *config)
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
			maxMag = magnitude;
			maxHertz = CalculateFrequency(i, boxsize);
			maxPhase = CalculatePhase(spectrum[i]);
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

long int DetectSignalStart(char *AllSamples, wav_hdr header, long int offset, int syncKnow, long int expectedSyncLen, long int *endPulse, int *toleranceIssue, parameters *config)
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
			logmsgFileOnly("Detect signal failed\n", offset);

		return -1;
	}

	if(config->debugSync)
		logmsgFileOnly("Detect signal return value %ld\n", position);
	return position;
}

//#define DEBUG_SYNC_INTERNAL_TOLERANCE
long int DetectSignalStartInternal(char *Samples, wav_hdr header, int factor, long int offset, int syncKnown, long int expectedSyncLen, int *maxdetected, long int *endPulse, int AudioChannels, int *toleranceIssue, parameters *config)
{
	long int			i = 0, TotalMS = 0, start = 0;
	long int			loadedBlockSize = 0;
	char				*buffer;
	size_t			 	buffersize = 0;
	long int			pos = 0, millisecondSize = 0;
	double				MaxMagnitude = 0;
	Pulses				*pulseArray;
	double 				total = 0;
	long int 			count = 0, length = 0, tolerance = 0, toleranceIssueOffset = -1, MaxTolerance = 4;
	double 				targetFrequency = 0, targetFrequencyHarmonic[2] = { NO_FREQ, NO_FREQ }, averageAmplitude = 0;

	/* Not a real ms, just approximate */
	millisecondSize = RoundToNbytes(floor((((double)header.fmt.SamplesPerSec*2.0*(double)AudioChannels)/1000.0)/(double)factor), AudioChannels, NULL, NULL, NULL);
	buffersize = millisecondSize*sizeof(char); 
	buffer = (char*)malloc(buffersize);
	if(!buffer)
	{
		logmsgFileOnly("\tmalloc failed\n");
		return(0);
	}
	
	TotalMS = header.data.DataSize / buffersize - 1;
	pulseArray = (Pulses*)malloc(sizeof(Pulses)*TotalMS);
	if(!pulseArray)
	{
		logmsgFileOnly("\tPulse malloc failed!\n");
		return(0);
	}
	memset(pulseArray, 0, sizeof(Pulses)*TotalMS);

	pos = offset;
	if(offset)
	{
		i = offset/buffersize;
		start = i;
	}
	else
		TotalMS /= 6;

	if(config->debugSync)
		logmsg("- Signal range from %ld to %ld\n", start, TotalMS);

	while(i < TotalMS)
	{
		loadedBlockSize = millisecondSize;

		memset(buffer, 0, buffersize);
		if(pos + loadedBlockSize > header.data.DataSize)
		{
			logmsgFileOnly("\tunexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
			break;
		}

		pulseArray[i].bytes = pos;
		memcpy(buffer, Samples + pos, loadedBlockSize);

#ifdef SAVE_CHUNKS
		if(1)
		{	
			char FName[4096];
			AudioSignal s;

			s.header = header;
			sprintf(FName,	 "%06ld_SYNC_chunk.wav", i);

			SaveWAVEChunk(FName, &s, buffer, 0, loadedBlockSize, config); 
		}
#endif

		pos += loadedBlockSize;

		/* we go left, any signal is fine here */
		ProcessChunkForSyncPulse((int16_t*)buffer, loadedBlockSize/2, 
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
		//logmsgFileOnly("===== Searching for %gHz  =======\n", targetFrequency);
		for(i = 0; i < TotalMS; i++)
		{
				logmsgFileOnly("B: %ld Hz: %g A: %g\n", 
					pulseArray[i].bytes, 
					pulseArray[i].hertz, 
					pulseArray[i].amplitude);
		}
		//logmsgFileOnly("========  End listing =========\n");
	}
	*/

	/* if not found, compare all */
	offset = -1;
	if(endPulse) 
		*endPulse = -1;

	if(syncKnown)
	{
		targetFrequency = FindFrequencyBracketForSync(syncKnown, 
					millisecondSize/2, AudioChannels, header.fmt.SamplesPerSec, config);
		/*
		targetFrequencyHarmonic[0] = FindFrequencyBracketForSync(syncKnown*2, 
					millisecondSize/2, AudioChannels, header.fmt.SamplesPerSec, config);
		targetFrequencyHarmonic[1] = FindFrequencyBracketForSync(syncKnown*3, 
					millisecondSize/2, AudioChannels, header.fmt.SamplesPerSec, config);
		*/
		averageAmplitude = findAverageAmplitudeForTarget(pulseArray, targetFrequency, targetFrequencyHarmonic, TotalMS, start, factor, config);
	}

	for(i = start; i < TotalMS; i++)
	{
#ifdef DEBUG_SYNC_INTERNAL_TOLERANCE
		/*
		if(pulseArray[i].hertz)
			logmsgFileOnly("Bytes: %ld Hz: %g Ampl: %g Avg: %g (%ld)\n", 
				pulseArray[i].bytes, 
				pulseArray[i].hertz, 
				pulseArray[i].amplitude, 
				averageAmplitude, i);
		*/
#endif

		if(syncKnown)
		{
			if(pulseArray[i].amplitude > averageAmplitude &&
				pulseArray[i].hertz == targetFrequency) /* ||
				pulseArray[i].hertz == targetFrequencyHarmonic[0] ||
				pulseArray[i].hertz == targetFrequencyHarmonic[1]))  // harmonic */
			{
				if(offset == -1)
					offset = pulseArray[i].bytes;
				length++;
			}
			else
			{
				if(offset != -1 && length > 4)
				{
					int endProcess = 0;

					// If we are way smaller than expected...
					if(abs(expectedSyncLen - (pulseArray[i].bytes-offset)) > (expectedSyncLen)/5)
					{
						tolerance ++;

#ifdef DEBUG_SYNC_INTERNAL_TOLERANCE
						logmsg("- Tolerance %ld/%ld (bytes %ld from expected %ld)\n",
								tolerance, MaxTolerance, (pulseArray[i].bytes-offset), expectedSyncLen);
#endif
						if(toleranceIssueOffset == -1)
							toleranceIssueOffset = pulseArray[i].bytes;
						if(tolerance > MaxTolerance)
							endProcess = 1;
					}
					else
						endProcess = 1;
					
					if(endProcess)
					{
						if(endPulse)
							*endPulse = pulseArray[i].bytes;
#ifdef DEBUG_SYNC_INTERNAL_TOLERANCE
						logmsg("EndPulse Set %ld\n", pulseArray[i].bytes);
#endif
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
					offset = pulseArray[i].bytes;
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
		logmsg("\t- WARNING: Internal sync tone has anomalies at %ld bytes\n", toleranceIssueOffset);
		if(toleranceIssue)
			*toleranceIssue = 1;
	}

	free(pulseArray);
	free(buffer);

	return offset;
}
