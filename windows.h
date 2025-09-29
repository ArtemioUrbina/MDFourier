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

#ifndef MDFOURIER_WINDOWS_H
#define MDFOURIER_WINDOWS_H

#include "mdfourier.h"

double *hannWindow(long int n);
double *flattopWindow(long int n);
double *tukeyWindow(long int n);
double *hammingWindow(long int n);
double *rectWindow(long int n);

int initWindows(windowManager *wm, double SampleRate, char winType, parameters *config);
double *getWindowByLength(windowManager *wm, long int frames, long int cutFrames, double framerate, parameters *config);
double *CreateWindow(windowManager *wm, long int frames, long int cutFrames, double framerate, parameters *config);
void freeWindows(windowManager *windows);
double CompensateValueForWindow(double value, char winType);
double CalculateCorrectionFactor(windowManager *wm, long int frames);
void printWindows(windowManager *wm);

#endif
