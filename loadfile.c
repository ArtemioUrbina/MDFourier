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
#include "profile.h"
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
		if(!FLACtoSignal(fileName, *Signal))
		{
			char *error  = NULL;

			error = getflacErrorStr();
			if(!flacErrorReported())
			{
				if(!error)
					logmsg("\nERROR: Invalid FLAC file %s\n", fileName);
				else
					logmsg("\nERROR: Invalid FLAC (%s) file %s\n", error, fileName);
			}
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

		if(!LoadWAVFile(file, *Signal, config))
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

// no endianess considerations, PCM in RIFF is little endian and this code is little endian
void ConvertByteArrayToIEEE32Sample(const void *buf, void *target)
{
	const unsigned char *b = (const unsigned char *)buf;
	unsigned char *t = (unsigned char *)target;

	t[0] = b[0];
	t[1] = b[1];
	t[2] = b[2];
	t[3] = b[3];
}

// no endianess considerations, PCM in RIFF is little endian and this code is little endian
void ConvertByteArrayToIEEE64Sample(const void *buf, void *target)
{
	const unsigned char *b = (const unsigned char *)buf;
	unsigned char *t = (unsigned char *)target;

	t[0] = b[0];
	t[1] = b[1];
	t[2] = b[2];
	t[3] = b[3];
	t[4] = b[4];
	t[5] = b[5];
	t[6] = b[6];
	t[7] = b[7];
}

// For future(?) Endianess compatibility
uint32_t EndianessChange32bits(uint32_t num)
{
	uint32_t swapped = 0;
	
	swapped = ((num>>24)&0xff) | // move byte 3 to byte 0
              ((num<<8)&0xff0000) | // move byte 1 to byte 2
              ((num>>8)&0xff00) | // move byte 2 to byte 1
              ((num<<24)&0xff000000); // byte 0 to byte 3
	return swapped;
}

uint16_t EndianessChange16bits(uint16_t num)
{
	uint16_t swapped = 0;
	
	swapped = (num>>8) | (num<<8);
	return swapped;
}


int CheckFactChunk(FILE *file, AudioSignal *Signal)
{
	size_t				bytesRead = 0;

	bytesRead = fread(&Signal->fact, 1, sizeof(fact_ck), file);
	if(bytesRead == sizeof(fact_ck))
		Signal->factExists = 1;
	else
		logmsg("\tWARNING: Extensible wave requires a fact chunk. Using header data.\n");

	if(Signal->fmtType == FMT_TYPE_3_SIZE)  // cautious
	{
		fmt_hdr_ext2 *fmt_ext = NULL;

		fmt_ext = (fmt_hdr_ext2*)Signal->fmtExtra;
		if(fmt_ext->wValidBitsPerSample != Signal->header.fmt.bitsPerSample)
			logmsg("\tWARNING: Extensible wave bits per sample differ from header bits per sample.\n");
		if(fmt_ext->formatCode != WAVE_FORMAT_PCM && fmt_ext->formatCode != WAVE_FORMAT_IEEE_FLOAT)
		{
			logmsg("\tERROR: Only 16/24/32bit PCM or 32/64 bit IEEE float supported. (fact chunk)\n\tPlease convert file sample format.\n");
			return 0;
		}
		Signal->header.fmt.AudioFormat = fmt_ext->formatCode;
	}
	return 1;
}

int LoadWAVFile(FILE *file, AudioSignal *Signal, parameters *config)
{
	int					found = 0, samplesLoaded = 0, validformat = 0;
	size_t				bytesRead = 0;
	struct timespec		start, end;
	long int			samplePos = 0, srcPos = 0, byteOffset = 0;
	uint8_t				*fileBytes = NULL;

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	if(!file)
		return 0;

	if(fread(&Signal->header.riff, 1, sizeof(riff_hdr), file) != sizeof(riff_hdr))
	{
		logmsg("\tERROR: Invalid Audio file. File too small. (RIFF not found)\n");
		return(0);
	}

	if(strncmp((char*)Signal->header.riff.RIFF, "RIFF", 4) != 0)
	{
		logmsg("\tERROR: Invalid Audio file. Probably not a WAV file (RIFF header not found).\n");
		if(strncmp((char*)Signal->header.riff.RIFF, "fLaC", 4) == 0)
			logmsg("\tWARNING: File has a WAV file extension, but has a FLAC header. Please rename the file.\n");
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
			logmsg("\tERROR: Invalid Audio file. File too small. (Sub chunk not found)\n");
			return(0);
		}
		if(strncmp((char*)schunk.chunkID, "fmt", 3) != 0)
		{
			if(fseek(file, schunk.Size*sizeof(uint8_t), SEEK_CUR) != 0)
				return 0;
		}
		else
		{
			if(fseek(file, -1*(long int)sizeof(sub_chunk), SEEK_CUR) != 0)
				return 0;
			found = 1;
		}
	}while(!found);

	if(fread(&Signal->header.fmt, 1, sizeof(fmt_hdr), file) != sizeof(fmt_hdr))
	{
		logmsg("\tERROR: Invalid Audio file. File too small. (fmt chunk not found)\n");
		return(0);
	}

	switch(Signal->header.fmt.Subchunk1Size)
	{
		case FMT_TYPE_1:
			Signal->fmtType = FMT_TYPE_1_SIZE;
			break;
		case FMT_TYPE_2:
			if(fread(&Signal->fmtExtra, 1, sizeof(fmt_hdr_ext1), file) != sizeof(fmt_hdr_ext1))
			{
				logmsg("\tERROR: Invalid Audio file. File too small. (fmt chunk ext1)\n");
				return(0);
			}
			Signal->fmtType = FMT_TYPE_2_SIZE;
			break;
		case FMT_TYPE_3:
			if(fread(&Signal->fmtExtra, 1, sizeof(fmt_hdr_ext2), file) != sizeof(fmt_hdr_ext2))
			{
				logmsg("\tERROR: Invalid Audio file. File too small. (fmt chunk ext2)\n");
				return(0);
			}
			Signal->fmtType = FMT_TYPE_3_SIZE;
			break;
		default:
			if(Signal->header.fmt.Subchunk1Size + 8 > sizeof(fmt_hdr))  // Add the fmt and chunksize length: 8 bytes
			{
				if(fseek(file, Signal->header.fmt.Subchunk1Size + 8 - sizeof(fmt_hdr), SEEK_CUR) != 0)
					return 0;
			}

			if(config->verbose)
				logmsg("\t-WARNING: Unsupported fmt sub chunk size: %lu\n", Signal->header.fmt.Subchunk1Size);
			break;
	}

	// look for data chunk
	found = 0;
	do
	{
		sub_chunk	schunk;

		if(fread(&schunk, 1, sizeof(sub_chunk), file) != sizeof(sub_chunk))
		{
			logmsg("\tERROR: Invalid Audio file. File too small. (data chunk not found)\n");
			return(0);
		}
		if(strncmp((char*)schunk.chunkID, "data", 4) != 0)
		{
			if(fseek(file, schunk.Size*sizeof(uint8_t), SEEK_CUR) != 0)
				return 0;
		}
		else
		{
			if(fseek(file, -1*(long int)sizeof(sub_chunk), SEEK_CUR))
				return 0;
			found = 1;
		}

		// Read Total Fact if available
		if(Signal->header.fmt.AudioFormat == WAVE_FORMAT_EXTENSIBLE)
		{
			if(strncmp((char*)schunk.chunkID, "fact", 4) == 0)
			{
				// rewind the block and read it
				if(fseek(file, -1*(long int)(sizeof(sub_chunk)+schunk.Size*sizeof(uint8_t)), SEEK_CUR) != 0)
					return 0;

				// fact chunk read
				if(!CheckFactChunk(file, Signal))
					return 0;
			}
		}
	}while(!found);

	if(fread(&Signal->header.data, 1, sizeof(data_hdr), file) != sizeof(data_hdr))
	{
		logmsg("\tERROR: Invalid Audio file. File too small. 5\n");
		return(0);
	}

	if(Signal->header.fmt.NumOfChan == 2 || Signal->header.fmt.NumOfChan == 1) /* Check for Stereo and Mono */
		Signal->AudioChannels = Signal->header.fmt.NumOfChan;

	if(Signal->AudioChannels == INVALID_CHANNELS)
	{
		logmsg("\tERROR: Only Mono and Stereo files are supported. ");
        logmsg("File has %d channels\n", Signal->header.fmt.NumOfChan);
		return(0);
	}

	/* Check bit depth */
	if(Signal->header.fmt.AudioFormat == WAVE_FORMAT_PCM)
	{
		if(Signal->header.fmt.bitsPerSample != 8 && Signal->header.fmt.bitsPerSample != 16 &&
			Signal->header.fmt.bitsPerSample != 24 && Signal->header.fmt.bitsPerSample != 32)
		{
			logmsg("\tERROR: Only 8/16/24/32 bit PCM formats are supported. ");
			return(0);
		}
		validformat = 1;
	}

	if(Signal->header.fmt.AudioFormat == WAVE_FORMAT_IEEE_FLOAT)
	{
		if(Signal->header.fmt.bitsPerSample != 32 && Signal->header.fmt.bitsPerSample != 64)
		{
			logmsg("\tERROR: Only 32/64 bit IEEE float are supported. ");
			return(0);
		}
		validformat = 1;
	}

	if(!validformat && Signal->header.fmt.AudioFormat != WAVE_FORMAT_EXTENSIBLE)
	{
		logmsg("\tERROR: Only 8/16/24/32bit PCM or 32/64 bit IEEE float supported.\n\tPlease convert file sample format.\n");
		return(0);
	}

	if(Signal->header.data.DataSize == 0)
	{
		logmsg("\tERROR: RIFF header has an invalid Data length %ld\n", Signal->header.data.DataSize);
		return(0);
	}

	if(Signal->header.fmt.bitsPerSample == 0)
	{
		logmsg("\tERROR: RIFF header has bits per sample at 0 (AudioFormat: 0x%X)\n", Signal->header.fmt.AudioFormat);
		return(0);
	}

	Signal->bytesPerSample = Signal->header.fmt.bitsPerSample/8;
	Signal->numSamples = Signal->header.data.DataSize/Signal->bytesPerSample;

	if(Signal->factExists) // cautious with IEEE float format
	{
		if(Signal->fact.dwSampleLength*Signal->AudioChannels != (unsigned int)Signal->numSamples)
			logmsg("\tWARNING: Header byte count and fact chunk sample count are not consistent\n");
	}

	byteOffset = ftell(file);
	Signal->SamplesStart = byteOffset;

	fileBytes = (uint8_t*)malloc(sizeof(uint8_t)*Signal->header.data.DataSize);
	if(!fileBytes)
	{
		logmsg("\tERROR: All Chunks malloc failed! [Signal->header.data.DataSize]\n");
		return(0);
	}

	memset(fileBytes, 0, sizeof(uint8_t)*Signal->header.data.DataSize);

	bytesRead = fread(fileBytes, 1, sizeof(uint8_t)*Signal->header.data.DataSize, file);
	if(bytesRead != sizeof(uint8_t)*Signal->header.data.DataSize)
	{
		free(fileBytes);
		logmsg("\tERROR: Corrupt RIFF Header\n\tCould not read the whole sample block from disk to RAM.\n\tBytes Read: %ld Expected: %ld\n",
			bytesRead, sizeof(int8_t)*Signal->header.data.DataSize);
		return(0);
	}

	if(Signal->header.fmt.AudioFormat == WAVE_FORMAT_EXTENSIBLE)
	{
		if(!CheckFactChunk(file, Signal))
		{
			free(fileBytes);
			return 0;
		}
		validformat = 1;
	}

	if(Signal->header.fmt.AudioFormat != WAVE_FORMAT_PCM && /* If fact chunk check didn't remove EXTENSIBLE... */
		Signal->header.fmt.AudioFormat != WAVE_FORMAT_IEEE_FLOAT)
	{
		free(fileBytes);
		logmsg("\tERROR: Only 8/16/24/32bit PCM or 32/64 bit IEEE float supported.\n\tPlease convert file sample format.\n");
		return(0);
	}

	// Convert samples to internal double ones
	Signal->Samples = (double*)malloc(sizeof(double)*Signal->numSamples);
	if(!Signal->Samples)
	{
		free(fileBytes);
		logmsg("\tERROR: Internal sample array malloc failed! [Signal->numSamples]\n");
		return(0);
	}
	memset(Signal->Samples, 0, sizeof(double)*Signal->numSamples);

	// no endianess considerations, PCM in RIFF is little endian and this code is little endian
	if(Signal->header.fmt.AudioFormat == WAVE_FORMAT_PCM)
	{
		for(samplePos = 0; samplePos < Signal->numSamples; samplePos++)
		{
			int32_t	sample = 0;
			int8_t	signSample = 0;
	
			switch(Signal->bytesPerSample)
			{
				case 1:
					sample = fileBytes[srcPos]-0x80;	// 8 bit is unsigned. Convert to signed
					break;
				case 2:
					signSample = fileBytes[srcPos+1];
					if(signSample < 0)
						sample = 0xffff0000;
					sample |= (fileBytes[srcPos+1] << 8) | fileBytes[srcPos];
					break;
				case 3:
					signSample = fileBytes[srcPos+2];
					if(signSample < 0)
						sample = 0xff000000;
					sample |= (fileBytes[srcPos+2] << 16) | (fileBytes[srcPos+1] << 8) | fileBytes[srcPos];
					break;
				case 4:
					sample = (fileBytes[srcPos+3] << 24) | (fileBytes[srcPos+2] << 16) | (fileBytes[srcPos+1] << 8) | fileBytes[srcPos];
					break;
				default:
					logmsg("ERROR: Unsupported audio format (bytes sample %d)\n", Signal->bytesPerSample);
					free(fileBytes);
					return 0;
			}
			srcPos += Signal->bytesPerSample;
	
			Signal->Samples[samplePos] = (double)sample;
		}

		samplesLoaded = 1;
	}

	if(Signal->header.fmt.AudioFormat == WAVE_FORMAT_IEEE_FLOAT && Signal->header.fmt.bitsPerSample == 32)
	{
		for(samplePos = 0; samplePos < Signal->numSamples; samplePos++)
		{
			float	sample = 0;
	
			ConvertByteArrayToIEEE32Sample(fileBytes+srcPos, &sample);
			Signal->Samples[samplePos] = (double)sample;
			srcPos += 4;
		}

		samplesLoaded = 1;
	}

	if(Signal->header.fmt.AudioFormat == WAVE_FORMAT_IEEE_FLOAT && Signal->header.fmt.bitsPerSample == 64)
	{
		for(samplePos = 0; samplePos < Signal->numSamples; samplePos++)
		{
			double	sample = 0;
	
			ConvertByteArrayToIEEE64Sample(fileBytes+srcPos, &sample);
			Signal->Samples[samplePos] = (double)sample;
			srcPos += 8;
		}

		samplesLoaded = 1;
	}

	free(fileBytes);
	fileBytes = NULL;

	if(!samplesLoaded)
	{
		logmsg("ERROR: Unsupported audio format, samples were not loaded\n");
		return 0;
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

	if(config->ManualSyncRef || config->ManualSyncComp)
	{
		int setvalue = 0;

		if(config->ManualSyncRef && Signal->role == ROLE_REF)
		{
			Signal->startOffset = config->ManualSyncRefStart*Signal->AudioChannels; 
			Signal->endOffset = config->ManualSyncRefEnd*Signal->AudioChannels;
			if(Signal->endOffset > Signal->numSamples)
			{
				logmsg("\nERROR: End offset is out of bounds, file ends at sample %ld and was asked for %ld.\n",
					Signal->numSamples,
					SamplesForDisplay(Signal->endOffset, Signal->AudioChannels));
				return 0;
			}
			Signal->framerate = CalculateFrameRateAndCheckSamplerate(Signal, config);
			setvalue = 1;
		}

		if(config->ManualSyncComp && Signal->role == ROLE_COMP)
		{
			Signal->startOffset = config->ManualSyncCompStart*Signal->AudioChannels; 
			Signal->endOffset = config->ManualSyncCompEnd*Signal->AudioChannels;
			if(Signal->endOffset > Signal->numSamples)
			{
				logmsg("\nERROR: End offset is out of bounds, file ends at sample %ld and was asked for %ld.\n",
					Signal->numSamples, 
					SamplesForDisplay(Signal->endOffset, Signal->AudioChannels));
				return 0;
			}
			Signal->framerate = CalculateFrameRateAndCheckSamplerate(Signal, config);
			setvalue = 1;
		}

		if(setvalue)
		{
			if(!Signal->framerate)
			{
				logmsg("\nERROR: Framerate could not be detected.\n");
				return 0;
			}

			logmsg(" - Detected %.8g Hz signal (%.8gms per frame) from Audio file\n", 
						roundFloat(CalculateScanRate(Signal)), Signal->framerate);

			seconds = (double)Signal->numSamples/Signal->SampleRate/(double)Signal->AudioChannels;
			if(seconds < GetSignalTotalDuration(Signal->framerate, config))
				logmsg(" - File length is smaller than the expected %gs\n",
					GetSignalTotalDuration(Signal->framerate, config));

			if(GetFirstSilenceIndex(config) != NO_INDEX)
				Signal->hasSilenceBlock = 1;
			return 1;
		}
	}
	
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

			logmsg("\nERROR: '%s' starting pulse train was not detected.\nProfile used: [%s]\n", 
				getRoleText(Signal), config->types.Name);

			if(Signal->role == ROLE_REF)
				format = config->videoFormatRef;
			else
				format = config->videoFormatCom;
			if(!config->syncTolerance)
				logmsg(" - You can try using -T for a more tolerant pulse detection algorithm\n");
			if(config->syncTolerance == 1)
				logmsg(" - You can try using -TT for an even more tolerant pulse detection algorithm\n");
			if(config->syncTolerance == 2)
				logmsg(" - You can try using -TTT for the most tolerant pulse detection algorithm\n");
			if((format != 0 || config->smallFile) && config->types.syncCount != 1)
				logmsg(" - This signal is configured as '%s'%s, check if that is not the issue.\n", 
							config->types.SyncFormat[format].syncName, config->smallFile ? " and is smaller than expected" : "");
			if(config->trimmingNeeded)
				logmsg(" - Leading/tailing silence too long, if sync detection fails please consider trimming\n");
			return 0;
		}
		config->syncAlignIterator++;
		
		if(config->verbose || config->debugSync) {
			logmsg("\n\t   %gs [%ld samples", 
				SamplesToSeconds(Signal->SampleRate, Signal->startOffset, Signal->AudioChannels),
				SamplesForDisplay(Signal->startOffset, Signal->AudioChannels));
			if(!IsFlac(Signal->SourceFile))
				logmsg("|%ld bytes|%ld bytes/head", 
					SamplesToBytes(Signal->startOffset, Signal->bytesPerSample),
					SamplesToBytes(Signal->startOffset, Signal->bytesPerSample)+Signal->SamplesStart);
			logmsg("]\n");
		}

		if(GetLastSyncIndex(config) != NO_INDEX)
		{
			double diff = 0, expected = 0;

			if(config->verbose) { 
				logmsg("\t to");
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
				if((format != 0 || config->smallFile) && config->types.syncCount != 1)
					logmsg(" - This signal is configured as '%s'%s, check if that is not the issue.\n", 
							config->types.SyncFormat[format].syncName, config->smallFile ? " and is smaller than expected" : "");
				return 0;
			}

			config->syncAlignIterator++;

			if(config->verbose) {
				logmsg(" %gs [%ld samples", 
					SamplesToSeconds(Signal->SampleRate, Signal->endOffset, Signal->AudioChannels),
					SamplesForDisplay(Signal->endOffset, Signal->AudioChannels));
				if(!IsFlac(Signal->SourceFile))
					logmsg("|%ld bytes|%ld bytes/head", 
						SamplesToBytes(Signal->endOffset, Signal->bytesPerSample),
						SamplesToBytes(Signal->endOffset, Signal->bytesPerSample)+Signal->SamplesStart);
				logmsg("]\n");
			}

			Signal->framerate = CalculateFrameRateAndCheckSamplerate(Signal, config);
			if(!Signal->framerate)
			{
				logmsg("\nERROR: Framerate could not be detected.\n");
				return 0;
			}

			if(Signal->originalSR != 0.0)
				logmsg(" - Using adjusted %.8g Hz signal (%.8gms per frame) from Audio signal duration\n",
					roundFloat(CalculateScanRate(Signal)), Signal->framerate);
			else
				logmsg(" - Detected %.8g Hz signal (%.8gms per frame) from Audio file\n", 
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
		
				logmsg("\n\t   %gs [%ld samples", 
					SamplesToSeconds(Signal->SampleRate, Signal->startOffset, Signal->AudioChannels),
					SamplesForDisplay(Signal->startOffset, Signal->AudioChannels));
				if(!IsFlac(Signal->SourceFile))
					logmsg("|%ld bytes|%ld bytes/head", 
						SamplesToBytes(Signal->startOffset, Signal->bytesPerSample),
						SamplesToBytes(Signal->startOffset, Signal->bytesPerSample)+Signal->SamplesStart);
				logmsg("]\n");

				Signal->endOffset = SecondsToSamples(Signal->SampleRate, 
										GetSignalTotalDuration(Signal->framerate, config), 
										Signal->AudioChannels, NULL, NULL);
			}
			break;
			case NO_SYNC_MANUAL:
			{
				double diff = 0, expected = 0;
		
				logmsg(" - WARNING: Files must be identically trimmed for this to work at some level\n");
				Signal->startOffset = 0;
				Signal->endOffset = Signal->numSamples;
		
				expected = Signal->framerate;
		
				if(Signal->role == ROLE_REF)
				{
					double secondsTmp = 0;
					secondsTmp = SamplesToSeconds(Signal->SampleRate, Signal->endOffset, Signal->AudioChannels);
					config->NoSyncTotalFrames = (secondsTmp*1000)/expected;
					Signal->framerate = expected;
					logmsg(" - Loaded %.8g Hz signal (%.8gms per frame) from profile file\n", 
								CalculateScanRate(Signal), Signal->framerate);
				}
				else
				{
					Signal->framerate = CalculateFrameRateNS(Signal, config->NoSyncTotalFrames, config);
					logmsg(" - Detected %.8g Hz signal (%.8gms per frame) from Audio file\n", 
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
			case NO_SYNC_LENGTH:
			{
				Signal->startOffset = 0;
				Signal->endOffset = SecondsToSamples(Signal->SampleRate,
					GetSignalTotalDuration(Signal->framerate, config),
					Signal->AudioChannels, NULL, NULL);
				if (Signal->endOffset > Signal->numSamples)
				{
					logmsg(" - ERROR: Files must be at least %g seconds long\n", 
							SamplesToSeconds(Signal->SampleRate, Signal->endOffset, Signal->AudioChannels));
					return 0;
				}
			}
			break;
			case NO_SYNC_DIGITAL:
			{
				/* Find the start offset based on zeroes */
				int					found = 0;
				long int			i = 0, startOffset = -1;
				
				logmsg(" - Detecting audio signal from pure digital source recording: ");
				
				for(i = 0; i < Signal->numSamples; i += Signal->AudioChannels)
				{
					int ac =0;

					for(ac = 0; ac < Signal->AudioChannels; ac++)
					{
						if(Signal->Samples[i+ac] != 0)
						{
							startOffset = i;
							found = 1;
							break;
						}
					}
					if(found)
						break;
				}
				Signal->startOffset = startOffset;
				if(Signal->startOffset == -1)
				{
					logmsg("\nERROR: Starting position was not detected.\n");
					return 0;
				}
		
				logmsg("\n\t   %gs [%ld samples", 
					SamplesToSeconds(Signal->SampleRate, Signal->startOffset, Signal->AudioChannels),
					SamplesForDisplay(Signal->startOffset, Signal->AudioChannels));
				if(!IsFlac(Signal->SourceFile))
					logmsg("|%ld bytes|%ld bytes/head", 
						SamplesToBytes(Signal->startOffset, Signal->bytesPerSample),
						SamplesToBytes(Signal->startOffset, Signal->bytesPerSample)+Signal->SamplesStart);
				logmsg("]\n");

				Signal->endOffset = SecondsToSamples(Signal->SampleRate, 
										GetSignalTotalDuration(Signal->framerate, config), 
										Signal->AudioChannels, NULL, NULL);
			}
			config->significantAmplitude = -90;
			break;
			default:
			{
				logmsg("\nERROR: Invalid profile\n");
				return 0;
			}
		}
	}

	seconds = (double)Signal->numSamples/Signal->SampleRate/(double)Signal->AudioChannels;
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

	// initial assignment as double
	Signal->SampleRate = Signal->header.fmt.SamplesPerSec;

	// Default if none is found
	Signal->framerate = GetMSPerFrame(Signal, config);

	Signal->AudioChannels = Signal->header.fmt.NumOfChan;
	if(Signal->SampleRate/2 < config->endHz)
	{
		logmsg(" - %d Hz sample rate was too low for %gHz-%gHz analysis\n",
			 Signal->header.fmt.SamplesPerSec, config->startHz, config->endHz);

		Signal->endHz = Signal->SampleRate/2;
		Signal->nyquistLimit = 1;

		logmsg(" - Changed to %gHz-%gHz for this file\n", config->startHz, Signal->endHz);
	}

	seconds = (double)Signal->numSamples/(double)Signal->header.fmt.SamplesPerSec/(double)Signal->AudioChannels;
	logmsg(" - Audio file header reports %dHz %dbits %s %s and %g seconds long\n", 
		Signal->header.fmt.SamplesPerSec, 
		Signal->header.fmt.bitsPerSample,
		Signal->header.fmt.AudioFormat == WAVE_FORMAT_IEEE_FLOAT ? "IEEE float" : "PCM", 
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
	double		*sampleBuffer = NULL;
	double		signalLengthSeconds = 0;
	long int	signalLengthFrames = 0, signalLengthSamples = 0;

	signalLengthFrames = GetInternalSyncTotalLength(element, config);
	if(!signalLengthFrames)
	{
		logmsg("\tERROR: Internal Sync block has no frame duration. Aborting.\n");
		return 0;
	}

	signalLengthSeconds = FramesToSeconds(signalLengthFrames, config->referenceFramerate);
	signalLengthSamples = SecondsToSamples(Signal->SampleRate, signalLengthSeconds, Signal->AudioChannels, NULL, NULL);

	if(pos + signalStartOffset + signalLengthSamples > Signal->numSamples)
	{
		signalLengthSamples = Signal->numSamples - (pos+signalStartOffset);
		if(config->verbose) {
			logmsg(" - Internal sync adjust: Signal is smaller than expected\n");
		}
	}

	if(config->verbose) {
		logmsg(" - Internal Segment Info:\n\tSignal Start Offset: %ld Frames: %d Seconds: %g NumSamples: %ld\n\n",
				SamplesForDisplay(pos+signalStartOffset, Signal->AudioChannels), 
				signalLengthFrames, signalLengthSeconds,
				SamplesForDisplay(signalLengthSamples, Signal->AudioChannels));
	}

	sampleBuffer = (double*)malloc(sizeof(double)*signalLengthSamples);
	if(!sampleBuffer)
	{
		logmsg("\tERROR: Out of memory [signalLengthSamples]\n");
		return 0;
	}

	memset(sampleBuffer, 0, sizeof(double)*signalLengthSamples);

	/*
	if(config->verbose)
	{
		logmsg(" - MOVEMENTS:\n");
		logmsg("\tCopy: From %ld Samples: %ld\n",
				SamplesForDisplay(pos + signalStartOffset, Signal->AudioChannels), 
				SamplesForDisplay(signalLengthSamples, Signal->AudioChannels));
		logmsg("\tZero Out: Pos: %ld Samples: %ld\n",
				SamplesForDisplay(pos+signalStartOffset, Signal->AudioChannels),
				SamplesForDisplay(signalLengthSamples, Signal->AudioChannels));
		logmsg("\tStore: Pos: %ld Samples: %ld\n",
				SamplesForDisplay(pos, Signal->AudioChannels),
				SamplesForDisplay(signalLengthSamples, Signal->AudioChannels));
	}
	*/

	memcpy(sampleBuffer, Signal->Samples + pos + signalStartOffset, signalLengthSamples*sizeof(double));
	memset(Signal->Samples + pos + signalStartOffset, 0, signalLengthSamples*sizeof(double));
	memcpy(Signal->Samples + pos, sampleBuffer, signalLengthSamples*sizeof(double));

	free(sampleBuffer);
	return 1;
}

int MoveSampleBlockExternal(AudioSignal *Signal, long int element, long int pos, long int signalStartOffset, parameters *config)
{
	double		*sampleBuffer = NULL;
	double		signalLengthSeconds = 0;
	long int	signalLengthFrames = 0, signalLengthSamples = 0;

	signalLengthFrames = GetRemainingLengthFromElement(element, config);
	if(!signalLengthFrames)
	{
		logmsg("\tERROR: Internal Sync block has no frame duration. Aborting.\n");
		return 0;
	}

	signalLengthSeconds = FramesToSeconds(signalLengthFrames, config->referenceFramerate);
	signalLengthSamples = SecondsToSamples(Signal->SampleRate, signalLengthSeconds, Signal->AudioChannels, NULL, NULL);

	if(pos + signalStartOffset + signalLengthSamples > Signal->numSamples)
	{
		signalLengthSamples = Signal->numSamples - (pos+signalStartOffset);
		if(config->verbose) {
			logmsg(" - Internal sync adjust: Signal is smaller than expected\n");
		}
	}
	if(config->verbose) {
		logmsg(" - Internal Segment Info:\n\tFinal Offset: %ld Frames: %d Seconds: %g Samples: %ld\n",
				SamplesForDisplay(pos+signalStartOffset, Signal->AudioChannels), 
				signalLengthFrames, signalLengthSeconds,
				SamplesForDisplay(signalLengthSamples, Signal->AudioChannels));
	}

	sampleBuffer = (double*)malloc(sizeof(double)*signalLengthSamples);
	if(!sampleBuffer)
	{
		logmsg("\tERROR: Out of memory while performing internal Sync adjustments. [signalLengthSamples]\n");
		return 0;
	}
	memset(sampleBuffer, 0, sizeof(double)*signalLengthSamples);

	/*
	if(config->verbose)
	{
		logmsg(" - MOVEMENTS:\n");
		logmsg("\tCopy: From %ld Samples: %ld\n",
				SamplesForDisplay(pos + signalStartOffset, Signal->AudioChannels), 
				SamplesForDisplay(signalLengthSamples, Signal->AudioChannels));
		logmsg("\tZero Out: Pos: %ld Samples: %ld\n",
				SamplesForDisplay(pos, Signal->AudioChannels), 
				SamplesForDisplay(Signal->numSamples-pos, Signal->AudioChannels));
		logmsg("\tStore: Pos: %ld Samples: %ld\n",
				SamplesForDisplay(pos, Signal->AudioChannels),
				SamplesForDisplay(signalLengthSamples, Signal->AudioChannels));
	}
	*/

	memcpy(sampleBuffer, Signal->Samples + pos + signalStartOffset, signalLengthSamples*sizeof(double));
	memset(Signal->Samples + pos, 0, (Signal->numSamples-pos)*sizeof(double));
	memcpy(Signal->Samples + pos, sampleBuffer, signalLengthSamples*sizeof(double));

	free(sampleBuffer);
	return 1;
}

int ProcessInternalSync(AudioSignal *Signal, long int element, long int pos, int *syncinternal, long int *syncAdvance, int knownLength, parameters *config)
{
	int			syncToneFreq = 0, toleranceIssue = 0;
	double		syncLenSeconds = 0;
	long int	internalSyncOffset = 0, syncLengthSamples = 0,
				endPulseSamples = 0, pulseLengthSamples = 0, signalStart = 0;

	if(!syncinternal)
	{
		logmsg("-ERROR: Internal error, ProcessInternalSync called without pointer\n");
		return 0;
	}

	if(*syncinternal)
	{
		*syncinternal = 0;
		return 1;
	}


	syncToneFreq = GetInternalSyncTone(element, config);
	syncLenSeconds = GetInternalSyncLen(element, config);
	syncLengthSamples = SecondsToSamples(Signal->SampleRate, syncLenSeconds, Signal->AudioChannels, NULL, NULL);

	// we send , syncLengthSamples/2 since it is half silence half pulse
	internalSyncOffset = DetectSignalStart(Signal->Samples, Signal->header, pos, syncToneFreq, syncLengthSamples/2, &endPulseSamples, &toleranceIssue, config);
	if(internalSyncOffset == -1)
	{
		logmsg("\tERROR: No signal found while in internal sync detection.\n");
		return 0;  // Was warning with -1
	}
	*syncinternal = 1;

	if(toleranceIssue)
		config->internalSyncTolerance |= Signal->role;
	
	pulseLengthSamples = endPulseSamples - internalSyncOffset;
	internalSyncOffset -= pos;
	signalStart = internalSyncOffset;

	if(pulseLengthSamples < (syncLengthSamples/2)*0.90)
	{
		double expected = 0, got = 0;

		got = SamplesToSeconds(Signal->SampleRate, pulseLengthSamples, Signal->AudioChannels);
		expected = SamplesToSeconds(Signal->SampleRate, syncLengthSamples, Signal->AudioChannels);

		if(!config->syncTolerance)
		{
			logmsg(" - ERROR: Internal Sync %dhz tone starting at %gs was shorter than expected. Found %gms instead of %gms (can ignore with -T)\n",
				syncToneFreq, SamplesToSeconds(Signal->SampleRate, pos + internalSyncOffset, Signal->AudioChannels),
				got, expected);
			return 0;
		}
		logmsg(" - NOTE: (Ignored by -T) Internal Sync %dhz tone starting at %gs was shorter than expected. Found %gms instead of %gms\n",
				syncToneFreq, SamplesToSeconds(Signal->SampleRate, pos + internalSyncOffset, Signal->AudioChannels),
				got, expected);
	}

	if(knownLength == TYPE_INTERNAL_KNOWN)
	{
		Signal->delayArray[Signal->delayElemCount++] = SamplesToSeconds(Signal->SampleRate, internalSyncOffset, Signal->AudioChannels)*1000.0;
		logmsg(" - %s command delay: %g ms [%g frames]\n",
			GetBlockName(config, element),
			SamplesToSeconds(Signal->SampleRate, internalSyncOffset, Signal->AudioChannels)*1000.0,
			SamplesToFrames(Signal->SampleRate, internalSyncOffset, config->referenceFramerate, Signal->AudioChannels));

		if(config->verbose) {
				logmsg("  > Found at: %ld (%ld+%ld) smp\n\tPulse Length: %ld smp Silence Length: %ld smp\n", 
					SamplesForDisplay(pos + internalSyncOffset, Signal->AudioChannels),
					SamplesForDisplay(pos, Signal->AudioChannels),
					SamplesForDisplay(internalSyncOffset, Signal->AudioChannels),
					SamplesForDisplay(pulseLengthSamples, Signal->AudioChannels), 
					SamplesForDisplay(syncLengthSamples/2, Signal->AudioChannels));
		}

		/* Copy waveforms for visual inspection, create 3 slots: silence, sync pulse, silence */
		if(config->plotAllNotes)
		{
			if(!initInternalSync(&Signal->Blocks[element], 3))
				return 0;
		
			if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
					(Signal->Samples + pos), 
					internalSyncOffset, 0, 
					NULL, Signal->AudioChannels, config))
				return 0;

			if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
					(Signal->Samples + pos + internalSyncOffset), 
					pulseLengthSamples, 1, 
					NULL, Signal->AudioChannels, config))
				return 0;

			if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
					(Signal->Samples + pos + internalSyncOffset + pulseLengthSamples), 
					syncLengthSamples/2, 2, 
					NULL, Signal->AudioChannels, config))
				return 0;
		}

		// skip sync tone-which is silence-taken from config file
		internalSyncOffset += syncLengthSamples;

		if(!MoveSampleBlockInternal(Signal, element, pos, internalSyncOffset, config))
			return 0;
	}
	if(knownLength == TYPE_INTERNAL_UNKNOWN)  // Our sync is outside the frame detection zone
	{
		long int 	silenceLengthSamples = 0;
		/* long int 	oneframe = 0; */

		silenceLengthSamples = syncLengthSamples/2;

		if(pulseLengthSamples != silenceLengthSamples)
		{
			if(pulseLengthSamples > silenceLengthSamples)
			{
				long int compensate = 0;

				// Length must be fixed, so we detected start sooner
				compensate = pulseLengthSamples - silenceLengthSamples;

				signalStart += compensate;
				internalSyncOffset += compensate;

				pulseLengthSamples = silenceLengthSamples;
			}
			else
			{
				double expected = 0, got = 0;

				got = SamplesToSeconds(Signal->SampleRate, pulseLengthSamples, Signal->AudioChannels);
				expected = SamplesToSeconds(Signal->SampleRate, silenceLengthSamples, Signal->AudioChannels);

				if(expected - got > 0.00015)  // This is the expected error in ms
					logmsg(" - WARNING: Internal Sync was shorter than the expected %gms by %gms\n", expected, expected - got);
			}
			//silenceLengthSamples = syncLengthSamples - pulseLengthSamples;
		}

		Signal->delayArray[Signal->delayElemCount++] = SamplesToSeconds(Signal->SampleRate, internalSyncOffset, Signal->AudioChannels)*1000.0;
		logmsg(" - %s command delay: %g ms [%g frames]\n",
			GetBlockName(config, element),
			SamplesToSeconds(Signal->SampleRate, internalSyncOffset, Signal->AudioChannels)*1000.0,
			SamplesToFrames(Signal->SampleRate, internalSyncOffset, config->referenceFramerate, Signal->AudioChannels));

		if(config->verbose) {
			logmsg("  > Found at: %ld (%ld+%ld) smp\n\tPulse Length: %ld smp Silence Length: %ld smp\n", 
				SamplesForDisplay(pos + internalSyncOffset, Signal->AudioChannels),
				SamplesForDisplay(pos, Signal->AudioChannels),
				SamplesForDisplay(internalSyncOffset, Signal->AudioChannels),
				SamplesForDisplay(pulseLengthSamples, Signal->AudioChannels), 
				SamplesForDisplay(syncLengthSamples/2, Signal->AudioChannels));
		}

		// skip the pulse real duration to sync perfectly
		signalStart += pulseLengthSamples;
		// skip half the sync tone-which is silence-taken from config file
		signalStart += silenceLengthSamples;

		/* Copy waveforms for visual inspection, create 3 slots: silence, sync pulse, silence */
		if(config->plotAllNotes)
		{
			if(!initInternalSync(&Signal->Blocks[element], 3))
				return 0;
		
			if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
					(Signal->Samples + pos), 
					internalSyncOffset, 0, 
					NULL, Signal->AudioChannels, config))
				return 0;

			if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
					(Signal->Samples + pos + internalSyncOffset), 
					pulseLengthSamples, 1, 
					NULL, Signal->AudioChannels, config))
				return 0;

			if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
					(Signal->Samples + pos + internalSyncOffset + pulseLengthSamples), 
					silenceLengthSamples/2, 2, 
					NULL, Signal->AudioChannels, config))
				return 0;

			/*
			oneframe = SecondsToSamples(Signal->SampleRate, FramesToSeconds(1, config->referenceFramerate), Signal->AudioChannels, NULL, NULL);
			if(!CopySamplesForTimeDomainPlotInternalSync(&Signal->Blocks[element], 
					(Signal->Samples + pos + signalStart), 
					(oneframe*2), 3, 
					NULL, Signal->AudioChannels, config))
				return 0;
			*/
		}

		/* Do the real processing */
		if(!MoveSampleBlockExternal(Signal, element, pos, signalStart, config))
			return 0;

		// Adjust for MDWave with syncAdvance
		internalSyncOffset = signalStart;
	}

	if(syncAdvance)
		*syncAdvance += internalSyncOffset;

	return 1;
}

int CopySamplesForTimeDomainPlotInternalSync(AudioBlocks *AudioArray, double *samples, size_t size, int slotForSamples, double *window, int AudioChannels, parameters *config)
{
	char			channel = 0;
	long			stereoSignalSize = 0;	
	long			i = 0, monoSignalSize = 0;
	double			*signal = NULL, *windowed_samples = NULL;
	
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

	signal = (double*)malloc(sizeof(double)*(monoSignalSize+1));
	if(!signal)
	{
		logmsg("Not enough memory [monoSignalSize]\n");
		return(0);
	}
	memset(signal, 0, sizeof(double)*(monoSignalSize+1));

	if(config->plotAllNotesWindowed && window)
	{
		windowed_samples = (double*)malloc(sizeof(double)*(monoSignalSize+1));
		if(!windowed_samples)
		{
			logmsg("Not enough memory [windowed_samples]\n");
			return(0);
		}
		memset(windowed_samples, 0, sizeof(double)*(monoSignalSize+1));
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
			signal[i] = (double)samples[i*AudioChannels+1];
		if(channel == CHANNEL_STEREO)
			signal[i] = (double)((double)samples[i*AudioChannels]+(double)samples[i*AudioChannels+1])/2.0;
	}

	AudioArray->internalSync[slotForSamples].samples = signal;
	AudioArray->internalSync[slotForSamples].size = monoSignalSize;
	AudioArray->internalSync[slotForSamples].difference = 0;
	AudioArray->internalSync[slotForSamples].padding = 0;

	if(config->plotAllNotesWindowed && window)
	{
		for(i = 0; i < monoSignalSize; i++)
			windowed_samples[i] = (double)((double)signal[i]*window[i]);
		AudioArray->audio.windowed_samples = windowed_samples;
	}

	return(1);
}
