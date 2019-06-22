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
#include "log.h"
#include "cline.h"
#include "windows.h"
#include "freq.h"
#include "diff.h"
#include "plot.h"
#include "sync.h"
#include "balance.h"

int LoadAndProcessAudioFiles(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config);
int LoadFile(FILE *file, AudioSignal *Signal, parameters *config, char *fileName);
int ProcessFile(AudioSignal *Signal, parameters *config);
void PrepareSignal(AudioSignal *Signal, double ZeroDbMagReference, parameters *config);
int ExecuteDFFT(AudioBlocks *AudioArray, int16_t *samples, size_t size, long samplerate, double *window, parameters *config);
double CompareAudioBlocks(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config);
void CleanUp(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config);
void CloseFiles(FILE **ref, FILE **comp);
void NormalizeAudio(AudioSignal *Signal);

// Time domain
MaxVolum FindMaxVolume(AudioSignal *Signal);
void NormalizeAudioByRatio(AudioSignal *Signal, double ratio);
int16_t FindLocalMaximumAroundSample(AudioSignal *Signal, MaxVolum refMax);

// Frequency domain
void NormalizeMagnitudesByRatio(AudioSignal *Signal, double ratio, parameters *config);
MaxMagn FindMaxMagnitudeBlock(AudioSignal *Signal, parameters *config);
double FindLocalMaximumInBlock(AudioSignal *Signal, MaxMagn refMax, parameters *config);
double FindFundamentalMagnitudeAverage(AudioSignal *Signal, parameters *config);

int main(int argc , char *argv[])
{
	AudioSignal  		*ReferenceSignal = NULL;
	AudioSignal  		*ComparisonSignal = NULL;
	parameters			config;
	struct	timespec	start, end;

	Header(0);
	if(!commandline(argc, argv, &config))
	{
		printf("	 -h: Shows command line help\n");
		return 1;
	}

	if(config.clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	if(!LoadProfile(&config))
		return 1;

	if(strcmp(config.referenceFile, config.targetFile) == 0)
	{
		CleanUp(&ReferenceSignal, &ComparisonSignal, &config);
		logmsg("Both inputs are the same file %s, skipping to save time\n",
			 config.referenceFile);
		return 1;
	}

	if(!LoadAndProcessAudioFiles(&ReferenceSignal, &ComparisonSignal, &config))
		return 1;

	logmsg("\n* Comparing frequencies\n");
	CompareAudioBlocks(ReferenceSignal, ComparisonSignal, &config);

	logmsg("* Plotting results to PNGs:\n");
	PlotResults(ReferenceSignal, &config);

	if(config.reverseCompare)
	{
		/* Clear up everything for inverse compare */
		CleanMatched(ReferenceSignal, ComparisonSignal, &config);
		ReleaseDifferenceArray(&config);
		InvertComparedName(&config);
	
		logmsg("* Comparing frequencies 'Comparison' -> 'Reference'\n");
	
		/* Detect Signal Floor */
		config.significantVolume = config.origSignificantVolume;
		if(ComparisonSignal->hasFloor && !config.ignoreFloor && 
			ComparisonSignal->floorAmplitude != 0.0 && ComparisonSignal->floorAmplitude > config.significantVolume)
		{
			config.significantVolume = ComparisonSignal->floorAmplitude;
			CreateBaseName(&config);
		}
	
		logmsg(" - Using %gdBFS as minimum significant volume for analisys\n",
			config.significantVolume);
		CompareAudioBlocks(ComparisonSignal, ReferenceSignal, &config);
	
		PlotResults(ComparisonSignal, &config);
	}
	else
	{
		if(config.plotSpectrogram)
		{
			char 	*CurrentPath = NULL;

			CurrentPath = GetCurrentPathAndChangeToResultsFolder(&config);

			logmsg(" - Spectrogram Comparison");
			PlotSpectrograms(ComparisonSignal, &config);
			logmsg("\n");

			ReturnToMainPath(&CurrentPath);
		}
	}

	if(IsLogEnabled())
	{
		endLog();
		printf("\nCheck logfile for extended results\n");
	}

	/* Clear up everything */
	ReleaseDifferenceArray(&config);

	CleanUp(&ReferenceSignal, &ComparisonSignal, &config);

	if(config.clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - clk: MDFourier took %0.2fs\n", elapsedSeconds);
	}

	printf("\nResults stored in %s\n", config.folderName);
	
	return(0);
}

int LoadAndProcessAudioFiles(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config)
{
	FILE				*reference = NULL;
	FILE				*compare = NULL;
	double				ZeroDbMagnitudeRef = 0;

	reference = fopen(config->referenceFile, "rb");
	if(!reference)
	{
		CleanUp(ReferenceSignal, ComparisonSignal, config);
		logmsg("\tCould not open 'Reference' file: \"%s\"\n", config->referenceFile);
		return 0;
	}

	compare = fopen(config->targetFile, "rb");
	if(!compare)
	{
		CloseFiles(&reference, &compare);
		CleanUp(ReferenceSignal, ComparisonSignal, config);
		logmsg("\tCould not open 'Comparison' file: \"%s\"\n", config->targetFile);
		return 0;
	}

	*ReferenceSignal = CreateAudioSignal(config);
	if(!*ReferenceSignal)
	{
		CloseFiles(&reference, &compare);
		CleanUp(ReferenceSignal, ComparisonSignal, config);
		return 0;
	}
	(*ReferenceSignal)->role = ROLE_REF;

	*ComparisonSignal = CreateAudioSignal(config);
	if(!*ComparisonSignal)
	{
		CloseFiles(&reference, &compare);
		CleanUp(ReferenceSignal, ComparisonSignal, config);
		return 0;
	}
	(*ComparisonSignal)->role = ROLE_COMP;

	logmsg("\n* Loading 'Reference' audio file %s\n", config->referenceFile);
	if(!LoadFile(reference, *ReferenceSignal, config, config->referenceFile))
	{
		CloseFiles(&reference, &compare);
		CleanUp(ReferenceSignal, ComparisonSignal, config);
		return 0;
	}

	logmsg("\n* Loading 'Comparison' audio file %s\n", config->targetFile);
	if(!LoadFile(compare, *ComparisonSignal, config, config->targetFile))
	{
		CloseFiles(&reference, &compare);
		CleanUp(ReferenceSignal, ComparisonSignal, config);
		return 0;
	}

	if(config->channel == 's')
	{
		int block = NO_INDEX;

		block = GetFirstMonoIndex(config);
		if(block != NO_INDEX)
		{
			CheckBalance(*ReferenceSignal, block, config);
			CheckBalance(*ComparisonSignal, block, config);
		}
	}

	/* Although dithering would be better, there has been no need */
	/* Tested a file scaled with ths method against itself using  */
	/* the frequency domain solution, and differences are negligible */
	/* (less than 0.2dBFS) */

	if(config->normType == max_time)
	{
		MaxVolum			MaxRef, MaxTar;
		double				ComparisonLocalMaximum = 0;
		double				ratioTar = 0, ratioRef = 0;

		// Find Normalization factors
		MaxRef = FindMaxVolume(*ReferenceSignal);
		if(!MaxRef.magnitude)
		{
			logmsg("Could not detect Max volume in 'Reference' File for normalization\n");
			CloseFiles(&reference, &compare);
			CleanUp(ReferenceSignal, ComparisonSignal, config);
			return 0;
		}
		MaxTar = FindMaxVolume(*ComparisonSignal);
		if(!MaxTar.magnitude)
		{
			logmsg("Could not detect Max volume in 'Comparison' file for normalization\n");
			CloseFiles(&reference, &compare);
			CleanUp(ReferenceSignal, ComparisonSignal, config);
			return 0;
		}
	
		ratioTar = (double)0x7FFF/MaxTar.magnitude;
		NormalizeAudioByRatio(*ComparisonSignal, ratioTar);
		ComparisonLocalMaximum = FindLocalMaximumAroundSample(*ComparisonSignal, MaxRef);
		if(!ComparisonLocalMaximum)
		{
			logmsg("Could not detect Max volume in 'Comparison' file for normalization\n");
			CloseFiles(&reference, &compare);
			CleanUp(ReferenceSignal, ComparisonSignal, config);
			return 0;
		}

		ratioRef = (ComparisonLocalMaximum/MaxRef.magnitude);
		NormalizeAudioByRatio(*ReferenceSignal, ratioRef);

		// Uncomment if you want to check the WAV files as normalized
		//SaveWAVEChunk(NULL, *ReferenceSignal, (*ReferenceSignal)->Samples, 0, (*ReferenceSignal)->header.Subchunk2Size, config); 
		//SaveWAVEChunk(NULL, *ComparisonSignal, (*ComparisonSignal)->Samples, 0, (*ComparisonSignal)->header.Subchunk2Size, config); 
	}

	CompareFrameRates((*ReferenceSignal)->framerate, (*ComparisonSignal)->framerate, config);
	
	logmsg("\n* Executing Discrete Fast Fourier Transforms on 'Reference' file\n");
	if(!ProcessFile(*ReferenceSignal, config))
	{
		CleanUp(ReferenceSignal, ComparisonSignal, config);
		return 0;
	}

	logmsg("* Executing Discrete Fast Fourier Transforms on 'Comparison' file\n");

	if(!ProcessFile(*ComparisonSignal, config))
	{
		CleanUp(ReferenceSignal, ComparisonSignal, config);
		return 0;
	}

	CalcuateFrequencyBrackets(*ReferenceSignal, config);
	CalcuateFrequencyBrackets(*ComparisonSignal, config);

	if(config->normType == max_frequency)
	{
		MaxMagn				MaxRef, MaxTar;
		double				ComparisonLocalMaximum = 0;
		double				ratioRef = 0;
		double				RefAvg = 0;
		double				CompAvg = 0;
		double				ratio = 0;

		// Find Normalization factors
		MaxRef = FindMaxMagnitudeBlock(*ReferenceSignal, config);
		if(!MaxRef.magnitude)
		{
			logmsg("Could not detect Max volume in 'Reference' File for normalization\n");
			CloseFiles(&reference, &compare);
			CleanUp(ReferenceSignal, ComparisonSignal, config);
			return 0;
		}
		MaxTar = FindMaxMagnitudeBlock(*ComparisonSignal, config);
		if(!MaxTar.magnitude)
		{
			logmsg("Could not detect Max volume in 'Comparison' file for normalization\n");
			CloseFiles(&reference, &compare);
			CleanUp(ReferenceSignal, ComparisonSignal, config);
			return 0;
		}
	
		ComparisonLocalMaximum = FindLocalMaximumInBlock(*ComparisonSignal, MaxRef, config);
		if(!ComparisonLocalMaximum)
		{
			logmsg("Could not detect Max volume in 'Comparison' file for normalization\n");
			CloseFiles(&reference, &compare);
			CleanUp(ReferenceSignal, ComparisonSignal, config);
			return 0;
		}
	
		ratioRef = ComparisonLocalMaximum/MaxRef.magnitude;
		NormalizeMagnitudesByRatio(*ReferenceSignal, ratioRef, config);

		RefAvg = FindFundamentalMagnitudeAverage(*ReferenceSignal, config);
		CompAvg = FindFundamentalMagnitudeAverage(*ComparisonSignal, config);
		
		if(RefAvg > CompAvg)
			ratio = RefAvg/CompAvg;
		else
			ratio = CompAvg/RefAvg;
		if(ratio > 10)
		{
			logmsg("\nWARNING: Average frequency difference after normalization\n");
			logmsg("\tbetween the signals is too high (ratio:%g to 1)\n", ratio);
			logmsg("\tIf results make no sense, try using the Time domain normalization\n");
			logmsg("\toption, enabled with the -n t option.\n\n");
		}
	}

	if(config->normType == average)
	{
		double RefAvg = FindFundamentalMagnitudeAverage(*ReferenceSignal, config);
		double CompAvg = FindFundamentalMagnitudeAverage(*ComparisonSignal, config);

		if(CompAvg > RefAvg)
		{
			double ratio = CompAvg/RefAvg;
			NormalizeMagnitudesByRatio(*ReferenceSignal, ratio, config);
		}
		else
		{
			double ratio = RefAvg/CompAvg;
			NormalizeMagnitudesByRatio(*ComparisonSignal, ratio, config);
		}
	}

	logmsg("* Processing Signal Frequencies and Amplitudes\n");
	/*
	// Compensate time normalization to match frequency normalization
	if(config->normType == max_time)
	{
		// Check if we have the same peaks
		// This is in order to compensate TimeDomain Differences
		// But we probably default to Frequency domain and leave this commented out
		if((*ReferenceSignal)->MaxMagnitude.hertz == (*ComparisonSignal)->MaxMagnitude.hertz &&
			(*ReferenceSignal)->MaxMagnitude.block == (*ComparisonSignal)->MaxMagnitude.block)
		{
			double diff = 0;
	
			diff = fabs((*ReferenceSignal)->MaxMagnitude.magnitude - (*ComparisonSignal)->MaxMagnitude.magnitude);
			if(diff > 0.5)
			{
				double ratio = 0;
	
				if((*ReferenceSignal)->MaxMagnitude.magnitude > (*ComparisonSignal)->MaxMagnitude.magnitude)
				{
					//if(config->verbose)
						logmsg(" - Both Peaks match in frequency and Block, readjusting magnitudes");
					ratio = (*ComparisonSignal)->MaxMagnitude.magnitude/(*ReferenceSignal)->MaxMagnitude.magnitude;
					NormalizeMagnitudesByRatio(*ReferenceSignal, ratio, config);
				}
				else
				{
					if(config->verbose)
						logmsg(" - Both Peaks match in frequency and Block, readjusting magnitudes");
					ratio = (*ReferenceSignal)->MaxMagnitude.magnitude/(*ComparisonSignal)->MaxMagnitude.magnitude;
					NormalizeMagnitudesByRatio(*ComparisonSignal, ratio, config);
				}
			}
			//ZeroDbMagnitudeRef = (*ReferenceSignal)->MaxMagnitude.magnitude;
		}
	}
	*/

	if((*ReferenceSignal)->MaxMagnitude.magnitude < (*ComparisonSignal)->MaxMagnitude.magnitude)
	{
		ZeroDbMagnitudeRef = (*ComparisonSignal)->MaxMagnitude.magnitude;
		if(config->verbose)
			logmsg(" - Comparison file has the highest peak at %g vs %g\n",
				ZeroDbMagnitudeRef, (*ReferenceSignal)->MaxMagnitude.magnitude);
	}
	else
	{
		ZeroDbMagnitudeRef = (*ReferenceSignal)->MaxMagnitude.magnitude;
		if(config->verbose)
			logmsg(" - Reference file has the highest peak at %g vs %g\n",
				ZeroDbMagnitudeRef, (*ComparisonSignal)->MaxMagnitude.magnitude);
	}

	PrepareSignal(*ReferenceSignal, ZeroDbMagnitudeRef, config);
	PrepareSignal(*ComparisonSignal, ZeroDbMagnitudeRef, config);

	/* Detect Signal Floor */
	if((*ReferenceSignal)->hasFloor && !config->ignoreFloor && 
		(*ReferenceSignal)->floorAmplitude != 0.0 && (*ReferenceSignal)->floorAmplitude > config->significantVolume)
	{
		config->significantVolume = (*ReferenceSignal)->floorAmplitude;
		CreateBaseName(config);
	}

	logmsg(" - Using %gdBFS as minimum significant volume for analisys\n",
		config->significantVolume);

	if(config->verbose)
	{
		PrintFrequencies(*ReferenceSignal, config);
		PrintFrequencies(*ComparisonSignal, config);
	}
	return 1;
}

void CleanUp(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config)
{
	if(*ReferenceSignal)
	{
		ReleaseAudio(*ReferenceSignal, config);
		free(*ReferenceSignal);
		*ReferenceSignal = NULL;
	}
	
	if(*ComparisonSignal)
	{
		ReleaseAudio(*ComparisonSignal, config);
		free(*ComparisonSignal);
		*ComparisonSignal = NULL;
	}

	ReleaseAudioBlockStructure(config);
}

void CloseFiles(FILE **ref, FILE **comp)
{
	if(*ref)
	{
		fclose(*ref);
		*ref = NULL;
	}

	if(*comp)
	{
		fclose(*comp);
		*comp = NULL;
	}
}

int LoadFile(FILE *file, AudioSignal *Signal, parameters *config, char *fileName)
{
	struct	timespec	start, end;
	double				seconds = 0;

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	if(!file)
		return 0;

	if(fread(&Signal->header, 1, sizeof(wav_hdr), file) != sizeof(wav_hdr))
	{
		logmsg("\tInvalid WAV file: File too small\n");
		return(0);
	}

	if(Signal->header.AudioFormat != 1) /* Check for PCM */
	{
		logmsg("\tInvalid WAV File: Only PCM is supported\n\tPlease convert file to WAV PCM 16 bit 44/48kHz ");
		return(0);
	}

	if(Signal->header.NumOfChan != 2) /* Check for Stereo */
	{
		logmsg("\tInvalid WAV file: Only Stereo is supported\n");
		return(0);
	}

	if(Signal->header.bitsPerSample != 16) /* Check bit depth */
	{
		logmsg("\tInvalid WAV file: Only 16 bit supported for now\n\tPlease use WAV PCM 16 bit 44/48kHz ");
		return(0);
	}
	
	if(Signal->header.SamplesPerSec/2 < config->endHz)
	{
		logmsg(" - %d Hz sample rate was too low for %d-%d Hz analysis\n",
			 Signal->header.SamplesPerSec, config->startHz, config->endHz);
		config->endHz = Signal->header.SamplesPerSec/2;
		logmsg(" - changed to %d-%d Hz\n", config->startHz, config->endHz);
	}

	// Default if none is found
	Signal->framerate = GetMSPerFrame(Signal, config);

	seconds = (double)Signal->header.Subchunk2Size/4.0/(double)Signal->header.SamplesPerSec;
	logmsg(" - WAV file is PCM %dHz %dbits and %g seconds long\n", 
		Signal->header.SamplesPerSec, Signal->header.bitsPerSample, seconds);

	if(seconds < GetSignalTotalDuration(Signal->framerate, config))
		logmsg(" - Estimated file length is smaller than the expected %gs\n",
				GetSignalTotalDuration(Signal->framerate, config));

	Signal->Samples = (char*)malloc(sizeof(char)*Signal->header.Subchunk2Size);
	if(!Signal->Samples)
	{
		logmsg("\tAll Chunks malloc failed!\n");
		return(0);
	}

	if(fread(Signal->Samples, 1, sizeof(char)*Signal->header.Subchunk2Size, file) !=
			 sizeof(char)*Signal->header.Subchunk2Size)
	{
		logmsg("\tCould not read the whole sample block from disk to RAM\n");
		return(0);
	}

	fclose(file);

	if(config->clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - clk: Loading WAV took %0.2fs\n", elapsedSeconds);
	}

	if(GetFirstSyncIndex(config) != NO_INDEX)
	{
		if(config->clock)
			clock_gettime(CLOCK_MONOTONIC, &start);

		/* Find the start offset */
		logmsg(" - Starting sync pulse train: ");
		Signal->startOffset = DetectPulse(Signal->Samples, Signal->header, config);
		if(Signal->startOffset == -1)
		{
			logmsg("\nStarting pulse train was not detected\n");
			return 0;
		}
		logmsg(" %gs [%ld bytes]\n", 
				BytesToSeconds(Signal->header.SamplesPerSec, Signal->startOffset),
				Signal->startOffset);

		if(GetLastSyncIndex(config) != NO_INDEX)
		{
			logmsg(" - Trailing sync pulse train: ");
			Signal->endOffset = DetectEndPulse(Signal->Samples, Signal->startOffset, Signal->header, config);
			if(Signal->endOffset == -1)
			{
				logmsg("\nTrailing sync pulse train was not detected, aborting\n");
				return 0;
			}
			logmsg(" %gs [%ld bytes]\n", 
				BytesToSeconds(Signal->header.SamplesPerSec, Signal->endOffset),
				Signal->endOffset);
			Signal->framerate = CalculateFrameRate(Signal, config);
			logmsg(" - Detected %g Hz video signal (%gms per frame) from WAV file\n", 
						CalculateScanRate(Signal), Signal->framerate);
		}
		else
		{
			logmsg(" - Trailing sync pulse train not defined in config file, aborting\n");
			PrintAudioBlocks(config);
			return 0;
		}

		if(config->clock)
		{
			double	elapsedSeconds;
			clock_gettime(CLOCK_MONOTONIC, &end);
			elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
			logmsg(" - clk: Detecting sync took %0.2fs\n", elapsedSeconds);
		}
	}
	else
	{
		Signal->framerate = GetMSPerFrame(Signal, config);

		/* Find the start offset */
		logmsg(" - Detecting audio signal: ");
		Signal->startOffset = DetectSignalStart(Signal->Samples, Signal->header, config);
		if(Signal->startOffset == -1)
		{
			logmsg("\nStarting position was not detected\n");
			return 0;
		}
		logmsg(" %gs [%ld bytes]\n", 
				BytesToSeconds(Signal->header.SamplesPerSec, Signal->startOffset),
				Signal->startOffset);
	}

	if(seconds < GetSignalTotalDuration(Signal->framerate, config))
		logmsg(" - Adjusted File length is smaller than the expected %gs\n",
				GetSignalTotalDuration(Signal->framerate, config));

	if(GetFirstSilenceIndex(config) != NO_INDEX)
		Signal->hasFloor = 1;

	sprintf(Signal->SourceFile, "%s", fileName);

	return 1;
}

int ProcessFile(AudioSignal *Signal, parameters *config)
{
	long int		pos = 0;
	double			longest = 0;
	char			*buffer;
	size_t			buffersize = 0;
	windowManager	windows;
	double			*windowUsed = NULL;
	long int		loadedBlockSize = 0, i = 0;
	struct timespec	start, end;
	int				leftover = 0, discardBytes = 0;
	double			leftDecimals = 0;

	pos = Signal->startOffset;

	longest = FramesToSeconds(Signal->framerate, GetLongestElementFrames(config));
	if(!longest)
	{
		logmsg("Block definitions are invalid, total length is 0\n");
		return 0;
	}

	buffersize = SecondsToBytes(Signal->header.SamplesPerSec, longest, NULL, NULL, NULL);
	buffer = (char*)malloc(buffersize);
	if(!buffer)
	{
		logmsg("\tmalloc failed\n");
		return(0);
	}

	if(!initWindows(&windows, Signal->framerate, Signal->header.SamplesPerSec, config->window, config))
		return 0;

	if(config->drawWindows)
	{
		VisualizeWindows(&windows, config);
		PlotBetaFunctions(config);
	}

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	while(i < config->types.totalChunks)
	{
		double duration = 0;
		long int frames = 0, difference = 0;

		frames = GetBlockFrames(config, i);
		duration = FramesToSeconds(Signal->framerate, frames);
		windowUsed = getWindowByLength(&windows, frames);
		
		loadedBlockSize = SecondsToBytes(Signal->header.SamplesPerSec, duration, &leftover, &discardBytes, &leftDecimals);

		difference = GetByteSizeDifferenceByFrameRate(Signal->framerate, frames, Signal->header.SamplesPerSec, config);

		Signal->Blocks[i].index = GetBlockSubIndex(config, i);
		Signal->Blocks[i].type = GetBlockType(config, i);
		if(Signal->Blocks[i].type >= TYPE_SILENCE)
		{
			//logmsg("loadedBlockSize -diff %ld leftover %ld discardBytes %ld leftDecimals %g\n",
				//	loadedBlockSize - difference, leftover, discardBytes, leftDecimals);
			memset(buffer, 0, buffersize);
			if(pos + loadedBlockSize > Signal->header.Subchunk2Size)
			{
				logmsg("\tunexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
				break;
			}
			
			memcpy(buffer, Signal->Samples + pos, loadedBlockSize-difference);
	
			if(!ExecuteDFFT(&Signal->Blocks[i], (int16_t*)buffer, (loadedBlockSize-difference)/2, Signal->header.SamplesPerSec, windowUsed, config))
				return 0;

			FillFrequencyStructures(&Signal->Blocks[i], config);
		}

		// MDWAVE exists for this, but just in case it is ever needed within MDFourier
		//SaveWAVEChunk(NULL, Signal, buffer, i, loadedBlockSize-difference, config);
	
		pos += loadedBlockSize;
		pos += discardBytes;
		i++;
	}

	if(config->normType != max_frequency)
		FindMaxMagnitude(Signal, config);

	if(config->clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - clk: Processing took %0.2fs\n", elapsedSeconds);
	}

	free(buffer);
	freeWindows(&windows);

	return i;
}

void PrepareSignal(AudioSignal *Signal, double ZeroDbMagReference, parameters *config)
{
	CalculateAmplitudes(Signal, ZeroDbMagReference, config);

	/* analyze silence floor if available */
	if(!config->ignoreFloor && Signal->hasFloor)
		FindFloor(Signal, config);
}

int ExecuteDFFT(AudioBlocks *AudioArray, int16_t *samples, size_t size, long samplerate, double *window, parameters *config)
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
		if(config->channel == 'l')
			signal[i] = (double)samples[i*2];
		if(config->channel == 'r')
			signal[i] = (double)samples[i*2+1];
		if(config->channel == 's')
			signal[i] = ((double)samples[i*2]+(double)samples[i*2+1])/2.0;

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

int CalculateMaxCompare(int block, AudioSignal *Signal, parameters *config, int limitRef)
{
	double limit = 0;

	limit = config->significantVolume;
	if(Signal->role == ROLE_COMP)
		limit += -20;	// Allow going 20 dbfs "deeper"

	for(int freq = 0; freq < config->MaxFreq; freq++)
	{
		/* Volumne is too low */
		if(limitRef && Signal->Blocks[block].freq[freq].amplitude < limit)
			return freq;

		/* Volume is too low with tolerance */
		if(!limitRef && Signal->Blocks[block].freq[freq].amplitude < limit - config->tolerance*2)
			return freq;

		/* Out of valid frequencies */
		if(!Signal->Blocks[block].freq[freq].hertz)
			return freq;
	}

	return config->MaxFreq;
}

double CompareAudioBlocks(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config)
{
	int			block = 0;
	struct	timespec	start, end;

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	if(!CreateDifferenceArray(config))
		return 0;

	for(block = 0; block < config->types.regularChunks; block++)
	{
		int refSize = 0, testSize = 0;

		/* Ignore Control blocks */
		if(GetBlockType(config, block) <= TYPE_CONTROL)
			continue;

		refSize = CalculateMaxCompare(block, ReferenceSignal, config, 1);
		testSize = CalculateMaxCompare(block, ComparisonSignal, config, 0);

		for(int freq = 0; freq < refSize; freq++)
		{
			int found = 0, index = 0;

			/* Ignore CRT noise */
			//if(IsCRTNoise(ReferenceSignal->Blocks[block].freq[freq].hertz))
				//continue;

			IncrementCompared(block, config);
			for(int comp = 0; comp < testSize; comp++)
			{
				if(!ComparisonSignal->Blocks[block].freq[comp].matched && 
					ReferenceSignal->Blocks[block].freq[freq].hertz ==
					ComparisonSignal->Blocks[block].freq[comp].hertz)
				{
					ComparisonSignal->Blocks[block].freq[comp].matched = freq + 1;
					ReferenceSignal->Blocks[block].freq[freq].matched = comp + 1;
					found = 1;
					index = comp;
					break;
				}
			}

			if(found)  /* Now in either case, compare amplitudes */
			{
				double test;

				test = fabs(fabs(ComparisonSignal->Blocks[block].freq[index].amplitude) - fabs(ReferenceSignal->Blocks[block].freq[freq].amplitude));
				if(test > config->tolerance)
				{
					/* Difference in Amplitude */
					double value = 1;

					if(config->useOutputFilter)
					{
						double amplitude;

						amplitude = ReferenceSignal->Blocks[block].freq[freq].amplitude;

						// we get the proportional linear error in range 0-1
						if(amplitude >= config->significantVolume)
						{
							value = (fabs(config->significantVolume)-fabs(amplitude))/fabs(config->significantVolume);
							if(value)
								value = CalculateWeightedError(value, config);
						}
						else
							value = 0;
					}

					InsertAmplDifference(block, ReferenceSignal->Blocks[block].freq[freq].hertz, 
							ReferenceSignal->Blocks[block].freq[freq].amplitude,
							ComparisonSignal->Blocks[block].freq[index].amplitude, value, config);
				}
	
				if(test && test <= config->tolerance)
				{
					/* Adjusted Amplitude to tolerance */
				}
			}

			if(!found)
			{
				/* Frequency Not Found */
				double value = 1;
						
				if(config->useOutputFilter)
				{
					double amplitude;

					amplitude = ReferenceSignal->Blocks[block].freq[freq].amplitude;

					// we get the proportional linear error in range 0-1
					if(amplitude >= config->significantVolume)
					{
						value = (fabs(config->significantVolume)-fabs(amplitude))/fabs(config->significantVolume);
						if(value)
							value = CalculateWeightedError(value, config);
					}
					else
						value = 0;
				}

				InsertFreqNotFound(block, ReferenceSignal->Blocks[block].freq[freq].hertz, 
					ReferenceSignal->Blocks[block].freq[freq].amplitude, value, config);
			}
		}

		if(config->Differences.BlockDiffArray[block].cntFreqBlkDiff && !config->justResults)
		{
			if(config->extendedResults)
			{
				OutputFileOnlyStart();
				logmsg("Unmatched Block Report for %s# %ld (%ld)\n", GetBlockName(config, block), GetBlockSubIndex(config, block), block);
				PrintComparedBlocks(&ReferenceSignal->Blocks[block], &ComparisonSignal->Blocks[block],
					config, ReferenceSignal);
				OutputFileOnlyEnd();
			}
		}
		else
		{
			if(!config->justResults && config->showAll)
			{
				OutputFileOnlyStart();
				logmsg("Matched Block Report for %s# %ld (%ld)\n", GetBlockName(config, block), GetBlockSubIndex(config, block), block);
				PrintComparedBlocks(&ReferenceSignal->Blocks[block], &ComparisonSignal->Blocks[block], 
					config, ReferenceSignal);
				OutputFileOnlyEnd();
			}
		}
	}

	if(config->extendedResults)
	{
		OutputFileOnlyStart();
		PrintDifferenceArray(config);
		OutputFileOnlyEnd();
	}
	
	if(config->clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - clk: Comparing frequencies took %0.2fs\n", elapsedSeconds);
	}
	return 0;
}

/* Tiem domain normalization functions */

// this is never used
void NormalizeAudio(AudioSignal *Signal)
{
	long int 	i = 0, start = 0, end = 0;
	int16_t		*samples = NULL, maxSampleValue = 0;
	double		ratio = 0;

	if(!Signal)
		return;

	samples = (int16_t*)Signal->Samples;
	start = Signal->startOffset/2;
	end = Signal->endOffset/2;
	for(i = start; i < end; i++)
	{
		int16_t sample;

		sample = abs(samples[i]);
		if(sample > maxSampleValue)
			maxSampleValue = sample;
	}

	if(!maxSampleValue)
		return;

	ratio = (double)0x7FFF/(double)maxSampleValue;

	for(i = start; i < end; i++)
		samples[i] = (int16_t)((double)samples[i])*ratio;
}

// These work in the time domain
void NormalizeAudioByRatio(AudioSignal *Signal, double ratio)
{
	long int 	i = 0, start = 0, end = 0;
	int16_t		*samples = NULL;

	if(!Signal)
		return;

	if(!ratio)
		return;

	samples = (int16_t*)Signal->Samples;
	start = Signal->startOffset/2;
	end = Signal->endOffset/2;

	for(i = start; i < end; i++)
		samples[i] = (int16_t)((double)samples[i])*ratio;
}

// Find the Maximum Volume in the Audio File
MaxVolum FindMaxVolume(AudioSignal *Signal)
{
	long int 		i = 0, start = 0, end = 0;
	int16_t			*samples = NULL;
	MaxVolum	maxSampleValue;

	maxSampleValue.magnitude = 0;
	maxSampleValue.offset = 0;
	maxSampleValue.samplerate = Signal->header.SamplesPerSec;
	maxSampleValue.framerate = Signal->framerate;

	if(!Signal)
		return maxSampleValue;

	samples = (int16_t*)Signal->Samples;
	start = Signal->startOffset/2;
	end = Signal->endOffset/2;
	for(i = start; i < end; i++)
	{
		int16_t sample;

		sample = abs(samples[i]);
		if(sample > maxSampleValue.magnitude)
		{
			maxSampleValue.magnitude = sample;
			maxSampleValue.offset = i - start;
		}
	}

	return(maxSampleValue);
}

// Find the Maximum Volume in the Reference Audio File
int16_t FindLocalMaximumAroundSample(AudioSignal *Signal, MaxVolum refMax)
{
	long int 		i, start = 0, end = 0, pos = 0;
	int16_t			*samples = NULL, MaxLocalSample = 0;
	double			refSeconds = 0, refFrames = 0, tarSeconds = 0, fraction = 0;

	if(!Signal)
		return 0;

	start = Signal->startOffset/2;
	end = Signal->endOffset/2;

	if(refMax.framerate != Signal->framerate)
	{
		refSeconds = BytesToSeconds(refMax.samplerate, refMax.offset);
		refFrames = refSeconds/(refMax.framerate/1000.0);
	
		tarSeconds = FramesToSeconds(refFrames, Signal->framerate);
		pos = start + SecondsToBytes(Signal->header.SamplesPerSec, tarSeconds, NULL, NULL, NULL);
	}
	else
	{
		pos = start + refMax.offset;
		pos = (double)pos*(double)Signal->header.SamplesPerSec/(double)refMax.samplerate;
	}

	if(pos > end)
		return 0;

	// Search in 2/faction of Signal->header.SamplesPerSec
	// around the position of the sample

	fraction = 60.0; // around 1 frame
	if(pos - Signal->header.SamplesPerSec/fraction >= start)
		start = pos - Signal->header.SamplesPerSec/fraction;
	
	if(end >= pos + Signal->header.SamplesPerSec/fraction)
		end = pos + Signal->header.SamplesPerSec/fraction;

	samples = (int16_t*)Signal->Samples;
	for(i = start; i < end; i++)
	{
		int16_t sample;

		sample = abs(samples[i]);
		if(sample > MaxLocalSample)
			MaxLocalSample = sample;
	}

	return MaxLocalSample;
}

/**********************************************************/
// Frequency domain normalization functions

void NormalizeMagnitudesByRatio(AudioSignal *Signal, double ratio, parameters *config)
{
	if(!Signal)
		return;

	if(!ratio)
		return;

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
	
				Signal->Blocks[block].freq[i].magnitude *= ratio;
			}
		}
	}
	Signal->MaxMagnitude.magnitude *= ratio;
}

MaxMagn FindMaxMagnitudeBlock(AudioSignal *Signal, parameters *config)
{
	MaxMagn	MaxMag;

	MaxMag.magnitude = 0;
	MaxMag.hertz = 0;
	MaxMag.block = -1;

	if(!Signal)
		return MaxMag;

	// Find global peak
	for(int block = 0; block < config->types.totalChunks; block++)
	{
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(type > TYPE_CONTROL)
		{
			for(int i = 0; i < config->MaxFreq; i++)
			{
				if(!Signal->Blocks[block].freq[i].hertz)
					break;
				if(Signal->Blocks[block].freq[i].magnitude > MaxMag.magnitude)
				{
					MaxMag.magnitude = Signal->Blocks[block].freq[i].magnitude;
					MaxMag.hertz = Signal->Blocks[block].freq[i].hertz;
					MaxMag.block = block;
				}
			}
		}
	}

	if(MaxMag.block != -1)
	{
		Signal->MaxMagnitude.magnitude = MaxMag.magnitude;
		Signal->MaxMagnitude.hertz = MaxMag.hertz;
		Signal->MaxMagnitude.block = MaxMag.block;
	}

	if(config->verbose)
	{
		if(MaxMag.block != -1)
			logmsg(" - Max Magnitude found in %s# %d (%d) at %g Hz with %g\n", 
					GetBlockName(config, MaxMag.block), GetBlockSubIndex(config, MaxMag.block),
					MaxMag.block, MaxMag.hertz, MaxMag.magnitude);
	}

	return MaxMag;
}

double FindLocalMaximumInBlock(AudioSignal *Signal, MaxMagn refMax, parameters *config)
{
	double		highest = 0;

	if(!Signal)
		return highest;

	for(int i = 0; i < config->MaxFreq; i++)
	{
		double diff = 0, binSize = 0;
		double magnitude = 0;

		if(!Signal->Blocks[refMax.block].freq[i].hertz)
			break;

		// we regularly end in a case where the 
		// peak is a few bins lower or higher
		// and we don't want to normalize against
		// the magnitude of a harmonic sine wave
		// we allow a difference of +/- 5 frequency bins
		magnitude = Signal->Blocks[refMax.block].freq[i].magnitude;
		diff = fabs(refMax.hertz - Signal->Blocks[refMax.block].freq[i].hertz);

		binSize = FindFrequencyBinSizeForBlock(Signal, refMax.block);
		if(diff < 5*binSize)
		{
			if(config->verbose)
			{
				logmsg(" - Comparison Local Max magnitude for [R:%g->C:%g] Hz is %g. Block %d Pos:%d\n",
					refMax.hertz, Signal->Blocks[refMax.block].freq[i].hertz, 
					magnitude, refMax.block, i);
			}
			return (magnitude);
		}
		if(magnitude > highest)
			highest = magnitude;
	}

	if(config->verbose)
	{
		logmsg(" - Comparison Local Maximum (No Hz match) with %g magnitude at block %d\n",
			highest, refMax.block);
	}
	return highest;
}

double FindFundamentalMagnitudeAverage(AudioSignal *Signal, parameters *config)
{
	double		AvgFundMag = 0;
	long int	count = 0;

	if(!Signal)
		return 0;

	// Find global peak
	for(int block = 0; block < config->types.totalChunks; block++)
	{
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(type > TYPE_CONTROL && Signal->Blocks[block].freq[0].hertz != 0)
		{
			AvgFundMag += Signal->Blocks[block].freq[0].magnitude;
			count ++;
		}
	}

	if(count)
		AvgFundMag /= count;

	if(config->verbose)
	{
		logmsg(" - Average Fundamental Magnitude %g from %ld elements\n", 
				AvgFundMag, count);
	}

	return AvgFundMag;
}