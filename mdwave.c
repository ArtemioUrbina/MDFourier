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
 * Compile with: 
 *	  gcc -Wall -std=gnu99 -o mdwave mdwave.c -lfftw3 -lm
 */

#define MDWAVE
#define MDWVERSION "0.75"

#include "mdfourier.h"
#include "log.h"
#include "windows.h"
#include "freq.h"
#include "cline.h"

int LoadFile(FILE *file, AudioSignal *Signal, parameters *config, char *fileName);
double ProcessSamples(AudioBlocks *AudioArray, short *samples, size_t size, long samplerate, float *window, parameters *config, int reverse);
int commandline_wave(int argc , char *argv[], parameters *config);
void FindMaxMagnitude(AudioSignal *Signal, parameters *config);
void PrintUsage_wave();
void Header_wave(int log);

int main(int argc , char *argv[])
{
	FILE				*reference = NULL;
	AudioSignal  		*ReferenceSignal;
	parameters			config;

	Header_wave(0);
	if(!commandline_wave(argc, argv, &config))
	{
		printf("	 -h: Shows command line help\n");
		return 1;
	}

	if(!LoadAudioBlockStructure(&config))
		return 1;

	reference = fopen(config.referenceFile, "rb");
	if(!reference)
	{
		logmsg("\tCould not open REFERENCE file: \"%s\"\n", config.referenceFile);
		return 0;
	}

	ReferenceSignal = CreateAudioSignal(&config);
	if(!ReferenceSignal)
	{
		logmsg("Not enough memory for Data Structures\n");
		return 0;
	}

	logmsg("\nLoading Reference audio file %s\n", config.referenceFile);
	if(!LoadFile(reference, ReferenceSignal, &config, config.referenceFile))
	{
		free(ReferenceSignal);
		return 0;
	}
	logmsg("Max blanked frequencies per block from the spectrum %d\n", config.maxBlanked);
	
	ReleaseAudio(ReferenceSignal, &config);
	free(ReferenceSignal);
	ReferenceSignal = NULL;

	ReleaseAudioBlockStructure(&config);
	
	return(0);
}

void GenerateFileName(parameters *config, char *Name, char *Target)
{
	char text[200];
	
	if(!config->ignoreFloor)
		sprintf(text, "Floor");
	else
		sprintf(text, "F-%04d", config->MaxFreq);
	sprintf(Target, "%s_%s_%s", config->invert ? "Discarded" : "Used", text, Name);
}

int LoadFile(FILE *file, AudioSignal *Signal, parameters *config, char *fileName)
{
	int 				i = 0;
	int 				loadedBlockSize = 0;
	char				*buffer, silence = -1;
	size_t			 	buffersize = 0;
	size_t			 	discardBytes = 0, discardSamples = 0;
	wav_hdr 			header;
	windowManager		windows;
	float				*windowUsed = NULL;
	struct	timespec	start, end;
	double				seconds = 0, longest = 0;
	FILE				*processed = NULL;
	char				Name[2048];
	char 				*AllSamples = NULL;
	long int			pos = 0;

	if(!file)
		return 0;

	if(fread(&header, 1, sizeof(wav_hdr), file) != sizeof(wav_hdr))
	{
		logmsg("\tInvalid WAV file: File too small\n");
		return(0);
	}

	GenerateFileName(config, basename(fileName), Name);
	processed = fopen(Name, "wb");
	if(!processed)
	{
		logmsg("\tCould not open processed file %s\n", Name);
		return 0;
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
	if(seconds < GetTotalBlockDuration(config))
		logmsg("File length is smaller than expected\n");

	logmsg("WAV file is PCM %dhz %dbits and %g seconds long\n", 
		header.SamplesPerSec, header.bitsPerSample, seconds);

	// We need to convert buffersize to the 16.688ms per frame by the Genesis
	// Mega Drive is 1.00128, now loaded fomr file
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

	buffersize = header.SamplesPerSec*4*sizeof(char)*(int)longest; // 2 bytes per sample, stereo
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

	silence = GetSilenceIndex(config);
	if(silence != NO_INDEX)
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

		ProcessSamples(&Signal->Blocks[i], (short*)buffer, loadedBlockSize/2, header.SamplesPerSec, windowUsed, config, 0);

		if(config->chunks)
		{
			FILE 		*chunk = NULL;
			wav_hdr		cheader;

			cheader = header;
			sprintf(Name, "%03d_Source_chunk_%s", i, basename(fileName));
			chunk = fopen(Name, "wb");
			if(!chunk)
			{
				logmsg("\tCould not open chunk file %s\n", Name);
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

		pos += discardBytes;  // Advance to adjust the time for the Sega Genesis Frame Rate
		i++;
	}

	// Instead of Global Normalization by default, do...
	FindMaxMagnitude(Signal, config);

	if(Signal->hasFloor && !config->ignoreFloor) // analyze noise floor if available
		FindFloor(Signal, config);

	// Clean up everything again
	pos = 0;
	i = 0;

	// redo with Floor
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
		
		Signal->Blocks[i].index = GetBlockSubIndex(config, i);
		Signal->Blocks[i].type = GetBlockType(config, i);

		// now rewrite array
		if(i != silence)  // Ignore Silence!
			ProcessSamples(&Signal->Blocks[i], (short*)buffer, loadedBlockSize/2, header.SamplesPerSec, windowUsed, config, 1);

		// Now rewrite global
		memcpy(AllSamples + pos, buffer, loadedBlockSize);

		pos += loadedBlockSize;

		if(config->chunks)
		{
			FILE 		*chunk = NULL;
			wav_hdr		cheader;
			char		fName[2048];

			cheader = header;
			GenerateFileName(config, basename(fileName), Name);
			sprintf(fName, "%03d_%s", i, Name);
			chunk = fopen(fName, "wb");
			if(!chunk)
			{
				logmsg("\tCould not open chunk file %s\n", Name);
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
		// clean this noise since we didn't process it
		memset(AllSamples + pos, 0, discardBytes);
		pos += discardBytes;  // Advance to adjust the time for the Sega Genesis Frame Rate
		i++;
	}

	// clear the rest of the buffer
	memset(AllSamples + pos, 0, (sizeof(char)*(header.Subchunk2Size - pos)));
	if(fwrite(&header, 1, sizeof(wav_hdr), processed) != sizeof(wav_hdr))
	{
		logmsg("\tCould not write processed header\n");
		return(0);
	}

	if(fwrite(AllSamples, 1, sizeof(char)*header.Subchunk2Size, processed) != sizeof(char)*header.Subchunk2Size)
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
		double			elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg("Processing WAV took %f\n", elapsedSeconds);
	}

	fclose(file);
	free(buffer);
	free(AllSamples);

	freeWindows(&windows);

	return i;
}

void FindMaxMagnitude(AudioSignal *Signal, parameters *config)
{
	double MaxMagnitude = 0;
	double MinAmplitude = 0;

	// Find global peak
	for(int block = 0; block < config->types.totalChunks; block++)
	{
		for(int i = 0; i < config->MaxFreq; i++)
		{
			if(Signal->Blocks[block].freq[i].magnitude > MaxMagnitude)
				MaxMagnitude = Signal->Blocks[block].freq[i].magnitude;
		}
	}

	config->MaxMagnitude = MaxMagnitude;

	//Calculate Amplitude in dbs
	for(int block = 0; block < config->types.totalChunks; block++)
	{
		for(int i = 0; i < config->MaxFreq; i++)
		{
			Signal->Blocks[block].freq[i].amplitude = 
				20*log10(Signal->Blocks[block].freq[i].magnitude / MaxMagnitude);
			Signal->Blocks[block].freq[i].magnitude = 
				Signal->Blocks[block].freq[i].magnitude * 100.0 / MaxMagnitude;
			if(Signal->Blocks[block].freq[i].amplitude < MinAmplitude)
				MinAmplitude = Signal->Blocks[block].freq[i].amplitude;
		}
	}

	config->MinAmplitude = MinAmplitude;
}


double ProcessSamples(AudioBlocks *AudioArray, short *samples, size_t size, long samplerate, float *window, parameters *config, int reverse)
{
	fftw_plan		p = NULL, pBack = NULL;
	long		  	stereoSignalSize = 0, blanked = 0;	
	long		  	i = 0, monoSignalSize = 0; 
	double		  	*signal = NULL;
	fftw_complex  	*spectrum = NULL;
	double		 	boxsize = 0, seconds = 0;
	struct			timespec start, end;
	double			CutOff = 0;
	
	if(!AudioArray)
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

	if(config->clockBlock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	p = fftw_plan_dft_r2c_1d(monoSignalSize, signal, spectrum, FFTW_MEASURE);
	if(reverse)
		pBack = fftw_plan_dft_c2r_1d(monoSignalSize, spectrum, signal, FFTW_MEASURE);

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

	if(!reverse)
	{
		for(i = config->startHz*boxsize; i < config->endHz*boxsize; i++)	// Nyquist at 44.1khz
		{
			double r1 = creal(spectrum[i]);
			double i1 = cimag(spectrum[i]);
			double magnitude, previous;
			int    j = 0;
			double Hertz;
	
			magnitude = sqrt(r1*r1 + i1*i1)/monoSignalSize;
			Hertz = ((double)i/seconds);
	
			previous = 1.e30;
			if(!IsCRTNoise(Hertz))
			{
				for(j = 0; j < config->MaxFreq; j++)
				{
					if(magnitude > AudioArray->freq[j].magnitude && magnitude < previous)
					{
						//Move the previous values down the array
						for(int k = config->MaxFreq-1; k > j; k--)
							AudioArray->freq[k] = AudioArray->freq[k - 1];
		
						AudioArray->freq[j].hertz = Hertz;
						AudioArray->freq[j].magnitude = magnitude;
						AudioArray->freq[j].amplitude = 0;
						AudioArray->freq[j].phase = atan2(i1, r1);
						break;
					}
					previous = AudioArray->freq[j].magnitude;
				}
			}
		}
	}

	if(reverse)
	{
		double MinAmplitude = 0;

		// This needs exploring, global minimum at that frequency count
		//CutOff = config->MinAmplitude;

		// Find the Max magnitude for frequency at -f cuttoff
		for(int j = 0; j < config->MaxFreq; j++)
		{
			if(!AudioArray->freq[j].hertz)
				break;
			if(AudioArray->freq[j].amplitude < MinAmplitude)
				MinAmplitude = AudioArray->freq[j].amplitude;
		}
		CutOff = MinAmplitude;
		//logmsg("Cutoff: %g\n", CutOff);

		//Process the whole frequency spectrum
		for(i = config->startHz*boxsize; i < config->endHz*boxsize; i++)	// Nyquist at 44.1khz
		{
			double r1 = creal(spectrum[i]);
			double i1 = cimag(spectrum[i]);
			double amplitude = 0, magnitude = 0;
			int blank = 0;
			double Hertz;
	
			magnitude = sqrt(r1*r1 + i1*i1)/monoSignalSize;
			amplitude = 20*log10(magnitude / config->MaxMagnitude);
			Hertz = ((double)i/seconds);

			if(config->invert)
			{
				if(!config->ignoreFloor && amplitude > config->floorAmplitude)
					blank = 1;
				
				if(config->ignoreFloor && amplitude > CutOff)
					blank = 1;

				if(IsCRTNoise(Hertz))
					blank = 0;	
			}
			else
			{
				if(!config->ignoreFloor && amplitude <= config->floorAmplitude)
					blank = 1;
				
				if(config->ignoreFloor && amplitude <= CutOff)
					blank = 1;

				if(IsCRTNoise(Hertz))
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
	
		if(config->window != 'n' && window)
		{
			for(i = 0; i < monoSignalSize; i++)
			{
				double value;
	
				// reversing window causes distortion since we have the genesis discontiuity above!
				//value = (signal[i]/window[i])/monoSignalSize;
				value = signal[i]/monoSignalSize;
				if(config->channel == 'l')
					samples[i*2] = round(value);
				if(config->channel == 'r')
					samples[i*2+1] = round(value);
				if(config->channel == 's')
				{
					samples[i*2] = round(value);
					samples[i*2+1] = round(value);
				}
			}
		}
		else
		{
			for(i = 0; i < monoSignalSize; i++)
			{
				double value = 0;
		
				value = signal[i]/monoSignalSize;
				if(config->channel == 'l')
					samples[i*2] = round(value);
				if(config->channel == 'r')
					samples[i*2+1] = round(value);
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
	
	if(config->clockBlock)
	{
		double			elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg("Processing Block took %f\n", elapsedSeconds);
	}

	free(signal);
	signal = NULL;

	AudioArray->fftwValues.spectrum = spectrum;
	AudioArray->fftwValues.size = monoSignalSize;
	AudioArray->fftwValues.seconds = seconds;

	return(0);
}

int commandline_wave(int argc , char *argv[], parameters *config)
{
	int c, index, ref = 0, tar = 0;
	
	opterr = 0;
	
	CleanParameters(config);

	config->maxBlanked = 0;
	config->invert = 0;
	config->chunks = 0;
	config->MaxMagnitude = 0;
	config->MinAmplitude = 0;
	config->floorAmplitude = 0;

	while ((c = getopt (argc, argv, "ihvkgclxw:n:d:a:t:r:c:f:b:s:z:")) != -1)
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
	  case 'g':
		config->clockBlock = 1;
		break;
	  case 'l':
		EnableConsole();
		break;
	  case 'i':
		config->invert = 1;   // RELEVANT HERE!
		break;
	  case 'x':
		config->ignoreFloor = 0;   // RELEVANT HERE!
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
		if(config->tolerance < 0.0 || config->tolerance > 100.0)
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
	  case 'r':
		sprintf(config->referenceFile, "%s", optarg);
		ref = 1;
		break;
	  case '?':
		if (optopt == 'r')
		  logmsg("Reference File -%c requires an argument.\n", optopt);
		else if (optopt == 'n')
		  logmsg("Normalization option -%c requires an argument: g,n or r\n", optopt);
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

	if(!ref)
	{
		logmsg("Please define the reference audio file\n");
		return 0;
	}

	if(tar)
		logmsg("Target file will be ignored\n");

	if(config->endHz <= config->startHz)
	{
		logmsg("Invalid frequency range for FFTW (%d Hz to %d Hz)\n", config->startHz, config->endHz);
		return 0;
	}

	if(IsLogEnabled())
	{
		int len;
		char tmp[LOG_NAME_LEN];
		
		sprintf(tmp, "WAVE_%s", basename(config->referenceFile));
		len = strlen(tmp);
		tmp[len-4] = '\0';

		if(!setLogName(tmp))
			return 0;

		DisableConsole();
		Header_wave(1);
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
		logmsg("\tA %s window will be applied to each block to be compared\n", GetWindow(config->window));

	return 1;
}

void PrintUsage_wave()
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
	logmsg("	 -j: Cuts all the per block information and shows <j>ust the total results\n");
	logmsg("	 -e: Enables <e>xtended results. Shows a table with all matched\n\tfrequencies for each block with differences\n");
	logmsg("	 -m: Enables Show all blocks compared with <m>atched frequencies\n");
	logmsg("	 -x: Enables totaled output for use with grep and spreadsheet\n");
	logmsg("	 -l: <l>og output to file [reference]_vs_[compare].txt\n");
	logmsg("	 -k: cloc<k> FFTW operations\n");
	logmsg("	 -g: clock FFTW operations for each <n>ote\n");
}

void Header_wave(int log)
{
	char title1[] = "== MDWave " MDWVERSION " == (MDFourier Companion)\nSega Genesis/Mega Drive Fourier Audio compare tool for 240p Test Suite\n";
	char title2[] = "by Artemio Urbina 2019, licensed under GPL\n\n";

	if(log)
		logmsg("%s%s", title1, title2);
	else
		printf("%s%s", title1, title2);
}
