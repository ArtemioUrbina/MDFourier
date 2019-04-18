/* 
 * MDFourier
 * A Fourier Transform analysis tool to compare different 
 * Sega Genesis/Mega Drive audio hardware revisions.
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
 * Compile with: 
 *	  gcc -Wall -std=gnu99 -o mdfourier mdfourier.c -lfftw3 -lm
 */

#include "cline.h"
#include "log.h"

void PrintUsage()
{
	// b,d and y options are not documented since they are mostly for testing or found not as usefull as desired
	logmsg("  usage: mdfourier -r reference.wav -c compare.wav\n\n");
	logmsg("   FFT and Analysis options:\n");
	logmsg("	 -w: enable <w>indowing. Default is a custom Tukey window.\n\tOptions are 'n' for none, 't' for Tukey, 'h' for Hann and 'f' for FlatTop\n");
	logmsg("	 -f: Change the number of frequencies to use from FFTW\n");
	logmsg("	 -t: Defines the <t>olerance when comparing amplitudes in dbs\n");
	logmsg("	 -c <l,r,s>: select audio <c>hannel to compare. Default is both channels\n\t's' stereo, 'l' for left or 'r' for right\n");
	logmsg("	 -s: Defines <s>tart of the frequency range to compare with FFT\n\tA smaller range will compare more frequencies unless changed\n");
	logmsg("	 -z: Defines end of the frequency range to compare with FFT\n\tA smaller range will compare more frequencies unless changed\n");
	logmsg("   Output options:\n");
	logmsg("	 -l: <l>og output to file [reference]_vs_[compare].txt\n");
	logmsg("	 -v: Enable <v>erbose mode, spits all the FFTW results\n");
	logmsg("	 -j: Cuts all the per note information and shows <j>ust the total results\n");
	logmsg("	 -e: Enables <e>xtended results. Shows a table with all matched\n\tfrequencies for each note with differences\n");
	logmsg("	 -m: Enables Show all notes compared with <m>atched frequencies\n");
	logmsg("	 -x: Enables totaled output for use with grep and spreadsheet\n");
	logmsg("	 -k: cloc<k> FFTW operations\n");
	logmsg("	 -g: clock FFTW operations for each <n>ote\n");
}

void Header(int log)
{
	char title1[] = "== MDFourier " MDVERSION " ==\nSega Genesis/Mega Drive Fourier Audio compare tool for 240p Test Suite\n";
	char title2[] = "by Artemio Urbina 2019, licensed under GPL\n\n";

	if(log)
		logmsg("%s%s", title1, title2);
	else
		printf("%s%s", title1, title2);
}

void CleanParameters(parameters *config)
{
	config->tolerance = DBS_TOLERANCE;
	config->startHz = START_HZ;
	config->endHz = END_HZ;
	config->extendedResults = 0;
	config->justResults = 0;
	config->verbose = 0;
	config->window = 't';
	config->channel = 's';
	config->MaxFreq = FREQ_COUNT;
	config->clock = 0;
	config->clockNote = 0;
	config->HzWidth = HERTZ_WIDTH;
	config->HzDiff = HERTZ_DIFF;
	config->showAll = 0;
	config->debugVerbose = 0;
	config->spreadsheet = 0;
	config->normalize = 'g';
	config->relativeMaxMagnitude = 0;
	config->ignoreFloor = 1;
	config->useOutputFilter = 1;
	config->outputFilterFunction = 2;
}

int commandline(int argc , char *argv[], parameters *config)
{
	int c, index, ref = 0, tar = 0;
	
	opterr = 0;
	
	CleanParameters(config);

	while ((c = getopt (argc, argv, "hejvkgimlxyo:w:n:d:a:t:r:c:f:b:s:z:")) != -1)
	switch (c)
	  {
	  case 'h':
		PrintUsage();
		return 0;
		break;
	  case 'e':
		config->extendedResults = 1;
		break;
	  case 'j':
		config->justResults = 1;
		break;
	  case 'm':
		config->showAll = 1;
		break;
	  case 'v':
		config->verbose = 1;
		break;
	  case 'i':
		config->ignoreFloor = 0;
		break;
	  case 'k':
		config->clock = 1;
		break;
	  case 'g':
		config->clockNote = 1;
		break;
	  case 'l':
		EnableConsole();
		break;
	  case 'x':
		config->spreadsheet = 1;
		break;
	  case 'y':
		config->debugVerbose = 1;
		config->verbose = 1;
		break;
	  case 'o':
		config->outputFilterFunction = atoi(optarg);
		if(config->outputFilterFunction < 0 || config->outputFilterFunction > 6)
			config->outputFilterFunction = 2;
		if(!config->outputFilterFunction)
			config->useOutputFilter = 0;
		break;
	  case 's':
		config->startHz = atoi(optarg);
		if(config->startHz < 1 || config->startHz > 19900)
			config->startHz = START_HZ;
		break;
	  case 'z':
		config->endHz = atoi(optarg);
		if(config->endHz < 10 || config->endHz > 20000)
			config->endHz = END_HZ;
		break;
	  case 'f':
		config->MaxFreq = atoi(optarg);
		if(config->MaxFreq < 1 || config->MaxFreq > MAX_FREQ_COUNT)
			config->MaxFreq = MAX_FREQ_COUNT;
		break;
	  case 'b':
		config->HzWidth = atof(optarg);
		if(config->HzWidth < 0.0 || config->HzWidth > 5000.0)
			config->HzWidth = HERTZ_WIDTH;
		break;
	  case 'd':
		config->HzDiff = atof(optarg);
		if(config->HzDiff < 0.0 || config->HzDiff > 5000.0)
			config->HzDiff = HERTZ_DIFF;
		break;
	  case 't':
		config->tolerance = atof(optarg);
		if(config->tolerance < 0.0 || config->tolerance > 40.0)
			config->tolerance = DBS_TOLERANCE;
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
	 case 'w':
		switch(optarg[0])
		{
			case 'n':
			case 'f':
			case 'h':
			case 't':
				config->window = optarg[0];
				break;
			default:
				logmsg("Invalid Window for FFT option '%c'\n", optarg[0]);
				logmsg("\tUse n for None, t for Tukey window (default), f for Flattop or h for Hann window\n");
				return 0;
				break;
		}
		break;
	  case 'n':
		switch(optarg[0])
		{
			case 'n':
			case 'g':
			case 'r':
				config->normalize = optarg[0];
				break;
			default:
				logmsg("Invalid Normalization option '%c'\n", optarg[0]);
				logmsg("\tUse n for per note normalization, default is g for global\n");
				return 0;
				break;
		}
		break;
	  case 'r':
		sprintf(config->referenceFile, "%s", optarg);
		ref = 1;
		break;
	  case 'c':
		sprintf(config->targetFile, "%s", optarg);
		tar = 1;
		break;
	  case '?':
		if (optopt == 'r')
		  logmsg("Reference File -%c requires an argument.\n", optopt);
		else if (optopt == 'c')
		  logmsg("Compare File -%c requires an argument.\n", optopt);
		else if (optopt == 'n')
		  logmsg("Normalization option -%c requires an argument: g,n or r\n", optopt);
		else if (optopt == 'a')
		  logmsg("Audio channel option -%c requires an argument: l,r or s\n", optopt);
		else if (optopt == 'w')
		  logmsg("FFT Window option -%c requires an argument: n,t,f or h\n", optopt);
		else if (optopt == 'o')
		  logmsg("Output curve -%c requires an argument 0-4\n", optopt);
		else if (optopt == 't')
		  logmsg("Amplitude tolerance -%c requires an argument: 0.0-40.0 dbs\n", optopt);
		else if (optopt == 'f')
		  logmsg("Max # of frequencies to use from FFTW -%c requires an argument: 1-%d\n", optopt, MAX_FREQ_COUNT);
		else if (optopt == 's')
		  logmsg("Min frequency range for FFTW -%c requires an argument: 0-19900\n", optopt);
		else if (optopt == 'z')
		  logmsg("Max frequency range for FFTW -%c requires an argument: 10-20000\n", optopt);
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
	
	for (index = optind; index < argc; index++)
	{
		logmsg("Invalid argument %s\n", argv[index]);
		return 0;
	}

	if(!ref || !tar)
	{
		logmsg("Please define both reference and compare audio files\n");
		return 0;
	}

	if(config->extendedResults && config->justResults)
	{
		logmsg("Just Results cancels Extended results\n");
		return 0;
	}

	if(config->showAll && config->justResults)
	{
		logmsg("Just Results cancels Show All\n");
		return 0;
	}

	if(config->endHz <= config->startHz)
	{
		logmsg("Invalid frequency range for FFTW (%d Hz to %d Hz)\n", config->startHz, config->endHz);
		return 0;
	}

	if(IsLogEnabled())
	{
		int len;
		char tmp[LOG_NAME_LEN];
		
		sprintf(tmp, "%s", basename(config->referenceFile));
		len = strlen(tmp);
		sprintf(tmp+len-4, "_vs_%s", basename(config->targetFile));
		len = strlen(tmp);
		tmp[len-4] = '\0';

		if(!setLogName(tmp))
			return 0;

		DisableConsole();
		Header(1);
		EnableConsole();
	}

	if(config->normalize == 'r')
		config->relativeMaxMagnitude = 0.0;

	logmsg("\tAmplitude tolerance while comparing is +/-%0.2f dbs\n", config->tolerance);
	logmsg("\tAudio Channel is: %s\n", GetChannel(config->channel));
	logmsg("\tMax frequencies to use from FFTW are %d (default %d)\n", config->MaxFreq, FREQ_COUNT);
	if(config->startHz != START_HZ)
		logmsg("\tFrequency start range for FFTW is now %d (default %d)\n", config->startHz, START_HZ);
	if(config->endHz != END_HZ)
		logmsg("\tFrequency end range for FFTW is now %d (default %d)\n", config->endHz, END_HZ);
	if(config->HzWidth != HERTZ_WIDTH)
		logmsg("\tHertz Width Compression changed to %g (default %g Hz)\n", config->HzWidth, HERTZ_WIDTH);
	if(config->HzDiff != HERTZ_DIFF)
		logmsg("\tHertz Difference tolerance +/-%g (default %d Hz)\n", config->HzDiff, HERTZ_DIFF);
	if(config->window != 'n')
		logmsg("\tA %s window will be applied to each note to be compared\n", GetWindow(config->window));
	else
		logmsg("\tNo window (rectangle) will be applied to each note to be compared\n");
	if(config->useOutputFilter)
	{
		if(config->outputFilterFunction >= 2)
			logmsg("\tA Beta function #%d filter will be applied to the results\n", 
				config->outputFilterFunction);
		else
			logmsg("\tA linear function filter will be applied to the results\n");
	}
	else
		logmsg("\tNo filtering will be applied to the results\n");
	return 1;
}

double TimeSpecToSeconds(struct timespec* ts)
{
	return (double)ts->tv_sec + (double)ts->tv_nsec / 1000000000.0;
}

char *GetRange(int index)
{
	if(index == MAX_NOTES)
		return("Floor");
	if(index < PSG_COUNT)
		return("FM");
	if(index >= NOISE_COUNT)
	{
		if(index - NOISE_COUNT > 20)
			return("Periodic Noise");
		else
			return("White Noise");
	}
	if(index - PSG_COUNT < 20)
		return("PSG 1");
	if(index - PSG_COUNT < 40)
		return("PSG 2");
	return("PSG 3");
}

int GetSubIndex(int index)
{
	if(index == MAX_NOTES)
		return(0);
	if(index < PSG_COUNT) // FM
		return(index + 1);
	if(index >= NOISE_COUNT) // NOISE
		return((index - NOISE_COUNT)% 20 + 1);
	return((index - PSG_COUNT) % 20 + 1);  // PSG
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
			return "No Window/Rectangular";
		case 't':
			return "Custom Tukey";
		case 'f':
			return "Flattop";
		case 'h':
			return "Hann";
		default:
			return "ERROR";
	}
}
