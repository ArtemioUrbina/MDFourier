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
 * Compile with: 
 *	  gcc -Wall -std=gnu99 -o mdfourier mdfourier.c -lfftw3 -lm
 */

#include "mdfourier.h"

#ifndef MDFOURIER_CMD_H
#define MDFOURIER_CMD_H

typedef struct parameters_st {
	char 		referenceFile[1024];
	char 		targetFile[1024];
	double		tolerance;
	double		HzWidth;
	double		HzDiff;
	int 		startHz, endHz;
	int 		showAll;
	int 		extendedResults;
	int 		justResults;
	int 		verbose;
	char 		window;
	char		channel;
	int 		MaxFreq;
	int 		clock;
	int 		clockNote;
	int 		debugVerbose;
	int 		spreadsheet;	
	char 		normalize;
	double		relativeMaxMagnitude;
	int			ignoreFloor;
	int			useOutputFilter;
	int			outputFilterFunction;
} parameters;

void CleanParameters(parameters *config);
int commandline(int argc , char *argv[], parameters *config);
char *GetRange(int index);
int GetSubIndex(int index);
char *GetChannel(char c);
char *GetWindow(char c);
void Header(int log);
void PrintUsage();
double TimeSpecToSeconds(struct timespec* ts);

#endif
