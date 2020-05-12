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

/*
 * This implements libFLAC to decode a FLAC file to a data structure in RAM
 * It only supports 16-bit stereo files.
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
#include "FLAC/stream_decoder.h"

#include <ctype.h>

extern char *getFilenameExtension(char *filename);
extern int getExtensionLength(char *filename);
static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
static void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
static void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);

char *strtoupper(char *str)
{
	unsigned char *p = (unsigned char *)str;
	
	while (*p) {
		*p = toupper((unsigned char)*p);
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

int FLACtoSignal(char *input, AudioSignal *Signal, parameters *config)
{
	FLAC__bool ok = true;
	FLAC__StreamDecoder *decoder = 0;
	FLAC__StreamDecoderInitStatus init_status;

	if(!Signal) {
		logmsg("ERROR: opening empty Data Sturcture\n");
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
			logmsg("ERROR: (FLAC) %s\n", FLAC__StreamDecoderStateString[FLAC__stream_decoder_get_state(decoder)]);
			Signal->errorFLAC++;
		}
	}

	FLAC__stream_decoder_delete(decoder);

	if(Signal->header.data.DataSize != Signal->samplesPosFLAC)
	{
		if(Signal->samplesPosFLAC > Signal->header.data.DataSize)  // Buffer overflow!!!
		{
			logmsg("ERROR: FLAC decoder made a buffer overflow\n Got%ld bytes and expected %ld bytes\n",
				Signal->samplesPosFLAC, Signal->header.data.DataSize);
			return 0;
		}
		//if(config->verbose)
		logmsg(" - WARNING: FLAC decoder got %ld bytes and expected %ld bytes (fixed internally)\n",
			Signal->samplesPosFLAC, Signal->header.data.DataSize);
		Signal->header.data.DataSize = Signal->samplesPosFLAC;
	}

	// Fill header data for MDWave/SaveWAVEChunk

	// riff
	memcpy(Signal->header.riff.RIFF, "RIFF", sizeof(char)*4);
	Signal->header.riff.ChunkSize = Signal->header.data.DataSize+36;
	memcpy(Signal->header.riff.WAVE, "WAVE", sizeof(char)*4);

	// fmt
	memcpy(Signal->header.fmt.fmt, "fmt ", sizeof(char)*4);
	Signal->header.fmt.Subchunk1Size = 16;

	// data
	memcpy(Signal->header.data.DataID, "data", sizeof(char)*4);

	if(Signal->errorFLAC)
		return 0;
	return ok ? 1 : 0;
}

FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
	AudioSignal *Signal = (AudioSignal*)client_data;
	int16_t *samples16 = NULL;
	long int pos = 0;
	size_t i;

	(void)decoder;

	if(!Signal) {
		logmsg("ERROR: Got empty SIgnal structure for FLAC decoding\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	if(Signal->header.fmt.NumOfChan != frame->header.channels) {
		logmsg("ERROR: FLAC Channel definition discrepancy %d vs %d\n", Signal->header.fmt.NumOfChan, frame->header.channels);
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
	if(buffer[0] == NULL) {
		logmsg("ERROR: FLAC buffer[0] is NULL\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
	if(Signal->header.fmt.NumOfChan == 2 && buffer[1] == NULL) {
		logmsg("ERROR: FLAC buffer[1] is NULL\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	/* write header data before we write the first frame */
	if(frame->header.number.sample_number == 0) 
	{
		if(Signal->header.data.DataSize == 0) {
			logmsg("ERROR: MDFourier only works for FLAC files that have total_samples count in STREAMINFO\n");
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
		if(Signal->header.fmt.bitsPerSample != 16) {
			logmsg("ERROR: Please use a 16bit stream\n");
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
		if(Signal->header.fmt.NumOfChan != 2 && Signal->header.fmt.NumOfChan != 1) {
			logmsg("ERROR: Please use a mono or stereo stream\n");
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
		Signal->Samples = (char*)malloc(sizeof(char)*Signal->header.data.DataSize);
		if(!Signal->Samples)
		{
			logmsg("\tERROR: FLAC data chunks malloc failed!\n");
			return(FLAC__STREAM_DECODER_WRITE_STATUS_ABORT);
		}
		memset(Signal->Samples, 0, sizeof(char)*Signal->header.data.DataSize);
	}

	/* save decoded PCM samples */
	samples16 = (int16_t*)(Signal->Samples + Signal->samplesPosFLAC);
	for(i = 0; i < frame->header.blocksize; i++)
	{
		samples16[pos++] = (FLAC__int16)buffer[0][i];
		if(Signal->header.fmt.NumOfChan == 2)
			samples16[pos++] = (FLAC__int16)buffer[1][i];
	}
	Signal->samplesPosFLAC += pos*2;

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	AudioSignal *Signal = (AudioSignal*)client_data;

	(void)decoder;

	if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
	{
		long int total_samples = 0;
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
