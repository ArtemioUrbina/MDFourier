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

#define PLOT_PROCESS_CHAR "-"
#define PLOT_ADVANCE_CHAR ">"

#define PLOT_COMPARE	1
#define PLOT_SINGLE_REF	2
#define PLOT_SINGLE_COM	3

typedef struct plot_st {
	char			FileName[T_BUFFER_SIZE];
	plPlotter		*plotter;
	plPlotterParams *plotter_params;
	FILE			*file;
	int				sizex, sizey;
	double			x0, x1, y0, y1;
	double			Rx0, Rx1, Ry0, Ry1;
	double			penWidth;
} PlotFile;

typedef struct averaged_freq{
	double		avgfreq;
	double		avgvol;
} AveragedFrequencies;

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
#define COLOR_NULL		-1

#define	MODE_DIFF		1
#define	MODE_MISS		2
#define	MODE_SPEC		3

typedef enum differencePlotType
{
	normalPlot,
	floorPlot,
} diffPlotType;

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

typedef struct flat_FrequencySt {
	double	hertz;
	double	amplitude;
	int		type;
	int		color;
} FlatFrequency;

void PlotResults(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config);
void PlotAmpDifferences(parameters *config);
void PlotAllWeightedAmpDifferences(parameters *config);
void PlotFreqMissing(parameters *config);
void PlotSpectrograms(AudioSignal *Signal, parameters *config);
void PlotDifferentAmplitudesWithBetaFunctions(parameters *config);
void PlotNoiseFloor(AudioSignal *Signal, parameters *config);

int FillPlot(PlotFile *plot, char *name, int sizex, int sizey, double x0, double y0, double x1, double y1, double penWidth, parameters *config);
int CreatePlotFile(PlotFile *plot, parameters *config);
int ClosePlot(PlotFile *plot);
void SetPenColorStr(char *colorName, long int color, PlotFile *plot);
void SetPenColor(int colorIndex, long int color, PlotFile *plot);
void SetFillColor(int colorIndex, long int color, PlotFile *plot);
int MatchColor(char *color);

void PlotAllDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config);
int PlotEachTypeDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config);
void PlotSingleTypeDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, int type, char *filename, parameters *config);

int PlotNoiseDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config, AudioSignal *Signal);
void PlotSilenceBlockDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, int type, char *filename, parameters *config, AudioSignal *Signal);

int PlotEachTypeMissingFrequencies(FlatFreqDifference *freqDiff, long int size, char *filename, parameters *config);
void PlotSingleTypeMissingFrequencies(FlatFreqDifference *freqDiff, long int size, int type, char *filename, parameters *config);
void PlotAllMissingFrequencies(FlatFreqDifference *freqDiff, long int size, char *filename, parameters *config);

int PlotEachTypeSpectrogram(FlatFrequency *freqs, long int size, char *filename, int signal, parameters *config, AudioSignal *Signal);
void PlotSingleTypeSpectrogram(FlatFrequency *freqs, long int size, int type, char *filename, int signal, parameters *config);
void PlotAllSpectrogram(FlatFrequency *freqs, long int size, char *filename, int signal, parameters *config);

void PlotWindow(windowUnit *windowUnit, parameters *config);
void PlotBetaFunctions(parameters *config);

FlatAmplDifference *CreateFlatDifferences(parameters *config, long int *size, diffPlotType plotType);
FlatFreqDifference *CreateFlatMissing(parameters *config, long int *size);
FlatFrequency *CreateFlatFrequencies(AudioSignal *Signal, long int *size, parameters *config);

double transformtoLog(double coord, parameters *config);
void DrawGridZeroDBCentered(PlotFile *plot, double dbs, double dbIncrement, double hz, double hzIncrement, parameters *config);
void DrawLabelsZeroDBCentered(PlotFile *plot, double dbs, double dbIncrement, double hz, double hzIncrement,  parameters *config);
void DrawGridZeroToLimit(PlotFile *plot, double dbs, double dbIncrement, double hz, double hzIncrement, parameters *config);
void DrawLabelsZeroToLimit(PlotFile *plot, double dbs, double dbIncrement, double hz, double hzIncrement,  parameters *config);
void DrawColorScale(PlotFile *plot, int type, int mode, double x, double y, double width, double height, double startDbs, double endDbs, double dbIncrement, parameters *config);
void DrawColorAllTypeScale(PlotFile *plot, int mode, double x, double y, double width, double height, double endDbs, double dbIncrement, parameters *config);

int PlotDifferentAmplitudesAveraged(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config);
AveragedFrequencies *CreateFlatDifferencesAveraged(int matchType, long int *avgSize, int chunks, diffPlotType plotType, parameters *config);
void PlotSingleTypeDifferentAmplitudesAveraged(FlatAmplDifference *amplDiff, long int size, int type, char *filename, AveragedFrequencies *averaged, long int avgsize, parameters *config);
void PlotAllDifferentAmplitudesAveraged(FlatAmplDifference *amplDiff, long int size, char *filename, AveragedFrequencies **averaged, long int *avgsize, parameters *config);
double DrawMatchBar(PlotFile *plot, int colorName, double x, double y, double width, double height, double notFound, double total, parameters *config);

void PlotTest(char *filename, parameters *config);
void PlotTestZL(char *filename, parameters *config);
void VisualizeWindows(windowManager *wm, parameters *config);

char *GetCurrentPathAndChangeToResultsFolder(parameters *config);
void ReturnToMainPath(char **CurrentPath);

int PlotNoiseDifferentAmplitudesAveraged(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config, AudioSignal *Signal);
void PlotNoiseDifferentAmplitudesAveragedInternal(FlatAmplDifference *amplDiff, long int size, int type, char *filename, AveragedFrequencies *averaged, long int avgsize, parameters *config, AudioSignal *Signal);
void PlotNoiseSpectrogram(FlatFrequency *freqs, long int size, int type, char *filename, int signal, parameters *config, AudioSignal *Signal);
void SaveCSV(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config);

void DrawFrequencyHorizontalGrid(PlotFile *plot, double hz, double hzIncrement, parameters *config);
void PlotTimeSpectrogram(AudioSignal *Signal, parameters *config);
void PlotTimeSpectrogramUnMatchedContent(AudioSignal *Signal, parameters *config);

void PlotTimeDomainGraphs(AudioSignal *Signal, parameters *config);
void PlotBlockTimeDomainGraph(AudioSignal *Signal, int block, char *name, int window, parameters *config);

#endif
