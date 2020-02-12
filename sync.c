/* 
 * MDFourier
 * A Fourier Transform analysis tool to compare different 
 * Sega Genesis/Mega Drive audio hardware revisions.
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
 */

#include "mdfourier.h"
#include "sync.h"
#include "log.h"
#include "freq.h"

/*
	There are the number of subdivisions to use. 
	The higher, the less precise in frequency but more in position and vice versa 
*/
#define	FACTOR_EXPLORE	4
#define	FACTOR_DETECT	9

#define DOUBLE_SYNC
/* #define FREQ_DETECT */

long int DetectPulse(char *AllSamples, wav_hdr header, int role, parameters *config)
{
	int			maxdetected = 0, errcount = 0, AudioChannels = 0;
	long int	position = 0, offset = 0;

	if(config->debugSync)
		logmsgFileOnly("\nStarting Detect start pulse\n");

	AudioChannels = header.fmt.NumOfChan;
	position = DetectPulseInternal(AllSamples, header, FACTOR_EXPLORE, 0, &maxdetected, role, AudioChannels, config);
	if(position == -1)
	{
		if(config->debugSync)
			logmsgFileOnly("First round start pulse failed\n", offset);

		return -1;
	}

	if(errcount && position != -1)
		logmsgFileOnly(" - Sync pulse had %d imperfections\n", errcount);

#ifdef DOUBLE_SYNC
	if(config->debugSync)
		logmsgFileOnly("First round start pulse detected at %ld, refinement\n", position);

	offset = position;
	if(offset >= 8*22*AudioChannels)  /* return 8 "ms segments" as dictated by ratio "9" below */
		offset -= 8*22*AudioChannels;

	maxdetected = 0;
	errcount = 0;
	position = DetectPulseInternal(AllSamples, header, FACTOR_DETECT, offset, &maxdetected, role, AudioChannels, config);
#endif

	if(config->debugSync)
		logmsgFileOnly("Start pulse return value %ld\n", position);

	return position;
}

/*
 positions relative to the expected one
 Start with common sense ones, then search all around the place
 2.1 and above were added for PAL MD at 60 detection. Yes, that is 2.1
*/

#define END_SYNC_MAX_TRIES		44
#define END_SYNC_VALUES			{ 0.50, 0.25, 0.0, 1.25, 1.50,\
								0.9, 0.8, 0.7, 0.6, 1.6, 1.7, 1.8, 1.9, \
								0.4, 0.3, 0.1, 1.1, 1.3, 1.4,\
								1.0, -1,0, 2.0, -2.0,\
								2.1, 2.2, 2.3, 2.4, 2.5, 2.5, 2.7, 2.8, 2.9, 3.0,\
								-2.1, -2.2, -2.3, -2.4, -2.5, -2.5, -2.7, -2.8, -2.9, -3.0 }

long int DetectEndPulse(char *AllSamples, long int startpulse, wav_hdr header, int role, parameters *config)
{
	int			maxdetected = 0, frameAdjust = 0, tries = 0, maxtries = END_SYNC_MAX_TRIES, errcount = 0, AudioChannels = 0;
	long int 	position = 0, offset = 0;
	double		silenceOffset[END_SYNC_MAX_TRIES] = END_SYNC_VALUES;

	/* Try a clean detection */
	offset = GetSecondSyncSilenceByteOffset(GetMSPerFrameRole(role, config), header, 0, 1, config) + startpulse;
	if(config->debugSync)
		logmsgFileOnly("\nStarting CLEAN Detect end pulse with offset %ld\n", offset);
	AudioChannels = header.fmt.NumOfChan;
	position = DetectPulseInternal(AllSamples, header, FACTOR_DETECT, offset, &maxdetected, role, AudioChannels, config);
	if(position != -1)
		return position;
	
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
		errcount = 0;
		position = DetectPulseInternal(AllSamples, header, FACTOR_EXPLORE, offset, &maxdetected, role, AudioChannels, config);
		if(position == -1 && !maxdetected)
		{
			if(config->debugSync)
				logmsgFileOnly("End pulse failed try %d, started search at %ld bytes [%g silence]\n", tries+1, offset, silenceOffset[tries]);
		}

		/*
		if(position == -1 && maxdetected)
			frameAdjust = getPulseCount(role, config) - maxdetected/2;
		*/
		tries ++;
	}while(position == -1 && tries < maxtries);


	if(errcount && position != -1)
		logmsgFileOnly(" - Sync pulse had %d imperfections\n", errcount);

	if(tries == maxtries)
		return -1;

#ifdef DOUBLE_SYNC
	if(config->debugSync)
		logmsgFileOnly("First round end pulse detected at %ld, refinement\n", position);

	offset = position;
	if(offset >= 8*22*AudioChannels)  /* return 8 "ms segments" as dictated by ratio "9" below */
		offset -= 8*22*AudioChannels;

	maxdetected = 0;
	errcount = 0;
	position = DetectPulseInternal(AllSamples, header, FACTOR_DETECT, offset, &maxdetected, role, AudioChannels, config);
#endif

	if(config->debugSync)
		logmsgFileOnly("End pulse return value %ld\n", position);

	return position;
}

void smoothAmplitudes(Pulses *pulseArray, double targetFrequency, long int TotalMS, long int start)
{
	long	i = 0;

	for(i = start; i < TotalMS - 1; i++)
	{
		if(pulseArray[i].hertz && i > 1)
		{
			double average = 0;

			/*
			logmsgFileOnly("Checking %7ld [%5gHz %0.2f dBFS]\n", 
				pulseArray[i].bytes, pulseArray[i].hertz, pulseArray[i].amplitude);
			*/
			average = (pulseArray[i-1].amplitude + pulseArray[i].amplitude + pulseArray[i+1].amplitude)/3;
			if(pulseArray[i].amplitude < average - 3 || pulseArray[i].amplitude > average + 3)
			{
				pulseArray[i].amplitude = average;
				/* logmsgFileOnly(" -> Changed to %g\n", average); */
			}
		}
	}
}

double executeFindAverageAmplitudeForTarget(Pulses *pulseArray, double targetFrequency, long int TotalMS, long int start)
{
	int		freq_pos = 0;
	long	count = 0, i = 0;
	double	lastFreqs[4], averageAmplitude = 0;

	memset(lastFreqs, 0, sizeof(double)*4);
	for(i = start; i < TotalMS; i++)
	{
		if(pulseArray[i].hertz)
		{
			lastFreqs[freq_pos++] = pulseArray[i].hertz;
			if(freq_pos > 3)
				freq_pos = 0;
			if(lastFreqs[0] == targetFrequency && lastFreqs[1] == targetFrequency &&
				lastFreqs[2] == targetFrequency && lastFreqs[3] == targetFrequency)
			{
				averageAmplitude += pulseArray[i].amplitude;
				count ++;
			}
		}
	}

	if(!count)
	{
		logmsgFileOnly("WARNING! Average Amplitude values for sync not found in range (NULL from digital/emu)\n");
		/*
		for(i = start; i < TotalMS; i++)
		{
			if(pulseArray[i].hertz)
				logmsgFileOnly("byte: %ld ms: %ld hz: %g amp: %g\n", pulseArray[i].bytes, i, pulseArray[i].hertz, pulseArray[i].amplitude);
		}
		*/
		return 0;
	}
	/* Get an amplitude below the average value for the pulses */
	averageAmplitude /= count;
	averageAmplitude += -6;

	return averageAmplitude;
}

double findAverageAmplitudeForTarget(Pulses *pulseArray, double targetFrequency, long int TotalMS, long int start, parameters *config)
{
	double averageAmplitude = 0;

	averageAmplitude = executeFindAverageAmplitudeForTarget(pulseArray, targetFrequency, TotalMS, start);

	if(config->debugSync)
		logmsgFileOnly("Searching for Average amplitude in block: F %g Total Start byte: %ld milliseconds to check: %ld\n", 
			targetFrequency, 
			pulseArray[start].bytes,
			TotalMS);

	if(averageAmplitude < -20)  /* if we have too much noise at the TargetFrequency, smooth it out */
	{
		if(config->debugSync)
			logmsgFileOnly("Average Amplitude was %g, smoothing range\n", averageAmplitude);
		smoothAmplitudes(pulseArray, targetFrequency, TotalMS, start);
		averageAmplitude = executeFindAverageAmplitudeForTarget(pulseArray, targetFrequency, TotalMS, start);
		if(config->debugSync)
			logmsgFileOnly("Average Amplitude Smoothing result: %g\n", averageAmplitude);

		if(averageAmplitude < -35)  /* Still too much noise? gamble... */
		{
			averageAmplitude = -30;
			if(config->debugSync)
				logmsgFileOnly("Average Amplitude Still too high, forcing to: %g\n", averageAmplitude);
		}
	}

	return averageAmplitude;
}

long int DetectPulseTrainSequence(Pulses *pulseArray, double targetFrequency, double targetFrequencyHarmonic, long int TotalMS, int factor, int *maxdetected, long int start, int role, parameters *config)
{
	long	i, sequence_start = 0;
	int		frame_pulse_count = 0, frame_silence_count = 0, 
			pulse_count = 0, silence_count = 0, lastcounted = 0;
	double	averageAmplitude = 0;

	*maxdetected = 0;

	averageAmplitude = findAverageAmplitudeForTarget(pulseArray, targetFrequency, TotalMS, start, config);
	if(averageAmplitude == 0)
		return -1;

	if(config->debugSync)
		logmsgFileOnly("== Searching for %g/%g Average Amplitude %g looking for %d (%d*%d)\n", 
			targetFrequency, targetFrequencyHarmonic, averageAmplitude, getPulseFrameLen(role, config)*factor,
			getPulseFrameLen(role, config), factor);

	for(i = start; i < TotalMS; i++)
	{
		if(pulseArray[i].hertz)
		{
			if(config->syncTolerance)
				targetFrequency = pulseArray[i].hertz;
			if(pulseArray[i].amplitude >= averageAmplitude
				&& (pulseArray[i].hertz == targetFrequency /*|| pulseArray[i].hertz == targetFrequencyHarmonic*/))
			{
				frame_pulse_count++;
				lastcounted = 1;
				if(config->debugSync)
					logmsgFileOnly("[i:%ld] byte:%7ld [%5gHz %0.2f dBFS]Pulse Frame counted %d\n", 
						i, pulseArray[i].bytes, pulseArray[i].hertz, pulseArray[i].amplitude, 
						frame_pulse_count);
	
				if(!sequence_start)
				{
					if(config->debugSync)
						logmsgFileOnly("This starts the sequence\n");
					sequence_start = pulseArray[i].bytes;
					frame_silence_count = 0;
				}
	
				if(frame_silence_count >= getPulseFrameLen(role, config)*factor*4/5)  /* allow silence to have some stray noise */
				{
					silence_count++;
	
					if(config->debugSync)
						logmsgFileOnly("Closed a silence cycle %d\n", silence_count);
					if(silence_count > getPulseCount(role, config))
					{
						if(config->debugSync)
							logmsgFileOnly("Resets the sequence\n");
						sequence_start = 0;
						silence_count = 0;
					}
				}
	
				if(frame_silence_count)
					frame_silence_count = 0;
			}
			else
			{
				if(pulseArray[i].amplitude < averageAmplitude)
				{
					frame_silence_count ++;
					lastcounted = 0;
					if(config->debugSync)
						logmsgFileOnly("[i:%ld] byte:%7ld [%5gHz %0.2f dBFS] Silence Frame counted %d\n", 
							i, pulseArray[i].bytes, pulseArray[i].hertz, pulseArray[i].amplitude, 
							frame_silence_count);

					if(frame_pulse_count >= getPulseFrameLen(role, config)*factor)
					{
						pulse_count++;
		
						if(config->debugSync)
							logmsgFileOnly("Closed pulse #%d cycle, silence count %d pulse count %d\n", pulse_count, silence_count, frame_pulse_count);
						
						if(config->syncTolerance)
							silence_count = pulse_count - 1;

						if(pulse_count == getPulseCount(role, config) && silence_count == pulse_count - 1)
						{
							if(config->debugSync)
								logmsgFileOnly("Completed the sequence %ld\n", sequence_start);
							return sequence_start;
						}
					}
		
					if(frame_pulse_count > 0)
					{
						if(!pulse_count && sequence_start)
						{
							if(config->debugSync)
								logmsgFileOnly("Resets the sequence (no pulse count)\n");
							sequence_start = 0;
						}
		
						frame_pulse_count = 0;
					}
				}
				else
				{
					if(lastcounted == 0)
					{
						if(config->debugSync)
							logmsgFileOnly("NON SKIPPED and counting as silence\n");
						frame_silence_count++;
					}

					if(lastcounted == 1 && frame_pulse_count >= getPulseFrameLen(role, config)*factor)
					{
						if(config->debugSync)
							logmsgFileOnly("NON SKIPPED and counting as silence due to pulse count\n");
						frame_silence_count++;
					}

					if(config->debugSync)
						logmsgFileOnly("%7ld [%5gHz %0.2f dBFS] Non Frame skipped %d\n", 
							pulseArray[i].bytes, pulseArray[i].hertz, pulseArray[i].amplitude, 
							frame_silence_count);
				}
			}
		}
		else
		{
			/*if(lastcounted == 0)*/
			{
				frame_silence_count++;
				if(config->debugSync)
				{
					if(pulseArray[i].hertz)
						logmsgFileOnly("SKIPPED and counting as silence %gHz %gdBFs\n",
							pulseArray[i].hertz, pulseArray[i].amplitude);
					else
						logmsgFileOnly("SKIPPED and counting as silence [NULL]\n");
				}

				/* Extra CHECK for emulators */
				if(frame_pulse_count >= getPulseFrameLen(role, config)*factor)
				{
					pulse_count++;
	
					if(config->debugSync)
						logmsgFileOnly("Closed pulse #%d cycle, silence count %d pulse count %d [NULLs]\n", pulse_count, silence_count, frame_pulse_count);
					
					if(config->syncTolerance)
						silence_count = pulse_count - 1;

					if(pulse_count == getPulseCount(role, config) && silence_count == pulse_count - 1)
					{
						if(config->debugSync)
							logmsgFileOnly("Completed the sequence %ld [NULLs]\n", sequence_start);
						return sequence_start;
					}
					frame_pulse_count = 0;
				}
			}
			/*
			else
				if(config->debugSync)
					logmsgFileOnly("SKIPPED\n");
			*/
		}
	}

	if(config->debugSync)
		logmsgFileOnly("Failed\n");
	*maxdetected = pulse_count;
	return -1;
}


long int DetectPulseTrainSequenceFreqOnly(Pulses *pulseArray, double targetFrequency, double targetFrequencyHarmonic, long int TotalMS, int factor, int *maxdetected, long int start, int role, parameters *config)
{
	long	i, sequence_start = 0;
	int		frame_pulse_count = 0, frame_silence_count = 0, 
			pulse_count = 0, silence_count = 0;

	*maxdetected = 0;

	if(config->debugSync)
		logmsgFileOnly("== Searching for %g/%g looking for %d (%d*%d)\n", 
			targetFrequency, targetFrequencyHarmonic, getPulseFrameLen(role, config)*factor,
			getPulseFrameLen(role, config), factor);

	for(i = start; i < TotalMS; i++)
	{
		if(pulseArray[i].hertz == targetFrequency /* || pulseArray[i].hertz == targetFrequencyHarmonic */)
		{
			frame_pulse_count++;

			if(config->debugSync)
				logmsgFileOnly("[i:%ld] byte:%7ld [%5gHz %0.2f dBFS]Pulse Frame counted %d\n", 
					i, pulseArray[i].bytes, pulseArray[i].hertz, pulseArray[i].amplitude, 
					frame_pulse_count);

			if(!sequence_start)
			{
				if(config->debugSync)
					logmsgFileOnly("This starts the sequence\n");
				sequence_start = pulseArray[i].bytes;
				frame_silence_count = 0;
			}

			if(frame_silence_count >= getPulseFrameLen(role, config)*factor)
			{
				silence_count++;

				if(config->debugSync)
					logmsgFileOnly("Closed a silence cycle %d\n", silence_count);
				if(silence_count > getPulseCount(role, config))
				{
					if(config->debugSync)
						logmsgFileOnly("Resets the sequence\n");
					sequence_start = 0;
					silence_count = 0;
				}
			}

			if(frame_silence_count)
				frame_silence_count = 0;
		}
		else
		{
			frame_silence_count ++;

			if(config->debugSync)
				logmsgFileOnly("[i:%ld] byte:%7ld [%5gHz %0.2f dBFS] Silence Frame counted %d\n", 
					i, pulseArray[i].bytes, pulseArray[i].hertz, pulseArray[i].amplitude, 
					frame_silence_count);

			if(frame_pulse_count >= getPulseFrameLen(role, config)*factor)
			{
				pulse_count++;

				if(config->debugSync)
					logmsgFileOnly("Closed pulse #%d cycle, silence count %d pulse count %d\n", pulse_count, silence_count, frame_pulse_count);
				
				if(config->syncTolerance)
					silence_count = pulse_count - 1;

				if(pulse_count == getPulseCount(role, config) /* && silence_count == pulse_count - 1 */)
				{
					if(config->debugSync)
						logmsgFileOnly("Completed the sequence %ld\n", sequence_start);
					return sequence_start;
				}
			}

			if(frame_pulse_count > 0)
			{
				if(!pulse_count && sequence_start)
				{
					if(config->debugSync)
						logmsgFileOnly("Resets the sequence (no pulse count)\n");
					sequence_start = 0;
				}

				frame_pulse_count = 0;
			}
		}
	}

	if(config->debugSync)
		logmsgFileOnly("Failed\n");
	*maxdetected = pulse_count;
	return -1;
}

long int DetectPulseInternal(char *Samples, wav_hdr header, int factor, long int offset, int *maxdetected, int role, int AudioChannels, parameters *config)
{
	long int			i = 0, TotalMS = 0;
	long int			loadedBlockSize = 0;
	char				*buffer;
	size_t			 	buffersize = 0;
	long int			pos = 0, millisecondSize = 0, startPos = 0;
	Pulses				*pulseArray;
	double				targetFrequency = 0, targetFrequencyHarmonic = 0, MaxMagnitude = 0;

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
			msLen *= 1.5;  /* widen so that the silence offset is compensated for */
		else
			msLen *= 1.1;  /* widen so that the silence offset is compensated for */
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

	targetFrequency = FindFrequencyBracket(GetPulseSyncFreq(role, config), 	
						millisecondSize/2, AudioChannels, header.fmt.SamplesPerSec, config);
	targetFrequencyHarmonic = FindFrequencyBracket(GetPulseSyncFreq(role, config)*2, 	
						millisecondSize/2, AudioChannels, header.fmt.SamplesPerSec, config);
	if(config->debugSync)
	{
		logmsgFileOnly("Defined Sync %d Adjusted to %g/%g\n", 
				GetPulseSyncFreq(role, config), targetFrequency, targetFrequencyHarmonic);
		logmsgFileOnly("Start ms %ld Total MS: %ld (%ld)\n",
			 i, TotalMS, header.data.DataSize / buffersize - 1);
	}

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

		/* We use left channel by default, we don't know about channel imbalances yet */
		ProcessChunkForSyncPulse((int16_t*)buffer, loadedBlockSize/2, 
			header.fmt.SamplesPerSec, &pulseArray[i], 
			config->channel == 's' ? 'l' : config->channel, AudioChannels, config);

		if(pulseArray[i].magnitude > MaxMagnitude)
			MaxMagnitude = pulseArray[i].magnitude;
		i++;
	}


	for(i = startPos; i < TotalMS; i++)
	{
		if(pulseArray[i].hertz)  /* this can be zero if samples were zeroed */
			pulseArray[i].amplitude = CalculateAmplitude(pulseArray[i].magnitude, MaxMagnitude, config);
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

#ifdef FREQ_DETECT
	offset = DetectPulseTrainSequenceFreqOnly(pulseArray, targetFrequency, targetFrequencyHarmonic, TotalMS, factor, maxdetected, startPos, role, config);
#else
	offset = DetectPulseTrainSequence(pulseArray, targetFrequency, targetFrequencyHarmonic, TotalMS, factor, maxdetected, startPos, role, config);
#endif

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
	double			maxHertz = 0, maxMag = 0;

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

	if(AudioChannels == 1)
		channel = 'l';

	for(i = 0; i < monoSignalSize; i++)
	{
		if(channel == 'l')
			signal[i] = (double)samples[i*AudioChannels];
		if(channel == 'r')
			signal[i] = (double)samples[i*2+1];
		if(channel == 's')
			signal[i] = ((double)samples[i*2]+(double)samples[i*2+1])/2.0;
	}

	fftw_execute(p); 
	fftw_destroy_plan(p);
	p = NULL;

	for(i = 1; i < monoSignalSize/2+1; i++)
	{
		double magnitude;
		double Hertz;

		magnitude = CalculateMagnitude(spectrum[i], size);
		Hertz = CalculateFrequency(i, boxsize, config);

		if(magnitude > maxMag)
		{
			maxMag = magnitude;
			maxHertz = Hertz;
		}
	}

	fftw_free(spectrum);
	spectrum = NULL;

	free(signal);
	signal = NULL;

	pulse->hertz = maxHertz;
	pulse->magnitude = maxMag;

	return(maxHertz);
}

long int DetectSignalStart(char *AllSamples, wav_hdr header, long int offset, int syncKnow, long int *endPulse, parameters *config)
{
	int			maxdetected = 0, AudioChannels = 0;
	long int	position = 0;

	if(config->debugSync)
		logmsgFileOnly("\nStarting Detect Signal\n");

	AudioChannels = header.fmt.NumOfChan;
	position = DetectSignalStartInternal(AllSamples, header, FACTOR_DETECT, offset, syncKnow, &maxdetected, endPulse, AudioChannels, config);
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

long int DetectSignalStartInternal(char *Samples, wav_hdr header, int factor, long int offset, int syncKnown, int *maxdetected, long int *endPulse, int AudioChannels, parameters *config)
{
	long int			i = 0, TotalMS = 0, start = 0;
	long int			loadedBlockSize = 0;
	char				*buffer;
	size_t			 	buffersize = 0;
	long int			pos = 0, millisecondSize = 0;
	double				MaxMagnitude = 0;
	Pulses				*pulseArray;
	double 				total = 0;
	long int 			count = 0, length = 0;
	double 				targetFrequency = 0, targetFrequencyHarmonic = 0, averageAmplitude = 0;

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

		/* we go stereo, any signal is fine here */
		ProcessChunkForSyncPulse((int16_t*)buffer, loadedBlockSize/2, 
				header.fmt.SamplesPerSec, &pulseArray[i], 
				's', AudioChannels, config);
		if(pulseArray[i].magnitude > MaxMagnitude)
			MaxMagnitude = pulseArray[i].magnitude;
		i++;
	}

	for(i = start; i < TotalMS; i++)
	{
		if(pulseArray[i].hertz)  /* we can get this empty due to zeroes in samples */
			pulseArray[i].amplitude = CalculateAmplitude(pulseArray[i].magnitude, MaxMagnitude, config);
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
		targetFrequency = FindFrequencyBracket(syncKnown, 
					millisecondSize/2, AudioChannels, header.fmt.SamplesPerSec, config);
		targetFrequencyHarmonic = FindFrequencyBracket(syncKnown*2, 
					millisecondSize/2, AudioChannels, header.fmt.SamplesPerSec, config);
		averageAmplitude = findAverageAmplitudeForTarget(pulseArray, targetFrequency, TotalMS, start, config);
	}

	for(i = start; i < TotalMS; i++)
	{
		if(pulseArray[i].hertz)
		{
/*
			if(config->debugSync)
				logmsgFileOnly("B: %ld Hz: %g A: %g avg: %g\n", 
					pulseArray[i].bytes, 
					pulseArray[i].hertz, 
					pulseArray[i].amplitude, 
					average);
*/
			if(syncKnown)
			{
				if(pulseArray[i].amplitude > averageAmplitude &&
					(pulseArray[i].hertz == targetFrequency ||
					pulseArray[i].hertz == targetFrequencyHarmonic))  // harmonic
				{
					if(offset == -1)
						offset = pulseArray[i].bytes;
					length++;
				}
				else
				{
					if(offset != -1 && length > 4)
					{
						if(endPulse)
							*endPulse = pulseArray[i].bytes;
						break;
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
				double average = 0;

				total += pulseArray[i].amplitude;
				count ++;
				average = total/count;
		
				if(pulseArray[i].amplitude * 1.5 > average)
				{
					offset = pulseArray[i].bytes;
					break;
				}
			}
		}
	}

	free(pulseArray);
	free(buffer);

	return offset;
}
