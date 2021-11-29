/* 
 * MDFourier
 * A Fourier Transform analysis tool to compare game console audio
 * http://junkerhq.net/MDFourier/
 *
 * Copyright (C)2019-2021 Artemio Urbina
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

/*
 * This implements libFLAC to decode a FLAC file to a data structure in RAM
 * It only supports 16/24-bit mono/stereo files.
 *
 * Complete API documentation can be found at:
 *	 http://xiph.org/flac/api/
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include "flac.h"
#include "log.h"
#include "freq.h"
#include "FLAC/stream_decoder.h"

#include <ctype.h>

int flacInternalMDFErrors = 0;

extern char *getFilenameExtension(char *filename);
extern int getExtensionLength(char *filename);
static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
static void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
static void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);

int flacErrorReported()
{
	return(flacInternalMDFErrors);
}

char *strtoupper(char *str)
{
	unsigned char *p = (unsigned char *)str;
	
	while (*p) {
		*p = (unsigned char)toupper((unsigned char)*p);
		p++;
	}
	
	return str;
}

int IsFlac(char *name)
{
	int		len = 0;
	const char 	*ext = NULL;

	ext = getFilenameExtension(name);
	if(ext)
	{
		char extHC[10];

		len = strlen(ext);
		if(len != 4)
			return 0;

		memset(extHC, 0, sizeof(char)*10);
		strncpy(extHC, ext, len);
		if(strncmp(strtoupper(extHC), "FLAC", 4) == 0)
			return 1;
	}
	return 0;
}

// Fill header data for MDWave/SaveWAVEChunk
int FillRIFFHeader(wav_hdr *header)
{
	if(!header)
	{
		logmsg("ERROR: Received NULL RIFF header\n");
		return 0;
	}

	// RIFF
	memcpy(header->riff.RIFF, "RIFF", sizeof(char)*4);
	header->riff.ChunkSize = header->data.DataSize+36;
	memcpy(header->riff.WAVE, "WAVE", sizeof(char)*4);

	// fmt
	memcpy(header->fmt.fmt, "fmt ", sizeof(char)*4);
	header->fmt.Subchunk1Size = 16;

	// data
	memcpy(header->data.DataID, "data", sizeof(char)*4);
	return 1;
}

int FLACtoSignal(char *input, AudioSignal *Signal)
{
	FLAC__bool ok = true;
	FLAC__StreamDecoder *decoder = 0;
	FLAC__StreamDecoderInitStatus init_status;

	if(!Signal) {
		logmsg("ERROR: opening empty Data Structure\n");
		return 0;
	}

	if((decoder = FLAC__stream_decoder_new()) == NULL) {
		logmsg("ERROR: allocating decoder\n");
		return 0;
	}

	(void)FLAC__stream_decoder_set_md5_checking(decoder, true);

	init_status = FLAC__stream_decoder_init_file(decoder, input, write_callback, metadata_callback, error_callback, /*client_data=*/Signal);
	if(init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
		logmsg("ERROR: Initializing FLAC decoder: %s\n", FLAC__StreamDecoderInitStatusString[init_status]);
		ok = false;
	}

	if(ok) {
		ok = FLAC__stream_decoder_process_until_end_of_stream(decoder);
		if(!ok)
		{
			if(!flacInternalMDFErrors)
				logmsg("ERROR: (FLAC) %s\n", FLAC__StreamDecoderStateString[FLAC__stream_decoder_get_state(decoder)]);
			Signal->errorFLAC++;
		}
	}

	FLAC__stream_decoder_delete(decoder);

	if(Signal->header.data.DataSize != (uint32_t)Signal->samplesPosFLAC*Signal->bytesPerSample)
	{
		if((uint32_t)Signal->samplesPosFLAC > Signal->header.data.DataSize)  // Buffer overflow!!!
		{
			logmsg("ERROR: FLAC decoder had a buffer overflow\n Got%lu bytes and expected %lu bytes\n",
				Signal->samplesPosFLAC, Signal->header.data.DataSize);
			return 0;
		}
		//if(config->verbose)
		if(!flacInternalMDFErrors)
			logmsg(" - WARNING: FLAC decoder got %lu bytes and expected %lu bytes (fixed internally)\n",
				Signal->samplesPosFLAC*Signal->bytesPerSample, Signal->header.data.DataSize);
		Signal->header.data.DataSize = Signal->samplesPosFLAC;
	}

	if(!FillRIFFHeader(&Signal->header))
		return 0;
	
	if(Signal->errorFLAC)
		return 0;
	return ok ? 1 : 0;
}

FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
	AudioSignal *Signal = (AudioSignal*)client_data;
	long int pos = 0;
	size_t i;

	(void)decoder;

	if(!Signal) {
		logmsg("ERROR: Got empty Signal structure for FLAC decoding\n");
		flacInternalMDFErrors = 1;
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	if(Signal->header.fmt.NumOfChan != frame->header.channels) {
		logmsg("ERROR: FLAC Channel definition discrepancy %d vs %d\n", Signal->header.fmt.NumOfChan, frame->header.channels);
		flacInternalMDFErrors = 1;
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
	if(buffer[0] == NULL) {
		logmsg("ERROR: FLAC buffer[0] is NULL\n");
		flacInternalMDFErrors = 1;
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
	if(Signal->header.fmt.NumOfChan == 2 && buffer[1] == NULL) {
		logmsg("ERROR: FLAC buffer[1] is NULL\n");
		flacInternalMDFErrors = 1;
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	/* write header data before we write the first frame */
	if(frame->header.number.sample_number == 0) 
	{
		if(Signal->header.data.DataSize == 0) {
			logmsg("ERROR: MDFourier only works for FLAC files that have total_samples count in STREAMINFO\n");
			flacInternalMDFErrors = 1;
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
		if(Signal->header.fmt.bitsPerSample != 16 && Signal->header.fmt.bitsPerSample != 24) {
			logmsg("ERROR: Only 16/24 bit flac supported.\n\tPlease convert file to 16/24 bit flac.\n");
			flacInternalMDFErrors = 1;
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
		if(Signal->header.fmt.NumOfChan != 2 && Signal->header.fmt.NumOfChan != 1) {
			logmsg("ERROR: Only Mono and Stereo files are supported.\n");
			flacInternalMDFErrors = 1;
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
		Signal->Samples = (double*)malloc(sizeof(double)*Signal->numSamples*Signal->header.fmt.NumOfChan);
		if(!Signal->Samples)
		{
			logmsg("\tERROR: FLAC data chunks malloc failed!\n");
			flacInternalMDFErrors = 1;
			return(FLAC__STREAM_DECODER_WRITE_STATUS_ABORT);
		}
		memset(Signal->Samples, 0, sizeof(double)*Signal->numSamples*Signal->header.fmt.NumOfChan);
	}

	/* save decoded PCM samples */
	pos = Signal->samplesPosFLAC;
	for(i = 0; i < frame->header.blocksize; i++)
	{
		Signal->Samples[pos++] = (double)(FLAC__int32)buffer[0][i];
		if(Signal->header.fmt.NumOfChan == 2)
			Signal->Samples[pos++] = (double)(FLAC__int32)buffer[1][i];
	}
	Signal->samplesPosFLAC = pos;

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	AudioSignal *Signal = (AudioSignal*)client_data;

	(void)decoder;

	if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
	{
		uint64_t total_samples = 0;
		unsigned sample_rate = 0;
		unsigned channels = 0;
		unsigned bps = 0;

		total_samples = metadata->data.stream_info.total_samples;
		sample_rate = metadata->data.stream_info.sample_rate;
		channels = metadata->data.stream_info.channels;
		bps = metadata->data.stream_info.bits_per_sample;

		Signal->header.fmt.AudioFormat = WAVE_FORMAT_PCM;
		Signal->header.fmt.NumOfChan = channels;
		Signal->header.fmt.SamplesPerSec = sample_rate;
		Signal->header.fmt.bytesPerSec = sample_rate * channels * (bps/8);
		Signal->header.fmt.blockAlign = (FLAC__uint16)(channels * (bps/8));
		Signal->header.fmt.bitsPerSample = bps;
		Signal->header.data.DataSize = (FLAC__uint32)(total_samples * channels * (bps/8));
		Signal->numSamples = total_samples*channels;
		Signal->bytesPerSample = bps/8;
		Signal->SamplesStart = 0;
		Signal->samplesPosFLAC = 0;
	}
}

void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	AudioSignal *Signal = (AudioSignal*)client_data;

	(void)decoder;

	logmsgFileOnly("Got error while decoding FLAC: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
	if(Signal)
		Signal->errorFLAC ++;
}
