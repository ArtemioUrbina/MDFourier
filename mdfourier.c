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

int LoadAndProcessAudioFiles(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config);
int LoadFile(FILE *file, AudioSignal *Signal, parameters *config, char *fileName);
int ProcessFile(AudioSignal *Signal, parameters *config);
void PrepareSignal(AudioSignal *Signal, double ZeroDbMagReference, parameters *config);
int ExecuteDFFT(AudioBlocks *AudioArray, int16_t *samples, size_t size, long samplerate, double *window, parameters *config);
double CompareAudioBlocks(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config);
void CleanUp(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config);
void CloseFiles(FILE **ref, FILE **comp);
void NormalizeAudio(AudioSignal *Signal, parameters *config);

// Time domain
MaximumVolume FindMaxVolume(AudioSignal *Signal, parameters *config);
void NormalizeAudioByRatio(AudioSignal *Signal, double ratio, parameters *config);
int16_t FindLocalMaximumAroundSample(AudioSignal *Signal, MaximumVolume refMax, parameters *config);

// Frequency domain
void NormalizeMagnitudesByRatio(AudioSignal *Signal, double ratio, parameters *config);
MaximumMagnitude FindMaxMagnitudeBlock(AudioSignal *Signal, parameters *config);
double FindLocalMaximumInBlock(AudioSignal *Signal, MaximumMagnitude refMax, parameters *config);

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

	if(!LoadAudioBlockStructure(&config))
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

	logmsg("* Plotting results to PNGs\n");
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
			ComparisonSignal->floorAmplitude > config.significantVolume)
		{
			config.significantVolume = ComparisonSignal->floorAmplitude;
			logmsg(" - Using %g as minimum significant volume for analisys\n",
				config.significantVolume);
			CreateBaseName(&config);
		}
	
		CompareAudioBlocks(ComparisonSignal, ReferenceSignal, &config);
	
		PlotResults(ComparisonSignal, &config);
	}
	else
		PlotSpectrograms(ComparisonSignal, &config);

	if(IsLogEnabled())
	{
		endLog();
		printf("\nCheck logfile for extended results\n");
	}

	printf("\nResults stored in %s\n", config.folderName);

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

	*ComparisonSignal = CreateAudioSignal(config);
	if(!*ComparisonSignal)
	{
		CloseFiles(&reference, &compare);
		CleanUp(ReferenceSignal, ComparisonSignal, config);
		return 0;
	}

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

	/* Although dithering would be better, there has been no need */
	/* Tested a file scaled with ths method against itself using  */
	/* the previous frequency domain solution, and differences are*/
	/* negligible (less than 0.2dBFS) */
	/* We go with time domain normalization because different bin */
	/* size caused by different frame rates cghanges the relative */
	/* volumes, this can be fixed by using zero paddng by default */
	/* but that seems to be less favored */
	if(config->timeDomainNormalize)
	{
		MaximumVolume		MaxRef, MaxTar;
		double				ComparisonLocalMaximum = 0;
		double				ratioTar = 0, ratioRef = 0;

		// Find Normalization factors
		MaxRef = FindMaxVolume(*ReferenceSignal, config);
		if(!MaxRef.magnitude)
		{
			logmsg("Could not detect Max volume in 'Reference' File for normalization\n");
			CloseFiles(&reference, &compare);
			CleanUp(ReferenceSignal, ComparisonSignal, config);
			return 0;
		}
		MaxTar = FindMaxVolume(*ComparisonSignal, config);
		if(!MaxTar.magnitude)
		{
			logmsg("Could not detect Max volume in 'Comparison' file for normalization\n");
			CloseFiles(&reference, &compare);
			CleanUp(ReferenceSignal, ComparisonSignal, config);
			return 0;
		}
	
		ComparisonLocalMaximum = FindLocalMaximumAroundSample(*ComparisonSignal, MaxRef, config);
		if(!ComparisonLocalMaximum)
		{
			logmsg("Could not detect Max volume in 'Comparison' file for normalization\n");
			CloseFiles(&reference, &compare);
			CleanUp(ReferenceSignal, ComparisonSignal, config);
			return 0;
		}
	
		ratioTar = (double)0x7FFF/MaxTar.magnitude;
		ratioRef = (ComparisonLocalMaximum/MaxRef.magnitude)*ratioTar;
	
		NormalizeAudioByRatio(*ReferenceSignal, ratioRef, config);
		NormalizeAudioByRatio(*ComparisonSignal, ratioTar, config);

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

	CalcuateFrequencyBrackets(*ReferenceSignal);
	CalcuateFrequencyBrackets(*ComparisonSignal);

	if(!config->timeDomainNormalize)
	{
		MaximumMagnitude	MaxRef, MaxTar;
		double				ComparisonLocalMaximum = 0;
		double				ratioRef = 0;

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
	}
	
	logmsg("* Processing Signal Frequencies and Amplitudes\n");
	if((*ReferenceSignal)->MaxMagnitude < (*ComparisonSignal)->MaxMagnitude)
	{
		ZeroDbMagnitudeRef = (*ComparisonSignal)->MaxMagnitude;
		if(config->verbose)
			logmsg(" - Comparison file has the highest peak at %g vs %g\n",
				ZeroDbMagnitudeRef, (*ReferenceSignal)->MaxMagnitude);
	}
	else
	{
		ZeroDbMagnitudeRef = (*ReferenceSignal)->MaxMagnitude;
		if(config->verbose)
			logmsg(" - Reference file has the highest peak at %g vs %g\n",
				ZeroDbMagnitudeRef, (*ComparisonSignal)->MaxMagnitude);
	}

	PrepareSignal(*ReferenceSignal, ZeroDbMagnitudeRef, config);
	PrepareSignal(*ComparisonSignal, ZeroDbMagnitudeRef, config);

	/* Detect Signal Floor */
	if((*ReferenceSignal)->hasFloor && !config->ignoreFloor && 
		(*ReferenceSignal)->floorAmplitude > config->significantVolume)
	{
		config->significantVolume = (*ReferenceSignal)->floorAmplitude;
		logmsg(" - Using 'Reference' Signal\'s %g as minimum significant volume for analisys\n",
			config->significantVolume);
		CreateBaseName(config);
	}

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
		logmsg("\tInvalid WAV File: Only PCM is supported\n\tPlease convert file to WAV PCM 16 bit 44/48khz ");
		return(0);
	}

	if(Signal->header.NumOfChan != 2) /* Check for Stereo */
	{
		logmsg("\tInvalid WAV file: Only Stereo is supported\n");
		return(0);
	}

	if(Signal->header.bitsPerSample != 16) /* Check bit depth */
	{
		logmsg("\tInvalid WAV file: Only 16 bit supported for now\n\tPlease use WAV PCM 16 bit 44/48khz ");
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
	Signal->framerate = GetPlatformMSPerFrame(config);

	seconds = (double)Signal->header.Subchunk2Size/4.0/(double)Signal->header.SamplesPerSec;
	logmsg(" - WAV file is PCM %dhz %dbits and %g seconds long\n", 
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
			logmsg(" - Detected %g hz video signal (%gms per frame) from WAV file\n", 
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
		Signal->framerate = GetPlatformMSPerFrame(config);

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

	if(!initWindows(&windows, Signal->framerate, Signal->header.SamplesPerSec, config))
		return 0;

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

		memset(buffer, 0, buffersize);
		if(pos + loadedBlockSize > Signal->header.Subchunk2Size)
		{
			logmsg("\tunexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
			break;
		}
		
		memcpy(buffer, Signal->Samples + pos, loadedBlockSize);

		Signal->Blocks[i].index = GetBlockSubIndex(config, i);
		Signal->Blocks[i].type = GetBlockType(config, i);

		if(!ExecuteDFFT(&Signal->Blocks[i], (int16_t*)buffer, (loadedBlockSize-difference)/2, Signal->header.SamplesPerSec, windowUsed, config))
			return 0;
		// MDWAVE exists for this, but just in case it is ever needed within MDFourier
		//SaveWAVEChunk(NULL, Signal, buffer, i, loadedBlockSize, config);

		FillFrequencyStructures(&Signal->Blocks[i], config);

		pos += loadedBlockSize;
		pos += discardBytes;
		i++;
	}

	if(config->timeDomainNormalize)
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
	for(int freq = 0; freq < config->MaxFreq; freq++)
	{
		/* Volumne is too low */
		if(limitRef && Signal->Blocks[block].freq[freq].amplitude < config->significantVolume)
			return freq;

		/* Volume is too low with tolerance */
		if(!limitRef && Signal->Blocks[block].freq[freq].amplitude < config->significantVolume - config->tolerance*2)
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

			/* One compared item is frequency the other is amplitude */
			config->Differences.cntTotalCompared ++;
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
			OutputFileOnlyStart();
			if(config->extendedResults)
			{
				logmsg("Unmatched Block Report for %s# %ld (%ld)\n", GetBlockName(config, block), GetBlockSubIndex(config, block), block);
				PrintComparedBlocks(&ReferenceSignal->Blocks[block], &ComparisonSignal->Blocks[block],
					config, ReferenceSignal);
			}
			OutputFileOnlyEnd();
		}
		else
		{
			OutputFileOnlyStart();
			if(!config->justResults && config->showAll)
			{
				logmsg("Matched Block Report for %s# %ld (%ld)\n", GetBlockName(config, block), GetBlockSubIndex(config, block), block);
				PrintComparedBlocks(&ReferenceSignal->Blocks[block], &ComparisonSignal->Blocks[block], 
					config, ReferenceSignal);
			}
			OutputFileOnlyEnd();
		}
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
void NormalizeAudio(AudioSignal *Signal, parameters *config)
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
void NormalizeAudioByRatio(AudioSignal *Signal, double ratio, parameters *config)
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

// Find the Maximum Volume in the Reference Audio File
MaximumVolume FindMaxVolume(AudioSignal *Signal, parameters *config)
{
	long int 		i = 0, start = 0, end = 0;
	int16_t			*samples = NULL;
	MaximumVolume	maxSampleValue;

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
int16_t FindLocalMaximumAroundSample(AudioSignal *Signal, MaximumVolume refMax, parameters *config)
{
	long int 		i, start = 0, end = 0, pos = 0;
	int16_t			*samples = NULL, MaxLocalSample = 0;
	double			refSeconds = 0, refFrames = 0, tarSeconds = 0;

	if(!Signal)
		return 0;

	start = Signal->startOffset/2;
	end = Signal->endOffset/2;

	if(refMax.framerate != Signal->framerate)
	{
		refSeconds = BytesToSeconds(refMax.samplerate, refMax.offset);
		refFrames = refSeconds/(refMax.framerate/1000);
	
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

	// Search in 1/10 of Signal->header.SamplesPerSec
	// around the position of the sample

	if(pos - Signal->header.SamplesPerSec/20 >= start)
		start = pos - Signal->header.SamplesPerSec/20;
	
	if(end >= pos + Signal->header.SamplesPerSec/20)
		end = pos + Signal->header.SamplesPerSec/20;

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
		for(int i = 0; i < config->MaxFreq; i++)
		{
			if(!Signal->Blocks[block].freq[i].hertz)
				break;

			Signal->Blocks[block].freq[i].magnitude *= ratio;
		}
	}
	Signal->MaxMagnitude *= ratio;
}

MaximumMagnitude FindMaxMagnitudeBlock(AudioSignal *Signal, parameters *config)
{
	MaximumMagnitude	MaxMag;

	MaxMag.magnitude = 0;
	MaxMag.hertz = 0;
	MaxMag.block = -1;

	if(!Signal)
		return MaxMag;

	// Find global peak
	for(int block = 0; block < config->types.totalChunks; block++)
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

	if(MaxMag.block != -1)
		Signal->MaxMagnitude = MaxMag.magnitude;

	if(config->verbose)
	{
		if(MaxMag.block != -1)
			logmsg(" - Max Volume found in %s# %d (%d) at %g Hz with %g magnitude\n", 
					GetBlockName(config, MaxMag.block), GetBlockSubIndex(config, MaxMag.block),
					MaxMag.block, MaxMag.hertz, MaxMag.magnitude);
	}

	return MaxMag;
}

double FindLocalMaximumInBlock(AudioSignal *Signal, MaximumMagnitude refMax, parameters *config)
{
	double		highest = 0;

	if(!Signal)
		return highest;

	for(int i = 0; i < config->MaxFreq; i++)
	{
		double magnitude = 0;

		magnitude = Signal->Blocks[refMax.block].freq[i].magnitude;
		if(!Signal->Blocks[refMax.block].freq[i].hertz)
			break;
		if(Signal->Blocks[refMax.block].freq[i].hertz == refMax.hertz)
		{
			if(config->verbose)
			{
				logmsg(" - Comparison Local Maximum %g Hz with %g magnitude at %d [%d]\n",
					refMax.hertz, magnitude, refMax.block, i);
			}
			return (magnitude);
		}
		if(magnitude > highest)
			highest = magnitude;
	}
	return highest;
}
