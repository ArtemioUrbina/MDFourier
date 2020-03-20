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
#include "diff.h"
#include "cline.h"
#include "sync.h"
#include "balance.h"
#include "loadfile.h"

int ProcessSignalMDW(AudioSignal *Signal, parameters *config);
int ProcessSamples(AudioBlocks *AudioArray, int16_t *samples, size_t size, long samplerate, double *window, parameters *config, int reverse, AudioSignal *Signal);
int commandline_wave(int argc , char *argv[], parameters *config);
void PrintUsage_wave();
void Header_wave(int log);
void CleanUp(AudioSignal **ReferenceSignal, parameters *config);
int ExecuteMDWave(parameters *config, int invert);

int main(int argc , char *argv[])
{
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

	if(!LoadProfile(&config))
	{
		logmsg("Aborting\n");
		return 1;
	}

	if(!SetupFolders(OUTPUT_FOLDER_MDW, "WAVE_Log_", &config))
	{
		logmsg("Aborting\n");
		return 1;
	}

	if(!EndProfileLoad(&config))
	{
		logmsg("Aborting\n");
		return 1;
	}

	if(ExecuteMDWave(&config, 0) == 1)
	{
		logmsg("Aborting\n");
		return 1;
	}

	if(config.executefft)
	{
		if(!LoadProfile(&config))
			return 1;

		if(ExecuteMDWave(&config, 1) == 1)
			return 1;
	}
	else
	{
		printf("\nResults stored in %s%c%s\n", 
			config.outputPath, 
			config.outputPath[0] == '\0' ? ' ' : FOLDERCHAR, 
			config.folderName);
	}

	if(config.clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - clk: MDWave took %0.2fs\n", elapsedSeconds);
	}
}

int ExecuteMDWave(parameters *config, int invert)
{
	AudioSignal  		*ReferenceSignal = NULL;
	char 				*MainPath = NULL;

	if(invert)
	{
		logmsg("\n* Calculating values for Discard file\n");
		config->invert = 1;
	}

	if(!LoadFile(&ReferenceSignal, config->referenceFile, ROLE_REF, config))
	{
		CleanUp(&ReferenceSignal, config);
		return 1;
	}

	if(!config->useCompProfile)
		ReferenceSignal->role = ROLE_REF;
	else
		ReferenceSignal->role = ROLE_COMP;

	config->referenceFramerate = ReferenceSignal->framerate;
	config->smallerFramerate = ReferenceSignal->framerate;

	if(ReferenceSignal->AudioChannels == 2)
	{
		int block = NO_INDEX;

		block = GetFirstMonoIndex(config);
		if(block != NO_INDEX)
		{
			logmsg("\n* Comparing Stereo channel amplitude\n");
			if(config->verbose)
				logmsg(" - Mono block used for balance: %s# %d\n", 
					GetBlockName(config, block), GetBlockSubIndex(config, block));
			CheckBalance(ReferenceSignal, block, config);
		}
		else
		{
			logmsg(" - No mono block for stereo balance check\n");
		}
	}

	logmsg("* Processing Audio\n");
	MainPath = PushMainPath(config);
	if(!ProcessSignalMDW(ReferenceSignal, config))
	{
		CleanUp(&ReferenceSignal, config);
		PopMainPath(&MainPath);
		return 1;
	}
	PopMainPath(&MainPath);

	//logmsg("* Max blanked frequencies per block %d\n", config->maxBlanked);
	CleanUp(&ReferenceSignal, config);

	if(invert)
		printf("\nResults stored in %s%s\n", 
			config->outputPath,
			config->folderName);
	
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

char *GenerateFileNamePrefix(parameters *config)
{
	return(config->invert ? "2_Discarded" : "1_Used");
}

int CreateChunksFolder(parameters *config)
{
	char name[BUFFER_SIZE*4];

	sprintf(name, "%s%cChunks", config->folderName, FOLDERCHAR);
	if(!CreateFolder(name))
		return 0;
	return 1;
}

int ProcessSignalMDW(AudioSignal *Signal, parameters *config)
{
	long int		pos = 0;
	double			longest = 0;
	char			*buffer;
	size_t			buffersize = 0;
	windowManager	windows;
	double			*windowUsed = NULL;
	long int		loadedBlockSize = 0, i = 0, syncAdvance = 0;
	struct timespec	start, end;
	FILE			*processed = NULL;
	char			Name[BUFFER_SIZE*2+256], tempName[BUFFER_SIZE];
	int				leftover = 0, discardBytes = 0, syncinternal = 0;
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

	buffersize = SecondsToBytes(Signal->header.fmt.SamplesPerSec, longest, Signal->AudioChannels, NULL, NULL, NULL);
	buffer = (char*)malloc(buffersize);
	if(!buffer)
	{
		logmsg("\tmalloc failed\n");
		return(0);
	}

	if(!initWindows(&windows, Signal->header.fmt.SamplesPerSec, config->window, config))
	{
		logmsg("Copuld not create windows\n");
		return 0;
	}

	CompareFrameRatesMDW(Signal, GetMSPerFrame(Signal, config), config);

	while(i < config->types.totalBlocks)
	{
		double duration = 0, framerate = 0;
		long int frames = 0, difference = 0, cutFrames = 0;

		if(!syncinternal)
			framerate = Signal->framerate;
		else
			framerate = config->referenceFramerate;

		frames = GetBlockFrames(config, i);
		cutFrames = GetBlockCutFrames(config, i);
		duration = FramesToSeconds(framerate, frames);
				
		loadedBlockSize = SecondsToBytes(Signal->header.fmt.SamplesPerSec, duration, Signal->AudioChannels, &leftover, &discardBytes, &leftDecimals);

		difference = GetByteSizeDifferenceByFrameRate(framerate, frames, Signal->header.fmt.SamplesPerSec, Signal->AudioChannels, config);

		windowUsed = NULL;
		if(Signal->Blocks[i].type >= TYPE_SILENCE)
			windowUsed = getWindowByLength(&windows, frames, cutFrames, Signal->framerate, config);

		//logmsg("Loaded %ld Left %ld Discard %ld difference %ld Decimals %g\n", loadedBlockSize, leftover, discardBytes, difference, leftDecimals);
		memset(buffer, 0, buffersize);
		if(pos + loadedBlockSize > Signal->header.data.DataSize)
		{
			logmsg("\tunexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
			break;
		}
		memcpy(buffer, Signal->Samples + pos, loadedBlockSize);

		if(Signal->Blocks[i].type >= TYPE_SILENCE && config->executefft)
		{
			if(!ProcessSamples(&Signal->Blocks[i], (int16_t*)buffer, (loadedBlockSize-difference)/2, Signal->header.fmt.SamplesPerSec, windowUsed, config, 0, Signal))
				return 0;
		}

		if(config->chunks && !config->invert)
		{
			if(!CreateChunksFolder(config))
				return 0;
			sprintf(Name, "%s%cChunks%c%03ld_0_Source_%010ld_%s_%03d_chunk.wav", 
				config->folderName, FOLDERCHAR, FOLDERCHAR,
				i, pos+syncAdvance+Signal->SamplesStart, 
				GetBlockName(config, i), GetBlockSubIndex(config, i));
			SaveWAVEChunk(Name, Signal, buffer, 0, loadedBlockSize, 0, config); 
		}

		pos += loadedBlockSize;
		pos += discardBytes;

		if(config->executefft)
		{
			if(Signal->Blocks[i].type == TYPE_INTERNAL_KNOWN)
			{
				if(!ProcessInternal(Signal, i, pos, &syncinternal, &syncAdvance, TYPE_INTERNAL_KNOWN, config))
					return 0;
			}
	
			if(Signal->Blocks[i].type == TYPE_INTERNAL_UNKNOWN)
			{
				if(!ProcessInternal(Signal, i, pos, &syncinternal, &syncAdvance, TYPE_INTERNAL_UNKNOWN, config))
					return 0;
			}
		}

		i++;
	}

	if(config->executefft)
	{
		GlobalNormalize(Signal, config);
		CalcuateFrequencyBrackets(Signal, config);
	
		if(Signal->hasSilenceBlock && !config->ignoreFloor) // analyze noise floor if available
		{
			FindFloor(Signal, config);
	
			if(Signal->floorAmplitude != 0.0 && Signal->floorAmplitude > config->significantAmplitude)
			{
				config->significantAmplitude = Signal->floorAmplitude;
			}
		}
	
		logmsg(" - Using %g dBFS as minimum significant amplitude for analysis\n",
				config->significantAmplitude);
	
		if(config->verbose)
			PrintFrequencies(Signal, config);
	}

	if(config->clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - clk: FFTW on Audio chunks took %0.2fs\n", elapsedSeconds);
	}

	if(config->executefft)
	{
		if(config->clock)
			clock_gettime(CLOCK_MONOTONIC, &start);
	
		// Clean up everything again
		pos = Signal->startOffset;
		leftover = 0;
		discardBytes = 0;
		leftDecimals = 0;
		i = 0;
	
		// redo after processing
		while(i < config->types.totalBlocks)
		{
			double duration = 0;
			long int frames = 0, difference = 0, cutFrames = 0;
	
			frames = GetBlockFrames(config, i);
			cutFrames = GetBlockCutFrames(config, i);
			duration = FramesToSeconds(Signal->framerate, frames);
			if(Signal->Blocks[i].type >= TYPE_SILENCE)
				windowUsed = getWindowByLength(&windows, frames, cutFrames, Signal->framerate, config);
			
			loadedBlockSize = SecondsToBytes(Signal->header.fmt.SamplesPerSec, duration, Signal->AudioChannels, &leftover, &discardBytes, &leftDecimals);
	
			difference = GetByteSizeDifferenceByFrameRate(Signal->framerate, frames, Signal->header.fmt.SamplesPerSec, Signal->AudioChannels, config);
	
			memset(buffer, 0, buffersize);
			if(pos + loadedBlockSize > Signal->header.data.DataSize)
			{
				logmsg("\tunexpected end of File, please record the full Audio Test from the 240p Test Suite\n");
				break;
			}
			memcpy(buffer, Signal->Samples + pos, loadedBlockSize);
		
			if(Signal->Blocks[i].type >= TYPE_SILENCE)
			{
				// now rewrite array
				if(!ProcessSamples(&Signal->Blocks[i], (int16_t*)buffer, (loadedBlockSize-difference)/2, Signal->header.fmt.SamplesPerSec, windowUsed, config, 1, Signal))
					return 0;
	
				// Now rewrite global
				memcpy(Signal->Samples + pos, buffer, loadedBlockSize);
			}

			if(Signal->Blocks[i].type < TYPE_SILENCE && !config->invert)
			{
				if(Signal->Blocks[i].type != TYPE_SYNC)
				{
					memset(buffer, 0, sizeof(char)*loadedBlockSize);
					memcpy(Signal->Samples + pos, buffer, loadedBlockSize);
				}
			}
	
			pos += loadedBlockSize;
			pos += discardBytes;
	
			if(config->chunks && Signal->Blocks[i].type >= TYPE_SILENCE)
			{
				if(!CreateChunksFolder(config))
					return 0;
				sprintf(tempName, "Chunks%c%03ld_%s_Processed_%s_%03d_chunk", FOLDERCHAR, i, 
					GenerateFileNamePrefix(config), GetBlockName(config, i), 
					GetBlockSubIndex(config, i));
				ComposeFileName(Name, tempName, ".wav", config);
				SaveWAVEChunk(Name, Signal, buffer, 0, loadedBlockSize, 0, config);
			}
	
			i++;
		}

		// clear the rest of the buffer
		memset(Signal->Samples + pos, 0, (sizeof(char)*(Signal->header.data.DataSize - pos)));

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
	
		if(fwrite(Signal->Samples, 1, sizeof(char)*Signal->header.data.DataSize, processed) !=
				sizeof(char)*Signal->header.data.DataSize)
		{
			logmsg("\tCould not write samples to processed file\n");
			return (0);
		}
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
		logmsg(" - clk: iFFTW on Audio chunks took %0.2fs\n", elapsedSeconds);
	}

	free(buffer);
	freeWindows(&windows);

	return 1;
}

int ProcessSamples(AudioBlocks *AudioArray, int16_t *samples, size_t size, long samplerate, double *window, parameters *config, int reverse, AudioSignal *Signal)
{
	fftw_plan		p = NULL, pBack = NULL;
	char			channel = 0;
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
	monoSignalSize = stereoSignalSize/Signal->AudioChannels;	 // 4 is 2 16 bit values
	seconds = (double)size/((double)samplerate*Signal->AudioChannels);

	if(config->ZeroPad)  /* disabled by default */
		zeropadding = GetZeroPadValues(&monoSignalSize, &seconds, samplerate);

	boxsize = roundFloat(seconds);

	startBin = floor(config->startHz*boxsize);
	endBin = floor(config->endHz*boxsize);

	signal = (double*)malloc(sizeof(double)*(monoSignalSize+1));
	if(!signal)
	{
		logmsg("Not enough memory (malloc)\n");
		return(0);
	}
	spectrum = (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*(monoSignalSize/2+1));
	if(!spectrum)
	{
		logmsg("Not enough memory (fftw_malloc)\n");
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
				logmsg("FFTW failed to create FFTW_MEASURE reverse plan\n");
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

	if(Signal->AudioChannels == 1)
		channel = CHANNEL_LEFT;
	else
		channel = CHANNEL_STEREO;

	for(i = 0; i < monoSignalSize - zeropadding; i++)
	{
		if(channel == CHANNEL_LEFT)
		{
			signal[i] = (double)samples[i*Signal->AudioChannels];
			if(Signal->AudioChannels == 2)
				samples[i*2+1] = 0;
		}
		if(channel == CHANNEL_RIGHT)
		{
			signal[i] = (double)samples[i*2+1];
			samples[i*2] = 0;
		}
		if(channel == CHANNEL_STEREO)
		{
			signal[i] = ((double)samples[i*2]+(double)samples[i*2+1])/2.0;
			samples[i*2] = signal[i];
			samples[i*2+1] = signal[i];
		}

		if(window)
			signal[i] = (int16_t)((double)signal[i]*window[i]);
	}

	fftw_execute(p); 
	fftw_destroy_plan(p);
	p = NULL;

	if(!reverse)
	{
		AudioArray->fftwValues.spectrum = spectrum;
		AudioArray->fftwValues.size = monoSignalSize;
		AudioArray->seconds = seconds;

		if(!FillFrequencyStructures(NULL, AudioArray, config))
			return 0;
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

		if(AudioArray->freqRight)
		{
			for(int j = 0; j < config->MaxFreq; j++)
			{
				if(!AudioArray->freqRight[j].hertz)
					break;
				if(AudioArray->freqRight[j].amplitude < MinAmplitude)
					MinAmplitude = AudioArray->freqRight[j].amplitude;
			}
		}

		CutOff = MinAmplitude;
		if(CutOff < config->significantAmplitude)
			CutOff = config->significantAmplitude;

		if(!config->ignoreFloor && Signal->hasSilenceBlock &&
			CutOff < Signal->floorAmplitude && Signal->floorAmplitude != 0.0)
			CutOff = Signal->floorAmplitude;

		//Process the defined frequency spectrum
		for(i = 1; i < floor(boxsize*(samplerate/2)); i++)
		{
			double amplitude = 0, magnitude = 0;
			int blank = 0;
	
			magnitude = CalculateMagnitude(spectrum[i], monoSignalSize);
			amplitude = CalculateAmplitude(magnitude, Signal->MaxMagnitude.magnitude, config);

			if(amplitude <= CutOff)
				blank = 1;
			if(i < startBin || i > endBin)
				blank = 1;

			if(config->invert)
				blank = !blank;

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
	
		for(i = 0; i < monoSignalSize - zeropadding; i++)
		{
			double value;

			// reversing window causes distortion since we have zeroes
			// but we do want t see the windows in the iFFT anyway
			// uncomment if needed
			//if(window)
			//value = (signal[i]/window[i])/monoSignalSize;
			//else
			value = signal[i]/monoSignalSize; /* check CalculateMagnitude if changed */
			if(channel == CHANNEL_LEFT)
			{
				samples[i*Signal->AudioChannels] = round(value);
				if(Signal->AudioChannels == 2)
					samples[i*2+1] = 0;
			}
			if(channel == CHANNEL_RIGHT)
			{
				samples[i*2] = 0;
				samples[i*2+1] = round(value);
			}
			if(channel == CHANNEL_STEREO)
			{
				samples[i*2] = round(value);
				samples[i*2+1] = round(value);
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
	config->useCompProfile = 0;
	config->executefft = 1;

	while ((c = getopt (argc, argv, "qnhvzcklyCBis:e:f:t:p:w:r:P:IY:0:")) != -1)
	switch (c)
	  {
	  case 'h':
		PrintUsage_wave();
		return 0;
		break;
	  case 'n':
		config->executefft = 0;
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
	  case 'i':
		config->ignoreFloor = 1;   // RELEVANT HERE!
		break;
	  case 'y':
		config->debugSync = 1;
		break;
	  case 's':
		config->startHz = atoi(optarg);
		if(config->startHz < 1 || config->startHz > END_HZ-100)
			config->startHz = START_HZ;
		break;
	  case 'e':
		config->endHz = atof(optarg);
		if(config->endHz < START_HZ*2.0 || config->endHz > END_HZ)
			config->endHz = END_HZ;
		break;
	  case 'f':
		config->MaxFreq = atoi(optarg);
		if(config->MaxFreq < 1 || config->MaxFreq > MAX_FREQ_COUNT)
			config->MaxFreq = MAX_FREQ_COUNT;
		break;
	  case 'p':
		config->significantAmplitude = atof(optarg);
		if(config->significantAmplitude <= -120.0 || config->significantAmplitude >= -1.0)
			config->significantAmplitude = SIGNIFICANT_VOLUME;
		config->origSignificantAmplitude = config->significantAmplitude;
		break;
	  case 'q':
		config->compressToBlocks = 1;
		break;
	  case 'Y':
		config->videoFormatRef = atof(optarg);
		if(config->videoFormatRef < 0 || config->videoFormatRef > MAX_SYNC)  // We'll confirm this later
		{
			logmsg("\tProfile can have up to %d types\n", MAX_SYNC);
			return 0;
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
	  case 'B':
		config->channelBalance = 0;
		break;
	  case 'C':
		config->useCompProfile = 1;
		break;
	  case 'I':
		config->ignoreFrameRateDiff = 1;
		break;
	  case '0':
		sprintf(config->outputPath, "%s", optarg);
		break;
	  case '?':
		if (optopt == 'r')
		  logmsg("\t ERROR:  Reference File -%c requires an argument.\n", optopt);
		else if (optopt == 'w')
		  logmsg("\t ERROR:  FFT Window option -%c requires an argument: n,t,f or h\n", optopt);
		else if (optopt == 'f')
		  logmsg("\t ERROR:  Max # of frequencies to use from FFTW -%c requires an argument: 1-%d\n", optopt, MAX_FREQ_COUNT);
		else if (optopt == 's')
		  logmsg("\t ERROR:  Min frequency range for FFTW -%c requires an argument: %d-%d\n", 1, END_HZ-100, optopt);
		else if (optopt == 'e')
		  logmsg("\t ERROR:  Max frequency range for FFTW -%c requires an argument: %d-%d\n", START_HZ*2, END_HZ, optopt);
		else if (optopt == 'P')
		  logmsg("\t ERROR:  Profile File -%c requires a file argument\n", optopt);
		else if (optopt == 'Y')
		  logmsg("\t ERROR:  Reference format: needs a number with a selection from the profile\n");
		else if (isprint (optopt))
		  logmsg("\t ERROR:  Unknown option `-%c'.\n", optopt);
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
		logmsg("ERROR: Invalid argument %s\n", argv[index]);
		return 0;
	}

	if(!ref)
	{
		logmsg("ERROR: Please define the reference audio file\n");
		return 0;
	}

	if(config->endHz <= config->startHz)
	{
		logmsg("ERROR: Invalid frequency range for FFTW (%d Hz to %d Hz)\n", config->startHz, config->endHz);
		return 0;
	}

	file = fopen(config->referenceFile, "rb");
	if(!file)
	{
		logmsg("\nERROR: Could not open REFERENCE file: \"%s\"\n", config->referenceFile);
		return 0;
	}
	fclose(file);

	if(config->MaxFreq != FREQ_COUNT)
		logmsg("\tMax frequencies to use from FFTW are %d (default %d)\n", config->MaxFreq, FREQ_COUNT);
	if(config->startHz != START_HZ)
		logmsg("\tFrequency start range for FFTW is now %g (default %g)\n", config->startHz, START_HZ);
	if(config->endHz != END_HZ)
		logmsg("\tFrequency end range for FFTW is now %g (default %g)\n", config->endHz, END_HZ);
	if(config->window != 'n')
		logmsg("\tA %s window will be applied to each block to be compared\n", GetWindow(config->window));
	else
		logmsg("\tNo window (rectangle) will be applied to each block to be compared\n");
	if(config->ZeroPad)
		logmsg("\tFFT bins will be aligned to 1Hz, this is slower\n");
	if(config->ignoreFloor)
		logmsg("\tIgnoring Silence block noise floor\n");
	if(config->invert)
		logmsg("\tSaving Discarded part fo the signal to WAV file\n");
	if(config->chunks)
		logmsg("\tSaving WAV chunks to individual files\n");

	return 1;
}

void PrintUsage_wave()
{
	logmsg("  usage: mdwave -P profile.mdf -r audio.wav\n");
	logmsg("   FFT and Analysis options:\n");
	logmsg("	 -c: Enable Audio <c>hunk creation, an individual WAV for each block\n");
	logmsg("	 -w: enable <w>indowing. Default is a custom Tukey window.\n");
	logmsg("		'n' none, 't' Tukey, 'h' Hann, 'f' FlatTop & 'm' Hamming\n");
	logmsg("	 -i: <i>gnores the silence block noise floor if present\n");
	logmsg("	 -f: Change the number of <f>requencies to use from FFTW\n");
	logmsg("	 -s: Defines <s>tart of the frequency range to compare with FFT\n");
	logmsg("	 -e: Defines <e>nd of the frequency range to compare with FFT\n");
	logmsg("	 -t: Defines the <t>olerance when comparing amplitudes in dBFS\n");
	logmsg("	 -z: Uses Zero Padding to equal 1 Hz FFT bins\n");
	logmsg("	 -B: Do not do stereo channel audio <B>alancing\n");
	logmsg("	 -C: Use <C>omparison framerate profile in 'No-Sync' compare mode\n");
	logmsg("	 -Y: Define the Video Format from the profile\n");
	logmsg("   Output options:\n");
	logmsg("	 -v: Enable <v>erbose mode, spits all the FFTW results\n");
	logmsg("	 -l: Do not <l>og output to file [reference]_vs_[compare].txt\n");
	logmsg("	 -k: cloc<k> FFTW operations\n");
	logmsg("	 -0: Change output folder\n");
}

void Header_wave(int log)
{
	char title1[] = " MDWave " MDWVERSION " (MDFourier Companion)\n [240p Test Suite Fourier Audio compare tool]\n";
	char title2[] = "Artemio Urbina 2019-2020 free software under GPL - http://junkerhq.net/MDFourier\n";

	if(log)
		logmsg("%s%s", title1, title2);
	else
		printf("%s%s", title1, title2);
}
