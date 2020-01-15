/* 
 * MDFourier
 * A Fourier Transform analysis tool to compare different 
 * Sega Genesis/Mega Drive audio hardware revisions.
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

#include "log.h"
#include "mdfourier.h"
#include "freq.h"
#include "cline.h"

#ifndef MAX_PATH
#ifdef __MINGW32__
//MAX_PATH
#include "minwindef.h"
#endif
#endif

int do_log = 0;
char log_file[T_BUFFER_SIZE];
FILE *logfile = NULL;

#define	CONSOLE_ENABLED		1
#define	CONSOLE_DISABLED	2

void EnableLog() { do_log = CONSOLE_ENABLED; }
void DisableConsole() { do_log = CONSOLE_DISABLED; }
void EnableConsole() { do_log = CONSOLE_ENABLED; }
int IsLogEnabled() { return do_log; }
void OutputFileOnlyStart() { if(IsLogEnabled()) DisableConsole();}
void OutputFileOnlyEnd() { if(IsLogEnabled()) EnableConsole();}

void logmsg(char *fmt, ... )
{
	va_list arguments;

	va_start(arguments, fmt);
	if(do_log != CONSOLE_DISABLED)
	{
		vprintf(fmt, arguments);
#if defined (WIN32)
		fflush(stdout);  // output to Front end ASAP
#endif
	}

	if(do_log && logfile)
	{
		vfprintf(logfile, fmt, arguments);
		// uncomment to output to log file ASAP when debugging
		//fflush(logfile);
	}

	va_end(arguments);
	return;
}

void logmsgFileOnly(char *fmt, ... )
{
	va_list arguments;

	va_start(arguments, fmt);
	OutputFileOnlyStart();

	if(do_log != CONSOLE_DISABLED)
	{
		vprintf(fmt, arguments);
#if defined (WIN32)
		fflush(stdout);  // output to Front end ASAP
#endif
	}

	if(do_log && logfile)
	{
		vfprintf(logfile, fmt, arguments);
		// uncomment to output to log file ASAP when debugging
		//fflush(logfile);
	}

	OutputFileOnlyEnd();
	va_end(arguments);
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

void endLog()
{
	if(do_log && logfile)
	{
		fclose(logfile);
		logfile = NULL;
	}
}

int SaveWAVEChunk(char *filename, AudioSignal *Signal, char *buffer, long int block, long int loadedBlockSize, int diff, parameters *config)
{
	FILE 		*chunk = NULL;
	wav_hdr		cheader;
	char 		FName[4096];

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
		logmsg("\tCould not open chunk file %s\n", filename);
		return 0;
	}

	cheader.riff.ChunkSize = loadedBlockSize+36;
	cheader.data.DataSize = loadedBlockSize;
	if(fwrite(&cheader, 1, sizeof(wav_hdr), chunk) != sizeof(wav_hdr))
	{
		fclose(chunk);
		logmsg("\tCould not write chunk header to file %s\n", filename);
		return(0);
	}

	if(fwrite(buffer, 1, sizeof(char)*loadedBlockSize, chunk) != sizeof(char)*loadedBlockSize)
	{
		fclose(chunk);
		logmsg("\tCould not write samples to chunk file %s\n", filename);
		return (0);
	}

	fclose(chunk);
	return 1;
}