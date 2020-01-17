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

#define SORT_NAME FFT_Frequency
#define SORT_TYPE Frequency
#define SORT_CMP(x, y)  ((x).magnitude > (y).magnitude ? -1 : ((x).magnitude == (y).magnitude ? 0 : 1))
#include "sort.h"  // https://github.com/swenson/sort/


double FindFrequencyBinSizeForBlock(AudioSignal *Signal, long int block)
{
	if(!Signal)
		return 0;
	if(!Signal->Blocks)
		return 0;
	
	return((double)Signal->header.fmt.SamplesPerSec/(double)Signal->Blocks[block].fftwValues.size);
}

double FindFrequencyBracket(double frequency, size_t size, int AudioChannels, long samplerate)
{
	long int startBin= 0, endBin = 0;
	double seconds = 0, boxsize = 0, minDiff = 0, targetFreq = 0;

	minDiff = (double)samplerate/2.0;
	targetFreq = frequency;

	seconds = (double)size/((double)samplerate*(double)AudioChannels);
	boxsize = RoundFloat(seconds, 3);
	if(boxsize == 0)
		boxsize = seconds;

	startBin = ceil(10*boxsize);
	endBin = floor(20000*boxsize);

	for(int i = startBin; i < endBin; i++)
	{
		double Hertz = 0, difference = 0;

		Hertz = CalculateFrequency(i, boxsize, 0);
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
		double gridNoise = 0, scanNoise = 0;

		Signal->SilenceBinSize = FindFrequencyBinSizeForBlock(Signal, index);

		// NTSC or PAL
		gridNoise = fabs(60.0 - roundFloat(1000.0/Signal->framerate)) < 5 ? 60.0 : 50.0;
		Signal->gridFrequency = FindFrequencyBracket(gridNoise, Signal->Blocks[index].fftwValues.size, Signal->AudioChannels, Signal->header.fmt.SamplesPerSec);

		scanNoise = CalculateScanRate(Signal)*(double)GetLineCount(Signal->role, config);
		Signal->scanrateFrequency = FindFrequencyBracket(scanNoise, Signal->Blocks[index].fftwValues.size, Signal->AudioChannels, Signal->header.fmt.SamplesPerSec);

		if(config->verbose)
		{
			logmsg(" - Searching for noise frequencies [%s]: Power grid %g Hz Scan Rate: %g Hz\n", 
				Signal->role == ROLE_REF ? "Reference" : "Comparison",
				Signal->gridFrequency, Signal->scanrateFrequency);
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


	if(Signal->scanrateFrequency == 0)
		return 0;

	if(freq >= Signal->scanrateFrequency - Signal->SilenceBinSize*5 && freq <= Signal->scanrateFrequency)
		return 1;
	return 0;
}

int IsGridFrequencyNoise(AudioSignal *Signal, double freq)
{
	if(!Signal)
		return 0;

	if(Signal->gridFrequency == 0)
		return 0;

	if(freq == Signal->gridFrequency)
		return 1;
	return 0;
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
		Signal->Blocks[n].freq = (Frequency*)malloc(sizeof(Frequency)*config->MaxFreq);
		if(!Signal->Blocks[n].freq)
		{
			logmsg("Not enough memory for Data Structures (Chunk %d)\n", n);
			return NULL;
		}
		memset(Signal->Blocks[n].freq, 0, sizeof(Frequency)*config->MaxFreq);
	}

	InitAudio(Signal, config);
	return Signal;
}

void InitAudio(AudioSignal *Signal, parameters *config)
{
	if(!Signal)
		return;

	if(Signal->Blocks)
	{
		for(int n = 0; n < config->types.totalBlocks; n++)
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
	
			Signal->Blocks[n].audio.samples = NULL;
			Signal->Blocks[n].audio.window_samples = NULL;
			Signal->Blocks[n].audio.size = 0;
			Signal->Blocks[n].audio.difference = 0;
	
			Signal->Blocks[n].index = GetBlockSubIndex(config, n);
			Signal->Blocks[n].type = GetBlockType(config, n);
			Signal->Blocks[n].frames = GetBlockFrames(config, n);
		}
	}
	Signal->SourceFile[0] = '\0';
	Signal->AudioChannels = INVALID_CHANNELS;
	Signal->role = NO_ROLE;

	Signal->hasFloor = 0;
	Signal->floorFreq = 0.0;
	Signal->floorAmplitude = 0.0;	

	Signal->Samples = NULL;
	Signal->framerate = 0.0;

	Signal->startOffset = 0;
	Signal->endOffset = 0;

	memset(&Signal->MaxMagnitude, 0, sizeof(MaxMagn));
	Signal->MinAmplitude = 0;

	Signal->gridFrequency = 0;
	Signal->scanrateFrequency = 0;
	Signal->SilenceBinSize = 0;

	Signal->nyquistLimit = 0;
	Signal->startHz = config->startHz;
	Signal->endHz = config->endHz;

	memset(&Signal->header, 0, sizeof(wav_hdr));
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
	AudioArray->fftwValues.size = 0;
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
}

void ReleaseBlock(AudioBlocks * AudioArray)
{
	ReleaseFrequencies(AudioArray);
	ReleaseFFTW(AudioArray);
	ReleaseSamples(AudioArray);

	AudioArray->index = 0;
	AudioArray->type = 0;
	AudioArray->seconds = 0;
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

	if(Signal->Samples)
	{
		free(Signal->Samples);
		Signal->Samples = NULL;
	}

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
		logmsg("ERROR: Could not load profile configuration file: \"%s\"\n", config->profileFile);
		return 0;
	}
	
	readLine(lineBuffer, file);
	sscanf(lineBuffer, "%s ", buffer);
	if(strcmp(buffer, "MDFourierAudioBlockFile") == 0)
	{
		sscanf(lineBuffer, "%*s %s\n", buffer);
		if(atof(buffer) < 1.7)
		{
			logmsg("ERROR: Please update your profile files to version 1.7\n");
			fclose(file);
			return 0;
		}
		if(atof(buffer) > 1.7)
		{
			logmsg("ERROR: This executable can parse \"MDFourierAudioBlockFile 1.7\" files only\n");
			fclose(file);
			return 0;
		}
		return(LoadAudioBlockStructure(file, config));
	}

	if(strcmp(buffer, "MDFourierNoSyncProfile") == 0)
	{
		sscanf(lineBuffer, "%*s %s\n", buffer);
		if(atof(buffer) != 1.1)
		{
			logmsg("ERROR: This executable can parse \"MDFourierNoSyncProfile 1.1\" files only\n");
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

void EndProfileLoad(parameters *config)
{
	logmsg("* Using profile [%s]\n", config->types.Name);	
	if(config->compressToBlocks)
		FlattenProfile(config);

	PrintAudioBlocks(config);
}

int LoadAudioBlockStructure(FILE *file, parameters *config)
{
	int		insideInternal = 0, i = 0;
	char	lineBuffer[LINE_BUFFER_SIZE];
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

	/* Line 3: NTSC and PAL Frame rates and Sync */
	for(i = 0; i < 2; i++)
	{
		readLine(lineBuffer, file);
		if(sscanf(lineBuffer, "%s %s %s %d %d %d\n", 
								buffer, buffer2, buffer3,
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
			logmsg("ERROR: Invalid line count Adjustment '%s'\n", lineBuffer);
			fclose(file);
			return 0;
		}
		config->types.SyncFormat[i].LineCount = strtod(buffer3, NULL);
		if(!config->types.SyncFormat[i].LineCount)
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

	/* Line 5: CLK estimation */
	readLine(lineBuffer, file);
	if(sscanf(lineBuffer, "%s %c", config->clkName, &config->clkProcess) != 2)
	{
		logmsg("ERROR: Invalid MD Fourier Audio Blocks File (CLK): %s\n", lineBuffer);
		fclose(file);
		return 0;
	}
	if(config->clkProcess == 'y')
	{
		if(sscanf(lineBuffer, "%*s %*c %d %d %d %lf %d\n", 
			&config->clkBlock,
			&config->clkFreq,
			&config->clkFreqCount,
			&config->clkAmpl,
			&config->clkRatio) != 5)
		{
			logmsg("ERROR: Invalid MD Fourier Audio Blocks File (CLK): %s\n", lineBuffer);
			fclose(file);
			return 0;
		}
	}

	/* Line 6: Type count */
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

	/* Line 7 and beyond: types */
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
		else
		{
			if(sscanf(lineBuffer, "%*s %*s %d %d %20s %c\n", 
				&config->types.typeArray[i].elementCount,
				&config->types.typeArray[i].frames,
				&config->types.typeArray[i].color[0],
				&config->types.typeArray[i].channel) != 4)
			{
				logmsg("ERROR: Invalid MD Fourier Audio Blocks File (Element Count, frames, color, channel): %s\n", lineBuffer);
				fclose(file);
				return 0;
			}
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

		config->types.typeArray[i].IsaddOnData = MatchesPreviousType(config->types.typeArray[i].type, config);
		// make silent if duplicate and not silence block
		if(!config->useExtraData && config->types.typeArray[i].IsaddOnData)
		{
			if(config->types.typeArray[i].type != TYPE_SILENCE)
				config->types.typeArray[i].type = TYPE_SKIP;
		}
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

	EndProfileLoad(config);

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
	config->types.SyncFormat[0].MSPerFrame = strtod(buffer, NULL);
	if(!config->types.SyncFormat[0].MSPerFrame)
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
	config->types.SyncFormat[1].MSPerFrame = strtod(buffer, NULL);
	if(!config->types.SyncFormat[1].MSPerFrame)
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
		logmsg("Invalid type count:\n'%s'\n", buffer);
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
			logmsg("Invalid Block Name\n%s\n", lineBuffer);
			fclose(file);
			return 0;
		}
		CleanName(config->types.typeArray[i].typeName, config->types.typeArray[i].typeDisplayName);

		if(sscanf(lineBuffer, "%*s %c ", &type) != 1)
		{
			logmsg("Invalid Block Type %s\n", lineBuffer);
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
		
		if(sscanf(lineBuffer, "%*s %*s %d %d %s %c\n", 
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

	config->types.regularBlocks = GetActiveAudioBlocks(config);
	config->types.totalBlocks = GetTotalAudioBlocks(config);
	if(!config->types.totalBlocks)
	{
		logmsg("Total Audio Blocks should be at least 1\n");
		return 0;
	}

	config->significantAmplitude = NS_SIGNIFICANT_VOLUME;
	fclose(file);
	
	EndProfileLoad(config);
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

		logmsgFileOnly("%s%s %s %d %d %s %c %s | Frames: %ld | Seconds: %g [%g to %g]\n", 
			config->types.typeArray[i].type == TYPE_SKIP ? "     " : "",
			config->types.typeArray[i].typeName,
			type,
			config->types.typeArray[i].elementCount,
			config->types.typeArray[i].frames,
			config->types.typeArray[i].color,
			config->types.typeArray[i].channel,
			config->types.typeArray[i].IsaddOnData ? "(r)" : "",
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

void CompareFrameRates(double framerate1, double framerate2, parameters *config)
{
	double diff = 0;

	diff = fabs(framerate1 - framerate2);
	if(diff == 0.0)
		config->smallerFramerate = framerate1;
	else
	{
		config->smallerFramerate = 
				GetLowerFrameRate(framerate1, 
									framerate2);
		/*
		config->biggerFramerate = 
				GetBiggerFrameRate(framerate1, 
									framerate2);
		*/
		if(config->verbose && diff > 0.001)
			logmsg("\n= Different frame rates found (%g), compensating to %g =\n", 
				diff, config->smallerFramerate);
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

int MatchesPreviousType(int type, parameters *config)
{
	int count = 0;

	if(!config)
		return 0;

	if(type < TYPE_CONTROL)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type == type)
			count++;
	}
	
	if(count > 1)
		return 1;
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
		if(config->types.typeArray[i].type == TYPE_INTERNAL_KNOWN ||
			config->types.typeArray[i].type == TYPE_INTERNAL_UNKNOWN)
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

Frequency FindNoiseBlockAverage(AudioSignal *Signal, parameters *config)
{
	Frequency	cutOff;
	int 		noiseBlock = -1;
	int			count = 0;

	cutOff.hertz = 0;
	cutOff.amplitude = 0;

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

	for(int i = 0; i < config->MaxFreq; i++)
	{
		if(Signal->Blocks[noiseBlock].freq[i].hertz)
		{
			cutOff.hertz += Signal->Blocks[noiseBlock].freq[i].hertz;
			cutOff.amplitude += fabs(Signal->Blocks[noiseBlock].freq[i].amplitude);
			count ++;
		}
	}
	
	if(count)
	{
		cutOff.hertz = cutOff.hertz/count;
		cutOff.amplitude = -1.0*(fabs(cutOff.amplitude)/count);
		if(config->verbose)
			logmsg("  - %s signal profile defined noise channel averages: %g dBFS [%g Hz]\n",
				Signal->role == ROLE_REF ? "Reference" : "Comparison",
				cutOff.amplitude, cutOff.hertz);

		cutOff.amplitude += -3.0;
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

	loudest.hertz = 0;
	loudest.magnitude = 0;
	loudest.amplitude = NO_AMPLITUDE;

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
		loudest.amplitude = CalculateAmplitude(loudest.magnitude, maxMagnitude);

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

void FindFloor(AudioSignal *Signal, parameters *config)
{
	int 		index, foundScan = 0, foundGrid = 0;
	Frequency	loudest, noiseFreq;

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
		logmsg(" > %s signal relative noise floor: %g dBFS [%g Hz] %s\n", 
			Signal->role == ROLE_REF ? "Reference" : "Comparison",
			loudest.amplitude,
			loudest.hertz,
			loudest.amplitude < PCM_16BIT_MIN_AMPLITUDE ? "(not significant)" : "");

		if(Signal->role == ROLE_REF)
			config->referenceNoiseFloor = loudest.amplitude;
	}

	noiseFreq = FindNoiseBlockAverage(Signal, config);

	for(int i = 0; i < config->MaxFreq; i++)
	{
		if(Signal->Blocks[index].freq[i].hertz && Signal->Blocks[index].freq[i].amplitude != NO_AMPLITUDE)
		{
			if(!foundGrid && IsGridFrequencyNoise(Signal, Signal->Blocks[index].freq[i].hertz))
			{
				foundGrid = 1;
				if(noiseFreq.amplitude > Signal->Blocks[index].freq[i].amplitude)
				{
					logmsg("  - Possible electrical grid frequency noise: %g dBFS [%g Hz]\n",
						Signal->Blocks[index].freq[i].amplitude, Signal->Blocks[index].freq[i].hertz);

					if(Signal->floorAmplitude == 0)
					{
						Signal->floorAmplitude = Signal->Blocks[index].freq[i].amplitude;
						Signal->floorFreq = Signal->Blocks[index].freq[i].hertz;
					}
				}
			}

			if(!foundScan && IsHRefreshNoise(Signal, Signal->Blocks[index].freq[i].hertz))
			{
				foundScan = 1;
				if(noiseFreq.amplitude > Signal->Blocks[index].freq[i].amplitude)
				{
					logmsg("  - Possible horizontal scan rate noise : %g dBFS [%g Hz]\n",
						Signal->Blocks[index].freq[i].amplitude, Signal->Blocks[index].freq[i].hertz);

					if(Signal->floorAmplitude == 0)
					{
						Signal->floorAmplitude = Signal->Blocks[index].freq[i].amplitude;
						Signal->floorFreq = Signal->Blocks[index].freq[i].hertz;
					}
				}
			}

			if(foundScan && foundGrid)
				break;
		}
	}

	if(Signal->floorAmplitude != 0 && noiseFreq.amplitude < Signal->floorAmplitude )
		return;

	if(loudest.hertz && loudest.amplitude != NO_AMPLITUDE)
	{
		if(noiseFreq.amplitude > loudest.amplitude)
		{
			Signal->floorAmplitude = loudest.amplitude;
			Signal->floorFreq = loudest.hertz;
			return;
		}
	}

	if(noiseFreq.hertz)
	{
		Signal->floorAmplitude = noiseFreq.amplitude;
		Signal->floorFreq = noiseFreq.hertz;

		logmsg("  - %s Noise Channel relative comparison  signal floor: %g dBFS [%g Hz] %s\n", 
			Signal->role == ROLE_REF ? "Reference" : "Comparison",
			noiseFreq.amplitude,
			noiseFreq.hertz,
			noiseFreq.amplitude < PCM_16BIT_MIN_AMPLITUDE ? "(not significant)" : "");
		return;
	}

	logmsg(" - No meaningful floor found, using the whole range for relative comparison\n");
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

	if(config->verbose)
	{
		if(MaxBlock != -1)
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

		if(type == TYPE_SILENCE && significant > freq[j].amplitude && j > 50)
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

		logmsgFileOnly("==================== %s# %d (%d) ===================\n", 
				GetBlockName(config, block), GetBlockSubIndex(config, block), block);

		type = GetBlockType(config, block);
		PrintFrequenciesBlockMagnitude(Signal, Signal->Blocks[block].freq, type, config);
	}
}

void PrintFrequencies(AudioSignal *Signal, parameters *config)
{
	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int type = TYPE_NOTYPE;

		logmsgFileOnly("==================== %s# %d (%d) ===================\n", 
				GetBlockName(config, block), GetBlockSubIndex(config, block), block);

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

int FillFrequencyStructures(AudioSignal *Signal, AudioBlocks *AudioArray, parameters *config)
{
	long int 	i = 0, startBin= 0, endBin = 0, count = 0, size = 0;
	double 		boxsize = 0;
	int			nyquistLimit = 0;
	Frequency	*f_array;

	size = AudioArray->fftwValues.size;
	if(!size)
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

			f_array[count++] = element;
		}
	}

	FFT_Frequency_tim_sort(f_array, count);

	i = 0;
	while(i < count && i < config->MaxFreq)
	{
		AudioArray->freq[i] = f_array[i];
		i++;
	}

	free(f_array);
	f_array = NULL;
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
	return(((double)samplerate*frames*framerate)/1000.0);
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
	framerate = framerate*1000.0/(2.0*Signal->AudioChannels);  // 1000 ms and 2/4 bytes per stereo sample
	framerate = roundFloat(framerate);

	diff = roundFloat(fabs(expectedFR - framerate));
	//The range to report this on will probably vary by console...
	if(config->verbose && diff > 0.001) //   && diff < 0.02)
	{
		double ACsamplerate = 0;

		ACsamplerate = (endOffset-startOffset)/(expectedFR*LastSyncFrameOffset);
		ACsamplerate = ACsamplerate*1000.0/(2.0*Signal->AudioChannels);
		logmsg(" - %s file framerate difference is %g.\n",
				Signal->role == ROLE_REF ? "Reference" : "Comparision",
				diff);
		logmsg("\tAssuming recording is not from an emulator\n\tAudio Card sample rate estimated at %g\n",
				ACsamplerate);
		
	}

	return framerate;
}

/*
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
*/

double CalculateScanRate(AudioSignal *Signal)
{
	//return(roundFloat(1000.0/Signal->framerate));
	return(1000.0/Signal->framerate);
}

double FindDifferenceAverage(parameters *config)
{
	double		AvgDifAmp = 0;
	long int	count = 0;

	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		if(config->Differences.BlockDiffArray[b].type <= TYPE_CONTROL)
			continue;

		for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
		{
			AvgDifAmp += config->Differences.BlockDiffArray[b].amplDiffArray[a].diffAmplitude;
			count ++;
		}
	}

	if(count)
		AvgDifAmp /= count;

	return AvgDifAmp;
}

void SubstractDifferenceAverage(parameters *config, double average)
{
	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		if(config->Differences.BlockDiffArray[b].type <= TYPE_CONTROL)
			continue;


		for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
			config->Differences.BlockDiffArray[b].amplDiffArray[a].diffAmplitude -= average;
	}

	return;
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

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		if(config->Differences.BlockDiffArray[b].type < TYPE_SILENCE)
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

	for(int b = 0; b < config->types.totalBlocks; b++)
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

int FindDifferenceWithinInterval(int type, long int *inside, long int *count, double MaxInterval, parameters *config)
{
	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	if(!inside || !count)
		return 0;

	*inside = 0;
	*count = 0;

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		if(config->Differences.BlockDiffArray[b].type < TYPE_SILENCE)
			continue;

		if(type == config->Differences.BlockDiffArray[b].type)
		{
			for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
			{
				if(fabs(config->Differences.BlockDiffArray[b].amplDiffArray[a].diffAmplitude) <= MaxInterval)
					(*inside)++;
				
			}
			(*inside) += config->Differences.BlockDiffArray[b].perfectAmplMatch;
			(*count) += config->Differences.BlockDiffArray[b].cmpAmplBlkDiff;
		}
	}

	return 1;
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
	double HighestFreq = 0, HighestAmpFreq = 0;

	if(config->clkProcess != 'y')
		return 0;

	if(!Signal)
		return 0;

	if(!Signal->Blocks)
		return 0;

	if(config->clkBlock > config->types.totalBlocks)
		return 0;

	if(config->clkFreqCount > config->MaxFreq)
		return 0;

	if(config->ZeroPad)
		return Signal->Blocks[config->clkBlock].freq[0].hertz * config->clkRatio;
/*
	{
		double count = 0;
		double sum = 0;

		for(int i = 0; i < config->clkFreqCount; i++)
		{
			double currentFreq = 0, currentAmp = 0, difference = 0;

			currentFreq = Signal->Blocks[config->clkBlock].freq[i].hertz;
			currentAmp = Signal->Blocks[config->clkBlock].freq[i].amplitude;
			if(!HighestAmpFreq)
				HighestAmpFreq = currentAmp;
			difference = fabs(fabs(currentAmp) - fabs(HighestAmpFreq));
			if(difference < config->clkAmpl)
			{
				sum += currentFreq;
				count ++;
			}
		}
		return sum/count * config->clkRatio;
	}
*/
	for(int i = 0; i < config->clkFreqCount; i++)
	{
		double currentFreq = 0, currentAmp = 0, difference = 0;

		currentFreq = Signal->Blocks[config->clkBlock].freq[i].hertz;
		currentAmp = Signal->Blocks[config->clkBlock].freq[i].amplitude;
		if(!HighestAmpFreq)
			HighestAmpFreq = currentAmp;
		else
		{
			difference = fabs(fabs(currentAmp) - fabs(HighestAmpFreq));
			if(difference > config->clkAmpl && HighestFreq != 0)
				break;
		}
		if(difference <= config->clkAmpl && currentFreq > HighestFreq)
			HighestFreq = currentFreq;
	}
	return HighestFreq * config->clkRatio;
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