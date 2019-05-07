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
 * 
 * Compile with: 
 *	  gcc -Wall -std=gnu99 -o mdfourier mdfourier.c -lfftw3 -lm
 */

#include "mdfourier.h"
#include "sync.h"
#include "log.h"
#include "freq.h"

long int DetectPulse(char *AllSamples, wav_hdr header, parameters *config)
{
	long int position = 0, offset = 0;

	position = DetectPulseInternal(AllSamples, header, 4, 0, config);
	if(position == -1)
		return -1;

	offset = position;
	if(offset >= 2*44)  // return 2 segments at ratio 4 above
		offset -= 2*44;
		
	position = DetectPulseInternal(AllSamples, header, 9, offset, config);
	return position;
}

long int DetectEndPulse(char *AllSamples, long int startpulse, wav_hdr header, parameters *config)
{
	long int 	position = 0, offset = 0;

	// Use defaults to calculate real frame rate
	offset = GetLastSilenceByteOffset(GetPlatformMSPerFrame(config), header, config) + startpulse;
	position = DetectPulseInternal(AllSamples, header, 4, offset, config);
	if(position == -1)
		return -1;

	offset = position;
	if(offset >= 2*44)  // return 2 segments at ratio 4 above
		offset -= 2*44;
		
	position = DetectPulseInternal(AllSamples, header, 9, offset, config);
	return position;
}
	
//#define DEBUG_SYNC

long int DetectPulseTrainSequence(Pulses *pulseArray, double targetFrequency, long int TotalMS, int factor)
{
	long int			inside_pulse = 0, inside_silence = 0;
	long int			pulse_start = 0, pulse_count = 0, 
						sequence_start = 0, last_pulse_start = 0, 
						last_pulse_pos = 0, last_silence_pos = 0, i = 0;
	double				pulse_volume = 0, pulse_amplitudes = 0;
	double				silence_volume = 0, silence_amplitudes = 0;

	for(i = 0; i < TotalMS; i++)
	{
		if(pulseArray[i].amplitude >= -25 && pulseArray[i].hertz >= targetFrequency - 2.0 && pulseArray[i].hertz <= targetFrequency + 2.0) 
		{
			if(!inside_pulse)
			{
#ifdef DEBUG_SYNC
				logmsg("PULSE Start %ld\n", pulseArray[i].bytes);
#endif
				pulse_start = pulseArray[i].bytes;

				pulse_amplitudes = 0;
				pulse_volume = 0;
				silence_amplitudes = 0;
				silence_volume = 0;
				last_pulse_start = 0;
				last_pulse_pos = 0;
			}

			if(!sequence_start)
				sequence_start = pulse_start;

			if(last_pulse_pos && i > last_pulse_pos + 2)
			{
#ifdef DEBUG_SYNC
				logmsg("pulse reset due to discontinuity %ld and %ld\n", i, last_pulse_pos);
#endif

				pulse_count = 0;
				sequence_start = 0;
				inside_silence = 0;
				inside_pulse = 0;
			}
			else
			{
#ifdef DEBUG_SYNC
				logmsg("PULSE Increment at %ld (%ld)\n", pulseArray[i].bytes, i);
#endif

				inside_pulse++;
				last_pulse_pos = i;
				pulse_amplitudes += pulseArray[i].amplitude;
			}

			// moved form 17 to 18 due to different lengths in
			// FPGA and emulator implementation
			if(inside_pulse >= 18*factor)
			{
#ifdef DEBUG_SYNC
				logmsg("reset pulse too long %ld\n", inside_pulse);
#endif

				pulse_count = 0;
				sequence_start = 0;
				inside_silence = 0;
				inside_pulse = 0;
			}
		}
		else
		{
			if(inside_pulse >= 14*factor)
			{
				if(last_silence_pos && i > last_silence_pos + 2)
				{
#ifdef DEBUG_SYNC
					logmsg("pulse silence due to discontinuity\n");
#endif
	
					pulse_count = 0;
					sequence_start = 0;
					inside_silence = 0;
					inside_pulse = 0;
				}
				else
				{
#ifdef DEBUG_SYNC
					logmsg("SILENCE Increment at %ld (%ld)\n", pulseArray[i].bytes, i);
#endif
					inside_silence++;	
					silence_amplitudes += pulseArray[i].amplitude;
				}

				if(pulse_start != last_pulse_start && inside_silence >= 14*factor)
				{
					pulse_volume = pulse_amplitudes/inside_pulse;
					silence_volume = silence_amplitudes/inside_silence;
					
					if(fabs(silence_volume) - fabs(pulse_volume) >= 30.0)
					{
						pulse_count++;
						last_pulse_start = pulse_start;
#ifdef DEBUG_SYNC
						logmsg("Pulse %ld Start: %ld Volume %g Length %ld Silence: %ld\n", 
							pulse_count, pulse_start, pulse_volume, inside_pulse, inside_silence);
#endif
						if(pulse_count == 10)
							return sequence_start;
					}
					else
					{
#ifdef DEBUG_SYNC
						logmsg("Reset Pulse No volume difference\n");
#endif

						pulse_count = 0;
						sequence_start = 0;
					}

					inside_silence = 0;
					inside_pulse = 0;
				}

				if(inside_silence >= 18*factor)
				{
#ifdef DEBUG_SYNC
					logmsg("Reset Pulse too much silence\n");
#endif

					pulse_count = 0;
					sequence_start = 0;
					inside_silence = 0;
					inside_pulse = 0;
				}
			}
			else
			{
				if(inside_pulse >= 18*factor || inside_silence >= 18*factor)
				{
#ifdef DEBUG_SYNC
					logmsg("Reset Pulse OB\n");
#endif

					pulse_count = 0;
					sequence_start = 0;
					inside_silence = 0;
					inside_pulse = 0;
				}
			}
		}
	}

	return -1;
}

long int DetectPulseInternal(char *Samples, wav_hdr header, int factor, long int offset, parameters *config)
{
	long int			i = 0, TotalMS = 0;
	long int			loadedBlockSize = 0;
	char				*buffer;
	size_t			 	buffersize = 0;
	long int			pos = 0, millisecondSize = 0;
	Pulses				*pulseArray;
	double				MaxMagnitude = 0, targetFrequency = 0;

	// Not a real ms, just approximate
	millisecondSize = RoundTo4bytes(floor((((double)header.SamplesPerSec*4.0)/1000.0)/(double)factor)); /* 2 bytes per sample, stereo */
	buffersize = millisecondSize*sizeof(char); 
	buffer = (char*)malloc(buffersize);
	if(!buffer)
	{
		logmsg("\tmalloc failed\n");
		return(0);
	}
	
	// Adjust for a real MS
	//MSfactor = ((double)factor*1000.0)/((double)header.SamplesPerSec/(double)millisecondSize*4.0);

	TotalMS = header.Subchunk2Size / buffersize - 1;
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

	while(i < TotalMS)
	{
		loadedBlockSize = millisecondSize;

		memset(buffer, 0, buffersize);
		if(pos + loadedBlockSize > header.Subchunk2Size)
		{
			logmsg("\tunexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
			break;
		}

		pulseArray[i].bytes = pos;
		memcpy(buffer, Samples + pos, loadedBlockSize);

#ifdef SAVE_CHUNKS
		if(1)
		{
			FILE 		*chunk = NULL;
			wav_hdr		cheader;
			char		FName[4096];

			cheader = header;
			sprintf(FName, "%06ld_Source_chunk.wav", i);
			chunk = fopen(FName, "wb");
			if(!chunk)
			{
				logmsg("\tCould not open chunk file %s\n", FName);
				return 0;
			}

			cheader.ChunkSize = loadedBlockSize+36;
			cheader.Subchunk2Size = loadedBlockSize;
			if(fwrite(&cheader, 1, sizeof(wav_hdr), chunk) != sizeof(wav_hdr))
			{
				logmsg("\tCould not write chunk header\n");
				return(0);
			}
		
			if(fwrite(buffer, 1, sizeof(char)*loadedBlockSize, chunk) != sizeof(char)*loadedBlockSize)
			{
				logmsg("\tCould not write samples to chunk file\n");
				return (0);
			}
		
			fclose(chunk);
		}
#endif

		pos += loadedBlockSize;

		ProcessChunkForSyncPulse((short*)buffer, loadedBlockSize/2, header.SamplesPerSec, &pulseArray[i], config);
		if(pulseArray[i].magnitude > MaxMagnitude)
			MaxMagnitude = pulseArray[i].magnitude;
		i++;
	}

	for(i = 0; i < TotalMS; i++)
	{
		if(pulseArray[i].hertz)
			pulseArray[i].amplitude = RoundFloat(20*log10(pulseArray[i].magnitude/MaxMagnitude), 2);
		else
			pulseArray[i].amplitude = -100;
	}

	/*
	logmsg("=====================\n");
	for(i = 0; i < TotalMS; i++)
	{
		if(pulseArray[i].amplitude >= -30 && pulseArray[i].hertz >= hertzArray[factor] - 2.0 && pulseArray[i].hertz <= hertzArray[factor] + 2.0)
			logmsg("B: %ld Hz: %g A: %g\n", 
				pulseArray[i].bytes, 
				pulseArray[i].hertz, 
				pulseArray[i].amplitude);
	}
	*/

	targetFrequency = FindFrequencyBracket(GetPulseSyncFreq(config), millisecondSize/2, header.SamplesPerSec);
	offset = DetectPulseTrainSequence(pulseArray, targetFrequency, TotalMS, factor);

	free(pulseArray);
	free(buffer);

	return offset;
}


double FindFrequencyBracket(int frequency, size_t size, long samplerate)
{
	double seconds = 0, minDiff = 0, targetFreq = 0;
	long int monoSignalSize;

	minDiff = samplerate/2;
	targetFreq = frequency;
	monoSignalSize = size/2;
	seconds = (double)size/((double)samplerate*2);

	for(int i = 1; i < monoSignalSize/2+1; i++)
	{
		double Hertz = 0, difference = 0;

		Hertz = ((double)i/seconds);
		difference = abs(Hertz - frequency);
		if(difference < minDiff)
		{
			targetFreq = Hertz;
			minDiff = difference;
		}
	}
	return targetFreq;
}


double ProcessChunkForSyncPulse(short *samples, size_t size, long samplerate, Pulses *pulse, parameters *config)
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

	p = fftw_plan_dft_r2c_1d(monoSignalSize, signal, spectrum, FFTW_MEASURE);

	memset(signal, 0, sizeof(double)*(monoSignalSize+1));
	memset(spectrum, 0, sizeof(fftw_complex)*(monoSignalSize/2+1));

	for(i = 0; i < monoSignalSize; i++)
	{
		if(config->channel == 'l')
			signal[i] = (double)samples[i*2];
		if(config->channel == 'r')
			signal[i] = (double)samples[i*2+1];
		if(config->channel == 's')
			signal[i] = ((double)samples[i*2]+(double)samples[i*2+1])/2.0;
	}

	fftw_execute(p); 
	fftw_destroy_plan(p);

	for(i = 1; i < monoSignalSize/2+1; i++)
	{
		double r1 = creal(spectrum[i]);
		double i1 = cimag(spectrum[i]);
		double magnitude;
		double Hertz;

		magnitude = sqrt(r1*r1 + i1*i1)/sqrt(size);
		Hertz = ((double)i/boxsize);

		if(magnitude > maxMag)
		{
			maxMag = magnitude;
			maxHertz = Hertz;
		}
	}

	fftw_free(spectrum);

	free(signal);
	signal = NULL;

	pulse->hertz = maxHertz;
	pulse->magnitude = maxMag;

	return(maxHertz);
}

