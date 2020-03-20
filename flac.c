/* example_c_decode_file - Simple FLAC file decoder using libFLAC
 * Copyright (C) 2007-2009	Josh Coalson
 * Copyright (C) 2011-2016	Xiph.Org Foundation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * This example shows how to use libFLAC to decode a FLAC file to a WAVE
 * file.  It only supports 16-bit stereo files.
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
#include "mdfourier.h"
#include "FLAC/stream_decoder.h"

#include <ctype.h>

extern char *getFilenameExtension(char *filename);
extern int getExtensionLength(char *filename);

int errorFLAC = 0;

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

void renameFLAC(char *flac, char *wav, char *path)
{
	int len = 0, ext = 0;

	if(!path || !strlen(path))
	{
		len = strlen(flac);
		ext = getExtensionLength(flac)+1;
	
		memcpy(wav, flac, sizeof(char)*(len-ext));
		sprintf(wav+len-ext, "__tmp_mdf__");
	
		len = strlen(wav);
		sprintf(wav+len, ".wav");
	}
	else
	{
		len = strlen(path);
		if(path[len-1] != FOLDERCHAR)
			sprintf(wav, "%s%ctmp__mdf__%s", path, FOLDERCHAR, basename(flac));
		else
			sprintf(wav, "%stmp__mdf__%s", path, basename(flac));
	
		len = strlen(wav);
		ext = getExtensionLength(basename(flac))+1;
		sprintf(wav+len-ext, ".wav");
	}
}

static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
static void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
static void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);

static FLAC__uint64 total_samples = 0;
static unsigned sample_rate = 0;
static unsigned channels = 0;
static unsigned bps = 0;

static FLAC__bool write_little_endian_uint16(FILE *f, FLAC__uint16 x)
{
	return
		fputc(x, f) != EOF &&
		fputc(x >> 8, f) != EOF
	;
}

static FLAC__bool write_little_endian_int16(FILE *f, FLAC__int16 x)
{
	return write_little_endian_uint16(f, (FLAC__uint16)x);
}

static FLAC__bool write_little_endian_uint32(FILE *f, FLAC__uint32 x)
{
	return
		fputc(x, f) != EOF &&
		fputc(x >> 8, f) != EOF &&
		fputc(x >> 16, f) != EOF &&
		fputc(x >> 24, f) != EOF
	;
}

int FLACtoWAV(char *input, char *output)
{
	FLAC__bool ok = true;
	FLAC__StreamDecoder *decoder = 0;
	FLAC__StreamDecoderInitStatus init_status;
	FILE *fout;

	if((fout = fopen(output, "wb")) == NULL) {
		logmsg("ERROR: opening %s for output\n", output);
		return 0;
	}

	if((decoder = FLAC__stream_decoder_new()) == NULL) {
		logmsg("ERROR: allocating decoder\n");
		fclose(fout);
		return 0;
	}

	(void)FLAC__stream_decoder_set_md5_checking(decoder, true);

	init_status = FLAC__stream_decoder_init_file(decoder, input, write_callback, metadata_callback, error_callback, /*client_data=*/fout);
	if(init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
		logmsg("ERROR: initializing decoder: %s\n", FLAC__StreamDecoderInitStatusString[init_status]);
		ok = false;
	}

	if(ok) {
		ok = FLAC__stream_decoder_process_until_end_of_stream(decoder);
		if(!ok)
		{
			logmsg("	state: %s\n", FLAC__StreamDecoderStateString[FLAC__stream_decoder_get_state(decoder)]);

			errorFLAC++;
		}
	}

	FLAC__stream_decoder_delete(decoder);
	fclose(fout);

	if(errorFLAC)
		return 0;
	return 1;
}

FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
	FILE *f = (FILE*)client_data;
	const FLAC__uint32 total_size = (FLAC__uint32)(total_samples * channels * (bps/8));
	size_t i;

	(void)decoder;

	if(total_samples == 0) {
		logmsg("ERROR: MDFourier only works for FLAC files that have total_samples count in STREAMINFO\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
	if(bps != 16) {
		logmsg("ERROR: Please use a 16bit stream\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
	if(channels != frame->header.channels) {
		logmsg("ERROR: Channel definition discrepancy %d vs %d\n", channels, frame->header.channels);
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
	if(channels != 2 && channels != 1) {
		logmsg("ERROR: Please use a mono or stereo stream\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
	if(buffer [0] == NULL) {
		logmsg("ERROR: buffer [0] is NULL\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
	if(channels == 2 && buffer [1] == NULL) {
		logmsg("ERROR: buffer [1] is NULL\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	/* write WAVE header before we write the first frame */
	if(frame->header.number.sample_number == 0) {
		if(
			fwrite("RIFF", 1, 4, f) < 4 ||
			!write_little_endian_uint32(f, total_size + 36) ||
			fwrite("WAVEfmt ", 1, 8, f) < 8 ||
			!write_little_endian_uint32(f, 16) ||
			!write_little_endian_uint16(f, 1) ||
			!write_little_endian_uint16(f, (FLAC__uint16)channels) ||
			!write_little_endian_uint32(f, sample_rate) ||
			!write_little_endian_uint32(f, sample_rate * channels * (bps/8)) ||
			!write_little_endian_uint16(f, (FLAC__uint16)(channels * (bps/8))) || /* block align */
			!write_little_endian_uint16(f, (FLAC__uint16)bps) ||
			fwrite("data", 1, 4, f) < 4 ||
			!write_little_endian_uint32(f, total_size)
		) {
			logmsg("ERROR: write error\n");
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
	}

	/* write decoded PCM samples */
	for(i = 0; i < frame->header.blocksize; i++) {
		if(
			!write_little_endian_int16(f, (FLAC__int16)buffer[0][i]) ||  /* left channel */
			(channels == 2 && !write_little_endian_int16(f, (FLAC__int16)buffer[1][i]))	 /* right channel */
		) {
			logmsg("ERROR: write error\n");
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	(void)decoder, (void)client_data;

	/* print some stats */
	if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
		/* save for later */
		total_samples = metadata->data.stream_info.total_samples;
		sample_rate = metadata->data.stream_info.sample_rate;
		channels = metadata->data.stream_info.channels;
		bps = metadata->data.stream_info.bits_per_sample;

		//logmsg("sample rate	  : %u Hz\n", sample_rate);
		//logmsg("channels 	  : %u\n", channels);
		//logmsg("bits per sample: %u\n", bps);
		//logmsg("total samples  : %" PRIu64 "\n", total_samples);
	}
}

void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	(void)decoder, (void)client_data;

	logmsg("Got error while decoding FLAC: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
	errorFLAC ++;
}
