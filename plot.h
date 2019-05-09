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

#define SPECTR_Y_FACTOR	0.1
#define SPECTR_WIDTH	10
#define TOP_FREQUENCY	20000   // we don't use 22050 for integer scaling

#define COLOR_NONE		0	
#define COLOR_RED 		1
#define COLOR_GREEN		2
#define COLOR_BLUE		3
#define COLOR_YELLOW	4
#define COLOR_MAGENTA	5
#define COLOR_AQUA		6
#define COLOR_PURPLE	7
#define COLOR_ORANGE	8
#define COLOR_GRAY		9

typedef struct flat_ampl_diff_st {
	double	hertz;
	double	refAmplitude;
	double	diffAmplitude;
	int		type;
	int		color;
} FlatAmplDifference;

typedef struct flat_freq_diff_st {
	double	hertz;
	double	amplitude;
	int		type;
	int		color;
} FlatFreqDifference;

void PlotResults(AudioSignal *Signal, parameters *config);

int FillPlot(PlotFile *plot, char *name, int sizex, int sizey, double x0, double y0, double x1, double y1, double penWidth, parameters *config);
int CreatePlotFile(PlotFile *plot);
int ClosePlot(PlotFile *plot);
void SetPenColorStr(char *colorName, long int color, PlotFile *plot);
void SetPenColor(int colorIndex, long int color, PlotFile *plot);
int MatchColor(char *color);

void PlotAllDifferentAmplitudes(FlatAmplDifference *amplDiff, char *filename, parameters *config);
int PlotEachTypeDifferentAmplitudes(FlatAmplDifference *amplDiff, char *filename, parameters *config);
void PlotSingleTypeDifferentAmplitudes(FlatAmplDifference *amplDiff, int type, char *filename, parameters *config);

int PlotEachTypeMissingFrequencies(FlatFreqDifference *freqDiff, char *filename, parameters *config);
void PlotSingleTypeMissingFrequencies(FlatFreqDifference *freqDiff, int type, char *filename, parameters *config);
void PlotAllMissingFrequencies(FlatFreqDifference *freqDiff, char *filename, parameters *config);

void PlotAllSpectrogram(char *filename, AudioSignal *Signal, parameters *config);
//void PlotAllSpectrogramLineBased(char *filename, AudioSignal *Signal, parameters *config);

void PlotWindow(windowManager *wm, parameters *config);
void PlotBetaFunctions(parameters *config);

FlatAmplDifference *CreateFlatDifferences(parameters *config);
FlatFreqDifference *CreateFlatMissing(parameters *config);

double transformtoLog(double coord, double top, parameters *config);



#endif
