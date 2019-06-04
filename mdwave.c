/* 
 * MDWave
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

#define MDWAVE
#define MDWVERSION MDVERSION

#include "mdfourier.h"
#include "log.h"
#include "windows.h"
#include "freq.h"
#include "cline.h"
#include "sync.h"

int LoadFile(FILE *file, AudioSignal *Signal, parameters *config, char *fileName);
int ProcessFile(AudioSignal *Signal, parameters *config);
int ProcessSamples(AudioBlocks *AudioArray, int16_t *samples, size_t size, long samplerate, double *window, parameters *config, int reverse, AudioSignal *Signal);
int commandline_wave(int argc , char *argv[], parameters *config);
void PrintUsage_wave();
void Header_wave(int log);
void CleanUp(AudioSignal **ReferenceSignal, parameters *config);
void CloseFiles(FILE **ref);

int main(int argc , char *argv[])
{
	FILE				*reference = NULL;
	AudioSignal  		*ReferenceSignal = NULL;
	parameters			config;
	struct	timespec	start, end;

	Header_wave(0);
	if(!commandline_wave(argc, argv, &config))
	{
		printf("	 -h: Shows command line help\n");
		return 1;
	}

	if(config.clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	if(!LoadAudioBlockStructure(&config))
		return 1;

	reference = fopen(config.referenceFile, "rb");
	if(!reference)
	{
		logmsg("\tCould not open REFERENCE file: \"%s\"\n", config.referenceFile);
		CleanUp(&ReferenceSignal, &config);
		return 0;
	}

	ReferenceSignal = CreateAudioSignal(&config);
	if(!ReferenceSignal)
	{
		CloseFiles(&reference);
		CleanUp(&ReferenceSignal, &config);
		logmsg("Not enough memory for Data Structures\n");
		return 0;
	}

	logmsg("\n* Loading Reference audio file %s\n", config.referenceFile);
	if(!LoadFile(reference, ReferenceSignal, &config, config.referenceFile))
	{
		CloseFiles(&reference);
		CleanUp(&ReferenceSignal, &config);
		return 0;
	}

	logmsg("* Processing WAV file %s\n", config.referenceFile);
	if(!ProcessFile(ReferenceSignal, &config))
	{
		CleanUp(&ReferenceSignal, &config);
		return 1;
	}

	logmsg("* Max blanked frequencies per block %d\n", config.maxBlanked);
	
	CleanUp(&ReferenceSignal, &config);

	if(config.clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - clk: MDWave took %0.2fs\n", elapsedSeconds);
	}
	
	return(0);
}

void CleanUp(AudioSignal **ReferenceSignal, parameters *config)
{
	if(*ReferenceSignal)
	{
		ReleaseAudio(*ReferenceSignal, config);
		free(*ReferenceSignal);
		*ReferenceSignal = NULL;
	}

	ReleaseAudioBlockStructure(config);
}

void CloseFiles(FILE **ref)
{
	if(*ref)
	{
		fclose(*ref);
		*ref = NULL;
	}
}


char *GenerateFileNamePrefix(parameters *config)
{
	return(config->invert ? "Discarded" : "Used");
}

int LoadFile(FILE *file, AudioSignal *Signal, parameters *config, char *fileName)
{
	struct	timespec	start, end;
	double				seconds = 0;

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	if(!file)
		return 0;

	if(fread(&Signal->header, 1, sizeof(wav_hdr), file) != sizeof(wav_hdr))
	{
		logmsg("\tInvalid WAV file: File too small\n");
		return(0);
	}

	if(Signal->header.AudioFormat != 1) /* Check for PCM */
	{
		logmsg("\tInvalid WAV File: Only PCM is supported\n\tPlease use WAV PCM 16 bit");
		return(0);
	}

	if(Signal->header.NumOfChan != 2) /* Check for Stereo */
	{
		logmsg("\tInvalid WAV file: Only Stereo supported\n");
		return(0);
	}

	if(Signal->header.bitsPerSample != 16) /* Check bit depth */
	{
		logmsg("\tInvalid WAV file: Only 16 bit supported for now\n\tPlease use WAV PCM 16 bit %dhz");
		return(0);
	}
	
	if(Signal->header.SamplesPerSec/2 < config->endHz)
	{
		logmsg(" - %d Hz sample rate was too low for %d-%d Hz analysis\n",
			 Signal->header.SamplesPerSec, config->startHz, config->endHz);
		config->endHz = Signal->header.SamplesPerSec/2;
		logmsg(" - changed to %d-%d Hz\n", config->startHz, config->endHz);
	}

	// Default if none is found
	Signal->framerate = GetPlatformMSPerFrame(config);

	seconds = (double)Signal->header.Subchunk2Size/4.0/(double)Signal->header.SamplesPerSec;
	logmsg(" - WAV file is PCM %dhz %dbits and %g seconds long\n", 
		Signal->header.SamplesPerSec, Signal->header.bitsPerSample, seconds);

	if(seconds < GetSignalTotalDuration(Signal->framerate, config))
		logmsg(" - Estimated file length is smaller than the expected %gs\n",
				GetSignalTotalDuration(Signal->framerate, config));

	Signal->Samples = (char*)malloc(sizeof(char)*Signal->header.Subchunk2Size);
	if(!Signal->Samples)
	{
		logmsg("\tAll Chunks malloc failed!\n");
		return(0);
	}

	if(fread(Signal->Samples, 1, sizeof(char)*Signal->header.Subchunk2Size, file) !=
			 sizeof(char)*Signal->header.Subchunk2Size)
	{
		logmsg("\tCould not read the whole sample block from disk to RAM\n");
		return(0);
	}

	fclose(file);

	if(config->clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - clk: Loading WAV took %0.2fs\n", elapsedSeconds);
	}

	if(GetFirstSyncIndex(config) != NO_INDEX)
	{
		if(config->clock)
			clock_gettime(CLOCK_MONOTONIC, &start);

		/* Find the start offset */
		logmsg(" - Starting sync pulse train: ");
		Signal->startOffset = DetectPulse(Signal->Samples, Signal->header, config);
		if(Signal->startOffset == -1)
		{
			logmsg("\nStarting pulse train was not detected\n");
			return 0;
		}
		logmsg(" %gs [%ld bytes]\n", 
				BytesToSeconds(Signal->header.SamplesPerSec, Signal->startOffset),
				Signal->startOffset);

		if(GetLastSyncIndex(config) != NO_INDEX)
		{
			logmsg(" - Trailing sync pulse train: ");
			Signal->endOffset = DetectEndPulse(Signal->Samples, Signal->startOffset, Signal->header, config);
			if(Signal->endOffset == -1)
			{
				logmsg("\nTrailing sync pulse train was not detected, aborting\n");
				return 0;
			}
			logmsg(" %gs [%ld bytes]\n", 
				BytesToSeconds(Signal->header.SamplesPerSec, Signal->endOffset),
				Signal->endOffset);
			Signal->framerate = CalculateFrameRate(Signal, config);
			logmsg(" - Detected %g hz video signal (%gms per frame) from WAV file\n", 
						CalculateScanRate(Signal), Signal->framerate);
		}
		else
		{
			logmsg(" - Trailing sync pulse train not defined in config file, aborting\n");
			PrintAudioBlocks(config);
			return 0;
		}

		if(config->clock)
		{
			double	elapsedSeconds;
			clock_gettime(CLOCK_MONOTONIC, &end);
			elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
			logmsg(" - clk: Detecting sync took %0.2fs\n", elapsedSeconds);
		}
	}
	else
		Signal->framerate = GetPlatformMSPerFrame(config);

	if(seconds < GetSignalTotalDuration(Signal->framerate, config))
		logmsg(" - Adjusted File length is smaller than the expected %gs\n",
				GetSignalTotalDuration(Signal->framerate, config));

	if(GetFirstSilenceIndex(config) != NO_INDEX)
		Signal->hasFloor = 1;

	sprintf(Signal->SourceFile, "%s", fileName);

	return 1;
}

int ProcessFile(AudioSignal *Signal, parameters *config)
{
	long int		pos = 0;
	double			longest = 0;
	char			*buffer;
	size_t			buffersize = 0;
	windowManager	windows;
	double			*windowUsed = NULL;
	long int		loadedBlockSize = 0, i = 0;
	struct timespec	start, end;
	FILE			*processed = NULL;
	char			Name[4096], tempName[4096];
	int				leftover = 0, discardBytes = 0;
	double			leftDecimals = 0;

	pos = Signal->startOffset;
	
	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	longest = FramesToSeconds(Signal->framerate, GetLongestElementFrames(config));
	if(!longest)
	{
		logmsg("Block definitions are invalid, total length is 0\n");
		return 0;
	}

	buffersize = SecondsToBytes(Signal->header.SamplesPerSec, longest, NULL, NULL, NULL);
	buffer = (char*)malloc(buffersize);
	if(!buffer)
	{
		logmsg("\tmalloc failed\n");
		return(0);
	}

	if(!initWindows(&windows, Signal->framerate, Signal->header.SamplesPerSec, config))
		return 0;

	CompareFrameRates(Signal->framerate, GetPlatformMSPerFrame(config), config);

	while(i < config->types.totalChunks)
	{
		double duration = 0;
		long int frames = 0, difference = 0;

		frames = GetBlockFrames(config, i);
		duration = FramesToSeconds(Signal->framerate, frames);
		windowUsed = getWindowByLength(&windows, frames);
		
		loadedBlockSize = SecondsToBytes(Signal->header.SamplesPerSec, duration, &leftover, &discardBytes, &leftDecimals);

		difference = GetByteSizeDifferenceByFrameRate(Signal->framerate, frames, Signal->header.SamplesPerSec, config);

		Signal->Blocks[i].index = GetBlockSubIndex(config, i);
		Signal->Blocks[i].type = GetBlockType(config, i);

		if(Signal->Blocks[i].type >= TYPE_SILENCE)
		{
			//logmsg("Loaded %ld Left %ld Discard %ld difference %ld Decimals %g\n", loadedBlockSize, leftover, discardBytes, difference, leftDecimals);
			memset(buffer, 0, buffersize);
			if(pos + loadedBlockSize > Signal->header.Subchunk2Size)
			{
				logmsg("\tunexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
				break;
			}
			memcpy(buffer, Signal->Samples + pos, loadedBlockSize);
		
			if(!ProcessSamples(&Signal->Blocks[i], (int16_t*)buffer, (loadedBlockSize-difference)/2, Signal->header.SamplesPerSec, windowUsed, config, 0, Signal))
				return 0;
		}

		if(config->chunks)
		{
			sprintf(tempName, "%03ld_Source_%010ld_%s_%03d_chunk_", 
				i, pos, 
				GetBlockName(config, i), GetBlockSubIndex(config, i));
			ComposeFileName(Name, tempName, ".wav", config);
			SaveWAVEChunk(Name, Signal, buffer, 0, loadedBlockSize, config); 
		}

		pos += loadedBlockSize;
		pos += discardBytes;
		i++;
	}

	GlobalNormalize(Signal, config);
	CalcuateFrequencyBrackets(Signal, config);

	if(Signal->hasFloor && !config->ignoreFloor) // analyze noise floor if available
	{
		FindFloor(Signal, config);

		if(Signal->floorAmplitude != 0.0 && Signal->floorAmplitude > config->significantVolume)
		{
			config->significantVolume = Signal->floorAmplitude;
			logmsg(" - Using %g as minimum significant volume for analisys\n",
				config->significantVolume);
			CreateBaseName(config);
		}
	}

	if(config->verbose)
		PrintFrequencies(Signal, config);

	if(config->clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - clk: FFTW on WAV chunks took %0.2fs\n", elapsedSeconds);
	}

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	CreateBaseName(config);

	// Clean up everything again
	pos = Signal->startOffset;
	leftover = 0;
	discardBytes = 0;
	leftDecimals = 0;
	i = 0;

	// redo after processing
	while(i < config->types.totalChunks)
	{
		double duration = 0;
		long int frames = 0, difference = 0;

		frames = GetBlockFrames(config, i);
		duration = FramesToSeconds(Signal->framerate, frames);
		windowUsed = getWindowByLength(&windows, frames);
		
		loadedBlockSize = SecondsToBytes(Signal->header.SamplesPerSec, duration, &leftover, &discardBytes, &leftDecimals);

		difference = GetByteSizeDifferenceByFrameRate(Signal->framerate, frames, Signal->header.SamplesPerSec, config);

		if(Signal->Blocks[i].type >= TYPE_SILENCE)
		{
			memset(buffer, 0, buffersize);
			if(pos + loadedBlockSize > Signal->header.Subchunk2Size)
			{
				logmsg("\tunexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
				break;
			}
			memcpy(buffer, Signal->Samples + pos, loadedBlockSize);
	
			// now rewrite array
			if(!ProcessSamples(&Signal->Blocks[i], (int16_t*)buffer, (loadedBlockSize-difference)/2, Signal->header.SamplesPerSec, windowUsed, config, 1, Signal))
				return 0;

			// Now rewrite global
			memcpy(Signal->Samples + pos, buffer, loadedBlockSize);
		}

		pos += loadedBlockSize;
		pos += discardBytes;

		if(config->chunks)
		{
			sprintf(tempName, "%03ld_%s_Processed_%s_%03d_chunk_", i, 
				GenerateFileNamePrefix(config), GetBlockName(config, i), 
				GetBlockSubIndex(config, i));
			ComposeFileName(Name, tempName, ".wav", config);
			SaveWAVEChunk(Name, Signal, buffer, 0, loadedBlockSize, config);
		}
		i++;
	}

	// clear the rest of the buffer
	memset(Signal->Samples + pos, 0, (sizeof(char)*(Signal->header.Subchunk2Size - pos)));

	ComposeFileName(Name, GenerateFileNamePrefix(config), ".wav", config);
	processed = fopen(Name, "wb");
	if(!processed)
	{
		logmsg("\tCould not open processed file %s\n", Name);
		return 0;
	}

	if(fwrite(&Signal->header, 1, sizeof(wav_hdr), processed) != sizeof(wav_hdr))
	{
		logmsg("\tCould not write processed header\n");
		return(0);
	}

	if(fwrite(Signal->Samples, 1, sizeof(char)*Signal->header.Subchunk2Size, processed) !=
			 sizeof(char)*Signal->header.Subchunk2Size)
	{
		logmsg("\tCould not write samples to processed file\n");
		return (0);
	}

	if(processed)
	{
		fclose(processed);
		processed = NULL;
	}

	if(config->clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - clk: iFFTW on WAV chunks took %0.2fs\n", elapsedSeconds);
	}

	free(buffer);
	freeWindows(&windows);

	return 1;
}

int ProcessSamples(AudioBlocks *AudioArray, int16_t *samples, size_t size, long samplerate, double *window, parameters *config, int reverse, AudioSignal *Signal)
{
	fftw_plan		p = NULL, pBack = NULL;
	long		  	stereoSignalSize = 0, blanked = 0;	
	long		  	i = 0, monoSignalSize = 0, zeropadding = 0; 
	double		  	*signal = NULL;
	fftw_complex  	*spectrum = NULL;
	double		 	boxsize = 0, seconds = 0;
	double			CutOff = 0;
	long int 		startBin = 0, endBin = 0;
	
	if(!AudioArray)
	{
		logmsg("No Array for results\n");
		return 0;
	}

	stereoSignalSize = (long)size;
	monoSignalSize = stereoSignalSize/2;	 // 4 is 2 16 bit values
	seconds = (double)size/((double)samplerate*2);

	if(config->ZeroPad)  /* disabled by default */
		zeropadding = GetZeroPadValues(&monoSignalSize, &seconds, samplerate);

	boxsize = RoundFloat(seconds, 4);

	startBin = floor(config->startHz*boxsize);
	endBin = floor(config->endHz*boxsize);

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

	memset(signal, 0, sizeof(double)*(monoSignalSize+1));
	memset(spectrum, 0, sizeof(fftw_complex)*(monoSignalSize/2+1));

	if(!config->model_plan)
	{
		config->model_plan = fftw_plan_dft_r2c_1d(monoSignalSize, signal, spectrum, FFTW_MEASURE);
		if(!config->model_plan)
		{
			logmsg("FFTW failed to create FFTW_MEASURE plan\n");
			free(signal);
			signal = NULL;
			return 0;
		}
	}

	p = fftw_plan_dft_r2c_1d(monoSignalSize, signal, spectrum, FFTW_MEASURE);
	if(!p)
	{
		logmsg("FFTW failed to create FFTW_MEASURE plan\n");
		free(signal);
		signal = NULL;
		return 0;
	}

	if(reverse)
	{
		if(!config->reverse_plan)
		{
			config->reverse_plan = fftw_plan_dft_c2r_1d(monoSignalSize, spectrum, signal, FFTW_MEASURE);
			if(!config->reverse_plan)
			{
				logmsg("FFTW failed to create FFTW_MEASURE plan\n");
				free(signal);
				signal = NULL;
				return 0;
			}
		}
		pBack = fftw_plan_dft_c2r_1d(monoSignalSize, spectrum, signal, FFTW_MEASURE);
		if(!pBack)
		{
			logmsg("FFTW failed to create FFTW_MEASURE plan\n");
			free(signal);
			signal = NULL;
			return 0;
		}
	}

	for(i = 0; i < monoSignalSize - zeropadding; i++)
	{
		if(config->channel == 'l')
		{
			signal[i] = (double)samples[i*2];
			samples[i*2+1] = 0;
		}
		if(config->channel == 'r')
		{
			signal[i] = (double)samples[i*2+1];
			samples[i*2] = 0;
		}
		if(config->channel == 's')
		{
			signal[i] = ((double)samples[i*2]+(double)samples[i*2+1])/2.0;
			samples[i*2] = signal[i];
			samples[i*2+1] = signal[i];
		}

		if(window)
			signal[i] *= window[i];
	}

	fftw_execute(p); 
	fftw_destroy_plan(p);
	p = NULL;

	if(!reverse)
	{
		AudioArray->fftwValues.spectrum = spectrum;
		AudioArray->fftwValues.size = monoSignalSize;
		AudioArray->fftwValues.seconds = seconds;

		FillFrequencyStructures(AudioArray, config);
	}

	if(reverse)
	{
		double MinAmplitude = 0;

		// Find the Max magnitude for frequency at -f cuttoff
		for(int j = 0; j < config->MaxFreq; j++)
		{
			if(!AudioArray->freq[j].hertz)
				break;
			if(AudioArray->freq[j].amplitude < MinAmplitude)
				MinAmplitude = AudioArray->freq[j].amplitude;
		}

		CutOff = MinAmplitude;
		if(CutOff < config->significantVolume)
			CutOff = config->significantVolume;

		if(!config->ignoreFloor && Signal->hasFloor &&
			CutOff < Signal->floorAmplitude && Signal->floorAmplitude != 0.0)
			CutOff = Signal->floorAmplitude;

		//Process the whole frequency spectrum
		for(i = startBin; i < endBin; i++)
		{
			double amplitude = 0, magnitude = 0;
			int blank = 0;
	
			magnitude = CalculateMagnitude(spectrum[i], monoSignalSize);
			amplitude = CalculateAmplitude(magnitude, Signal->MaxMagnitude.magnitude);

			if(config->invert)
			{
				if(amplitude > CutOff)
					blank = 1;
			}
			else
			{
				if(amplitude <= CutOff)
					blank = 1;
			}

			if(blank)
			{
				float filter;

				// This should never bed one as such
				// A proper filter shoudl be used, or you'll get
				// ringing artifacts via Gibbs phenomenon
				// Here it "works" because we are just
				// "visualizing" the results
				filter = 0;
				spectrum[i] = spectrum[i]*filter;
				blanked ++;
			}
		}
	
		// Magic! iFFTW
		fftw_execute(pBack); 
		fftw_destroy_plan(pBack);
		pBack = NULL;
	
		if(window)
		{
			for(i = 0; i < monoSignalSize - zeropadding; i++)
			{
				double value;
	
				// reversing window causes distortion since we have zeroes
				// but we do want t see the windows in the iFFT anyway
				// uncomment if needed
				//value = (signal[i]/window[i])/monoSignalSize;
				value = signal[i]/monoSignalSize; /* check CalculateMagnitude if changed */
				if(config->channel == 'l')
				{
					samples[i*2] = round(value);
					samples[i*2+1] = 0;
				}
				if(config->channel == 'r')
				{
					samples[i*2] = 0;
					samples[i*2+1] = round(value);
				}
				if(config->channel == 's')
				{
					samples[i*2] = round(value);
					samples[i*2+1] = round(value);
				}
			}
		}
		else
		{
			for(i = 0; i < monoSignalSize - zeropadding; i++)
			{
				double value = 0;
		
				value = signal[i]/monoSignalSize;
				if(config->channel == 'l')
				{
					samples[i*2] = round(value);
					samples[i*2+1] = 0;
				}
				if(config->channel == 'r')
				{
					samples[i*2] = 0;
					samples[i*2+1] = round(value);
				}
				if(config->channel == 's')
				{
					samples[i*2] = round(value);
					samples[i*2+1] = round(value);
				}
			}
		}
		//logmsg("Blanked frequencies were %ld from %ld\n", blanked, monoSignalSize/2);
		if(blanked > config->maxBlanked)
			config->maxBlanked = blanked;
	}

	free(signal);
	signal = NULL;

	return(1);
}

int commandline_wave(int argc , char *argv[], parameters *config)
{
	FILE *file = NULL;
	int c, index, ref = 0;
	
	opterr = 0;
	
	CleanParameters(config);

	config->maxBlanked = 0;
	config->invert = 0;
	config->chunks = 0;
	config->floorAmplitude = 0;

	while ((c = getopt (argc, argv, "hvzcklyxis:e:f:t:p:a:w:r:P:")) != -1)
	switch (c)
	  {
	  case 'h':
		PrintUsage_wave();
		return 0;
		break;
	  case 'v':
		config->verbose = 1;
		break;
	  case 'c':
		config->chunks = 1;
		break;
	  case 'k':
		config->clock = 1;
		break;
	  case 'l':
		EnableLog();
		break;
	  case 'z':
		config->ZeroPad = 1;
		break;
	  case 'x':
		config->invert = 1;   // RELEVANT HERE!
		break;
	  case 'i':
		config->ignoreFloor = 1;   // RELEVANT HERE!
		break;
	  case 'y':
		config->debugSync = 1;
		break;
	  case 's':
		config->startHz = atoi(optarg);
		if(config->startHz < 1 || config->startHz > 19900)
			config->startHz = START_HZ;
		break;
	  case 'e':
		config->endHz = atoi(optarg);
		if(config->endHz < 10 || config->endHz > 20000)
			config->endHz = END_HZ;
		break;
	  case 'f':
		config->MaxFreq = atoi(optarg);
		if(config->MaxFreq < 1 || config->MaxFreq > MAX_FREQ_COUNT)
			config->MaxFreq = MAX_FREQ_COUNT;
		break;
	  case 't':
		config->tolerance = atof(optarg);
		if(config->tolerance < 0.0 || config->tolerance > 100.0)
			config->tolerance = DBS_TOLERANCE;
		break;
	  case 'p':
		config->significantVolume = atof(optarg);
		if(config->significantVolume <= -100.0 || config->significantVolume >= -1.0)
			config->significantVolume = SIGNIFICANT_VOLUME;
		config->origSignificantVolume = config->significantVolume;
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
	  case 'r':
		sprintf(config->referenceFile, "%s", optarg);
		ref = 1;
		break;
	  case 'P':
		sprintf(config->profileFile, "%s", optarg);
		break;
	  case '?':
		if (optopt == 'r')
		  logmsg("Reference File -%c requires an argument.\n", optopt);
		else if (optopt == 'a')
		  logmsg("Audio channel option -%c requires an argument: l,r or s\n", optopt);
		else if (optopt == 'w')
		  logmsg("FFT Window option -%c requires an argument: n,t,f or h\n", optopt);
		else if (optopt == 't')
		  logmsg("Magitude tolerance -%c requires an argument: 0.0-100.0\n", optopt);
		else if (optopt == 'f')
		  logmsg("Max # of frequencies to use from FFTW -%c requires an argument: 1-%d\n", optopt, MAX_FREQ_COUNT);
		else if (optopt == 's')
		  logmsg("Min frequency range for FFTW -%c requires an argument: 0-19900\n", optopt);
		else if (optopt == 'e')
		  logmsg("Max frequency range for FFTW -%c requires an argument: 10-20000\n", optopt);
		else if (optopt == 'P')
		  logmsg("Profile File -%c requires a file argument\n", optopt);
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

	if(!ref)
	{
		logmsg("Please define the reference audio file\n");
		return 0;
	}

	if(config->endHz <= config->startHz)
	{
		logmsg("Invalid frequency range for FFTW (%d Hz to %d Hz)\n", config->startHz, config->endHz);
		return 0;
	}

	file = fopen(config->referenceFile, "rb");
	if(!file)
	{
		logmsg("\tCould not open REFERENCE file: \"%s\"\n", config->referenceFile);
		return 1;
	}
	fclose(file);

	CreateFolderName_wave(config);
	CreateBaseName(config);

	if(IsLogEnabled())
	{
		char tmp[LOG_NAME_LEN];

		ComposeFileName(tmp, "WAVE_Log_", ".txt", config);

		if(!setLogName(tmp))
			return 0;

		DisableConsole();
		Header(1);
		EnableConsole();
	}

	if(config->channel != 's')
		logmsg("\tAudio Channel is: %s\n", GetChannel(config->channel));
	if(config->tolerance != 0.0)
		logmsg("\tAmplitude tolerance while comparing is +/-%0.2f dBFS\n", config->tolerance);
	if(config->MaxFreq != FREQ_COUNT)
		logmsg("\tMax frequencies to use from FFTW are %d (default %d)\n", config->MaxFreq, FREQ_COUNT);
	if(config->startHz != START_HZ)
		logmsg("\tFrequency start range for FFTW is now %d (default %d)\n", config->startHz, START_HZ);
	if(config->endHz != END_HZ)
		logmsg("\tFrequency end range for FFTW is now %d (default %d)\n", config->endHz, END_HZ);
	if(config->window != 'n')
		logmsg("\tA %s window will be applied to each block to be compared\n", GetWindow(config->window));
	else
		logmsg("\tNo window (rectangle) will be applied to each block to be compared\n");
	if(config->ZeroPad)
		logmsg("\tFFT bins will be aligned to 1hz, this is slower\n");
	if(config->ignoreFloor)
		logmsg("\tIgnoring Silence block noise floor\n");
	if(config->invert)
		logmsg("\tSaving Discarded part fo the signal to WAV file\n");
	if(config->chunks)
		logmsg("\tSaving WAv chunks to individual files\n");

	return 1;
}

void PrintUsage_wave()
{
	logmsg("  usage: mdwave -r reference.wav\n");
	logmsg("   FFT and Analysis options:\n");
	logmsg("	 -a: select <a>udio channel to compare. 's', 'l' or 'r'\n");
	logmsg("	 -c: Enable Audio <c>hunk creation, an individual WAV for each block\n");
	logmsg("	 -w: enable <w>indowing. Default is a custom Tukey window.\n");
	logmsg("		'n' none, 't' Tukey, 'h' Hann, 'f' FlatTop & 'm' Hamming\n");
	logmsg("	 -x: e<x>cludes results to generate discarded frequencies wav file\n");
	logmsg("	 -i: <i>gnores the silence block noise floor if present\n");
	logmsg("	 -f: Change the number of <f>requencies to use from FFTW\n");
	logmsg("	 -s: Defines <s>tart of the frequency range to compare with FFT\n");
	logmsg("	 -e: Defines <e>nd of the frequency range to compare with FFT\n");
	logmsg("	 -t: Defines the <t>olerance when comparing amplitudes in dBFS\n");
	logmsg("	 -z: Uses Zero Padding to equal 1 hz FFT bins\n");
	logmsg("   Output options:\n");
	logmsg("	 -v: Enable <v>erbose mode, spits all the FFTW results\n");
	logmsg("	 -l: <l>og output to file [reference]_vs_[compare].txt\n");
	logmsg("	 -k: cloc<k> FFTW operations\n");
}

void Header_wave(int log)
{
	char title1[] = " MDWave " MDWVERSION " (MDFourier Companion)\n [240p Test Suite Fourier Audio compare tool]\n";
	char title2[] = " Artemio Urbina 2019 free software under GPL\n\n";

	if(log)
		logmsg("%s%s", title1, title2);
	else
		printf("%s%s", title1, title2);
}
