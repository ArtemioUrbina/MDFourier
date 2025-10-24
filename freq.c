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

#include "freq.h"
#include "log.h"
#include "cline.h"
#include "plot.h"
#include "float.h"
#include "profile.h"

#define SORT_NAME FFT_Frequency_Magnitude
#define SORT_TYPE Frequency
#define SORT_CMP(x, y)  ((x).magnitude > (y).magnitude ? -1 : ((x).magnitude == (y).magnitude ? 0 : 1))
#include "sort.h"  // https://github.com/swenson/sort/

inline int areDoublesEqual(double a, double b)
{
	double diff = 0;

	// Exact matches for internal representation
	if(a == b)
		return 1;
	// matches between very close values
	if(fabs(a - b) < (DBL_EPSILON * fabs(a + b)))
		return 1;

	// matches with tolerance, DBL_PERFECT_MATCH or 0.00001 seems way more than enough
	diff = fabs(fabs(b) - fabs(a));
	if(diff != 0.0 && diff < DBL_PERFECT_MATCH)
		return 1;
	return 0;
}

double FindFrequencyBinSizeForBlock(AudioSignal *Signal, long int block)
{
	if(!Signal)
		return 0;
	if(!Signal->Blocks || !Signal->Blocks[block].fftwValues.size)
		return 0;
	
	return(Signal->SampleRate/(double)Signal->Blocks[block].fftwValues.size);
}

double FindFrequencyBracketForSync(double frequency, size_t size, int AudioChannels, double samplerate, parameters *config)
{
	long int startBin= 0, endBin = 0;
	double seconds = 0, boxsize = 0, minDiff = 0, targetFreq = 0;

	if(!config)
	{
		logmsg("ERROR: Config was NULL for FindFrequencyBracket\n");
		return 0;
	}

	minDiff = samplerate/2.0; // Nyquist
	targetFreq = frequency;

	seconds = (double)size/(samplerate*(double)AudioChannels);
	boxsize = RoundFloat(seconds, 3);
	if(boxsize == 0)
		boxsize = seconds;

	startBin = ceil(1*boxsize);
	endBin = floor(samplerate*boxsize);

	for(int i = startBin; i < endBin; i++)
	{
		double Hertz = 0, difference = 0;

		Hertz = CalculateFrequency(i, boxsize);
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

double FindFrequencyBracket(double frequency, size_t size, int AudioChannels, double samplerate, parameters *config)
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

	minDiff = samplerate/2.0; // Nyquist
	targetFreq = frequency;

	// Samplerate CHECK
	seconds = (double)size/(samplerate*(double)AudioChannels);
	boxsize = RoundFloat(seconds, 3);
	if(boxsize == 0)
		boxsize = seconds;

	startBin = ceil(config->startHz*boxsize);
	endBin = floor(config->endHz*boxsize);

	for(int i = startBin; i < endBin; i++)
	{
		double Hertz = 0, difference = 0;

		Hertz = CalculateFrequency(i, boxsize);
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

void CalculateFrequencyBrackets(AudioSignal *Signal, parameters *config)
{
	long int index = 0;

	if(!Signal)
		return;
	if(!Signal->Blocks)
		return;

	index = GetFirstSilenceIndex(config);
	if(index != NO_INDEX)
	{
		double vertNoise = 0, scanNoise = 0, crossNoise = 0;

		Signal->SilenceBinSize = FindFrequencyBinSizeForBlock(Signal, index);

		vertNoise = CalculateScanRate(Signal);
		Signal->gridFrequency = FindFrequencyBracket(vertNoise, Signal->Blocks[index].fftwValues.size, Signal->AudioChannels, Signal->SampleRate, config);

		scanNoise = CalculateScanRate(Signal)*(double)GetLineCount(Signal->role, config);
		Signal->scanrateFrequency = FindFrequencyBracket(scanNoise, Signal->Blocks[index].fftwValues.size, Signal->AudioChannels, Signal->SampleRate, config);

		crossNoise = scanNoise/2;
		Signal->crossFrequency = FindFrequencyBracket(crossNoise, Signal->Blocks[index].fftwValues.size, Signal->AudioChannels, Signal->SampleRate, config);

		logmsg(" - Expected noise [%s Predict/DFT]: Vertical: %g/%g Hz Scan: %g/%g Hz Cross: %g/%g Hz\n",
			getRoleText(Signal),
			vertNoise, Signal->gridFrequency, scanNoise, Signal->scanrateFrequency, crossNoise, Signal->crossFrequency);
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

int InitFreqStruc(Frequency **freq, parameters *config)
{
	if(*freq)
	{
		logmsg("ERROR: InitFreqStruc, frequency block already full\n");
		return 0;
	}
	*freq = (Frequency*)malloc(sizeof(Frequency)*config->MaxFreq);
	if(!*freq)
	{
		logmsg("ERROR: InitFreqStruc, not enough memory for Data Structures\n");
		return 0;
	}
	memset(*freq, 0, sizeof(Frequency)*config->MaxFreq);
	return 1;
}

int InitAudioBlock(AudioBlocks* block, char channel, char maskType, parameters *config)
{
	if(!block)
		return 0;

	memset(block, 0, sizeof(AudioBlocks));
	if(channel == CHANNEL_STEREO)
	{
		if(!InitFreqStruc(&block->freqRight, config))
			return 0;
	}
	block->channel = channel;
	block->maskType = maskType;
	return(InitFreqStruc(&block->freq, config));
}

long int GetBlockFreqSize(AudioSignal *Signal, int block, char channel, parameters *config)
{
	long int	size = 0;

	if(GetBlockType(config, block) == TYPE_SILENCE)
	{
		switch(channel)
		{ 
			case CHANNEL_LEFT:
				size = Signal->Blocks[block].SilenceSizeLeft;
				break;
			case CHANNEL_RIGHT:
				size = Signal->Blocks[block].SilenceSizeRight;
				break;
			default:
				logmsg("ERROR: Invalid Channel for GetBlockFreqSize '%c'\n", channel);
				size = 0;
				break;
		}
	}
	else
		size = config->MaxFreq;
	return size;
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
		if(!InitAudioBlock(&Signal->Blocks[n], GetBlockChannel(config, n), GetBlockMaskType(config, n), config))
			return NULL;
	}

	InitAudio(Signal, config);
	if(config->clkMeasure)
		InitAudioBlock(&Signal->clkFrequencies, CHANNEL_MONO, MASK_DEFAULT, config);	// we use the new mask default here
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
	if(AudioArray->freq)
	{
		for(int i = 0; i < config->MaxFreq; i++)
			CleanFrequency(&AudioArray->freq[i]);
	}

	if(AudioArray->freqRight)
	{
		for(int i = 0; i < config->MaxFreq; i++)
			CleanFrequency(&AudioArray->freqRight[i]);
	}
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
			Signal->Blocks[n].fftwValues.ENBW = 0;

			Signal->Blocks[n].audio.samples = NULL;
			Signal->Blocks[n].audio.windowed_samples = NULL;
			Signal->Blocks[n].audio.size = 0;
			Signal->Blocks[n].audio.difference = 0;
			Signal->Blocks[n].audio.padding = 0;
			Signal->Blocks[n].audio.sampleOffset = 0;

			Signal->Blocks[n].fftwValuesRight.spectrum = NULL;
			Signal->Blocks[n].fftwValuesRight.size = 0;
			Signal->Blocks[n].fftwValuesRight.ENBW = 0;

			Signal->Blocks[n].audioRight.samples = NULL;
			Signal->Blocks[n].audioRight.windowed_samples = NULL;
			Signal->Blocks[n].audioRight.size = 0;
			Signal->Blocks[n].audioRight.difference = 0;
			Signal->Blocks[n].audioRight.padding = 0;
			Signal->Blocks[n].audioRight.sampleOffset = 0;

			Signal->Blocks[n].offset = 0;
			Signal->Blocks[n].loadSize = 0;
			Signal->Blocks[n].difference = 0;

			Signal->Blocks[n].SilenceSizeLeft = 0;
			Signal->Blocks[n].SilenceSizeRight = 0;

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
	Signal->SampleRate = 0;
	Signal->bytesPerSample = 0;
	Signal->numSamples = 0;
	Signal->SamplesStart = 0;
	Signal->samplesPosFLAC = 0;
	Signal->errorFLAC = 0;
	Signal->framerate = 0.0;
	memset(&Signal->header, 0, sizeof(wav_hdr));
	memset(&Signal->fmtExtra, 0, sizeof(uint8_t)*FMT_EXTRA_SIZE);
	Signal->fmtType = 0;
	memset(&Signal->fact, 0, sizeof(fact_ck));
	Signal->factExists = 0;

	Signal->startOffset = 0;
	Signal->endOffset = 0;

	memset(&Signal->MaxMagnitude, 0, sizeof(MaxMagn));

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

	memset(&Signal->delayArray, 0, sizeof(double)*DELAYCOUNT);
	Signal->delayElemCount = 0;

	Signal->balance = 0;
	memset(&Signal->clkFrequencies, 0, sizeof(AudioBlocks));
	Signal->originalCLK = 0;
	Signal->EstimatedSR = 0;
	Signal->originalSR = 0;

	Signal->EstimatedSR_CLK = 0;
	Signal->originalSR_CLK = 0;
	Signal->originalFrameRate = 0;
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
		AudioArray->internalSync[i].windowed_samples = NULL;
		AudioArray->internalSync[i].size = 0;
		AudioArray->internalSync[i].difference = 0;
		AudioArray->internalSync[i].padding = 0;
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
	AudioArray->fftwValues.ENBW = 0;
}

void ReleaseFFTW(AudioBlocks * AudioArray)
{
	if(!AudioArray)
		return;

	if(AudioArray->fftwValues.spectrum)
	{
		fftw_free(AudioArray->fftwValues.spectrum);
		AudioArray->fftwValues.spectrum = NULL;
	}

	if(AudioArray->fftwValuesRight.spectrum)
	{
		fftw_free(AudioArray->fftwValuesRight.spectrum);
		AudioArray->fftwValuesRight.spectrum = NULL;
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
	if(AudioArray->audio.windowed_samples)
	{
		free(AudioArray->audio.windowed_samples);
		AudioArray->audio.windowed_samples = NULL;
	}
	AudioArray->audio.size = 0;
	AudioArray->audio.difference = 0;
	AudioArray->audio.padding = 0;

	if(AudioArray->audioRight.samples)
	{
		free(AudioArray->audioRight.samples);
		AudioArray->audioRight.samples = NULL;
	}
	if(AudioArray->audioRight.windowed_samples)
	{
		free(AudioArray->audioRight.windowed_samples);
		AudioArray->audioRight.windowed_samples = NULL;
	}
	AudioArray->audioRight.size = 0;
	AudioArray->audioRight.difference = 0;
	AudioArray->audioRight.padding = 0;

	if(AudioArray->internalSync)
	{
		for(int i = 0; i < AudioArray->internalSyncCount; i++)
		{
			if(AudioArray->internalSync[i].samples)
			{
				free(AudioArray->internalSync[i].samples);
				AudioArray->internalSync[i].samples = NULL;
			}
			if(AudioArray->internalSync[i].windowed_samples)
			{
				free(AudioArray->internalSync[i].windowed_samples);
				AudioArray->internalSync[i].windowed_samples = NULL;
			}
			AudioArray->internalSync[i].size = 0;
			AudioArray->internalSync[i].difference = 0;
			AudioArray->internalSync[i].padding = 0;
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

	if(AudioArray->freqRight)
	{
		free(AudioArray->freqRight);
		AudioArray->freqRight = NULL;
	}
}

void ReleaseBlock(AudioBlocks * AudioArray)
{
	ReleaseFrequencies(AudioArray);
	CleanAndReleaseFFTW(AudioArray);
	ReleaseSamples(AudioArray);

	AudioArray->index = 0;
	AudioArray->type = TYPE_NOTYPE;
	AudioArray->seconds = 0;

	AudioArray->offset = 0;
	AudioArray->loadSize = 0;
	AudioArray->difference = 0;
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

double GetHigherFrameRate(double framerateA, double framerateB)
{
	if(framerateA < framerateB)
		return framerateB;

	return framerateA;
}

void CompareFrameRates(AudioSignal *Signal1, AudioSignal *Signal2, parameters *config)
{
	// Any difference needs to be accounted for
	if(Signal1->framerate == Signal2->framerate)
	{
		config->smallerFramerate = Signal1->framerate;
		config->biggerFramerate = Signal1->framerate;
	}
	else
	{
		double diff = 0;

		diff = fabs(Signal1->framerate - Signal2->framerate);
		config->smallerFramerate = GetLowerFrameRate(Signal1->framerate, Signal2->framerate);
		config->biggerFramerate  = GetHigherFrameRate(Signal1->framerate, Signal2->framerate);
		if((config->verbose && diff > 0.001) || diff > 0.1) {
			logmsg("\n= Different frame rates found (%.8g), compensating to %.8gms =\n", 
				diff, config->smallerFramerate);
		}
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
		config->smallerFramerate = GetLowerFrameRate(Signal->framerate, framerate);
		if(config->verbose && diff > 0.001) {
			logmsg("\n= Different frame rates found (%.8g), compensating to %.8gms =\n", 
				diff, config->smallerFramerate);
		}
		if(diff <= 0.0005) // this is a sync detection error
			Signal->framerate = framerate;
	}
}

long int GetSampleSizeDifferenceByFrameRate(double framerate, long int frames, double samplerate, int AudioChannels, parameters *config)
{
	long int difference = 0;

	if(config->smallerFramerate == 0)
		return 0;

	if(framerate > config->smallerFramerate)
	{
		long int SmallerSamples = 0;
		long int BiggerSamples = 0;

		SmallerSamples = SecondsToSamples(samplerate, FramesToSeconds(config->smallerFramerate, frames), AudioChannels, NULL, NULL);
		BiggerSamples = SecondsToSamples(samplerate, FramesToSeconds(framerate, frames), AudioChannels, NULL, NULL);
	
		difference = BiggerSamples - SmallerSamples;
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

int MatchesExtraDataColor(int pos, int type, parameters *config)
{
	if(!config)
		return 0;

	if(type <= TYPE_CONTROL)
		return 0;

	for(int i = 0; i < pos; i++)
	{
		if(config->types.typeArray[i].type == type)
		{
			if(MatchColor(config->types.typeArray[pos].color) !=
				MatchColor(config->types.typeArray[i].color))
					return i;
		}
	}
	
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

#define WATERMARK_SEARCH_PCT	(1.0/100.0)
#define WATERMARK_TOLERANCE		20.0

int DetectWatermark(AudioSignal *Signal, parameters *config)
{
	int watermark = -1, found = 0;
	double	WaterMarkValid = 0, WaterMarkInvalid = 0;
	double	freqSearch = config->MaxFreq * WATERMARK_SEARCH_PCT;

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
					Signal->header.fmt.NumOfChan, Signal->SampleRate, config);
	WaterMarkInvalid = FindFrequencyBracket(config->types.watermarkInvalidFreq, Signal->Blocks[watermark].fftwValues.size, 
					Signal->header.fmt.NumOfChan, Signal->SampleRate, config);

	for(int i = 0; i < freqSearch; i++)
	{
		if(Signal->Blocks[watermark].freq[i].hertz &&
			Signal->Blocks[watermark].freq[i].amplitude > config->significantAmplitude/2)
		{
			if(fabs(Signal->Blocks[watermark].freq[i].hertz - WaterMarkValid) < WATERMARK_TOLERANCE)
			{
				Signal->watermarkStatus = WATERMARK_VALID;
				found = 1;
				break;
			}
			if(fabs(Signal->Blocks[watermark].freq[i].hertz - WaterMarkInvalid) < WATERMARK_TOLERANCE)
			{
				Signal->watermarkStatus = WATERMARK_INVALID;
				logmsg(" - WARNING: %s signal was recorded with %s difference. Results are probably incorrect.\n",
						getRoleText(Signal), config->types.watermarkDisplayName);
				found = 1;
				break;
			}
		}
	}
	if(!found)
	{
		Signal->watermarkStatus = WATERMARK_INDETERMINATE;
		logmsg(" - WARNING: %s file has an unknown %s status. Results might be incorrect.\n",
				getRoleText(Signal), config->types.watermarkDisplayName);
	}

	return 1;
}

int DetectWatermarkIssue(char *msg, AudioSignal* Signal, parameters *config)
{
	if(!Signal)
	{
		sprintf(msg, "ERROR: Invalid watermark parameters!");
		return 1;
	}

	switch (Signal->watermarkStatus)
	{
		case WATERMARK_NONE:
		case WATERMARK_VALID:
			return 0;
			break;
		case WATERMARK_INVALID:
			sprintf(msg, "WARNING: Invalid %s in %s. Results are probably incorrect",
				config->types.watermarkDisplayName,
				Signal->role == ROLE_REF ? "Ref" : "Comp");
			return 1;
			break;
		case WATERMARK_INDETERMINATE:
			sprintf(msg, "WARNING: %s state unknown in %s. Results might be incorrect",
				config->types.watermarkDisplayName,
				Signal->role == ROLE_REF ? "Ref" : "Comp");
			return 1;
			break;
	}
		
	sprintf(msg, "ERROR: Invalid watermark status!");
	return 1;
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

double GetFirstSyncDuration(double framerate, parameters *config)
{
	int		first = 0;
	double	frames = 0;

	if(!config)
		return 0;

	first = GetFirstSyncIndex(config);
	if(first == NO_INDEX)
		return 0;
	frames = config->types.typeArray[first].elementCount * config->types.typeArray[first].frames;
	return(FramesToSeconds(frames, framerate));
}

double GetFirstElementFrameOffset(parameters *config)
{
	double 	frames = 0;

	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type > TYPE_CONTROL)
			break;
		else
			frames += config->types.typeArray[i].elementCount * config->types.typeArray[i].frames;
	}
	
	return(frames);
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

double GetFirstSilenceDuration(double framerate, parameters *config)
{
	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type == TYPE_SILENCE)
		{
			double	frames = 0;

			frames = config->types.typeArray[i].elementCount * config->types.typeArray[i].frames;
			return(FramesToSeconds(frames, framerate));
		}
	}
	return 0;
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

long int GetSecondSyncSilenceSampleOffset(double framerate, wav_hdr header, int frameAdjust, double silenceOffset, parameters *config)
{
	int silence_count = 0, i = 0, firstsyncfound = 0;

	if(!config)
		return 0;

	for(i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type == TYPE_SYNC)
		{
			if(!firstsyncfound)
				firstsyncfound = 1;
		}
		if(config->types.typeArray[i].type == TYPE_SILENCE)
		{
			if(!silence_count && firstsyncfound)  // first sync
				silence_count ++;
			else
			{
				if(i + 1 < config->types.typeCount && config->types.typeArray[i+1].type == TYPE_SYNC)
					silence_count ++;
			}
		}
		if(silence_count == 2)
		{
			double seconds = 0;
			long int offset = 0, length = 0;

			seconds = FramesToSeconds(GetBlockFrameOffset(i, config) - frameAdjust, framerate);
			offset = SecondsToSamples(header.fmt.SamplesPerSec, seconds, header.fmt.NumOfChan, NULL, NULL);

			seconds = FramesToSeconds(config->types.typeArray[i].frames*silenceOffset, framerate);
			length = SecondsToSamples(header.fmt.SamplesPerSec, seconds, header.fmt.NumOfChan, NULL, NULL);
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

long int GetElementFrameOffset(int block, parameters *config)
{
	int counter = 0;
	double offset = 0;

	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		for(int e = 0; e < config->types.typeArray[i].elementCount; e++)
		{
			offset += config->types.typeArray[i].frames;
			counter ++;
			if(counter == block)
				return offset;
		}
	}
	return 0;
}

long int GetLastSyncFrameOffset(parameters *config)
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

int getArrayIndexforType(int type, int *typeArray, int typeCount)
{
	for(int i = 0; i < typeCount; i++)
	{
		if(typeArray[i] == type)
			return i;
	}
#ifdef DEBUG
	logmsg("WARNING: getArrayIndexforType failed to match (%d/%d)\n", type, typeCount);
#endif
	return -1;
}

int isInArray(int element, int *elemArray, int elemCount)
{
	for(int i = 0; i < elemCount; i++)
	{
		if(elemArray[i] == element)
			return 1;
	}
	return 0;
}

// Returns amount of block types excluding control and silence
int GetActiveBlockTypesNoRepeatArray(int **types, parameters *config)
{
	int count = 0, norepeat = 0;

	if(!config || !types)
		return 0;

	*types = NULL;
	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type > TYPE_CONTROL && !config->types.typeArray[i].IsaddOnData)
			count ++;
	}

	*types = (int*)malloc(sizeof(int)*count);
	if(!(*types))
		return 0;

	memset(*types, 0, sizeof(int)*count);
	for(int i = 0; i < config->types.typeCount; i++)
	{
		int type = config->types.typeArray[i].type;
		if(type > TYPE_CONTROL && !config->types.typeArray[i].IsaddOnData)
		{
			if(!isInArray(type, *types, norepeat))
				(*types)[norepeat++] = type;
		}
	}

	return norepeat;
}

int GetActiveBlockTypesNoRepeat(parameters *config)
{
	int numTypes = 0, *types = NULL;

	numTypes = GetActiveBlockTypesNoRepeatArray(&types, config);
	if(numTypes)
		free(types);
	return numTypes;
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
		return NULL;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		elementsCounted += config->types.typeArray[i].elementCount;
		if(elementsCounted > pos)
			return(config->types.typeArray[i].typeName);
	}
	
	return NULL;
}

char* GetBlockDisplayName(parameters* config, int pos)
{
	int elementsCounted = 0;

	if (!config)
		return NULL;

	for (int i = 0; i < config->types.typeCount; i++)
	{
		elementsCounted += config->types.typeArray[i].elementCount;
		if (elementsCounted > pos)
			return(config->types.typeArray[i].typeDisplayName);
	}

	return NULL;
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

char GetBlockMaskType(parameters *config, int pos)
{
	int elementsCounted = 0;

	if(!config)
		return MASK_DEFAULT;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		elementsCounted += config->types.typeArray[i].elementCount;
		if(elementsCounted > pos)
			return(config->types.typeArray[i].maskType);
	}
	
	return MASK_DEFAULT;
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
	return "NO NAME";
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

int GetRemainingLengthFromElement(int pos, parameters *config)
{
	int frames = 0, index = 0;

	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(index > pos)
		{
			if(config->types.typeArray[i].type >= TYPE_SILENCE || 
				config->types.typeArray[i].type == TYPE_INTERNAL_KNOWN ||
				config->types.typeArray[i].type == TYPE_INTERNAL_UNKNOWN)
				frames += config->types.typeArray[i].elementCount *
						config->types.typeArray[i].frames;
		}
		index += config->types.typeArray[i].elementCount;
	}
	return frames;
}

Frequency FindNoiseBlockInsideOneStandardDeviation(AudioSignal *Signal, parameters *config)
{
	Frequency	cutOff, mean, sd;
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

	// Calculate Mean
	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(GetTypeChannel(config, type) == CHANNEL_NOISE)
		{
			for(int i = 0; i < config->MaxFreq; i++)
			{
				if(Signal->Blocks[block].freq[i].hertz && Signal->Blocks[block].freq[i].amplitude != NO_AMPLITUDE)
				{
					double hz, amp;
		
					hz = Signal->Blocks[block].freq[i].hertz;
					amp = fabs(Signal->Blocks[block].freq[i].amplitude);
		
					mean.hertz += hz;
					mean.amplitude += amp;
		
					count ++;
				}
			}
		}
	}

	if(!count)
		return cutOff;

	mean.hertz = mean.hertz/count;
	mean.amplitude = mean.amplitude/count;

	count = 0;
	// Calculate StandardDeviation
	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(GetTypeChannel(config, type) == CHANNEL_NOISE)
		{
			for(int i = 0; i < config->MaxFreq; i++)
			{
				if(Signal->Blocks[block].freq[i].hertz && Signal->Blocks[block].freq[i].amplitude != NO_AMPLITUDE)
				{
					double hz, amp;
			
					hz = Signal->Blocks[block].freq[i].hertz;
					amp = fabs(Signal->Blocks[block].freq[i].amplitude);
			
					sd.hertz += pow(hz - mean.hertz, 2);
					sd.amplitude += pow(amp - mean.amplitude, 2);
			
					count ++;
				}
			}
		}
	}

	if(!count)
		return cutOff;

	sd.hertz = sqrt(sd.hertz/(count-1));
	sd.amplitude = sqrt(sd.amplitude/(count-1));

	cutOff.hertz = mean.hertz+sd.hertz;
	cutOff.amplitude = -1.0*(mean.amplitude+sd.amplitude);

	if(config->verbose){
		logmsg("  - %s signal profile defined noise channel data:\n", 
			getRoleText(Signal));
		logmsg("      Standard deviation: %g dBFS [%g Hz] Mean: %g dBFS [%g Hz] Cutoff: %g dBFS [%g Hz]\n",
			sd.amplitude, sd.hertz,
			mean.amplitude, mean.hertz,
			cutOff.amplitude, cutOff.hertz);
	}

	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(GetTypeChannel(config, type) == CHANNEL_NOISE)
		{
			for(int i = 0; i < config->MaxFreq; i++)
			{
				if(Signal->Blocks[block].freq[i].hertz && Signal->Blocks[block].freq[i].amplitude != NO_AMPLITUDE)
				{
					double amp;
		
					amp = Signal->Blocks[block].freq[i].amplitude;
		
					if(amp <= cutOff.amplitude)
						outside ++;
				}
			}
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
		loudest.amplitude = CalculateAmplitude(loudest.magnitude, maxMagnitude);

		logmsg(" - %s signal noise floor: %g dBFS [%g Hz]\n", 
			getRoleText(Signal),
			loudest.amplitude,
			loudest.hertz);
	}
	else
		logmsg(" - Could not determine Noise floor\n");
}

long int GatherAllSilenceData(AudioSignal *Signal, Frequency **allData, Frequency *loudest, int *silenceBlocks, parameters *config)
{
	long int	freqCount = 0, count = 0;
	Frequency* data = NULL;

	if(!allData || !loudest || !silenceBlocks)
		return -1;

	*allData = NULL;
	*silenceBlocks = 0;

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		if(GetBlockType(config, b) == TYPE_SILENCE)
		{
			(*silenceBlocks)++;
			for(int i = 0; i < Signal->Blocks[b].SilenceSizeLeft; i++)
			{
				if(Signal->Blocks[b].freq[i].hertz && Signal->Blocks[b].freq[i].amplitude != NO_AMPLITUDE)
					freqCount++;
			}

			if(Signal->Blocks[b].freqRight)
			{
				for(int i = 0; i < Signal->Blocks[b].SilenceSizeRight; i++)
				{
					if(Signal->Blocks[b].freqRight[i].hertz && Signal->Blocks[b].freqRight[i].amplitude != NO_AMPLITUDE)
						freqCount++;
				}
			}
		}
	}

	if(!freqCount)
		return 0;

	data = (Frequency*)malloc(sizeof(Frequency)*freqCount);
	if(!data)
	{
		logmsg("- ERROR: Insuffient memory for Silence data\n");
		return -1;
	}

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		if(GetBlockType(config, b) == TYPE_SILENCE)
		{
			for(int i = 0; i < Signal->Blocks[b].SilenceSizeLeft; i++)
			{
				if(Signal->Blocks[b].freq[i].hertz && Signal->Blocks[b].freq[i].amplitude != NO_AMPLITUDE)
				{
					data[count] = Signal->Blocks[b].freq[i];
					if(data[count].amplitude > loudest->amplitude)
						*loudest = data[count];
					count++;
				}
			}

			if(Signal->Blocks[b].freqRight)
			{
				for(int i = 0; i < Signal->Blocks[b].SilenceSizeRight; i++)
				{
					if(Signal->Blocks[b].freqRight[i].hertz && Signal->Blocks[b].freqRight[i].amplitude != NO_AMPLITUDE)
					{
						data[count] = Signal->Blocks[b].freqRight[i];
						if(data[count].amplitude > loudest->amplitude)
							*loudest = data[count];
						count++;
					}
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
	if (size == -1)
	{
		logmsg(" - %s signal FAILED GatherAllSilenceData\n", getRoleText(Signal));
		return;
	}

	if (size == 0)  // Digitally generated file with perfect alignment and no noise
	{
		double selectedNoise = 0;

		selectedNoise = floor(GetSignalMinDBFS(Signal)*1.20);

		logmsg(" - %s signal is a digitally generated file, no noise found. Using %g dBFS\n",
			getRoleText(Signal), selectedNoise);

		if (Signal->role == ROLE_REF)
			config->referenceNoiseFloor = selectedNoise;

		Signal->floorAmplitude = selectedNoise;
		Signal->floorFreq = Signal->gridFrequency;  // we assign a default
		return;
	}

	if(loudestFreq.hertz && loudestFreq.amplitude != NO_AMPLITUDE)
	{
		logmsg(" - %s signal relative noise floor: %g Hz %g dBFS\n", 
			getRoleText(Signal),
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
			logmsg("[Vertical scan noise: %0.2f Hz %0.2f dBFS] ", gridFreq.hertz, gridFreq.amplitude);

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
				getRoleText(Signal),
				loudestFreq.hertz,
				loudestFreq.amplitude);
		return;
	}
	*/

	if(noiseFreq.hertz)
	{
		logmsg(" - %s Noise Channel relative floor: %g Hz %g dBFS\n",
			getRoleText(Signal),
			noiseFreq.hertz,
			noiseFreq.amplitude);
		if(noiseFreq.amplitude > loudestFreq.amplitude)
		{
			Signal->floorAmplitude = loudestFreq.amplitude;
			Signal->floorFreq = loudestFreq.hertz;
		}
		else
		{
			Signal->floorAmplitude = noiseFreq.amplitude;
			Signal->floorFreq = noiseFreq.hertz;
			config->channelWithLowFundamentals = 1;
		}
		return;
	}

	if(loudestFreq.hertz && loudestFreq.amplitude != NO_AMPLITUDE)
	{
		Signal->floorAmplitude = loudestFreq.amplitude;
		Signal->floorFreq = loudestFreq.hertz;
		return;
	}

	logmsg(" - No meaningful floor found, using the whole range for relative comparison\n");
}

/// Now only called by MDWave
void GlobalNormalize(AudioSignal *Signal, parameters *config)
{
	double		MaxMagnitude = 0;
	double		MaxFreq = 0;
	int			MaxBlock = -1;
	long int	size = 0;

	if(!Signal)
		return;

	// Find global peak 
	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(type >= TYPE_SILENCE)
		{
			size = GetBlockFreqSize(Signal, block, CHANNEL_LEFT, config);
			for(long int i = 0; i < size; i++)
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

			if(Signal->Blocks[block].freqRight)
			{
				size = GetBlockFreqSize(Signal, block, CHANNEL_RIGHT, config);
				for(long int i = 0; i < size; i++)
				{
					if(!Signal->Blocks[block].freqRight[i].hertz)
						break;
					if(Signal->Blocks[block].freqRight[i].magnitude > MaxMagnitude)
					{
						MaxMagnitude = Signal->Blocks[block].freqRight[i].magnitude;
						MaxFreq = Signal->Blocks[block].freqRight[i].hertz;
						MaxBlock = block;
					}
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
			size = GetBlockFreqSize(Signal, block, CHANNEL_LEFT, config);
			for(long int i = 0; i < size; i++)
			{
				if(!Signal->Blocks[block].freq[i].hertz)
					break;
	
				Signal->Blocks[block].freq[i].amplitude = 
					CalculateAmplitude(Signal->Blocks[block].freq[i].magnitude, MaxMagnitude);
			}

			if(Signal->Blocks[block].freqRight)
			{
				size = GetBlockFreqSize(Signal, block, CHANNEL_RIGHT, config);
				for(long int i = 0; i < size; i++)
				{
					if(!Signal->Blocks[block].freqRight[i].hertz)
						break;
		
					Signal->Blocks[block].freqRight[i].amplitude = 
						CalculateAmplitude(Signal->Blocks[block].freqRight[i].magnitude, MaxMagnitude);
				}
			}
		}
	}
}

void FindMaxMagnitude(AudioSignal *Signal, parameters *config)
{
	double		MaxMagnitude = 0;
	double		MaxFreq = 0;
	int			MaxBlock = -1;
	char		MaxChannel = CHANNEL_NONE;

	if(!Signal)
		return;

	// Find global peak
	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(type > TYPE_SILENCE || type == TYPE_WATERMARK)
		{
			for(long int i = 0; i < config->MaxFreq; i++)
			{
				if(!Signal->Blocks[block].freq[i].hertz)
					break;
				if(Signal->Blocks[block].freq[i].magnitude > MaxMagnitude)
				{
					MaxMagnitude = Signal->Blocks[block].freq[i].magnitude;
					MaxFreq = Signal->Blocks[block].freq[i].hertz;
					MaxBlock = block;
					MaxChannel = CHANNEL_LEFT;
				}
			}

			if(Signal->Blocks[block].freqRight)
			{
				for(long int i = 0; i < config->MaxFreq; i++)
				{
					if(!Signal->Blocks[block].freqRight[i].hertz)
						break;
					if(Signal->Blocks[block].freqRight[i].magnitude > MaxMagnitude)
					{
						MaxMagnitude = Signal->Blocks[block].freqRight[i].magnitude;
						MaxFreq = Signal->Blocks[block].freqRight[i].hertz;
						MaxBlock = block;	
						MaxChannel = CHANNEL_RIGHT;
					}
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

	if(config->verbose && MaxBlock != -1)
	{
		double seconds = 0;
		long int offset = 0;

		seconds = FramesToSeconds(GetElementFrameOffset(MaxBlock, config), Signal->framerate);
		offset = SecondsToSamples(Signal->SampleRate, seconds, Signal->header.fmt.NumOfChan, NULL, NULL);
		offset = SamplesForDisplay(offset, Signal->header.fmt.NumOfChan);

		logmsg(" - %s Max Magnitude found in %s# %d (%d) [ %c ] at %g Hz with %g (%g seconds/%ld samples)\n", 
					getRoleText(Signal),
					GetBlockName(config, MaxBlock), GetBlockSubIndex(config, MaxBlock),
					MaxBlock, MaxChannel, MaxFreq, MaxMagnitude,
					seconds, offset);
	}
}

void CalculateAmplitudes(AudioSignal *Signal, double ZeroDbMagReference, parameters *config)
{
	if(!Signal)
		return;

	//Calculate Amplitude in dBFS 
	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int			type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(type >= TYPE_SILENCE || type == TYPE_WATERMARK)
		{
			long int	size = 0;

			size = GetBlockFreqSize(Signal, block, CHANNEL_LEFT, config);
			for(long int i = 0; i < size; i++)
			{
				if(!Signal->Blocks[block].freq[i].hertz)
					break;
	
				if(Signal->Blocks[block].freq[i].magnitude)
				{
					Signal->Blocks[block].freq[i].amplitude =
						CalculateAmplitude(Signal->Blocks[block].freq[i].magnitude, ZeroDbMagReference);
				}
				else
					Signal->Blocks[block].freq[i].amplitude = NO_AMPLITUDE;
			}

			if(Signal->Blocks[block].freqRight)
			{
				size = GetBlockFreqSize(Signal, block, CHANNEL_RIGHT, config);
				for(long int i = 0; i < size; i++)
				{
					if(!Signal->Blocks[block].freqRight[i].hertz)
						break;
		
					if (Signal->Blocks[block].freqRight[i].magnitude)
						Signal->Blocks[block].freqRight[i].amplitude = 
							CalculateAmplitude(Signal->Blocks[block].freqRight[i].magnitude, ZeroDbMagReference);
					else
						Signal->Blocks[block].freqRight[i].amplitude = NO_AMPLITUDE;
				}
			}
		}
	}
}

void CleanMatched(AudioSignal *ReferenceSignal, AudioSignal *TestSignal, parameters *config)
{
	long int size = 0;

	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		size = GetBlockFreqSize(ReferenceSignal, block, CHANNEL_LEFT, config);
		for(long int i = 0; i < size; i++)
		{
			if(!ReferenceSignal->Blocks[block].freq[i].hertz)
				break;
			ReferenceSignal->Blocks[block].freq[i].matched = 0;	
		}

		if(ReferenceSignal->Blocks[block].freqRight)
		{
			size = GetBlockFreqSize(ReferenceSignal, block, CHANNEL_RIGHT, config);
			for(long int i = 0; i < size; i++)
			{
				if(!ReferenceSignal->Blocks[block].freqRight[i].hertz)
					break;
				ReferenceSignal->Blocks[block].freqRight[i].matched = 0;	
			}
		}
	}

	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		size = GetBlockFreqSize(TestSignal, block, CHANNEL_LEFT, config);
		for(int i = 0; i < size; i++)
		{
			if(!TestSignal->Blocks[block].freq[i].hertz)
				break;
			TestSignal->Blocks[block].freq[i].matched = 0;
		}

		if(TestSignal->Blocks[block].freqRight)
		{
			size = GetBlockFreqSize(TestSignal, block, CHANNEL_RIGHT, config);
			for(int i = 0; i < size; i++)
			{
				if(!TestSignal->Blocks[block].freqRight[i].hertz)
					break;
				TestSignal->Blocks[block].freqRight[i].matched = 0;
			}
		}
	}
}

void PrintFrequenciesBlockMagnitude(AudioSignal *Signal, Frequency *freq, parameters *config)
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

void PrintFrequenciesBlock(AudioSignal *Signal, Frequency *freq, long int size, int type, parameters *config)
{
	double	 significant = 0;

	if(!freq)
		return;

	significant = config->significantAmplitude;
	if(GetTypeChannel(config, type) == CHANNEL_NOISE)
	{
		if(significant > SIGNIFICANT_AMPLITUDE)
			significant = SIGNIFICANT_AMPLITUDE;
	}

	for(long int j = 0; j < size; j++)
	{
		if(type != TYPE_SILENCE && significant > freq[j].amplitude)
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
		logmsgFileOnly("==================== %s %s# %d (%d) ===================\n", 
				getRoleText(Signal), GetBlockName(config, block), GetBlockSubIndex(config, block), block);

		if(Signal->Blocks[block].freqRight)
			logmsgFileOnly("Left Channel:\n");
		PrintFrequenciesBlockMagnitude(Signal, Signal->Blocks[block].freq, config);
		if(Signal->Blocks[block].freqRight)
		{
			logmsgFileOnly("Right Channel:\n");
			PrintFrequenciesBlockMagnitude(Signal, Signal->Blocks[block].freqRight, config);
		}
	}
}

void PrintFrequencies(AudioSignal *Signal, parameters *config)
{
	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int			type = TYPE_NOTYPE;
		long int	size = 0;

		logmsgFileOnly("==================== %s %s# %d (%d) ===================\n", 
				getRoleText(Signal), GetBlockName(config, block), GetBlockSubIndex(config, block), block);

		type = GetBlockType(config, block);
		if(Signal->Blocks[block].freqRight)
			logmsgFileOnly("Left Channel:\n");
		size = GetBlockFreqSize(Signal, block, CHANNEL_LEFT, config);
		PrintFrequenciesBlock(Signal, Signal->Blocks[block].freq, size, type, config);
		if(Signal->Blocks[block].freqRight)
		{
			logmsgFileOnly("Right Channel:\n");
			size = GetBlockFreqSize(Signal, block, CHANNEL_RIGHT, config);
			PrintFrequenciesBlock(Signal, Signal->Blocks[block].freqRight, size, type, config);
		}
	}
}

/* check ProcessSamples in mdwave if changed, for reverse FFTW */
/* power spectral density(PSD) rms	*/
inline double CalculateMagnitude(fftw_complex *value, double factor)
{
	double r1 = 0;
	double i1 = 0;
	double magnitude = 0;

	r1 = creal(*value);
	i1 = cimag(*value);
	magnitude = (2*sqrt(r1*r1 + i1*i1))/factor;
	return magnitude;
}

inline double CalculatePhase(fftw_complex *value)
{
	double r1 = 0;
	double i1 = 0;
	double phase = 0;

	r1 = creal(*value);
	i1 = cimag(*value);
	phase = atan2(i1, r1)*180/M_PI;

	return phase;
}

inline double CalculateAmplitude(double magnitude, double MaxMagnitude)
{
	if(magnitude == 0.0 || MaxMagnitude == 0.0)
	{
#ifdef DEBUG
		logmsg("WARNING: Invalid data for CalculateAmplitude (%g/%g)\n", magnitude, MaxMagnitude);
#endif
		return NO_AMPLITUDE;
	}

	if(magnitude > MaxMagnitude)
	{
#ifdef DEBUG
		logmsg("WARNING: Invalid data for CalculateAmplitude (%g>%g)\n", magnitude, MaxMagnitude);
#endif
		return NO_AMPLITUDE;
	}

	return(CalculateAmplitudeInternal(magnitude, MaxMagnitude));
}

inline double CalculateAmplitudeInternal(double magnitude, double MaxMagnitude)
{
	return 20*log10(magnitude/MaxMagnitude);
}

inline double CalculatePCMMagnitude(double amplitude, double MaxMagnitude)
{
	double magnitude = 0;

	magnitude = MaxMagnitude*pow(10, amplitude/20);
	return magnitude;
}

inline double CalculateFrequency(double boxindex, double boxsize)
{
	double Hertz = 0;

	Hertz = boxindex/boxsize;
	
	return Hertz;
}

int FillFrequencyStructures(AudioSignal *Signal, AudioBlocks *AudioArray, parameters *config)
{
	char channel = CHANNEL_LEFT;

	if(Signal && Signal->AudioChannels == 2 && AudioArray->channel == CHANNEL_STEREO)
	{
		channel = CHANNEL_RIGHT;
		if(!FillFrequencyStructuresInternal(Signal, AudioArray, channel, config))
			return 0;
		channel = CHANNEL_LEFT;
	}
	if(!FillFrequencyStructuresInternal(Signal, AudioArray, channel, config))
		return 0;
	ReleaseFFTW(AudioArray);
	return 1;
}

int FillFrequencyStructuresInternal(AudioSignal *Signal, AudioBlocks *AudioArray, char channel, parameters *config)
{
	long int 		i = 0, startBin= 0, endBin = 0, count = 0, size = 0, amount = 0;
	double 			boxsize = 0;
	int				nyquistLimit = 0;
	long int		*SilenceSize = NULL;
	double			ENBW = 0;
	Frequency		*f_array = NULL, **targetFreq = NULL;
	FFTWSpectrum	*fftw = NULL;

	if(channel == CHANNEL_LEFT)
	{
		fftw = &AudioArray->fftwValues;
		targetFreq = &AudioArray->freq;
		SilenceSize = &AudioArray->SilenceSizeLeft;
	}
	else
	{
		fftw = &AudioArray->fftwValuesRight;
		targetFreq = &AudioArray->freqRight;
		SilenceSize = &AudioArray->SilenceSizeRight;
	}
	size = fftw->size;
	ENBW = fftw->ENBW;

	if(!size || !fftw->spectrum || !targetFreq || !(*targetFreq) || !ENBW)
	{
		logmsg("ERROR: Invalid FillFrequencyStructures params\n");
		return 0;
	}

	if(AudioArray->seconds == 0.0)
	{
		logmsg("ERROR: FillFrequencyStructures seconds == 0\n");
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

	for(i = startBin; i < endBin; i++)
	{
		f_array[count].hertz = CalculateFrequency(i, boxsize);
		f_array[count].magnitude = CalculateMagnitude(&fftw->spectrum[i], ENBW);
		f_array[count].amplitude = NO_AMPLITUDE;
		f_array[count].phase = CalculatePhase(&fftw->spectrum[i]);
		f_array[count].matched = 0;
		count++;
	}

	if(config->MaxFreq > count)
		amount = count;
	else
		amount = config->MaxFreq;

	// Sort the array by top magnitudes
	FFT_Frequency_Magnitude_tim_sort(f_array, count);

	if(AudioArray->type != TYPE_SILENCE)
	{
		// Only copy Top amount frequencies
		memcpy(*targetFreq, f_array, sizeof(Frequency)*amount);

		// release temporal storage
		free(f_array);
		f_array = NULL;
	}
	else
	{
		// We use the whole frequency range for Noise floor analysis
		free(*targetFreq);
		*targetFreq = f_array;
		*SilenceSize = count;	
	}

	return 1;
}

void PrintComparedBlocks(AudioBlocks *ReferenceArray, AudioBlocks *ComparedArray, parameters *config)
{
	if(ReferenceArray->freqRight)
		logmsgFileOnly("LEFT Channel\n");
	
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
				if(areDoublesEqual(ReferenceArray->freq[j].amplitude, 
									ComparedArray->freq[match].amplitude))
					logmsgFileOnly("F+");
				else
					logmsgFileOnly("F-");
			}
			else
				logmsgFileOnly("XX");
			logmsgFileOnly("\n");
		}
	}

	if(ReferenceArray->freqRight)
	{
		logmsgFileOnly("RIGHT Channel\n");
		for(int j = 0; j < config->MaxFreq; j++)
		{
			if(config->significantAmplitude > ReferenceArray->freqRight[j].amplitude)
				break;
	
			if(ReferenceArray->freqRight[j].hertz)
			{
				int match = 0;
	
				logmsgFileOnly("[%5d] Ref: %7g Hz %6.4f dBFS [>%3d]", 
							j,
							ReferenceArray->freqRight[j].hertz,
							ReferenceArray->freqRight[j].amplitude,
							ReferenceArray->freqRight[j].matched - 1);
	
				if(ComparedArray->freqRight[j].hertz)
					logmsgFileOnly("\tComp: %7g Hz %6.4f dBFS [<%3d]", 
							ComparedArray->freqRight[j].hertz,
							ComparedArray->freqRight[j].amplitude,
							ComparedArray->freqRight[j].matched - 1);
				else
					logmsgFileOnly("\tCompared:\tNULL");
				match = ReferenceArray->freqRight[j].matched - 1;
				if(match != -1)
				{
					if(areDoublesEqual(ReferenceArray->freqRight[j].amplitude,
										ComparedArray->freqRight[match].amplitude))
						logmsgFileOnly("F+");
					else
						logmsgFileOnly("F-");
				}
				else
					logmsgFileOnly("XX");
				logmsgFileOnly("\n");
			}
		}
	}
	logmsgFileOnly("\n\n");
}

void PrintThesholdDifferenceBlocks(AudioBlocks *ReferenceArray, AudioBlocks *ComparedArray, parameters *config, double threshold)
{
	if(ReferenceArray->freqRight)
		logmsgFileOnly("LEFT Channel\n");
	
	/* changed Magnitude->amplitude */
	for(int j = 0; j < config->MaxFreq; j++)
	{
		if(config->significantAmplitude > ReferenceArray->freq[j].amplitude)
			break;

		if(ReferenceArray->freq[j].hertz)
		{
			int matchedto = 0;

			matchedto = ReferenceArray->freq[j].matched - 1;
			if(matchedto >= 0 && 
					fabs(ReferenceArray->freq[j].amplitude - ComparedArray->freq[matchedto].amplitude) > threshold)
			{
				logmsgFileOnly("[%5d] Ref: %7g Hz %6.4f dB [%+0.3g dBFS]", 
							j,
							ReferenceArray->freq[j].hertz,
							ReferenceArray->freq[j].amplitude,
							ReferenceArray->freq[j].matched - 1 >= 0 ? ReferenceArray->freq[j].amplitude - ComparedArray->freq[ReferenceArray->freq[j].matched - 1].amplitude : -1000);
	
				if(ComparedArray->freq[matchedto].hertz)
					logmsgFileOnly("\tComp: %7g Hz %6.4f dBFS", 
							ComparedArray->freq[matchedto].hertz,
							ComparedArray->freq[matchedto].amplitude);
				else
					logmsgFileOnly("\tCompared:\tNULL");
				
				logmsgFileOnly("\n");
			}
		}
	}

	if(ReferenceArray->freqRight)
	{
		logmsgFileOnly("RIGHT Channel\n");
		for(int j = 0; j < config->MaxFreq; j++)
		{
			if(config->significantAmplitude > ReferenceArray->freqRight[j].amplitude)
				break;
	
			if(ReferenceArray->freqRight[j].hertz)
			{
				int matchedto = 0;
	
				matchedto = ReferenceArray->freq[j].matched - 1;
				if(matchedto >= 0 && 
					fabs(ReferenceArray->freqRight[j].amplitude - ComparedArray->freqRight[matchedto].amplitude) > threshold)
				{
					logmsgFileOnly("[%5d] Ref: %7g Hz %6.4f dBFS [%+0.3g dBFS]", 
								j,
								ReferenceArray->freqRight[j].hertz,
								ReferenceArray->freqRight[j].amplitude,
								ReferenceArray->freqRight[j].matched - 1 >= 0 ? ReferenceArray->freqRight[j].amplitude - ComparedArray->freqRight[ReferenceArray->freqRight[j].matched - 1].amplitude : -1000);
		
					if(ComparedArray->freqRight[matchedto].hertz)
						logmsgFileOnly("\tComp: %7g Hz %6.4f dBFS", 
								ComparedArray->freqRight[matchedto].hertz,
								ComparedArray->freqRight[matchedto].amplitude);
					else
						logmsgFileOnly("\tCompared:\tNULL");
					logmsgFileOnly("\n");
				}
			}
		}
	}
	logmsgFileOnly("\n\n");
}

double CalculateWeightedError(double pError, parameters *config)
{
	int option = 0;

	if(pError < 0.0)  // this should never happen
	{
		if(!config->pErrorReport)
			logmsg("WARNING: pERROR < 0! (%g)\n", pError);
		config->pErrorReport++;

		pError = fabs(pError);
		if(pError > 1)
		{
			if(!config->pErrorReport)
				logmsg("WARNING: pERROR > 1! (%g)\n", pError);
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
			logmsg("WARNING: CalculateWeightedError, out of range value %d\n", option);
			//pError = pError;
			break;
	}

	//map to a sub range so that we always have color when within range
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

inline long int SamplesForDisplay(long int samples, int AudioChannels)
{
	return(samples/AudioChannels);
}

inline long int SamplesToBytes(long int samples, int bytesPerSample)
{
	return(samples*bytesPerSample);
}

inline double FramesToSeconds(double frames, double framerate)
{
	return(frames*framerate/1000.0);
}

inline double SecondsToFrames(double seconds, double framerate)
{
	return((seconds*1000.0)/framerate);
}

inline long int SecondsToSamples(double samplerate, double seconds, int AudioChannels, int *discard, double *leftDecimals)
{
	return(RoundToNsamples(samplerate*(double)AudioChannels*seconds, AudioChannels, discard, leftDecimals));
}

inline double SamplesToSeconds(double samplerate, long int samples, int AudioChannels)
{
	return((double)samples/(samplerate*(double)AudioChannels));
}

inline double FramesToSamples(double frames, double samplerate, double framerate)
{
	return(floor(samplerate/1000.0*frames*framerate));
}

inline double SamplesToFrames(double samplerate, long int samples, double framerate, int AudioChannels)
{
	return(roundFloat(((double)samples*1000.0)/(samplerate*(double)AudioChannels*framerate)));
}

inline double BytesToSamples(long int samples, int bytesPerSample)
{
	return(samples/bytesPerSample);
}

/* This function compensates for sub sample frame rate inconsistencies between signals */
/* compensation values are stored in leftover discard left Decimals after adjusting the */
/* return value to the closest floored sample requested */
long int RoundToNsamples(double src, int AudioChannels, int *discard, double *leftDecimals)
{
	int extra = 0, roundSamples = 0;

	if(discard)
		*discard = 0;

	if(leftDecimals)
		*leftDecimals += GetDecimalValues(src);
	
	src = floor(src);

	roundSamples = AudioChannels;
	extra = ((long int)src) % roundSamples;
	if(leftDecimals && discard)
	{
		src -= extra;
		(*leftDecimals) += extra;
		if(*leftDecimals > (double)roundSamples
			|| areDoublesEqual(*leftDecimals, roundSamples))  // this allows "negative" values when close enough
		{
			*leftDecimals -= roundSamples;
			*discard = roundSamples;
		}
	}
	else
	{
		if(extra != 0)
			src += roundSamples - extra;
	}

	return(src);
}

inline double GetDecimalValues(double value)
{
	double integer = 0;

	value = modf(value, &integer);
	return value;
}

// Sets the time length to use in order to match uneven blocks
// so that power/amplitude is kept when using differennt frame length blocks
void SetAmplitudeMatchByDuration(__attribute__((unused))AudioSignal *reference, parameters *config)
{
	if(!config->padBlockSizes)
		return;

	// we use the bigger framerate so that when padding, everything is covered
	config->maxBlockSeconds = FramesToSeconds(config->biggerFramerate, config->maxBlockFrameCount);
}

void SetAmplitudeMatchByDurationMDW(AudioSignal *reference, parameters *config)
{
	if(!config->padBlockSizes)
		return;

	config->maxBlockSeconds = FramesToSeconds(reference->framerate, config->maxBlockFrameCount);
}

// Zero padding to make all blocks the same frame size
long int GetBlockZeroPadValues(long int *monoSignalSize, double *seconds, double maxBlockSeconds, double samplerate)
{
	long int padding = 0;

	if(!monoSignalSize || !seconds || maxBlockSeconds == *seconds)
		return padding;

	// It has to be lower or equal, since by definition block size is the biggest block
	// It is only longer for TYPE_SILENCE blocks, but we don't know that at this level
	// so we ignore it for such case and apply it elsewhere. 
	if (maxBlockSeconds > *seconds)
	{
		padding = SecondsToSamples(samplerate, maxBlockSeconds - *seconds, 1, NULL, NULL);
		*monoSignalSize += padding;
		*seconds = maxBlockSeconds;
	}
	else
		logmsgFileOnly("WARNING: Block length longer than max block lentgth %g > %g (might be normal with silence being longer than the regular blocks)\n", *seconds, maxBlockSeconds);

	return padding;
}

// Zero pad to 1 second for 1hz alignment of bins
long int GetZeroPadValues(long int *monoSignalSize, double *seconds, double samplerate)
{
	long int zeropadding = 0;

	// Samplerate CHECK
	if(!monoSignalSize || !seconds)
		return zeropadding;

	// Align frequency bins to 1Hz
	if(*monoSignalSize != (long int)samplerate)
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

			times = ceil((double)*monoSignalSize/samplerate);
			zeropadding = times*samplerate - *monoSignalSize;
			*monoSignalSize += zeropadding;
			*seconds = times;
		}
	}
	return zeropadding;
}

int ApplySamplerateChange(AudioSignal *Signal, parameters *config)
{
	if(config->doSamplerateAdjust == 'b' ||
			(config->doSamplerateAdjust == 'r' && Signal->role == ROLE_REF) ||
			(config->doSamplerateAdjust == 'c' && Signal->role == ROLE_COMP))
		return 1;
	return 0;
}

double CalculateFrameRateAndCheckSamplerate(AudioSignal *Signal, parameters *config)
{
	double framerate = 0, endOffset = 0, startOffset = 0;
	double expectedFR = 0, diffFR = 0, SRDifference = 0;
	double calculatedSamplerate = 0, LastSyncFrameOffset = 0;
	double centsDifferenceSR = 0;

	startOffset = Signal->startOffset;
	endOffset = Signal->endOffset;
	expectedFR = GetMSPerFrame(Signal, config);
	LastSyncFrameOffset = GetLastSyncFrameOffset(config);

	if(startOffset == endOffset)
	{
		if(config->verbose)
			logmsg("Start Offset is equal to End Offset, no signal detected.\n");
		return 0;
	}
	if(startOffset > endOffset)
	{
		if(config->verbose)
			logmsg("Start Offset bigger than End Offset, how did that happen.\n");
		return 0;
	}
	if(!LastSyncFrameOffset)
	{
		if(config->verbose)
			logmsg("Could not get last sync offset in frames.\n");
		return 0;
	}

	framerate = CalculateFrameRate(Signal, config);

	diffFR = roundFloat(fabs(expectedFR - framerate));

	calculatedSamplerate = (endOffset-startOffset)/(expectedFR*LastSyncFrameOffset);
	calculatedSamplerate = calculatedSamplerate*1000.0/(double)Signal->AudioChannels;

	centsDifferenceSR = 1200.0*log2(Signal->SampleRate/calculatedSamplerate);
	SRDifference = calculatedSamplerate - Signal->SampleRate;

#ifdef DEBUG
	Signal->EstimatedSR = RoundFloat(calculatedSamplerate, 0);
#endif
	/* This code detects the exact numer of samples the signal is off, will enable later */
	if(config->verbose)
	{
		double tDiff = 0, samples = 0; /*, sign = 1; */

		tDiff = fabs(expectedFR*LastSyncFrameOffset) - fabs(framerate*LastSyncFrameOffset);
		samples = -1 * calculatedSamplerate / 1000.0 * tDiff;
		//sign = samples > 0 ? -1 : 1;
		//samples = ceil(RoundFloat(fabs(samples), 2));

		if(fabs(samples) > 0.70)
		{
			logmsg(" - WARNING Expected %.12gms and got %.12gms (Difference %gms/%g samples/%g frames of %gms) [%ld-%ld]\n", 
				expectedFR*LastSyncFrameOffset, framerate*LastSyncFrameOffset,
				-1*tDiff, samples,
				-1*tDiff/expectedFR,
				expectedFR,
				SamplesForDisplay(Signal->startOffset, Signal->AudioChannels),
				SamplesForDisplay(Signal->endOffset, Signal->AudioChannels));
		}

		/*
		if(1 && fabs(samples) > 0.70 && fabs(samples) < 10.0)
		{
			//logmsg("WARNING: Endoffset altered by %g %d channel samples from %ld samples\n", sign*samples, Signal->AudioChannels, SamplesForDisplay((long)endOffset, Signal->AudioChannels));
			endOffset += sign*samples*Signal->AudioChannels;
			
			Signal->endOffset = (long int)endOffset;

			framerate = CalculateFrameRate(Signal, config);
			diffFR = roundFloat(fabs(expectedFR - framerate));

			calculatedSamplerate = (endOffset-startOffset)/(expectedFR*LastSyncFrameOffset);
			calculatedSamplerate = calculatedSamplerate*1000.0/(double)Signal->AudioChannels;
			//calculatedSamplerate = RoundFloat(calculatedSamplerate, 5); 

			centsDifferenceSR = 1200.0*log2(calculatedSamplerate/(double)Signal->SampleRate);
			SRDifference = calculatedSamplerate - Signal->SampleRate;

			tDiff = fabs(expectedFR*LastSyncFrameOffset) - fabs(framerate*LastSyncFrameOffset);
			samples = -1 * calculatedSamplerate / 1000.0 * tDiff;

			tDiff = RoundFloat(tDiff, 5);
			samples = RoundFloat(samples, 5);
			expectedFR = RoundFloat(expectedFR, 5);

			logmsg(" - WARNING RECALC start %ld end %ld, Expected %gms and got %gms (Difference is %gms/%g samples/%g frames of %gms)\n", 
				SamplesForDisplay(Signal->startOffset, Signal->AudioChannels),
				SamplesForDisplay(Signal->endOffset, Signal->AudioChannels),
				expectedFR*LastSyncFrameOffset, framerate*LastSyncFrameOffset,
				-1*tDiff, samples,
				-1*tDiff/expectedFR,
				expectedFR);
		}
		*/
	}

	if(areDoublesEqual(fabs(SRDifference), 0.0))  // do not make adjustments if they make no change at all 
		return framerate;

	if(config->doSamplerateAdjust != 'n' && fabs(centsDifferenceSR) > 0)
	{
		if(ApplySamplerateChange(Signal, config))
		{
			logmsg(" - NOTE: Auto adjustment of samplerate and frame rate applied\n");
			logmsg("    Original framerate: %gms (%g difference) detected %fHz (%gHz difference) sample rate from audio duration\n", framerate, diffFR, calculatedSamplerate, SRDifference);

			Signal->originalSR = Signal->SampleRate;
			Signal->originalFrameRate = framerate;
;
			framerate = (endOffset-startOffset)/(calculatedSamplerate *LastSyncFrameOffset);
			framerate = framerate*1000.0/(double)Signal->AudioChannels;  // 1000 ms

			// Samplerate CHECK, removed all rounding here
			Signal->SampleRate = calculatedSamplerate;
			logmsg("    Adjusted sample rate to %.12gHz, compensating for a pitch difference of %g cents\n", Signal->SampleRate, centsDifferenceSR);

			Signal->EstimatedSR = Signal->SampleRate;
		}
		else
		{
			logmsg(" - %s: %s file framerate difference is %g ms per frame.\n",
					fabs(centsDifferenceSR) >= MAX_CENTS_DIFF ? "WARNING" : "INFO",
					getRoleText(Signal), diffFR);
			logmsg(" - Estimated sample rate seems off: %fhz calculated from signal duration\n\tThe source might have a timing difference.\n", calculatedSamplerate);
			if(config->verbose)
			{
				double tDiff = 0;

				tDiff = fabs(expectedFR*LastSyncFrameOffset) - fabs(framerate*LastSyncFrameOffset);
				logmsg(" - Expected %.10gms and got %gms (Difference is %gms/%g samples/%g frames of %gms)\n", 
					expectedFR*LastSyncFrameOffset, framerate*LastSyncFrameOffset,
					-1*tDiff, -1 * calculatedSamplerate / 1000.0 * tDiff,
					-1*tDiff/expectedFR,
					expectedFR);
			}

			/*
			if(fabs(centsDifferenceSR) >= MAX_CENTS_DIFF)
			{
				Signal->EstimatedSR = calculatedSamplerate;
				Signal->originalSR = Signal->SampleRate;
			}
			*/

			logmsg(" - There might be a pitch difference of %g cents\n", centsDifferenceSR);
		}
		
		if(fabs(centsDifferenceSR) >= MAX_CENTS_DIFF || ApplySamplerateChange(Signal, config))
		{
			if(Signal->role == ROLE_REF)
				config->RefCentsDifferenceSR = centsDifferenceSR;
			else
				config->ComCentsDifferenceSR = centsDifferenceSR;
			
			config->SRNoMatch |= Signal->role;
		}
	}

	return framerate;
}

double CalculateFrameRate(AudioSignal *Signal, parameters *config)
{
	double		framerate = 0;
	double		endOffset = 0, startOffset = 0, samplerate = 0;
	double		LastSyncFrameOffset = 0;

	startOffset = Signal->startOffset;
	endOffset = Signal->endOffset;
	samplerate = Signal->SampleRate;
	LastSyncFrameOffset = GetLastSyncFrameOffset(config);

	if(startOffset == endOffset)
		return 0;
	if(!LastSyncFrameOffset)
		return 0;

	framerate = (endOffset-startOffset)/(samplerate*LastSyncFrameOffset); // 1000 ms 
	framerate = framerate*1000.0/(double)Signal->AudioChannels;  // 1000 ms
	//framerate = roundFloat(framerate);

	return framerate;
}

double CalculateFrameRateNS(AudioSignal *Signal, double Frames, parameters *config)
{
	double framerate = 0, endOffset = 0, startOffset = 0, samplerate = 0;
	double expectedFR = 0, diff = 0;

	startOffset = Signal->startOffset;
	endOffset = Signal->endOffset;
	samplerate = Signal->SampleRate;
	expectedFR = GetMSPerFrame(Signal, config);

	framerate = (endOffset-startOffset)/(samplerate*Frames);
	framerate = framerate*1000.0/Signal->AudioChannels;  // 1000 ms
	//framerate = roundFloat(framerate);

	diff = roundFloat(fabs(expectedFR - framerate));
	//The range to report this on will probably vary by console...
	if(config->verbose && diff > 0.001) //  && diff < 0.02)
	{
		double calculatedSamplerate = 0;

		calculatedSamplerate = (endOffset-startOffset)/(expectedFR*Frames);
		calculatedSamplerate = calculatedSamplerate*1000.0/Signal->AudioChannels;
		logmsg(" - %s file framerate difference is %g.\n\tAudio card sample rate estimated at %g\n",
				getRoleText(Signal),
				diff, calculatedSamplerate);
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

char *GetFileName(int role, parameters *config)
{
	if(role == ROLE_REF)
		return(config->referenceFile);
	else
		return(config->comparisonFile);
}

double CalculateClk(AudioSignal *Signal, parameters *config)
{
	int i = 0, highestWithinRange = -1;
	double target = 0, tolerance = 0.05;

	if(!config->clkMeasure)
		return 0;

	if(!Signal || !Signal->clkFrequencies.freq)
		return 0;

	// Discard harmonics in the clock, we use 5% tolerance
	target = config->clkFreq * config->clkRatio;
	for(i = 0; i < config->MaxFreq; i++)
	{
		if(fabs(Signal->clkFrequencies.freq[i].hertz*config->clkRatio - target) < tolerance*target)
		{
			highestWithinRange = i;
			break;
		}
	}

	// Not found
	if(highestWithinRange == -1)
	{
		config->clkNotFound |= Signal->role;
		return Signal->clkFrequencies.freq[0].hertz * config->clkRatio;
	}
	if(highestWithinRange != 0)
		config->clkWarning |= Signal->role;

	return Signal->clkFrequencies.freq[highestWithinRange].hertz * config->clkRatio;
}

double FindMaxMagnitudeCLKSignal(AudioSignal *Signal, parameters *config)
{
	double		MaxMagnitude = -1;

	// Find global peak
	for(int i = 0; i < config->MaxFreq; i++)
	{
		if(!Signal->clkFrequencies.freq[i].hertz)
			break;
		if(Signal->clkFrequencies.freq[i].magnitude > MaxMagnitude)
			MaxMagnitude = Signal->clkFrequencies.freq[i].magnitude;
	}
	
	return MaxMagnitude;
}

void CalculateSignalCLKAmplitudes(AudioSignal *Signal, double ZeroMagCLK, parameters *config)
{
	if(config->clkMeasure && Signal->clkFrequencies.freq)
	{
		for(int i = 0; i < config->MaxFreq; i++)
		{
			if(!Signal->clkFrequencies.freq[i].hertz)
				break;

			Signal->clkFrequencies.freq[i].amplitude = 
				CalculateAmplitude(Signal->clkFrequencies.freq[i].magnitude, ZeroMagCLK);
		}
	}

	if(config->verbose)
	{
		logmsgFileOnly("==================== CLK-FREQ %s ===================\n", 
				config->clkName);
		PrintFrequenciesBlock(Signal, Signal->clkFrequencies.freq, config->MaxFreq, TYPE_CLK_ANALYSIS, config);
	}
}

int CalculateCLKAmplitudes(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config)
{
	double ZeroMagCLK = 0, MaxRef = 0, MaxCom = 0;

	if(!config->clkMeasure || !ReferenceSignal->clkFrequencies.freq || !ComparisonSignal->clkFrequencies.freq)
		return 0;

	MaxRef = FindMaxMagnitudeCLKSignal(ReferenceSignal, config);
	MaxCom = FindMaxMagnitudeCLKSignal(ComparisonSignal, config);

	if(MaxRef == -1 || MaxCom == -1)
	{
		logmsg("WARNING: Could not calculate CLK amplitudes\n");
		return 0;
	}

	if(MaxRef > MaxCom)
		ZeroMagCLK = MaxRef;
	else
		ZeroMagCLK = MaxCom;

	CalculateSignalCLKAmplitudes(ReferenceSignal, ZeroMagCLK, config);
	CalculateSignalCLKAmplitudes(ComparisonSignal, ZeroMagCLK, config);
	return 1;
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

	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if((type > TYPE_SILENCE || type == TYPE_WATERMARK) && Signal->Blocks[block].freq[0].hertz != 0 &&
			Signal->Blocks[block].freq[0].amplitude != NO_AMPLITUDE)
		{
			AvgFundAmp += Signal->Blocks[block].freq[0].amplitude;
			count ++;
		}
	}

	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		if(Signal->Blocks[block].freqRight)
		{
			int type = TYPE_NOTYPE;

			type = GetBlockType(config, block);
			if((type > TYPE_SILENCE || type == TYPE_WATERMARK) && Signal->Blocks[block].freqRight[0].hertz != 0  &&
				Signal->Blocks[block].freqRight[0].amplitude != NO_AMPLITUDE)
			{
				AvgFundAmp += Signal->Blocks[block].freqRight[0].amplitude;
				count ++;
			}
		}
	}

	if(count)
		AvgFundAmp /= count;

	if(config->verbose) {
		logmsg(" - %s signal Average Fundamental Amplitude %g dBFS from %ld elements\n", 
				getRoleText(Signal),
				AvgFundAmp, count);
	}

	return AvgFundAmp;
}

double FindNoiseFloorAmplitudeAverage(AudioSignal *Signal, parameters *config)
{
	double		AvgFundAmp = 0;
	long int	count = 0, freqUse = 0;

	if(!Signal)
		return 0;

	freqUse = config->MaxFreq;
	if(config->MaxFreq > 50)  // et a limit so the average makes some sense, higher 50 frequencies here
		freqUse = 50;

	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(type == TYPE_SILENCE)
		{
			for(int i = 0; i < freqUse; i++)
			{
				if(Signal->Blocks[block].freq[i].hertz && Signal->Blocks[block].freq[i].amplitude != NO_AMPLITUDE &&
					Signal->Blocks[block].freq[i].amplitude > Signal->floorAmplitude - 30) // we set a limit in amplitude as well
				{
					AvgFundAmp += Signal->Blocks[block].freq[i].amplitude;
					count ++;
					//logmsg("%g dBFS @ %g\n", Signal->Blocks[block].freq[i].amplitude, Signal->Blocks[block].freq[i].hertz);
				}
			}
		}
	}

	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		if(Signal->Blocks[block].freqRight)
		{
			int type = TYPE_NOTYPE;

			type = GetBlockType(config, block);
			if(type == TYPE_SILENCE)
			{
				for(int i = 0; i < freqUse; i++)
				{
					if(Signal->Blocks[block].freqRight[i].hertz && Signal->Blocks[block].freqRight[i].amplitude != NO_AMPLITUDE &&
						Signal->Blocks[block].freq[i].amplitude > Signal->floorAmplitude - 30)
					{
						AvgFundAmp += Signal->Blocks[block].freqRight[i].amplitude;
						count ++;
						//logmsg("%g dBFS @ %g\n", Signal->Blocks[block].freqRight[i].amplitude, Signal->Blocks[block].freqRight[i].hertz);
					}
				}
			}
		}
	}

	if(count)
	{
		AvgFundAmp /= count;
		logmsg(" - %s signal Noise Floor Average Amplitude %g dBFS\n", 
			getRoleText(Signal), AvgFundAmp);
	}

	return AvgFundAmp;
}

inline long int GetSignalMaxInt(AudioSignal *Signal)
{
	long int Max = 0;

	if(Signal->header.fmt.AudioFormat == WAVE_FORMAT_IEEE_FLOAT)
		Max = 1.0;	
	else
	{
		switch(Signal->bytesPerSample)
		{
			case 1:
				Max = MAXINT8;
				break;
			case 2:
				Max = MAXINT16;
				break;
			case 3:
				Max = MAXINT24;
				break;
			case 4:
				Max = MAXINT32;
				break;
			default:
				logmsg("Invalid bit depth (%dbits)\n", Signal->header.fmt.bitsPerSample);
				break;
		}
	}
	return Max;
}

inline long int GetSignalMinInt(AudioSignal *Signal)
{
	long int Min = 0;

	if(Signal->header.fmt.AudioFormat == WAVE_FORMAT_IEEE_FLOAT)
		Min = -1.0;	
	else
	{
		switch(Signal->bytesPerSample)
		{
			case 1:
				Min = MININT8;
				break;
			case 2:
				Min = MININT16;
				break;
			case 3:
				Min = MININT24;
				break;
			case 4:
				Min = MININT32;
				break;
			default:
				logmsg("Invalid bit depth (%dbits)\n", Signal->header.fmt.bitsPerSample);
				break;
		}
	}
	return Min;
}

inline double GetSignalMinDBFS(AudioSignal *Signal)
{
	double Min = 0;

	if(Signal->header.fmt.AudioFormat == WAVE_FORMAT_IEEE_FLOAT)
		Min = PCM_32BIT_MIN_AMPLITUDE;	
	else
	{
		switch(Signal->bytesPerSample)
		{
			case 1:
				Min = PCM_8BIT_MIN_AMPLITUDE;
				break;
			case 2:
				Min = PCM_16BIT_MIN_AMPLITUDE;
				break;
			case 3:
				Min = PCM_24BIT_MIN_AMPLITUDE;
				break;
			case 4:
				Min = PCM_32BIT_MIN_AMPLITUDE;
				break;
			default:
				logmsg("Invalid bit depth (%dbits)\n", Signal->header.fmt.bitsPerSample);
				break;
		}
	}
	return Min;
}
