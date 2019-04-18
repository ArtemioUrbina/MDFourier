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

#include "mdfourier.h"
#include "log.h"
#include "cline.h"
#include "windows.h"
#include "freq.h"

int LoadFile(FILE *file, GenesisAudio *Signal, parameters *config, char *fileName);
double ProcessSamples(MaxFreq *MaxFreqArray, short *samples, size_t size, long samplerate, float *window, parameters *config);
double CompareNotes(GenesisAudio *ReferenceSignal, GenesisAudio *TestSignal, parameters *config);

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
		logmsg("Spreadsheet, %s, %s, %g, %g\n",
				 basename(ReferenceSignal->SourceFile), basename(TestSignal->SourceFile),
				result1, result2);

	free(ReferenceSignal);
	free(TestSignal);
	ReferenceSignal = NULL;
	TestSignal = NULL;

	if(IsLogEnabled())
	{
		endLog();
		printf("Check logfile for extended results\n");
	}
	
	return(0);
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

#ifdef SAVE_CHUNKS
		if(1)
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
#endif

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

	if(!config->ignoreFloor && Signal->hasFloor) // analyze silence floor if available
		FindFloor(Signal, config);

	fclose(file);
	free(buffer);
	free(AllSamples);

	freeWindows(&windows);

	if(config->verbose)
		PrintFrequencies(Signal, config);

	return i;
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
					}
					else
					{
						MaxFreqArray->freq[j].magnitude += MaxFreqArray->freq[i].magnitude;
						//MaxFreqArray->freq[j].magnitude/= 2;

						MaxFreqArray->freq[i].hertz = 0;
						MaxFreqArray->freq[i].magnitude = 0;
						MaxFreqArray->freq[i].phase = 0;
						MaxFreqArray->freq[i].amplitude = 0;
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
			if(MaxFreqArray->freq[i].hertz)
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

int CalculateMaxCompare(int note, msgbuff *message, GenesisAudio *Signal, parameters *config)
{
	int count = config->MaxFreq;

	// find how many to compare
	if(!config->ignoreFloor && Signal->hasFloor)
	{
		for(int freq = 0; freq < config->MaxFreq; freq ++)
		{
			double difference = 0;

			if(!Signal->Notes[note].freq[freq].hertz)
			{
				count = freq;
				return count;
			}

			if(Signal->Notes[note].freq[freq].amplitude <= Signal->floorAmplitude)
			{
				count = freq;
				return count;
			}

			difference = fabs(fabs(Signal->floorAmplitude) - fabs(Signal->Notes[note].freq[freq].amplitude));
			if((Signal->Notes[note].freq[freq].hertz == Signal->floorFreq &&
				difference <= config->tolerance))  // this in dbs
			{
				count = freq;
				return count;
			}
		}
	}
	else
	{
		for(int freq = 0; freq < config->MaxFreq; freq++)
		{
			if(!Signal->Notes[note].freq[freq].hertz)
			{
				count = freq;
				return count;
			}
		}
	}

	return count;
}

double CalculateWeightedError(double pError, parameters *config)
{
	int option = 0;

	option = config->outputFilterFunction;
	switch(option)
	{
		case 1:
			// The integral of x^1 in the 0-1 Range is 1/2
			pError *= 2.0;  // compensate for an error of 50%
			break;
		case 2:
			// Map to Beta function
			pError = incbeta(8.0, 8.0, pError);
			// Compensate for non linear deviation
			// The integral of Beta above is 1/2
			pError *= 2.0;
			break;
		case 3:
			// Map to Beta function
			pError = incbeta(3.0, 1.0, pError);
			// Compensate for non linear deviation
			// The integral of Beta above in the 0-1 Range is 1/4
			pError *= 4.0;
			break;
		case 4:
			// Map to Beta function
			pError = incbeta(5.0, 0.5, pError);
			// Compensate for non linear deviation
			pError *= 10.99;
			break;

		case 5:
			// Map to Beta function
			pError = incbeta(1.0, 3.0, pError);
			// Compensate for non linear deviation
			// The integral of Beta above in the 0-1 Range is 3/4
			pError *= 1.33333333;
			break;
		case 6:
			// Map to Beta function
			pError = incbeta(0.5, 6, pError);
			// Compensate for non linear deviation
			pError *= 1.0831;
			break;
		default:
			// This is unexpected behaviour, log it
			logmsg("CalculateWeightedError, out of range value %d\n", option);
			pError = 1;
			break;
	}

	return pError;
}

double CompareNotes(GenesisAudio *ReferenceSignal, GenesisAudio *TestSignal, parameters *config)
{
	int			note = 0;
	long int 	msg = 0, HadCRTNoise = 0;
	double		totalDiff = 0, totalError = 0;
	long int	FM_amplitudes = 0, FMnotfound = 0;
	long int	FMadjAmplitudes = 0, FMadjHz = 0;
	long int 	FMcompared = 0, PSGcompared = 0;
	double		FMhighDiffAdj = 0, FMhighDiff = 0, FMhighHz = 0;
	long int	PSG_Amplitudes = 0, PSGnotfound = 0;
	long int	PSGadjAmplitudes = 0, PSGadjHz = 0;
	double		PSGhighDiffAdj = 0, PSGhighDiff = 0, PSGhighHz = 0;
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

		for(long int freq = 0; freq < refSize; freq++)
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

				for(long int comp = 0; comp < testSize; comp++)
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
					for(long int comp = 0; comp < testSize; comp++)
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
						sprintf(message.buffer, "\tDifferent Amplitude: %g Hz at %0.4fdbs instead of %g Hz at %0.2fdbs {%g}\n",
							TestSignal->Notes[note].freq[index].hertz,
							TestSignal->Notes[note].freq[index].amplitude,
							ReferenceSignal->Notes[note].freq[freq].hertz,
							ReferenceSignal->Notes[note].freq[freq].amplitude,
							test);	
						InsertMessageInBuffer(&message, config);
						msg++;

						if(config->useOutputFilter)
						{
							double value = 0;
							
							// we get the proportional linear error in range 0-1
							//value = ReferenceSignal->Notes[note].freq[freq].magnitude/100.0;
							value = 1.0 - (double)freq/(double)config->MaxFreq;
							totalDiff += CalculateWeightedError(value, config);
							/*
							sprintf(message.buffer, "Amplitude [%ld/%d] Value: %g Calculated: %g Accum: %g\n",
								freq, config->MaxFreq, value, CalculateWeightedError(value, config), totalDiff);
							InsertMessageInBuffer(&message, config);	
							*/

							// Keep values to compensate for non linear deviation
							totalError ++;
						}
						else
							totalDiff ++;

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
					sprintf(message.buffer, "\tReference Frequency not found: %g Hz at %5.4f db (index: %ld)\n",
							ReferenceSignal->Notes[note].freq[freq].hertz,
							ReferenceSignal->Notes[note].freq[freq].amplitude,
							freq);
					InsertMessageInBuffer(&message, config);
					msg ++;

					if(config->useOutputFilter)
					{
						double value = 0;
						
						// we get the proportional linear error in range 0-1
						value = 1.0 - (double)freq/(double)config->MaxFreq;

						totalDiff += CalculateWeightedError(value, config);
						/*
						sprintf(message.buffer, "Frequency [%ld/%d] Value: %g Calculated: %g Accum: %g\n",
								freq, config->MaxFreq, value, CalculateWeightedError(value, config), totalDiff);
						InsertMessageInBuffer(&message, config);
						*/
						// Keep values to compensate for non linear deviation
						totalError ++;
					}
					else
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
			if(IsLogEnabled())
				DisableConsole();
			logmsg("%s\n", message.message);
			if(config->extendedResults)
			{
				logmsg("Unmatched Note Report for %s# %ld (%ld)\n", GetRange(note), GetSubIndex(note), note);
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
				logmsg("Matched Note Report for %s# %ld (%ld)\n", GetRange(note), GetSubIndex(note), note);
				PrintComparedNotes(&ReferenceSignal->Notes[note], &TestSignal->Notes[note], 
					config, ReferenceSignal);
			}
			if(IsLogEnabled())
				EnableConsole();
		}
	}

	if(config->useOutputFilter)
	{
		// Compensate error for data deviation with x^2*3.0 or x*2.0
		//logmsg("Real Diff %g\n", totalDiff);
		if(totalDiff > totalError)
			totalDiff = totalError;
	}

	logmsg("============================================================\n");
	logmsg("Reference: %s\nCompared to: %s\n", ReferenceSignal->SourceFile, TestSignal->SourceFile);
	if(FMcompared+PSGcompared)
	{
		logmsg("Total differences are %g out of %ld [%3.2f%% different]\n============================================================\n",
				 round(totalDiff), FMcompared+PSGcompared, (double)totalDiff*100.0/(double)(PSGcompared+FMcompared));
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
		logmsg("\tFM differences: %ld\n",
			FMnotfound+FM_amplitudes);
		logmsg("\t\tNot found: %ld of %ld (%0.2f%%)\n", 
			FMnotfound, FMcompared, (double)FMnotfound/(double)FMcompared*100);
		logmsg("\t\tDifferent Amplitudes: %ld of %ld (%0.2f%%) [highest difference: %0.4f dbs]\n",
			 FM_amplitudes, FMcompared, (double)FM_amplitudes/(double)FMcompared*100, FMhighDiff);

		logmsg("\n\tAdjustments made to match within defined ranges: %ld total\n",
			FMadjHz+FMadjAmplitudes);
		//if(config->HzDiff != 0.0)
		logmsg("\t\tFrequencies adjusted: %ld of %ld (%0.2f%%) [highest difference: %0.4f Hz]\n",
				FMadjHz, FMcompared, (double)FMadjHz/(double)FMcompared*100, FMhighHz);
		logmsg("\t\tAmplitudes adjusted: %ld of %ld (%0.2f%%) [highest difference: %0.4f dbs]\n",
			FMadjAmplitudes, FMcompared, (double)FMadjAmplitudes/(double)FMcompared*100, FMhighDiffAdj);

		logmsg("PSG Sound\n");
		logmsg("\tPSG differences: %ld\n",
			PSGnotfound+PSG_Amplitudes);
		logmsg("\t\tNot found: %ld of %ld (%0.2f%%)\n", 
			PSGnotfound, PSGcompared, (double)PSGnotfound/(double)PSGcompared*100);
		logmsg("\t\tDifferent Amplitudes: %ld of %ld (%0.2f%%) [highest difference: %0.4f]\n",
			PSG_Amplitudes, PSGcompared, (double)PSG_Amplitudes/(double)PSGcompared*100, PSGhighDiff);
		logmsg("\n\tAdjustments made to match within defined ranges: %ld total\n",
		PSGadjHz+PSGadjAmplitudes);
		//if(config->HzDiff != 0.0)
		logmsg("\t\tFrequencies adjusted: %ld of %ld (%0.2f%%) [highest difference: %0.4f Hz]\n",
			PSGadjHz, PSGcompared, (double)PSGadjHz/(double)PSGcompared*100, PSGhighHz);
		logmsg("\t\tAmplitudes adjusted: %ld of %ld (%0.2f%%) [highest difference: %0.4f dbs]\n",
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
