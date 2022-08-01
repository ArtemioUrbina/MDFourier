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
#include "balance.h"
#include "freq.h"
#include "windows.h"
#include "log.h"
#include "cline.h"
#include "profile.h"

int CheckBalance(AudioSignal *Signal, int block, parameters *config)
{
	long int		pos = 0;
	double			longest = 0;
	double			*buffer;
	long int		buffersize = 0;
	windowManager	windows;
	double			*windowUsed = NULL;
	long int		loadedBlockSize = 0, i = 0, matchIndex = 0;
	struct timespec	start, end;
	int				discardBytes = 0;
	double			leftDecimals = 0, MaxMagLeft = 0, MaxMagRight = 0;
	AudioBlocks		Channels[2];

	if(Signal->AudioChannels != 2)
	{
		logmsg(" - %s signal is mono\n", getRoleText(Signal));
		if(config->allowStereoVsMono)
			return -1;
		else
		{
			if(config->usesStereo)
			{
				logmsg("ERROR: Stereo vs Mono not allowed by profile\n");
				return 0;
			}
			else  // All fine
				return -1;
		}
	}

	memset(&Channels, 0, sizeof(AudioBlocks)*2);

	pos = Signal->startOffset;

	longest = FramesToSeconds(Signal->framerate, GetLongestElementFrames(config));
	if(!longest)
	{
		logmsg("Block definitions are invalid, total length is 0.\n");
		return 0;
	}

	buffersize = SecondsToSamples(Signal->SampleRate, longest, Signal->AudioChannels, NULL, NULL);
	buffer = (double*)malloc(sizeof(double)*buffersize);
	if(!buffer)
	{
		logmsg("\tmalloc failed\n");
		return(0);
	}

	// Use flattop for Amplitude accuracy
	if(!initWindows(&windows, Signal->SampleRate, 'f', config))
		return 0;

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	while(i <= block)
	{
		double duration = 0;
		long int frames = 0, difference = 0, cutFrames = 0;

		frames = GetBlockFrames(config, i);
		cutFrames = GetBlockCutFrames(config, i);
		duration = FramesToSeconds(Signal->framerate, frames);
		
		loadedBlockSize = SecondsToSamples(Signal->SampleRate, duration, Signal->AudioChannels, &discardBytes, &leftDecimals);

		difference = GetSampleSizeDifferenceByFrameRate(Signal->framerate, frames, Signal->SampleRate, Signal->AudioChannels, config);

		windowUsed = getWindowByLength(&windows, frames, cutFrames, config->smallerFramerate, config);

		if(i == block)
		{
			Channels[0].index = GetBlockSubIndex(config, i);
			Channels[0].type = GetBlockType(config, i);
			Channels[0].seconds = 0;

			Channels[1].index = Channels[0].index;
			Channels[1].type = Channels[0].type;
			Channels[1].seconds = 0;

			memset(buffer, 0, buffersize);
			if((uint32_t)(pos + loadedBlockSize) > Signal->header.data.DataSize)
			{
				logmsg("\tunexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
				break;
			}
			
			memcpy(buffer, Signal->Samples + pos, sizeof(double)*(loadedBlockSize-difference));
	
			if(!ExecuteBalanceDFFT(&Channels[0], buffer, (loadedBlockSize-difference), Signal->SampleRate, windowUsed, CHANNEL_LEFT, config))
				return 0;

			if(!ExecuteBalanceDFFT(&Channels[1], buffer, (loadedBlockSize-difference), Signal->SampleRate, windowUsed, CHANNEL_RIGHT, config))
				return 0;

			Channels[0].freq = (Frequency*)malloc(sizeof(Frequency)*config->MaxFreq);
			if(!Channels[0].freq)
			{
				logmsg("ERROR: Not enough memory for Data Structures\n");
				return 0;
			}
			memset(Channels[0].freq, 0, sizeof(Frequency)*config->MaxFreq);

			Channels[1].freq = (Frequency*)malloc(sizeof(Frequency)*config->MaxFreq);
			if(!Channels[1].freq)
			{
				ReleaseBlock(&Channels[0]);
				logmsg("ERROR: Not enough memory for Data Structures\n");
				return 0;
			}
			memset(Channels[1].freq, 0, sizeof(Frequency)*config->MaxFreq);
			if(!FillFrequencyStructures(Signal, &Channels[0], config))
			{
				ReleaseBlock(&Channels[0]);
				ReleaseBlock(&Channels[1]);
		
				logmsg("- Could not detect Stereo channel balance.\n");
				return 0;
			}
			if(!FillFrequencyStructures(Signal, &Channels[1], config))
			{
				ReleaseBlock(&Channels[0]);
				ReleaseBlock(&Channels[1]);
		
				logmsg("- Could not detect Stereo channel balance.\n");
				return 0;
			}
		}

		pos += loadedBlockSize;
		pos += discardBytes;
		i++;
	}

	if(!Channels[0].freq || !Channels[1].freq)
	{
		ReleaseBlock(&Channels[0]);
		ReleaseBlock(&Channels[1]);

		logmsg("- Could not detect Stereo channel balance.\n");
		return 0;
	}

	if(!areDoublesEqual(Channels[0].freq[0].hertz, Channels[1].freq[matchIndex].hertz))
		matchIndex = 1; // Allow one bin difference

	if(!areDoublesEqual(Channels[0].freq[0].hertz, Channels[1].freq[matchIndex].hertz))
	{
		logmsg("\nERROR: Channel balance block has different frequency content. (use -B to ignore)\n");
		logmsg("\tNot a MONO signal for balance check. %s# %d (%d) at [%g Hz/%g] vs [%g Hz/%g]\n",
					GetBlockName(config, block), GetBlockSubIndex(config, block), block, 
					Channels[0].freq[0].hertz, Channels[0].freq[0].magnitude,
					Channels[1].freq[0].hertz, Channels[1].freq[0].magnitude);

		if(config->verbose)
		{
			logmsgFileOnly("Left Channel:\n");
			PrintFrequenciesBlockMagnitude(NULL, Channels[0].freq, config);
			logmsgFileOnly("Right Channel:\n");
			PrintFrequenciesBlockMagnitude(NULL, Channels[1].freq, config);
		}
		ReleaseBlock(&Channels[0]);
		ReleaseBlock(&Channels[1]);

		config->noBalance |= Signal->role;

		return 0;
	}

	for(i = 0; i < config->MaxFreq; i++)
	{
		if(!Channels[0].freq[i].hertz && Channels[1].freq[i].hertz)
			break;

		if(Channels[0].freq[i].hertz && Channels[0].freq[i].magnitude > MaxMagLeft)
			MaxMagLeft = Channels[0].freq[i].magnitude;

		if(Channels[1].freq[i].hertz && Channels[1].freq[i].magnitude > MaxMagRight)
			MaxMagRight = Channels[1].freq[i].magnitude;
	}

	if(!areDoublesEqual(Channels[0].freq[0].magnitude, Channels[1].freq[matchIndex].magnitude))
	{
		double 	ratio = 0;
		double	amplLeft = 0, amplRight = 0, amplDiff = 0;
		char	diffNam = '\0';

		if(Channels[0].freq[0].magnitude > Channels[1].freq[matchIndex].magnitude)
		{
			diffNam = CHANNEL_LEFT;
			ratio = Channels[1].freq[matchIndex].magnitude/Channels[0].freq[0].magnitude;

			amplLeft = CalculateAmplitude(Channels[0].freq[0].magnitude, MaxMagLeft);
			amplRight = CalculateAmplitude(Channels[1].freq[0].magnitude, MaxMagLeft);
			amplDiff = amplLeft - amplRight;
		}
		else
		{
			diffNam = CHANNEL_RIGHT;
			ratio = Channels[0].freq[0].magnitude/Channels[1].freq[matchIndex].magnitude;

			amplLeft = CalculateAmplitude(Channels[0].freq[0].magnitude, MaxMagRight);
			amplRight = CalculateAmplitude(Channels[1].freq[0].magnitude, MaxMagRight);
			amplDiff = amplRight - amplLeft;
		}

		//logmsg("Amplitudes: Left %gdBFS Right %gdBFS, diff: %gdBFS\n", amplLeft, amplRight, amplDiff);
		
		if(fabs(amplDiff) >= 0.0001)
		{
			logmsg(" - %s signal stereo imbalance: %s channel is higher by %g dBFS",
					getRoleText(Signal),
					diffNam == CHANNEL_LEFT ? "left" : "right", amplDiff);
		}
		else
		{
			logmsg(" - %s signal stereo imbalance: %s channel is higher by less than 0.0001 dBFS",
					getRoleText(Signal),
					diffNam == CHANNEL_LEFT ? "left" : "right");
		}

		if(config->verbose)
		{
			double	percent_higher;

			percent_higher = 100.0 * (pow(10, amplDiff / 20)-1.0);
			logmsg(" (%0.5f%%)",
				percent_higher);
		}
		logmsg("\n");

		Signal->balance = amplDiff;
		if(diffNam == CHANNEL_LEFT)
			Signal->balance *= -1;
		
		if(config->channelBalance)
			BalanceAudioChannel(Signal, diffNam, ratio);
	}
	else
		logmsg(" - %s signal has no stereo imbalance\n",
			getRoleText(Signal));

	if(config->clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - clk: Audio Channel Balancing took %0.2fs\n", elapsedSeconds);
	}

	ReleaseBlock(&Channels[0]);
	ReleaseBlock(&Channels[1]);

	free(buffer);
	freeWindows(&windows);

	return 1;
}

int ExecuteBalanceDFFT(AudioBlocks *AudioArray, double *samples, size_t size, double samplerate, double *window, char channel, parameters *config)
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
	monoSignalSize = stereoSignalSize/2;
	seconds = (double)size/(samplerate*2);

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
		if(channel == CHANNEL_LEFT)
			signal[i] = (double)samples[i*2];
		if(channel == CHANNEL_RIGHT)
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
	AudioArray->seconds = seconds;

	return(1);
}

void BalanceAudioChannel(AudioSignal *Signal, char channel, double ratio)
{
	long int 	i = 0, start = 0, end = 0;
	double		*samples = NULL;

	if(!Signal)
		return;

	if(!Signal->Samples)
		return;

	samples = Signal->Samples;
	start = Signal->startOffset;
	end = Signal->endOffset;

	for(i = start; i < end; i+=2)
	{
		if(channel == CHANNEL_LEFT)
			samples[i] = samples[i]*ratio;
		if(channel == CHANNEL_RIGHT)
			samples[i+1] = samples[i+1]*ratio;
	}
}
