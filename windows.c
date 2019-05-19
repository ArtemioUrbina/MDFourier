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

int initWindows(windowManager *wm, double framerate, int SamplesPerSec, parameters *config)
{
	double 	lengths[1024];	// yes, we are lazy
	int 	count = 0;

	if(!wm || !config)
		return 0;
	
	if(config->window == 'n')
	{
		wm->windowArray = NULL;
		wm->windowCount = 0;
		return 1;
	}

	// Count how many differen window sizes are needed
	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(!existsInArray(config->types.typeArray[i].frames, lengths, count))
			lengths[count++] = config->types.typeArray[i].frames;
	}

	if(!count)
	{
		logmsg("There are no blocks with valid lengths\n");
		return 0;
	}

	// Create the wm
	wm->windowArray = (windowUnit*)malloc(sizeof(windowUnit)*count);
	wm->windowCount = count;

	// Adjust in case they have different frame rates
	if(config->smallerFramerate != 0 && config->smallerFramerate < framerate)
		framerate = config->smallerFramerate;

	for(int i = 0; i < count; i++)
	{
		double seconds = 0;

		wm->windowArray[i].window = NULL;
		wm->windowArray[i].frames = lengths[i];
		wm->windowArray[i].size = 0;

		seconds = FramesToSeconds(wm->windowArray[i].frames, framerate);
		if(config->window == 't')
		{
			wm->windowArray[i].window = tukeyWindow(SamplesPerSec*seconds);
			if(!wm->windowArray[i].window)
			{
				logmsg ("Tukey window creation failed\n");
				return(0);
			}
			wm->windowArray[i].size = SamplesPerSec*seconds;
		}

		if(config->window == 'f')
		{
			wm->windowArray[i].window = flattopWindow(SamplesPerSec*seconds);
			if(!wm->windowArray[i].window)
			{
				logmsg ("FlatTop window creation failed\n");
				return(0);
			}
			wm->windowArray[i].size = SamplesPerSec*seconds;
		}

		if(config->window == 'h')
		{
			wm->windowArray[i].window = hannWindow(SamplesPerSec*seconds);
			if(!wm->windowArray[i].window)
			{
				logmsg ("Hann window creation failed\n");
				return(0);
			}
			wm->windowArray[i].size = SamplesPerSec*seconds;
		}

		if(config->window == 'm')
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
// 2.5% slopes 20-25, half a frame in 20
//#define TUKEY_5
// 5.0% slopes 40-85, 1 frame in 20
double *tukeyWindow(int n)
{
	int slope, i, idx;
	double *w;
 
	w = (double*) calloc(n, sizeof(double));
	memset(w, 0, n*sizeof(double));
 
	if(n%2==0)
	{
#ifdef	TUKEY_5
		slope = n/20;
#else
		slope = (n+1)/40;
#endif
		for(i=0; i<slope; i++)
		{
#ifdef	TUKEY_5
			w[i] = 25*(1+cos(2*M_PI/(n-1)*(i-(n-1)/2)));
#else
			w[i] = 85*(1+cos(2*M_PI/(n-1)*(i-(n-1)/2)));
#endif
			if(w[i] > 1.0)
				w[i] = 1.0;
		}

		for(i=0; i<n-2*slope; i++)
			w[i+slope] = 1;
 
		idx = slope-1;
		for(i=n - slope; i<n; i++) {
			w[i] = w[idx];
			idx--;
		}
	}
	else
	{
#ifdef	TUKEY_5
		slope = (n+1)/20;
#else
		slope = (n+1)/40;
#endif
		for(i=0; i<slope; i++)
		{
#ifdef	TUKEY_5
			w[i] = 25*(1+cos(2*M_PI/(n-1)*(i-(n-1)/2)));
#else
			w[i] = 85*(1+cos(2*M_PI/(n-1)*(i-(n-1)/2)));
#endif
			if(w[i] > 1.0)
				w[i] = 1.0;
		}

		for(i=0; i<n-2*slope; i++)
			w[i+slope] = 1;
 
		idx = slope-2;
		for(i=n - slope; i<n; i++) {
			w[i] = w[idx];
			idx--;
		}
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

double CompensateValueForWindow(double value, parameters *config)
{
	switch(config->window)
	{
		case 'n':
			break;
		case 't':
			value *= 1.03366;
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
