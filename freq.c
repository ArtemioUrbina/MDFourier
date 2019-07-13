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

double FindFrequencyBinSizeForBlock(AudioSignal *Signal, long int block)
{
	if(!Signal)
		return 0;
	if(!Signal->Blocks)
		return 0;
	
	return((double)Signal->header.fmt.SamplesPerSec/(double)Signal->Blocks[block].fftwValues.size);
}

double FindFrequencyBracket(double frequency, size_t size, long samplerate)
{
	double seconds = 0, minDiff = 0, targetFreq = 0;
	long int monoSignalSize;

	minDiff = (double)samplerate/2.0;
	targetFreq = frequency;
	monoSignalSize = size/2;
	seconds = (double)size/((double)samplerate*2.0);

	for(int i = 1; i < monoSignalSize/2+1; i++)
	{
		double Hertz = 0, difference = 0;

		Hertz = CalculateFrequency(i, seconds, 0);
		difference = fabs(Hertz - frequency);
		if(difference < minDiff)
		{
			targetFreq = Hertz;
			minDiff = difference;
		}
	}
	return targetFreq;
}


void CalcuateFrequencyBrackets(AudioSignal *Signal, parameters *config)
{
	long int index = 0;

	if(!Signal)
		return;
	if(!Signal->Blocks)
		return;

	index = GetFirstSilenceIndex(config);
	if(index != NO_INDEX)
	{
		double binSize = 0, refreshNoise = 0;

		binSize = FindFrequencyBinSizeForBlock(Signal, index);

		refreshNoise = FindFrequencyBracket(roundFloat(1000.0/Signal->framerate), Signal->Blocks[index].fftwValues.size, Signal->header.fmt.SamplesPerSec);
		Signal->gridFrequencyLow = refreshNoise - binSize;
		Signal->gridFrequencyHigh = refreshNoise + binSize;

		Signal->HRefreshLow = FindFrequencyBracket(15625, Signal->Blocks[index].fftwValues.size, Signal->header.fmt.SamplesPerSec) - binSize;
		Signal->HRefreshHigh = FindFrequencyBracket(15750, Signal->Blocks[index].fftwValues.size, Signal->header.fmt.SamplesPerSec) + binSize;
	}
	else
	{
		if(!config->noSyncProfile)
			logmsg("\nWARNING: Frequency Brackets can't be found since there is no Silence block in MFN file\n\n");
	}
}

int IsHRefreshNoise(AudioSignal *Signal, double freq)
{
	if(!Signal)
		return 0;

	/* Peak around 15.625 kHz and 15.750 kHz  */
	if(freq >= Signal->HRefreshLow && freq <= Signal->HRefreshHigh)
		return 1;
	return 0;
}

int IsGridFrequencyNoise(AudioSignal *Signal, double freq)
{
	if(!Signal)
		return 0;

	if(freq >= Signal->gridFrequencyLow && freq <= Signal->gridFrequencyHigh)
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

	Signal->role = NO_ROLE;
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

	memset(&Signal->MaxMagnitude, 0, sizeof(MaxMagn));
	Signal->MinAmplitude = 0;

	Signal->gridFrequencyLow = 0;
	Signal->gridFrequencyHigh = 0;
	Signal->HRefreshLow = 0;
	Signal->HRefreshHigh = 0;

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

	memset(&Signal->MaxMagnitude, 0, sizeof(MaxMagn));
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

int LoadProfile(parameters *config)
{
	FILE 	*file;
	char	lineBuffer[LINE_BUFFER_SIZE];
	char	buffer[PARAM_BUFFER_SIZE];

	if(!config)
		return 0;

	file = fopen(config->profileFile, "r");
	if(!file)
	{
		logmsg("Could not load profile configuration file: \"%s\"\n", config->profileFile);
		return 0;
	}
	
	readLine(lineBuffer, file);
	sscanf(lineBuffer, "%s ", buffer);
	if(strcmp(buffer, "MDFourierAudioBlockFile") == 0)
	{
		sscanf(lineBuffer, "%*s %s\n", buffer);
		if(atof(buffer) > 1.2)
		{
			logmsg("This executable can parse \"MDFourierAudioBlockFile 1.2\" files only\n");
			fclose(file);
			return 0;
		}
		return(LoadAudioBlockStructure(file, config));
	}

	if(strcmp(buffer, "MDFourierNoSyncProfile") == 0)
	{
		sscanf(lineBuffer, "%*s %s\n", buffer);
		if(atof(buffer) > 1.1)
		{
			logmsg("This executable can parse \"MDFourierNoSyncProfile 1.1\" files only\n");
			fclose(file);
			return 0;
		}
		return(LoadAudioNoSyncProfile(file, config));
	}

	logmsg("Not an MD Fourier Audio Profile File\n");
	fclose(file);
	return 0;
}

int LoadAudioBlockStructure(FILE *file, parameters *config)
{
	int		insideInternal = 0;
	char	lineBuffer[LINE_BUFFER_SIZE];
	char	buffer[PARAM_BUFFER_SIZE];

	config->noSyncProfile = 0;

	readLine(lineBuffer, file);
	if(sscanf(lineBuffer, "%255[^\n]\n", config->types.Name) != 1)
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
	config->types.referenceMSPerFrame = strtod(buffer, NULL);
	if(!config->types.referenceMSPerFrame)
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
		logmsg("Invalid Pulse Minimum Amplitude:\n%s\n", lineBuffer);
		fclose(file);
		return 0;
	}

	if(!config->types.pulseVolDiff)
	{
		logmsg("Invalid Pulse Amplitude Difference:\n%s\n", lineBuffer);
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
	memset(config->types.typeArray, 0, sizeof(AudioBlockType));

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
			case 'i':
				config->types.typeArray[i].type = TYPE_INTERNAL;
				break;
			case 'k':
				config->types.typeArray[i].type = TYPE_SKIP;
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
		
		if(config->types.typeArray[i].type == TYPE_INTERNAL)
		{
			if(insideInternal)
				insideInternal = 0;
			else
				insideInternal = 1;

			if(sscanf(lineBuffer, "%*s %*c %d %d %s %c %d %lf\n", 
				&config->types.typeArray[i].elementCount,
				&config->types.typeArray[i].frames,
				&config->types.typeArray[i].color [0],
				&config->types.typeArray[i].channel,
				&config->types.typeArray[i].syncTone,
				&config->types.typeArray[i].syncLen) != 6)
			{
				logmsg("Invalid MD Fourier Audio Blocks File (Element Count, frames, color, channel): %s\n", lineBuffer);
				fclose(file);
				return 0;
			}
		}
		else
		{
			if(sscanf(lineBuffer, "%*s %*c %d %d %s %c\n", 
				&config->types.typeArray[i].elementCount,
				&config->types.typeArray[i].frames,
				&config->types.typeArray[i].color [0],
				&config->types.typeArray[i].channel) != 4)
			{
				logmsg("Invalid MD Fourier Audio Blocks File (Element Count, frames, color, channel): %s\n", lineBuffer);
				fclose(file);
				return 0;
			}
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

	if(insideInternal)
	{
		logmsg("Internal sync detection block didn't have a closing section\n");
		return 0;
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

int LoadAudioNoSyncProfile(FILE *file, parameters *config)
{
	char	lineBuffer[LINE_BUFFER_SIZE];
	char	buffer[PARAM_BUFFER_SIZE];

	config->noSyncProfile = 1;
	if(config->plotDifferences)
	{
		config->averagePlot = 1;
		config->plotDifferences = 0;
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
		logmsg("Invalid Reference Frame Rate Adjustment '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}
	config->types.referenceMSPerFrame = strtod(buffer, NULL);
	if(!config->types.referenceMSPerFrame)
	{
		logmsg("Invalid Reference Frame Rate Adjustment '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}

	readLine(lineBuffer, file);
	if(sscanf(lineBuffer, "%s\n", buffer) != 1)
	{
		logmsg("Invalid Comparison Frame Rate Adjustment '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}
	config->types.comparisonMSPerFrame = strtod(buffer, NULL);
	if(!config->types.comparisonMSPerFrame)
	{
		logmsg("Invalid Comparison Frame Rate Adjustment '%s'\n", lineBuffer);
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
	memset(config->types.typeArray, 0, sizeof(AudioBlockType));

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
		
		if(sscanf(lineBuffer, "%*s %*c %d %d %s %c\n", 
			&config->types.typeArray[i].elementCount,
			&config->types.typeArray[i].frames,
			&config->types.typeArray[i].color [0],
			&config->types.typeArray[i].channel) != 4)
		{
			logmsg("Invalid MD Fourier Audio Blocks File (Element Count, frames, color, channel): %s\n", lineBuffer);
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
		logmsg("%s %d %d %d %s %c\n", 
			config->types.typeArray[i].typeName,
			config->types.typeArray[i].type,
			config->types.typeArray[i].elementCount,
			config->types.typeArray[i].frames,
			config->types.typeArray[i].color,
			config->types.typeArray[i].channel);
	}
}

double GetMSPerFrame(AudioSignal *Signal, parameters *config)
{
	if(!config)
		return 0;

	if(!Signal || !config->noSyncProfile)
		return(roundFloat(config->types.referenceMSPerFrame));
	if(Signal->role == ROLE_COMP)
		return(roundFloat(config->types.comparisonMSPerFrame));
	if(Signal->role == NO_ROLE)
		logmsg("WARNING: No role assigned, using Reference Frame Rate\n");
	return(roundFloat(config->types.referenceMSPerFrame));
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
		if(config->verbose && diff > 0.001)
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
	for(int i = config->types.typeCount - 1; i > first; i--)
	{
		if(config->types.typeArray[i].type == TYPE_SYNC)
			return i;
	}
	return NO_INDEX;
}

int GetLastSyncElementIndex(parameters *config)
{
	int first = 0, elementsCounted = 0;

	if(!config)
		return 0;

	first = GetFirstSyncIndex(config);
	for(int i = config->types.typeCount - 1; i > first; i--)
	{
		if(config->types.typeArray[i].type == TYPE_SYNC)
		{
			for(int j = 0; j < i; j++)
				elementsCounted += config->types.typeArray[j].elementCount;
			return elementsCounted;
		}
	}
	
	return 0;
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

int GetFirstMonoIndex(parameters *config)
{
	int index = 0;

	if(!config)
		return NO_INDEX;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type > TYPE_SILENCE && 
			config->types.typeArray[i].channel == 'm')
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
			double offset = 0, length = 0;

			offset = FramesToSeconds(GetBlockFrameOffset(i, config) - frameAdjust, framerate);
			offset = SecondsToBytes(header.fmt.SamplesPerSec, offset, NULL, NULL, NULL);

			// and at 3/4 the silence length
			length = FramesToSeconds(config->types.typeArray[i].frames/4*3, framerate);
			length = SecondsToBytes(header.fmt.SamplesPerSec, length, NULL, NULL, NULL);
			offset += length;
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
	int first = 0;

	if(!config)
		return NO_INDEX;

	first = GetFirstSyncIndex(config);
	for(int i = config->types.typeCount - 1; i > first; i--)
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
		if(elementsCounted > pos)
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

int GetInternalSyncTone(int pos, parameters *config)
{
	int elementsCounted = 0;

	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		elementsCounted += config->types.typeArray[i].elementCount;
		if(elementsCounted > pos && config->types.typeArray[i].type == TYPE_INTERNAL)
			return(config->types.typeArray[i].syncTone);
	}
	
	logmsg("WARNING: sync tone request for invalid block\n");
	return 0;
}

double GetInternalSyncLen(int pos, parameters *config)
{
	int elementsCounted = 0;

	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		elementsCounted += config->types.typeArray[i].elementCount;
		if(elementsCounted > pos && config->types.typeArray[i].type == TYPE_INTERNAL)
			return(config->types.typeArray[i].syncLen);
	}
	
	logmsg("WARNING: sync lenght request for invalid block\n");
	return 0;
}

int GetInternalSyncTotalLength(int pos, parameters *config)
{
	int frames = 0, inside = 0, index = 0;

	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(index >= pos)
		{
			if(config->types.typeArray[i].type == TYPE_INTERNAL)
			{
				if(!inside)
					inside = 1;
				else
				{
					inside = 0;
					return frames;
				}
			}
			else
			{
				if(inside)
					frames += config->types.typeArray[i].elementCount *
							config->types.typeArray[i].frames;
			}
		}
		index += config->types.typeArray[i].elementCount;
	}
	return 0;
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

	loudest.hertz = 0;
	loudest.amplitude = NO_AMPLITUDE;

	for(int i = 0; i < config->MaxFreq; i++)
	{
		if(Signal->Blocks[index].freq[i].hertz && Signal->Blocks[index].freq[i].amplitude != NO_AMPLITUDE)
		{
			if(Signal->Blocks[index].freq[i].amplitude > loudest.amplitude)
				loudest = Signal->Blocks[index].freq[i];
		}
		else
			break;
	}

	if(loudest.hertz && loudest.amplitude != NO_AMPLITUDE)
	{
		logmsg(" - %s Signal Noise floor: %g dBFS [%g Hz] %s\n", 
			Signal->role == ROLE_REF ? "Reference" : "Comparison",
			loudest.amplitude,
			loudest.hertz,
			loudest.amplitude < PCM_16BIT_MIN_AMPLITUDE ? "(not significant)" : "");
	}
	/*
	if(config->verbose)
		logmsg(" - Frame Rate %g RefreshNoise %g\n",
			Signal->framerate, Signal->RefreshNoise);
	*/

	for(int i = 0; i < config->MaxFreq; i++)
	{
		if(Signal->Blocks[index].freq[i].hertz && Signal->Blocks[index].freq[i].amplitude != NO_AMPLITUDE)
		{
			if(IsGridFrequencyNoise(Signal, Signal->Blocks[index].freq[i].hertz))
			{
				Signal->floorAmplitude = Signal->Blocks[index].freq[i].amplitude;
				Signal->floorFreq = Signal->Blocks[index].freq[i].hertz;
				logmsg(" - Possible electrical grid frequency noise: %g dBFS [%g Hz]\n",
						Signal->floorAmplitude, Signal->floorFreq);
				return;
			}

			if(IsHRefreshNoise(Signal, Signal->Blocks[index].freq[i].hertz))
			{
				Signal->floorAmplitude = Signal->Blocks[index].freq[i].amplitude;
				Signal->floorFreq = Signal->Blocks[index].freq[i].hertz;
				logmsg(" - Possible horizontal scan rate noise : %g dBFS [%g Hz]\n",
						Signal->floorAmplitude, Signal->floorFreq);
				return;
			}
		}
	}

	if(loudest.hertz && loudest.amplitude != NO_AMPLITUDE)
	{
		Signal->floorAmplitude = loudest.amplitude;
		Signal->floorFreq = loudest.hertz;
		logmsg(" - Silence block max amplitude: %g Hz at %g dBFS\n",
			Signal->floorFreq, Signal->floorAmplitude);
		return;
	}

	logmsg(" - No meaningful Noise floor found, using the whole range\n");
	Signal->hasFloor = 0;  /* revoke it if not found */
}

/// Now only called by MDWave
void GlobalNormalize(AudioSignal *Signal, parameters *config)
{
	double		MaxMagnitude = 0;
	double		MinAmplitude = 0;
	double		MaxFreq = 0;
	int			MaxBlock = -1;

	if(!Signal)
		return;

	// Find global peak 
	for(int block = 0; block < config->types.totalChunks; block++)
	{
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(type >= TYPE_SILENCE)
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
	}

	if(config->verbose)
	{
		if(MaxBlock != -1)
			logmsg(" - MAX Amplitude found in block %d at %g Hz with %g magnitude\n", MaxBlock, MaxFreq, MaxMagnitude);
	}

	Signal->MaxMagnitude.magnitude = MaxMagnitude;
	Signal->MaxMagnitude.hertz = MaxFreq;
	Signal->MaxMagnitude.block = MaxBlock;

	// Normalize and calculate Amplitude in dBFSs 
	for(int block = 0; block < config->types.totalChunks; block++)
	{
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(type >= TYPE_SILENCE)
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
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(type > TYPE_SILENCE)
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
	}

	if(MaxBlock != -1)
	{
		Signal->MaxMagnitude.magnitude = MaxMagnitude;
		Signal->MaxMagnitude.hertz = MaxFreq;
		Signal->MaxMagnitude.block = MaxBlock;
	}

	if(config->verbose)
	{
		if(MaxBlock != -1)
			logmsg(" - Max Amplitude found in block %d (%s %d) at %g Hz with magnitude [%g]\n", 
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
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(type >= TYPE_SILENCE)
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

void PrintFrequenciesBlockMagnitude(AudioSignal *Signal, Frequency *freq, int type, parameters *config)
{
	if(!freq)
		return;

	for(int j = 0; j < config->MaxFreq; j++)
	{
		if(freq[j].hertz)
		{
			logmsg("Frequency [%5d] %7g Hz Magnitude: %g Phase: %g",
				j, 
				freq[j].hertz,
				freq[j].magnitude,
				freq[j].phase);
			/* detect VideoRefresh frequency */
			if(Signal && IsHRefreshNoise(Signal, freq[j].hertz))
				logmsg(" [Horizontal Refresh Noise?]");
			logmsg("\n");
		}
	}
}


void PrintFrequenciesBlock(AudioSignal *Signal, Frequency *freq, int type, parameters *config)
{
	if(!freq)
		return;

	for(int j = 0; j < config->MaxFreq; j++)
	{
		if(type != TYPE_SILENCE && config->significantAmplitude > freq[j].amplitude)
			break;

		if(freq[j].hertz && freq[j].amplitude != NO_AMPLITUDE)
		{
			logmsg("Frequency [%5d] %7g Hz Amplitude: %g dBFS Phase: %g",
				j, 
				freq[j].hertz,
				freq[j].amplitude,
				freq[j].phase);
			/* detect VideoRefresh frequency */
			if(Signal && IsHRefreshNoise(Signal, freq[j].hertz))
				logmsg(" [Horizontal Refresh Noise?]");
			logmsg("\n");
		}
	}
}

void PrintFrequenciesWMagnitudes(AudioSignal *Signal, parameters *config)
{
	OutputFileOnlyStart();

	for(int block = 0; block < config->types.totalChunks; block++)
	{
		int type = TYPE_NOTYPE;

		logmsg("==================== %s# %d (%d) ===================\n", 
				GetBlockName(config, block), GetBlockSubIndex(config, block), block);

		type = GetBlockType(config, block);
		PrintFrequenciesBlockMagnitude(Signal, Signal->Blocks[block].freq, type, config);
	}
	OutputFileOnlyEnd();
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
		PrintFrequenciesBlock(Signal, Signal->Blocks[block].freq, type, config);
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

	if(magnitude == 0.0 || MaxMagnitude == 0.0)
		return NO_AMPLITUDE;

	amplitude = 20*log10(magnitude/MaxMagnitude);
	amplitude = roundFloat(amplitude);
	return amplitude;
}

inline double CalculateFrequency(double boxindex, double boxsize, int HertzAligned)
{
	double Hertz = 0;

	Hertz = boxindex/boxsize;
	if(!HertzAligned) // if zero padded (Hertz Aligned), we are using 1Hz integer bins
		Hertz = roundFloat(Hertz);  // default, overkill yes
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

 	logmsg("\nWARNING InsertFrequencySorted No match found!\n\n");
	return 0;
}

void FillFrequencyStructures(AudioBlocks *AudioArray, parameters *config)
{
	long int i = 0, startBin= 0, endBin = 0, count = 0;
	double boxsize = 0, size = 0;

	size = AudioArray->fftwValues.size;
	// Round to 3 decimal places so that 48kHz and 44 kHz line up
	boxsize = RoundFloat(AudioArray->fftwValues.seconds, 3);

	if(AudioArray->type != TYPE_SILENCE)
	{
		startBin = ceil(config->startHz*boxsize);
		endBin = floor(config->endHz*boxsize);
	}
	else
	{
		startBin = ceil(START_HZ*boxsize);
		endBin = floor(END_HZ*boxsize);
	}

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
		if(config->significantAmplitude > ReferenceArray->freq[j].amplitude)
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
	return(RoundTo4bytes((double)samplerate*4.0*seconds*sizeof(char), leftover, discard, leftDecimals));
}

inline double BytesToSeconds(long int samplerate, long int bytes)
{
	return((double)bytes/((double)samplerate*4.0));
}

inline double BytesToFrames(long int samplerate, long int bytes, double framerate)
{
	return(roundFloat((double)bytes/((double)samplerate*4.0)/framerate*1000.0));
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

	// Align frequency bins to 1Hz
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
	samplerate = Signal->header.fmt.SamplesPerSec;
	LastSyncFrameOffset = GetLastSyncFrameOffset(Signal->header, config);
	expectedFR = GetMSPerFrame(Signal, config);

	framerate = (endOffset-startOffset)/(samplerate*LastSyncFrameOffset);
	framerate = framerate*1000.0/4.0;  // 1000 ms and 4 bytes per stereo sample
	framerate = roundFloat(framerate);

	diff = roundFloat(fabs(expectedFR - framerate));
	//The range to report this on will probably vary by console...
	if(config->verbose && diff > 0.001 && diff < 0.02)
	{
		double ACsamplerate = 0;

		ACsamplerate = (endOffset-startOffset)/(expectedFR*LastSyncFrameOffset);
		ACsamplerate = ACsamplerate*1000.0/4.0;
		logmsg(" - %s file framerate difference is %g.\n\tAudio card sample rate estimated at %g\n",
				Signal->role == ROLE_REF ? "Reference" : "Comparision",
				diff, ACsamplerate);
		
	}

	return framerate;
}

double CalculateScanRate(AudioSignal *Signal)
{
	return(roundFloat(1000.0/Signal->framerate));
}

double FindDifferenceAverage(parameters *config)
{
	double		AvgDifAmp = 0;
	long int	count = 0;

	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	for(int b = 0; b < config->types.totalChunks; b++)
	{
		if(config->Differences.BlockDiffArray[b].type <= TYPE_CONTROL)
			continue;


		for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
		{
			AvgDifAmp += fabs(config->Differences.BlockDiffArray[b].amplDiffArray[a].diffAmplitude);
			count ++;
		}
	}

	if(count)
		AvgDifAmp /= count;

	return AvgDifAmp;
}

int FindDifferenceTypeTotals(int type, long int *cntAmplBlkDiff, long int *cmpAmplBlkDiff, parameters *config)
{
	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	if(!cntAmplBlkDiff || !cmpAmplBlkDiff)
		return 0;

	*cntAmplBlkDiff = 0;
	*cmpAmplBlkDiff = 0;

	for(int b = 0; b < config->types.totalChunks; b++)
	{
		if(config->Differences.BlockDiffArray[b].type <= TYPE_CONTROL)
			continue;

		if(type == config->Differences.BlockDiffArray[b].type)
		{
			*cntAmplBlkDiff += config->Differences.BlockDiffArray[b].cntAmplBlkDiff;
			*cmpAmplBlkDiff += config->Differences.BlockDiffArray[b].cmpAmplBlkDiff;
		}
	}

	return 1;
}

int FindMissingTypeTotals(int type, long int *cntFreqBlkDiff, long int *cmpFreqBlkDiff, parameters *config)
{
	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	if(!cntFreqBlkDiff || !cmpFreqBlkDiff)
		return 0;

	*cntFreqBlkDiff = 0;
	*cmpFreqBlkDiff = 0;

	for(int b = 0; b < config->types.totalChunks; b++)
	{
		if(config->Differences.BlockDiffArray[b].type <= TYPE_CONTROL)
			continue;

		if(type == config->Differences.BlockDiffArray[b].type)
		{
			*cntFreqBlkDiff += config->Differences.BlockDiffArray[b].cntFreqBlkDiff;
			*cmpFreqBlkDiff += config->Differences.BlockDiffArray[b].cmpFreqBlkDiff;
		}
	}

	return 1;
}