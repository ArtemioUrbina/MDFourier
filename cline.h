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

#ifndef MDFOURIER_CMD_H
#define MDFOURIER_CMD_H

#include "mdfourier.h"

#if defined (WIN32)
	#include <direct.h>
	#define GetCurrentDir _getcwd
#else
	#include <unistd.h>
	#define GetCurrentDir getcwd
#endif

int SetupFolders(char *folder, char *logname, parameters *config);
int CreateFolder(char *name);
int CreateFolderName(char *mainfolder, parameters *config);
void InvertComparedName(parameters *config);
void ComposeFileName(char *target, char *subname, char *ext, parameters *config);
void ComposeFileNameoPath(char *target, char *subname, char *ext, parameters *config);
void CleanParameters(parameters *config);
int commandline(int argc , char *argv[], parameters *config);
char *GetChannel(char c);
char *GetWindow(char c);
int Header(int log, int argc, char *argv[]);
void PrintUsage(void);
double TimeSpecToSeconds(struct timespec* ts);
char *getFilenameExtension(char *filename);
int getExtensionLength(char *filename);
void ShortenFileName(char *filename, char *copy, int maxlen);
int CleanFolderName(char *name, char *origName);

char *PushMainPath(parameters *config);
void PopMainPath(char **CurrentPath);

#endif

// clock_gettime is not implemented on older versions of OS X (< 10.12).
// If implemented, CLOCK_MONOTONIC will have already been defined.
#ifndef CLOCK_MONOTONIC
#include <sys/time.h>
#define CLOCK_MONOTONIC 0
#define USE_GETTIME_INSTEAD
int clock_gettime(int clk_id, struct timespec* t);
#endif
