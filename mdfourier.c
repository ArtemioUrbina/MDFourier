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


#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>

#include <string.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>
#include <libgen.h>

#define MDVERSION "0.9"

#define SAMPLE_RATE 44100 //sice we are in stereo!

#define PSG_COUNT	40
#define NOISE_COUNT 100

#define COUNT		400 	// Number of frequencies to account for (MAX) 
#define MAX_NOTES	140		// we have 140 notes in the 240p Test Suite

#define TYPE_NONE	0
#define TYPE_FM 	1
#define TYPE_PSG	2
#define TYPE_NOISE	3

// This is the percentual difference allowed between reference and compared
// to be a match
#define PERCENT_TOLERANCE	0.3

// Width of each peak
#define HERTZ_WIDTH			0.0

// +/- Tolerance in frequency Difference to be the same one
#define HERTZ_DIFF			0.0

//Percentage of normalized weighted frequencies to match
#define FREQ_COMPARE		90.0

#define START_HZ	10
#define END_HZ		20000

typedef struct FrequencySt {
	double weight;
	double hertz;
	long   index;
	int    matched;
} Frequency;

typedef struct MaxFreqSt {
	Frequency	freq[COUNT];
	int 		index;
	int 		type;
} MaxFreq;

typedef struct GenesisAudioSt {
	MaxFreq 	Notes[MAX_NOTES]; 
}  GenesisAudio;

// WAV data structures
typedef struct	WAV_HEADER
{
	/* RIFF Chunk Descriptor */
	uint8_t 		RIFF[4];		// RIFF Header Magic header
	uint32_t		ChunkSize;		// RIFF Chunk Size
	uint8_t 		WAVE[4];		// WAVE Header
	/* "fmt" sub-chunk */
	uint8_t 		fmt[4]; 		// FMT header
	uint32_t		Subchunk1Size;	// Size of the fmt chunk
	uint16_t		AudioFormat;	// Audio format 1=PCM,6=mulaw,7=alaw,	  257=IBM Mu-Law, 258=IBM A-Law, 259=ADPCM
	uint16_t		NumOfChan;		// Number of channels 1=Mono 2=Sterio
	uint32_t		SamplesPerSec;	// Sampling Frequency in Hz
	uint32_t		bytesPerSec;	// bytes per second
	uint16_t		blockAlign; 	// 2=16-bit mono, 4=16-bit stereo
	uint16_t		bitsPerSample;	// Number of bits per sample
	/* "data" sub-chunk */
	uint8_t 		Subchunk2ID[4]; // "data"  string
	uint32_t		Subchunk2Size;	// Sampled data length
} wav_hdr;

typedef struct parameters_st {
	char 		referenceFile[1024];
	char 		targetFile[1024];
	double		sigMatch;
	double		tolerance;
	double		HzWidth;
	double		HzDiff;
	int 		startHz, endHz;
	int 		showAll;
	int 		extendedResults;
	int 		justResults;
	int 		verbose;
	int 		hanning;
	char		channel;
	int 		MaxFreq;
	int 		clock;
	int 		clockNote;
	int 		debugVerbose;
} parameters;

int do_log = 0;
char log_file[2048];

int LoadFile(FILE *file, GenesisAudio *Signal, parameters *config, char *fileName);
double ProcessSamples(MaxFreq *MaxFreqArray, short *samples, size_t size, long samplerate, float *hanning, parameters *config);
void CompareNotes(GenesisAudio *ReferenceSignal, GenesisAudio *TestSignal, parameters *config);
void PrintComparedNotes(MaxFreq *ReferenceArray, MaxFreq *ComparedArray, parameters *config);
int commandline(int argc , char *argv[], parameters *config);
static double TimeSpecToSeconds(struct timespec* ts);
float *hanning(int N, short itype);
void logmsg(char *fmt, ... );
char *GetRange(int index);
int GetSubIndex(int index);
void PrintUsage();

void Header()
{
	logmsg("== MDFourier " MDVERSION " ==\nSega Genesis/Mega Drive Fourier Audio compare tool for 240p Test Suite\n");
	logmsg("by Artemio Urbina 2019, licensed under GPL\n\n");
}

int main(int argc , char *argv[])
{
	FILE				*reference = NULL;
	FILE				*compare = NULL;
	GenesisAudio  		*ReferenceSignal;
	GenesisAudio  		*TestSignal;
	parameters			config;

	if(!commandline(argc, argv, &config))
	{
		Header();
		PrintUsage();
		return 1;
	}
	
	Header();
	reference = fopen(config.referenceFile, "rb");
	if(!reference)
	{
		logmsg("\tCould not open REFERENCE file: \"%s\"\n", config.referenceFile);
		return 0;
	}

	compare = fopen(config.targetFile, "rb");
	if(!compare)
	{
		fclose(reference);
		logmsg("\tCould not open COMPARE file: \"%s\"\n", config.targetFile);
		return 0;
	}

	
	logmsg("\nLoading Reference audio file %s\n", config.referenceFile);
	
	ReferenceSignal = (GenesisAudio*)malloc(sizeof(GenesisAudio));
	if(!ReferenceSignal)
	{
		logmsg("Not enough memory for Data Structures\n");
		return 0;
	}

	TestSignal = (GenesisAudio*)malloc(sizeof(GenesisAudio));
	if(!TestSignal)
	{
		free(ReferenceSignal);
		logmsg("Not enough memory for Data Structures\n");
		return 0;
	}

	if(!LoadFile(reference, ReferenceSignal, &config, config.referenceFile))
	{
		free(ReferenceSignal);
		free(TestSignal);
		return 0;
	}

	logmsg("Loading Compare audio file %s\n", config.targetFile);
	if(!LoadFile(compare, TestSignal, &config, config.targetFile))
	{
		free(ReferenceSignal);
		free(TestSignal);
		return 0;
	}

	CompareNotes(ReferenceSignal, TestSignal, &config);

	free(ReferenceSignal);
	free(TestSignal);
	ReferenceSignal = NULL;
	TestSignal = NULL;
	
	return(0);
}

int LoadFile(FILE *file, GenesisAudio *Signal, parameters *config, char *fileName)
{
	int 	i = 0;
	int 	loadedNoteSize = 0;
	char	*buffer;
	size_t	buffersize = 0;
	wav_hdr header;
	float	*windowUsed = NULL; 
	float	*window2Second = NULL; 
	float	*window1Second = NULL; 
	struct	timespec start, end;

	if(!file)
		return 0;

	if(fread(&header, 1, sizeof(wav_hdr), file) != sizeof(wav_hdr))
	{
		logmsg("\tInvalid WAV file: File too small\n");
		return(0);
	}

	if(header.AudioFormat != 1) // Check for PCM
	{
		logmsg("\tInvalid WAV File: Only PCM is supported\n\tPlease use WAV PCM 16 bit");
		return(0);
	}

	if(header.NumOfChan != 2) // Check for Stereo
	{
		logmsg("\tInvalid WAV file: Only Stereo supported\n");
		return(0);
	}

	if(header.bitsPerSample != 16) // Check bit depth
	{
		logmsg("\tInvalid WAV file: Only 16 bit supported for now\n\tPlease use WAV PCM 16 bit %dhz");
		return(0);
	}
	
	logmsg("WAV file is PCM %dhz %dbits\n", header.SamplesPerSec, header.bitsPerSample);
	buffersize = header.SamplesPerSec*8*sizeof(char); // 2 bytes per sample, stereo
	buffer = (char*)malloc(buffersize);
	if(!buffer)
	{
		logmsg("\tmalloc failed\n");
		return(0);
	}

	window2Second = hanning(header.SamplesPerSec*2, 0);
	if(!window2Second)
	{
		logmsg ("hanning creation failed\n");
		return(0);
	}

	window1Second = hanning(header.SamplesPerSec, 0);
	if(!window1Second)
	{
		logmsg ("hanning creation failed\n");
		return(0);
	}

	memset(Signal, 0, sizeof(GenesisAudio));

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	while(i < MAX_NOTES)
	{
		if(!config->hanning)
			windowUsed = NULL;

		if(i < PSG_COUNT)
		{
			loadedNoteSize = buffersize;   //2 second blocks
			if(config->hanning)
				windowUsed = window2Second;
		}
		else
		{
			loadedNoteSize = buffersize/2; // 1 second block
			if(config->hanning)
				windowUsed = window1Second;
		}
		memset(buffer, 0, buffersize);
		if(fread(buffer, 1, loadedNoteSize, file) > 0)
		{
			if(config->verbose)
				logmsg("==================== %s# %d (%d)===================\n", GetRange(i), GetSubIndex(i), i);
		}
		else
		{
			logmsg("\tunexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
			break;
		}
		Signal->Notes[i].index = i < PSG_COUNT ? i : i - PSG_COUNT;
		Signal->Notes[i].type = i < PSG_COUNT ? TYPE_FM : TYPE_PSG;

		ProcessSamples(&Signal->Notes[i], (short*)buffer, loadedNoteSize/2, header.SamplesPerSec, windowUsed, config);
		i++;
	}

	if(config->clock)
	{
		double			elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg("Processing WAV took %f\n", elapsedSeconds);
	}

	fclose(file);
	free(buffer);

	free(window2Second);
	free(window1Second);
	return i;
}

void logmsg(char *fmt, ... )
{
	va_list arguments; 

	va_start(arguments, fmt);
	vprintf(fmt, arguments);

	if(do_log)
	{
		FILE *logfile = fopen(log_file, "a");
		if(!logfile)
			return;
		vfprintf(logfile, fmt, arguments);
		fclose(logfile);
	}
	va_end(arguments);
	return;
}

static double TimeSpecToSeconds(struct timespec* ts)
{
	return (double)ts->tv_sec + (double)ts->tv_nsec / 1000000000.0;
}

double ProcessSamples(MaxFreq *MaxFreqArray, short *samples, size_t size, long samplerate, float *window, parameters *config)
{
	fftw_plan		p = NULL;
	long		  	stereoSignalSize = 0;	
	long		  	i = 0, monoSignalSize = 0; 
	double		  	*in = NULL, root = 0;
	fftw_complex  	*out = NULL;
	double		 	boxsize = 1, seconds = 0;
	struct			timespec start, end;
	long			removed = 0;
	double			TotalWeight = 0;
	
	if(!MaxFreqArray)
	{
		logmsg("No Array for results\n");
		return 0;
	}

	stereoSignalSize = (long)size;
	monoSignalSize = stereoSignalSize/2;	 // 4 is 2 16 bit values
	seconds = size/(samplerate*2);

	in = (double*)malloc(sizeof(double)*(monoSignalSize+1));
	if(!in)
	{
		logmsg("Not enough memory\n");
		return(0);
	}
	out = (fftw_complex*)malloc(sizeof(fftw_complex)*(monoSignalSize/2+1));
	if(!out)
	{
		logmsg("Not enough memory\n");
		return(0);
	}

	if(config->verbose)
		logmsg("Samples are at %lu Khz and %g seconds long.\n",
			samplerate, seconds);

	if(config->clockNote)
		clock_gettime(CLOCK_MONOTONIC, &start);

	p = fftw_plan_dft_r2c_1d(monoSignalSize, in, out, FFTW_MEASURE);

	memset(in, 0, sizeof(double)*(monoSignalSize+1));
	memset(out, 0, sizeof(fftw_complex)*(monoSignalSize/2+1));

	for(i = 0; i < monoSignalSize; i++)
	{
		if(config->channel == 'l')
			in[i] = (double)samples[i*2];
		if(config->channel == 'r')
			in[i] = (double)samples[i*2+1];
		if(config->channel == 's')
			in[i] = ((double)samples[i*2]+(double)samples[i*2+1])/2.0;

		if(config->hanning)
			in[i] *= window[i];
	}
	

	fftw_execute(p); 
	fftw_destroy_plan(p);

	root = sqrt(monoSignalSize);

	for(i = 0; i < config->MaxFreq; i++)
	{
		MaxFreqArray->freq[i].hertz = 0;
		MaxFreqArray->freq[i].weight = 0;
		MaxFreqArray->freq[i].index = -1;
		MaxFreqArray->freq[i].matched = 0;
	}

	//for(i = 0; i < monoSignalSize/2+1; i++)
	// analyze only 0khz to 20khz
	// i/boxwsize = 0 to i/boxsize = 20000
	// 0*boxsize to 20000*boxsize
	for(i = config->startHz*boxsize; i < config->endHz*boxsize; i++)  // Nyquist at 44.1khz
	{
		double r1 = creal(out[i]);
		double i1 = cimag(out[i]);
		double val, previous;
		int    j = 0;

		val = sqrt(sqrt(r1 * r1 + i1 * i1) / root);

		previous = 10000;
		for(j = 0; j < config->MaxFreq; j++)
		{
			if(val > MaxFreqArray->freq[j].weight && val < previous)
			{
				int k = 0;
				//Move the previous values down the array
				for(k = config->MaxFreq-1; k > j; k--)
					MaxFreqArray->freq[k] = MaxFreqArray->freq[k - 1];

				MaxFreqArray->freq[j].weight = val;
				MaxFreqArray->freq[j].index = i;
			}

			previous = MaxFreqArray->freq[j].weight;
		}
	}
	
	// Calculate Hz
	// Estimate TotalWeight for Normalization
	for(i = 0; i < config->MaxFreq; i++)
	{
		if(MaxFreqArray->freq[i].index != -1)
		{
			MaxFreqArray->freq[i].hertz = (double)(MaxFreqArray->freq[i].index)/(boxsize*seconds);
			TotalWeight += MaxFreqArray->freq[i].weight;
		}
	}

	// Normalize to 100
	for(i = 0; i < config->MaxFreq; i++)
		MaxFreqArray->freq[i].weight = MaxFreqArray->freq[i].weight * 100.0 / TotalWeight;

	// The following mess compressed adjacent frequencies
	if(config->HzWidth > 0.0)
	{
		for(i = 0; i < config->MaxFreq; i++)
		{
			for(int j = 0; j < config->MaxFreq; j++)
			{
				double hertzDiff;
	
				hertzDiff = fabs(MaxFreqArray->freq[j].hertz - MaxFreqArray->freq[i].hertz);
				
				if(MaxFreqArray->freq[i].hertz && MaxFreqArray->freq[j].hertz
					&& i != j && hertzDiff <= config->HzWidth)
				{
					if(MaxFreqArray->freq[i].weight > MaxFreqArray->freq[j].weight)
					{
						MaxFreqArray->freq[i].weight += MaxFreqArray->freq[j].weight;
	
						MaxFreqArray->freq[j].hertz = 0;
						MaxFreqArray->freq[j].weight = 0;
						MaxFreqArray->freq[j].index = 0;
					}
					else
					{
						MaxFreqArray->freq[j].weight += MaxFreqArray->freq[i].weight;
	
						MaxFreqArray->freq[i].hertz = 0;
						MaxFreqArray->freq[i].weight = 0;
						MaxFreqArray->freq[i].index = 0;
					}
					/*
					if(i > j)
					{
						Frequency	t;
					
						t = MaxFreqArray->freq[j];
						MaxFreqArray->freq[j] = MaxFreqArray->freq[i];
						MaxFreqArray->freq[i] = t;
					}
					*/
					removed ++;
				}
			}
		}

		// sort array by weight
		for(i = 0; i < config->MaxFreq; i++)
		{
			for(int j = i + 1; j < config->MaxFreq; j++)
			{
				if (MaxFreqArray->freq[i].weight < MaxFreqArray->freq[j].weight) 
				{
						Frequency	t;
					
						t = MaxFreqArray->freq[i];
						MaxFreqArray->freq[i] = MaxFreqArray->freq[j];
						MaxFreqArray->freq[j] = t;
					}
			}
		}
	}

	if(config->verbose)
	{
		double			Percentage = 0;
	
		for(int j = 0; j < config->MaxFreq; j++)
		{
			if(MaxFreqArray->freq[j].weight && MaxFreqArray->freq[j].hertz)
			{
				Percentage += MaxFreqArray->freq[j].weight;
				//detect CRT frequency
				if(MaxFreqArray->freq[j].hertz > 15670 && MaxFreqArray->freq[j].hertz < 15700)
					logmsg("Frequency [%2d] with %5.2f%% (%5.2f%%): %g Hz *** CRT Noise ***\n", j, MaxFreqArray->freq[j].weight, Percentage, MaxFreqArray->freq[j].hertz);
				else
					logmsg("Frequency [%2d] with %5.2f%% (%5.2f%%): %g Hz\n", j, MaxFreqArray->freq[j].weight, Percentage, MaxFreqArray->freq[j].hertz);
			}
			if(config->debugVerbose && j == 20)
				exit(1);
		}
	}
	
	if(config->clockNote)
	{
		double			elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg("Processing Note took %f\n", elapsedSeconds);
	}

	free(in);
	free(out);

	return(0);
}


void CompareNotes(GenesisAudio *ReferenceSignal, GenesisAudio *TestSignal, parameters *config)
{
	int 	note, msg = 0, totalDiff = 0, HadCRTNoise = 0;
	int 	FMweights = 0, FMnotfound = 0;
	int 	FMadjWeight = 0, FMadjHz = 0;
	int 	FMcompared = 0, PSGcompared = 0;
	double	FMhighDiffAdj = 0, FMhighDiff = 0, FMhighHz = 0;
	int 	PSGweights = 0, PSGnotfound = 0;
	int 	PSGadjWeight = 0, PSGadjHz = 0;
	double	PSGhighDiffAdj = 0, PSGhighDiff = 0, PSGhighHz = 0;
	char	*diff = NULL;
	int 	msgSize = 0, msgPos = 0;
	char	buffer[1024];

	diff = malloc(sizeof(char)*4096);  // I know this sounds strange, but we are protecting
	if(!diff)							 // for when many differences do appear
	{
		logmsg("Insufficient memory\n");
		return;
	}	
	msgSize = 4096;
	logmsg("-- Results --\n\n");
	for(note = 0; note < MAX_NOTES; note++)
	{
		double percent = 0;
		int count = 0;

		msg = 0;
		sprintf(diff, "Note: %s# %d (%d)\n", GetRange(note), GetSubIndex(note), note);
		msgPos = strlen(diff);
		if(msgPos > msgSize - 512)
		{
			diff = (char*)realloc(diff, (msgSize+4096)*(sizeof(char)));
			msgSize += 4096;
		}

		// Determine up to which index we reach the percentage defined by config->sigMatch (90% default) 
		for(int freq = 0; freq < config->MaxFreq; freq ++)
		{
			percent += ReferenceSignal->Notes[note].freq[freq].weight;
			if(percent >= config->sigMatch)
			{
				count = freq;
				if(count < config->MaxFreq)
					count ++;
				break;
			}
		}

		for(int freq = 0; freq < count; freq++)
		{
			int CRTNoise;
				
			CRTNoise = (ReferenceSignal->Notes[note].freq[freq].hertz ==15697);
			if(CRTNoise)
				HadCRTNoise = 1;
			// Remove CRT noise
			if(ReferenceSignal->Notes[note].freq[freq].hertz && !CRTNoise)
			{
				int found = 0, index = 0, compSize = 0;

				if(ReferenceSignal->Notes[note].type == TYPE_FM)
					FMcompared ++;
				else
					PSGcompared ++;

				// Find Compared valid array size
				for(int comp = 0; comp < config->MaxFreq; comp++)
				{
					if(TestSignal->Notes[note].freq[comp].hertz)
						compSize ++;
					else
						break;
				}

				for(int comp = 0; comp < compSize; comp++)
				{
					if(!TestSignal->Notes[note].freq[comp].matched && 
						ReferenceSignal->Notes[note].freq[freq].hertz ==
						TestSignal->Notes[note].freq[comp].hertz)
					{
						TestSignal->Notes[note].freq[comp].matched = freq + 1;
						ReferenceSignal->Notes[note].freq[freq].matched = comp + 1;
						found = 1;
						index = comp;
						break;
					}
				}

				if(!found && config->HzDiff != 0.0) // search with tolerance, if done in one pass, false positives emerge
				{
					double	lowest = 20000, tmpDiff = 0;
					int 	lowIndex = -1;

					// Find closest match
					for(int comp = 0; comp < compSize; comp++)
					{
						if(!TestSignal->Notes[note].freq[comp].matched)
						{
							double hertzDiff;
		
							hertzDiff = fabs(TestSignal->Notes[note].freq[comp].hertz -
											 ReferenceSignal->Notes[note].freq[freq].hertz);

							if(hertzDiff <= config->HzDiff)
							{
								if(hertzDiff < lowest)
								{
									lowest = hertzDiff;
									lowIndex = comp;
									tmpDiff = hertzDiff;
								}
							}
						}
					}

					if(lowIndex >= 0)
					{
						TestSignal->Notes[note].freq[lowIndex].matched = freq + 1;
						ReferenceSignal->Notes[note].freq[freq].matched = lowIndex + 1;

						found = 2;
						index = lowIndex;

						if(ReferenceSignal->Notes[note].type == TYPE_FM)
						{
							FMadjHz++;
							
							if(tmpDiff > FMhighHz)
								FMhighHz = tmpDiff;
						}
						else
						{
							PSGadjHz++;
							
							if(tmpDiff > PSGhighHz)
								PSGhighHz = tmpDiff;
						}
					}
				}

				if(found)  // Now in either case, compare weights
				{
					double test;
	
					test = fabs(TestSignal->Notes[note].freq[index].weight - ReferenceSignal->Notes[note].freq[freq].weight);
					if(test > config->tolerance)
					{
						sprintf(buffer, "\tDifferent Weight found: %g Hz at %.4f%% instead of %g Hz at %.4f%% (%g)\n",
							TestSignal->Notes[note].freq[index].hertz,
							TestSignal->Notes[note].freq[index].weight,
							ReferenceSignal->Notes[note].freq[freq].hertz,
							ReferenceSignal->Notes[note].freq[freq].weight,
							test);	
						strcat(diff, buffer);
						msgPos = strlen(diff);
						if(msgPos > msgSize - 512)
						{
							diff = (char*)realloc(diff, (msgSize+4096)*(sizeof(char)));
							msgSize += 4096;
						}
						msg ++;
						totalDiff ++;

						if(ReferenceSignal->Notes[note].type == TYPE_FM)
						{
							FMweights ++;
	
							if(test > FMhighDiff)
								FMhighDiff = test;
						}
						else
						{
							PSGweights ++;
	
							if(test > PSGhighDiff)
								PSGhighDiff = test;
						}
					}
		
					if(test && test <= config->tolerance)
					{
						if(ReferenceSignal->Notes[note].type == TYPE_FM)
						{
							FMadjWeight++;
							if(test > FMhighDiffAdj)
								FMhighDiffAdj = test;
						}
						else
						{
							PSGadjWeight++;
							if(test > PSGhighDiffAdj)
								PSGhighDiffAdj = test;
						}
					}
				}

				if(!found)
				{
					sprintf(buffer, "\tReference Frequency not found: %g Hz at %.2f%% (index: %d)\n",
							ReferenceSignal->Notes[note].freq[freq].hertz,
							ReferenceSignal->Notes[note].freq[freq].weight,
							freq);
					strcat(diff, buffer);
					msgPos = strlen(diff);
					if(msgPos > msgSize - 512)
					{
						diff = (char*)realloc(diff, (msgSize+4096)*(sizeof(char)));
						msgSize += 4096;
					}
					msg ++;
					totalDiff ++;

					if(ReferenceSignal->Notes[note].type == TYPE_FM)
						FMnotfound ++;
					else
						PSGnotfound ++;
				}
			}
		}

		if(msg && !config->justResults)
		{
			logmsg("%s\n", diff);
			if(config->extendedResults)
			{
				logmsg("Unmatched Note Report for %s# %d (%d)\n", GetRange(note), GetSubIndex(note), note);
				PrintComparedNotes(&ReferenceSignal->Notes[note], &TestSignal->Notes[note], config);
			}
		}
		else
		{
			if(!config->justResults && config->showAll)
			{
				logmsg("Matched Note Report for %s# %d (%d)\n", GetRange(note), GetSubIndex(note), note);
				PrintComparedNotes(&ReferenceSignal->Notes[note], &TestSignal->Notes[note], config);
			}
		}
	}

	logmsg("============================================================\n");
	logmsg("Reference: %s\nCompared to: %s\n", config->referenceFile, config->targetFile);

	if(!totalDiff && FMcompared+PSGcompared)
	{
		if(config->tolerance == 0.0 && config->sigMatch == 100.0)
			logmsg("\n== WAV files are acoustically identical  == \n");
		else
			logmsg("\n== WAV files are equivalent under these parameters  == \n");
		logmsg("============================================================\n");
		if(FMadjHz+FMadjWeight)
		{
			logmsg("FM Sound\n");
			logmsg("\tAdjustments made to match within defined ranges: %d\n",
				FMadjHz+FMadjWeight);
			if(config->HzDiff != 0.0)
				logmsg("\t\tFrequencies adjusted: %d of %d (%0.2f%%) [highest difference: %0.2f Hz]\n",
					FMadjHz, FMcompared, (double)FMadjHz/(double)FMcompared*100, FMhighHz);
			logmsg("\t\tWeights adjusted: %d of %d (%0.2f%%) [highest difference: %0.2f%%]\n",
				FMadjWeight, FMcompared, (double)FMadjWeight/(double)FMcompared*100, FMhighDiffAdj);
		}
		if(PSGadjHz+PSGadjWeight)
		{
			logmsg("PSG Sound\n");
			logmsg("\tAdjustments made to match within defined ranges: %d\n",
				PSGadjHz+PSGadjWeight);
			if(config->HzDiff != 0.0)
				logmsg("\t\tFrequencies adjusted: %d of %d (%0.2f%%) [highest difference: %0.2f Hz]\n",
					PSGadjHz, PSGcompared, (double)PSGadjHz/(double)PSGcompared*100, PSGhighHz);
			logmsg("\t\tWeights adjusted: %d of %d (%0.2f%%) [highest difference: %0.2f%%]\n",
				PSGadjWeight, PSGcompared, (double)PSGadjWeight/(double)PSGcompared*100, PSGhighDiffAdj);
		}
	}
	else
	{
		if(FMcompared+PSGcompared)
		{
			logmsg("Total differences are %d out of %d [%g%% different]\n============================================================\n",
				 totalDiff, FMcompared+PSGcompared, (double)totalDiff*100.0/(double)(PSGcompared+FMcompared));
	
			if(FMnotfound+FMweights)
			{
				logmsg("\nFM differences %d\n",
					FMnotfound+FMweights);
				logmsg("\tNot found: %d of %d (%0.2f%%)\n", 
					FMnotfound, FMcompared, (double)FMnotfound/(double)FMcompared*100);
				logmsg("\tDifferent weights: %d of %d (%0.2f%%) [highest: %0.2f%%]\n",
					 FMweights, FMcompared, (double)FMweights/(double)FMcompared*100, FMhighDiff);
				logmsg("\n\tMatched frequencies analysis:\n");
				if(config->HzDiff != 0.0)
					logmsg("\tFrequencies adjusted: %d of %d (%0.2f%%) [highest difference: %0.2f Hz]\n",
						FMadjHz, FMcompared, (double)FMadjHz/(double)FMcompared*100, FMhighHz);
				logmsg("\tWeights adjusted: %d of %d (%0.2f%%) [highest difference: %0.2f%%]\n",
					FMadjWeight, FMcompared, (double)FMadjWeight/(double)FMcompared*100, FMhighDiffAdj);
			}
	
			if(PSGnotfound+PSGweights)
			{
				logmsg("\nPSG differences %d\n",
					PSGnotfound+PSGweights);
				logmsg("\tNot found: %d of %d (%0.2f%%)\n", 
					PSGnotfound, PSGcompared, (double)PSGnotfound/(double)PSGcompared*100);
				logmsg("\tDifferent weights: %d of %d (%0.2f%%) [highest: %0.2f%%]\n",
					PSGweights, PSGcompared, (double)PSGweights/(double)PSGcompared*100, PSGhighDiff);
				logmsg("\n\tMatched frequencies analysis:\n");
				if(config->HzDiff != 0.0)
					logmsg("\tFrequencies adjusted: %d of %d (%0.2f%%) [highest difference: %0.2f Hz]\n",
						PSGadjHz, PSGcompared, (double)PSGadjHz/(double)PSGcompared*100, PSGhighHz);
				logmsg("\tWeights adjusted: %d of %d (%0.2f%%) [highest difference: %0.2f%%]\n",
					PSGadjWeight, PSGcompared, (double)PSGadjWeight/(double)PSGcompared*100, PSGhighDiffAdj);
			}
		}
		else
			logmsg("Reference file has no frequencies at all!\n");
	}
	if(HadCRTNoise)
		logmsg("\nReference Signal has CRT noise (15697 hz)\n");

	free(diff);
}


void PrintComparedNotes(MaxFreq *ReferenceArray, MaxFreq *ComparedArray, parameters *config)
{
	double total = 0;

	for(int j = 0; j < config->MaxFreq; j++)
	{
		if(ReferenceArray->freq[j].weight && ReferenceArray->freq[j].hertz)
		{
			int match = 0;

			total += ReferenceArray->freq[j].weight;
			logmsg("[%4.2d] (%5.2f%%) Ref: %5g Hz\t%0.4f%% [>%3d]", 
						j, total,
						ReferenceArray->freq[j].hertz,
						ReferenceArray->freq[j].weight,
						ReferenceArray->freq[j].matched - 1);

			if(ComparedArray->freq[j].hertz)
				logmsg("\tComp:\t%5g Hz\t%0.4f%% [<%3d]", 
						ComparedArray->freq[j].hertz,
						ComparedArray->freq[j].weight,
						ComparedArray->freq[j].matched - 1);
			else
				logmsg("\tCompared:\tNULL");
			match = ReferenceArray->freq[j].matched - 1;
			if(match != -1 &&
				ReferenceArray->freq[j].hertz != 
				ComparedArray->freq[match].hertz)
					logmsg("H");
			else
					logmsg(" ");
			if(match != -1 &&
				ReferenceArray->freq[j].weight != 
				ComparedArray->freq[match].weight)
					logmsg("W");
			logmsg("\n");
			if(total >= config->sigMatch)
				break;
		}
	}
	logmsg("\n\n");
}

char *GetRange(int index)
{
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
	if(index < PSG_COUNT) // FM
		return(index + 1);
	if(index >= NOISE_COUNT) // NOISE
		return((index - NOISE_COUNT)% 20 + 1);
	return((index - PSG_COUNT) % 20 + 1);  // PSG
}

int commandline(int argc , char *argv[], parameters *config)
{
	int c, index, ref = 0, tar = 0;
	
	opterr = 0;
	
	config->sigMatch = FREQ_COMPARE;
	config->tolerance = PERCENT_TOLERANCE;
	config->startHz = START_HZ;
	config->endHz = END_HZ;
	config->extendedResults = 0;
	config->justResults = 0;
	config->verbose = 0;
	config->hanning = 0;
	config->channel = 's';
	config->MaxFreq = 120;
	config->clock = 0;
	config->clockNote = 0;
	config->HzWidth = HERTZ_WIDTH;
	config->HzDiff = HERTZ_DIFF;
	config->showAll = 0;
	config->debugVerbose = 0;

	while ((c = getopt (argc, argv, "hlkxnwejyvd:a:p:t:r:c:f:b:s:z:")) != -1)
	switch (c)
	  {
	  case 'h':
		return 0;
		break;
	  case 'e':
		config->extendedResults = 1;
		break;
	  case 'j':
		config->justResults = 1;
		break;
	  case 'v':
		config->verbose = 1;
		break;
	  case 'w':
		config->hanning = 1;
		break;
	  case 'k':
		config->clock = 1;
		break;
	  case 'n':
		config->clockNote = 1;
		break;
	  case 'x':
		config->showAll = 1;
		break;
	  case 'l':
		do_log = 1;
		break;
	  case 'y':
		config->debugVerbose = 1;
		config->verbose = 1;
		break;
	  case 's':
		config->startHz = atoi(optarg);
		if(config->startHz < 0 || config->startHz > 19900)
			config->startHz = START_HZ;
		logmsg("\tFrequency start range for FFTW is now %d (default 10)\n", config->startHz);
		break;
	  case 'z':
		config->endHz = atoi(optarg);
		if(config->endHz < 10 || config->endHz > 20000)
			config->endHz = END_HZ;
		logmsg("\tFrequency end range for FFTW is now %d (default 20000)\n", config->endHz);
		break;
	  case 'f':
		config->MaxFreq = atoi(optarg);
		if(config->MaxFreq < 10 || config->MaxFreq > 400)
			config->MaxFreq = COUNT;
		logmsg("\tMax frequencies to use from FFTW are %d (default 120)\n", config->MaxFreq);
		break;
	  case 'p':
		config->sigMatch = atof(optarg);
		if(config->sigMatch < 0.0 || config->sigMatch > 100.0)
			config->sigMatch = FREQ_COMPARE;
		break;
	  case 'b':
		config->HzWidth = atof(optarg);
		if(config->HzWidth < 0.0 || config->HzWidth > 5000.0)
			config->HzWidth = HERTZ_WIDTH;
		logmsg("\tHertz Width Compression changed to %f (default 0.0)\n", config->HzWidth);
		break;
	  case 'd':
		config->HzDiff = atof(optarg);
		if(config->HzDiff < 0.0 || config->HzDiff > 5000.0)
			config->HzDiff = HERTZ_DIFF;
		logmsg("\tHertz Difference tolerance %f (default 0.0)\n", config->HzDiff);
		break;
	  case 't':
		config->tolerance = atof(optarg);
		if(config->tolerance < 0.0 || config->tolerance > 100.0)
			config->tolerance = PERCENT_TOLERANCE;
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
				logmsg("Invalid audio channel option %c\n", optarg[0]);
				logmsg("Use l for Left, r for Right or s for Stereo\n");
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
		else if (optopt == 'a')
		  logmsg("Audio channel option -%c requires an argument: l,r or s\n", optopt);
		else if (optopt == 'p')
		  logmsg("Percentage match to compare -%c requires an argument: 0.0-100.0\n", optopt);
		else if (optopt == 't')
		  logmsg("Weight tolerance percentage -%c requires an argument: 0.0-100.0\n", optopt);
		else if (optopt == 'f')
		  logmsg("Max frequencies to use from FFTW -%c requires an argument: 0-400\n", optopt);
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
		return 0;
		break;
	  }
	
	for (index = optind; index < argc; index++)
	{
		logmsg("Invalid argument %s\n", argv[index]);
		return 0;
	}

	if(!ref || !tar)
	{
		logmsg("Please define both -r reference and -c compare wav files\n");
		return 0;
	}

	if(config->endHz <= config->startHz)
	{
		logmsg("Invalid frequency range for FFTW (%d Hz to %d Hz)\n", config->startHz, config->endHz);
		return 0;
	}

	logmsg("\tSignal Percentage match to compare is %0.2f%%\n", config->sigMatch);
	logmsg("\tWeight tolerance percentage to compare is %0.2f%%\n", config->tolerance);
	logmsg("\tAudio Channel is: %c\n", config->channel);

	if(do_log)
	{
        int len;
        
		sprintf(log_file, "%s", basename(config->referenceFile));
        len = strlen(log_file);
        sprintf(log_file+len-4, "_vs_%s", basename(config->targetFile));
        len = strlen(log_file);
        sprintf(log_file+len-4, ".txt");
		remove(log_file);
		printf("\tLog enabled to file: %s\n", log_file);
	}

	return 1;
}

void PrintUsage()
{
	logmsg("\n\n\tusage: mdfourier -r reference.wav -c compare.wav\n");
	logmsg("\t\t-v: verbose mode enabled, spits the FFTW results\n");
	logmsg("\t\t-e: extended results enabled, shows a table with all matched\n\t\t\tfrequencies for each note with differences\n");
	logmsg("\t\t-w: enable Hanning windowing. I've found this introduces\n\t\t\tmore differences\n");
	logmsg("\t\t-c <l,r,s>: select audio channel to compare. Default is both channels\n\t\t\t's' stereo, 'l' for left or 'r' for right\n");
	logmsg("\t\t-l: log output to file [compare].txt\n");
	logmsg("\t\t-k: clock FFTW operations\n");
	logmsg("\t\t-n: clock FFTW operations for each Note\n");
}

/*
%	HANNING   Hanning window.
%	HANNING(N) returns the N-point symmetric Hanning window in a column
%	vector.  Note that the first and last zero-weighted window samples
%	are not included.
%
%	HANNING(N,'symmetric') returns the same result as HANNING(N).
%
%	HANNING(N,'periodic') returns the N-point periodic Hanning window,
%	and includes the first zero-weighted window sample.
%
%	NOTE: Use the HANN function to get a Hanning window which has the
%		   first and last zero-weighted samples.ep
	itype = 1 --> periodic
	itype = 0 --> symmetric
	default itype=0 (symmetric)
 
	Copyright 1988-2004 The MathWorks, Inc.
%	$Revision: 1.11.4.3 $  $Date: 2007/12/14 15:05:04 $
*/

float *hanning(int N, short itype)
{
	int half, i, idx, n;
	float *w;
 
	w = (float*) calloc(N, sizeof(float));
	memset(w, 0, N*sizeof(float));
 
	if(itype==1)	//periodic function
		n = N-1;
	else
		n = N;
 
	if(n%2==0)
	{
		half = n/2;
		for(i=0; i<half; i++) //CALC_HANNING   Calculates Hanning window samples.
			w[i] = 0.5 * (1 - cos(2*M_PI*(i+1) / (n+1)));
 
		idx = half-1;
		for(i=half; i<n; i++) {
			w[i] = w[idx];
			idx--;
		}
	}
	else
	{
		half = (n+1)/2;
		for(i=0; i<half; i++) //CALC_HANNING   Calculates Hanning window samples.
			w[i] = 0.5 * (1 - cos(2*M_PI*(i+1) / (n+1)));
 
		idx = half-2;
		for(i=half; i<n; i++) {
			w[i] = w[idx];
			idx--;
		}
	}
 
	if(itype==1)	//periodic function
	{
		for(i=N-1; i>=1; i--)
			w[i] = w[i-1];
		w[0] = 0.0;
	}
	return(w);
}
