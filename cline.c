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

#include "cline.h"
#include "log.h"
#include "plot.h"
#include "profile.h"

#define CHAR_FOLDER_REMOVE		0
#define CHAR_FOLDER_OK			1
#define CHAR_FOLDER_CHANGE_T1	2
#define CHAR_FOLDER_CHANGE_T2	3

// -9 and -V not shown
void PrintUsage()
{
	logmsg("  usage: mdfourier -P profile.mdf -r reference.wav -c compare.wav\n");
	logmsg("   FFT and Analysis options:\n");
	logmsg("	 -w: enable <w>indowing. Default is a custom Tukey window.\n");
	logmsg("		'n' none, 't' Tukey, 'h' Hann, 'f' FlatTop & 'm' Hamming\n");
	logmsg("	 -f: Change the number of analyzed frequencies to use from FFTW\n");
	logmsg("	 -s: Defines <s>tart of the frequency range to compare with FFT\n");
	logmsg("	 -e: Defines <e>nd of the frequency range to compare with FFT\n");
	logmsg("	 -i: <i>gnores the silence block noise floor if present\n");
	logmsg("	 -z: Uses <z>ero Padding to equal 1 Hz FFT bins\n");
	logmsg("	 -n: <N>ormalize:\n");
	logmsg("		'f' Frequency Domain Max, 't' Time Domain, 'a' Average\n");
	logmsg("		'n' No normalization\n");
	logmsg("	 -B: Do not do stereo channel audio <B>alancing\n");
	logmsg("	 -I: <I>gnore frame rate difference for analysis\n");
	logmsg("	 -p: Define the noise floor value in dBFS (0 to disable auto adjust)\n");
	logmsg("	 -T: Increase Sync detection <T>olerance (ignore frequency for pulses)\n");
	logmsg("	 -Y: Define the Reference Video Format from the profile\n");
	logmsg("	 -Z: Define the Comparison Video Format from the profile\n");
	logmsg("	 -m: Set <m>anual sync samples, takes format [r|c]:<start sample>:<end sample>\n");
	logmsg("		where r is for reference and c is for comparison\n");
	logmsg("		example: -m r:64001:12236801\n");
	logmsg("		sample values must be the start of each pulse sequence\n");
	logmsg("	 -R: Adjust sample <R>ate if duration difference is found\n");
	logmsg("	 -j: Ad<j>ust clock (profile defined) via FFTW if difference is found\n");
	logmsg("	 -k: cloc<k> FFTW operations\n");
	logmsg("	 -X: Do not use E<x>tra Data from the Profile\n");
	logmsg("   Output options:\n");
	logmsg("	 -l: Do not <l>og output to file [reference]_vs_[compare].txt\n");
	logmsg("	 -v: Enable <v>erbose mode, spits all the FFTW results\n");
	logmsg("	 -C: Create <C>SV file with plot values.\n");
	logmsg("	 -b: Change <b>ar value for frequency match, default is 1.0dB.\n");
	logmsg("	 -A: Do not weight values in <A>veraged Plot (implies -g)\n");
	logmsg("	 -G: Adjust difference plots around avera<G>e difference.\n");
	logmsg("	 -W: Use <W>hite background for plots.\n");
	logmsg("	 -d: Max <d>BFS for plots vertically\n");
	logmsg("	 -a: Zoom in to dB in waveform plots\n");
	logmsg("	 -L: Plot resolution:\n");
	logmsg("		1: %gx%g  2: %gx%g 3: %gx%g\n",
			PLOT_RES_X_LOW, PLOT_RES_Y_LOW, PLOT_RES_X, PLOT_RES_Y, PLOT_RES_X_1K, PLOT_RES_Y_1K);
	logmsg("		4: %gx%g 5: %gx%g 6: %gx%g\n",
			PLOT_RES_X_HI, PLOT_RES_Y_HI, PLOT_RES_X_4K, PLOT_RES_Y_4K, PLOT_RES_X_8K, PLOT_RES_Y_8K);
	logmsg("		7: %gx%g 8: %gx%g 9: %gx%g\n",
			PLOT_RES_X_16K, PLOT_RES_Y_16K, PLOT_RES_X_32K, PLOT_RES_Y_32K, PLOT_RES_X_64K, PLOT_RES_Y_64K);
	logmsg("	 -D: Don't create <D>ifferences Plots\n");
	logmsg("	 -g: Don't create avera<g>e points over the plotted graphs\n");
	logmsg("	 -M: Don't create <M>issing Plots\n");
	logmsg("	 -S: Don't create <S>pectrogram Plots\n");
	logmsg("	 -F: Don't create Noise <F>loor Plots\n");
	logmsg("	 -t: Don't create Time Spectrogram Plots\n");
	logmsg("	 -O: Don't create Phase Pl<O>ts\n");
	logmsg("	 -Q: Don't create Time Domain Plots\n");
	logmsg("	 -H: Output waveform plots for <H>ighly different notes, takes a value in percentage\n");
	logmsg("	 -o: Define the output filter function for color weights [0-5]\n");
	logmsg("	 -u: Create waveform plots for all notes\n");
	logmsg("	      -uu:  Create DFT windowed waveform plots for all notes\n");
	logmsg("	      -uuu: Create waveform plots for all notes, including DFT windows\n");
	logmsg("	 -E: Defines Full frequency rang<E> for Time Spectrogram plots\n");
	logmsg("	 -N: Use li<N>ear scale instead of logaritmic scale for plots\n");
	logmsg("	 -x: (text) Enables e<x>tended log results. Shows a table with matches\n");
	logmsg("	 -0: Change output folder\n");
	logmsg("	 -y: Output debug Sync pulse detection algorithm information\n");
}

int Header(int log, int argc, char *argv[])
{
#ifdef DEBUG
	char title1[] = "MDFourier DEBUG " MDVERSION " [240p Test Suite Fourier Audio compare tool] " BITS_MDF "\n";
#else
	char title1[] = "MDFourier " MDVERSION " [240p Test Suite Fourier Audio compare tool] " BITS_MDF "\n";
#endif
	char title2[] = "Artemio Urbina 2019-2022 free software under GPL - http://junkerhq.net/MDFourier\n";

	if(argc == 2 && !strncmp(argv[1], "-V", 2))
	{
		printf("version %s %s %0.1f\n", MDVERSION, BITS_MDF, PROFILE_VER);
		return 0;
	}

	if(log)
		logmsgFileOnly("%s%s", title1, title2);
	else
		printf("%s%s", title1, title2);
	return 1;
}

void CleanParameters(parameters *config)
{
	int i = 0;

	memset(config, 0, sizeof(parameters));

	initLog();

	sprintf(config->outputFolder, OUTPUT_FOLDER);
	config->outputPath[0] = '\0';

	config->startHz = START_HZ;
	config->endHz = END_HZ;
	config->startHzPlot = START_HZ_PLOT;
	config->endHzPlot = END_HZ;
	config->maxDbPlotZC = DB_HEIGHT;
	config->maxDbPlotZCChanged = 0;
	config->extendedResults = 0;
	config->verbose = 0;
	config->window = 't';
	config->MaxFreq = FREQ_COUNT;
	config->clock = 0;
	config->showAll = 0;
	config->ignoreFloor = 0;
	config->outputFilterFunction = 3;
	config->origSignificantAmplitude = SIGNIFICANT_AMPLITUDE;
	config->significantAmplitude = SIGNIFICANT_AMPLITUDE;
	config->referenceNoiseFloor = 0;
	config->smallerFramerate = 0;
	config->referenceFramerate = 0;
	config->ZeroPad = 0;
	config->debugSync = 0;
	config->timeDomainSync = 1;
	config->drawWindows = 0;
	config->channelBalance = 1;
	config->showPercent = 1;
	config->ignoreFrameRateDiff = 0;
	config->labelNames = 1;
	config->outputCSV = 0;
	config->whiteBG = 0;
	config->smallFile = 0;
	config->videoFormatRef = 0;
	config->videoFormatCom = 0;
	config->syncTolerance = 0;
	config->AmpBarRange = BAR_DIFF_DB_TOLERANCE;
	config->FullTimeSpectroScale = 0;
	config->hasTimeDomain = 0;
	config->hasSilenceOverRide = 0;
	config->hasAddOnData = 0;
	config->noSyncProfile = 0;
	config->noSyncProfileType = NO_SYNC_AUTO;
	config->frequencyNormalizationTries = 0;
	config->frequencyNormalizationTolerant = 0;
	config->noiseFloorTooHigh = 0;
	config->noiseFloorBigDifference = 0;
	config->channelWithLowFundamentals = 0;
	config->notVisible = 0;
	config->usesStereo = 0;
	config->allowStereoVsMono = 0;
	config->stereoNotFound = 0;
	config->stereoBalanceBlock = 0;
	config->internalSyncTolerance = 0;
	config->zoomWaveForm = 0;
	config->trimmingNeeded = 0;
	config->highestValueBitDepth = 0;
	config->lowestValueBitDepth = 0;
	config->lowestDBFS = 0;

	config->averageDifference = 0;
	config->averageDifferenceOrig = 0;
	config->substractAveragePlot = 0;

	config->warningStereoReversed = 0;
	config->warningRatioTooHigh = 0;
	for (i = 0; i < 4; i++)
	{
		config->syncAlignPct[i] = 0;
		config->syncAlignTolerance[i] = 0;
	}
	config->syncAlignIterator = 0;

	config->logScale = 1;
	config->logScaleTS = 0;
	config->normType = max_frequency;

	config->refNoiseMin = 0;
	config->refNoiseMax = 0;

	config->plotResX = PLOT_RES_X;
	config->plotResY = PLOT_RES_Y;
	config->plotRatio = 0;

	config->plotDifferences = 1;
	config->plotMissing = 1;
	config->plotSpectrogram = 1;
	config->plotTimeSpectrogram = 1;
	config->plotNoiseFloor = 1;
	config->plotTimeDomain = 1;
	config->plotPhase = 1;
	config->plotAllNotes = 0;
	config->plotAllNotesWindowed = 0;
	config->plotTimeDomainHiDiff = 0;
	config->averagePlot = 1;
	config->weightedAveragePlot = 1;
	config->noiseFloorAutoAdjust = 1;
	config->changedCLKFrom = 0;
	config->pErrorReport = 0;
	config->noBalance = 0;

	config->Differences.BlockDiffArray = NULL;
	config->Differences.cntFreqAudioDiff = 0;
	config->Differences.cntAmplAudioDiff = 0;
	
	config->Differences.cntTotalCompared = 0;
	config->Differences.cntTotalAudioDiff = 0;
	
	config->types.totalBlocks = 0;
	config->types.regularBlocks = 0;

	memset(config->types.SyncFormat, 0, sizeof(VideoBlockDef)*2);
	config->types.typeArray = NULL;
	config->types.typeCount = 0;

	config->types.useWatermark = 0;
	config->types.watermarkValidFreq = 0;
	config->types.watermarkInvalidFreq = 0;

	config->thresholdAmplitudeHiDif = AMPL_HIDIFF;
	config->thresholdMissingHiDif = MISS_HIDIFF;
	config->thresholdExtraHiDif = EXTRA_HIDIFF;

	config->sync_plan = NULL;
	config->model_plan = NULL;
	config->reverse_plan = NULL;

	config->referenceSignal = NULL;
	config->comparisonSignal = NULL;
	config->nyquistLimit = 0;  // only used in MDWave

	config->clkBlock = NO_CLK;
	config->clkFreq = 0;
	config->clkRatio = 0;
	config->clkNotFound = 0;
	config->clkWarning = 0;
	config->clkRef = 0;
	config->clkCom = 0;

	config->doSamplerateAdjust = 0;
	config->doClkAdjust = 0;

	config->useExtraData = 1;
	config->compressToBlocks = 0;
	config->drawPerfect = 1;
	config->drawMissExtraFreq = 1;

	config->SRNoMatch = 0;
	config->diffClkNoMatch = 0;

	config->centsDifferenceCLK = 0;
	config->RefCentsDifferenceSR = 0;
	config->ComCentsDifferenceSR = 0;

	EnableLog();
}

int commandline(int argc , char *argv[], parameters *config)
{
	FILE *file = NULL;
	int c, index, ref = 0, tar = 0;
	
	opterr = 0;
	
	CleanParameters(config);

	// Available: JKUq1234567
	while ((c = getopt (argc, argv, "Aa:Bb:Cc:Dd:Ee:Ff:GgH:hIijkL:lMm:Nn:Oo:P:p:QRr:Ss:TtuVvWw:XxY:yZ:z0:89")) != -1)
	switch (c)
	  {
	  case 'A':
		config->averagePlot = 1;
		config->weightedAveragePlot = 0;
		break;
	  case 'a':
		config->zoomWaveForm = atof(optarg);
		if(config->zoomWaveForm > 0)
			config->zoomWaveForm *= -1;
		if(config->zoomWaveForm < -200)
		{
			logmsg(" -ERROR: Waveform Zoom Range must be between %d and %d\n", 0, -200);
			return 0;
		}
		if(config->zoomWaveForm)
			logmsg("\t-Zooming waveform from 0dbfs to %dbfs\n", config->zoomWaveForm);
		else
			logmsg("\t-Zooming waveform kept at 0dbfs\n");
		break;
	  case 'B':
		config->channelBalance = 0;
		logmsg("\t -Audio channel balance disabled\n");
		break;
	  case 'b':
		config->AmpBarRange = atof(optarg);
		if(config->AmpBarRange < 0 || config->AmpBarRange > 16)
		{
			logmsg("-ERROR: Range must be between %d and %d\n", 0, 16);
			return 0;
		}
		logmsg("\t-Changing Bar range to %dbfs\n", config->AmpBarRange);
		break;
	  case 'C':
		config->outputCSV = 1;
		break;
	  case 'c':
		sprintf(config->comparisonFile, "%s", optarg);
		tar = 1;
		break;
	  case 'D':
		config->plotDifferences = 0;
		break;
	  case 'd':
		config->maxDbPlotZC = atof(optarg);
		if(config->maxDbPlotZC < 0 || config->maxDbPlotZC > 120.0)
		{
			logmsg("-ERROR: Range must be between %d and %d\n", 0, 120);
			return 0;
		}
		config->maxDbPlotZCChanged = 1;
		logmsg("\t-Plot range set to %g\n", config->maxDbPlotZC);
		break;
	  case 'E':
		config->FullTimeSpectroScale = 1;
		logmsg("\t-Full Time spectrogram selected, this is slower\n");
		break;
	  case 'e':
		config->endHz = atof(optarg);
		if(config->endHz < START_HZ*2.0)
		{
			logmsg("-ERROR: Requested %g end frequency is lower than possible\n", atof(optarg));
			return 0;
		}
		if(config->endHz > MAX_HZ)
		{
			logmsg("-ERROR: Requested %g end frequency is higher than possible\n", atof(optarg));
			return 0;
		}
		if(config->endHz > END_HZ)
			config->endHzPlot = config->endHz;
		logmsg("\t-Frequency end range for FFTW is now %g (default %g)\n", config->endHz, END_HZ);
		break;
	  case 'F':
		config->plotNoiseFloor = 0;
		break;
	  case 'f':
		config->MaxFreq = atoi(optarg);
		if(config->MaxFreq < 1 || config->MaxFreq > MAX_FREQ_COUNT)
		{
			logmsg("-ERROR: Number of frequencies must be between %d and %d\n", 1, MAX_FREQ_COUNT);
			return 0;
		}
		logmsg("\t-Max frequencies to use from FFTW are %d (default %d)\n", config->MaxFreq, FREQ_COUNT);
		break;
	  case 'G':
		config->substractAveragePlot = 1;
		break;
	  case 'g':
		config->averagePlot = 0;
		break;
	  case 'H':
		{
			double percent = 0;

			percent = atof(optarg);
			if(percent > 100.0 || percent < 0.0)
			{
				logmsg("-ERROR: Percent for comparison must be between 0.0 and 100.0\n");
				return 0;
			}
			
			config->thresholdAmplitudeHiDif = percent;
			config->thresholdMissingHiDif = percent;
			config->thresholdExtraHiDif = percent;

			logmsg("\t-Percentage for highly different wavefors set at %g%% (old value 1%%)\n", percent);
			config->plotTimeDomainHiDiff = 1;
		}
		break;
	  case 'h':
		PrintUsage();
		return 0;
		break;
	  case 'I':
		config->ignoreFrameRateDiff = 1;
		logmsg("\t-Ignoring frame rate differences\n");
		break;
	  case 'i':
		config->ignoreFloor = 1;
		logmsg("\t-Ignoring Silence block noise floor\n");
		break;
	  case 'j':
		config->doClkAdjust = 1;
		logmsg("\t-Adjusting Clock\n");
		break;
	  case 'k':
		config->clock = 1;
		break;
	  case 'L':
		switch(atoi(optarg))
		{
			case 0:
				config->plotResX = PLOT_RES_X_LL;
				config->plotResY = PLOT_RES_Y_LL;
				config->showPercent = 0;
				break;
			case 1:
				config->plotResX = PLOT_RES_X_LOW;
				config->plotResY = PLOT_RES_Y_LOW;
				config->showPercent = 0;
				break;
			case 2:
				config->plotResX = PLOT_RES_X;
				config->plotResY = PLOT_RES_Y;
				break;
			case 3:
				config->plotResX = PLOT_RES_X_1K;
				config->plotResY = PLOT_RES_Y_1K;
				break;
			case 4:
				config->plotResX = PLOT_RES_X_HI;
				config->plotResY = PLOT_RES_Y_HI;
				break;
			case 5:
				config->plotResX = PLOT_RES_X_4K;
				config->plotResY = PLOT_RES_Y_4K;
				break;
			case 6:
				config->plotResX = PLOT_RES_X_8K;
				config->plotResY = PLOT_RES_Y_8K;
				break;
			case 7:
				config->plotResX = PLOT_RES_X_16K;
				config->plotResY = PLOT_RES_Y_16K;
				break;
			case 8:
				config->plotResX = PLOT_RES_X_32K;
				config->plotResY = PLOT_RES_Y_32K;
				break;
			case 9:
				config->plotResX = PLOT_RES_X_64K;
				config->plotResY = PLOT_RES_Y_64K;
				break;
			default:
				logmsg("-ERROR: Invalid resolution (-%c) parameter %s\n", optopt, optarg);
				return 0;
				break;
		}
		logmsg("Plot resolution set to %gx%g\n", config->plotResX, config->plotResY);
		break;
	  case 'l':
		DisableLog();
		break;
	  case 'M':
		config->plotMissing = 0;
		break;
	  case 'm':
		{
			char manualType = '\0';
			long start = 0, end = 0;

			if(sscanf(optarg, "%c:%ld:%ld\n", &manualType, &start, &end) != 3 ||  
				(manualType != 'r' && manualType != 'c'))
			{
				logmsg("-ERROR: Invalid manual offset (-m) parameter: %s.\n", optarg);
				logmsg("  Must be of the form [r|c]:<start sample>:<end sample>\n", optarg);
				return 0;
			}

			if(end <= start)
			{
				logmsg("ERROR: For manual sample offset, ending offset must be bigger than the starting offset\n", optarg);
				return 0;
			}
			if(manualType == 'r')
			{
				config->ManualSyncRef = 1;
				config->ManualSyncRefStart = start;
				config->ManualSyncRefEnd = end;
				logmsg("- Reference ");
			}

			if(manualType == 'c')
			{
				config->ManualSyncComp = 1;
				config->ManualSyncCompStart = start;
				config->ManualSyncCompEnd = end;
				logmsg("- Comparison ");
			}
			logmsg("manual sample offset set %ld-%ld\n", start, end);
		}
		break;
	  case 'N':
		config->logScale = 0;
		logmsg("\t-Plots will not be adjusted to log scale\n");
		break;
	  case 'n':
		switch(optarg[0])
		{
			case 't':
				config->normType = max_time;
				logmsg("\t-Using Time Domain Normalization\n");
				break;
			case 'f':
				config->normType = max_frequency;
				logmsg("\t-Using Frequency Domain Normalization\n");
				break;
			case 'a':
				config->normType = average;
				logmsg("\t-Using Average Fundamental Frequency Normalization\n");
				break;
			case 'n':
				config->normType = none;
				logmsg("\t-Not using Normalization\n");
				break;
			default:
				logmsg("-ERROR: Invalid Normalization option '%c'\n", optarg[0]);
				logmsg("\tUse 't' Time Domain Max, 'f' Frequency Domain Max or 'a' Average\n");
				return 0;
				break;
		}
		break;
	  case 'O':
		config->plotPhase = 0;
		break;
	  case 'o':
		config->outputFilterFunction = atoi(optarg);
		if(config->outputFilterFunction < 0 || config->outputFilterFunction > 5)
		{
			logmsg("-ERROR: Invalid Output Filter option '%s'\n", optarg);
			return 0;
		}
		break;
	  case 'P':
		sprintf(config->profileFile, "%s", optarg);
		break;
	  case 'p':
		config->significantAmplitude = atof(optarg);
		if(config->significantAmplitude == 0)
		{
			config->noiseFloorAutoAdjust = 0;
			config->significantAmplitude = SIGNIFICANT_AMPLITUDE;
		}
		else if(config->significantAmplitude < -250.0 || config->significantAmplitude > -1.0)
		{
			logmsg("-ERROR: Significant amplitude must be between %d and %d\n", -1, -250);
			return 0;
		} else {
			config->ignoreFloor = 2;
			config->origSignificantAmplitude = config->significantAmplitude;
		}
		break;
	  case 'Q':
		config->plotTimeDomain = 0;
		break;
	  case 'R':
		config->doSamplerateAdjust = 1;
		logmsg("\t-Adjusting sample rate if inconsistency found\n");
		break;
	  case 'r':
		sprintf(config->referenceFile, "%s", optarg);
		ref = 1;
		break;
	  case 'S':
		config->plotSpectrogram = 0;
		break;
	  case 's':
		config->startHz = atof(optarg);
		if(config->startHz < 1.0 || config->startHz > MAX_HZ-100.0)
		{
			logmsg(" - ERROR: Requested %g start frequency is out of range\n", atof(optarg));
			return 0;
		}
		logmsg("\t-Frequency start range for FFTW is now %g (default %g)\n", config->startHz, START_HZ);
		break;
	  case 'T':
		config->syncTolerance++;
		if(config->syncTolerance > 3)
			config->syncTolerance = 3;
		break;
	  case 't':
		config->plotTimeSpectrogram = 0;
		break;
	  case 'u':
		config->plotAllNotes++;
		if(config->plotAllNotes > 3)
			config->plotAllNotes = 3;
		break;
	  case 'V':  // reserved
		break;
	  case 'v':
		config->verbose++;
		if(config->verbose > 3)
			config->verbose = 3;
		break;
	  case 'W':
		config->whiteBG = 1;
		break;
	 case 'w':
		switch(optarg[0])
		{
			case 'n':
			case 'f':
			case 'h':
			case 't':
			case 'm':
				config->window = optarg[0];
				break;
			default:
				logmsg("-ERROR: Invalid Window for FFT option '%c'\n", optarg[0]);
				logmsg("\t  Use n for None, t for Tukey window (default), f for Flattop, h for Hann or m for Hamming window\n");
				return 0;
				break;
		}
		break;
	  case 'X':
		config->useExtraData = 0;
		break;
	  case 'x':
		config->extendedResults = 1;
		config->showAll = 1;
		break;
	  case 'Y':
		config->videoFormatRef = atoi(optarg);
		if(config->videoFormatRef < 0 || config->videoFormatRef > MAX_SYNC)  // We'll confirm this later
		{
			logmsg("-ERROR: Profile can have up to %d types\n", MAX_SYNC);
			return 0;
		}
		break;
	  case 'y':
		config->debugSync = 1;
		config->timeDomainSync = 1;
		break;
	  case 'Z':
		config->videoFormatCom = atoi(optarg);
		if(config->videoFormatRef < 0 || config->videoFormatRef > MAX_SYNC)
		{
			logmsg("- ERROR: Profile can have up to %d types\n", MAX_SYNC);
			return 0;
		}
		break;
	  case 'z':
		config->ZeroPad = 1;
		logmsg("\t-FFT bins will be aligned to 1Hz, this is slower\n");
		break;
	  case '0':
		sprintf(config->outputPath, "%s", optarg);
		break;
	  case '8':
		config->logScaleTS = 1;
		logmsg("\t-Using linear scale for Time Spectrogram plots\n");
		break;
	  case '9':
		config->compressToBlocks = 1;
		break;
	  case '?':
		if (optopt == 'b')
		  logmsg("\t ERROR: Bar Difference -%c option requires a real number.\n", optopt);
		else if (optopt == 'c')
		  logmsg("\t ERROR: Compare File -%c requires an argument.\n", optopt);
		else if (optopt == 'd')
		  logmsg("\t ERROR: Max DB Height for Plots -%c requires an argument: %g-%g\n", 0.1, 60.0, optopt);
		else if (optopt == 'e')
		  logmsg("\t ERROR: Max frequency range for FFTW -%c requires an argument: %d-%d\n", START_HZ*2, END_HZ, optopt);
		else if (optopt == 'f')
		  logmsg("\t ERROR: Max # of frequencies to use from FFTW -%c requires an argument: 1-%d\n", optopt, MAX_FREQ_COUNT);
		else if (optopt == 'H')
		  logmsg("\t ERROR: Highly different waveform  -%c requires an argument: 0.0-100.0\n", optopt);
		else if (optopt == 'L')
		  logmsg("\t ERROR: Plot Resolution -%c requires an argument: 1-6\n", optopt);
		else if (optopt == 'm')
		  logmsg("\t ERROR: Manual sync -%c requires an argument: [r|c]:<s>:<e>\n", optopt);
		else if (optopt == 'n')
		  logmsg("\t ERROR: Normalization type -%c requires an argument:\n\tUse 't' Time Domain Max, 'f' Frequency Domain Max or 'a' Average\n");
		else if (optopt == 'o')
		  logmsg("\t ERROR: Output curve -%c requires an argument 0-4\n", optopt);
		else if (optopt == 'P')
		  logmsg("\t ERROR: Profile File -%c requires a file argument\n", optopt);
		else if (optopt == 'p')
		  logmsg("\t ERROR: Significant Amplitude -%c requires an argument: -1.0 to -200.0 dBFS\n\t\tOr 0 for Auto Adjustment to Comparision Noise Floor\n", optopt);
		else if (optopt == 'r')
		  logmsg("\t ERROR: Reference File -%c requires an argument.\n", optopt);
		else if (optopt == 's')
		  logmsg("\t ERROR: Min frequency range for FFTW -%c requires an argument: %d-%d\n", 1, END_HZ-100, optopt);
		else if (optopt == 'w')
		  logmsg("\t ERROR: FFT Window option -%c requires an argument: n,t,f or h\n", optopt);
		else if (optopt == 'Y')
		  logmsg("\t ERROR: Reference format: needs a number with a selection from the profile\n");
		else if (optopt == 'Z')
		  logmsg("\t ERROR: Comparison format: needs a number with a selection from the profile\n");
		else if (optopt == '0')
		  logmsg("\t ERROR: Output folder argument -%c requires a valid path.\n", optopt);
		else if (isprint (optopt))
		  logmsg("\t ERROR: Unknown option `-%c'.\n", optopt);
		else
		  logmsg("\t ERROR: Unknown option character `\\x%x'.\n", optopt);
		return 0;
		break;
	  default:
		logmsg("\t ERROR: Invalid argument %c\n", optopt);
		return(0);
		break;
	  }
	
	for(index = optind; index < argc; index++)
	{
		logmsg("ERROR: Invalid argument %s\n", argv[index]);
		return 0;
	}

	if(!ref || !tar)
	{
		logmsg("  usage: mdfourier -P profile.mdf -r reference.wav -c compare.wav\n");
		logmsg("  ERROR: Please define both reference and compare audio files\n");
		return 0;
	}

	if(config->FullTimeSpectroScale)
		config->MaxFreq = END_HZ;

	if(config->endHz <= config->startHz)
	{
		logmsg("-ERROR: Invalid frequency range for FFTW (%g Hz to %g Hz)\n", 
				config->startHz, config->endHz);
		return 0;
	}

	if(!config->plotDifferences && !config->plotMissing &&
		!config->plotSpectrogram && !config->averagePlot &&
		!config->plotNoiseFloor && !config->plotTimeSpectrogram &&
		!config->plotTimeDomain && !config->plotPhase)
	{
		logmsg("-ERROR: It makes no sense to process everything and plot nothing\nAborting.\n");
		return 0;
	}

	file = fopen(config->profileFile, "rb");
	if(!file)
	{
		logmsg("- ERROR: Could not load profile configuration file: \"%s\"\n", config->profileFile);
		return 0;
	}
	fclose(file);

	file = fopen(config->referenceFile, "rb");
	if(!file)
	{
		logmsg("- ERROR: Could not open REFERENCE file: \"%s\"\n", config->referenceFile);
		return 0;
	}
	fclose(file);

	file = fopen(config->comparisonFile, "rb");
	if(!file)
	{
		logmsg("- ERROR: Could not open COMPARE file: \"%s\"\n", config->comparisonFile);
		return 0;
	}
	fclose(file);

	if(config->verbose)
	{
		if(config->window != 'n')
			logmsg("\tA %s window will be applied to each block to be compared\n", GetWindow(config->window));
		else
			logmsg("\tNo window (rectangle) will be applied to each block to be compared\n");
	}

	if(config->ZeroPad && config->FullTimeSpectroScale)
		logmsg("\t -Go and play an arcade game credit if you have a slow CPU like mine...\n");
	if(config->averagePlot && !config->weightedAveragePlot)
		logmsg("\tAveraged Plots will not be weighted\n");

	if(config->logScale && config->plotRatio == 0)
		config->plotRatio = config->endHzPlot/log10(config->endHzPlot);

	if(config->plotAllNotes)
	{
		switch(config->plotAllNotes)
		{
			case 1:
				logmsg("\t-Creating waveform plots for all notes\n");
				break;
			case 2:
				config->plotAllNotesWindowed = 1;
				logmsg("\t-Creating DFT windowed waveform plots for all notes\n");
				break;
			case 3:
				config->plotAllNotesWindowed = 1;
				logmsg("\t-Creating waveform plots for all notes with window for DFFT\n");
				break;
		}
	}

	return 1;
}

int checkPath(char *path)
{
	int		len = 0;
	char 	*CurrentPath = NULL;

	if(!path || strlen(path) == 0)
		return 1;

	len = strlen(path);
	if(path[len-1] != FOLDERCHAR)
	{
		if(len < BUFFER_SIZE)
		{
			path[len] = FOLDERCHAR;
			path[len+1] = '\0';
		}
		else
		{
			logmsg("Path too long %s\n", path);
			return 0;
		}
	}

	CurrentPath = (char*)malloc(sizeof(char)*FILENAME_MAX);
	if(!CurrentPath)
		return 0;

	if(!GetCurrentDir(CurrentPath, sizeof(char)*FILENAME_MAX))
	{
		free(CurrentPath);
		logmsg("Could not get current path\n");
		return 0;
	}

	if(chdir(path) == -1)
	{
		free(CurrentPath);
		logmsg("Could not open selected path '%s'\n", path);
		return 0;
	}

	PopMainPath(&CurrentPath);
	return 1;
}

/*
char *getTempDir()
{
	char *tmp = NULL;
	
	tmp = getenv("TMPDIR");
	if(!tmp)
		tmp = getenv("TEMP");
	if(!tmp)
		tmp = getenv("TMP");
	return tmp;
}
*/

int checkAlternatePaths(parameters *config)
{
	if(!checkPath(config->outputPath))
		return 0;
	return 1;
}

char *PushMainPath(parameters *config)
{
	char 	*CurrentPath = NULL;

	if(strlen(config->outputPath) == 0)
		return NULL;

	CurrentPath = (char*)malloc(sizeof(char)*FILENAME_MAX);
	if(!CurrentPath)
		return NULL;

	if(!GetCurrentDir(CurrentPath, sizeof(char)*FILENAME_MAX))
	{
		free(CurrentPath);
		logmsg("Could not get current path\n");
		return NULL;
	}

	if(chdir(config->outputPath) == -1)
	{
		free(CurrentPath);
		logmsg("Could not change to selected path '%s'\n", config->outputPath);
		return NULL;
	}
	return CurrentPath;
}

void PopMainPath(char **CurrentPath)
{
	if(!CurrentPath || !*CurrentPath)
		return;

	if(chdir(*CurrentPath) == -1)
		logmsg("Could not open working folder %s\n", CurrentPath);

	free(*CurrentPath);
	*CurrentPath = NULL;
}

int SetupFolders(char *folder, char *logname, parameters *config)
{
	char *mainDir = NULL;

	if(!checkAlternatePaths(config))
		return 0;

	mainDir = PushMainPath(config);
	if(!CreateFolderName(folder, config))
	{
		PopMainPath(&mainDir);
		return 0;
	}

	if(IsLogEnabled())
	{
		char tmp[BUFFER_SIZE*4+256];
		char logfname[BUFFER_SIZE*2];

		sprintf(logfname, "%s_%s", logname, config->compareName);

		ComposeFileName(tmp, logfname, ".txt", config);

		if(!setLogName(tmp))
		{
			PopMainPath(&mainDir);
			return 0;
		}

		Header(1, 0, NULL);
	}

	PopMainPath(&mainDir);
	return 1;
}

void ShortenFileName(char *filename, char *copy)
{
	int len = 0, ext = 0;

	sprintf(copy, "%s", filename);
	len = strlen(copy);
	ext = getExtensionLength(copy)+1;
	copy[len-ext] = '\0';
	len = strlen(copy);

#if defined (WIN32)
	if(len > MAX_FILE_NAME)
		copy[MAX_FILE_NAME - 1] = '\0';
#endif
}

int CreateFolder(char *name)
{
#if defined (WIN32)
#if INTPTR_MAX == INT64_MAX
#define	_mkdir mkdir
#endif
	if(_mkdir(name) != 0)
	{
		if(errno != EEXIST)
			return 0;
	}
#else
	if(mkdir(name, 0755) != 0)
	{
		if(errno != EEXIST)
			return 0;
	}
#endif
	return 1;
}

int IsValidFolderCharacter(char c)
{
	switch(c)
	{
		case '/':	// the rest are invalid in windows only, but we remove them anyway
			return CHAR_FOLDER_CHANGE_T1;
			break;
		case '\\':
		case '<':
		case '>':
		case '"':
		case '|':
		case '?':
		case '*':
		case ' ':		// valid, but we remove it
			return CHAR_FOLDER_REMOVE;
			break;
		case ':':
			return CHAR_FOLDER_CHANGE_T2;
			break;
		default:
			return CHAR_FOLDER_OK;
			break;
	}
	return 1;
}

int CleanFolderName(char *name, char *origName)
{
	int len = 0, size = 0, change = 0;

	if(!name || !origName)
		return -1;

	len = strlen(origName);
	if(!len)
		return -1;
	for(int i = 0; i < len; i++)
	{
		int act;

		act = IsValidFolderCharacter(origName[i]);
		if(act == CHAR_FOLDER_OK)
			name[size++] = origName[i];
		if(act == CHAR_FOLDER_CHANGE_T1)
		{
			name[size++] = '_';
			change ++;
		}
		if(act == CHAR_FOLDER_CHANGE_T2)
		{
			name[size++] = '-';
			change ++;
		}
		if(act == CHAR_FOLDER_REMOVE)
			change ++;
	}
	name[size] = '\0';
	return change;
}

int CreateFolderName(char *mainfolder, parameters *config)
{
	int len = 0;
	char tmp[BUFFER_SIZE/2], fn[BUFFER_SIZE/2], pname[BUFFER_SIZE/2], popfolder[FILENAME_MAX];

	if(!config)
		return 0;

	// Compose the folder name a_vs_b_0000
	ShortenFileName(basename(config->referenceFile), tmp);
	len = strlen(tmp);
	if(strlen(config->comparisonFile))
	{
		ShortenFileName(basename(config->comparisonFile), fn);
		sprintf(tmp+len, "_vs_%s_0000", fn);

		len = strlen(tmp);
	}

	for(int i = 0; i < len; i++)
	{
		if(tmp[i] == ' ')
			tmp[i] = '_';
	}

	// Setup the top level folder for profile if it doesn't exist
	sprintf(pname, "%s", config->types.Name);
	if(CleanFolderName(pname, config->types.Name) == -1)
	{
		logmsg("ERROR: Invalid Name '%s'\n", config->types.Name);
		return 0;
	}

	sprintf(config->compareName, "%s", tmp);
	sprintf(config->folderName, "%s%c%s", mainfolder, FOLDERCHAR, pname);

	// Create the top level folder "MDFResults"
	if(!CreateFolder(mainfolder))
	{
		logmsg("ERROR: Could not create '%s'\n", mainfolder);
		return 0;
	}
	// Create the top level folder for profile if it doesn't exist
	if(!CreateFolder(config->folderName))
	{
		logmsg("ERROR: Could not create '%s'\n", config->folderName);
		return 0;
	}

	// Finally, set the current results folder name
	sprintf(config->folderName, "%s%c%s%c%s", mainfolder, FOLDERCHAR, pname, FOLDERCHAR, tmp);
	// get top folder if in case we need to pop out of it
	if(!GetCurrentDir(popfolder, sizeof(char)*FILENAME_MAX))
	{
		logmsg("ERROR: Could not get current path\n");
		return 0;
	}
	// Check if folder already exists
	if(chdir(config->folderName) != -1)
	{
		int value = 0, found = 0;

		if(chdir(popfolder) == -1)  // return to proper parent folder
		{
			// what we failed?!?!?!
			logmsg("ERROR: Could not change folder to '%s'\n", popfolder);
			return 0;
		}

		len = strlen(config->folderName);
		value = atoi(config->folderName+len-4);
		do
		{
			value++;
			sprintf(config->folderName+len-4, "%04d", value);
			if(chdir(config->folderName) == -1) // we couldn't push to it, got it
				found = 1;
			else
			{
				if(chdir(popfolder) == -1)  // return to proper parent folder
				{
					logmsg("ERROR: Could not change folder to '%s'\n", popfolder);
					return 0;
				}
			}
		}while(!found && value < 10000);

		if(value >= 10000)
		{
			logmsg("ERROR: Could not create '%s', only 10000 folder per comparison supported\n", config->folderName);
			return 0;
		}
	}

	if(!CreateFolder(config->folderName))
	{
		logmsg("ERROR: Could not create '%s'\n", config->folderName);
		return 0;
	}
	return 1;
}

void InvertComparedName(parameters *config)
{
	int len;
	char tmp[BUFFER_SIZE], fn[BUFFER_SIZE];

	ShortenFileName(basename(config->referenceFile), tmp);
	len = strlen(tmp);
	ShortenFileName(basename(config->comparisonFile), fn);
	sprintf(tmp+len, "_vs_%s", fn);

	len = strlen(tmp);
	for(int i = 0; i < len; i++)
	{
		if(tmp[i] == ' ')
			tmp[i] = '_';
	}

	sprintf(config->compareName, "%s", tmp);
}

char *GetNormalization(enum normalize n)
{
	switch(n)
	{
		case max_time:
			return "TD";
		case max_frequency:
			return "FD";
		case average:
			return "AV";
		default:
			return "ERROR";
	}
}

void ComposeFileName(char *target, char *subname, char *ext, parameters *config)
{
	if(!config)
		return;

	sprintf(target, "%s%c%s%s",
		config->folderName, FOLDERCHAR, subname, ext); 
}

void ComposeFileNameoPath(char *target, char *subname, char *ext, parameters *config)
{
	if(!config)
		return;

	sprintf(target, "%s%s", subname, ext); 
}

double TimeSpecToSeconds(struct timespec* ts)
{
	return (double)ts->tv_sec + (double)ts->tv_nsec / 1000000000.0;
}

char *GetChannel(char c)
{
	switch(c)
	{
		case 'l':
			return "Left";
		case 'r':
			return "Right";
		case 's':
			return "Stereo";
		default:
			return "ERROR";
	}
}

char *GetWindow(char c)
{
	switch(c)
	{
		case 'n':
			return "Rectangular";
		case 't':
			return "Tukey";
		case 'f':
			return "Flattop";
		case 'h':
			return "Hann";
		case 'm':
			return "Hamming";
		default:
			return "ERROR";
	}
}

char *getFilenameExtension(char *filename)
{
	char *dot = NULL;

	dot = strrchr(filename, '.');
	if(!dot || dot == filename) 
		return "";
	return dot + 1;
}

int getExtensionLength(char *filename)
{
	const char *ext = NULL;

	ext = getFilenameExtension(filename);
	if(ext)
		return strlen(ext);

	return 0;
}

// clock_gettime is not implemented on older versions of OS X (< 10.12).
// so this is only used for those builds
#ifdef USE_GETTIME_INSTEAD
int clock_gettime(int clk_id, struct timespec* t)
{
	struct timeval now;
	int rv = gettimeofday(&now, NULL);

	(void)clk_id;
	if (rv)
		return rv;
	t->tv_sec  = now.tv_sec;
	t->tv_nsec = now.tv_usec * 1000;
	return 0;
}
#endif
