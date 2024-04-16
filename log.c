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

#include "log.h"
#include "mdfourier.h"
#include "freq.h"
#include "cline.h"

#define	CONSOLE_ENABLED		1

int do_log = 0;
char log_file[T_BUFFER_SIZE];
FILE *logfile = NULL;

void EnableLog(void) { do_log = CONSOLE_ENABLED; }
void DisableLog(void) { do_log = 0; }
int IsLogEnabled(void) { return do_log; }

void initLog(void)
{
	do_log = 0;
	logfile = NULL;
}

void logmsg(char *fmt, ... )
{
	va_list arguments;

	va_start(arguments, fmt);
	vprintf(fmt, arguments);
	fflush(stdout);  // output to Front end ASAP
	va_end(arguments);

	if(do_log && logfile)
	{
		va_list arguments_f;

		va_start(arguments_f, fmt);
		vfprintf(logfile, fmt, arguments_f);
		va_end(arguments_f);
#ifdef DEBUG
		fflush(logfile);
#endif
	}
}

void logmsgFileOnly(char *fmt, ... )
{
	if(do_log && logfile)
	{
		va_list arguments;

		va_start(arguments, fmt);
		vfprintf(logfile, fmt, arguments);
		va_end(arguments);
#ifdef DEBUG
		fflush(logfile);
#endif
	}
}

#if defined (WIN32)
void FixLogFileName(char *name)
{
	int len;

	if(!name)
		return;
	len = strlen(name);

	if(len > MAX_PATH)
	{
		name[MAX_PATH - 5] = '.';
		name[MAX_PATH - 4] = 't';
		name[MAX_PATH - 3] = 'x';
		name[MAX_PATH - 2] = 't';
		name[MAX_PATH - 1] = '\0';
	}
}
#endif

int setLogName(char *name)
{
	sprintf(log_file, "%s", name);

	if(!do_log)
		return 0;

	remove(log_file);

#if defined (WIN32)
	FixLogFileName(log_file);
#endif

	logfile = fopen(log_file, "w");
	if(!logfile)
	{
		printf("Could not create log file %s\n", log_file);
		return 0;
	}

	//printf("\tLog enabled to file: %s\n", log_file);
	return 1;
}

void endLog(void)
{
	if(logfile)
	{
		fclose(logfile);
		logfile = NULL;
	}
	do_log = 0;
}

// no endianess considerations, PCM in RIFF is little endian and this code is little endian
void ConvertPCMSampleToByteArray(double sample, char *bytes, int size)
{
	int32_t	sampleInt = 0;

	sampleInt = (int32_t)(sample + 0.5);
	
	bytes[0] = (char)((sampleInt & 0x000000ff));
	if(size >= 2)
		bytes[1] = (char)((sampleInt & 0x0000ff00) >> 8);
	if(size >= 3)
		bytes[2] = (char)((sampleInt & 0x00ff0000) >> 16);
	if(size == 4)
		bytes[3] = (char)((sampleInt & 0xff000000) >> 24);

	return;
}

int SaveWAVEChunk(char *filename, AudioSignal *Signal, double *buffer, long int block, long int loadedBlockSize, int diff, parameters *config)
{
	FILE 		*chunk = NULL;
	wav_hdr		cheader;
	char 		*samples = NULL, FName[4096];
	long int	i = 0;
	int			convertedSamples = 0;

	samples = (char*)malloc(sizeof(char)*loadedBlockSize*Signal->bytesPerSample);
	if(!samples)
	{
		logmsg("\tCould not allocate WAV memory\n");
		return 0;
	}
	memset(samples, 0, sizeof(char)*loadedBlockSize*Signal->bytesPerSample);

	cheader = Signal->header;
	if(!filename)
	{
		char Name[2048];

		sprintf(Name, "%03ld_SRC_%s_%03d_%s_%s", 
			block, GetBlockName(config, block), GetBlockSubIndex(config, block), 
			basename(Signal->SourceFile), diff ? "_diff_": "");
		ComposeFileName(FName, Name, ".wav", config);
		chunk = fopen(FName, "wb");
		filename = FName;
	}
	else
		chunk = fopen(filename, "wb");
	if(!chunk)
	{
		logmsg("\tERROR: Could not open chunk file %s\n", filename);
		free(samples);
		return 0;
	}

	if(Signal->header.fmt.AudioFormat == WAVE_FORMAT_PCM ||
		Signal->header.fmt.AudioFormat == WAVE_FORMAT_EXTENSIBLE)
	{
		for(i = 0; i < loadedBlockSize; i++)
			ConvertPCMSampleToByteArray(buffer[i], samples+i*Signal->bytesPerSample, Signal->bytesPerSample);
		convertedSamples = 1;
	}

	if(Signal->header.fmt.AudioFormat == WAVE_FORMAT_IEEE_FLOAT && Signal->header.fmt.bitsPerSample == 32)
	{
		float *samplesf = NULL;

		samplesf = (float*)samples;
		for(i = 0; i < loadedBlockSize; i++)
			samplesf[i] = (float)buffer[i];
		convertedSamples = 1;
	}

	if(Signal->header.fmt.AudioFormat == WAVE_FORMAT_IEEE_FLOAT && Signal->header.fmt.bitsPerSample == 64)
	{
		double *samplesd = NULL;

		samplesd = (double*)samples;
		for(i = 0; i < loadedBlockSize; i++)
			samplesd[i] = (double)buffer[i];
		convertedSamples = 1;
	}

	if(!convertedSamples)
	{
		logmsg("ERROR: Unsupported audio format, samples were not loaded\n");
		return 0;
	}

	cheader.riff.ChunkSize = sizeof(riff_hdr)+sizeof(fmt_hdr)+Signal->fmtType*sizeof(int8_t)+
				loadedBlockSize*Signal->bytesPerSample+Signal->factExists*sizeof(fact_ck);

	if(fwrite(&cheader.riff, 1, sizeof(riff_hdr), chunk) != sizeof(riff_hdr))
	{
		fclose(chunk);
		logmsg("\tERROR: Could not write RIFf header chunk to file %s\n", filename);
		free(samples);
		return(0);
	}

	if(fwrite(&cheader.fmt, 1, sizeof(fmt_hdr), chunk) != sizeof(fmt_hdr))
	{
		fclose(chunk);
		logmsg("\tERROR: Could not write fmt header chunk to file %s\n", filename);
		free(samples);
		return(0);
	}

	// Check extended fmt header
	if(Signal->fmtType != FMT_TYPE_1_SIZE)
	{
		if(fwrite(Signal->fmtExtra, 1, sizeof(int8_t)*Signal->fmtType, chunk) != sizeof(int8_t)*Signal->fmtType)
		{
			fclose(chunk);
			logmsg("\tERROR: Could not write fmt extended header chunk to file %s\n", filename);
			free(samples);
			return(0);
		}
	}

	cheader.data.DataSize = loadedBlockSize*Signal->bytesPerSample;
	if(fwrite(&cheader.data, 1, sizeof(data_hdr), chunk) != sizeof(data_hdr))
	{
		fclose(chunk);
		logmsg("\tERROR: Could not write data header chunk to file %s\n", filename);
		free(samples);
		return(0);
	}

	if(fwrite(samples, 1, sizeof(char)*loadedBlockSize*Signal->bytesPerSample, chunk) != sizeof(char)*loadedBlockSize*Signal->bytesPerSample)
	{
		fclose(chunk);
		logmsg("\tERROR: Could not write samples to chunk file %s\n", filename);
		free(samples);
		return (0);
	}

	if(Signal->header.fmt.AudioFormat == WAVE_FORMAT_EXTENSIBLE && !Signal->factExists)
	{
		if(config->verbose)
			logmsg("\tWARNING: Extensible wave requires a fact chunk. generating one.\n");
		memcpy(Signal->fact.DataID, "fact", sizeof(char)*4);
		Signal->fact.DataSize = 4;
		Signal->factExists = 1;
	}

	if(Signal->factExists)
	{
		Signal->fact.dwSampleLength = loadedBlockSize/Signal->AudioChannels;
		if(fwrite(&Signal->fact, 1, sizeof(fact_ck), chunk) != sizeof(fact_ck))
		{
			fclose(chunk);
			logmsg("\tERROR: Could not write fact header chunk to file %s\n", filename);
			free(samples);
			return(0);
		}
	}

	fclose(chunk);
	free(samples);
	return 1;
}
