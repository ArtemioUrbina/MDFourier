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

int IsCRTNoise(double freq)
{
	/* Peak around 15697-15698 usualy */
	if(freq >= 15620 && freq <= 15710)
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
				Signal->Blocks[n].freq[i].amplitude = 0;
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
}

void ReleaseAudioBlockStructure(parameters *config)
{
	if(config->types.typeCount && config->types.typeArray)
	{
		free(config->types.typeArray);
		config->types.typeArray = NULL;
		config->types.typeCount = 0;
	}
}

int LoadAudioBlockStructure(parameters *config)
{
	FILE 	*file;
	char	buffer[512];

	if(!config)
		return 0;

	file = fopen("mdfblocks.mfn", "r");
	if(!file)
	{
		logmsg("Could not load audio configuiration file mdfblocks.mfn\n");
		return 0;
	}
	
	fscanf(file, "%s ", buffer);
	if(strcmp(buffer, "MDFourierAudioBlockFile") != 0)
	{
		logmsg("Not an MD Fourier Audio Block File\n");
		fclose(file);
		return 0;
	}
	fscanf(file, "%s\n", buffer);
	if(atof(buffer) > 1.0)
	{
		logmsg("This executable can parse 1.0 files only\n");
		fclose(file);
		return 0;
	}
	if(fscanf(file, "%s\n", config->types.Name) != 1)
	{
		logmsg("Invalid Name '%s'\n", buffer);
		fclose(file);
		return 0;
	}

	if(fscanf(file, "%s\n", buffer) != 1)
	{
		logmsg("Invalid Frame Rate Adjustment '%s'\n", buffer);
		fclose(file);
		return 0;
	}
	config->types.platformMSPerFrame = strtod(buffer, NULL);
	if(!config->types.platformMSPerFrame)
	{
		logmsg("Invalid Frame Rate Adjustment '%s'\n", buffer);
		fclose(file);
		return 0;
	}

	if(fscanf(file, "%s\n", buffer) != 1)
	{
		logmsg("Invalid Pulse Sync Frequency '%s'\n", buffer);
		fclose(file);
		return 0;
	}
	config->types.pulseSyncFreq = atoi(buffer);
	if(!config->types.pulseSyncFreq)
	{
		logmsg("Invalid Pulse Sync Frequency '%s'\n", buffer);
		fclose(file);
		return 0;
	}

	fscanf(file, "%s\n", buffer);
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

		if(fscanf(file, "%s ", config->types.typeArray[i].typeName) != 1)
		{
			logmsg("Invalid Block Name %s\n", config->types.typeArray[i].typeName);
			fclose(file);
			return 0;
		}

		type = fgetc(file);
		if(type == EOF)
		{
			logmsg("Config file is too short\n");
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
				ungetc(type, file);
				if(fscanf(file, "%d ", &config->types.typeArray[i].type) != 1)
				{
					logmsg("Invalid MD Fourier Audio Blocks File\n");
					fclose(file);
					return 0;
				}
				break;
		}
		
		if(fscanf(file, "%d %d %s\n", 
			&config->types.typeArray[i].elementCount,
			&config->types.typeArray[i].frames,
			&config->types.typeArray[i].color [0]) != 3)
		{
			logmsg("Invalid MD Fourier Audio Blocks File\n");
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
	return(config->types.platformMSPerFrame);
}

double GetPulseSyncFreq(parameters *config)
{
	return(config->types.pulseSyncFreq);
}

long int GetByteSizeDifferenceByFrameRate(double framerate, long int frames, long int samplerate, parameters *config)
{
	long int difference = 0;

	if(config->smallerFramerate != 0 && framerate > config->smallerFramerate)
	{
		long int SmallerBytes = 0;
		long int BiggerBytes = 0;

		SmallerBytes = SecondsToBytes(samplerate, FramesToSeconds(config->smallerFramerate, frames), NULL, NULL);
		BiggerBytes = SecondsToBytes(samplerate, FramesToSeconds(framerate, frames), NULL, NULL);
	
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

long int GetLastSilenceByteOffset(double framerate, wav_hdr header, parameters *config)
{
	if(!config)
		return NO_INDEX;

	for(int i = config->types.typeCount - 1; i >= 0; i--)
	{
		if(config->types.typeArray[i].type == TYPE_SILENCE)
		{
			double offset = 0;

			offset = FramesToSeconds(GetBlockFrameOffset(i, config), framerate);
			offset = SecondsToBytes(header.SamplesPerSec, offset, NULL, NULL);
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

void FindFloor(AudioSignal *Signal, parameters *config)
{
	int index;

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

	for(int i = 0; i < config->MaxFreq; i++)
	{
		if(Signal->Blocks[index].freq[i].hertz && !IsCRTNoise(Signal->Blocks[index].freq[i].hertz))
		{
			Signal->floorAmplitude = Signal->Blocks[index].freq[i].amplitude;
			Signal->floorFreq = Signal->Blocks[index].freq[i].hertz;
			logmsg(" - Silence block max volume: %g Hz at %0.4f.db\n",
				Signal->floorFreq, Signal->floorAmplitude);
			return;
		}
	}
	Signal->hasFloor = 0;  /* revoke it if not found */
}

void GlobalNormalize(AudioSignal *Signal, parameters *config)
{
	double MaxMagnitude = 0;

	if(!Signal)
		return;

	/* Find global peak */
	if(config->normalize == 'g' ||
	  (config->normalize == 'r' && config->relativeMaxMagnitude == 0.0))
	{
		for(int block = 0; block < config->types.totalChunks; block++)
		{
			for(int i = 0; i < config->MaxFreq; i++)
			{
				if(!Signal->Blocks[block].freq[i].hertz)
					break;
				if(Signal->Blocks[block].freq[i].magnitude > MaxMagnitude)
					MaxMagnitude = Signal->Blocks[block].freq[i].magnitude;
			}
		}

		if(config->normalize == 'r')
			config->relativeMaxMagnitude = MaxMagnitude;
	}

	if(config->normalize == 'r' && config->relativeMaxMagnitude != 0.0)
		MaxMagnitude = config->relativeMaxMagnitude;

	/* Normalize and calculate Amplitude in dbs */
	for(int block = 0; block < config->types.totalChunks; block++)
	{
		for(int i = 0; i < config->MaxFreq; i++)
		{
			if(!Signal->Blocks[block].freq[i].hertz)
				break;
			Signal->Blocks[block].freq[i].amplitude = 
				RoundFloat(20*log10(Signal->Blocks[block].freq[i].magnitude/MaxMagnitude), 2);
			Signal->Blocks[block].freq[i].magnitude = 
				RoundFloat(Signal->Blocks[block].freq[i].magnitude*100.0/MaxMagnitude, 2);
		}
	}
}

/* Do per block normalization if requested */
/* This makes it so relative channel/block */
/* volume is ignored at a global level */
void LocalNormalize(AudioBlocks *AudioArray, parameters *config)
{
	double MaxMagnitude = 0;

	if(!AudioArray)
		return;

	/* Find MaxMagnitude for Normalization */
	for(long int i = 0; i < config->MaxFreq; i++)
	{
		if(AudioArray->freq[i].hertz)
		{
			if(AudioArray->freq[i].magnitude > MaxMagnitude)
				MaxMagnitude = AudioArray->freq[i].magnitude;
		}
	}
 
	/* Normalize to 100 */
	/* Calculate per Block dbs */
	for(long int i = 0; i < config->MaxFreq; i++)
	{
		AudioArray->freq[i].amplitude = 
			RoundFloat(20*log10(AudioArray->freq[i].magnitude/MaxMagnitude), 2);
		AudioArray->freq[i].magnitude = 
			RoundFloat(AudioArray->freq[i].magnitude*100.0/MaxMagnitude, 2);
	}
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

void SortFrequencies(AudioSignal *Signal, parameters *config)
{
	for(int block = 0; block < config->types.totalChunks; block++)
	{
		for(int i = 0; i < config->MaxFreq - 1; i++)
		{
			for(int j = 0; j < config->MaxFreq - i - 1; j++)
			{
				if(Signal->Blocks[block].freq[j].hertz > 
					Signal->Blocks[block].freq[j+1].hertz)
				{
					Frequency	t;
				
					t = Signal->Blocks[block].freq[j];
					Signal->Blocks[block].freq[j] = Signal->Blocks[block].freq[j+1];
					Signal->Blocks[block].freq[j+1] = t;
				}
			}
		}
	}
}

void PrintFrequencies(AudioSignal *Signal, parameters *config)
{
	if(IsLogEnabled())
			DisableConsole();

	for(int block = 0; block < config->types.totalChunks; block++)
	{
		logmsg("==================== %s# %d (%d) ===================\n", 
				GetBlockName(config, block), GetBlockSubIndex(config, block), block);
		if(config->spreadsheet)
			logmsg("Spreadsheet-%s#%d\n", GetBlockName(config, block), GetBlockSubIndex(config, block));

		for(int j = 0; j < config->MaxFreq; j++)
		{
			if(config->significantVolume > Signal->Blocks[block].freq[j].amplitude)
				break;

			if(Signal->Blocks[block].freq[j].hertz)
			{
				logmsg("Frequency [%5d] %7g Hz Amplitude: %g Phase: %g",
					j, 
					Signal->Blocks[block].freq[j].hertz,
					Signal->Blocks[block].freq[j].amplitude,
					Signal->Blocks[block].freq[j].phase);
				/* detect CRT frequency */
				if(IsCRTNoise(Signal->Blocks[block].freq[j].hertz))
					logmsg(" *** CRT Noise ***");
				logmsg("\n");
			}
	
			if(config->spreadsheet)
			{
				logmsg("Spreadsheet-index-Hz-amplitude, %d, %g, %g\n",
					j, Signal->Blocks[block].freq[j].hertz, Signal->Blocks[block].freq[j].amplitude);
			}

			if(config->debugVerbose && j == 100)  /* this is just for internal quick debugging */
				exit(1);
		}
	}

	if(IsLogEnabled())
		EnableConsole();
}

void FillFrequencyStructures(AudioBlocks *AudioArray, parameters *config)
{
	long int i = 0, startBin= 0, endBin = 0;
	double boxsize = 0, size = 0;

	size = AudioArray->fftwValues.size;
	boxsize = RoundFloat(AudioArray->fftwValues.seconds, 4);

	startBin = floor(config->startHz*boxsize);
	endBin = floor(config->endHz*boxsize);

	for(i = startBin; i < endBin; i++)
	{
		double r1 = creal(AudioArray->fftwValues.spectrum[i]);
		double i1 = cimag(AudioArray->fftwValues.spectrum[i]);
		double magnitude, previous;
		int    j = 0;
		double Hertz;

		magnitude = sqrt(r1*r1 + i1*i1)/sqrt(size);
		Hertz = ((double)i/boxsize);
		Hertz = RoundFloat(Hertz, 2);  // default, overkill yes

		previous = 1.e30;
		if(!IsCRTNoise(Hertz))
		{
			for(j = 0; j < config->MaxFreq; j++)
			{
				if(Hertz == AudioArray->freq[j].hertz)
					logmsg("Freq Collision!!!! %g\n", Hertz);
				if(magnitude > AudioArray->freq[j].magnitude && magnitude < previous)
				{
					/* Move the previous values down the array */
					for(int k = config->MaxFreq-1; k > j; k--)
					{
						if(AudioArray->freq[k].hertz)
							AudioArray->freq[k] = AudioArray->freq[k - 1];
					}
	
					AudioArray->freq[j].hertz = Hertz;
					AudioArray->freq[j].magnitude = magnitude;
					AudioArray->freq[j].amplitude = 0;
					AudioArray->freq[j].phase = atan2(i1, r1)*180/M_PI;
					break;
				}
				previous = AudioArray->freq[j].magnitude;
			}
		}
	}
}

void CompressFrequencies(AudioBlocks *AudioArray, parameters *config)
{
	long int i = 0;

	/* The following mess compressed adjacent frequencies */
	/* Disabled by default and it is not as useful as expected in the current form */
	
	for(i = 0; i < config->MaxFreq; i++)
	{
		for(int j = 0; j < config->MaxFreq; j++)
		{
			double hertzDiff;

			hertzDiff = fabs(AudioArray->freq[j].hertz - AudioArray->freq[i].hertz);
			
			if(AudioArray->freq[i].hertz && AudioArray->freq[j].hertz
				&& i != j && hertzDiff <= config->HzWidth)
			{
				if(AudioArray->freq[i].magnitude > AudioArray->freq[j].magnitude)
				{
					AudioArray->freq[i].magnitude += AudioArray->freq[j].magnitude;
					/* AudioArray->freq[i].magnitude/= 2; */

					AudioArray->freq[j].hertz = 0;
					AudioArray->freq[j].magnitude = 0;
					AudioArray->freq[j].phase = 0;
					AudioArray->freq[i].amplitude = 0;
				}
				else
				{
					AudioArray->freq[j].magnitude += AudioArray->freq[i].magnitude;
					/* AudioArray->freq[j].magnitude/= 2; */

					AudioArray->freq[i].hertz = 0;
					AudioArray->freq[i].magnitude = 0;
					AudioArray->freq[i].phase = 0;
					AudioArray->freq[i].amplitude = 0;
				}
			}
		}
	}

	/* sort array by magnitude */
	for(i = 0; i < config->MaxFreq; i++)
	{
		for(int j = i + 1; j < config->MaxFreq; j++)
		{
			if (AudioArray->freq[i].magnitude < AudioArray->freq[j].magnitude) 
			{
					Frequency	t;
				
					t = AudioArray->freq[i];
					AudioArray->freq[i] = AudioArray->freq[j];
					AudioArray->freq[j] = t;
				}
		}
	}
}

void PrintComparedBlocks(AudioBlocks *ReferenceArray, AudioBlocks *ComparedArray, parameters *config, AudioSignal *Signal)
{
	if(IsLogEnabled())
		DisableConsole();

	/* changed Magnitude->amplitude */
	for(int j = 0; j < config->MaxFreq; j++)
	{
		if(config->significantVolume > ReferenceArray->freq[j].amplitude)
			break;

		if(ReferenceArray->freq[j].hertz)
		{
			int match = 0;

			logmsg("[%5d] Ref: %7g Hz %6.2fdb [>%3d]", 
						j,
						ReferenceArray->freq[j].hertz,
						ReferenceArray->freq[j].amplitude,
						ReferenceArray->freq[j].matched - 1);

			if(ComparedArray->freq[j].hertz)
				logmsg("\tComp: %7g Hz %6.2fdb [<%3d]", 
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

	if(IsLogEnabled())
		EnableConsole();
}

double CalculateWeightedError(double pError, parameters *config)
{
	int option = 0;

	option = config->outputFilterFunction;
	switch(option)
	{
		case 0:
			pError = 1;
			break;
		case 1:
			pError = pError;
			break;
		case 2:
			/* Map to Beta function */
			pError = incbeta(8.0, 8.0, pError);
			break;
		case 3:
			/* Map to Beta function */
			pError = incbeta(6.0, 4.0, pError);
			break;
		case 4:
			/* Map to Beta function */
			pError = incbeta(4.0, 2.0, pError);
			break;
		case 5:
			/* Map to Beta function */
			pError = incbeta(1.0, 3.0, pError);
			break;
		case 6:
			/* Map to Beta function */
			pError = incbeta(0.5, 6, pError);
			break;
		default:
			/* This is unexpected behaviour, log it */
			logmsg("CalculateWeightedError, out of range value %d\n", option);
			pError = 1;
			break;
	}

	return pError;
}

double RoundFloat(double x, int p)
{
	if (x != 0.0) {
		return ((floor((fabs(x)*pow((double)(10.0),p))+0.5))/pow((double)(10.0),p))*(x/fabs(x));
	} else {
		return 0.0;
	}
}

double FramesToSeconds(double frames, double framerate)
{
	return(frames*framerate/1000.0);
}

long int SecondsToBytes(long int samplerate, double seconds, int *leftover, int *discard)
{
	return(RoundTo4bytes(samplerate*4.0*seconds*sizeof(char), leftover, discard));
}

double BytesToSeconds(long int samplerate, long int bytes)
{
	return((double)bytes/(samplerate*4.0));
}

long int RoundTo4bytes(double src, int *leftover, int *discard)
{
	int extra = 0;

	if(!leftover)
		src = ceil(src);
	else
		src = floor(src);
	extra = ((long int)src) % 4;
	if(extra != 0)
	{
		if(leftover)
		{
			src -= extra;
			if(*leftover + extra >= 4)
			{
				*leftover -= 4;
				*discard = 4;
			}
			else
			{
				*leftover += extra;
				*discard = 0;
			}
		}
		else
			src += 4 - extra;
	}
	return (src);
}

double GetDecimalValues(double value)
{
	double integer = 0;

	value = modf(value, &integer);
	return value;
}