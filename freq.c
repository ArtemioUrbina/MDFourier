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

#define SORT_NAME FFT_Frequency_Magnitude
#define SORT_TYPE Frequency
#define SORT_CMP(x, y)  ((x).magnitude > (y).magnitude ? -1 : ((x).magnitude == (y).magnitude ? 0 : 1))
#include "sort.h"  // https://github.com/swenson/sort/


double FindFrequencyBinSizeForBlock(AudioSignal *Signal, long int block)
{
	if(!Signal)
		return 0;
	if(!Signal->Blocks || !Signal->Blocks[block].fftwValues.size)
		return 0;
	
	return((double)Signal->header.fmt.SamplesPerSec/(double)Signal->Blocks[block].fftwValues.size);
}

double FindFrequencyBracketForSync(double frequency, size_t size, int AudioChannels, long samplerate, parameters *config)
{
	long int startBin= 0, endBin = 0;
	double seconds = 0, boxsize = 0, minDiff = 0, targetFreq = 0;

	if(!config)
	{
		logmsg("ERROR: Config was NULL for FindFrequencyBracket\n");
		return 0;
	}

	minDiff = (double)samplerate/2.0;
	targetFreq = frequency;

	seconds = (double)size/((double)samplerate*(double)AudioChannels);
	boxsize = RoundFloat(seconds, 3);
	if(boxsize == 0)
		boxsize = seconds;

	startBin = ceil(1*boxsize);
	endBin = floor(samplerate*boxsize);

	for(int i = startBin; i < endBin; i++)
	{
		double Hertz = 0, difference = 0;

		Hertz = CalculateFrequency(i, boxsize, config);
		difference = fabs(Hertz - frequency);
		if(difference < minDiff)
		{
			targetFreq = Hertz;
			minDiff = difference;
			if(minDiff == 0)
				break;
		}
	}
	return targetFreq;
}

double FindFrequencyBracket(double frequency, size_t size, int AudioChannels, long samplerate, parameters *config)
{
	long int startBin= 0, endBin = 0;
	double seconds = 0, boxsize = 0, minDiff = 0, targetFreq = 0;

	if(!config)
	{
		logmsg("ERROR: Config was NULL for FindFrequencyBracket\n");
		return 0;
	}

	if(config->startHz > frequency)
		return 0;

	if(config->endHz < frequency)
		return 0;

	minDiff = (double)samplerate/2.0;
	targetFreq = frequency;

	seconds = (double)size/((double)samplerate*(double)AudioChannels);
	boxsize = RoundFloat(seconds, 3);
	if(boxsize == 0)
		boxsize = seconds;

	startBin = ceil(config->startHz*boxsize);
	endBin = floor(config->endHz*boxsize);

	for(int i = startBin; i < endBin; i++)
	{
		double Hertz = 0, difference = 0;

		Hertz = CalculateFrequency(i, boxsize, config);
		difference = fabs(Hertz - frequency);
		if(difference < minDiff)
		{
			targetFreq = Hertz;
			minDiff = difference;
			if(minDiff == 0)
				break;
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
		double gridNoise = 0, scanNoise = 0, crossNoise = 0, ntsc = 0, pal = 0;

		Signal->SilenceBinSize = FindFrequencyBinSizeForBlock(Signal, index);

		// NTSC or PAL
		ntsc = fabs(60.0 - roundFloat(1000.0/Signal->framerate));
		pal = fabs(50.0 - roundFloat(1000.0/Signal->framerate));
		gridNoise = ntsc < pal ? 60.0 : 50.0;
		Signal->gridFrequency = FindFrequencyBracket(gridNoise, Signal->Blocks[index].fftwValues.size, Signal->AudioChannels, Signal->header.fmt.SamplesPerSec, config);

		scanNoise = CalculateScanRate(Signal)*(double)GetLineCount(Signal->role, config);
		Signal->scanrateFrequency = FindFrequencyBracket(scanNoise, Signal->Blocks[index].fftwValues.size, Signal->AudioChannels, Signal->header.fmt.SamplesPerSec, config);

		crossNoise = scanNoise/2;
		Signal->crossFrequency = FindFrequencyBracket(crossNoise, Signal->Blocks[index].fftwValues.size, Signal->AudioChannels, Signal->header.fmt.SamplesPerSec, config);

		if(config->verbose) {
			logmsg(" - Searching for noise frequencies [%s]: Power grid %g Hz Scan Rate: %g Hz Crossnoise %g Hz\n", 
				Signal->role == ROLE_REF ? "Reference" : "Comparison",
				Signal->gridFrequency, Signal->scanrateFrequency, Signal->crossFrequency);
		}
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

	freq = roundFloat(freq);
	if(Signal->scanrateFrequency == 0 || !Signal->SilenceBinSize)
		return 0;

	if(freq >= Signal->scanrateFrequency - Signal->SilenceBinSize*8 && 
		freq <= Signal->scanrateFrequency + Signal->SilenceBinSize*8)
		return 1;
	return 0;
}

int IsHRefreshNoiseCrossTalk(AudioSignal *Signal, double freq)
{
	if(!Signal)
		return 0;

	freq = roundFloat(freq);
	if(Signal->crossFrequency == 0 || !Signal->SilenceBinSize)
		return 0;

	if(freq >= Signal->crossFrequency - Signal->SilenceBinSize*8 && 
		freq <= Signal->crossFrequency + Signal->SilenceBinSize*8)
		return 1;
	return 0;
}

int IsGridFrequencyNoise(AudioSignal *Signal, double freq)
{
	if(!Signal)
		return 0;

	if(Signal->gridFrequency == 0 || !Signal->SilenceBinSize)
		return 0;

	if(freq == Signal->gridFrequency)
		return 1;
	return 0;
}

int InitAudioBlock(AudioBlocks* block, parameters *config)
{
	if(!block)
		return 0;

	memset(block, 0, sizeof(AudioBlocks));
	block->freq = (Frequency*)malloc(sizeof(Frequency)*config->MaxFreq);
	if(!block->freq)
	{
		logmsg("Not enough memory for Data Structures\n");
		return 0;
	}
	memset(block->freq, 0, sizeof(Frequency)*config->MaxFreq);

#ifdef INDIVPHASE
	block->linFreq = NULL;
	block->linFreqSize = 0;
#endif
	return 1;
}

AudioSignal *CreateAudioSignal(parameters *config)
{
	AudioSignal *Signal = NULL;

	if(!config)
		return NULL;

	if(!config->types.totalBlocks)
		return NULL;

	Signal = (AudioSignal*)malloc(sizeof(AudioSignal));
	if(!Signal)
	{
		logmsg("Not enough memory for Data Structures (AS)\n");
		return NULL;
	}
	memset(Signal, 0, sizeof(AudioSignal));

	Signal->Blocks = (AudioBlocks*)malloc(sizeof(AudioBlocks)*config->types.totalBlocks);
	if(!Signal->Blocks)
	{
		logmsg("Not enough memory for Data Structures (Blocks %d)\n", 
			config->types.totalBlocks);	
		return NULL;
	}
	memset(Signal->Blocks, 0, sizeof(AudioBlocks)*config->types.totalBlocks);

	for(int n = 0; n < config->types.totalBlocks; n++)
	{
		if(!InitAudioBlock(&Signal->Blocks[n], config))
			return NULL;
	}

	InitAudio(Signal, config);
	if(config->clkMeasure)
		InitAudioBlock(&Signal->clkFrequencies, config);
	return Signal;
}

void CleanFrequency(Frequency *freq)
{
	if(!freq)
		return;

	freq->hertz = 0;
	freq->magnitude = 0;
	freq->amplitude = NO_AMPLITUDE;
	freq->phase = 0;
	freq->matched = 0;
}

void CleanFrequenciesInBlock(AudioBlocks * AudioArray,  parameters *config)
{
	if(!AudioArray->freq)
		return;

	for(int i = 0; i < config->MaxFreq; i++)
		CleanFrequency(&AudioArray->freq[i]);
}

void InitAudio(AudioSignal *Signal, parameters *config)
{
	if(!Signal)
		return;

	if(Signal->Blocks)
	{
		for(int n = 0; n < config->types.totalBlocks; n++)
		{
			CleanFrequenciesInBlock(&Signal->Blocks[n], config);
			
			Signal->Blocks[n].fftwValues.spectrum = NULL;
			Signal->Blocks[n].fftwValues.size = 0;
	
			Signal->Blocks[n].audio.samples = NULL;
			Signal->Blocks[n].audio.window_samples = NULL;
			Signal->Blocks[n].audio.size = 0;
			Signal->Blocks[n].audio.difference = 0;

			Signal->Blocks[n].internalSync = NULL;
			Signal->Blocks[n].internalSyncCount = 0;
	
			Signal->Blocks[n].index = GetBlockSubIndex(config, n);
			Signal->Blocks[n].type = ConvertAudioTypeForProcessing(GetBlockType(config, n), config);
			Signal->Blocks[n].frames = GetBlockFrames(config, n);

			Signal->Blocks[n].AverageDifference = 0;
			Signal->Blocks[n].missingPercent = 0;
			Signal->Blocks[n].extraPercent = 0;
		}
	}
	Signal->SourceFile[0] = '\0';
	Signal->AudioChannels = INVALID_CHANNELS;
	Signal->role = NO_ROLE;

	Signal->hasSilenceBlock = 0;
	Signal->floorFreq = 0.0;
	Signal->floorAmplitude = 0.0;	

	Signal->Samples = NULL;
	Signal->framerate = 0.0;

	Signal->startOffset = 0;
	Signal->endOffset = 0;

	memset(&Signal->MaxMagnitude, 0, sizeof(MaxMagn));
	Signal->MinAmplitude = 0;

	Signal->gridFrequency = 0;
	Signal->gridAmplitude = 0;
	Signal->scanrateFrequency = 0;
	Signal->scanrateAmplitude = 0;
	Signal->crossFrequency = 0;
	Signal->crossAmplitude = 0;
	Signal->SilenceBinSize = 0;

	Signal->nyquistLimit = 0;
	Signal->watermarkStatus = WATERMARK_NONE;
	Signal->startHz = config->startHz;
	Signal->endHz = config->endHz;

	Signal->balance = 0;
	memset(&Signal->clkFrequencies, 0, sizeof(AudioBlocks));
	Signal->EstimatedSR = 0;
	Signal->originalSR = 0;
	Signal->originalFrameRate = 0;

	Signal->EstimatedSR_CLK = 0;
	Signal->originalSR_CLK = 0;
	Signal->originalCLK = 0;

	memset(&Signal->delayArray, 0, sizeof(double)*DELAYCOUNT);
	Signal->delayElemCount = 0;

	memset(&Signal->header, 0, sizeof(wav_hdr));
}

int initInternalSync(AudioBlocks * AudioArray, int size)
{
	if(!AudioArray)
		return 0 ;

	if(AudioArray->internalSync)
		return 0;

	AudioArray->internalSync = (BlockSamples*)malloc(sizeof(BlockSamples)*size);
	if(!AudioArray->internalSync)
		return 0;

	for(int i = 0; i < size; i++)
	{
		AudioArray->internalSync[i].samples = NULL;
		AudioArray->internalSync[i].window_samples = NULL;
		AudioArray->internalSync[i].size = 0;
		AudioArray->internalSync[i].difference = 0;
	}

	AudioArray->internalSyncCount = size;
	return 1;
}

void CleanAndReleaseFFTW(AudioBlocks * AudioArray)
{
	if(!AudioArray)
		return;

	ReleaseFFTW(AudioArray);
	AudioArray->fftwValues.size = 0;
}

void ReleaseFFTW(AudioBlocks * AudioArray)
{
	if(!AudioArray)
		return;

	if(AudioArray->fftwValues.spectrum)
	{
		free(AudioArray->fftwValues.spectrum);
		AudioArray->fftwValues.spectrum = NULL;
	}
}

void ReleaseSamples(AudioBlocks * AudioArray)
{
	if(!AudioArray)
		return;

	if(AudioArray->audio.samples)
	{
		free(AudioArray->audio.samples);
		AudioArray->audio.samples = NULL;
	}
	if(AudioArray->audio.window_samples)
	{
		free(AudioArray->audio.window_samples);
		AudioArray->audio.window_samples = NULL;
	}
	AudioArray->audio.size = 0;
	AudioArray->audio.difference = 0;

	if(AudioArray->internalSync)
	{
		for(int i = 0; i < AudioArray->internalSyncCount; i++)
		{
			if(AudioArray->internalSync[i].samples)
			{
				free(AudioArray->internalSync[i].samples);
				AudioArray->internalSync[i].samples = NULL;
			}
			if(AudioArray->internalSync[i].window_samples)
			{
				free(AudioArray->internalSync[i].window_samples);
				AudioArray->internalSync[i].window_samples = NULL;
			}
			AudioArray->internalSync[i].size = 0;
			AudioArray->internalSync[i].difference = 0;
		}
		free(AudioArray->internalSync);
		AudioArray->internalSync = NULL;
	}
	AudioArray->internalSyncCount = 0;
}

void ReleaseFrequencies(AudioBlocks * AudioArray)
{
	if(!AudioArray)
		return;

	if(AudioArray->freq)
	{
		free(AudioArray->freq);
		AudioArray->freq = NULL;
	}

#ifdef INDIVPHASE
	if(AudioArray->linFreq)
	{
		free(AudioArray->linFreq);
		AudioArray->linFreq = NULL;
	}
	AudioArray->linFreqSize = 0;
#endif
}

void ReleaseBlock(AudioBlocks * AudioArray)
{
	ReleaseFrequencies(AudioArray);
	CleanAndReleaseFFTW(AudioArray);
	ReleaseSamples(AudioArray);

	AudioArray->index = 0;
	AudioArray->type = 0;
	AudioArray->seconds = 0;
}

void ReleasePCM(AudioSignal *Signal)
{
	if(!Signal)
		return;

	if(Signal->Samples)
	{
		free(Signal->Samples);
		Signal->Samples = NULL;
	}
}

void ReleaseAudio(AudioSignal *Signal, parameters *config)
{
	if(!Signal)
		return;

	if(Signal->Blocks)
	{
		for(int n = 0; n < config->types.totalBlocks; n++)
			ReleaseBlock(&Signal->Blocks[n]);
	
		free(Signal->Blocks);
		Signal->Blocks = NULL;
	}

	if(config->clkMeasure)
		ReleaseBlock(&Signal->clkFrequencies);
	ReleasePCM(Signal);

	InitAudio(Signal, config);
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
		logmsg("ERROR: Could not load profile configuration file: \"%s\" [errno %d]\n", config->profileFile, errno);
		return 0;
	}
	
	readLine(lineBuffer, file);
	sscanf(lineBuffer, "%s ", buffer);
	if(strcmp(buffer, "MDFourierAudioBlockFile") == 0)
	{
		sscanf(lineBuffer, "%*s %s\n", buffer);
		if(atof(buffer) < PROFILE_VER)
		{
			logmsg("ERROR: Please update your profile files to version %g\n", PROFILE_VER);
			fclose(file);
			return 0;
		}
		if(atof(buffer) > PROFILE_VER)
		{
			logmsg("ERROR: This executable can parse \"MDFourierAudioBlockFile %g\" files only\n", PROFILE_VER);
			fclose(file);
			return 0;
		}
		return(LoadAudioBlockStructure(file, config));
	}

	if(strcmp(buffer, "MDFourierNoSyncProfile") == 0)
	{
		sscanf(lineBuffer, "%*s %s\n", buffer);
		if(atof(buffer) != PROFILE_VER)
		{
			logmsg("ERROR: This executable can parse \"MDFourierNoSyncProfile %g\" files only\n", PROFILE_VER);
			fclose(file);
			return 0;
		}
		return(LoadAudioNoSyncProfile(file, config));
	}

	logmsg("ERROR: Not an MD Fourier Audio Profile File\n");
	fclose(file);

	return 0;
}

void FlattenProfile(parameters *config)
{
	if(!config)
		return;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		int total = 0;

		total = config->types.typeArray[i].elementCount * config->types.typeArray[i].frames;
		config->types.typeArray[i].elementCount = 1;
		config->types.typeArray[i].frames = total;
	}
	config->types.regularBlocks = GetActiveAudioBlocks(config);
	config->types.totalBlocks = GetTotalAudioBlocks(config);
	logmsg("Audio Blocks flattened\n");
}

int CheckSyncFormats(parameters *config)
{
	if(config->noSyncProfile && !config->types.syncCount)
		return 1;

	if(config->videoFormatRef < 0|| config->videoFormatRef >= config->types.syncCount)
	{
		logmsg("\tERROR: Invalid format '%d' for Reference, profile defines %d types\n\t[", 
				config->videoFormatRef, config->types.syncCount);
		for(int s = 0; s < config->types.syncCount; s++)
		{
			logmsg("%d:%s", s, config->types.SyncFormat[s].syncName);
			if(s != config->types.syncCount - 1)
				logmsg(", ");
		}
		logmsg("]\n");
		return 0;
	}

	if(config->videoFormatCom < 0|| config->videoFormatCom >= config->types.syncCount)
	{
		logmsg("\tERROR: Invalid format '%d' for Comparison, profile defines %d types:\n\t[", 
				config->videoFormatCom, config->types.syncCount);
		for(int s = 0; s < config->types.syncCount; s++)
		{
			logmsg("%d:%s", s, config->types.SyncFormat[s].syncName);
			if(s != config->types.syncCount - 1)
				logmsg(", ");
		}
		logmsg("]\n");
		return 0;
	}
	return 1;
}

int EndProfileLoad(parameters *config)
{
	logmsg("* Using profile [%s]\n", config->types.Name);	
	if(config->compressToBlocks)
		FlattenProfile(config);

	if(config->doClkAdjust)
	{
		if(config->clkMeasure)
		{
			logmsg(" - Adjusting CLK rates, align to 1hz enabled (Zero padding)\n");
			config->ZeroPad = 1;
		}
		else
		{
			logmsg(" - Ignoring -j since no CLK rates were found in profile\n");
			config->doClkAdjust = 0;  // no current need to, but anyway...
		}
	}

	CheckSilenceOverride(config);
	PrintAudioBlocks(config);
	if(!CheckSyncFormats(config))
		return 0;
	return 1;
}

int LoadAudioBlockStructure(FILE *file, parameters *config)
{
	int		insideInternal = 0, i = 0;
	char	lineBuffer[LINE_BUFFER_SIZE], tmp = '\0';
	char	buffer[PARAM_BUFFER_SIZE], buffer2[PARAM_BUFFER_SIZE], buffer3[PARAM_BUFFER_SIZE];

	config->noSyncProfile = 0;

	/* Line 2: Profile Name */
	readLine(lineBuffer, file);
	if(sscanf(lineBuffer, "%255[^\n]\n", config->types.Name) != 1)
	{
		logmsg("ERROR: Invalid Name '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}

	/* Line 3: Profile Frame rates numbers */
	readLine(lineBuffer, file);
	sscanf(lineBuffer, "%s\n", buffer);
	config->types.syncCount = atoi(buffer);
	if(!config->types.syncCount)
	{
		logmsg("ERROR: Invalid Sync count '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}
	if(config->types.syncCount > 10)
	{
		logmsg("ERROR: Invalid Sync count '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}
	/* Lines 4 to 4+config->types.syncCount:sync types*/
	for(i = 0; i < config->types.syncCount; i++)
	{
		readLine(lineBuffer, file);
		if(sscanf(lineBuffer, "%255s %s %s %d %d %d\n", 
								config->types.SyncFormat[i].syncName, buffer2, buffer3,
								&config->types.SyncFormat[i].pulseSyncFreq,
								&config->types.SyncFormat[i].pulseFrameLen,
								&config->types.SyncFormat[i].pulseCount) != 6)
		{
			logmsg("ERROR: Invalid Frame Rate Adjustment '%s'\n", lineBuffer);
			fclose(file);
			return 0;
		}
		config->types.SyncFormat[i].MSPerFrame = strtod(buffer2, NULL);
		if(!config->types.SyncFormat[i].MSPerFrame)
		{
			logmsg("ERROR: Invalid MS per frame  Adjustment '%s'\n", lineBuffer);
			fclose(file);
			return 0;
		}
		config->types.SyncFormat[i].LineCount = strtod(buffer3, NULL);
		if(config->types.SyncFormat[i].LineCount < 0)
		{
			logmsg("ERROR: Invalid line count Adjustment '%s'\n", lineBuffer);
			fclose(file);
			return 0;
		}
	
		if(!config->types.SyncFormat[i].pulseSyncFreq)
		{
			logmsg("ERROR: Invalid Pulse Sync Frequency:\n%s\n", lineBuffer);
			fclose(file);
			return 0;
		}
	
		if(!config->types.SyncFormat[i].pulseFrameLen)
		{
			logmsg("ERROR: Invalid Pulse Length:\n%s\n", lineBuffer);
			fclose(file);
			return 0;
		}
	
		if(!config->types.SyncFormat[i].pulseCount)
		{
			logmsg("ERROR: Invalid Pulse Count value:\n%s\n", lineBuffer);
			fclose(file);
			return 0;
		}
	}

	/* CLK estimation */
	readLine(lineBuffer, file);
	if(sscanf(lineBuffer, "%s %c", config->clkName, &tmp) != 2)
	{
		logmsg("ERROR: Invalid MD Fourier Audio Blocks File (CLK): %s\n", lineBuffer);
		fclose(file);
		return 0;
	}
	config->clkMeasure = (tmp == 'y');
	if(config->clkMeasure)
	{
		if(sscanf(lineBuffer, "%*s %*c %d %d %d\n", 
			&config->clkBlock,
			&config->clkFreq,
			&config->clkRatio) != 3)
		{
			logmsg("ERROR: Invalid MD Fourier Audio Blocks File (CLK): %s\n", lineBuffer);
			fclose(file);
			return 0;
		}

		if(config->clkBlock <= 0 || config->clkFreq <= 0 || config->clkRatio <= 0)
		{
			logmsg("ERROR: Invalid MD Fourier Audio Blocks File (CLK): %s\n", lineBuffer);
			fclose(file);
			return 0;
		}
	}

	/* Type count */
	readLine(lineBuffer, file);
	sscanf(lineBuffer, "%s\n", buffer);
	config->types.typeCount = atoi(buffer);
	if(!config->types.typeCount)
	{
		logmsg("ERROR: Invalid type count '%s'\n", buffer);
		fclose(file);
		return 0;
	}
	config->types.typeArray = (AudioBlockType*)malloc(sizeof(AudioBlockType)*config->types.typeCount);
	if(!config->types.typeArray)
	{
		logmsg("ERROR: Not enough memory\n");
		fclose(file);
		return 0;
	}
	memset(config->types.typeArray, 0, sizeof(AudioBlockType)*config->types.typeCount);

	/* types */
	for(int i = 0; i < config->types.typeCount; i++)
	{
		char type = 0;

		readLine(lineBuffer, file);
		if(sscanf(lineBuffer, "%128s ", config->types.typeArray[i].typeName) != 1)
		{
			logmsg("ERROR: Invalid Block Name %s\n", config->types.typeArray[i].typeName);
			fclose(file);
			return 0;
		}
		CleanName(config->types.typeArray[i].typeName, config->types.typeArray[i].typeDisplayName);

		if(sscanf(lineBuffer, "%*s %c ", &type) != 1)
		{
			logmsg("ERROR: Invalid Block Type %c\n", type);
			fclose(file);
			return 0;
		}

		switch(type)
		{
			case TYPE_SILENCE_C:
				config->types.typeArray[i].type = TYPE_SILENCE;
				break;
			case TYPE_SYNC_C:
				config->types.typeArray[i].type = TYPE_SYNC;
				if(config->debugSync)
					config->hasTimeDomain++;
				break;
			case TYPE_INTERNAL_KNOWN_C:
				config->types.typeArray[i].type = TYPE_INTERNAL_KNOWN;
				break;
			case TYPE_INTERNAL_UNKNOWN_C:
				config->types.typeArray[i].type = TYPE_INTERNAL_UNKNOWN;
				break;
			case TYPE_SKIP_C:
				config->types.typeArray[i].type = TYPE_SKIP;
				break;
			case TYPE_TIMEDOMAIN_C:
				config->types.typeArray[i].type = TYPE_TIMEDOMAIN;
				config->hasTimeDomain++;
				break;
			case TYPE_SILENCE_OVER_C:
				config->types.typeArray[i].type = TYPE_SILENCE_OVERRIDE;
				break;
			case TYPE_WATERMARK_C:
				config->types.typeArray[i].type = TYPE_WATERMARK;
				config->types.useWatermark = 1;
				break;
			default:
				if(sscanf(lineBuffer, "%*s %d ", &config->types.typeArray[i].type) != 1)
				{
					logmsg("ERROR: Invalid MD Fourier Block ID\n%s\n", lineBuffer);
					fclose(file);
					return 0;
				}
				break;
		}
		
		if(config->types.typeArray[i].type == TYPE_INTERNAL_KNOWN ||
			config->types.typeArray[i].type == TYPE_INTERNAL_UNKNOWN)
		{
			if(insideInternal)
				insideInternal = 0;
			else
				insideInternal = 1;

			if(sscanf(lineBuffer, "%*s %*s %d %d %20s %c %d %lf\n", 
				&config->types.typeArray[i].elementCount,
				&config->types.typeArray[i].frames,
				&config->types.typeArray[i].color[0],
				&config->types.typeArray[i].channel,
				&config->types.typeArray[i].syncTone,
				&config->types.typeArray[i].syncLen) != 6)
			{
				logmsg("ERROR: Invalid MD Fourier Audio Blocks File (Element Count, frames, color, channel): %s\n", lineBuffer);
				fclose(file);
				return 0;
			}
		}
		else if(config->types.typeArray[i].type == TYPE_WATERMARK)
		{
			if(sscanf(lineBuffer, "%*s %*s %d %d %20s %c %d %d %128s\n", 
				&config->types.typeArray[i].elementCount,
				&config->types.typeArray[i].frames,
				&config->types.typeArray[i].color[0],
				&config->types.typeArray[i].channel,
				&config->types.watermarkValidFreq,
				&config->types.watermarkInvalidFreq,
				config->types.watermarkDisplayName) != 7)
			{
				logmsg("ERROR: Invalid MD Fourier Audio Blocks File (Element Count, frames, color, channel, WMValid, WMFail, Name): %s\n", lineBuffer);
				fclose(file);
				return 0;
			}

			if(!config->types.watermarkValidFreq || !config->types.watermarkInvalidFreq)
			{
				logmsg("ERROR: Invalid Watermark values: %s\n", lineBuffer);
				return 0;
			}
		}
		else
		{
			if(sscanf(lineBuffer, "%*s %*s %d %d %d %20s %c\n", 
				&config->types.typeArray[i].elementCount,
				&config->types.typeArray[i].frames,
				&config->types.typeArray[i].cutFrames,
				&config->types.typeArray[i].color[0],
				&config->types.typeArray[i].channel) != 5)
			{
				logmsg("ERROR: Invalid MD Fourier Audio Blocks File (Element Count, frames, skip, color, channel): %s\n", lineBuffer);
				fclose(file);
				return 0;
			}

			if(config->types.typeArray[i].cutFrames != 0 && config->types.typeArray[i].frames - fabs(config->types.typeArray[i].cutFrames) <= 0)
			{
				logmsg("ERROR: Invalid MD Fourier Audio Blocks File: %s, Skip bigger than element\n", lineBuffer);
				fclose(file);
				return 0;
			}
			config->types.typeArray[i].cutFrames = fabs(config->types.typeArray[i].cutFrames);
		}

		if(!config->types.typeArray[i].elementCount)
		{
			logmsg("ERROR: Element Count must have a value > 0\n%s\n", lineBuffer);
			fclose(file);
			return 0;
		}

		if(!config->types.typeArray[i].frames)
		{
			logmsg("ERROR: Frames must have a value > 0\n%s\n", lineBuffer);
			fclose(file);
			return 0;
		}
	
		if(MatchColor(config->types.typeArray[i].color) == COLOR_NONE)
		{
			logmsg("ERROR: Unrecognized color \"%s\" aborting\n", 
				config->types.typeArray[i].color);
			fclose(file);
			return 0;
		}

		config->types.typeArray[i].IsaddOnData = MatchesPreviousType(i, config->types.typeArray[i].type, config);
		// make silent if duplicate and not silence block
		if(!config->useExtraData && config->types.typeArray[i].IsaddOnData)
		{
			if(config->types.typeArray[i].type != TYPE_SILENCE)
				config->types.typeArray[i].type = TYPE_TIMEDOMAIN;  // TYPE_SKIP
		}
		if(config->useExtraData && config->types.typeArray[i].IsaddOnData)
			config->hasAddOnData ++;
	}

	if(insideInternal)
	{
		logmsg("ERROR: Internal sync detection block didn't have a closing section\n");
		return 0;
	}

	config->types.regularBlocks = GetActiveAudioBlocks(config);
	config->types.totalBlocks = GetTotalAudioBlocks(config);
	if(!config->types.totalBlocks)
	{
		logmsg("ERROR: Total Audio Blocks should be at least 1\n");
		return 0;
	}

	fclose(file);

	return 1;
}

int LoadAudioNoSyncProfile(FILE *file, parameters *config)
{
	char	insideInternal = 0, type = '\0';
	char	lineBuffer[LINE_BUFFER_SIZE];
	char	buffer[PARAM_BUFFER_SIZE];

	config->noSyncProfile = 1;
	if(config->plotDifferences)
		config->averagePlot = 1;

	readLine(lineBuffer, file);
	if(sscanf(lineBuffer, "%255[^\n]\n", config->types.Name) != 1)
	{
		logmsg("ERROR: Invalid Name '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}

	readLine(lineBuffer, file);
	if(sscanf(lineBuffer, "%s\n", buffer) != 1)
	{
		logmsg("ERROR: Invalid Reference Frame Rate Adjustment '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}
	config->types.SyncFormat[0].MSPerFrame = strtod(buffer, NULL);
	if(!config->types.SyncFormat[0].MSPerFrame)
	{
		logmsg("ERROR: Invalid Reference Frame Rate Adjustment '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}

	readLine(lineBuffer, file);
	if(sscanf(lineBuffer, "%s\n", buffer) != 1)
	{
		logmsg("ERROR: Invalid Comparison Frame Rate Adjustment '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}
	config->types.SyncFormat[1].MSPerFrame = strtod(buffer, NULL);
	if(!config->types.SyncFormat[1].MSPerFrame)
	{
		logmsg("ERROR: Invalid Comparison Frame Rate Adjustment '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}

	/* Read type of Sync */

	readLine(lineBuffer, file);
	sscanf(lineBuffer, "%c\n", &type);

	switch(type)
	{
		case NO_SYNC_AUTO_C:
			config->noSyncProfileType = NO_SYNC_AUTO;
			break;
		case NO_SYNC_MANUAL_C:
			config->noSyncProfileType = NO_SYNC_MANUAL;
			break;
		default:
			logmsg("ERROR: Invalid Free profile type '%c'. Use '%c' or '%c'\n", 
				type, NO_SYNC_AUTO_C, NO_SYNC_MANUAL_C);
			fclose(file);
			return 0;
			break;
	}
	/*  Read blocks */
	readLine(lineBuffer, file);
	sscanf(lineBuffer, "%s\n", buffer);
	config->types.typeCount = atoi(buffer);
	if(!config->types.typeCount)
	{
		logmsg("ERROR: Invalid type count:\n'%s'\n", buffer);
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
		if(sscanf(lineBuffer, "%128s ", config->types.typeArray[i].typeName) != 1)
		{
			logmsg("ERROR: Invalid Block Name\n%s\n", lineBuffer);
			fclose(file);
			return 0;
		}
		CleanName(config->types.typeArray[i].typeName, config->types.typeArray[i].typeDisplayName);

		if(sscanf(lineBuffer, "%*s %c ", &type) != 1)
		{
			logmsg("ERROR: Invalid Block Type %s\n", lineBuffer);
			fclose(file);
			return 0;
		}

		switch(type)
		{
			case TYPE_SILENCE_C:
				config->types.typeArray[i].type = TYPE_SILENCE;
				break;
			case TYPE_INTERNAL_KNOWN_C:
				config->types.typeArray[i].type = TYPE_INTERNAL_KNOWN;
				break;
			case TYPE_INTERNAL_UNKNOWN_C:
				config->types.typeArray[i].type = TYPE_INTERNAL_UNKNOWN;
				break;
			case TYPE_SKIP_C:
				config->types.typeArray[i].type = TYPE_SKIP;
				break;
			case TYPE_TIMEDOMAIN_C:
				config->types.typeArray[i].type = TYPE_TIMEDOMAIN;
				config->hasTimeDomain++;
				break;
			case TYPE_SILENCE_OVER_C:
				config->types.typeArray[i].type = TYPE_SILENCE_OVERRIDE;
				break;
			case TYPE_WATERMARK_C:
				config->types.typeArray[i].type = TYPE_WATERMARK;
				config->types.useWatermark = 1;
				break;
			default:
				if(sscanf(lineBuffer, "%*s %d ", &config->types.typeArray[i].type) != 1)
				{
					logmsg("ERROR: Invalid MD Fourier Block ID\n", config->types.typeArray[i].type);
					fclose(file);
					return 0;
				}
				break;
		}
		
		if(config->types.typeArray[i].type == TYPE_INTERNAL_KNOWN ||
			config->types.typeArray[i].type == TYPE_INTERNAL_UNKNOWN)
		{
			if(insideInternal)
				insideInternal = 0;
			else
				insideInternal = 1;

			if(sscanf(lineBuffer, "%*s %*s %d %d %20s %c %d %lf\n", 
				&config->types.typeArray[i].elementCount,
				&config->types.typeArray[i].frames,
				&config->types.typeArray[i].color[0],
				&config->types.typeArray[i].channel,
				&config->types.typeArray[i].syncTone,
				&config->types.typeArray[i].syncLen) != 6)
			{
				logmsg("ERROR: Invalid MD Fourier Audio Blocks File (Element Count, frames, color, channel): %s\n", lineBuffer);
				fclose(file);
				return 0;
			}
		}
		else if(config->types.typeArray[i].type == TYPE_WATERMARK)
		{
			if(sscanf(lineBuffer, "%*s %*s %d %d %20s %c %d %d %128s\n", 
				&config->types.typeArray[i].elementCount,
				&config->types.typeArray[i].frames,
				&config->types.typeArray[i].color[0],
				&config->types.typeArray[i].channel,
				&config->types.watermarkValidFreq,
				&config->types.watermarkInvalidFreq,
				config->types.watermarkDisplayName) != 7)
			{
				logmsg("ERROR: Invalid MD Fourier Audio Blocks File (Element Count, frames, color, channel, WMValid, WMFail, Name): %s\n", lineBuffer);
				fclose(file);
				return 0;
			}

			if(!config->types.watermarkValidFreq || !config->types.watermarkInvalidFreq)
			{
				logmsg("ERROR: Invalid Watermark values: %s\n", lineBuffer);
				return 0;
			}
		}
		else
		{
			if(sscanf(lineBuffer, "%*s %*s %d %d %s %c\n", 
				&config->types.typeArray[i].elementCount,
				&config->types.typeArray[i].frames,
				&config->types.typeArray[i].color [0],
				&config->types.typeArray[i].channel) != 4)
			{
				logmsg("ERROR: Invalid MD Fourier Audio Blocks File (Element Count, frames, color, channel): %s\n", lineBuffer);
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

		config->types.typeArray[i].IsaddOnData = MatchesPreviousType(i, config->types.typeArray[i].type, config);
		// make silent if duplicate and not silence block
		if(!config->useExtraData && config->types.typeArray[i].IsaddOnData)
		{
			if(config->types.typeArray[i].type != TYPE_SILENCE)
				config->types.typeArray[i].type = TYPE_TIMEDOMAIN;  // TYPE_SKIP
		}
		if(config->useExtraData && config->types.typeArray[i].IsaddOnData)
			config->hasAddOnData ++;
	}

	if(insideInternal)
	{
		logmsg("ERROR: Internal sync detection block didn't have a closing section\n");
		return 0;
	}

	config->types.regularBlocks = GetActiveAudioBlocks(config);
	config->types.totalBlocks = GetTotalAudioBlocks(config);
	if(!config->types.totalBlocks)
	{
		logmsg("Total Audio Blocks should be at least 1\n");
		return 0;
	}

	config->significantAmplitude = NS_SIGNIFICANT_VOLUME;
	fclose(file);
	
	return 1;
}

void PrintAudioBlocks(parameters *config)
{
	long int frames = 0;
	double TotalSeconds = 0;

	if(!config)
		return;

	logmsgFileOnly("\n======== PROFILE ========\n");
	for(int i = 0; i < config->types.typeCount; i++)
	{
		char	type[5], t;
		double	seconds =	 0, StartSeconds = 0;

		t = GetTypeProfileName(config->types.typeArray[i].type);
		if(t == TYPE_NULLTYPE_C)
			sprintf(type, "%d", config->types.typeArray[i].type);
		else
			sprintf(type, "%c", t);

		StartSeconds = TotalSeconds;
		seconds = FramesToSeconds(config->types.typeArray[i].frames, config->types.SyncFormat[0].MSPerFrame);
		seconds *= config->types.typeArray[i].elementCount;
		TotalSeconds += seconds;

		logmsgFileOnly("%s%s %s %d %d %d %s %c %s | Frames: %ld | Seconds: %g [%g to %g]\n", 
			config->types.typeArray[i].type == TYPE_SKIP ? "     " : "",
			config->types.typeArray[i].typeName,
			type,
			config->types.typeArray[i].elementCount,
			config->types.typeArray[i].frames,
			config->types.typeArray[i].cutFrames*-1,
			config->types.typeArray[i].color,
			config->types.typeArray[i].channel,
			config->types.typeArray[i].IsaddOnData ? "(ExtraData)" : "",
			config->types.typeArray[i].elementCount*config->types.typeArray[i].frames,
			seconds, 
			StartSeconds,
			TotalSeconds);
		frames += config->types.typeArray[i].elementCount*config->types.typeArray[i].frames;
	}
	logmsgFileOnly("Total frames: %ld\n================\n", frames);
}

int CalculateTimeDurations(AudioSignal *Signal, parameters *config)
{
	if(!Signal)
		return 0;

	if(!Signal->Blocks)
		return 0;

	for(int n = 0; n < config->types.totalBlocks; n++)
		Signal->Blocks[n].seconds = Signal->Blocks[n].frames*GetMSPerFrame(Signal, config)/1000.0;

	return 1;
}

double GetMSPerFrame(AudioSignal *Signal, parameters *config)
{
	if(!config)
		return 0;
	if(!Signal)
	{
		logmsg("ERROR: Null Signal for GetMSPerFrame\n");
		return 0;
	}
	return(roundFloat(getMSPerFrameInternal(Signal->role, config)));
}

double GetMSPerFrameRole(int role, parameters *config)
{
	if(!config)
		return 0;

	return(roundFloat(getMSPerFrameInternal(role, config)));
}

double GetLowerFrameRate(double framerateA, double framerateB)
{
	if(framerateA > framerateB)
		return framerateB;

	return framerateA;
}

void CompareFrameRates(AudioSignal *Signal1, AudioSignal *Signal2, parameters *config)
{
	double diff = 0;

	diff = fabs(Signal1->framerate - Signal2->framerate);
	if(diff == 0.0)
		config->smallerFramerate = Signal1->framerate;
	else
	{
		config->smallerFramerate =  GetLowerFrameRate(Signal1->framerate, Signal2->framerate);
		if((config->verbose && diff > 0.001) || diff > 0.1) {
			logmsg("\n= Different frame rates found (%g), compensating to %g =\n", 
				diff, config->smallerFramerate);
		}

		/*
		if(diff <= 0.0004) // this is probably a sync detection error
		{
			double expected = 0;

			expected = GetMSPerFrame(Signal1, config);
			diff = fabs(Signal1->framerate - expected);
			if(diff <= 0.0005)
				config->smallerFramerate = expected;

			Signal1->framerate = config->smallerFramerate;
			Signal2->framerate = config->smallerFramerate;
			logmsg(" - Analysis will be done as %g ms per frame\n", config->smallerFramerate);
		}
		*/
	}
}

void CompareFrameRatesMDW(AudioSignal *Signal, double framerate, parameters *config)
{
	double diff = 0;

	diff = fabs(Signal->framerate - framerate);
	if(diff == 0.0)
		config->smallerFramerate = framerate;
	else
	{
		config->smallerFramerate =  GetLowerFrameRate(Signal->framerate, framerate);
		if(config->verbose && diff > 0.001) {
			logmsg("\n= Different frame rates found (%g), compensating to %g =\n", 
				diff, config->smallerFramerate);
		}
		if(diff <= 0.0005) // this is a sync detection error
			Signal->framerate = framerate;
	}
}

long int GetByteSizeDifferenceByFrameRate(double framerate, long int frames, long int samplerate, int AudioChannels, parameters *config)
{
	long int difference = 0;

	if(config->smallerFramerate == 0)
		return 0;

	if(framerate > config->smallerFramerate)
	{
		long int SmallerBytes = 0;
		long int BiggerBytes = 0;

		SmallerBytes = SecondsToBytes(samplerate, FramesToSeconds(config->smallerFramerate, frames), AudioChannels, NULL, NULL, NULL);
		BiggerBytes = SecondsToBytes(samplerate, FramesToSeconds(framerate, frames), AudioChannels, NULL, NULL, NULL);
	
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

int MatchesPreviousType(int pos, int type, parameters *config)
{
	int count = 0;

	if(!config)
		return 0;

	if(type <= TYPE_CONTROL)
		return 0;

	for(int i = 0; i < pos; i++)
	{
		if(config->types.typeArray[i].type == type)
			count++;
	}
	
	if(count)
		return 1;
	return 0;
}

void CheckSilenceOverride(parameters *config)
{
	int count = 0;

	if(!config)
		return;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type == TYPE_SILENCE_OVERRIDE)
			count++;
	}
	
	if(!count)  // Regular profile, use silence padding for noise floor analyis
		return;

	config->hasSilenceOverRide = 1;
}

int ConvertAudioTypeForProcessing(int type, parameters *config)
{
	if(config->hasSilenceOverRide)
	{
		if(type == TYPE_SILENCE)
			return TYPE_SKIP;
	
		if(type == TYPE_SILENCE_OVERRIDE)
			return TYPE_SILENCE;
	}

	return type;
}

// Must be called after sync has been detected
void SelectSilenceProfile(parameters *config)
{
	if(!config)
		return;

	if(!config->hasSilenceOverRide)
		return;

	// Convert silence padding to skip blocks
	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type == TYPE_SILENCE)
			config->types.typeArray[i].type = TYPE_SKIP;
	}

	// Convert Silence Overrride to silence blocks
	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type == TYPE_SILENCE_OVERRIDE)
			config->types.typeArray[i].type = TYPE_SILENCE;
	}
}

int DetectWatermark(AudioSignal *Signal, parameters *config)
{
	int watermark = -1, found = 0;
	double	WaterMarkValid = 0, WaterMarkInvalid = 0;

	if(!config || !Signal)
		return 0;

	if(!config->types.useWatermark)
	{
		logmsg("ERROR: Watermark expected but not defined\n");
		return 0;
	}

	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		if(GetBlockType(config, block) == TYPE_WATERMARK)
		{
			watermark = block;
			break;
		}
	}

	if(watermark == -1)
	{
		Signal->watermarkStatus = WATERMARK_NONE;
		logmsg("ERROR: Watermark expected but not found\n");
		return 0;
	}

	if(!Signal->Blocks[watermark].fftwValues.size)
	{
		logmsg("ERROR: Invalid FFTW size for Watermark detection\n");
		return 0;
	}

	if(!config->types.watermarkValidFreq || !config->types.watermarkInvalidFreq)
	{
		logmsg("ERROR: Invalid Watermark values\n");
		return 0;
	}

	WaterMarkValid = FindFrequencyBracket(config->types.watermarkValidFreq, Signal->Blocks[watermark].fftwValues.size, 
					Signal->header.fmt.NumOfChan, Signal->header.fmt.SamplesPerSec, config);
	WaterMarkInvalid = FindFrequencyBracket(config->types.watermarkInvalidFreq, Signal->Blocks[watermark].fftwValues.size, 
					Signal->header.fmt.NumOfChan, Signal->header.fmt.SamplesPerSec, config);

	for(int i = 0; i < 5; i++)
	{
		if(Signal->Blocks[watermark].freq[i].hertz &&
			Signal->Blocks[watermark].freq[i].amplitude > config->significantAmplitude/2)
		{
			if(fabs(Signal->Blocks[watermark].freq[i].hertz - WaterMarkValid) < 10)
			{
				Signal->watermarkStatus = WATERMARK_VALID;
				found = 1;
				break;
			}
			if(fabs(Signal->Blocks[watermark].freq[i].hertz - WaterMarkInvalid) < 10)
			{
				Signal->watermarkStatus = WATERMARK_INVALID;
				logmsg(" - WARNING: %s signal was recorded with %s difference. Results are probably incorrect.\n",
						Signal->role == ROLE_REF ? "Reference" : "Comparison", config->types.watermarkDisplayName);
				found = 1;
				break;
			}
		}
	}
	if(!found)
	{
		Signal->watermarkStatus = WATERMARK_INDETERMINATE;
		logmsg(" - WARNING: %s file has an unknown %s status. Results might be incorrect.\n",
				Signal->role == ROLE_REF ? "Reference" : "Comparison", config->types.watermarkDisplayName);
	}

	return 1;
}

int DetectWatermarkIssue(char *msg, parameters *config)
{
	int wmRef = WATERMARK_NONE, wmComp = WATERMARK_NONE;

	if(!config || !config->referenceSignal || !config->comparisonSignal)
	{
		sprintf(msg, "ERROR: Invalid watermark parameters!");
		return 1;
	}

	wmRef = config->referenceSignal->watermarkStatus;
	wmComp = config->comparisonSignal->watermarkStatus;
	if(wmRef == WATERMARK_NONE && wmComp == WATERMARK_NONE)
		return 0;
	if(wmRef == WATERMARK_VALID && wmComp == WATERMARK_VALID)
		return 0;

	if(wmRef == WATERMARK_INVALID || wmComp == WATERMARK_INVALID)
	{
		sprintf(msg, "WARNING: Invalid %s. Results are probably incorrect", config->types.watermarkDisplayName);
		return 1;
	}

	if(wmRef == WATERMARK_INDETERMINATE || wmComp == WATERMARK_INDETERMINATE)
	{
		sprintf(msg, "WARNING: %s state unknown. Results might be incorrect", config->types.watermarkDisplayName);
		return 1;
	}
		
	return 0;
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

double GetLastSyncDuration(double framerate, parameters *config)
{
	int		first = 0;

	if(!config)
		return NO_INDEX;

	first = GetFirstSyncIndex(config);
	for(int i = config->types.typeCount - 1; i > first; i--)
	{
		if(config->types.typeArray[i].type == TYPE_SYNC)
		{
			double	frames = 0;

			frames = config->types.typeArray[i].elementCount * config->types.typeArray[i].frames;
			return(FramesToSeconds(frames, framerate));
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
			config->types.typeArray[i].channel == CHANNEL_MONO)
			return index;
		else
			index += config->types.typeArray[i].elementCount;
	}
	return NO_INDEX;
}

long int GetLastSilenceByteOffset(double framerate, wav_hdr header, int frameAdjust, double silenceOffset, parameters *config)
{
	if(!config)
		return NO_INDEX;

	for(int i = config->types.typeCount - 1; i >= 0; i--)
	{
		if(config->types.typeArray[i].type == TYPE_SILENCE)
		{
			double offset = 0, length = 0;

			offset = FramesToSeconds(GetBlockFrameOffset(i, config) - frameAdjust, framerate);
			offset = SecondsToBytes(header.fmt.SamplesPerSec, offset, header.fmt.NumOfChan, NULL, NULL, NULL);

			length = FramesToSeconds(config->types.typeArray[i].frames*silenceOffset, framerate);
			length = SecondsToBytes(header.fmt.SamplesPerSec, length, header.fmt.NumOfChan, NULL, NULL, NULL);
			offset += length;
			return(offset);
		}
	}
	return 0;
}

long int GetSecondSilenceByteOffset(double framerate, wav_hdr header, int frameAdjust, double silenceOffset, parameters *config)
{
	int silence_count = 0, i = 0;

	if(!config)
		return NO_INDEX;

	for(i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type == TYPE_SILENCE)
			silence_count ++;
		if(silence_count == 2)
		{
			double offset = 0, length = 0;

			offset = FramesToSeconds(GetBlockFrameOffset(i, config) - frameAdjust, framerate);
			offset = SecondsToBytes(header.fmt.SamplesPerSec, offset, header.fmt.NumOfChan, NULL, NULL, NULL);

			length = FramesToSeconds(config->types.typeArray[i].frames*silenceOffset, framerate);
			length = SecondsToBytes(header.fmt.SamplesPerSec, length, header.fmt.NumOfChan, NULL, NULL, NULL);
			offset += length;
			return(offset);
		}
	}
	return 0;
}

long int GetSecondSyncSilenceByteOffset(double framerate, wav_hdr header, int frameAdjust, double silenceOffset, parameters *config)
{
	int silence_count = 0, i = 0;

	if(!config)
		return NO_INDEX;

	for(i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type == TYPE_SILENCE)
		{
			if(!silence_count && i > 0 && config->types.typeArray[i-1].type == TYPE_SYNC)
				silence_count ++;
			else
			{
				if(i + 1 < config->types.typeCount && config->types.typeArray[i+1].type == TYPE_SYNC)
					silence_count ++;
			}
		}
		if(silence_count == 2)
		{
			double offset = 0, length = 0;

			offset = FramesToSeconds(GetBlockFrameOffset(i, config) - frameAdjust, framerate);
			offset = SecondsToBytes(header.fmt.SamplesPerSec, offset, header.fmt.NumOfChan, NULL, NULL, NULL);

			length = FramesToSeconds(config->types.typeArray[i].frames*silenceOffset, framerate);
			length = SecondsToBytes(header.fmt.SamplesPerSec, length, header.fmt.NumOfChan, NULL, NULL, NULL);
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

int GetActiveBlockTypesNoRepeat(parameters *config)
{
	int count = 0;

	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type > TYPE_CONTROL && !config->types.typeArray[i].IsaddOnData)
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

long int GetBlockCutFrames(parameters *config, int pos)
{
	int elementsCounted = 0;

	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		elementsCounted += config->types.typeArray[i].elementCount;
		if(elementsCounted > pos)
			return(config->types.typeArray[i].cutFrames);
	}
	
	return 0;
}

int GetBlockElements(parameters *config, int pos)
{
	int elementsCounted = 0;

	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		elementsCounted += config->types.typeArray[i].elementCount;
		if(elementsCounted > pos)
			return(config->types.typeArray[i].elementCount);
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

char GetBlockChannel(parameters *config, int pos)
{
	int elementsCounted = 0;

	if(!config)
		return CHANNEL_NONE;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		elementsCounted += config->types.typeArray[i].elementCount;
		if(elementsCounted > pos)
			return(config->types.typeArray[i].channel);
	}
	
	return CHANNEL_NONE;
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

char *GetTypeDisplayName(parameters *config, int type)
{
	if(!config)
		return "nconfig";

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type == type)
			return(config->types.typeArray[i].typeDisplayName);
	}
	
	return "Type Name";
}

char GetTypeChannel(parameters *config, int type)
{
	if(!config)
		return CHANNEL_NONE;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type == type)
			return(config->types.typeArray[i].channel);
	}
	
	return CHANNEL_NONE;
}

char *GetInternalSyncSequentialName(int sequence, parameters *config)
{
	int found = 0;

	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type == TYPE_INTERNAL_KNOWN ||
			 config->types.typeArray[i].type == TYPE_INTERNAL_UNKNOWN)
		{
			if(found == sequence*2)
				return(config->types.typeArray[i].typeDisplayName);
			found++;
		}
	}
	
	logmsg("WARNING: sync tone request for invalid sequence %d\n", sequence);
	return "";
}

int GetInternalSyncTone(int pos, parameters *config)
{
	int elementsCounted = 0;

	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		elementsCounted += config->types.typeArray[i].elementCount;
		if(elementsCounted > pos && 
			(config->types.typeArray[i].type == TYPE_INTERNAL_KNOWN ||
			 config->types.typeArray[i].type == TYPE_INTERNAL_UNKNOWN))
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
		if(elementsCounted > pos && 
			(config->types.typeArray[i].type == TYPE_INTERNAL_KNOWN ||
			config->types.typeArray[i].type == TYPE_INTERNAL_UNKNOWN))
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
			if(config->types.typeArray[i].type == TYPE_INTERNAL_KNOWN ||
				config->types.typeArray[i].type == TYPE_INTERNAL_UNKNOWN)
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

Frequency FindNoiseBlockInsideOneStandardDeviation(AudioSignal *Signal, parameters *config)
{
	Frequency	cutOff, mean, sd;
	int 		noiseBlock = -1;
	double		count = 0, outside = 0;

	CleanFrequency(&cutOff);
	cutOff.hertz = 0;
	cutOff.amplitude = 0;

	CleanFrequency(&mean);
	mean.hertz = 0;
	mean.amplitude = 0;

	CleanFrequency(&sd);
	sd.hertz = 0;
	sd.amplitude = 0;

	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int type = GetBlockType(config, block);
		if(GetTypeChannel(config, type) == CHANNEL_NOISE)
		{
			noiseBlock = block;
			break;
		}
	}

	if(noiseBlock == -1)
		return cutOff;

	// Calculate Mean
	for(int i = 0; i < config->MaxFreq; i++)
	{
		if(Signal->Blocks[noiseBlock].freq[i].hertz)
		{
			double hz, amp;

			hz = Signal->Blocks[noiseBlock].freq[i].hertz;
			amp = fabs(Signal->Blocks[noiseBlock].freq[i].amplitude);

			mean.hertz += hz;
			mean.amplitude += amp;

			count ++;
		}
	}

	if(!count)
		return cutOff;

	mean.hertz = mean.hertz/count;
	mean.amplitude = mean.amplitude/count;

	count = 0;
	// Calculate StandardDeviation
	for(int i = 0; i < config->MaxFreq; i++)
	{
		if(Signal->Blocks[noiseBlock].freq[i].hertz)
		{
			double hz, amp;

			hz = Signal->Blocks[noiseBlock].freq[i].hertz;
			amp = fabs(Signal->Blocks[noiseBlock].freq[i].amplitude);

			sd.hertz += pow(hz - mean.hertz, 2);
			sd.amplitude += pow(amp - mean.amplitude, 2);

			count ++;
		}
	}
	
	if(!count)
		return cutOff;

	sd.hertz = sqrt(sd.hertz/count);
	sd.amplitude = sqrt(sd.amplitude/count);

	cutOff.hertz = mean.hertz+sd.hertz;
	cutOff.amplitude = -1.0*(mean.amplitude+sd.amplitude);

	if(config->verbose) {
		logmsg("  - %s signal profile defined noise channel data:\n", 
			Signal->role == ROLE_REF ? "Reference" : "Comparison");
		logmsg("      Standard deviation: %g dBFS [%g Hz] Mean: %g dBFS [%g Hz] Cutoff: %g dBFS [%g Hz]\n",
			sd.amplitude, sd.hertz,
			mean.amplitude, mean.hertz,
			cutOff.amplitude, cutOff.hertz);
	}

	for(int i = 0; i < config->MaxFreq; i++)
	{
		if(Signal->Blocks[noiseBlock].freq[i].hertz)
		{
			double amp;

			amp = Signal->Blocks[noiseBlock].freq[i].amplitude;

			if(amp <= cutOff.amplitude)
				outside ++;
		}
	}

	if(config->verbose) {
		logmsg("  - Using %g would leave %g%% data out\n", 
				cutOff.amplitude, outside/count*100);
	}
	return cutOff;
}

void FindStandAloneFloor(AudioSignal *Signal, parameters *config)
{
	int 		Silentindex;
	Frequency	loudest;
	double		maxMagnitude = 0;

	if(!Signal)
		return;
	
	Silentindex = GetFirstSilenceIndex(config);
	if(config->noSyncProfile || Silentindex == NO_INDEX)
	{
		logmsg("There is no Silence block defined in the current profile\n");
		return;
	}

	CleanFrequency(&loudest);

	// Find global peak
	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(type > TYPE_CONTROL && Signal->Blocks[block].freq[0].hertz != 0)
		{
			if(Signal->Blocks[block].freq[0].magnitude > maxMagnitude)
				maxMagnitude = Signal->Blocks[block].freq[0].magnitude;
		}
	}

	if(maxMagnitude == 0)
	{
		logmsg(" - Could not determine Noise floor\n");
		return;
	}

	for(int i = 0; i < config->MaxFreq; i++)
	{
		if(Signal->Blocks[Silentindex].freq[i].hertz != 0)
		{
			if(Signal->Blocks[Silentindex].freq[i].magnitude > loudest.magnitude)
				loudest = Signal->Blocks[Silentindex].freq[i];
		}
		else
			break;
	}

	if(loudest.hertz && loudest.magnitude != 0)
	{
		loudest.amplitude = CalculateAmplitude(loudest.magnitude, maxMagnitude, config);

		logmsg(" - %s signal noise floor: %g dBFS [%g Hz]\n", 
			Signal->role == ROLE_REF ? "Reference" : "Comparison",
			loudest.amplitude,
			loudest.hertz);
	}
	else
	{
		logmsg(" - Could not determine Noise floor\n");
		return;
	}
}

long int GatherAllSilenceData(AudioSignal *Signal, Frequency **allData, Frequency *loudest, int *silenceBlocks, parameters *config)
{
	long int	freqCount = 0, count = 0;
	Frequency	*data = NULL;

	if(!allData || !loudest || !silenceBlocks)
		return 0;

	*allData = NULL;
	*silenceBlocks = 0;

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		if(GetBlockType(config, b) == TYPE_SILENCE)
		{
			(*silenceBlocks)++;
			for(int i = 0; i < config->MaxFreq; i++)
			{
				if(Signal->Blocks[b].freq[i].hertz && Signal->Blocks[b].freq[i].amplitude != NO_AMPLITUDE)
					freqCount++;
			}
		}
	}

	if(!freqCount)
		return 0;

	data = (Frequency*)malloc(sizeof(Frequency)*freqCount);
	if(!data)
	{
		logmsg("Insuffient memory for Silence data\n");
		return 0;
	}

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		if(GetBlockType(config, b) == TYPE_SILENCE)
		{
			for(int i = 0; i < config->MaxFreq; i++)
			{
				if(Signal->Blocks[b].freq[i].hertz && Signal->Blocks[b].freq[i].amplitude != NO_AMPLITUDE)
				{
					data[count] = Signal->Blocks[b].freq[i];
					if(data[count].amplitude > loudest->amplitude)
						*loudest = data[count];
					count++;
				}
			}
		}
	}

	*allData = data;

	return freqCount;
}

void FindFloor(AudioSignal *Signal, parameters *config)
{
	long int	size = 0;
	int 		foundScan = 0, foundGrid = 0, foundCross = 0, silenceBlocks = 0;
	Frequency	loudestFreq, noiseFreq, gridFreq, horizontalFreq, crossFreq, *silenceData = NULL;

	if(!Signal)
		return;
	
	if(!Signal->hasSilenceBlock)
		return;

	CleanFrequency(&loudestFreq);
	CleanFrequency(&noiseFreq);
	CleanFrequency(&gridFreq);
	CleanFrequency(&horizontalFreq);
	CleanFrequency(&crossFreq);

	size = GatherAllSilenceData(Signal, &silenceData, &loudestFreq, &silenceBlocks, config);
	if(!size)
		return;

	if(loudestFreq.hertz && loudestFreq.amplitude != NO_AMPLITUDE)
	{
		logmsg(" - %s signal relative noise floor: %g Hz %g dBFS\n", 
			Signal->role == ROLE_REF ? "Reference" : "Comparison",
			loudestFreq.hertz,
			loudestFreq.amplitude);

		if(Signal->role == ROLE_REF)
			config->referenceNoiseFloor = loudestFreq.amplitude;
	}

	// This only works if a low fundamental frequency is defined in profile, otherwise
	// returns amplitude at 0
	noiseFreq = FindNoiseBlockInsideOneStandardDeviation(Signal, config);

	for(long int i = 0; i < size; i++)
	{
		if(foundGrid != silenceBlocks && IsGridFrequencyNoise(Signal, silenceData[i].hertz))
		{
			if(noiseFreq.amplitude > silenceData[i].amplitude)
			{
				foundGrid++;
				if(silenceData[i].amplitude > gridFreq.amplitude)
				{
					gridFreq = silenceData[i];
					Signal->gridAmplitude = silenceData[i].amplitude;
				}

				if(Signal->floorAmplitude == 0)
				{
					Signal->floorAmplitude = silenceData[i].amplitude;
					Signal->floorFreq = silenceData[i].hertz;
				}
			}
		}

		if(foundScan != silenceBlocks && IsHRefreshNoise(Signal, silenceData[i].hertz))
		{
			if(noiseFreq.amplitude > silenceData[i].amplitude)
			{
				foundScan++;
				if(silenceData[i].amplitude > horizontalFreq.amplitude)
				{
					horizontalFreq = silenceData[i];
					Signal->scanrateAmplitude = silenceData[i].amplitude;
				}

				if(Signal->floorAmplitude == 0)
				{
					Signal->floorAmplitude = silenceData[i].amplitude;
					Signal->floorFreq = silenceData[i].hertz;
				}
			}
		}

		if(foundCross != silenceBlocks && IsHRefreshNoiseCrossTalk(Signal, silenceData[i].hertz))
		{
			if(noiseFreq.amplitude > silenceData[i].amplitude)
			{
				foundCross++;
				if(silenceData[i].amplitude > crossFreq.amplitude)
				{
					crossFreq = silenceData[i];
					Signal->crossAmplitude = silenceData[i].amplitude;
				}

				if(Signal->floorAmplitude == 0)
				{
					Signal->floorAmplitude = silenceData[i].amplitude;
					Signal->floorFreq = silenceData[i].hertz;
				}
			}
		}

		if(foundScan == silenceBlocks && foundGrid == silenceBlocks && foundCross == silenceBlocks)
			break;
	}

	if(foundScan || foundGrid || foundCross)
	{
		logmsg("    ");

		if(foundScan)
			logmsg("[Horizontal scan rate: %0.2f Hz %0.2f dBFS] ", horizontalFreq.hertz, horizontalFreq.amplitude);

		if(foundGrid)
			logmsg("[Electrical grid: %0.2f Hz %0.2f dBFS] ", gridFreq.hertz, gridFreq.amplitude);

		if(foundCross)
			logmsg("[Crosstalk: %0.2f Hz %0.2f dBFS]", crossFreq.hertz, crossFreq.amplitude);

		logmsg("\n");
	}

	free(silenceData);
	silenceData = NULL;

/*
	if(Signal->floorAmplitude != 0 && noiseFreq.amplitude < Signal->floorAmplitude)
		return;
*/
	/* This helps ignoring harmonic noise when noise floor is too high */
	/* Not used here, but in LoadAndProcessed 
	if(loudestFreq.amplitude > LOWEST_NOISEFLOOR_ALLOWED)
	{
		if(Signal->floorAmplitude == 0)
			logmsg("- %s signal relative noise floor is too loud ( %g Hz %g dBFS)\n", 
				Signal->role == ROLE_REF ? "Reference" : "Comparison",
				loudestFreq.hertz,
				loudestFreq.amplitude);
		return;
	}
	*/

	if(loudestFreq.hertz && loudestFreq.amplitude != NO_AMPLITUDE && loudestFreq.hertz)
	{
		if(noiseFreq.amplitude > loudestFreq.amplitude)
		{
			Signal->floorAmplitude = loudestFreq.amplitude;
			Signal->floorFreq = loudestFreq.hertz;
			return;
		}
	}

	if(noiseFreq.hertz)
	{
		Signal->floorAmplitude = noiseFreq.amplitude;
		Signal->floorFreq = noiseFreq.hertz;

		logmsg("  - %s Noise Channel relative floor: %g Hz %g dBFS\n", 
			Signal->role == ROLE_REF ? "Reference" : "Comparison",
			noiseFreq.hertz,
			noiseFreq.amplitude);
		config->channelWithLowFundamentals = 1;
		return;
	}

	logmsg(" - No meaningful floor found, using the whole range for relative comparison\n");
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
	for(int block = 0; block < config->types.totalBlocks; block++)
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

	if(config->verbose&& MaxBlock != -1) {
		logmsg(" - MAX Amplitude found in block %d at %g Hz with %g magnitude\n", MaxBlock, MaxFreq, MaxMagnitude);
	}

	Signal->MaxMagnitude.magnitude = MaxMagnitude;
	Signal->MaxMagnitude.hertz = MaxFreq;
	Signal->MaxMagnitude.block = MaxBlock;

	// Normalize and calculate Amplitude in dBFSs 
	for(int block = 0; block < config->types.totalBlocks; block++)
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
					CalculateAmplitude(Signal->Blocks[block].freq[i].magnitude, MaxMagnitude, config);
				
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
	for(int block = 0; block < config->types.totalBlocks; block++)
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

	if(config->verbose && MaxBlock != -1) {
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
	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(type >= TYPE_SILENCE || type == TYPE_WATERMARK)
		{
			for(int i = 0; i < config->MaxFreq; i++)
			{
				if(!Signal->Blocks[block].freq[i].hertz)
					break;
	
				Signal->Blocks[block].freq[i].amplitude = 
					CalculateAmplitude(Signal->Blocks[block].freq[i].magnitude, ZeroDbMagReference, config);
	
				if(Signal->Blocks[block].freq[i].amplitude < MinAmplitude)
					MinAmplitude = Signal->Blocks[block].freq[i].amplitude;
			}
		}
	}

	Signal->MinAmplitude = MinAmplitude;
}


void CleanMatched(AudioSignal *ReferenceSignal, AudioSignal *TestSignal, parameters *config)
{
	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		for(int i = 0; i < config->MaxFreq; i++)
		{
			if(!ReferenceSignal->Blocks[block].freq[i].hertz)
				break;
			ReferenceSignal->Blocks[block].freq[i].matched = 0;	
		}
	}

	for(int block = 0; block < config->types.totalBlocks; block++)
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
			logmsgFileOnly("Frequency [%5d] %7g Hz Magnitude: %g Phase: %g",
				j, 
				freq[j].hertz,
				freq[j].magnitude,
				freq[j].phase);
			/* detect VideoRefresh frequency */
			if(Signal && IsHRefreshNoise(Signal, freq[j].hertz))
				logmsgFileOnly(" [Horizontal Refresh Noise?]");
			logmsgFileOnly("\n");
		}
	}
}


void PrintFrequenciesBlock(AudioSignal *Signal, Frequency *freq, int type, parameters *config)
{
	double	 significant = 0;

	if(!freq)
		return;

	significant = config->significantAmplitude;
	if(GetTypeChannel(config, type) == CHANNEL_NOISE)
	{
		if(significant > SIGNIFICANT_VOLUME)
			significant = SIGNIFICANT_VOLUME;
	}

	for(int j = 0; j < config->MaxFreq; j++)
	{
		if(type != TYPE_SILENCE && significant > freq[j].amplitude)
			break;

		if(type == TYPE_SILENCE && significant > freq[j].amplitude && j > 500)
			break;

		if(freq[j].hertz && freq[j].amplitude != NO_AMPLITUDE)
		{
			logmsgFileOnly("Frequency [%5d] %7g Hz Amplitude: %g dBFS Phase: %g",
				j, 
				freq[j].hertz,
				freq[j].amplitude,
				freq[j].phase);
			/* detect VideoRefresh frequency */
			if(Signal && IsHRefreshNoise(Signal, freq[j].hertz))
				logmsgFileOnly(" [Horizontal Refresh Noise?]");
			logmsgFileOnly("\n");
		}
	}
}

void PrintFrequenciesWMagnitudes(AudioSignal *Signal, parameters *config)
{
	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int type = TYPE_NOTYPE;

		logmsgFileOnly("==================== %s %s# %d (%d) ===================\n", 
				Signal->role == ROLE_REF ? "Reference" : "Comparision", GetBlockName(config, block), GetBlockSubIndex(config, block), block);

		type = GetBlockType(config, block);
		PrintFrequenciesBlockMagnitude(Signal, Signal->Blocks[block].freq, type, config);
	}
}

void PrintFrequencies(AudioSignal *Signal, parameters *config)
{
	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int type = TYPE_NOTYPE;

		logmsgFileOnly("==================== %s %s# %d (%d) ===================\n", 
				Signal->role == ROLE_REF ? "Reference" : "Comparision", GetBlockName(config, block), GetBlockSubIndex(config, block), block);

		type = GetBlockType(config, block);
		PrintFrequenciesBlock(Signal, Signal->Blocks[block].freq, type, config);
	}
}

/* check ProcessSamples in mdwave if changed, for reverse FFTW */
inline double CalculateMagnitude(fftw_complex value, long int size)
{
	double r1 = 0;
	double i1 = 0;
	double magnitude = 0;

	r1 = creal(value);
	i1 = cimag(value);
	magnitude = sqrt(r1*r1 + i1*i1)/(double)size;
	return magnitude;
}

inline double CalculatePhase(fftw_complex value, parameters *config)
{
	double r1 = 0;
	double i1 = 0;
	double phase = 0;

	r1 = creal(value);
	i1 = cimag(value);
	phase = atan2(i1, r1)*180/M_PI;
	if(config && config->quantizeRound)
		phase = roundFloat(phase);
	return phase;
}

inline double CalculateAmplitude(double magnitude, double MaxMagnitude, parameters *config)
{
	double amplitude = 0;

	if(magnitude == 0.0 || MaxMagnitude == 0.0)
		return NO_AMPLITUDE;

	amplitude = 20*log10(magnitude/MaxMagnitude);
	if(config && config->quantizeRound)
		amplitude = roundFloat(amplitude);
	return amplitude;
}

inline double CalculateFrequency(double boxindex, double boxsize, parameters *config)
{
	double Hertz = 0;

	Hertz = boxindex/boxsize;
	if(config && !config->ZeroPad && config->quantizeRound) // if zero padded (Hertz Aligned), we are using 1Hz integer bins
		Hertz = roundFloat(Hertz);  // default, overkill yes
	return Hertz;
}

int FillFrequencyStructures(AudioSignal *Signal, AudioBlocks *AudioArray, parameters *config)
{
	long int 	i = 0, startBin= 0, endBin = 0, count = 0, size = 0, amount = 0;
	double 		boxsize = 0;
	int			nyquistLimit = 0;
	Frequency	*f_array = NULL;
#ifdef INDIVPHASE
	Frequency	*f_linarray = NULL;
#endif

	size = AudioArray->fftwValues.size;
	if(!size || !AudioArray->fftwValues.spectrum)
	{
		logmsg("FillFrequencyStructures size == 0\n");
		return 0;
	}

	if(AudioArray->seconds == 0.0)
	{
		logmsg("FillFrequencyStructures seconds == 0\n");
		return 0;
	}

	// Round to 3 decimal places so that 48kHz and 44 kHz line up
	boxsize = RoundFloat(AudioArray->seconds, 3);

	startBin = ceil(config->startHz*boxsize);
	endBin = floor(config->endHz*boxsize);
	if(Signal && Signal->nyquistLimit)
		nyquistLimit = 1;

	if(!Signal && config->nyquistLimit)
		nyquistLimit = 1;
	
	if(nyquistLimit || endBin > size/2)
		endBin = ceil(size/2);

	/*
	logmsgFileOnly("Size: %ld BoxSize: %g StartBin: %ld EndBin %ld\n",
		 size, boxsize, startBin, endBin);
	*/
	f_array = (Frequency*)malloc(sizeof(Frequency)*(endBin-startBin));
	if(!f_array)
	{
		logmsg("ERROR: Not enough memory (f_array)\n");
		return 0;
	}
	memset(f_array, 0, sizeof(Frequency)*(endBin-startBin));

#ifdef INDIVPHASE
	f_linarray = (Frequency*)malloc(sizeof(Frequency)*(endBin-startBin));
	if(!f_linarray)
	{
		free(f_array);
		logmsg("ERROR: Not enough memory (f_linarray)\n");
		return 0;
	}
	memset(f_linarray, 0, sizeof(Frequency)*(endBin-startBin));
#endif

	for(i = startBin; i < endBin; i++)
	{
		f_array[count].hertz = CalculateFrequency(i, boxsize, config);
		f_array[count].magnitude = CalculateMagnitude(AudioArray->fftwValues.spectrum[i], size);
		f_array[count].amplitude = NO_AMPLITUDE;
		f_array[count].phase = CalculatePhase(AudioArray->fftwValues.spectrum[i], config);
		f_array[count].matched = 0;
		count++;
	}

#ifdef INDIVPHASE
	// Copy all available to Lin Array
	memcpy(f_linarray, f_array, sizeof(Frequency)*count);
	
	AudioArray->linFreq = f_linarray;
	AudioArray->linFreqSize = count;
#endif

	if(config->MaxFreq > count)
		amount = count;
	else
		amount = config->MaxFreq;

	// Sort the array by top magnitudes
	FFT_Frequency_Magnitude_tim_sort(f_array, count);
	// Only copy Top amount frequencies
	memcpy(AudioArray->freq, f_array, sizeof(Frequency)*amount);

	// release temporal storage
	free(f_array);
	f_array = NULL;

	ReleaseFFTW(AudioArray);

	return 1;
}

void PrintComparedBlocks(AudioBlocks *ReferenceArray, AudioBlocks *ComparedArray, parameters *config, AudioSignal *Signal)
{
	/* changed Magnitude->amplitude */
	for(int j = 0; j < config->MaxFreq; j++)
	{
		if(config->significantAmplitude > ReferenceArray->freq[j].amplitude)
			break;

		if(ReferenceArray->freq[j].hertz)
		{
			int match = 0;

			logmsgFileOnly("[%5d] Ref: %7g Hz %6.4f dBFS [>%3d]", 
						j,
						ReferenceArray->freq[j].hertz,
						ReferenceArray->freq[j].amplitude,
						ReferenceArray->freq[j].matched - 1);

			if(ComparedArray->freq[j].hertz)
				logmsgFileOnly("\tComp: %7g Hz %6.4f dBFS [<%3d]", 
						ComparedArray->freq[j].hertz,
						ComparedArray->freq[j].amplitude,
						ComparedArray->freq[j].matched - 1);
			else
				logmsgFileOnly("\tCompared:\tNULL");
			match = ReferenceArray->freq[j].matched - 1;
			if(match != -1)
			{
				if(ReferenceArray->freq[j].amplitude == 
				ComparedArray->freq[match].amplitude)
					logmsgFileOnly("FA");
				else
					logmsgFileOnly("F-");
			}
			logmsgFileOnly("\n");
		}
	}
	logmsgFileOnly("\n\n");
}

double CalculateWeightedError(double pError, parameters *config)
{
	int option = 0;

	if(pError < 0.0)  // this should never happen
	{
		logmsg("pERROR < 0! (%g)\n", pError);
		pError = fabs(pError);
		if(pError > 1)
		{
			logmsg("pERROR > 1! (%g)\n", pError);
			return 1;
		}
	}

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
			//pError = pError;
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
			//pError = pError;
			break;
	}

	//map to a sub range so tyhat we always have color when within renge
	pError = pError * 0.85 + 0.15;
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

inline double SecondsToFrames(double seconds, double framerate)
{
	return((seconds*1000.0)/framerate);
}

inline long int SecondsToBytes(long int samplerate, double seconds, int AudioChannels, int *leftover, int *discard, double *leftDecimals)
{
	return(RoundToNbytes((double)samplerate*2.0*(double)AudioChannels*seconds*sizeof(char), AudioChannels, leftover, discard, leftDecimals));
}

inline double BytesToSeconds(long int samplerate, long int bytes, int AudioChannels)
{
	return((double)bytes/((double)samplerate*2.0*(double)AudioChannels));
}

inline double BytesToFrames(long int samplerate, long int bytes, double framerate, int AudioChannels)
{
	return(roundFloat((double)bytes/((double)samplerate*2.0*(double)AudioChannels)/framerate*1000.0));
}

inline double FramesToSamples(double frames, long int samplerate, double framerate)
{
	return(floor((double)samplerate/1000.0*frames*framerate));
}

inline double SamplesToFrames(double samples, long int samplerate, double framerate)
{
	return((samples/(double)samplerate*1000)/framerate);
}

long int RoundToNbytes(double src, int AudioChannels, int *leftover, int *discard, double *leftDecimals)
{
	int extra = 0, roundValue = 0;

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

	roundValue = 2*AudioChannels;
	extra = ((long int)src) % roundValue;
	if(extra != 0)
	{
		if(leftover && discard)
		{
			src -= extra;
			(*leftover) += extra;
			if(*leftover >= roundValue)
			{
				*leftover -= roundValue;
				*discard = roundValue;
			}
			else
				*discard = 0;
		}
		else
			src += roundValue - extra;
	}

	if(leftDecimals)
	{
		if(*leftDecimals >= (double)roundValue)
		{
			*discard += roundValue;
			*leftDecimals -= (double)roundValue;
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

double CalculateFrameRateAndCheckSamplerate(AudioSignal *Signal, parameters *config)
{
	double framerate = 0, endOffset = 0, startOffset = 0, samplerate = 0;
	double expectedFR = 0, diff = 0;
	double ACsamplerate = 0, LastSyncFrameOffset = 0;

	startOffset = Signal->startOffset;
	endOffset = Signal->endOffset;
	samplerate = Signal->header.fmt.SamplesPerSec;
	expectedFR = GetMSPerFrame(Signal, config);
	LastSyncFrameOffset = GetLastSyncFrameOffset(Signal->header, config);

	if(startOffset == endOffset)
		return 0;
	if(!LastSyncFrameOffset)
		return 0;

	framerate = CalculateFrameRate(Signal, config);

	diff = roundFloat(fabs(expectedFR - framerate));	

	ACsamplerate = (endOffset-startOffset)/(expectedFR*LastSyncFrameOffset);
	ACsamplerate = ACsamplerate*1000.0/(2.0*Signal->AudioChannels);
	if(fabs(ACsamplerate - samplerate) >= 5.0)
	{	
		if(config->doSamplerateAdjust)
		{
			logmsg(" - WARNING: Auto adjustment of samplerate and frame rate applied\n");
			logmsg("    Original framerate: %gms (%g difference)\n", framerate, diff);

			Signal->originalSR = Signal->header.fmt.SamplesPerSec;
			Signal->originalFrameRate = framerate;

			Signal->header.fmt.SamplesPerSec = RoundFloat(ACsamplerate, 0);
			samplerate = Signal->header.fmt.SamplesPerSec;
			framerate = (endOffset-startOffset)/(samplerate*LastSyncFrameOffset); // 1000 ms 
			framerate = framerate*1000.0/(2.0*Signal->AudioChannels);  // 1000 ms and 2/4 bytes per stereo sample

			logmsg("    Adjusted sample rate to: %dHz\n", Signal->header.fmt.SamplesPerSec);

			Signal->EstimatedSR = Signal->header.fmt.SamplesPerSec;
		}
		else
		{
			logmsg(" - WARNING: %s file framerate difference is %g ms per frame.\n",
					Signal->role == ROLE_REF ? "Reference" : "Comparision", diff);
			logmsg(" - Sample rate estimated at %f\n", ACsamplerate);

			Signal->EstimatedSR = ACsamplerate;
			Signal->originalSR = Signal->header.fmt.SamplesPerSec;
		}
		
		config->centsDifferenceSR = 1200*log2(Signal->EstimatedSR/Signal->originalSR);
		logmsg(" - Pitch difference in cents: %g\n", config->centsDifferenceSR);
		config->SRNoMatch |= Signal->role;
	}

	return framerate;
}

double CalculateFrameRate(AudioSignal *Signal, parameters *config)
{
	double framerate = 0, endOffset = 0, startOffset = 0, samplerate = 0;
	double LastSyncFrameOffset = 0;

	startOffset = Signal->startOffset;
	endOffset = Signal->endOffset;
	samplerate = Signal->header.fmt.SamplesPerSec;
	LastSyncFrameOffset = GetLastSyncFrameOffset(Signal->header, config);

	if(startOffset == endOffset)
		return 0;
	if(!LastSyncFrameOffset)
		return 0;

	framerate = (endOffset-startOffset)/(samplerate*LastSyncFrameOffset); // 1000 ms 
	framerate = framerate*1000.0/(2.0*Signal->AudioChannels);  // 1000 ms and 2/4 bytes per stereo sample
	//framerate = roundFloat(framerate);

	return framerate;
}

double CalculateFrameRateNS(AudioSignal *Signal, double Frames, parameters *config)
{
	double framerate = 0, endOffset = 0, startOffset = 0, samplerate = 0;
	double expectedFR = 0, diff = 0;

	startOffset = Signal->startOffset;
	endOffset = Signal->endOffset;
	samplerate = Signal->header.fmt.SamplesPerSec;
	expectedFR = GetMSPerFrame(Signal, config);

	framerate = (endOffset-startOffset)/(samplerate*Frames);
	framerate = framerate*1000.0/(2.0*Signal->AudioChannels);  // 1000 ms and 2/4 bytes per stereo sample
	framerate = roundFloat(framerate);

	diff = roundFloat(fabs(expectedFR - framerate));
	//The range to report this on will probably vary by console...
	if(config->verbose && diff > 0.001) //  && diff < 0.02)
	{
		double ACsamplerate = 0;

		ACsamplerate = (endOffset-startOffset)/(expectedFR*Frames);
		ACsamplerate = ACsamplerate*1000.0/(2.0*Signal->AudioChannels);
		logmsg(" - %s file framerate difference is %g.\n\tAudio card sample rate estimated at %g\n",
				Signal->role == ROLE_REF ? "Reference" : "Comparision",
				diff, ACsamplerate);
	}

	return framerate;
}

double CalculateScanRateOriginalFramerate(AudioSignal *Signal)
{
	if(Signal->originalFrameRate == 0)
		return 0;
	return(1000.0/Signal->originalFrameRate);
}

double CalculateScanRate(AudioSignal *Signal)
{
	if(Signal->framerate == 0)
		return 0;
	return(1000.0/Signal->framerate);
}

int getPulseCount(int role, parameters *config)
{
	if(role == ROLE_REF)
		return(config->types.SyncFormat[config->videoFormatRef].pulseCount);
	else
		return(config->types.SyncFormat[config->videoFormatCom].pulseCount);
}

int getPulseFrameLen(int role, parameters *config)
{
	if(role == ROLE_REF)
		return(config->types.SyncFormat[config->videoFormatRef].pulseFrameLen);
	else
		return(config->types.SyncFormat[config->videoFormatCom].pulseFrameLen);
}

int GetPulseSyncFreq(int role, parameters *config)
{
	if(role == ROLE_REF)
		return(config->types.SyncFormat[config->videoFormatRef].pulseSyncFreq);
	else
		return(config->types.SyncFormat[config->videoFormatCom].pulseSyncFreq);
}

int GetLineCount(int role, parameters *config)
{
	if(role == ROLE_REF)
		return(config->types.SyncFormat[config->videoFormatRef].LineCount);
	else
		return(config->types.SyncFormat[config->videoFormatCom].LineCount);
}

double getMSPerFrameInternal(int role, parameters *config)
{
	if(role == NO_ROLE)
	{
		logmsg("WARNING: No role assigned, using Reference Frame Rate\n");
		return(config->types.SyncFormat[config->videoFormatRef].MSPerFrame);
	}	
	if(role == ROLE_REF)
		return(config->types.SyncFormat[config->videoFormatRef].MSPerFrame);
	else
		return(config->types.SyncFormat[config->videoFormatCom].MSPerFrame);
}

double CalculateClk(AudioSignal *Signal, parameters *config)
{
	if(!config->clkMeasure)
		return 0;

	if(!Signal || !Signal->clkFrequencies.freq)
		return 0;

	return Signal->clkFrequencies.freq[0].hertz * config->clkRatio;
}

char GetTypeProfileName(int type)
{
	char c = TYPE_NULLTYPE_C;

	switch(type)
	{
		case TYPE_SILENCE:
			c = TYPE_SILENCE_C;
			break;
		case TYPE_SYNC:
			c = TYPE_SYNC_C;
			break;
		case TYPE_NOTYPE:
			c = TYPE_NOTYPE_C;
			break;
		case TYPE_INTERNAL_KNOWN:
			c = TYPE_INTERNAL_KNOWN_C;
			break;
		case TYPE_INTERNAL_UNKNOWN:
			c = TYPE_INTERNAL_UNKNOWN_C;
			break;
		case TYPE_SKIP:
			c = TYPE_SKIP_C;
			break;
		case TYPE_TIMEDOMAIN:
			c = TYPE_TIMEDOMAIN_C;
			break;
		case TYPE_SILENCE_OVERRIDE:
			c = TYPE_SILENCE_OVER_C;
			break;
		case TYPE_WATERMARK:
			c = TYPE_WATERMARK_C;
			break;
		default:
			c = TYPE_NULLTYPE_C;
			break;
	}
	return c;
}

void CleanName(char *name, char *display)
{
	int size = 0, i = 0;

	size = strlen(name);
	for(i = 0; i < size; i++)
	{
		if(name[i] == '_')
			display[i] = ' ';
		else
			display[i] = name[i];
	}
	display[i] = '\0';
}

double FindFundamentalAmplitudeAverage(AudioSignal *Signal, parameters *config)
{
	double		AvgFundAmp = 0;
	long int	count = 0;

	if(!Signal)
		return 0;

	// Find global peak
	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(type > TYPE_SILENCE && Signal->Blocks[block].freq[0].hertz != 0)
		{
			AvgFundAmp += Signal->Blocks[block].freq[0].amplitude;
			count ++;
		}
	}

	if(count)
		AvgFundAmp /= count;

	if(config->verbose) {
		logmsg(" - %s signal Average Fundamental Amplitude %g dBFS from %ld elements\n", 
				Signal->role == ROLE_REF ? "Reference" : "Comparison",
				AvgFundAmp, count);
	}

	return AvgFundAmp;
}

/*
void DetectOvertoneStart(AudioSignal *Signal, parameters *config)
{
	Frequency *tones = NULL;
	
	tones = (Frequency*)malloc(sizeof(Frequency)*config->MaxFreq);
	if(!tones)
	{
		logmsg("Not enough memory\n");
		return;
	}
	for(int n = 0; n < config->types.totalBlocks; n++)
	{
		int		fcount = 0;

		logmsg("BLOCK (%d) %s# %d\n", n, GetBlockName(config, n), GetBlockSubIndex(config, n));
		for(int i = 0; i < config->MaxFreq; i++)
		{
			Frequency freq;

			freq = Signal->Blocks[n].freq[i];
			if(freq.hertz != 0)
			{
				int found = 0;

				for(int f = 0; f < fcount; f++)
				{
					int overtone = 0;

					overtone = (int)round(freq.hertz) % (int)round(tones[f].hertz);
					if(overtone == 0)
					{
						logmsgFileOnly("Overtone found! %g[%d] Hz %g dBFS of %g[%d] Hz %g dBFS at pos %d\n", 
							freq.hertz, (int)round(freq.hertz), freq.amplitude, 
							tones[f].hertz, (int)round(tones[f].hertz), tones[f].amplitude, i);
						found = 1;
						// destroy the rest!
						Signal->Blocks[n].freq[i].hertz = 0;
						Signal->Blocks[n].freq[i].amplitude = NO_AMPLITUDE;
						break;
					}
				}

				if	(!found)
				{
					tones[fcount] = freq;
					fcount ++;
				}
				else
					break;
			}
		}
	}
	free(tones);
	tones = NULL;
}

*/
