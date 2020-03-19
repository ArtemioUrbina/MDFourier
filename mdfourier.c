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
int ProcessSignal(AudioSignal *Signal, parameters *config);
int ExecuteDFFT(AudioBlocks *AudioArray, int16_t *samples, size_t size, long samplerate, double *window, int AudioChannels, int ZeroPad, parameters *config);
int ExecuteDFFTInternal(AudioBlocks *AudioArray, int16_t *samples, size_t size, long samplerate, double *window, char channel, int AudioChannels, int ZeroPad, parameters *config);
int CompareAudioBlocks(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config);
int CopySamplesForTimeDomainPlot(AudioBlocks *AudioArray, int16_t *samples, size_t size, size_t diff, long samplerate, double *window, int AudioChannels, parameters *config);
int CopySamplesForTimeDomainPlotInternalSync(AudioBlocks *AudioArray, int16_t *samples, size_t size, int slotForSamples, long samplerate, double *window, int AudioChannels, parameters *config);
void CleanUp(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config);
void CloseFiles(FILE **ref, FILE **comp);
void NormalizeAudio(AudioSignal *Signal);
void NormalizeTimeDomainByFrequencyRatio(AudioSignal *Signal, double normalizationRatio, parameters *config);
double FindClippingAndRatio(AudioSignal *Signal, double normalizationRatio, parameters *config);
int FindClippingAndRatioForBlock(AudioBlocks *AudioArray, double ratio);
void NormalizeBlockByRatio(AudioBlocks *AudioArray, double ratio);
void ProcessWaveformsByBlock(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, double ratioRef, parameters *config);
int FindMaxSampleForWaveform(AudioSignal *Signal, int *block, parameters *config);
int FindMaxSampleInBlock(AudioBlocks *AudioArray);
void FindViewPort(parameters *config);
int ReportClockResults(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config);
int RecalculateFrequencyStructures(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config);
int NormalizeAndFinishProcess(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config);
int FrequencyDomainNormalize(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config);

// Time domain
MaxSample FindMaxSampleAmplitude(AudioSignal *Signal);
void NormalizeAudioByRatio(AudioSignal *Signal, double ratio);
int16_t FindLocalMaximumAroundSample(AudioSignal *Signal, MaxSample refMax);

// Frequency domain
void NormalizeMagnitudesByRatio(AudioSignal *Signal, double ratio, parameters *config);
MaxMagn FindMaxMagnitudeBlock(AudioSignal *Signal, parameters *config);
int FindMultiMaxMagnitudeBlock(AudioSignal *Signal, MaxMagn	*MaxMag, int size, parameters *config);
double FindLocalMaximumInBlock(AudioSignal *Signal, MaxMagn refMax, int allowDifference, parameters *config);
double FindFundamentalMagnitudeAverage(AudioSignal *Signal, parameters *config);

int main(int argc , char *argv[])
{
	AudioSignal  		*ReferenceSignal = NULL;
	AudioSignal  		*ComparisonSignal = NULL;
	parameters			config;
	struct	timespec	start, end;

	if(!Header(0, argc, argv))
		return 1;

	if(!commandline(argc, argv, &config))
	{
		printf("	 -h: Shows command line help\n");
		return 1;
	}

	clock_gettime(CLOCK_MONOTONIC, &start);

	if(!LoadProfile(&config))
	{
		logmsg("Aborting\n");
		return 1;
	}

	if(!SetupFolders(config.outputFolder, "Log", &config))
	{
		logmsg("Aborting\n");
		return 1;
	}

	if(!EndProfileLoad(&config))
	{
		logmsg("Aborting\n");
		return 1;
	}

	if(strcmp(config.referenceFile, config.comparisonFile) == 0)
	{
		CleanUp(&ReferenceSignal, &ComparisonSignal, &config);
		logmsg("Both inputs are the same file %s, skipping to save time\n",
			 config.referenceFile);
		return 1;
	}

	if(!LoadAndProcessAudioFiles(&ReferenceSignal, &ComparisonSignal, &config))
	{
		logmsg("Aborting\n");
		if(config.debugSync)
			printf("\nResults stored in %s%s\n", 
				config.outputPath, 
				config.folderName);
		CleanUp(&ReferenceSignal, &ComparisonSignal, &config);
		return 1;
	}

	if(!ReportClockResults(ReferenceSignal, ComparisonSignal, &config))
	{
		if(config.doClkAdjust)
		{
			if(!RecalculateFrequencyStructures(ReferenceSignal, ComparisonSignal, &config))
			{
				logmsg("Could not recalculate frequencies, Aborting\n");
				return 1;
			}
		}
	}

	logmsg("\n* Comparing frequencies: ");
	if(!CompareAudioBlocks(ReferenceSignal, ComparisonSignal, &config))
	{
		logmsg("Aborting\n");
		return 1;
	}

	FindViewPort(&config);
	
	logmsg("* Plotting results to PNGs:\n");
	PlotResults(ReferenceSignal, ComparisonSignal, &config);

	if(IsLogEnabled())
		endLog();

	/* Clear up everything */
	ReleaseDifferenceArray(&config);

	CleanUp(&ReferenceSignal, &ComparisonSignal, &config);
	fftw_cleanup();

	//if(config.clock)
	{
		int minutes = 0;
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		minutes = elapsedSeconds / 60.0;
		logmsg("* MDFourier Analysis took %0.2f seconds", elapsedSeconds);
		if(minutes)
			logmsg(" (%d minute%s %0.2f seconds)", minutes, minutes == 1 ? "" : "s", elapsedSeconds - minutes*60);
		logmsg("\n");
	}

	printf("\nResults stored in %s%s\n", 
			config.outputPath,
			config.folderName);
	
	return(0);
}

void FindViewPort(parameters *config)
{
	int		type = 0;
	char	*name = NULL;
	double	average = 0, outside = 0, maxDiff = 0;

	average = FindDifferenceAverage(config);
	logmsg("Average difference is %g dBFS\n", average);

	outside = FindDifferencePercentOutsideViewPort(&maxDiff, &type, fabs(config->maxDbPlotZC), config);		
	if(outside)
	{
		name = GetTypeDisplayName(config, type);

		logmsg(" - Differences outside +/-%gdBFS in [%s]: %g%%\n", 
				config->maxDbPlotZC, name, outside);
		if(outside > 8 && config->maxDbPlotZC == DB_HEIGHT)  // if the user has not changed it
		{
			double value = 0;

			value = ceil(FindVisibleInViewPortWithinStandardDeviation(&maxDiff, &outside, type, 1, config));
			if(value != -1 && outside < 5)
				config->maxDbPlotZC = value;
			else
			{
				value = ceil(FindVisibleInViewPortWithinStandardDeviation(&maxDiff, &outside, type, 2, config));
				if(value != -1)
					config->maxDbPlotZC = value;
				// swap above for this to expand fully if needed
				/*  
				value = ceil(FindVisibleInViewPortWithinStandardDeviation(&maxDiff, &outside, type, 2, config));
				if(value != -1 && outside < 5)
					config->maxDbPlotZC = value;
				else
				{
					config->maxDbPlotZC = ceil(maxDiff);
					outside = FindDifferencePercentOutsideViewPort(&maxDiff, &type, fabs(config->maxDbPlotZC), config);		
				}
				*/
			}
			logmsg(" - Auto adjusting viewport to %gdBFS for graphs\n", config->maxDbPlotZC);
			if(outside >= 1)
				logmsg(" - The %g%% of differences in [%s] will not be visible within the %gdBFS for graphs\n - If needed you can graph them all with \"-d %g\" for this particular case\n\n", 
					outside, name, config->maxDbPlotZC, ceil(maxDiff));
		}
	}
	else
		logmsg("\n");

	config->notVisible = outside;
}
void PrintSignalCLKData(AudioSignal *Signal, parameters *config)
{
	if(Signal->EstimatedSR)
	{
		if(!config->doSamplerateAdjust)
			logmsg(", WARNING: sample rate estimated at %gHz from signal length (can be auto matched with -R)",
				Signal->EstimatedSR);
		else
		{
			if(Signal->originalSR)
			{
				logmsg(" [adjusted from %dHz]", 
					Signal->originalSR, Signal->EstimatedSR);
			}
		}
	}
	logmsg("\n");
}

int ReportClockResults(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config)
{
	double refClk = 0, compClk = 0;

	if(!config->clkMeasure)
		return 1;

	refClk = CalculateClk(ReferenceSignal, config);
	compClk = CalculateClk(ComparisonSignal, config);

	logmsg("\n* Estimated %s Clocks based on expected %d Hz on note %s# %d:\n", 
		config->clkName, config->clkFreq, GetBlockName(config, config->clkBlock), 
		GetBlockSubIndex(config, config->clkBlock));
	logmsg(" - Reference: %gHz", refClk);
	PrintSignalCLKData(ReferenceSignal, config);

	logmsg(" - Comparison: %gHz", compClk);
	PrintSignalCLKData(ComparisonSignal, config);

	config->centsDifferenceCLK = 1200*log2(refClk/compClk);
	if(fabs(config->centsDifferenceCLK) >= SIG_CENTS_DIFF)
	{
		logmsg(" - Pitch difference in cents: %g\n", config->centsDifferenceCLK);
		if(!config->doClkAdjust)
		{
			if(config->RefCentsDifferenceSR || config->ComCentsDifferenceSR)
			{
				double diff = 0;
	
				if(config->RefCentsDifferenceSR)
					diff = fabs(config->RefCentsDifferenceSR - config->centsDifferenceCLK);
				else
					diff = fabs(config->ComCentsDifferenceSR - config->centsDifferenceCLK);
				if(diff >= 5) // Same issue between both, reported above
					logmsg(" - WARNING: Clocks don't match, results may vary considerably. Can adjust with -j\n");
				else if(diff >= 1)
					logmsg(" - WARNING: Clocks don't match, results will show differences. Can adjust with -j\n");
			}
			else
				logmsg(" - WARNING: Clocks don't match, results may vary considerably. Can adjust with -j\n");
			config->diffClkNoMatch = 1;
		}
		return 0;
	}
	else
	{
		if(config->doClkAdjust)
			logmsg(" - Pitch difference in cents: %g, ignoring -j\n", config->centsDifferenceCLK);
	}
	return 1;
}

void RemoveFLACTemp(char *referenceFile, char *comparisonFile, parameters *config)
{
	char tmpFile[BUFFER_SIZE];

	if(IsFlac(referenceFile))
	{
		renameFLAC(referenceFile, tmpFile, config->tmpPath);
		remove(tmpFile);
	}
	if(IsFlac(comparisonFile))
	{
		renameFLAC(comparisonFile, tmpFile, config->tmpPath);
		remove(tmpFile);
	}
}

int LoadAudioFiles(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config)
{
	FILE				*reference = NULL;
	FILE				*compare = NULL;

	if(IsFlac(config->referenceFile))
	{
		char tmpFile[BUFFER_SIZE];
		struct	timespec	start, end;

		if(config->clock)
			clock_gettime(CLOCK_MONOTONIC, &start);

		if(config->verbose) { logmsg(" - Decoding FLAC\n"); }
		renameFLAC(config->referenceFile, tmpFile, config->tmpPath);
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
		RemoveFLACTemp(config->referenceFile, config->comparisonFile, config);
		logmsg("\tERROR: Could not open 'Reference' file:\n\t\"%s\"\n", config->referenceFile);
		return 0;
	}

	if(IsFlac(config->comparisonFile))
	{
		char tmpFile[BUFFER_SIZE];
		struct	timespec	start, end;

		if(config->clock)
			clock_gettime(CLOCK_MONOTONIC, &start);

		if(config->verbose) { logmsg(" - Decoding FLAC\n"); }
		renameFLAC(config->comparisonFile, tmpFile, config->tmpPath);
		if(!FLACtoWAV(config->comparisonFile, tmpFile))
		{
			CloseFiles(&reference, &compare);
			logmsg("\nERROR: Invalid FLAC file %s\n", config->comparisonFile);
			RemoveFLACTemp(config->referenceFile, config->comparisonFile, config);
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
		compare = fopen(config->comparisonFile, "rb");
	if(!compare)
	{
		CloseFiles(&reference, &compare);
		RemoveFLACTemp(config->referenceFile, config->comparisonFile, config);
		logmsg("\tERROR: Could not open 'Comparison' file:\n\t\"%s\"\n", config->comparisonFile);
		return 0;
	}

	*ReferenceSignal = CreateAudioSignal(config);
	if(!*ReferenceSignal)
	{
		CloseFiles(&reference, &compare);
		RemoveFLACTemp(config->referenceFile, config->comparisonFile, config);
		return 0;
	}
	(*ReferenceSignal)->role = ROLE_REF;

	*ComparisonSignal = CreateAudioSignal(config);
	if(!*ComparisonSignal)
	{
		CloseFiles(&reference, &compare);
		RemoveFLACTemp(config->referenceFile, config->comparisonFile, config);
		return 0;
	}
	(*ComparisonSignal)->role = ROLE_COMP;

	logmsg("\n* Loading 'Reference' audio file %s\n", config->referenceFile);
	if(!LoadFile(reference, *ReferenceSignal, config, config->referenceFile))
	{
		CloseFiles(&reference, &compare);
		RemoveFLACTemp(config->referenceFile, config->comparisonFile, config);
		return 0;
	}

	logmsg("\n* Loading 'Comparison' audio file %s\n", config->comparisonFile);
	if(!LoadFile(compare, *ComparisonSignal, config, config->comparisonFile))
	{
		CloseFiles(&reference, &compare);
		RemoveFLACTemp(config->referenceFile, config->comparisonFile, config);
		return 0;
	}

	CloseFiles(&reference, &compare);
	RemoveFLACTemp(config->referenceFile, config->comparisonFile, config);
	return 1;
}

/* Although dithering would be better, there has been no need */
/* Tested a file scaled with ths method against itself using  */
/* the frequency domain solution, and differences are negligible */
/* (less than 0.2dBFS) */

int TimeDomainNormalize(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config)
{
	MaxSample			MaxRef, MaxTar;
	double				ComparisonLocalMaximum = 0;
	double				ratioTar = 0, ratioRef = 0;

	// Find Normalization factors
	MaxRef = FindMaxSampleAmplitude(*ReferenceSignal);
	if(!MaxRef.maxSample)
	{
		logmsg("ERROR: Could not detect Max amplitude in 'Reference' File for normalization\n");
		return 0;
	}
	MaxTar = FindMaxSampleAmplitude(*ComparisonSignal);
	if(!MaxTar.maxSample)
	{
		logmsg("ERROR: Could not detect Max amplitude in 'Comparison' file for normalization\n");
		return 0;
	}

	ratioTar = MAXINT16/(double)MaxTar.maxSample;
	NormalizeAudioByRatio(*ComparisonSignal, ratioTar);
	ComparisonLocalMaximum = FindLocalMaximumAroundSample(*ComparisonSignal, MaxRef);
	if(!ComparisonLocalMaximum)
	{
		logmsg("ERROR: Could not detect Max amplitude in 'Comparison' file for normalization\n");
		return 0;
	}

	ratioRef = (ComparisonLocalMaximum/(double)MaxRef.maxSample);
	NormalizeAudioByRatio(*ReferenceSignal, ratioRef);

	// Uncomment if you want to check the WAV files as normalized
	//SaveWAVEChunk(NULL, *ReferenceSignal, (*ReferenceSignal)->Samples, 0, (*ReferenceSignal)->header.data.DataSize, config); 
	//SaveWAVEChunk(NULL, *ComparisonSignal, (*ComparisonSignal)->Samples, 0, (*ComparisonSignal)->header.data.DataSize, config); 
	return 1;
}

/* The default */
int FrequencyDomainNormalize(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config)
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
		logmsg("ERROR: Could not detect Max amplitude in 'Reference' File for normalization\n");
		return 0;
	}
	MaxTar = FindMaxMagnitudeBlock(*ComparisonSignal, config);
	if(!MaxTar.magnitude)
	{
		logmsg("ERROR: Could not detect Max amplitude in 'Comparison' file for normalization\n");
		return 0;
	}

	ComparisonLocalMaximum = FindLocalMaximumInBlock(*ComparisonSignal, MaxRef, 0, config);
	if(ComparisonLocalMaximum)
		ratioRef = ComparisonLocalMaximum/MaxRef.magnitude;

	/* Detect extreme cases, and try another approach */
	if(ComparisonLocalMaximum == 0 || (ratioRef && 1.0/ratioRef > FREQDOMRATIO))
	{
		int		found = 0, pos = 1, allowDifference = 0, tries = 0;
		MaxMagn	MaxRefArray[FREQDOMTRIES];
		double	ComparisonLocalMaximumArray = 0, ratioRefArray = 0;

		memset(MaxRefArray, 0, FREQDOMTRIES*sizeof(MaxMagn));
		if(FindMultiMaxMagnitudeBlock(*ReferenceSignal, MaxRefArray, FREQDOMTRIES, config))
		{
			// We have a do for a second cycle allowing tolerance
			do
			{
				while(MaxRefArray[pos].magnitude != 0 && pos < FREQDOMTRIES)
				{
					if(config->verbose) {
						logmsg(" - Reference Max Magnitude[%d] found in %s# %d (%d) at %g Hz with %g\n", 
							pos,
							GetBlockName(config, MaxRefArray[pos].block), GetBlockSubIndex(config, MaxRefArray[pos].block),
							MaxRefArray[pos].block, MaxRefArray[pos].hertz, MaxRefArray[pos].magnitude);
					}

					ratioRefArray = 0;
					ComparisonLocalMaximumArray = FindLocalMaximumInBlock(*ComparisonSignal, MaxRefArray[pos], allowDifference, config);
					if(ComparisonLocalMaximumArray)
					{
						ratioRefArray = ComparisonLocalMaximumArray/MaxRefArray[pos].magnitude;
						if(config->verbose) { logmsg(" - Comparision ratio is %g (%g/%g)\n", 1.0/ratioRefArray, ComparisonLocalMaximumArray, MaxRefArray[pos].magnitude); }
						if(1.0/ratioRefArray <= FREQDOMRATIO)
						{
							found = 1;
							break;
						}
					}
					pos++;
				}

				if(found){
					int copy = 0;

					if(ComparisonLocalMaximum)  // we are here because no clean match was found
						copy = 1;
					else  /* we are here because of ratio */ {
						if(ratioRefArray < ratioRef)
							copy = 1;

						if(!ratioRef)
							copy = 1;
					}

					if(copy){
						ComparisonLocalMaximum = ComparisonLocalMaximumArray;
						ratioRef = ratioRefArray;
						config->frequencyNormalizationTries = pos + 1;
					}
					else {
						if(config->verbose) { logmsg(" - Alternative matches were worse than original, (%g<-%g)reverting\n", ratioRefArray, ratioRef); }
					}
				}
				else {
					config->frequencyNormalizationTries = -1;
					allowDifference = 1;
					pos = 0;
				}
				tries ++;
			}while(tries == 1 && allowDifference == 1);
		}
	}
	else
		config->frequencyNormalizationTries = 0;

	if(!ComparisonLocalMaximum || !ratioRef)
	{
		PrintFrequenciesWMagnitudes(*ReferenceSignal, config);
		PrintFrequenciesWMagnitudes(*ComparisonSignal, config);

		logmsg("ERROR: Could not detect Local Maximum in 'Comparison' file for normalization\n");
		return 0;
	}

	NormalizeMagnitudesByRatio(*ReferenceSignal, ratioRef, config);

	RefAvg = FindFundamentalMagnitudeAverage(*ReferenceSignal, config);
	CompAvg = FindFundamentalMagnitudeAverage(*ComparisonSignal, config);
	
	if(RefAvg > CompAvg)
		ratio = RefAvg/CompAvg;
	else
		ratio = CompAvg/RefAvg;
	if(ratio > FREQDOMRATIO)
	{
		logmsg("\n=====WARNING=====\n");
		logmsg("\tAverage frequency difference after normalization between the signals is too high. (Ratio:%g to 1)\n", ratio);
		logmsg("\tIf results make no sense please try the following in the Extra Commands box:\n");
		logmsg("\t* Use Time Domain normalization: -n t\n");
		logmsg("\tThis can be caused by: comparing very different signals, a capacitor problem,\n");
		logmsg("\tframerate difference causing pitch drifting, an unusual frequency scenario, etc.\n");
	}

	/* This is just for waveform visualization */
	if((config->hasTimeDomain && config->plotTimeDomain) || config->plotAllNotes)
		ProcessWaveformsByBlock(*ReferenceSignal, *ComparisonSignal, ratioRef, config);

	return 1;
}

int AverageNormalize(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config)
{
	double		RefAvg = 0, CompAvg = 0, ratio = 0;

	RefAvg = FindFundamentalMagnitudeAverage(*ReferenceSignal, config);
	CompAvg = FindFundamentalMagnitudeAverage(*ComparisonSignal, config);
	if(CompAvg > RefAvg)
	{
		ratio = CompAvg/RefAvg;
		NormalizeMagnitudesByRatio(*ReferenceSignal, ratio, config);
		/* waveforms */
		ProcessWaveformsByBlock(*ReferenceSignal, *ComparisonSignal, ratio, config);
	}
	else
	{
		ratio = RefAvg/CompAvg;
		NormalizeMagnitudesByRatio(*ComparisonSignal, ratio, config);
		/* waveforms */
		ProcessWaveformsByBlock(*ComparisonSignal, *ReferenceSignal, ratio, config);
	}
	return 1;
}

// Compensate time normalization to match frequency normalization
/*
int TimeDomainNormalizeCompensate(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config)
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
	return 1;
}
*/

int ProcessNoiseFloor(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config)
{
	int		refHasFloor = 0, comHasFloor = 0;
	double	avgRef = 0, avgComp = 0;

	/* analyze silence floor if available */
	if(ReferenceSignal->hasSilenceBlock)
		FindFloor(ReferenceSignal, config);
	if(ComparisonSignal->hasSilenceBlock)
		FindFloor(ComparisonSignal, config);

	refHasFloor = (ReferenceSignal->hasSilenceBlock && ReferenceSignal->floorAmplitude != 0.0);
	comHasFloor = (ComparisonSignal->hasSilenceBlock && ComparisonSignal->floorAmplitude != 0.0);

	avgRef = FindFundamentalAmplitudeAverage(ReferenceSignal, config);
	avgComp = FindFundamentalAmplitudeAverage(ComparisonSignal, config);

	if(refHasFloor && avgRef < ReferenceSignal->floorAmplitude)
	{
		config->noiseFloorTooHigh |= ReferenceSignal->role;
		logmsg(" - Reference noise floor %g dBFS is louder than the average %g dBFS of the signal, ignoring\n",
				ReferenceSignal->floorAmplitude, avgRef);
		ReferenceSignal->floorAmplitude = SIGNIFICANT_VOLUME;
	}

	if(comHasFloor && avgComp < ComparisonSignal->floorAmplitude)
	{
		config->noiseFloorTooHigh |= ComparisonSignal->role;
		logmsg(" - Comparison noise floor %g dBFS is louder than the average %g dBFS of the signal, ignoring\n",
				ComparisonSignal->floorAmplitude, avgComp);
		ComparisonSignal->floorAmplitude = SIGNIFICANT_VOLUME;
	}

	/* Detect Signal Floor, default to Reference's*/
	if(refHasFloor)
		config->significantAmplitude = ReferenceSignal->floorAmplitude;

	if(refHasFloor && ReferenceSignal->floorAmplitude > LOWEST_NOISEFLOOR_ALLOWED)
		config->noiseFloorTooHigh |= ReferenceSignal->role;

	if(comHasFloor && ComparisonSignal->floorAmplitude > LOWEST_NOISEFLOOR_ALLOWED)
		config->noiseFloorTooHigh |= ComparisonSignal->role;


	// If comparison is lower than the default and higher than reference, use that
	if(config->noiseFloorAutoAdjust)
	{ 
		if(refHasFloor && comHasFloor &&
			config->significantAmplitude < SIGNIFICANT_VOLUME && 
			ComparisonSignal->floorAmplitude <= LOWEST_NOISEFLOOR_ALLOWED &&
			ReferenceSignal->floorAmplitude < ComparisonSignal->floorAmplitude &&
			ComparisonSignal->floorAmplitude < SIGNIFICANT_VOLUME)
		{
			double diff = 0;
	
			config->significantAmplitude = ComparisonSignal->floorAmplitude;
	
			diff = fabs(ReferenceSignal->floorAmplitude - ComparisonSignal->floorAmplitude);
			if(diff > 20)
				config->noiseFloorBigDifference = 1;
		}
	}
	else
	{
		if(config->significantAmplitude < SIGNIFICANT_VOLUME)
		{
			logmsg(" - Limiting noise floor to %g from %g (from -p 0)\n", 
				SIGNIFICANT_VOLUME, config->significantAmplitude);
			config->significantAmplitude = SIGNIFICANT_VOLUME;
		}
	}

	if(config->significantAmplitude >= LOWEST_NOISEFLOOR_ALLOWED)
	{
		logmsg(" - WARNING: Noise floor %g dBFS is louder than the default %g dBFS\n\tIf differences are not visible, define a limit with -p <dbfs>\n",
				config->significantAmplitude, LOWEST_NOISEFLOOR_ALLOWED);
		//if(!config->noiseFloorTooHigh)
		//	config->noiseFloorTooHigh = ROLE_COMP;
		// we rather not take action for now
		//config->significantAmplitude = SIGNIFICANT_VOLUME;
	}

	logmsg(" - Using %g dBFS as minimum significant amplitude for analysis\n",
		config->significantAmplitude);
	return 1;
}

int LoadAndProcessAudioFiles(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config)
{
	if(!LoadAudioFiles(ReferenceSignal, ComparisonSignal, config))
		return 0;

	SelectSilenceProfile(config);

	config->referenceFramerate = (*ReferenceSignal)->framerate;
	CompareFrameRates(*ReferenceSignal, *ComparisonSignal, config);

	/* Balance check */
	if((*ReferenceSignal)->AudioChannels == 2 || (*ComparisonSignal)->AudioChannels == 2)
	{
		int block = NO_INDEX;

		block = GetFirstMonoIndex(config);
		if(block != NO_INDEX)
		{
			logmsg("\n* Comparing Stereo channel amplitude\n");
			if(config->verbose) {
				logmsg(" - Mono block used for balance: %s# %d\n", 
					GetBlockName(config, block), GetBlockSubIndex(config, block));
			}
			CheckBalance(*ReferenceSignal, block, config);
			CheckBalance(*ComparisonSignal, block, config);
		}
		else
			logmsg(" - No mono block for stereo balance check\n");
	}

	if(config->normType == max_time)
	{
		if(!TimeDomainNormalize(ReferenceSignal, ComparisonSignal, config))
			return 0;
	}

	logmsg("\n* Executing Discrete Fast Fourier Transforms on 'Reference' file\n");
	if(!ProcessSignal(*ReferenceSignal, config))
		return 0;

	logmsg("* Executing Discrete Fast Fourier Transforms on 'Comparison' file\n");
	if(!ProcessSignal(*ComparisonSignal, config))
		return 0;

	ReleasePCM(*ReferenceSignal);
	ReleasePCM(*ComparisonSignal);

	CalcuateFrequencyBrackets(*ReferenceSignal, config);
	CalcuateFrequencyBrackets(*ComparisonSignal, config);

	if(!NormalizeAndFinishProcess(ReferenceSignal, ComparisonSignal, config))
		return 0;

	// Display Absolute and independent Noise Floor
	/*
	if(!config->ignoreFloor)
	{
		FindStandAloneFloor(*ReferenceSignal, config);
		FindStandAloneFloor(*ComparisonSignal, config);
	}
	*/

	if(!config->ignoreFloor)
	{
		if(!ProcessNoiseFloor(*ReferenceSignal, *ComparisonSignal, config))
			return 0;
	}
	else
		logmsg(" - Ignoring Noise floor, using %gdBFS\n", config->significantAmplitude);
	return 1;
}

int NormalizeAndFinishProcess(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config)
{
	double ZeroDbMagnitudeRef = 0;

	if(config->normType == max_frequency)
	{
		if(!FrequencyDomainNormalize(ReferenceSignal, ComparisonSignal, config))
			return 0;
	}

	if(config->normType == average)
	{
		if(!AverageNormalize(ReferenceSignal, ComparisonSignal, config))
			return 0;
	}

	/*
	// Compensate time normalization to match frequency normalization
	if(config->normType == max_time)
	{
		if(!TimeDomainNormalizeCompensate(ReferenceSignal, ComparisonSignal, config))
			return 0;
	}
	*/

	logmsg("\n* Processing Signal Frequencies and Amplitudes\n");
	if((*ReferenceSignal)->MaxMagnitude.magnitude < (*ComparisonSignal)->MaxMagnitude.magnitude)
	{
		ZeroDbMagnitudeRef = (*ComparisonSignal)->MaxMagnitude.magnitude;
		if(config->verbose) {
			logmsg(" - Comparison file has the highest peak at %g vs %g\n",
				ZeroDbMagnitudeRef, (*ReferenceSignal)->MaxMagnitude.magnitude);
		}
	}
	else
	{
		ZeroDbMagnitudeRef = (*ReferenceSignal)->MaxMagnitude.magnitude;
		if(config->verbose) {
			logmsg(" - Reference file has the highest peak at %g vs %g\n",
				ZeroDbMagnitudeRef, (*ComparisonSignal)->MaxMagnitude.magnitude);
		}
	}

	CalculateAmplitudes(*ReferenceSignal, ZeroDbMagnitudeRef, config);
	CalculateAmplitudes(*ComparisonSignal, ZeroDbMagnitudeRef, config);

	if(config->verbose)
	{
		PrintFrequencies(*ReferenceSignal, config);
		PrintFrequencies(*ComparisonSignal, config);
	}

	if(config->types.useWatermark)
	{
		if(!DetectWatermark(*ReferenceSignal, config))
		{
			logmsg("ERROR: Reference signal could not be properly evaluated\n");
			return 0;
		}
		if(!DetectWatermark(*ComparisonSignal, config))
		{
			logmsg("ERROR: Comparison signal could not be properly evaluated\n");
			return 0;
		}
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

	// look for fmt chunk
	found = 0;
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
			fseek(file, -1*(long int)sizeof(sub_chunk), SEEK_CUR);
			found = 1;
		}
	}while(!found);

	if(fread(&Signal->header.fmt, 1, sizeof(fmt_hdr), file) != sizeof(fmt_hdr))
	{
		logmsg("\tERROR: Invalid Audio file. File too small.\n");
		return(0);
	}

	if(Signal->header.fmt.Subchunk1Size + 8 > sizeof(fmt_hdr))  // Add the fmt and chunksize length: 8 bytes
		fseek(file, Signal->header.fmt.Subchunk1Size + 8 - sizeof(fmt_hdr), SEEK_CUR);

	// look for data chunk
	found = 0;
	do
	{
		sub_chunk	schunk;

		if(fread(&schunk, 1, sizeof(sub_chunk), file) != sizeof(sub_chunk))
		{
			logmsg("\tERROR: Invalid Audio file. File too small.\n");
			return(0);
		}
		if(strncmp((char*)schunk.chunkID, "data", 4) != 0)
			fseek(file, schunk.Size*sizeof(uint8_t), SEEK_CUR);
		else
		{
			fseek(file, -1*(long int)sizeof(sub_chunk), SEEK_CUR);
			found = 1;
		}
	}while(!found);

	if(fread(&Signal->header.data, 1, sizeof(data_hdr), file) != sizeof(data_hdr))
	{
		logmsg("\tERROR: Invalid Audio file. File too small.\n");
		return(0);
	}

	if(Signal->header.fmt.AudioFormat != WAVE_FORMAT_PCM) /* Check for PCM */
	{
		logmsg("\tERROR: Invalid Audio File. Only 16 bit PCM supported.\n\tPlease convert file to 16 bit PCM.");
		return(0);
	}

	if(Signal->header.fmt.NumOfChan == 2 || Signal->header.fmt.NumOfChan == 1) /* Check for Stereo and Mono */
		Signal->AudioChannels = Signal->header.fmt.NumOfChan;

	if(Signal->AudioChannels == INVALID_CHANNELS)
	{
		logmsg("\tERROR: Invalid Audio file. Only Mono and Stereo files are supported.\n");
		return(0);
	}

	if(Signal->header.fmt.bitsPerSample != 16) /* Check bit depth */
	{
		logmsg("\tERROR: Invalid Audio file. Only 16 bit supported for now.\n\tPlease use 16 bit.");
		return(0);
	}
	
	if(Signal->header.fmt.SamplesPerSec/2 < config->endHz)
	{
		logmsg(" - %d Hz sample rate was too low for %gHz-%gHz analysis\n",
			 Signal->header.fmt.SamplesPerSec, config->startHz, config->endHz);

		Signal->endHz = Signal->header.fmt.SamplesPerSec/2;
		Signal->nyquistLimit = 1;

		logmsg(" - Changed to %gHz-%gHz for this file\n", config->startHz, Signal->endHz);
	}

	if(Signal->header.data.DataSize <= 0)
	{
		logmsg("\tERROR: RIFf header has an invalid Data length %ld\n", Signal->header.data.DataSize);
		return(0);
	}

	// Default if none is found
	Signal->framerate = GetMSPerFrame(Signal, config);

	seconds = (double)Signal->header.data.DataSize/2.0/(double)Signal->header.fmt.SamplesPerSec/Signal->AudioChannels;
	logmsg(" - Audio file is %dHz %dbits %s and %g seconds long\n", 
		Signal->header.fmt.SamplesPerSec, 
		Signal->header.fmt.bitsPerSample, 
		Signal->AudioChannels == 2 ? "Stereo" : "Mono", 
		seconds);

	if(seconds < GetSignalTotalDuration(Signal->framerate, config))
	{
		logmsg(" - WARNING: Estimated file length is shorter than the expected %g seconds\n",
				GetSignalTotalDuration(Signal->framerate, config));
		config->smallFile |= Signal->role;
	}

	Signal->Samples = (char*)malloc(sizeof(char)*Signal->header.data.DataSize);
	if(!Signal->Samples)
	{
		logmsg("\tERROR: All Chunks malloc failed!\n");
		return(0);
	}

	Signal->SamplesStart = ftell(file);
	if(fread(Signal->Samples, 1, sizeof(char)*Signal->header.data.DataSize, file) !=
			 sizeof(char)*Signal->header.data.DataSize)
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

	if(GetFirstSyncIndex(config) != NO_INDEX && !config->noSyncProfile)
	{
		if(config->clock)
			clock_gettime(CLOCK_MONOTONIC, &start);

		/* Find the start offset */
		if(config->verbose) { 
			logmsg(" - Sync pulse train: "); 
		}
		Signal->startOffset = DetectPulse(Signal->Samples, Signal->header, Signal->role, config);
		if(Signal->startOffset == -1)
		{
			int format = 0;

			logmsg("\nERROR: Starting pulse train was not detected.\nProfile used: [%s]\n", config->types.Name);

			if(Signal->role == ROLE_REF)
				format = config->videoFormatRef;
			else
				format = config->videoFormatCom;
			if(!config->syncTolerance)
				logmsg(" - You can try using -T for a frequency tolerant pulse detection algorithm\n");
			if(format != 0 || config->smallFile)
				logmsg(" - This signal is configured as '%s'%s, check if that is not the issue.\n", 
							config->types.SyncFormat[format].syncName, config->smallFile ? " and is smaller than expected" : "");
			return 0;
		}
		if(config->verbose) {
			logmsg(" %gs [%ld samples|%ld bytes|%ld bytes/head]", 
				BytesToSeconds(Signal->header.fmt.SamplesPerSec, Signal->startOffset, Signal->AudioChannels),
				Signal->startOffset/2/Signal->AudioChannels, Signal->startOffset, Signal->startOffset + Signal->SamplesStart);
		}

		if(GetLastSyncIndex(config) != NO_INDEX)
		{
			double diff = 0, expected = 0;

			if(config->verbose) { 
				logmsg(" to");
			}
			Signal->endOffset = DetectEndPulse(Signal->Samples, Signal->startOffset, Signal->header, Signal->role, config);
			if(Signal->endOffset == -1)
			{
				int format = 0;

				logmsg("\nERROR: Ending pulse train was not detected.\nProfile used: [%s]\n", config->types.Name);				
				if(Signal->role == ROLE_REF)
					format = config->videoFormatRef;
				else
					format = config->videoFormatCom;
				if(!config->syncTolerance)
					logmsg(" - You can try using -T for a frequency tolerant pulse detection algorithm\n");
				if(format != 0 || config->smallFile)
					logmsg(" - This signal is configured as '%s'%s, check if that is not the issue.\n", 
							config->types.SyncFormat[format].syncName, config->smallFile ? " and is smaller than expected" : "");
				return 0;
			}
			if(config->verbose) {
				logmsg(" %gs [%ld samples|%ld bytes|%ld bytes/head]\n", 
					BytesToSeconds(Signal->header.fmt.SamplesPerSec, Signal->endOffset, Signal->AudioChannels),
					Signal->endOffset/2/Signal->AudioChannels, Signal->endOffset, Signal->endOffset + Signal->SamplesStart);
			}
			Signal->framerate = CalculateFrameRateAndCheckSamplerate(Signal, config);
			if(!Signal->framerate)
			{
				logmsg("\nERROR: Framerate could not be detected.\n");
				return 0;
			}

			logmsg(" - Detected %g Hz video signal (%gms per frame) from Audio file\n", 
						roundFloat(CalculateScanRate(Signal)), Signal->framerate);

			expected = GetMSPerFrame(Signal, config);
			diff = fabs(100.0 - Signal->framerate*100.0/expected);
			if(diff > 1.0)
			{
				logmsg("\nERROR: Framerate is %g%% different from the expected %gms.\n",
						diff, expected);
				logmsg("\tThis might be due a mismatched profile.\n");
				if(!config->ignoreFrameRateDiff)
				{
					logmsg("\tIf you want to ignore this and compare the files, use -I.\n");
					return 0;
				}
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

	if(config->noSyncProfile)
	{
		switch(config->noSyncProfileType)
		{
			case NO_SYNC_AUTO:
			{
				/* Find the start offset */
				
				logmsg(" - Detecting audio signal: ");
				Signal->startOffset = DetectSignalStart(Signal->Samples, Signal->header, 0, 0, NULL, config);
				if(Signal->startOffset == -1)
				{
					logmsg("\nERROR: Starting position was not detected.\n");
					return 0;
				}
				logmsg(" %gs [%ld samples|%ld bytes|%ld bytes/head]\n", 
						BytesToSeconds(Signal->header.fmt.SamplesPerSec, Signal->startOffset, Signal->AudioChannels),
						Signal->startOffset/2/Signal->AudioChannels, Signal->startOffset, Signal->startOffset + Signal->SamplesStart);
				Signal->endOffset = SecondsToBytes(Signal->header.fmt.SamplesPerSec, 
										GetSignalTotalDuration(Signal->framerate, config), 
										Signal->AudioChannels, 
										NULL, NULL, NULL);
			}
			break;
			case NO_SYNC_MANUAL:
			{
				double diff = 0, expected = 0;
		
				logmsg(" - WARNING: Files must be identically trimmed for this to work at some level\n");
				Signal->startOffset = 0;
				Signal->endOffset = Signal->header.data.DataSize;
		
				expected = Signal->framerate;
		
				if(Signal->role == ROLE_REF)
				{
					double seconds = 0;
					seconds = BytesToSeconds(Signal->header.fmt.SamplesPerSec, Signal->endOffset, Signal->AudioChannels);
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
			}
			break;
			default:
			{
				logmsg("\nERROR: Invalid profile\n");
				return 0;
			}
		}
	}

	if(seconds < GetSignalTotalDuration(Signal->framerate, config))
		logmsg(" - File length is smaller than the expected %gs\n",
				GetSignalTotalDuration(Signal->framerate, config));

	if(GetFirstSilenceIndex(config) != NO_INDEX)
		Signal->hasSilenceBlock = 1;

	sprintf(Signal->SourceFile, "%s", fileName);
	CalculateTimeDurations(Signal, config);

	return 1;
}

int MoveSampleBlockInternal(AudioSignal *Signal, long int element, long int pos, long int signalStartOffset, parameters *config)
{
	char		*sampleBuffer = NULL;
	double		signalLengthSeconds = 0;
	long int	signalLengthFrames = 0, signalLengthBytes = 0;

	signalLengthFrames = GetInternalSyncTotalLength(element, config);
	if(!signalLengthFrames)
	{
		logmsg("\tERROR: Internal Sync block has no frame duration. Aborting.\n");
		return 0;
	}

	signalLengthSeconds = FramesToSeconds(signalLengthFrames, config->referenceFramerate);
	signalLengthBytes = SecondsToBytes(Signal->header.fmt.SamplesPerSec, signalLengthSeconds, Signal->AudioChannels, NULL, NULL, NULL);

	if(pos + signalStartOffset + signalLengthBytes > Signal->header.data.DataSize)
	{
		signalLengthBytes = Signal->header.data.DataSize - (pos+signalStartOffset);
		if(config->verbose) {
			logmsg(" - Internal sync adjust: Signal is smaller than expected\n");
		}
	}

	if(config->verbose) {
		logmsg(" - Internal Segment Info:\n\tFinal Offset: %ld Frames: %d Seconds: %g Bytes: %ld\n",
				pos+signalStartOffset, signalLengthFrames, signalLengthSeconds, signalLengthBytes);
	}

	sampleBuffer = (char*)malloc(sizeof(char)*signalLengthBytes);
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
				pos + signalStartOffset, signalLengthBytes);
		logmsg("\tZero Out: Pos: %ld Bytes: %ld\n",
				pos+signalStartOffset, signalLengthBytes);
		logmsg("\tStore: Pos: %ld Bytes: %ld\n",
				pos, signalLengthBytes);
	}
	*/
	memcpy(sampleBuffer, Signal->Samples + pos + signalStartOffset, signalLengthBytes);
	memset(Signal->Samples + pos + signalStartOffset, 0, signalLengthBytes);
	memcpy(Signal->Samples + pos, sampleBuffer, signalLengthBytes);

	free(sampleBuffer);
	return 1;
}

int MoveSampleBlockExternal(AudioSignal *Signal, long int element, long int pos, long int signalStartOffset, long int internalSyncToneSize, parameters *config)
{
	char		*sampleBuffer = NULL;
	double		signalLengthSeconds = 0;
	long int	signalLengthFrames = 0, signalLengthBytes = 0;

	signalLengthFrames = GetInternalSyncTotalLength(element, config);
	if(!signalLengthFrames)
	{
		logmsg("\tERROR: Internal Sync block has no frame duration. Aborting.\n");
		return 0;
	}

	signalLengthSeconds = FramesToSeconds(signalLengthFrames, config->referenceFramerate);
	signalLengthBytes = SecondsToBytes(Signal->header.fmt.SamplesPerSec, signalLengthSeconds, Signal->AudioChannels, NULL, NULL, NULL);

	if(pos + signalStartOffset + signalLengthBytes > Signal->header.data.DataSize)
	{
		signalLengthBytes = Signal->header.data.DataSize - (pos+signalStartOffset);
		if(config->verbose) {
			logmsg(" - Internal sync adjust: Signal is smaller than expected\n");
		}
	}
	if(config->verbose) {
		logmsg(" - Internal Segment Info:\n\tFinal Offset: %ld Frames: %d Seconds: %g Bytes: %ld\n",
				pos+signalStartOffset, signalLengthFrames, signalLengthSeconds, signalLengthBytes);
	}

	sampleBuffer = (char*)malloc(sizeof(char)*signalLengthBytes);
	if(!sampleBuffer)
	{
		logmsg("\tERROR: Out of memory while performing internal Sync adjustments.\n");
		return 0;
	}

	/*
	if(config->verbose)
	{
		logmsg(" - MOVEMENTS:\n");
		logmsg("\tCopy: From %ld Bytes: %ld\n",
				pos + signalStartOffset, signalLengthBytes);
		logmsg("\tZero Out: Pos: %ld Bytes: %ld\n",
				pos + pos, Signal->header.data.DataSize-pos);
		logmsg("\tStore: Pos: %ld Bytes: %ld\n",
				pos, signalLengthBytes);
	}
	*/

	memcpy(sampleBuffer, Signal->Samples + pos + signalStartOffset, signalLengthBytes);
	memset(Signal->Samples + pos, 0, Signal->header.data.DataSize-pos);
	memcpy(Signal->Samples + pos, sampleBuffer, signalLengthBytes);

	free(sampleBuffer);
	return 1;
}

int ProcessInternal(AudioSignal *Signal, long int element, long int pos, int *syncinternal, long int *advanceBytes, int knownLength, parameters *config)
{
	if(*syncinternal)
		*syncinternal = 0;
	else
	{
		int			syncToneFreq = 0;
		double		syncLenSeconds = 0;
		long int	internalSyncOffset = 0, syncLengthBytes = 0,
					endPulseBytes = 0, pulseLengthBytes = 0, signalStart = 0;

		syncToneFreq = GetInternalSyncTone(element, config);
		syncLenSeconds = GetInternalSyncLen(element, config);
		internalSyncOffset = DetectSignalStart(Signal->Samples, Signal->header, pos, syncToneFreq, &endPulseBytes, config);
		if(internalSyncOffset == -1)
		{
			logmsg("\tWARNING: No signal found while in internal sync detection.\n");
			return -1;
		}
		*syncinternal = 1;

		pulseLengthBytes = endPulseBytes - internalSyncOffset;
		syncLengthBytes = SecondsToBytes(Signal->header.fmt.SamplesPerSec, syncLenSeconds, Signal->AudioChannels, NULL, NULL, NULL);
		internalSyncOffset -= pos;
		signalStart = internalSyncOffset;

		if(pulseLengthBytes < syncLengthBytes/20)
		{
			logmsg("\tWARNING: No real signal found while in internal sync detection.\n");
			return 0;
		}

		if(knownLength == TYPE_INTERNAL_KNOWN)
		{
			Signal->delayArray[Signal->delayElemCount++] = BytesToSeconds(Signal->header.fmt.SamplesPerSec, internalSyncOffset, Signal->AudioChannels)*1000.0;
			logmsg(" - %s command delay: %g ms [%g frames]\n",
				GetBlockName(config, element),
				BytesToSeconds(Signal->header.fmt.SamplesPerSec, internalSyncOffset, Signal->AudioChannels)*1000.0,
				BytesToFrames(Signal->header.fmt.SamplesPerSec, internalSyncOffset, config->referenceFramerate, Signal->AudioChannels));

			if(config->verbose) {
					logmsg("  > Found at: %ld Previous: %ld Offset: %ld\n\tPulse Length: %ld Silence Length: %ld\n", 
						pos + internalSyncOffset, pos, internalSyncOffset, pulseLengthBytes, syncLengthBytes/2);
			}

#ifndef MDWAVE
			/* Copy waveforms for visual inspection, create 3 slots: silence, sync pulse, silence */
			if(config->plotAllNotes)
			{
				if(!initInternalSync(&Signal->Blocks[element], 3))
					return 0;
			
				if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
						(int16_t*)(Signal->Samples + pos), 
						internalSyncOffset/2, 0, 
						Signal->header.fmt.SamplesPerSec, NULL, Signal->AudioChannels, config))
					return 0;
	
				if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
						(int16_t*)(Signal->Samples + pos + internalSyncOffset), 
						pulseLengthBytes/2, 1, 
						Signal->header.fmt.SamplesPerSec, NULL, Signal->AudioChannels, config))
					return 0;
	
				if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
						(int16_t*)(Signal->Samples + pos + internalSyncOffset + pulseLengthBytes), 
						syncLengthBytes/4, 2, 
						Signal->header.fmt.SamplesPerSec, NULL, Signal->AudioChannels, config))
					return 0;
			}
#endif
			// skip sync tone-which is silence-taken from config file
			internalSyncOffset += syncLengthBytes;

			if(!MoveSampleBlockInternal(Signal, element, pos, internalSyncOffset, config))
				return 0;
		}
		if(knownLength == TYPE_INTERNAL_UNKNOWN)  // Our sync is outside the frame detection zone
		{
			long int 	silenceLengthBytes = 0;
#ifndef MDWAVE
			/* long int 	oneframe = 0; */
#endif
			silenceLengthBytes = syncLengthBytes/2;

			if(pulseLengthBytes != silenceLengthBytes)
			{
				if(pulseLengthBytes > silenceLengthBytes)
				{
					long int compensate = 0;

					// Length must be fixed, so we detected start sooner
					compensate = pulseLengthBytes - silenceLengthBytes;

					signalStart += compensate;
					internalSyncOffset += compensate;

					pulseLengthBytes = silenceLengthBytes;
				}
				else
				{
					logmsg(" - WARNING: Internal Sync too short. Got %ld] expected %ld\n", 
						pulseLengthBytes, silenceLengthBytes);
				}
				//silenceLengthBytes = syncLengthBytes - pulseLengthBytes;
			}

			Signal->delayArray[Signal->delayElemCount++] = BytesToSeconds(Signal->header.fmt.SamplesPerSec, internalSyncOffset, Signal->AudioChannels)*1000.0;
			logmsg(" - %s command delay: %g ms [%g frames]\n",
				GetBlockName(config, element),
				BytesToSeconds(Signal->header.fmt.SamplesPerSec, internalSyncOffset, Signal->AudioChannels)*1000.0,
				BytesToFrames(Signal->header.fmt.SamplesPerSec, internalSyncOffset, config->referenceFramerate, Signal->AudioChannels));

			if(config->verbose) {
				logmsg("  > Found at: %ld Previous: %ld\n\tPulse Length: %ld Silence Length: %ld\n", 
					pos + internalSyncOffset, pos, pulseLengthBytes, silenceLengthBytes);
			}
			
			// skip the pulse real duration to sync perfectly
			signalStart += pulseLengthBytes;
			// skip half the sync tone-which is silence-taken from config file
			signalStart += silenceLengthBytes;

#ifndef MDWAVE
			/* Copy waveforms for visual inspection, create 3 slots: silence, sync pulse, silence */
			if(config->plotAllNotes)
			{
				if(!initInternalSync(&Signal->Blocks[element], 3))
					return 0;
			
				if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
						(int16_t*)(Signal->Samples + pos), 
						internalSyncOffset/2, 0, 
						Signal->header.fmt.SamplesPerSec, NULL, Signal->AudioChannels, config))
					return 0;
	
				if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
						(int16_t*)(Signal->Samples + pos + internalSyncOffset), 
						pulseLengthBytes/2, 1, 
						Signal->header.fmt.SamplesPerSec, NULL, Signal->AudioChannels, config))
					return 0;
	
				if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
						(int16_t*)(Signal->Samples + pos + internalSyncOffset + pulseLengthBytes), 
						silenceLengthBytes	/2, 2, 
						Signal->header.fmt.SamplesPerSec, NULL, Signal->AudioChannels, config))
					return 0;

				/*
				oneframe = SecondsToBytes(Signal->header.fmt.SamplesPerSec, FramesToSeconds(1, config->referenceFramerate), Signal->AudioChannels, NULL, NULL, NULL);
				if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
						(int16_t*)(Signal->Samples + pos + signalStart), 
						(oneframe*2)/2, 3, 
						Signal->header.fmt.SamplesPerSec, NULL, Signal->AudioChannels, config))
					return 0;
				*/
			}
#endif
			/* Do the real processing */
			if(!MoveSampleBlockExternal(Signal, element, pos, signalStart, pulseLengthBytes + silenceLengthBytes, config))
				return 0;
		}
		if(advanceBytes)
			*advanceBytes += internalSyncOffset;
	}
	return 1;
}

int CopySamplesForTimeDomainPlotWindowOnly(AudioBlocks *AudioArray, long samplerate, double *window, parameters *config)
{
	long			i = 0, monoSignalSize = 0, difference = 0;
	int16_t			*signal = NULL, *window_samples = NULL;
	
	if(!AudioArray)
	{
		logmsg("No Array for results\n");
		return 0;
	}

	if(!config->plotAllNotesWindowed || !window || !config->doClkAdjust)
	{
		logmsg("Unplanned function call\n");
		return 0;
	}

	if(AudioArray->audio.window_samples)
	{
		logmsg("ERROR: Window waveforms already stored\n");
		return 0;
	}

	if(!AudioArray->audio.samples)
	{
		logmsg("ERROR: Waveforms not stored\n");
		return 0;
	}

	signal = AudioArray->audio.samples;
	monoSignalSize = AudioArray->audio.size;
	difference = AudioArray->audio.difference;

	window_samples = (int16_t*)malloc(sizeof(int16_t)*(monoSignalSize+1));
	if(!window_samples)
	{
		logmsg("Not enough memory for window\n");
		return(0);
	}
	memset(window_samples, 0, sizeof(int16_t)*(monoSignalSize+1));

	AudioArray->audio.size = monoSignalSize;
	AudioArray->audio.difference = difference;

	for(i = 0; i < monoSignalSize - difference; i++)
		window_samples[i] = (double)((double)signal[i]*window[i]);
	AudioArray->audio.window_samples = window_samples;

	return(1);
}

int CopySamplesForTimeDomainPlot(AudioBlocks *AudioArray, int16_t *samples, size_t size, size_t diff, long samplerate, double *window, int AudioChannels, parameters *config)
{
	char			channel = 0;
	long			stereoSignalSize = 0;	
	long			i = 0, monoSignalSize = 0, diffSize = 0, difference = 0;
	int16_t			*signal = NULL, *window_samples = NULL;
	
	if(!AudioArray)
	{
		logmsg("No Array for results\n");
		return 0;
	}

	if(AudioArray->audio.samples)
	{
		logmsg("ERROR: Waveforms already stored\n");
		return 0;
	}

	stereoSignalSize = (long)size;
	monoSignalSize = stereoSignalSize/AudioChannels;	 /* 4 is 2 16 bit values */
	diffSize = (long)diff;
	difference = diffSize/AudioChannels;	 /* 4 is 2 16 bit values */

	signal = (int16_t*)malloc(sizeof(int16_t)*(monoSignalSize+1));
	if(!signal)
	{
		logmsg("Not enough memory\n");
		return(0);
	}
	memset(signal, 0, sizeof(int16_t)*(monoSignalSize+1));

	if(config->plotAllNotesWindowed && window && !config->doClkAdjust)
	{
		window_samples = (int16_t*)malloc(sizeof(int16_t)*(monoSignalSize+1));
		if(!window_samples)
		{
			logmsg("Not enough memory\n");
			return(0);
		}
		memset(window_samples, 0, sizeof(int16_t)*(monoSignalSize+1));
	}

	if(AudioChannels == 1)
		channel = CHANNEL_LEFT;
	else
		channel = CHANNEL_STEREO;

	for(i = 0; i < monoSignalSize; i++)
	{
		if(channel == CHANNEL_LEFT)
			signal[i] = (double)samples[i*AudioChannels];
		if(channel == CHANNEL_RIGHT)
			signal[i] = (double)samples[i*2+1];
		if(channel == CHANNEL_STEREO)
			signal[i] = (double)((double)samples[i*2]+(double)samples[i*2+1])/2.0;
	}

	AudioArray->audio.samples = signal;
	AudioArray->audio.size = monoSignalSize;
	AudioArray->audio.difference = difference;

	if(config->plotAllNotesWindowed && window && !config->doClkAdjust)
	{
		for(i = 0; i < monoSignalSize - difference; i++)
			window_samples[i] = (double)((double)signal[i]*window[i]);
		AudioArray->audio.window_samples = window_samples;
	}

	return(1);
}

int CopySamplesForTimeDomainPlotInternalSync(AudioBlocks *AudioArray, int16_t *samples, size_t size, int slotForSamples, long samplerate, double *window, int AudioChannels, parameters *config)
{
	char			channel = 0;
	long			stereoSignalSize = 0;	
	long			i = 0, monoSignalSize = 0;
	int16_t			*signal = NULL, *window_samples = NULL;
	
	if(!AudioArray)
	{
		logmsg("No Array for results\n");
		return 0;
	}

	if(!AudioArray->internalSync)
	{
		logmsg("ERROR: Internal Sync not allocated\n");
		return 0;
	}

	if(AudioArray->internalSync[slotForSamples].samples)
	{
		logmsg("ERROR: Waveforms already stored\n");
		return 0;
	}

	if(slotForSamples > AudioArray->internalSyncCount - 1)
	{
		logmsg("ERROR: Insufficient slots\n");
		return 0;
	}

	stereoSignalSize = (long)size;
	monoSignalSize = stereoSignalSize/AudioChannels;	 /* 4 is 2 16 bit values */

	signal = (int16_t*)malloc(sizeof(int16_t)*(monoSignalSize+1));
	if(!signal)
	{
		logmsg("Not enough memory\n");
		return(0);
	}
	memset(signal, 0, sizeof(int16_t)*(monoSignalSize+1));

	if(config->plotAllNotesWindowed && window)
	{
		window_samples = (int16_t*)malloc(sizeof(int16_t)*(monoSignalSize+1));
		if(!window_samples)
		{
			logmsg("Not enough memory\n");
			return(0);
		}
		memset(window_samples, 0, sizeof(int16_t)*(monoSignalSize+1));
	}

	if(AudioChannels == 1)
		channel = CHANNEL_LEFT;
	else
		channel = CHANNEL_STEREO;

	for(i = 0; i < monoSignalSize; i++)
	{
		if(channel == CHANNEL_LEFT)
			signal[i] = (double)samples[i*AudioChannels];
		if(channel == CHANNEL_RIGHT)
			signal[i] = (double)samples[i*2+1];
		if(channel == CHANNEL_STEREO)
			signal[i] = (double)((double)samples[i*2]+(double)samples[i*2+1])/2.0;
	}

	AudioArray->internalSync[slotForSamples].samples = signal;
	AudioArray->internalSync[slotForSamples].size = monoSignalSize;
	AudioArray->internalSync[slotForSamples].difference = 0;

	if(config->plotAllNotesWindowed && window)
	{
		for(i = 0; i < monoSignalSize; i++)
			window_samples[i] = (double)((double)signal[i]*window[i]);
		AudioArray->audio.window_samples = window_samples;
	}

	return(1);
}

int RecalculateFFTW(AudioSignal *Signal, parameters *config)
{
	long int		i = 0;
	double			*windowUsed = NULL;
	windowManager	windows;

	if(!config->doClkAdjust)
		return 0;

	if(!initWindows(&windows, Signal->header.fmt.SamplesPerSec, config->window, config))
		return 0;

	while(i < config->types.totalBlocks)
	{
		if(Signal->Blocks[i].type > TYPE_SILENCE)
		{
			long int frames = 0, cutFrames = 0;

			frames = GetBlockFrames(config, i);
			cutFrames = GetBlockCutFrames(config, i);
	
			windowUsed = getWindowByLength(&windows, frames, cutFrames, config->smallerFramerate, config);

			CleanFrequenciesInBlock(&Signal->Blocks[i], config);
			if(!ExecuteDFFT(&Signal->Blocks[i], Signal->Blocks[i].audio.samples, Signal->Blocks[i].audio.size-Signal->Blocks[i].audio.difference, Signal->header.fmt.SamplesPerSec, windowUsed, Signal->AudioChannels, config->ZeroPad, config))
				return 0;
			if(!FillFrequencyStructures(Signal, &Signal->Blocks[i], config))
				return 0;

			if(config->plotAllNotesWindowed && !CopySamplesForTimeDomainPlotWindowOnly(&Signal->Blocks[i], Signal->header.fmt.SamplesPerSec, windowUsed, config))
				return 0;

			if(config->clkMeasure && config->clkBlock == i)
			{
				CleanFrequenciesInBlock(&Signal->clkFrequencies, config);
				if(!ExecuteDFFT(&Signal->clkFrequencies, Signal->Blocks[i].audio.samples, Signal->Blocks[i].audio.size-Signal->Blocks[i].audio.difference, Signal->header.fmt.SamplesPerSec, windowUsed, Signal->AudioChannels, 1 /* zeropad on */, config))
					return 0;
	
				if(!FillFrequencyStructures(Signal, &Signal->clkFrequencies, config))
					return 0;
			}
		}
		i++;
	}

	freeWindows(&windows);

	if(config->normType != max_frequency)
		FindMaxMagnitude(Signal, config);

	return 1;
}

/*
// This rebased them both to the expected ideal CLK, noisier
void RecalculateFrameRateAndSamplerate(AudioSignal *Signal, parameters *config)
{
	double	ratio = 0;
	int		estimatedSampleRate = 0;

	ratio = ((double)(config->clkFreq*config->clkRatio))/CalculateClk(Signal, config);
	estimatedSampleRate = ceil((double)Signal->header.fmt.SamplesPerSec*ratio);
	Signal->EstimatedSR_CLK = estimatedSampleRate;

	Signal->originalSR_CLK = Signal->header.fmt.SamplesPerSec;
	Signal->header.fmt.SamplesPerSec = estimatedSampleRate;
	Signal->framerate = CalculateFrameRate(Signal, config);
	logmsg("New frame rate: %g SR: %d->%d (%g)\n", 
		Signal->framerate,
		Signal->originalSR_CLK, Signal->header.fmt.SamplesPerSec,
		ratio);
}
*/

// Doing comparison always was default
// It is noisier to adjust clk upwards than downwards though
// so we go for the lower one
double RecalculateFrameRateAndSamplerateComp(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config)
{
	double		ratio = 0, refCLK = 0, compCLK = 0, adjustedTo = 0;
	int			estimatedSampleRate = 0;
	AudioSignal	*changedSignal = 0;

	refCLK = CalculateClk(ReferenceSignal, config);
	compCLK = CalculateClk(ComparisonSignal, config);

	if(refCLK < compCLK)
	{
		changedSignal = ComparisonSignal;
		ratio = refCLK/compCLK;
		changedSignal->originalCLK = compCLK;
		adjustedTo = refCLK;
	}
	else
	{
		changedSignal = ReferenceSignal;
		ratio = compCLK/refCLK;
		changedSignal->originalCLK = refCLK;
		adjustedTo = compCLK;
	}

	config->changedCLKFrom = changedSignal->role;
	
	estimatedSampleRate = ceil((double)changedSignal->header.fmt.SamplesPerSec*ratio);
	changedSignal->EstimatedSR_CLK = estimatedSampleRate;

	changedSignal->originalSR_CLK = changedSignal->header.fmt.SamplesPerSec;
	changedSignal->header.fmt.SamplesPerSec = estimatedSampleRate;
	changedSignal->originalFrameRate = changedSignal->framerate;
	changedSignal->framerate = CalculateFrameRate(changedSignal, config);
	if(config->verbose) {
		logmsg(" - Adjusted frame rate to match same lengths with %s: %gms [SR: %d->%dHz]\n", 
			config->clkName,
			changedSignal->framerate,
			changedSignal->originalSR_CLK, changedSignal->header.fmt.SamplesPerSec);
	}
	return adjustedTo;
}

int RecalculateFrequencyStructures(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config)
{
	double adjusted = 0;

	//RecalculateFrameRateAndSamplerate(ReferenceSignal, config);
	//RecalculateFrameRateAndSamplerate(ComparisonSignal, config);

	adjusted = RecalculateFrameRateAndSamplerateComp(ReferenceSignal, ComparisonSignal, config);
	logmsg(" - Adjusted %s %s to %gHz\n", 
			config->changedCLKFrom == ROLE_REF? "Reference" : "Comparison", 
			config->clkName, adjusted);
	CompareFrameRates(ReferenceSignal, ComparisonSignal, config);

	logmsg(" - Recalculation Discrete Fast Fourier Transforms with adjusted %s value\n", config->clkName);
	if(!RecalculateFFTW(ReferenceSignal, config))
		return 0;

	if(!RecalculateFFTW(ComparisonSignal, config))
		return 0;

	if(!NormalizeAndFinishProcess(&ReferenceSignal, &ComparisonSignal, config))
		return 0;

	return 1;
}

int DuplicateSamplesForWavefromPlots(AudioSignal *Signal, long int element, long int pos, long int loadedBlockSize, long int difference, double framerate, double *windowUsed, parameters *config)
{
	if(config->plotTimeDomainHiDiff || config->plotAllNotes || 
			config->doClkAdjust || Signal->Blocks[element].type == TYPE_TIMEDOMAIN)
	{
		if(!CopySamplesForTimeDomainPlot(&Signal->Blocks[element], (int16_t*)(Signal->Samples + pos), loadedBlockSize/2, difference/2, Signal->header.fmt.SamplesPerSec, windowUsed, Signal->AudioChannels, config))
			return 0;
	}
	
	if(config->debugSync && Signal->Blocks[element].type == TYPE_SYNC)
	{
		long int oneFrameBytes = 0;
		
		oneFrameBytes = SecondsToBytes(Signal->header.fmt.SamplesPerSec, FramesToSeconds(framerate, 1), Signal->AudioChannels, NULL, NULL, NULL);
		if(pos > oneFrameBytes) {
			if(!CopySamplesForTimeDomainPlot(&Signal->Blocks[element], (int16_t*)(Signal->Samples + pos - oneFrameBytes), loadedBlockSize/2+oneFrameBytes/2, difference/2, Signal->header.fmt.SamplesPerSec, NULL, Signal->AudioChannels, config))
				return 0;
		}
		else {
			if(!CopySamplesForTimeDomainPlot(&Signal->Blocks[element], (int16_t*)(Signal->Samples + pos), loadedBlockSize/2, difference/2, Signal->header.fmt.SamplesPerSec, NULL, Signal->AudioChannels, config))
				return 0;
		}
	}
	return 1;
}

int ProcessSignal(AudioSignal *Signal, parameters *config)
{
	long int		pos = 0;
	double			longest = 0;
	char			*buffer;
	size_t			buffersize = 0;
	windowManager	windows;
	double			*windowUsed = NULL;
	long int		loadedBlockSize = 0, i = 0;
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

	buffersize = SecondsToBytes(Signal->header.fmt.SamplesPerSec, longest, Signal->AudioChannels, NULL, NULL, NULL);
	buffer = (char*)malloc(buffersize);
	if(!buffer)
	{
		logmsg("\tERROR: malloc failed.\n");
		return(0);
	}

	if(!initWindows(&windows, Signal->header.fmt.SamplesPerSec, config->window, config))
		return 0;

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	while(i < config->types.totalBlocks)
	{
		double duration = 0, framerate = 0;
		long int frames = 0, difference = 0, cutFrames = 0;

		if(!syncinternal)
			framerate = Signal->framerate;
		else
			framerate = config->referenceFramerate;

		frames = GetBlockFrames(config, i);
		cutFrames = GetBlockCutFrames(config, i);
		duration = FramesToSeconds(framerate, frames);
		
		loadedBlockSize = SecondsToBytes(Signal->header.fmt.SamplesPerSec, duration, Signal->AudioChannels, &leftover, &discardBytes, &leftDecimals);

		difference = GetByteSizeDifferenceByFrameRate(framerate, frames, Signal->header.fmt.SamplesPerSec, Signal->AudioChannels, config);

		// config->smallerFramerate
		windowUsed = NULL;
		if(Signal->Blocks[i].type >= TYPE_SILENCE || Signal->Blocks[i].type == TYPE_WATERMARK) // We get the smaller window, since we'll truncate
		{
			if(!syncinternal)
				windowUsed = getWindowByLength(&windows, frames, cutFrames, config->smallerFramerate, config);
			else
				windowUsed = getWindowByLength(&windows, frames, cutFrames, framerate, config);
		}

/*
		logmsg("loadedBlockSize %ld Diff %ld loadedBlockSize -diff %ld leftover %ld discardBytes %ld leftDecimals %g\n",
				loadedBlockSize, difference, loadedBlockSize - difference, leftover, discardBytes, leftDecimals);
*/
		memset(buffer, 0, buffersize);
		if(pos + loadedBlockSize > Signal->header.data.DataSize)
		{
			if(i != config->types.totalBlocks - 1)
			{
				config->smallFile |= Signal->role;
				logmsg("\tunexpected end of File, please record the full Audio Test from the 240p Test Suite.\n");
			}
			break;
		}
		memcpy(buffer, Signal->Samples + pos, loadedBlockSize-difference);

		if(!DuplicateSamplesForWavefromPlots(Signal, i, pos, loadedBlockSize, difference, framerate, windowUsed, config))
			return 0;

		if(Signal->Blocks[i].type >= TYPE_SILENCE || Signal->Blocks[i].type == TYPE_WATERMARK)
		{
			if(!ExecuteDFFT(&Signal->Blocks[i], (int16_t*)buffer, (loadedBlockSize-difference)/2, Signal->header.fmt.SamplesPerSec, windowUsed, Signal->AudioChannels, config->ZeroPad, config))
				return 0;

			//logmsg("estimated %g (difference %ld)\n", Signal->Blocks[i].frames*Signal->framerate/1000.0, difference);
			// uncomment in ExecuteDFFT as well
			if(!FillFrequencyStructures(Signal, &Signal->Blocks[i], config))
				return 0;
		}

		if(config->clkMeasure && config->clkBlock == i)
		{
			if(!ExecuteDFFT(&Signal->clkFrequencies, (int16_t*)buffer, (loadedBlockSize-difference)/2, Signal->header.fmt.SamplesPerSec, windowUsed, Signal->AudioChannels, 1, config))
				return 0;

			if(!FillFrequencyStructures(Signal, &Signal->clkFrequencies, config))
				return 0;
		}

#ifdef CHECKWAV
		// MDWAVE exists for this, but just in case it is ever needed within MDFourier
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
			if(!ProcessInternal(Signal, i, pos, &syncinternal, NULL, TYPE_INTERNAL_KNOWN, config))
				return 0;
		}

		if(Signal->Blocks[i].type == TYPE_INTERNAL_UNKNOWN)
		{
			if(!ProcessInternal(Signal, i, pos, &syncinternal, NULL, TYPE_INTERNAL_UNKNOWN, config))
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

	if(config->drawWindows)
	{
		VisualizeWindows(&windows, config);
		PlotBetaFunctions(config);
	}

	free(buffer);
	freeWindows(&windows);

	return i;
}

int ExecuteDFFT(AudioBlocks *AudioArray, int16_t *samples, size_t size, long samplerate, double *window, int AudioChannels, int ZeroPad, parameters *config)
{
	char channel = CHANNEL_STEREO;

	if(AudioChannels == 1)
		channel = CHANNEL_LEFT;
	else
	{
		// If we are procesing a mono signal in a stereo file, use both channels
		if(AudioArray->channel == CHANNEL_MONO)
			channel = CHANNEL_STEREO;

		if(AudioArray->channel == CHANNEL_STEREO)
		{
			channel = CHANNEL_RIGHT;
			if(!ExecuteDFFTInternal(AudioArray, samples, size, samplerate, window, channel, AudioChannels, ZeroPad, config))
				return 0;
			channel = CHANNEL_LEFT;
		}
	}
	return(ExecuteDFFTInternal(AudioArray, samples, size, samplerate, window, channel, AudioChannels, ZeroPad, config));
}

int ExecuteDFFTInternal(AudioBlocks *AudioArray, int16_t *samples, size_t size, long samplerate, double *window, char channel, int AudioChannels, int ZeroPad, parameters *config)
{
	fftw_plan		p = NULL;
	long			stereoSignalSize = 0;	
	long			i = 0, monoSignalSize = 0, zeropadding = 0;
	double			*signal = NULL;
	fftw_complex	*spectrum = NULL;
	double			seconds = 0;
	
	if(!AudioArray)
	{
		logmsg("No Array for results\n");
		return 0;
	}

	stereoSignalSize = (long)size;
	monoSignalSize = stereoSignalSize/AudioChannels;	 /* 4 is 2 16 bit values */
	seconds = (double)size/((double)samplerate*AudioChannels);

	if(ZeroPad)  /* disabled by default */
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
			signal[i] = (double)samples[i*AudioChannels];
		if(channel == CHANNEL_RIGHT)
			signal[i] = (double)samples[i*AudioChannels+1];
		if(channel == CHANNEL_STEREO)
			signal[i] = ((double)samples[i*AudioChannels]+(double)samples[i*AudioChannels+1])/2.0;

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

	//logmsg("Seconds %g was %g ", seconds, AudioArray->seconds); // uncomment estimated above as well
	if(channel != CHANNEL_RIGHT)
	{
		AudioArray->fftwValues.spectrum = spectrum;
		AudioArray->fftwValues.size = monoSignalSize;
	}
	else
	{
		AudioArray->fftwValuesRight.spectrum = spectrum;
		AudioArray->fftwValuesRight.size = monoSignalSize;
	}
	AudioArray->seconds = seconds;
	free(signal);
	signal = NULL;

	return(1);
}

int CalculateMaxCompare(int block, AudioSignal *Signal, parameters *config)
{
	double	limit = 0;
	int		posFound = -1;

	limit = config->significantAmplitude;

	if(Signal->role == ROLE_COMP)
		limit += -20;	// Allow going 20 dbfs "deeper"

	for(int freq = 0; freq < config->MaxFreq; freq++)
	{
		/* Out of valid frequencies */
		if(!Signal->Blocks[block].freq[freq].hertz)
		{
			posFound = freq - 1;
			break;
		}

		/* Amplitude is too low */
		if(Signal->Blocks[block].freq[freq].amplitude < limit)
		{
			posFound = freq;
			break;
		}
	}

	if(Signal->Blocks[block].freqRight)
	{
		for(int freq = 0; freq < config->MaxFreq; freq++)
		{
			/* Out of valid frequencies */
			if(!Signal->Blocks[block].freqRight[freq].hertz)
			{
				if(posFound > freq - 1)
					return freq - 1;
				break;
			}
	
			/* Amplitude is too low */
			if(Signal->Blocks[block].freqRight[freq].amplitude < limit)
			{
				if(posFound > freq)
					return freq;
				break;
			}
		}
	}

	if(posFound != -1)
		return posFound;
	return config->MaxFreq;
}

int CompareFrequencies(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, char channel, int block, int refSize, int testSize, parameters *config)
{
	Frequency	*freqRef = NULL, *freqComp = NULL;

	if(channel == CHANNEL_LEFT)
	{
		freqRef = ReferenceSignal->Blocks[block].freq;
		freqComp = ComparisonSignal->Blocks[block].freq;
	}
	else
	{
		freqRef = ReferenceSignal->Blocks[block].freqRight;
		freqComp = ComparisonSignal->Blocks[block].freqRight;
	}

	for(int freq = 0; freq < refSize; freq++)
	{
		int found = 0, index = 0;

		if(!IncrementCompared(block, config))
		{
			logmsg("Internal consistency failure, please send error log (compare)\n");
			return 0;
		}

		for(int comp = 0; comp < testSize; comp++)
		{
			if(!freqRef[freq].matched && !freqComp[comp].matched && 
				areDoublesEqual(freqRef[freq].hertz, freqComp[comp].hertz))
			{
				freqComp[comp].matched = freq + 1;
				freqRef[freq].matched = comp + 1;

				found = 1;
				index = comp;

				break;
			}
		}

  		/* Now in either case, compare amplitude and phase */
		if(found)
		{
			if(!areDoublesEqual(freqRef[freq].amplitude, freqComp[index].amplitude))
			{
				if(!InsertAmplDifference(block, freqRef[freq], freqComp[index], config))
				{
					logmsg("Internal consistency failure, please send error log (AmplDiff)\n");
					return 0;
				}
			}
			else /* perfect match */
			{
				if(!IncrementPerfectMatch(block, config))
				{
					logmsg("Internal consistency failure, please send error log (perfect)\n");
					return 0;
				}
			}

			if(!areDoublesEqual(freqRef[freq].phase, freqComp[index].phase))
			{
				if(!InsertPhaseDifference(block, freqRef[freq], freqComp[index], config))
				{
					logmsg("Internal consistency failure, please send error log (PhaseDiff)\n");
					return 0;
				}
			}
			else /* perfect match phase */
			{
			}
		}
		else /* Frequency Not Found */
		{
			if(!InsertFreqNotFound(block, freqRef[freq].hertz, freqRef[freq].amplitude, config))
			{
				logmsg("Internal consistency failure, please send error log (Not found)\n");
				return 0;
			}
		}
	}
	return 1;
}

int CompareAudioBlocks(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config)
{
	int		block = 0, warn = 0;
	struct	timespec	start, end;

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	if(!CreateDifferenceArray(config))
		return 0;

	for(block = 0; block < config->types.totalBlocks; block++)
	{
		char	channel = CHANNEL_LEFT;
		int 	refSize = 0, testSize = 0, type = 0;

		/* Ignore Control blocks */
		type = GetBlockType(config, block);
		channel = GetBlockChannel(config, block);

		/* For Time Domain Plots with big framerate difference */
		if(ReferenceSignal->Blocks[block].audio.difference != 0)
			ComparisonSignal->Blocks[block].audio.difference = -1*ReferenceSignal->Blocks[block].audio.difference;
		if(ComparisonSignal->Blocks[block].audio.difference != 0)
			ReferenceSignal->Blocks[block].audio.difference = -1*ComparisonSignal->Blocks[block].audio.difference;

		if(type < TYPE_CONTROL)
			continue;

		if(type != TYPE_SILENCE)
		{
			refSize = CalculateMaxCompare(block, ReferenceSignal, config);
			testSize = CalculateMaxCompare(block, ComparisonSignal, config);
			/*
			if(refSize == 0)
			{
				if(!warn)
					logmsg("\n");
				logmsg("WARNING: Reference %s# %ld (%ld) was below the Noise Floor\n", GetBlockName(config, block), GetBlockSubIndex(config, block), block);
				config->noiseFloorTooHigh |= ComparisonSignal->role;
				warn = 1;
			}
			if(testSize == 0)
			{
				if(!warn)
					logmsg("\n");
				logmsg("WARNING: Comparison %s# %ld (%ld) was below the Noise Floor\n", GetBlockName(config, block), GetBlockSubIndex(config, block), block);
				config->noiseFloorTooHigh |= ComparisonSignal->role;
				warn = 1;
			}
			*/
		}
		else
		{
			refSize = config->MaxFreq;
			testSize = config->MaxFreq;
		}

		if(config->verbose)
		{
			logmsgFileOnly("Comparing %s# %d (%d) %ld vs %ld\n", 
					GetBlockName(config, block), GetBlockSubIndex(config, block), block,
					refSize, testSize);
		}

		if(!CompareFrequencies(ReferenceSignal, ComparisonSignal, CHANNEL_LEFT, block, refSize, testSize, config))
			return 0;

		if(channel == CHANNEL_STEREO)
		{
			if(!CompareFrequencies(ReferenceSignal, ComparisonSignal, CHANNEL_RIGHT, block, refSize, testSize, config))
				return 0;
		}

		if(config->Differences.BlockDiffArray[block].cntFreqBlkDiff)
		{
			if(config->extendedResults)
			{
				logmsgFileOnly("Unmatched Block Report for %s# %ld (%ld)\n", GetBlockName(config, block), GetBlockSubIndex(config, block), block);
				PrintComparedBlocks(&ReferenceSignal->Blocks[block], &ComparisonSignal->Blocks[block],
					config, ReferenceSignal);
			}
		}
		else
		{
			if(config->showAll)
			{
				logmsgFileOnly("Matched Block Report for %s# %ld (%ld)\n", GetBlockName(config, block), GetBlockSubIndex(config, block), block);
				PrintComparedBlocks(&ReferenceSignal->Blocks[block], &ComparisonSignal->Blocks[block], 
					config, ReferenceSignal);
			}
		}
	}

	if(config->extendedResults)
		PrintDifferenceArray(config);
	
	if(config->clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		if(!warn)
			logmsg("\n");
		logmsg(" - clk: Comparing frequencies took %0.2fs\n", elapsedSeconds);
	}
	return 1;
}

/* Time domain normalization functions */

// this is never used
/*
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
		samples[i] = (int16_t)((((double)samples[i])*ratio)+0.5);
}
*/

// These work in the time domain only, not during regular use
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

	// improvement suggested by plgDavid
	for(i = start; i < end; i++)
		samples[i] = (int16_t)((((double)samples[i])*ratio)+0.5);
}

// This is used to Normalize in the time domain, after finding the
// frequency domain ratio, used by Waveform plots only

double FindClippingAndRatio(AudioSignal *Signal, double normalizationRatio, parameters *config)
{
	int 	i = 0;
	double	MaxSample = 0, scaleRatio = 0;

	if(normalizationRatio == 0)
		return 0;

	for(i = 0; i < config->types.totalBlocks; i++)
	{
		if(Signal->Blocks[i].audio.samples)
		{
			double localMax = 0;

			localMax = FindClippingAndRatioForBlock(&Signal->Blocks[i], normalizationRatio);
			if(localMax > MaxSample)
				MaxSample = localMax;
		}
	}

	if(MaxSample)
		scaleRatio = (double)WAVEFORM_SCALE/(MaxSample*normalizationRatio);
	return scaleRatio;
}

int FindClippingAndRatioForBlock(AudioBlocks *AudioArray, double ratio)
{
	long int 	i = 0;
	int			MaxSample = 0;
	double		MaxSampleScaled = 0;
	int16_t		*samples = NULL;

	if(!AudioArray || !ratio)
		return 0;

	samples = AudioArray->audio.samples;
	if(!samples)
		return 0;

	for(i = 0; i < AudioArray->audio.size; i++)
	{
		double sample = 0;

		sample = (((double)samples[i])*ratio)+0.5;
		if(sample > MAXINT16 || sample < MININT16)
		{
			if(fabs(sample) > fabs(MaxSampleScaled))
			{
				MaxSample = samples[i];
				MaxSampleScaled = sample;
			}
		}
	}
	return abs(MaxSample);
}

int FindMaxSampleForWaveform(AudioSignal *Signal, int *block, parameters *config)
{
	int 	i = 0, MaxSample = 0;

	for(i = 0; i < config->types.totalBlocks; i++)
	{
		if(Signal->Blocks[i].audio.samples)
		{
			int localMax = 0;

			localMax = FindMaxSampleInBlock(&Signal->Blocks[i]);
			if(localMax > MaxSample)
			{
				MaxSample = localMax;
				if(block)
					*block = i;
			}
		}
	}

	return MaxSample;
}

int FindMaxSampleInBlock(AudioBlocks *AudioArray)
{
	long int 	i = 0;
	int			MaxSample = 0;
	int16_t		*samples = NULL;

	if(!AudioArray)
		return 0;

	samples = AudioArray->audio.samples;
	if(!samples)
		return 0;

	for(i = 0; i < AudioArray->audio.size; i++)
	{
		int sample = 0;

		sample = abs(samples[i]);
		if(sample > MaxSample)
			MaxSample = sample;
	}
	return MaxSample;
}

/* This is just for waveform visualization */
void ProcessWaveformsByBlock(AudioSignal *SignalToModify, AudioSignal *FixedSignal, double ratio, parameters *config)
{
	double scaleRatio = 0;
	int	MaxSampleToModify = 0, MaxSampleFixed = 0, blockMod = 0, blockFixed = 0;

	scaleRatio = FindClippingAndRatio(SignalToModify, ratio, config);
	if(scaleRatio != 0)
	{
		if(config->verbose) {
			logmsg(" - Found clipping values: %g scaling by %g -> %g \n",
				ratio, scaleRatio, ratio*scaleRatio);
		}

		ratio *= scaleRatio;
		NormalizeTimeDomainByFrequencyRatio(FixedSignal, scaleRatio, config);
	}
	NormalizeTimeDomainByFrequencyRatio(SignalToModify, ratio, config);

	// scale max point to -3dbfs
	MaxSampleToModify = FindMaxSampleForWaveform(SignalToModify, &blockMod, config);
	MaxSampleFixed = FindMaxSampleForWaveform(FixedSignal, &blockFixed, config);

	if(MaxSampleToModify && MaxSampleFixed)
	{
		if(config->verbose) {
			logmsg(" - Found Sample values. Modify(%s): %d at %s# %d (%d) and Fixed(%s): %d at %s# %d (%d)\n",
				SignalToModify->role == ROLE_REF ? "Reference" : "Comparison",
				MaxSampleToModify, GetBlockName(config, blockMod), GetBlockSubIndex(config, blockMod), blockMod,
				FixedSignal->role == ROLE_REF ? "Reference" : "Comparison",
				MaxSampleFixed, GetBlockName(config, blockFixed), GetBlockSubIndex(config, blockFixed), blockFixed);
		}

		if(MaxSampleToModify > MaxSampleFixed)
			scaleRatio = WAVEFORM_SCALE/(double)MaxSampleToModify;
		else
			scaleRatio = WAVEFORM_SCALE/(double)MaxSampleFixed;
		
		if(config->verbose) {
			logmsg(" - Scale factor to reach -3dBFS: %g\n", scaleRatio);
		}

		if(scaleRatio != 1.0)
		{
			NormalizeTimeDomainByFrequencyRatio(SignalToModify, scaleRatio, config);
			NormalizeTimeDomainByFrequencyRatio(FixedSignal, scaleRatio, config);
		}
	}
	else
		logmsg(" - WARNING: Could not scale waveforms\n");
}

void NormalizeTimeDomainByFrequencyRatio(AudioSignal *Signal, double normalizationRatio, parameters *config)
{
	int i = 0;

	if(normalizationRatio == 0)
		return;

	for(i = 0; i < config->types.totalBlocks; i++)
	{
		if(Signal->Blocks[i].audio.samples)
			NormalizeBlockByRatio(&Signal->Blocks[i], normalizationRatio);
	}
}

void NormalizeBlockByRatio(AudioBlocks *AudioArray, double ratio)
{
	long int 	i = 0;
	int16_t		*samples = NULL;

	if(!AudioArray || !ratio)
		return;

	samples = AudioArray->audio.samples;
	if(samples)
	{
		// improvement suggested by plgDavid
		for(i = 0; i < AudioArray->audio.size; i++)
		{
			double sample = 0;
	
			sample = (((double)samples[i])*ratio)+0.5;
			if(sample > MAXINT16 || sample < MININT16)
				logmsg("WARNING: Clipping while doing waveform visualization (%g)\n", sample);
			samples[i] = sample;
		}
	}

	// Do window as well
	samples = AudioArray->audio.window_samples;
	if(samples)
	{
		for(i = 0; i < AudioArray->audio.size; i++)
		{
			double sample = 0;
	
			sample = (((double)samples[i])*ratio)+0.5;
			if(sample > MAXINT16 || sample < MININT16)
				logmsg("WARNING: Clipping while doing windowed waveform visualization (%g)\n", sample);
			samples[i] = sample;
		}
	}

	// Do Internal Sync  as well
	if(AudioArray->internalSyncCount)
	{
		for(int slot = 0; slot < AudioArray->internalSyncCount; slot ++)
		{
			samples = AudioArray->internalSync[slot].samples;
			if(samples)
			{
				for(i = 0; i < AudioArray->internalSync[slot].size; i++)
				{
					double sample = 0;
			
					sample = (((double)samples[i])*ratio)+0.5;
					if(sample > MAXINT16 || sample < MININT16)
						logmsg("WARNING: Clipping while doing windowed waveform visualization (%g)\n", sample);
					samples[i] = sample;
				}
			}
		}
	}
}

// Find the Maximum Amplitude in the Audio File
MaxSample FindMaxSampleAmplitude(AudioSignal *Signal)
{
	long int 		i = 0, start = 0, end = 0;
	int16_t			*samples = NULL;
	MaxSample		maxSampleValue;

	maxSampleValue.maxSample = 0;
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
		if(sample > maxSampleValue.maxSample)
		{
			maxSampleValue.maxSample = sample;
			maxSampleValue.offset = i - start;
		}
	}

	return(maxSampleValue);
}

// Find the Maximum Amplitude in the Reference Audio File
int16_t FindLocalMaximumAroundSample(AudioSignal *Signal, MaxSample refMax)
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
		refSeconds = BytesToSeconds(refMax.samplerate, refMax.offset, Signal->AudioChannels);
		refFrames = refSeconds/(refMax.framerate/1000.0);
	
		tarSeconds = FramesToSeconds(refFrames, Signal->framerate);
		pos = start + SecondsToBytes(Signal->header.fmt.SamplesPerSec, tarSeconds, Signal->AudioChannels, NULL, NULL, NULL);
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
	
				Signal->Blocks[block].freq[i].magnitude *= ratio;
			}

			if(Signal->Blocks[block].freqRight)
			{
				for(int i = 0; i < config->MaxFreq; i++)
				{
					if(!Signal->Blocks[block].freqRight[i].hertz)
						break;
		
					Signal->Blocks[block].freqRight[i].magnitude *= ratio;
				}
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
	MaxMag.channel = CHANNEL_NONE;
	MaxMag.block = -1;

	if(!Signal)
		return MaxMag;

	// Find global peak
	for(int block = 0; block < config->types.totalBlocks; block++)
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
					MaxMag.channel = CHANNEL_LEFT;
				}
			}

			if(Signal->Blocks[block].freqRight)
			{
				for(int i = 0; i < config->MaxFreq; i++)
				{
					if(!Signal->Blocks[block].freqRight[i].hertz)
						break;
					if(Signal->Blocks[block].freqRight[i].magnitude > MaxMag.magnitude)
					{
						MaxMag.magnitude = Signal->Blocks[block].freqRight[i].magnitude;
						MaxMag.hertz = Signal->Blocks[block].freqRight[i].hertz;
						MaxMag.block = block;
						MaxMag.channel = CHANNEL_RIGHT;
					}
				}
			}
		}
	}

	if(MaxMag.block != -1)
	{
		Signal->MaxMagnitude.magnitude = MaxMag.magnitude;
		Signal->MaxMagnitude.hertz = MaxMag.hertz;
		Signal->MaxMagnitude.block = MaxMag.block;
		Signal->MaxMagnitude.channel = MaxMag.channel;
	}

	if(config->verbose && MaxMag.block != -1) {
			logmsg(" - %s Max Magnitude found in %s# %d (%d) [%c] at %g Hz with %g\n", 
					Signal->role == ROLE_REF ? "Reference" : "Comparison",
					GetBlockName(config, MaxMag.block), GetBlockSubIndex(config, MaxMag.block),
					MaxMag.block, MaxMag.channel, MaxMag.hertz, MaxMag.magnitude);
	}

	return MaxMag;
}

int FindMultiMaxMagnitudeBlock(AudioSignal *Signal, MaxMagn	*MaxMag, int size, parameters *config)
{
	if(!MaxMag)
		return 0;

	for(int i = 0; i < size; i++)
	{
		MaxMag[i].magnitude = 0;
		MaxMag[i].hertz = 0;
		MaxMag[i].block = -1;
		MaxMag[i].channel = CHANNEL_NONE;
	}

	if(!Signal)
		return 0;

	// Find global peak
	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(type > TYPE_CONTROL)
		{
			for(int i = 0; i < config->MaxFreq; i++)
			{
				if(!Signal->Blocks[block].freq[i].hertz)
					break;
				if(Signal->Blocks[block].freq[i].magnitude > MaxMag[0].magnitude)
				{
					for(int j = size - 1; j > 0; j--)
						MaxMag[j] = MaxMag[j - 1];

					MaxMag[0].magnitude = Signal->Blocks[block].freq[i].magnitude;
					MaxMag[0].hertz = Signal->Blocks[block].freq[i].hertz;
					MaxMag[0].block = block;
					MaxMag[0].channel = CHANNEL_LEFT;
				}
			}

			if(Signal->Blocks[block].freqRight)
			{
				for(int i = 0; i < config->MaxFreq; i++)
				{
					if(!Signal->Blocks[block].freqRight[i].hertz)
						break;
					if(Signal->Blocks[block].freqRight[i].magnitude > MaxMag[0].magnitude)
					{
						for(int j = size - 1; j > 0; j--)
							MaxMag[j] = MaxMag[j - 1];
	
						MaxMag[0].magnitude = Signal->Blocks[block].freqRight[i].magnitude;
						MaxMag[0].hertz = Signal->Blocks[block].freqRight[i].hertz;
						MaxMag[0].block = block;
						MaxMag[0].channel = CHANNEL_RIGHT;
					}
				}
			}
		}
	}

	if(MaxMag[0].block != -1)
	{
		Signal->MaxMagnitude.magnitude = MaxMag[0].magnitude;
		Signal->MaxMagnitude.hertz = MaxMag[0].hertz;
		Signal->MaxMagnitude.block = MaxMag[0].block;
		Signal->MaxMagnitude.channel = MaxMag[0].channel;
	}

	/*
	if(config->verbose)
	{
		for(int j = 0; j < size; j++)
		{
			if(MaxMag[j].block != -1)
			{
				logmsg(" - %s Max Magnitude[%d] found in %s# %d (%d) at %g Hz with %g\n",
						Signal->role == ROLE_REF ? "Reference" : "Comparison", j, 
						GetBlockName(config, MaxMag[j].block), GetBlockSubIndex(config, MaxMag[j].block),
						MaxMag[j].block, MaxMag[j].hertz, MaxMag[j].magnitude);
			}
		}
	}
	*/

	return 1;
}

double FindLocalMaximumInBlock(AudioSignal *Signal, MaxMagn refMax, int allowDifference, parameters *config)
{
	double		highest = 0;

	if(!Signal)
		return highest;

	// we first try a perfect match
	if(refMax.channel == CHANNEL_LEFT)
	{
		for(int i = 0; i < config->MaxFreq; i++)
		{
			double diff = 0;
			double magnitude = 0;
	
			if(!Signal->Blocks[refMax.block].freq[i].hertz)
				break;
	
			magnitude = Signal->Blocks[refMax.block].freq[i].magnitude;
			diff = fabs(refMax.hertz - Signal->Blocks[refMax.block].freq[i].hertz);
	
			if(diff == 0)
			{
				if(config->verbose) {
					logmsg(" - Comparison Local Max magnitude for [R:%g->C:%g] Hz is %g at %s# %d (%d)\n",
						refMax.hertz, Signal->Blocks[refMax.block].freq[i].hertz, 
						magnitude, GetBlockName(config, refMax.block), GetBlockSubIndex(config, refMax.block), refMax.block);
				}
				return (magnitude);
			}
		}
	}

	if(refMax.channel == CHANNEL_RIGHT)
	{
		if(Signal->Blocks[refMax.block].freqRight)
		{
			for(int i = 0; i < config->MaxFreq; i++)
			{
				double diff = 0;
				double magnitude = 0;
		
				if(!Signal->Blocks[refMax.block].freqRight[i].hertz)
					break;
		
				magnitude = Signal->Blocks[refMax.block].freqRight[i].magnitude;
				diff = fabs(refMax.hertz - Signal->Blocks[refMax.block].freqRight[i].hertz);
		
				if(diff == 0)
				{
					if(config->verbose) {
						logmsg(" - Comparison Local Max magnitude for [R:%g->C:%g] Hz is %g at %s# %d (%d)\n",
							refMax.hertz, Signal->Blocks[refMax.block].freqRight[i].hertz, 
							magnitude, GetBlockName(config, refMax.block), GetBlockSubIndex(config, refMax.block), refMax.block);
					}
					return (magnitude);
				}
			}
		}
		else
		{
			if(config->verbose)
				logmsg("WARNING: Comparison has no right Channel data for match\n");
		}
	}

	if(allowDifference)
	{
		// Now with the tolerance
		// we regularly end in a case where the 
		// peak is a few bins lower or higher
		// and we don't want to normalize against
		// the magnitude of a harmonic sine wave
		// we allow a difference of +/- 5 frequency bins
		if(refMax.channel == CHANNEL_LEFT)
		{
			for(int i = 0; i < config->MaxFreq; i++)
			{
				double diff = 0, binSize = 0;
				double magnitude = 0;
		
				if(!Signal->Blocks[refMax.block].freq[i].hertz)
					break;
		
				magnitude = Signal->Blocks[refMax.block].freq[i].magnitude;
				diff = fabs(refMax.hertz - Signal->Blocks[refMax.block].freq[i].hertz);
		
				binSize = FindFrequencyBinSizeForBlock(Signal, refMax.block);
				if(diff < 5*binSize)
				{
					if(config->verbose) {
						logmsg(" - Comparison Local Max magnitude with tolerance for [R:%g->C:%g] Hz is %g at %s# %d (%d)\n",
							refMax.hertz, Signal->Blocks[refMax.block].freq[i].hertz, 
							magnitude, GetBlockName(config, refMax.block), GetBlockSubIndex(config, refMax.block), refMax.block);
					}
					config->frequencyNormalizationTolerant = diff/binSize;
					return (magnitude);
				}
				if(magnitude > highest)
					highest = magnitude;
			}
		}

		if(refMax.channel == CHANNEL_RIGHT)
		{
			if(Signal->Blocks[refMax.block].freqRight)
			{
				for(int i = 0; i < config->MaxFreq; i++)
				{
					double diff = 0, binSize = 0;
					double magnitude = 0;
			
					if(!Signal->Blocks[refMax.block].freqRight[i].hertz)
						break;
			
					magnitude = Signal->Blocks[refMax.block].freqRight[i].magnitude;
					diff = fabs(refMax.hertz - Signal->Blocks[refMax.block].freqRight[i].hertz);
			
					binSize = FindFrequencyBinSizeForBlock(Signal, refMax.block);
					if(diff < 5*binSize)
					{
						if(config->verbose) {
							logmsg(" - Comparison Local Max magnitude with tolerance for [R:%g->C:%g] Hz is %g at %s# %d (%d)\n",
								refMax.hertz, Signal->Blocks[refMax.block].freqRight[i].hertz, 
								magnitude, GetBlockName(config, refMax.block), GetBlockSubIndex(config, refMax.block), refMax.block);
						}
						config->frequencyNormalizationTolerant = diff/binSize;
						return (magnitude);
					}
					if(magnitude > highest)
						highest = magnitude;
				}
			}
			else
			{
				if(config->verbose)
					logmsg("WARNING: Comparison has no right Channel data for match\n");
			}
		}
	}

	if(config->verbose) {
		logmsg(" - Comparison Local Maximum (No Hz match%s) with %g magnitude at block %d\n",
			allowDifference ? " with tolerance": "", highest, refMax.block);
	}
	return 0;
}

double FindFundamentalMagnitudeAverage(AudioSignal *Signal, parameters *config)
{
	double		AvgFundMag = 0;
	long int	count = 0;

	if(!Signal)
		return 0;

	// Find global peak
	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(type > TYPE_CONTROL && Signal->Blocks[block].freq[0].hertz != 0)
		{
			AvgFundMag += Signal->Blocks[block].freq[0].magnitude;
			count ++;
		}
	}

	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		if(Signal->Blocks[block].freqRight)
		{
			int type = TYPE_NOTYPE;
	
			type = GetBlockType(config, block);
			if(type > TYPE_CONTROL && Signal->Blocks[block].freqRight[0].hertz != 0)
			{
				AvgFundMag += Signal->Blocks[block].freqRight[0].magnitude;
				count ++;
			}
		}
	}

	if(count)
		AvgFundMag /= count;

	if(config->verbose) {
		logmsg(" - %s signal Average Fundamental Magnitude %g from %ld elements\n", 
				Signal->role == ROLE_REF ? "Reference" : "Comparison",
				AvgFundMag, count);
	}

	return AvgFundMag;
}
