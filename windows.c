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
 */

#include "mdfourier.h"
#include "windows.h"
#include "log.h"
#include "freq.h"

int existsInArray(long int element, double *array, int count)
{
	if(!array)
		return 0;

	for(int i = 0; i < count; i++)
	{
		if(element == array[i])
			return 1;
	}

	return 0;
}

double *getWindowByLength(windowManager *wm, long int frames)
{
	if(!wm)
		return 0;

	for(int i = 0; i < wm->windowCount; i++)
	{
		if(frames == wm->windowArray[i].frames)
			return wm->windowArray[i].window;
	}

	return NULL;
}

double *getWindowByLengthForInternalSync(windowManager *wm, long int frames)
{
	return(getWindowByLength(wm, frames*-1));
}

long int getWindowSizeByLength(windowManager *wm, long int frames)
{
	if(!wm)
		return 0;

	for(int i = 0; i < wm->windowCount; i++)
	{
		if(frames == wm->windowArray[i].frames)
			return wm->windowArray[i].size;
	}

	return 0;
}

int initWindows(windowManager *wm, double framerate, int SamplesPerSec, char winType, parameters *config)
{
	double 	lengths[1024];	// yes, we are lazy
	int 	count = 0, flip = 1, insideInternal = 0;

	if(!wm || !config)
		return 0;
	
	if(winType == 'n')
	{
		wm->windowArray = NULL;
		wm->windowCount = 0;
		return 1;
	}

	// Count how many differen window sizes are needed
	for(int i = 0; i < config->types.typeCount; i++)
	{
		// Check if we need to use the default frame rate
		if(insideInternal)
			flip = -1;
		else
			flip = 1;

		if(!existsInArray(config->types.typeArray[i].frames*flip, lengths, count))
			lengths[count++] = config->types.typeArray[i].frames*flip;

		if(config->types.typeArray[i].type == TYPE_INTERNAL)
		{
			if(insideInternal)
				insideInternal = 0;
			else
				insideInternal = 1;
		}
	}

	if(!count)
	{
		logmsg("There are no blocks with valid lengths\n");
		return 0;
	}

	// Create the wm
	wm->windowArray = (windowUnit*)malloc(sizeof(windowUnit)*count);
	wm->windowCount = count;

	for(int i = 0; i < count; i++)
	{
		double seconds = 0;

		wm->windowArray[i].window = NULL;
		wm->windowArray[i].frames = lengths[i];
		wm->windowArray[i].size = 0;

		// Adjust in case they have different frame rates
		if(config->smallerFramerate != 0 && config->smallerFramerate < framerate)
			framerate = config->smallerFramerate;

		if(lengths[i] < 0) // TYPE_INTERNAL
			framerate = config->referenceFramerate;

		seconds = FramesToSeconds(fabs(wm->windowArray[i].frames), framerate);
		if(winType == 't')
		{
			wm->windowArray[i].window = tukeyWindow(SamplesPerSec*seconds);
			if(!wm->windowArray[i].window)
			{
				logmsg ("Tukey window creation failed\n");
				return(0);
			}
			wm->windowArray[i].size = SamplesPerSec*seconds;
		}

		if(winType == 'f')
		{
			wm->windowArray[i].window = flattopWindow(SamplesPerSec*seconds);
			if(!wm->windowArray[i].window)
			{
				logmsg ("FlatTop window creation failed\n");
				return(0);
			}
			wm->windowArray[i].size = SamplesPerSec*seconds;
		}

		if(winType == 'h')
		{
			wm->windowArray[i].window = hannWindow(SamplesPerSec*seconds);
			if(!wm->windowArray[i].window)
			{
				logmsg ("Hann window creation failed\n");
				return(0);
			}
			wm->windowArray[i].size = SamplesPerSec*seconds;
		}

		if(winType == 'm')
		{
			wm->windowArray[i].window = hammingWindow(SamplesPerSec*seconds);
			if(!wm->windowArray[i].window)
			{
				logmsg ("Hamming window creation failed\n");
				return(0);
			}
			wm->windowArray[i].size = SamplesPerSec*seconds;
		}
	}

	return 1;
}


void freeWindows(windowManager *wm)
{
	if(!wm)
		return;

	for(int i = 0; i < wm->windowCount; i++)
	{
		if(wm->windowArray[i].window)
			free(wm->windowArray[i].window);
	}
	if(wm->windowCount)
		free(wm->windowArray);
	wm->windowArray = NULL;
	wm->windowCount = 0;
}


// reduce scalloping loss 
double *flattopWindow(int n)
{
	int half, i, idx;
	double *w;
 
	w = (double*) calloc(n, sizeof(double));
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


// Only attenuate the edges to reduce errors
double *tukeyWindow(int n)
{
	int i;
	double *w, M = 0, alpha = 0;
 
	w = (double*) calloc(n, sizeof(double));
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

double *hannWindow(int n)
{
	int half, i, idx;
	double *w;
 
	w = (double*) calloc(n, sizeof(double));
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

double *hammingWindow(int n)
{
	int half, i, idx;
	double *w;
 
	w = (double*) calloc(n, sizeof(double));
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
