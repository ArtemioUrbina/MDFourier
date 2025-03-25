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

#include "mdfourier.h"
#include "windows.h"
#include "log.h"
#include "freq.h"

#define MAX_WINDOWS	100

int initWindows(windowManager *wm, double SampleRate, char winType, parameters *config)
{
	if(!wm || !config)
		return 0;

	wm->MaxWindow = 0;
	wm->SampleRate = 0;
	wm->winType = 'n';
	
	// Create the wm
	wm->windowArray = (windowUnit*)malloc(sizeof(windowUnit)*MAX_WINDOWS);
	if(!wm->windowArray)
	{
		logmsg("Not enough memory for window manager\n");
		return 0;
	}
	wm->windowCount = 0;
	wm->MaxWindow = MAX_WINDOWS-1;
	wm->SampleRate = SampleRate;
	wm->winType = winType;

	memset(wm->windowArray, 0, sizeof(windowUnit)*MAX_WINDOWS);

	return 1;
}

double *CreateWindowInternal(windowManager *wm, double *(*creator)(long), char *name, double seconds, long size, long sizePadding, long clkAdjustBufferSize, double frames)
{
	double *window = NULL;

	window = creator(size);
	if(!window)
	{
		logmsg ("%s window creation failed\n", name);
		return NULL;
	}
	// Size difference at the end, for difference between frame rates
	if(sizePadding)
	{
		double *tmp = NULL;

		tmp = (double*)realloc(window, sizeof(double)*(size+sizePadding+clkAdjustBufferSize));
		if(!tmp)
		{
			free(window);
			logmsg ("%s window creation failed, padding\n", name);
			return NULL;
		}
		window = tmp;
		memset(window+size, 0, sizeof(double)*(sizePadding+clkAdjustBufferSize));
	}

	wm->windowArray[wm->windowCount].sizePadding = sizePadding;
	wm->windowArray[wm->windowCount].window = window;
	wm->windowArray[wm->windowCount].seconds = seconds;
	wm->windowArray[wm->windowCount].size = size;
	wm->windowArray[wm->windowCount].frames = frames;
	wm->windowCount++;
	return window;
}

double *CreateWindow(windowManager *wm, long int frames, long int cutFrames, double framerate, parameters *config)
{
	double		seconds = 0;
	long int	size = 0;
	double		secondsPadding = 0, oneFramePadding = 0;
	long int	sizePadding = 0, clkAdjustBufferSize = 0;

	if(!wm)
		return NULL;

	if(cutFrames >= frames)
	{
		logmsg("ERROR: Cutframes %ld >= frames %ld\n", cutFrames, frames);
		return NULL;
	}

	if(wm->windowCount == wm->MaxWindow)
	{
		logmsg("ERROR: Reached Max window limit %d\n", wm->MaxWindow);
		return NULL;
	}

	seconds = FramesToSeconds(frames-cutFrames, framerate);
	size = ceil(wm->SampleRate*seconds);

	secondsPadding = FramesToSeconds(cutFrames, framerate);
	sizePadding = ceil(wm->SampleRate*secondsPadding);

	/* Used for clk adjust, eventhough one frame is overkill */
	if(config->doClkAdjust)
	{
		oneFramePadding = FramesToSeconds(1, framerate);
		clkAdjustBufferSize = ceil(wm->SampleRate*oneFramePadding);
	}

	if(!size)
	{
		logmsg("ERROR: Asked for window with null size %ld Frames %g Framerate\n", frames, framerate);
		return NULL;
	}

#ifdef DEBUG
	if(config->verbose >= 2) {
		if(!config->doClkAdjust)
			logmsg("**** Creating window size %ld+%ld=%ld (%ld frames %g fr)\n", size, sizePadding, size+sizePadding, frames, framerate);
		else
			logmsg("**** Creating window size %ld+%ld(+%ld)=%ld(%ld) (%ld frames %g fr) clkAdjustBufferSize: $ld\n", size, sizePadding, clkAdjustBufferSize, size+sizePadding, size+sizePadding+clkAdjustBufferSize, frames, framerate, clkAdjustBufferSize);
	}
#endif
	
	if(wm->winType == 'n')
		return(CreateWindowInternal(wm, rectWindow, "Rectangle", seconds, size, sizePadding, clkAdjustBufferSize, frames));

	if(wm->winType == 't')
		return(CreateWindowInternal(wm, tukeyWindow, "Tukey", seconds, size, sizePadding, clkAdjustBufferSize, frames));

	if(wm->winType == 'f')
		return(CreateWindowInternal(wm, flattopWindow, "Flattop", seconds, size, sizePadding, clkAdjustBufferSize, frames));

	if(wm->winType == 'h')
		return(CreateWindowInternal(wm, hannWindow, "Hann", seconds, size, sizePadding, clkAdjustBufferSize, frames));

	if(wm->winType == 'm')
		return(CreateWindowInternal(wm, hammingWindow, "Hamming", seconds, size, sizePadding, clkAdjustBufferSize, frames));

	logmsg("FAILED Creating window size %g (%ld frames %g fr)\n", frames*framerate, frames, framerate);
	return NULL;
}

double *getWindowByLength(windowManager *wm, long int frames, long int cutFrames, double framerate, parameters *config)
{
	double		seconds = 0;
	long int	size = 0;
	double		secondsPadding = 0;
	long int	sizePadding = 0;

	if(!wm)
		return 0;

	seconds = FramesToSeconds(frames-cutFrames, framerate);
	size = ceil(wm->SampleRate*seconds);

	secondsPadding = FramesToSeconds(cutFrames, framerate);
	sizePadding = ceil(wm->SampleRate*secondsPadding);

#ifdef DEBUG
	if(config->verbose >= 3)
		logmsg("Asked for window %ld zero:%ld (%ld frames %ld cut frames %g fr)\n", size, sizePadding, frames, cutFrames, framerate);
#endif

	for(int i = 0; i < wm->windowCount; i++)
	{
		if(size == wm->windowArray[i].size && sizePadding == wm->windowArray[i].sizePadding)
		{
#ifdef DEBUG
			if(config->verbose >= 2)
				logmsg("Served window size %ld zero:%ld (%ld frames %ld cut frames %g fr)\n", size, sizePadding, frames, cutFrames, framerate);
#endif
			
			return wm->windowArray[i].window;
		}
	}

#ifdef DEBUG
	if(config->verbose >= 2)
		logmsg("Creating window %ld zero:%ld (%ld frames %ld cut frames %g fr)\n", size, sizePadding, frames, cutFrames, framerate);
#endif
	return CreateWindow(wm, frames, cutFrames, framerate, config);
}

void freeWindows(windowManager *wm)
{
	if(!wm)
		return;

	for(int i = 0; i < wm->windowCount; i++)
	{
		if(wm->windowArray[i].window)
		{
			free(wm->windowArray[i].window);
			wm->windowArray[i].window = NULL;
		}
	}
	if(wm->windowCount)
	{
		free(wm->windowArray);
		wm->windowArray = NULL;
		wm->windowCount = 0;
	}

	wm->MaxWindow = 0;
	wm->SampleRate = 0;
	wm->winType = 'n';
}

// reduce scalloping loss 
double *flattopWindow(long int n)
{
	int half, i, idx;
	double *w;
 
	w = (double*) calloc(n, sizeof(double));
	if(!w)
	{
		logmsg("Not enough memory for window\n");
		return NULL;
	}
	memset(w, 0, n*sizeof(double));
 
	if(n%2==0)
	{
		half = n/2;
		for(i=0; i<half; i++)
		{
			double factor = 2*M_PI*i/(n-1);
			w[i] = 0.21557895 - 0.41663158*cos(factor) + 0.277263158*cos(2*factor) 
				  -0.083578947*cos(3*factor) + 0.006947368*cos(4*factor);
		}
 
		idx = half-1;
		for(i=half; i<n; i++) {
			w[i] = w[idx];
			idx--;
		}
	}
	else
	{
		half = (n+1)/2;
		for(i=0; i<half; i++)
		{
			double factor = 2*M_PI*i/(n-1);
			w[i] = 0.21557895 - 0.41663158*cos(factor) + 0.277263158*cos(2*factor) 
				  -0.083578947*cos(3*factor) + 0.006947368*cos(4*factor);
		}
 
		idx = half-2;
		for(i=half; i<n; i++) {
			w[i] = w[idx];
			idx--;
		}
	}
 
	return(w);
}

// We need to create it because it is used as a mask sometimes
double *rectWindow(long int n)
{
	long int i;
	double *w;
 
	w = (double*) calloc(n, sizeof(double));
	if(!w)
	{
		logmsg("Not enough memory for window\n");
		return NULL;
	}
	memset(w, 0, n*sizeof(double));
 
	for(i=0; i<n; i++)
		w[i] = 1;
 
	return(w);
}

// Only attenuate the edges to reduce errors
double *tukeyWindow(long int n)
{
	long int i;
	double *w, M = 0, alpha = 0;
 
	w = (double*) calloc(n, sizeof(double));
	if(!w)
	{
		logmsg("Not enough memory for window\n");
		return NULL;
	}
	memset(w, 0, n*sizeof(double));
 
	alpha = 0.65;
	M = (n-1)/2;

	for(i=0; i<n; i++)
	{
		if(fabs(i-M) >= alpha*M)
			w[i] = 0.5*(1+cos(M_PI*(fabs(i-M)-alpha*M)/((1-alpha)*M)));
		else
			w[i] = 1;
	}
 
	return(w);
}

double *hannWindow(long int n)
{
	long int half, i, idx;
	double *w;
 
	w = (double*) calloc(n, sizeof(double));
	if(!w)
	{
		logmsg("Not enough memory for window\n");
		return NULL;
	}
	memset(w, 0, n*sizeof(double));

	if(n%2==0)
	{
		half = n/2;
		for(i=0; i<half; i++) //CALC_HANNING   Calculates Hanning window samples.
			w[i] = 0.5 * (1 - cos(2*M_PI*(i+1) / (n+1)));
 
		idx = half-1;
		for(i=half; i<n; i++) {
			w[i] = w[idx];
			idx--;
		}
	}
	else
	{
		half = (n+1)/2;
		for(i=0; i<half; i++) //CALC_HANNING   Calculates Hanning window samples.
			w[i] = 0.5 * (1 - cos(2*M_PI*(i+1) / (n+1)));
 
		idx = half-2;
		for(i=half; i<n; i++) {
			w[i] = w[idx];
			idx--;
		}
	}
 
	return(w);
}

double *hammingWindow(long int n)
{
	long int half, i, idx;
	double *w;
 
	w = (double*) calloc(n, sizeof(double));
	if(!w)
	{
		logmsg("Not enough memory for window\n");
		return NULL;
	}
	memset(w, 0, n*sizeof(double));

	if(n%2==0)
	{
		half = n/2;
		for(i=0; i<half; i++)
			w[i] = 0.54 -0.46*cos((2*M_PI*i)/(n-1));
 
		idx = half-1;
		for(i=half; i<n; i++) {
			w[i] = w[idx];
			idx--;
		}
	}
	else
	{
		half = (n+1)/2;
		for(i=0; i<half; i++)
			w[i] = 0.54 -0.46*cos((2*M_PI*i)/(n-1));
 
		idx = half-2;
		for(i=half; i<n; i++) {
			w[i] = w[idx];
			idx--;
		}
	}
 
	return(w);
}

double CalculateCorrectionFactor(windowManager *wm, long int frames)
{
	double		*window = NULL;
	double		factor = 0, sum = 0;
	long int	size = 0;

	if(!wm)
		return 1;

	for(int i = 0; i < wm->windowCount; i++)
	{
		if(frames == wm->windowArray[i].frames)
		{
			window = wm->windowArray[i].window;
			size = wm->windowArray[i].size;
			break;
		}
	}

	if(!window)
		return 1;

	for(long int i = 0; i < size; i++)
		sum += window[i];
	
	factor = (double)size/sum;

	return factor;
}

double CompensateValueForWindow(double value, char winType)
{
	switch(winType)
	{
		case 'n':
			break;
		case 't':
			value *= 1.2122;
			break;
		case 'f':
			value *= 4.63899;
			break;
		case 'h':
			value *= 1.99986;
			break;
		case 'm':
			value *= 1.85196;
			break;
	}

	return value;
}
