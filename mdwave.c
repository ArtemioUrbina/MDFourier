/* 
 * MDFourier
 * A Fourier Transform analysis tool to compare game console audio
 * http://junkerhq.net/MDFourier/
 *
 * Copyright (C)2019-2026 Artemio Urbina
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

#define MDWVERSION MDVERSION
#define FORWARD_FFTW 0
#define REVERSE_FFTW 1

#include "mdfourier.h"
#include "log.h"
#include "windows.h"
#include "freq.h"
#include "diff.h"
#include "cline.h"
#include "sync.h"
#include "balance.h"
#include "loadfile.h"
#include "profile.h"

int ProcessSignalMDW(AudioSignal *Signal, parameters *config);
int ExecuteDFFT(AudioBlocks *AudioArray, double *samples, double *discarded, long int size, double samplerate, double *window, parameters *config, int fftw_direction, AudioSignal *Signal);
int ExecuteDFFTInternal(AudioBlocks *AudioArray, double *samples, double *discarded, long int size, double samplerate, double *window, char channel, parameters *config, int fftw_direction, AudioSignal *Signal);
int commandline_wave(int argc , char *argv[], parameters *config);
void PrintUsage_wave(void);
void Header_wave(int log);
void CleanUp(AudioSignal **ReferenceSignal, parameters *config);
int ExecuteMDWave(parameters *config);

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

	// load saved FFTW wisdom so plans are not measured again every run
	InitFFTW();

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

	if(ExecuteMDWave(&config) == 1)
	{
		logmsg("Aborting\n");
		return 1;
	}

	if(config.clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - clk: MDWave took %0.2fs\n", elapsedSeconds);
	}

	printf("\nResults stored in %s%s\n", 
		config.outputPath,
		config.folderName);


	fftw_cleanup();

	if(IsLogEnabled())
		endLog();


	return 0;
}

int ExecuteMDWave(parameters *config)
{
	AudioSignal  		*ReferenceSignal = NULL;
	char 				*MainPath = NULL;

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

	/* Balance check */
	if(config->channelBalance && !config->noSyncProfile)
	{
		if(ReferenceSignal->AudioChannels == 2)
		{
			int block = NO_INDEX;
			char *name = NULL;
	
			if(config->stereoBalanceBlock)
			{
				block = config->stereoBalanceBlock;
				name = GetBlockName(config, block);
				if(!name)
				{
					logmsg("ERROR: Invalid Mono Balance Block %d\n", block);
					CleanUp(&ReferenceSignal, config);
					return 1;
				}
			}
			else
			{
				block = GetFirstMonoIndex(config);
				logmsg("- WARNING: MonoBalanceBlock was 0, Using first Mono Block\n");
			}
			if(block != NO_INDEX)
			{
				logmsg("\n* Comparing Stereo channel amplitude\n");
				if(config->verbose) {
					logmsg(" - Mono block used for balance: %s# %d\n", 
						name, GetBlockSubIndex(config, block));
				}
				if(CheckBalance(ReferenceSignal, block, config) == 0)
				{
					CleanUp(&ReferenceSignal, config);
					return 1;
				}
			}
			else
			{
				logmsg(" - WARNING: No mono block for stereo balance check\n");
				config->channelBalance = -1;
			}
		}
	}

	SetAmplitudeMatchByDurationMDW(ReferenceSignal, config);

	logmsg("\n* Processing Audio\n");
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

char *GenerateFileNamePrefix(int discarded)
{
	return(discarded ? "2_Discarded" : "1_Used");
}

int SaveProcessedChunk(AudioSignal *Signal, long int block, double *buffer, long int size, int discarded, parameters *config)
{
	char	Name[BUFFER_SIZE*2+256], tempName[BUFFER_SIZE];

	sprintf(tempName, "Chunks%cProcessed%c%03ld_%s_%s_%03d_chunk", FOLDERCHAR, FOLDERCHAR, block, 
		GenerateFileNamePrefix(discarded), GetBlockName(config, block), 
		GetBlockSubIndex(config, block));
	ComposeFileName(Name, tempName, ".wav", config);
	return SaveWAVEChunk(Name, Signal, buffer, 0, size, 0, config);
}

int CreateChunksFolder(parameters *config)
{
	char name[BUFFER_SIZE*4];

	sprintf(name, "%s%cChunks", config->folderName, FOLDERCHAR);
	if(!CreateFolder(name))
		return 0;

	sprintf(name, "%s%cChunks%cProcessed", config->folderName, FOLDERCHAR, FOLDERCHAR);
	if(!CreateFolder(name))
		return 0;

	sprintf(name, "%s%cChunks%cSource", config->folderName, FOLDERCHAR, FOLDERCHAR);
	if(!CreateFolder(name))
		return 0;
	return 1;
}

int ProcessSignalMDW(AudioSignal *Signal, parameters *config)
{
	long int		pos = 0;
	double			longest = 0;
	double			*sampleBuffer = NULL, *discardBuffer = NULL, *discardSignal = NULL;
	long int		sampleBufferSize = 0;
	windowManager	windows;
	double			*windowUsed = NULL;
	long int		loadedBlockSize = 0, i = 0, syncAdvance = 0;
	struct timespec	start, end;
	char			Name[BUFFER_SIZE*2+256];
	int				discardSamples = 0, syncinternal = 0, hadSync = 0;
	double			leftDecimals = 0;

	pos = Signal->startOffset;

	longest = FramesToSeconds(Signal->framerate, GetLongestElementFrames(config));
	if(!longest)
	{
		logmsg("\tERROR: Block definitions are invalid, total length is 0.\n");
		return 0;
	}

	sampleBufferSize = SecondsToSamples(Signal->SampleRate, longest, Signal->AudioChannels, NULL, NULL);
	sampleBuffer = (double*)malloc(sampleBufferSize*sizeof(double));
	if(!sampleBuffer)
	{
		logmsg("\tERROR: malloc failed.\n");
		return(0);
	}

	if(!initWindows(&windows, Signal->SampleRate, config->window, config))
	{
		logmsg("\tERROR: Could not create FFTW windows.\n");
		free(sampleBuffer);
		return 0;
	}

	CompareFrameRatesMDW(Signal, GetMSPerFrame(Signal, config), config);

	if(config->chunks && !CreateChunksFolder(config))
	{
		logmsg("\tERROR: Could not create output folders.\n");
		free(sampleBuffer);
		freeWindows(&windows);
		return 0;
	}

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

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

		if(Signal->Blocks[i].maskType == MASK_USE_WINDOW)
			difference = GetSampleSizeDifferenceByFrameRate(framerate, frames, Signal->SampleRate, Signal->AudioChannels, config);

		loadedBlockSize = SecondsToSamples(Signal->SampleRate, duration, Signal->AudioChannels, &discardSamples, &leftDecimals);

		windowUsed = NULL;
		if(Signal->Blocks[i].type >= TYPE_SILENCE || Signal->Blocks[i].type == TYPE_WATERMARK)
		{
			if(!syncinternal && Signal->Blocks[i].maskType == MASK_USE_WINDOW)
				windowUsed = getWindowByLength(&windows, frames, cutFrames, config->smallerFramerate, config);
			else
				windowUsed = getWindowByLength(&windows, frames, cutFrames, framerate, config);
		}

		//logmsg("Loaded %ld Left %ld Discard %ld difference %ld Decimals %g\n", loadedBlockSize, discardSamples, difference, leftDecimals);
		if(pos + loadedBlockSize > Signal->numSamples)
		{
			if(i != config->types.totalBlocks - 1)
			{
				config->smallFile |= Signal->role;
				logmsg("\tUnexpected end of File, please record the full Audio Test from the 240p Test Suite.\n");
				if(config->verbose)
					logmsg("load: %ld size: %ld exceed: %ld pos: %ld limit: %ld\n", loadedBlockSize, sampleBufferSize, pos + loadedBlockSize, pos, Signal->numSamples);
			}
			break;
		}

		// Clean Buffer and fill it
		memset(sampleBuffer, 0, sampleBufferSize*sizeof(double));
		memcpy(sampleBuffer, Signal->Samples + pos, loadedBlockSize*sizeof(double));

		if(Signal->Blocks[i].type >= TYPE_SILENCE && config->executefft)
		{
			if(!ExecuteDFFT(&Signal->Blocks[i], sampleBuffer, NULL, loadedBlockSize-difference, Signal->SampleRate, windowUsed, config, FORWARD_FFTW, Signal))
			{
				free(sampleBuffer);
				freeWindows(&windows);
				return 0;
			}
		}
		
		if(config->chunks)
		{
			sprintf(Name, "%s%cChunks%cSource%c%03ld_0_%010ld_%s_%03d_chunk.wav", 
				config->folderName, FOLDERCHAR, FOLDERCHAR, FOLDERCHAR,
				i, SamplesForDisplay(pos+syncAdvance, Signal->AudioChannels), 
				GetBlockName(config, i), GetBlockSubIndex(config, i));
			SaveWAVEChunk(Name, Signal, sampleBuffer, 0, loadedBlockSize, 0, config); 
		}

		pos += loadedBlockSize;
		pos += discardSamples;

		if(Signal->Blocks[i].type == TYPE_INTERNAL_KNOWN)
		{
			if(!ProcessInternalSync(Signal, i, pos, &syncinternal, &syncAdvance, TYPE_INTERNAL_KNOWN, config))
			{
				free(sampleBuffer);
				freeWindows(&windows);
				return 0;
			}
			
			if(!syncinternal)
				syncAdvance = 0;

			hadSync = 1;
		}

		if(Signal->Blocks[i].type == TYPE_INTERNAL_UNKNOWN)
		{
			if(!ProcessInternalSync(Signal, i, pos, &syncinternal, &syncAdvance, TYPE_INTERNAL_UNKNOWN, config))
			{
				free(sampleBuffer);
				freeWindows(&windows);
				return 0;
			}
			
			if(!syncinternal)
				syncAdvance = 0;

			hadSync = 1;
		}

		i++;
	}

	if(config->executefft)
	{
		GlobalNormalize(Signal, config);
		CalculateFrequencyBrackets(Signal, config);
	
		if(Signal->hasSilenceBlock && !config->ignoreFloor) // analyze noise floor if available
		{
			FindFloor(Signal, config);
	
			if(Signal->floorAmplitude != 0.0 && Signal->floorAmplitude > config->significantAmplitude)
				config->significantAmplitude = Signal->floorAmplitude;
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

		discardSignal = (double*)malloc(Signal->numSamples*sizeof(double));
		discardBuffer = (double*)malloc(sampleBufferSize*sizeof(double));
		if(!discardSignal || !discardBuffer)
		{
			logmsg("\tERROR: Not enough memory for the Discarded signal.\n");
			if(discardSignal)
				free(discardSignal);
			if(discardBuffer)
				free(discardBuffer);
			free(sampleBuffer);
			freeWindows(&windows);
			return 0;
		}
		memcpy(discardSignal, Signal->Samples, Signal->numSamples*sizeof(double));
	
		// Clean up everything again
		pos = Signal->startOffset;		
		discardSamples = 0;
		leftDecimals = 0;
		syncinternal = 0;
		i = 0;
	
		// redo after processing
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
	
			if(Signal->Blocks[i].maskType == MASK_USE_WINDOW)
				difference = GetSampleSizeDifferenceByFrameRate(framerate, frames, Signal->SampleRate, Signal->AudioChannels, config);

			loadedBlockSize = SecondsToSamples(Signal->SampleRate, duration, Signal->AudioChannels, &discardSamples, &leftDecimals);

			windowUsed = NULL;
			if(Signal->Blocks[i].type >= TYPE_SILENCE || Signal->Blocks[i].type == TYPE_WATERMARK)
			{
				if(!syncinternal && Signal->Blocks[i].maskType == MASK_USE_WINDOW)
					windowUsed = getWindowByLength(&windows, frames, cutFrames, config->smallerFramerate, config);
				else
					windowUsed = getWindowByLength(&windows, frames, cutFrames, framerate, config);
			}
			
			//logmsg("Loaded %ld Left %ld Discard %ld difference %ld Decimals %g\n", loadedBlockSize, discardSamples, difference, leftDecimals);
			if(pos + loadedBlockSize > Signal->numSamples)
			{
				if(i != config->types.totalBlocks - 1)
				{
					config->smallFile |= Signal->role;
					logmsg("\tUnexpected end of File, please record the full Audio Test from the 240p Test Suite.\n");
					if(config->verbose)
						logmsg("load: %ld size: %ld exceed: %ld pos: %ld limit: %ld\n", loadedBlockSize, sampleBufferSize, pos + loadedBlockSize, pos, Signal->numSamples);
				}
				break;
			}

			// Clean Buffers and fill them with the original block
			memset(sampleBuffer, 0, sampleBufferSize*sizeof(double));
			memcpy(sampleBuffer, Signal->Samples + pos, loadedBlockSize*sizeof(double));
			memcpy(discardBuffer, sampleBuffer, sampleBufferSize*sizeof(double));

			// Empty both output signals, and overlap
			if(pos > 4 && pos+loadedBlockSize+discardSamples+4 <= Signal->numSamples)
			{
				memset(Signal->Samples + pos-4, 0, (loadedBlockSize+discardSamples+4)*sizeof(double));
				memset(discardSignal + pos-4, 0, (loadedBlockSize+discardSamples+4)*sizeof(double));
			}
			else
			{
				memset(Signal->Samples + pos, 0, loadedBlockSize*sizeof(double));
				memset(discardSignal + pos, 0, loadedBlockSize*sizeof(double));
			}
		
			if(Signal->Blocks[i].type >= TYPE_SILENCE)
			{
				// Used goes to sampleBuffer, Discarded to discardBuffer
				if(!ExecuteDFFT(&Signal->Blocks[i], sampleBuffer, discardBuffer, loadedBlockSize-difference, Signal->SampleRate, windowUsed, config, REVERSE_FFTW, Signal))
				{
					free(discardSignal);
					free(discardBuffer);
					free(sampleBuffer);
					freeWindows(&windows);
					return 0;
				}
			}
			else
			{
				// Used keeps only the sync pulses,
				// Discarded keeps all of them for reference
				if(Signal->Blocks[i].type != TYPE_SYNC)
					memset(sampleBuffer, 0, loadedBlockSize*sizeof(double));
			}

			// Fill back both signals
			memcpy(Signal->Samples + pos, sampleBuffer, loadedBlockSize*sizeof(double));
			memcpy(discardSignal + pos, discardBuffer, loadedBlockSize*sizeof(double));

			if(config->chunks && (Signal->Blocks[i].type >= TYPE_SILENCE || Signal->Blocks[i].type == TYPE_WATERMARK))
			{
				SaveProcessedChunk(Signal, i, sampleBuffer, loadedBlockSize, 0, config);
				SaveProcessedChunk(Signal, i, discardBuffer, loadedBlockSize, 1, config);
			}
	
			pos += loadedBlockSize;
			pos += discardSamples;

			// Use original framerate for CD-DA chunks
			if(Signal->Blocks[i].type == TYPE_INTERNAL_KNOWN || Signal->Blocks[i].type == TYPE_INTERNAL_UNKNOWN)
				syncinternal = !syncinternal;

			i++;
		}

		// clear the rest of the buffers
		memset(Signal->Samples + pos, 0, (sizeof(double)*(Signal->numSamples - pos)));
		memset(discardSignal + pos, 0, (sizeof(double)*(Signal->numSamples - pos)));

		ComposeFileName(Name, GenerateFileNamePrefix(0), ".wav", config);
		if(!SaveWAVEChunk(Name, Signal, Signal->Samples, 0, Signal->numSamples, 0, config))
		{
			free(discardSignal);
			free(discardBuffer);
			free(sampleBuffer);
			freeWindows(&windows);
			return 0;
		}

		ComposeFileName(Name, GenerateFileNamePrefix(1), ".wav", config);
		if(!SaveWAVEChunk(Name, Signal, discardSignal, 0, Signal->numSamples, 0, config))
		{
			free(discardSignal);
			free(discardBuffer);
			free(sampleBuffer);
			freeWindows(&windows);
			return 0;
		}

		free(discardSignal);
		discardSignal = NULL;
		free(discardBuffer);
		discardBuffer = NULL;
	}

	// save frequency unprocesed wav if requested with -n, for internal sync and verification
	if(hadSync && !config->executefft)
	{
		ComposeFileName(Name, "SyncRemoved", ".wav", config);
		if(!SaveWAVEChunk(Name, Signal, Signal->Samples, 0, Signal->numSamples, 0, config))
		{
			free(sampleBuffer);
			freeWindows(&windows);
			return 0;
		}
	}

	if(config->clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - clk: iFFTW on Audio chunks took %0.2fs\n", elapsedSeconds);
	}

	free(sampleBuffer);
	freeWindows(&windows);

	return 1;
}

int ExecuteDFFT(AudioBlocks *AudioArray, double *samples, double *discarded, long int size, double samplerate, double *window, parameters *config, int fftw_direction, AudioSignal *Signal)
{
	int AudioChannels = Signal->AudioChannels;
	char channel = CHANNEL_STEREO;

	if(AudioChannels == 1)
		channel = CHANNEL_LEFT;
	else
	{
		// If we are procesing a mono signal in a stereo file, use both channels
		if(AudioArray->channel == CHANNEL_MONO)
			channel = CHANNEL_STEREO;

		if(AudioArray->channel == CHANNEL_STEREO)
		{
			channel = CHANNEL_RIGHT;
			if(!ExecuteDFFTInternal(AudioArray, samples, discarded, size, samplerate, window, channel, config, fftw_direction, Signal))
				return 0;
			channel = CHANNEL_LEFT;
		}
	}

	if(!ExecuteDFFTInternal(AudioArray, samples, discarded, size, samplerate, window, channel, config, fftw_direction, Signal))
		return 0;

	if(fftw_direction == FORWARD_FFTW)
	{
		if(!FillFrequencyStructures(Signal, AudioArray, config))
			return 0;
	}

	return 1;
}

int ExecuteDFFTInternal(AudioBlocks *AudioArray, double *samples, double *discarded, long int size, double samplerate, double *window, char channel, parameters *config, int fftw_direction, AudioSignal *Signal)
{
	fftw_plan		p = NULL, pBack = NULL;	/* shared per size, from the plan cache */
	long int		stereoSignalSize = 0, blanked = 0;	
	long int		i = 0, monoSignalSize = 0, zeropadding = 0; 
	double			*signal = NULL;
	fftw_complex	*spectrum = NULL, *spectrumUsed = NULL, *spectrumDiscard = NULL;
	double			boxsize = 0, seconds = 0;
	double			CutOff = 0;
	long int		startBin = 0, endBin = 0;
	int				AudioChannels = Signal->AudioChannels;
	
	if(!AudioArray)
	{
		logmsg("No Array for results\n");
		return 0;
	}

	//logmsg("discardMDW %d fftw_direction %s\n", config->discardMDW, fftw_direction == FORWARD_FFTW ? "forward" : "reverse");
	stereoSignalSize = (long)size;
	monoSignalSize = stereoSignalSize/AudioChannels;
	seconds = (double)size/(samplerate*(double)AudioChannels);

	if(config->ZeroPad)  /* disabled by default */
		zeropadding = GetZeroPadValues(&monoSignalSize, &seconds, samplerate, 1);

	// Round to 3 decimal places so that 48kHz and 44 kHz line up
	boxsize = RoundFloat(AudioArray->seconds, 3);

	startBin = floor(config->startHz*boxsize);
	endBin = floor(config->endHz*boxsize);

	// bins are from the mono transform, size counts all channels
	if(Signal->nyquistLimit && endBin > monoSignalSize/2)
		endBin = ceil(monoSignalSize/2);

	signal = (double*)fftw_malloc(sizeof(double)*(monoSignalSize+1));
	if(!signal)
	{
		logmsg("Not enough memory (malloc)\n");
		return(0);
	}
	spectrum = (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*(monoSignalSize/2+1));
	if(!spectrum)
	{
		logmsg("Not enough memory (fftw_malloc)\n");
		fftw_free(signal);
		return(0);
	}

	p = GetR2CPlan(monoSignalSize);
	if(!p)
	{
		logmsg("FFTW failed to create FFTW_MEASURE plan\n");
		fftw_free(signal);
		fftw_free(spectrum);
		return 0;
	}

	if(fftw_direction == REVERSE_FFTW)
	{
		pBack = GetC2RPlan(monoSignalSize);
		if(!pBack)
		{
			logmsg("FFTW failed to create FFTW_MEASURE reverse plan\n");
			fftw_free(signal);
			fftw_free(spectrum);
			return 0;
		}
	}

	memset(signal, 0, sizeof(double)*(monoSignalSize+1));
	memset(spectrum, 0, sizeof(fftw_complex)*(monoSignalSize/2+1));

	for(i = 0; i < monoSignalSize - zeropadding; i++)
	{
		if(channel == CHANNEL_LEFT)
			signal[i] = samples[i*AudioChannels];
		if(channel == CHANNEL_RIGHT)
			signal[i] = samples[i*AudioChannels+1];
		if(channel == CHANNEL_STEREO)
			signal[i] = (samples[i*AudioChannels]+samples[i*AudioChannels+1])/2.0;

		if(window)
			signal[i] = signal[i]*window[i];
	}

	fftw_execute_dft_r2c(p, signal, spectrum);
	p = NULL;

	if(fftw_direction == FORWARD_FFTW)
	{
		if(channel != CHANNEL_RIGHT)
		{
			AudioArray->fftwValues.spectrum = spectrum;
			AudioArray->fftwValues.size = monoSignalSize;
			AudioArray->fftwValues.ENBW = monoSignalSize;
		}
		else
		{
			AudioArray->fftwValuesRight.spectrum = spectrum;
			AudioArray->fftwValuesRight.size = monoSignalSize;
			AudioArray->fftwValuesRight.ENBW = monoSignalSize;
		}
		AudioArray->seconds = seconds;
	}

	if(fftw_direction == REVERSE_FFTW)
	{
		long int		endBinLimit = 0;
		double			MinAmplitude = 0;
		Frequency		*targetFreq = NULL;

		// Find the Max magnitude for frequency at -f cuttoff
		if(channel != CHANNEL_RIGHT)
			targetFreq = AudioArray->freq;
		else
			targetFreq = AudioArray->freqRight;

		if(!targetFreq || !discarded)
		{
			logmsg("Invalid channel data\n");
			fftw_free(signal);
			fftw_free(spectrum);
			return 0;
		}

		//	The windowed spectrum decides which bins are kept
		spectrumUsed = (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*(monoSignalSize/2+1));
		spectrumDiscard = (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*(monoSignalSize/2+1));
		if(!spectrumUsed || !spectrumDiscard)
		{
			logmsg("Not enough memory (fftw_malloc)\n");
			if(spectrumUsed)
				fftw_free(spectrumUsed);
			if(spectrumDiscard)
				fftw_free(spectrumDiscard);
			fftw_free(signal);
			fftw_free(spectrum);
			return 0;
		}

		if(window)
		{
			memset(signal, 0, sizeof(double)*(monoSignalSize+1));
			for(i = 0; i < monoSignalSize - zeropadding; i++)
			{
				if(channel == CHANNEL_LEFT)
					signal[i] = samples[i*AudioChannels];
				if(channel == CHANNEL_RIGHT)
					signal[i] = samples[i*AudioChannels+1];
				if(channel == CHANNEL_STEREO)
					signal[i] = (samples[i*AudioChannels]+samples[i*AudioChannels+1])/2.0;
			}
			fftw_execute_dft_r2c(GetR2CPlan(monoSignalSize), signal, spectrumUsed);
		}
		else
			memcpy(spectrumUsed, spectrum, sizeof(fftw_complex)*(monoSignalSize/2+1));
		memcpy(spectrumDiscard, spectrumUsed, sizeof(fftw_complex)*(monoSignalSize/2+1));

		for(int j = 0; j < config->MaxFreq; j++)
		{
			if(!targetFreq[j].hertz)
				break;
			if(targetFreq[j].amplitude < MinAmplitude)
				MinAmplitude = targetFreq[j].amplitude;
		}

		CutOff = MinAmplitude;
		if(CutOff < config->significantAmplitude)
			CutOff = config->significantAmplitude;

		if(!config->ignoreFloor && Signal->hasSilenceBlock &&
			CutOff < Signal->floorAmplitude && Signal->floorAmplitude != 0.0)
			CutOff = Signal->floorAmplitude;

		//Process the defined frequency spectrum
		endBinLimit = floor(boxsize*(samplerate/2));
		if(endBinLimit > monoSignalSize/2)
			endBinLimit = monoSignalSize/2;
		
		for(i = 1; i < endBinLimit; i++)
		{
			double amplitude = 0, magnitude = 0;
			int blank = 0;
	
			magnitude = CalculateMagnitude(&spectrum[i], monoSignalSize);
			amplitude = CalculateAmplitude(magnitude, Signal->MaxMagnitude.magnitude);

			// limit by noise cut frequncy count/amplitude
			if(amplitude <= CutOff)
				blank = 1;

			// Limit startHz to endHz
			if(i < startBin || i > endBin)
				blank = 1;

			if(blank)
			{
				fftw_complex filter;

				// This should never be done as such
				// A proper filter should be used, or you'll get
				// ringing artifacts via Gibbs phenomenon
				// Here it "works" because we are just
				// "visualizing" the results
				filter = 0;
				spectrumUsed[i] = spectrumUsed[i]*filter; // removed from Used
				blanked ++;
			}
			else
				spectrumDiscard[i] = 0;	// kept in Used, so not in Discarded
		}
		
		// Magic! iFFTW
		for(int out = 0; out < 2; out++)
		{
			double	*target = out == 0 ? samples : discarded;

			fftw_execute_dft_c2r(pBack, out == 0 ? spectrumUsed : spectrumDiscard, signal);
		
			for(i = 0; i < monoSignalSize - zeropadding; i++)
			{
				double value;

				// reversing window causes distortion since we have zeroes
				// but we do want to see the windows in the iFFT anyway
				// uncomment if needed
				//if(window && window[i])
				//value = (signal[i]/window[i])/monoSignalSize;
				//else
				value = signal[i]/monoSignalSize; /* check CalculateMagnitude if changed */
				if(channel == CHANNEL_LEFT || channel == CHANNEL_STEREO || channel == CHANNEL_MONO)
					target[i*AudioChannels] = value;
				if(channel == CHANNEL_RIGHT || channel == CHANNEL_STEREO)
					target[i*AudioChannels+1] = value;
			}
		}
		pBack = NULL;

		//logmsg("Blanked %ld frequencies from a total of %ld\n", blanked, monoSignalSize/2);
		if(blanked > config->maxBlanked)
			config->maxBlanked = blanked;
		fftw_free(spectrum);
		fftw_free(spectrumUsed);
		fftw_free(spectrumDiscard);
	}

	fftw_free(signal);
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
	config->discardMDW = 0;
	config->chunks = 0;
	config->useCompProfile = 0;
	config->executefft = 1;

	while ((c = getopt (argc, argv, "qnhvzcklyCBis:e:f:m:p:w:r:P:IY:T0:9")) != -1)
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
		config->startHz = atof(optarg);
		if(config->startHz < 1.0 || config->startHz > MAX_HZ-100.0)
		{
			logmsg("-ERROR: Requested %g start frequency is out of range\n", atof(optarg));
			return 0;
		}
		break;
	  case 'e':
		config->endHz = atof(optarg);
		if(config->endHz < START_HZ*2.0)
		{
			logmsg("- ERROR: Requested %g end frequency is lower than possible\n", atof(optarg));
			return 0;
		}
		if(config->endHz > MAX_HZ)
		{
			logmsg("-ERROR: Requested %g end frequency is higher than possible\n", atof(optarg));
			return 0;
		}
		if(config->endHz > END_HZ)
			config->endHzPlot = config->endHz;
		break;
	  case 'f':
		config->MaxFreq = atoi(optarg);
		if(config->MaxFreq < 1 || config->MaxFreq > MAX_FREQ_COUNT)
		{
			logmsg("-ERROR: Number fo frequencies must be between %d and %d\n", 1, MAX_FREQ_COUNT);
			return 0;
		}
		break;
	  case 'm':
		{
			char manualType = '\0';
			long start = 0, end = 0;

			if(sscanf(optarg, "%c:%ld:%ld\n", &manualType, &start, &end) != 3 ||  
				(manualType != 'r' && manualType != 'c'))
			{
				logmsg("-ERROR: Invalid manual offset (-m) parameter: %s.\n", optarg);
				logmsg("  Must be of the form [r|c]:<start sample>:<end sample>\n");
				return 0;
			}

			if(end <= start)
			{
				logmsg("ERROR: For manual sample offset, ending offset must be bigger than the starting offset\n");
				return 0;
			}
			if(manualType == 'r')
			{
				config->ManualSyncRef = 1;
				config->ManualSyncRefStart = start;
				config->ManualSyncRefEnd = end;
				logmsg("- Reference ");
			}

			if(manualType == 'c')
			{
				config->ManualSyncComp = 1;
				config->ManualSyncCompStart = start;
				config->ManualSyncCompEnd = end;
				logmsg("- Comparison ");
			}
			logmsg("manual sample offset set %ld-%ld\n", start, end);
		}
		break;
	  case 'p':
		config->significantAmplitude = atof(optarg);
		if(config->significantAmplitude == 0)
		{
			config->noiseFloorAutoAdjust = 0;
			config->significantAmplitude = SIGNIFICANT_AMPLITUDE;
		}
		else if(config->significantAmplitude < -250.0 || config->significantAmplitude > -1.0)
		{
			logmsg("-ERROR: Significant amplitude must be between %d and %d\n", -1, -250);
			return 0;
		} else {
			config->ignoreFloor = 2;
			config->origSignificantAmplitude = config->significantAmplitude;
		}
		break;
	  case 'q':
		config->compressToBlocks = 1;
		break;
	  case 'Y':
		config->videoFormatRef = atoi(optarg);
		if(config->videoFormatRef < 0 || config->videoFormatRef > MAX_SYNC)  // We'll confirm this later
		{
			logmsg("- ERROR: Profile can have up to %d types\n", MAX_SYNC);
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
			case 'm':
				config->window = optarg[0];
				break;
			default:
				logmsg("- ERROR: Invalid Window for FFT option '%c'\n", optarg[0]);
				logmsg("\tUse n for None, t for Tukey window (default), f for Flattop, h for Hann or m for Hamming window\n");
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
	  case 'T':
		config->syncTolerance++;
		if(config->syncTolerance > 4)
			config->syncTolerance = 4;
		break;
	  case '0':
		sprintf(config->outputPath, "%s", optarg);
		break;
	  case '9':
		config->compressToBlocks = 1;
		break;
	  case '?':
		if (optopt == 'r')
		  logmsg("\t ERROR:  Reference File -%c requires an argument.\n", optopt);
		else if (optopt == 'w')
		  logmsg("\t ERROR:  FFT Window option -%c requires an argument: n,t,f,h or m\n", optopt);
		else if (optopt == 'f')
		  logmsg("\t ERROR:  Max # of frequencies to use from FFTW -%c requires an argument: 1-%d\n", optopt, MAX_FREQ_COUNT);
		else if (optopt == 's')
		  logmsg("\t ERROR:  Min frequency range for FFTW -%c requires an argument: %d-%d\n", optopt, 1, (int)(END_HZ-100));
		else if (optopt == 'e')
		  logmsg("\t ERROR:  Max frequency range for FFTW -%c requires an argument: %d-%d\n", optopt, (int)(START_HZ*2), (int)END_HZ);
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
		logmsg("ERROR: Invalid frequency range for FFTW (%g Hz to %g Hz)\n", config->startHz, config->endHz);
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
	if(config->discardMDW)
		logmsg("\tSaving Discarded part fo the signal to WAV file\n");
	if(config->chunks)
		logmsg("\tSaving WAV chunks to individual files\n");

	return 1;
}

void PrintUsage_wave(void)
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
	logmsg("	 -z: Uses Zero Padding to equal 1 Hz FFT bins\n");
	logmsg("	 -B: Do not do stereo channel audio <B>alancing\n");
	logmsg("	 -C: Use <C>omparison framerate profile in 'No-Sync' compare mode\n");
	logmsg("	 -Y: Define the Video Format from the profile\n");
	logmsg("	 -n: Just cut the wav file without performing DFFT\n");
	logmsg("	 -T: Increase Sync detection <T>olerance (ignore frequency, can be added up to -TTT)\n");
	logmsg("		-TTTT also accepts pulses of half a frame (emulated audio)\n");
	logmsg("   Output options:\n");
	logmsg("	 -v: Enable <v>erbose mode, spits all the FFTW results\n");
	logmsg("	 -l: Do not <l>og output to file [reference]_vs_[compare].txt\n");
	logmsg("	 -k: cloc<k> FFTW operations\n");
	logmsg("	 -0: Change output folder\n");
}

void Header_wave(int log)
{
	char title1[] = " MDWave " MDWVERSION " (MDFourier Companion) [240p Test Suite Fourier Audio compare tool]\n";
	char title2[] = "Artemio Urbina 2019-2020 free software under GPL - http://junkerhq.net/MDFourier\n";

	if(log)
		logmsg("%s%s", title1, title2);
	else
		printf("%s%s", title1, title2);
}
