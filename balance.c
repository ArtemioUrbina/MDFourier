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

#include "mdfourier.h"
#include "balance.h"
#include "freq.h"
#include "windows.h"
#include "log.h"
#include "cline.h"

int CheckBalance(AudioSignal *Signal, int block, parameters *config)
{
	long int		pos = 0;
	double			longest = 0;
	char			*buffer;
	size_t			buffersize = 0;
	windowManager	windows;
	double			*windowUsed = NULL;
	long int		loadedBlockSize = 0, i = 0, matchIndex = 0;
	struct timespec	start, end;
	int				leftover = 0, discardBytes = 0;
	double			leftDecimals = 0, diff = 0;
	AudioBlocks		Channels[2];

	if(Signal->AudioChannels != 2)
	{
		logmsg(" - %s signal is mono\n",
			Signal->role == ROLE_REF ? "Reference" : "Comparison");
		return 0;
	}

	memset(&Channels, 0, sizeof(AudioBlocks)*2);

	pos = Signal->startOffset;

	longest = FramesToSeconds(Signal->framerate, GetLongestElementFrames(config));
	if(!longest)
	{
		logmsg("Block definitions are invalid, total length is 0.\n");
		return 0;
	}

	buffersize = SecondsToBytes(Signal->header.fmt.SamplesPerSec, longest, Signal->AudioChannels, NULL, NULL, NULL);
	buffer = (char*)malloc(buffersize);
	if(!buffer)
	{
		logmsg("\tmalloc failed\n");
		return(0);
	}

	// Use flattop for Amplitude accuracy
	if(!initWindows(&windows, Signal->header.fmt.SamplesPerSec, 'f', config))
		return 0;

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	while(i <= block)
	{
		double duration = 0;
		long int frames = 0, difference = 0;

		frames = GetBlockFrames(config, i);
		duration = FramesToSeconds(Signal->framerate, frames);
		
		loadedBlockSize = SecondsToBytes(Signal->header.fmt.SamplesPerSec, duration, Signal->AudioChannels, &leftover, &discardBytes, &leftDecimals);

		difference = GetByteSizeDifferenceByFrameRate(Signal->framerate, frames, Signal->header.fmt.SamplesPerSec, Signal->AudioChannels, config);

		windowUsed = getWindowByLength(&windows, frames, config->smallerFramerate);

		if(i == block)
		{
			Channels[0].index = GetBlockSubIndex(config, i);
			Channels[0].type = GetBlockType(config, i);

			Channels[1].index = Channels[0].index;
			Channels[1].type = Channels[0].type;

			memset(buffer, 0, buffersize);
			if(pos + loadedBlockSize > Signal->header.data.DataSize)
			{
				logmsg("\tunexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
				break;
			}
			
			memcpy(buffer, Signal->Samples + pos, loadedBlockSize-difference);
	
			if(!ExecuteBalanceDFFT(&Channels[0], (int16_t*)buffer, (loadedBlockSize-difference)/2, Signal->header.fmt.SamplesPerSec, windowUsed, 'l', config))
				return 0;

			if(!ExecuteBalanceDFFT(&Channels[1], (int16_t*)buffer, (loadedBlockSize-difference)/2, Signal->header.fmt.SamplesPerSec, windowUsed, 'r', config))
				return 0;

			Channels[0].freq = (Frequency*)malloc(sizeof(Frequency)*config->MaxFreq);
			if(!Channels[0].freq)
			{
				logmsg("ERROR: Not enough memory for Data Structures\n");
				return 0;
			}
			Channels[1].freq = (Frequency*)malloc(sizeof(Frequency)*config->MaxFreq);
			if(!Channels[1].freq)
			{
				logmsg("ERROR: Not enough memory for Data Structures\n");
				return 0;
			}
			FillFrequencyStructures(Signal, &Channels[0], config);
			FillFrequencyStructures(Signal, &Channels[1], config);
		}

		pos += loadedBlockSize;
		pos += discardBytes;
		i++;
	}

	if(!Channels[0].freq || !Channels[1].freq)
	{
		if(Channels[0].freq)
			free(Channels[0].freq);
		if(Channels[1].freq)
			free(Channels[1].freq);

		logmsg("- Could not detect Stereo channel balance.\n");
		return 0;
	}

	if(Channels[0].freq[0].hertz != Channels[1].freq[matchIndex].hertz)
		matchIndex = 1;

	if(Channels[0].freq[0].hertz != Channels[1].freq[matchIndex].hertz)
	{
		logmsg("\nWARNING: Channel balance block has different frequency content.\n");
		logmsg("\tNot a MONO signal for balance check. [%s# %d (%d) at %g Hz / %g vs %g Hz / %g]\n",
					GetBlockName(config, block), GetBlockSubIndex(config, block), block, 
					Channels[0].freq[0].hertz, Channels[0].freq[0].magnitude,
					Channels[1].freq[0].hertz, Channels[1].freq[0].magnitude);

		if(config->verbose)
		{
			OutputFileOnlyStart();
			logmsg("Left Channel:\n");
			PrintFrequenciesBlockMagnitude(NULL, Channels[0].freq, GetBlockType(config, block), config);
			logmsg("Right Channel:\n");
			PrintFrequenciesBlockMagnitude(NULL, Channels[1].freq, GetBlockType(config, block), config);
			OutputFileOnlyEnd();
		}
		if(Channels[0].freq)
			free(Channels[0].freq);
		if(Channels[1].freq)
			free(Channels[1].freq);

		return 0;
	}

	diff = fabs(Channels[0].freq[0].magnitude - Channels[1].freq[matchIndex].magnitude);
	if(diff > 0.0)
	{
		double 	ratio = 0;
		double 	channDiff = 0;
		char	diffNam = '\0';

		if(Channels[0].freq[0].magnitude > Channels[1].freq[matchIndex].magnitude)
		{
			diffNam = 'l';
			channDiff = diff*100.0/Channels[0].freq[0].magnitude;
			ratio = Channels[1].freq[matchIndex].magnitude/Channels[0].freq[0].magnitude;
		}
		else
		{
			diffNam = 'r';
			channDiff = diff*100.0/Channels[1].freq[matchIndex].magnitude;
			ratio = Channels[0].freq[0].magnitude/Channels[1].freq[matchIndex].magnitude;
		}

		/*
		if(channDiff >= 1.0 || (channDiff > 0.0 && !config->channelBalance))
		{
			logmsg("\nWARNING for %s file\n", Signal->role == ROLE_REF ? "Reference" : "Comparison");
			logmsg("\tStereo imbalance: %s channel is higher by %g%%\n",
				diffNam == 'l' ? "left" : "right", channDiff);
			if(config->channelBalance)
			{
				logmsg("\tCompensating in software. [Use -B to disable auto-balance]\n");
				logmsg("\tThis can be caused by gain controls in the audio card and/or\n");
				logmsg("\tbe present in the system generating the audio signal.\n");
			}
			else
				logmsg("\tAudio not compensated.\n");
		}
		else
		{
			if(config->verbose)
				logmsg(" - %s signal stereo imbalance: %s channel is higher by %0.2g%%\n",
					Signal->role == ROLE_REF ? "Reference" : "Comparison",
					diffNam == 'l' ? "left" : "right", channDiff);
		}
		*/
		
		
		logmsg(" - %s signal stereo imbalance: %s channel is higher by %0.5f%%\n",
				Signal->role == ROLE_REF ? "Reference" : "Comparison",
				diffNam == 'l' ? "left" : "right", channDiff);
		
		if(config->channelBalance)
			BalanceAudioChannel(Signal, diffNam, ratio);
	}
	else
		logmsg(" - %s signal has no stereo imbalance\n",
			Signal->role == ROLE_REF ? "Reference" : "Comparison");

	if(config->clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - clk: Audio Channel Balancing took %0.2fs\n", elapsedSeconds);
	}

	if(Channels[0].freq)
		free(Channels[0].freq);
	if(Channels[1].freq)
		free(Channels[1].freq);

	free(buffer);
	freeWindows(&windows);

	return 1;
}

int ExecuteBalanceDFFT(AudioBlocks *AudioArray, int16_t *samples, size_t size, long samplerate, double *window,  char channel, parameters *config)
{
	fftw_plan		p = NULL;
	long		  	stereoSignalSize = 0;	
	long		  	i = 0, monoSignalSize = 0, zeropadding = 0;
	double		  	*signal = NULL;
	fftw_complex  	*spectrum = NULL;
	double		 	seconds = 0;
	
	if(!AudioArray)
	{
		logmsg("No Array for results\n");
		return 0;
	}

	stereoSignalSize = (long)size;
	monoSignalSize = stereoSignalSize/2;	 /* 4 is 2 16 bit values */
	seconds = (double)size/((double)samplerate*2);

	if(config->ZeroPad)  /* disabled by default */
		zeropadding = GetZeroPadValues(&monoSignalSize, &seconds, samplerate);

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

	if(!config->model_plan)
	{
		config->model_plan = fftw_plan_dft_r2c_1d(monoSignalSize, signal, spectrum, FFTW_MEASURE);
		if(!config->model_plan)
		{
			logmsg("FFTW failed to create FFTW_MEASURE plan\n");
			free(signal);
			signal = NULL;
			return 0;
		}
	}

	p = fftw_plan_dft_r2c_1d(monoSignalSize, signal, spectrum, FFTW_MEASURE);
	if(!p)
	{
		logmsg("FFTW failed to create FFTW_MEASURE plan\n");
		free(signal);
		signal = NULL;
		return 0;
	}

	memset(signal, 0, sizeof(double)*(monoSignalSize+1));
	memset(spectrum, 0, sizeof(fftw_complex)*(monoSignalSize/2+1));

	for(i = 0; i < monoSignalSize - zeropadding; i++)
	{
		if(channel == 'l')
			signal[i] = (double)samples[i*2];
		if(channel == 'r')
			signal[i] = (double)samples[i*2+1];

		if(window)
			signal[i] *= window[i];
	}

	fftw_execute(p); 
	fftw_destroy_plan(p);
	p = NULL;

	free(signal);
	signal = NULL;

	AudioArray->fftwValues.spectrum = spectrum;
	AudioArray->fftwValues.size = monoSignalSize;
	AudioArray->fftwValues.seconds = seconds;

	return(1);
}

void BalanceAudioChannel(AudioSignal *Signal, char channel, double ratio)
{
	long int 	i = 0, start = 0, end = 0;
	int16_t		*samples = NULL;

	if(!Signal)
		return;

	if(!Signal->Samples)
		return;

	samples = (int16_t*)Signal->Samples;
	start = Signal->startOffset/2;
	end = Signal->endOffset/2;

	for(i = start; i < end; i+=2)
	{
		if(channel == 'l')
			samples[i] = (int16_t)((double)samples[i])*ratio;
		if(channel == 'r')
			samples[i+1] = (int16_t)((double)samples[i+1])*ratio;
	}
}