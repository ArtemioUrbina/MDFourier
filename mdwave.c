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
#include "flac.h"

int LoadFile(FILE *file, AudioSignal *Signal, parameters *config, char *fileName);
int ProcessFile(AudioSignal *Signal, parameters *config);
int ProcessSamples(AudioBlocks *AudioArray, int16_t *samples, size_t size, long samplerate, double *window, parameters *config, int reverse, AudioSignal *Signal);
int commandline_wave(int argc , char *argv[], parameters *config);
void PrintUsage_wave();
void Header_wave(int log);
void CleanUp(AudioSignal **ReferenceSignal, parameters *config);
void CloseFiles(FILE **ref);
void RemoveFLACTemp(char *referenceFile);
int ExecuteMDWave(parameters *config, int invert);
void FlattenProfile(parameters *config);

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

	if(!SetupFolders("MDWave", "WAVE_Log_", &config))
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
	FILE				*reference = NULL;
	AudioSignal  		*ReferenceSignal = NULL;
	char 				*MainPath = NULL;

	if(invert)
	{
		logmsg("\n* Calculating values for Discard file\n");
		config->invert = 1;
	}

	if(IsFlac(config->referenceFile))
	{
		char tmpFile[BUFFER_SIZE];
		struct	timespec	start, end;

		if(config->clock)
			clock_gettime(CLOCK_MONOTONIC, &start);

		if(config->verbose)
			logmsg(" - Decoding FLAC\n");
		renameFLAC(config->referenceFile, tmpFile);
		if(!FLACtoWAV(config->referenceFile, tmpFile))
		{
			logmsg("\nInvalid FLAC file %s\n", config->referenceFile);
			remove(tmpFile);
			return 1;
		}
		if(config->clock)
		{
			double	elapsedSeconds;
			clock_gettime(CLOCK_MONOTONIC, &end);
			elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
			logmsg(" - clk: Decoding FLAC took %0.2fs\n", elapsedSeconds);
		}
		reference = fopen(tmpFile, "rb");
	}
	else
		reference = fopen(config->referenceFile, "rb");
	if(!reference)
	{
		logmsg("\nERROR: Could not open REFERENCE file: \"%s\"\n", config->referenceFile);
		RemoveFLACTemp(config->referenceFile);
		CleanUp(&ReferenceSignal, config);
		return 1;
	}

	ReferenceSignal = CreateAudioSignal(config);
	if(!ReferenceSignal)
	{
		CloseFiles(&reference);
		RemoveFLACTemp(config->referenceFile);
		CleanUp(&ReferenceSignal, config);
		logmsg("Not enough memory for Data Structures\n");
		return 1;
	}

	if(!config->useCompProfile)
		ReferenceSignal->role = ROLE_REF;
	else
		ReferenceSignal->role = ROLE_COMP;

	logmsg("\n* Loading Reference audio file %s\n", config->referenceFile);
	if(!LoadFile(reference, ReferenceSignal, config, config->referenceFile))
	{
		CloseFiles(&reference);
		RemoveFLACTemp(config->referenceFile);
		CleanUp(&ReferenceSignal, config);
		return 1;
	}

	CloseFiles(&reference);
	RemoveFLACTemp(config->referenceFile);

	config->referenceFramerate = ReferenceSignal->framerate;
	config->smallerFramerate = ReferenceSignal->framerate;

	if(config->channel == 's')
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
	if(!ProcessFile(ReferenceSignal, config))
	{
		CleanUp(&ReferenceSignal, config);
		PopMainPath(&MainPath);
		return 1;
	}
	PopMainPath(&MainPath);

	//logmsg("* Max blanked frequencies per block %d\n", config->maxBlanked);
	CleanUp(&ReferenceSignal, config);

	if(invert)
		printf("\nResults stored in %s%c%s\n", 
			config->outputPath, 
			config->outputPath[0] == '\0' ? ' ' : FOLDERCHAR, 
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

void CloseFiles(FILE **ref)
{
	if(*ref)
	{
		fclose(*ref);
		*ref = NULL;
	}
}

void RemoveFLACTemp(char *referenceFile)
{
	char tmpFile[BUFFER_SIZE];

	if(IsFlac(referenceFile))
	{
		renameFLAC(referenceFile, tmpFile);
		remove(tmpFile);
	}
}

char *GenerateFileNamePrefix(parameters *config)
{
	return(config->invert ? "2_Discarded" : "1_Used");
}

int LoadFile(FILE *file, AudioSignal *Signal, parameters *config, char *fileName)
{
	int					found = 0;
	struct	timespec	start, end;
	double				seconds = 0;

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	if(!file)
		return 0;

	if(fread(&Signal->header.riff, 1, sizeof(riff_hdr), file) != sizeof(riff_hdr))
	{
		logmsg("\tERROR: Invalid Audio file. File too small.\n");
		return(0);
	}

	if(strncmp((char*)Signal->header.riff.RIFF, "RIFF", 4) != 0)
	{
		logmsg("\tERROR: Invalid Audio file. RIFF header not found.\n");
		return(0);
	}

	if(strncmp((char*)Signal->header.riff.WAVE, "WAVE", 4) != 0)
	{
		logmsg("\tERROR: Invalid Audio file. WAVE header not found.\n");
		return(0);
	}

	do
	{
		sub_chunk	schunk;

		if(fread(&schunk, 1, sizeof(sub_chunk), file) != sizeof(sub_chunk))
		{
			logmsg("\tERROR: Invalid Audio file. File too small.\n");
			return(0);
		}
		if(strncmp((char*)schunk.chunkID, "fmt", 3) != 0)
			fseek(file, schunk.Size*sizeof(uint8_t), SEEK_CUR);
		else
		{
			fseek(file, -1*(long int)sizeof(sub_chunk), SEEK_CUR);
			found = 1;
		}
	}while(!found);

	if(fread(&Signal->header.fmt, 1, sizeof(fmt_hdr), file) != sizeof(fmt_hdr))
	{
		logmsg("\tERROR: Invalid Audio file. File too small.\n");
		return(0);
	}

	if(Signal->header.fmt.Subchunk1Size + 8 > sizeof(fmt_hdr))  // Add the fmt and chunksize length: 8 bytes
		fseek(file, Signal->header.fmt.Subchunk1Size + 8 - sizeof(fmt_hdr), SEEK_CUR);

	if(fread(&Signal->header.data, 1, sizeof(data_hdr), file) != sizeof(data_hdr))
	{
		logmsg("\tERROR: Invalid Audio file. File too small.\n");
		return(0);
	}

	if(Signal->header.fmt.AudioFormat != WAVE_FORMAT_PCM) /* Check for PCM */
	{
		logmsg("\tERROR: Invalid Audio File. Only 16 bit PCM supported.\n\tPlease convert file to 16 bit PCM.");
		return(0);
	}

	if(Signal->header.fmt.NumOfChan == 2 || Signal->header.fmt.NumOfChan == 1) /* Check for Stereo and Mono */
		Signal->AudioChannels = Signal->header.fmt.NumOfChan;

	if(Signal->AudioChannels == INVALID_CHANNELS)
	{
		logmsg("\tERROR: Invalid Audio file. Only Stereo files are supported.\n");
		return(0);
	}

	if(Signal->header.fmt.bitsPerSample != 16) /* Check bit depth */
	{
		logmsg("\tInvalid Audio file: Only 16 bit supported for now\n\tPlease use PCM 16 bit %dHz");
		return(0);
	}
	
	if(Signal->header.fmt.SamplesPerSec/2 < config->endHz)
	{
		logmsg(" - %d Hz sample rate was too low for %gHz-%gHz analysis\n",
			 Signal->header.fmt.SamplesPerSec, config->startHz, config->endHz);

		Signal->endHz = Signal->header.fmt.SamplesPerSec/2;
		Signal->nyquistLimit = 1;

		config->endHz = Signal->endHz;
		config->nyquistLimit = 1;

		logmsg(" - Changed to %gHz-%gHz for this file\n", config->startHz, Signal->endHz);
	}

	// Default if none is found
	Signal->framerate = GetMSPerFrame(Signal, config);

	seconds = (double)Signal->header.data.DataSize/2.0/(double)Signal->header.fmt.SamplesPerSec/Signal->AudioChannels;
	logmsg(" - Audio file is %dHz %dbits %s and %g seconds long\n", 
		Signal->header.fmt.SamplesPerSec, 
		Signal->header.fmt.bitsPerSample, 
		Signal->AudioChannels == 2 ? "Stereo" : "Mono", 
		seconds);

	if(seconds < GetSignalTotalDuration(Signal->framerate, config))
	{
		logmsg(" - WARNING: Estimated file length is shorter than the expected %g seconds\n",
				GetSignalTotalDuration(Signal->framerate, config));
		config->smallFile = 1;
	}

	Signal->Samples = (char*)malloc(sizeof(char)*Signal->header.data.DataSize);
	if(!Signal->Samples)
	{
		logmsg("\tAll Chunks malloc failed!\n");
		return(0);
	}

	Signal->SamplesStart = ftell(file);
	if(fread(Signal->Samples, 1, sizeof(char)*Signal->header.data.DataSize, file) !=
			 sizeof(char)*Signal->header.data.DataSize)
	{
		logmsg("\tCould not read the whole sample block from disk to RAM\n");
		return(0);
	}

	if(config->clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - clk: Loading Audio took %0.2fs\n", elapsedSeconds);
	}

	if(GetFirstSyncIndex(config) != NO_INDEX && !config->noSyncProfile)
	{
		if(config->clock)
			clock_gettime(CLOCK_MONOTONIC, &start);

		/* Find the start offset */
		if(config->verbose)
			logmsg(" - Sync pulse train: ");
		Signal->startOffset = DetectPulse(Signal->Samples, Signal->header, Signal->role, config);
		if(Signal->startOffset == -1)
		{
			int format = 0;

			logmsg("\nERROR: Starting pulse train was not detected.\nProfile used: [%s]\n", config->types.Name);

			if(Signal->role == ROLE_REF)
				format = config->videoFormatRef;
			else
				format = config->videoFormatCom;
			if(!config->syncTolerance)
				logmsg(" - You can try using -T for a frequency tolerant pulse detection algorithm\n");
			if(format != 0)
				logmsg(" - This signal is configured as '%s', check if that is not the issue.\n", 
						config->types.SyncFormat[format].syncName);
			return 0;
		}
		if(config->verbose)
			logmsg(" %gs [%ld samples %ld [header %ld] bytes]", 
				BytesToSeconds(Signal->header.fmt.SamplesPerSec, Signal->startOffset, Signal->AudioChannels),
				Signal->startOffset/2/Signal->AudioChannels, Signal->startOffset, Signal->startOffset + Signal->SamplesStart);

		if(GetLastSyncIndex(config) != NO_INDEX)
		{
			double diff = 0, expected = 0;

			if(config->verbose)
				logmsg(" to");
			Signal->endOffset = DetectEndPulse(Signal->Samples, Signal->startOffset, Signal->header, Signal->role, config);
			if(Signal->endOffset == -1)
			{
				int format = 0;

				logmsg("\nERROR: Ending pulse train was not detected.\nProfile used: [%s]\n", config->types.Name);				
				if(Signal->role == ROLE_REF)
					format = config->videoFormatRef;
				else
					format = config->videoFormatCom;
				if(!config->syncTolerance)
					logmsg(" - You can try using -T for a frequency tolerant pulse detection algorithm\n");
				if(format != 0)
					logmsg(" - This signal is configured as '%s', check if that is not the issue.\n", 
							config->types.SyncFormat[format].syncName);
				return 0;
			}
			if(config->verbose)
				logmsg(" %gs [%ld samples %ld [header %ld] bytes]\n", 
					BytesToSeconds(Signal->header.fmt.SamplesPerSec, Signal->endOffset, Signal->AudioChannels),
					Signal->endOffset/2/Signal->AudioChannels, Signal->endOffset, Signal->endOffset + Signal->SamplesStart);
			Signal->framerate = CalculateFrameRate(Signal, config);
			if(!Signal->framerate)
			{
				logmsg("\nERROR: Framerate could not be detected.\n");
				return 0;
			}

			logmsg(" - Detected %g Hz video signal (%gms per frame) from Audio file\n", 
						roundFloat(CalculateScanRate(Signal)), Signal->framerate);

			expected = GetMSPerFrame(Signal, config);
			diff = fabs(100.0 - Signal->framerate*100.0/expected);
			if(diff > 1.0)
			{
				logmsg("\nERROR: Framerate is %g%% different from the expected %gms.\n",
						diff, expected);
				logmsg("\tThis might be due a mismatched profile.\n");
				if(!config->ignoreFrameRateDiff)
				{
					logmsg("\tIf you want to ignore this and compare the files, use -I.\n");
					return 0;
				}
			}
		}
		else
		{
			logmsg(" - ERROR: Trailing sync pulse train not defined in config file, aborting.\n");
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

	if(config->noSyncProfile)
	{
		switch(config->noSyncProfileType)
		{
			case NO_SYNC_AUTO:
			{
				/* Find the start offset */
				
				logmsg(" - Detecting audio signal: ");
				Signal->startOffset = DetectSignalStart(Signal->Samples, Signal->header, 0, 0, NULL, config);
				if(Signal->startOffset == -1)
				{
					logmsg("\nERROR: Starting position was not detected.\n");
					return 0;
				}
				logmsg(" %gs [%ld bytes]\n", 
						BytesToSeconds(Signal->header.fmt.SamplesPerSec, Signal->startOffset, Signal->AudioChannels),
						Signal->startOffset);
				Signal->endOffset = SecondsToBytes(Signal->header.fmt.SamplesPerSec, 
										GetSignalTotalDuration(Signal->framerate, config), 
										Signal->AudioChannels, 
										NULL, NULL, NULL);
			}
			break;
			case NO_SYNC_MANUAL:
			{
				double diff = 0, expected = 0;
		
				logmsg(" - WARNING: Files must be identically trimmed for this to work at some level\n");
				Signal->startOffset = 0;
				Signal->endOffset = Signal->header.data.DataSize;
		
				expected = Signal->framerate;
		
				if(Signal->role == ROLE_REF)
				{
					double seconds = 0;
					seconds = BytesToSeconds(Signal->header.fmt.SamplesPerSec, Signal->endOffset, Signal->AudioChannels);
					config->NoSyncTotalFrames = (seconds*1000)/expected;
					Signal->framerate = expected;
					logmsg(" - Loaded %g Hz video signal (%gms per frame) from profile file\n", 
								CalculateScanRate(Signal), Signal->framerate);
				}
				else
				{
					Signal->framerate = CalculateFrameRateNS(Signal, config->NoSyncTotalFrames, config);
					logmsg(" - Detected %g Hz video signal (%gms per frame) from Audio file\n", 
								CalculateScanRate(Signal), Signal->framerate);
				}
		
				diff = fabs(100.0 - Signal->framerate*100.0/expected);
				if(diff > 1.0)
				{
					logmsg("\nERROR: Framerate is %g%% different from the expected %gms.\n",
							diff, expected);
					logmsg("\tThis might be due a mismatched profile.\n");
					logmsg("\tIf you want to ignore this and compare the files, use -I.\n");
					if(!config->ignoreFrameRateDiff)
						return 0;
				}
			}
			break;
			default:
			{
				logmsg("\nERROR: Invalid profile\n");
				return 0;
			}
		}
	}

	if(seconds < GetSignalTotalDuration(Signal->framerate, config))
		logmsg(" - Adjusted File length is smaller than the expected %gs\n",
				GetSignalTotalDuration(Signal->framerate, config));

	if(GetFirstSilenceIndex(config) != NO_INDEX)
		Signal->hasSilenceBlock = 1;

	sprintf(Signal->SourceFile, "%s", fileName);

	return 1;
}

int MoveSampleBlockInternal(AudioSignal *Signal, long int element, long int pos, long int signalStartOffset, parameters *config)
{
	char		*sampleBuffer = NULL;
	double		signalLengthSeconds = 0;
	long int	signalLengthFrames = 0, signalLengthBytes = 0;

	signalLengthFrames = GetInternalSyncTotalLength(element, config);
	if(!signalLengthFrames)
	{
		logmsg("\tERROR: Internal Sync block has no frame duration. Aborting.\n");
		return 0;
	}

	signalLengthSeconds = FramesToSeconds(signalLengthFrames, config->referenceFramerate);
	signalLengthBytes = SecondsToBytes(Signal->header.fmt.SamplesPerSec, signalLengthSeconds, Signal->AudioChannels, NULL, NULL, NULL);

	if(pos + signalStartOffset + signalLengthBytes > Signal->header.data.DataSize)
	{
		signalLengthBytes = Signal->header.data.DataSize - (pos+signalStartOffset);
		if(config->verbose)
			logmsg(" - Inernal sync adjust: Signal is smaller than expected\n");
	}

	if(config->verbose)
		logmsg(" - Internal Segment Info:\n\tFinal Offset: %ld Frames: %d Seconds: %g Bytes: %ld\n",
				pos+signalStartOffset, signalLengthFrames, signalLengthSeconds, signalLengthBytes);

	sampleBuffer = (char*)malloc(sizeof(char)*signalLengthBytes);
	if(!sampleBuffer)
	{
		logmsg("\tERROR: Out of memory.\n");
		return 0;
	}

	/*
	if(config->verbose)
	{
		logmsg(" - MOVEMENTS:\n");
		logmsg("\tCopy: From %ld Bytes: %ld\n",
				pos + signalStartOffset, signalLengthBytes);
		logmsg("\tZero Out: Pos: %ld Bytes: %ld\n",
				pos+signalStartOffset, signalLengthBytes);
		logmsg("\tStore: Pos: %ld Bytes: %ld\n",
				pos, signalLengthBytes);
	}
	*/
	memcpy(sampleBuffer, Signal->Samples + pos + signalStartOffset, signalLengthBytes);
	memset(Signal->Samples + pos + signalStartOffset, 0, signalLengthBytes);
	memcpy(Signal->Samples + pos, sampleBuffer, signalLengthBytes);

	free(sampleBuffer);
	return 1;
}

int MoveSampleBlockExternal(AudioSignal *Signal, long int element, long int pos, long int signalStartOffset, long int internalSyncToneSize, parameters *config)
{
	char		*sampleBuffer = NULL;
	double		signalLengthSeconds = 0;
	long int	signalLengthFrames = 0, signalLengthBytes = 0;

	signalLengthFrames = GetInternalSyncTotalLength(element, config);
	if(!signalLengthFrames)
	{
		logmsg("\tERROR: Internal Sync block has no frame duration. Aborting.\n");
		return 0;
	}

	signalLengthSeconds = FramesToSeconds(signalLengthFrames, config->referenceFramerate);
	signalLengthBytes = SecondsToBytes(Signal->header.fmt.SamplesPerSec, signalLengthSeconds, Signal->AudioChannels, NULL, NULL, NULL);

	if(pos + signalStartOffset + signalLengthBytes > Signal->header.data.DataSize)
	{
		signalLengthBytes = Signal->header.data.DataSize - (pos+signalStartOffset);
		if(config->verbose)
			logmsg(" - Inernal sync adjust: Signal is smaller than expected\n");
	}
	if(config->verbose)
		logmsg(" - Internal Segment Info:\n\tFinal Offset: %ld Frames: %d Seconds: %g Bytes: %ld\n",
				pos+signalStartOffset, signalLengthFrames, signalLengthSeconds, signalLengthBytes);

	sampleBuffer = (char*)malloc(sizeof(char)*signalLengthBytes);
	if(!sampleBuffer)
	{
		logmsg("\tERROR: Out of memory while performing internal Sync adjustments.\n");
		return 0;
	}

	/*
	if(config->verbose)
	{
		logmsg(" - MOVEMENTS:\n");
		logmsg("\tCopy: From %ld Bytes: %ld\n",
				pos + signalStartOffset, signalLengthBytes);
		logmsg("\tZero Out: Pos: %ld Bytes: %ld\n",
				pos + pos, Signal->header.data.DataSize-pos);
		logmsg("\tStore: Pos: %ld Bytes: %ld\n",
				pos, signalLengthBytes);
	}
	*/

	memcpy(sampleBuffer, Signal->Samples + pos + signalStartOffset, signalLengthBytes);
	memset(Signal->Samples + pos, 0, Signal->header.data.DataSize-pos);
	memcpy(Signal->Samples + pos, sampleBuffer, signalLengthBytes);

	free(sampleBuffer);
	return 1;
}

int ProcessInternal(AudioSignal *Signal, long int element, long int pos, int *syncinternal, long int *advanceBytes, int knownLength, parameters *config)
{
	if(*syncinternal)
		*syncinternal = 0;
	else
	{
		int			syncToneFreq = 0;
		double		syncLenSeconds = 0;
		long int	internalSyncOffset = 0, syncLengthBytes = 0,
					endPulseBytes = 0, pulseLengthBytes = 0, signalStart = 0;

		*syncinternal = 1;

		syncToneFreq = GetInternalSyncTone(element, config);
		syncLenSeconds = GetInternalSyncLen(element, config);
		internalSyncOffset = DetectSignalStart(Signal->Samples, Signal->header, pos, syncToneFreq, &endPulseBytes, config);
		if(internalSyncOffset == -1)
		{
			logmsg("\tERROR: No signal found while in internal sync detection. Aborting\n");
			return 0;
		} 

		pulseLengthBytes = endPulseBytes - internalSyncOffset;
		syncLengthBytes = SecondsToBytes(Signal->header.fmt.SamplesPerSec, syncLenSeconds, Signal->AudioChannels, NULL, NULL, NULL);
		internalSyncOffset -= pos;
		signalStart = internalSyncOffset;

		if(knownLength == TYPE_INTERNAL_KNOWN)
		{
			logmsg(" - %s command delay: %g ms [%g frames]\n",
				GetBlockName(config, element),
				BytesToSeconds(Signal->header.fmt.SamplesPerSec, internalSyncOffset, Signal->AudioChannels)*1000.0,
				BytesToFrames(Signal->header.fmt.SamplesPerSec, internalSyncOffset, config->referenceFramerate, Signal->AudioChannels));

			if(config->verbose)
					logmsg("  > Found at: %ld Previous: %ld Offset: %ld\n\tPulse Length: %ld Silence Length: %ld\n", 
						pos + internalSyncOffset, pos, internalSyncOffset, pulseLengthBytes, syncLengthBytes/2);

#ifndef MDWAVE
			/* Copy waveforms for visual inspection, create 3 slots: silence, sync pulse, silence */
			if(config->plotAllNotes)
			{
				if(!initInternalSync(&Signal->Blocks[element], 3))
					return 0;
			
				if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
						(int16_t*)(Signal->Samples + pos), 
						internalSyncOffset/2, 0, 
						Signal->header.fmt.SamplesPerSec, NULL, Signal->AudioChannels, config))
					return 0;
	
				if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
						(int16_t*)(Signal->Samples + pos + internalSyncOffset), 
						pulseLengthBytes/2, 1, 
						Signal->header.fmt.SamplesPerSec, NULL, Signal->AudioChannels, config))
					return 0;
	
				if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
						(int16_t*)(Signal->Samples + pos + internalSyncOffset + pulseLengthBytes), 
						syncLengthBytes/4, 2, 
						Signal->header.fmt.SamplesPerSec, NULL, Signal->AudioChannels, config))
					return 0;
			}
#endif
			// skip sync tone-which is silence-taken from config file
			internalSyncOffset += syncLengthBytes;

			if(!MoveSampleBlockInternal(Signal, element, pos, internalSyncOffset, config))
				return 0;
		}
		if(knownLength == TYPE_INTERNAL_UNKNOWN)  // Our sync is outside the frame detection zone
		{
			long int 	silenceLengthBytes = 0;
#ifndef MDWAVE
			long int 	oneframe = 0;
#endif

			silenceLengthBytes = syncLengthBytes/2;

			if(pulseLengthBytes != silenceLengthBytes)
			{
				if(pulseLengthBytes > silenceLengthBytes)
				{
					long int compensate = 0;

					// Length must be fixed, so we detected start sooner
					compensate = pulseLengthBytes - silenceLengthBytes;

					signalStart += compensate;
					internalSyncOffset += compensate;

					pulseLengthBytes = silenceLengthBytes;
				}
				else
				{
					logmsg(" - WARNING: Internal Sync too short. Got %ld] expewcted %ld\n", 
						pulseLengthBytes, silenceLengthBytes);
				}
				//silenceLengthBytes = syncLengthBytes - pulseLengthBytes;
			}

			logmsg(" - %s command delay: %g ms [%g frames]\n",
				GetBlockName(config, element),
				BytesToSeconds(Signal->header.fmt.SamplesPerSec, internalSyncOffset, Signal->AudioChannels)*1000.0,
				BytesToFrames(Signal->header.fmt.SamplesPerSec, internalSyncOffset, config->referenceFramerate, Signal->AudioChannels));

			if(config->verbose)
				logmsg("  > Found at: %ld Previous: %ld\n\tPulse Length: %ld Silence Length: %ld\n", 
					pos + internalSyncOffset, pos, pulseLengthBytes, silenceLengthBytes);
			
			// skip the pulse real duration to sync perfectly
			signalStart += pulseLengthBytes;
			// skip half the sync tone-which is silence-taken from config file
			signalStart += silenceLengthBytes;
#ifndef MDWAVE
			/* Copy waveforms for visual inspection, create 3 slots: silence, sync pulse, silence */
			if(config->plotAllNotes)
			{
				if(!initInternalSync(&Signal->Blocks[element], 4))
					return 0;
			
				if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
						(int16_t*)(Signal->Samples + pos), 
						internalSyncOffset/2, 0, 
						Signal->header.fmt.SamplesPerSec, NULL, Signal->AudioChannels, config))
					return 0;
	
				if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
						(int16_t*)(Signal->Samples + pos + internalSyncOffset), 
						pulseLengthBytes/2, 1, 
						Signal->header.fmt.SamplesPerSec, NULL, Signal->AudioChannels, config))
					return 0;
	
				if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
						(int16_t*)(Signal->Samples + pos + internalSyncOffset + pulseLengthBytes), 
						silenceLengthBytes	/2, 2, 
						Signal->header.fmt.SamplesPerSec, NULL, Signal->AudioChannels, config))
					return 0;

				oneframe = SecondsToBytes(Signal->header.fmt.SamplesPerSec, FramesToSeconds(1, config->referenceFramerate), Signal->AudioChannels, NULL, NULL, NULL);
				if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
						(int16_t*)(Signal->Samples + pos + signalStart), 
						(oneframe*2)/2, 3, 
						Signal->header.fmt.SamplesPerSec, NULL, Signal->AudioChannels, config))
					return 0;
			}
#endif
			/* Do the real processing */
			if(!MoveSampleBlockExternal(Signal, element, pos, signalStart, pulseLengthBytes + silenceLengthBytes, config))
				return 0;
		}
		if(advanceBytes)
			*advanceBytes += internalSyncOffset;
	}
	return 1;
}


int CreateChunksFolder(parameters *config)
{
	char name[BUFFER_SIZE*4];

	sprintf(name, "%s%cChunks", config->folderName, FOLDERCHAR);
	if(!CreateFolder(name))
		return 0;
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
		long int frames = 0, difference = 0;

		if(!syncinternal)
			framerate = Signal->framerate;
		else
			framerate = config->referenceFramerate;

		frames = GetBlockFrames(config, i);
		duration = FramesToSeconds(framerate, frames);
				
		loadedBlockSize = SecondsToBytes(Signal->header.fmt.SamplesPerSec, duration, Signal->AudioChannels, &leftover, &discardBytes, &leftDecimals);

		difference = GetByteSizeDifferenceByFrameRate(framerate, frames, Signal->header.fmt.SamplesPerSec, Signal->AudioChannels, config);

		windowUsed = NULL;
		if(Signal->Blocks[i].type >= TYPE_SILENCE)
			windowUsed = getWindowByLength(&windows, frames, Signal->framerate);

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
			long int frames = 0, difference = 0;
	
			frames = GetBlockFrames(config, i);
			duration = FramesToSeconds(Signal->framerate, frames);
			if(Signal->Blocks[i].type >= TYPE_SILENCE)
				windowUsed = getWindowByLength(&windows, frames, Signal->framerate);
			
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
		channel = 'l';
	else
		channel = config->channel;

	for(i = 0; i < monoSignalSize - zeropadding; i++)
	{
		if(channel == 'l')
		{
			signal[i] = (double)samples[i*Signal->AudioChannels];
			if(Signal->AudioChannels == 2)
				samples[i*2+1] = 0;
		}
		if(channel == 'r')
		{
			signal[i] = (double)samples[i*2+1];
			samples[i*2] = 0;
		}
		if(channel == 's')
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
			if(channel == 'l')
			{
				samples[i*Signal->AudioChannels] = round(value);
				if(Signal->AudioChannels == 2)
					samples[i*2+1] = 0;
			}
			if(channel == 'r')
			{
				samples[i*2] = 0;
				samples[i*2+1] = round(value);
			}
			if(channel == 's')
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

	while ((c = getopt (argc, argv, "qnhvzcklyCBis:e:f:t:p:a:w:r:P:IY:0:")) != -1)
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
		else if (optopt == 'a')
		  logmsg("\t ERROR:  Audio channel option -%c requires an argument: l,r or s\n", optopt);
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

	if(config->channel != 's')
		logmsg("\tAudio Channel is: %s\n", GetChannel(config->channel));
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
	logmsg("  usage: mdwave -P profile.mdf -r reference.wav\n");
	logmsg("   FFT and Analysis options:\n");
	logmsg("	 -a: select <a>udio channel to compare. 's', 'l' or 'r'\n");
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
	logmsg("   Output options:\n");
	logmsg("	 -v: Enable <v>erbose mode, spits all the FFTW results\n");
	logmsg("	 -l: <l>og output to file [reference]_vs_[compare].txt\n");
	logmsg("	 -k: cloc<k> FFTW operations\n");
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
