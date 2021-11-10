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

#include "plot.h"
#include "log.h"
#include "freq.h"
#include "diff.h"
#include "cline.h"
#include "windows.h"
#include "profile.h"

#define SORT_NAME AmplitudeDifferences
#define SORT_TYPE FlatAmplDifference
#define SORT_CMP(x, y)  ((x).refAmplitude < (y).refAmplitude ? -1 : ((x).refAmplitude == (y).refAmplitude ? 0 : 1))
#include "sort.h"  // https://github.com/swenson/sort/

#define SORT_NAME AverageByFrequency
#define SORT_TYPE AveragedFrequencies
#define SORT_CMP(x, y)  ((x).avgfreq < (y).avgfreq ? -1 : ((x).avgfreq == (y).avgfreq ? 0 : 1))
#include "sort.h"  // https://github.com/swenson/sort/

#define SORT_NAME FlatFrequenciesByAmplitude
#define SORT_TYPE FlatFrequency
#define SORT_CMP(x, y)  ((x).amplitude < (y).amplitude ? -1 : ((x).amplitude == (y).amplitude ? 0 : 1))
#include "sort.h"  // https://github.com/swenson/sort/

#define SORT_NAME PhaseDifferencesByFrequency
#define SORT_TYPE FlatPhase
#define SORT_CMP(x, y)  ((x).hertz < (y).hertz ? -1 : ((x).hertz == (y).hertz ? 0 : 1))
#include "sort.h"  // https://github.com/swenson/sort/

#define SORT_NAME AmplitudeDifferencesBlock
#define SORT_TYPE AmplDifference
#define SORT_CMP(x, y)  ((x).diffAmplitude < (y).diffAmplitude ? -1 : ((x).diffAmplitude == (y).diffAmplitude ? 0 : 1))
#include "sort.h"  // https://github.com/swenson/sort/

#define DIFFERENCE_TITLE			"DIFFERENT AMPLITUDES [%s]"
#define DIFFERENCE_TITLE_LEFT		"DIFFERENT AMPLITUDES LEFT CHANNEL [%s]"
#define DIFFERENCE_TITLE_RIGHT		"DIFFERENT AMPLITUDES RIGHT CHANNEL [%s]"
#define EXTRA_TITLE_TS_REF			"Comparison - MISSING FREQUENCIES - Time Spectrogram [%s] (Expected in Comparison)"
#define EXTRA_TITLE_TS_REF_LEFT		"Comparison - MISSING FREQUENCIES LEFT CHANNEL - Time Spectrogram [%s] (Expected in Comparison)"
#define EXTRA_TITLE_TS_REF_RIGHT	"Comparison - MISSING FREQUENCIES RIGHT CHANNEL - Time Spectrogram [%s] (Expected in Comparison)"
#define EXTRA_TITLE_TS_COM			"Comparison - EXTRA FREQUENCIES - Time Spectrogram [%s] (Not in Reference)"
#define EXTRA_TITLE_TS_COM_LEFT		"Comparison - EXTRA FREQUENCIES LEFT CHANNEL - Time Spectrogram [%s] (Not in Reference)"
#define EXTRA_TITLE_TS_COM_RIGHT	"Comparison - EXTRA FREQUENCIES RIGHT CHANNEL - Time Spectrogram [%s] (Not in Reference)"
#define SPECTROGRAM_TITLE_REF		"Reference - SPECTROGRAM [%s]"
#define SPECTROGRAM_TITLE_REF_LEFT	"Reference - SPECTROGRAM LEFT CHANNEL [%s]"
#define SPECTROGRAM_TITLE_REF_RIGHT	"Reference - SPECTROGRAM RIGHT CHANNEL [%s]"
#define SPECTROGRAM_TITLE_COM		"Comparison - SPECTROGRAM [%s]"
#define SPECTROGRAM_TITLE_COM_LEFT	"Comparison - SPECTROGRAM LEFT CHANNEL [%s]"
#define SPECTROGRAM_TITLE_COM_RIGHT	"Comparison - SPECTROGRAM RIGHT CHANNEL [%s]"
#define TSPECTROGRAM_TITLE_REF		"Reference - TIME SPECTROGRAM [%s]"
#define TSPECTROGRAM_TITLE_REF_LFT	"Reference - TIME SPECTROGRAM LEFT CHANNEL [%s]"
#define TSPECTROGRAM_TITLE_REF_RGHT	"Reference - TIME SPECTROGRAM RIGHT CHANNEL [%s]"
#define TSPECTROGRAM_TITLE_COM		"Comparison - TIME SPECTROGRAM [%s]"
#define TSPECTROGRAM_TITLE_COM_LFT	"Comparison - TIME SPECTROGRAM LEFT CHANNEL [%s]"
#define TSPECTROGRAM_TITLE_COM_RGHT	"Comparison - TIME SPECTROGRAM RIGHT CHANNEL [%s]"
#define DIFFERENCE_AVG_TITLE		"DIFFERENT AMPLITUDES AVERAGED [%s]"
#define DIFFERENCE_AVG_TITLE_STEREO	"DIFFERENT AMPLITUDES STEREO AVERAGED  [%s]"
#define DIFFERENCE_AVG_TITLE_LEFT	"DIFFERENT AMPLITUDES LEFT CHANNEL AVERAGED [%s]"
#define DIFFERENCE_AVG_TITLE_RIGHT	"DIFFERENT AMPLITUDES RIGHT CHANNEL AVERAGED [%s]"
#define NOISE_TITLE					"NOISE FLOOR AVERAGED"
#define NOISE_AVG_TITLE				"NOISE FLOOR AVERAGED"
#define SPECTROGRAM_NOISE_REF		"Reference NOISE FLOOR - Spectrogram [%s]"
#define SPECTROGRAM_NOISE_REF_LEFT	"Reference NOISE FLOOR LEFT - Spectrogram [%s]"
#define SPECTROGRAM_NOISE_REF_RIGHT	"Reference NOISE FLOOR RIGHT - Spectrogram [%s]"
#define SPECTROGRAM_NOISE_COM		"Comparison NOISE FLOOR - Spectrogram [%s]"
#define SPECTROGRAM_NOISE_COM_LEFT	"Comparison NOISE FLOOR LEFT - Spectrogram [%s]"
#define SPECTROGRAM_NOISE_COM_RIGHT	"Comparison NOISE FLOOR RIGHT - Spectrogram [%s]"
#define WAVEFORM_TITLE_REF			"Reference - WAVEFORM [%s]"
#define WAVEFORM_TITLE_COM			"Comparison - WAVEFORM [%s]"
#define PHASE_DIFF_TITLE			"PHASE DIFFERENCE [%s]"
#define PHASE_DIFF_TITLE_LEFT		"PHASE DIFFERENCE LEFT CHANNEL [%s]"
#define PHASE_DIFF_TITLE_RIGHT		"PHASE DIFFERENCE RIGHT CHANNEL [%s]"
#define PHASE_SIG_TITLE_REF			"Reference - PHASE [%s]"
#define PHASE_SIG_TITLE_REF_LEFT	"Reference - PHASE LEFT CHANNEL [%s]"
#define PHASE_SIG_TITLE_REF_RIGHT	"Reference - PHASE RIGHT CHANNEL [%s]"
#define PHASE_SIG_TITLE_COM			"Comparison - PHASE [%s]"
#define PHASE_SIG_TITLE_COM_LEFT	"Comparison - PHASE LEFT CHANNEL [%s]"
#define PHASE_SIG_TITLE_COM_RIGHT	"Comparison - PHASE RIGHT CHANNEL [%s]"
#define TS_DIFFERENCE_TITLE			"DIFFERENT AMPLITUDES - TIME SPECTROGRAM [%s]"
#define SPECTROGRAM_CLK_REF			"Reference CLK - Spectrogram [%s]"
#define SPECTROGRAM_CLK_COM			"Comparison CLK - Spectrogram [%s]"

#define BAR_HEADER					"Matched frequencies"
#define BAR_DIFF					"w/any amplitude difference"
#define BAR_WITHIN					"within [0 to \\+-%gdB]"
#define BAR_WITHIN_PERFECT			"within (0 to \\+-%gdB]"
#define BAR_PERFECT					"Perfect Matches"

#define ALL_LABEL					"ALL"

#define BAR_WIDTH		config->plotResX/40
#define	BAR_HEIGHT		config->plotResY/60

#define	VERT_SCALE_STEP			3
#define	VERT_SCALE_STEP_BAR		3

#define	COLOR_BARS_WIDTH_SCALE	220

#define	DIFFERENCE_FOLDER	"Difference"
#define	SPECTROGRAM_FOLDER	"Spectrograms"
#define	WAVEFORM_FOLDER		"Waveforms"
#define	PHASE_FOLDER		"Phase"
#define	MISSING_FOLDER		"MissingAndExtra"
#define	WAVEFORMDIFF_FOLDER	"Waveform-Diff"
#define	WAVEFORMDIR_AMPL	"Amplitudes"
#define	WAVEFORMDIR_MISS	"Missing"
#define	WAVEFORMDIR_EXTRA	"Extra"
#define	T_SPECTR_FOLDER		"TimeSpectrograms"
#define	NOISEFLOOR_FOLDER	"NoiseFloor"
#define	CLK_FOLDER			"CLK"

//#define TESTWARNINGS
#define SYNC_DEBUG_SCALE	2

char *GetCurrentPathAndChangeToResultsFolder(parameters *config)
{
	char 	*CurrentPath = NULL;

	CurrentPath = (char*)malloc(sizeof(char)*FILENAME_MAX);
	if(!CurrentPath)
		return NULL;

	if(!GetCurrentDir(CurrentPath, sizeof(char)*FILENAME_MAX))
	{
		free(CurrentPath);
		logmsg("Could not get current path\n");
		return NULL;
	}

	if(chdir(config->folderName) == -1)
	{
		free(CurrentPath);
		logmsg("Could not open folder %s for results\n", config->folderName);
		return NULL;
	}
	return CurrentPath;
}

void ReturnToMainPath(char **CurrentPath)
{
	if(!*CurrentPath)
		return;

	if(chdir(*CurrentPath) == -1)
		logmsg("Could not open working folder %s\n", CurrentPath);

	free(*CurrentPath);
	*CurrentPath = NULL;
}

void StartPlot(char *name, struct timespec* start, parameters *config)
{
	logmsg(name);
	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, start);
}

void EndPlot(char *name, struct timespec* start, struct timespec* end, parameters *config)
{
	logmsg("\n");

	if(config->clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, end);
		elapsedSeconds = TimeSpecToSeconds(end) - TimeSpecToSeconds(start);
		logmsg(" - clk: %s took %0.2fs\n", name, elapsedSeconds);
	}
}

char *PushFolder(char *name)
{
	char 	*CurrentPath = NULL;

	CurrentPath = (char*)malloc(sizeof(char)*FILENAME_MAX);
	if(!CurrentPath)
		return NULL;

	if(!GetCurrentDir(CurrentPath, sizeof(char)*FILENAME_MAX))
	{
		free(CurrentPath);
		logmsg("ERROR: Could not get current path\n");
		return NULL;
	}

	if(!CreateFolder(name))
	{
		free(CurrentPath);
		logmsg("ERROR: Could not create %s subfolder\n", name);
		return NULL;
	}
	if(chdir(name) == -1)
	{
		free(CurrentPath);
		logmsg("ERROR: Could not open folder %s for results\n", name);
		return NULL;
	}
	return CurrentPath;
}

void PlotResults(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config)
{
	struct	timespec	start, end;
	char 	*CurrentPath = NULL, *MainPath = NULL;

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	MainPath = PushMainPath(config);
	CurrentPath = GetCurrentPathAndChangeToResultsFolder(config);

	if(config->plotDifferences || config->averagePlot)
	{
		struct	timespec lstart, lend;

		StartPlot(" - Difference", &lstart, config);
		PlotAmpDifferences(config);
		//PlotDifferenceTimeSpectrogram(config);
		EndPlot("Differences", &lstart, &lend, config);

		printf(" - Preliminary results in %s%s\n",
				config->outputPath,
				config->folderName);
	}

	if(config->plotMissing)
	{
		if(!config->FullTimeSpectroScale)
		{
			struct	timespec	lstart, lend;
	
			StartPlot(" - Missing and Extra Frequencies", &lstart, config);
	
			if(config->usesStereo)
			{
				char	*returnFolder = NULL;
			
				returnFolder = PushFolder(MISSING_FOLDER);
				if(!returnFolder)
					return;

#pragma omp parallel for
				for (int i = 0; i < 2; i++)
				{
					if (i == 0 && ReferenceSignal->AudioChannels == 2)
					{
						PlotTimeSpectrogramUnMatchedContent(ReferenceSignal, CHANNEL_LEFT, config);
						logmsg(PLOT_ADVANCE_CHAR);
						PlotTimeSpectrogramUnMatchedContent(ReferenceSignal, CHANNEL_RIGHT, config);
						logmsg(PLOT_ADVANCE_CHAR);
					}

					if (i == 1 && ComparisonSignal->AudioChannels == 2)
					{
						PlotTimeSpectrogramUnMatchedContent(ComparisonSignal, CHANNEL_LEFT, config);
						logmsg(PLOT_ADVANCE_CHAR);
						PlotTimeSpectrogramUnMatchedContent(ComparisonSignal, CHANNEL_RIGHT, config);
						logmsg(PLOT_ADVANCE_CHAR);
					}
				}

				ReturnToMainPath(&returnFolder);
			}

#pragma omp parallel for
			for (int i = 0; i < 2; i++)
			{
				if (i == 0)
				{
					PlotTimeSpectrogramUnMatchedContent(ReferenceSignal, CHANNEL_STEREO, config);
					logmsg(PLOT_ADVANCE_CHAR);
				}
				else
				{
					PlotTimeSpectrogramUnMatchedContent(ComparisonSignal, CHANNEL_STEREO, config);
					logmsg(PLOT_ADVANCE_CHAR);
				}
			}
	
			EndPlot("Missing and Extra", &lstart, &lend, config);
		}
		else
			logmsg(" X Skipped: Missing and Extra Frequencies, due to range\n");
	}

	if(config->plotSpectrogram)
	{
		int					typeCount = 0, doRefAll = 0, doCompAll = 0;
		struct	timespec	lstart, lend;
		char				*returnFolder = NULL;
		char				tmpNameRef[BUFFER_SIZE/2], tmpNameComp[BUFFER_SIZE/2];
		long int			sizeRef = 0, sizeComp = 0;
		FlatFrequency		*freqsRef = NULL, *freqsComp = NULL;

		StartPlot(" - Spectrograms", &lstart, config);
#pragma omp parallel for
		for(int i = 0; i < 2; i++)
		{
			if(i == 0)
				freqsRef = CreateSpectrogramFrequencies(ReferenceSignal, &sizeRef, config);
			else
				freqsComp = CreateSpectrogramFrequencies(ComparisonSignal, &sizeComp, config);
		}

		//Create subfolder if needed
		typeCount = GetActiveBlockTypesNoRepeat(config);
		if (typeCount > 1)
		{
			returnFolder = PushFolder(SPECTROGRAM_FOLDER);
			if (!returnFolder)
				return;
		}

		ShortenFileName(basename(ReferenceSignal->SourceFile), tmpNameRef);
		ShortenFileName(basename(ComparisonSignal->SourceFile), tmpNameComp);
#pragma omp parallel for
		for(int i = 0; i < 2; i++)
		{
			if(i == 0)
			{
				if(PlotEachTypeSpectrogram(freqsRef, sizeRef, tmpNameRef, ReferenceSignal->role, config, ReferenceSignal) > 1)
					doRefAll = 1;
			}
			else
			{
				if(PlotEachTypeSpectrogram(freqsComp, sizeComp, tmpNameComp, ComparisonSignal->role, config, ComparisonSignal) > 1)
					doCompAll = 1;
			}
		}

		if(typeCount > 1)
		{
			ReturnToMainPath(&returnFolder);
			returnFolder = NULL;
		}

		if(doRefAll && doCompAll)
		{
#pragma omp parallel for
			for(int i = 0; i < 2; i++)
			{
				if(i == 0)
				{
					PlotAllSpectrogram(freqsRef, sizeRef, tmpNameRef, ReferenceSignal->role, config);
					logmsg(PLOT_ADVANCE_CHAR);
				}
				else
				{
					PlotAllSpectrogram(freqsComp, sizeComp, tmpNameComp, ComparisonSignal->role, config);
					logmsg(PLOT_ADVANCE_CHAR);				
				}
			}
		}

		if(config->plotNoiseFloor)
		{
			PlotNoiseFloorSpectrogram(freqsRef, sizeRef, tmpNameRef, ReferenceSignal->role, config, ReferenceSignal);
			logmsg(PLOT_ADVANCE_CHAR);
			PlotNoiseFloorSpectrogram(freqsComp, sizeComp, tmpNameComp, ComparisonSignal->role, config, ComparisonSignal);
			logmsg(PLOT_ADVANCE_CHAR);
		}

		if(freqsRef)
			free(freqsRef);

		if(freqsComp)
			free(freqsComp);
		
		EndPlot("Spectrogram", &lstart, &lend, config);
	}

	if(config->clkMeasure)
	{
		struct	timespec lstart, lend;
		char	*returnFolder = NULL;

		StartPlot(" - Clocks", &lstart, config);
	
		returnFolder = PushFolder(CLK_FOLDER);
		if(!returnFolder)
			return;

#pragma omp parallel for
		for (int i = 0; i < 2; i++)
		{
			if (i == 0)
				PlotCLKSpectrogram(ReferenceSignal, config);
			else
				PlotCLKSpectrogram(ComparisonSignal, config);
		}

		ReturnToMainPath(&returnFolder);

		EndPlot("Clocks", &lstart, &lend, config);
	}

	if(config->plotTimeSpectrogram)
	{
		struct	timespec	lstart, lend;
		char*	returnFolder = NULL;

		StartPlot(" - Time Spectrogram", &lstart, config);

		if(config->usesStereo)
		{
			returnFolder = PushFolder(T_SPECTR_FOLDER);
			if(!returnFolder)
				return;

#pragma omp parallel for
			for (int i = 0; i < 2; i++)
			{
				if (i == 0)
				{
					if (ReferenceSignal->AudioChannels == 2)
					{
						PlotTimeSpectrogram(ReferenceSignal, CHANNEL_LEFT, config);
						logmsg(PLOT_ADVANCE_CHAR);
						PlotTimeSpectrogram(ReferenceSignal, CHANNEL_RIGHT, config);
						logmsg(PLOT_ADVANCE_CHAR);
					}
				}
				else
				{
					if (ComparisonSignal->AudioChannels == 2)
					{
						PlotTimeSpectrogram(ComparisonSignal, CHANNEL_LEFT, config);
						logmsg(PLOT_ADVANCE_CHAR);
						PlotTimeSpectrogram(ComparisonSignal, CHANNEL_RIGHT, config);
						logmsg(PLOT_ADVANCE_CHAR);
					}
				}
			}
			ReturnToMainPath(&returnFolder);
		}

		if (GetActiveBlockTypesNoRepeat(config))
		{
			returnFolder = PushFolder(T_SPECTR_FOLDER);
			if (!returnFolder)
				return;
#pragma omp parallel for
			for (int i = 0; i < config->types.typeCount; i++)
			{
				int type = 0;
				
				type = config->types.typeArray[i].type;
				if (type > TYPE_CONTROL && !config->types.typeArray[i].IsaddOnData)
				{
					PlotSingleTypeTimeSpectrogram(ReferenceSignal, CHANNEL_STEREO, type, config);
					logmsg(PLOT_ADVANCE_CHAR);
					
					PlotSingleTypeTimeSpectrogram(ComparisonSignal, CHANNEL_STEREO, type, config);
					logmsg(PLOT_ADVANCE_CHAR);
				}
			}
			ReturnToMainPath(&returnFolder);
		}

#pragma omp parallel for
		for (int i = 0; i < 2; i++)
		{
			if (i == 0)
			{
				PlotTimeSpectrogram(ReferenceSignal, CHANNEL_STEREO, config);
				logmsg(PLOT_ADVANCE_CHAR);
			}
			else
			{
				PlotTimeSpectrogram(ComparisonSignal, CHANNEL_STEREO, config);
				logmsg(PLOT_ADVANCE_CHAR);
			}
		}
		EndPlot("Time Spectrogram", &lstart, &lend, config);
	}

	if(config->plotPhase)
	{
		struct	timespec	lstart, lend;

		StartPlot(" - Phase", &lstart, config);

		PlotPhaseDifferences(config);
		//PlotPhaseFromSignal(ReferenceSignal, config);
		//PlotPhaseFromSignal(ComparisonSignal, config);
		
		logmsg(PLOT_ADVANCE_CHAR);
		EndPlot("Phase", &lstart, &lend, config);
	}

	if(config->plotNoiseFloor)
	{
		if(!config->noSyncProfile)
		{
			if(ReferenceSignal->hasSilenceBlock && ComparisonSignal->hasSilenceBlock)
			{
				struct	timespec	lstart, lend;
		
				StartPlot(" - Noise Floor", &lstart, config);

				PlotNoiseFloor(ReferenceSignal, config);
				EndPlot("Noise Floor", &lstart, &lend, config);
			}
			else
				logmsg(" X Noise Floor graphs ommited: no noise floor value found.\n");
		}
		else
			logmsg(" X Noise floor plots make no sense with current parameters.\n");
	}

	if((config->hasTimeDomain && config->plotTimeDomain) || config->plotAllNotes)
	{
		struct	timespec	lstart, lend;
		char 				*returnFolder = NULL;

		returnFolder = PushFolder(WAVEFORM_FOLDER);
		if(!returnFolder)
		{
			ReturnToMainPath(&CurrentPath);
			return;
		}

		StartPlot(" - Waveform Graphs\n  ", &lstart, config);
#pragma omp parallel for
		for (int i = 0; i < 2; i++)
		{
			if (i == 0)
				PlotTimeDomainGraphs(ReferenceSignal, config);
			else
				PlotTimeDomainGraphs(ComparisonSignal, config);
		}
		EndPlot("Waveform", &lstart, &lend, config);

		ReturnToMainPath(&returnFolder);
	}

	if(config->plotTimeDomainHiDiff)
	{
		if(FindDifferenceAveragesperBlock(config->thresholdAmplitudeHiDif, config->thresholdMissingHiDif, config->thresholdExtraHiDif, config))
		{
			struct	timespec	lstart, lend;

			StartPlot(" - Time Domain Graphs from highly different notes\n  ", &lstart, config);
#pragma omp parallel for
			for (int i = 0; i < 2; i++)
			{
				if (i == 0)
					PlotTimeDomainHighDifferenceGraphs(ReferenceSignal, config);
				else
					PlotTimeDomainHighDifferenceGraphs(ComparisonSignal, config);
			}
			EndPlot("Time Domain Graphs", &lstart, &lend, config);
		}
	}

	ReturnToMainPath(&CurrentPath);
	PopMainPath(&MainPath);

	if(config->clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - clk: Plotting PNGs took %0.2fs\n", elapsedSeconds);
	}
}

void PlotAmpDifferences(parameters *config)
{
	long int			size = 0;
	FlatAmplDifference	*amplDiff = NULL;
	
	amplDiff = CreateFlatDifferences(config, &size, normalPlot);
	if(!amplDiff)
	{
		logmsg("Not enough memory for plotting\n");
		return;
	}

	if(config->outputCSV)
		SaveCSVAmpDiff(amplDiff, size, config->compareName, config);

	if(config->plotDifferences)
	{
		int typeCount = 0, plotAll = 0;

		typeCount = GetActiveBlockTypesNoRepeat(config);
		if (typeCount > 1)
		{
			if (PlotEachTypeDifferentAmplitudes(amplDiff, size, config->compareName, config) > 1)
				plotAll = 1;
		}
		else
			plotAll = 1;

		if (plotAll)
		{
			PlotAllDifferentAmplitudes(amplDiff, size, CHANNEL_STEREO, config->compareName, config);
			if(config->channelBalance == 0 && config->referenceSignal->AudioChannels == 2 && config->comparisonSignal->AudioChannels == 2)
			{
				char		name[2*BUFFER_SIZE];
				char		*returnFolder = NULL;

				returnFolder = PushFolder(DIFFERENCE_FOLDER);
				if (!returnFolder)
					return;

				sprintf(name, "%s_%c", config->compareName, CHANNEL_LEFT);
				PlotAllDifferentAmplitudes(amplDiff, size, CHANNEL_LEFT, name, config);
				logmsg(PLOT_ADVANCE_CHAR);

				sprintf(name, "%s_%c", config->compareName, CHANNEL_RIGHT);
				PlotAllDifferentAmplitudes(amplDiff, size, CHANNEL_RIGHT, name, config);
				logmsg(PLOT_ADVANCE_CHAR);

				ReturnToMainPath(&returnFolder);
			}

			logmsg(PLOT_ADVANCE_CHAR);
		}
	}

	if(config->averagePlot)
		PlotDifferentAmplitudesAveraged(amplDiff, size, config->compareName, config);
	
	free(amplDiff);
	amplDiff = NULL;
}

void PlotDifferentAmplitudesWithBetaFunctions(parameters *config)
{
	long int 			size = 0;
	FlatAmplDifference	*amplDiff = NULL;
	
	amplDiff = CreateFlatDifferences(config, &size, normalPlot);
	if(!amplDiff)
	{
		logmsg("Not enough memory for plotting\n");
		return;
	}

	for(int o = 0; o < 6; o++)
	{
		config->outputFilterFunction = o;
		PlotAllDifferentAmplitudes(amplDiff, size, CHANNEL_STEREO, config->compareName, config);
	}

	free(amplDiff);
	amplDiff = NULL;
}

/*
void PlotFreqMissing(parameters *config)
{
	long int			size = 0;
	FlatFrequency	*freqDiff = NULL;
	
	freqDiff = CreateFlatMissing(config, &size);
	if(PlotEachTypeMissingFrequencies(freqDiff, size, config->compareName, config) > 1)
	{
		PlotAllMissingFrequencies(freqDiff, size, config->compareName, config);
		logmsg(PLOT_ADVANCE_CHAR);
	}

	free(freqDiff);
	freqDiff = NULL;
}
*/

FlatFrequency *CreateSpectrogramFrequencies(AudioSignal *Signal, long int *size, parameters *config)
{
	FlatFrequency		*frequencies = NULL;

	if(!size)
		return NULL;

	*size = 0;
	if (config->clock)
	{
		struct timespec start, end;
		double	elapsedSeconds;

		clock_gettime(CLOCK_MONOTONIC, &start);
		frequencies = CreateFlatFrequencies(Signal, size, config);
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - clk: %s took %0.2fs\n", "CreateFlatFrequencies", elapsedSeconds);
	}
	else
		frequencies = CreateFlatFrequencies(Signal, size, config);

	return(frequencies);
}

void PlotNoiseFloor(AudioSignal *Signal, parameters *config)
{
	long int 			size = 0;
	FlatAmplDifference	*amplDiff = NULL;
	
	amplDiff = CreateFlatDifferences(config, &size, floorPlot);
	if(!amplDiff)
	{
		logmsg("Not enough memory for plotting\n");
		return;
	}

	//PlotNoiseDifferentAmplitudes(amplDiff, size, config->compareName, config, Signal);
	
	//if(config->averagePlot)
	PlotNoiseDifferentAmplitudesAveraged(amplDiff, size, config->compareName, config, Signal);
	
	free(amplDiff);
	amplDiff = NULL;
}


int FillPlotExtra(PlotFile *plot, char *name, int sizex, int sizey, double x0, double y0, double x1, double y1, double penWidth, double leftMarginSize, parameters *config)
{
	int rt = 0;

	if(!plot)
		return 0;

	rt = FillPlot(plot, name, x0, y0, x1, y1, penWidth, leftMarginSize, config);
	plot->sizex = sizex;
	plot->sizey = sizey;
	return(rt);
}

int FillPlot(PlotFile *plot, char *name, double x0, double y0, double x1, double y1, double penWidth, double leftMarginSize, parameters *config)
{
	double dX = 0, dY = 0;

	if(!plot)
		return 0;

	plot->plotter = NULL;
	plot->plotter_params = NULL;
	plot->file = NULL;

	ComposeFileNameoPath(plot->FileName, name, ".png", config);

	plot->sizex = config->plotResX;
	plot->sizey = config->plotResY;

	plot->Rx0 = x0;
	plot->Rx1 = x1;
	plot->Ry0 = y0;
	plot->Ry1 = y1;

	plot->leftmargin = leftMarginSize;

	dX = X0BORDER*fabs(x0 - x1)*leftMarginSize;
	dY = Y0BORDER*fabs(y0 - y1);

	plot->x0 = x0-dX;
	plot->y0 = y0-dY;

	dX = X1BORDER*fabs(x0 - x1);
	dY = Y1BORDER*fabs(y0 - y1);

	plot->x1 = x1+dX;
	plot->y1 = y1+dY;

	plot->penWidth = penWidth;
	plot->SpecialWarning = NULL;

	return 1;
}

int CreatePlotFile(PlotFile *plot, parameters *config)
{
	char		size[20];

	plot->file = fopen(plot->FileName, "wb");
	if(!plot->file)
	{
		logmsg("WARNING: Couldn't create graph file %s\n%s\n", plot->FileName, strerror(errno));
		return 0;
	}

	sprintf(size, "%dx%d", plot->sizex, plot->sizey);
	plot->plotter_params = pl_newplparams ();
	pl_setplparam (plot->plotter_params, "BITMAPSIZE", size);
	if((plot->plotter = pl_newpl_r("png", stdin, plot->file, stderr, plot->plotter_params)) == NULL)
	{
		logmsg("Couldn't create Plotter\n");
		return 0;
	}

	if(pl_openpl_r(plot->plotter) < 0)
	{
		logmsg("Couldn't open Plotter\n");
		return 0;
	}
	pl_fspace_r(plot->plotter, plot->x0, plot->y0, plot->x1, plot->y1);
	pl_flinewidth_r(plot->plotter, plot->penWidth);
	if(config->whiteBG)
		pl_bgcolor_r(plot->plotter, 0xffff, 0xffff, 0xffff);
	else
		pl_bgcolor_r(plot->plotter, 0, 0, 0);
	pl_erase_r(plot->plotter);

	/*
	SetPenColor(COLOR_GREEN, 0xffff, plot);	
	pl_fbox_r(plot->plotter, plot->Rx0,  plot->Ry0,  plot->Rx1,  plot->Ry1);
	pl_endsubpath_r(plot->plotter);
	*/

	return 1;
}

int ClosePlot(PlotFile *plot)
{
	if(pl_closepl_r(plot->plotter) < 0)
	{
		logmsg("Couldn't close Plotter\n");
		return 0;
	}
	
	if(pl_deletepl_r(plot->plotter) < 0)
	{
		logmsg("Couldn't delete Plotter\n");
		return 0;
	}
	plot->plotter = NULL;

	if(pl_deleteplparams(plot->plotter_params) < 0)
	{
		logmsg("Couldn't delete Plotter Params\n");
		return 0;
	}
	plot->plotter_params = NULL;

	fclose(plot->file);
	plot->file = NULL;

	return 1;
}

void DrawFrequencyHorizontal(PlotFile *plot, double vertical, double hz, double hzIncrement, parameters *config)
{
	pl_pencolor_r (plot->plotter, 0, 0x5555, 0);
	for(int i = hzIncrement; i < hz; i += hzIncrement)
	{
		pl_fline_r(plot->plotter, transformtoLog(i, config), -1*vertical, transformtoLog(i, config), vertical);
		pl_endpath_r(plot->plotter);
	}

	pl_pencolor_r (plot->plotter, 0, 0x7777, 0);
	if(config->logScale)
	{
		pl_fline_r(plot->plotter, transformtoLog(10, config), -1*vertical, transformtoLog(10, config), vertical);
		pl_endpath_r(plot->plotter);
		pl_fline_r(plot->plotter, transformtoLog(100, config), -1*vertical, transformtoLog(100, config), vertical);
		pl_endpath_r(plot->plotter);
	}
	pl_fline_r(plot->plotter, transformtoLog(1000, config), -1*vertical, transformtoLog(1000, config), vertical);
	pl_endpath_r(plot->plotter);
	if(config->endHzPlot >= 10000)
	{
		for(int i = 10000; i < config->endHzPlot; i+= 10000)
		{
			pl_fline_r(plot->plotter, transformtoLog(i, config), -1*vertical, transformtoLog(i, config), vertical);
			pl_endpath_r(plot->plotter);
		}
	}
}

void DrawGridZeroDBCentered(PlotFile *plot, double dBFS, double dbIncrement, double hz, double hzIncrement, parameters *config)
{
	if(fabs(dBFS) <= 1.0)
		dbIncrement = fabs(dBFS)/10.0;
	else
	{
		if(fabs(dBFS) <= 3.0)
			dbIncrement = 1.0;
	}

	if(config->maxDbPlotZC == DB_HEIGHT)
		pl_pencolor_r(plot->plotter, 0, 0xaaaa, 0);
	else
		pl_pencolor_r(plot->plotter, 0xaaaa, 0xaaaa, 0);
	pl_fline_r(plot->plotter, 0, 0, hz, 0);
	pl_endpath_r(plot->plotter);

	if(config->maxDbPlotZC == DB_HEIGHT)
		pl_pencolor_r(plot->plotter, 0, 0x5555, 0);
	else
		pl_pencolor_r(plot->plotter, 0x5555, 0x5555, 0);
	for(double i = dbIncrement; i < dBFS; i += dbIncrement)
	{
		pl_fline_r(plot->plotter, 0, i, hz, i);
		pl_fline_r(plot->plotter, 0, -1*i, hz, -1*i);
	}
	pl_endpath_r(plot->plotter);

	DrawFrequencyHorizontal(plot, dBFS, hz, hzIncrement, config);

	pl_endpath_r(plot->plotter);
	pl_pencolor_r (plot->plotter, 0, 0xFFFF, 0);
}

void DrawGridZeroToLimit(PlotFile *plot, double dBFS, double dbIncrement, double hz, double hzIncrement, int drawSignificant, parameters *config)
{
	pl_pencolor_r (plot->plotter, 0, 0x5555, 0);
	for(int i = dbIncrement; i < fabs(dBFS); i += dbIncrement)
		pl_fline_r(plot->plotter, 0, -1*i, hz, -1*i);

	pl_pencolor_r (plot->plotter, 0, 0x5555, 0);
	for(int i = hzIncrement; i < hz; i += hzIncrement)
		pl_fline_r(plot->plotter, transformtoLog(i, config), dBFS, transformtoLog(i, config), 0);

	if(drawSignificant)
	{
		pl_pencolor_r (plot->plotter, 0x9999, 0x9999, 0);
		//pl_linemod_r(plot->plotter, "dotdashed");
		pl_fline_r(plot->plotter, 0, config->significantAmplitude, hz, config->significantAmplitude);
		//pl_linemod_r(plot->plotter, "solid");
	}

	pl_pencolor_r (plot->plotter, 0, 0x7777, 0);
	if(config->logScale)
	{
		pl_fline_r(plot->plotter, transformtoLog(10, config), dBFS, transformtoLog(10, config), 0);
		pl_fline_r(plot->plotter, transformtoLog(100, config), dBFS, transformtoLog(100, config), 0);
	}
	pl_fline_r(plot->plotter, transformtoLog(1000, config), dBFS, transformtoLog(1000, config), 0);
	if(config->endHzPlot >= 10000)
	{
		for(int i = 10000; i < config->endHzPlot; i+= 10000)
			pl_fline_r(plot->plotter, transformtoLog(i, config), dBFS, transformtoLog(i, config), 0);	
	}

	pl_pencolor_r (plot->plotter, 0, 0xFFFF, 0);
	pl_flinewidth_r(plot->plotter, 1);
	pl_endpath_r(plot->plotter);
}

void DrawLabelsZeroDBCentered(PlotFile *plot, double dBFS, double dbIncrement, double hz, parameters *config)
{
	double segments = 0;
	char label[20];

	if(fabs(dBFS) <= 1.0)
		dbIncrement = fabs(dBFS)/10.0;
	else
	{
		if(fabs(dBFS) <= 3.0)
			dbIncrement = 1.0;
	}

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0-X0BORDER*config->plotResX*plot->leftmargin, -1*config->plotResY/2-Y0BORDER*config->plotResY, config->plotResX+X1BORDER*config->plotResX, config->plotResY/2+Y1BORDER*config->plotResY);

	pl_ffontname_r(plot->plotter, PLOT_FONT);
	pl_ffontsize_r(plot->plotter, FONT_SIZE_1);

	if(config->maxDbPlotZC == DB_HEIGHT)
		pl_pencolor_r(plot->plotter, 0, 0xffff, 0);
	else
		pl_pencolor_r(plot->plotter, 0xffff, 0xffff, 0);
	pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, config->plotResY/100);
	pl_alabel_r(plot->plotter, 'l', 't', "0dB");

	if(dBFS < config->lowestDBFS)
		dbIncrement *= 2;

	segments = fabs(dBFS/dbIncrement);
	for(int i = 1; i <= segments; i ++)
	{
		pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, i*config->plotResY/segments/2+config->plotResY/100);
		sprintf(label, " %gdB", i*dbIncrement);
		pl_alabel_r(plot->plotter, 'l', 't', label);

		pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, -1*i*config->plotResY/segments/2+config->plotResY/100);
		sprintf(label, "-%gdB", i*dbIncrement);
		pl_alabel_r(plot->plotter, 'l', 't', label);
	}

	/* Frequency scale */
	pl_pencolor_r(plot->plotter, 0, 0xaaaa, 0);
	if(config->logScale)
	{
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(10, config), config->plotResY/2);
		sprintf(label, "%dHz", 10);
		pl_alabel_r(plot->plotter, 'c', 'b', label);
	
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(100, config), config->plotResY/2);
		sprintf(label, "%dHz", 100);
		pl_alabel_r(plot->plotter, 'c', 'b', label);
	}

	pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(1000, config), config->plotResY/2);
	sprintf(label, "  %dHz", 1000);
	pl_alabel_r(plot->plotter, 'c', 'b', label);

	if(config->endHzPlot >= 10000)
	{
		for(int i = 10000; i < config->endHzPlot; i+= 10000)
		{
			pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(i, config), config->plotResY/2);
			sprintf(label, "%d%s", i/1000, i >= 40000  ? "" : "khz");
			pl_alabel_r(plot->plotter, 'c', 'b', label);
		}
	}

	pl_restorestate_r(plot->plotter);
}

#ifdef TESTWARNINGS
void enableTestWarnings(parameters *config)
{
	int i = 0;
	config->frequencyNormalizationTries = 5;
	config->frequencyNormalizationTolerant = 1.5;

	config->ignoreFrameRateDiff = 1;
	config->ignoreFloor = 1;
	config->noiseFloorTooHigh = 1;

	config->types.useWatermark = 1;
	config->referenceSignal->watermarkStatus = WATERMARK_VALID;
	config->comparisonSignal->watermarkStatus = WATERMARK_INVALID;
	sprintf(config->types.watermarkDisplayName, "Testing");

	config->AmpBarRange = 2;
	config->smallFile = ROLE_REF;

	config->normType = none;
	config->noSyncProfile = 1;
	config->syncTolerance = 1;

	config->hasAddOnData = 1;

	config->ZeroPad = 1;

	config->channelBalance = 0;
	config->referenceSignal->AudioChannels = 2;

	/*
	config->startHz = START_HZ+1;
	config->endHz = END_HZ-1;
	*/

	config->MaxFreq = FREQ_COUNT/2;

	config->logScale = 0;
	config->stereoNotFound = 1;

	config->outputFilterFunction = 1;

	config->compressToBlocks = 1;
	config->channelWithLowFundamentals = 1;

	config->maxDbPlotZC = DB_HEIGHT+3;
	config->maxDbPlotZCChanged = 1;
	config->notVisible = 20.75;

	config->clkMeasure = 1;
	config->clkBlock = 2;
	config->clkFreq = 8000;
	config->clkRatio = 4;
	config->doClkAdjust = 1;

	config->warningStereoReversed = 1;

	config->doSamplerateAdjust = 1;
	config->SRNoMatch = 1;
	config->referenceSignal->EstimatedSR = 32780;
	config->comparisonSignal->EstimatedSR = 34812.87;
		
	config->referenceSignal->balance = -5.7;
	config->comparisonSignal->balance = 10.48;

	config->referenceSignal->delayArray[0] = 176.25;
	config->referenceSignal->delayArray[1] = 17.7;
	config->referenceSignal->delayArray[2] = 4000.15;
	config->referenceSignal->delayElemCount = 3;

	config->comparisonSignal->delayArray[0] = 5254.25;
	config->comparisonSignal->delayArray[1] = 45.5;
	config->comparisonSignal->delayArray[2] = 4012.1;
	config->comparisonSignal->delayElemCount = 3;

	config->referenceSignal->EstimatedSR = 192080;
	config->comparisonSignal->EstimatedSR = 48212;

	config->referenceSignal->originalSR = 192000;
	config->comparisonSignal->originalSR = 48000;

	config->internalSyncTolerance = ROLE_COMP;

	for(i = 0; i < 4; i++)
		syncAlignPct[i] = rand() % 100;
	syncAlignTolerance[3] = 1;

	logmsg("ERROR: enableTestWarnings Enabled\n");
}
#endif

#define XPOSWARN	3.5

#define PLOT_COLUMN(x,y) pl_fmove_r(plot->plotter, config->plotResX-(x)*config->plotResX/10, config->plotResY/2-(y)*BAR_HEIGHT)
#define PLOT_COLUMN_DISP(x,x1,y) pl_fmove_r(plot->plotter, config->plotResX-(x)*config->plotResX/10-config->plotResX/40+x1, config->plotResY/2-(y)*BAR_HEIGHT)

#define PLOT_WARN(x,y) pl_fmove_r(plot->plotter, x*config->plotResX-config->plotResX/XPOSWARN, -1*config->plotResY/2+config->plotResY/20+(y+2)*BAR_HEIGHT)
#define PLOT_WARN_XDISP(x,y,d) pl_fmove_r(plot->plotter, x*config->plotResX-config->plotResX/XPOSWARN+d, -1*config->plotResY/2+config->plotResY/20+(y+2)*BAR_HEIGHT)

void DrawSRData(PlotFile *plot, AudioSignal *Signal, char *msg, parameters *config)
{
	char str[100];

	msg[0] = '\0';
	
	if(!Signal->originalSR || !Signal->EstimatedSR)
		return;

	pl_pencolor_r(plot->plotter, 0xcccc, 0xcccc, 0);
	if(config->doSamplerateAdjust)
		sprintf(str, "\\ptSR %%s: %%d\\->%%gkHz");
	else
		sprintf(str, "\\ptSR %%s: %%d(%%g)kHz");
	sprintf(msg, str,
		Signal->role == ROLE_REF ? "RF" : "CM",
		Signal->originalSR/1000, Signal->EstimatedSR/1000.0);
}

void DrawClockData(PlotFile *plot, AudioSignal *Signal, char *msg, parameters *config)
{
	msg[0] = '\0';
	if(!config->clkMeasure)
		return;

	if(fabs(config->centsDifferenceCLK) >= MAX_CENTS_DIFF)
		pl_pencolor_r(plot->plotter, 0xcccc, 0xcccc, 0);
	else
		pl_pencolor_r(plot->plotter, 0, 0xcccc, 0xcccc);

	if(!Signal->originalCLK)
		sprintf(msg, "%s %s: %gHz",
			config->clkName,
			Signal->role == ROLE_REF ? "RF" : "CM",
			Signal->role == ROLE_REF ? config->clkRef : config->clkCom);
	else
	{
		char str[100];

		pl_pencolor_r(plot->plotter, 0xcccc, 0xcccc, 0);
		if(config->doClkAdjust)
			sprintf(str, "%s %%s: %%g\\->%%gHz", config->clkName);
		else
			sprintf(str, "%s %%s: %%g(%%g)Hz", config->clkName);
		sprintf(msg, str,
			Signal->role == ROLE_REF ? "RF" : "CM",
			Signal->originalCLK,
			config->changedCLKFrom == ROLE_REF ? config->clkCom : config->clkRef);
	}
}

void DrawImbalance(PlotFile *plot, AudioSignal *Signal, char *msg, parameters *config)
{
	if(Signal->AudioChannels == 1)
		return;

	if(config->channelBalance == -1)
	{
		pl_pencolor_r(plot->plotter, 0xcccc, 0xcccc, 0);
		pl_alabel_r(plot->plotter, 'l', 'l', "No mono in profile");
		return;
	}

	if(config->noBalance & Signal->role)
	{
		pl_pencolor_r(plot->plotter, 0xcccc, 0xcccc, 0);
		pl_alabel_r(plot->plotter, 'l', 'l', "Unmatched Mono");
		return;
	}

	if(fabs(Signal->balance) >= 10)
		pl_pencolor_r(plot->plotter, 0xcccc, 0xcccc, 0);
	else
		pl_pencolor_r(plot->plotter, 0, 0xcccc, 0xcccc);
	if(Signal->balance)
		sprintf(msg, "Imbalance %s %s: %0.2fdBFS", 
				Signal->role == ROLE_REF ? "RF" : "CM",
				Signal->balance > 0 ? "R" : "L", 
				fabs(Signal->balance));
	else
		sprintf(msg, "%s Stereo balanced", 
				Signal->role == ROLE_REF ? "RF" : "CM");
	pl_alabel_r(plot->plotter, 'l', 'l', msg);
}

void DrawFileInfo(PlotFile *plot, AudioSignal *Signal, char *msg, int type, int ypos, parameters *config)
{
	int		format = 0;
	char	*name = NULL;
	double	x = 0, y = 0;

	x = config->plotResX/2-config->plotResX/50*14;
	y = -1*config->plotResY/2+config->plotResY/80;

	if(!Signal)
		return;

	name = basename(Signal->role == ROLE_REF ? config->referenceFile : config->comparisonFile);
	format = Signal->role == ROLE_REF ? config->videoFormatRef : config->videoFormatCom;

	pl_pencolor_r(plot->plotter, 0, 0xeeee, 0);
	if(type == PLOT_COMPARE)
	{
		if(Signal)
		{
			sprintf(msg, "%s %5.5s %4dkHz %dbit %s %.92s%s",
				Signal->role == ROLE_REF ? "Reference:  " : "Comparison:",
				config->types.SyncFormat[format].syncName,
				Signal->originalSR ? Signal->originalSR/1000 : (int)Signal->header.fmt.SamplesPerSec/1000,
				Signal->bytesPerSample*8,
				Signal->AudioChannels == 2 ? "Stereo" : "Mono ",
				name,
				strlen(name) > 86 ? "\\.." : " ");
			pl_fmove_r(plot->plotter, x, y+config->plotResY/(ypos*40));
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
	
			if(Signal->originalFrameRate)
			{
				double labelwidth = 0;

				if(!config->doClkAdjust)
				{
					pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
					sprintf(msg, "[%0.4fms %0.4fHz]\\->", 
							Signal->originalFrameRate, roundFloat(CalculateScanRateOriginalFramerate(Signal)));
					labelwidth = pl_flabelwidth_r(plot->plotter, msg);
	
					sprintf(msg, "[%0.4fms %0.4fHz]\\->[%0.4fms %0.4fHz]", 
							Signal->originalFrameRate, roundFloat(CalculateScanRateOriginalFramerate(Signal)),
							Signal->framerate, roundFloat(CalculateScanRate(Signal)));
					pl_fmove_r(plot->plotter, config->plotResX/20*17-labelwidth, y+config->plotResY/(ypos*40));
				}
				else
				{
					pl_pencolor_r(plot->plotter, 0, 0xeeee, 0xeeee);
					sprintf(msg, "(%0.4fms %0.4fHz) ", 
						Signal->framerate, roundFloat(CalculateScanRate(Signal)));
					labelwidth = pl_flabelwidth_r(plot->plotter, msg);
					pl_fmove_r(plot->plotter, config->plotResX/20*17-labelwidth, y+config->plotResY/(ypos*40));
					pl_alabel_r(plot->plotter, 'l', 'l', msg);

					pl_pencolor_r(plot->plotter, 0, 0xeeee, 0);
					sprintf(msg, "[%0.4fms %0.4fHz]", 
						Signal->originalFrameRate, roundFloat(CalculateScanRateOriginalFramerate(Signal)));
					pl_fmove_r(plot->plotter, config->plotResX/20*17, y+config->plotResY/(ypos*40));
				}
			}
			else
			{
				sprintf(msg, "[%0.4fms %0.4fHz]", 
						Signal->framerate, roundFloat(CalculateScanRate(Signal)));
				pl_fmove_r(plot->plotter, config->plotResX/20*17, y+config->plotResY/(ypos*40));
			}
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}
		else
		{
			sprintf(msg, "%s:   %.92s", 
				Signal->role == ROLE_REF ? "Reference:  " : "Comparison:", name);
			pl_fmove_r(plot->plotter, x, y+config->plotResY/(ypos*40));
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}
	}

	if(type == PLOT_SINGLE_REF || type == PLOT_SINGLE_COM)
	{
		y += config->plotResY/60;

		sprintf(msg, "File: %5.5s %4dkHz %dbit %s %.92s%s",
			config->types.SyncFormat[format].syncName,
			Signal->originalSR ? Signal->originalSR/1000 : (int)Signal->header.fmt.SamplesPerSec/1000,
			Signal->bytesPerSample*8,
			Signal->AudioChannels == 2 ? "Stereo" : "Mono  ",
			name,
			strlen(name) > 86 ? "\\.." : " ");

		pl_fmove_r(plot->plotter, x, y);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);

		if(Signal->originalFrameRate)
		{
			double labelwidth = 0;

			pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
			sprintf(msg, "[%0.4fms %0.4fHz]\\->", 
					Signal->originalFrameRate, roundFloat(CalculateScanRateOriginalFramerate(Signal)));
			labelwidth = pl_flabelwidth_r(plot->plotter, msg);

			sprintf(msg, "[%0.4fms %0.4fHz]\\->[%0.4fms %0.4fHz]", 
					Signal->originalFrameRate, roundFloat(CalculateScanRateOriginalFramerate(Signal)),
					Signal->framerate, roundFloat(CalculateScanRate(Signal)));
			pl_fmove_r(plot->plotter, config->plotResX/20*17-labelwidth, y);
		}
		else
		{
			sprintf(msg, "[%0.4fms %0.4fHz]", Signal->framerate, roundFloat(CalculateScanRate(Signal)));
			pl_fmove_r(plot->plotter, config->plotResX/20*17, y);
		}
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}
}

void DrawLabelsMDF(PlotFile *plot, char *Gname, char *GType, int type, parameters *config)
{
	int		warning = 1;
	char	label[BUFFER_SIZE], msg[BUFFER_SIZE];
#ifdef TESTWARNINGS
	parameters backup;

	backup = *config;
	enableTestWarnings(config);
#endif

	pl_ffontsize_r(plot->plotter, FONT_SIZE_2);
	pl_ffontname_r(plot->plotter, PLOT_FONT);

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0, -1*config->plotResY/2, config->plotResX, config->plotResY/2);

	/* Profile */
	pl_fmove_r(plot->plotter, config->plotResX/40, config->plotResY/2-config->plotResY/30+BAR_HEIGHT/2);
	pl_pencolor_r(plot->plotter, 0xaaaa, 0xaaaa, 0xaaaa);
	pl_alabel_r(plot->plotter, 'l', 'l', config->types.Name);

	/* Plot Label */
	sprintf(label, Gname, GType);
	pl_fmove_r(plot->plotter, config->plotResX/40, config->plotResY/2-config->plotResY/30-BAR_HEIGHT);
	pl_pencolor_r(plot->plotter, 0xcccc, 0xcccc, 0xcccc);
	pl_alabel_r(plot->plotter, 'l', 'l', label);

	/* Version */
	pl_ffontsize_r(plot->plotter, FONT_SIZE_3);
	pl_fmove_r(plot->plotter, config->plotResX/60, -1*config->plotResY/2+config->plotResY/100);
	pl_pencolor_r(plot->plotter, 0, 0xcccc, 0);
	pl_alabel_r(plot->plotter, 'l', 'l', "MDFourier "MDVERSION" for 240p Test Suite by Artemio Urbina");
	pl_ffontsize_r(plot->plotter, FONT_SIZE_2);

	/* Window */
	pl_fmove_r(plot->plotter, config->plotResX/20*19, -1*config->plotResY/2+config->plotResY/80);
	pl_pencolor_r(plot->plotter, 0xffff, 0xffff, 0);
	switch(config->window)
	{
		case 'n':
			pl_alabel_r(plot->plotter, 'l', 'l', "Rectangle");
			break;
		case 't':
			pl_pencolor_r(plot->plotter, 0xaaaa, 0xaaaa, 0xaaaa);
			pl_alabel_r(plot->plotter, 'l', 'l', "Tukey");
			break;
		case 'f':
			pl_alabel_r(plot->plotter, 'l', 'l', "Flattop");
			break;
		case 'h':
			pl_alabel_r(plot->plotter, 'l', 'l', "Hann");
			break;
		case 'm':
			pl_alabel_r(plot->plotter, 'l', 'l', "Hamming");
			break;
		default:
			pl_alabel_r(plot->plotter, 'l', 'l', "UNKNOWN");
			break;
	}

	pl_pencolor_r(plot->plotter, 0xaaaa, 0xaaaa, 0xaaaa);
	/* Subpar Frequency domain normalization */
	if(config->frequencyNormalizationTries)
	{
		double width = 0;

		width = pl_flabelwidth_r(plot->plotter, "Rectangle ");
		pl_fmove_r(plot->plotter, config->plotResX/20*19+width, -1*config->plotResY/2+config->plotResY/80);
		pl_pencolor_r(plot->plotter, 0xaaaa, 0xaaaa, 0);
		sprintf(msg, "N%d", config->frequencyNormalizationTries);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
		if(config->frequencyNormalizationTolerant != 0)
		{
			pl_fmove_r(plot->plotter, config->plotResX/20*19+width, -1*config->plotResY/2+2*config->plotResY/80);
			sprintf(msg, "b:%g", config->frequencyNormalizationTolerant);
			pl_alabel_r(plot->plotter, 'c', 'l', msg);
		}
	}

	/* File information */
	if(config->labelNames)
	{
		if(type == PLOT_COMPARE)
		{
			DrawFileInfo(plot, config->referenceSignal, msg, type, 1, config);
			DrawFileInfo(plot, config->comparisonSignal, msg, type, 10, config);
		}
		else
		{
			if(type == PLOT_SINGLE_REF)
				DrawFileInfo(plot, config->referenceSignal, msg, type, 1, config);
			else
				DrawFileInfo(plot, config->comparisonSignal, msg, type, 1, config);
		}
	}

	/* Notes */
	pl_pencolor_r(plot->plotter, 0, 0xeeee, 0);

	if(plot->SpecialWarning)
	{
		PLOT_WARN(1, warning++);
		pl_alabel_r(plot->plotter, 'l', 'l', plot->SpecialWarning);
	}

	if(config->ignoreFrameRateDiff)
	{
		PLOT_WARN(1, warning++);
		pl_alabel_r(plot->plotter, 'l', 'l', "NOTE: Ignored frame rate difference during analysis (-I)");
	}

	if(config->compressToBlocks)
	{
		PLOT_WARN(1, warning++);
		pl_alabel_r(plot->plotter, 'l', 'l', "NOTE: Debug setting, blocks flattened (-9)");
	}

	if(!config->logScale)
	{
		PLOT_WARN(1, warning++);
		pl_alabel_r(plot->plotter, 'l', 'l', "NOTE: Log scale disabled (-N)");
	}

	if(config->channelBalance == 0 &&
		(config->referenceSignal->AudioChannels == 2 ||
		config->comparisonSignal->AudioChannels == 2))
	{
		PLOT_WARN(1, warning++);
		pl_alabel_r(plot->plotter, 'l', 'l', "NOTE: Audio channel balancing disabled (-B)");
	}


	if(config->ignoreFloor)
	{
		PLOT_WARN(1, warning++);
		if(config->ignoreFloor == 2)
		{
			sprintf(msg, "NOTE: Noise floor was manually set to %gdBFS (-p)", config->origSignificantAmplitude);
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}
		else
			pl_alabel_r(plot->plotter, 'l', 'l', "NOTE: Noise floor was ignored during analysis (-i)");
	}

	if(!config->noiseFloorAutoAdjust)
	{
		PLOT_WARN(1, warning++);
		pl_alabel_r(plot->plotter, 'l', 'l', "NOTE: Noise floor auto adjustment disabled (-p 0)");
	}

	if(config->AmpBarRange > BAR_DIFF_DB_TOLERANCE)
	{
		PLOT_WARN(1, warning++);
		pl_alabel_r(plot->plotter, 'l', 'l', "NOTE: Tolerance raised for matches (-b)");
	}

	if(config->normType != max_frequency)
	{
		if(config->normType == max_time)
		{
			PLOT_WARN(1, warning++);
			pl_alabel_r(plot->plotter, 'l', 'l', "NOTE: Time domain normalization (-n t)");
		}
		if(config->normType == average)
		{
			PLOT_WARN(1, warning++);
			pl_alabel_r(plot->plotter, 'l', 'l', "NOTE: Normalized by averages (-n a)");
		}
	}

	if(config->doSamplerateAdjust &&
		(config->referenceSignal->originalSR || config->comparisonSignal->originalSR))
	{
		if(config->comparisonSignal->originalSR)
		{
			PLOT_WARN(1, warning++);
			sprintf(msg, "NOTE: CM sample rate adj. to match duration \\!=%0.3f\\ct (-R)", config->ComCentsDifferenceSR);
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}

		if(config->referenceSignal->originalSR)
		{
			PLOT_WARN(1, warning++);
			sprintf(msg, "NOTE: RF sample rate adj. to match duration \\!=%0.3f\\ct (-R)", config->RefCentsDifferenceSR);
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}
	}

	if(config->clkMeasure && config->doClkAdjust && 
		(config->referenceSignal->originalCLK || config->comparisonSignal->originalCLK))
	{
		PLOT_WARN(1, warning++);
		sprintf(msg, "NOTE: %s %s clock adjusted by: %0.2f\\ct (-j)", 
				config->changedCLKFrom == ROLE_REF ? "Reference" : "Comparison",
				config->clkName, config->centsDifferenceCLK);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}
	else if(config->clkMeasure && config->doClkAdjust && fabs(config->centsDifferenceCLK) <= MAX_CENTS_DIFF)
	{
		PLOT_WARN(1, warning++);
		sprintf(msg, "NOTE: %s %s clock adjust ignored: %0.2f\\ct (-j)", 
				config->changedCLKFrom == ROLE_REF ? "Reference" : "Comparison",
				config->clkName, config->centsDifferenceCLK);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if(config->syncTolerance)
	{
		PLOT_WARN(1, warning++);
		pl_alabel_r(plot->plotter, 'l', 'l', "NOTE: Sync tolerance enabled (-T)");
	}

	if(config->maxDbPlotZC != DB_HEIGHT && type == PLOT_COMPARE)
	{
		PLOT_WARN(1, warning++);
		if (config->maxDbPlotZCChanged)
			sprintf(msg, "NOTE: Vertical scale changed (-d %g)", config->maxDbPlotZC);
		else
			sprintf(msg, "NOTE: Vertical scale auto-adjusted within one Std Dev");
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	/* Warnings */
	pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
	if(config->noSyncProfile && type < PLOT_SINGLE_REF)
	{
		PLOT_WARN(1, warning++);
		sprintf(msg, "WARNING: No sync profile [%s], PLEASE DISREGARD", 
					config->noSyncProfileType == NO_SYNC_AUTO ? "Auto" : config->noSyncProfileType == NO_SYNC_MANUAL ? "Manual" : "Digital Zero");
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if(config->noiseFloorTooHigh)
	{
		PLOT_WARN(1, warning++);
		sprintf(msg, "WARNING: %s noise floor too high", 
			config->noiseFloorTooHigh == ROLE_REF ? "Reference" : config->noiseFloorTooHigh == ROLE_COMP ? "Comparison" : "Both");
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if(config->smallFile)
	{
		PLOT_WARN(1, warning++);
		sprintf(msg, "WARNING: %s file%s shorter than expected", 
			config->smallFile == ROLE_REF ? "Reference" : config->smallFile == ROLE_COMP ? "Comparison" : "Both",
			config->smallFile == (ROLE_REF | ROLE_COMP) ? "s were" : " was");
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if(config->internalSyncTolerance)
	{
		PLOT_WARN(1, warning++);
		sprintf(msg, "WARNING: %s file%s internal sync anomalies", 
			config->internalSyncTolerance == ROLE_REF ? "Reference" : config->internalSyncTolerance == ROLE_COMP ? "Comparison" : "Both",
			config->internalSyncTolerance == (ROLE_REF | ROLE_COMP) ? "s have" : " has");
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if(config->warningRatioTooHigh != 0)
	{
		PLOT_WARN(1, warning++);
		sprintf(msg, "         Please read MDFourier text output for details.");
		pl_alabel_r(plot->plotter, 'l', 'l', msg);

		PLOT_WARN(1, warning++);
		sprintf(msg, "WARNING: Average signal difference too high. (%g to 1)", config->warningRatioTooHigh);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if (config->debugSync)
	{
		int i = 0;
		char *names[4] = {
							"Ref Start",
							"Ref End",
							"Com Start",
							"Com End"
						};

		for (i = 3; i >= 0; i--)
		{
			if (config->syncAlignTolerance[i])
			{
				PLOT_WARN(1, warning++);
				sprintf(msg, "WARNING: %s Sync was centered due to noise, pulse: %g%%", 
					names[i], config->syncAlignPct[i]);
				pl_alabel_r(plot->plotter, 'l', 'l', msg);
			}
			else
			{
				PLOT_WARN(1, warning++);
				sprintf(msg, "NOTE: Pulse standard deviation type for %s: %g%%",
					names[i], config->syncAlignPct[i]);
				pl_alabel_r(plot->plotter, 'l', 'l', msg);
			}
		}
	}

	if(config->normType == none)
	{
		PLOT_WARN(1, warning++);
		pl_alabel_r(plot->plotter, 'l', 'l', "WARNING: No Normalization, PLEASE DISREGARD (-n n)");
	}

	if (config->types.useWatermark && DetectWatermarkIssue(msg, config->comparisonSignal, config))
	{
		PLOT_WARN(1, warning++);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if(config->types.useWatermark && DetectWatermarkIssue(msg, config->referenceSignal, config))
	{
		PLOT_WARN(1, warning++);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if(config->stereoNotFound)
	{
		PLOT_WARN(1, warning++);
		sprintf(msg, "WARNING: %s %s mono for stereo profile", 
			config->stereoNotFound == ROLE_REF ? "Reference" : config->stereoNotFound == ROLE_COMP ? "Comparison" : "Both files",
			(config->stereoNotFound == ROLE_REF || config->stereoNotFound == ROLE_COMP) ? "is" : "are");
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if(config->warningStereoReversed)
	{
		PLOT_WARN(1, warning++);
		pl_alabel_r(plot->plotter, 'l', 'l', "WARNING: L/R Channels might be reversed (or mono)");
	}

	if(config->clkWarning)
	{
		PLOT_WARN(1, warning++);
		sprintf(msg, "WARNING: %s %s noise/harmonics in the clk block", 
			config->clkWarning == ROLE_REF ? "Reference" : config->clkWarning == ROLE_COMP ? "Comparison" : "Both files",
			(config->clkWarning == ROLE_REF || config->clkWarning == ROLE_COMP) ? "has" : "have");
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if(config->clkNotFound)
	{
		PLOT_WARN(1, warning++);
		sprintf(msg, "WARNING: %s clk could not be detected", 
			config->clkNotFound == ROLE_REF ? "Reference" : config->clkNotFound == ROLE_COMP ? "Comparison" : "Both files");
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}


	if(config->SRNoMatch && !config->doSamplerateAdjust)
	{
		double labelwidth = 0;

		labelwidth = pl_flabelwidth_r(plot->plotter, "WARNING: ");

		if(config->ComCentsDifferenceSR)
		{
			PLOT_WARN_XDISP(1, warning++, labelwidth);
			sprintf(msg, "CM pitch might be off by: %0.2f\\ct",
				config->ComCentsDifferenceSR);
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}

		if(config->RefCentsDifferenceSR)
		{
			PLOT_WARN_XDISP(1, warning++, labelwidth);
			sprintf(msg, "RF pitch might be off by: %0.2f\\ct",
				config->RefCentsDifferenceSR);
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}

		PLOT_WARN(1, warning++);
		sprintf(msg, "WARNING: %s%s match expected duration (can use -R)",
			config->SRNoMatch == (ROLE_REF | ROLE_COMP) ? "Signal" : config->SRNoMatch == ROLE_REF ? "RF" : "CM",
			config->SRNoMatch == (ROLE_REF | ROLE_COMP) ? "s don't" : " doesn't");
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if(config->clkMeasure && config->diffClkNoMatch && !config->doClkAdjust)
	{
		PLOT_WARN(1, warning++);
		sprintf(msg, "WARNING: %s clock don't match by: %0.2f\\ct (can use -j)", 
				config->clkName, config->centsDifferenceCLK);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if(config->noBalance)
	{
		PLOT_WARN(1, warning++);
		sprintf(msg, "WARNING: %stereo balancing could not be made",
			config->noBalance == ROLE_REF ? "RF s" : config->noBalance == ROLE_COMP ? "CM s" : "No s");
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	/*
	if(config->noiseFloorBigDifference)
	{
		PLOT_WARN(1, warning++);
		pl_alabel_r(plot->plotter, 'l', 'l', "WARNING: High noise floor difference");
	}
	*/

	/* Top messages */
	pl_pencolor_r(plot->plotter, 0, 0xcccc, 0);
	{
		PLOT_COLUMN(1, 1);
		if(config->significantAmplitude > LOWEST_NOISEFLOOR_ALLOWED || config->ignoreFloor
		 || config->significantAmplitude < SIGNIFICANT_VOLUME)
			pl_pencolor_r(plot->plotter, 0xcccc, 0xcccc, 0);
		sprintf(msg, "Significant: %0.1f dBFS", config->significantAmplitude);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	pl_pencolor_r(plot->plotter, 0, 0xcccc, 0);
	//if(config->startHz != START_HZ || config->endHz != END_HZ)
	{
		PLOT_COLUMN(1, 2);
		if(config->startHz != START_HZ || config->endHz != END_HZ)
			pl_pencolor_r(plot->plotter, 0xcccc, 0xcccc, 0);
		sprintf(msg, "Range: %g%s-%g%s", 
				config->startHz >= 1000 ? config->startHz/1000.0 : config->startHz,
				config->startHz >= 1000 ? "khz" : "hz",
				config->endHz >= 1000 ? config->endHz/1000.0 : config->endHz,
				config->endHz >= 1000 ? "khz" : "hz");
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	/* Noise floor */
	pl_pencolor_r(plot->plotter, 0xcccc, 0xcccc, 0xcccc);
	if(config->referenceSignal && (type == PLOT_COMPARE || type == PLOT_SINGLE_REF))
	{
		if(config->referenceSignal->gridAmplitude)
		{
			PLOT_COLUMN(3, 1);
			sprintf(msg, "Ref %0.1fhz:  %0.1fdBFS", 
					config->referenceSignal->gridFrequency,
					config->referenceSignal->gridAmplitude);
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}
	
		if(config->referenceSignal->scanrateAmplitude)
		{
			PLOT_COLUMN(3, 2);
			sprintf(msg, "Ref %0.1fkhz: %0.1fdBFS", 
					config->referenceSignal->scanrateFrequency/1000.0,
					config->referenceSignal->scanrateAmplitude);
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}
	}

	if(config->comparisonSignal && (type == PLOT_COMPARE || type == PLOT_SINGLE_COM))
	{
		if(config->comparisonSignal->gridAmplitude)
		{
			PLOT_COLUMN(2, 1);
			sprintf(msg, "Com %0.1fhz: %0.1fdBFS", 
					config->comparisonSignal->gridFrequency,
					config->comparisonSignal->gridAmplitude);
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}
	
		if(config->comparisonSignal->scanrateAmplitude)
		{
			PLOT_COLUMN(2, 2);
			sprintf(msg, "Com %0.1fkhz: %0.1fdBFS", 
					config->comparisonSignal->scanrateFrequency/1000.0,
					config->comparisonSignal->scanrateAmplitude);
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}
	}

	if(config->hasAddOnData)
	{
		PLOT_COLUMN(1, 3);
		if(config->useExtraData)
			pl_alabel_r(plot->plotter, 'l', 'l', "Extra Data: ON");
		else
			pl_alabel_r(plot->plotter, 'l', 'l', "Extra Data: OFF");
	}

	/* Aligned to 1hz */
	pl_pencolor_r(plot->plotter, 0, 0xcccc, 0xcccc);
	if(config->ZeroPad)
	{
		PLOT_COLUMN(2, 3);
		pl_alabel_r(plot->plotter, 'l', 'l', "1Hz Aligned");
	}

	if(config->outputFilterFunction != 3)
	{
		char * filer[6] = {"None", 	"Bright", "High", "Neutral", "Low", "Dimm" };

		PLOT_COLUMN(3, 3);
		sprintf(msg, "Color function: %s", filer[config->outputFilterFunction]);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if(config->MaxFreq != FREQ_COUNT)
	{
		PLOT_COLUMN(4, 1);
		sprintf(msg, "Frequencies/note: %d", config->MaxFreq);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}
	
	// 4,2 is empty was vertical scale changed

	if(config->channelWithLowFundamentals)
	{
		PLOT_COLUMN(4, 3);
		pl_pencolor_r(plot->plotter, 0, 0xeeee, 0xeeee);
		pl_alabel_r(plot->plotter, 'l', 'l', "Low Fundamentals present");
	}

	if(config->clkMeasure)
	{
		if(type == PLOT_COMPARE)
		{
			PLOT_COLUMN(5, 1);
			DrawClockData(plot, config->referenceSignal, msg, config);
			pl_alabel_r(plot->plotter, 'l', 'l', msg);

			PLOT_COLUMN(5, 2);
			DrawClockData(plot, config->comparisonSignal, msg, config);
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}
		else if(type == PLOT_SINGLE_REF)
		{
			PLOT_COLUMN(5, 1);
			DrawClockData(plot, config->referenceSignal, msg, config);
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}
		else
		{
			PLOT_COLUMN(5, 2);
			DrawClockData(plot, config->comparisonSignal, msg, config);
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}
	}

	if(config->notVisible > 1)
	{
		PLOT_COLUMN(5, 3);
		if(config->notVisible > 5)
			pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
		else
			pl_pencolor_r(plot->plotter, 0, 0xcccc, 0xcccc);
		sprintf(msg, "Data \\ua\\da %0.2fdBFS: %0.2f%%", 
			config->maxDbPlotZC, config->notVisible);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if(config->referenceSignal->EstimatedSR || config->comparisonSignal->EstimatedSR)
	{
		PLOT_COLUMN(6, 1);
		DrawSRData(plot, config->referenceSignal, msg, config);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
		PLOT_COLUMN(6, 2);
		DrawSRData(plot, config->comparisonSignal, msg, config);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if(type == PLOT_COMPARE)
	{
		PLOT_COLUMN(7, 1);
		DrawImbalance(plot, config->referenceSignal, msg, config);
		PLOT_COLUMN(7, 2);
		DrawImbalance(plot, config->comparisonSignal, msg, config);
	}
	else
	{
		if(type == PLOT_SINGLE_REF)
		{
			PLOT_COLUMN(7, 1);
			DrawImbalance(plot, config->referenceSignal, msg, config);
		}
		else
		{
			PLOT_COLUMN(7, 2);
			DrawImbalance(plot, config->comparisonSignal, msg, config);
		}
	}

	pl_pencolor_r(plot->plotter, 0, 0xeeee, 0xeeee);
	if(type != PLOT_SINGLE_COM && config->referenceSignal->delayElemCount)
	{
		double labelpos = 0, x = 0, y = 0;

		pl_ffontsize_r(plot->plotter, FONT_SIZE_3);
		x = config->plotResX/20*15;
		y = -1*config->plotResY/2+config->plotResY/80+config->plotResY/60*3;
		y += config->plotResY/60;

		sprintf(msg, "R delays  ");
		pl_fmove_r(plot->plotter, x, y);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
		labelpos += pl_flabelwidth_r(plot->plotter, msg);
		for(int i = 0; i < config->referenceSignal->delayElemCount; i++)
		{
			sprintf(msg, "%s: %0.1fms ", GetInternalSyncSequentialName(i,config), config->referenceSignal->delayArray[i]);
			pl_fmove_r(plot->plotter, x+labelpos, y);
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
			labelpos += pl_flabelwidth_r(plot->plotter, msg);
		}
	}

	if(type != PLOT_SINGLE_REF && config->comparisonSignal->delayElemCount)
	{
		double labelpos = 0, x = 0, y = 0;

		x = config->plotResX/20*15;
		y = -1*config->plotResY/2+config->plotResY/80+config->plotResY/60*3;

		pl_ffontsize_r(plot->plotter, FONT_SIZE_3);
		sprintf(msg, "C delays  ");
		pl_fmove_r(plot->plotter, x, y);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
		labelpos += pl_flabelwidth_r(plot->plotter, msg);
		for(int i = 0; i < config->comparisonSignal->delayElemCount; i++)
		{
			sprintf(msg, "%s: %0.1fms ", GetInternalSyncSequentialName(i,config), config->comparisonSignal->delayArray[i]);
			pl_fmove_r(plot->plotter, x+labelpos, y);
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
			labelpos += pl_flabelwidth_r(plot->plotter, msg);
		}
	}

	pl_restorestate_r(plot->plotter);

#ifdef TESTWARNINGS
	*config = backup;
#endif
}

void DrawLabelsZeroToLimit(PlotFile *plot, double dBFS, double dbIncrement, double hz, int drawSignificant, parameters *config)
{
	double segments = 0;
	char label[100];

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0-X0BORDER*config->plotResX*plot->leftmargin, -1*config->plotResY-Y0BORDER*config->plotResY, config->plotResX+X1BORDER*config->plotResX, 0+Y1BORDER*config->plotResY);
	pl_pencolor_r(plot->plotter, 0, 0xaaaa, 0);
	pl_ffontsize_r(plot->plotter, FONT_SIZE_1);

	if(fabs(dBFS) < config->lowestDBFS)
		dbIncrement *= 2;

	pl_ffontname_r(plot->plotter, PLOT_FONT);
	segments = ceil(fabs(dBFS/dbIncrement));
	for(int i = 0; i <= segments; i ++)
	{
		pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, -1*i*config->plotResY/segments);
		sprintf(label, "%gdBFS", -1*i*dbIncrement);
		pl_alabel_r(plot->plotter, 'l', 'c', label);
	}

	if(drawSignificant)
	{
		double labelwidth = 0;

		labelwidth = pl_flabelwidth_r(plot->plotter, "\\ua XXXXXXXXX");

		pl_fmove_r(plot->plotter, -1*labelwidth-PLOT_SPACER, -1*config->plotResY/fabs(dBFS)*fabs(config->significantAmplitude));
		pl_pencolor_r (plot->plotter, 0x9999, 0x9999, 0);
		pl_alabel_r(plot->plotter, 'l', 'c', "Significant");

		pl_fmove_r(plot->plotter, -1*labelwidth-PLOT_SPACER, -1*config->plotResY/fabs(dBFS)*fabs(config->significantAmplitude)+1.5*BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0, 0xaaaa, 0);
		pl_alabel_r(plot->plotter, 'l', 'c', "\\ua Analyzed");

		pl_fmove_r(plot->plotter, -1*labelwidth-PLOT_SPACER, -1*config->plotResY/fabs(dBFS)*fabs(config->significantAmplitude)-1.5*BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0xaaaa, 0, 0);
		pl_alabel_r(plot->plotter, 'l', 'c', "\\da Discarded");

		pl_pencolor_r(plot->plotter, 0, 0xaaaa, 0);
	}

	if(config->logScale)
	{
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(10, config), 0);
		sprintf(label, "%dHz", 10);
		pl_alabel_r(plot->plotter, 'c', 'b', label);
	
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(100, config), 0);
		sprintf(label, "%dHz", 100);
		pl_alabel_r(plot->plotter, 'c', 'b', label);
	}

	pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(1000, config), 0);
	sprintf(label, "  %dHz", 1000);
	pl_alabel_r(plot->plotter, 'c', 'b', label);

	if(config->endHzPlot >= 10000)
	{
		for(int i = 10000; i < config->endHzPlot; i+= 10000)
		{
			pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(i, config), 0);
			sprintf(label, "%d%s", i/1000, i > 40000 ? "" : "khz");
			pl_alabel_r(plot->plotter, 'c', 'b', label);
		}
	}

	pl_restorestate_r(plot->plotter);
}

void DrawColorScale(PlotFile *plot, int type, int mode, double x, double y, double width, double height, double startDbs, double endDbs, double dbIncrement, parameters *config)
{
	double		segments = 0;
	char 		*label = NULL;
	int 		colorName = 0;
	long int	cnt = 0, cmp = 0;
	double		labelwidth = 0, maxlabel = 0;

	label = GetTypeDisplayName(config, type);
	if(type == TYPE_CLK_ANALYSIS)
		colorName = MatchColor("green");
	else
		colorName = MatchColor(GetTypeColor(config, type));

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0, 0, config->plotResX, config->plotResY);
	pl_filltype_r(plot->plotter, 1);

	segments = floor(fabs(endDbs/dbIncrement));
	for(double i = 0; i < segments; i ++)
	{
		long int intensity;

		intensity = CalculateWeightedError(i/segments, config)*0xffff;

		SetPenColor(colorName, intensity, plot);
		SetFillColor(colorName, intensity, plot);
		pl_fbox_r(plot->plotter, x, y+i*height/segments, x+width, y+i*height/segments+height/segments);
		pl_endsubpath_r(plot->plotter);
	}

	pl_pencolor_r(plot->plotter, 0xaaaa, 0xaaaa, 0xaaaa);
	pl_filltype_r(plot->plotter, 0);
	pl_fbox_r(plot->plotter, x, y, x+width, y+height);

	SetPenColor(colorName, 0xaaaa, plot);
	pl_ffontsize_r(plot->plotter, FONT_SIZE_2);
	pl_ffontname_r(plot->plotter, PLOT_FONT);

	/* dB label */

	pl_fmove_r(plot->plotter, x+width/2, y-FONT_SIZE_2);
	pl_alabel_r(plot->plotter, 'c', 'c', "dBFS");

	for(double i = 0; i < segments; i++)
	{
		char labeldbs[20];

		pl_fmove_r(plot->plotter, x+width+PLOT_SPACER, y+height-i*height/segments-height/segments/2);
		sprintf(labeldbs, "%c%g", fabs(startDbs) + i*dbIncrement != 0 ? '-' : ' ', fabs(startDbs) + i*dbIncrement);
		pl_alabel_r(plot->plotter, 'l', 'c', labeldbs);

		labelwidth = pl_flabelwidth_r(plot->plotter, label);
		if(maxlabel < labelwidth)
			maxlabel = labelwidth;	
	}

	x = x+width+maxlabel+FONT_SIZE_1/2;
	labelwidth = 0;
	maxlabel = 0;

	SetPenColor(colorName, 0xaaaa, plot);
	pl_fmove_r(plot->plotter, x, y);
	pl_alabel_r(plot->plotter, 'l', 'l', label);
	labelwidth = pl_flabelwidth_r(plot->plotter, label);

	if(mode != MODE_SPEC)
	{
		double bar_text_width = 0;

		if(mode == MODE_DIFF)
		{
			FindDifferenceTypeTotals(type, &cnt, &cmp, config);

			SetPenColor(COLOR_GRAY, 0xaaaa, plot);
			pl_fmove_r(plot->plotter, x, y+1.5*BAR_HEIGHT);
			pl_alabel_r(plot->plotter, 'l', 'l', BAR_DIFF);
		}
		if(mode == MODE_MISS)
		{
			FindMissingTypeTotals(type, &cnt, &cmp, config);
		}
		bar_text_width = DrawMatchBar(plot, colorName,
			x+labelwidth+BAR_WIDTH*0.2, y,
			BAR_WIDTH, BAR_HEIGHT, 
			(double)cnt, (double)cmp, config);
		if(mode == MODE_DIFF)
		{
			int 	x_offset = 0;
			char	header[100];

			x_offset = x+labelwidth+BAR_WIDTH + bar_text_width;

			FindDifferenceWithinInterval(type, &cnt, &cmp, config->AmpBarRange, config);

			if(config->AmpBarRange > BAR_DIFF_DB_TOLERANCE)
				SetPenColor(COLOR_YELLOW, 0xaaaa, plot);
			else
				SetPenColor(COLOR_GRAY, 0xaaaa, plot);
			pl_fmove_r(plot->plotter, 1.1*x_offset, y+1.5*BAR_HEIGHT);
			if(!config->drawPerfect)
				sprintf(header, BAR_WITHIN, config->AmpBarRange);
			else
				sprintf(header, BAR_WITHIN_PERFECT, config->AmpBarRange);
			pl_alabel_r(plot->plotter, 'l', 'l', header);

			SetPenColor(COLOR_GRAY, 0xaaaa, plot);
			pl_fmove_r(plot->plotter, x_offset, y+3*BAR_HEIGHT);
			if(!config->drawPerfect)
				pl_alabel_r(plot->plotter, 'c', 'c', BAR_HEADER);
			else
				pl_alabel_r(plot->plotter, 'l', 'c', BAR_HEADER);


			bar_text_width = DrawMatchBar(plot, colorName,
				1.1*x_offset, y,
				BAR_WIDTH, BAR_HEIGHT, 
				(double)cnt, (double)cmp, config);

			/* Perfect */
			if(config->drawPerfect)
			{
				x_offset = x_offset + BAR_WIDTH + bar_text_width;
	
				FindPerfectMatches(type, &cnt, &cmp, config);
	
				SetPenColor(COLOR_GRAY, 0xaaaa, plot);
				pl_fmove_r(plot->plotter, 1.1*x_offset, y+1.5*BAR_HEIGHT);
				sprintf(header, BAR_PERFECT);
				pl_alabel_r(plot->plotter, 'l', 'l', header);
	
				DrawMatchBar(plot, colorName,
					1.1*x_offset, y,
					BAR_WIDTH, BAR_HEIGHT, 
					(double)cnt, (double)cmp, config);
			}
		}
	}
	pl_restorestate_r(plot->plotter);
}

void DrawColorAllTypeScale(PlotFile *plot, int mode, double x, double y, double width, double height, double endDbs, double dbIncrement, int drawBars, parameters *config)
{
	double 	segments = 0, maxlabel = 0, maxbarwidth = 0;
	int		*colorName = NULL, *typeID = NULL;
	int		numTypes = 0, t = 0, maxMatch = 0;

	numTypes = GetActiveBlockTypesNoRepeat(config);
	segments = floor(fabs(endDbs/dbIncrement));

	width *= numTypes;
	colorName = (int*)malloc(sizeof(int)*numTypes);
	if(!colorName)
		return;
	typeID = (int*)malloc(sizeof(int)*numTypes);
	if(!typeID)
	{
		free(colorName);
		return;
	}

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0, 0, config->plotResX, config->plotResY);
	pl_filltype_r(plot->plotter, 1);

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type > TYPE_CONTROL && !config->types.typeArray[i].IsaddOnData)
		{
			colorName[t] = MatchColor(GetTypeColor(config, config->types.typeArray[i].type));
			typeID[t] = config->types.typeArray[i].type;
			t++;
		}
	}

	pl_ffontsize_r(plot->plotter, FONT_SIZE_2);
	pl_ffontname_r(plot->plotter, PLOT_FONT);

	if(drawBars == DRAW_BARS)
	{
		for(double i = 0; i < segments; i ++)
		{
			long int intensity;
	
			if(mode != MODE_TSDIFF)
				intensity = CalculateWeightedError(i/segments, config)*0xffff;
			else
				intensity = 0xffff*CalculateWeightedError(1.0 - i/segments, config);
	
			for(int t = 0; t < numTypes; t++)
			{
				double bx, by;
	
				bx = x+(double)t*width/(double)numTypes;
				by = y+i*height/segments;
				SetPenColor(colorName[t], intensity, plot);
				SetFillColor(colorName[t], intensity, plot);
				pl_fbox_r(plot->plotter, bx, by, bx+width/(double)numTypes, by+height/segments);
				pl_endsubpath_r(plot->plotter);
			}
		}

		pl_pencolor_r(plot->plotter, 0xaaaa, 0xaaaa, 0xaaaa);
		pl_filltype_r(plot->plotter, 0);
		pl_fbox_r(plot->plotter, x, y, x+width, y+height);
	
		SetPenColor(COLOR_GRAY, 0xaaaa, plot);

		/* dB label */
	
		pl_fmove_r(plot->plotter, x+width/2, y-FONT_SIZE_2);
		pl_alabel_r(plot->plotter, 'c', 'c', "dBFS");

		for(double i = 0; i < segments; i++)
		{
			char label[20];
			double	labelwidth = 0;
	
			pl_fmove_r(plot->plotter, x+width+PLOT_SPACER, y+height-i*height/segments-height/segments/2);
			if(mode != MODE_TSDIFF)
				sprintf(label, "%c%g", i*dbIncrement > 0 ? '-' : ' ', i*dbIncrement);
			else
				sprintf(label, "%s%g", i ? "\\+-" : "", fabs(i*dbIncrement));
			pl_alabel_r(plot->plotter, 'l', 'c', label);
	
			labelwidth = pl_flabelwidth_r(plot->plotter, label);
			if(maxlabel < labelwidth)
				maxlabel = labelwidth;	
		}
	}

	x = x+width+maxlabel+FONT_SIZE_1/2;
	maxlabel = 0;

	for(int t = 0; t < numTypes; t++)
	{
		double	labelwidth = 0;
		char 	*label = NULL;

		label = GetTypeDisplayName(config, typeID[t]);
		SetPenColor(colorName[t], 0xaaaa, plot);
		pl_fmove_r(plot->plotter, x, y+(numTypes-1)*config->plotResY/50-t*config->plotResY/50);
		pl_alabel_r(plot->plotter, 'l', 'l', label);

		labelwidth = pl_flabelwidth_r(plot->plotter, label);
		if(maxlabel < labelwidth)
			maxlabel = labelwidth;
	}

	if(mode != MODE_SPEC && mode != MODE_TSDIFF)
	{
		// Draw label if needed
		if(mode == MODE_DIFF)
		{
			SetPenColor(COLOR_GRAY, 0xaaaa, plot);
			pl_fmove_r(plot->plotter, x, y+(numTypes-1)*config->plotResY/50+1.5*BAR_HEIGHT);
			pl_alabel_r(plot->plotter, 'l', 'l', BAR_DIFF);
		}

		for(int t = 0; t < numTypes; t++)
		{
			double		barwidth = 0;
			long int	cnt = 0, cmp = 0;
	
			if(mode == MODE_DIFF)
				FindDifferenceTypeTotals(typeID[t], &cnt, &cmp, config);
			if(mode == MODE_MISS)
				FindMissingTypeTotals(typeID[t], &cnt, &cmp, config);
			barwidth = DrawMatchBar(plot, colorName[t],
					x+maxlabel+BAR_WIDTH*0.2, y+(numTypes-1)*config->plotResY/50-t*config->plotResY/50,
					BAR_WIDTH, BAR_HEIGHT, 
					(double)cnt, (double)cmp, config);
			if(barwidth > maxbarwidth)
				maxbarwidth = barwidth;
		}
	}

	// Draw Match within bars
	if(mode == MODE_DIFF)
	{
		long int	cnt = 0, cmp = 0;
		int 		x_offset = 0;
		char		header[100];

		x_offset = x+BAR_WIDTH+maxbarwidth + maxlabel;

		if(config->AmpBarRange > BAR_DIFF_DB_TOLERANCE)
			SetPenColor(COLOR_YELLOW, 0xaaaa, plot);
		else
			SetPenColor(COLOR_GRAY, 0xaaaa, plot);
		pl_fmove_r(plot->plotter, 1.1*x_offset, y+(numTypes-1)*config->plotResY/50+1.5*BAR_HEIGHT);
		if(!config->drawPerfect)
			sprintf(header, BAR_WITHIN, config->AmpBarRange);
		else
			sprintf(header, BAR_WITHIN_PERFECT, config->AmpBarRange);
		pl_alabel_r(plot->plotter, 'l', 'l', header);

		
		SetPenColor(COLOR_GRAY, 0xaaaa, plot);
		pl_fmove_r(plot->plotter, x_offset, y+(numTypes-1)*config->plotResY/50+3*BAR_HEIGHT);
		if(!config->drawPerfect)
			pl_alabel_r(plot->plotter, 'c', 'c', BAR_HEADER);
		else
			pl_alabel_r(plot->plotter, 'l', 'c', BAR_HEADER);

		for(int t = 0; t < numTypes; t++)
		{
			int localMax = 0;

			FindDifferenceWithinInterval(typeID[t], &cnt, &cmp, config->AmpBarRange, config);

			localMax = DrawMatchBar(plot, colorName[t],
				1.1*x_offset, y+(numTypes-1)*config->plotResY/50-t*config->plotResY/50,
				BAR_WIDTH, BAR_HEIGHT, 
				(double)cnt, (double)cmp, config);
			if(localMax > maxMatch)
				maxMatch = localMax;
		}

		if(config->drawPerfect)
		{
			x_offset = x_offset + BAR_WIDTH + maxMatch;

			SetPenColor(COLOR_GRAY, 0xaaaa, plot);
			pl_fmove_r(plot->plotter, 1.1*x_offset, y+(numTypes-1)*config->plotResY/50+1.5*BAR_HEIGHT);
			sprintf(header, BAR_PERFECT);
			pl_alabel_r(plot->plotter, 'l', 'l', header);

			for(int t = 0; t < numTypes; t++)
			{
				FindPerfectMatches(typeID[t], &cnt, &cmp, config);
				DrawMatchBar(plot, colorName[t],
					1.1*x_offset, y+(numTypes-1)*config->plotResY/50-t*config->plotResY/50,
					BAR_WIDTH, BAR_HEIGHT, 
					(double)cnt, (double)cmp, config);
			}
		}
	}

	free(colorName);
	colorName = NULL;
	free(typeID);
	typeID = NULL;

	pl_restorestate_r(plot->plotter);
}

double DrawMatchBar(PlotFile *plot, int colorName, double x, double y, double width, double height, double notFound, double total, parameters *config)
{
	char percent[40];
	double labelwidth = 0, maxlabel = 0;

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0, 0, config->plotResX, config->plotResY);

	// Back
	pl_filltype_r(plot->plotter, 1);
	SetPenColor(COLOR_GRAY, 0x0000, plot);
	SetFillColor(COLOR_GRAY, 0x0000, plot);
	pl_fbox_r(plot->plotter, x, y, x+width, y+height);

	// FG
	pl_filltype_r(plot->plotter, 1);
	SetPenColor(colorName, 0x8888, plot);
	SetFillColor(colorName, 0x8888, plot);
	if(total)
		pl_fbox_r(plot->plotter, x, y, x+(notFound*width/total), y+height);

	// Border
	pl_filltype_r(plot->plotter, 0);
	SetPenColor(COLOR_GRAY, 0x8888, plot);
	pl_fbox_r(plot->plotter, x, y, x+width, y+height);

	pl_filltype_r(plot->plotter, 0);

	// percent
	if(config->showPercent)
	{
		pl_ffontsize_r(plot->plotter, FONT_SIZE_2);
		pl_ffontname_r(plot->plotter, PLOT_FONT);

		if(total)
			sprintf(percent, "%5.2f%% of %ld", notFound*100.0/total, (long int)total);
		else
			sprintf(percent, "NONE FOUND IN RANGE");

		SetPenColor(colorName, 0x8888, plot);
		pl_fmove_r(plot->plotter, x+width*1.10, y);
		pl_alabel_r(plot->plotter, 'l', 'l', percent);
		labelwidth = pl_flabelwidth_r(plot->plotter, percent);
		if(labelwidth > maxlabel)
			maxlabel = labelwidth;
	}
	pl_restorestate_r(plot->plotter);

	return maxlabel;
}

void DrawNoiseLines(PlotFile *plot, double start, double end, AudioSignal *Signal, parameters *config)
{
	int harmonic = 0;

	pl_pencolor_r (plot->plotter, 0xAAAA, 0xAAAA, 0);
	pl_linemod_r(plot->plotter, "dotdashed");
	if(Signal->gridFrequency)
	{
		for(harmonic = 1; harmonic < 32; harmonic++)
		{
			pl_pencolor_r (plot->plotter, 0xAAAA-0x400*harmonic, 0xAAAA-0x400*harmonic, 0);
			pl_fline_r(plot->plotter, transformtoLog(Signal->gridFrequency*harmonic, config), start, transformtoLog(Signal->gridFrequency*harmonic, config), end);
		}
	}
	if(Signal->scanrateFrequency)
	{
		// Scanrate
		pl_pencolor_r (plot->plotter, 0xAAAA, 0xAAAA, 0);
		pl_fline_r(plot->plotter, transformtoLog(Signal->scanrateFrequency, config), start, transformtoLog(Signal->scanrateFrequency, config), end);
		// Crosstalk
		pl_pencolor_r (plot->plotter, 0xAAAA, 0x8888, 0);
		pl_fline_r(plot->plotter, transformtoLog(Signal->scanrateFrequency/2, config), start, transformtoLog(Signal->scanrateFrequency/2, config), end);
	}
	pl_linemod_r(plot->plotter, "solid");
}

void DrawLabelsNoise(PlotFile *plot, double hz, AudioSignal *Signal, parameters *config)
{
	char label[20];

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0-X0BORDER*config->plotResX*plot->leftmargin, -1*config->plotResY/2-Y0BORDER*config->plotResY, config->plotResX+X1BORDER*config->plotResX, config->plotResY/2+Y1BORDER*config->plotResY);

	pl_ffontname_r(plot->plotter, PLOT_FONT);
	pl_ffontsize_r(plot->plotter, FONT_SIZE_1);

	if(Signal->gridFrequency)
	{
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(Signal->gridFrequency, config), config->plotResY/2+FONT_SIZE_1);
		sprintf(label, "  %0.2fHz", Signal->gridFrequency);
		pl_alabel_r(plot->plotter, 'c', 'b', label);

		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(Signal->gridFrequency*2, config), config->plotResY/2+FONT_SIZE_1);
		sprintf(label, "  %0.2fHz", Signal->gridFrequency*2);
		pl_alabel_r(plot->plotter, 'c', 'b', label);
	}

	if(Signal->scanrateFrequency)
	{
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(Signal->scanrateFrequency, config), config->plotResY/2+FONT_SIZE_1);
		sprintf(label, "  %0.2fkHz", Signal->scanrateFrequency/1000);
		pl_alabel_r(plot->plotter, 'c', 'b', label);

		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(Signal->scanrateFrequency/2, config), config->plotResY/2+FONT_SIZE_1);
		sprintf(label, "  %0.2fkHz", Signal->scanrateFrequency/2000);
		pl_alabel_r(plot->plotter, 'c', 'b', label);
	}

	pl_restorestate_r(plot->plotter);
}

void SaveCSVAmpDiff(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config)
{
	FILE 		*csv = NULL;
	char		name[BUFFER_SIZE];

	if(!config)
		return;

	if(!amplDiff)
		return;

	sprintf(name, "%s.csv", filename);
	
	csv = fopen(name, "wb");
	if(!csv)
		return;
	fprintf(csv, "Type, Frequency(Hz), Diff(dbfs)\n");
	for(int a = 0; a < size; a++)
	{
		if(amplDiff[a].type > TYPE_CONTROL)
		{ 
			if(amplDiff[a].refAmplitude > config->significantAmplitude)
				fprintf(csv, "%s, %g,%g\n", GetTypeName(config, amplDiff[a].type), amplDiff[a].hertz, amplDiff[a].diffAmplitude);
		}
	}
	fclose(csv);
}

void PlotAllDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, char channel, char *filename, parameters *config)
{
	PlotFile	plot;
	char		name[BUFFER_SIZE];
	char*		title = NULL;
	double		dBFS = config->maxDbPlotZC;

	if(!config)
		return;

	if(!amplDiff)
		return;

	sprintf(name, "DA__ALL_%s", filename);
	FillPlot(&plot, name, config->startHzPlot, -1*dBFS, config->endHzPlot, dBFS, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, config);

	for(int a = 0; a < size; a++)
	{
		if((channel == CHANNEL_STEREO || channel == amplDiff[a].channel) &&
			amplDiff[a].type > TYPE_CONTROL && fabs(amplDiff[a].diffAmplitude) <= fabs(dBFS))
		{ 
			long int intensity;

			// If channel is defined as noise, don't draw the lower than visible ones
			if(amplDiff[a].refAmplitude > config->significantAmplitude)
			{
				intensity = CalculateWeightedError((fabs(config->significantAmplitude) - fabs(amplDiff[a].refAmplitude))/fabs(config->significantAmplitude), config)*0xffff;
	
				SetPenColor(amplDiff[a].color, intensity, &plot);
				pl_fpoint_r(plot.plotter, transformtoLog(amplDiff[a].hertz, config), amplDiff[a].diffAmplitude);
			}
		}
	}

	if (channel == CHANNEL_STEREO)
		title = DIFFERENCE_TITLE;
	else
		title = channel == CHANNEL_LEFT ? DIFFERENCE_TITLE_LEFT : DIFFERENCE_TITLE_RIGHT;
	DrawColorAllTypeScale(&plot, MODE_DIFF, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, config->significantAmplitude, VERT_SCALE_STEP_BAR, DRAW_BARS, config);
	DrawLabelsMDF(&plot, title, ALL_LABEL, PLOT_COMPARE, config);

	ClosePlot(&plot);
}

int PlotEachTypeDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config)
{
	int 		i = 0, type = 0, types = 0, typeCount = 0, areBothStereo = 0;
	char		name[BUFFER_SIZE];

	areBothStereo = config->referenceSignal->AudioChannels == 2 && config->comparisonSignal->AudioChannels == 2;
	typeCount = GetActiveBlockTypesNoRepeat(config);
	for(i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;

		if(type > TYPE_CONTROL && !config->types.typeArray[i].IsaddOnData)
		{
			char	*returnFolder = NULL;
		
			if(typeCount > 1)
			{
				returnFolder = PushFolder(DIFFERENCE_FOLDER);
				if(!returnFolder)
					return 0;
			}

			sprintf(name, "DA_%s_%02d%s", filename, 
				type, config->types.typeArray[i].typeName);
			PlotSingleTypeDifferentAmplitudes(amplDiff, size, type, name, CHANNEL_STEREO, config);
			logmsg(PLOT_ADVANCE_CHAR);

			if(config->types.typeArray[i].channel == CHANNEL_STEREO && areBothStereo)
			{
				sprintf(name, "DA_%s_%02d%s_%c", filename,
					type, config->types.typeArray[i].typeName, CHANNEL_LEFT);
				PlotSingleTypeDifferentAmplitudes(amplDiff, size, type, name, CHANNEL_LEFT, config);
				logmsg(PLOT_ADVANCE_CHAR);

				sprintf(name, "DA_%s_%02d%s_%c", filename,
					type, config->types.typeArray[i].typeName, CHANNEL_RIGHT);
				PlotSingleTypeDifferentAmplitudes(amplDiff, size, type, name, CHANNEL_RIGHT, config);
				logmsg(PLOT_ADVANCE_CHAR);
			}
			if(typeCount > 1)
				ReturnToMainPath(&returnFolder);

			types ++;
		}
	}
	return types;
}

void PlotSingleTypeDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, int type, char *filename, char channel, parameters *config)
{
	PlotFile	plot;
	char		*title = NULL;
	double		dBFS = config->maxDbPlotZC;

	if(!config)
		return;

	if(!amplDiff)
		return;

	FillPlot(&plot, filename, config->startHzPlot, -1*dBFS, config->endHzPlot, dBFS, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, config);

	for(int a = 0; a < size; a++)
	{
		if((channel == CHANNEL_STEREO || channel == amplDiff[a].channel) &&
			amplDiff[a].hertz && amplDiff[a].type == type && fabs(amplDiff[a].diffAmplitude) <= fabs(dBFS) &&
			fabs(amplDiff[a].refAmplitude) <= fabs(config->significantAmplitude))  // should not be needed if data is correct
		{ 
			long int intensity;

			intensity = CalculateWeightedError((fabs(config->significantAmplitude) - fabs(amplDiff[a].refAmplitude))/fabs(config->significantAmplitude), config)*0xffff;

			SetPenColor(amplDiff[a].color, intensity, &plot);
			pl_fpoint_r(plot.plotter, transformtoLog(amplDiff[a].hertz, config), amplDiff[a].diffAmplitude);
		}
	}

	if(channel == CHANNEL_STEREO)
		title = DIFFERENCE_TITLE;
	else
		title = channel == CHANNEL_LEFT ? DIFFERENCE_TITLE_LEFT : DIFFERENCE_TITLE_RIGHT;
	DrawColorScale(&plot, type, MODE_DIFF,
		LEFT_MARGIN, HEIGHT_MARGIN, 
		config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15,
		0, config->significantAmplitude, VERT_SCALE_STEP_BAR, config);
	DrawLabelsMDF(&plot, title, GetTypeDisplayName(config, type), PLOT_COMPARE, config);
	ClosePlot(&plot);
}

/*
int PlotNoiseDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config, AudioSignal *Signal)
{
	int 		i = 0, type = 0;
	char		name[BUFFER_SIZE];

	for(i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;
		if(type == TYPE_SILENCE)
		{
			sprintf(name, "NF_%s_%d_%02d%s", filename, i,
				type, config->types.typeArray[i].typeName);
			PlotSilenceBlockDifferentAmplitudes(amplDiff, size, type, name, config, Signal);
			logmsg(PLOT_ADVANCE_CHAR);
			return 1;  // we only plot once
		}
	}
	return 0;
}

void PlotSilenceBlockDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, int type, char *filename, parameters *config, AudioSignal *Signal)
{
	PlotFile	plot;
	double		dBFS = config->maxDbPlotZC;
	double		startAmplitude = config->referenceNoiseFloor, endAmplitude = config->lowestDBFS;

	if(!config)
		return;

	if(!amplDiff)
		return;

	// Find limits
	for(int a = 0; a < size; a++)
	{
		if(amplDiff[a].type == type)
		{
			if(fabs(amplDiff[a].diffAmplitude) > dBFS)
				dBFS = fabs(amplDiff[a].diffAmplitude);
			if(amplDiff[a].refAmplitude > startAmplitude)
				startAmplitude = amplDiff[a].refAmplitude;
			if(amplDiff[a].refAmplitude < endAmplitude)
				endAmplitude = amplDiff[a].refAmplitude;
		}
	}

	if(endAmplitude < NS_LOWEST_AMPLITUDE)
		endAmplitude = NS_LOWEST_AMPLITUDE;

	FillPlot(&plot, filename, config->startHzPlot, -1*dBFS, config->endHzPlot, dBFS, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, config);

	DrawNoiseLines(&plot, 0, endAmplitude, Signal, config);
	DrawLabelsNoise(&plot, config->endHzPlot, Signal, config);

	for(int a = 0; a < size; a++)
	{
		if(amplDiff[a].hertz && amplDiff[a].type == type && fabs(amplDiff[a].diffAmplitude) <= fabs(dBFS))
		{
			if(amplDiff[a].refAmplitude > endAmplitude)
			{
				long int intensity;
	
				intensity = CalculateWeightedError(1.0  -(fabs(amplDiff[a].refAmplitude)-fabs(startAmplitude))/(fabs(endAmplitude)-fabs(startAmplitude)), config)*0xffff;
				//intensity = 0xffff;

				SetPenColor(amplDiff[a].color, intensity, &plot);
				pl_fpoint_r(plot.plotter, transformtoLog(amplDiff[a].hertz, config), amplDiff[a].diffAmplitude);
			}
		}
	}

	DrawColorScale(&plot, type, MODE_DIFF,
		LEFT_MARGIN, HEIGHT_MARGIN, 
		config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15,
		(int)startAmplitude, (int)(endAmplitude-startAmplitude), VERT_SCALE_STEP_BAR, config);

	DrawLabelsMDF(&plot, NOISE_TITLE, GetTypeDisplayName(config, type), PLOT_COMPARE, config);
	ClosePlot(&plot);
}
*/

void PlotAllSpectrogram(FlatFrequency *freqs, long int size, char *filename, int signal, parameters *config)
{
	PlotFile plot;
	char	 name[BUFFER_SIZE];
	double	 significant = 0, abs_significant = 0;

	if(!config)
		return;

	significant = config->significantAmplitude;
	abs_significant = fabs(significant);

	sprintf(name, "SP__ALL_%c_%s", signal == ROLE_REF ? 'A' : 'B', filename);
	FillPlot(&plot, name, config->startHzPlot, significant, config->endHzPlot, 0.0, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroToLimit(&plot, significant, VERT_SCALE_STEP, config->endHzPlot, 1000, 0, config);
	DrawLabelsZeroToLimit(&plot, significant, VERT_SCALE_STEP, config->endHzPlot, 0, config);

	if(size)
	{
		for(int f = size-1; f >= 0; f--)
		{
			if(freqs[f].type > TYPE_CONTROL && freqs[f].amplitude > significant && freqs[f].hertz)
			{ 
				long int intensity;
				double x, y;
	
				x = transformtoLog(freqs[f].hertz, config);
				y = freqs[f].amplitude;
				intensity = CalculateWeightedError((abs_significant - fabs(y))/abs_significant, config)*0xffff;
		
				SetPenColor(freqs[f].color, intensity, &plot);
				pl_fline_r(plot.plotter, x,	y, x, significant);
				pl_endpath_r(plot.plotter);
			}
		}
	}

	DrawColorAllTypeScale(&plot, MODE_SPEC, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, significant, VERT_SCALE_STEP_BAR, DRAW_BARS, config);
	DrawLabelsMDF(&plot, signal == ROLE_REF ? SPECTROGRAM_TITLE_REF : SPECTROGRAM_TITLE_COM, ALL_LABEL, signal == ROLE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);
	ClosePlot(&plot);
}

int PlotEachTypeSpectrogram(FlatFrequency* freqs, long int size, char* filename, int signal, parameters* config, AudioSignal* Signal)
{
	int 		i = 0, types = 0;
	char		name[BUFFER_SIZE];

	for (i = 0; i < config->types.typeCount; i++)
	{
		if (config->types.typeArray[i].type > TYPE_CONTROL && !config->types.typeArray[i].IsaddOnData)
		{
			sprintf(name, "SP_%c_%s_%02d%s", signal == ROLE_REF ? 'A' : 'B', filename,
				config->types.typeArray[i].type, config->types.typeArray[i].typeName);
			PlotSingleTypeSpectrogram(freqs, size, config->types.typeArray[i].type, name, signal, CHANNEL_STEREO, config);
			logmsg(PLOT_ADVANCE_CHAR);

			if (config->types.typeArray[i].channel == CHANNEL_STEREO && Signal->AudioChannels == 2)
			{
				sprintf(name, "SP_%c_%s_%02d%s_%c", signal == ROLE_REF ? 'A' : 'B', filename,
					config->types.typeArray[i].type, config->types.typeArray[i].typeName,
					CHANNEL_LEFT);
				PlotSingleTypeSpectrogram(freqs, size, config->types.typeArray[i].type, name, signal, CHANNEL_LEFT, config);
				logmsg(PLOT_ADVANCE_CHAR);

				sprintf(name, "SP_%c_%s_%02d%s_%c", signal == ROLE_REF ? 'A' : 'B', filename,
					config->types.typeArray[i].type, config->types.typeArray[i].typeName,
					CHANNEL_RIGHT);
				PlotSingleTypeSpectrogram(freqs, size, config->types.typeArray[i].type, name, signal, CHANNEL_RIGHT, config);
				logmsg(PLOT_ADVANCE_CHAR);
			}
			types++;
		}
	}

	return types;
}

int PlotNoiseFloorSpectrogram(FlatFrequency * freqs, long int size, char* filename, int signal, parameters * config, AudioSignal * Signal)
{
	int		i = 0, type = 0;
	char	name[BUFFER_SIZE];

	for (i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;
		if(type == TYPE_SILENCE)
		{
			sprintf(name, "NF_SP_%c_%s_%02d%s", signal == ROLE_REF ? 'A' : 'B', filename, 
					config->types.typeArray[i].type, config->types.typeArray[i].typeName);
			PlotNoiseSpectrogram(freqs, size, type, CHANNEL_STEREO, name, signal, config, Signal);
			logmsg(PLOT_ADVANCE_CHAR);

			if (config->types.typeArray[i].channel == CHANNEL_STEREO && Signal->AudioChannels == 2)
			{
				sprintf(name, "NF_SP_%c_%s_%02d%s_%c", signal == ROLE_REF ? 'A' : 'B', filename,
					config->types.typeArray[i].type, config->types.typeArray[i].typeName,
					CHANNEL_LEFT);
				PlotNoiseSpectrogram(freqs, size, type, CHANNEL_LEFT, name, signal, config, Signal);
				logmsg(PLOT_ADVANCE_CHAR);

				sprintf(name, "NF_SP_%c_%s_%02d%s_%c", signal == ROLE_REF ? 'A' : 'B', filename,
					config->types.typeArray[i].type, config->types.typeArray[i].typeName,
					CHANNEL_RIGHT);
				PlotNoiseSpectrogram(freqs, size, type, CHANNEL_RIGHT, name, signal, config, Signal);
				logmsg(PLOT_ADVANCE_CHAR);
			}
			return 1;
		}
	}
	return 0;
}

void PlotSingleTypeSpectrogram(FlatFrequency *freqs, long int size, int type, char *filename, int signal, char channel, parameters *config)
{
	char		*title = NULL;
	PlotFile	plot;
	double		significant = 0, abs_significant = 0;

	if(!config)
		return;

	significant = config->significantAmplitude;
	abs_significant = fabs(significant);

	FillPlot(&plot, filename, config->startHzPlot, significant, config->endHzPlot, 0.0, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroToLimit(&plot, significant, VERT_SCALE_STEP,config->endHzPlot, 1000, 0, config);
	DrawLabelsZeroToLimit(&plot, significant, VERT_SCALE_STEP,config->endHzPlot, 0, config);

	for(int f = 0; f < size; f++)
	{
		if(freqs[f].type == type && (channel == CHANNEL_STEREO || freqs[f].channel == channel) && 
			freqs[f].amplitude > significant && freqs[f].hertz)
		{ 
			long int intensity;
			double x, y;

			x = transformtoLog(freqs[f].hertz, config);
			y = freqs[f].amplitude;
			intensity = CalculateWeightedError((abs_significant - fabs(y)) / abs_significant, config) * 0xffff;

			//pl_flinewidth_r(plot.plotter, 100*range_0_1);
			SetPenColor(freqs[f].color, intensity, &plot);
			pl_fline_r(plot.plotter, x, y, x, significant);
			pl_endpath_r(plot.plotter);
		}
	}
	
	if(signal == ROLE_REF)
	{
		if(channel == CHANNEL_STEREO)
			title = SPECTROGRAM_TITLE_REF;
		else
			title = channel == CHANNEL_LEFT ? SPECTROGRAM_TITLE_REF_LEFT : SPECTROGRAM_TITLE_REF_RIGHT;
	}
	else
	{
		if(channel == CHANNEL_STEREO)
			title = SPECTROGRAM_TITLE_COM;
		else
			title = channel == CHANNEL_LEFT ? SPECTROGRAM_TITLE_COM_LEFT : SPECTROGRAM_TITLE_COM_RIGHT;
	}
	DrawColorScale(&plot, type, MODE_SPEC, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, 0, significant, VERT_SCALE_STEP,config);
	DrawLabelsMDF(&plot, title, GetTypeDisplayName(config, type), signal == ROLE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);
	ClosePlot(&plot);
}

void PlotNoiseSpectrogram(FlatFrequency *freqs, long int size, int type, char channel, char *filename, int signal, parameters *config, AudioSignal *Signal)
{
	PlotFile	plot;
	char*		title = NULL;
	double		startAmplitude = config->significantAmplitude, endAmplitude = config->lowestDBFS;

	if(!config)
		return;

	// Find limits
	for(int f = 0; f < size; f++)
	{
		if(freqs[f].type == type)
		{
			if (freqs[f].amplitude > startAmplitude)
				startAmplitude = freqs[f].amplitude;
			if (freqs[f].amplitude < endAmplitude)
				endAmplitude = freqs[f].amplitude;
		}
	}

	if(endAmplitude < NS_LOWEST_AMPLITUDE)
		endAmplitude = NS_LOWEST_AMPLITUDE;

	if (Signal->role == ROLE_REF)
	{
		config->refNoiseMin = startAmplitude;
		config->refNoiseMax = endAmplitude;
	}

	if(Signal->role == ROLE_COMP)
	{
		if(config->refNoiseMax != 0)
			endAmplitude = config->refNoiseMax;
		else
			logmsg("WARNING: Noise Floor Reference values were not set\n");
	}

	if(config->significantAmplitude < endAmplitude)
		endAmplitude = config->significantAmplitude;

	FillPlot(&plot, filename, config->startHzPlot, endAmplitude, config->endHzPlot, 0.0, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroToLimit(&plot, endAmplitude, VERT_SCALE_STEP, config->endHzPlot, 1000, 1, config);
	DrawLabelsZeroToLimit(&plot, endAmplitude, VERT_SCALE_STEP, config->endHzPlot, 1, config);

	DrawNoiseLines(&plot, 0, endAmplitude, Signal, config);
	DrawLabelsNoise(&plot, config->endHzPlot, Signal, config);

	for(int f = 0; f < size; f++)
	{
		if((channel == CHANNEL_STEREO || freqs[f].channel == channel) && 
			freqs[f].type == type && freqs[f].amplitude >= endAmplitude &&
			freqs[f].hertz)
		{ 
			long int intensity;
			double x, y;

			x = transformtoLog(freqs[f].hertz, config);
			y = freqs[f].amplitude;
			intensity = CalculateWeightedError(((fabs(endAmplitude) - fabs(startAmplitude)) - (fabs(freqs[f].amplitude)- fabs(startAmplitude)))/(fabs(endAmplitude)-fabs(startAmplitude)), config)*0xffff;
			
			//pl_flinewidth_r(plot.plotter, 100*range_0_1);
			SetPenColor(freqs[f].color, intensity, &plot);
			pl_fline_r(plot.plotter, x, y, x, endAmplitude);
			pl_endpath_r(plot.plotter);
		}
	}
	
	if (signal == ROLE_REF)
	{
		if (channel == CHANNEL_STEREO)
			title = SPECTROGRAM_NOISE_REF;
		else
			title = channel == CHANNEL_LEFT ? SPECTROGRAM_NOISE_REF_LEFT : SPECTROGRAM_NOISE_REF_RIGHT;
	}
	else
	{
		if (channel == CHANNEL_STEREO)
			title = SPECTROGRAM_NOISE_COM;
		else
			title = channel == CHANNEL_LEFT ? SPECTROGRAM_NOISE_COM_LEFT : SPECTROGRAM_NOISE_COM_RIGHT;
	}
	DrawColorScale(&plot, type, MODE_SPEC, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, (int)startAmplitude, (int)(endAmplitude-startAmplitude), VERT_SCALE_STEP,config);
	DrawLabelsMDF(&plot, title, GetTypeDisplayName(config, type), signal == ROLE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);
	ClosePlot(&plot);
}

void VisualizeWindows(windowManager *wm, parameters *config)
{
	if(!wm)
		return;

	for(int i = 0; i < wm->windowCount; i++)
	{
		//logmsg("Factor len %ld: %g\n", wm->windowArray[i].frames,
			//CalculateCorrectionFactor(wm, wm->windowArray[i].frames));

		//for(long int j = 0; j < wm->windowArray[i].size; j++)
			//logmsg("Window %ld %g\n", j, wm->windowArray[i].window[j]);

		PlotWindow(&wm->windowArray[i], config);
	}
}

void PlotWindow(windowUnit *windowUnit, parameters *config)
{
	PlotFile plot;
	char	 name[BUFFER_SIZE];
	double 	 *window = NULL, frames;
	long int size;

	if(!config || !windowUnit)
		return;

	window = windowUnit->window;
	frames = windowUnit->frames;
	size = windowUnit->size;

	sprintf(name, "WindowPlot_%s", GetWindow(config->window));
	FillPlotExtra(&plot, name, 320, 384, 0, -0.1, 1, 1.1, 0.001, 0, config);

	if(!CreatePlotFile(&plot, config))
		return;

	// Frames Grid
	pl_pencolor_r (plot.plotter, 0, 0x3333, 0);
	for(long int i = 0; i < frames; i++)
		pl_fline_r(plot.plotter, (double)i*1/(double)frames, -0.1, (double)i*1/(double)frames, 1.1);

	// horizontal grid
	pl_pencolor_r (plot.plotter, 0, 0x5555, 0);
	pl_fline_r(plot.plotter, 0, 1, 1, 1);
	pl_fline_r(plot.plotter, 0, 0, 1, 0);
	pl_endpath_r(plot.plotter);

	pl_pencolor_r (plot.plotter, 0, 0xFFFF, 0);	
	for(int i = 0; i < size; i++)
		pl_fpoint_r(plot.plotter, (double)i/(double)size, window[i]);
	
	ClosePlot(&plot);
}

void PlotBetaFunctions(parameters *config)
{
	char	 name[BUFFER_SIZE];
	int		 type = 0;

	if(!config)
		return;

	for(type = 0; type <= 5; type ++)
	{
		PlotFile plot;

		config->outputFilterFunction = type;
		sprintf(name, "BetaFunctionPlot_%d", type);
		FillPlotExtra(&plot, name, 320, 384, 0, -0.1, 1, 1.1, 0.001, 0, config);
	
		if(!CreatePlotFile(&plot, config))
			return;
	
		pl_pencolor_r (plot.plotter, 0, 0x5555, 0);
		pl_fline_r(plot.plotter, 0, 1, 1, 1);
		pl_fline_r(plot.plotter, 0, 0, 1, 0);

		pl_pencolor_r (plot.plotter, 0, 0x3333, 0);
		pl_fline_r(plot.plotter, .5, -0.1, .5, 1.1);
		pl_fline_r(plot.plotter, .25, -0.1, .25, 1.1);
		pl_fline_r(plot.plotter, .75, -0.1, .75, 1.1);

		pl_fline_r(plot.plotter, 0, .5, 1, .5);
		pl_fline_r(plot.plotter, 0, .25, 1, .25);
		pl_fline_r(plot.plotter, 0, .75, 1, .75);
		pl_endpath_r(plot.plotter);
	
		pl_pencolor_r (plot.plotter, 0, 0xFFFF, 0);	
		for(int i = 0; i < 320; i++)
		{
			double x, y;
			long int color;

			x = (double)i/(double)320;
			y = CalculateWeightedError((double)i/(double)320, config);
			
			color = y*0xffff;
	
			SetPenColor(COLOR_AQUA, color, &plot);
			//logmsg("x: %g (%g) y: %g (%g) c:%ld\n", x, x*60, y, y*60, color);
			pl_fpoint_r(plot.plotter, x, y);
		}
		
		ClosePlot(&plot);
	}
}

int MatchColor(char *color)
{
	unsigned int	i = 0, len = 0;
	char			colorcopy[640];

	strncpy(colorcopy, color, 512);

	len = strlen(colorcopy);
	for(i = 0; i < len; i++)
		colorcopy[i] = tolower(colorcopy[i]);

	if(strcmp(colorcopy, "red") == 0)
		return(COLOR_RED);
	if(strcmp(colorcopy, "green") == 0)
		return(COLOR_GREEN);
	if(strcmp(colorcopy, "blue") == 0)
		return(COLOR_BLUE);
	if(strcmp(colorcopy, "yellow") == 0)
		return(COLOR_YELLOW);
	if(strcmp(colorcopy, "magenta") == 0)
		return(COLOR_MAGENTA);
	if(strcmp(colorcopy, "aqua") == 0 || strcmp(color, "aquamarine") == 0)
		return(COLOR_AQUA);
	if(strcmp(colorcopy, "orange") == 0)
		return(COLOR_ORANGE);
	if(strcmp(colorcopy, "purple") == 0)
		return(COLOR_PURPLE);
	if(strcmp(colorcopy, "gray") == 0)
		return(COLOR_GRAY);
	if(strcmp(colorcopy, "null") == 0)
		return(COLOR_NULL);

	logmsg("Unmatched color %s, using green\n", color);
	return COLOR_GREEN;
}

void SetPenColorStr(char *colorName, long int color, PlotFile *plot)
{
	SetPenColor(MatchColor(colorName), color, plot);
}

void SetPenColor(int colorIndex, long int color, PlotFile *plot)
{
	switch(colorIndex)
	{
		case COLOR_RED:
			pl_pencolor_r(plot->plotter, color, 0, 0);
			break;
		case COLOR_GREEN:
			pl_pencolor_r(plot->plotter, 0, color, 0);
			break;
		case COLOR_BLUE:
			pl_pencolor_r(plot->plotter, 0, 0, color);
			break;
		case COLOR_YELLOW:
			pl_pencolor_r(plot->plotter, color, color, 0);
			break;
		case COLOR_AQUA:
			pl_pencolor_r(plot->plotter, 0, color, color);
			break;
		case COLOR_MAGENTA:
			pl_pencolor_r(plot->plotter, color, 0, color);
			break;
		case COLOR_PURPLE:
			pl_pencolor_r(plot->plotter, color/2, 0, color);
			break;
		case COLOR_ORANGE:
			pl_pencolor_r(plot->plotter, color, color/2, 0);
			break;
		case COLOR_GRAY:
			pl_pencolor_r(plot->plotter, color, color, color);
			break;
		case COLOR_NULL:
			pl_pencolor_r(plot->plotter, 0, 0, 0);
			break;
		default:
			pl_pencolor_r(plot->plotter, 0, color, 0);
			break;
	}
}

void SetFillColor(int colorIndex, long int color, PlotFile *plot)
{
	switch(colorIndex)
	{
		case COLOR_RED:
			pl_fillcolor_r(plot->plotter, color, 0, 0);
			break;
		case COLOR_GREEN:
			pl_fillcolor_r(plot->plotter, 0, color, 0);
			break;
		case COLOR_BLUE:
			pl_fillcolor_r(plot->plotter, 0, 0, color);
			break;
		case COLOR_YELLOW:
			pl_fillcolor_r(plot->plotter, color, color, 0);
			break;
		case COLOR_AQUA:
			pl_fillcolor_r(plot->plotter, 0, color, color);
			break;
		case COLOR_MAGENTA:
			pl_fillcolor_r(plot->plotter, color, 0, color);
			break;
		case COLOR_PURPLE:
			pl_fillcolor_r(plot->plotter, color/2, 0, color);
			break;
		case COLOR_ORANGE:
			pl_fillcolor_r(plot->plotter, color, color/2, 0);
			break;
		case COLOR_GRAY:
			pl_fillcolor_r(plot->plotter, color, color, color);
			break;
		case COLOR_NULL:
			pl_fillcolor_r(plot->plotter, 0, 0, 0);
			break;
		default:
			pl_fillcolor_r(plot->plotter, 0, color, 0);
			break;
	}
}


FlatAmplDifference *CreateFlatDifferences(parameters *config, long int *size, diffPlotType plotType)
{
	long int			count = 0;
	FlatAmplDifference	*ADiff = NULL;

	if(!config)
		return NULL;

	if(!size)
		return NULL;
	*size = 0;

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		int type = 0, doplot = 0;
		
		type = GetBlockType(config, b);
		if(plotType == normalPlot && type > TYPE_SILENCE)
			doplot = 1;
		if(plotType == floorPlot && type == TYPE_SILENCE)
			doplot = 1;
		if(doplot)
			count += config->Differences.BlockDiffArray[b].cntAmplBlkDiff;
	}

	ADiff = (FlatAmplDifference*)malloc(sizeof(FlatAmplDifference)*count);
	if(!ADiff)
		return NULL;
	memset(ADiff, 0, sizeof(FlatAmplDifference)*count);

	count = 0;
	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		int type = 0, doplot = 0;
		
		type = GetBlockType(config, b);
		if(plotType == normalPlot && type > TYPE_SILENCE)
			doplot = 1;
		if(plotType == floorPlot && type == TYPE_SILENCE)
			doplot = 1;
		if(doplot)
		{
			int		color = 0;

			color = MatchColor(GetBlockColor(config, b));
			for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
			{
				ADiff[count].hertz = config->Differences.BlockDiffArray[b].amplDiffArray[a].hertz;
				ADiff[count].refAmplitude = config->Differences.BlockDiffArray[b].amplDiffArray[a].refAmplitude;
				ADiff[count].diffAmplitude = config->Differences.BlockDiffArray[b].amplDiffArray[a].diffAmplitude;
				ADiff[count].type = type;
				ADiff[count].color = color;
				ADiff[count].channel = config->Differences.BlockDiffArray[b].amplDiffArray[a].channel;
				count ++;
			}
		}
	}
	logmsg(PLOT_PROCESS_CHAR);
	AmplitudeDifferences_tim_sort(ADiff, count);
	logmsg(PLOT_PROCESS_CHAR);
	*size = count;
	return(ADiff);
}

int InsertElementInPlace(FlatFrequency *Freqs, FlatFrequency Element, long int currentsize)
{
	if(!currentsize)
	{
		Freqs[0] = Element;
		return 1;
	}

	for(long int j = 0; j < currentsize; j++)
	{
		if(Element.type == Freqs[j].type && Element.channel == Freqs[j].channel && areDoublesEqual(Element.hertz, Freqs[j].hertz))
		{
			if(Element.amplitude > Freqs[j].amplitude)
				Freqs[j].amplitude = Element.amplitude;
			return 0;
		}
	}

	Freqs[currentsize] = Element;
	return 1;
}

FlatFrequency *CreateFlatFrequencies(AudioSignal *Signal, long int *size, parameters *config)
{
	long int		block = 0, i = 0;
	long int		count = 0, counter = 0;
	FlatFrequency	*Freqs = NULL;
	double			significant = 0;

	if(!size || !Signal || !config)
		return NULL;

	*size = 0;

	significant = config->significantAmplitude;

	for(block = 0; block < config->types.totalBlocks; block++)
	{
		int 	type = TYPE_NOTYPE;

		type = GetBlockType(config, block);

		if(type >= TYPE_SILENCE)
		{
			for(i = 0; i < config->MaxFreq; i++)
			{
				int insert = 0;

				if(Signal->Blocks[block].freq[i].hertz == 0)
					break;

				if(type > TYPE_SILENCE && Signal->Blocks[block].freq[i].hertz && Signal->Blocks[block].freq[i].amplitude > significant)
					insert = 1;
				if(type == TYPE_SILENCE && Signal->Blocks[block].freq[i].hertz)
					insert = 1;

				if(insert)
					count ++;
				else
					break;
			}

			if(Signal->Blocks[block].freqRight)
			{
				for(i = 0; i < config->MaxFreq; i++)
				{
					int insert = 0;
	
					if(Signal->Blocks[block].freqRight[i].hertz == 0)
						break;
	
					if(type > TYPE_SILENCE && Signal->Blocks[block].freqRight[i].hertz && Signal->Blocks[block].freqRight[i].amplitude > significant)
						insert = 1;
					if(type == TYPE_SILENCE && Signal->Blocks[block].freqRight[i].hertz)
						insert = 1;
	
					if(insert)
						count ++;
					else
						break;
				}
			}
		}
	}

	Freqs = (FlatFrequency*)malloc(sizeof(FlatFrequency)*count);
	if(!Freqs)
		return NULL;
	memset(Freqs, 0, sizeof(FlatFrequency)*count);

	for(block = 0; block < config->types.totalBlocks; block++)
	{
		int type = 0, color = 0;

		type = GetBlockType(config, block);
		if(type >= TYPE_SILENCE)
		{
			color = MatchColor(GetBlockColor(config, block));
	
			for(i = 0; i < config->MaxFreq; i++)
			{
				int insert = 0;

				if(type > TYPE_SILENCE && Signal->Blocks[block].freq[i].hertz && Signal->Blocks[block].freq[i].amplitude > significant)
					insert = 1;
				if(type == TYPE_SILENCE && Signal->Blocks[block].freq[i].hertz)
					insert = 1;

				if(insert)
				{
					FlatFrequency tmp;
	
					tmp.hertz = Signal->Blocks[block].freq[i].hertz;
					tmp.amplitude = Signal->Blocks[block].freq[i].amplitude;
					tmp.type = type;
					tmp.color = color;
					tmp.channel = CHANNEL_LEFT;
	
					if(InsertElementInPlace(Freqs, tmp, counter))
						counter ++;
				}
				else
					break;
			}

			if(Signal->Blocks[block].freqRight)
			{
				for(i = 0; i < config->MaxFreq; i++)
				{
					int insert = 0;
	
					if(type > TYPE_SILENCE && Signal->Blocks[block].freqRight[i].hertz && Signal->Blocks[block].freqRight[i].amplitude > significant)
						insert = 1;
					if(type == TYPE_SILENCE && Signal->Blocks[block].freqRight[i].hertz)
						insert = 1;
	
					if(insert)
					{
						FlatFrequency tmp;
		
						tmp.hertz = Signal->Blocks[block].freqRight[i].hertz;
						tmp.amplitude = Signal->Blocks[block].freqRight[i].amplitude;
						tmp.type = type;
						tmp.color = color;
						tmp.channel = CHANNEL_RIGHT;
		
						if(InsertElementInPlace(Freqs, tmp, counter))
							counter ++;
					}
					else
						break;
				}
			}
		}
	}
	
	logmsg(PLOT_PROCESS_CHAR);
	FlatFrequenciesByAmplitude_tim_sort(Freqs, counter);
	logmsg(PLOT_PROCESS_CHAR);

	*size = counter;
	return(Freqs);
}


void PlotTest(char *filename, parameters *config)
{
	PlotFile	plot;
	char		name[BUFFER_SIZE];
	double		dBFS = config->maxDbPlotZC;

	if(!config)
		return;

	sprintf(name, "Test_%s", filename);
	FillPlot(&plot, name, config->startHzPlot, -1*dBFS, config->endHzPlot, dBFS, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, config);

	srand(time(NULL));

	DrawLabelsMDF(&plot, "PLOT TEST [%s]", "ZDBC", PLOT_COMPARE, config);
	DrawColorAllTypeScale(&plot, MODE_DIFF, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, config->significantAmplitude, VERT_SCALE_STEP_BAR, DRAW_BARS, config);

	ClosePlot(&plot);
}

void PlotTestZL(char *filename, parameters *config)
{
	PlotFile	plot;
	char		name[BUFFER_SIZE];

	if(!config)
		return;

	sprintf(name, "Test_ZL_%s", filename);
	FillPlot(&plot, name, config->startHzPlot, config->significantAmplitude, config->endHzPlot, 0.0, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroToLimit(&plot, config->significantAmplitude, VERT_SCALE_STEP, config->endHzPlot, 1000, 0, config);
	DrawLabelsZeroToLimit(&plot, config->significantAmplitude, VERT_SCALE_STEP, config->endHzPlot, 0, config);

	DrawColorScale(&plot, 1, MODE_SPEC, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, 0, SIGNIFICANT_VOLUME, VERT_SCALE_STEP_BAR, config);
	
	DrawLabelsMDF(&plot, "PLOT TEST [%s]", "GZL", PLOT_COMPARE, config);
	ClosePlot(&plot);
}

inline double transformtoLog(double coord, parameters *config)
{
	if(coord <= 0)
	{
		logmsg("WARNING: transformtoLog received %g\n", coord);
		return 0;
	}
	if(config->logScale)
		return(config->plotRatio*log10(coord));
	else
		return(coord);
}

/* =============Best Fit ================ */

long int movingAverage(AveragedFrequencies *data, AveragedFrequencies *averages, long int size, long int period)
{
	long int 			current_index = 0, i = 0, pos = 0;
	AveragedFrequencies *periodArray = NULL;

	periodArray = (AveragedFrequencies*)malloc(sizeof(AveragedFrequencies)*period);
	if(!periodArray)
		return 0;
	
	memset(periodArray, 0, sizeof(AveragedFrequencies)*period);

	for(i = 0; i < size; i++)
	{
		AveragedFrequencies	ma;

		ma.avgfreq = 0;
		ma.avgvol = 0;
		periodArray[current_index].avgfreq = data[i].avgfreq/(double)period;
		periodArray[current_index].avgvol = data[i].avgvol/(double)period;
 		for(long int j = 0; j < period; j++)
		{
			ma.avgfreq += periodArray[j].avgfreq;
			ma.avgvol += periodArray[j].avgvol;
		}
		if(i >= period)
			averages[pos++] = ma;
		current_index = (current_index + 1) % period;
	}

	free(periodArray);
	periodArray = NULL;
	return pos;
}

/*
//This version is more strict, average out stuff that is within 1000 hz
long int movingAverage(AveragedFrequencies *data, AveragedFrequencies *averages, long int size, long int period)
{
	long int 			current_index = 0, i = 0, pos = 0, count = 0;
	AveragedFrequencies *periodArray = NULL;

	periodArray = (AveragedFrequencies*)malloc(sizeof(AveragedFrequencies)*period);
	if(!periodArray)
		return 0;
	
	memset(periodArray, 0, sizeof(AveragedFrequencies)*period);

	for(i = 1; i < size; i++)
	{
		AveragedFrequencies	ma;

		ma.avgfreq = 0;
		ma.avgvol = 0;
		// we only average out stuff that is within 1000 hz
		if(fabs(data[i].avgfreq - data[i-1].avgfreq) < 1000.0)
		{
			periodArray[current_index].avgfreq = data[i].avgfreq/(double)period;
			periodArray[current_index].avgvol = data[i].avgvol/(double)period;
 			for(long int j = 0; j < period; j++)
			{
				ma.avgfreq += periodArray[j].avgfreq;
				ma.avgvol += periodArray[j].avgvol;
			}
			if(count >= period)
				averages[pos++] = ma;
			current_index = (current_index + 1) % period;
			count ++;
		}
		else
		{
			count = 0;
			memset(periodArray, 0, sizeof(AveragedFrequencies)*period);
			//logmsg("Discarded %g to %g\n", data[i].avgfreq, data[i-1].avgfreq);
		}
	}

	free(periodArray);
	periodArray = NULL;
	return pos;
}
*/

long int AverageDuplicates(AveragedFrequencies *array, long int size)
{
	long int p = 0, finalCount = 0;

	// detect same frequency, with or without different amplitue (for optimization)
	// if they are different, the come form different blocks blocks
	// We need to average these out
	for(p = 1; p < size; p++)
	{
		if(areDoublesEqual(array[p-1].avgfreq, array[p].avgfreq))
		{
			long int	base = 0, elements = 0;
			double		hz = 0, sum = 0, avg = 0;

			base = p - 1;
			hz = array[base].avgfreq;
			while(p < size && areDoublesEqual(hz, array[p].avgfreq))
			{
				sum += array[p].avgvol;
				elements++;
				p++;
			}

			avg = roundFloat(sum/(double)elements);
			array[base].avgvol = avg;

			// move everything down
			array[finalCount] = array[base];
		}
		else
			array[finalCount] = array[p];
		finalCount++;
	}
	return finalCount;
}

AveragedFrequencies *CreateFlatDifferencesAveraged(int matchType, char channel, long int *avgSize, diffPlotType plotType, parameters *config)
{
	long int			count = 0;
	AveragedFrequencies	*averaged = NULL, *averagedSMA = NULL;
	double				significant = 0;

	if(!config)
		return NULL;

	if(!avgSize)
		return NULL;

	*avgSize = 0;
	// this is very important, to remove outliers, we consider only data within 50% of the amplitude
	significant = config->significantAmplitude*0.5;

	// Count how many we have
	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		if(GetBlockType(config, b) == matchType)
		{
			for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
			{
				if(config->Differences.BlockDiffArray[b].amplDiffArray[a].hertz > 0
					&& (channel == CHANNEL_STEREO || config->Differences.BlockDiffArray[b].amplDiffArray[a].channel == channel))
				{
					count++;
				}
			}
		}
	}

	if(!count)
		return NULL;

	averaged = (AveragedFrequencies*)malloc(sizeof(AveragedFrequencies)*count);
	if(!averaged)
		return NULL;
	memset(averaged, 0, sizeof(AveragedFrequencies)*count);

	count = 0;
	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		int type = 0;
		
		type = config->Differences.BlockDiffArray[b].type;
		if(type == matchType)
		{
			double		startAmplitude = config->referenceNoiseFloor, endAmplitude = config->lowestDBFS;

			if(plotType == floorPlot)
			{
				// Find limits
				for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
				{
					if(config->Differences.BlockDiffArray[b].amplDiffArray[a].hertz > 0
						&& (channel == CHANNEL_STEREO || config->Differences.BlockDiffArray[b].amplDiffArray[a].channel == channel))
					{
						if(config->Differences.BlockDiffArray[b].amplDiffArray[a].refAmplitude > startAmplitude)
							startAmplitude = config->Differences.BlockDiffArray[b].amplDiffArray[a].refAmplitude;
						if(config->Differences.BlockDiffArray[b].amplDiffArray[a].refAmplitude < endAmplitude)
							endAmplitude = config->Differences.BlockDiffArray[b].amplDiffArray[a].refAmplitude;
					}
				}

				if(endAmplitude < NS_LOWEST_AMPLITUDE)
					endAmplitude = NS_LOWEST_AMPLITUDE;
				significant = endAmplitude;
			}

			for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
			{
				if(config->Differences.BlockDiffArray[b].amplDiffArray[a].refAmplitude > significant
					&& (channel == CHANNEL_STEREO || config->Differences.BlockDiffArray[b].amplDiffArray[a].channel == channel))
				{
					averaged[count].avgfreq = config->Differences.BlockDiffArray[b].amplDiffArray[a].hertz;
					averaged[count].avgvol = config->Differences.BlockDiffArray[b].amplDiffArray[a].diffAmplitude;
					count ++;
				}
			}
		}
	}

	if(!count)
	{
		free(averaged);
		return NULL;
	}

	AverageByFrequency_tim_sort(averaged, count);
	count = AverageDuplicates(averaged, count);

	averagedSMA = (AveragedFrequencies*)malloc(sizeof(AveragedFrequencies)*count);
	if(!averagedSMA)
	{
		free(averaged);
		return NULL;
	}

	memset(averagedSMA, 0, sizeof(AveragedFrequencies)*count);

	logmsg(PLOT_PROCESS_CHAR);

	*avgSize = movingAverage(averaged, averagedSMA, count, plotType == floorPlot ? 50 : 4);

	free(averaged);
	averaged = NULL;

	return(averagedSMA);
}

int PlotDifferentAmplitudesAveraged(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config)
{
	int 				i = 0, type = 0, typeCount = 0, types = 0, bothStereo = 0;
	char				name[BUFFER_SIZE];
	long int			*averagedSizes = NULL;
	AveragedFrequencies	**averagedArray = NULL;

	typeCount = GetActiveBlockTypesNoRepeat(config);
	bothStereo = config->referenceSignal->AudioChannels == 2 && config->comparisonSignal->AudioChannels == 2;

	averagedArray = (AveragedFrequencies**)malloc(sizeof(AveragedFrequencies*)*typeCount);
	if(!averagedArray)
		return 0;

	averagedSizes = (long int*)malloc(sizeof(long int)*typeCount);
	if(!averagedSizes)
		return 0;

	memset(averagedArray, 0, sizeof(AveragedFrequencies*)*typeCount);
	memset(averagedSizes, 0, sizeof(long int)*typeCount);

	for(i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;

		if(type > TYPE_CONTROL && !config->types.typeArray[i].IsaddOnData)
		{
			if(typeCount == 1)
				sprintf(name, "DA__ALL_%s_AVG", filename);
			else
				sprintf(name, "DA_%s_%02d%s_AVG", filename, 
					config->types.typeArray[i].type, config->types.typeArray[i].typeName);

			averagedArray[types] = CreateFlatDifferencesAveraged(type, CHANNEL_STEREO, &averagedSizes[types], normalPlot, config);

			if(averagedArray[types])
			{
				char	*returnFolder = NULL;

				if(typeCount > 1)
				{
					returnFolder = PushFolder(DIFFERENCE_FOLDER);
					if(!returnFolder)
						return 0;
				}

				PlotSingleTypeDifferentAmplitudesAveraged(amplDiff, size, type, name, averagedArray[types], averagedSizes[types], config->types.typeArray[i].channel == CHANNEL_STEREO ? CHANNEL_STEREO : CHANNEL_MONO, config);
				logmsg(PLOT_ADVANCE_CHAR);

				if(config->types.typeArray[i].channel == CHANNEL_STEREO && bothStereo)
				{
					long int sizeLeft = 0, sizeRight = 0;
					AveragedFrequencies	*averagedArrayLeft = NULL, *averagedArrayRight = NULL;

					averagedArrayLeft = CreateFlatDifferencesAveraged(type, CHANNEL_LEFT, &sizeLeft, normalPlot, config);
					if(typeCount == 1)
						sprintf(name, "DA__ALL_%s_%c_AVG", filename, CHANNEL_LEFT);
					else
						sprintf(name, "DA_%s_%02d%s_%c_AVG", filename, 
							config->types.typeArray[i].type, config->types.typeArray[i].typeName, CHANNEL_LEFT);
					PlotSingleTypeDifferentAmplitudesAveraged(amplDiff, size, type, name, averagedArrayLeft, sizeLeft, CHANNEL_LEFT, config);
					logmsg(PLOT_ADVANCE_CHAR);
					free(averagedArrayLeft);

					averagedArrayRight = CreateFlatDifferencesAveraged(type, CHANNEL_RIGHT, &sizeRight, normalPlot, config);
					if(typeCount == 1)
						sprintf(name, "DA__ALL_%s_%c_AVG", filename, CHANNEL_RIGHT);
					else
						sprintf(name, "DA_%s_%02d%s_%c_AVG", filename, 
							config->types.typeArray[i].type, config->types.typeArray[i].typeName, CHANNEL_RIGHT);
					PlotSingleTypeDifferentAmplitudesAveraged(amplDiff, size, type, name, averagedArrayRight, sizeRight, CHANNEL_RIGHT, config);
					logmsg(PLOT_ADVANCE_CHAR);

					free(averagedArrayRight);
				}

				if(typeCount > 1)
					ReturnToMainPath(&returnFolder);
			}

			types ++;
		}
	}

	if(types > 1 && averagedArray && averagedSizes)
	{
		sprintf(name, "DA__ALL_AVG_%s", filename);
		PlotAllDifferentAmplitudesAveraged(amplDiff, size, name, averagedArray, averagedSizes, config);
		logmsg(PLOT_ADVANCE_CHAR);
	}

	for(i = 0; i < typeCount; i++)
	{
		free(averagedArray[i]);
		averagedArray[i]= NULL;
	}

	free(averagedArray);
	averagedArray = NULL;
	free(averagedSizes);
	averagedSizes = NULL;

	return types;
}

int PlotNoiseDifferentAmplitudesAveraged(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config, AudioSignal *Signal)
{
	int 				i = 0;
	char				name[BUFFER_SIZE];
	long int			avgsize = 0;
	AveragedFrequencies	*averagedArray = NULL;

	for(i = 0; i < config->types.typeCount; i++)
	{
		int type = config->types.typeArray[i].type;
		if(type == TYPE_SILENCE)
		{
			sprintf(name, "NF__%s_%02d%s_AVG_", filename, 
					config->types.typeArray[i].type, config->types.typeArray[i].typeName);

			averagedArray = CreateFlatDifferencesAveraged(type, CHANNEL_STEREO, &avgsize, floorPlot, config);

			if(averagedArray)
			{
				PlotNoiseDifferentAmplitudesAveragedInternal(amplDiff, size, type, name, averagedArray, avgsize, config, Signal);
				logmsg(PLOT_ADVANCE_CHAR);
				free(averagedArray);
				averagedArray = NULL;
				return 1;
			}
		}
	}

	return 0;
}

void PlotNoiseDifferentAmplitudesAveragedInternal(FlatAmplDifference *amplDiff, long int size, int type, char *filename, AveragedFrequencies *averaged, long int avgsize, parameters *config, AudioSignal *Signal)
{
	PlotFile	plot;
	double		dbs = config->maxDbPlotZC, vertscale = VERT_SCALE_STEP;;
	int			color = 0;
	double		startAmplitude = config->referenceNoiseFloor, endAmplitude = config->lowestDBFS;

	if(!config)
		return;

	if(!amplDiff)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	// Find limits
	for(int a = 0; a < size; a++)
	{
		if(amplDiff[a].type == type)
		{
			if(fabs(amplDiff[a].diffAmplitude) > dbs)
				dbs = fabs(amplDiff[a].diffAmplitude);
			if(amplDiff[a].refAmplitude > startAmplitude)
				startAmplitude = amplDiff[a].refAmplitude;
			if(amplDiff[a].refAmplitude < endAmplitude)
				endAmplitude = amplDiff[a].refAmplitude;
		}
	}

	if(endAmplitude < NS_LOWEST_AMPLITUDE)
		endAmplitude = NS_LOWEST_AMPLITUDE;

	FillPlot(&plot, filename, config->startHzPlot, -1*dbs, config->endHzPlot, dbs, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	if(dbs > 90)
		vertscale *= 2;
	if(dbs > 200)
		vertscale *= 10;
	DrawGridZeroDBCentered(&plot, dbs, vertscale, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dbs, vertscale, config->endHzPlot, config);

	DrawNoiseLines(&plot, dbs, -1*dbs, Signal, config);
	DrawLabelsNoise(&plot, config->endHzPlot, Signal, config);

	for(long int a = 0; a < size; a++)
	{
		if(amplDiff[a].type == type)
		{ 
			if(amplDiff[a].refAmplitude > endAmplitude)
			{
				long int intensity;
	
				intensity = CalculateWeightedError(1.0  -(fabs(amplDiff[a].refAmplitude)-fabs(startAmplitude))/(fabs(endAmplitude)-fabs(startAmplitude)), config)*0xffff;
	
				SetPenColor(amplDiff[a].color, intensity, &plot);
				pl_fpoint_r(plot.plotter, transformtoLog(amplDiff[a].hertz, config), amplDiff[a].diffAmplitude);
			}
		}
	}

	color = MatchColor(GetTypeColor(config, type));
	pl_endpath_r(plot.plotter);

	if(averaged && avgsize > 1)
	{
		int first = 1;

		pl_flinewidth_r(plot.plotter, 50);
		SetPenColor(COLOR_GRAY, 0x0000, &plot);
		for(long int a = 0; a < avgsize; a++)
		{
			if(first)
			{
				pl_fline_r(plot.plotter, transformtoLog(averaged[a].avgfreq, config), averaged[a].avgvol,
							transformtoLog(averaged[a+1].avgfreq, config), averaged[a+1].avgvol);
				first = 0;
			}
			else
				pl_fcont_r(plot.plotter, transformtoLog(averaged[a].avgfreq, config), averaged[a].avgvol);
		}
		pl_endpath_r(plot.plotter);

		first = 1;
		pl_flinewidth_r(plot.plotter, plot.penWidth);
		SetPenColor(color, 0xFFFF, &plot);
		for(long int a = 0; a < avgsize; a++)
		{
			if(first)
			{
				pl_fline_r(plot.plotter, transformtoLog(averaged[a].avgfreq, config), averaged[a].avgvol,
							transformtoLog(averaged[a+1].avgfreq, config), averaged[a+1].avgvol);
				first = 0;
			}
			else
				pl_fcont_r(plot.plotter, transformtoLog(averaged[a].avgfreq, config), averaged[a].avgvol);
		}
		pl_endpath_r(plot.plotter);
	}

	DrawColorScale(&plot, type, MODE_DIFF, LEFT_MARGIN, HEIGHT_MARGIN, 
		config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15,
		(int)startAmplitude, (int)(endAmplitude-startAmplitude), VERT_SCALE_STEP_BAR, config);
	DrawLabelsMDF(&plot, NOISE_AVG_TITLE, GetTypeDisplayName(config, type), PLOT_COMPARE, config);
	ClosePlot(&plot);
}

void PlotSingleTypeDifferentAmplitudesAveraged(FlatAmplDifference *amplDiff, long int size, int type, char *filename, AveragedFrequencies *averaged, long int avgsize, char channel, parameters *config)
{
	PlotFile	plot;
	double		dbs = config->maxDbPlotZC;
	int			color = 0, ismono = 0;
	char		*title = NULL;

	if(!config)
		return;

	if(!amplDiff)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	FillPlot(&plot, filename, config->startHzPlot, -1*dbs, config->endHzPlot, dbs, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroDBCentered(&plot, dbs, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dbs, VERT_SCALE_STEP, config->endHzPlot, config);

	if(channel == CHANNEL_MONO)
	{
		channel = CHANNEL_LEFT;
		ismono = 1;
	}

	for(long int a = 0; a < size; a++)
	{
		if((channel == CHANNEL_STEREO || channel == amplDiff[a].channel) &&
			amplDiff[a].hertz && amplDiff[a].type == type)
		{ 
			if(amplDiff[a].refAmplitude > config->significantAmplitude && fabs(amplDiff[a].diffAmplitude) <= fabs(dbs))
			{
				long int intensity;
	
				intensity = CalculateWeightedError((fabs(config->significantAmplitude) - fabs(amplDiff[a].refAmplitude))/fabs(config->significantAmplitude), config)*0xffff;
	
				SetPenColor(amplDiff[a].color, intensity, &plot);
				pl_fpoint_r(plot.plotter, transformtoLog(amplDiff[a].hertz, config), amplDiff[a].diffAmplitude);
			}
		}
	}

	color = MatchColor(GetTypeColor(config, type));
	pl_endpath_r(plot.plotter);

	if(averaged && avgsize > 1)
	{
		int first = 1;

		/*
		SetPenColor(color, 0xffff, &plot);
		for(long int a = 0; a < avgsize; a+=2)
		{
			if(a + 2 < avgsize)
			{
				pl_fbezier2_r(plot.plotter,
					transformtoLog(averaged[a].avgfreq, config), averaged[a].avgvol,
					transformtoLog(averaged[a+1].avgfreq, config), averaged[a+1].avgvol,
					transformtoLog(averaged[a+2].avgfreq, config), averaged[a+2].avgvol);
				//logmsg("Plot [%ld] %g->%g\n", a, averaged[a].avgfreq, averaged[a].avgvol);
			}
		}
		pl_endpath_r(plot.plotter);
		*/

		pl_flinewidth_r(plot.plotter, 50);
		SetPenColor(COLOR_GRAY, 0x0000, &plot);
		for(long int a = 0; a < avgsize; a++)
		{
			if(first)
			{
				pl_fline_r(plot.plotter, transformtoLog(averaged[a].avgfreq, config), averaged[a].avgvol,
							transformtoLog(averaged[a+1].avgfreq, config), averaged[a+1].avgvol);
				first = 0;
			}
			else
			{
				//if(fabs(averaged[a].avgvol) <= fabs(dbs))
					pl_fcont_r(plot.plotter, transformtoLog(averaged[a].avgfreq, config), averaged[a].avgvol);
				//else
					//break;
			}
		}
		pl_endpath_r(plot.plotter);

		first = 1;
		pl_flinewidth_r(plot.plotter, plot.penWidth);
		SetPenColor(color, 0xFFFF, &plot);
		for(long int a = 0; a < avgsize; a++)
		{
			if(first)
			{
				pl_fline_r(plot.plotter, transformtoLog(averaged[a].avgfreq, config), averaged[a].avgvol,
							transformtoLog(averaged[a+1].avgfreq, config), averaged[a+1].avgvol);
				first = 0;
			}
			else
			{
				//if(fabs(averaged[a].avgvol) <= fabs(dbs))
					pl_fcont_r(plot.plotter, transformtoLog(averaged[a].avgfreq, config), averaged[a].avgvol);
				//else
					//break;
			}
		}
		pl_endpath_r(plot.plotter);
	}

	if(ismono)
		channel = CHANNEL_MONO;

	switch(channel)
	{
		case CHANNEL_MONO:
			title = DIFFERENCE_AVG_TITLE;
			break;
		case CHANNEL_STEREO:
			title = DIFFERENCE_AVG_TITLE_STEREO;
			break;
		case CHANNEL_LEFT:
			title = DIFFERENCE_AVG_TITLE_LEFT;
			break;
		case CHANNEL_RIGHT:
			title = DIFFERENCE_AVG_TITLE_RIGHT;
			break;
		default:
			title = DIFFERENCE_AVG_TITLE;
			break;
	}
	DrawColorScale(&plot, type, MODE_DIFF, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, 0, config->significantAmplitude, VERT_SCALE_STEP_BAR, config);
	DrawLabelsMDF(&plot, title, GetTypeDisplayName(config, type), PLOT_COMPARE, config);
	ClosePlot(&plot);
}

void PlotAllDifferentAmplitudesAveraged(FlatAmplDifference *amplDiff, long int size, char *filename, AveragedFrequencies **averaged, long int *avgsize, parameters *config)
{
	PlotFile	plot;
	double		dBFS = config->maxDbPlotZC;
	int			currType = 0;

	if(!config)
		return;

	if(!amplDiff)
		return;

	if(!config->Differences.BlockDiffArray)
		return;
	
	FillPlot(&plot, filename, config->startHzPlot, -1*dBFS, config->endHzPlot, dBFS, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, config);

	for(long int a = 0; a < size; a++)
	{
		if(amplDiff[a].type > TYPE_CONTROL)
		{ 
			if(amplDiff[a].refAmplitude > config->significantAmplitude && fabs(amplDiff[a].diffAmplitude) <= fabs(dBFS))
			{
				long int intensity;
	
				intensity = CalculateWeightedError((fabs(config->significantAmplitude) - fabs(amplDiff[a].refAmplitude))/fabs(config->significantAmplitude), config)*0xffff;
	
				SetPenColor(amplDiff[a].color, intensity, &plot);
				pl_fpoint_r(plot.plotter, transformtoLog(amplDiff[a].hertz, config), amplDiff[a].diffAmplitude);
			}
		}
	}

	for(int t = 0; t < config->types.typeCount; t++)
	{
		int type = 0, color = 0;

		type = config->types.typeArray[t].type;
		if(type <= TYPE_CONTROL || config->types.typeArray[t].IsaddOnData)
			continue;

		color = MatchColor(GetTypeColor(config, type));
		pl_endpath_r(plot.plotter);
	
		if(averaged[currType] && avgsize[currType] > 1)
		{
			int first = 1;

			/*
			for(long int a = 0; a < avgsize[currType]; a+=2)
			{
				if(a + 2 < avgsize[currType])
				{
					pl_fbezier2_r(plot.plotter,
						transformtoLog(averaged[currType][a].avgfreq, config), averaged[currType][a].avgvol,
						transformtoLog(averaged[currType][a+1].avgfreq, config), averaged[currType][a+1].avgvol,
						transformtoLog(averaged[currType][a+2].avgfreq, config), averaged[currType][a+2].avgvol);
					//logmsg("Plot [%ld] %g->%g\n", a, averaged[currType][a].avgfreq, averaged[currType][a].avgvol);
				}
			}
			pl_endpath_r(plot.plotter);
			*/

			pl_flinewidth_r(plot.plotter, 50);
			SetPenColor(COLOR_GRAY, 0x0000, &plot);
			for(long int a = 0; a < avgsize[currType]; a++)
			{
				if(first)
				{
					pl_fline_r(plot.plotter, transformtoLog(averaged[currType][a].avgfreq, config), averaged[currType][a].avgvol,
								transformtoLog(averaged[currType][a+1].avgfreq, config), averaged[currType][a+1].avgvol);
					first = 0;
				}
				else
				{
					//if(fabs(averaged[currType][a].avgvol) <= fabs(dBFS))
						pl_fcont_r(plot.plotter, transformtoLog(averaged[currType][a].avgfreq, config), averaged[currType][a].avgvol);
					//else
						//break;
				}
			}
			pl_endpath_r(plot.plotter);
	
			first = 1;
			pl_flinewidth_r(plot.plotter, plot.penWidth);
			SetPenColor(color, 0xffff, &plot);
			for(long int a = 0; a < avgsize[currType]; a++)
			{
				if(first)
				{
					pl_fline_r(plot.plotter, transformtoLog(averaged[currType][a].avgfreq, config), averaged[currType][a].avgvol,
								transformtoLog(averaged[currType][a+1].avgfreq, config), averaged[currType][a+1].avgvol);
					first = 0;
				}
				else
				{
					//if(fabs(averaged[currType][a].avgvol) <= fabs(dBFS))
						pl_fcont_r(plot.plotter, transformtoLog(averaged[currType][a].avgfreq, config), averaged[currType][a].avgvol);
					//else
						//break;
				}
			}
			pl_endpath_r(plot.plotter);
		}
		currType++;
	}

	DrawColorAllTypeScale(&plot, MODE_DIFF, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, config->significantAmplitude, VERT_SCALE_STEP_BAR, DRAW_BARS, config);
	DrawLabelsMDF(&plot, DIFFERENCE_AVG_TITLE, ALL_LABEL, PLOT_COMPARE, config);

	ClosePlot(&plot);
}

void DrawFrequencyHorizontalGrid(PlotFile *plot, double hz, double hzIncrement, parameters *config)
{
	pl_pencolor_r (plot->plotter, 0, 0x5555, 0);
	for(int i = hzIncrement; i <= hz; i += hzIncrement)
	{
		double y = 0;

		if(config->logScaleTS)
			y = transformtoLog(i, config);
		else
			y = i;
		pl_fline_r(plot->plotter, 0, y, config->plotResX, y);
	}

	if(config->logScaleTS)
	{
		pl_fline_r(plot->plotter, 0, transformtoLog(10, config), config->plotResX, transformtoLog(10, config));
		pl_fline_r(plot->plotter, 0, transformtoLog(100, config), config->plotResX, transformtoLog(100, config));
	}

	pl_pencolor_r (plot->plotter, 0, 0x7777, 0);
	if(config->endHzPlot >= 10000)
	{
		for(int i = 10000; i < config->endHzPlot; i+= 10000)
		{
			double y = 0;
	
			if(config->logScaleTS)
				y = transformtoLog(i, config);
			else
				y = i;
			pl_fline_r(plot->plotter, 0, y, config->plotResX, y);
		}
	}

	pl_endpath_r(plot->plotter);
}

void DrawLabelsTimeSpectrogram(PlotFile *plot, int khz, int khzIncrement, parameters *config)
{
	double segments = 0, height = 0;
	char label[20];

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0-X0BORDER*config->plotResX*plot->leftmargin, -1*config->plotResY-Y0BORDER*config->plotResY, config->plotResX+X1BORDER*config->plotResX, 0+Y1BORDER*config->plotResY);
	pl_pencolor_r(plot->plotter, 0, 0xaaaa, 0);
	pl_ffontsize_r(plot->plotter, FONT_SIZE_1);

	if(!config->logScaleTS && khz >= 48 && khzIncrement == 1)
		khzIncrement = 2;

	pl_ffontname_r(plot->plotter, PLOT_FONT);
	segments = (double)khz/(double)khzIncrement;
	height = (double)config->plotResY/segments;
	for(int i = segments; i >= 0; i --)
	{
		int 	curkhz = 0;
		double	y = 0;

		curkhz = (int)floor(segments - i)*khzIncrement;
		y = -1*(double)i*height;

		if(config->logScaleTS && curkhz)
			y = -1*(config->plotResY-config->plotResY/(khz*1000)*transformtoLog(curkhz*1000, config));

		pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, y);
		sprintf(label, "%d%s", curkhz, curkhz ? "khz" : "hz");
		pl_alabel_r(plot->plotter, 'l', 'c', label);

		if(config->logScaleTS)
		{
			if(curkhz > 80)
				i -= 40;
			else if(curkhz > 40)
				i -= 20;
			else if(curkhz > 20)
				i -= 10;
			else if(curkhz > 6)
				i -= 2;
		}
	}

	if(config->logScaleTS)
	{
		double y = 0;

		y = -1*(config->plotResY-config->plotResY/(khz*1000)*transformtoLog(100, config));
		pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, y);
		pl_alabel_r(plot->plotter, 'l', 'c', "100hz");

		y = -1*(config->plotResY-config->plotResY/(khz*1000)*transformtoLog(10, config));
		pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, y);
		pl_alabel_r(plot->plotter, 'l', 'c', "10hz");
	}

	pl_restorestate_r(plot->plotter);
}

void DrawTimeCode(PlotFile *plot, double timecode, double x, double framerate, int color, double spaceAvailable, parameters *config)
{
	double	seconds = 0, labelwidth = 0;
	char	time[40];

	seconds = FramesToSeconds(timecode, framerate);

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0-X0BORDER*config->plotResX*plot->leftmargin, -1*config->plotResY/2-Y0BORDER*config->plotResY, config->plotResX+X1BORDER*config->plotResX, config->plotResY/2+Y1BORDER*config->plotResY);
	pl_ffontname_r(plot->plotter, PLOT_FONT);
	pl_ffontsize_r(plot->plotter, FONT_SIZE_2);
	SetPenColor(color, 0xFFFF, plot);
	pl_fmove_r(plot->plotter, x, config->plotResY/2);
	sprintf(time, "%0.1fs", seconds);
	labelwidth = pl_flabelwidth_r(plot->plotter, time);
	if(spaceAvailable >= labelwidth)
		pl_alabel_r(plot->plotter, 'l', 'b', time);
	pl_restorestate_r(plot->plotter);
}

void PlotTimeSpectrogram(AudioSignal *Signal, char channel, parameters *config)
{
	PlotFile	plot;
	double		significant = 0, x = 0, framewidth = 0, framecount = 0, timecode = 0, abs_significant = 0;
	double		frameOffset = 0;
	long int	block = 0, i = 0;
	int			lastType = TYPE_NOTYPE;
	char		filename[BUFFER_SIZE], name[BUFFER_SIZE/2], *title = NULL;

	if(!Signal || !config)
		return;

	// Name the plot
	ShortenFileName(basename(Signal->SourceFile), name);
	if(channel == CHANNEL_STEREO)
		sprintf(filename, "T_SP_%c_%s", Signal->role == ROLE_REF ? 'A' : 'B', name);
	else
		sprintf(filename, "T_SP_%c_%c_%s", channel, Signal->role == ROLE_REF ? 'A' : 'B', name);

	// total frame count minus silence
	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type > TYPE_SILENCE)
			framecount += config->types.typeArray[i].elementCount*config->types.typeArray[i].frames;
	}

	if(!framecount)
		return;

	if(config->FullTimeSpectroScale)
		significant = config->lowestDBFS;
	else
		significant = config->significantAmplitude;

	abs_significant = fabs(significant);

	FillPlot(&plot, filename, 0, 0, config->plotResX, config->endHzPlot, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawFrequencyHorizontalGrid(&plot, config->endHzPlot, 1000, config);
	DrawLabelsTimeSpectrogram(&plot, floor(config->endHzPlot/1000), 1, config);

	// Point to first valid frame
	frameOffset = SamplesToFrames(Signal->header.fmt.SamplesPerSec, Signal->startOffset, Signal->framerate, Signal->AudioChannels);
	//logmsg("\nSync Samples: %ld start->Frames %g Seconds %g\n", Signal->startOffset, frameOffset, FramesToSeconds(frameOffset, Signal->framerate));
	frameOffset += GetFirstElementFrameOffset(config);
	//logmsg("Signal Start->Frames %g Seconds %g\n", frameOffset, FramesToSeconds(frameOffset, Signal->framerate));

	// Determine how wide each frame is against plot resolution
	framewidth = config->plotResX / framecount;

	// Draw
	for(block = 0; block < config->types.totalBlocks; block++)
	{
		int		type = 0;
		double	frames = 0;

		type = GetBlockType(config, block);
		frames = GetBlockFrames(config, block);
		if(type > TYPE_SILENCE && config->MaxFreq > 0)
		{
			int		color = 0;
			double	noteWidth = 0, xpos = 0;

			noteWidth = framewidth*frames;
			xpos = x+noteWidth;
			color = MatchColor(GetBlockColor(config, block));

			// lowest amplitude first, so highest amplitude isn't drawn over
			for(i = config->MaxFreq-1; i >= 0; i--)
			{
				if(channel == CHANNEL_LEFT || channel == CHANNEL_STEREO
					|| Signal->Blocks[block].channel == CHANNEL_MONO
					|| Signal->Blocks[block].channel == CHANNEL_NOISE)
				{
					if(Signal->Blocks[block].freq[i].hertz && Signal->Blocks[block].freq[i].amplitude > significant)
					{
						long int intensity;
						double y, amplitude;
	
						// x is fixed by block division
						y = Signal->Blocks[block].freq[i].hertz;
						if(config->logScaleTS)
							y = transformtoLog(y, config);
						amplitude = Signal->Blocks[block].freq[i].amplitude;
						
						intensity = CalculateWeightedError(fabs(abs_significant - fabs(amplitude))/abs_significant, config)*0xffff;
						SetPenColor(color, intensity, &plot);
						pl_fline_r(plot.plotter, x,	y, xpos, y);
						pl_endpath_r(plot.plotter);
					}
				}

				if(channel == CHANNEL_RIGHT || channel == CHANNEL_STEREO)
				{
					if(Signal->Blocks[block].freqRight && Signal->Blocks[block].freqRight[i].hertz && Signal->Blocks[block].freqRight[i].amplitude > significant)
					{
						long int intensity;
						double y, amplitude;
	
						// x is fixed by block division
						y = Signal->Blocks[block].freqRight[i].hertz;
						if(config->logScaleTS)
							y = transformtoLog(y, config);
						amplitude = Signal->Blocks[block].freqRight[i].amplitude;
						
						intensity = CalculateWeightedError(fabs(abs_significant - fabs(amplitude))/abs_significant, config)*0xffff;
						SetPenColor(color, intensity, &plot);
						pl_fline_r(plot.plotter, x,	y, xpos, y);
						pl_endpath_r(plot.plotter);
					}
				}
			}

			if(lastType != type)
			{
				double	spaceAvailable = 0;

				SetPenColor(color, 0x9999, &plot);
				pl_fline_r(plot.plotter, x, 0, x, config->endHzPlot);

				spaceAvailable = noteWidth*GetBlockElements(config, block);
				DrawTimeCode(&plot, timecode+frameOffset, x, Signal->framerate, color, spaceAvailable, config);
				lastType = type;
			}
			x += noteWidth;
		}
		timecode += frames;
	}

	if(channel == CHANNEL_STEREO)
		title = Signal->role == ROLE_REF ? TSPECTROGRAM_TITLE_REF : TSPECTROGRAM_TITLE_COM;
	else
	{
		if(Signal->role == ROLE_REF)
			title = channel == CHANNEL_LEFT ? TSPECTROGRAM_TITLE_REF_LFT : TSPECTROGRAM_TITLE_REF_RGHT;
		else
			title = channel == CHANNEL_LEFT ? TSPECTROGRAM_TITLE_COM_LFT : TSPECTROGRAM_TITLE_COM_RGHT;
	}
	DrawColorAllTypeScale(&plot, MODE_SPEC, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, significant, VERT_SCALE_STEP_BAR, DRAW_BARS, config);
	DrawLabelsMDF(&plot, title, ALL_LABEL, Signal->role == ROLE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);

	ClosePlot(&plot);
}

void PlotSingleTypeTimeSpectrogram(AudioSignal* Signal, char channel, int plotType, parameters* config)
{
	PlotFile	plot;
	double		significant = 0, x = 0, framewidth = 0, framecount = 0, timecode = 0, abs_significant = 0;
	double		frameOffset = 0;
	long int	block = 0, i = 0;
	int			lastType = TYPE_NOTYPE;
	char		filename[BUFFER_SIZE], name[BUFFER_SIZE / 2], * title = NULL;

	if (!Signal || !config)
		return;

	// Name the plot
	ShortenFileName(basename(Signal->SourceFile), name);
	if (channel == CHANNEL_STEREO)
		sprintf(filename, "T_SP_%02d_%s_%c_%s", 
				plotType, GetTypeName(config, plotType), Signal->role == ROLE_REF ? 'A' : 'B', name);
	else
		sprintf(filename, "T_SP_%02d_%s_%c_%c_%s", 
			plotType, GetTypeName(config, plotType), channel, Signal->role == ROLE_REF ? 'A' : 'B', name);

	// total frame count minus silence
	for (int i = 0; i < config->types.typeCount; i++)
	{
		if (config->types.typeArray[i].type == plotType)
			framecount += config->types.typeArray[i].elementCount * config->types.typeArray[i].frames;
	}

	if (!framecount)
		return;

	if (config->FullTimeSpectroScale)
		significant = config->lowestDBFS;
	else
		significant = config->significantAmplitude;

	abs_significant = fabs(significant);

	FillPlot(&plot, filename, 0, 0, config->plotResX, config->endHzPlot, 1, 1, config);

	if (!CreatePlotFile(&plot, config))
		return;

	DrawFrequencyHorizontalGrid(&plot, config->endHzPlot, 1000, config);
	DrawLabelsTimeSpectrogram(&plot, floor(config->endHzPlot / 1000), 1, config);

	// Point to first valid frame
	frameOffset = SamplesToFrames(Signal->header.fmt.SamplesPerSec, Signal->startOffset, Signal->framerate, Signal->AudioChannels);
	//logmsg("\nSync Samples: %ld start->Frames %g Seconds %g\n", Signal->startOffset, frameOffset, FramesToSeconds(frameOffset, Signal->framerate));
	frameOffset += GetFirstElementFrameOffset(config);
	//logmsg("Signal Start->Frames %g Seconds %g\n", frameOffset, FramesToSeconds(frameOffset, Signal->framerate));

	// Determine how wide each frame is against plot resolution
	framewidth = config->plotResX / framecount;

	// Draw
	for (block = 0; block < config->types.totalBlocks; block++)
	{
		int		type = 0;
		double	frames = 0;

		type = GetBlockType(config, block);
		frames = GetBlockFrames(config, block);
		if (type == plotType && config->MaxFreq > 0)
		{
			int		color = 0;
			double	noteWidth = 0, xpos = 0;

			noteWidth = framewidth * frames;
			xpos = x + noteWidth;
			color = MatchColor(GetBlockColor(config, block));

			// lowest amplitude first, so highest amplitude isn't drawn over
			for (i = config->MaxFreq - 1; i >= 0; i--)
			{
				if (channel == CHANNEL_LEFT || channel == CHANNEL_STEREO
					|| Signal->Blocks[block].channel == CHANNEL_MONO
					|| Signal->Blocks[block].channel == CHANNEL_NOISE)
				{
					if (Signal->Blocks[block].freq[i].hertz && Signal->Blocks[block].freq[i].amplitude > significant)
					{
						long int intensity;
						double y, amplitude;

						// x is fixed by block division
						y = Signal->Blocks[block].freq[i].hertz;
						if (config->logScaleTS)
							y = transformtoLog(y, config);
						amplitude = Signal->Blocks[block].freq[i].amplitude;

						intensity = CalculateWeightedError(fabs(abs_significant - fabs(amplitude)) / abs_significant, config) * 0xffff;
						SetPenColor(color, intensity, &plot);
						pl_fline_r(plot.plotter, x, y, xpos, y);
						pl_endpath_r(plot.plotter);
					}
				}

				if (channel == CHANNEL_RIGHT || channel == CHANNEL_STEREO)
				{
					if (Signal->Blocks[block].freqRight && Signal->Blocks[block].freqRight[i].hertz && 
						Signal->Blocks[block].freqRight[i].amplitude > significant)
					{
						long int intensity;
						double y, amplitude;

						// x is fixed by block division
						y = Signal->Blocks[block].freqRight[i].hertz;
						if (config->logScaleTS)
							y = transformtoLog(y, config);
						amplitude = Signal->Blocks[block].freqRight[i].amplitude;

						intensity = CalculateWeightedError(fabs(abs_significant - fabs(amplitude)) / abs_significant, config) * 0xffff;
						SetPenColor(color, intensity, &plot);
						pl_fline_r(plot.plotter, x, y, xpos, y);
						pl_endpath_r(plot.plotter);
					}
				}
			}

			if (lastType != type)
			{
				double	spaceAvailable = 0;

				SetPenColor(color, 0x9999, &plot);
				pl_fline_r(plot.plotter, x, 0, x, config->endHzPlot);

				spaceAvailable = noteWidth * GetBlockElements(config, block);
				DrawTimeCode(&plot, timecode + frameOffset, x, Signal->framerate, color, spaceAvailable, config);
				lastType = type;
			}
			x += noteWidth;
		}
		timecode += frames;
	}

	if (channel == CHANNEL_STEREO)
		title = Signal->role == ROLE_REF ? TSPECTROGRAM_TITLE_REF : TSPECTROGRAM_TITLE_COM;
	else
	{
		if (Signal->role == ROLE_REF)
			title = channel == CHANNEL_LEFT ? TSPECTROGRAM_TITLE_REF_LFT : TSPECTROGRAM_TITLE_REF_RGHT;
		else
			title = channel == CHANNEL_LEFT ? TSPECTROGRAM_TITLE_COM_LFT : TSPECTROGRAM_TITLE_COM_RGHT;
	}
	DrawColorAllTypeScale(&plot, MODE_SPEC, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX / COLOR_BARS_WIDTH_SCALE, config->plotResY / 1.15, significant, VERT_SCALE_STEP_BAR, DRAW_BARS, config);
	DrawLabelsMDF(&plot, title, ALL_LABEL, Signal->role == ROLE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);

	ClosePlot(&plot);
}

void PlotTimeSpectrogramUnMatchedContent(AudioSignal *Signal, char channel, parameters *config)
{
	PlotFile	plot;
	double		significant = 0, x = 0, framewidth = 0, framecount = 0, tc = 0, abs_significant = 0;
	double		frameOffset = 0;
	long int	block = 0, i = 0;
	int			lastType = TYPE_NOTYPE;
	char		filename[BUFFER_SIZE], name[BUFFER_SIZE/2], *title = NULL;

	if(!Signal || !config)
		return;

	ShortenFileName(basename(Signal->SourceFile), name);
	if(Signal->role == ROLE_REF)
		sprintf(filename, "MISSING-A-T_SP_%s_%c", name, channel);
	else
		sprintf(filename, "MISSING-EXTRA_T_SP_%s_%c", name, channel);

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type > TYPE_SILENCE)
			framecount += config->types.typeArray[i].elementCount*config->types.typeArray[i].frames;
	}

	if(!framecount)
		return;

	if(config->FullTimeSpectroScale)
		significant = config->lowestDBFS;
	else
		significant = config->significantAmplitude;
	abs_significant = fabs(significant);

	FillPlot(&plot, filename, 0, 0, config->plotResX, config->endHzPlot, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawFrequencyHorizontalGrid(&plot, config->endHzPlot, 1000, config);
	DrawLabelsTimeSpectrogram(&plot, floor(config->endHzPlot/1000), 1, config);

	framewidth = config->plotResX / framecount;
	frameOffset = SamplesToFrames(Signal->header.fmt.SamplesPerSec, Signal->startOffset, Signal->framerate, Signal->AudioChannels);
	for(block = 0; block < config->types.totalBlocks; block++)
	{
		int		type = 0;
		double	frames = 0;

		type = GetBlockType(config, block);
		frames = GetBlockFrames(config, block);
		if(type > TYPE_SILENCE && config->MaxFreq > 0)
		{
			int		color = 0;
			double	noteWidth = 0, xpos = 0;

			noteWidth = framewidth*frames;
			xpos = x+noteWidth;
			color = MatchColor(GetBlockColor(config, block));

			for(i = config->MaxFreq-1; i >= 0; i--)
			{
				if(channel == CHANNEL_LEFT || channel == CHANNEL_STEREO
					|| Signal->Blocks[block].channel == CHANNEL_MONO 
					|| Signal->Blocks[block].channel == CHANNEL_NOISE)
				{
					if(Signal->Blocks[block].freq[i].hertz && !Signal->Blocks[block].freq[i].matched
						&& Signal->Blocks[block].freq[i].amplitude > significant)
					{
						long int intensity;
						double y, amplitude;
	
						// x is fixed by block division
						y = Signal->Blocks[block].freq[i].hertz;
						if(config->logScaleTS)
							y = transformtoLog(y, config);
						amplitude = Signal->Blocks[block].freq[i].amplitude;
						
						intensity = CalculateWeightedError(fabs(abs_significant - fabs(amplitude))/abs_significant, config)*0xffff;
						SetPenColor(color, intensity, &plot);
						pl_fline_r(plot.plotter, x,	y, xpos, y);
						pl_endpath_r(plot.plotter);
					}
				}

				if(channel == CHANNEL_RIGHT || channel == CHANNEL_STEREO)
				{
					if(Signal->Blocks[block].freqRight && Signal->Blocks[block].freqRight[i].hertz && !Signal->Blocks[block].freqRight[i].matched
							&& Signal->Blocks[block].freqRight[i].amplitude > significant)
					{
						long int intensity;
						double y, amplitude;
	
						// x is fixed by block division
						y = Signal->Blocks[block].freqRight[i].hertz;
						if(config->logScaleTS)
							y = transformtoLog(y, config);
						amplitude = Signal->Blocks[block].freqRight[i].amplitude;
						
						intensity = CalculateWeightedError(fabs(abs_significant - fabs(amplitude))/abs_significant, config)*0xffff;
						SetPenColor(color, intensity, &plot);
						pl_fline_r(plot.plotter, x,	y, xpos, y);
						pl_endpath_r(plot.plotter);
					}
				}
			}

			if(lastType != type)
			{
				double	spaceAvailable = 0;

				SetPenColor(color, 0x9999, &plot);
				pl_fline_r(plot.plotter, x, 0, x, config->endHzPlot);

				spaceAvailable = noteWidth*GetBlockElements(config, block);
				DrawTimeCode(&plot, tc+frameOffset, x, Signal->framerate, color, spaceAvailable, config);
				lastType = type;
			}
			x += noteWidth;
			tc += frames;
		}
	}

	if(channel == CHANNEL_STEREO)
		title = Signal->role == ROLE_REF ? EXTRA_TITLE_TS_REF : EXTRA_TITLE_TS_COM;
	else
	{
		if(Signal->role == ROLE_REF)
			title = channel == CHANNEL_LEFT ? EXTRA_TITLE_TS_REF_LEFT : EXTRA_TITLE_TS_REF_RIGHT;
		else
			title = channel == CHANNEL_LEFT ? EXTRA_TITLE_TS_COM_LEFT : EXTRA_TITLE_TS_COM_RIGHT;
	}
	DrawColorAllTypeScale(&plot, MODE_SPEC, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, significant, VERT_SCALE_STEP_BAR, DRAW_BARS, config);
	DrawLabelsMDF(&plot, title, ALL_LABEL, PLOT_COMPARE, config);

	ClosePlot(&plot);
}

void PlotTimeDomainGraphs(AudioSignal *Signal, parameters *config)
{
	long int	plots = 0, i = 0;
	char		name[BUFFER_SIZE*2];

	if(config->plotAllNotes)
	{
		for(i = 0; i < config->types.totalBlocks; i++)
		{
			if(config->plotAllNotes || Signal->Blocks[i].type == TYPE_TIMEDOMAIN)
			{
				plots++;
				/*
				if(config->plotPhase)
					plots++;
				*/
				if(config->plotAllNotesWindowed && Signal->Blocks[i].audio.window_samples)
					plots++;
				if(Signal->Blocks[i].internalSyncCount)
					plots += Signal->Blocks[i].internalSyncCount;
			}
		}
		logmsg("\n  Creating %ld plots for %s:\n  ", plots, getRoleText(Signal));
	}
	plots = 0;
	for(i = 0; i < config->types.totalBlocks; i++)
	{
		if(config->plotAllNotes || Signal->Blocks[i].type == TYPE_TIMEDOMAIN || (config->timeDomainSync && Signal->Blocks[i].type == TYPE_SYNC))
		{
			sprintf(name, "TD_%05ld_%s_%s_%05d_%s", 
				i, Signal->role == ROLE_REF ? "1" : "2",
				GetBlockName(config, i), GetBlockSubIndex(config, i), config->compareName);

			PlotBlockTimeDomainGraph(Signal, i, name, WAVEFORM_GENERAL, 0, config);
			logmsg(PLOT_ADVANCE_CHAR);
			plots++;
			if(plots == 80)
			{
				plots = 0;
				logmsg("\n  ");
			}

			if(Signal->Blocks[i].internalSyncCount)
			{
				for(int slot = 0; slot < Signal->Blocks[i].internalSyncCount; slot++)
				{
					sprintf(name, "TD_%05ld_%s_%s_%05d_%s_%02d", 
									i, Signal->role == ROLE_REF ? "1" : "2",
									GetBlockName(config, i), GetBlockSubIndex(config, i), 
									config->compareName, slot);
					PlotBlockTimeDomainInternalSyncGraph(Signal, i, name, slot, config);
				}
			}

			if(config->plotAllNotesWindowed && Signal->Blocks[i].audio.window_samples)
			{
				sprintf(name, "TD_%05ld_%s_%s_%05d_%s", 
					i, Signal->role == ROLE_REF ? "3" : "4",
					GetBlockName(config, i), GetBlockSubIndex(config, i), config->compareName);

				PlotBlockTimeDomainGraph(Signal, i, name, WAVEFORM_WINDOW, 0, config);

				logmsg(PLOT_ADVANCE_CHAR);
				plots++;
				if(plots == 80)
				{
					plots = 0;
					logmsg("\n  ");
				}
			}
		}
	}
	if(!config->plotAllNotes && plots > 40)
		logmsg("\n  ");
}

int ExecutePlotBlockTimeDomainGraph(int waveType, AudioSignal *Signal, long int block, double data, char *folder, parameters *config)
{
	char *returnFolder = NULL;
	char name[BUFFER_SIZE*2];

	returnFolder = PushFolder(folder);
	if(!returnFolder)
		return 0;
	
	sprintf(name, "TD_%05ld_%s_%s_%05d_%s", 
		block, Signal->role == ROLE_REF ? "1" : "2",
		GetBlockName(config, block), GetBlockSubIndex(config, block), config->compareName);

	PlotBlockTimeDomainGraph(Signal, block, name, waveType, data, config);

	ReturnToMainPath(&returnFolder);
	return 1;
}

void PlotTimeDomainHighDifferenceGraphs(AudioSignal *Signal, parameters *config)
{
	int		plots = 0;
	char	*returnFolder = NULL;

	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	returnFolder = PushFolder(WAVEFORMDIFF_FOLDER);
	if(!returnFolder)
		return;

	for(long int b = 0; b < config->types.totalBlocks; b++)
	{
		if(Signal->Blocks[b].type > TYPE_CONTROL)
		{
			double diff = 0;
	
			diff = Signal->Blocks[b].AverageDifference;
			if(diff > 0)
			{
				if(!ExecutePlotBlockTimeDomainGraph(WAVEFORM_AMPDIFF, Signal, b, diff, WAVEFORMDIR_AMPL, config))
				{
					ReturnToMainPath(&returnFolder);
					return;
				}
				logmsg(PLOT_ADVANCE_CHAR);
				plots++;
				if(plots == 80)
				{
					plots = 0;
					logmsg("\n  ");
				}
			}
			
			diff = Signal->Blocks[b].missingPercent;
			if(diff > 0)
			{
				if(!ExecutePlotBlockTimeDomainGraph(WAVEFORM_MISSING, Signal, b, diff, WAVEFORMDIR_MISS, config))
				{
					ReturnToMainPath(&returnFolder);
					return;
				}
				logmsg(PLOT_ADVANCE_CHAR);
				plots++;
				if(plots == 80)
				{
					plots = 0;
					logmsg("\n  ");
				}
			}
	
			diff = Signal->Blocks[b].extraPercent;
			if(diff > 0)
			{
				if(!ExecutePlotBlockTimeDomainGraph(WAVEFORM_EXTRA, Signal, b, diff, WAVEFORMDIR_EXTRA, config))
				{
					ReturnToMainPath(&returnFolder);
					return;
				}
				logmsg(PLOT_ADVANCE_CHAR);
				plots++;
				if(plots == 80)
				{
					plots = 0;
					logmsg("\n  ");
				}
			}
		}
	}
	logmsg("\n  ");
	ReturnToMainPath(&returnFolder);
}

void DrawVerticalFrameGrid(PlotFile *plot, AudioSignal *Signal, double frames, double frameIncrement, double MaxSamples, int forceDrawMS, parameters *config)
{
	int		drawMS = 0;
	double	x = 0, xfactor = 0, segment = 0, MaxY = config->lowestValueBitDepth, MinY = config->highestValueBitDepth;

	if(config->zoomWaveForm != 0.0)
	{
		MaxY = CalculatePCMMagnitude(config->zoomWaveForm, config->highestValueBitDepth);
		MinY = CalculatePCMMagnitude(config->zoomWaveForm, config->lowestValueBitDepth);
	}

	if(frames <= 10 || forceDrawMS)
		drawMS = 1;

	/* draw ms lines */
	if(drawMS)
	{
		SetPenColor(COLOR_GRAY, 0x4444, plot);
		for(int f = 0; f < frames; f++)
		{
			double offset, factor;

			factor = (double)Signal->header.fmt.SamplesPerSec/1000.0;
			offset = (double)f*Signal->framerate*factor;
			for(int i = 0; i <= Signal->framerate; i += 1)
			{
				x = offset+(double)i*factor;
				pl_fline_r(plot->plotter, x, MinY, x, MaxY);
				pl_endpath_r(plot->plotter);
			}
		}
	}

	/* draw frame lines */
	SetPenColor(COLOR_GREEN, 0x7777, plot);
	for(int i = frameIncrement; i <= frames; i += frameIncrement)
	{
		x = FramesToSamples(i, Signal->header.fmt.SamplesPerSec, Signal->framerate);
		pl_fline_r(plot->plotter, x, MinY, x, MaxY);
		pl_endpath_r(plot->plotter);
	}

	if(frames > 1) {
		if(frames <= 10)
			segment = frames/2;
		else {
			if(frames <= 100) segment = 10;
			else {
				if(frames <= 500) segment = 25;
				else segment = 200;
			}
		}
	}
	else segment = 1;

	SetPenColor(COLOR_GREEN, 0x9999, plot);
	for(int i = 0; i <= frames; i += segment)
	{
		x = FramesToSamples(i, Signal->header.fmt.SamplesPerSec, Signal->framerate);
		pl_fline_r(plot->plotter, x, MinY, x, MaxY);
		pl_endpath_r(plot->plotter);
	}

	/* Draw the labels ¨*/
	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0-X0BORDER*config->plotResX*plot->leftmargin, -1*config->plotResY/2-Y0BORDER*config->plotResY, config->plotResX+X1BORDER*config->plotResX, config->plotResY/2+Y1BORDER*config->plotResY);
	pl_ffontsize_r(plot->plotter, FONT_SIZE_1);
	pl_ffontname_r(plot->plotter, PLOT_FONT);
	SetPenColor(COLOR_GREEN, 0x9999, plot);

	xfactor = config->plotResX/MaxSamples;
	for(int i = 0; i <= frames; i += segment)
	{
		char label[40];

		x = FramesToSamples(i, Signal->header.fmt.SamplesPerSec, Signal->framerate);
		sprintf(label, "Frame %d", i);
		pl_fmove_r(plot->plotter, x*xfactor, config->plotResY/2);
		pl_alabel_r(plot->plotter, 'c', 'b', label);
	}

	pl_restorestate_r(plot->plotter);
}

void DrawINTXXDBFSLines(PlotFile *plot, double resx, int AudioChannels, parameters *config)
{
	char 	label[20];
	double	factor = 0, MaxY = config->highestValueBitDepth, MinY = config->lowestValueBitDepth;
	double	startDB = 3, endDB = 27, dbstep = 3;
	int		channel = 0;

	if(config->zoomWaveForm != 0)
	{
		MaxY = CalculatePCMMagnitude(config->zoomWaveForm, config->highestValueBitDepth);
		MinY = CalculatePCMMagnitude(config->zoomWaveForm, config->lowestValueBitDepth);

		startDB = fabs(config->zoomWaveForm)+3;
		endDB = fabs(config->zoomWaveForm)+30;
	}

	for(channel = 1; channel <= AudioChannels; channel++)
	{
		if(AudioChannels == 2)
		{
			double margin1 = 0, margin2 = 0;

			margin1 = fabs(MaxY-MinY)*Y0BORDER;
			margin2 = fabs(MaxY-MinY)*Y1BORDER;

			// Split the vertical axis
			pl_savestate_r(plot->plotter);
			if(channel == 1)
				pl_fspace_r(plot->plotter, plot->x0, 3*MinY-margin1, plot->x1, MaxY+margin2);
			if(channel == 2)
				pl_fspace_r(plot->plotter, plot->x0, MinY-margin1, plot->x1, 3*MaxY+margin2);
		}

		// center line
		SetPenColor(COLOR_GRAY, 0x5555, plot);
		pl_fline_r(plot->plotter, 0, 0, resx, 0);
	
		// 3dbfs step lines
		SetPenColor(COLOR_GRAY, 0x3333, plot);
		for(double db = startDB; db <= endDB; db += dbstep)
		{
			double height = 0;

			height = CalculatePCMMagnitude(-1*db, config->highestValueBitDepth);
			pl_fline_r(plot->plotter, 0, height, resx, height);

			height = CalculatePCMMagnitude(-1*db, config->lowestValueBitDepth);
			pl_fline_r(plot->plotter, 0, height, resx, height);
		}

		pl_endpath_r(plot->plotter);

		if(AudioChannels == 2)
			pl_restorestate_r(plot->plotter);

		/* Draw the labels ¨*/
	
		pl_savestate_r(plot->plotter);
		if(AudioChannels == 2)
		{
			// Split the vertical axis
			if(channel == 1)
				pl_fspace_r(plot->plotter, 0-X0BORDER*config->plotResX*plot->leftmargin, -3*config->plotResY/2-Y0BORDER*config->plotResY, config->plotResX+X1BORDER*config->plotResX, config->plotResY/2+Y1BORDER*config->plotResY);
			if(channel == 2)
				pl_fspace_r(plot->plotter, 0-X0BORDER*config->plotResX*plot->leftmargin, -1*config->plotResY/2-Y0BORDER*config->plotResY, config->plotResX+X1BORDER*config->plotResX, 3*config->plotResY/2+Y1BORDER*config->plotResY);
		}
		else
			pl_fspace_r(plot->plotter, 0-X0BORDER*config->plotResX*plot->leftmargin, -1*config->plotResY/2-Y0BORDER*config->plotResY, config->plotResX+X1BORDER*config->plotResX, config->plotResY/2+Y1BORDER*config->plotResY);

		pl_ffontsize_r(plot->plotter, FONT_SIZE_1);
		pl_ffontname_r(plot->plotter, PLOT_FONT);
	
		if(AudioChannels == 2)
		{
			// Channel Label
			pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, 0);
			SetPenColor(COLOR_GRAY, 0xAAAA, plot);
			if(channel == 1)
				pl_alabel_r(plot->plotter, 'l', 'c', " Left");
			if(channel == 2)
				pl_alabel_r(plot->plotter, 'l', 'c', " Right");
		}

		SetPenColor(COLOR_GRAY, 0x7777, plot);
		factor = config->plotResY/(2*MaxY);
		for(double db = startDB; db <= endDB; db += dbstep)
		{
			double height;
	
			height = CalculatePCMMagnitude(-1*db, config->highestValueBitDepth);
			sprintf(label, "%ddBFS", (int)(-1*db));
			pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, height*factor);
			pl_alabel_r(plot->plotter, 'l', 'c', label);
			pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, -1*height*factor);
			pl_alabel_r(plot->plotter, 'l', 'c', label);
	
			height = CalculatePCMMagnitude(-1*db, config->lowestValueBitDepth);
			sprintf(label, "%ddBFS", (int)(-1*db));
			pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, height*factor);
			pl_alabel_r(plot->plotter, 'l', 'c', label);
			pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, -1*height*factor);
			pl_alabel_r(plot->plotter, 'l', 'c', label);
		}
	
		pl_restorestate_r(plot->plotter);
	}
}

char *GetWFMTypeText(int wftype, char *buffer, double data, int role)
{
	buffer[0] = '\0';

	if(wftype ==  WAVEFORM_WINDOW)
		sprintf(buffer, " -- Windowed");
	if(wftype ==  WAVEFORM_AMPDIFF)
	{
		if(role == ROLE_REF)
			sprintf(buffer, " -- Average Differences in matching comparision: %g dBFS", data);
		else
			sprintf(buffer, " -- Average Differences here: %g dBFS", data);
	}
	if(wftype ==  WAVEFORM_MISSING)
	{
		if(role == ROLE_REF)
			sprintf(buffer, " -- Missing Frequencies in matching comparision: %g%%", data);
		else
			sprintf(buffer, " -- Missing Frequencies from here: %g%%", data);
	}
	if(wftype ==  WAVEFORM_EXTRA)
	{
		if(role == ROLE_REF)
			sprintf(buffer, " -- Extra Frequencies found in matching comparision: %g%%", data);
		else
			sprintf(buffer, " -- Extra Frequencies found here: %g%%", data);
	}
	return buffer;
}

void PlotBlockTimeDomainGraph(AudioSignal *Signal, int block, char *name, int wavetype, double data, parameters *config)
{
	int			forceMS = 0;
	char		title[BUFFER_SIZE/2], buffer[BUFFER_SIZE];
	PlotFile	plot;
	long int	color = 0, sample = 0, numSamples = 0, difference = 0, plotSize = 0, sampleOffset = 0;
	double		*samples = NULL;
	double		margin1 = 0, margin2 = 0, MaxY = config->highestValueBitDepth, MinY = config->lowestValueBitDepth;

	if(config->zoomWaveForm != 0)
	{
		MaxY = CalculatePCMMagnitude(config->zoomWaveForm, config->highestValueBitDepth);
		MinY = CalculatePCMMagnitude(config->zoomWaveForm, config->lowestValueBitDepth);
	}

	// for plots
	margin1 = fabs(MaxY-MinY)*Y0BORDER;
	margin2 = fabs(MaxY-MinY)*Y1BORDER;

	if(!Signal || !config)
		return;

	if(block > config->types.totalBlocks)
		return;

	if(wavetype == WAVEFORM_WINDOW)
		samples = Signal->Blocks[block].audio.window_samples;
	else
		samples = Signal->Blocks[block].audio.samples;

	if(!samples)
		return;

	numSamples = Signal->Blocks[block].audio.size;
	difference = Signal->Blocks[block].audio.difference;
	if(difference < 0)
		plotSize += numSamples - difference;
	else
		plotSize = numSamples;

	sampleOffset = Signal->Blocks[block].audio.sampleOffset;
	/*
	if(config->timeDomainSync && Signal->Blocks[block].type == TYPE_SYNC)
	{
		FillPlotExtra(&plot, name, SYNC_DEBUG_SCALE*config->plotResX, config->plotResY, 0, MinY, plotSize, MaxY, 1, 0.2, config);
		forceMS = 1;
	}
	else
	*/
	FillPlot(&plot, name, 0, MinY, plotSize, MaxY, 1, 0.2, config);

	if(!CreatePlotFile(&plot, config))
		return;

	// discarded samples box (difference)
	if(difference > 0 && Signal->Blocks[block].type != TYPE_SYNC)
	{
		pl_filltype_r(plot.plotter, 1);
		pl_pencolor_r(plot.plotter, 0x6666, 0, 0);
		pl_fillcolor_r(plot.plotter, 0x6666, 0, 0);
		pl_fbox_r(plot.plotter, numSamples-difference, MinY, numSamples-1, MaxY);
		pl_filltype_r(plot.plotter, 0);
	}

	DrawVerticalFrameGrid(&plot, Signal, Signal->Blocks[block].frames, 1, plotSize, forceMS, config);
	DrawINTXXDBFSLines(&plot, numSamples, Signal->AudioChannels, config);

	color = MatchColor(GetBlockColor(config, block));

	if(Signal->AudioChannels == 2)
	{
		// Split the vertical axis
		pl_savestate_r(plot.plotter);
		pl_fspace_r(plot.plotter, plot.x0, 3*MinY-margin1, plot.x1, MaxY+margin2);
	}

	// Draw samples
	SetPenColor(color, 0xffff, &plot);
	if(config->zoomWaveForm == 0)	// This is the regular plot
	{
		for(sample = 0; sample < numSamples - 1; sample ++)
			pl_fline_r(plot.plotter, sample, samples[sample], sample+1, samples[sample+1]);
	}
	else	// This is for zoomed in plots
	{
		for(sample = 0; sample < numSamples - 1; sample ++)
		{
			double s0 = samples[sample], s1 = samples[sample+1];

			// draw samples outside zoom up to the zoom point
			if(s0 > MaxY) s0 = MaxY;
			if(s1 < MinY) s1 = MinY;

			if(s0 < MinY) s0 = MinY;
			if(s1 > MaxY) s1 = MaxY;

			if(!(s0 == s1 && (s0 == MaxY || s0 == MinY)))  // clear samples fully outside zoom
				pl_fline_r(plot.plotter, sample, s0, sample+1, s1);
		}
	}
	pl_endpath_r(plot.plotter);

	// Draw Extra Channel samples
	if(Signal->AudioChannels == 2)
	{
		// End top split
		pl_restorestate_r(plot.plotter);
		// New lower split
		pl_savestate_r(plot.plotter);
		pl_fspace_r(plot.plotter, plot.x0, MinY-margin1, plot.x1, 3*MaxY+margin2);

		if(wavetype == WAVEFORM_WINDOW)
			samples = Signal->Blocks[block].audioRight.window_samples;
		else
			samples = Signal->Blocks[block].audioRight.samples;
		SetPenColor(color, 0xffff, &plot);
		if(config->zoomWaveForm == 0)	// This is the regular plot
		{
			for(sample = 0; sample < numSamples - 1; sample ++)
				pl_fline_r(plot.plotter, sample, samples[sample], sample+1, samples[sample+1]);
		}
		else	// This is for zoomed in plots
		{
			for(sample = 0; sample < numSamples - 1; sample ++)
			{
				double s0 = samples[sample], s1 = samples[sample+1];
	
				// draw samples outside zoom up to the zoom point
				if(s0 > MaxY) s0 = MaxY;
				if(s1 < MinY) s1 = MinY;
	
				if(s0 < MinY) s0 = MinY;
				if(s1 > MaxY) s1 = MaxY;
	
				if(!(s0 == s1 && (s0 == MaxY || s0 == MinY)))  // clear samples fully outside zoom
					pl_fline_r(plot.plotter, sample, s0, sample+1, s1);
			}
		}
		pl_endpath_r(plot.plotter);
		pl_restorestate_r(plot.plotter);
	}
	
	sprintf(title, "%s# %d%s | samples %ld-%ld", GetBlockDisplayName(config, block), GetBlockSubIndex(config, block),
			GetWFMTypeText(wavetype, buffer, data, Signal->role), 
			SamplesForDisplay(sampleOffset, Signal->AudioChannels),
			SamplesForDisplay(sampleOffset+numSamples*Signal->AudioChannels, Signal->AudioChannels));
	DrawLabelsMDF(&plot, Signal->role == ROLE_REF ? WAVEFORM_TITLE_REF : WAVEFORM_TITLE_COM, title, Signal->role == ROLE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);

	ClosePlot(&plot);
}

void PlotBlockTimeDomainInternalSyncGraph(AudioSignal *Signal, int block, char *name, int slot, parameters *config)
{
	char		title[1024];
	PlotFile	plot;
	long int	color = 0, sample = 0, numSamples = 0, difference = 0, plotSize = 0, frames = 0, sampleOffset = 0;
	double		*samples = NULL;
	int			forceMS = 0;

	if(!Signal || !config)
		return;

	if(block > config->types.totalBlocks)
		return;

	if(!Signal->Blocks[block].internalSync)
		return;

	if(slot > Signal->Blocks[block].internalSyncCount - 1)
		return;

	samples = Signal->Blocks[block].internalSync[slot].samples;

	if(!samples)
		return;

	numSamples = Signal->Blocks[block].internalSync[slot].size;
	frames = SamplesToFrames(Signal->header.fmt.SamplesPerSec, numSamples, Signal->framerate, Signal->AudioChannels);

	difference = Signal->Blocks[block].internalSync[slot].difference;
	if(difference < 0)
		plotSize += numSamples - difference;
	else
		plotSize = numSamples;
	sampleOffset = Signal->Blocks[block].audio.sampleOffset;
	/*
	if(config->timeDomainSync && Signal->Blocks[block].type == TYPE_SYNC)
	{
		FillPlotExtra(&plot, name, SYNC_DEBUG_SCALE*config->plotResX, config->plotResY, 0, config->lowestValueBitDepth, plotSize, config->highestValueBitDepth, 1, 0.2, config);
		forceMS = 1;
	}
	else
	*/
	FillPlot(&plot, name, 0, config->lowestValueBitDepth, plotSize, config->highestValueBitDepth, 1, 0.2, config);

	if(!CreatePlotFile(&plot, config))
		return;

	// discarded samples box (difference)
	if(difference > 0 && Signal->Blocks[block].type != TYPE_SYNC)
	{
		pl_filltype_r(plot.plotter, 1);
		pl_pencolor_r(plot.plotter, 0x6666, 0, 0);
		pl_fillcolor_r(plot.plotter, 0x6666, 0, 0);
		pl_fbox_r(plot.plotter, numSamples-difference, config->lowestValueBitDepth, numSamples-1, config->highestValueBitDepth);
		pl_filltype_r(plot.plotter, 0);
	}

	DrawVerticalFrameGrid(&plot, Signal, frames, 1, plotSize, forceMS, config);
	DrawINTXXDBFSLines(&plot, numSamples, Signal->AudioChannels, config);

	color = MatchColor(GetBlockColor(config, block));

	// Draw samples
	SetPenColor(color, 0xffff, &plot);
	for(sample = 0; sample < numSamples - 1; sample ++)
		pl_fline_r(plot.plotter, sample, samples[sample], sample+1, samples[sample+1]);
	pl_endpath_r(plot.plotter);

	sprintf(title, "%s# %d-%d at %g (samples: %ld-%ld)", GetBlockName(config, block), GetBlockSubIndex(config, block),
			slot+1, Signal->framerate, 
			SamplesForDisplay(sampleOffset, Signal->AudioChannels),
			SamplesForDisplay(sampleOffset+numSamples*Signal->AudioChannels, Signal->AudioChannels));
	DrawLabelsMDF(&plot, Signal->role == ROLE_REF ? WAVEFORM_TITLE_REF : WAVEFORM_TITLE_COM, title, Signal->role == ROLE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);

	ClosePlot(&plot);
}

FlatPhase *CreatePhaseFlatDifferences(parameters *config, long int *size)
{
	long int 			count = 0;
	FlatPhase	*PDiff = NULL;

	if(!size)
		return NULL;

	*size = 0;
	PDiff = (FlatPhase*)malloc(sizeof(FlatPhase)*config->Differences.cntPhaseAudioDiff);
	if(!PDiff)
		return NULL;
	memset(PDiff, 0, sizeof(FlatPhase)*config->Differences.cntPhaseAudioDiff);

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		int type = 0;
		int color = 0;
		
		type = GetBlockType(config, b);
		color = MatchColor(GetBlockColor(config, b));

		for(int p = 0; p < config->Differences.BlockDiffArray[b].cntPhaseBlkDiff; p++)
		{
			PDiff[count].hertz = config->Differences.BlockDiffArray[b].phaseDiffArray[p].hertz;
			PDiff[count].phase = config->Differences.BlockDiffArray[b].phaseDiffArray[p].diffPhase;
			PDiff[count].type = type;
			PDiff[count].color = color;
			PDiff[count].channel = config->Differences.BlockDiffArray[b].phaseDiffArray[p].channel;
			count ++;
		}
	}

	logmsg(PLOT_PROCESS_CHAR);
	PhaseDifferencesByFrequency_tim_sort(PDiff, count);
	logmsg(PLOT_PROCESS_CHAR);
	*size = count;
	return PDiff;
}

/*
void PlotPhaseFromSignal(AudioSignal *Signal, parameters *config)
{
	long int			size = 0;
	FlatPhase	*phaseDiff = NULL;
	
	phaseDiff = CreatePhaseFlatFromSignal(Signal, &size, config);
	if(!phaseDiff)
	{
		logmsg("Not enough memory for plotting\n");
		return;
	}

	if(PlotEachTypePhase(phaseDiff, size, config->compareName, Signal->role == ROLE_REF ? PHASE_REF : PHASE_COMP, config) > 1)
	{
		PlotAllPhase(phaseDiff, size, config->compareName, Signal->role == ROLE_REF ? PHASE_REF : PHASE_COMP, config);
		logmsg(PLOT_ADVANCE_CHAR);
	}
	
	free(phaseDiff);
	phaseDiff = NULL;
}
*/

void PlotPhaseDifferences(parameters *config)
{
	long int	size = 0;
	FlatPhase	*phaseDiff = NULL;
	
	phaseDiff = CreatePhaseFlatDifferences(config, &size);
	if(!phaseDiff)
	{
		logmsg("Not enough memory for plotting\n");
		return;
	}

	if(PlotEachTypePhase(phaseDiff, size, config->compareName, PHASE_DIFF, config) > 1)
	{
		PlotAllPhase(phaseDiff, size, config->compareName, PHASE_DIFF, config);
		logmsg(PLOT_ADVANCE_CHAR);
	}
	
	free(phaseDiff);
	phaseDiff = NULL;
}

void PlotAllPhase(FlatPhase *phaseDiff, long int size, char *filename, int pType, parameters *config)
{
	PlotFile	plot;
	char		name[BUFFER_SIZE];

	if(!config)
		return;

	if(!phaseDiff)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	if(pType == PHASE_DIFF)
		sprintf(name, "PHASE_DIFF__ALL_%s", filename);
	else
		sprintf(name, "PHASE__ALL_%c_%s", pType == PHASE_REF ? 'A' : 'B', filename);
	FillPlot(&plot, name, config->startHzPlot, -1*PHASE_ANGLE, config->endHzPlot, PHASE_ANGLE, 1, 0.5, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroAngleCentered(&plot, PHASE_ANGLE, 90, config->endHzPlot, config);
	DrawLabelsZeroAngleCentered(&plot, PHASE_ANGLE, 90, config->endHzPlot, config);

	for(int p = 0; p < size; p++)
	{
		if(phaseDiff[p].hertz && phaseDiff[p].type > TYPE_CONTROL)
		{ 
			SetPenColor(phaseDiff[p].color, 0xFFFF, &plot);
			pl_fpoint_r(plot.plotter, transformtoLog(phaseDiff[p].hertz, config), phaseDiff[p].phase);
		}
	}

	DrawColorAllTypeScale(&plot, MODE_SPEC, LEFT_MARGIN, HEIGHT_MARGIN, 0, 0, 0, VERT_SCALE_STEP_BAR, NO_DRAW_BARS, config);
	if(pType == PHASE_DIFF)
		DrawLabelsMDF(&plot, PHASE_DIFF_TITLE, ALL_LABEL, PLOT_COMPARE, config);
	else
		DrawLabelsMDF(&plot, pType == PHASE_REF ? PHASE_SIG_TITLE_REF : PHASE_SIG_TITLE_COM, ALL_LABEL, pType == PHASE_REF  ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);

	ClosePlot(&plot);
}

int PlotEachTypePhase(FlatPhase *phaseDiff, long int size, char *filename, int pType, parameters *config)
{
	int 		i = 0, type = 0, types = 0, typeCount = 0, bothStereo = 0;
	char		name[BUFFER_SIZE];

	bothStereo = config->referenceSignal->AudioChannels == 2 && config->comparisonSignal->AudioChannels == 2;
	typeCount = GetActiveBlockTypesNoRepeat(config);
	for(i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;

		if(type > TYPE_CONTROL && !config->types.typeArray[i].IsaddOnData)
		{
			char	*returnFolder = NULL;

			if(typeCount > 1)
			{
				returnFolder = PushFolder(PHASE_FOLDER);
				if(!returnFolder)
					return 0;
			}

			if(config->types.typeArray[i].channel == CHANNEL_STEREO && bothStereo)
			{
				if(pType == PHASE_DIFF)
					sprintf(name, "PHASE_DIFF_%s_%02d%s_%c", filename, 
						type, config->types.typeArray[i].typeName, CHANNEL_LEFT);
				else
					sprintf(name, "PHASE_%c_%s_%02d%s_%c", pType == PHASE_REF ? 'A' : 'B', filename, 
						type, config->types.typeArray[i].typeName, CHANNEL_LEFT);
				PlotSingleTypePhase(phaseDiff, size, type, name, pType, CHANNEL_LEFT, config);
				logmsg(PLOT_ADVANCE_CHAR);

				if(pType == PHASE_DIFF)
					sprintf(name, "PHASE_DIFF_%s_%02d%s_%c", filename, 
						type, config->types.typeArray[i].typeName, CHANNEL_RIGHT);
				else
					sprintf(name, "PHASE_%c_%s_%02d%s_%c", pType == PHASE_REF ? 'A' : 'B', filename, 
						type, config->types.typeArray[i].typeName, CHANNEL_RIGHT);
				PlotSingleTypePhase(phaseDiff, size, type, name, pType, CHANNEL_RIGHT, config);
				logmsg(PLOT_ADVANCE_CHAR);
			}
			
			if(pType == PHASE_DIFF)
				sprintf(name, "PHASE_DIFF_%s_%02d%s", filename, 
					type, config->types.typeArray[i].typeName);
			else
				sprintf(name, "PHASE_%c_%s_%02d%s", pType == PHASE_REF ? 'A' : 'B', filename, 
					type, config->types.typeArray[i].typeName);
			PlotSingleTypePhase(phaseDiff, size, type, name, pType, CHANNEL_STEREO, config);
			logmsg(PLOT_ADVANCE_CHAR);

			if(typeCount > 1)
				ReturnToMainPath(&returnFolder);

			types ++;
		}
	}
	return types;
}

void PlotSingleTypePhase(FlatPhase *phaseDiff, long int size, int type, char *filename, int pType, char channel, parameters *config)
{
	PlotFile	plot;
	char		*title;

	if(!config)
		return;

	if(!phaseDiff)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	FillPlot(&plot, filename, config->startHzPlot, -1*PHASE_ANGLE, config->endHzPlot, PHASE_ANGLE, 1, 0.5, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroAngleCentered(&plot, PHASE_ANGLE, 90, config->endHzPlot, config);
	DrawLabelsZeroAngleCentered(&plot, PHASE_ANGLE, 90, config->endHzPlot, config);

	for(int p = 0; p < size; p++)
	{
		if((channel == CHANNEL_STEREO || channel == phaseDiff[p].channel) &&
			phaseDiff[p].hertz && phaseDiff[p].type == type)
		{ 
			SetPenColor(phaseDiff[p].color, 0xFFFF, &plot);
			pl_fpoint_r(plot.plotter, transformtoLog(phaseDiff[p].hertz, config), phaseDiff[p].phase);
		}
	}

	if(pType == PHASE_DIFF)
	{
		if(channel == CHANNEL_STEREO)
			title = PHASE_DIFF_TITLE;
		else
			title = channel == CHANNEL_LEFT ? PHASE_DIFF_TITLE_LEFT : PHASE_DIFF_TITLE_RIGHT;
		DrawLabelsMDF(&plot, title, GetTypeDisplayName(config, type), PLOT_COMPARE, config);
	}
	else
	{
		if(channel == CHANNEL_STEREO)
			title = pType == PHASE_REF ? PHASE_SIG_TITLE_REF : PHASE_SIG_TITLE_COM;
		else
		{
			if(pType == PHASE_REF)
				title = channel == CHANNEL_LEFT ? PHASE_SIG_TITLE_REF_LEFT : PHASE_SIG_TITLE_REF_RIGHT;
			else
				title = channel == CHANNEL_LEFT ? PHASE_SIG_TITLE_COM_LEFT : PHASE_SIG_TITLE_COM_RIGHT;
		}
		DrawLabelsMDF(&plot, title, GetTypeDisplayName(config, type), pType == PHASE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);
	}
	ClosePlot(&plot);
}

void DrawGridZeroAngleCentered(PlotFile *plot, double maxAngle, double angleIncrement, double hz, parameters *config)
{
	pl_pencolor_r (plot->plotter, 0, 0xaaaa, 0);
	pl_fline_r(plot->plotter, 0, 0, hz, 0);
	pl_endpath_r(plot->plotter);

	pl_pencolor_r (plot->plotter, 0, 0x5555, 0);
	for(int i = angleIncrement; i < maxAngle; i += angleIncrement)
	{
		pl_fline_r(plot->plotter, 0, i, hz, i);
		pl_fline_r(plot->plotter, 0, -1*i, hz, -1*i);
	}
	pl_endpath_r(plot->plotter);

	DrawFrequencyHorizontal(plot, maxAngle, hz, 1000, config);

	pl_endpath_r(plot->plotter);
	pl_pencolor_r (plot->plotter, 0, 0xFFFF, 0);
}

void DrawLabelsZeroAngleCentered(PlotFile *plot, double maxAngle, double angleIncrement, double hz, parameters *config)
{
	double segments = 0;
	char label[20];

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0-X0BORDER*config->plotResX*plot->leftmargin, -1*config->plotResY/2-Y0BORDER*config->plotResY, config->plotResX+X1BORDER*config->plotResX, config->plotResY/2+Y1BORDER*config->plotResY);

	pl_ffontname_r(plot->plotter, PLOT_FONT);
	pl_ffontsize_r(plot->plotter, FONT_SIZE_1);

	pl_pencolor_r(plot->plotter, 0, 0xffff	, 0);
	pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, config->plotResY/100);
	pl_alabel_r(plot->plotter, 'l', 't', "0\\de");

	pl_pencolor_r(plot->plotter, 0, 0xaaaa, 0);
	segments = fabs(maxAngle/angleIncrement);
	for(int i = 1; i < segments; i ++)
	{
		pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, i*config->plotResY/segments/2+config->plotResY/100);
		sprintf(label, " %g\\de", i*angleIncrement);
		pl_alabel_r(plot->plotter, 'l', 't', label);

		pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, -1*i*config->plotResY/segments/2+config->plotResY/100);
		sprintf(label, "-%g\\de", i*angleIncrement);
		pl_alabel_r(plot->plotter, 'l', 't', label);
	}

	if(config->logScale)
	{
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(10, config), config->plotResY/2);
		sprintf(label, "%dHz", 10);
		pl_alabel_r(plot->plotter, 'c', 'b', label);
	
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(100, config), config->plotResY/2);
		sprintf(label, "%dHz", 100);
		pl_alabel_r(plot->plotter, 'c', 'b', label);
	}

	pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(1000, config), config->plotResY/2);
	sprintf(label, "  %dHz", 1000);
	pl_alabel_r(plot->plotter, 'c', 'b', label);

	if(config->endHzPlot >= 10000)
	{
		for(int i = 10000; i < config->endHzPlot; i+= 10000)
		{
			pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(i, config), config->plotResY/2);
			sprintf(label, "%d%s", i/1000, i >= 40000  ? "" : "khz");
			pl_alabel_r(plot->plotter, 'c', 'b', label);
		}
	}

	pl_restorestate_r(plot->plotter);
}

void PlotCLKSpectrogram(AudioSignal *Signal, parameters *config)
{
	char 				tmpName[BUFFER_SIZE/2], name[BUFFER_SIZE];
	long int			size = 0;
	FlatFrequency		*frequencies = NULL;
	
	ShortenFileName(basename(Signal->SourceFile), tmpName);
	frequencies = CreateFlatFrequenciesCLK(Signal, &size, config);
	sprintf(name, "SP_%c_%s_CLK_%s", Signal->role == ROLE_REF ? 'A' : 'B', tmpName, config->clkName);
	PlotCLKSpectrogramInternal(frequencies, size, name, Signal->role, config);

	free(frequencies);
	frequencies = NULL;
}

FlatFrequency *CreateFlatFrequenciesCLK(AudioSignal *Signal, long int *size, parameters *config)
{
	long int		i = 0;
	long int		count = 0, counter = 0;
	FlatFrequency	*Freqs = NULL;

	if(!size || !Signal || !config)
		return NULL;

	*size = 0;

	for(i = 0; i < config->MaxFreq; i++)
	{
		if(Signal->clkFrequencies.freq[i].hertz != 0)
			count ++;
		else
			break;
	}

	Freqs = (FlatFrequency*)malloc(sizeof(FlatFrequency)*count);
	if(!Freqs)
		return NULL;
	memset(Freqs, 0, sizeof(FlatFrequency)*count);

	for(i = 0; i < count; i++)
	{
		FlatFrequency tmp;

		tmp.hertz = Signal->clkFrequencies.freq[i].hertz;
		tmp.amplitude = Signal->clkFrequencies.freq[i].amplitude;
		tmp.type = TYPE_CLK_ANALYSIS;
		tmp.color = COLOR_GREEN;
		tmp.channel = CHANNEL_LEFT;

		if(InsertElementInPlace(Freqs, tmp, counter))
			counter ++;
	}
	
	logmsg(PLOT_PROCESS_CHAR);
	FlatFrequenciesByAmplitude_tim_sort(Freqs, counter);
	logmsg(PLOT_PROCESS_CHAR);

	*size = counter;
	return(Freqs);
}

void PlotCLKSpectrogramInternal(FlatFrequency *freqs, long int size, char *filename, int signal, parameters *config)
{
	PlotFile	plot;
	double		startAmplitude = config->significantAmplitude, endAmplitude = config->lowestDBFS;

	if(!config)
		return;

	// Find limits
	for(int f = 0; f < size; f++)
	{
		if(freqs[f].amplitude > startAmplitude)
			startAmplitude = freqs[f].amplitude;
		if(freqs[f].amplitude < endAmplitude)
			endAmplitude = freqs[f].amplitude;
	}

	if(endAmplitude < NS_LOWEST_AMPLITUDE)
		endAmplitude = NS_LOWEST_AMPLITUDE;

	FillPlot(&plot, filename, config->startHzPlot, endAmplitude, config->endHzPlot, 0.0, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroToLimit(&plot, endAmplitude, VERT_SCALE_STEP, config->endHzPlot, 1000, 0, config);
	DrawLabelsZeroToLimit(&plot, endAmplitude, VERT_SCALE_STEP, config->endHzPlot, 0, config);

	//DrawNoiseLines(&plot, 0, endAmplitude, Signal, config);
	//DrawLabelsNoise(&plot, config->endHzPlot, Signal, config);

	for(int f = 0; f < size; f++)
	{
		if(freqs[f].amplitude >= endAmplitude && freqs[f].hertz)
		{ 
			long int intensity;
			double x, y;

			x = transformtoLog(freqs[f].hertz, config);
			y = freqs[f].amplitude;
			intensity = CalculateWeightedError(((fabs(endAmplitude) - fabs(startAmplitude)) - (fabs(freqs[f].amplitude)- fabs(startAmplitude)))/(fabs(endAmplitude)-fabs(startAmplitude)), config)*0xffff;
			
			//pl_flinewidth_r(plot.plotter, 100*range_0_1);
			SetPenColor(freqs[f].color, intensity, &plot);
			pl_fline_r(plot.plotter, x, y, x, endAmplitude);
			pl_endpath_r(plot.plotter);
		}
	}

	plot.SpecialWarning = "NOTE: dBFS scale relative between CLK signals";
	DrawColorScale(&plot, TYPE_CLK_ANALYSIS, MODE_SPEC, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, (int)startAmplitude, (int)(endAmplitude-startAmplitude), VERT_SCALE_STEP,config);
	DrawLabelsMDF(&plot, signal == ROLE_REF ? SPECTROGRAM_CLK_REF : SPECTROGRAM_CLK_COM, config->clkName, signal == ROLE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);

	ClosePlot(&plot);
}

/*
void PlotDifferenceTimeSpectrogram(parameters *config)
{
	PlotFile	plot;
	double		x = 0, framewidth = 0, framecount = 0, tc = 0, abs_significant = 0, steps = 0;
	double		outside = 0, maxDiff = 0, frameOffset = 0;
	long int	block = 0, i = 0;
	int			lastType = TYPE_NOTYPE, type = 0;
	char		filename[2*BUFFER_SIZE];


	if(!config || !config->Differences.BlockDiffArray)
		return;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type > TYPE_SILENCE)
			framecount += config->types.typeArray[i].elementCount*config->types.typeArray[i].frames;
	}

	if(!framecount)
		return;

	// This calculates the viewport in dBFS 
	steps = 1.0;
	abs_significant = fabs(config->maxDbPlotZC);
	outside = FindDifferencePercentOutsideViewPort(&maxDiff, &type, abs_significant, config);
	if(outside == 0 && fabs(maxDiff) && fabs(maxDiff) > abs_significant*2)
	{
		abs_significant = abs_significant/2;
		do
		{
			outside = FindDifferencePercentOutsideViewPort(&maxDiff, &type, abs_significant, config);
			if(outside == 0)
				abs_significant = abs_significant/2;
		}while(!outside && fabs(maxDiff) && fabs(maxDiff) > abs_significant*2);
	}

	if(outside >= 10)
		abs_significant = abs_significant*1.5;

	if(abs_significant > VERT_SCALE_STEP_BAR*10)
		steps = VERT_SCALE_STEP_BAR;
	else
		steps = abs_significant / 10;

	sprintf(filename, "DA__ALL_TS_%s", config->compareName);
	FillPlot(&plot, filename, 0, 0, config->plotResX, config->endHzPlot, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawFrequencyHorizontalGrid(&plot, config->endHzPlot, 1000, config);
	DrawLabelsTimeSpectrogram(&plot, floor(config->endHzPlot/1000), 1, config);

	framewidth = config->plotResX / framecount;
	frameOffset = SamplesToFrames(Signal->header.fmt.SamplesPerSec, Signal->startOffset, Signal->framerate, Signal->AudioChannels);
	for(block = 0; block < config->types.totalBlocks; block++)
	{
		int		type = 0;
		double	frames = 0;

		type = GetBlockType(config, block);
		frames = GetBlockFrames(config, block);
		if(type > TYPE_SILENCE && config->MaxFreq > 0)
		{
			int				color = 0;
			long int		count = 0;
			double			noteWidth = 0, xpos = 0;

			noteWidth = framewidth*frames;
			xpos = x+noteWidth;
			color = MatchColor(GetBlockColor(config, block));
			count = config->Differences.BlockDiffArray[block].cntAmplBlkDiff;
			if(count)
			{
				AmplDifference 	*amplDiffArraySorted = NULL;

				amplDiffArraySorted = (AmplDifference*)malloc(sizeof(AmplDifference)*count);
				if(!amplDiffArraySorted)
				{
					logmsg("ERROR: Not enough memory (amplDiffArraySorted)\n");
					return;
				}

				memcpy(amplDiffArraySorted, config->Differences.BlockDiffArray[block].amplDiffArray, sizeof(AmplDifference)*count);
				AmplitudeDifferencesBlock_tim_sort(amplDiffArraySorted, count);

				for(i = count - 1; i >= 0; i--)
				{
					double refAmplitude = 0, amplitude = 0;
	
					refAmplitude = amplDiffArraySorted[i].refAmplitude;
					amplitude = amplDiffArraySorted[i].diffAmplitude;
					if(refAmplitude > config->significantAmplitude && fabs(amplitude) <= abs_significant)
					{
						long int intensity;
						double y;
		
						// x is fixed by block division
						y = amplDiffArraySorted[i].hertz;
						if(config->logScaleTS)
							y = transformtoLog(y, config);
						
						intensity = 0xffff*CalculateWeightedError(1.0-(fabs(abs_significant - fabs(amplitude))/abs_significant), config);
						//if(1.0-(fabs(abs_significant - fabs(amplitude))/abs_significant) >= 0.5)
							//logmsgFileOnly("%ghz: %g %g 0x%X [%g=1-(%g-%g)/%g]\n", y,
								//amplitude, abs_significant, intensity, 1.0-(fabs(abs_significant - fabs(amplitude))/abs_significant), abs_significant, fabs(amplitude), abs_significant);
						SetPenColor(color, intensity, &plot);
						pl_fline_r(plot.plotter, x, y, xpos, y);
						pl_endpath_r(plot.plotter);
					}
				}

				free(amplDiffArraySorted);
			}

			if(lastType != type)
			{
				double	spaceAvailable = 0;

				SetPenColor(color, 0x9999, &plot);
				pl_fline_r(plot.plotter, x, 0, x, config->endHzPlot);

				spaceAvailable = noteWidth*GetBlockElements(config, block);
				DrawTimeCode(&plot, tc, x+frameOffset, config->smallerFramerate, color, spaceAvailable, config);
				lastType = type;
			}
			x += noteWidth;
			tc += frames;
		}
	}

	DrawColorAllTypeScale(&plot, MODE_TSDIFF, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, abs_significant, steps, DRAW_BARS, config);
	DrawLabelsMDF(&plot, TS_DIFFERENCE_TITLE, ALL_LABEL, PLOT_COMPARE, config);

	ClosePlot(&plot);
}
*/
