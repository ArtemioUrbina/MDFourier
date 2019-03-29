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

#define PSG_COUNT 42

#define COUNT 100		// Number of frequencies to account for 
#define MAX_NOTES 100	// we have 100 notes in the 240p Test Suite

#define TYPE_NONE	0
#define TYPE_FM 	1
#define TYPE_PSG	2

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
void CompareNotes(GenesisAudio *ModelSignal, GenesisAudio *TestSignal, double sigMatch, double tolerance, int extend);
int LoadFile(FILE *file, GenesisAudio *Signal, fftw_plan *p);
void PrintComparedNotes(MaxFreq *ModelArray, MaxFreq *ComparedArray, double sigMatch);

int main(int argc , char *argv[])
{
	fftw_plan			p = NULL;
	FILE				*model = NULL;
	FILE				*compare = NULL;
	GenesisAudio  		ModelSignal;
	GenesisAudio  		TestSignal;
	double				sigMatch = FREQ_COMPARE;
	double				tolerance = PERCENT_TOLERANCE;
	int 				extend = 1;

	printf("Sega Genesis/Mega Drive Fourier Audio compare tool for 240p Test Suite\n");
	printf("by Artemio Urbina 2019, licensed under GPL\n");

	if(argc < 2)
	{
		logmsg("\tusage: comparegenesis [model.wav] [compare.wav]\n");
		return(1);
	}

	printf("\tMODEL file is %s\n", argv[1]);
	model = fopen(argv[1], "rb");
	if(!model)
	{
		logmsg("\tCould not open MODEL file: \"%s\"\n", argv[1]);
		return 0;
	}

	printf("\tCOMPARE file is %s\n", argv[2]);
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
		printf("\tWeight TOLERANCE changed to %0.2f%%\n", tolerance);
	}
	else
		printf("\tWeight TOLERANCE percentage to compare is %0.2f%%\n", tolerance);

	logmsg("\tLoading MODEL file %s\n", argv[1]);
	
	if(!LoadFile(model, &ModelSignal, &p))
		return 0;

	logmsg("\tLoading Compare file %s\n", argv[2]);
	if(!LoadFile(compare, &TestSignal, &p))
		return 0;

	CompareNotes(&ModelSignal, &TestSignal, sigMatch, tolerance, extend);

	if(p)
		fftw_destroy_plan(p);
	
	return(0);
}

int LoadFile(FILE *file, GenesisAudio *Signal, fftw_plan *p)
{
	int 	i = 0;
	int 	buffer_frames = SAMPLE_RATE;
	char	*buffer;
	size_t	buffersize = 0;

	buffersize = buffer_frames * 4 + 44; // plus header
	buffer = malloc(buffersize);
	if(!buffer)
	{
		logmsg("malloc failed\n");
		return(0);
	}

	fseek (file, 44, SEEK_SET ); // Skip wav header

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
			logmsg("==================== %s# %d ===================\n", i < PSG_COUNT ? "FM": "PSG", i < PSG_COUNT ? i : i - PSG_COUNT + 1);
#endif
		}
		else
		{
			logmsg("unexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
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


void CompareNotes(GenesisAudio *ModelSignal, GenesisAudio *TestSignal, double sigMatch, double tolerance, int extend)
{
	int 	note, msg = 0, total = 0;
	int 	weights = 0, notfound = 0;
	int 	adjusted = 0, adjWeight = 0, adjHz = 0;
	double	highDiffAdj = 0, highDiff = 0, highHz = 0;
	char	diff[4096];
	char	buffer[512];

	for(note = 0; note < MAX_NOTES; note++)
	{
		double percent = 0;
		int count = 0;

		msg = 0;
		sprintf(diff, "Note: %s# %d \n", ModelSignal->Notes[note].type == TYPE_FM ? "FM" : "PSG", ModelSignal->Notes[note].index);

		// Determine up to what frequency the 90% of the peaks from the signal are 
		for(int freq = 0; freq < COUNT; freq ++)
		{
			percent += ModelSignal->Notes[note].freq[freq].weight;
			if(percent >= sigMatch)
			{
				count = freq;
				break;
			}
		}

		for(int freq = 0; freq < count; freq++)
		{
			if(ModelSignal->Notes[note].freq[freq].hertz && !(ModelSignal->Notes[note].freq[freq].hertz > 15670 && ModelSignal->Notes[note].freq[freq].hertz < 15700))
			{
				int found = 0, index = 0, compSize = 0;

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
					if(ModelSignal->Notes[note].freq[freq].hertz == TestSignal->Notes[note].freq[comp].hertz)
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
	
						hertzDiff = fabs(TestSignal->Notes[note].freq[comp].hertz - ModelSignal->Notes[note].freq[freq].hertz);
						if(hertzDiff < HERTZ_DIFF)
						{
							found = 2;
							index = comp;
							adjusted++;
							adjHz++;
							
							if(hertzDiff > highHz)
								highHz = hertzDiff;
							break;
						}
					}
				}

				if(found)  // Now in either case, compare weights
				{
					double test;
	
					test = fabs(TestSignal->Notes[note].freq[index].weight - ModelSignal->Notes[note].freq[freq].weight);
					if(test >= tolerance)
					{
						sprintf(buffer, "  Different Weight found: %g Hz at %.2f%% instead of %g Hz at %.2f%% (%0.2f)\n",
							TestSignal->Notes[note].freq[index].hertz,
							TestSignal->Notes[note].freq[index].weight,
							ModelSignal->Notes[note].freq[freq].hertz,
							ModelSignal->Notes[note].freq[freq].weight,
							test);	
						strcat(diff, buffer);
						msg ++;
						total ++;
						weights ++;

						if(test > highDiff)
							highDiff = test;
					}
		
					if(test && test < tolerance)
					{
						adjWeight++;
						adjusted++;
						if(test > highDiffAdj)
							highDiffAdj = test;
					}
				}

				if(!found)
				{
					sprintf(buffer, "  Model Frequency not found: %g Hz at %.2f%%\n",
							 ModelSignal->Notes[note].freq[freq].hertz,
							 ModelSignal->Notes[note].freq[freq].weight);
					strcat(diff, buffer);
					msg ++;
					total ++;
					notfound ++;
				}
			}
		}

		if(msg)
		{
			logmsg("%s\n", diff);	
			if(extend)
				PrintComparedNotes(&ModelSignal->Notes[note], &TestSignal->Notes[note], sigMatch);
		}
	}
	if(!total)
	{
		logmsg("WAV files are acustically identical\n");
		if(adjusted)
			logmsg("\tAdjusted to match with ranges: %d (Hz: %d [highest: %g Hz], W: %d [highest: %0.2f%%])\n", adjusted, adjHz, highHz, adjWeight, highDiffAdj);
	}
	else
		logmsg("Total differences are %d\n\tNot found: %d\n\tDifferent weights: %d [highest: %0.2f%%]\n\tAdjusted to match with ranges: %d (Hz: %d [highest: %g Hz], W: %d [highest: %0.2f%%])\n", 
			total, notfound, weights, highDiff, adjusted, adjHz, highHz, adjWeight, highDiffAdj);
}


void PrintComparedNotes(MaxFreq *ModelArray, MaxFreq *ComparedArray, double sigMatch)
{
	double total = 0;

	for(int j = 0; j < COUNT; j++)
	{
		if(ModelArray->freq[j].weight && ModelArray->freq[j].hertz)
		{
			total += ModelArray->freq[j].weight;
			logmsg("[%0.2d] (%0.2f%%) Model: %5g Hz\t%0.2f%% ", 
						j, total,
						ModelArray->freq[j].hertz,
						ModelArray->freq[j].weight);

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