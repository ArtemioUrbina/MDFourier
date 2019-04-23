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

#ifndef MDFOURIER_PLOT_H
#define MDFOURIER_PLOT_H

#include "mdfourier.h"
#include <plot.h>

typedef struct plot_st {
	char			FileName[2048];
	plPlotter		*plotter;
	plPlotterParams *plotter_params;
	FILE			*file;
	int				sizex, sizey;
	double			x0, x1, y0, y1;
	double			penWidth;
} PlotFile;

void PlotDifferentBlockAmplitudes(int block, parameters *config);
void PlotAllDifferentAmplitudes(char *filename, parameters *config);
void PlotAllMissingFrequencies(char *filename, parameters *config);
void PlotSpectrogram(char *filename, AudioSignal *Signal, int block, parameters *config);
void PlotAllSpectrogram(char *filename, AudioSignal *Signal, parameters *config);
void PlotWindow(windowManager *wm, parameters *config);
int FillPlot(PlotFile *plot, char *name, int sizex, int sizey, double x0, double y0, double x1, double y1, double penWidth, parameters *config);
int CreatePlotFile(PlotFile *plot);
int ClosePlot(PlotFile *plot);
#endif
