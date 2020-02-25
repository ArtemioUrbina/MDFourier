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

#ifndef MDFOURIER_LOG_H
#define MDFOURIER_LOG_H

#include "mdfourier.h"

void EnableLog();
void DisableLog();
void DisableConsole();
void EnableConsole();
int IsLogEnabled();

void logmsg(char *fmt, ... );
void logmsgFileOnly(char *fmt, ... );

int setLogName(char *name);
void endLog();

int SaveWAVEChunk(char *filename, AudioSignal *Signal, char *buffer, long int block, long int loadedBlockSize, int diff, parameters *config);

#endif
