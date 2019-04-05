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

#define MDVERSION "0.95"

#define SAMPLE_RATE 44100 //sice we are in stereo!

#define PSG_COUNT	40
#define NOISE_COUNT 100

#define COUNT		2000 	// Number of frequencies to account for (MAX) 
#define FREQ_COUNT	800 	// Number of frequencies to account for (default)
#define MAX_NOTES	140		// we have 140 notes in the 240p Test Suite

#define TYPE_NONE	0
#define TYPE_FM 	1
#define TYPE_PSG	2
#define TYPE_NOISE	3

// This is the percentual difference allowed between reference and compared
// to be a match
#define PERCENT_TOLERANCE	3.0

// Width of each peak
#define HERTZ_WIDTH			0.0

// +/- Tolerance in frequency Difference to be the same one
#define HERTZ_DIFF			0.0

//Percentage of normalized weighted frequencies to match
#define FREQ_COMPARE		1.0

#define START_HZ	10
#define END_HZ		20000

typedef struct FrequencySt {
	double hertz;
	double weight;
	double amplitude;
	double phase;
	long   indexFFT;
	int    matched;
} Frequency;

typedef struct MaxFreqSt {
	Frequency	freq[COUNT];
	int 		index;
	int 		type;
} MaxFreq;

typedef struct GenesisAudioSt {
	MaxFreq 	Notes[MAX_NOTES+1]; 
	char		SourceFile[1024];
	int 		hasFloor;
	double		floorWeight;
	double		floorAmplitude;
}  GenesisAudio;

int IsCRTNoise(double freq)
{
	if(freq >= 15690 && freq <= 15710) //15697-15698 usualy
		return 1;
	return 0;
}

void CleanAudio(GenesisAudio *Signal)
{
	if(!Signal)
		return;
	for(int n = 0; n < MAX_NOTES+1; n++)
	{
		for(int i = 0; i < COUNT; i++)
		{
			Signal->Notes[n].freq[i].hertz = 0;
			Signal->Notes[n].freq[i].weight = 0;
			Signal->Notes[n].freq[i].amplitude = 0;
			Signal->Notes[n].freq[i].phase = 0;
			Signal->Notes[n].freq[i].indexFFT = -1;
			Signal->Notes[n].freq[i].matched = 0;
		}
	}
	Signal->SourceFile[0] = '\0';
	Signal->hasFloor = 0;
	Signal->floorWeight = 0.0;
	Signal->floorAmplitude = 0.0;
}

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
	double		tolerance;
	double		HzWidth;
	double		HzDiff;
	int 		startHz, endHz;
	int 		showAll;
	int 		extendedResults;
	int 		justResults;
	int 		verbose;
	char 		window;
	char		channel;
	int 		MaxFreq;
	int 		clock;
	int 		clockNote;
	int 		debugVerbose;
	int 		spreadsheet;
	int 		invertCompare;
	char 		normalize;
	double		relativeMaxWeight;
} parameters;

void CleanParameters(parameters *config)
{
	config->tolerance = PERCENT_TOLERANCE;
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
	config->invertCompare = 0;
	config->normalize = 'g';
	config->relativeMaxWeight = 0;
}

int do_log = 0;
char log_file[2048];

int LoadFile(FILE *file, GenesisAudio *Signal, parameters *config, char *fileName);
double ProcessSamples(MaxFreq *MaxFreqArray, short *samples, size_t size, long samplerate, float *window, parameters *config);
void CompareNotes(GenesisAudio *ReferenceSignal, GenesisAudio *TestSignal, parameters *config);
void CleanMatched(GenesisAudio *ReferenceSignal, GenesisAudio *TestSignal, parameters *config);
void PrintComparedNotes(MaxFreq *ReferenceArray, MaxFreq *ComparedArray, parameters *config, int hasFloor, double floorAmplitude);
void PrintFrequencies(GenesisAudio *Signal, parameters *config);
void GlobalNormalize(GenesisAudio *Signal, parameters *config);
int commandline(int argc , char *argv[], parameters *config);
static double TimeSpecToSeconds(struct timespec* ts);
void FindFloor(GenesisAudio *Signal, parameters *config);
float *hannWindow(int n);
float *flattopWindow(int n);
float *tukeyWindow(int n);
void logmsg(char *fmt, ... );
char *GetRange(int index);
int GetSubIndex(int index);
void Header(int log);
void PrintUsage();

int main(int argc , char *argv[])
{
	FILE				*reference = NULL;
	FILE				*compare = NULL;
	GenesisAudio  		*ReferenceSignal;
	GenesisAudio  		*TestSignal;
	parameters			config;

	Header(0);
	if(!commandline(argc, argv, &config))
	{
		printf("	 -h: Shows command line help\n");
		return 1;
	}

	if(strcmp(config.referenceFile, config.targetFile) == 0)
	{
		logmsg("Both inputs are the same file %s, skipping to save time\n",
			 config.referenceFile);
		return 0;
	}	

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

	if(config.normalize == 'r')
		config.relativeMaxWeight = 0.0;

	logmsg("\nLoading Reference audio file %s\n", config.referenceFile);
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
	if(config.invertCompare)
	{
		if(config.normalize == 'r')
			config.relativeMaxWeight = 0.0;
		CleanMatched(ReferenceSignal, TestSignal, &config);
		CompareNotes(TestSignal, ReferenceSignal, &config);
	}

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
	double	seconds = 0;

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
	
	if(header.SamplesPerSec/2 < config->endHz)
	{
		logmsg("%d Hz sample rate was too low for %d-%d Hz analysis\n",
			 header.SamplesPerSec, config->startHz, config->endHz);
		config->endHz = header.SamplesPerSec/2;
		logmsg("changed to %d-%d Hz\n", config->startHz, config->endHz);
	}

	seconds = (double)header.Subchunk2Size/4.0/(double)header.SamplesPerSec;
	if(seconds < 180)
		logmsg("File length is smaller than expected\n");

	logmsg("WAV file is PCM %dhz %dbits and %g seconds long\n", 
		header.SamplesPerSec, header.bitsPerSample, seconds);
	buffersize = header.SamplesPerSec*8*sizeof(char); // 2 bytes per sample, stereo
	buffer = (char*)malloc(buffersize);
	if(!buffer)
	{
		logmsg("\tmalloc failed\n");
		return(0);
	}

	if(config->window != 'n')
	{
		if(config->window == 't')
		{
			window2Second = tukeyWindow(header.SamplesPerSec*2);
			if(!window2Second)
			{
				logmsg ("Tukey window creation failed\n");
				return(0);
			}
		
			window1Second = tukeyWindow(header.SamplesPerSec);
			if(!window1Second)
			{
				logmsg ("Tukey window creation failed\n");
				return(0);
			}
		}

		if(config->window == 'f')
		{
			window2Second = flattopWindow(header.SamplesPerSec*2);
			if(!window2Second)
			{
				logmsg ("FlatTop window creation failed\n");
				return(0);
			}
		
			window1Second = flattopWindow(header.SamplesPerSec);
			if(!window1Second)
			{
				logmsg ("FlatTop window creation failed\n");
				return(0);
			}
		}

		if(config->window == 'h')
		{
			window2Second = hannWindow(header.SamplesPerSec*2);
			if(!window2Second)
			{
				logmsg ("Hann window creation failed\n");
				return(0);
			}
		
			window1Second = hannWindow(header.SamplesPerSec);
			if(!window1Second)
			{
				logmsg ("Hann window creation failed\n");
				return(0);
			}
		}
	}

	/*
	if(window1Second)
		for(int w= 0; w < header.SamplesPerSec; w++)
			logmsg("%d,%g\n", w, window1Second[w]);
	exit(1); 
	*/

	CleanAudio(Signal);

	if(seconds >= 181)
		Signal->hasFloor = 1;

	sprintf(Signal->SourceFile, "%s", fileName);
	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	while(i < MAX_NOTES+Signal->hasFloor)
	{
		if(config->window == 'n')
			windowUsed = NULL;

		if(i < PSG_COUNT)
		{
			loadedNoteSize = buffersize;   //2 second blocks
			if(config->window != 'n')
				windowUsed = window2Second;
		}
		else
		{
			loadedNoteSize = buffersize/2; // 1 second block
			if(config->window != 'n')
				windowUsed = window1Second;
		}
		memset(buffer, 0, buffersize);
		if(fread(buffer, 1, loadedNoteSize, file) != loadedNoteSize)
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

	// Global Normalization by default
	if(config->normalize != 'n')
		GlobalNormalize(Signal, config);

	if(Signal->hasFloor) // analyze noise floor if available
		FindFloor(Signal, config);

	fclose(file);
	free(buffer);

	if(window2Second)
		free(window2Second);
	if(window1Second)
		free(window1Second);

	if(config->verbose)
		PrintFrequencies(Signal, config);

	return i;
}

void FindFloor(GenesisAudio *Signal, parameters *config)
{
	if(!Signal)
		return;
	
	for(int i = 0; i < config->MaxFreq; i++)
	{
		if(!IsCRTNoise(Signal->Notes[MAX_NOTES].freq[i].hertz))
		{
			Signal->floorWeight = Signal->Notes[MAX_NOTES].freq[i].weight;
			Signal->floorAmplitude = Signal->Notes[MAX_NOTES].freq[i].amplitude;
			/*
            if(Signal->floorAmplitude <= -3.0)  // Don't search at the limit
				Signal->floorAmplitude += 3.0;
            */
			logmsg("Found Silent block, with top Noise at %0.4f.db (%0.4f%%) Using as Noisefloor\n",
				Signal->floorAmplitude, Signal->floorWeight);
			return;
		}
	}
	Signal->hasFloor = 0;  // revoke it if not found
}

void GlobalNormalize(GenesisAudio *Signal, parameters *config)
{
	double MaxWeight = 0;

	// Find global peak
	if(config->normalize == 'g' ||
	  (config->normalize == 'r' && config->relativeMaxWeight == 0.0))
	{
		for(int note = 0; note < MAX_NOTES+Signal->hasFloor; note++)
		{
			for(int i = 0; i < config->MaxFreq; i++)
			{
				if(Signal->Notes[note].freq[i].weight > MaxWeight)
					MaxWeight = Signal->Notes[note].freq[i].weight;
			}
		}

		if(config->normalize == 'r')
			config->relativeMaxWeight = MaxWeight;
	}

	if(config->normalize == 'r' && config->relativeMaxWeight != 0.0)
		MaxWeight = config->relativeMaxWeight;

	//Normalize and calculate Amplitude in dbs
	for(int note = 0; note < MAX_NOTES+Signal->hasFloor; note++)
	{
		for(int i = 0; i < config->MaxFreq; i++)
		{
			Signal->Notes[note].freq[i].amplitude = 
				20*log10(Signal->Notes[note].freq[i].weight / MaxWeight);
			Signal->Notes[note].freq[i].weight = 
				Signal->Notes[note].freq[i].weight * 100.0 / MaxWeight;
		}
	}
}

void CleanMatched(GenesisAudio *ReferenceSignal, GenesisAudio *TestSignal, parameters *config)
{
	for(int note = 0; note < MAX_NOTES+ReferenceSignal->hasFloor; note++)
	{
		for(int i = 0; i < config->MaxFreq; i++)
			ReferenceSignal->Notes[note].freq[i].matched = 0;
	}

	for(int note = 0; note < MAX_NOTES+TestSignal->hasFloor; note++)
	{
		for(int i = 0; i < config->MaxFreq; i++)
			TestSignal->Notes[note].freq[i].matched = 0;
	}
}

void PrintFrequencies(GenesisAudio *Signal, parameters *config)
{
	for(int note = 0; note < MAX_NOTES+Signal->hasFloor; note++)
	{
		logmsg("==================== %s# %d (%d) ===================\n", GetRange(note), GetSubIndex(note), note);
			if(config->spreadsheet)
				logmsg("Spreadsheet-%s#%d\n", GetRange(note), GetSubIndex(note));

		for(int j = 0; j < config->MaxFreq; j++)
		{
			if(Signal->Notes[note].freq[j].weight && Signal->Notes[note].freq[j].hertz)
			{
				logmsg("Frequency [%2d] with %5.2f%%: %g Hz [Amplitude: %g] [Phase: %g]",
					j, Signal->Notes[note].freq[j].weight, 
					Signal->Notes[note].freq[j].hertz,
					Signal->Notes[note].freq[j].amplitude,
					Signal->Notes[note].freq[j].phase);
				//detect CRT frequency
				if(IsCRTNoise(Signal->Notes[note].freq[j].hertz))
					logmsg(" *** CRT Noise ***");
				logmsg("\n");
			}
	
			if(config->spreadsheet)
			{
				logmsg("Spreadsheet-index-Hz-weight, %d, %g, %g\n",
					j, Signal->Notes[note].freq[j].hertz, Signal->Notes[note].freq[j].weight);
			}

			if(config->debugVerbose && j == 20)  // this is just for internal quick debugging
				exit(1);
		}
	}
}

void logmsg(char *fmt, ... )
{
	va_list arguments; 

	va_start(arguments, fmt);
	if(do_log != 2)
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
	double		  	*signal = NULL;
	fftw_complex  	*spectrum = NULL;
	double		 	boxsize = 1, seconds = 0;
	struct			timespec start, end;
	long			removed = 0;
	double			MaxWeight = 0;
	
	if(!MaxFreqArray)
	{
		logmsg("No Array for results\n");
		return 0;
	}

	stereoSignalSize = (long)size;
	monoSignalSize = stereoSignalSize/2;	 // 4 is 2 16 bit values
	seconds = size/(samplerate*2);

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

	if(config->clockNote)
		clock_gettime(CLOCK_MONOTONIC, &start);

	p = fftw_plan_dft_r2c_1d(monoSignalSize, signal, spectrum, FFTW_MEASURE);

	memset(signal, 0, sizeof(double)*(monoSignalSize+1));
	memset(spectrum, 0, sizeof(fftw_complex)*(monoSignalSize/2+1));

	for(i = 0; i < monoSignalSize; i++)
	{
		if(config->channel == 'l')
			signal[i] = (double)samples[i*2];
		if(config->channel == 'r')
			signal[i] = (double)samples[i*2+1];
		if(config->channel == 's')
			signal[i] = ((double)samples[i*2]+(double)samples[i*2+1])/2.0;

		if(config->window != 'n' && window)
			signal[i] *= window[i];
	}
	

	fftw_execute(p); 
	fftw_destroy_plan(p);

	//for(i = 0; i < monoSignalSize/2+1; i++)
	// analyze only 0khz to 20khz
	// i/boxwsize = 0 to i/boxsize = 20000
	// 0*boxsize to 20000*boxsize
	for(i = config->startHz*boxsize; i < config->endHz*boxsize; i++)	// Nyquist at 44.1khz
	{
		double r1 = creal(spectrum[i]);
		double i1 = cimag(spectrum[i]);
		double magnitude, phase, previous;
		int    j = 0;

		//magnitude  = sqrt(sqrt(r1 * r1 + i1 * i1) / root);
		magnitude = 2*sqrt(r1*r1 + i1*i1)/monoSignalSize;
		phase = atan2(i1, r1);

		previous = 10000;
		for(j = 0; j < config->MaxFreq; j++)
		{
			if(magnitude  > MaxFreqArray->freq[j].weight && magnitude  < previous)
			{
				//Move the previous values down the array
				for(int k = config->MaxFreq-1; k > j; k--)
					MaxFreqArray->freq[k] = MaxFreqArray->freq[k - 1];

				MaxFreqArray->freq[j].hertz = 
					(double)i/(boxsize*seconds);
				MaxFreqArray->freq[j].weight = magnitude;
				MaxFreqArray->freq[j].amplitude = 0;
				MaxFreqArray->freq[j].phase = phase;
				MaxFreqArray->freq[j].indexFFT = i;
				
			}

			previous = MaxFreqArray->freq[j].weight;
		}
	}
	
	// The following mess compressed adjacent frequencies
	// Disabled by default and nt as useful as expected in the current form
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
						MaxFreqArray->freq[i].weight/= 2;

						MaxFreqArray->freq[i].amplitude += MaxFreqArray->freq[j].amplitude;
						MaxFreqArray->freq[i].amplitude/= 2;

						MaxFreqArray->freq[j].hertz = 0;
						MaxFreqArray->freq[j].weight = 0;
						MaxFreqArray->freq[j].phase = 0;
						MaxFreqArray->freq[j].indexFFT = -1;
					}
					else
					{
						MaxFreqArray->freq[j].weight += MaxFreqArray->freq[i].weight;
						MaxFreqArray->freq[j].weight/= 2;

						MaxFreqArray->freq[j].amplitude += MaxFreqArray->freq[j].amplitude;
						MaxFreqArray->freq[j].amplitude/= 2;
	
						MaxFreqArray->freq[i].hertz = 0;
						MaxFreqArray->freq[i].weight = 0;
						MaxFreqArray->freq[i].amplitude = 0;
						MaxFreqArray->freq[i].phase = 0;
						MaxFreqArray->freq[i].indexFFT = -1;
					}

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

	// Do per note normalization if requested
	// This make it so total volume is ignored at a global level
	if(config->normalize == 'n')
	{
		MaxWeight = 0;
		// Find MaxWeight for Normalization
		for(i = 0; i < config->MaxFreq; i++)
		{
			if(MaxFreqArray->freq[i].indexFFT != -1)
			{
				if(MaxFreqArray->freq[i].weight > MaxWeight)
					MaxWeight = MaxFreqArray->freq[i].weight;
			}
		}
	 
		// Normalize to 100
		// Calculate per Note dbs
		for(i = 0; i < config->MaxFreq; i++)
		{
			MaxFreqArray->freq[i].amplitude = 
				20*log10(MaxFreqArray->freq[i].weight/MaxWeight);
			MaxFreqArray->freq[i].weight = 
				MaxFreqArray->freq[i].weight*100.0/MaxWeight;
		}
	}

	if(config->clockNote)
	{
		double			elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg("Processing Note took %f\n", elapsedSeconds);
	}

	free(signal);
	free(spectrum);

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
	logmsg("\n-- Results --\n\n");
	for(note = 0; note < MAX_NOTES; note++)
	{
		int count = 0;

		msg = 0;
		sprintf(diff, "Note: %s# %d (%d)\n", GetRange(note), GetSubIndex(note), note);
		msgPos = strlen(diff);
		if(msgPos > msgSize - 512)
		{
			diff = (char*)realloc(diff, (msgSize+4096)*(sizeof(char)));
			msgSize += 4096;
		}

		count = config->MaxFreq;

		// Use Silence as reference for floor Noise and amplitude cutoff
		if(ReferenceSignal->hasFloor)
		{
			for(int freq = 0; freq < config->MaxFreq; freq ++)
			{
				if(ReferenceSignal->Notes[note].freq[freq].amplitude <= ReferenceSignal->floorAmplitude)
				{
					count = freq;
					break;
				}
			}
		}

		for(int freq = 0; freq < count; freq++)
		{
			int CRTNoise;
				
			CRTNoise = IsCRTNoise(ReferenceSignal->Notes[note].freq[freq].hertz);
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

				compSize = config->MaxFreq;
				// Find Compared valid array size
				if(TestSignal->hasFloor)
				{
					for(int freq = 0; freq < config->MaxFreq; freq ++)
					{
						if(TestSignal->Notes[note].freq[freq].amplitude <= TestSignal->floorAmplitude)
						{
							compSize = freq;
							break;
						}
					}
				}
				else
				{
					for(int comp = 0; comp < config->MaxFreq; comp++)
					{
						if(TestSignal->Notes[note].freq[comp].hertz)
							compSize ++;
						else
							break;
					}
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
						sprintf(buffer, "\tDifferent Weight: %g Hz at %.4f%% (%0.2fdbs/%0.2f) instead of %g Hz at %.4f%% (%0.2fdbs/%0.2f) {%g}\n",
							TestSignal->Notes[note].freq[index].hertz,
							TestSignal->Notes[note].freq[index].weight,
							TestSignal->Notes[note].freq[index].amplitude,
							TestSignal->Notes[note].freq[index].phase,
							ReferenceSignal->Notes[note].freq[freq].hertz,
							ReferenceSignal->Notes[note].freq[freq].weight,
							ReferenceSignal->Notes[note].freq[freq].amplitude,
							ReferenceSignal->Notes[note].freq[freq].phase,
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
					sprintf(buffer, "\tReference Frequency not found: %g Hz at %.2f%% %.2f%% db (index: %d)\n",
							ReferenceSignal->Notes[note].freq[freq].hertz,
							ReferenceSignal->Notes[note].freq[freq].weight,
							ReferenceSignal->Notes[note].freq[freq].amplitude,
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
				PrintComparedNotes(&ReferenceSignal->Notes[note], &TestSignal->Notes[note],
					config, ReferenceSignal->hasFloor, ReferenceSignal->floorAmplitude);
			}
		}
		else
		{
			if(!config->justResults && config->showAll)
			{
				logmsg("Matched Note Report for %s# %d (%d)\n", GetRange(note), GetSubIndex(note), note);
				PrintComparedNotes(&ReferenceSignal->Notes[note], &TestSignal->Notes[note], 
					config, ReferenceSignal->hasFloor, ReferenceSignal->floorAmplitude);
			}
		}
	}

	logmsg("============================================================\n");
	logmsg("Reference: %s\nCompared to: %s\n", ReferenceSignal->SourceFile, TestSignal->SourceFile);
	if(FMcompared+PSGcompared)
	{
		logmsg("Total differences are %d out of %d [%g%% different]\n============================================================\n",
				 totalDiff, FMcompared+PSGcompared, (double)totalDiff*100.0/(double)(PSGcompared+FMcompared));
		if(config->spreadsheet)
		{
			logmsg("Spreadsheet-RefFile-CompFile-Diff-Comp-Percent, %s, %s, %d, %d, %g\n",
				 basename(ReferenceSignal->SourceFile), basename(TestSignal->SourceFile),
				 totalDiff, FMcompared+PSGcompared, (double)totalDiff*100.0/(double)(PSGcompared+FMcompared));
		}
	}	
	if(!totalDiff && FMcompared+PSGcompared)
	{
		if(config->tolerance == 0.0)
			logmsg("\n== WAV files are acoustically identical  == \n");
		else
			logmsg("\n== WAV files are equivalent under these parameters  == \n");
		logmsg("============================================================\n");
	}

	if(FMcompared+PSGcompared)
	{
		logmsg("FM Sound\n");
		logmsg("\tFM differences: %d\n",
			FMnotfound+FMweights);
		logmsg("\t\tNot found: %d of %d (%0.2f%%)\n", 
			FMnotfound, FMcompared, (double)FMnotfound/(double)FMcompared*100);
		logmsg("\t\tDifferent weights: %d of %d (%0.2f%%) [highest: %0.4f%%]\n",
			 FMweights, FMcompared, (double)FMweights/(double)FMcompared*100, FMhighDiff);

		logmsg("\n\tAdjustments made to match within defined ranges: %d total\n",
			FMadjHz+FMadjWeight);
		//if(config->HzDiff != 0.0)
		logmsg("\t\tFrequencies adjusted: %d of %d (%0.2f%%) [highest difference: %0.4f Hz]\n",
				FMadjHz, FMcompared, (double)FMadjHz/(double)FMcompared*100, FMhighHz);
		logmsg("\t\tWeights adjusted: %d of %d (%0.2f%%) [highest difference: %0.4f%%]\n",
			FMadjWeight, FMcompared, (double)FMadjWeight/(double)FMcompared*100, FMhighDiffAdj);

		logmsg("PSG Sound\n");
		logmsg("\tPSG differences: %d\n",
			PSGnotfound+PSGweights);
		logmsg("\t\tNot found: %d of %d (%0.2f%%)\n", 
			PSGnotfound, PSGcompared, (double)PSGnotfound/(double)PSGcompared*100);
		logmsg("\t\tDifferent weights: %d of %d (%0.2f%%) [highest: %0.4f%%]\n",
			PSGweights, PSGcompared, (double)PSGweights/(double)PSGcompared*100, PSGhighDiff);
		logmsg("\n\tAdjustments made to match within defined ranges: %d total\n",
		PSGadjHz+PSGadjWeight);
		//if(config->HzDiff != 0.0)
		logmsg("\t\tFrequencies adjusted: %d of %d (%0.2f%%) [highest difference: %0.4f Hz]\n",
			PSGadjHz, PSGcompared, (double)PSGadjHz/(double)PSGcompared*100, PSGhighHz);
		logmsg("\t\tWeights adjusted: %d of %d (%0.2f%%) [highest difference: %0.4f%%]\n",
			PSGadjWeight, PSGcompared, (double)PSGadjWeight/(double)PSGcompared*100, PSGhighDiffAdj);
	}
	else
		logmsg("Reference file has no frequencies at all!\n");

	if(HadCRTNoise)
		logmsg("\nReference Signal has CRT noise (15697 hz)\n");
	logmsg("\n");
	free(diff);
}


void PrintComparedNotes(MaxFreq *ReferenceArray, MaxFreq *ComparedArray, parameters *config, int hasFloor, double floorAmplitude)
{
	// changed weight->amplitude
	for(int j = 0; j < config->MaxFreq; j++)
	{
		if(ReferenceArray->freq[j].weight && ReferenceArray->freq[j].hertz)
		{
			int match = 0;

			logmsg("[%4.2d] Ref: %5g Hz\t%0.4f%%\t%0.4fdb [>%3d]", 
						j,
						ReferenceArray->freq[j].hertz,
						ReferenceArray->freq[j].weight,
						ReferenceArray->freq[j].amplitude,
						ReferenceArray->freq[j].matched - 1);

			if(ComparedArray->freq[j].hertz)
				logmsg("\tComp:\t%5g Hz\t%0.4f%%\t%0.4fdb [<%3d]", 
						ComparedArray->freq[j].hertz,
						ComparedArray->freq[j].weight,
						ComparedArray->freq[j].amplitude,
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
			// Use Silence as reference for floor Noise and amplitude cutoff
			if(hasFloor && ReferenceArray->freq[j].weight <= floorAmplitude)
				break;
		}
	}
	logmsg("\n\n");
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

int commandline(int argc , char *argv[], parameters *config)
{
	int c, index, ref = 0, tar = 0;
	
	opterr = 0;
	
	CleanParameters(config);

	while ((c = getopt (argc, argv, "hlkmxgiejyvw:n:d:a:p:t:r:c:f:b:s:z:")) != -1)
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
	  case 'v':
		config->verbose = 1;
		break;
	  case 'k':
		config->clock = 1;
		break;
	  case 'g':
		config->clockNote = 1;
		break;
	  case 'm':
		config->showAll = 1;
		break;
	  case 'l':
		do_log = 1;
		break;
	  case 'x':
		config->spreadsheet = 1;
		break;
	  case 'i':
		config->invertCompare = 1;
		break;
	  case 'y':
		config->debugVerbose = 1;
		config->verbose = 1;
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
		if(config->MaxFreq < 1 || config->MaxFreq > COUNT)
			config->MaxFreq = COUNT;
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
		else if (optopt == 't')
		  logmsg("Weight tolerance percentage -%c requires an argument: 0.0-100.0\n", optopt);
		else if (optopt == 'f')
		  logmsg("Max # of frequencies to use from FFTW -%c requires an argument: 1-%d\n", optopt, COUNT);
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

	if(config->endHz <= config->startHz)
	{
		logmsg("Invalid frequency range for FFTW (%d Hz to %d Hz)\n", config->startHz, config->endHz);
		return 0;
	}

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
		do_log = 2;
		Header(1);
		do_log = 1;
	}

	logmsg("\tWeight tolerance percentage to compare is %0.2f%%\n", config->tolerance);
	logmsg("\tAudio Channel is: %s\n", GetChannel(config->channel));
	if(config->startHz != START_HZ)
		logmsg("\tFrequency start range for FFTW is now %d (default %d)\n", config->startHz, START_HZ);
	if(config->endHz != END_HZ)
		logmsg("\tFrequency end range for FFTW is now %d (default %d)\n", config->endHz, END_HZ);
	if(config->MaxFreq != FREQ_COUNT)
		logmsg("\tMax frequencies to use from FFTW are %d (default %d)\n", config->MaxFreq, FREQ_COUNT);
	if(config->HzWidth != HERTZ_WIDTH)
		logmsg("\tHertz Width Compression changed to %g (default %g)\n", config->HzWidth, HERTZ_WIDTH);
	if(config->HzDiff != HERTZ_DIFF)
		logmsg("\tHertz Difference tolerance %f (default %d)\n", config->HzDiff, HERTZ_DIFF);
	if(config->window != 'n')
		logmsg("\tA %s window will be applied to each note to be compared\n", GetWindow(config->window));

	return 1;
}

void PrintUsage()
{
	// b,d and y options are not documented since they are mostly for testing or found not as usefull as desired
	logmsg("  usage: mdfourier -r reference.wav -c compare.wav\n\n");
	logmsg("   FFT and Analysis options:\n");
	logmsg("	 -p: Defines the top <p>ercentage of the\n\tnormalized signal frequency weights to match from the reference audio\n");
	logmsg("	 -t: Defines the percentage of <t>olerance when comparing weights\n");
	logmsg("	 -s: Defines <s>tart of the frequency range to compare with FFT\n\tA smaller range will compare more frequencies unless changed\n");
	logmsg("	 -z: Defines end of the frequency range to compare with FFT\n\tA smaller range will compare more frequencies unless changed\n");
	logmsg("	 -c <l,r,s>: select audio <c>hannel to compare. Default is both channels\n\t's' stereo, 'l' for left or 'r' for right\n");
	logmsg("	 -w: enable Hanning <w>indowing. This introduces more differences\n");
	logmsg("	 -f: Change the number of frequencies to use from FFTW\n");
	logmsg("   Output options:\n");
	logmsg("	 -v: Enable <v>erbose mode, spits all the FFTW results\n");
	logmsg("	 -j: Cuts all the per note information and shows <j>ust the total results\n");
	logmsg("	 -e: Enables <e>xtended results. Shows a table with all matched\n\tfrequencies for each note with differences\n");
	logmsg("	 -m: Enables Show all notes compared with <m>atched frequencies\n");
	logmsg("	 -x: Enables totaled output for use with grep and spreadsheet\n");
	logmsg("	 -l: <l>og output to file [reference]_vs_[compare].txt\n");
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

float *hannWindow(int n)
{
	int half, i, idx;
	float *w;
 
	w = (float*) calloc(n, sizeof(float));
	memset(w, 0, n*sizeof(float));

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
 
	return(w);
}

// reduce scalloping loss 
float *flattopWindow(int n)
{
	int half, i, idx;
	float *w;
 
	w = (float*) calloc(n, sizeof(float));
	memset(w, 0, n*sizeof(float));
 
	if(n%2==0)
	{
		half = n/2;
		for(i=0; i<half; i++)
		{
			double factor = 2*M_PI*i/(n-1);
			w[i] = 0.21557895 - 0.41663158*cos(factor) + 0.277263158*cos(2*factor) 
				  -0.083578947*cos(3*factor) + 0.006947368*cos(4*factor);
		}
 
		idx = half-1;
		for(i=half; i<n; i++) {
			w[i] = w[idx];
			idx--;
		}
	}
	else
	{
		half = (n+1)/2;
		for(i=0; i<half; i++)
		{
			double factor = 2*M_PI*i/(n-1);
			w[i] = 0.21557895 - 0.41663158*cos(factor) + 0.277263158*cos(2*factor) 
				  -0.083578947*cos(3*factor) + 0.006947368*cos(4*factor);
		}
 
		idx = half-2;
		for(i=half; i<n; i++) {
			w[i] = w[idx];
			idx--;
		}
	}
 
	return(w);
}


// Only attenuate the edges to reduce errors
// 2.5% slopes
float *tukeyWindow(int n)
{
	int slope, i, idx;
	float *w;
 
	w = (float*) calloc(n, sizeof(float));
	memset(w, 0, n*sizeof(float));
 
	if(n%2==0)
	{
		slope = n/40;
		for(i=0; i<slope; i++)
		{
			w[i] = 85*(1+cos(2*M_PI/(n-1)*(i-(n-1)/2)));
			if(w[i] > 1.0)
				w[i] = 1.0;
		}

		for(i=0; i<n-2*slope; i++)
			w[i+slope] = 1;
 
		idx = slope-1;
		for(i=n - slope; i<n; i++) {
			w[i] = w[idx];
			idx--;
		}
	}
	else
	{
		slope = (n+1)/40;
		for(i=0; i<slope; i++)
		{
			w[i] = 85*(1+cos(2*M_PI/(n-1)*(i-(n-1)/2)));
			if(w[i] > 1.0)
				w[i] = 1.0;
		}

		for(i=0; i<n-2*slope; i++)
			w[i+slope] = 1;
 
		idx = slope-2;
		for(i=n - slope; i<n; i++) {
			w[i] = w[idx];
			idx--;
		}
	}
 
	return(w);
}
