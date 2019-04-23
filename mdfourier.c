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

#include "mdfourier.h"
#include "log.h"
#include "cline.h"
#include "windows.h"
#include "freq.h"
#include "diff.h"
#include "plot.h"

int LoadFile(FILE *file, AudioSignal *Signal, parameters *config, char *fileName);
double ExecuteDFFT(AudioBlocks *AudioArray, short *samples, size_t size, long samplerate, float *window, parameters *config);
double CompareAudioBlocks(AudioSignal *ReferenceSignal, AudioSignal *TestSignal, parameters *config);

int main(int argc , char *argv[])
{
	FILE				*reference = NULL;
	FILE				*compare = NULL;
	AudioSignal  		*ReferenceSignal;
	AudioSignal  		*TestSignal;
	parameters			config;
	double				result1 = 0, result2 = 0;

	Header(0);
	if(!commandline(argc, argv, &config))
	{
		printf("	 -h: Shows command line help\n");
		return 1;
	}

	if(!LoadAudioBlockStructure(&config))
		return 1;

	if(strcmp(config.referenceFile, config.targetFile) == 0)
	{
		logmsg("Both inputs are the same file %s, skipping to save time\n",
			 config.referenceFile);
		return 1;
	}	

	reference = fopen(config.referenceFile, "rb");
	if(!reference)
	{
		logmsg("\tCould not open REFERENCE file: \"%s\"\n", config.referenceFile);
		return 1;
	}

	compare = fopen(config.targetFile, "rb");
	if(!compare)
	{
		fclose(reference);
		logmsg("\tCould not open COMPARE file: \"%s\"\n", config.targetFile);
		return 1;
	}
	
	ReferenceSignal = CreateAudioSignal(&config);
	if(!ReferenceSignal)
		return 1;

	TestSignal = CreateAudioSignal(&config);
	if(!TestSignal)
	{
		free(ReferenceSignal);
		return 1;
	}

	logmsg("\nLoading Reference audio file %s\n", config.referenceFile);
	if(!LoadFile(reference, ReferenceSignal, &config, config.referenceFile))
	{
		free(ReferenceSignal);
		free(TestSignal);
		return 1;
	}

	logmsg("Loading Compare audio file %s\n", config.targetFile);
	if(!LoadFile(compare, TestSignal, &config, config.targetFile))
	{
		free(ReferenceSignal);
		free(TestSignal);
		return 1;
	}

	result1 = CompareAudioBlocks(ReferenceSignal, TestSignal, &config);
	if(config.normalize == 'r')
		config.relativeMaxMagnitude = 0.0;
	CleanMatched(ReferenceSignal, TestSignal, &config);
	ReleaseDifferenceArray(&config);
	InvertComparedName(&config);

	result2 = CompareAudioBlocks(TestSignal, ReferenceSignal, &config);
	ReleaseDifferenceArray(&config);

	if(config.spreadsheet)
		logmsg("Spreadsheet, %s, %s, %g, %g\n",
				 basename(ReferenceSignal->SourceFile), basename(TestSignal->SourceFile),
				result1, result2);

	ReleaseAudio(ReferenceSignal, &config);
	ReleaseAudio(TestSignal, &config);

	free(ReferenceSignal);
	free(TestSignal);
	ReferenceSignal = NULL;
	TestSignal = NULL;

	if(IsLogEnabled())
	{
		endLog();
		printf("\nCheck logfile for extended results\n");
	}

	ReleaseAudioBlockStructure(&config);
	
	return(0);
}

int LoadFile(FILE *file, AudioSignal *Signal, parameters *config, char *fileName)
{
	int 				i = 0;
	int 				loadedBlockSize = 0;
	char				*buffer;
	size_t			 	buffersize = 0;
	size_t			 	discardBytes = 0, discardSamples = 0;
	wav_hdr 			header;
	windowManager		windows;
	float				*windowUsed = NULL;
	struct	timespec	start, end;
	double				seconds = 0, longest = 0;
	char 				*AllSamples = NULL;
	long int			pos = 0;

	if(!file)
		return 0;

	if(fread(&header, 1, sizeof(wav_hdr), file) != sizeof(wav_hdr))
	{
		logmsg("\tInvalid WAV file: File too small\n");
		return(0);
	}

	if(header.AudioFormat != 1) /* Check for PCM */
	{
		logmsg("\tInvalid WAV File: Only PCM is supported\n\tPlease use WAV PCM 16 bit");
		return(0);
	}

	if(header.NumOfChan != 2) /* Check for Stereo */
	{
		logmsg("\tInvalid WAV file: Only Stereo supported\n");
		return(0);
	}

	if(header.bitsPerSample != 16) /* Check bit depth */
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
	if(seconds < GetTotalBlockDuration(config))
		logmsg("File length is smaller than expected\n");

	logmsg("WAV file is PCM %dhz %dbits and %g seconds long\n", 
		header.SamplesPerSec, header.bitsPerSample, seconds);

	/* We need to convert buffersize to the 16.688ms per frame by the Genesis */
	/* Mega Drive is 1.00128, now loaded fomr file */
	discardSamples = (size_t)round(GetFramerateAdjust(config)*header.SamplesPerSec);
	if(discardSamples % 2)
		discardSamples += 1;

	discardSamples -= header.SamplesPerSec;
	longest = GetLongestBlock(config);
	if(!longest)
	{
		logmsg("Block definitions are invalid, total length is 0\n");
		return 0;
	}

	buffersize = header.SamplesPerSec*4*sizeof(char)*(int)longest; /* 2 bytes per sample, stereo */
	buffer = (char*)malloc(buffersize);
	if(!buffer)
	{
		logmsg("\tmalloc failed\n");
		return(0);
	}

	if(!initWindows(&windows, header.SamplesPerSec, config))
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

	if(GetSilenceIndex(config) != NO_INDEX)
		Signal->hasFloor = 1;

	sprintf(Signal->SourceFile, "%s", fileName);
	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	while(i < config->types.totalChunks)
	{
		double duration = 0;

		duration = GetBlockDuration(config, i);
		if(config->window != 'n')
			windowUsed = getWindowByLength(&windows, duration);
		
		loadedBlockSize = header.SamplesPerSec*4*sizeof(char)*(int)duration;
		discardBytes = discardSamples * 4 * duration;

		memset(buffer, 0, buffersize);
		if(pos + loadedBlockSize > header.Subchunk2Size)
		{
			logmsg("\tunexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
			break;
		}
		memcpy(buffer, AllSamples + pos, loadedBlockSize);
		pos += loadedBlockSize;

		Signal->Blocks[i].index = GetBlockSubIndex(config, i);
		Signal->Blocks[i].type = GetBlockType(config, i);

#ifdef SAVE_CHUNKS
		if(1)
		{
			FILE 		*chunk = NULL;
			wav_hdr		cheader;
			char		Name[2048], FName[4096];

			cheader = header;
			sprintf(Name, "%03d_Source_chunk_%s", i, basename(fileName));
			ComposeFileName(FName, Name, "", config);
			chunk = fopen(FName, "wb");
			if(!chunk)
			{
				logmsg("\tCould not open chunk file %s\n", FName);
				return 0;
			}

			cheader.ChunkSize = loadedBlockSize+36;
			cheader.Subchunk2Size = loadedBlockSize;
			if(fwrite(&cheader, 1, sizeof(wav_hdr), chunk) != sizeof(wav_hdr))
			{
				logmsg("\tCould not write chunk header\n");
				return(0);
			}
		
			if(fwrite(buffer, 1, sizeof(char)*loadedBlockSize, chunk) != sizeof(char)*loadedBlockSize)
			{
				logmsg("\tCould not write samples to chunk file\n");
				return (0);
			}
		
			fclose(chunk);
		}
#endif

		ExecuteDFFT(&Signal->Blocks[i], (short*)buffer, loadedBlockSize/2, header.SamplesPerSec, windowUsed, config);

		FillFrequencyStructures(&Signal->Blocks[i], config);
	
		if(config->HzWidth > 0.0)
			CompressFrequencies(&Signal->Blocks[i], config);

		if(config->normalize == 'n')
			LocalNormalize(&Signal->Blocks[i], config);

		pos += discardBytes;  /* Advance to adjust the time for the Sega Genesis Frame Rate */
		i++;
	}

	if(config->clock)
	{
		double			elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg("Processing WAV took %f\n", elapsedSeconds);
	}

	/* Global Normalization by default */
	if(config->normalize != 'n')
		GlobalNormalize(Signal, config);

	if(!config->ignoreFloor && Signal->hasFloor) /* analyze silence floor if available */
		FindFloor(Signal, config);

	if(config->verbose)
		PrintFrequencies(Signal, config);

	fclose(file);
	free(buffer);
	free(AllSamples);

	freeWindows(&windows);

	return i;
}

double ExecuteDFFT(AudioBlocks *AudioArray, short *samples, size_t size, long samplerate, float *window, parameters *config)
{
	fftw_plan		p = NULL;
	long		  	stereoSignalSize = 0;	
	long		  	i = 0, monoSignalSize = 0; 
	double		  	*signal = NULL;
	fftw_complex  	*spectrum = NULL;
	double		 	seconds = 0;
	struct			timespec start, end;
	
	if(!AudioArray)
	{
		logmsg("No Array for results\n");
		return 0;
	}

	stereoSignalSize = (long)size;
	monoSignalSize = stereoSignalSize/2;	 /* 4 is 2 16 bit values */
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

	if(config->clockBlock)
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

	if(config->clockBlock)
	{
		double			elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg("Processing Audio Block took %f\n", elapsedSeconds);
	}

	free(signal);
	signal = NULL;

	AudioArray->fftwValues.spectrum = spectrum;
	AudioArray->fftwValues.size = monoSignalSize;
	AudioArray->fftwValues.seconds = seconds;

	return(0);
}

int CalculateMaxCompare(int block, AudioSignal *Signal, parameters *config, int limitRef)
{
	int count = config->MaxFreq;

	/* find how many to compare */
	if(!config->ignoreFloor && Signal->hasFloor)
	{
		for(int freq = 0; freq < config->MaxFreq; freq ++)
		{
			double difference = 0;

			if(!Signal->Blocks[block].freq[freq].hertz)
			{
				count = freq;
				return count;
			}

			if(Signal->Blocks[block].freq[freq].amplitude <= Signal->floorAmplitude)
			{
				count = freq;
				return count;
			}

			difference = fabs(fabs(Signal->floorAmplitude) - fabs(Signal->Blocks[block].freq[freq].amplitude));
			if((Signal->Blocks[block].freq[freq].hertz == Signal->floorFreq &&
				difference <= config->tolerance))  /* this in dbs */
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
			if(limitRef && Signal->Blocks[block].freq[freq].amplitude < config->significantVolume)
			{
				count = freq;
				return count;
			}

			if(!limitRef && Signal->Blocks[block].freq[freq].amplitude < config->significantVolume - config->tolerance*2)
			{
				count = freq;
				return count;
			}

			if(!Signal->Blocks[block].freq[freq].hertz)
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
		case 0:
			pError = 1;
			break;
		case 1:
			/* The integral of x^1 in the 0-1 Range is 1/2 */
			pError *= 2.0;  /* compensate for an error of 50% */
			break;
		case 2:
			/* Map to Beta function */
			pError = incbeta(8.0, 8.0, pError);
			/* Compensate for non linear deviation */
			/* The integral of Beta above is 1/2 */
			pError *= 2.0;
			break;
		case 3:
			/* Map to Beta function */
			pError = incbeta(3.0, 1.0, pError);
			/* Compensate for non linear deviation */
			/* The integral of Beta above in the 0-1 Range is 1/4 */
			pError *= 4.0;
			break;
		case 4:
			/* Map to Beta function */
			pError = incbeta(5.0, 0.5, pError);
			/* Compensate for non linear deviation */
			pError *= 10.99;
			break;

		case 5:
			/* Map to Beta function */
			pError = incbeta(1.0, 3.0, pError);
			/* Compensate for non linear deviation */
			/* The integral of Beta above in the 0-1 Range is 3/4 */
			pError *= 1.33333333;
			break;
		case 6:
			/* Map to Beta function */
			pError = incbeta(0.5, 6, pError);
			/* Compensate for non linear deviation */
			pError *= 1.0831;
			break;
		default:
			/* This is unexpected behaviour, log it */
			logmsg("CalculateWeightedError, out of range value %d\n", option);
			pError = 1;
			break;
	}

	return pError;
}

double CompareAudioBlocks(AudioSignal *ReferenceSignal, AudioSignal *TestSignal, parameters *config)
{
	int			block = 0;

	if(!CreateDifferenceArray(config))
		return 0;

	for(block = 0; block < config->types.regularChunks; block++)
	{
		int refSize = 0, testSize = 0;

		refSize = CalculateMaxCompare(block, ReferenceSignal, config, 1);
		testSize = CalculateMaxCompare(block, TestSignal, config, 0);

		for(int freq = 0; freq < refSize; freq++)
		{
			int found = 0, index = 0, type = 0;

			/* Ignore Silence blocks */
			type = GetBlockType(config, block);
			if(type == TYPE_SILENCE)
				continue;

			/* Ignore CRT noise */
			if(IsCRTNoise(ReferenceSignal->Blocks[block].freq[freq].hertz))
				continue;

			/* One compared item is frequency the other is amplitude */
			config->Differences.cntTotalCompared ++;
			for(int comp = 0; comp < testSize; comp++)
			{
				if(!TestSignal->Blocks[block].freq[comp].matched && 
					ReferenceSignal->Blocks[block].freq[freq].hertz ==
					TestSignal->Blocks[block].freq[comp].hertz)
				{
					TestSignal->Blocks[block].freq[comp].matched = freq + 1;
					ReferenceSignal->Blocks[block].freq[freq].matched = comp + 1;
					found = 1;
					index = comp;
					break;
				}
			}

#ifdef	HERTZ_TOLERANCE
			if(!found && config->HzDiff != 0.0) /* search with tolerance, if done in one pass, false positives emerge */
			{
				double	lowest = 22050;
				int 	lowIndex = -1;

				/* Find closest match */
				for(long int comp = 0; comp < testSize; comp++)
				{
					if(!TestSignal->Blocks[block].freq[comp].matched)
					{
						double hertzDiff;
	
						hertzDiff = fabs(TestSignal->Blocks[block].freq[comp].hertz -
										 ReferenceSignal->Blocks[block].freq[freq].hertz);

						if(hertzDiff <= config->HzDiff)
						{
							if(hertzDiff < lowest)
							{
								lowest = hertzDiff;
								lowIndex = comp;
							}
						}
					}
				}

				if(lowIndex >= 0)
				{
					TestSignal->Blocks[block].freq[lowIndex].matched = freq + 1;
					ReferenceSignal->Blocks[block].freq[freq].matched = lowIndex + 1;

					found = 2;
					index = lowIndex;

					/* Adjusted Frequency to tolerance */
				}
			}
#endif
			if(found)  /* Now in either case, compare amplitudes */
			{
				double test;

				test = fabs(fabs(TestSignal->Blocks[block].freq[index].amplitude) - fabs(ReferenceSignal->Blocks[block].freq[freq].amplitude));
				if(test > config->tolerance)
				{
					/* Difference in Amplitude */
					double value = 1;

					if(config->useOutputFilter)
					{
						double amplitude;

						amplitude = ReferenceSignal->Blocks[block].freq[freq].amplitude;

						// we get the proportional linear error in range 0-1
						if(amplitude >= config->significantVolume)
						{
							value = (fabs(config->significantVolume)-fabs(amplitude))/fabs(config->significantVolume);
							if(value)
								value = CalculateWeightedError(value, config);
						}
						else
							value = 0;
					}

					InsertAmplDifference(block, ReferenceSignal->Blocks[block].freq[freq].hertz, 
							ReferenceSignal->Blocks[block].freq[freq].amplitude,
							TestSignal->Blocks[block].freq[index].amplitude, value, config);
				}
	
				if(test && test <= config->tolerance)
				{
					/* Adjusted Amplitude to tolerance */
				}
			}

			if(!found)
			{
				/* Frequency Not Found */
				double value = 1;
						
				if(config->useOutputFilter)
				{
					double amplitude;

					amplitude = ReferenceSignal->Blocks[block].freq[freq].amplitude;

					// we get the proportional linear error in range 0-1
					if(amplitude >= config->significantVolume)
					{
						value = (fabs(config->significantVolume)-fabs(amplitude))/fabs(config->significantVolume);
						if(value)
							value = CalculateWeightedError(value, config);
					}
					else
						value = 0;
				}

				InsertFreqNotFound(block, ReferenceSignal->Blocks[block].freq[freq].hertz, 
					ReferenceSignal->Blocks[block].freq[freq].amplitude, value, config);
			}
		}

		if(config->Differences.BlockDiffArray[block].cntFreqBlkDiff && !config->justResults)
		{
			if(IsLogEnabled())
				DisableConsole();
			if(config->extendedResults)
			{
				logmsg("Unmatched Block Report for %s# %ld (%ld)\n", GetBlockName(config, block), GetBlockSubIndex(config, block), block);
				PrintComparedBlocks(&ReferenceSignal->Blocks[block], &TestSignal->Blocks[block],
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
				logmsg("Matched Block Report for %s# %ld (%ld)\n", GetBlockName(config, block), GetBlockSubIndex(config, block), block);
				PrintComparedBlocks(&ReferenceSignal->Blocks[block], &TestSignal->Blocks[block], 
					config, ReferenceSignal);
			}
			if(IsLogEnabled())
				EnableConsole();
		}
	}

	PlotAllDifferentAmplitudes(config->compareName, config);
	PlotAllSpectrogram(basename(ReferenceSignal->SourceFile), ReferenceSignal, config);

	//PlotAllMissingFrequencies(name, config);
	//PrintDifferenceArray(config);
	
	return 0;
}
