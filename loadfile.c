/* 
 * MDFourier
 * A Fourier Transform analysis tool to compare game console audio
 * http://junkerhq.net/MDFourier/
 *
 * Copyright (C)2019-2020 Artemio Urbina
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
#include "flac.h"
#include "freq.h"
#include "loadfile.h"
#include "sync.h"

int LoadFile(AudioSignal **Signal, char *fileName, int role, parameters *config)
{
	*Signal = CreateAudioSignal(config);
	if(!*Signal)
		return 0;
	(*Signal)->role = role;

	logmsg("\n* Loading '%s' audio file %s\n", role == ROLE_REF ? "Reference" : "Comparison", fileName);

	if(IsFlac(fileName))
	{
		struct	timespec	start, end;

		if(config->clock)
			clock_gettime(CLOCK_MONOTONIC, &start);

		if(config->verbose) { logmsg(" - Decoding FLAC\n"); }
		if(!FLACtoSignal(fileName, *Signal, config))
		{
			logmsg("\nERROR: Invalid FLAC file %s\n", fileName);
			return 0;
		}
		if(config->clock)
		{
			double	elapsedSeconds;
			clock_gettime(CLOCK_MONOTONIC, &end);
			elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
			logmsg(" - clk: Decoding FLAC took %0.2fs\n", elapsedSeconds);
		}
	}
	else
	{
		FILE	*file = NULL;

		file = fopen(fileName, "rb");
		if(!file)
		{
			logmsg("\tERROR: Could not open 'Reference' file:\n\t\"%s\"\n", fileName);
			return 0;
		}

		if(!LoadWAVFile(file, *Signal, config, fileName))
		{
			fclose(file);
			return 0;
		}
		fclose(file);
		file = NULL;
	}

	if(!AdjustSignalValues(*Signal, config))
		return 0;

	sprintf((*Signal)->SourceFile, "%s", fileName);

	if(!DetectSync(*Signal, config))
		return 0;	
	return 1;
}

int LoadWAVFile(FILE *file, AudioSignal *Signal, parameters *config, char *fileName)
{
	int					found = 0;
	size_t				bytesRead = 0;
	struct	timespec	start, end;

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

	// look for fmt chunk
	found = 0;
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

	// look for data chunk
	found = 0;
	do
	{
		sub_chunk	schunk;

		if(fread(&schunk, 1, sizeof(sub_chunk), file) != sizeof(sub_chunk))
		{
			logmsg("\tERROR: Invalid Audio file. File too small.\n");
			return(0);
		}
		if(strncmp((char*)schunk.chunkID, "data", 4) != 0)
			fseek(file, schunk.Size*sizeof(uint8_t), SEEK_CUR);
		else
		{
			fseek(file, -1*(long int)sizeof(sub_chunk), SEEK_CUR);
			found = 1;
		}
	}while(!found);

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
		logmsg("\tERROR: Invalid Audio file. Only Mono and Stereo files are supported.\n");
		return(0);
	}

	if(Signal->header.fmt.bitsPerSample != 16) /* Check bit depth */
	{
		logmsg("\tERROR: Invalid Audio file. Only 16 bit supported for now.\n\tPlease use 16 bit.");
		return(0);
	}

	if(Signal->header.data.DataSize <= 0)
	{
		logmsg("\tERROR: RIFf header has an invalid Data length %ld\n", Signal->header.data.DataSize);
		return(0);
	}

	Signal->Samples = (char*)malloc(sizeof(char)*Signal->header.data.DataSize);
	if(!Signal->Samples)
	{
		logmsg("\tERROR: All Chunks malloc failed!\n");
		return(0);
	}

	memset(Signal->Samples, 0, sizeof(char)*Signal->header.data.DataSize);
	Signal->SamplesStart = ftell(file);
	bytesRead = fread(Signal->Samples, 1, sizeof(char)*Signal->header.data.DataSize, file);
	if(bytesRead != sizeof(char)*Signal->header.data.DataSize)
	{
		logmsg("\tERROR: Corrupt RIFF Header\n\tCould not read the whole sample block from disk to RAM.\n\tBytes Read: %ld Expected: %ld\n",
			bytesRead, sizeof(char)*Signal->header.data.DataSize);
		return(0);
	}

	if(config->clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - clk: Loading Audio took %0.2fs\n", elapsedSeconds);
	}

	return 1;
}

int DetectSync(AudioSignal *Signal, parameters *config)
{
	struct	timespec	start, end;
	double				seconds = 0;

	Signal->framerate = GetMSPerFrame(Signal, config);
	if(GetFirstSyncIndex(config) != NO_INDEX && !config->noSyncProfile)
	{
		if(config->clock)
			clock_gettime(CLOCK_MONOTONIC, &start);

		/* Find the start offset */
		if(config->verbose) { 
			logmsg(" - Sync pulse train: "); 
		}
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
			if(format != 0 || config->smallFile)
				logmsg(" - This signal is configured as '%s'%s, check if that is not the issue.\n", 
							config->types.SyncFormat[format].syncName, config->smallFile ? " and is smaller than expected" : "");
			return 0;
		}
		if(config->verbose) {
			logmsg(" %gs [%ld samples|%ld bytes|%ld bytes/head]", 
				BytesToSeconds(Signal->header.fmt.SamplesPerSec, Signal->startOffset, Signal->AudioChannels),
				Signal->startOffset/2/Signal->AudioChannels, Signal->startOffset, Signal->startOffset + Signal->SamplesStart);
		}

		if(GetLastSyncIndex(config) != NO_INDEX)
		{
			double diff = 0, expected = 0;

			if(config->verbose) { 
				logmsg(" to");
			}
			Signal->endOffset = DetectEndPulse(Signal->Samples, Signal->startOffset, Signal->header, Signal->role, config);
			if(Signal->endOffset == -1)
			{
				int format = 0;

				logmsg("\n ERROR: Ending pulse train was not detected.\n - Profile used: [%s]\n", config->types.Name);				
				if(Signal->role == ROLE_REF)
					format = config->videoFormatRef;
				else
					format = config->videoFormatCom;
				if(!config->syncTolerance)
					logmsg(" - You can try using -T for a frequency tolerant pulse detection algorithm\n");
				if(format != 0 || config->smallFile)
					logmsg(" - This signal is configured as '%s'%s, check if that is not the issue.\n", 
							config->types.SyncFormat[format].syncName, config->smallFile ? " and is smaller than expected" : "");
				return 0;
			}
			if(config->verbose) {
				logmsg(" %gs [%ld samples|%ld bytes|%ld bytes/head]\n", 
					BytesToSeconds(Signal->header.fmt.SamplesPerSec, Signal->endOffset, Signal->AudioChannels),
					Signal->endOffset/2/Signal->AudioChannels, Signal->endOffset, Signal->endOffset + Signal->SamplesStart);
			}
			Signal->framerate = CalculateFrameRateAndCheckSamplerate(Signal, config);
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
				logmsg("\n%s: Framerate is %g%% different from the expected %gms.\n",
						!config->ignoreFrameRateDiff ? "ERROR" : "WARNING", diff, expected);
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
				Signal->startOffset = DetectSignalStart(Signal->Samples, Signal->header, 0, 0, 0, NULL, NULL, config);
				if(Signal->startOffset == -1)
				{
					logmsg("\nERROR: Starting position was not detected.\n");
					return 0;
				}
				logmsg(" %gs [%ld samples|%ld bytes|%ld bytes/head]\n", 
						BytesToSeconds(Signal->header.fmt.SamplesPerSec, Signal->startOffset, Signal->AudioChannels),
						Signal->startOffset/2/Signal->AudioChannels, Signal->startOffset, Signal->startOffset + Signal->SamplesStart);
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

	seconds = (double)Signal->header.data.DataSize/2.0/(double)Signal->header.fmt.SamplesPerSec/Signal->AudioChannels;
	if(seconds < GetSignalTotalDuration(Signal->framerate, config))
		logmsg(" - File length is smaller than the expected %gs\n",
				GetSignalTotalDuration(Signal->framerate, config));

	if(GetFirstSilenceIndex(config) != NO_INDEX)
		Signal->hasSilenceBlock = 1;

	return 1;
}

int AdjustSignalValues(AudioSignal *Signal, parameters *config)
{
	double seconds = 0;

	// Default if none is found
	Signal->framerate = GetMSPerFrame(Signal, config);

	Signal->AudioChannels = Signal->header.fmt.NumOfChan;
	if(Signal->header.fmt.SamplesPerSec/2 < config->endHz)
	{
		logmsg(" - %d Hz sample rate was too low for %gHz-%gHz analysis\n",
			 Signal->header.fmt.SamplesPerSec, config->startHz, config->endHz);

		Signal->endHz = Signal->header.fmt.SamplesPerSec/2;
		Signal->nyquistLimit = 1;

		logmsg(" - Changed to %gHz-%gHz for this file\n", config->startHz, Signal->endHz);
	}

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
		config->smallFile |= Signal->role;
	}

	if(config->usesStereo && Signal->AudioChannels != 2)
	{
		if(!config->allowStereoVsMono)
		{
			config->stereoNotFound |= Signal->role;
			logmsg(" - ERROR: Profile requests Stereo and file is Mono\n");
			return 0;
		}
		logmsg(" - Enabling Mono vs Stereo compare mode\n");
	}
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
		if(config->verbose) {
			logmsg(" - Internal sync adjust: Signal is smaller than expected\n");
		}
	}

	if(config->verbose) {
		logmsg(" - Internal Segment Info:\n\tSignal Start Offset: %ld Frames: %d Seconds: %g NumBytes: %ld\n\n",
				pos+signalStartOffset, signalLengthFrames, signalLengthSeconds, signalLengthBytes);
	}

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
		if(config->verbose) {
			logmsg(" - Internal sync adjust: Signal is smaller than expected\n");
		}
	}
	if(config->verbose) {
		logmsg(" - Internal Segment Info:\n\tFinal Offset: %ld Frames: %d Seconds: %g Bytes: %ld\n",
				pos+signalStartOffset, signalLengthFrames, signalLengthSeconds, signalLengthBytes);
	}

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
		int			syncToneFreq = 0, toleranceIssue = 0;
		double		syncLenSeconds = 0;
		long int	internalSyncOffset = 0, syncLengthBytes = 0,
					endPulseBytes = 0, pulseLengthBytes = 0, signalStart = 0;

		syncToneFreq = GetInternalSyncTone(element, config);
		syncLenSeconds = GetInternalSyncLen(element, config);
		syncLengthBytes = SecondsToBytes(Signal->header.fmt.SamplesPerSec, syncLenSeconds, Signal->AudioChannels, NULL, NULL, NULL);

		// we send , syncLenSeconds/2 since it is half silence half pulse
		internalSyncOffset = DetectSignalStart(Signal->Samples, Signal->header, pos, syncToneFreq, syncLengthBytes/2, &endPulseBytes, &toleranceIssue, config);
		if(internalSyncOffset == -1)
		{
			logmsg("\tERROR: No signal found while in internal sync detection.\n");
			return 0;  // Was warning with -1
		}
		*syncinternal = 1;

		if(toleranceIssue)
			config->internalSyncTolerance |= Signal->role;
		
		pulseLengthBytes = endPulseBytes - internalSyncOffset;
		internalSyncOffset -= pos;
		signalStart = internalSyncOffset;

		if(pulseLengthBytes < (syncLengthBytes/2)*0.90)
		{
			double expected = 0, got = 0;

			got = BytesToSeconds(Signal->header.fmt.SamplesPerSec, pulseLengthBytes/2, Signal->AudioChannels);
			expected = BytesToSeconds(Signal->header.fmt.SamplesPerSec, syncLengthBytes/2, Signal->AudioChannels);

			logmsg(" - ERROR: Internal Sync %dhz tone at %ld bytes was shorter than the expected %gms by %gms\n",
					syncToneFreq, pos + internalSyncOffset, expected, expected - got);
			return 0;
		}

		if(knownLength == TYPE_INTERNAL_KNOWN)
		{
			Signal->delayArray[Signal->delayElemCount++] = BytesToSeconds(Signal->header.fmt.SamplesPerSec, internalSyncOffset, Signal->AudioChannels)*1000.0;
			logmsg(" - %s command delay: %g ms [%g frames]\n",
				GetBlockName(config, element),
				BytesToSeconds(Signal->header.fmt.SamplesPerSec, internalSyncOffset, Signal->AudioChannels)*1000.0,
				BytesToFrames(Signal->header.fmt.SamplesPerSec, internalSyncOffset, config->referenceFramerate, Signal->AudioChannels));

			if(config->verbose) {
					logmsg("  > Found at: %ld (%ld +%ld)\n\tPulse Length: %ld Silence Length: %ld\n", 
						pos + internalSyncOffset, pos, internalSyncOffset,
						pulseLengthBytes, syncLengthBytes/2);
			}

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

			// skip sync tone-which is silence-taken from config file
			internalSyncOffset += syncLengthBytes;

			if(!MoveSampleBlockInternal(Signal, element, pos, internalSyncOffset, config))
				return 0;
		}
		if(knownLength == TYPE_INTERNAL_UNKNOWN)  // Our sync is outside the frame detection zone
		{
			long int 	silenceLengthBytes = 0;
			/* long int 	oneframe = 0; */

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
					double expected = 0, got = 0;

					got = BytesToSeconds(Signal->header.fmt.SamplesPerSec, pulseLengthBytes, Signal->AudioChannels);
					expected = BytesToSeconds(Signal->header.fmt.SamplesPerSec, silenceLengthBytes, Signal->AudioChannels);

					if(expected - got > 0.00015)  // This is the expected error in ms
						logmsg(" - WARNING: Internal Sync was shorter than the expected %gms by %gms\n", expected, expected - got);
				}
				//silenceLengthBytes = syncLengthBytes - pulseLengthBytes;
			}

			Signal->delayArray[Signal->delayElemCount++] = BytesToSeconds(Signal->header.fmt.SamplesPerSec, internalSyncOffset, Signal->AudioChannels)*1000.0;
			logmsg(" - %s command delay: %g ms [%g frames]\n",
				GetBlockName(config, element),
				BytesToSeconds(Signal->header.fmt.SamplesPerSec, internalSyncOffset, Signal->AudioChannels)*1000.0,
				BytesToFrames(Signal->header.fmt.SamplesPerSec, internalSyncOffset, config->referenceFramerate, Signal->AudioChannels));

			if(config->verbose) {
				logmsg("  > Found at: %ld Previous: %ld\n\tPulse Length: %ld Silence Length: %ld\n", 
					pos + internalSyncOffset, pos, pulseLengthBytes, silenceLengthBytes);
			}
			
			// skip the pulse real duration to sync perfectly
			signalStart += pulseLengthBytes;
			// skip half the sync tone-which is silence-taken from config file
			signalStart += silenceLengthBytes;

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
						silenceLengthBytes	/2, 2, 
						Signal->header.fmt.SamplesPerSec, NULL, Signal->AudioChannels, config))
					return 0;

				/*
				oneframe = SecondsToBytes(Signal->header.fmt.SamplesPerSec, FramesToSeconds(1, config->referenceFramerate), Signal->AudioChannels, NULL, NULL, NULL);
				if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
						(int16_t*)(Signal->Samples + pos + signalStart), 
						(oneframe*2)/2, 3, 
						Signal->header.fmt.SamplesPerSec, NULL, Signal->AudioChannels, config))
					return 0;
				*/
			}

			/* Do the real processing */
			if(!MoveSampleBlockExternal(Signal, element, pos, signalStart, pulseLengthBytes + silenceLengthBytes, config))
				return 0;
		}
		if(advanceBytes)
			*advanceBytes += internalSyncOffset;
	}
	return 1;
}

int CopySamplesForTimeDomainPlotInternalSync(AudioBlocks *AudioArray, int16_t *samples, size_t size, int slotForSamples, long samplerate, double *window, int AudioChannels, parameters *config)
{
	char			channel = 0;
	long			stereoSignalSize = 0;	
	long			i = 0, monoSignalSize = 0;
	int16_t			*signal = NULL, *window_samples = NULL;
	
	if(!AudioArray)
	{
		logmsg("No Array for results\n");
		return 0;
	}

	if(!AudioArray->internalSync)
	{
		logmsg("ERROR: Internal Sync not allocated\n");
		return 0;
	}

	if(AudioArray->internalSync[slotForSamples].samples)
	{
		logmsg("ERROR: Waveforms already stored\n");
		return 0;
	}

	if(slotForSamples > AudioArray->internalSyncCount - 1)
	{
		logmsg("ERROR: Insufficient slots\n");
		return 0;
	}

	stereoSignalSize = (long)size;
	monoSignalSize = stereoSignalSize/AudioChannels;	 /* 4 is 2 16 bit values */

	signal = (int16_t*)malloc(sizeof(int16_t)*(monoSignalSize+1));
	if(!signal)
	{
		logmsg("Not enough memory\n");
		return(0);
	}
	memset(signal, 0, sizeof(int16_t)*(monoSignalSize+1));

	if(config->plotAllNotesWindowed && window)
	{
		window_samples = (int16_t*)malloc(sizeof(int16_t)*(monoSignalSize+1));
		if(!window_samples)
		{
			logmsg("Not enough memory\n");
			return(0);
		}
		memset(window_samples, 0, sizeof(int16_t)*(monoSignalSize+1));
	}

	if(AudioChannels == 1)
		channel = CHANNEL_LEFT;
	else
		channel = CHANNEL_STEREO;

	for(i = 0; i < monoSignalSize; i++)
	{
		if(channel == CHANNEL_LEFT)
			signal[i] = (double)samples[i*AudioChannels];
		if(channel == CHANNEL_RIGHT)
			signal[i] = (double)samples[i*2+1];
		if(channel == CHANNEL_STEREO)
			signal[i] = (double)((double)samples[i*2]+(double)samples[i*2+1])/2.0;
	}

	AudioArray->internalSync[slotForSamples].samples = signal;
	AudioArray->internalSync[slotForSamples].size = monoSignalSize;
	AudioArray->internalSync[slotForSamples].difference = 0;

	if(config->plotAllNotesWindowed && window)
	{
		for(i = 0; i < monoSignalSize; i++)
			window_samples[i] = (double)((double)signal[i]*window[i]);
		AudioArray->audio.window_samples = window_samples;
	}

	return(1);
}
