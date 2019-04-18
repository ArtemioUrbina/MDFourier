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
#include "windows.h"
#include "log.h"

int initWindows(windowManager *windows, int SamplesPerSec, char type)
{
	if(!windows)
		return 0;

	windows->win1Sec = NULL;
	windows->win2Sec = NULL;

	if(type != 'n')
	{
		if(type == 't')
		{
			windows->win2Sec = tukeyWindow(SamplesPerSec*2);
			if(!windows->win2Sec)
			{
				logmsg ("Tukey window creation failed\n");
				return(0);
			}
		
			windows->win1Sec = tukeyWindow(SamplesPerSec);
			if(!windows->win2Sec)
			{
				logmsg ("Tukey window creation failed\n");
				return(0);
			}
		}

		if(type == 'f')
		{
			windows->win2Sec = flattopWindow(SamplesPerSec*2);
			if(!windows->win2Sec)
			{
				logmsg ("FlatTop window creation failed\n");
				return(0);
			}
		
			windows->win1Sec = flattopWindow(SamplesPerSec);
			if(!windows->win1Sec)
			{
				logmsg ("FlatTop window creation failed\n");
				return(0);
			}
		}

		if(type == 'h')
		{
			windows->win2Sec = hannWindow(SamplesPerSec*2);
			if(!windows->win2Sec)
			{
				logmsg ("Hann window creation failed\n");
				return(0);
			}
		
			windows->win1Sec = hannWindow(SamplesPerSec);
			if(!windows->win1Sec)
			{
				logmsg ("Hann window creation failed\n");
				return(0);
			}
		}
	}

	/*
	if(windows->win2Sec)
		for(int w= 0; w < SamplesPerSec*2; w++)
			logmsg("%d,%g\n", w, windows->win2Sec[w]);
	exit(1); 
	*/

	return 1;
}


void freeWindows(windowManager *windows)
{
	if(!windows)
		return;

	free(windows->win1Sec);
	windows->win1Sec = NULL;
	free(windows->win2Sec);
	windows->win2Sec = NULL;
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
