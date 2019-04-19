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
 * Compile with: 
 *	  gcc -Wall -std=gnu99 -o mdfourier mdfourier.c -lfftw3 -lm
 */

#include "mdfourier.h"
#include "windows.h"
#include "log.h"

int existsInArray(double element, double *array, int count)
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

float *getWindowByLength(windowManager *wm, double length)
{
	if(!wm)
		return 0;

	for(int i = 0; i < wm->windowCount; i++)
	{
		if(length == wm->windowArray[i].seconds)
			return wm->windowArray[i].window;
	}

	return NULL;
}

int initWindows(windowManager *wm, int SamplesPerSec, parameters *config)
{
	double 	lengths[1024];	// yes, we are lazy
	int 	count = 0;

	if(!wm || !config)
		return 0;
	
	// Count how many differen window sizes are needed
	for(int i = 0; i < config->types.typeCount; i++)
	{
		double value;

		value = config->types.typeArray[i].seconds;
		if(!existsInArray(value, lengths, count))
			lengths[count++] = value;
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
		wm->windowArray[i].seconds = lengths[i];
		if(config->window != 'n')
		{
			if(config->window == 't')
			{
				wm->windowArray[i].window = tukeyWindow(SamplesPerSec*lengths[i]);
				if(!wm->windowArray[i].window)
				{
					logmsg ("Tukey window creation failed\n");
					return(0);
				}
			}
	
			if(config->window == 'f')
			{
				wm->windowArray[i].window = flattopWindow(SamplesPerSec*lengths[i]);
				if(!wm->windowArray[i].window)
				{
					logmsg ("FlatTop window creation failed\n");
					return(0);
				}
			}
	
			if(config->window == 'h')
			{
				wm->windowArray[i].window = hannWindow(SamplesPerSec*lengths[i]);
				if(!wm->windowArray[i].window)
				{
					logmsg ("Hann window creation failed\n");
					return(0);
				}
			}
		}
	}

	/*
	// This is to graph the wm exterally for the docs
	if(wm->windowArray && wm->windowArray->windowCount && wm->windowArray[0])
		for(int w= 0; w < SamplesPerSec*lengths[0]; w++)
			logmsg("%d,%g\n", w, wm->windowArray[i].window[w]);
	exit(1); 
	*/

	return 1;
}


void freeWindows(windowManager *wm)
{
	if(!wm)
		return;

	for(int i = 0; i < wm->windowCount; i++)
		free(wm->windowArray[i].window);
	free(wm->windowArray);
	wm->windowArray = NULL;
	wm->windowCount = 0;
}


// reduce scalloping loss 
float *flattopWindow(int n)
{
	int half, i, idx;
	float *w;
 
	w = (float*) calloc(n, sizeof(float));
	memset(w, 0, n*sizeof(float));
 
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
// 2.5% slopes
float *tukeyWindow(int n)
{
	int slope, i, idx;
	float *w;
 
	w = (float*) calloc(n, sizeof(float));
	memset(w, 0, n*sizeof(float));
 
	if(n%2==0)
	{
		slope = n/40;
		for(i=0; i<slope; i++)
		{
			w[i] = 85*(1+cos(2*M_PI/(n-1)*(i-(n-1)/2)));
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
		slope = (n+1)/40;
		for(i=0; i<slope; i++)
		{
			w[i] = 85*(1+cos(2*M_PI/(n-1)*(i-(n-1)/2)));
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

float *hannWindow(int n)
{
	int half, i, idx;
	float *w;
 
	w = (float*) calloc(n, sizeof(float));
	memset(w, 0, n*sizeof(float));

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
