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

#include <string.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>


#define SAMPLE_RATE 44100*2 //sice we are in stereo!

#define	FFT_OM			-5000
#define	FFT_NOT_FOUND	-500

#define BASE 1			// frequency width size
#define SIZE_BASE SAMPLE_RATE/BASE

#define PSG_COUNT	40
#define NOISE_COUNT 100

#define COUNT 100		// Number of frequencies to account for 
#define MAX_NOTES 140	// we have 140 notes in the 240p Test Suite

#define TYPE_NONE	0
#define TYPE_FM 	1
#define TYPE_PSG	2
#define TYPE_NOISE	3

// This is the minimum percentual value to consider for matches
#define MIN_PERCENT 		0.0f

// This is the percentual difference allowed between model and compared
// to be a match
#define PERCENT_TOLERANCE	5.0f

// +/- Tolerance in frequency Difference to be the same one
#define HERTZ_DIFF			10.0f

//Percentage of normalized weighted frequencies to match
#define FREQ_COMPARE		90.0f

typedef struct FrequencySt {
	double weight;
	double hertz;
	long   index;
} Frequency;

typedef struct MaxFreqSt {
	Frequency	freq[COUNT];
	int 		index;
	int 		type;
} MaxFreq;

typedef struct GenesisAudioSt {
	MaxFreq 	Notes[MAX_NOTES]; 
}  GenesisAudio;

void logmsg(char *fmt, ... );
double ProcessSamples(fftw_plan *p, MaxFreq *MaxFreqArray, short *samples, size_t size, long samplerate, double secondunits);
void CompareNotes(GenesisAudio *ReferenceSignal, GenesisAudio *TestSignal, double sigMatch, double tolerance, int extend);
int LoadFile(FILE *file, GenesisAudio *Signal, fftw_plan *p);
void PrintComparedNotes(MaxFreq *ReferenceArray, MaxFreq *ComparedArray, double sigMatch);
char *GetRange(int index);
int GetSubIndex(int index);


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

int main(int argc , char *argv[])
{
	fftw_plan			p = NULL;
	FILE				*model = NULL;
	FILE				*compare = NULL;
	GenesisAudio  		*ReferenceSignal;
	GenesisAudio  		*TestSignal;
	double				sigMatch = FREQ_COMPARE;
	double				tolerance = PERCENT_TOLERANCE;
	int 				extend = 1;

	printf("== MDFourier ==\nSega Genesis/Mega Drive Fourier Audio compare tool for 240p Test Suite\n");
	printf("by Artemio Urbina 2019, licensed under GPL\n\n");

	if(argc < 2)
	{
		logmsg("\tusage: mdfourier [reference.wav] [compare.wav]\n", argv[0]);
		return(1);
	}

	model = fopen(argv[1], "rb");
	if(!model)
	{
		logmsg("\tCould not open REFERENCE file: \"%s\"\n", argv[1]);
		return 0;
	}

	compare = fopen(argv[2], "rb");
	if(!compare)
	{
		fclose(model);
		logmsg("\tCould not open COMPARE file: \"%s\"\n", argv[2]);
		return 0;
	}

	if(argc >= 4)
	{
		sigMatch = atof(argv[3]);
		if(sigMatch < 0.0f || sigMatch > 100.0f)
			sigMatch = FREQ_COMPARE;
		printf("\tSignal Percentage match changed to %0.2f%%\n", sigMatch);
	}
	else
		printf("\tSignal Percentage match to compare is %0.2f%%\n", sigMatch);

	if(argc >= 5)
	{
		tolerance = atof(argv[4]);
		printf("\tWeight tolerance changed to %0.2f%%\n", tolerance);
	}
	else
		printf("\tWeight tolerance percentage to compare is %0.2f%%\n", tolerance);

	logmsg("\nLoading Reference audio file %s\n", argv[1]);
	
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

	if(!LoadFile(model, ReferenceSignal, &p))
	{
		free(ReferenceSignal);
		free(TestSignal);
		return 0;
	}

	logmsg("Loading Compare audio file %s\n", argv[2]);
	if(!LoadFile(compare, TestSignal, &p))
	{
		free(ReferenceSignal);
		free(TestSignal);
		return 0;
	}

	CompareNotes(ReferenceSignal, TestSignal, sigMatch, tolerance, extend);

	if(p)
		fftw_destroy_plan(p);

	free(ReferenceSignal);
	free(TestSignal);
	ReferenceSignal = NULL;
	TestSignal = NULL;
	
	return(0);
}

int LoadFile(FILE *file, GenesisAudio *Signal, fftw_plan *p)
{
	int 	i = 0;
	int 	buffer_frames = SAMPLE_RATE;
	char	*buffer;
	size_t	buffersize = 0;
	wav_hdr   header;

	buffersize = buffer_frames * 4 + 44; // plus header
	buffer = malloc(buffersize);
	if(!buffer)
	{
		logmsg("\tmalloc failed\n");
		return(0);
	}

	
	if(fread(&header, 1, sizeof(wav_hdr), file) != sizeof(wav_hdr))
	{
		logmsg("\tInvalid WAV file: File too small\n");
		return(0);
	}

	if(header.AudioFormat != 1) // Check for PCM
	{
		logmsg("\tInvalid WAV File: Only PCM is supported\n\tPlease use WAV PCM 16 bit 44100hz");
		return(0);
	}

	if(header.NumOfChan != 2) // Check for Stereo
	{
		logmsg("\tInvalid WAV file: Only Stereo supported\n");
		return(0);
	}

	if(header.SamplesPerSec != 44100) // Check for PCM
	{
		logmsg("\tInvalid WAV file: Only 44100 Hz supported for now\n\tPlease use WAV PCM 16 bit 44100hz");
		return(0);
	}

	if(header.bitsPerSample != 16) // Check for PCM
	{
		logmsg("\tInvalid WAV file: Only 16 bit supported for now\n\tPlease use WAV PCM 16 bit 44100hz");
		return(0);
	}

	memset(buffer, 0, buffersize);
	memset(Signal, 0, sizeof(GenesisAudio));

	while(i < MAX_NOTES)
	{
		if(i < PSG_COUNT)
			buffer_frames = SAMPLE_RATE;   //2 second blocks
		else
			buffer_frames = SAMPLE_RATE / 2; // 1 second block
		memset(buffer, 0, buffersize);
		if(fread(buffer, 4, buffer_frames, file) > 0)
		{
#ifdef VERBOSE
			logmsg("==================== %s# %d (%d)===================\n", GetRange(i), GetSubIndex(i), i);
#endif
		}
		else
		{
			logmsg("\tunexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
			return 0;
		}
		Signal->Notes[i].index = i < PSG_COUNT ? i : i - PSG_COUNT;
		Signal->Notes[i].type = i < PSG_COUNT ? TYPE_FM : TYPE_PSG;

		ProcessSamples(p, &Signal->Notes[i], (short*)buffer, buffer_frames, SAMPLE_RATE, BASE);
		i++;
	}
	fclose(file);
	free(buffer);
	return i;
}

void logmsg(char *fmt, ... )
{
	va_list arguments; 

	va_start(arguments, fmt);
	vprintf(fmt, arguments);
	va_end(arguments);

	return;
}

#ifdef ELAPSED
static double TimeSpecToSeconds(struct timespec* ts)
{
	return (double)ts->tv_sec + (double)ts->tv_nsec / 1000000000.0;
}
#endif

double ProcessSamples(fftw_plan *p, MaxFreq *MaxFreqArray, short *samples, size_t size, long samplerate, double secondunits)
{
	long		  	samplesize = 0, arraysize = 0;	
	long		  	i = 0, f = 0, framesizernd = 0; 
	double		  	in[2*(SIZE_BASE/2+1)], root = 0, framesize = 0;  
	double		 	boxsize = 0;
	fftw_complex  	out[SIZE_BASE/2+1];
#ifdef ELAPSED
	struct timespec start, end;
	double			elapsedSeconds;
#endif
	
	if(!MaxFreqArray)
	{
		logmsg("No Array for results\n");
		return 0;
	}

	samplesize = (long)size;  

	framesize = samplerate/secondunits;
	framesizernd = (long)framesize;  

#ifdef VERBOSE
	logmsg("Samples are at %lu Khz and %g seconds long.\n",
			samplerate/2, (double)samplesize/samplerate*2);	  
#endif

#ifdef ELAPSED
	clock_gettime(CLOCK_MONOTONIC, &start);
#endif

	arraysize = framesizernd;

	boxsize = (double)arraysize/(double)samplerate;  
	for(f = 0; f < samplesize/framesize; f++)
	{
		long	framestart = 0;
		long	removed = 0;
		double	TotalWeight = 0;

		framestart = framesize*f;

		if(!(*p))
			*p = fftw_plan_dft_r2c_1d(arraysize, in, out, FFTW_MEASURE);

		for(i = 0; i < framesizernd; i++)
			in[i] = (double)samples[i+framestart];

		fftw_execute(*p); 
	
		root = sqrt(arraysize);

		for(i = 0; i < COUNT; i++)
		{
			MaxFreqArray[f].freq[i].hertz = 0;
			MaxFreqArray[f].freq[i].weight = 0;
			MaxFreqArray[f].freq[i].index = -1;
		}

		//for(i = 0; i < arraysize/2+1; i++)
		// analyze only 0khz to 20khz
		// i/boxwsize = 0 to i/boxsize = 20000
		// 0*boxsize to 20000*boxsize
		for(i = 10*boxsize; i < 20000*boxsize; i++)  // Nyquist at 44.1khz
		{
			double r1 = creal(out[i]);
			double i1 = cimag(out[i]);
			double val, previous;
			int    j = 0;
	
			val = sqrt(sqrt(r1 * r1 + i1 * i1) / root);

			previous = 10000;
			for(j = 0; j < COUNT; j++)
			{
				if(val > MaxFreqArray[f].freq[j].weight && val < previous)
				{
					int k = 0;
					//Move the previous values down the array
					for(k = COUNT-1; k > j; k--)
						MaxFreqArray[f].freq[k] = MaxFreqArray[f].freq[k - 1];

					MaxFreqArray[f].freq[j].weight = val;
					MaxFreqArray[f].freq[j].index = i;
				}

				previous = MaxFreqArray[f].freq[j].weight;
			}
		}
		
		// Calculate Hz
		// Estimate TotalWeight for Normalization
		for(i = 0; i < COUNT; i++)
		{
			MaxFreqArray[f].freq[i].hertz = (double)((double)MaxFreqArray[f].freq[i].index/boxsize);
			TotalWeight += MaxFreqArray[f].freq[i].weight;
		}

		// Normalize to 100
		for(i = 0; i < COUNT; i++)
			MaxFreqArray[f].freq[i].weight = MaxFreqArray[f].freq[i].weight * 100.0f / TotalWeight;

		// The following mess compressed adjacent frequencies
		for(i = 0; i < COUNT; i++)
		{
			for(int j = 0; j < COUNT; j++)
			{
				double hertzDiff;
	
				hertzDiff = fabs(MaxFreqArray[f].freq[j].hertz - MaxFreqArray[f].freq[i].hertz);
				
				if(MaxFreqArray[f].freq[i].hertz >= HERTZ_DIFF && MaxFreqArray[f].freq[j].hertz >= HERTZ_DIFF 
					&& MaxFreqArray[f].freq[j].weight > 0 && i != j &&(hertzDiff <= HERTZ_DIFF*BASE))
				{
					if(MaxFreqArray[f].freq[i].weight > MaxFreqArray[f].freq[j].weight)
					{
						MaxFreqArray[f].freq[i].weight += MaxFreqArray[f].freq[j].weight;
	
						MaxFreqArray[f].freq[j].hertz = 0;
						MaxFreqArray[f].freq[j].weight = 0;
						MaxFreqArray[f].freq[j].index = 0;
					}
					else
					{
						MaxFreqArray[f].freq[j].weight += MaxFreqArray[f].freq[i].weight;
	
						MaxFreqArray[f].freq[i].hertz = 0;
						MaxFreqArray[f].freq[i].weight = 0;
						MaxFreqArray[f].freq[i].index = 0;
					}
					if(i > j)
					{
						Frequency	t;
					
						t = MaxFreqArray[f].freq[j];
						MaxFreqArray[f].freq[j] = MaxFreqArray[f].freq[i];
						MaxFreqArray[f].freq[i] = t;
					}
					removed ++;
				}
			}
		}

		// sort array by weight
		for(i = 0; i < COUNT; i++)
		{
			for(int j = i + 1; j < COUNT; j++)
			{
				if (MaxFreqArray[f].freq[i].weight < MaxFreqArray[f].freq[j].weight) 
				{
						Frequency	t;
					
						t = MaxFreqArray[f].freq[i];
						MaxFreqArray[f].freq[i] = MaxFreqArray[f].freq[j];
						MaxFreqArray[f].freq[j] = t;
					}
			}
		}
	}

	// Print results 
#ifdef VERBOSE
	for(f = 0; f < samplesize/framesize; f++)
	{
		int j = 0;
		double Percentage = 0;

		for(j = 0; j < COUNT; j++)
		{
			if(MaxFreqArray[f].freq[j].weight && MaxFreqArray[f].freq[j].hertz)
			{
				Percentage += MaxFreqArray[f].freq[j].weight;
				//detect CRT frequency and abort after it
				if(MaxFreqArray[f].freq[j].hertz > 15670 && MaxFreqArray[f].freq[j].hertz < 15700)
					logmsg("Frequency [%0.2d] with %0.2f%% (%2.2f%%):\t%g Hz *** CRT Noise ***\n", j, MaxFreqArray[f].freq[j].weight, Percentage, MaxFreqArray[f].freq[j].hertz);
				else
					logmsg("Frequency [%0.2d] with %0.2f%% (%2.2f%%):\t%g Hz\n", j, MaxFreqArray[f].freq[j].weight, Percentage, MaxFreqArray[f].freq[j].hertz);
			}
		}
	}
#endif
	
#ifdef ELAPSED
	clock_gettime(CLOCK_MONOTONIC, &end);
	elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
	logmsg("Processing WAV took %f\n", elapsedSeconds);
#endif

	return(0);
}


void CompareNotes(GenesisAudio *ReferenceSignal, GenesisAudio *TestSignal, double sigMatch, double tolerance, int extend)
{
	int 	note, msg = 0, total = 0, HadCRTNoise = 0;
	int 	FMweights = 0, FMnotfound = 0;
	int 	FMadjWeight = 0, FMadjHz = 0;
	int 	FMcompared = 0, PSGcompared = 0;
	double	FMhighDiffAdj = 0, FMhighDiff = 0, FMhighHz = 0;
	int 	PSGweights = 0, PSGnotfound = 0;
	int 	PSGadjWeight = 0, PSGadjHz = 0;
	double	PSGhighDiffAdj = 0, PSGhighDiff = 0, PSGhighHz = 0;
	char	diff[4096];
	char	buffer[512];

	for(note = 0; note < MAX_NOTES; note++)
	{
		double percent = 0;
		int count = 0;

		msg = 0;
		sprintf(diff, "Note: %s# %d (%d)\n", GetRange(note), GetSubIndex(note), note);

		// Determine up to what frequency the 90% of the peaks from the signal are 
		for(int freq = 0; freq < COUNT; freq ++)
		{
			percent += ReferenceSignal->Notes[note].freq[freq].weight;
			if(percent >= sigMatch)
			{
				count = freq;
				break;
			}
		}

		for(int freq = 0; freq < count; freq++)
		{
			int CRTNoise;
				
			CRTNoise = (ReferenceSignal->Notes[note].freq[freq].hertz > 15670 && ReferenceSignal->Notes[note].freq[freq].hertz < 15700);
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
				for(int comp = 0; comp < COUNT; comp++)
				{
					if(TestSignal->Notes[note].freq[comp].hertz)
						compSize ++;
					else
						break;
				}

				for(int comp = 0; comp < compSize; comp++)
				{
					if(ReferenceSignal->Notes[note].freq[freq].hertz == TestSignal->Notes[note].freq[comp].hertz)
					{
						found = 1;
						index = comp;
						break;
					}
				}

				if(!found) // search with tolerance, if done in one pass, false positives emerge
				{
					for(int comp = 0; comp < compSize; comp++)
					{
						double hertzDiff;
	
						hertzDiff = fabs(TestSignal->Notes[note].freq[comp].hertz - ReferenceSignal->Notes[note].freq[freq].hertz);
						if(hertzDiff < HERTZ_DIFF)
						{
							found = 2;
							index = comp;

							if(ReferenceSignal->Notes[note].type == TYPE_FM)
							{
								FMadjHz++;
								
								if(hertzDiff > FMhighHz)
									FMhighHz = hertzDiff;
							}
							else
							{
								PSGadjHz++;
								
								if(hertzDiff > PSGhighHz)
									PSGhighHz = hertzDiff;
							}
							break;
						}
					}
				}

				if(found)  // Now in either case, compare weights
				{
					double test;
	
					test = fabs(TestSignal->Notes[note].freq[index].weight - ReferenceSignal->Notes[note].freq[freq].weight);
					if(test >= tolerance)
					{
						sprintf(buffer, "\tDifferent Weight found: %g Hz at %.2f%% instead of %g Hz at %.2f%% (%0.2f)\n",
							TestSignal->Notes[note].freq[index].hertz,
							TestSignal->Notes[note].freq[index].weight,
							ReferenceSignal->Notes[note].freq[freq].hertz,
							ReferenceSignal->Notes[note].freq[freq].weight,
							test);	
						strcat(diff, buffer);
						msg ++;
						total ++;

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
		
					if(test && test < tolerance)
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
					sprintf(buffer, "\tReference Frequency not found: %g Hz at %.2f%%\n",
							 ReferenceSignal->Notes[note].freq[freq].hertz,
							 ReferenceSignal->Notes[note].freq[freq].weight);
					strcat(diff, buffer);
					msg ++;
					total ++;

					if(ReferenceSignal->Notes[note].type == TYPE_FM)
						FMnotfound ++;
					else
						PSGnotfound ++;
				}
			}
		}

		if(msg)
		{
			logmsg("%s\n", diff);	
			if(extend)
				PrintComparedNotes(&ReferenceSignal->Notes[note], &TestSignal->Notes[note], sigMatch);
		}
	}
	if(!total)
	{
		logmsg("\n==WAV files are acoustically identical==\n");
		if(FMadjHz+FMadjWeight)
		{
			logmsg("FM Sound\n");
			logmsg("\tAdjustments made to match within defined ranges: %d\n",
				FMadjHz+FMadjWeight);
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
			logmsg("\t\tFrequencies adjusted: %d of %d (%0.2f%%) [highest difference: %0.2f Hz]\n",
				PSGadjHz, PSGcompared, (double)PSGadjHz/(double)PSGcompared*100, PSGhighHz);
            logmsg("\t\tWeights adjusted: %d of %d (%0.2f%%) [highest difference: %0.2f%%]\n",
				PSGadjWeight, PSGcompared, (double)PSGadjWeight/(double)PSGcompared*100, PSGhighDiffAdj);
        }
	}
	else
	{
		logmsg("Total differences are %d\n========================\n", total);

		if(FMnotfound+FMweights)
		{
			logmsg("\nFM differences %d\n",
				FMnotfound+FMweights);
			logmsg("\tNot found: %d of %d (%0.2f%%)\n", 
                FMnotfound, FMcompared, (double)FMnotfound/(double)FMcompared*100);
			logmsg("\tDifferent weights: %d of %d (%0.2f%%) [highest: %0.2f%%]\n",
                 FMweights, FMcompared, (double)FMweights/(double)FMcompared*100, FMhighDiff);
            logmsg("\n\tMatched frequencies analysis:\n");
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
			logmsg("\tFrequencies adjusted: %d of %d (%0.2f%%) [highest difference: %0.2f Hz]\n",
				PSGadjHz, PSGcompared, (double)PSGadjHz/(double)PSGcompared*100, PSGhighHz);
            logmsg("\tWeights adjusted: %d of %d (%0.2f%%) [highest difference: %0.2f%%]\n",
				PSGadjWeight, PSGcompared, (double)PSGadjWeight/(double)PSGcompared*100, PSGhighDiffAdj);
		}
	}
	if(HadCRTNoise)
		logmsg("Reference Signal had CRT noise at 15670 hz\n");
}


void PrintComparedNotes(MaxFreq *ReferenceArray, MaxFreq *ComparedArray, double sigMatch)
{
	double total = 0;

	for(int j = 0; j < COUNT; j++)
	{
		if(ReferenceArray->freq[j].weight && ReferenceArray->freq[j].hertz)
		{
			total += ReferenceArray->freq[j].weight;
			logmsg("[%0.2d] (%0.2f%%) Reference: %5g Hz\t%0.2f%% ", 
						j, total,
						ReferenceArray->freq[j].hertz,
						ReferenceArray->freq[j].weight);

			if(ComparedArray->freq[j].hertz)
				logmsg("\tCompared:\t%5g Hz\t%0.2f%%", 
						ComparedArray->freq[j].hertz,
						ComparedArray->freq[j].weight);
			else
				logmsg("\tCompared:\t=====");
			logmsg("\n");
			if(total > sigMatch)
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
