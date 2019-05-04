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

long int DetectPulse(char *AllSamples, wav_hdr header, int factor, int offset, parameters *config)
{
	long int			i = 0, TotalMS = 0;
	long int			loadedBlockSize = 0;
	char				*buffer;
	size_t			 	buffersize = 0;
	//double				seconds = 0;
	long int			pos = 0, millisecondSize = 0, missing = 0;
	Pulses				*pulseArray;
	//double				hertzArray[10] = { 0, 5011.36, 6013.64, 5880, 4009.09, 4900, 5512.5, 6300, 7350, 8820 };
	double				hertzArray[10] = { 0, 8018.18, 8018.18, 8820, 8018.18, 9800, 5512.5, 6300, 7350, 8820 };
 	long int			currCnt = 0, pulseCnt = 0, lastpos = 0, startpos = 0, MaxMagnitude = 0;
	long int 			fstartpos = 0, silence = 0, lastSilence = 0;

	pos = offset;

	//seconds = (double)header.Subchunk2Size/4.0/(double)header.SamplesPerSec;

	// Not a real ms, just approximate
	millisecondSize = (((double)header.SamplesPerSec*4.0)/1000.0)/(double)factor; /* 2 bytes per sample, stereo */
	missing = millisecondSize % 4;
	if(missing != 0)
		millisecondSize += 4 - missing;
	buffersize = millisecondSize*sizeof(char); 
	buffer = (char*)malloc(buffersize);
	if(!buffer)
	{
		logmsg("\tmalloc failed\n");
		return(0);
	}
	
	// Adjust for a real MS
	//MSfactor = ((double)factor*1000.0)/((double)header.SamplesPerSec/(double)millisecondSize*4.0);

	//TotalMS = seconds*1000*factor;  // Compleate File
	TotalMS = GetTotalBlockDuration(config)*1000*factor/4;  // /4 to search only the 4th part of the file for the start
	pulseArray = (Pulses*)malloc(sizeof(Pulses)*TotalMS);
	if(!pulseArray)
	{
		logmsg("\tPulse malloc failed!\n");
		return(0);
	}
	memset(pulseArray, 0, sizeof(Pulses)*TotalMS);

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
		memcpy(buffer, AllSamples + pos, loadedBlockSize);

#ifdef SAVE_CHUNKS
		if(0)
		{
			FILE 		*chunk = NULL;
			wav_hdr		cheader;
			char		FName[4096];

			cheader = header;
			sprintf(FName, "%s_%06ld_Source_chunk.wav", fileName, i);
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

		ProcessChunkForSyncPulse((short*)buffer, loadedBlockSize/2, header.SamplesPerSec, &pulseArray[i]);
		if(pulseArray[i].magnitude > MaxMagnitude)
			MaxMagnitude = pulseArray[i].magnitude;
		i++;
	}

	for(i = 0; i < TotalMS; i++)
		pulseArray[i].amplitude = RoundFloat(20*log10(pulseArray[i].magnitude/MaxMagnitude), 2);

	/*
	for(i = 0; i < TotalMS; i++)
	{
		if(pulseArray[i].amplitude >= -30 && pulseArray[i].hertz >= hertzArray[factor] - 2.0 && pulseArray[i].hertz <= hertzArray[factor] + 2.0)
			printf("B: %ld Hz: %g A: %g\n", 
				pulseArray[i].bytes, 
				pulseArray[i].hertz, 
				pulseArray[i].amplitude);
	}
	*/

	for(i = 0; i < TotalMS; i++)
	{
		if(pulseArray[i].amplitude >= -30 && pulseArray[i].hertz >= hertzArray[factor] - 2.0 && pulseArray[i].hertz <= hertzArray[factor] + 2.0) 
		{
			// A match, either the first or in a sequence
			if(currCnt == 0)
				startpos = pulseArray[i].bytes;
			currCnt++;
			if(silence)
				lastSilence = silence;
			silence = 0;
		}
		else
		{
			// out of sequence

			// are we closing the sequence? was it valid?
			// we are expecting 16-17 frames each
			if(lastSilence && currCnt >= 14*factor && currCnt <= 17*factor) 
			{
				if(lastpos == 0)
					fstartpos = startpos;
				pulseCnt++;

				if(lastpos && (lastSilence < 14*factor  || lastSilence > 17*factor))
					pulseCnt = 0;

				printf("(Lastpos %ld) Startpos %ld Len %ld Sil: %ld\n", 
					lastpos, 
					startpos, 
					pulseArray[i].bytes - startpos, 
					lastSilence);

				// Found
				if(pulseCnt == 10)
				{
					free(pulseArray);
					free(buffer);
				
					return fstartpos;
				}
				
				lastpos = startpos;
				lastSilence = 0;
			}
			currCnt = 0;
		}

		if(pulseArray[i].amplitude <= -30)
			silence ++;
	}

	free(pulseArray);
	free(buffer);

	return 0;
}


double ProcessChunkForSyncPulse(short *samples, size_t size, long samplerate, Pulses *pulse)
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
		/*
		if(config->channel == 'l')
			signal[i] = (double)samples[i*2];
		if(config->channel == 'r')
			signal[i] = (double)samples[i*2+1];
		if(config->channel == 's')
		*/
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

