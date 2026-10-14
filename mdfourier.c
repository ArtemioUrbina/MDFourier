/*
 * MDFourier
 * A Fourier Transform analysis tool to compare game console audio
 * http://junkerhq.net/MDFourier/
 *
 * Copyright (C)2019-2026 Artemio Urbina
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
#include "loadfile.h"
#include "profile.h"
#include <float.h>

int LoadAndProcessAudioFiles(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config);
int ProcessSignal(AudioSignal *Signal, parameters *config);
int ExecuteDFFT(AudioBlocks *AudioArray, double *samples, size_t size, double samplerate, double *window, int AudioChannels, int ZeroPad, parameters *config);
int ExecuteDFFTInternal(AudioBlocks *AudioArray, double *samples, size_t size, double samplerate, double *window, char channel, int AudioChannels, int ZeroPad, parameters *config);
int CompareAudioBlocks(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config);
int CopySamplesForTimeDomainPlot(AudioBlocks *AudioArray, double *samples, size_t size, size_t diff, double *window, int AudioChannels, int forcecopy, parameters *config);
void CleanUp(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config);
void NormalizeTimeDomainByFrequencyRatio(AudioSignal *Signal, double normalizationRatio, parameters *config);
double FindRatio(AudioSignal *Signal, double normalizationRatio, parameters *config);
double FindRatioForBlock(AudioBlocks *AudioArray, double ratio);
void NormalizeBlockByRatio(AudioBlocks *AudioArray, double ratio);
void ProcessWaveformsByBlock(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, double ratioRef, parameters *config);
double FindMaxSampleForWaveform(AudioSignal *Signal, int *block, parameters *config);
double FindMaxSampleInBlock(AudioBlocks *AudioArray);
void FindViewPort(parameters *config);
int ReportClockResults(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config);
int ReportClock(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, int clockIdx, parameters *config);
int IdentifyCLK(double measured, int *matches, double *ppmDiff, ClockDef *clock);
void ReportCLKIdentity(char *roleName, double measured, int *clkID, ClockDef *clock);
int RecalculateFrequencyStructures(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config);
int NormalizeAndFinishProcess(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config);
int FrequencyDomainNormalize(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config);
void AdjustTimeDomainData(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config);
void printTextResults(parameters *config);

// Time domain
MaxSample FindMaxSampleAmplitude(AudioSignal *Signal);
void NormalizeAudioByRatio(AudioSignal *Signal, double ratio);
double FindLocalMaximumAroundSample(AudioSignal *Signal, MaxSample refMax);

// Frequency domain
void NormalizeMagnitudesByRatio(AudioSignal *Signal, double ratio, parameters *config);
MaxMagn FindMaxMagnitudeBlock(AudioSignal *Signal, parameters *config);
int FindMultiMaxMagnitudeBlock(AudioSignal *Signal, MaxMagn	*MaxMag, int *size, parameters *config);
double FindLocalMaximumInBlock(AudioSignal *Signal, MaxMagn refMax, int allowDifference, parameters *config);
double FindPeakNearInBlock(AudioSignal *Signal, MaxMagn refMax, double *peakHertz, parameters *config);
double FindFundamentalMagnitudeAverage(AudioSignal *Signal, parameters *config);
double FindFundamentalMagnitudeStdDev(AudioSignal *Signal, double AvgFundMag, parameters *config);

int main(int argc, char *argv[])
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
		CleanUp(&ReferenceSignal, &ComparisonSignal, &config);
		return 1;
	}

	clock_gettime(CLOCK_MONOTONIC, &start);

	if(!SetupFolders(config.outputFolder, "Log", &config))
	{
		logmsg("Aborting\n");
		CleanUp(&ReferenceSignal, &ComparisonSignal, &config);
		return 1;
	}

	if(!EndProfileLoad(&config))
	{
		logmsg("Aborting\n");
		CleanUp(&ReferenceSignal, &ComparisonSignal, &config);
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

	ReleasePCM(ReferenceSignal);
	ReleasePCM(ComparisonSignal);

	AdjustTimeDomainData(ReferenceSignal, ComparisonSignal, &config);

	logmsg("\n* Comparing frequencies: ");
	if(!CompareAudioBlocks(ReferenceSignal, ComparisonSignal, &config))
	{
		logmsg("Aborting\n");
		CleanUp(&ReferenceSignal, &ComparisonSignal, &config);
		return 1;
	}

	config.averageDifference = FindDifferenceAverage(&config);
	logmsg("Average difference is %g dB\n", config.averageDifference);
	if(config.substractAveragePlot)
	{
		config.averageDifferenceOrig = config.averageDifference;
		SubstractDifferenceAverageFromResults(&config);
		config.averageDifference = FindDifferenceAverage(&config);
		logmsg(" - Adjusted plots around average, the new average is %g dB\n", config.averageDifference);
	}

	FindViewPort(&config);

	logmsg("* Plotting results to PNGs:\n");
	PlotResults(ReferenceSignal, ComparisonSignal, &config);

	printTextResults(&config);

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

void printTextResults(parameters *config)
{
	double lowest = 100.0f;
	int *typeID = NULL, t = 0, numTypes = 0, lowID = -1;

	numTypes = GetActiveBlockTypesNoRepeat(config);

	typeID = (int*)malloc(sizeof(int)*numTypes);
	if(!typeID)
	{
		logmsg("Not enough memory for text results\n");
		return;
	}
	
	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type > TYPE_CONTROL && !config->types.typeArray[i].IsaddOnData)
		{
			typeID[t] = config->types.typeArray[i].type;
			t++;
		}
	}

	/* Find worst with any amplitude difference */
	for(t = 0; t < numTypes; t++)
	{
		double		pcnt = 0;
		long int	cnt = 0, cmp = 0;

		FindDifferenceWithinInterval(typeID[t], &cnt, &cmp, config->AmpBarRange, CHANNEL_STEREO, config);

		pcnt = ((double)cnt*100.0f)/(double)cmp;
		if(pcnt < lowest)
		{
			lowID = t;
			lowest = pcnt;
		}
	}

	if(lowID != -1)
	{
		char	*label = NULL;
		label = GetTypeDisplayName(config, typeID[lowID]);
		logmsg("* Worst within [0 to \\+-%gdB] Difference: %s~%0.4f%%\n", config->AmpBarRange, label, lowest);
	}
	else
		logmsg("* Worst within [0 to \\+-%gdB] Difference: NOT FOUND\n", config->AmpBarRange);

	free(typeID);
	typeID = NULL;
}

void FindViewPort(parameters *config)
{
	int		type = 0;
	char	*name = NULL;
	double	outside = 0, maxDiff = 0;

	outside = FindDifferencePercentOutsideViewPort(&maxDiff, &type, fabs(config->maxDbPlotZC), config);
	if(outside)
	{
		name = GetTypeDisplayName(config, type);

		if(config->maxDbPlotZCChanged)
		{
			config->notVisible = outside;
			logmsg("- Not auto adjusting vertical, using requested +/-%gdB. Missing [%s]: %g%%\n",
					config->maxDbPlotZC, name, outside);
			return;
		}

		logmsg(" - Differences outside +/-%gdB in [%s]: %g%%\n",
				config->maxDbPlotZC, name, outside);

		if(outside > PCNT_VISIBLE_WRN && outside < PCNT_VISIBLE_ACT)
		{
			config->notVisible = outside;
			logmsg(" - If needed you can graph them all with \"-d %g\" for this particular case\n\n", ceil(maxDiff));
			return;
		}

		if(outside >= PCNT_VISIBLE_ACT && !config->maxDbPlotZCChanged)  // if the user has not changed it
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

				// swap above for this to expand auto expand fully if needed
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
			logmsg(" - Auto adjusting viewport to %gdB for graphs\n", config->maxDbPlotZC);
			if(outside >= 1)
				logmsg(" - The %g%% of differences in [%s] will not be visible within the %gdB for graphs\n - If needed you can graph them all with \"-d %g\" for this particular case\n\n",
					outside, name, config->maxDbPlotZC, ceil(maxDiff));
#ifdef DEBUG
			logmsg("WARNING: Viewport auto adjusted\n");
#endif
		}
	}
	else
		logmsg("\n");

	config->notVisible = outside;
}
void PrintSignalCLKData(AudioSignal *Signal, ClockDef *clock)
{
	if(Signal->EstimatedSR)
	{
		/*
		if(!ApplySamplerateChange(Signal, config))
			logmsg(", WARNING: %s sample rate estimated at %gHz from signal length (can be auto matched with -R)",
				Signal->role == ROLE_REF ? "Reference" : "Comparison",
				Signal->EstimatedSR);
		else
		*/
		{
			if(Signal->originalSR)
			{
				if(Signal->EstimatedSR != (int)Signal->EstimatedSR)
					logmsg(" [Sample rate adjusted %dHz->%0.4fHz with -R]",
						Signal->originalSR, Signal->EstimatedSR);
				else
					logmsg(" [Sample rate adjusted %dHz->%dHz with -R]",
						Signal->originalSR, (int)Signal->EstimatedSR);
			}
		}
	}

	logmsg("\n");

	if(clock->warning & Signal->role)
		logmsg("WARNING: %s has noise or higher harmonics than the fundamental in the %s clock block.\n",
			Signal->role == ROLE_REF ? "Reference" : "Comparison", clock->name);

	if(clock->weak & Signal->role)
		logmsg("WARNING: %s %s clock tone is at %0.2f dBFS below the significant amplitude. Treated as not found.\n",
			Signal->role == ROLE_REF ? "Reference" : "Comparison", clock->name,
			Signal->role == ROLE_REF ? clock->refTone : clock->comTone);
	else if(clock->notFound & Signal->role)
		logmsg("WARNING: %s %s clock tone not found within 5%% of %gHz, the value shown is the strongest frequency.\n",
			Signal->role == ROLE_REF ? "Reference" : "Comparison", clock->name, clock->freq);
}

double GetSignalPeakMagnitude(AudioSignal *Signal, parameters *config)
{
	double	peak = 0;

	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int type = GetBlockType(config, block);

		if(type > TYPE_CONTROL)
		{
			if(Signal->Blocks[block].freq && Signal->Blocks[block].freq[0].magnitude > peak)
				peak = Signal->Blocks[block].freq[0].magnitude;
			if(Signal->Blocks[block].freqRight && Signal->Blocks[block].freqRight[0].magnitude > peak)
				peak = Signal->Blocks[block].freqRight[0].magnitude;
		}
	}
	return peak;
}

// A tone within 5% can still be noise when the signal is not present 
void CheckClockToneLevel(AudioSignal *Signal, int c, double hertz, parameters *config)
{
	double		magnitude = 0, peak = 0, amplitude = NO_AMPLITUDE;
	ClockDef	*clock = &config->clocks[c];
	Frequency	*freq = Signal->clkFrequencies[c].freq;

	if(!freq || (clock->notFound & Signal->role))
		return;

	for(int i = 0; i < config->MaxFreq && freq[i].hertz && !magnitude; i++)
	{
		if(freq[i].hertz == hertz)
			magnitude = freq[i].magnitude;
	}

	peak = GetSignalPeakMagnitude(Signal, config);
	if(magnitude && peak)
		amplitude = CalculateAmplitudeInternal(magnitude, peak);

	if(Signal->role == ROLE_REF)
		clock->refTone = amplitude;
	else
		clock->comTone = amplitude;

	if(amplitude < config->significantAmplitude)
	{
		clock->notFound |= Signal->role;
		clock->weak |= Signal->role;
	}
}

// Optional clocks with 'o' in the profile
void SkipBlockType(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, int type, parameters *config)
{
	for(int t = 0; t < config->types.typeCount; t++)
	{
		if(config->types.typeArray[t].type == type)
			config->types.typeArray[t].type = TYPE_SKIP;
	}

	for(int n = 0; n < config->types.totalBlocks; n++)
	{
		if(ReferenceSignal->Blocks[n].type == type)
			ReferenceSignal->Blocks[n].type = TYPE_SKIP;
		if(ComparisonSignal->Blocks[n].type == type)
			ComparisonSignal->Blocks[n].type = TYPE_SKIP;
	}
}

int SkipMissingOptionalClocks(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config)
{
	int disabled = 0;

	for(int c = 0; c < config->clockCount; c++)
	{
		ClockDef	*clock = &config->clocks[c];

		if(clock->optional && clock->notFound)
		{
			logmsg(" - %s is optional and its tone is missing in %s, skipping:", clock->name,
				clock->notFound == (ROLE_REF | ROLE_COMP) ? "both signals" : clock->notFound == ROLE_REF ? "Reference" : "Comparison");
			for(int i = 0; i < clock->blkAdjustNum; i++)
			{
				logmsg(" %s", GetTypeDisplayName(config, clock->blocksAdjust[i]));
				SkipBlockType(ReferenceSignal, ComparisonSignal, clock->blocksAdjust[i], config);
			}
			logmsg("\n");
			clock->disabled = 1;
			disabled++;
		}
	}

	if(disabled)
	{
		config->types.regularBlocks = GetActiveAudioBlocks(config);
		// The peak could have been in a skipped block
		// Max Frequency runs later in teh process
		if(config->normType != max_frequency)
		{
			FindMaxMagnitude(ReferenceSignal, config);
			FindMaxMagnitude(ComparisonSignal, config);
		}
	}
	return disabled;
}

int IdentifyCLK(double measured, int *matches, double *ppmDiff, ClockDef *clock)
{
	int		i = 0, bestMatch = CLKID_NONE, nearest = CLKID_NONE;
	double	ppm = 0, bestMatchPPM = 0, nearestPPM = 0;

	*matches = 0;
	*ppmDiff = 0;

	if(!clock->ids || !clock->idCount || measured <= 0)
		return CLKID_NONE;

	for(i = 0; i < clock->idCount; i++)
	{
		ppm = (measured - clock->ids[i].nominal)/clock->ids[i].nominal*1000000.0; 

		if(nearest == CLKID_NONE || fabs(ppm) < fabs(nearestPPM))
		{
			nearest = i;
			nearestPPM = ppm;
		}

		if(measured >= clock->ids[i].minHz && measured <= clock->ids[i].maxHz)
		{
			(*matches)++;
			if(bestMatch == CLKID_NONE || fabs(ppm) < fabs(bestMatchPPM))
			{
				bestMatch = i;
				bestMatchPPM = ppm;
			}
		}
	}

	if(bestMatch != CLKID_NONE)
	{
		*ppmDiff = bestMatchPPM;
		return bestMatch;
	}

	*ppmDiff = nearestPPM;
	return nearest;
}

void ReportCLKIdentity(char *roleName, double measured, int *clkID, ClockDef *clock)
{
	int		index = CLKID_NONE, matches = 0;
	double	ppmDiff = 0;

	*clkID = CLKID_NONE;
	index = IdentifyCLK(measured, &matches, &ppmDiff, clock);
	if(index == CLKID_NONE)
		return;

	*clkID = CLKID_UNKNOWN;
	if(matches)
	{
		*clkID = index;
		logmsg(" - %s identified as: %s [%0.2lf ppm from %0.04lfHz]",
			roleName, clock->ids[index].displayName,
			ppmDiff, clock->ids[index].nominal);
		if(matches > 1)
			logmsg(" (%d ranges matched, closest used)", matches);
		logmsg("\n");
	}
	else
	{
		logmsg(" - %s identified as: Unknown, nearest is %s [%0.2lf ppm from %0.04lfHz, range is +/-%g%s]\n",
			roleName, clock->ids[index].displayName,
			ppmDiff, clock->ids[index].nominal,
			clock->ids[index].tolerance, GetCLKToleranceUnit(&clock->ids[index]));
	}
}

int ReportClock(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, int clockIdx, parameters *config)
{
	double		refClkFraction = 0, compClkFraction = 0;
	ClockDef	*clock = &config->clocks[clockIdx];

	if(!CalculateCLKAmplitudes(ReferenceSignal, ComparisonSignal, clockIdx, config)) {
		logmsg("\n* WARNING: %s Clocks based on expected %0.2lf Hz on note %s# %d could not be calculated\n",
		clock->name, clock->freq, GetBlockName(config, clock->block),
		GetBlockSubIndex(config, clock->block));
		// A clock block without frequencies is digital silence, the tone is missing there
		if(!ReferenceSignal->clkFrequencies[clockIdx].freq || !ReferenceSignal->clkFrequencies[clockIdx].freq[0].hertz)
			clock->notFound |= ROLE_REF;
		if(!ComparisonSignal->clkFrequencies[clockIdx].freq || !ComparisonSignal->clkFrequencies[clockIdx].freq[0].hertz)
			clock->notFound |= ROLE_COMP;
		return 0;
	}

	refClkFraction = CalculateClkFraction(ReferenceSignal, clockIdx, config);
	compClkFraction = CalculateClkFraction(ComparisonSignal, clockIdx, config);
	if(!refClkFraction || !compClkFraction)
	{
		logmsg("\n* WARNING: %s Clocks could not be measured\n", clock->name);
		clock->notFound |= (!refClkFraction ? ROLE_REF : 0) | (!compClkFraction ? ROLE_COMP : 0);
		return 0;
	}

	CheckClockToneLevel(ReferenceSignal, clockIdx, refClkFraction, config);
	CheckClockToneLevel(ComparisonSignal, clockIdx, compClkFraction, config);

	clock->ref = refClkFraction * clock->ratio;
	clock->com = compClkFraction * clock->ratio;

	logmsg("\n* Estimated %s Clocks based on expected %0.04lf Hz on note %s# %d:\n",
		clock->name, clock->freq, GetBlockName(config, clock->block),
		GetBlockSubIndex(config, clock->block));

	logmsg(" - Reference: %0.04lfHz -> %0.04lfHz", refClkFraction, clock->ref);
	PrintSignalCLKData(ReferenceSignal, clock);

	logmsg(" - Comparison: %0.04lfHz -> %0.04lfHz", compClkFraction, clock->com);
	PrintSignalCLKData(ComparisonSignal, clock);

	/* Identify before -j changes all */
	if(clock->idCount)
	{
		if(!(clock->notFound & ROLE_REF))
			ReportCLKIdentity("Reference", clock->ref, &clock->refID, clock);
		if(!(clock->notFound & ROLE_COMP))
			ReportCLKIdentity("Comparison", clock->com, &clock->comID, clock);
	}

	/* Without the tone in both signals there is nothing to compare or adjust */
	if(clock->notFound)
	{
		clock->centsDifference = 0;
		logmsg(" - %s clock not compared, tone missing in %s%s\n", clock->name,
			clock->notFound == (ROLE_REF | ROLE_COMP) ? "both signals" : clock->notFound == ROLE_REF ? "Reference" : "Comparison",
			config->doClkAdjust ? ". Not adjusted with -j" : "");
		return 0;
	}

	clock->centsDifference = 1200*log2(refClkFraction/compClkFraction);
	if(fabs(clock->centsDifference) >= MIN_CENTS_DIFF)
	{
		logmsg(" - Pitch difference in cents: %g\n", clock->centsDifference);
		if(!config->doClkAdjust)
		{
			clock->noMatch = 1;
			logmsg(" - WARNING: %s clocks don't match, results may vary considerably. Can adjust with -j\n", clock->name);
		}
		return 1;
	}

	if(config->doClkAdjust)
		logmsg(" - WARNING: Ignoring -j for %s since pitch difference in cents is: %g\n", clock->name, clock->centsDifference);
	return 0;
}

int ReportClockResults(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config)
{
	int differ = 0;

	for(int c = 0; c < config->clockCount; c++)
	{
		config->clocks[c].adjusted = 0;
		if(ReportClock(ReferenceSignal, ComparisonSignal, c, config))
		{
			config->clocks[c].adjusted = config->doClkAdjust;
			differ++;
		}
	}
	return differ;
}

int LoadAudioFiles(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config)
{
	AudioSignal *higher = NULL;

	if(!LoadFile(ReferenceSignal, config->referenceFile, ROLE_REF, config))
		return 0;

	if(!LoadFile(ComparisonSignal, config->comparisonFile, ROLE_COMP, config))
		return 0;

	if(GetSignalMaxInt(*ReferenceSignal) >= GetSignalMaxInt(*ComparisonSignal))
		higher = *ReferenceSignal;
	else
		higher= *ComparisonSignal;

	config->highestValueBitDepth = GetSignalMaxInt(higher);
	config->lowestValueBitDepth = GetSignalMinInt(higher);

	config->lowestDBFS = GetSignalMinDBFS(higher);

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

	ratioTar = config->highestValueBitDepth/MaxTar.maxSample;
	NormalizeAudioByRatio(*ComparisonSignal, ratioTar);
	ComparisonLocalMaximum = FindLocalMaximumAroundSample(*ComparisonSignal, MaxRef);
	if(!ComparisonLocalMaximum)
	{
		logmsg("ERROR: Could not detect Max amplitude in 'Comparison' file for normalization\n");
		return 0;
	}

	ratioRef = (ComparisonLocalMaximum/MaxRef.maxSample);
	if(config->verbose) { logmsg(" - Sample ratio is %g\n", ratioRef); }
	NormalizeAudioByRatio(*ReferenceSignal, ratioRef);

	// Uncomment if you want to check the WAV files as normalized
	//SaveWAVEChunk(NULL, *ReferenceSignal, (*ReferenceSignal)->Samples, 0, (*ReferenceSignal)->numSamples, config);
	//SaveWAVEChunk(NULL, *ComparisonSignal, (*ComparisonSignal)->Samples, 0, (*ComparisonSignal)->numSamples, config);
	return 1;
}

/* The default */
int FrequencyDomainNormalize(AudioSignal **ReferenceSignal, AudioSignal **ComparisonSignal, parameters *config)
{
	MaxMagn				MaxRef, MaxTar;
	double				ComparisonLocalMaximum = 0;
	double				ratioRef = 0, ratiodBFS = 0;
	double				RefAvg = 0;
	double				CompAvg = 0;
	double				ratio = 0, maxRatiodBFS = fabs(FREQDOMRATIO);

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
	ratiodBFS = CalculateAmplitudeInternal(ComparisonLocalMaximum, MaxRef.magnitude);

	if(!config->allowStereoVsMono &&
		MaxRef.block == MaxTar.block &&
		MaxRef.channel != MaxTar.channel &&
		fabs(ratiodBFS) > STEREO_TOLERANCE_REPORT)
	{
		logmsg(" - WARNING: Left and right channels might be reversed or converted from mono to stereo");
		if(config->verbose)
			logmsg(" %g/%g", fabs(ratiodBFS), STEREO_TOLERANCE_REPORT);
		logmsg("\n");
		config->warningStereoReversed = 1;
	}

	if(config->verbose) { logmsg(" - Amplitude ratio is %gdB\n", ratiodBFS == NO_AMPLITUDE ? 0 : ratiodBFS); }

	/* The peak can sit at another frequency in the Comparison when the block runs from a clock of its own */
	if(ComparisonLocalMaximum == 0 || fabs(ratiodBFS) > maxRatiodBFS || ratiodBFS == NO_AMPLITUDE)
	{
		double	nearPeak = 0, nearHertz = 0, nearRatiodBFS = 0;

		nearPeak = FindPeakNearInBlock(*ComparisonSignal, MaxRef, &nearHertz, config);
		if(nearPeak)
		{
			nearRatiodBFS = CalculateAmplitudeInternal(nearPeak, MaxRef.magnitude);
			if(ratiodBFS == NO_AMPLITUDE || fabs(nearRatiodBFS) < fabs(ratiodBFS))
			{
				if(config->verbose) {
					logmsg(" - Comparison peak near %g Hz is at %g Hz, Amplitude ratio is %gdB\n",
						MaxRef.hertz, nearHertz, nearRatiodBFS);
				}
				ComparisonLocalMaximum = nearPeak;
				ratioRef = nearPeak/MaxRef.magnitude;
				ratiodBFS = nearRatiodBFS;
			}
		}
	}

	if(ComparisonLocalMaximum == 0 || fabs(ratiodBFS) > maxRatiodBFS || ratiodBFS == NO_AMPLITUDE)
	{
		int		found = 0, pos = 1, allowDifference = 0, tries = 0, numMatches = FREQDOMTRIES;
		MaxMagn	MaxRefArray[FREQDOMTRIES];
		double	ComparisonLocalMaximumArray = 0, ratioRefArray = 0;

		if(config->verbose) { logmsg(" - Searching for lower ratio alternatives\n"); }
		memset(MaxRefArray, 0, sizeof(MaxRefArray));
		if(FindMultiMaxMagnitudeBlock(*ReferenceSignal, MaxRefArray, &numMatches, config))
		{
			int		matchIndex = -1;
			double	lowestRatio = 0, ComparisonLocalMaximumArrayforLowest = 0, localRatiodBFS = ratiodBFS;

			// We have a do for a second cycle allowing tolerance
			do
			{
				while(MaxRefArray[pos].magnitude != 0 && pos < numMatches)
				{
					if(config->verbose >= 2) {
						logmsg(" - Reference Max Magnitude[%d] found in %s# %d (%d) at %g Hz with %g\n",
							pos,
							GetBlockName(config, MaxRefArray[pos].block), GetBlockSubIndex(config, MaxRefArray[pos].block),
							MaxRefArray[pos].block, MaxRefArray[pos].hertz, MaxRefArray[pos].magnitude);
					}

					ratioRefArray = 0;
					ComparisonLocalMaximumArray = FindLocalMaximumInBlock(*ComparisonSignal, MaxRefArray[pos], allowDifference, config);
					if(ComparisonLocalMaximumArray)
					{
						double dbfsratioArray = 0;

						dbfsratioArray = CalculateAmplitudeInternal(ComparisonLocalMaximumArray, MaxRefArray[pos].magnitude);
						ratioRefArray = ComparisonLocalMaximumArray/MaxRefArray[pos].magnitude;
						if(dbfsratioArray > localRatiodBFS)
						{
							localRatiodBFS = dbfsratioArray;
							lowestRatio = ratioRefArray;
							ComparisonLocalMaximumArrayforLowest = ComparisonLocalMaximumArray;
							matchIndex = pos;
						}
						if(config->verbose >= 2) { logmsg(" - Comparision ratio is %gdB\n", dbfsratioArray); }
						if(fabs(dbfsratioArray) <= fabs(FREQDOMRATIO)) {
							found = 1;
							break;
						}
					}
					pos++;
				}

				if(localRatiodBFS > ratiodBFS)
				{
					found = 1;
					ComparisonLocalMaximumArray = ComparisonLocalMaximumArrayforLowest;
					ratioRefArray = lowestRatio;
					pos = matchIndex;

					if(config->verbose) {
						logmsg(" - Reference Max Magnitude[%d] lowest match %s# %d (%d) at %g Hz with %g\n",
							pos,
							GetBlockName(config, MaxRefArray[pos].block), GetBlockSubIndex(config, MaxRefArray[pos].block),
							MaxRefArray[pos].block, MaxRefArray[pos].hertz, MaxRefArray[pos].magnitude);
					}
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
#ifdef DEBUG
		if(found)
			logmsg("WARNING: Frequency Domain Normalization at %d value\n", config->frequencyNormalizationTries);
#endif

	}
	else
		config->frequencyNormalizationTries = 0;

	if(!ComparisonLocalMaximum || !ratioRef)
	{
		logmsg("ERROR: Could not detect Local Maximum in 'Comparison' file for normalization\n");
		logmsg("\t* Use Time Domain normalization: -n t\n");
		logmsgFileOnly("ComparisonLocalMaximum %g ratioRef %g\n", ComparisonLocalMaximum, ratioRef);
		PrintFrequenciesWMagnitudes(*ReferenceSignal, config);
		PrintFrequenciesWMagnitudes(*ComparisonSignal, config);
		return 0;
	}

	NormalizeMagnitudesByRatio(*ReferenceSignal, ratioRef, config);

	RefAvg = FindFundamentalMagnitudeAverage(*ReferenceSignal, config);
	CompAvg = FindFundamentalMagnitudeAverage(*ComparisonSignal, config);

	if(RefAvg > CompAvg)
		ratio = RefAvg/CompAvg;
	else
		ratio = CompAvg/RefAvg;
	if(fabs(ratio) > fabs(FREQDOMRATIO)) {
		//double dbfsratioArray = 0;

		//dbfsratioArray = CalculateAmplitude(ComparisonLocalMaximum,  MaxRef.magnitude);
		logmsg("\tWARNING: Average frequency difference after normalization between the signals is too high. (Ratio:%g to 1)\n", ratio);
		logmsg("\tIf results make no sense please try the following in the Extra Commands box:\n");
		logmsg("\t* Use Time Domain normalization: -n t\n");
		logmsg("\tThis can be caused by: comparing very different signals, a capacitor problem,\n");
		logmsg("\tframerate difference causing pitch drifting, an unusual frequency scenario, etc.\n");
		config->warningRatioTooHigh = ratio;
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
	{
		FindFloor(ReferenceSignal, config);
		refHasFloor = ReferenceSignal->floorAmplitude != 0.0;
	}
	if(ComparisonSignal->hasSilenceBlock)
	{
		FindFloor(ComparisonSignal, config);
		comHasFloor = ComparisonSignal->floorAmplitude != 0.0;
	}

	avgRef = FindFundamentalAmplitudeAverage(ReferenceSignal, config);
	avgComp = FindFundamentalAmplitudeAverage(ComparisonSignal, config);

	if(refHasFloor && avgRef < ReferenceSignal->floorAmplitude)
	{
		config->noiseFloorTooHigh |= ReferenceSignal->role;
		logmsg(" - Reference noise floor %g dBFS is louder than the average %g dBFS of the signal, ignoring\n",
				ReferenceSignal->floorAmplitude, avgRef);
		ReferenceSignal->floorAmplitude = SIGNIFICANT_AMPLITUDE;
	}

	// Higher noise floor was found, biut low fundamental noise was used
	if(config->referenceNoiseFloor > ReferenceSignal->floorAmplitude)
		config->noiseFloorTooHigh |= ReferenceSignal->role;

	if(comHasFloor && avgComp < ComparisonSignal->floorAmplitude)
	{
		config->noiseFloorTooHigh |= ComparisonSignal->role;
		logmsg(" - Comparison noise floor %g dBFS is louder than the average %g dBFS of the signal, ignoring\n",
				ComparisonSignal->floorAmplitude, avgComp);
		ComparisonSignal->floorAmplitude = SIGNIFICANT_AMPLITUDE;
	}

	/* Detect Signal Floor, default to Reference's*/
	if(refHasFloor)
		config->significantAmplitude = ReferenceSignal->floorAmplitude;

	if(refHasFloor && ReferenceSignal->floorAmplitude > HIGHEST_NOISEFLOOR_ALLOWED)
		config->noiseFloorTooHigh |= ReferenceSignal->role;

	if(comHasFloor && ComparisonSignal->floorAmplitude > HIGHEST_NOISEFLOOR_ALLOWED)
		config->noiseFloorTooHigh |= ComparisonSignal->role;


	// If comparison is lower than the default and higher than reference, use that
	if(config->noiseFloorAutoAdjust)
	{
		if(refHasFloor && comHasFloor &&
			config->significantAmplitude < SIGNIFICANT_AMPLITUDE &&
			ComparisonSignal->floorAmplitude <= HIGHEST_NOISEFLOOR_ALLOWED &&
			ReferenceSignal->floorAmplitude < ComparisonSignal->floorAmplitude)
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
		if(config->significantAmplitude < SIGNIFICANT_AMPLITUDE)
		{
			logmsg(" - Limiting noise floor to %g from %g (from -p 0)\n",
				SIGNIFICANT_AMPLITUDE, config->significantAmplitude);
			config->significantAmplitude = SIGNIFICANT_AMPLITUDE;
		}
	}

	if(config->significantAmplitude >= HIGHEST_NOISEFLOOR_ALLOWED)
	{
		logmsg(" - WARNING: Noise floor %g dBFS is louder than the default %g dBFS. If differences are not visible, define a limit with -p <dbfs>\n",
				config->significantAmplitude, HIGHEST_NOISEFLOOR_ALLOWED);
		//if(!config->noiseFloorTooHigh)
		//	config->noiseFloorTooHigh = ROLE_COMP;
		// we rather not take action for now
		//config->significantAmplitude = SIGNIFICANT_VOLUME;
	}

	/*
	// check if digital noise floor, so we limit ourselves to a reasonable noise floor
	if(config->significantAmplitude < NOISE_FLOOR_DIGITAL_FAIR)
	{
		logmsg(" - Found possible digital noise floor at %g dBFS. Limiting to %g dBFS, use -p to override\n",
			config->significantAmplitude, NOISE_FLOOR_DIGITAL_FAIR);
		config->significantAmplitude = NOISE_FLOOR_DIGITAL_FAIR;
	}
	else
	*/
	logmsg(" - Using %g dBFS as minimum significant amplitude for analysis\n",	config->significantAmplitude);

	if(refHasFloor)
		FindNoiseFloorAmplitudeAverage(ReferenceSignal, config);
	if(comHasFloor)
		FindNoiseFloorAmplitudeAverage(ComparisonSignal, config);
	
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
	if(config->channelBalance && !config->noSyncProfile)
	{
		if((*ReferenceSignal)->AudioChannels == 2 || (*ComparisonSignal)->AudioChannels == 2)
		{
			int block = NO_INDEX;
			char *name = NULL;

			if(config->stereoBalanceBlock)
			{
				block = config->stereoBalanceBlock;
				name = GetBlockName(config, block);
				if(!name)
				{
					logmsg("ERROR: Invalid Mono Balance Block %d\n", block);
					return 0;
				}
			}
			else
			{
				block = GetFirstMonoIndex(config);
				logmsg("- WARNING: MonoBalanceBlock was 0, Using first Mono Block\n");
			}
			if(block != NO_INDEX)
			{
				logmsg("\n* Comparing Stereo channel amplitude\n");
				if(config->verbose) {
					logmsg(" - Mono block used for balance: %s# %d\n",
						name, GetBlockSubIndex(config, block));
				}
				if(CheckBalance(*ReferenceSignal, block, config) == 0)
					return 0;
				if(CheckBalance(*ComparisonSignal, block, config) == 0)
					return 0;
			}
			else
			{
				logmsg(" - WARNING: No mono block for stereo balance check\n");
				config->channelBalance = -1;
			}
		}
	}

	if(config->normType == max_time)
	{
		if(!TimeDomainNormalize(ReferenceSignal, ComparisonSignal, config))
			return 0;
	}

	SetAmplitudeMatchByDuration(*ReferenceSignal, config);

	logmsg("\n* Executing Discrete Fast Fourier Transforms on 'Reference' file\n");
	if(!ProcessSignal(*ReferenceSignal, config))
		return 0;

	logmsg("* Executing Discrete Fast Fourier Transforms on 'Comparison' file\n");
	if(!ProcessSignal(*ComparisonSignal, config))
		return 0;

	CalculateFrequencyBrackets(*ReferenceSignal, config);
	CalculateFrequencyBrackets(*ComparisonSignal, config);

	if(ReportClockResults(*ReferenceSignal, *ComparisonSignal, config) && config->doClkAdjust)
	{
		if(!RecalculateFrequencyStructures(*ReferenceSignal, *ComparisonSignal, config))
		{
			logmsg("Could not recalculate frequencies, Aborting\n");
			return 0;
		}
	}

	SkipMissingOptionalClocks(*ReferenceSignal, *ComparisonSignal, config);

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
	if ((*ReferenceSignal)->MaxMagnitude.magnitude < (*ComparisonSignal)->MaxMagnitude.magnitude)
	{
		ZeroDbMagnitudeRef = (*ComparisonSignal)->MaxMagnitude.magnitude;
		if (config->verbose) {
			logmsg(" - Comparison file has the highest peak at %g vs %g\n",
				ZeroDbMagnitudeRef, (*ReferenceSignal)->MaxMagnitude.magnitude);
		}
	}
	else
	{
		ZeroDbMagnitudeRef = (*ReferenceSignal)->MaxMagnitude.magnitude;
		if (config->verbose) {
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

int CopySamplesForTimeDomainPlotWindowOnly(AudioBlocks *AudioArray, double *window, int AudioChannels, parameters *config)
{
	long			i = 0, monoSignalSize = 0, difference = 0;
	double			*signal = NULL, *window_samples = NULL;

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

	if(AudioArray->audio.windowed_samples)
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

	window_samples = (double*)malloc(sizeof(double)*(monoSignalSize+1));
	if(!window_samples)
	{
		logmsg("Not enough memory for window\n");
		return(0);
	}
	memset(window_samples, 0, sizeof(double)*(monoSignalSize+1));

	for(i = 0; i < monoSignalSize - difference; i++)
		window_samples[i] = signal[i]*window[i];
	AudioArray->audio.windowed_samples = window_samples;

	if(AudioChannels == 2)
	{
		if(AudioArray->audioRight.windowed_samples)
		{
			logmsg("ERROR: Window waveforms already stored\n");
			return 0;
		}

		if(!AudioArray->audioRight.samples)
		{
			logmsg("ERROR: Waveforms not stored\n");
			return 0;
		}

		signal = AudioArray->audioRight.samples;
		monoSignalSize = AudioArray->audioRight.size;
		difference = AudioArray->audioRight.difference;

		window_samples = (double*)malloc(sizeof(double)*(monoSignalSize+1));
		if(!window_samples)
		{
			logmsg("Not enough memory for window\n");
			return(0);
		}
		memset(window_samples, 0, sizeof(double)*(monoSignalSize+1));

		AudioArray->audioRight.size = monoSignalSize;
		AudioArray->audioRight.difference = difference;
		AudioArray->audioRight.padding = 0;

		for(i = 0; i < monoSignalSize - difference; i++)
			window_samples[i] = signal[i]*window[i];
		AudioArray->audioRight.windowed_samples = window_samples;
	}

	return(1);
}

int CopySamplesForTimeDomainPlot(AudioBlocks *AudioArray, double *samples, size_t size, size_t diff, double *window, int AudioChannels, int copywindow, parameters *config)
{
	long			stereoSignalSize = 0;
	long			i = 0, monoSignalSize = 0, diffSize = 0, difference = 0;
	double			*signal = NULL, *signalRight = NULL, *window_samples = NULL;

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
	monoSignalSize = stereoSignalSize/AudioChannels;
	diffSize = (long)diff;
	difference = diffSize/AudioChannels;

	signal = (double*)malloc(sizeof(double)*(monoSignalSize+1));
	if(!signal)
	{
		logmsg("Not enough memory\n");
		return(0);
	}
	memset(signal, 0, sizeof(double)*(monoSignalSize+1));

	if((config->plotAllNotesWindowed && window && !config->doClkAdjust) || (copywindow && window))
	{
		window_samples = (double*)malloc(sizeof(double)*(monoSignalSize+1));
		if(!window_samples)
		{
			logmsg("Not enough memory\n");
			return(0);
		}
		memset(window_samples, 0, sizeof(double)*(monoSignalSize+1));
	}

	for(i = 0; i < monoSignalSize; i++)
		signal[i] = samples[i*AudioChannels];

	AudioArray->audio.samples = signal;
	AudioArray->audio.size = monoSignalSize;
	AudioArray->audio.difference = difference;
	AudioArray->audio.padding = 0;

	if(AudioChannels == 2)
	{
		signalRight = (double*)malloc(sizeof(double)*(monoSignalSize+1));
		if(!signalRight)
		{
			logmsg("Not enough memory for right channel\n");
			return(0);
		}
		memset(signalRight, 0, sizeof(double)*(monoSignalSize+1));

		for(i = 0; i < monoSignalSize; i++)
			signalRight[i] = samples[i*AudioChannels+1];

		AudioArray->audioRight.samples = signalRight;
		AudioArray->audioRight.size = monoSignalSize;
		AudioArray->audioRight.difference = difference;
		AudioArray->audioRight.padding = 0;
	}

	if((config->plotAllNotesWindowed && window && !config->doClkAdjust) || (copywindow && window))
	{
		for(i = 0; i < monoSignalSize - difference; i++)
			window_samples[i] = signal[i]*window[i];
		AudioArray->audio.windowed_samples = window_samples;

		if(AudioChannels == 2 && signalRight)
		{
			double *window_samplesRight = NULL;

			window_samplesRight = (double*)malloc(sizeof(double)*(monoSignalSize+1));
			if(!window_samplesRight)
			{
				logmsg("Not enough memory for window right channel\n");
				return(0);
			}
			memset(window_samplesRight, 0, sizeof(double)*(monoSignalSize+1));
			for(i = 0; i < monoSignalSize - difference; i++)
				window_samplesRight[i] = signalRight[i]*window[i];
			AudioArray->audioRight.windowed_samples = window_samplesRight;
		}
	}

	return(1);
}

void AdjustTimeDomainData(AudioSignal* ReferenceSignal, AudioSignal* ComparisonSignal, parameters* config)
{
	int		block = 0;

	for(block = 0; block < config->types.totalBlocks; block++)
	{	
		if(ReferenceSignal->Blocks[block].type < TYPE_SILENCE)
			continue;

		if(ReferenceSignal->Blocks[block].maskType == MASK_USE_WINDOW)
		{
			/* For Time Domain Plots with big framerate difference we set negative difference for the smaller, so they are drawn at the same scale. */
			if(ReferenceSignal->Blocks[block].audio.difference != 0)
			{
				double seconds = 0;
				seconds = SamplesToSeconds(ReferenceSignal->SampleRate, ReferenceSignal->Blocks[block].audio.difference, 1);
				ComparisonSignal->Blocks[block].audio.padding = SecondsToSamples(ComparisonSignal->SampleRate, seconds, 1, NULL, NULL);
			}
			if(ComparisonSignal->Blocks[block].audio.difference != 0)
			{
				double seconds = 0;
				seconds = SamplesToSeconds(ComparisonSignal->SampleRate, ComparisonSignal->Blocks[block].audio.difference, 1);
				ReferenceSignal->Blocks[block].audio.padding = SecondsToSamples(ReferenceSignal->SampleRate, seconds, 1, NULL, NULL);
			}
		}

		// this might tell a white lie, for visualization, when config->padBlockSizes is true it clips all zeropadding when in both Ref and Comp
		if(config->plotAllNotes != 4)
		{
			if(ReferenceSignal->Blocks[block].type < TYPE_SILENCE)
				continue;

			if(ReferenceSignal->Blocks[block].maskType == MASK_NONE)
			{
				long		frames = 0;

				frames = GetBlockFrames(config, block);
				if(ReferenceSignal->framerate != ComparisonSignal->framerate)
				{
					double secondsRef = 0, secondsComp = 0, padding = 0;

					secondsRef = FramesToSeconds(frames, ReferenceSignal->framerate);
					secondsComp = FramesToSeconds(frames, ComparisonSignal->framerate);
					if(ReferenceSignal->framerate > ComparisonSignal->framerate)
					{
						padding = secondsRef - secondsComp;
						ComparisonSignal->Blocks[block].audio.padding += SecondsToSamples(ComparisonSignal->SampleRate, padding, 1, NULL, NULL);
					}
					else
					{
						padding = secondsComp - secondsRef;
						ReferenceSignal->Blocks[block].audio.padding += SecondsToSamples(ReferenceSignal->SampleRate, padding, 1, NULL, NULL);
					}
				}
			}
		}
		else
		{
			// This one shows the true full zero padding
			if(config->padBlockSizes && ReferenceSignal->Blocks[block].type >= TYPE_SILENCE)
			{
				long		frames = 0;
				double		padding = 0, duration = 0, longest_seconds = 0;

				frames = GetBlockFrames(config, block);
				longest_seconds = FramesToSeconds(config->maxBlockFrameCount, config->biggerFramerate);

				duration = FramesToSeconds(frames, ReferenceSignal->framerate);
				if(longest_seconds > duration)
				{
					padding = longest_seconds - duration;
					ReferenceSignal->Blocks[block].audio.padding += SecondsToSamples(ReferenceSignal->SampleRate, padding, 1, NULL, NULL);
				}
			
				duration = FramesToSeconds(frames, ComparisonSignal->framerate);
				if(longest_seconds > duration)
				{
					padding = longest_seconds - duration;
					ComparisonSignal->Blocks[block].audio.padding += SecondsToSamples(ComparisonSignal->SampleRate, padding, 1, NULL, NULL);
				}
			}
		}
	}
}

// Sample rate to use for a block's DFT 
double GetBlockSampleRate(AudioSignal *Signal, int type, parameters *config)
{
	int c = -1;

	if(!config->doClkAdjust || Signal->role != ROLE_COMP)
		return Signal->SampleRate;

	c = GetClockForType(type, config);
	if(c < 0 || !config->clocks[c].adjusted || !config->clocks[c].comSampleRate)
		return Signal->SampleRate;

	return config->clocks[c].comSampleRate;
}

/* A block DFT queued while walking the signal.
 * Compared the results without paralelizing with OMP
 * The resulting pngs and logs are identical */
typedef struct dfft_job_st {
	long int	block;
	long int	offset;
	long int	size;
	double		samplerate;
	double		*window;
} DFFTJob;

int RunDFFTJobs(AudioSignal *Signal, DFFTJob *jobs, long int count, int recalculate, parameters *config)
{
	int failed = 0;

#ifdef OPENMP_ENABLE
	#pragma omp parallel for schedule(dynamic)
#endif
	for(long int j = 0; j < count; j++)
	{
		AudioBlocks	*block = &Signal->Blocks[jobs[j].block];
		int			ok = 1;

		if(recalculate)
			CleanFrequenciesInBlock(block, config);

		ok = ExecuteDFFT(block, Signal->Samples + jobs[j].offset, jobs[j].size, jobs[j].samplerate, jobs[j].window, Signal->AudioChannels, config->ZeroPad, config);
		if(ok)
			ok = FillFrequencyStructures(Signal, block, config);
		if(ok && recalculate && config->plotAllNotesWindowed)
			ok = CopySamplesForTimeDomainPlotWindowOnly(block, jobs[j].window, Signal->AudioChannels, config);

		if(!ok)
		{
#ifdef OPENMP_ENABLE
			#pragma omp atomic write
#endif
			failed = 1;
		}
	}

	return !failed;
}

// Force a Hamming window for the clock signal and zero padding
int ExecuteClockDFFT(AudioSignal *Signal, int c, double *samples, long int size, long int frames, long int cutFrames, double framerate, double samplerate, char *label, parameters *config)
{
	windowManager	clockWindows;
	double			*window = NULL;
	int				ok = 0;
	char			name[64];

	if(!initWindows(&clockWindows, Signal->SampleRate, 'm', config))
	{
		freeWindows(&clockWindows);
		return 0;
	}

	window = getWindowByLength(&clockWindows, frames, cutFrames, framerate, config);
	CleanFrequenciesInBlock(&Signal->clkFrequencies[c], config);
	ok = ExecuteDFFT(&Signal->clkFrequencies[c], samples, size, samplerate, window, Signal->AudioChannels, ZEROPAD_FACTOR_CLK /* force ZeroPad */, config);
	if(ok)
		ok = FillFrequencyStructures(Signal, &Signal->clkFrequencies[c], config);

	if(ok && config->drawWindows)
	{
		// one clock keeps the old name
		if(config->clockCount == 1)
			sprintf(name, "%s", label);
		else
			sprintf(name, "%s-%.20s", label, config->clocks[c].name);
		VisualizeWindows(&clockWindows, name, Signal->role, config);
	}

	freeWindows(&clockWindows);
	return ok;
}

/* -j recalculates the blocks of adjusted clocks */
int RecalculateFFTW(AudioSignal *Signal, parameters *config)
{
	long int		i = 0, jobCount = 0;
	double			*windowUsed = NULL;
	windowManager	windows;
	DFFTJob			*jobs = NULL;

	if(!config->doClkAdjust)
		return 0;

	jobs = (DFFTJob*)malloc(sizeof(DFFTJob)*config->types.totalBlocks);
	if(!jobs)
	{
		logmsg("\tERROR: malloc failed RecalculateFFTW.\n");
		return(0);
	}

	if(!initWindows(&windows, Signal->SampleRate, config->window, config))
	{
		free(jobs);
		return 0;
	}

	while(i < config->types.totalBlocks)
	{
		int clock = -1;

		if(Signal->Blocks[i].type > TYPE_SILENCE || Signal->Blocks[i].type == TYPE_WATERMARK)
			clock = GetClockForType(Signal->Blocks[i].type, config);

		if(clock >= 0 && config->clocks[clock].adjusted)
		{
			long int	frames = 0, cutFrames = 0, currSamplesSize = 0;
			double		samplerate = 0;

			frames = GetBlockFrames(config, i);
			cutFrames = GetBlockCutFrames(config, i);
			samplerate = GetBlockSampleRate(Signal, Signal->Blocks[i].type, config);

			// Same window selection as ProcessSignal, the block data was loaded with it
			// Windows are created here, serially, the window manager is not thread safe
			if(Signal->Blocks[i].maskType == MASK_USE_WINDOW)
				windowUsed = getWindowByLength(&windows, frames, cutFrames, config->smallerFramerate, config);
			else
				windowUsed = getWindowByLength(&windows, frames, cutFrames, Signal->framerate, config);

			currSamplesSize = Signal->Blocks[i].loadSize - Signal->Blocks[i].difference;

			jobs[jobCount].block = i;
			jobs[jobCount].offset = Signal->Blocks[i].offset;
			jobs[jobCount].size = currSamplesSize;
			jobs[jobCount].samplerate = samplerate;
			jobs[jobCount].window = windowUsed;
			jobCount++;

			// Draws the recalculated, not the original ones
			for(int k = 0; k < config->clockCount; k++)
			{
				if(config->clocks[k].block != i || !config->clocks[k].adjusted)
					continue;

				if(!ExecuteClockDFFT(Signal, k, Signal->Samples + Signal->Blocks[i].offset, currSamplesSize,
						frames, cutFrames, Signal->framerate, samplerate, "CLK-RECALC", config))
				{
					free(jobs);
					freeWindows(&windows);
					return 0;
				}
			}
		}
		i++;
	}

	if(!RunDFFTJobs(Signal, jobs, jobCount, 1, config))
	{
		free(jobs);
		freeWindows(&windows);
		return 0;
	}

	if(config->drawWindows)
		VisualizeWindows(&windows, "CLK-RECALC", Signal->role, config);

	free(jobs);
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

	ratio = config->clkFreq*config->clkRatio/CalculateClk(Signal, config);
	estimatedSampleRate = ceil(Signal->SampleRate*ratio);
	Signal->EstimatedSR_CLK = estimatedSampleRate;

	Signal->originalSR_CLK = Signal->SampleRate;
	Signal->SampleRate = estimatedSampleRate;
	Signal->framerate = CalculateFrameRate(Signal, config);
	logmsg("New frame rate: %g SR: %g->%g (%g)\n",
		Signal->framerate,
		Signal->originalSR_CLK, Signal->SampleRate,
		ratio);
}
*/

// Doing comparison always was default
// It is noisier to adjust clk upwards than downwards though
// so we go for the lower one
int RecalculateFrequencyStructures(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config)
{
	logmsg(" * Recalculating Discrete Fast Fourier Transforms with adjusted clocks\n");

	for(int i = 0; i < config->clockCount; i++)
	{
		ClockDef	*clock = &config->clocks[i];

		if(!clock->adjusted)
			continue;

		// Needs more validation, works with DOS profile
		// Commenting out the auto select, 
		// Keep the reference and only change the clk of the comparision
		clock->comSampleRate = ComparisonSignal->SampleRate*(clock->ref/clock->com);
		logmsg(" - Adjusted Comparison %s to %0.04lfHz\n", clock->name, clock->ref);
		if(config->verbose)
			logmsg("   %s blocks use %0.4lfHz instead of %gHz\n", clock->name,
				clock->comSampleRate, ComparisonSignal->SampleRate);
	}

	if(!RecalculateFFTW(ReferenceSignal, config))
		return 0;

	if(!RecalculateFFTW(ComparisonSignal, config))
		return 0;

	for(int i = 0; i < config->clockCount; i++)
	{
		if(config->clocks[i].adjusted && !CalculateCLKAmplitudes(ReferenceSignal, ComparisonSignal, i, config))
			return 0; 
	}
	return 1;
}

int DuplicateSamplesForWaveformPlots(AudioSignal *Signal, long int element, long int pos, long int loadedBlockSize, long int difference, double framerate, double *windowUsed, parameters *config, long int syncAdvance)
{
	if(config->timeDomainSync && Signal->Blocks[element].type == TYPE_SYNC)
	{
		long int oneFrameSamples = 0;

		oneFrameSamples = SecondsToSamples(Signal->SampleRate, FramesToSeconds(framerate, 1), Signal->AudioChannels, NULL, NULL);
		if(pos > oneFrameSamples) {
			if(!CopySamplesForTimeDomainPlot(&Signal->Blocks[element], Signal->Samples + pos - oneFrameSamples, loadedBlockSize+oneFrameSamples, difference, windowUsed, Signal->AudioChannels, 0, config))
				return 0;
			Signal->Blocks[element].audio.sampleOffset = pos - oneFrameSamples + syncAdvance;
			if(Signal->AudioChannels == 2)
				Signal->Blocks[element].audioRight.sampleOffset = pos - oneFrameSamples + syncAdvance;
		}
		else {
			if(!CopySamplesForTimeDomainPlot(&Signal->Blocks[element], Signal->Samples + pos, loadedBlockSize, difference, windowUsed, Signal->AudioChannels, 0, config))
				return 0;
			Signal->Blocks[element].audio.sampleOffset = pos + syncAdvance;
			if(Signal->AudioChannels == 2)
				Signal->Blocks[element].audioRight.sampleOffset = pos + syncAdvance;
		}
	}
	else
	{
		if(config->plotTimeDomainHiDiff || config->plotAllNotes ||
				config->doClkAdjust || Signal->Blocks[element].type == TYPE_TIMEDOMAIN ||
				Signal->Blocks[element].type == TYPE_SILENCE)
		{
			int forcecopy = 0;

			if(Signal->Blocks[element].type == TYPE_SILENCE)
				forcecopy = 1;
			if(!CopySamplesForTimeDomainPlot(&Signal->Blocks[element], Signal->Samples + pos, loadedBlockSize, difference, windowUsed, Signal->AudioChannels, forcecopy, config))
				return 0;
			Signal->Blocks[element].audio.sampleOffset = pos + syncAdvance;
			if(Signal->AudioChannels == 2)
				Signal->Blocks[element].audioRight.sampleOffset = pos + syncAdvance;
		}
	}

	return 1;
}

#define DIGITAL_SILENCE_LSB		0.5		// samples are integer values
#define DIGITAL_SILENCE_CHANGES	0.001	// fraction of samples allowed above it

double GetChannelSample(double *samples, long int i, int AudioChannels, char channel)
{
	if(channel == CHANNEL_RIGHT)
		return samples[i*AudioChannels+1];
	if(channel == CHANNEL_STEREO)
		return (samples[i*AudioChannels]+samples[i*AudioChannels+1])/2.0;
	return samples[i*AudioChannels];
}

long int CountSilenceChanges(AudioSignal *Signal, double *samples, long int size, double *window, char channel)
{
	long int	monoSize = 0, changes = 0;
	double		rest = 0;

	monoSize = size/Signal->AudioChannels;
	if(monoSize <= 1)
		return monoSize;

	rest = GetChannelSample(samples, monoSize/2, Signal->AudioChannels, channel);
	for(long int i = 0; i < monoSize; i++)
	{
		double	value = 0;

		value = GetChannelSample(samples, i, Signal->AudioChannels, channel) - rest;
		if(window)
			value *= window[i];
		if(fabs(value) >= DIGITAL_SILENCE_LSB)
			changes++;
	}

	return changes;
}

int IsDigitalSilence(AudioSignal *Signal, double *samples, long int size, double *window, char channel, parameters *config)
{
	long int	monoSize = 0, changes = 0;

	monoSize = size/Signal->AudioChannels;
	changes = CountSilenceChanges(Signal, samples, size, window, channel);
	if(config->verbose)
		logmsgFileOnly(" - %s Silence (%c): %ld of %ld windowed samples move from rest (limit %g)\n",
			getRoleText(Signal), channel, changes, monoSize, monoSize*DIGITAL_SILENCE_CHANGES);
	return(monoSize > 1 && changes <= monoSize*DIGITAL_SILENCE_CHANGES);
}

void DiscardDigitalSilence(AudioSignal *Signal, DFFTJob *jobs, long int jobCount, parameters *config)
{
	for(long int j = 0; j < jobCount; j++)
	{
		long int	b = jobs[j].block;
		double		*samples = Signal->Samples + jobs[j].offset;

		if(Signal->Blocks[b].type == TYPE_SILENCE)
		{
			int		left = 0, right = 0;
			char	channel = CHANNEL_LEFT;

			// Same channel selection as ExecuteDFFT
			if(Signal->AudioChannels == 2 && Signal->Blocks[b].channel != CHANNEL_STEREO)
				channel = CHANNEL_STEREO;

			if(Signal->Blocks[b].freq && Signal->Blocks[b].SilenceSizeLeft &&
				IsDigitalSilence(Signal, samples, jobs[j].size, jobs[j].window, channel, config))
			{
				for(int i = 0; i < config->MaxFreq; i++)
					CleanFrequency(&Signal->Blocks[b].freq[i]);
				Signal->Blocks[b].SilenceSizeLeft = 0;
				left = 1;
			}

			if(Signal->AudioChannels == 2 && Signal->Blocks[b].freqRight && Signal->Blocks[b].SilenceSizeRight &&
				IsDigitalSilence(Signal, samples, jobs[j].size, jobs[j].window, CHANNEL_RIGHT, config))
			{
				for(int i = 0; i < config->MaxFreq; i++)
					CleanFrequency(&Signal->Blocks[b].freqRight[i]);
				Signal->Blocks[b].SilenceSizeRight = 0;
				right = 1;
			}

			if(left || right)
				logmsg(" - %s %s# %d (%ld) is digital silence%s, no noise floor data from it\n",
					getRoleText(Signal), GetBlockName(config, b), GetBlockSubIndex(config, b), b,
					Signal->Blocks[b].freqRight && left != right ? (left ? " (left channel)" : " (right channel)") : "");
		}
	}
}

int ProcessSignal(AudioSignal *Signal, parameters *config)
{
	long int		pos = 0;
	double			longest = 0;
	double			*sampleBuffer = NULL;
	long int		sampleBufferSize = 0;
	windowManager	windows;
	long int		loadedBlockSize = 0, i = 0, syncAdvance = 0, jobCount = 0;
	struct timespec	start, end;
	int				discardSamples = 0, syncinternal = 0;
	double			leftDecimals = 0;
	DFFTJob			*jobs = NULL;
#ifdef DEBUG
	long int		totalDiscarded = 0, totalProcessed = 0, totalDifference = 0;
	double			totalTimeEst = 0, totalTimeReal = 0;
#endif

	pos = Signal->startOffset;

	longest = FramesToSeconds(Signal->framerate, GetLongestElementFrames(config));
	if(!longest)
	{
		logmsg("\tERROR: Block definitions are invalid, total length is 0.\n");
		return 0;
	}

	sampleBufferSize = SecondsToSamples(Signal->SampleRate, longest, Signal->AudioChannels, NULL, NULL);
	sampleBuffer = (double*)malloc(sampleBufferSize*sizeof(double));
	if(!sampleBuffer)
	{
		logmsg("\tERROR: malloc failed.\n");
		return(0);
	}

	jobs = (DFFTJob*)malloc(sizeof(DFFTJob)*config->types.totalBlocks);
	if(!jobs)
	{
		free(sampleBuffer);
		logmsg("\tERROR: malloc failed.\n");
		return(0);
	}

	if(!initWindows(&windows, Signal->SampleRate, config->window, config))
	{
		free(sampleBuffer);
		free(jobs);
		logmsg("\tERROR: Could not create FFTW windows.\n");
		return 0;
	}

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	while(i < config->types.totalBlocks)
	{
		int			endProcess = 0;
		double		*windowUsed = NULL;
		double		duration = 0, framerate = 0;
		long int	frames = 0, difference = 0, cutFrames = 0;

		if(!syncinternal)
			framerate = Signal->framerate;
		else
			framerate = config->referenceFramerate;

		frames = GetBlockFrames(config, i);
		cutFrames = GetBlockCutFrames(config, i);
		duration = FramesToSeconds(framerate, frames);

		/* This version looks better when -1hz difference, but is incorrect in frame alignment
		if(areDoublesEqual(framerate, config->smallerFramerate) && !syncinternal)  // this compensates for shorter durations
			loadedBlockSize = SecondsToSamples(Signal->SampleRate, duration, Signal->AudioChannels, NULL, NULL);
		else
			loadedBlockSize = SecondsToSamples(Signal->SampleRate, duration, Signal->AudioChannels, &discardSamples, &leftDecimals);
		*/

		// This compensates framerate difference in the old style, and used in Mega Drive FM between PAL and NTSC
		// Here the window masks out part of the signal in the old style, needed when
		// XTAL for audio is the same between PAL and NTSC (?)
		// C64 needs this off (MASK_NONE) and Genesis on (MASK_USE_WINDOW)
		// current new default is off (MASK_NONE) if the profile doesn't specify it
		if(Signal->Blocks[i].maskType == MASK_USE_WINDOW)
			difference = GetSampleSizeDifferenceByFrameRate(framerate, frames, Signal->SampleRate, Signal->AudioChannels, config);

		// This compensates for subsample time differences
		loadedBlockSize = SecondsToSamples(Signal->SampleRate, duration, Signal->AudioChannels, &discardSamples, &leftDecimals);

		if(Signal->Blocks[i].type >= TYPE_SILENCE || Signal->Blocks[i].type == TYPE_WATERMARK)
		{
			if(!syncinternal && Signal->Blocks[i].maskType == MASK_USE_WINDOW)
				windowUsed = getWindowByLength(&windows, frames, cutFrames, config->smallerFramerate, config); // We get the smaller window, since we'll truncate
			else
				windowUsed = getWindowByLength(&windows, frames, cutFrames, framerate, config);
		}

		if(pos + loadedBlockSize > Signal->numSamples)
		{
			loadedBlockSize = Signal->numSamples - pos;
			endProcess = 1;
		}

		if(!DuplicateSamplesForWaveformPlots(Signal, i, pos, loadedBlockSize, difference, framerate, windowUsed, config, syncAdvance))
		{
			free(sampleBuffer);
			free(jobs);
			freeWindows(&windows);
			return 0;
		}

		if(endProcess || pos + loadedBlockSize > Signal->numSamples)
		{
#ifdef DEBUG
			logmsg("WARNING: End of File load: %ld size: %ld exceed: %ld pos: %ld limit: %ld\n", 
				loadedBlockSize, sampleBufferSize, pos + loadedBlockSize, pos, Signal->numSamples);
#endif
			if(i != config->types.totalBlocks - 1)
			{
				config->smallFile |= Signal->role;
				logmsg("\tUnexpected end of File, please record the full Audio Test from the 240p Test Suite.\n");
				if(config->verbose)
					logmsg("load: %ld size: %ld exceed: %ld pos: %ld limit: %ld\n",
						loadedBlockSize, sampleBufferSize, pos + loadedBlockSize, pos, Signal->numSamples);
			}
			break;
		}

#ifdef DEBUG
		if(config->verbose >= 2)
			logmsg("Pos: %ld (%s %gs) loaded %ld Diff %ld loaded-diff %ld (%.10g s) discard %ld restDec %lg Type: %s Block: %ld\n",
				SamplesForDisplay(pos, Signal->AudioChannels), 
				Signal->Blocks[i].type >= TYPE_SILENCE || Signal->Blocks[i].type == TYPE_WATERMARK ? "" : "NO DFT", duration,
				SamplesForDisplay(loadedBlockSize, Signal->AudioChannels),
				SamplesForDisplay(difference, Signal->AudioChannels),
				SamplesForDisplay(loadedBlockSize - difference, Signal->AudioChannels), SamplesToSeconds(Signal->SampleRate, loadedBlockSize - difference, Signal->AudioChannels),
				SamplesForDisplay(discardSamples, Signal->AudioChannels), leftDecimals/(double)Signal->AudioChannels,
				GetTypeName(config, Signal->Blocks[i].type), i);
#endif
		
		Signal->Blocks[i].offset = pos;
		Signal->Blocks[i].loadSize = loadedBlockSize;
		Signal->Blocks[i].difference = difference;
		if(Signal->Blocks[i].type >= TYPE_SILENCE || Signal->Blocks[i].type == TYPE_WATERMARK)
		{
			// Queued, all block DFTs run in parallel after the signal is walked
			jobs[jobCount].block = i;
			jobs[jobCount].offset = pos;
			jobs[jobCount].size = loadedBlockSize-difference;
			jobs[jobCount].samplerate = Signal->SampleRate;
			jobs[jobCount].window = windowUsed;
			jobCount++;
#ifdef DEBUG
			if(config->verbose >= 3)
				logmsg("estimated %g (difference %ld)\n", Signal->Blocks[i].frames*Signal->framerate/1000.0, difference);
#endif
		}

		// Clocks measured on this block
		for(int c = 0; c < config->clockCount; c++)
		{
			if(config->clocks[c].block != i)
				continue;

			memset(sampleBuffer, 0, sampleBufferSize*sizeof(double));
			memcpy(sampleBuffer, Signal->Samples + pos, (loadedBlockSize-difference)*sizeof(double));

			if(!ExecuteClockDFFT(Signal, c, sampleBuffer, loadedBlockSize-difference, frames, cutFrames,
					framerate, Signal->SampleRate, "CLK", config))
			{
				free(sampleBuffer);
				free(jobs);
				freeWindows(&windows);
				return 0;
			}
		}

		pos += loadedBlockSize;
		pos += discardSamples;

#ifdef DEBUG
		totalProcessed += loadedBlockSize;
		totalDiscarded += discardSamples;
		totalDifference += difference;
		totalTimeEst += SamplesToSeconds(Signal->EstimatedSR, loadedBlockSize - difference, Signal->AudioChannels);
		totalTimeReal += SamplesToSeconds(Signal->SampleRate, loadedBlockSize - difference, Signal->AudioChannels);
#endif

		if(Signal->Blocks[i].type == TYPE_INTERNAL_KNOWN)
		{
			if(!ProcessInternalSync(Signal, i, pos, &syncinternal, &syncAdvance, TYPE_INTERNAL_KNOWN, config))
			{
				free(sampleBuffer);
				free(jobs);
				freeWindows(&windows);
				return 0;
			}
		}

		if(Signal->Blocks[i].type == TYPE_INTERNAL_UNKNOWN)
		{
			if(!ProcessInternalSync(Signal, i, pos, &syncinternal, &syncAdvance, TYPE_INTERNAL_UNKNOWN, config))
			{
				free(sampleBuffer);
				free(jobs);
				freeWindows(&windows);
				return 0;
			}
		}

		i++;
	}

	if(!RunDFFTJobs(Signal, jobs, jobCount, 0, config))
	{
		free(sampleBuffer);
		free(jobs);
		freeWindows(&windows);
		return 0;
	}
	DiscardDigitalSilence(Signal, jobs, jobCount, config);
	free(jobs);
	jobs = NULL;

#ifdef DEBUG
	logmsg("Total discarded %s samples: %ld of %d bytes each (%ld bytes total)\n", 
		Signal->AudioChannels == 2 ? "stereo" : "mono",	SamplesForDisplay(totalDiscarded, Signal->AudioChannels),
		Signal->bytesPerSample, SamplesToBytes(totalDiscarded, Signal->bytesPerSample));
	logmsg("Total difference %s samples: %ld of %d bytes each (%ld bytes total)\n", 
		Signal->AudioChannels == 2 ? "stereo" : "mono",	SamplesForDisplay(totalDifference, Signal->AudioChannels),
		Signal->bytesPerSample,	SamplesToBytes(totalDifference, Signal->bytesPerSample));
	logmsg("Total processed %s samples: %ld of %d bytes each (%ld bytes total)\n", 
		Signal->AudioChannels == 2 ? "stereo" : "mono",	SamplesForDisplay(totalProcessed, Signal->AudioChannels),
		Signal->bytesPerSample,	SamplesToBytes(totalProcessed, Signal->bytesPerSample));
	totalProcessed -= totalDifference;
	totalProcessed -= totalDiscarded;
	logmsg("Final processed %s samples: %ld of %d bytes each (%ld bytes total)\n", 
		Signal->AudioChannels == 2 ? "stereo" : "mono",	SamplesForDisplay(totalProcessed, Signal->AudioChannels),
		Signal->bytesPerSample,	SamplesToBytes(totalProcessed, Signal->bytesPerSample));
	logmsg("Total Time: %.10g @ %g\n", totalTimeReal, Signal->SampleRate);
	if(Signal->SampleRate != Signal->EstimatedSR)
		logmsg("Total Time at Estimated SR: %.10g @ %g\n", totalTimeEst, Signal->EstimatedSR);
#endif

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
		VisualizeWindows(&windows, "MAIN", Signal->role, config);
		PlotBetaFunctions(config);
	}

	free(sampleBuffer);
	freeWindows(&windows);

	return i;
}

int ExecuteDFFT(AudioBlocks *AudioArray, double *samples, size_t size, double samplerate, double *window, int AudioChannels, int ZeroPad, parameters *config)
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

// we use this for normalization now that we zeropad
// https://holometer.fnal.gov/GH_FFT.pdf
int ExecuteDFFTInternal(AudioBlocks *AudioArray, double *samples, size_t size, double samplerate, double *window, char channel, int AudioChannels, int ZeroPad, parameters *config)
{
	fftw_plan		p = NULL;	/* shared per size, from the plan cache */
	long			stereoSignalSize = 0, realMonoSize = 0;
	long			i = 0, monoSignalSize = 0, zeropadding = 0;
	double			*signal = NULL;
	fftw_complex	*spectrum = NULL;
	double			seconds = 0, S2 = 0;

	if(!AudioArray)
	{
		logmsg("No Array for results\n");
		return 0;
	}

	stereoSignalSize = (long)size;
	monoSignalSize = stereoSignalSize/AudioChannels;
	seconds = (double)size/(samplerate*(double)AudioChannels);
	realMonoSize = monoSignalSize;
	
	if(ZeroPad)  /* explicit alignment target wins (CLK path, or -Z) */
		GetZeroPadValues(&monoSignalSize, &seconds, samplerate, ZeroPad);
	else if(config->padBlockSizes)
		GetBlockZeroPadValues(&monoSignalSize, &seconds, config->maxBlockSeconds, samplerate);
	
	zeropadding = monoSignalSize - realMonoSize;

#ifdef DEBUG
	if(config->verbose >= 2)
		logmsg("ExecuteDFFTInternal> stereoSignalSize: %ld monoSignalSize: %ld zeropadding: %ld windowed: %ld seconds: %g\n", 
			stereoSignalSize, monoSignalSize, zeropadding, monoSignalSize - zeropadding, seconds);
#endif

	// fftw_malloc so the alignment matches the cached plan's
	signal = (double*)fftw_malloc(sizeof(double)*(monoSignalSize+1));
	if(!signal)
	{
		logmsg("Not enough memory\n");
		return(0);
	}
	spectrum = (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*(monoSignalSize/2+1));
	if(!spectrum)
	{
		fftw_free(signal);
		logmsg("Not enough memory\n");
		return(0);
	}

	p = GetR2CPlan(monoSignalSize);
	if(!p)
	{
		logmsg("FFTW failed to create FFTW_MEASURE plan\n");
		fftw_free(signal);
		signal = NULL;
		fftw_free(spectrum);
		return 0;
	}

	memset(signal, 0, sizeof(double)*(monoSignalSize+1));
	memset(spectrum, 0, sizeof(fftw_complex)*(monoSignalSize/2+1));

#ifdef DEBUG
	if(config->verbose >= 3)
		logmsg("monoSignalSize: %ld zeropadding: %ld monoSignalSize - zeropadding: %ld\n",
			monoSignalSize, zeropadding, monoSignalSize - zeropadding);
#endif

	for(i = 0; i < monoSignalSize - zeropadding; i++)
	{
		if(channel == CHANNEL_LEFT)
			signal[i] = samples[i*AudioChannels];
		if(channel == CHANNEL_RIGHT)
			signal[i] = samples[i*AudioChannels+1];
		if(channel == CHANNEL_STEREO)
			signal[i] = (samples[i*AudioChannels]+samples[i*AudioChannels+1])/2.0;

		if(window)
		{
			signal[i] *= window[i];
			S2 += window[i]*window[i];
			if(isinf(S2)) {
				logmsg("i: %ld S2: %g window[i]: %g\n", i, S2, window[i]);
				logmsg("monoSignalSize: %ld zeropadding: %ld monoSignalSize - zeropadding: %ld\n",
					monoSignalSize, zeropadding, monoSignalSize - zeropadding);
				logmsg("ERROR: Window error in code detected\n");
				fftw_free(signal);
				fftw_free(spectrum);
				return 0;
			}
		}
	}

	fftw_execute_dft_r2c(p, signal, spectrum);
	p = NULL;

#ifdef DEBUG
	if(config->verbose >= 3)
		logmsg("Seconds %g was %g. S2: %ld\n", seconds, AudioArray->seconds, S2);
#endif

	if(channel != CHANNEL_RIGHT)
	{
		AudioArray->fftwValues.spectrum = spectrum;
		AudioArray->fftwValues.size = monoSignalSize;
		AudioArray->fftwValues.ENBW = samplerate*S2;
	}
	else
	{
		AudioArray->fftwValuesRight.spectrum = spectrum;
		AudioArray->fftwValuesRight.size = monoSignalSize;
		AudioArray->fftwValuesRight.ENBW = samplerate*S2;
	}
	AudioArray->seconds = seconds;
	fftw_free(signal);
	signal = NULL;

	return(1);
}

int CalculateMaxCompare(int block, AudioSignal *Signal, double significant, char channel, parameters *config)
{
	long int	size = 0;
	double		limit = 0;
	Frequency	*freqCheck = NULL;

	size = GetBlockFreqSize(Signal, block, channel, config);
	if(channel == CHANNEL_LEFT)
		freqCheck = Signal->Blocks[block].freq;
	else
		freqCheck = Signal->Blocks[block].freqRight;

	if(!freqCheck)
		return 0;

	limit = significant;

	if(Signal->role == ROLE_COMP)
		limit += -20;	// Allow going 20 dbfs "deeper"

	for(int freq = 0; freq < size; freq++)
	{
		/* Out of valid frequencies */
		if(!freqCheck[freq].hertz)
			return(freq);

		/* Amplitude is too low */
		if(freqCheck[freq].amplitude <= limit)
			return(freq);
	}

	return size;
}

typedef struct hertz_index_st {
	double	hertz;
	int		index;
} HertzIndex;

static inline int HertzIndexOrder(const HertzIndex a, const HertzIndex b)
{
	if(a.hertz < b.hertz)
		return -1;
	if(a.hertz > b.hertz)
		return 1;
	if(a.index < b.index)
		return -1;
	if(a.index > b.index)
		return 1;
	return 0;
}

#define SORT_NAME HertzIndex
#define SORT_TYPE HertzIndex
#define SORT_CMP(x, y)  HertzIndexOrder((x), (y))
#include "sort.h"  // https://github.com/swenson/sort/

int FindMatchingFrequency(HertzIndex *sorted, int size, Frequency *freqComp, double hertz)
{
	int		low = 0, high = size, best = -1;
	double	tolerance = 0;

	tolerance = 2*DBL_PERFECT_MATCH + 4*DBL_EPSILON*fabs(hertz);

	while(low < high)	// first entry >= hertz - tolerance
	{
		int mid = low + (high - low)/2;

		if(sorted[mid].hertz < hertz - tolerance)
			low = mid + 1;
		else
			high = mid;
	}

	for(int i = low; i < size && sorted[i].hertz <= hertz + tolerance; i++)
	{
		int comp = sorted[i].index;

		if(!freqComp[comp].matched && areDoublesEqual(hertz, freqComp[comp].hertz))
		{
			if(best == -1 || comp < best)
				best = comp;
		}
	}
	return best;
}

int CompareFrequencies(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, char channel, int block, int refSize, int testSize, parameters *config)
{
	Frequency	*freqRef = NULL, *freqComp = NULL;
	HertzIndex	*sorted = NULL;

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

	if(!freqRef || !freqComp)
	{
		logmsg("Internal consistency failure, please send error log (invalid channel)\n");
		return 0;
	}

	// For binary search
	if(testSize > 0)
	{
		sorted = (HertzIndex*)malloc(sizeof(HertzIndex)*testSize);
		if(!sorted)
		{
			logmsg("Not enough memory for frequency comparison\n");
			return 0;
		}
		for(int comp = 0; comp < testSize; comp++)
		{
			sorted[comp].hertz = freqComp[comp].hertz;
			sorted[comp].index = comp;
		}
		
		HertzIndex_heap_sort(sorted, testSize);
	}

	for(int freq = 0; freq < refSize; freq++)
	{
		int found = 0, index = 0;

		if(!IncrementCompared(block, channel, config))
		{
			logmsg("Internal consistency failure, please send error log (compare)\n");
			free(sorted);
			return 0;
		}

		if(!freqRef[freq].matched && sorted)
		{
			int comp = FindMatchingFrequency(sorted, testSize, freqComp, freqRef[freq].hertz);

			if(comp != -1)
			{
				freqComp[comp].matched = freq + 1;
				freqRef[freq].matched = comp + 1;

				found = 1;
				index = comp;
			}
		}

  		/* Now in either case, compare amplitude and phase */
		if(found)
		{
			if(!areDoublesEqual(freqRef[freq].amplitude, freqComp[index].amplitude))
			{
				if(!InsertAmplDifference(block, freqRef[freq], freqComp[index], channel, config))
				{
					logmsg("Internal consistency failure, please send error log (AmplDiff)\n");
					free(sorted);
					return 0;
				}
			}
			else /* perfect match */
			{
				if(!IncrementPerfectMatch(block, channel, config))
				{
					logmsg("Internal consistency failure, please send error log (perfect)\n");
					free(sorted);
					return 0;
				}
			}

			if(!areDoublesEqual(freqRef[freq].phase, freqComp[index].phase))
			{
				if(!InsertPhaseDifference(block, freqRef[freq], freqComp[index], channel, config))
				{
					logmsg("Internal consistency failure, please send error log (PhaseDiff)\n");
					free(sorted);
					return 0;
				}
			}
			else /* perfect match phase */
			{
			}
		}
		else /* Frequency Not Found */
		{
			if(!InsertFreqNotFound(block, freqRef[freq].hertz, freqRef[freq].amplitude, channel, config))
			{
				logmsg("Internal consistency failure, please send error log (Not found)\n");
				free(sorted);
				return 0;
			}
		}
	}
	free(sorted);
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

	if(config->extendedResults || config->showAll)
		logmsg("\n");

	for(block = 0; block < config->types.totalBlocks; block++)
	{
		char	channel = CHANNEL_MONO;
		int 	refSize = 0, testSize = 0, type = 0;

		type = GetBlockType(config, block);
		channel = GetBlockChannel(config, block);
 
		/* Ignore Control blocks */
		if(type < TYPE_CONTROL)
			continue;

		refSize = CalculateMaxCompare(block, ReferenceSignal, type != TYPE_SILENCE ? config->significantAmplitude : SILENCE_LIMIT, CHANNEL_LEFT, config);
		testSize = CalculateMaxCompare(block, ComparisonSignal, type != TYPE_SILENCE ? config->significantAmplitude : SILENCE_LIMIT, CHANNEL_LEFT, config);

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
			refSize = CalculateMaxCompare(block, ReferenceSignal, type != TYPE_SILENCE ? config->significantAmplitude : SILENCE_LIMIT, CHANNEL_RIGHT, config);
			testSize = CalculateMaxCompare(block, ComparisonSignal, type != TYPE_SILENCE ? config->significantAmplitude : SILENCE_LIMIT, CHANNEL_RIGHT, config);

			if(!CompareFrequencies(ReferenceSignal, ComparisonSignal, CHANNEL_RIGHT, block, refSize, testSize, config))
				return 0;
		}

		if(type > TYPE_CONTROL)
		{
			if(config->Differences.BlockDiffArray[block].cntFreqBlkDiff)
			{
				if(config->extendedResults)
				{
					logmsgFileOnly("Matched Block Report for %s# %ld (%ld) (differences: %ld)\n", 
						GetBlockName(config, block), GetBlockSubIndex(config, block), block,
						config->Differences.BlockDiffArray[block].cntFreqBlkDiff);
					PrintComparedBlocks(&ReferenceSignal->Blocks[block], &ComparisonSignal->Blocks[block], config);
				}
			}
			else
			{
				if(config->showAll)
				{
					logmsgFileOnly("Matched Block Report for %s# %ld (%ld)\n", GetBlockName(config, block), GetBlockSubIndex(config, block), block);
					PrintComparedBlocks(&ReferenceSignal->Blocks[block], &ComparisonSignal->Blocks[block], config);
				}
			}
		}
	}

	if(config->showAll)
	{
		for(block = 0; block < config->types.totalBlocks; block++)
		{
			logmsgFileOnly("Values above 3dB for %s# %ld (%ld)\n", GetBlockName(config, block), GetBlockSubIndex(config, block), block);
			PrintThesholdDifferenceBlocks(&ReferenceSignal->Blocks[block], &ComparisonSignal->Blocks[block],
				config, 3.0);
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
	double		*samples = NULL, maxSampleValue = 0;
	double		ratio = 0;

	if(!Signal)
		return;

	samples = Signal->Samples;
	start = Signal->startOffset;
	end = Signal->endOffset;
	for(i = start; i < end; i++)
	{
		double sample;

		sample = fabs(samples[i]);
		if(sample > maxSampleValue)
			maxSampleValue = sample;
	}

	if(!maxSampleValue)
		return;

	ratio = (double)0x7FFF/(double)maxSampleValue;

	for(i = start; i < end; i++)
		samples[i] = samples[i]*ratio;
}
*/

// These work in the time domain only, not during regular use
void NormalizeAudioByRatio(AudioSignal *Signal, double ratio)
{
	long int 	i = 0, start = 0, end = 0;
	double		*samples = NULL;

	if(!Signal)
		return;

	if(!ratio)
		return;

	samples = Signal->Samples;
	start = Signal->startOffset;
	end = Signal->endOffset;

	// improvement suggested by plgDavid
	// Removed the * 0.5 since we changed to internal double representation)
	for(i = start; i < end; i++)
		samples[i] = samples[i]*ratio;
}

// This is used to Normalize in the time domain, after finding the
// frequency domain ratio, used by Waveform plots only

double FindRatio(AudioSignal *Signal, double normalizationRatio, parameters *config)
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

			localMax = FindRatioForBlock(&Signal->Blocks[i], normalizationRatio);
			if(localMax > MaxSample)
				MaxSample = localMax;
		}
	}

	if(MaxSample)
		scaleRatio = CalculatePCMMagnitude(-3.0, config->highestValueBitDepth)/(MaxSample*normalizationRatio);
	return scaleRatio;
}

double FindRatioForBlock(AudioBlocks *AudioArray, double ratio)
{
	long int 	i = 0;
	double		MaxSample = 0;
	double		MaxSampleScaled = 0;
	double		*samples = NULL;

	if(!AudioArray || !ratio)
		return 0;

	samples = AudioArray->audio.samples;
	if(!samples)
		return 0;

	for(i = 0; i < AudioArray->audio.size; i++)
	{
		double sample = 0;

		sample = samples[i]*ratio;
		if(fabs(sample) > fabs(MaxSampleScaled))
		{
			MaxSample = samples[i];
			MaxSampleScaled = sample;
		}
	}

	samples = AudioArray->audioRight.samples;
	if(!samples)
		return fabs(MaxSample);

	for(i = 0; i < AudioArray->audioRight.size; i++)
	{
		double sample = 0;

		sample = samples[i]*ratio;
		if(fabs(sample) > fabs(MaxSampleScaled))
		{
			MaxSample = samples[i];
			MaxSampleScaled = sample;
		}
	}

	return fabs(MaxSample);
}

double FindMaxSampleForWaveform(AudioSignal *Signal, int *block, parameters *config)
{
	int 	i = 0;
	double	MaxSample = 0;

	for(i = 0; i < config->types.totalBlocks; i++)
	{
		if(Signal->Blocks[i].audio.samples)
		{
			double localMax = 0;

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

double FindMaxSampleInBlock(AudioBlocks *AudioArray)
{
	long int 	i = 0;
	double		MaxSample = 0;
	double		*samples = NULL;

	if(!AudioArray)
		return 0;

	samples = AudioArray->audio.samples;
	if(!samples)
		return 0;

	for(i = 0; i < AudioArray->audio.size; i++)
	{
		double sample = 0;

		sample = fabs(samples[i]);
		if(sample > MaxSample)
			MaxSample = sample;
	}

	samples = AudioArray->audioRight.samples;
	if(!samples)
		return MaxSample;

	for(i = 0; i < AudioArray->audioRight.size; i++)
	{
		double sample = 0;

		sample = fabs(samples[i]);
		if(sample > MaxSample)
			MaxSample = sample;
	}
	return MaxSample;
}

/* This is just for waveform visualization */
void ProcessWaveformsByBlock(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, double ratio, parameters *config)
{
	double 	scaleRatio = 0;
	double	MaxSampleInRef = 0, MaxSampleInComp = 0;
	int		blockMod = 0, blockFixed = 0;

	// Gets us the Maxsample in the Reference Signal scaled to -3dbfs based on bitdepth
	scaleRatio = FindRatio(ReferenceSignal, ratio, config);
	if(scaleRatio == 0)
	{
		logmsg("ERROR: Could not find Waveform scale\n");
		return;
	}

	if(config->verbose) {
		logmsg(" - Found waveform scaling ratio %g \n",
			ratio*scaleRatio);
	}

	ratio *= scaleRatio;
	NormalizeTimeDomainByFrequencyRatio(ComparisonSignal, scaleRatio, config);
	NormalizeTimeDomainByFrequencyRatio(ReferenceSignal, ratio, config);

	// scale max point in reference to -3dbfs
	MaxSampleInRef = FindMaxSampleForWaveform(ReferenceSignal, &blockMod, config);
	MaxSampleInComp = FindMaxSampleForWaveform(ComparisonSignal, &blockFixed, config);

	if(MaxSampleInRef != 0 && MaxSampleInComp != 0)
	{
		if(config->verbose) {
			logmsg(" - Visual wave values: Modify (%s/%dbits): %g at %s# %d (%d) and Fixed(%s/%dbits): %g at %s# %d (%d)\n",
				ReferenceSignal->role == ROLE_REF ? "Reference" : "Comparison", ReferenceSignal->bytesPerSample*8,
				MaxSampleInRef, GetBlockName(config, blockMod), GetBlockSubIndex(config, blockMod), blockMod,
				getRoleText(ComparisonSignal), ComparisonSignal->bytesPerSample*8,
				MaxSampleInComp, GetBlockName(config, blockFixed), GetBlockSubIndex(config, blockFixed), blockFixed);
		}

		
		scaleRatio = CalculatePCMMagnitude(-3.0, config->highestValueBitDepth)/MaxSampleInRef;
		
		if(config->verbose) {
			logmsg(" - Scale factor to reach -3dBFS in waveforms: %g\n", fabs(1.0 - scaleRatio) > 0.001 ? scaleRatio : 1.0);
		}

		if(fabs(1.0 - scaleRatio) > 0.001)
		{
			NormalizeTimeDomainByFrequencyRatio(ReferenceSignal, scaleRatio, config);
			NormalizeTimeDomainByFrequencyRatio(ComparisonSignal, scaleRatio, config);
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
	double		*samples = NULL;

	if(!AudioArray || !ratio)
		return;

	samples = AudioArray->audio.samples;
	if(samples)
	{
		for(i = 0; i < AudioArray->audio.size; i++)
			samples[i] = samples[i]*ratio;
	}

	samples = AudioArray->audioRight.samples;
	if(samples)
	{
		for(i = 0; i < AudioArray->audioRight.size; i++)
			samples[i] = samples[i]*ratio;
	}

	// Do window as well
	samples = AudioArray->audio.windowed_samples;
	if(samples)
	{
		for(i = 0; i < AudioArray->audio.size; i++)
			samples[i] = samples[i]*ratio;
	}

	samples = AudioArray->audioRight.windowed_samples;
	if(samples)
	{
		for(i = 0; i < AudioArray->audio.size; i++)
			samples[i] = samples[i]*ratio;
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
					samples[i] = samples[i]*ratio;
			}
		}
	}
}

// Find the Maximum Amplitude in the Audio File
MaxSample FindMaxSampleAmplitude(AudioSignal *Signal)
{
	long int 		i = 0, start = 0, end = 0;
	double			*samples = NULL;
	MaxSample		maxSampleValue;

	maxSampleValue.maxSample = 0;
	maxSampleValue.offset = 0;
	maxSampleValue.samplerate = Signal->SampleRate;
	maxSampleValue.framerate = Signal->framerate;

	if(!Signal)
		return maxSampleValue;

	samples = Signal->Samples;
	start = Signal->startOffset;
	end = Signal->endOffset;
	for(i = start; i < end; i++)
	{
		double sample;

		sample = fabs(samples[i]);
		if(sample > maxSampleValue.maxSample)
		{
			maxSampleValue.maxSample = sample;
			maxSampleValue.offset = i - start;
		}
	}

	return(maxSampleValue);
}

// Find the Maximum Amplitude in the Reference Audio File
double FindLocalMaximumAroundSample(AudioSignal *Signal, MaxSample refMax)
{
	long int 		i, start = 0, end = 0, pos = 0;
	double			*samples = NULL, MaxLocalSample = 0;
	double			refSeconds = 0, refFrames = 0, tarSeconds = 0, fraction = 0;

	if(!Signal)
		return 0;

	start = Signal->startOffset;
	end = Signal->endOffset;

	if(!areDoublesEqual(refMax.framerate, Signal->framerate))
	{
		refSeconds = SamplesToSeconds(refMax.samplerate, refMax.offset, Signal->AudioChannels);
		refFrames = refSeconds/(refMax.framerate/1000.0);

		tarSeconds = FramesToSeconds(refFrames, Signal->framerate);
		pos = start + SecondsToSamples(Signal->SampleRate, tarSeconds, Signal->AudioChannels, NULL, NULL);
	}
	else
	{
		pos = start + refMax.offset;
		pos = (double)pos*Signal->SampleRate/refMax.samplerate;
	}

	if(pos > end)
		return 0;

	// Search in 2/faction of Signal->SampleRate
	// around the position of the sample

	fraction = 60.0; // around 1 frame
	if(pos - Signal->SampleRate/fraction >= start)
		start = pos - Signal->SampleRate/fraction;

	if(end >= pos + Signal->SampleRate/fraction)
		end = pos + Signal->SampleRate/fraction;

	samples = Signal->Samples;
	for(i = start; i < end; i++)
	{
		double sample;

		sample = fabs(samples[i]);
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
		int			type = TYPE_NOTYPE;
		long int	size = 0;

		type = GetBlockType(config, block);
		if(type >= TYPE_SILENCE || type == TYPE_WATERMARK || type == TYPE_CLK_ANALYSIS)
		{
			size = GetBlockFreqSize(Signal, block, CHANNEL_LEFT, config);
			for(long int i = 0; i < size; i++)
			{
				if(!Signal->Blocks[block].freq[i].hertz)
					break;

				Signal->Blocks[block].freq[i].magnitude *= ratio;
			}

			if(Signal->Blocks[block].freqRight)
			{
				size = GetBlockFreqSize(Signal, block, CHANNEL_RIGHT, config);
				for(long int i = 0; i < size; i++)
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
	long int size = 0;

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
		if(type > TYPE_CONTROL || type == TYPE_WATERMARK)
		{
			size = GetBlockFreqSize(Signal, block, CHANNEL_LEFT, config);
			for(long int i = 0; i < size; i++)
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
				size = GetBlockFreqSize(Signal, block, CHANNEL_RIGHT, config);
				for(long int i = 0; i < size; i++)
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
			double seconds = 0;
			long int offset = 0;

			seconds = FramesToSeconds(GetElementFrameOffset(MaxMag.block, config), Signal->framerate);
			offset = SecondsToSamples(Signal->SampleRate, seconds, Signal->header.fmt.NumOfChan, NULL, NULL);
			offset = SamplesForDisplay(offset, Signal->header.fmt.NumOfChan);

			logmsg(" - %s Max Magnitude found in %s# %d (%d) [ %c ] at %g Hz with %g (%g seconds/%ld samples)\n",
					getRoleText(Signal),
					GetBlockName(config, MaxMag.block), GetBlockSubIndex(config, MaxMag.block),
					MaxMag.block, MaxMag.channel, MaxMag.hertz, MaxMag.magnitude,
					seconds, offset);
	}

	return MaxMag;
}

void swapMagnitudes(MaxMagn *xp, MaxMagn *yp)
{
    MaxMagn temp = *xp;

    *xp = *yp;
    *yp = temp;
}
 
void bubbleSortMagnitudes(MaxMagn *arr, int n)
{
	int i, j;

	for (i = 0; i < n-1; i++)
	{
		for (j = 0; j < n-i-1; j++)
		{
			if (arr[j].magnitude < arr[j+1].magnitude)
				swapMagnitudes(&arr[j], &arr[j+1]);
		}
	}
}

int FindMultiMaxMagnitudeBlock(AudioSignal *Signal, MaxMagn	*MaxMag, int *size, parameters *config)
{
	long int blocksize = 0;
	double averageMagRef = 0, stdDevMagRef = 0, threshold = 0;

	if(!MaxMag)
		return 0;
	if(!Signal)
		return 0;
	if(!size)
		return 0;

	for(int i = 0; i < *size; i++)
	{
		MaxMag[i].magnitude = 0;
		MaxMag[i].hertz = 0;
		MaxMag[i].block = -1;
		MaxMag[i].channel = CHANNEL_NONE;
	}

	averageMagRef = FindFundamentalMagnitudeAverage(Signal, config);
	stdDevMagRef = FindFundamentalMagnitudeStdDev(Signal, averageMagRef, config);
	threshold = averageMagRef + stdDevMagRef;

	// Find global peak
	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(type > TYPE_CONTROL || type == TYPE_WATERMARK)
		{
			blocksize = GetBlockFreqSize(Signal, block, CHANNEL_LEFT, config);
			for(long int i = 0; i < blocksize; i++)
			{
				if(!Signal->Blocks[block].freq[i].hertz)
					break;
				if(threshold < Signal->Blocks[block].freq[i].magnitude && 
					Signal->Blocks[block].freq[i].magnitude > MaxMag[*size-1].magnitude)
				{
					for(long int j = *size - 1; j > 0; j--)
						MaxMag[j] = MaxMag[j - 1];

					MaxMag[0].magnitude = Signal->Blocks[block].freq[i].magnitude;
					MaxMag[0].hertz = Signal->Blocks[block].freq[i].hertz;
					MaxMag[0].block = block;
					MaxMag[0].channel = CHANNEL_LEFT;

					bubbleSortMagnitudes(MaxMag, *size);
				}
			}

			if(Signal->Blocks[block].freqRight)
			{
				blocksize = GetBlockFreqSize(Signal, block, CHANNEL_RIGHT, config);
				for(long int i = 0; i < blocksize; i++)
				{
					if(!Signal->Blocks[block].freqRight[i].hertz)
						break;
					if(threshold < Signal->Blocks[block].freqRight[i].magnitude && 
						Signal->Blocks[block].freqRight[i].magnitude > MaxMag[*size-1].magnitude)
					{
						for(int j = *size - 1; j > 0; j--)
							MaxMag[j] = MaxMag[j - 1];

						MaxMag[0].magnitude = Signal->Blocks[block].freqRight[i].magnitude;
						MaxMag[0].hertz = Signal->Blocks[block].freqRight[i].hertz;
						MaxMag[0].block = block;
						MaxMag[0].channel = CHANNEL_RIGHT;

						bubbleSortMagnitudes(MaxMag, *size);
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
						getRoleText(Signal), j,
						GetBlockName(config, MaxMag[j].block), GetBlockSubIndex(config, MaxMag[j].block),
						MaxMag[j].block, MaxMag[j].hertz, MaxMag[j].magnitude);
			}
		}
	}
	*/

	for(int i = 0; i < *size; i++)
	{
		if(MaxMag[i].block == -1)
		{
			*size = i;
			break;
		}
	}
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
				if(config->verbose >= 2) {
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

/* Search window around the Reference peak, as a fraction of its frequency */
#define PEAK_NEAR_PCT	0.05

double FindPeakNearInBlock(AudioSignal *Signal, MaxMagn refMax, double *peakHertz, parameters *config)
{
	double		highest = 0, window = 0, binSize = 0;
	Frequency	*freq = NULL;

	if(!Signal || refMax.block < 0)
		return 0;

	if(refMax.channel == CHANNEL_LEFT)
		freq = Signal->Blocks[refMax.block].freq;
	if(refMax.channel == CHANNEL_RIGHT)
		freq = Signal->Blocks[refMax.block].freqRight;
	if(!freq)
		return 0;

	window = refMax.hertz*PEAK_NEAR_PCT;
	binSize = FindFrequencyBinSizeForBlock(Signal, refMax.block);
	if(window < 5*binSize)
		window = 5*binSize;

	for(int i = 0; i < config->MaxFreq; i++)
	{
		if(!freq[i].hertz)
			break;
		if(fabs(refMax.hertz - freq[i].hertz) <= window && freq[i].magnitude > highest)
		{
			highest = freq[i].magnitude;
			if(peakHertz)
				*peakHertz = freq[i].hertz;
		}
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
				getRoleText(Signal),
				AvgFundMag, count);
	}

	return AvgFundMag;
}

double FindFundamentalMagnitudeStdDev(AudioSignal *Signal, double AvgFundMag, parameters *config)
{
	double		stdDevFundMag = 0;
	long int	count = 0;

	if(!Signal)
		return 0;

	if(AvgFundMag == 0)
		return 0;

	// Find global peak
	for(int block = 0; block < config->types.totalBlocks; block++)
	{
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(type > TYPE_CONTROL && Signal->Blocks[block].freq[0].hertz != 0)
		{
			stdDevFundMag += pow(Signal->Blocks[block].freq[0].magnitude - AvgFundMag, 2);
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
				stdDevFundMag += pow(Signal->Blocks[block].freqRight[0].magnitude - AvgFundMag, 2);
				count ++;
			}
		}
	}

	if(count)
		stdDevFundMag = sqrt(stdDevFundMag / (count - 1));

	if(config->verbose) {
		logmsg(" - %s signal Standard Deviation Fundamental Magnitude %g from %ld elements\n",
				getRoleText(Signal),
				stdDevFundMag, count);
	}

	return stdDevFundMag;
}
