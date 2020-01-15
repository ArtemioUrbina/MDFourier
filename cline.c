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

#include "cline.h"
#include "log.h"

void PrintUsage()
{
	logmsg("  usage: mdfourier -P profile.mdf -r reference.wav -c compare.wav\n");
	logmsg("   FFT and Analysis options:\n");
	logmsg("	 -a: select <a>udio channel to compare. 's', 'l' or 'r'\n");
	logmsg("	 -w: enable <w>indowing. Default is a custom Tukey window.\n");
	logmsg("		'n' none, 't' Tukey, 'h' Hann, 'f' FlatTop & 'm' Hamming\n");
	logmsg("	 -f: Change the number of analyzed frequencies to use from FFTW\n");
	logmsg("	 -s: Defines <s>tart of the frequency range to compare with FFT\n");
	logmsg("	 -e: Defines <e>nd of the frequency range to compare with FFT\n");
	logmsg("	 -i: <i>gnores the silence block noise floor if present\n");
	logmsg("	 -z: Uses <z>ero Padding to equal 1 Hz FFT bins\n");
	logmsg("	 -n: <N>ormalize: 't' Time Domain Max, 'f' Frequency Domain Max or 'a' Average\n");
	logmsg("	 -B: Do not do stereo channel audio <B>alancing\n");
	logmsg("	 -V: Ignore a<V>erage for analysis\n");
	logmsg("	 -I: <I>gnore frame rate difference for analysis\n");
	logmsg("	 -p: Define the significant volume value in dBFS\n");
	logmsg("	 -T: Increase Sync detection <T>olerance\n");
	logmsg("	 -Y: Define the Reference Video Format 0:NTSC 1:PAL\n");
	logmsg("	 -Z: Define the Comparison Video Format 0:NTSC 1:PAL\n");
	logmsg("	 -k: cloc<k> FFTW operations\n");
	logmsg("	 -X: do not include extra Data\n");
	logmsg("   Output options:\n");
	logmsg("	 -l: <l>og output to file [reference]_vs_[compare].txt\n");
	logmsg("	 -v: Enable <v>erbose mode, spits all the FFTW results\n");
	logmsg("	 -C: Create <C>SV file with plot values.\n");
	logmsg("	 -b: Change <b>ar value for frequency match tolerance, default is 1.0dBFS.\n");
	logmsg("	 -g: Create avera<g>e points over the plotted graphs\n");
	logmsg("	 -A: Do not weight values in <A>veraged Plot (implies -g)\n");
	logmsg("	 -W: Use <W>hite background for plots.\n");
	logmsg("	 -L: Create 800x400 plots, as used in the manual\n");
	logmsg("	 -H: Create 1920x1080 plots\n");
	logmsg("	 -D: Don't create <D>ifferences Plots\n");
	logmsg("	 -M: Don't create <M>issing Plots\n");
	logmsg("	 -S: Don't create <S>pectrogram Plots\n");
	logmsg("	 -F: Don't create Noise <F>loor Plots\n");
	logmsg("	 -t: Don't create Time Spectrogram Plots\n");
	logmsg("	 -Q: Don't create Time Domain Plots\n");
	logmsg("	 -o: Define the output filter function for color weights [0-5]\n");
	logmsg("	 -E: Defines Full frequency rang<E> for Time Spectrogram plots\n");
	logmsg("	 -R: Do the reverse compare plots\n");
	logmsg("	 -N: Use li<N>ear scale instead of logaritmic scale for plots\n");
	logmsg("	 -d: Max <d>BFS for plots vertically\n");
	logmsg("	 -j: (text) Cuts per block information and shows <j>ust total results\n");
	logmsg("	 -x: (text) Enables e<x>tended log results. Shows a table with all matches\n");
	logmsg("	 -m: (text) Enables Show all blocks compared with <m>atched frequencies\n");
	logmsg("	 -y: Output debug Sync pulse detection algorithm information\n");
}

void Header(int log)
{
	char title1[] = "MDFourier " MDVERSION " [240p Test Suite Fourier Audio compare tool]\n";
	char title2[] = "Artemio Urbina 2019-2020 free software under GPL - http://junkerhq.net/MDFourier\n";

	if(log)
		logmsg("%s%s", title1, title2);
	else
		printf("%s%s", title1, title2);
}

void CleanParameters(parameters *config)
{
	memset(config, 0, sizeof(parameters));

	sprintf(config->profileFile, PROFILE_FILE);
	config->startHz = START_HZ;
	config->endHz = END_HZ;
	config->startHzPlot = 0;
	config->endHzPlot = END_HZ;
	config->maxDbPlotZC = DB_HEIGHT;
	config->extendedResults = 0;
	config->justResults = 0;
	config->verbose = 0;
	config->window = 't';
	config->channel = 's';
	config->MaxFreq = FREQ_COUNT;
	config->clock = 0;
	config->showAll = 0;
	config->ignoreFloor = 0;
	config->useOutputFilter = 1;
	config->outputFilterFunction = 3;
	config->origSignificantAmplitude = SIGNIFICANT_VOLUME;
	config->significantAmplitude = SIGNIFICANT_VOLUME;
	config->referenceNoiseFloor = 0;
	config->smallerFramerate = 0;
	config->referenceFramerate = 0;
	config->ZeroPad = 0;
	config->debugSync = 0;
	config->drawWindows = 0;
	config->channelBalance = 1;
	config->showPercent = 1;
	config->ignoreFrameRateDiff = 0;
	config->labelNames = 1;
	config->averageIgnore = 0;
	config->averageLine = 0.0;
	config->outputCSV = 0;
	config->whiteBG = 0;
	config->smallFile = 0;
	config->videoFormatRef = NTSC;
	config->videoFormatCom = NTSC;
	config->syncTolerance = 0;
	config->AmpBarRange = BAR_DIFF_DB_TOLERANCE;
	config->FullTimeSpectroScale = 0;
	config->normalizationRatio = 0;
	config->hasTimeDomain = 0;

	config->logScale = 1;
	config->reverseCompare = 0;
	config->normType = max_frequency;

	config->refNoiseMin = 0;
	config->refNoiseMax = 0;

	config->plotResX = PLOT_RES_X;
	config->plotResY = PLOT_RES_Y;

	config->plotDifferences = 1;
	config->plotMissing = 1;
	config->plotSpectrogram = 1;
	config->plotTimeSpectrogram = 1;
	config->plotNoiseFloor = 1;
	config->plotTimeDomain = 1;
	config->plotAllNotes = 0;
	config->plotAllNotesWindowed = 0;
	config->averagePlot = 0;
	config->weightedAveragePlot = 1;

	config->Differences.BlockDiffArray = NULL;
	config->Differences.cntFreqAudioDiff = 0;
	config->Differences.cntAmplAudioDiff = 0;
	config->Differences.weightedFreqAudio = 0;
	config->Differences.weightedAmplAudio = 0;
	config->Differences.cntTotalCompared = 0;
	config->Differences.cntTotalAudioDiff = 0;
	config->Differences.weightedAudioDiff = 0;
	
	config->types.totalChunks = 0;
	config->types.regularChunks = 0;

	memset(config->types.SyncFormat, 0, sizeof(VideoBlockDef)*2);
	config->types.typeArray = NULL;
	config->types.typeCount = 0;

	config->sync_plan = NULL;
	config->model_plan = NULL;
	config->reverse_plan = NULL;

	config->referenceSignal = NULL;
	config->comparisonSignal = NULL;
	config->nyquistLimit = 0;  // only used in MDWave

	config->clkBlock = NO_CLK;
	config->clkFreq = 0;
	config->clkFreqCount = 0;
	config->clkRatio = 0;
	config->useExtraData = 1;
	config->compressToBlocks = 0;
}

int commandline(int argc , char *argv[], parameters *config)
{
	FILE *file = NULL;
	int c, index, ref = 0, tar = 0;
	
	opterr = 0;
	
	CleanParameters(config);

	// Available: GHJKLOq2356789
	while ((c = getopt (argc, argv, "Aa:Bb:Cc:Dd:Ee:Ff:ghIijkLlMmNn:o:P:p:QRr:Ss:TtUuVvWw:XxY:yZ:z014")) != -1)
	switch (c)
	  {
	  case 'A':
		config->averagePlot = 1;
		config->weightedAveragePlot = 0;
		break;
	  case 'a':
		switch(optarg[0])
		{
			case 'l':
			case 'r':
			case 's':
				config->channel = optarg[0];
				break;
			default:
				logmsg("Invalid audio channel option '%c'\n", optarg[0]);
				logmsg("\tUse l for Left, r for Right or s for Stereo\n");
				return 0;
				break;
		}
		break;
	  case 'B':
		config->channelBalance = 0;
		break;
	  case 'b':
		config->AmpBarRange = atof(optarg);
		if(config->AmpBarRange < 0 || config->AmpBarRange > 16)
			config->AmpBarRange = BAR_DIFF_DB_TOLERANCE;
		break;
	  case 'C':
		config->outputCSV = 1;
		break;
	  case 'c':
		sprintf(config->targetFile, "%s", optarg);
		tar = 1;
		break;
	  case 'D':
		config->plotDifferences = 0;
		break;
	  case 'd':
		config->maxDbPlotZC = atof(optarg);
		if(config->maxDbPlotZC < 0.1 || config->maxDbPlotZC > 96.0)
			config->maxDbPlotZC = DB_HEIGHT;
		break;
	  case 'E':
		config->FullTimeSpectroScale = 1;
		break;
	  case 'e':
		config->endHz = atof(optarg);
		if(config->endHz < START_HZ*2.0 || config->endHz > END_HZ)
			config->endHz = END_HZ;
		break;
	  case 'F':
		config->plotNoiseFloor = 0;
		break;
	  case 'f':
		config->MaxFreq = atoi(optarg);
		if(config->MaxFreq < 1 || config->MaxFreq > MAX_FREQ_COUNT)
			config->MaxFreq = MAX_FREQ_COUNT;
		break;
	  case 'g':
		config->averagePlot = 1;
		break;
	  case 'h':
		PrintUsage();
		return 0;
		break;
	  case 'I':
		config->ignoreFrameRateDiff = 1;
		break;
	  case 'i':
		config->ignoreFloor = 1;
		break;
	  case 'j':
		config->justResults = 1;
		break;
	  case 'k':
		config->clock = 1;
		break;
	  case 'l':
		EnableLog();
		break;
	  case 'M':
		config->plotMissing = 0;
		break;
	  case 'm':
		config->showAll = 1;
		break;
	  case 'N':
		config->logScale = 0;
		break;
	  case 'n':
		switch(optarg[0])
		{
			case 't':
				config->normType = max_time;
				break;
			case 'f':
				config->normType = max_frequency;
				break;
			case 'a':
				config->normType = average;
				break;
			case 'n':
				config->normType = none;
				break;
			default:
				logmsg("Invalid Normalization option '%c'\n", optarg[0]);
				logmsg("\tUse 't' Time Domain Max, 'f' Frequency Domain Max or 'a' Average\n");
				return 0;
				break;
		}
		break;
	  case 'o':
		config->outputFilterFunction = atoi(optarg);
		if(config->outputFilterFunction < 0 || config->outputFilterFunction > 5)
			config->outputFilterFunction = 3;
		if(!config->outputFilterFunction)
			config->useOutputFilter = 0;
		break;
	  case 'P':
		sprintf(config->profileFile, "%s", optarg);
		break;
	  case 'p':
		config->significantAmplitude = atof(optarg);
		if(config->significantAmplitude <= -120.0 || config->significantAmplitude >= -1.0)
			config->significantAmplitude = SIGNIFICANT_VOLUME;
		config->origSignificantAmplitude = config->significantAmplitude;
		break;
	  case 'Q':
		config->plotTimeDomain = 1;
		break;
	  case 'R':
		config->reverseCompare = 1;
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
		if(config->startHz < 1.0 || config->startHz > END_HZ-100.0)
			config->startHz = START_HZ;
		break;
	  case 'T':
		config->syncTolerance = 1;
		break;
	  case 't':
		config->plotTimeSpectrogram = 0;
		break;
	  case 'U':
		config->plotAllNotes = 1;
		config->plotAllNotesWindowed = 1;
		break;
	  case 'u':
		config->plotAllNotes = 1;
		break;
	  case 'V':
		config->averageIgnore = 1;
		break;
	  case 'v':
		config->verbose = 1;
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
				logmsg("Invalid Window for FFT option '%c'\n", optarg[0]);
				logmsg("\tUse n for None, t for Tukey window (default), f for Flattop, h for Hann or m for Hamming window\n");
				return 0;
				break;
		}
		break;
	  case 'X':
		config->useExtraData = 0;
		break;
	  case 'x':
		config->extendedResults = 1;
		break;
	  case 'Y':
		config->videoFormatRef = atoi(optarg);
		if(config->videoFormatRef < NTSC|| config->videoFormatRef > PAL)
			config->videoFormatRef = NTSC;
		break;
	  case 'y':
		config->debugSync = 1;
		break;
	  case 'Z':
		config->videoFormatCom = atoi(optarg);
		if(config->videoFormatCom < NTSC|| config->videoFormatCom > PAL)
			config->videoFormatCom = NTSC;
		break;
	  case 'z':
		config->ZeroPad = 1;
		break;
	  case '0':
		config->plotResX = PLOT_RES_X_LOW;
		config->plotResY = PLOT_RES_Y_LOW;
		config->showPercent = 0;
		break;
	  case '1':
		config->plotResX = PLOT_RES_X_1K;
		config->plotResY = PLOT_RES_Y_1K;
		break;
	  case '4':
		config->plotResX = PLOT_RES_X_4K;
		config->plotResY = PLOT_RES_Y_4K;
		break;
	  case '?':
		if (optopt == 'a')
		  logmsg("Audio channel option -%c requires an argument: l,r or s\n", optopt);
		else if (optopt == 'b')
		  logmsg("Bar Difference -%c option requires a real number.\n", optopt);
		else if (optopt == 'c')
		  logmsg("Compare File -%c requires an argument.\n", optopt);
		else if (optopt == 'd')
		  logmsg("Max DB Height for Plots -%c requires an argument: %g-%g\n", 0.1, 60.0, optopt);
		else if (optopt == 'e')
		  logmsg("Max frequency range for FFTW -%c requires an argument: %d-%d\n", START_HZ*2, END_HZ, optopt);
		else if (optopt == 'f')
		  logmsg("Max # of frequencies to use from FFTW -%c requires an argument: 1-%d\n", optopt, MAX_FREQ_COUNT);
		else if (optopt == 'n')
		  logmsg("Normalizatiuon type -%c requires an argument:\n\tUse 't' Time Domain Max, 'f' Frequency Domain Max or 'a' Average\n");
		else if (optopt == 'o')
		  logmsg("Output curve -%c requires an argument 0-4\n", optopt);
		else if (optopt == 'P')
		  logmsg("Profile File -%c requires a file argument\n", optopt);
		else if (optopt == 'p')
		  logmsg("Significant Amplitude -%c requires an argument: -1.0 to -100.0 dBFS\n", optopt);
		else if (optopt == 'r')
		  logmsg("Reference File -%c requires an argument.\n", optopt);
		else if (optopt == 's')
		  logmsg("Min frequency range for FFTW -%c requires an argument: %d-%d\n", 1, END_HZ-100, optopt);
		else if (optopt == 'w')
		  logmsg("FFT Window option -%c requires an argument: n,t,f or h\n", optopt);
		else if (optopt == 'Y')
		  logmsg("Reference format: Use 0 for NTSC and 1 for PAL\n");
		else if (optopt == 'Z')
		  logmsg("Comparison format: Use 0 for NTSC and 1 for PAL\n");
		else if (isprint (optopt))
		  logmsg("Unknown option `-%c'.\n", optopt);
		else
		  logmsg("Unknown option character `\\x%x'.\n", optopt);
		return 0;
		break;
	  default:
		logmsg("Invalid argument %c\n", optopt);
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
		logmsg("ERROR: Please define both reference and compare audio files\n");
		return 0;
	}

	if(config->extendedResults && config->justResults)
	{
		logmsg("* Just Results cancels Extended results\n");
		return 0;
	}

	if(config->showAll && config->justResults)
	{
		logmsg("* Just Results cancels Show All\n");
		return 0;
	}

	if(config->FullTimeSpectroScale)
		config->MaxFreq = END_HZ;

	if(config->endHz <= config->startHz)
	{
		logmsg("* Invalid frequency range for FFTW (%g Hz to %g Hz)\n", 
				config->startHz, config->endHz);
		return 0;
	}

	if(!config->plotDifferences && !config->plotMissing &&
		!config->plotSpectrogram && !config->averagePlot &&
		!config->plotNoiseFloor && !config->plotTimeSpectrogram &&
		!config->plotTimeDomain)
	{
		logmsg("* It makes no sense to process everything and plot nothing\nAborting.\n");
		return 0;
	}

	file = fopen(config->profileFile, "rb");
	if(!file)
	{
		logmsg("* ERROR: Could not load profile configuration file: \"%s\"\n", config->profileFile);
		return 0;
	}
	fclose(file);

	file = fopen(config->referenceFile, "rb");
	if(!file)
	{
		logmsg("* ERROR: Could not open REFERENCE file: \"%s\"\n", config->referenceFile);
		return 0;
	}
	fclose(file);

	file = fopen(config->targetFile, "rb");
	if(!file)
	{
		logmsg("* ERROR: Could not open COMPARE file: \"%s\"\n", config->targetFile);
		return 0;
	}
	fclose(file);

	if(!CreateFolderName(config))
	{
		logmsg("* ERROR: Could not create output folders\n");
		return 0;
	}
	CreateBaseName(config);

	if(IsLogEnabled())
	{
		char tmp[T_BUFFER_SIZE*2];

		ComposeFileName(tmp, "Log", ".txt", config);

		if(!setLogName(tmp))
			return 0;

		DisableConsole();
		Header(1);
		EnableConsole();
	}

	if(config->verbose)
	{
		if(config->window != 'n')
			logmsg("\tA %s window will be applied to each block to be compared\n", GetWindow(config->window));
		else
			logmsg("\tNo window (rectangle) will be applied to each block to be compared\n");
		if(config->useOutputFilter)
		{
			logmsg("\tOutput Filter function #%d will be applied to the results\n", 
					config->outputFilterFunction);
		}
		else
			logmsg("\tNo filtering will be applied to the results\n");
	}

	if(config->ZeroPad)
		logmsg("\tFFT bins will be aligned to 1Hz, this is slower\n");
	if(config->FullTimeSpectroScale)
		logmsg("\tFull Time spectrogram selected, this is slower\n");
	if(config->ZeroPad && config->FullTimeSpectroScale)
		logmsg("\tGo and play an arcade game credit if you have a slow CPU like mine...\n");
	if(config->ignoreFloor)
		logmsg("\tIgnoring Silence block noise floor\n");
	if(config->channel != 's')
		logmsg("\tAudio Channel is: %s\n", GetChannel(config->channel));
	if(config->MaxFreq != FREQ_COUNT)
		logmsg("\tMax frequencies to use from FFTW are %d (default %d)\n", config->MaxFreq, FREQ_COUNT);
	if(config->startHz != START_HZ)
		logmsg("\tFrequency start range for FFTW is now %g (default %g)\n", config->startHz, START_HZ);
	if(config->endHz != END_HZ)
		logmsg("\tFrequency end range for FFTW is now %g (default %g)\n", config->endHz, END_HZ);
	if(config->normType != max_frequency)
	{
		if(config->normType == max_time)
			logmsg("\tUsing Time Domain Normalization\n");
		if(config->normType == average)
			logmsg("\tUsing Average Fundamental Frequency Normalization\n");
	}
	if(!config->logScale)
		logmsg("\tPlots will not be adjusted to log scale\n");
	if(config->averagePlot && !config->weightedAveragePlot)
		logmsg("\tAveraged Plots will not be weighted\n");
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
	{	
		copy[MAX_FILE_NAME - 4] = rand() % 26 + 'a';
		copy[MAX_FILE_NAME - 3] = rand() % 26 + 'a';
		copy[MAX_FILE_NAME - 2] = rand() % 26 + 'a';
		copy[MAX_FILE_NAME - 1] = '\0';
	}
#endif
	
}

int CreateFolderName(parameters *config)
{
	int len;
	char tmp[BUFFER_SIZE], fn[BUFFER_SIZE];

	if(!config)
		return 0;

#if defined (WIN32)
	srand(time(NULL));
#endif

	ShortenFileName(basename(config->referenceFile), tmp);
	len = strlen(tmp);
	ShortenFileName(basename(config->targetFile), fn);
	sprintf(tmp+len, "_vs_%s", fn);

	len = strlen(tmp);
	for(int i = 0; i < len; i++)
	{
		if(tmp[i] == ' ')
			tmp[i] = '_';
	}

	sprintf(config->compareName, "%s", tmp);
	sprintf(config->folderName, "MDFResults\\%s", tmp);

#if defined (WIN32)
	if(_mkdir("MDFResults") != 0)
	{
		if(errno != EEXIST)
			return 0;
	}
	if(_mkdir(config->folderName) != 0)
	{
		if(errno != EEXIST)
			return 0;
	}
#else
	if(mkdir("MDFResults", 0755) != 0)
	{
		if(errno != EEXIST)
			return 0;
	}
	if(mkdir(config->folderName, 0755) != 0)
	{
		if(errno != EEXIST)
			return 0;
	}
#endif
	return 1;
}

void InvertComparedName(parameters *config)
{
	int len;
	char tmp[BUFFER_SIZE], fn[BUFFER_SIZE];

	ShortenFileName(basename(config->referenceFile), tmp);
	len = strlen(tmp);
	ShortenFileName(basename(config->targetFile), fn);
	sprintf(tmp+len, "_vs_%s", fn);

	len = strlen(tmp);
	for(int i = 0; i < len; i++)
	{
		if(tmp[i] == ' ')
			tmp[i] = '_';
	}

	sprintf(config->compareName, "%s", tmp);
}

int CreateFolderName_wave(parameters *config)
{
	int len;
	char tmp[BUFFER_SIZE+20], fn[BUFFER_SIZE];

	if(!config)
		return 0;

	ShortenFileName(basename(config->referenceFile), fn);
	sprintf(tmp, "MDWave\\%s", fn);

	len = strlen(tmp);
	for(int i = 0; i < len; i++)
	{
		if(tmp[i] == ' ')
			tmp[i] = '_';
	}

	sprintf(config->folderName, "%s", tmp);

#if defined (WIN32)
	if(_mkdir("MDWave") != 0)
	{
		if(errno != EEXIST)
			return 0;
	}
	if(_mkdir(config->folderName) != 0)
	{
		if(errno != EEXIST)
			return 0;
	}
#else
	if(mkdir("MDWave", 0755) != 0)
	{
		if(errno != EEXIST)
			return 0;
	}
	if(mkdir(config->folderName, 0755) != 0)
	{
		if(errno != EEXIST)
			return 0;
	}
#endif

	return 1;
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

void CreateBaseName(parameters *config)
{
	if(!config)
		return;
	
	sprintf(config->baseName, "_f%d_%s_%s_v_%g_OF%d_%s_%s_%s",
			config->MaxFreq,
			GetWindow(config->window),
			GetChannel(config->channel),
			fabs(config->significantAmplitude),
			config->outputFilterFunction,
			GetNormalization(config->normType),
			config->ZeroPad ? "ZP" : "NP",
			config->channelBalance ? "B" : "NB");
}

void ComposeFileName(char *target, char *subname, char *ext, parameters *config)
{
	if(!config)
		return;

	sprintf(target, "%s\\%s%s%s",
		config->folderName, subname, config->baseName, ext); 
}

void ComposeFileNameoPath(char *target, char *subname, char *ext, parameters *config)
{
	if(!config)
		return;

	sprintf(target, "%s%s%s", subname, config->baseName, ext); 
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