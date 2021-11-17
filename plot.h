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
	double			leftmargin;
	char			*SpecialWarning;
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
#define	MODE_TSDIFF		4

#define X0BORDER	0.125	// left border
#define Y0BORDER 	0.06	// bottom border
#define X1BORDER	0.045	// right border
#define Y1BORDER 	0.10	// top border

/* 1 */
#define PLOT_RES_X_LOW 1200.0
#define PLOT_RES_Y_LOW 600.0
/* 2 */
#define PLOT_RES_X 1800.0
#define PLOT_RES_Y 900.0
/* 3 */
#define PLOT_RES_X_1K 1920.0
#define PLOT_RES_Y_1K 1080.0
/* 4 */
#define PLOT_RES_X_HI 3600.0
#define PLOT_RES_Y_HI 1800.0
/* 5 */
#define PLOT_RES_X_4K 3840.0
#define PLOT_RES_Y_4K 2160.0
/* 6 */
#define PLOT_RES_X_FP 8000.0
#define PLOT_RES_Y_FP 4000.0

#define PHASE_ANGLE		200
#define	PHASE_DIFF		0
#define	PHASE_REF		1
#define	PHASE_COMP		2

#define FONT_SIZE_1		config->plotResY/60
#define FONT_SIZE_2		config->plotResY/70
#define FONT_SIZE_3		config->plotResY/80

#define	PLOT_FONT		"HersheySans"
#define	PLOT_SPACER		2

#define LEFT_MARGIN		config->plotResX/150
#define HEIGHT_MARGIN	config->plotResY/15

#define NO_DRAW_BARS	0
#define DRAW_BARS		1

#define	WAVEFORM_GENERAL	0
#define	WAVEFORM_WINDOW		1
#define	WAVEFORM_AMPDIFF	2
#define	WAVEFORM_MISSING	3
#define	WAVEFORM_EXTRA		4

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
	char	channel;
} FlatAmplDifference;

typedef struct flat_FrequencySt {
	double	hertz;
	double	amplitude;
	int		type;
	int		color;
	char	channel;
} FlatFrequency;

typedef struct flat_phase_St {
	double	hertz;
	double	phase;
	int		type;
	int		color;
	char	channel;
} FlatPhase;

void PlotResults(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config);
void PlotAmpDifferences(parameters *config);
void PlotAllWeightedAmpDifferences(parameters *config);
//void PlotFreqMissing(parameters *config);
FlatFrequency *CreateSpectrogramFrequencies(AudioSignal *Signal, long int *size, int NoiseFloor, parameters *config);
void PlotDifferentAmplitudesWithBetaFunctions(parameters *config);
void PlotNoiseFloor(AudioSignal *Signal, parameters *config);

int FillPlot(PlotFile *plot, char *name, double x0, double y0, double x1, double y1, double penWidth, double leftMarginSize, parameters *config);
int FillPlotExtra(PlotFile *plot, char *name, int sizex, int sizey, double x0, double y0, double x1, double y1, double penWidth, double leftMarginSize, parameters *config);
int CreatePlotFile(PlotFile *plot, parameters *config);
int ClosePlot(PlotFile *plot);
void SetPenColorStr(char *colorName, long int color, PlotFile *plot);
void SetPenColor(int colorIndex, long int color, PlotFile *plot);
void SetFillColor(int colorIndex, long int color, PlotFile *plot);
int MatchColor(char *color);

void PlotAllDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, char channel, char *filename, parameters *config);
int PlotEachTypeDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config);
void PlotSingleTypeDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, int type, char *filename, char channel, parameters *config);

//int PlotNoiseDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config, AudioSignal *Signal);
//void PlotSilenceBlockDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, int type, char *filename, parameters *config, AudioSignal *Signal);

//int PlotEachTypeMissingFrequencies(FlatFrequency *freqDiff, long int size, char *filename, parameters *config);
//void PlotSingleTypeMissingFrequencies(FlatFrequency *freqDiff, long int size, int type, char *filename, parameters *config);
//void PlotAllMissingFrequencies(FlatFrequency *freqDiff, long int size, char *filename, parameters *config);

int PlotEachTypeSpectrogram(FlatFrequency *freqs, long int size, char *filename, int signal, parameters *config, AudioSignal *Signal);
int PlotNoiseFloorSpectrogram(FlatFrequency* freqs, long int size, char* filename, int signal, parameters* config, AudioSignal* Signal);

void PlotSingleTypeSpectrogram(FlatFrequency *freqs, long int size, int type, char *filename, int signal, char channel, parameters *config);
void PlotAllSpectrogram(FlatFrequency *freqs, long int size, char *filename, int signal, parameters *config);

void PlotWindow(windowUnit *windowUnit, parameters *config);
void PlotBetaFunctions(parameters *config);

FlatAmplDifference *CreateFlatDifferences(parameters *config, long int *size, diffPlotType plotType);
//FlatFrequency *CreateFlatMissing(parameters *config, long int *size);
FlatFrequency *CreateFlatFrequencies(AudioSignal *Signal, long int *size, int NoiseFloor, parameters *config);

double transformtoLog(double coord, parameters *config);
void DrawGridZeroDBCentered(PlotFile *plot, double dbs, double dbIncrement, double hz, double hzIncrement, parameters *config);
void DrawLabelsZeroDBCentered(PlotFile *plot, double dbs, double dbIncrement, double hz, parameters *config);
void DrawGridZeroToLimit(PlotFile *plot, double dBFS, double dbIncrement, double hz, double hzIncrement, int drawSignificant, parameters *config);
void DrawLabelsZeroToLimit(PlotFile *plot, double dbs, double dbIncrement, double hz, int drawSignificant, parameters *config);
void DrawColorScale(PlotFile *plot, int type, int mode, double x, double y, double width, double height, double startDbs, double endDbs, double dbIncrement, parameters *config);
void DrawColorAllTypeScale(PlotFile *plot, int mode, double x, double y, double width, double height, double endDbs, double dbIncrement, int drawBars, parameters *config);

int PlotDifferentAmplitudesAveraged(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config);
AveragedFrequencies *CreateFlatDifferencesAveraged(int matchType, char channel, long int *avgSize, diffPlotType plotType, parameters *config);
void PlotSingleTypeDifferentAmplitudesAveraged(FlatAmplDifference *amplDiff, long int size, int type, char *filename, AveragedFrequencies *averaged, long int avgsize, char channel, parameters *config);
void PlotAllDifferentAmplitudesAveraged(FlatAmplDifference *amplDiff, long int size, char *filename, AveragedFrequencies **averaged, long int *avgsize, parameters *config);
double DrawMatchBar(PlotFile *plot, int colorName, double x, double y, double width, double height, double notFound, double total, parameters *config);

void PlotTest(char *filename, parameters *config);
void PlotTestZL(char *filename, parameters *config);
void VisualizeWindows(windowManager *wm, parameters *config);

char *GetCurrentPathAndChangeToResultsFolder(parameters *config);
void ReturnToMainPath(char **CurrentPath);

int PlotNoiseDifferentAmplitudesAveraged(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config, AudioSignal *Signal);
void PlotNoiseDifferentAmplitudesAveragedInternal(FlatAmplDifference *amplDiff, long int size, int type, char *filename, AveragedFrequencies *averaged, long int avgsize, parameters *config, AudioSignal *Signal);
void PlotNoiseSpectrogram(FlatFrequency *freqs, long int size, char channel, char *filename, int signal, parameters *config, AudioSignal *Signal);
void SaveCSVAmpDiff(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config);

void DrawFrequencyHorizontal(PlotFile *plot, double vertical, double hz, double hzIncrement, parameters *config);
void DrawFrequencyHorizontalGrid(PlotFile *plot, double hz, double hzIncrement, parameters *config);
void PlotTimeSpectrogram(AudioSignal *Signal, char channel, parameters *config);
void PlotSingleTypeTimeSpectrogram(AudioSignal* Signal, char channel, int plotType, parameters* config);
void PlotTimeSpectrogramUnMatchedContent(AudioSignal *Signal, char channel, parameters *config);

void DrawLabelsTimeSpectrogram(PlotFile *plot, int khz, int khzIncrement, parameters *config);
void PlotTimeDomainGraphs(AudioSignal *Signal, parameters *config);
void PlotBlockTimeDomainGraph(AudioSignal *Signal, int block, char *name, int window, double data, parameters *config);
void PlotBlockPhaseGraph(AudioSignal *Signal, int block, char *name, parameters *config);
void PlotBlockTimeDomainInternalSyncGraph(AudioSignal *Signal, int block, char *name, int slot, parameters *config);
void PlotTimeDomainHighDifferenceGraphs(AudioSignal *Signal, parameters *config);

FlatPhase *CreatePhaseFlatDifferences(parameters *config, long int *size);
void PlotSingleTypePhase(FlatPhase *phaseDiff, long int size, int type, char *filename, int pType, char channel, parameters *config);
int PlotEachTypePhase(FlatPhase *phaseDiff, long int size, char *filename, int pType, parameters *config);
void PlotAllPhase(FlatPhase *phaseDiff, long int size, char *filename, int pType, parameters *config);
void PlotPhaseDifferences(parameters *config);

//FlatPhase *CreatePhaseFlatFromSignal(AudioSignal *Signal, long int *size, parameters *config);
//void PlotPhaseFromSignal(AudioSignal *Signal, parameters *config);

void DrawGridZeroAngleCentered(PlotFile *plot, double maxAngle, double angleIncrement, double hz, parameters *config);
void DrawLabelsZeroAngleCentered(PlotFile *plot, double maxAngle, double angleIncrement, double hz, parameters *config);
void PlotDifferenceTimeSpectrogram(parameters *config);

void PlotCLKSpectrogram(AudioSignal *Signal, parameters *config);
FlatFrequency *CreateFlatFrequenciesCLK(AudioSignal *Signal, long int *size, parameters *config);
void PlotCLKSpectrogramInternal(FlatFrequency *freqs, long int size, char *filename, int signal, parameters *config);

#endif
