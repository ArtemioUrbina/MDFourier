/* 
 * MDFourier
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

#ifndef MDFOURIER_CMD_H
#define MDFOURIER_CMD_H

#include "mdfourier.h"

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
void Header(int log);
void PrintUsage();
double TimeSpecToSeconds(struct timespec* ts);
char *getFilenameExtension(char *filename);
int getExtensionLength(char *filename);
void ShortenFileName(char *filename, char *copy);

#endif
