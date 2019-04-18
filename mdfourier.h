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

#ifndef MDFOURIER_H
#define MDFOURIER_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>

#include <string.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>
#include <libgen.h>

#include "incbeta.h"

#define MDVERSION "0.75"

#define PSG_COUNT	40
#define NOISE_COUNT 100

#define MAX_FREQ_COUNT		22050 	// Number of frequencies to account for (MAX) 
#define FREQ_COUNT			2000	// Number of frequencies to account for (default)
#define MAX_NOTES			140		// we have 140 notes in the 240p Test Suite

#define TYPE_NONE	0
#define TYPE_FM 	1
#define TYPE_PSG	2
#define TYPE_NOISE	3

// This is the difference allowed between reference and compared
// amplitudes to match, in dbs
#define DBS_TOLERANCE	3.0

// Width of each peak
#define HERTZ_WIDTH			0.0

// +/- Tolerance in frequency Difference to be the same one
#define HERTZ_DIFF			0.0

//Percentage of normalized magnitude frequencies to match
#define FREQ_COMPARE		1.0

#define START_HZ	10
#define END_HZ		MAX_FREQ_COUNT

#endif
