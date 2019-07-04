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

long int DetectPulse(char *AllSamples, wav_hdr header, double *MaxMagnitude, parameters *config)
{
	int			maxdetected = 0;
	long int	position = 0, offset = 0;

	OutputFileOnlyStart();

	if(config->debugSync)
		logmsg("\nStarting Detect start pulse\n");

	position = DetectPulseInternal(AllSamples, header, 4, 0, &maxdetected, MaxMagnitude, config);
	if(position == -1)
	{
		if(config->debugSync)
			logmsg("First round start pulse failed\n", offset);

		OutputFileOnlyEnd();
		return -1;
	}

	offset = position;
	if(offset >= 2*44)  // return 2 "byte segments" as dictated by ratio "4" above
		offset -= 2*44;

	if(config->debugSync)
		logmsg("First round start pulse detected at %ld, refinement\n", offset);

	position = DetectPulseInternal(AllSamples, header, 9, offset, &maxdetected, MaxMagnitude, config);
	if(config->debugSync)
		logmsg("Start pulse return value %ld\n", position);
	OutputFileOnlyEnd();
	return position;
}

long int DetectEndPulse(char *AllSamples, long int startpulse, wav_hdr header, double *MaxMagnitude, parameters *config)
{
	int			maxdetected = 0, frameAdjust = 0, tries = 0;
	long int 	position = 0, offset = 0;

	OutputFileOnlyStart();
	do
	{
		// Use defaults to calculate real frame rate
		offset = GetLastSilenceByteOffset(GetMSPerFrame(NULL, config), header, frameAdjust, config) + startpulse;
	
		frameAdjust = 0;
		if(config->debugSync)
			logmsg("Starting Detect end pulse with offset %ld\n", offset);
	
		position = DetectPulseInternal(AllSamples, header, 4, offset, &maxdetected, MaxMagnitude, config);
		if(position == -1 && !maxdetected)
		{
			if(config->debugSync)
				logmsg("First round end pulse failed, started search at %ld bytes\n", offset);
			OutputFileOnlyEnd();
			return -1;
		}

		if(position == -1 && maxdetected)
			frameAdjust = config->types.pulseCount - maxdetected/2;
		tries ++;
	}while(position == -1 && tries < 4);

	offset = position;
	if(offset >= 2*44)  // return 2 segments at ratio 4 above
		offset -= 2*44;
	
	if(config->debugSync)
		logmsg("First round end pulse detected at %ld, refinement\n", offset);

	position = DetectPulseInternal(AllSamples, header, 9, offset, &maxdetected, MaxMagnitude, config);
	if(config->debugSync)
		logmsg("End pulse return value %ld\n", position);
	OutputFileOnlyEnd();
	return position;
}

long int DetectPulseTrainSequence(Pulses *pulseArray, double targetFrequency, long int TotalMS, int factor, int *maxdetected, parameters *config)
{
	long int			inside_pulse = 0, inside_silence = 0;
	long int			pulse_start = 0, pulse_count = 0, 
						sequence_start = 0, last_pulse_start = 0, 
						last_pulse_pos = 0, last_silence_pos = 0, i = 0;
	double				pulse_amplitude = 0, pulse_amplitudes = 0;
	double				silence_amplitude = 0, silence_amplitudes = 0;

	*maxdetected = 0;
	for(i = 0; i < TotalMS; i++)
	{
		if(pulseArray[i].amplitude >= config->types.pulseMinVol 
			&& pulseArray[i].hertz == targetFrequency)
		{
			if(!inside_pulse)
			{
				if(config->debugSync)
					logmsg("PULSE Start %ld [%gHz %gdBFS]\n", 
							pulseArray[i].bytes, pulseArray[i].hertz, pulseArray[i].amplitude);

				pulse_start = pulseArray[i].bytes;

				pulse_amplitudes = 0;
				pulse_amplitude = 0;
				//last_pulse_start = 0;
				last_pulse_pos = 0;
			}

			if(!sequence_start)
				sequence_start = pulse_start;

			if(last_pulse_pos && i > last_pulse_pos + 2)
			{
				if(config->debugSync)
					logmsg("pulse reset due to discontinuity at %ld: %ld and %ld\n", 
							pulseArray[i].bytes, i, last_pulse_pos);

				pulse_count = 0;
				sequence_start = 0;
				inside_pulse = 0;
			}
			else
			{
				inside_pulse++;
				last_pulse_pos = i;

				if(config->debugSync)
					logmsg("PULSE Increment [%ld] at %ld (%ld)\n", 
						inside_pulse, pulseArray[i].bytes, i);

				pulse_amplitudes += pulseArray[i].amplitude;
			}

			// moved from 17 to 18 due to different lengths in
			// FPGA and emulator implementation
			if(inside_pulse >= config->types.pulseFrameMaxLen*factor)
			{
				if(config->debugSync)
					logmsg("reset pulse too long %ld at %ld\n", inside_pulse, pulseArray[i].bytes);

				pulse_count = 0;
				sequence_start = 0;
				inside_pulse = 0;
			}

			inside_silence = 0;
			last_silence_pos = 0;
			silence_amplitudes = 0;
			silence_amplitude = 0;
		}
		else  // Was not matched to the pulse values
		{
			//if(inside_pulse >= config->types.pulseFrameMinLen*factor)
			if(inside_pulse || inside_silence)
			{
				if(last_silence_pos && i > last_silence_pos + 2)
				{
					if(config->debugSync)
						logmsg("reset due to SILENCE discontinuity %ld: %ld and %ld\n", 
								pulseArray[i].bytes, i, last_silence_pos);
	
					pulse_count = 0;
					sequence_start = 0;
					inside_silence = 0;
					inside_pulse = 0;
					last_silence_pos = 0;
				}
				else
				{
					last_silence_pos = i;
					inside_silence++;

					if(config->debugSync)
						logmsg("SILENCE Increment [%ld] at %ld (%ld)\n", 
							inside_silence, pulseArray[i].bytes, i);

					silence_amplitudes += pulseArray[i].amplitude;
				}

				if(pulse_start != last_pulse_start && inside_silence >= config->types.pulseFrameMinLen*factor)
				{
					pulse_amplitude = pulse_amplitudes/inside_pulse;
					silence_amplitude = silence_amplitudes/inside_silence;
					
					if(floor(fabs(silence_amplitude)) - floor(fabs(pulse_amplitude)) >= config->types.pulseVolDiff)
					{
						pulse_count++;
						last_pulse_start = pulse_start;
						if(config->debugSync)
							logmsg("Pulse %ld Start: %ld Amplitude %g Length %ld Silence: %ld\n", 
								pulse_count, pulse_start, pulse_amplitude, inside_pulse, inside_silence);

						if(pulse_count == config->types.pulseCount)
							return sequence_start;
					}
					else
					{
						if(config->debugSync)
							logmsg("reset Pulse No amplitude difference S: %g P: %g Compare: %d at %ld\n",
									silence_amplitude, pulse_amplitude, config->types.pulseVolDiff, pulseArray[i].bytes);

						pulse_count = 0;
						sequence_start = 0;
					}

					//inside_silence = 0;
					inside_pulse = 0;
				}

				if(inside_silence >= config->types.pulseFrameMaxLen*factor)
				{
					if(config->debugSync)
						logmsg("reset Pulse too much silence at %ld\n", pulseArray[i].bytes);

					pulse_count = 0;
					sequence_start = 0;
					inside_silence = 0;
					inside_pulse = 0;
				}
			}
			/*
			else
			{
				if(inside_pulse >= config->types.pulseFrameMaxLen*factor || inside_silence >= config->types.pulseFrameMaxLen*factor)
				{
					if(config->debugSync)
						logmsg("reset Pulse OB at %ld\n", pulseArray[i].bytes);

					pulse_count = 0;
					sequence_start = 0;
					inside_silence = 0;
					inside_pulse = 0;
				}
			}
			*/
		}
	}

	*maxdetected = pulse_count;
	return -1;
}

long int DetectPulseInternal(char *Samples, wav_hdr header, int factor, long int offset, int *maxdetected, double *MaxMagnitude, parameters *config)
{
	long int			i = 0, TotalMS = 0;
	long int			loadedBlockSize = 0;
	char				*buffer;
	size_t			 	buffersize = 0;
	long int			pos = 0, millisecondSize = 0;
	Pulses				*pulseArray;
	double				targetFrequency = 0;

	// Not a real ms, just approximate
	millisecondSize = RoundTo4bytes(floor((((double)header.fmt.SamplesPerSec*4.0)/1000.0)/(double)factor), NULL, NULL, NULL);
	buffersize = millisecondSize*sizeof(char); 
	buffer = (char*)malloc(buffersize);
	if(!buffer)
	{
		logmsg("\tmalloc failed\n");
		return(0);
	}
	
	TotalMS = header.fmt.Subchunk2Size / buffersize - 1;
	pulseArray = (Pulses*)malloc(sizeof(Pulses)*TotalMS);
	if(!pulseArray)
	{
		logmsg("\tPulse malloc failed!\n");
		return(0);
	}
	memset(pulseArray, 0, sizeof(Pulses)*TotalMS);

	pos = offset;
	if(offset)
		i = offset/buffersize;
	else
		TotalMS /= 6;

	targetFrequency = FindFrequencyBracket(GetPulseSyncFreq(config), 	
						millisecondSize/2, header.fmt.SamplesPerSec);
	if(config->debugSync)
		logmsg("Defined Sync %g Adjusted to %g\n", GetPulseSyncFreq(config), targetFrequency);
	while(i < TotalMS)
	{
		loadedBlockSize = millisecondSize;

		memset(buffer, 0, buffersize);
		if(pos + loadedBlockSize > header.fmt.Subchunk2Size)
		{
			logmsg("\tunexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
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

		// We use left channel by default, we don't know about channel imbalances yet
		ProcessChunkForSyncPulse((int16_t*)buffer, loadedBlockSize/2, 
				header.fmt.SamplesPerSec, &pulseArray[i], 
				config->channel == 's' ? 'l' : config->channel, targetFrequency, config);
		if(pulseArray[i].magnitude > *MaxMagnitude)
			*MaxMagnitude = pulseArray[i].magnitude;
		i++;
	}

	/*
	if(config->debugSync)
		logmsg("MaxMagnitude: %g\n", *MaxMagnitude);
	*/

	for(i = 0; i < TotalMS; i++)
	{
		if(pulseArray[i].hertz)
			pulseArray[i].amplitude = CalculateAmplitude(pulseArray[i].magnitude, *MaxMagnitude);
		else
			pulseArray[i].amplitude = NO_AMPLITUDE;
	}

	/*
	if(config->debugSync)
	{
		logmsg("===== Searching for %gHz at %ddBFS =======\n", targetFrequency, config->types.pulseMinVol);
		for(i = 0; i < TotalMS; i++)
		{
			if(pulseArray[i].amplitude >= config->types.pulseMinVol && pulseArray[i].hertz == targetFrequency)
				logmsg("B: %ld Hz: %g A: %g\n", 
					pulseArray[i].bytes, 
					pulseArray[i].hertz, 
					pulseArray[i].amplitude);
		}
		logmsg("========  End listing =========\n");
	}
	*/

	offset = DetectPulseTrainSequence(pulseArray, targetFrequency, TotalMS, factor, maxdetected, config);

	free(pulseArray);
	free(buffer);

	return offset;
}

double ProcessChunkForSyncPulse(int16_t *samples, size_t size, long samplerate, Pulses *pulse, char channel, double target, parameters *config)
{
	fftw_plan		p = NULL;
	long		  	stereoSignalSize = 0;	
	long		  	i = 0, monoSignalSize = 0; 
	double		  	*signal = NULL;
	fftw_complex  	*spectrum = NULL;
	double		 	seconds = 0, boxsize = 0;
	double			maxHertz = 0, maxMag = 0;

	stereoSignalSize = (long)size;
	monoSignalSize = stereoSignalSize/2;	 // 4 is 2 16 bit values
	seconds = (double)size/((double)samplerate*2);
	boxsize = seconds;

	signal = (double*)malloc(sizeof(double)*(monoSignalSize+1));
	if(!signal)
	{
		logmsg("Not enough memory\n");
		return(0);
	}
	spectrum = (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*(monoSignalSize/2+1));
	if(!spectrum)
	{
		logmsg("Not enough memory\n");
		return(0);
	}

	if(!config->sync_plan)
	{
 		fftw_import_wisdom_from_filename("wisdom.fftw");

		config->sync_plan = fftw_plan_dft_r2c_1d(monoSignalSize, signal, spectrum, FFTW_MEASURE);
		if(!config->sync_plan)
		{
			logmsg("FFTW failed to create FFTW_MEASURE plan\n");
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
		logmsg("FFTW failed to create FFTW_MEASURE plan\n");

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
		if(channel == 'l')
			signal[i] = (double)samples[i*2];
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
		Hertz = CalculateFrequency(i, boxsize, config->ZeroPad);

		if(config->laxSync && target)
		{
			if(maxHertz != target && magnitude > maxMag)
			{
				maxMag = magnitude;
				maxHertz = Hertz;
			}
	
			if(Hertz == target)
			{
				maxMag = magnitude;
				maxHertz = Hertz;
				break;
			}
		}
		else
		{
			if(magnitude > maxMag)
			{
				maxMag = magnitude;
				maxHertz = Hertz;
			}
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

long int DetectSignalStart(char *AllSamples, wav_hdr header, long int offset, int syncKnow, parameters *config)
{
	int			maxdetected = 0;
	long int	position = 0;

	OutputFileOnlyStart();

	if(config->debugSync)
		logmsg("\nStarting Detect Signal\n");

	position = DetectSignalStartInternal(AllSamples, header, 9, offset, syncKnow, &maxdetected, config);
	if(position == -1)
	{
		if(config->debugSync)
			logmsg("Detect signal failed\n", offset);

		OutputFileOnlyEnd();
		return -1;
	}

	if(config->debugSync)
		logmsg("Detect signal return value %ld\n", position);
	OutputFileOnlyEnd();
	return position;
}

long int DetectSignalStartInternal(char *Samples, wav_hdr header, int factor, long int offset, int syncKnown, int *maxdetected, parameters *config)
{
	long int			i = 0, TotalMS = 0;
	long int			loadedBlockSize = 0;
	char				*buffer;
	size_t			 	buffersize = 0;
	long int			pos = 0, millisecondSize = 0;
	double				MaxMagnitude = 0;
	Pulses				*pulseArray;
	double 				total = 0;
	long int 			count = 0;
	double 				targetFrequency = 0;

	// Not a real ms, just approximate
	millisecondSize = RoundTo4bytes(floor((((double)header.fmt.SamplesPerSec*4.0)/1000.0)/(double)factor), NULL, NULL, NULL);
	buffersize = millisecondSize*sizeof(char); 
	buffer = (char*)malloc(buffersize);
	if(!buffer)
	{
		logmsg("\tmalloc failed\n");
		return(0);
	}
	
	TotalMS = header.fmt.Subchunk2Size / buffersize - 1;
	pulseArray = (Pulses*)malloc(sizeof(Pulses)*TotalMS);
	if(!pulseArray)
	{
		logmsg("\tPulse malloc failed!\n");
		return(0);
	}
	memset(pulseArray, 0, sizeof(Pulses)*TotalMS);

	pos = offset;
	if(offset)
		i = offset/buffersize;
	else
		TotalMS /= 6;

	while(i < TotalMS)
	{
		loadedBlockSize = millisecondSize;

		memset(buffer, 0, buffersize);
		if(pos + loadedBlockSize > header.fmt.Subchunk2Size)
		{
			logmsg("\tunexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
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

		// we go stereo, any signal is fine here
		ProcessChunkForSyncPulse((int16_t*)buffer, loadedBlockSize/2, 
				header.fmt.SamplesPerSec, &pulseArray[i], 
				's', 0, config);
		if(pulseArray[i].magnitude > MaxMagnitude)
			MaxMagnitude = pulseArray[i].magnitude;
		i++;
	}

	for(i = 0; i < TotalMS; i++)
	{
		if(pulseArray[i].hertz)
			pulseArray[i].amplitude = CalculateAmplitude(pulseArray[i].magnitude, MaxMagnitude);
		else
			pulseArray[i].amplitude = NO_AMPLITUDE;
	}

	/*
	if(config->debugSync)
	{
		//logmsg("===== Searching for %gHz at %ddBFS =======\n", targetFrequency, config->types.pulseMinVol);
		for(i = 0; i < TotalMS; i++)
		{
				logmsg("B: %ld Hz: %g A: %g\n", 
					pulseArray[i].bytes, 
					pulseArray[i].hertz, 
					pulseArray[i].amplitude);
		}
		//logmsg("========  End listing =========\n");
	}
	*/

	// if not found, compare all
	offset = 0;
	
	if(syncKnown)
		targetFrequency = FindFrequencyBracket(syncKnown, 
					millisecondSize/2, header.fmt.SamplesPerSec);
	for(i = 0; i < TotalMS; i++)
	{
		if(pulseArray[i].hertz)
		{
			double average = 0;

			total += pulseArray[i].amplitude;
			count ++;
			average = total/count;

/*
			if(config->debugSync)
				logmsg("B: %ld Hz: %g A: %g avg: %g\n", 
					pulseArray[i].bytes, 
					pulseArray[i].hertz, 
					pulseArray[i].amplitude, 
					average);
*/

			if(syncKnown)
			{
				if(pulseArray[i].amplitude > config->types.pulseMinVol &&
					pulseArray[i].hertz >= targetFrequency - 2.0 && pulseArray[i].hertz <= targetFrequency + 2.0)
				{
					offset = pulseArray[i].bytes;
					break;
				}
			}
			else
			{
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
