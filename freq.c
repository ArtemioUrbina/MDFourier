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

#include "freq.h"
#include "log.h"
#include "cline.h"
#include "plot.h"

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

		Hertz = CalculateFrequency(i, seconds, 0);
		difference = abs(Hertz - frequency);
		if(difference < minDiff)
		{
			targetFreq = Hertz;
			minDiff = difference;
		}
	}
	return targetFreq;
}


void CalcuateFrequencyBrackets(AudioSignal *Signal)
{
	if(!Signal)
		return;
	if(!Signal->Blocks)
		return;

	Signal->RefreshNoise = FindFrequencyBracket(RoundFloat(1000.0/Signal->framerate, 2), Signal->Blocks[0].fftwValues.size, Signal->header.SamplesPerSec);
	Signal->CRTLow = FindFrequencyBracket(15680, Signal->Blocks[0].fftwValues.size, Signal->header.SamplesPerSec);
	Signal->CRTHigh = FindFrequencyBracket(15710, Signal->Blocks[0].fftwValues.size, Signal->header.SamplesPerSec);
	//logmsg("Mains noise %g CRT Noise %g-%g\n", mains,  CRTLow, CRTHigh);
}

int IsCRTNoise(AudioSignal *Signal, double freq)
{
	if(!Signal)
		return 0;

	/* Peak around 15697-15698 usually */
	if(freq >= Signal->CRTLow && freq <= Signal->CRTHigh)
		return 1;
	return 0;
}

AudioSignal *CreateAudioSignal(parameters *config)
{
	AudioSignal *Signal = NULL;

	if(!config)
		return NULL;

	if(!config->types.totalChunks)
		return NULL;

	Signal = (AudioSignal*)malloc(sizeof(AudioSignal));
	if(!Signal)
	{
		logmsg("Not enough memory for Data Structures\n");
		return NULL;
	}

	Signal->Blocks = (AudioBlocks*)malloc(sizeof(AudioBlocks)*config->types.totalChunks);
	if(!Signal->Blocks)
	{
		logmsg("Not enough memory for Data Structures\n");	
		return NULL;
	}

	for(int n = 0; n < config->types.totalChunks; n++)
	{
		Signal->Blocks[n].freq = (Frequency*)malloc(sizeof(Frequency)*config->MaxFreq);
		if(!Signal->Blocks[n].freq)
		{
			logmsg("Not enough memory for Data Structures\n");
			return NULL;
		}
	}

	CleanAudio(Signal, config);
	return Signal;
}

void CleanAudio(AudioSignal *Signal, parameters *config)
{
	if(!Signal)
		return;

	if(!Signal->Blocks)
		return;

	for(int n = 0; n < config->types.totalChunks; n++)
	{
		if(Signal->Blocks[n].freq)
		{
			for(int i = 0; i < config->MaxFreq; i++)
			{
				Signal->Blocks[n].freq[i].hertz = 0;
				Signal->Blocks[n].freq[i].magnitude = 0;
				Signal->Blocks[n].freq[i].amplitude = NO_AMPLITUDE;
				Signal->Blocks[n].freq[i].phase = 0;
				Signal->Blocks[n].freq[i].matched = 0;
			}
		}
		Signal->Blocks[n].fftwValues.spectrum = NULL;
		Signal->Blocks[n].fftwValues.size = 0;
		Signal->Blocks[n].fftwValues.seconds = 0;
	}
	Signal->SourceFile[0] = '\0';

	Signal->hasFloor = 0;
	Signal->floorFreq = 0.0;
	Signal->floorAmplitude = 0.0;	

	Signal->Samples = NULL;
	Signal->framerate = 0.0;

	Signal->startOffset = 0;
	Signal->endOffset = 0;

	Signal->MaxMagnitude = 0;
	Signal->MinAmplitude = 0;

	Signal->RefreshNoise = 0;
	Signal->CRTLow = 0;
	Signal->CRTHigh = 0;

	memset(&Signal->header, 0, sizeof(wav_hdr));
}

void ReleaseAudio(AudioSignal *Signal, parameters *config)
{
	if(!Signal)
		return;

	if(!Signal->Blocks)
		return;

	for(int n = 0; n < config->types.totalChunks; n++)
	{
		if(Signal->Blocks[n].freq)
		{
			free(Signal->Blocks[n].freq);
			Signal->Blocks[n].freq = NULL;
		}

		if(Signal->Blocks[n].fftwValues.spectrum)
		{
			fftw_free(Signal->Blocks[n].fftwValues.spectrum);
			Signal->Blocks[n].fftwValues.spectrum = NULL;
		}
		Signal->Blocks[n].fftwValues.spectrum = NULL;
		Signal->Blocks[n].fftwValues.size = 0;
		Signal->Blocks[n].fftwValues.seconds = 0;
	}

	free(Signal->Blocks);
	Signal->Blocks = NULL;

	Signal->SourceFile[0] = '\0';

	Signal->hasFloor = 0;
	Signal->floorFreq = 0.0;
	Signal->floorAmplitude = 0.0;	

	if(Signal->Samples)
	{
		free(Signal->Samples);
		Signal->Samples = NULL;
	}
	Signal->framerate = 0.0;

	Signal->startOffset = 0;
	Signal->endOffset = 0;

	Signal->MaxMagnitude = 0;
	Signal->MinAmplitude = 0;
}

void ReleaseAudioBlockStructure(parameters *config)
{
	if(config->types.typeCount && config->types.typeArray)
	{
		free(config->types.typeArray);
		config->types.typeArray = NULL;
		config->types.typeCount = 0;
	}

	if(config->model_plan)
	{
		fftw_export_wisdom_to_filename("wisdom.fftw");

		fftw_destroy_plan(config->model_plan);
		config->model_plan = NULL;
	}
	if(config->reverse_plan)
	{
		fftw_destroy_plan(config->reverse_plan);
		config->reverse_plan = NULL;
	}
	if(config->sync_plan)
	{
		fftw_destroy_plan(config->sync_plan);
		config->sync_plan = NULL;
	}
}

int LoadAudioBlockStructure(parameters *config)
{
	FILE 	*file;
	char	lineBuffer[LINE_BUFFER_SIZE];
	char	buffer[PARAM_BUFFER_SIZE];

	if(!config)
		return 0;

	file = fopen("mdfblocks.mfn", "r");
	if(!file)
	{
		logmsg("Could not load audio configuration file mdfblocks.mfn\n");
		return 0;
	}
	
	readLine(lineBuffer, file);
	sscanf(lineBuffer, "%s ", buffer);
	if(strcmp(buffer, "MDFourierAudioBlockFile") != 0)
	{
		logmsg("Not an MD Fourier Audio Block File\n");
		fclose(file);
		return 0;
	}
	sscanf(lineBuffer, "%*s %s\n", buffer);
	if(atof(buffer) > 1.0)
	{
		logmsg("This executable can parse 1.0 files only\n");
		fclose(file);
		return 0;
	}

	readLine(lineBuffer, file);
	if(sscanf(lineBuffer, "%s\n", config->types.Name) != 1)
	{
		logmsg("Invalid Name '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}

	readLine(lineBuffer, file);
	if(sscanf(lineBuffer, "%s\n", buffer) != 1)
	{
		logmsg("Invalid Frame Rate Adjustment '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}
	config->types.platformMSPerFrame = strtod(buffer, NULL);
	if(!config->types.platformMSPerFrame)
	{
		logmsg("Invalid Frame Rate Adjustment '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}

	readLine(lineBuffer, file);
	if(sscanf(lineBuffer, "%d %d %d %d %d %d\n", 
		&config->types.pulseSyncFreq,
		&config->types.pulseMinVol,
		&config->types.pulseVolDiff,
		&config->types.pulseFrameMinLen,
		&config->types.pulseFrameMaxLen,
		&config->types.pulseCount) != 6)
	{
		logmsg("Invalid Pulse Parameters '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}
	
	if(!config->types.pulseSyncFreq)
	{
		logmsg("Invalid Pulse Sync Frequency:\n%s\n", lineBuffer);
		fclose(file);
		return 0;
	}

	if(!config->types.pulseMinVol)
	{
		logmsg("Invalid Pulse Minimum Volume:\n%s\n", lineBuffer);
		fclose(file);
		return 0;
	}

	if(!config->types.pulseVolDiff)
	{
		logmsg("Invalid Pulse Volume Difference:\n%s\n", lineBuffer);
		fclose(file);
		return 0;
	}

	if(!config->types.pulseFrameMinLen || !config->types.pulseFrameMaxLen ||
		config->types.pulseFrameMinLen >= config->types.pulseFrameMaxLen)
	{
		logmsg("Invalid Pulse Min Max Range:\n%s\n", lineBuffer);
		fclose(file);
		return 0;
	}

	if(!config->types.pulseCount)
	{
		logmsg("Invalid Pulse Count value:\n%s\n", lineBuffer);
		fclose(file);
		return 0;
	}

	readLine(lineBuffer, file);
	sscanf(lineBuffer, "%s\n", buffer);
	config->types.typeCount = atoi(buffer);
	if(!config->types.typeCount)
	{
		logmsg("Invalid type count '%s'\n", buffer);
		fclose(file);
		return 0;
	}
	config->types.typeArray = (AudioBlockType*)malloc(sizeof(AudioBlockType)*config->types.typeCount);
	if(!config->types.typeArray)
	{
		logmsg("Not enough memory\n");
		fclose(file);
		return 0;
	}

	for(int i = 0; i < config->types.typeCount; i++)
	{
		char type = 0;

		readLine(lineBuffer, file);
		if(sscanf(lineBuffer, "%s ", config->types.typeArray[i].typeName) != 1)
		{
			logmsg("Invalid Block Name %s\n", config->types.typeArray[i].typeName);
			fclose(file);
			return 0;
		}

		if(sscanf(lineBuffer, "%*s %c ", &type) != 1)
		{
			logmsg("Invalid Block Type %c\n", type);
			fclose(file);
			return 0;
		}

		switch(type)
		{
			case 'n':
				config->types.typeArray[i].type = TYPE_SILENCE;
				break;
			case 's':
				config->types.typeArray[i].type = TYPE_SYNC;
				break;
			default:
				if(sscanf(lineBuffer, "%*s %d ", &config->types.typeArray[i].type) != 1)
				{
					logmsg("Invalid MD Fourier Block ID\n", config->types.typeArray[i].type);
					fclose(file);
					return 0;
				}
				break;
		}
		
		if(sscanf(lineBuffer, "%*s %*c %d %d %s\n", 
			&config->types.typeArray[i].elementCount,
			&config->types.typeArray[i].frames,
			&config->types.typeArray[i].color [0]) != 3)
		{
			logmsg("Invalid MD Fourier Audio Blocks File (Element Count, frames, color): %s\n", lineBuffer);
			fclose(file);
			return 0;
		}

		if(!config->types.typeArray[i].elementCount)
		{
			logmsg("Element Count must have a value > 0\n");
			fclose(file);
			return 0;
		}

		if(!config->types.typeArray[i].frames)
		{
			logmsg("Frames must have a value > 0\n");
			fclose(file);
			return 0;
		}
	
		if(MatchColor(config->types.typeArray[i].color) == COLOR_NONE)
		{
			logmsg("Unrecognized color \"%s\" aborting\n", 
				config->types.typeArray[i].color);
			fclose(file);
			return 0;
		}
	}

	config->types.regularChunks = GetActiveAudioBlocks(config);
	config->types.totalChunks = GetTotalAudioBlocks(config);
	if(!config->types.totalChunks)
	{
		logmsg("Total Audio Blocks should be at least 1\n");
		return 0;
	}

	fclose(file);
	
	//PrintAudioBlocks(config);
	return 1;
}

void PrintAudioBlocks(parameters *config)
{
	if(!config)
		return;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		logmsg("%s %d %d %d %s\n", 
			config->types.typeArray[i].typeName,
			config->types.typeArray[i].type,
			config->types.typeArray[i].elementCount,
			config->types.typeArray[i].frames,
			config->types.typeArray[i].color);
	}
}

double GetPlatformMSPerFrame(parameters *config)
{
	return(RoundFloat(config->types.platformMSPerFrame, 3));
}

double GetPulseSyncFreq(parameters *config)
{
	return(config->types.pulseSyncFreq);
}

double GetLowerFrameRate(double framerateA, double framerateB)
{
	if(framerateA > framerateB)
		return framerateB;

	return framerateA;
}

void CompareFrameRates(double framerate1, double framerate2, parameters *config)
{
	double diff = 0;

	diff = fabs(framerate1 - framerate2);
	if(diff > 0.0)
	{
		config->smallerFramerate = 
				GetLowerFrameRate(framerate1, 
									framerate2);
		if(diff > 0.001)
		logmsg("\n= Different frame rates found (%g), compensating to %g =\n", 
				diff, config->smallerFramerate);
	}
}

long int GetByteSizeDifferenceByFrameRate(double framerate, long int frames, long int samplerate, parameters *config)
{
	long int difference = 0;

	if(config->smallerFramerate != 0 && framerate > config->smallerFramerate)
	{
		long int SmallerBytes = 0;
		long int BiggerBytes = 0;

		SmallerBytes = SecondsToBytes(samplerate, FramesToSeconds(config->smallerFramerate, frames), NULL, NULL, NULL);
		BiggerBytes = SecondsToBytes(samplerate, FramesToSeconds(framerate, frames), NULL, NULL, NULL);
	
		difference = BiggerBytes - SmallerBytes;
	}
	return difference;
}

double GetSignalTotalDuration(double framerate, parameters *config)
{
	long int frames = 0;

	frames = GetSignalTotalFrames(config);
	return(FramesToSeconds(frames, framerate));
}

int GetFirstSyncIndex(parameters *config)
{
	int index = 0;

	if(!config)
		return NO_INDEX;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type == TYPE_SYNC)
			return index;
		else
			index += config->types.typeArray[i].elementCount;
	}
	return NO_INDEX;
}

int GetLastSyncIndex(parameters *config)
{
	int first = 0;

	if(!config)
		return NO_INDEX;

	first = GetFirstSyncIndex(config);
	for(int i = config->types.typeCount - 1; i >= 0; i--)
	{
		if(config->types.typeArray[i].type == TYPE_SYNC && i != first)
			return i;
	}
	return NO_INDEX;
}

int GetFirstSilenceIndex(parameters *config)
{
	int index = 0;

	if(!config)
		return NO_INDEX;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type == TYPE_SILENCE)
			return index;
		else
			index += config->types.typeArray[i].elementCount;
	}
	return NO_INDEX;
}

long int GetLastSilenceByteOffset(double framerate, wav_hdr header, int frameAdjust, parameters *config)
{
	if(!config)
		return NO_INDEX;

	for(int i = config->types.typeCount - 1; i >= 0; i--)
	{
		if(config->types.typeArray[i].type == TYPE_SILENCE)
		{
			double offset = 0;

			// We remove 10 frames in order to not miss it due to frame rate differences
			offset = FramesToSeconds(GetBlockFrameOffset(i, config) - frameAdjust, framerate);
			offset = SecondsToBytes(header.SamplesPerSec, offset, NULL, NULL, NULL);
			return(offset);
		}
	}
	return 0;
}

long int GetBlockFrameOffset(int block, parameters *config)
{
	double offset = 0;

	if(!config)
		return 0;

	if(block > config->types.typeCount)
		return 0;

	for(int i = 0; i < block; i++)
		offset += config->types.typeArray[i].frames * config->types.typeArray[i].elementCount;
	return offset;
}

long int GetLastSyncFrameOffset(wav_hdr header, parameters *config)
{
	if(!config)
		return 0;

	for(int i = config->types.typeCount - 1; i >= 0; i--)
	{
		if(config->types.typeArray[i].type == TYPE_SYNC)
			return(GetBlockFrameOffset(i, config));
	}
	return 0;
}

int GetActiveBlockTypes(parameters *config)
{
	int count = 0;

	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type > TYPE_CONTROL)
			count ++;
	}
	return count;
}

int GetActiveAudioBlocks(parameters *config)
{
	int count = 0;

	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type > TYPE_CONTROL)
			count += config->types.typeArray[i].elementCount;
	}
	return count;
}

int GetTotalAudioBlocks(parameters *config)
{
	int count = 0;

	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
		count += config->types.typeArray[i].elementCount;

	return count;
}


long int GetLongestElementFrames(parameters *config)
{
	double longest = 0;

	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].frames > longest)
			longest = config->types.typeArray[i].frames;
	}
	return longest;
}

long int GetSignalTotalFrames(parameters *config)
{
	double total = 0;

	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
		total += config->types.typeArray[i].elementCount * config->types.typeArray[i].frames;

	return total;
}

long int GetBlockFrames(parameters *config, int pos)
{
	int elementsCounted = 0;

	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		elementsCounted += config->types.typeArray[i].elementCount;
		if(elementsCounted >= pos)
			return(config->types.typeArray[i].frames);
	}
	
	return 0;
}

char *GetBlockName(parameters *config, int pos)
{
	int elementsCounted = 0;

	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		elementsCounted += config->types.typeArray[i].elementCount;
		if(elementsCounted > pos)
			return(config->types.typeArray[i].typeName);
	}
	
	return 0;
}

int GetBlockSubIndex(parameters *config, int pos)
{
	int elementsCounted = 0, last = 0;

	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		elementsCounted += config->types.typeArray[i].elementCount;
		if(elementsCounted > pos)
			return(pos - last);
		last = elementsCounted;
	}
	
	return 0;
}

int GetBlockType(parameters *config, int pos)
{
	int elementsCounted = 0;

	if(!config)
		return TYPE_NOTYPE;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		elementsCounted += config->types.typeArray[i].elementCount;
		if(elementsCounted > pos)
			return(config->types.typeArray[i].type);
	}
	
	return TYPE_NOTYPE;
}

char *GetBlockColor(parameters *config, int pos)
{
	int elementsCounted = 0;

	if(!config)
		return "nconfig";

	for(int i = 0; i < config->types.typeCount; i++)
	{
		elementsCounted += config->types.typeArray[i].elementCount;
		if(elementsCounted > pos)
			return(config->types.typeArray[i].color);
	}
	
	return "black";
}

char *GetTypeColor(parameters *config, int type)
{
	if(!config)
		return "nconfig";

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type == type)
			return(config->types.typeArray[i].color);
	}
	
	return "black";
}

char *GetTypeName(parameters *config, int type)
{
	if(!config)
		return "nconfig";

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type == type)
			return(config->types.typeArray[i].typeName);
	}
	
	return "Type Name";
}

void FindFloor(AudioSignal *Signal, parameters *config)
{
	int 		index;
	Frequency	loudest;

	if(!Signal)
		return;
	
	if(!Signal->hasFloor)
		return;

	index = GetFirstSilenceIndex(config);
	if(index == NO_INDEX)
	{
		logmsg("There is no Silence block defined in the current format\n");
		return;
	}

	memset(&loudest, 0, sizeof(Frequency));

	if(config->verbose)
		logmsg(" - Frame Rate %g RefreshNoise %g tolerance %g\n",
			Signal->framerate, Signal->RefreshNoise, REFRESH_RATE_NOISE_DETECT_TOLERANCE);

	for(int i = 0; i < config->MaxFreq; i++)
	{
		if(Signal->Blocks[index].freq[i].hertz && Signal->Blocks[index].freq[i].amplitude != NO_AMPLITUDE)
		{
			//double difference;
	
			//difference = fabs(fabs(Signal->Blocks[index].freq[i].hertz) - Signal->RefreshNoise);
			//if(difference < REFRESH_RATE_NOISE_DETECT_TOLERANCE)
			if(Signal->Blocks[index].freq[i].hertz == Signal->RefreshNoise)
			{
				Signal->floorAmplitude = Signal->Blocks[index].freq[i].amplitude;
				Signal->floorFreq = Signal->Blocks[index].freq[i].hertz;
				logmsg(" - Silence block mains noise: %g Hz at %g dBFS\n",
					Signal->floorFreq, Signal->floorAmplitude);
				return;
			}
		}
	}

	for(int i = 0; i < config->MaxFreq; i++)
	{
		if(Signal->Blocks[index].freq[i].hertz && Signal->Blocks[index].freq[i].amplitude != NO_AMPLITUDE)
		{
			if(IsCRTNoise(Signal, Signal->Blocks[index].freq[i].hertz))
			{
				Signal->floorAmplitude = Signal->Blocks[index].freq[i].amplitude;
				Signal->floorFreq = Signal->Blocks[index].freq[i].hertz;
				logmsg(" - Silence block possible Video/CRT noise: %g Hz at %g dBFS\n",
					Signal->floorFreq, Signal->floorAmplitude);
				return;
			}
	
			if(!loudest.hertz && Signal->Blocks[index].freq[i].hertz)
				loudest = Signal->Blocks[index].freq[i];
		}
	}

	if(loudest.hertz)
	{
		Signal->floorAmplitude = loudest.amplitude;
		Signal->floorFreq = loudest.hertz;
		logmsg(" - Silence block max volume: %g Hz at %g dBFS\n",
			Signal->floorFreq, Signal->floorAmplitude);
		return;
	}

	logmsg(" - No meaninful Noise floor found, using the whole range\n");
	Signal->hasFloor = 0;  /* revoke it if not found */
}

void GlobalNormalize(AudioSignal *Signal, parameters *config)
{
	double		MaxMagnitude = 0;
	double		MinAmplitude = 0;
	double		MaxFreq = 0;
	int			MaxBlock = -1;

	if(!Signal)
		return;

	/* Find global peak */
	
	for(int block = 0; block < config->types.totalChunks; block++)
	{
		for(int i = 0; i < config->MaxFreq; i++)
		{
			if(!Signal->Blocks[block].freq[i].hertz)
				break;
			if(Signal->Blocks[block].freq[i].magnitude > MaxMagnitude)
			{
				MaxMagnitude = Signal->Blocks[block].freq[i].magnitude;
				MaxFreq = Signal->Blocks[block].freq[i].hertz;
				MaxBlock = block;
			}
		}
	}

	if(config->verbose)
	{
		if(MaxBlock != -1)
			logmsg(" - Max Volume found in block %d at %g Hz with %g magnitude\n", MaxBlock, MaxFreq, MaxMagnitude);
	}
	Signal->MaxMagnitude = MaxMagnitude;

	/* Normalize and calculate Amplitude in dBFSs */
	for(int block = 0; block < config->types.totalChunks; block++)
	{
		for(int i = 0; i < config->MaxFreq; i++)
		{
			if(!Signal->Blocks[block].freq[i].hertz)
				break;

			Signal->Blocks[block].freq[i].amplitude = 
				CalculateAmplitude(Signal->Blocks[block].freq[i].magnitude, MaxMagnitude);
			
			if(Signal->Blocks[block].freq[i].amplitude < MinAmplitude)
				MinAmplitude = Signal->Blocks[block].freq[i].amplitude;
		}
	}
	Signal->MinAmplitude = MinAmplitude;
}

void FindMaxMagnitude(AudioSignal *Signal, parameters *config)
{
	double		MaxMagnitude = 0;
	double		MaxFreq = 0;
	int			MaxBlock = -1;

	if(!Signal)
		return;

	// Find global peak
	for(int block = 0; block < config->types.totalChunks; block++)
	{
		for(int i = 0; i < config->MaxFreq; i++)
		{
			if(!Signal->Blocks[block].freq[i].hertz)
				break;
			if(Signal->Blocks[block].freq[i].magnitude > MaxMagnitude)
			{
				MaxMagnitude = Signal->Blocks[block].freq[i].magnitude;
				MaxFreq = Signal->Blocks[block].freq[i].hertz;
				MaxBlock = block;
			}
		}
	}

	if(MaxBlock != -1)
		Signal->MaxMagnitude = MaxMagnitude;

	if(config->verbose)
	{
		if(MaxBlock != -1)
			logmsg(" - Max Volume found in block %d (%s %d) at %g Hz with %g magnitude\n", 
					MaxBlock, GetBlockName(config, MaxBlock), GetBlockSubIndex(config, MaxBlock), MaxFreq, MaxMagnitude);
	}
}

void CalculateAmplitudes(AudioSignal *Signal, double ZeroDbMagReference, parameters *config)
{
	double MinAmplitude = 0;

	if(!Signal)
		return;

	//Calculate Amplitude in dBFS 
	for(int block = 0; block < config->types.totalChunks; block++)
	{
		for(int i = 0; i < config->MaxFreq; i++)
		{
			if(!Signal->Blocks[block].freq[i].hertz)
				break;

			Signal->Blocks[block].freq[i].amplitude = 
				CalculateAmplitude(Signal->Blocks[block].freq[i].magnitude, ZeroDbMagReference);

			if(Signal->Blocks[block].freq[i].amplitude < MinAmplitude)
				MinAmplitude = Signal->Blocks[block].freq[i].amplitude;
		}
	}

	Signal->MinAmplitude = MinAmplitude;
}


void CleanMatched(AudioSignal *ReferenceSignal, AudioSignal *TestSignal, parameters *config)
{
	for(int block = 0; block < config->types.totalChunks; block++)
	{
		for(int i = 0; i < config->MaxFreq; i++)
		{
			if(!ReferenceSignal->Blocks[block].freq[i].hertz)
				break;
			ReferenceSignal->Blocks[block].freq[i].matched = 0;	
		}
	}

	for(int block = 0; block < config->types.totalChunks; block++)
	{
		for(int i = 0; i < config->MaxFreq; i++)
		{
			if(!TestSignal->Blocks[block].freq[i].hertz)
				break;
			TestSignal->Blocks[block].freq[i].matched = 0;
		}
	}
}

void PrintFrequencies(AudioSignal *Signal, parameters *config)
{
	OutputFileOnlyStart();

	for(int block = 0; block < config->types.totalChunks; block++)
	{
		int type = TYPE_NOTYPE;
		logmsg("==================== %s# %d (%d) ===================\n", 
				GetBlockName(config, block), GetBlockSubIndex(config, block), block);

		type = GetBlockType(config, block);
		for(int j = 0; j < config->MaxFreq; j++)
		{
			if(type != TYPE_SILENCE && config->significantVolume > Signal->Blocks[block].freq[j].amplitude)
				break;

			//if(Signal->Blocks[block].freq[j].amplitude == NO_AMPLITUDE)
				//break;

			if(Signal->Blocks[block].freq[j].hertz)
			{
				logmsg("Frequency [%5d] %7g Hz Amplitude: %g Phase: %g",
					j, 
					Signal->Blocks[block].freq[j].hertz,
					Signal->Blocks[block].freq[j].amplitude,
					Signal->Blocks[block].freq[j].phase);
				/* detect CRT frequency */
				if(IsCRTNoise(Signal, Signal->Blocks[block].freq[j].hertz))
					logmsg(" [CRT Noise?]");
				logmsg("\n");
			}
		}
	}

	OutputFileOnlyEnd();
}

/* check ProcessSamples in mdwave if changed, for reverse FFTW */
inline double CalculateMagnitude(fftw_complex value, long int size)
{
	double r1 = 0;
	double i1 = 0;
	double magnitude = 0;

	r1 = creal(value);
	i1 = cimag(value);
	magnitude = sqrt(r1*r1 + i1*i1)/size;
	return magnitude;
}

inline double CalculatePhase(fftw_complex value)
{
	double r1 = 0;
	double i1 = 0;
	double phase = 0;

	r1 = creal(value);
	i1 = cimag(value);
	phase = atan2(i1, r1)*180/M_PI;
	return phase;
}

inline double CalculateAmplitude(double magnitude, double MaxMagnitude)
{
	double amplitude = 0;

	amplitude = 20*log10(magnitude/MaxMagnitude);
	amplitude = RoundFloat(amplitude, 2);
	return amplitude;
}

inline double CalculateFrequency(double boxindex, double boxsize, int HertzAligned)
{
	double Hertz = 0;

	Hertz = boxindex/boxsize;
	if(!HertzAligned) // if zero padded (Hertz Aligned), we are using 1hz integer bins
		Hertz = RoundFloat(Hertz, 2);  // default, overkill yes
	return Hertz;
}

int InsertFrequencySorted(AudioBlocks *AudioArray, Frequency element, long int currentsize)
{
	if(!AudioArray || !AudioArray->freq)
		return 0;

	if(!currentsize)
	{
		AudioArray->freq[0] = element;
		return 1;
	}

	// worst case scenario
	if(AudioArray->freq[currentsize-1].magnitude >= element.magnitude)
	{
		AudioArray->freq[currentsize] = element;
		return 1;
	}

	for(long int j = 0; j < currentsize; j++)
	{
		if(element.magnitude > AudioArray->freq[j].magnitude)
		{
			// Move the previous values down the array 
			for(int k = currentsize; k > j; k--)
				AudioArray->freq[k] = AudioArray->freq[k - 1];
	
			AudioArray->freq[j] = element;
			return 1;
		}
	}

 	logmsg("WARNING InsertFrequencySorted No match found!\n");
	return 0;
}

void FillFrequencyStructures(AudioBlocks *AudioArray, parameters *config)
{
	long int i = 0, startBin= 0, endBin = 0, count = 0;
	double boxsize = 0, size = 0;

	size = AudioArray->fftwValues.size;
	boxsize = RoundFloat(AudioArray->fftwValues.seconds, 4);

	startBin = floor(config->startHz*boxsize);
	endBin = floor(config->endHz*boxsize);

	for(i = startBin; i < endBin; i++)
	{
		double		Hertz;

		Hertz = CalculateFrequency(i, boxsize, config->ZeroPad);

		if(Hertz)
		{
			Frequency	element;

			memset(&element, 0, sizeof(Frequency));
			element.hertz = Hertz;
			element.magnitude = CalculateMagnitude(AudioArray->fftwValues.spectrum[i], size);
			element.amplitude = NO_AMPLITUDE;
			element.phase = CalculatePhase(AudioArray->fftwValues.spectrum[i]);

			if(InsertFrequencySorted(AudioArray, element, count))
			{
				if(count < config->MaxFreq - 1)
					count++;
			}
		}
	}
}

void PrintComparedBlocks(AudioBlocks *ReferenceArray, AudioBlocks *ComparedArray, parameters *config, AudioSignal *Signal)
{
	OutputFileOnlyStart();

	/* changed Magnitude->amplitude */
	for(int j = 0; j < config->MaxFreq; j++)
	{
		if(config->significantVolume > ReferenceArray->freq[j].amplitude)
			break;

		if(ReferenceArray->freq[j].hertz)
		{
			int match = 0;

			logmsg("[%5d] Ref: %7g Hz %6.2fdBFS [>%3d]", 
						j,
						ReferenceArray->freq[j].hertz,
						ReferenceArray->freq[j].amplitude,
						ReferenceArray->freq[j].matched - 1);

			if(ComparedArray->freq[j].hertz)
				logmsg("\tComp: %7g Hz %6.2fdBFS [<%3d]", 
						ComparedArray->freq[j].hertz,
						ComparedArray->freq[j].amplitude,
						ComparedArray->freq[j].matched - 1);
			else
				logmsg("\tCompared:\tNULL");
			match = ReferenceArray->freq[j].matched - 1;
			if(match != -1)
			{
				if(ReferenceArray->freq[j].amplitude == 
				ComparedArray->freq[match].amplitude)
					logmsg("FA");
				else
				{
					double diff = fabs(fabs(ReferenceArray->freq[j].amplitude) - 
						fabs(ComparedArray->freq[match].amplitude));
					if(diff < config->tolerance)
						logmsg("FT");
					else
						logmsg("F-");
				}
			}
			logmsg("\n");
		}
	}
	logmsg("\n\n");

	OutputFileOnlyEnd();
}

double CalculateWeightedError(double pError, parameters *config)
{
	int option = 0;

	option = config->outputFilterFunction;
	switch(option)
	{
		case 0:
			/* No weighting, paint everything equally */
			pError = 1;
			break;
		case 1:
			/* square root */
			pError = sqrt(fabs(pError));
			break;
		case 2:
			/* Map to Beta function */
			pError = incbeta(3.0, 3.0, pError);
			break;
		case 3:
			/* Linear from input anged 0 - 1*/
			pError = pError;
			break;
		case 4:
			/* x^2 */
			pError = pError*pError;
			break;
		case 5:
			/* Map to Beta function */
			pError = incbeta(16.0, 2.0, pError);
			break;
		default:
			/* This is unexpected behaviour, log it */
			logmsg("CalculateWeightedError, out of range value %d\n", option);
			pError = pError;
			break;
	}

	return pError;
}

inline double RoundFloat(double x, int p)
{
	if (x != 0.0) {
		return ((floor((fabs(x)*pow((double)(10.0),p))+0.5))/pow((double)(10.0),p))*(x/fabs(x));
	} else {
		return 0.0;
	}
}

inline double FramesToSeconds(double frames, double framerate)
{
	return(frames*framerate/1000.0);
}

inline long int SecondsToBytes(long int samplerate, double seconds, int *leftover, int *discard, double *leftDecimals)
{
	return(RoundTo4bytes(samplerate*4.0*seconds*sizeof(char), leftover, discard, leftDecimals));
}

inline double BytesToSeconds(long int samplerate, long int bytes)
{
	return((double)bytes/(samplerate*4.0));
}

long int RoundTo4bytes(double src, int *leftover, int *discard, double *leftDecimals)
{
	int extra = 0;

	if(discard)
		*discard = 0;

	if(!leftover)
		src = ceil(src);
	else
	{
		if(leftDecimals)
			*leftDecimals += GetDecimalValues(src);
		src = floor(src);
	}

	extra = ((long int)src) % 4;
	if(extra != 0)
	{
		if(leftover && discard)
		{
			src -= extra;
			(*leftover) += extra;
			if(*leftover >= 4)
			{
				*leftover -= 4;
				*discard = 4;
			}
			else
				*discard = 0;
		}
		else
			src += 4 - extra;
	}

	if(leftDecimals)
	{
		if(*leftDecimals >= 4.0)
		{
			*discard += 4;
			*leftDecimals -= 4.0;
		}
	}

	return (src);
}

inline double GetDecimalValues(double value)
{
	double integer = 0;

	value = modf(value, &integer);
	return value;
}

long int GetZeroPadValues(long int *monoSignalSize, double *seconds, long int samplerate)
{
	long int zeropadding = 0;

	// Align frequency bins to 1hz
	if(*monoSignalSize != samplerate)
	{
		if(*monoSignalSize < samplerate)
		{
			zeropadding = samplerate - *monoSignalSize;
			*monoSignalSize += zeropadding;
			*seconds = 1;
		}
		else
		{
			int times;

			times = ceil((double)*monoSignalSize/(double)samplerate);
			zeropadding = times*samplerate - *monoSignalSize;
			*monoSignalSize += zeropadding;
			*seconds = times;
		}
	}
	return zeropadding;
}

double CalculateFrameRate(AudioSignal *Signal, parameters *config)
{
	double framerate = 0, endOffset = 0, startOffset = 0, samplerate = 0;
	double LastSyncFrameOffset = 0;
	double expectedFR = 0, diff = 0;

	startOffset = Signal->startOffset;
	endOffset = Signal->endOffset;
	samplerate = Signal->header.SamplesPerSec;
	LastSyncFrameOffset = GetLastSyncFrameOffset(Signal->header, config);
	expectedFR = GetPlatformMSPerFrame(config);

	framerate = (endOffset-startOffset)/(samplerate*LastSyncFrameOffset);
	framerate = framerate*1000.0/4.0;  // 1000 ms and 4 bytes per stereo sample
	//framerate = RoundFloat(framerate, 4);

	diff = RoundFloat(fabs(expectedFR - framerate), 3);
	if(diff > 0.002 && diff < 0.02)
		logmsg(" - Framerate difference is %g (Audio card timing?)\n", diff);

	return framerate;
}

double CalculateScanRate(AudioSignal *Signal)
{
	return(RoundFloat(1000.0/Signal->framerate, 4));
}

