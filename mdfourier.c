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

#define MDVERSION "0.75"

#define PSG_COUNT	40
#define NOISE_COUNT 100

#define MAX_FREQ_COUNT		22050 	// Number of frequencies to account for (MAX) 
#define FREQ_COUNT			2000	// Number of frequencies to account for (default)
#define MAX_NOTES			140		// we have 140 notes in the 240p Test Suite

#define TYPE_NONE	0
#define TYPE_FM 	1
#define TYPE_PSG	2
#define TYPE_NOISE	3

// This is the difference allowed between reference and compared
// amplitudes to match, in dbs
#define DBS_TOLERANCE	3.0

// Width of each peak
#define HERTZ_WIDTH			0.0

// +/- Tolerance in frequency Difference to be the same one
#define HERTZ_DIFF			0.0

//Percentage of normalized magnitude frequencies to match
#define FREQ_COMPARE		1.0

#define START_HZ	10
#define END_HZ		MAX_FREQ_COUNT

// Number of frequencies to characterize Noise
#define FLOORSIZE	4

typedef struct FrequencySt {
	double hertz;
	double magnitude;
	double amplitude;
	double phase;
	long   indexFFT;
	int    matched;
} Frequency;

typedef struct MaxFreqSt {
	Frequency	freq[MAX_FREQ_COUNT];
	int 		index;
	int 		type;
} MaxFreq;

typedef struct GenesisAudioSt {
	MaxFreq 	Notes[MAX_NOTES+1]; 
	char		SourceFile[1024];
	int 		hasFloor;
	double		floorFreq[FLOORSIZE];
	double		floorAmplitude[FLOORSIZE];
	int			floorCount;
}  GenesisAudio;

int IsCRTNoise(double freq)
{
	if(freq >= 15620 && freq <= 15710) //Peak around 15697-15698 usualy
		return 1;
	return 0;
}

void CleanAudio(GenesisAudio *Signal)
{
	if(!Signal)
		return;
	for(int n = 0; n < MAX_NOTES+1; n++)
	{
		for(int i = 0; i < MAX_FREQ_COUNT; i++)
		{
			Signal->Notes[n].freq[i].hertz = 0;
			Signal->Notes[n].freq[i].magnitude = 0;
			Signal->Notes[n].freq[i].amplitude = 0;
			Signal->Notes[n].freq[i].phase = 0;
			Signal->Notes[n].freq[i].indexFFT = -1;
			Signal->Notes[n].freq[i].matched = 0;
		}
	}
	Signal->SourceFile[0] = '\0';
	Signal->hasFloor = 0;
	for(int i = 0; i < FLOORSIZE; i++)
	{
		Signal->floorFreq[i] = 0.0;
		Signal->floorAmplitude[i] = 0.0;	
	}
	Signal->floorCount = 0;
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

typedef struct window_st {
	float *win1Sec;
	float *win2Sec;
} windowManager;


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
	char 		normalize;
	double		relativeMaxMagnitude;
	int			ignoreFloor;
} parameters;

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
}

int do_log = 0;
char log_file[2048];
FILE *logfile = NULL;

void DisableConsole() { do_log = 2; }
void EnableConsole() { do_log = 1; }
int IsLogEnabled() { return do_log; }

int LoadFile(FILE *file, GenesisAudio *Signal, parameters *config, char *fileName);
double ProcessSamples(MaxFreq *MaxFreqArray, short *samples, size_t size, long samplerate, float *window, parameters *config);
double CompareNotes(GenesisAudio *ReferenceSignal, GenesisAudio *TestSignal, parameters *config);
void CleanMatched(GenesisAudio *ReferenceSignal, GenesisAudio *TestSignal, parameters *config);
void PrintComparedNotes(MaxFreq *ReferenceArray, MaxFreq *ComparedArray, parameters *config, GenesisAudio *Signal);
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
	double				result1 = 0, result2 = 0;

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
		config.relativeMaxMagnitude = 0.0;

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

	result1 = CompareNotes(ReferenceSignal, TestSignal, &config);
	if(config.normalize == 'r')
		config.relativeMaxMagnitude = 0.0;
	CleanMatched(ReferenceSignal, TestSignal, &config);
	result2 = CompareNotes(TestSignal, ReferenceSignal, &config);

	if(config.spreadsheet)
		logmsg("Spreadsheet-RefFile-CompFile-NoiseR-NoiseC-Percent-PrecentReverse, %s, %s, %g, %g, %g, %g\n",
				 basename(ReferenceSignal->SourceFile), basename(TestSignal->SourceFile),
				 ReferenceSignal->floorAmplitude[0], TestSignal->floorAmplitude[0],
				result1, result2);

	free(ReferenceSignal);
	free(TestSignal);
	ReferenceSignal = NULL;
	TestSignal = NULL;

	if(IsLogEnabled())
	{
		fclose(logfile);
		printf("Check logfile for extended results\n");
	}
	
	return(0);
}

int initWindows(windowManager *windows, int SamplesPerSec, char type)
{
	if(!windows)
		return 0;

	windows->win1Sec = NULL;
	windows->win2Sec = NULL;

	if(type != 'n')
	{
		if(type == 't')
		{
			windows->win2Sec = tukeyWindow(SamplesPerSec*2);
			if(!windows->win2Sec)
			{
				logmsg ("Tukey window creation failed\n");
				return(0);
			}
		
			windows->win1Sec = tukeyWindow(SamplesPerSec);
			if(!windows->win2Sec)
			{
				logmsg ("Tukey window creation failed\n");
				return(0);
			}
		}

		if(type == 'f')
		{
			windows->win2Sec = flattopWindow(SamplesPerSec*2);
			if(!windows->win2Sec)
			{
				logmsg ("FlatTop window creation failed\n");
				return(0);
			}
		
			windows->win1Sec = flattopWindow(SamplesPerSec);
			if(!windows->win1Sec)
			{
				logmsg ("FlatTop window creation failed\n");
				return(0);
			}
		}

		if(type == 'h')
		{
			windows->win2Sec = hannWindow(SamplesPerSec*2);
			if(!windows->win2Sec)
			{
				logmsg ("Hann window creation failed\n");
				return(0);
			}
		
			windows->win1Sec = hannWindow(SamplesPerSec);
			if(!windows->win1Sec)
			{
				logmsg ("Hann window creation failed\n");
				return(0);
			}
		}
	}

	/*
	if(windows->win2Sec)
		for(int w= 0; w < SamplesPerSec*2; w++)
			logmsg("%d,%g\n", w, windows->win2Sec[w]);
	exit(1); 
	*/

	return 1;
}

void freeWindows(windowManager *windows)
{
	if(!windows)
		return;

	free(windows->win1Sec);
	windows->win1Sec = NULL;
	free(windows->win2Sec);
	windows->win2Sec = NULL;
}

int LoadFile(FILE *file, GenesisAudio *Signal, parameters *config, char *fileName)
{
	int 				i = 0;
	int 				loadedNoteSize = 0;
	char				*buffer;
	size_t			 	buffersize = 0;
	size_t			 	discardBytes = 0, discardSamples = 0;
	wav_hdr 			header;
	windowManager		windows;
	float				*windowUsed = NULL;
	struct	timespec	start, end;
	double				seconds = 0;
	char 				*AllSamples = NULL;
	long int			pos = 0;

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

	// We need to convert buffersize to the 16.688ms per frame by the Genesis
	discardSamples = (size_t)round(1.00128*header.SamplesPerSec);
	if(discardSamples % 2)
		discardSamples += 1;

	discardSamples -= header.SamplesPerSec;
	buffersize = header.SamplesPerSec*8*sizeof(char); // 2 bytes per sample, stereo
	buffer = (char*)malloc(buffersize);
	if(!buffer)
	{
		logmsg("\tmalloc failed\n");
		return(0);
	}

	if(!initWindows(&windows, header.SamplesPerSec, config->window))
		return 0;

	AllSamples = (char*)malloc(sizeof(char)*header.Subchunk2Size);
	if(!AllSamples)
	{
		logmsg("\tAll Chunks malloc failed!\n");
		return(0);
	}

	if(fread(AllSamples, 1, sizeof(char)*header.Subchunk2Size, file) != sizeof(char)*header.Subchunk2Size)
	{
		logmsg("\tCould not read the whole sample block from disk to RAM\n");
		return(0);
	}

	CleanAudio(Signal);

	if(seconds >= 181)
		Signal->hasFloor = 1;

	sprintf(Signal->SourceFile, "%s", fileName);
	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	while(i < MAX_NOTES+Signal->hasFloor)
	{
		if(i < PSG_COUNT)
		{
			loadedNoteSize = buffersize;   //2 second blocks
			discardBytes = discardSamples * 8;
			if(config->window != 'n')
				windowUsed = windows.win2Sec;
		}
		else
		{
			loadedNoteSize = buffersize/2; // 1 second block
			discardBytes = discardSamples * 4;
			if(config->window != 'n')
				windowUsed = windows.win1Sec;
		}

		memset(buffer, 0, buffersize);
		if(pos + loadedNoteSize > header.Subchunk2Size)
		{
			logmsg("\tunexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
			break;
		}
		memcpy(buffer, AllSamples + pos, loadedNoteSize);
		pos += loadedNoteSize;

		Signal->Notes[i].index = i < PSG_COUNT ? i : i - PSG_COUNT;
		Signal->Notes[i].type = i < PSG_COUNT ? TYPE_FM : TYPE_PSG;

		if(0)
		{
			FILE 		*chunk = NULL;
			wav_hdr		cheader;
			char		Name[2048];

			cheader = header;
			sprintf(Name, "%03d_Source_chunk_%s", i, basename(fileName));
			chunk = fopen(Name, "wb");
			if(!chunk)
			{
				logmsg("\tCould not open chunk file %s\n", Name);
				return 0;
			}

			cheader.ChunkSize = loadedNoteSize+36;
			cheader.Subchunk2Size = loadedNoteSize;
			if(fwrite(&cheader, 1, sizeof(wav_hdr), chunk) != sizeof(wav_hdr))
			{
				logmsg("\tCould not write chunk header\n");
				return(0);
			}
		
			if(fwrite(buffer, 1, sizeof(char)*loadedNoteSize, chunk) != sizeof(char)*loadedNoteSize)
			{
				logmsg("\tCould not write samples to chunk file\n");
				return (0);
			}
		
			fclose(chunk);
		}

		ProcessSamples(&Signal->Notes[i], (short*)buffer, loadedNoteSize/2, header.SamplesPerSec, windowUsed, config);

		pos += discardBytes;  // Advance to adjust the time for the Sega Genesis Frame Rate
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

	if(!config->ignoreFloor && Signal->hasFloor) // analyze noise floor if available
		FindFloor(Signal, config);

	fclose(file);
	free(buffer);
	free(AllSamples);

	freeWindows(&windows);

	if(config->verbose)
		PrintFrequencies(Signal, config);

	return i;
}

void FindFloor(GenesisAudio *Signal, parameters *config)
{
	int count = 0;
	if(!Signal)
		return;
	
	if(!Signal->hasFloor)
		return;

	for(int i = 0; i < config->MaxFreq; i++)
	{
		if(!IsCRTNoise(Signal->Notes[MAX_NOTES].freq[i].hertz))
		{
			Signal->floorAmplitude[count] = Signal->Notes[MAX_NOTES].freq[i].amplitude;
			Signal->floorFreq[count] = Signal->Notes[MAX_NOTES].freq[i].hertz;
			logmsg("Found 'Silence' block, top[%d]: %g Hz at %0.4f.db\n",
				count+1, Signal->floorFreq[count], Signal->floorAmplitude[count]);
			count++;
			Signal->floorCount = count;
			if(count == FLOORSIZE)
				return;
		}
	}
	if(!count)
		Signal->hasFloor = 0;  // revoke it if not found
	Signal->floorCount = count;
}


void GlobalNormalize(GenesisAudio *Signal, parameters *config)
{
	double MaxMagnitude = 0;

	// Find global peak
	if(config->normalize == 'g' ||
	  (config->normalize == 'r' && config->relativeMaxMagnitude == 0.0))
	{
		for(int note = 0; note < MAX_NOTES+Signal->hasFloor; note++)
		{
			for(int i = 0; i < config->MaxFreq; i++)
			{
				if(!Signal->Notes[note].freq[i].hertz)
					break;
				if(Signal->Notes[note].freq[i].magnitude > MaxMagnitude)
					MaxMagnitude = Signal->Notes[note].freq[i].magnitude;
			}
		}

		if(config->normalize == 'r')
			config->relativeMaxMagnitude = MaxMagnitude;
	}

	if(config->normalize == 'r' && config->relativeMaxMagnitude != 0.0)
		MaxMagnitude = config->relativeMaxMagnitude;

	//Normalize and calculate Amplitude in dbs
	for(int note = 0; note < MAX_NOTES+Signal->hasFloor; note++)
	{
		for(int i = 0; i < config->MaxFreq; i++)
		{
			if(!Signal->Notes[note].freq[i].hertz)
				break;
			Signal->Notes[note].freq[i].amplitude = 
				20*log10(Signal->Notes[note].freq[i].magnitude / MaxMagnitude);
			Signal->Notes[note].freq[i].magnitude = 
				Signal->Notes[note].freq[i].magnitude * 100.0 / MaxMagnitude;
		}
	}
}

void CleanMatched(GenesisAudio *ReferenceSignal, GenesisAudio *TestSignal, parameters *config)
{
	for(int note = 0; note < MAX_NOTES+ReferenceSignal->hasFloor; note++)
	{
		for(int i = 0; i < config->MaxFreq; i++)
		{
			if(!ReferenceSignal->Notes[note].freq[i].hertz)
				break;
			ReferenceSignal->Notes[note].freq[i].matched = 0;	
		}
	}

	for(int note = 0; note < MAX_NOTES+TestSignal->hasFloor; note++)
	{
		for(int i = 0; i < config->MaxFreq; i++)
		{
			if(!TestSignal->Notes[note].freq[i].hertz)
				break;
			TestSignal->Notes[note].freq[i].matched = 0;
		}
	}
}

void PrintFrequencies(GenesisAudio *Signal, parameters *config)
{
	if(IsLogEnabled())
			DisableConsole();

	for(int note = 0; note < MAX_NOTES+Signal->hasFloor; note++)
	{
		logmsg("==================== %s# %d (%d) ===================\n", GetRange(note), GetSubIndex(note), note);
			if(config->spreadsheet)
				logmsg("Spreadsheet-%s#%d\n", GetRange(note), GetSubIndex(note));

		for(int j = 0; j < config->MaxFreq; j++)
		{
			if(Signal->Notes[note].freq[j].hertz)
			{
				logmsg("Frequency [%2d] %5.4g Hz [Amplitude: %g] [Phase: %g]",
					j, 
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
				logmsg("Spreadsheet-index-Hz-amplitude, %d, %g, %g\n",
					j, Signal->Notes[note].freq[j].hertz, Signal->Notes[note].freq[j].amplitude);
			}

			if(config->debugVerbose && j == 20)  // this is just for internal quick debugging
				exit(1);
		}
	}

	if(IsLogEnabled())
		EnableConsole();
}

void logmsg(char *fmt, ... )
{
	va_list arguments; 

	va_start(arguments, fmt);
	if(do_log != 2)
		vprintf(fmt, arguments);

	if(do_log)
		vfprintf(logfile, fmt, arguments);

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
	double		 	boxsize = 0, seconds = 0;
	struct			timespec start, end;
	double			MaxMagnitude = 0;
	
	if(!MaxFreqArray)
	{
		logmsg("No Array for results\n");
		return 0;
	}

	stereoSignalSize = (long)size;
	monoSignalSize = stereoSignalSize/2;	 // 4 is 2 16 bit values
	seconds = size/(samplerate*2);
	boxsize = seconds;

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

	//for(i = 1; i < monoSignalSize/2+1; i++)
	for(i = config->startHz*boxsize; i < config->endHz*boxsize; i++)	// Nyquist at 44.1khz
	{
		double r1 = creal(spectrum[i]);
		double i1 = cimag(spectrum[i]);
		double magnitude, previous;
		int    j = 0;
		double Hertz;

		magnitude = sqrt(r1*r1 + i1*i1)/monoSignalSize;
		//magnitude = 2*(r1*r1 + i1*i1)/(monoSignalSize*monoSignalSize);
 		//amplitude = 10 * log10(magnitude+DBL_EPSILON);
		Hertz = ((double)i/seconds);

		previous = 1.e30;
		//printf("%g Hz %g m (old %g) %g dbs\n", Hertz, m, magnitude, a);
		if(!IsCRTNoise(Hertz))
		{
			for(j = 0; j < config->MaxFreq; j++)
			{
				if(magnitude > MaxFreqArray->freq[j].magnitude && magnitude < previous)
				{
					//Move the previous values down the array
					for(int k = config->MaxFreq-1; k > j; k--)
						MaxFreqArray->freq[k] = MaxFreqArray->freq[k - 1];
	
					MaxFreqArray->freq[j].hertz = Hertz;
					MaxFreqArray->freq[j].magnitude = magnitude;
					MaxFreqArray->freq[j].amplitude = 0;
					MaxFreqArray->freq[j].phase = atan2(i1, r1);
					MaxFreqArray->freq[j].indexFFT = i;
					break;
				}
				previous = MaxFreqArray->freq[j].magnitude;
			}
		}
	}
	
	// The following mess compressed adjacent frequencies
	// Disabled by default and it is not as useful as expected in the current form
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
					if(MaxFreqArray->freq[i].magnitude > MaxFreqArray->freq[j].magnitude)
					{
						MaxFreqArray->freq[i].magnitude += MaxFreqArray->freq[j].magnitude;
						//MaxFreqArray->freq[i].magnitude/= 2;

						MaxFreqArray->freq[j].hertz = 0;
						MaxFreqArray->freq[j].magnitude = 0;
						MaxFreqArray->freq[j].phase = 0;
						MaxFreqArray->freq[i].amplitude = 0;
						MaxFreqArray->freq[j].indexFFT = -1;
					}
					else
					{
						MaxFreqArray->freq[j].magnitude += MaxFreqArray->freq[i].magnitude;
						//MaxFreqArray->freq[j].magnitude/= 2;

						MaxFreqArray->freq[i].hertz = 0;
						MaxFreqArray->freq[i].magnitude = 0;
						MaxFreqArray->freq[i].phase = 0;
						MaxFreqArray->freq[i].amplitude = 0;
						MaxFreqArray->freq[i].indexFFT = -1;
					}
				}
			}
		}

		// sort array by magnitude
		for(i = 0; i < config->MaxFreq; i++)
		{
			for(int j = i + 1; j < config->MaxFreq; j++)
			{
				if (MaxFreqArray->freq[i].magnitude < MaxFreqArray->freq[j].magnitude) 
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
	// This makes it so relative channel/note 
	// volume is ignored at a global level
	if(config->normalize == 'n')
	{
		MaxMagnitude = 0;
		// Find MaxMagnitude for Normalization
		for(i = 0; i < config->MaxFreq; i++)
		{
			if(MaxFreqArray->freq[i].indexFFT != -1)
			{
				if(MaxFreqArray->freq[i].magnitude > MaxMagnitude)
					MaxMagnitude = MaxFreqArray->freq[i].magnitude;
			}
		}
	 
		// Normalize to 100
		// Calculate per Note dbs
		for(i = 0; i < config->MaxFreq; i++)
		{
			MaxFreqArray->freq[i].amplitude = 
				20*log10(MaxFreqArray->freq[i].magnitude/MaxMagnitude);
			MaxFreqArray->freq[i].magnitude = 
				MaxFreqArray->freq[i].magnitude*100.0/MaxMagnitude;
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
	fftw_free(spectrum);

	return(0);
}

typedef struct msg_buffer_st {
	char *message;
	char buffer[1024];
	int msgPos;
	int msgSize;
} msgbuff;

void InsertMessageInBuffer(msgbuff *message, parameters *config)
{
	if(config->justResults)
		return;

	strcat(message->message, message->buffer);
	message->msgPos = strlen(message->message);
	if(message->msgPos > message->msgSize - 512)
	{
		message->message = (char*)realloc(message->message, (message->msgSize+4096)*(sizeof(char)));
		message->msgSize += 4096;
	}
}

int CalculateMaxCompare(int note, msgbuff *message, GenesisAudio *Signal, parameters *config)
{
	int count = config->MaxFreq;

	// find how many to compare
	if(!config->ignoreFloor && Signal->hasFloor)
	{
		for(int freq = 0; freq < config->MaxFreq; freq ++)
		{
			if(!Signal->Notes[note].freq[freq].hertz)
			{
				/*
				if(IsLogEnabled())
					DisableConsole();

				sprintf(message->buffer, "Note %d Broke at %d: %g Hz %g dbs\n", 
					note, freq, Signal->Notes[note].freq[freq].hertz, Signal->Notes[note].freq[freq].amplitude);
				InsertMessageInBuffer(message, config);
			
				if(IsLogEnabled())
					EnableConsole();
				*/

				count = freq;
				return count;
			}


			for(int count = 0; count < Signal->floorCount; count ++)
			{
				double difference;

				if(Signal->Notes[note].freq[freq].amplitude <= Signal->floorAmplitude[Signal->floorCount-1])
				{
					/*
					if(IsLogEnabled())
						DisableConsole();

					sprintf(message->buffer, "XNote %d Broke at %d: %g Hz %g dbs\n", 
						note, freq, Signal->Notes[note].freq[freq].hertz, Signal->Notes[note].freq[freq].amplitude);
					InsertMessageInBuffer(message, config);
				
					if(IsLogEnabled())
						EnableConsole();
					*/

					count = freq;
					return count;
				}

				difference = fabs(fabs(Signal->floorAmplitude[count]) - fabs(Signal->Notes[note].freq[freq].amplitude));
				if((Signal->Notes[note].freq[freq].hertz == Signal->floorFreq[count] &&
					difference <= config->tolerance))  // this in dbs
				{
					/*
					if(IsLogEnabled())
						DisableConsole();

					sprintf(message->buffer, "*Note %d Broke at %d: %g Hz %g dbs (%g)\n", 
						note, freq, Signal->Notes[note].freq[freq].hertz, Signal->Notes[note].freq[freq].amplitude, difference);
					InsertMessageInBuffer(message, config);
				
					if(IsLogEnabled())
						EnableConsole();
					*/

					count = freq;
					return count;
				}
			}
		}
	}
	else
	{
		for(int freq = 0; freq < config->MaxFreq; freq++)
		{
			if(!Signal->Notes[note].freq[freq].hertz)
			{
				/*
				if(IsLogEnabled())
					DisableConsole();

				sprintf(message->buffer, "Note %d Broke at %d: %g Hz %g dbs\n", 
					note, freq, Signal->Notes[note].freq[freq].hertz, Signal->Notes[note].freq[freq].amplitude);
				InsertMessageInBuffer(message, config);
			
				if(IsLogEnabled())
					EnableConsole();
				*/

				count = freq;
				return count;
			}
		}
	}

	/*
	if(IsLogEnabled())
		DisableConsole();

	sprintf(message->buffer, "+Note %d Broke at %d: %g Hz %g dbs\n", 
		note, count, Signal->Notes[note].freq[count-1].hertz, Signal->Notes[note].freq[count-1].amplitude);
	InsertMessageInBuffer(message, config);

	if(IsLogEnabled())
		EnableConsole();
	*/
	return count;
}

double CompareNotes(GenesisAudio *ReferenceSignal, GenesisAudio *TestSignal, parameters *config)
{
	int 	note, msg = 0, totalDiff = 0, HadCRTNoise = 0;
	int 	FM_amplitudes = 0, FMnotfound = 0;
	int 	FMadjAmplitudes = 0, FMadjHz = 0;
	int 	FMcompared = 0, PSGcompared = 0;
	double	FMhighDiffAdj = 0, FMhighDiff = 0, FMhighHz = 0;
	int 	PSG_Amplitudes = 0, PSGnotfound = 0;
	int 	PSGadjAmplitudes = 0, PSGadjHz = 0;
	double	PSGhighDiffAdj = 0, PSGhighDiff = 0, PSGhighHz = 0;
	msgbuff message;

	message.message = malloc(sizeof(char)*4096);  // I know this sounds strange, but we are protecting
	if(!message.message)						   // for when many differences do appear
	{
		logmsg("Insufficient memory\n");
		return 0;
	}	
	message.msgSize = 4096;

	logmsg("\n-- Results --\n\n");
	for(note = 0; note < MAX_NOTES; note++)
	{
		int refSize = 0, testSize = 0;

		msg = 0;
		message.msgPos = 0;
		message.message[0] = '\0';

		sprintf(message.buffer, "Note: %s# %d (%d)\n", GetRange(note), GetSubIndex(note), note);
		InsertMessageInBuffer(&message, config);

		refSize = CalculateMaxCompare(note, &message, ReferenceSignal, config);
		testSize = CalculateMaxCompare(note, &message, TestSignal, config);

		for(int freq = 0; freq < refSize; freq++)
		{
			int CRTNoise;
				
			CRTNoise = IsCRTNoise(ReferenceSignal->Notes[note].freq[freq].hertz);
			if(CRTNoise)
				HadCRTNoise = 1;

			// Ignore CRT noise
			if(!CRTNoise)
			{
				int found = 0, index = 0;

				if(ReferenceSignal->Notes[note].type == TYPE_FM)
					FMcompared ++;
				else
					PSGcompared ++;

				for(int comp = 0; comp < testSize; comp++)
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
					double	lowest = 22050, tmpDiff = 0;
					int 	lowIndex = -1;

					// Find closest match
					for(int comp = 0; comp < testSize; comp++)
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

				if(found)  // Now in either case, compare amplitudes
				{
					double test;
	
					test = fabs(TestSignal->Notes[note].freq[index].amplitude - ReferenceSignal->Notes[note].freq[freq].amplitude);
					//if(test > config->tolerance && ReferenceSignal->Notes[note].freq[freq].amplitude > -60.0 && freq < config->MaxFreq*.9) // TEST
					if(test > config->tolerance)
					{
						sprintf(message.buffer, "\tDifferent Amplitude: %g Hz at %0.4fdbs (ph:%0.2f) instead of %g Hz at %0.2fdbs (ph:%0.2f) {%g}\n",
							TestSignal->Notes[note].freq[index].hertz,
							TestSignal->Notes[note].freq[index].amplitude,
							TestSignal->Notes[note].freq[index].phase,
							ReferenceSignal->Notes[note].freq[freq].hertz,
							ReferenceSignal->Notes[note].freq[freq].amplitude,
							ReferenceSignal->Notes[note].freq[freq].phase,
							test);	
						InsertMessageInBuffer(&message, config);
						msg++;
						totalDiff++;

						if(ReferenceSignal->Notes[note].type == TYPE_FM)
						{
							FM_amplitudes ++;
	
							if(test > FMhighDiff)
								FMhighDiff = test;
						}
						else
						{
							PSG_Amplitudes ++;
	
							if(test > PSGhighDiff)
								PSGhighDiff = test;
						}
					}
		
					if(test && test <= config->tolerance)
					{
						if(ReferenceSignal->Notes[note].type == TYPE_FM)
						{
							FMadjAmplitudes++;
							if(test > FMhighDiffAdj)
								FMhighDiffAdj = test;
						}
						else
						{
							PSGadjAmplitudes++;
							if(test > PSGhighDiffAdj)
								PSGhighDiffAdj = test;
						}
					}
				}

				//if(!found && ReferenceSignal->Notes[note].freq[freq].amplitude > -60.0 && freq < config->MaxFreq*.9)  //TEST
				if(!found)
				{
					sprintf(message.buffer, "\tReference Frequency not found: %g Hz at %5.4f db (index: %d)\n",
							ReferenceSignal->Notes[note].freq[freq].hertz,
							ReferenceSignal->Notes[note].freq[freq].amplitude,
							freq);
					InsertMessageInBuffer(&message, config);
					msg ++;
					totalDiff++;

					if(ReferenceSignal->Notes[note].type == TYPE_FM)
						FMnotfound ++;
					else
						PSGnotfound ++;
				}
			}
		}

		if(msg && !config->justResults)
		{
			if(IsLogEnabled())
				DisableConsole();
			logmsg("%s\n", message.message);
			if(config->extendedResults)
			{
				logmsg("Unmatched Note Report for %s# %d (%d)\n", GetRange(note), GetSubIndex(note), note);
				PrintComparedNotes(&ReferenceSignal->Notes[note], &TestSignal->Notes[note],
					config, ReferenceSignal);
			}
			if(IsLogEnabled())
				EnableConsole();
		}
		else
		{
			if(IsLogEnabled())
				DisableConsole();
			if(!config->justResults && config->showAll)
			{
				logmsg("Matched Note Report for %s# %d (%d)\n", GetRange(note), GetSubIndex(note), note);
				PrintComparedNotes(&ReferenceSignal->Notes[note], &TestSignal->Notes[note], 
					config, ReferenceSignal);
			}
			if(IsLogEnabled())
				EnableConsole();
		}
	}

	logmsg("============================================================\n");
	logmsg("Reference: %s\nCompared to: %s\n", ReferenceSignal->SourceFile, TestSignal->SourceFile);
	if(FMcompared+PSGcompared)
	{
		logmsg("Total differences are %d out of %d [%g%% different]\n============================================================\n",
				 totalDiff, FMcompared+PSGcompared, (double)totalDiff*100.0/(double)(PSGcompared+FMcompared));
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
			FMnotfound+FM_amplitudes);
		logmsg("\t\tNot found: %d of %d (%0.2f%%)\n", 
			FMnotfound, FMcompared, (double)FMnotfound/(double)FMcompared*100);
		logmsg("\t\tDifferent Amplitudes: %d of %d (%0.2f%%) [highest difference: %0.4f dbs]\n",
			 FM_amplitudes, FMcompared, (double)FM_amplitudes/(double)FMcompared*100, FMhighDiff);

		logmsg("\n\tAdjustments made to match within defined ranges: %d total\n",
			FMadjHz+FMadjAmplitudes);
		//if(config->HzDiff != 0.0)
		logmsg("\t\tFrequencies adjusted: %d of %d (%0.2f%%) [highest difference: %0.4f Hz]\n",
				FMadjHz, FMcompared, (double)FMadjHz/(double)FMcompared*100, FMhighHz);
		logmsg("\t\tAmplitudes adjusted: %d of %d (%0.2f%%) [highest difference: %0.4f dbs]\n",
			FMadjAmplitudes, FMcompared, (double)FMadjAmplitudes/(double)FMcompared*100, FMhighDiffAdj);

		logmsg("PSG Sound\n");
		logmsg("\tPSG differences: %d\n",
			PSGnotfound+PSG_Amplitudes);
		logmsg("\t\tNot found: %d of %d (%0.2f%%)\n", 
			PSGnotfound, PSGcompared, (double)PSGnotfound/(double)PSGcompared*100);
		logmsg("\t\tDifferent Amplitudes: %d of %d (%0.2f%%) [highest difference: %0.4f]\n",
			PSG_Amplitudes, PSGcompared, (double)PSG_Amplitudes/(double)PSGcompared*100, PSGhighDiff);
		logmsg("\n\tAdjustments made to match within defined ranges: %d total\n",
		PSGadjHz+PSGadjAmplitudes);
		//if(config->HzDiff != 0.0)
		logmsg("\t\tFrequencies adjusted: %d of %d (%0.2f%%) [highest difference: %0.4f Hz]\n",
			PSGadjHz, PSGcompared, (double)PSGadjHz/(double)PSGcompared*100, PSGhighHz);
		logmsg("\t\tAmplitudes adjusted: %d of %d (%0.2f%%) [highest difference: %0.4f]\n",
			PSGadjAmplitudes, PSGcompared, (double)PSGadjAmplitudes/(double)PSGcompared*100, PSGhighDiffAdj);
	}
	else
		logmsg("Reference file has no frequencies at all!\n");

	if(HadCRTNoise)
		logmsg("\nReference Signal has CRT noise (15697 hz)\n");
	logmsg("\n");

	if(message.message)
		free(message.message);
	return (double)totalDiff*100.0/(double)(PSGcompared+FMcompared);
}


void PrintComparedNotes(MaxFreq *ReferenceArray, MaxFreq *ComparedArray, parameters *config, GenesisAudio *Signal)
{
	if(IsLogEnabled())
		DisableConsole();

	// changed Magnitude->amplitude
	for(int j = 0; j < config->MaxFreq; j++)
	{
		if(ReferenceArray->freq[j].hertz)
		{
			int match = 0;

			logmsg("[%4.2d] Ref: %5g Hz\t%0.4fdb [>%3d]", 
						j,
						ReferenceArray->freq[j].hertz,
						ReferenceArray->freq[j].amplitude,
						ReferenceArray->freq[j].matched - 1);

			if(ComparedArray->freq[j].hertz)
				logmsg("\tComp:\t%5g Hz\t%0.4fdb [<%3d]", 
						ComparedArray->freq[j].hertz,
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
				ReferenceArray->freq[j].amplitude != 
				ComparedArray->freq[match].amplitude)
					logmsg("W");
			logmsg("\n");
			// Use Silence as reference for floor Noise and amplitude cutoff
			//if(Signal->hasFloor && ReferenceArray->freq[j].amplitude <= Signal->floorAmplitude[Signal->floorCount-1])
				//break;
		}
	}
	logmsg("\n\n");

	if(IsLogEnabled())
		EnableConsole();
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

	while ((c = getopt (argc, argv, "hejvkgimlxyw:n:d:a:t:r:c:f:b:s:z:")) != -1)
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
	  case 'i':
		config->ignoreFloor = 0;
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
		EnableConsole();
		break;
	  case 'x':
		config->spreadsheet = 1;
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
		if(config->tolerance < 0.0 || config->tolerance > 20.0)
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
		else if (optopt == 't')
		  logmsg("Amplitude tolerance -%c requires an argument: 0.0-100.0\n", optopt);
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

	if(config->endHz <= config->startHz)
	{
		logmsg("Invalid frequency range for FFTW (%d Hz to %d Hz)\n", config->startHz, config->endHz);
		return 0;
	}

	if(IsLogEnabled())
	{
		int len;
		
		sprintf(log_file, "%s", basename(config->referenceFile));
		len = strlen(log_file);
		sprintf(log_file+len-4, "_vs_%s", basename(config->targetFile));
		len = strlen(log_file);
		sprintf(log_file+len-4, ".txt");
		remove(log_file);

		logfile = fopen(log_file, "a");
		if(!logfile)
		{
			printf("Could not create log file %s\n", log_file);
			return 0;
		}

		printf("\tLog enabled to file: %s\n", log_file);
		DisableConsole();
		Header(1);
		EnableConsole();
	}

	logmsg("\tAmplitude tolerance while comparing is %0.2f dbs\n", config->tolerance);
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
	logmsg("	 -t: Defines the <t>olerance when comparing amplitudes in dbs\n");
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
