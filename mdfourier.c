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
#include "flac.h"

int LoadAndProcessAudioFiles(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config);
int LoadFile(FILE *file, AudioSignal *Signal, parameters *config, char *fileName);
int ProcessFile(AudioSignal *Signal, parameters *config);
int ExecuteDFFT(AudioBlocks *AudioArray, int16_t *samples, size_t size, long samplerate, double *window, parameters *config);
double CompareAudioBlocks(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config);
void CleanUp(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config);
void CloseFiles(FILE **ref, FILE **comp);
void NormalizeAudio(AudioSignal *Signal);

// Time domain
MaxVolum FindMaxAmplitude(AudioSignal *Signal);
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
	double 				average = 0;

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

	logmsg("\n* Comparing frequencies: ");
	CompareAudioBlocks(ReferenceSignal, ComparisonSignal, &config);
	average = FindDifferenceAverage(&config);
	logmsg("Average difference is %g dBFS\n", average);
	if(average > DB_DIFF)
	{
		logmsg(" - WARNING: The average difference is too high.\n");
		if(config.averageIgnore)
		{
			logmsg("\tThis is abnormal, if results make no sense you can try:\n");
			logmsg("\tLimit the frequency range to be analyzed with -s and/or -e\n");
			logmsg("\tUse time domain normalization -n t\n");
			logmsg("\tVerify analog filters or cabling\n");
		}
		else
		{
			logmsg("\tSubstracting average for clarity, yellow dotted line marks real zero.\n");
			SubstractDifferenceAverage(&config, average);
			config.averageLine = average;
		}
		if(average > config.maxDbPlotZC)
		{
			config.maxDbPlotZC = average*1.5;
			logmsg("\tAdjusting viewport to %gdBFS for graphs\n\n", config.maxDbPlotZC);
		}
	}

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
		config.significantAmplitude = config.origSignificantAmplitude;
		if(ComparisonSignal->hasFloor && !config.ignoreFloor && 
			ComparisonSignal->floorAmplitude != 0.0 && ComparisonSignal->floorAmplitude > config.significantAmplitude)
		{
			config.significantAmplitude = ComparisonSignal->floorAmplitude;
			CreateBaseName(&config);
		}
	
		logmsg(" - Using %g dBFS as minimum significant amplitude for analysis\n",
			config.significantAmplitude);
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
		//printf("\nCheck logfile for extended results\n");
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

void RemoveFLACTemp(char *referenceFile, char *targetFile)
{
	char tmpFile[BUFFER_SIZE];

	if(IsFlac(referenceFile))
	{
		renameFLAC(referenceFile, tmpFile);
		remove(tmpFile);
	}
	if(IsFlac(targetFile))
	{
		renameFLAC(targetFile, tmpFile);
		remove(tmpFile);
	}
}

int LoadAndProcessAudioFiles(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config)
{
	FILE				*reference = NULL;
	FILE				*compare = NULL;
	double				ZeroDbMagnitudeRef = 0;

	if(IsFlac(config->referenceFile))
	{
		char tmpFile[BUFFER_SIZE];
		struct	timespec	start, end;

		if(config->clock)
			clock_gettime(CLOCK_MONOTONIC, &start);

		if(config->verbose)
			logmsg(" - Decoding FLAC\n");
		renameFLAC(config->referenceFile, tmpFile);
		if(!FLACtoWAV(config->referenceFile, tmpFile))
		{
			logmsg("\nERROR: Invalid FLAC file %s\n", config->referenceFile);
			remove(tmpFile);
			return 0;
		}
		if(config->clock)
		{
			double	elapsedSeconds;
			clock_gettime(CLOCK_MONOTONIC, &end);
			elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
			logmsg(" - clk: Decoding FLAC took %0.2fs\n", elapsedSeconds);
		}
		reference = fopen(tmpFile, "rb");
	}
	else
		reference = fopen(config->referenceFile, "rb");

	if(!reference)
	{
		RemoveFLACTemp(config->referenceFile, config->targetFile);
		CleanUp(ReferenceSignal, ComparisonSignal, config);
		logmsg("\tERROR: Could not open 'Reference' file:\n\t\"%s\"\n", config->referenceFile);
		return 0;
	}

	if(IsFlac(config->targetFile))
	{
		char tmpFile[BUFFER_SIZE];
		struct	timespec	start, end;

		if(config->clock)
			clock_gettime(CLOCK_MONOTONIC, &start);

		if(config->verbose)
			logmsg(" - Decoding FLAC\n");
		renameFLAC(config->targetFile, tmpFile);
		if(!FLACtoWAV(config->targetFile, tmpFile))
		{
			logmsg("\nERROR: Invalid FLAC file %s\n", config->targetFile);
			RemoveFLACTemp(config->referenceFile, config->targetFile);
			return 0;
		}
		if(config->clock)
		{
			double	elapsedSeconds;
			clock_gettime(CLOCK_MONOTONIC, &end);
			elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
			logmsg(" - clk: Decoding FLAC took %0.2fs\n", elapsedSeconds);
		}
		compare = fopen(tmpFile, "rb");
	}
	else
		compare = fopen(config->targetFile, "rb");
	if(!compare)
	{
		CloseFiles(&reference, &compare);
		RemoveFLACTemp(config->referenceFile, config->targetFile);
		CleanUp(ReferenceSignal, ComparisonSignal, config);
		logmsg("\tERROR: Could not open 'Comparison' file:\n\t\"%s\"\n", config->targetFile);
		return 0;
	}

	*ReferenceSignal = CreateAudioSignal(config);
	if(!*ReferenceSignal)
	{
		CloseFiles(&reference, &compare);
		RemoveFLACTemp(config->referenceFile, config->targetFile);
		CleanUp(ReferenceSignal, ComparisonSignal, config);
		return 0;
	}
	(*ReferenceSignal)->role = ROLE_REF;

	*ComparisonSignal = CreateAudioSignal(config);
	if(!*ComparisonSignal)
	{
		CloseFiles(&reference, &compare);
		RemoveFLACTemp(config->referenceFile, config->targetFile);
		CleanUp(ReferenceSignal, ComparisonSignal, config);
		return 0;
	}
	(*ComparisonSignal)->role = ROLE_COMP;

	logmsg("\n* Loading 'Reference' audio file %s\n", config->referenceFile);
	if(!LoadFile(reference, *ReferenceSignal, config, config->referenceFile))
	{
		CloseFiles(&reference, &compare);
		RemoveFLACTemp(config->referenceFile, config->targetFile);
		CleanUp(ReferenceSignal, ComparisonSignal, config);
		return 0;
	}

	logmsg("\n* Loading 'Comparison' audio file %s\n", config->targetFile);
	if(!LoadFile(compare, *ComparisonSignal, config, config->targetFile))
	{
		CloseFiles(&reference, &compare);
		RemoveFLACTemp(config->referenceFile, config->targetFile);
		CleanUp(ReferenceSignal, ComparisonSignal, config);
		return 0;
	}

	CloseFiles(&reference, &compare);
	RemoveFLACTemp(config->referenceFile, config->targetFile);

	config->referenceFramerate = (*ReferenceSignal)->framerate;

	CompareFrameRates((*ReferenceSignal)->framerate, (*ComparisonSignal)->framerate, config);

	if(config->channel == 's')
	{
		int block = NO_INDEX;

		block = GetFirstMonoIndex(config);
		if(block != NO_INDEX)
		{
			logmsg("\n* Comparing Stereo channel amplitude\n");
			if(config->verbose)
				logmsg(" - Mono block used for balance: %s# %d\n", 
					GetBlockName(config, block), GetBlockSubIndex(config, block));
			CheckBalance(*ReferenceSignal, block, config);
			CheckBalance(*ComparisonSignal, block, config);
		}
		else
		{
			logmsg(" - No mono block for stereo balance check\n");
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
		MaxRef = FindMaxAmplitude(*ReferenceSignal);
		if(!MaxRef.magnitude)
		{
			logmsg("Could not detect Max amplitude in 'Reference' File for normalization\n");
			CloseFiles(&reference, &compare);
			CleanUp(ReferenceSignal, ComparisonSignal, config);
			return 0;
		}
		MaxTar = FindMaxAmplitude(*ComparisonSignal);
		if(!MaxTar.magnitude)
		{
			logmsg("Could not detect Max amplitude in 'Comparison' file for normalization\n");
			CloseFiles(&reference, &compare);
			CleanUp(ReferenceSignal, ComparisonSignal, config);
			return 0;
		}
	
		ratioTar = (double)0x7FFF/MaxTar.magnitude;
		NormalizeAudioByRatio(*ComparisonSignal, ratioTar);
		ComparisonLocalMaximum = FindLocalMaximumAroundSample(*ComparisonSignal, MaxRef);
		if(!ComparisonLocalMaximum)
		{
			logmsg("Could not detect Max amplitude in 'Comparison' file for normalization\n");
			CloseFiles(&reference, &compare);
			CleanUp(ReferenceSignal, ComparisonSignal, config);
			return 0;
		}

		ratioRef = (ComparisonLocalMaximum/MaxRef.magnitude);
		NormalizeAudioByRatio(*ReferenceSignal, ratioRef);

		// Uncomment if you want to check the WAV files as normalized
		//SaveWAVEChunk(NULL, *ReferenceSignal, (*ReferenceSignal)->Samples, 0, (*ReferenceSignal)->header.fmt.Subchunk2Size, config); 
		//SaveWAVEChunk(NULL, *ComparisonSignal, (*ComparisonSignal)->Samples, 0, (*ComparisonSignal)->header.fmt.Subchunk2Size, config); 
	}
	
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
			logmsg("Could not detect Max amplitude in 'Reference' File for normalization\n");
			CloseFiles(&reference, &compare);
			CleanUp(ReferenceSignal, ComparisonSignal, config);
			return 0;
		}
		MaxTar = FindMaxMagnitudeBlock(*ComparisonSignal, config);
		if(!MaxTar.magnitude)
		{
			logmsg("Could not detect Max amplitude in 'Comparison' file for normalization\n");
			CloseFiles(&reference, &compare);
			CleanUp(ReferenceSignal, ComparisonSignal, config);
			return 0;
		}
	
		ComparisonLocalMaximum = FindLocalMaximumInBlock(*ComparisonSignal, MaxRef, config);
		if(!ComparisonLocalMaximum)
		{
			PrintFrequenciesWMagnitudes(*ReferenceSignal, config);
			PrintFrequenciesWMagnitudes(*ComparisonSignal, config);

			logmsg("Could not detect Local Maximum in 'Comparison' file for normalization\n");
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
			logmsg("\n=====WARNING=====\n");
			logmsg("\tAverage frequency difference after normalization between the signals is too high.\n");
			logmsg("\t(Ratio:%g to 1)\n", ratio);
			logmsg("\tIf results make no sense (emply graphs), please try the following in the Extra Commands box:\n");
			logmsg("\t* Compare just the left channel: -a l\n");
			logmsg("\t* Compare just the right channel: -a r\n");
			logmsg("\t* Use Time Domain normalization: -n t\n");
			logmsg("\tThis can be caused by comparing different signals, a capacitor problem in one channel\n");
			logmsg("\twhen comparing stereo recorings, etc. Details stored in log file.\n\n");
			PrintFrequenciesWMagnitudes(*ReferenceSignal, config);
			PrintFrequenciesWMagnitudes(*ComparisonSignal, config);
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

	logmsg("\n* Processing Signal Frequencies and Amplitudes\n");
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

	CalculateAmplitudes(*ReferenceSignal, ZeroDbMagnitudeRef, config);
	CalculateAmplitudes(*ComparisonSignal, ZeroDbMagnitudeRef, config);

	// Display Aboslute and independent Noise Floor
	/*
	if(!config->ignoreFloor)
	{
		FindStandAloneFloor(*ReferenceSignal, config);
		FindStandAloneFloor(*ComparisonSignal, config);
	}
	*/

	/* analyze silence floor if available */
	if(!config->ignoreFloor && (*ReferenceSignal)->hasFloor)
		FindFloor(*ReferenceSignal, config);
	if(!config->ignoreFloor && (*ComparisonSignal)->hasFloor)
		FindFloor(*ComparisonSignal, config);

	//DetectOvertoneStart(*ReferenceSignal, config);
	//DetectOvertoneStart(*ComparisonSignal, config);

	/* Detect Signal Floor */
	if((*ReferenceSignal)->hasFloor && !config->ignoreFloor && 
		(*ReferenceSignal)->floorAmplitude != 0.0 && (*ReferenceSignal)->floorAmplitude > config->significantAmplitude)
	{
		config->significantAmplitude = (*ReferenceSignal)->floorAmplitude;
		CreateBaseName(config);
	}

	logmsg(" - Using %gdBFS as minimum significant amplitude for analisys\n",
		config->significantAmplitude);

	if(config->verbose)
	{
		PrintFrequencies(*ReferenceSignal, config);
		PrintFrequencies(*ComparisonSignal, config);
	}

	config->referenceSignal = *ReferenceSignal;
	config->comparisonSignal = *ComparisonSignal;
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
	int					found = 0;
	struct	timespec	start, end;
	double				seconds = 0;

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	if(!file)
		return 0;

	if(fread(&Signal->header.riff, 1, sizeof(riff_hdr), file) != sizeof(riff_hdr))
	{
		logmsg("\tERROR: Invalid Audio file. File too small.\n");
		return(0);
	}

	if(strncmp((char*)Signal->header.riff.RIFF, "RIFF", 4) != 0)
	{
		logmsg("\tERROR: Invalid Audio file. RIFF header not found.\n");
		return(0);
	}

	if(strncmp((char*)Signal->header.riff.WAVE, "WAVE", 4) != 0)
	{
		logmsg("\tERROR: Invalid Audio file. WAVE header not found.\n");
		return(0);
	}

	do
	{
		sub_chunk	schunk;

		if(fread(&schunk, 1, sizeof(sub_chunk), file) != sizeof(sub_chunk))
		{
			logmsg("\tERROR: Invalid Audio file. File too small.\n");
			return(0);
		}
		if(strncmp((char*)schunk.chunkID, "fmt", 3) != 0)
			fseek(file, schunk.Size*sizeof(uint8_t), SEEK_CUR);
		else
		{
			fseek(file, -1*sizeof(sub_chunk), SEEK_CUR);
			found = 1;
		}
	}while(!found);

	if(fread(&Signal->header.fmt, 1, sizeof(fmt_hdr), file) != sizeof(fmt_hdr))
	{
		logmsg("\tERROR: Invalid Audio file. File too small.\n");
		return(0);
	}

	if(Signal->header.fmt.AudioFormat != 1) /* Check for PCM */
	{
		logmsg("\tERROR: Invalid Audio File. Only PCM encoding is supported\n\tPlease convert file to PCM 16 bit 44/48kHz.");
		return(0);
	}

	if(Signal->header.fmt.NumOfChan != 2) /* Check for Stereo */
	{
		logmsg("\tERROR: Invalid Audio file. Only Stereo files are supported.\n");
		return(0);
	}

	if(Signal->header.fmt.bitsPerSample != 16) /* Check bit depth */
	{
		logmsg("\tERROR: Invalid Audio file. Only 16 bit supported for now.\n\tPlease use 16 bit 44/48kHz.");
		return(0);
	}
	
	if(Signal->header.fmt.SamplesPerSec/2 < config->endHz)
	{
		logmsg(" - %d Hz sample rate was too low for %d-%d Hz analysis\n",
			 Signal->header.fmt.SamplesPerSec, config->startHz, config->endHz);
		config->endHz = Signal->header.fmt.SamplesPerSec/2;
		logmsg(" - changed to %d-%d Hz\n", config->startHz, config->endHz);
	}

	// Default if none is found
	Signal->framerate = GetMSPerFrame(Signal, config);

	seconds = (double)Signal->header.fmt.Subchunk2Size/4.0/(double)Signal->header.fmt.SamplesPerSec;
	logmsg(" - Audio file is %dHz %dbits and %g seconds long\n", 
		Signal->header.fmt.SamplesPerSec, Signal->header.fmt.bitsPerSample, seconds);

	if(seconds < GetSignalTotalDuration(Signal->framerate, config))
	{
		logmsg(" - WARNING: Estimated file length is smaller than the expected %g seconds\n",
				GetSignalTotalDuration(Signal->framerate, config));
		config->smallFile = 1;
	}

	Signal->Samples = (char*)malloc(sizeof(char)*Signal->header.fmt.Subchunk2Size);
	if(!Signal->Samples)
	{
		logmsg("\tERROR: All Chunks malloc failed!\n");
		return(0);
	}

	if(fread(Signal->Samples, 1, sizeof(char)*Signal->header.fmt.Subchunk2Size, file) !=
			 sizeof(char)*Signal->header.fmt.Subchunk2Size)
	{
		logmsg("\tERROR: Could not read the whole sample block from disk to RAM.\n");
		return(0);
	}

	if(config->clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - clk: Loading Audio took %0.2fs\n", elapsedSeconds);
	}

	if(GetFirstSyncIndex(config) != NO_INDEX)
	{
		if(config->clock)
			clock_gettime(CLOCK_MONOTONIC, &start);

		/* Find the start offset */
		if(config->verbose)
			logmsg(" - Sync pulse train: ");
		Signal->startOffset = DetectPulse(Signal->Samples, Signal->header, Signal->role, config);
		if(Signal->startOffset == -1)
		{
			logmsg("\nERROR: Starting pulse train was not detected.\n");
			return 0;
		}
		if(config->verbose)
			logmsg(" %gs [%ld bytes]", 
				BytesToSeconds(Signal->header.fmt.SamplesPerSec, Signal->startOffset),
				Signal->startOffset);

		if(GetLastSyncIndex(config) != NO_INDEX)
		{
			double diff = 0, expected = 0;

			if(config->verbose)
				logmsg(" to");
			Signal->endOffset = DetectEndPulse(Signal->Samples, Signal->startOffset, Signal->header, Signal->role, config);
			if(Signal->endOffset == -1)
			{
				logmsg("\nERROR: Trailing sync pulse train was not detected, aborting.\n");
				logmsg("\tPlease record the whole audio sequence.\n");
				return 0;
			}
			if(config->verbose)
				logmsg(" %gs [%ld bytes]\n", 
					BytesToSeconds(Signal->header.fmt.SamplesPerSec, Signal->endOffset),
					Signal->endOffset);
			Signal->framerate = CalculateFrameRate(Signal, config);
			logmsg(" - Detected %g Hz video signal (%gms per frame) from Audio file\n", 
						roundFloat(CalculateScanRate(Signal)), Signal->framerate);

			expected = GetMSPerFrame(Signal, config);
			diff = fabs(100.0 - Signal->framerate*100.0/expected);
			if(diff > 1.0)
			{
				logmsg("\nERROR: Framerate is %g%% different from the expected %gms.\n",
						diff, expected);
				logmsg("\tThis might be due a mismatched profile.\n");
				logmsg("\tIf you want to ignore this and compare the files, use -I.\n");
				if(!config->ignoreFrameRateDiff)
					return 0;
			}
		}
		else
		{
			logmsg(" - ERROR: Trailing sync pulse train not defined in config file, aborting.\n");
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
		/* Find the start offset */
		
		logmsg(" - Detecting audio signal: ");
		Signal->startOffset = DetectSignalStart(Signal->Samples, Signal->header, 0, 0, NULL, config);
		if(Signal->startOffset == -1)
		{
			logmsg("\nERROR: Starting position was not detected.\n");
			return 0;
		}
		logmsg(" %gs [%ld bytes]\n", 
				BytesToSeconds(Signal->header.fmt.SamplesPerSec, Signal->startOffset),
				Signal->startOffset);
		Signal->endOffset = SecondsToBytes(Signal->header.fmt.SamplesPerSec, 
								GetSignalTotalDuration(Signal->framerate, config), NULL, NULL, NULL);
		
	/*
		double diff = 0, expected = 0;

		// Files must be identically trimmed for this to work at some level
		Signal->startOffset = 0;
		Signal->endOffset = Signal->header.fmt.Subchunk2Size;

		expected = Signal->framerate;

		if(Signal->role == ROLE_REF)
		{
			double seconds = 0;
			seconds = BytesToSeconds(Signal->header.fmt.SamplesPerSec, Signal->endOffset);
			config->NoSyncTotalFrames = (seconds*1000)/expected;
			Signal->framerate = expected;
			logmsg(" - Loaded %g Hz video signal (%gms per frame) from profile file\n", 
						CalculateScanRate(Signal), Signal->framerate);
		}
		else
		{
			Signal->framerate = CalculateFrameRateNS(Signal, config->NoSyncTotalFrames, config);
			logmsg(" - Detected %g Hz video signal (%gms per frame) from Audio file\n", 
						CalculateScanRate(Signal), Signal->framerate);
		}

		diff = fabs(100.0 - Signal->framerate*100.0/expected);
		if(diff > 1.0)
		{
			logmsg("\nERROR: Framerate is %g%% different from the expected %gms.\n",
					diff, expected);
			logmsg("\tThis might be due a mismatched profile.\n");
			logmsg("\tIf you want to ignore this and compare the files, use -I.\n");
			if(!config->ignoreFrameRateDiff)
				return 0;
		}
		*/
	}

	if(seconds < GetSignalTotalDuration(Signal->framerate, config))
		logmsg(" - Adjusted File length is smaller than the expected %gs\n",
				GetSignalTotalDuration(Signal->framerate, config));

	if(GetFirstSilenceIndex(config) != NO_INDEX)
		Signal->hasFloor = 1;

	sprintf(Signal->SourceFile, "%s", fileName);

	return 1;
}

int MoveSampleBlockInternal(AudioSignal *Signal, long int element, long int pos, long int internalSyncOffset, parameters *config)
{
	char		*sampleBuffer = NULL;
	double		seconds = 0;
	long int	buffsize = 0, frames = 0, bytes = 0;

	frames = GetInternalSyncTotalLength(element, config);
	if(!frames)
	{
		logmsg("\tERROR: Internal Sync block has no frame duration. Aborting.\n");
		return 0;
	}

	seconds = FramesToSeconds(frames, config->referenceFramerate);
	bytes = SecondsToBytes(Signal->header.fmt.SamplesPerSec, seconds, NULL, NULL, NULL);

	if(pos + bytes > Signal->header.fmt.Subchunk2Size)
	{
		bytes = Signal->header.fmt.Subchunk2Size - pos;
		if(config->verbose)
			logmsg(" - Inernal sync adjust: Signal is smaller than expected\n");
	}

	if(config->verbose)
		logmsg(" - Internal Segment Info:\n\tFinal Offset: %ld Frames: %d Seconds: %g Bytes: %ld\n",
				pos+internalSyncOffset, frames, seconds, bytes);
	if(bytes <= internalSyncOffset)
	{
		logmsg("\tERROR: Internal Sync could not be aligned, signal out of bounds.\n");
		return 0;
	}
	buffsize = bytes - internalSyncOffset;

	sampleBuffer = (char*)malloc(sizeof(char)*buffsize);
	if(!sampleBuffer)
	{
		logmsg("\tERROR: Out of memory.\n");
		return 0;
	}

	/*
	if(config->verbose)
	{
		logmsg(" - MOVEMENTS:\n");
		logmsg("\tCopy: From %ld Bytes: %ld\n",
				pos + internalSyncOffset, buffsize);
		logmsg("\tZero Out: Pos: %ld Bytes: %ld\n",
				pos, bytes);
		logmsg("\tStore: Pos: %ld Bytes: %ld\n",
				pos, buffsize);
	}
	*/
	memcpy(sampleBuffer, Signal->Samples + pos + internalSyncOffset, buffsize);
	memset(Signal->Samples + pos, 0, bytes);
	memcpy(Signal->Samples + pos, sampleBuffer, buffsize);

	free(sampleBuffer);
	return 1;
}

int MoveSampleBlockExternal(AudioSignal *Signal, long int element, long int pos, long int internalSyncOffset, long int paddingSize, parameters *config)
{
	char		*sampleBuffer = NULL;
	double		seconds = 0;
	long int	buffsize = 0, frames = 0, bytes = 0;

	frames = GetInternalSyncTotalLength(element, config);
	if(!frames)
	{
		logmsg("\tERROR: Internal Sync block has no frame duration. Aborting.\n");
		return 0;
	}

	seconds = FramesToSeconds(frames, config->referenceFramerate);
	bytes = SecondsToBytes(Signal->header.fmt.SamplesPerSec, seconds, NULL, NULL, NULL);

	if(pos + bytes > Signal->header.fmt.Subchunk2Size)
	{
		bytes = Signal->header.fmt.Subchunk2Size - pos;
		if(config->verbose)
			logmsg(" - Inernal sync adjust: Signal is smaller than expected\n");
	}
	if(config->verbose)
		logmsg(" - Internal Segment Info:\n\tFinal Offset: %ld Frames: %d Seconds: %g Bytes: %ld\n",
				pos+internalSyncOffset, frames, seconds, bytes);
	if(bytes <= internalSyncOffset)
	{
		logmsg("\tERROR: Internal Sync could not be aligned, signal out of bounds.\n");
		return 0;
	}

	if(pos + internalSyncOffset + bytes - paddingSize > Signal->header.fmt.Subchunk2Size)
		bytes = Signal->header.fmt.Subchunk2Size - (pos + internalSyncOffset)+paddingSize;

	buffsize = bytes - paddingSize;

	sampleBuffer = (char*)malloc(sizeof(char)*buffsize);
	if(!sampleBuffer)
	{
		logmsg("\tERROR: Out of memory.\n");
		return 0;
	}

	/*
	if(config->verbose)
	{
		logmsg(" - MOVEMENTS:\n");
		logmsg("\tCopy: From %ld Bytes: %ld\n",
				pos + internalSyncOffset, buffsize);
		logmsg("\tZero Out: Pos: %ld Bytes: %ld\n",
				pos + internalSyncOffset, buffsize);
		logmsg("\tStore: Pos: %ld Bytes: %ld\n",
				pos, buffsize);
	}
	*/
	memcpy(sampleBuffer, Signal->Samples + pos + internalSyncOffset, buffsize);
	memset(Signal->Samples + pos + internalSyncOffset, 0, buffsize);
	memcpy(Signal->Samples + pos, sampleBuffer, buffsize);

	free(sampleBuffer);
	return 1;
}

int ProcessInternal(AudioSignal *Signal, long int element, long int pos, int *syncinternal, long int *advanceFrames, int knownLength, parameters *config)
{
	if(*syncinternal)
		*syncinternal = 0;
	else
	{
		int			syncTone = 0, lastsync = 0;
		double		syncLen = 0;
		long int	internalSyncOffset = 0,
					endPulse = 0, pulseLength = 0, syncLength = 0;

		*syncinternal = 1;
		syncTone = GetInternalSyncTone(element, config);
		syncLen = GetInternalSyncLen(element, config);
		internalSyncOffset = DetectSignalStart(Signal->Samples, Signal->header, pos, syncTone, &endPulse, config);
		if(internalSyncOffset == -1)
		{
			logmsg("\tERROR: No signal found while in internal sync detection. Aborting\n");
			return 0;
		}

		pulseLength = endPulse - internalSyncOffset;
		syncLength = SecondsToBytes(Signal->header.fmt.SamplesPerSec, syncLen, NULL, NULL, NULL);
		internalSyncOffset -= pos;

		lastsync = GetLastSyncElementIndex(config);
		if(lastsync == NO_INDEX)
		{
			logmsg("\tERROR: Profile has no Sync Index. Aborting.\n");
			return 0;
		}
		if(knownLength) // lastsync > element
		{
			logmsg(" - %s command delay: %g ms [%g frames]\n",
				GetBlockName(config, element),
				BytesToSeconds(Signal->header.fmt.SamplesPerSec, internalSyncOffset)*1000.0,
				BytesToFrames(Signal->header.fmt.SamplesPerSec, internalSyncOffset, config->referenceFramerate));

			if(config->verbose)
					logmsg("  > Found at: %ld Previous: %ld Offset: %ld\n\tPulse Length: %ld Half Sync Length: %ld\n", 
						pos + internalSyncOffset, pos, internalSyncOffset, pulseLength, syncLength/2);

			// skip sync tone-which is silence-taken from config file
			internalSyncOffset += syncLength;

			if(!MoveSampleBlockInternal(Signal, element, pos, internalSyncOffset, config))
				return 0;
		}
		else  // Our sync is outside the frame detection zone
		{
			long int 	halfSyncLength = 0; //, diffOffset = 0;

			halfSyncLength = syncLength/2;

			if(pulseLength > halfSyncLength)
				pulseLength = halfSyncLength; 

			//diffOffset = halfSyncLength - pulseLength;
			/*
			if(internalSyncOffset == 0 && diffOffset) // we are in negative offset territory (emulator)
			{
				logmsg(" - %s command delay: %g ms [%g frames] (Emulator)\n",
					GetBlockName(config, element),
					-1.0*BytesToSeconds(Signal->header.fmt.SamplesPerSec, diffOffset)*1000.0,
					-1.0*BytesToFrames(Signal->header.fmt.SamplesPerSec, diffOffset, config->referenceFramerate));

				if(config->verbose)
					logmsg("  > Found at: %ld Previous: %ld Offset: %ld\n\tPulse Length: %ld Half Sync Length: %ld\n", 
						pos + internalSyncOffset - diffOffset, pos, diffOffset, pulseLength, halfSyncLength);

			}
			else
			*/
			{
//				pulseLength = halfSyncLength; 

				logmsg(" - %s command delay: %g ms [%g frames]\n",
					GetBlockName(config, element),
					BytesToSeconds(Signal->header.fmt.SamplesPerSec, internalSyncOffset)*1000.0,
					BytesToFrames(Signal->header.fmt.SamplesPerSec, internalSyncOffset, config->referenceFramerate));

				if(config->verbose)
					logmsg("  > Found at: %ld Previous: %ld\n\tPulse Length: %ld Half Sync Length: %ld\n", 
						pos + internalSyncOffset, pos, pulseLength, halfSyncLength);
			}
			
			// skip the pulse real duration to sync perfectly
			internalSyncOffset += pulseLength;
			// skip half the sync tone-which is silence-taken from config file
			internalSyncOffset += halfSyncLength;

			if(!MoveSampleBlockExternal(Signal, element, pos, internalSyncOffset, halfSyncLength + pulseLength, config))
				return 0;
		}
		*advanceFrames += internalSyncOffset;
	}
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
	long int		loadedBlockSize = 0, i = 0, syncAdvance = 0;
	struct timespec	start, end;
	int				leftover = 0, discardBytes = 0, syncinternal = 0;
	double			leftDecimals = 0;

	pos = Signal->startOffset;

	longest = FramesToSeconds(Signal->framerate, GetLongestElementFrames(config));
	if(!longest)
	{
		logmsg("\tERROR: Block definitions are invalid, total length is 0.\n");
		return 0;
	}

	buffersize = SecondsToBytes(Signal->header.fmt.SamplesPerSec, longest, NULL, NULL, NULL);
	buffer = (char*)malloc(buffersize);
	if(!buffer)
	{
		logmsg("\tERROR: malloc failed.\n");
		return(0);
	}

	if(!initWindows(&windows, Signal->header.fmt.SamplesPerSec, config->window, config))
		return 0;

	if(config->drawWindows)
	{
		//VisualizeWindows(&windows, config);
		PlotBetaFunctions(config);
	}

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	while(i < config->types.totalChunks)
	{
		double duration = 0, framerate = 0;
		long int frames = 0, difference = 0;

		Signal->Blocks[i].index = GetBlockSubIndex(config, i);
		Signal->Blocks[i].type = GetBlockType(config, i);

		if(!syncinternal)
			framerate = Signal->framerate;
		else
			framerate = config->referenceFramerate;

		frames = GetBlockFrames(config, i);
		duration = FramesToSeconds(framerate, frames);
		
		loadedBlockSize = SecondsToBytes(Signal->header.fmt.SamplesPerSec, duration, &leftover, &discardBytes, &leftDecimals);

		difference = GetByteSizeDifferenceByFrameRate(framerate, frames, Signal->header.fmt.SamplesPerSec, config);

		// config->smallerFramerate
		if(Signal->Blocks[i].type >= TYPE_SILENCE) // We get the smaller window, since we'll truncate
		{
			if(!syncinternal)
				windowUsed = getWindowByLength(&windows, frames, config->smallerFramerate);
			else
				windowUsed = getWindowByLength(&windows, frames, framerate);
		}

/*
		logmsg("loadedBlockSize %ld Diff %ld loadedBlockSize -diff %ld leftover %ld discardBytes %ld leftDecimals %g\n",
				loadedBlockSize, difference, loadedBlockSize - difference, leftover, discardBytes, leftDecimals);
*/
		memset(buffer, 0, buffersize);
		if(pos + loadedBlockSize > Signal->header.fmt.Subchunk2Size)
		{
			config->smallFile = 1;
			logmsg("\tunexpected end of File, please record the full Audio Test from the 240p Test Suite.\n");
			break;
		}
		memcpy(buffer, Signal->Samples + pos, loadedBlockSize-difference);

		if(Signal->Blocks[i].type >= TYPE_SILENCE)
		{
			if(!ExecuteDFFT(&Signal->Blocks[i], (int16_t*)buffer, (loadedBlockSize-difference)/2, Signal->header.fmt.SamplesPerSec, windowUsed, config))
				return 0;

			FillFrequencyStructures(&Signal->Blocks[i], config);
		}

		// MDWAVE exists for this, but just in case it is ever needed within MDFourier
#ifdef CHECKWAV
		if(config->verbose)
		{
			SaveWAVEChunk(NULL, Signal, buffer, i, loadedBlockSize-difference, 0, config);
			SaveWAVEChunk(NULL, Signal, Signal->Samples + pos + loadedBlockSize, i, difference, 1, config);
		}
#endif

		pos += loadedBlockSize;
		pos += discardBytes;

		if(Signal->Blocks[i].type == TYPE_INTERNAL_KNOWN)
		{
			if(!ProcessInternal(Signal, i, pos, &syncinternal, &syncAdvance, 1, config))
				return 0;
		}

		if(Signal->Blocks[i].type == TYPE_INTERNAL_UNKNOWN)
		{
			if(!ProcessInternal(Signal, i, pos, &syncinternal, &syncAdvance, 0, config))
				return 0;
		}

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

#ifdef	FFTSIZEDEBUG
	if(config->fftsize == 0)
		config->fftsize = (long)size;
	else
		if((long)size != config->fftsize)
			logmsg("WARNING: Got Different FFT Size %ld, first is %ld\n", (long)size, config->fftsize);
#endif

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
		{
			signal[i] *= window[i];
#ifdef CHECKWAV
			// for saving the wav with window
			samples[i*2] *= window[i];
			samples[i*2+1] *= window[i];
#endif
		}
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

	limit = config->significantAmplitude;

	if(Signal->role == ROLE_COMP)
		limit += -20;	// Allow going 20 dbfs "deeper"

	for(int freq = 0; freq < config->MaxFreq; freq++)
	{
		/* Out of valid frequencies */
		if(!Signal->Blocks[block].freq[freq].hertz)
			return freq - 1;

		/* Amplitude is too low */
		if(limitRef && Signal->Blocks[block].freq[freq].amplitude < limit)
			return freq;

		/* Amplitude is too low with tolerance */
		if(!limitRef && Signal->Blocks[block].freq[freq].amplitude < limit - config->tolerance*2)
			return freq;
	}

	return config->MaxFreq;
}

double CompareAudioBlocks(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config)
{
	int				block = 0;
	struct timespec	start, end;

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	if(!CreateDifferenceArray(config))
		return 0;

	for(block = 0; block < config->types.totalChunks; block++)
	{
		int refSize = 0, testSize = 0, type = 0;

		/* Ignore Control blocks */
		type = GetBlockType(config, block);
		if(type < TYPE_CONTROL)
			continue;

		if(type != TYPE_SILENCE)
		{
			refSize = CalculateMaxCompare(block, ReferenceSignal, config, 1);
			testSize = CalculateMaxCompare(block, ComparisonSignal, config, 0);
		}
		else
		{
			refSize = config->MaxFreq;
			testSize = config->MaxFreq;
		}

		if(config->verbose)
		{
			OutputFileOnlyStart();
			logmsg("Comparing %s# %d (%d) %ld vs %ld\n", 
					GetBlockName(config, block), GetBlockSubIndex(config, block), block,
					refSize, testSize);
			OutputFileOnlyEnd();
		}
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

/*
			if(!found && GetTypeChannel(config, type) == CHANNEL_NOISE) // search with tolerance, if done in one pass, false positives emerge 
			{
				double	lowest = 22050;
				int 	lowIndex = -1;

				// Find closest match
				for(long int comp = 0; comp < testSize; comp++)
				{
					if(!ComparisonSignal->Blocks[block].freq[comp].matched)
					{
						double hertzDiff;
	
						hertzDiff = fabs(ComparisonSignal->Blocks[block].freq[comp].hertz -
										 ReferenceSignal->Blocks[block].freq[freq].hertz);

						if(hertzDiff <= 2.0)
						{
							if(hertzDiff < lowest)
							{
								lowest = hertzDiff;
								lowIndex = comp;
							}
						}
					}
				}

				if(lowIndex >= 0)
				{
					ComparisonSignal->Blocks[block].freq[lowIndex].matched = freq + 1;
					ReferenceSignal->Blocks[block].freq[freq].matched = lowIndex + 1;

					found = 2;
					index = lowIndex;

					// Adjusted Frequency to tolerance
				}
			}
*/
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
						if(amplitude >= config->significantAmplitude)
						{
							value = (fabs(config->significantAmplitude)-fabs(amplitude))/fabs(config->significantAmplitude);
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
					if(amplitude >= config->significantAmplitude)
					{
						value = (fabs(config->significantAmplitude)-fabs(amplitude))/fabs(config->significantAmplitude);
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

// Find the Maximum Amplitude in the Audio File
MaxVolum FindMaxAmplitude(AudioSignal *Signal)
{
	long int 		i = 0, start = 0, end = 0;
	int16_t			*samples = NULL;
	MaxVolum		maxSampleValue;

	maxSampleValue.magnitude = 0;
	maxSampleValue.offset = 0;
	maxSampleValue.samplerate = Signal->header.fmt.SamplesPerSec;
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

// Find the Maximum Amplitude in the Reference Audio File
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
		pos = start + SecondsToBytes(Signal->header.fmt.SamplesPerSec, tarSeconds, NULL, NULL, NULL);
	}
	else
	{
		pos = start + refMax.offset;
		pos = (double)pos*(double)Signal->header.fmt.SamplesPerSec/(double)refMax.samplerate;
	}

	if(pos > end)
		return 0;

	// Search in 2/faction of Signal->header.fmt.SamplesPerSec
	// around the position of the sample

	fraction = 60.0; // around 1 frame
	if(pos - Signal->header.fmt.SamplesPerSec/fraction >= start)
		start = pos - Signal->header.fmt.SamplesPerSec/fraction;
	
	if(end >= pos + Signal->header.fmt.SamplesPerSec/fraction)
		end = pos + Signal->header.fmt.SamplesPerSec/fraction;

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
			logmsg(" - %s Max Magnitude found in %s# %d (%d) at %g Hz with %g\n", 
					Signal->role == ROLE_REF ? "Reference" : "Comparison",
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
		logmsg(" - %s signal Average Fundamental Magnitude %g from %ld elements\n", 
				Signal->role == ROLE_REF ? "Reference" : "Comparison",
				AvgFundMag, count);
	}

	return AvgFundMag;
}