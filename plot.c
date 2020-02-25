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

#include "plot.h"
#include "log.h"
#include "freq.h"
#include "diff.h"
#include "cline.h"
#include "windows.h"

#define SORT_NAME AmplitudeDifferences
#define SORT_TYPE FlatAmplDifference
#define SORT_CMP(x, y)  ((x).refAmplitude < (y).refAmplitude ? -1 : ((x).refAmplitude == (y).refAmplitude ? 0 : 1))
#include "sort.h"  // https://github.com/swenson/sort/

#define SORT_NAME AmplitudeDifferencesByFrequencyAveraged
#define SORT_TYPE FlatAmplDifference
#define SORT_CMP(x, y)  ((x).hertz < (y).hertz ? -1 : ((x).hertz == (y).hertz ? 0 : 1))
#include "sort.h"  // https://github.com/swenson/sort/

/*
#define SORT_NAME FlatMissingDifferencesByAmplitude
#define SORT_TYPE FlatFrequency
#define SORT_CMP(x, y)  ((x).amplitude < (y).amplitude ? -1 : ((x).amplitude == (y).amplitude ? 0 : 1))
#include "sort.h"  // https://github.com/swenson/sort/
*/

#define SORT_NAME FlatFrequenciesByAmplitude
#define SORT_TYPE FlatFrequency
#define SORT_CMP(x, y)  ((x).amplitude < (y).amplitude ? -1 : ((x).amplitude == (y).amplitude ? 0 : 1))
#include "sort.h"  // https://github.com/swenson/sort/

#define SORT_NAME PhaseDifferencesByFrequency
#define SORT_TYPE FlatPhase
#define SORT_CMP(x, y)  ((x).hertz < (y).hertz ? -1 : ((x).hertz == (y).hertz ? 0 : 1))
#include "sort.h"  // https://github.com/swenson/sort/

#define SORT_NAME AmplitudeDifferencesBlock
#define SORT_TYPE AmplDifference
#define SORT_CMP(x, y)  ((x).diffAmplitude < (y).diffAmplitude ? -1 : ((x).diffAmplitude == (y).diffAmplitude ? 0 : 1))
#include "sort.h"  // https://github.com/swenson/sort/

#define DIFFERENCE_TITLE			"DIFFERENT AMPLITUDES [%s]"
#define MISSING_TITLE				"MISSING FREQUENCIES [%s]"
#define EXTRA_TITLE_TS_REF			"Comparison - MISSING FREQUENCIES - Time Spectrogram [%s] (Frequencies expected in Comparison)"
#define EXTRA_TITLE_TS_COM			"Comparison - EXTRA FREQUENCIES - Time Spectrogram [%s] (Frequencies not in Reference file)"
#define SPECTROGRAM_TITLE_REF		"Reference - SPECTROGRAM [%s]"
#define SPECTROGRAM_TITLE_COM		"Comparison - SPECTROGRAM [%s]"
#define TSPECTROGRAM_TITLE_REF		"Reference - TIME SPECTROGRAM [%s]"
#define TSPECTROGRAM_TITLE_COM		"Comparison - TIME SPECTROGRAM [%s]"
#define DIFFERENCE_AVG_TITLE		"DIFFERENT AMPLITUDES AVERAGED [%s]"
#define NOISE_TITLE					"NOISE FLOOR AVERAGED"
#define NOISE_AVG_TITLE				"NOISE FLOOR AVERAGED"
#define SPECTROGRAM_NOISE_REF		"Reference NOISE FLOOR - Spectrogram [%s]"
#define SPECTROGRAM_NOISE_COM		"Comparison NOISE FLOOR - Spectrogram [%s]"
#define WAVEFORM_TITLE_REF			"Reference - WAVEFORM[%s]"
#define WAVEFORM_TITLE_COM			"Comparison - WAVEFORM [%s]"
#define PHASE_DIFF_TITLE			"PHASE DIFFERENCE [%s]"
#define PHASE_SIG_TITLE_REF			"Reference - PHASE [%s]"
#define PHASE_SIG_TITLE_COM			"Comparison - PHASE [%s]"
#define TS_DIFFERENCE_TITLE			"DIFFERENT AMPLITUDES - TIME SPECTROGRAM [%s]"

#define BAR_HEADER					"Matched frequencies"
#define BAR_DIFF					"w/any amplitude difference"
#define BAR_WITHIN					"within [0 to \\+-%gdBFS]"
#define BAR_WITHIN_PERFECT			"within (0 to \\+-%gdBFS]"
#define BAR_PERFECT					"Perfect Matches"

#define ALL_LABEL					"ALL"

#define BAR_WIDTH		config->plotResX/40
#define	BAR_HEIGHT		config->plotResY/60

#define	VERT_SCALE_STEP			3
#define	VERT_SCALE_STEP_BAR		3

#define	COLOR_BARS_WIDTH_SCALE	220

#define	DIFFERENCE_FOLDER	"Difference"
#define	SPECTROGRAM_FOLDER	"Spectrograms"
#define	WAVEFORM_FOLDER		"Waveforms"
#define	PHASE_FOLDER		"Phase"
#define	MISSING_FOLDER		"Missing"
#define	WAVEFORMDIFF_FOLDER	"Waveform-Diff"
#define	WAVEFORMDIR_AMPL	"Amplitudes"
#define	WAVEFORMDIR_MISS	"Missing"
#define	WAVEFORMDIR_EXTRA	"Extra"

//#define TESTWARNINGS

char *GetCurrentPathAndChangeToResultsFolder(parameters *config)
{
	char 	*CurrentPath = NULL;

	CurrentPath = (char*)malloc(sizeof(char)*FILENAME_MAX);
	if(!CurrentPath)
		return NULL;

	if(!GetCurrentDir(CurrentPath, sizeof(char)*FILENAME_MAX))
	{
		free(CurrentPath);
		logmsg("Could not get current path\n");
		return NULL;
	}

	if(chdir(config->folderName) == -1)
	{
		free(CurrentPath);
		logmsg("Could not open folder %s for results\n", config->folderName);
		return NULL;
	}
	return CurrentPath;
}

void ReturnToMainPath(char **CurrentPath)
{
	if(!*CurrentPath)
		return;

	if(chdir(*CurrentPath) == -1)
		logmsg("Could not open working folder %s\n", CurrentPath);

	free(*CurrentPath);
	*CurrentPath = NULL;
}

void StartPlot(char *name, struct timespec* start, parameters *config)
{
	logmsg(name);
	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, start);
}

void EndPlot(char *name, struct timespec* start, struct timespec* end, parameters *config)
{
	logmsg("\n");

	if(config->clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, end);
		elapsedSeconds = TimeSpecToSeconds(end) - TimeSpecToSeconds(start);
		logmsg(" - clk: %s took %0.2fs\n", name, elapsedSeconds);
	}
}

char *PushFolder(char *name)
{
	char 	*CurrentPath = NULL;

	CurrentPath = (char*)malloc(sizeof(char)*FILENAME_MAX);
	if(!CurrentPath)
		return NULL;

	if(!GetCurrentDir(CurrentPath, sizeof(char)*FILENAME_MAX))
	{
		free(CurrentPath);
		logmsg("Could not get current path\n");
		return NULL;
	}

	if(!CreateFolder(name))
	{
		free(CurrentPath);
		logmsg("Could not create %s subfolder\n", name);
		return NULL;
	}
	if(chdir(name) == -1)
	{
		free(CurrentPath);
		logmsg("Could not open folder %s for results\n", name);
		return NULL;
	}
	return CurrentPath;
}

void PlotResults(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config)
{
	struct	timespec	start, end;
	char 	*CurrentPath = NULL, *MainPath = NULL;

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	MainPath = PushMainPath(config);
	CurrentPath = GetCurrentPathAndChangeToResultsFolder(config);

	if(config->plotDifferences || config->averagePlot)
	{
		struct	timespec lstart, lend;

		StartPlot(" - Difference", &lstart, config);
		PlotAmpDifferences(config);
		PlotDifferenceTimeSpectrogram(config);
		EndPlot("Differences", &lstart, &lend, config);
	}

	if(config->plotMissing)
	{
		if(!config->FullTimeSpectroScale)
		{
			struct	timespec	lstart, lend;
	
			StartPlot(" - Missing and Extra Frequencies", &lstart, config);
	
			//Old Missig plots, rendered obsolete?
			//PlotFreqMissing(config);
			PlotTimeSpectrogramUnMatchedContent(ReferenceSignal, config);
			logmsg(PLOT_ADVANCE_CHAR);
			PlotTimeSpectrogramUnMatchedContent(ComparisonSignal, config);
			logmsg(PLOT_ADVANCE_CHAR);
	
			EndPlot("Missing", &lstart, &lend, config);
		}
		else
			logmsg(" X Skipped: Missing and Extra Frequencies, due to range\n");
	}

	if(config->plotSpectrogram)
	{
		struct	timespec	lstart, lend;

		StartPlot(" - Spectrograms", &lstart, config);
		PlotSpectrograms(ReferenceSignal, config);
		PlotSpectrograms(ComparisonSignal, config);

		EndPlot("Spectrogram", &lstart, &lend, config);
	}

	if(config->plotTimeSpectrogram)
	{
		struct	timespec	lstart, lend;

		StartPlot(" - Time Spectrogram", &lstart, config);

		PlotTimeSpectrogram(ReferenceSignal, config);
		logmsg(PLOT_ADVANCE_CHAR);
		PlotTimeSpectrogram(ComparisonSignal, config);
		logmsg(PLOT_ADVANCE_CHAR);
		EndPlot("Time Spectrogram", &lstart, &lend, config);
	}

	if(config->plotPhase)
	{
		struct	timespec	lstart, lend;

		StartPlot(" - Phase", &lstart, config);

		PlotPhaseDifferences(config);
		//PlotPhaseFromSignal(ReferenceSignal, config);
		//PlotPhaseFromSignal(ComparisonSignal, config);
		
		logmsg(PLOT_ADVANCE_CHAR);
		EndPlot("Phase", &lstart, &lend, config);
	}

	if(config->plotNoiseFloor)
	{
		if(!config->noSyncProfile)
		{
			if(ReferenceSignal->hasSilenceBlock && ComparisonSignal->hasSilenceBlock)
			{
				struct	timespec	lstart, lend;
		
				StartPlot(" - Noise Floor", &lstart, config);

				PlotNoiseFloor(ReferenceSignal, config);
				EndPlot("Noise Floor", &lstart, &lend, config);
			}
			else
				logmsg(" X Noise Floor graphs ommited: no noise floor value found.\n");
		}
		else
			logmsg(" X Noise floor plots make no sense with current parameters.\n");
	}

	if((config->hasTimeDomain && config->plotTimeDomain) || config->plotAllNotes)
	{
		struct	timespec	lstart, lend;
		char 				*returnFolder = NULL;

		returnFolder = PushFolder(WAVEFORM_FOLDER);
		if(!returnFolder)
		{
			ReturnToMainPath(&CurrentPath);
			return;
		}

		StartPlot(" - Waveform Graphs\n  ", &lstart, config);
		PlotTimeDomainGraphs(ReferenceSignal, config);
		PlotTimeDomainGraphs(ComparisonSignal, config);
		EndPlot("Waveform", &lstart, &lend, config);

		ReturnToMainPath(&returnFolder);
	}

	if(config->plotTimeDomainHiDiff)
	{
		if(FindDifferenceAveragesperBlock(config->thresholdAmplitudeHiDif, config->thresholdMissingHiDif, config->thresholdExtraHiDif, config))
		{
			struct	timespec	lstart, lend;

			StartPlot(" - Time Domain Graphs from highly different notes\n  ", &lstart, config);
			PlotTimeDomainHighDifferenceGraphs(ReferenceSignal, config);
			PlotTimeDomainHighDifferenceGraphs(ComparisonSignal, config);
			EndPlot("Time Domain Graphs", &lstart, &lend, config);
		}
	}

	ReturnToMainPath(&CurrentPath);
	PopMainPath(&MainPath);

	if(config->clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - clk: Plotting PNGs took %0.2fs\n", elapsedSeconds);
	}
}

void PlotAmpDifferences(parameters *config)
{
	long int			size = 0;
	FlatAmplDifference	*amplDiff = NULL;
	
	amplDiff = CreateFlatDifferences(config, &size, normalPlot);
	if(!amplDiff)
	{
		logmsg("Not enough memory for plotting\n");
		return;
	}

	if(config->outputCSV)
		SaveCSV(amplDiff, size, config->compareName, config);

	if(config->plotDifferences)
	{
		int typeCount = 0;

		typeCount = GetActiveBlockTypesNoRepeat(config);
		if(typeCount > 1)
		{
			if(PlotEachTypeDifferentAmplitudes(amplDiff, size, config->compareName, config) > 1)
			{
				PlotAllDifferentAmplitudes(amplDiff, size, config->compareName, config);
				logmsg(PLOT_ADVANCE_CHAR);
			}
		}
		else
			PlotAllDifferentAmplitudes(amplDiff, size, config->compareName, config);
	}

	if(config->averagePlot)
		PlotDifferentAmplitudesAveraged(amplDiff, size, config->compareName, config);
	
	free(amplDiff);
	amplDiff = NULL;
}

void PlotDifferentAmplitudesWithBetaFunctions(parameters *config)
{
	long int 			size = 0;
	FlatAmplDifference	*amplDiff = NULL;
	
	amplDiff = CreateFlatDifferences(config, &size, normalPlot);
	if(!amplDiff)
	{
		logmsg("Not enough memory for plotting\n");
		return;
	}

	for(int o = 0; o < 6; o++)
	{
		config->outputFilterFunction = o;
		PlotAllDifferentAmplitudes(amplDiff, size, config->compareName, config);
	}

	free(amplDiff);
	amplDiff = NULL;
}

/*
void PlotFreqMissing(parameters *config)
{
	long int			size = 0;
	FlatFrequency	*freqDiff = NULL;
	
	freqDiff = CreateFlatMissing(config, &size);
	if(PlotEachTypeMissingFrequencies(freqDiff, size, config->compareName, config) > 1)
	{
		PlotAllMissingFrequencies(freqDiff, size, config->compareName, config);
		logmsg(PLOT_ADVANCE_CHAR);
	}

	free(freqDiff);
	freqDiff = NULL;
}
*/

void PlotSpectrograms(AudioSignal *Signal, parameters *config)
{
	char 				tmpName[BUFFER_SIZE/2];
	long int			size = 0;
	FlatFrequency		*frequencies = NULL;
	
	ShortenFileName(basename(Signal->SourceFile), tmpName);
	frequencies = CreateFlatFrequencies(Signal, &size, config);
	if(PlotEachTypeSpectrogram(frequencies, size, tmpName, Signal->role, config, Signal) > 1)
	{
		PlotAllSpectrogram(frequencies, size, tmpName, Signal->role, config);
		logmsg(PLOT_ADVANCE_CHAR);
	}

	free(frequencies);
	frequencies = NULL;
}

void PlotNoiseFloor(AudioSignal *Signal, parameters *config)
{
	long int 			size = 0;
	FlatAmplDifference	*amplDiff = NULL;
	
	amplDiff = CreateFlatDifferences(config, &size, floorPlot);
	if(!amplDiff)
	{
		logmsg("Not enough memory for plotting\n");
		return;
	}

	//PlotNoiseDifferentAmplitudes(amplDiff, size, config->compareName, config, Signal);
	
	//if(config->averagePlot)
	PlotNoiseDifferentAmplitudesAveraged(amplDiff, size, config->compareName, config, Signal);
	
	free(amplDiff);
	amplDiff = NULL;
}


int FillPlotExtra(PlotFile *plot, char *name, int sizex, int sizey, double x0, double y0, double x1, double y1, double penWidth, double leftMarginSize, parameters *config)
{
	int rt = 0;

	if(!plot)
		return 0;

	rt = FillPlot(plot, name, x0, y0, x1, y1, penWidth, leftMarginSize, config);
	plot->sizex = sizex;
	plot->sizey = sizey;
	return(rt);
}

int FillPlot(PlotFile *plot, char *name, double x0, double y0, double x1, double y1, double penWidth, double leftMarginSize, parameters *config)
{
	double dX = 0, dY = 0;

	if(!plot)
		return 0;

	plot->plotter = NULL;
	plot->plotter_params = NULL;
	plot->file = NULL;

	ComposeFileNameoPath(plot->FileName, name, ".png", config);

	plot->sizex = config->plotResX;
	plot->sizey = config->plotResY;

	plot->Rx0 = x0;
	plot->Rx1 = x1;
	plot->Ry0 = y0;
	plot->Ry1 = y1;

	plot->leftmargin = leftMarginSize;
	// Commented for partial release
	dX = X0BORDER*fabs(x0 - x1)*leftMarginSize;
	dY = Y0BORDER*fabs(y0 - y1);

	// Commented for partial release
	plot->x0 = x0-dX;
	plot->y0 = y0-dY;

	dX = X1BORDER*fabs(x0 - x1);
	dY = Y1BORDER*fabs(y0 - y1);

	plot->x1 = x1+dX;
	plot->y1 = y1+dY;

	plot->penWidth = penWidth;

	return 1;
}

int CreatePlotFile(PlotFile *plot, parameters *config)
{
	char		size[20];

	plot->file = fopen(plot->FileName, "wb");
	if(!plot->file)
	{
		logmsg("Couldn't create graph file %s\n%s\n", plot->FileName, strerror(errno));
		return 0;
	}

	sprintf(size, "%dx%d", plot->sizex, plot->sizey);
	plot->plotter_params = pl_newplparams ();
	pl_setplparam (plot->plotter_params, "BITMAPSIZE", size);
	if((plot->plotter = pl_newpl_r("png", stdin, plot->file, stderr, plot->plotter_params)) == NULL)
	{
		logmsg("Couldn't create Plotter\n");
		return 0;
	}

	if(pl_openpl_r(plot->plotter) < 0)
	{
		logmsg("Couldn't open Plotter\n");
		return 0;
	}
	pl_fspace_r(plot->plotter, plot->x0, plot->y0, plot->x1, plot->y1);
	pl_flinewidth_r(plot->plotter, plot->penWidth);
	if(config->whiteBG)
		pl_bgcolor_r(plot->plotter, 0xffff, 0xffff, 0xffff);
	else
		pl_bgcolor_r(plot->plotter, 0, 0, 0);
	pl_erase_r(plot->plotter);

	/*
	SetPenColor(COLOR_GREEN, 0xffff, plot);	
	pl_fbox_r(plot->plotter, plot->Rx0,  plot->Ry0,  plot->Rx1,  plot->Ry1);
	pl_endsubpath_r(plot->plotter);
	*/

	return 1;
}

int ClosePlot(PlotFile *plot)
{
	if(pl_closepl_r(plot->plotter) < 0)
	{
		logmsg("Couldn't close Plotter\n");
		return 0;
	}
	
	if(pl_deletepl_r(plot->plotter) < 0)
	{
		logmsg("Couldn't delete Plotter\n");
		return 0;
	}
	plot->plotter = NULL;

	if(pl_deleteplparams(plot->plotter_params) < 0)
	{
		logmsg("Couldn't delete Plotter Params\n");
		return 0;
	}
	plot->plotter_params = NULL;

	fclose(plot->file);
	plot->file = NULL;

	return 1;
}

void DrawFrequencyHorizontal(PlotFile *plot, double vertical, double hz, double hzIncrement, parameters *config)
{
	pl_pencolor_r (plot->plotter, 0, 0x5555, 0);
	for(int i = hzIncrement; i < hz; i += hzIncrement)
	{
		pl_fline_r(plot->plotter, transformtoLog(i, config), -1*vertical, transformtoLog(i, config), vertical);
		pl_endpath_r(plot->plotter);
	}

	pl_pencolor_r (plot->plotter, 0, 0x7777, 0);
	if(config->logScale)
	{
		pl_fline_r(plot->plotter, transformtoLog(10, config), -1*vertical, transformtoLog(10, config), vertical);
		pl_endpath_r(plot->plotter);
		pl_fline_r(plot->plotter, transformtoLog(100, config), -1*vertical, transformtoLog(100, config), vertical);
		pl_endpath_r(plot->plotter);
	}
	pl_fline_r(plot->plotter, transformtoLog(1000, config), -1*vertical, transformtoLog(1000, config), vertical);
	pl_endpath_r(plot->plotter);
	if(config->endHzPlot >= 10000)
	{
		for(int i = 10000; i < config->endHzPlot; i+= 10000)
		{
			pl_fline_r(plot->plotter, transformtoLog(i, config), -1*vertical, transformtoLog(i, config), vertical);
			pl_endpath_r(plot->plotter);
		}
	}
}

void DrawGridZeroDBCentered(PlotFile *plot, double dBFS, double dbIncrement, double hz, double hzIncrement, parameters *config)
{
	if(fabs(dBFS) <= 1.0)
		dbIncrement = fabs(dBFS)/10.0;
	else
	{
		if(fabs(dBFS) <= 3.0)
			dbIncrement = 1.0;
	}

	if(config->maxDbPlotZC == DB_HEIGHT)
		pl_pencolor_r(plot->plotter, 0, 0xaaaa, 0);
	else
		pl_pencolor_r(plot->plotter, 0xaaaa, 0xaaaa, 0);
	pl_fline_r(plot->plotter, 0, 0, hz, 0);
	pl_endpath_r(plot->plotter);

	if(config->maxDbPlotZC == DB_HEIGHT)
		pl_pencolor_r(plot->plotter, 0, 0x5555, 0);
	else
		pl_pencolor_r(plot->plotter, 0x5555, 0x5555, 0);
	for(double i = dbIncrement; i < dBFS; i += dbIncrement)
	{
		pl_fline_r(plot->plotter, 0, i, hz, i);
		pl_fline_r(plot->plotter, 0, -1*i, hz, -1*i);
	}
	pl_endpath_r(plot->plotter);

	DrawFrequencyHorizontal(plot, dBFS, hz, hzIncrement, config);

	pl_endpath_r(plot->plotter);
	pl_pencolor_r (plot->plotter, 0, 0xFFFF, 0);
}

void DrawGridZeroToLimit(PlotFile *plot, double dBFS, double dbIncrement, double hz, double hzIncrement, int drawSignificant, parameters *config)
{
	pl_pencolor_r (plot->plotter, 0, 0x5555, 0);
	for(int i = dbIncrement; i < fabs(dBFS); i += dbIncrement)
		pl_fline_r(plot->plotter, 0, -1*i, hz, -1*i);

	pl_pencolor_r (plot->plotter, 0, 0x5555, 0);
	for(int i = hzIncrement; i < hz; i += hzIncrement)
		pl_fline_r(plot->plotter, transformtoLog(i, config), dBFS, transformtoLog(i, config), 0);

	if(drawSignificant)
	{
		pl_pencolor_r (plot->plotter, 0x9999, 0x9999, 0);
		//pl_linemod_r(plot->plotter, "dotdashed");
		pl_fline_r(plot->plotter, 0, config->significantAmplitude, hz, config->significantAmplitude);
		//pl_linemod_r(plot->plotter, "solid");
	}

	pl_pencolor_r (plot->plotter, 0, 0x7777, 0);
	if(config->logScale)
	{
		pl_fline_r(plot->plotter, transformtoLog(10, config), dBFS, transformtoLog(10, config), 0);
		pl_fline_r(plot->plotter, transformtoLog(100, config), dBFS, transformtoLog(100, config), 0);
	}
	pl_fline_r(plot->plotter, transformtoLog(1000, config), dBFS, transformtoLog(1000, config), 0);
	if(config->endHzPlot >= 10000)
	{
		for(int i = 10000; i < config->endHzPlot; i+= 10000)
			pl_fline_r(plot->plotter, transformtoLog(i, config), dBFS, transformtoLog(i, config), 0);	
	}

	pl_pencolor_r (plot->plotter, 0, 0xFFFF, 0);
	pl_flinewidth_r(plot->plotter, 1);
	pl_endpath_r(plot->plotter);
}

void DrawLabelsZeroDBCentered(PlotFile *plot, double dBFS, double dbIncrement, double hz, double hzIncrement,  parameters *config)
{
	double segments = 0;
	char label[20];

	if(fabs(dBFS) <= 1.0)
		dbIncrement = fabs(dBFS)/10.0;
	else
	{
		if(fabs(dBFS) <= 3.0)
			dbIncrement = 1.0;
	}

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0-X0BORDER*config->plotResX*plot->leftmargin, -1*config->plotResY/2-Y0BORDER*config->plotResY, config->plotResX+X1BORDER*config->plotResX, config->plotResY/2+Y1BORDER*config->plotResY);

	pl_ffontname_r(plot->plotter, PLOT_FONT);
	pl_ffontsize_r(plot->plotter, FONT_SIZE_1);

	if(config->maxDbPlotZC == DB_HEIGHT)
		pl_pencolor_r(plot->plotter, 0, 0xffff, 0);
	else
		pl_pencolor_r(plot->plotter, 0xffff, 0xffff, 0);
	pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, config->plotResY/100);
	pl_alabel_r(plot->plotter, 'l', 't', "0dBFS");

	if(dBFS < PCM_16BIT_MIN_AMPLITUDE)
		dbIncrement *= 2;

	segments = fabs(dBFS/dbIncrement);
	for(int i = 1; i <= segments; i ++)
	{
		pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, i*config->plotResY/segments/2+config->plotResY/100);
		sprintf(label, " %gdBFS", i*dbIncrement);
		pl_alabel_r(plot->plotter, 'l', 't', label);

		pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, -1*i*config->plotResY/segments/2+config->plotResY/100);
		sprintf(label, "-%gdBFS", i*dbIncrement);
		pl_alabel_r(plot->plotter, 'l', 't', label);
	}

	/* Frequency scale */
	pl_pencolor_r(plot->plotter, 0, 0xaaaa, 0);
	if(config->logScale)
	{
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(10, config), config->plotResY/2);
		sprintf(label, "%dHz", 10);
		pl_alabel_r(plot->plotter, 'c', 'b', label);
	
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(100, config), config->plotResY/2);
		sprintf(label, "%dHz", 100);
		pl_alabel_r(plot->plotter, 'c', 'b', label);
	}

	pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(1000, config), config->plotResY/2);
	sprintf(label, "  %dHz", 1000);
	pl_alabel_r(plot->plotter, 'c', 'b', label);

	if(config->endHzPlot >= 10000)
	{
		for(int i = 10000; i < config->endHzPlot; i+= 10000)
		{
			pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(i, config), config->plotResY/2);
			sprintf(label, "%d%s", i/1000, i >= 40000  ? "" : "khz");
			pl_alabel_r(plot->plotter, 'c', 'b', label);
		}
	}

	pl_restorestate_r(plot->plotter);
}

#ifdef TESTWARNINGS
void enableTestWarnings(parameters *config)
{
	config->frequencyNormalizationTries = 5;
	config->frequencyNormalizationTolerant = 1.5;

	config->ignoreFrameRateDiff = 1;
	config->ignoreFloor = 1;
	config->noiseFloorTooHigh = 1;

	config->types.useWatermark = 1;
	config->referenceSignal->watermarkStatus = WATERMARK_VALID;
	config->comparisonSignal->watermarkStatus = WATERMARK_INVALID;
	sprintf(config->types.watermarkDisplayName, "Testing");

	config->AmpBarRange = 2;
	config->smallFile = ROLE_REF;

	config->normType = none;
	config->noSyncProfile = 1;
	config->syncTolerance = 1;

	config->channel = 'l';

	config->hasAddOnData = 1;
	config->quantizeRound = 0;

	config->ZeroPad = 1;

	config->channelBalance = 0;
	config->referenceSignal->AudioChannels = 2;

	/*
	config->startHz = START_HZ+1;
	config->endHz = END_HZ-1;
	*/

	config->MaxFreq = FREQ_COUNT/2;

	config->logScale = 0;

	config->outputFilterFunction = 1;

	config->compressToBlocks = 1;
	config->channelWithLowFundamentals = 1;
	config->singleSyncUsed = 1;

	config->maxDbPlotZC = DB_HEIGHT+3;
	config->notVisible = 20.75;

	config->clkProcess = 'y';

	config->clkBlock = 2;
	config->clkFreq = 8000;
	config->clkFreqCount = 10;
	config->clkAmpl = 1.2;
	config->clkRatio = 4;
		
	config->referenceSignal->balance = -5.7;
	config->comparisonSignal->balance = 10.48;

	logmsg("ERROR: enableTestWarnings Enabled\n");
}
#endif

#define PLOT_COLUMN(x,y) pl_fmove_r(plot->plotter, config->plotResX-x*config->plotResX/10-config->plotResX/40, config->plotResY/2-y*BAR_HEIGHT)

void DrawLabelsMDF(PlotFile *plot, char *Gname, char *GType, int type, parameters *config)
{
	char	label[BUFFER_SIZE], msg[BUFFER_SIZE];
#ifdef TESTWARNINGS
	parameters backup;

	backup = *config;
	enableTestWarnings(config);
#endif

	pl_ffontsize_r(plot->plotter, FONT_SIZE_2);
	pl_ffontname_r(plot->plotter, PLOT_FONT);

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0, -1*config->plotResY/2, config->plotResX, config->plotResY/2);

	/* Profile */
	pl_fmove_r(plot->plotter, config->plotResX/40, config->plotResY/2-config->plotResY/30+BAR_HEIGHT/2);
	pl_pencolor_r(plot->plotter, 0xaaaa, 0xaaaa, 0xaaaa);
	pl_alabel_r(plot->plotter, 'l', 'l', config->types.Name);

	/* Plot Label */
	sprintf(label, Gname, GType);
	pl_fmove_r(plot->plotter, config->plotResX/40, config->plotResY/2-config->plotResY/30-BAR_HEIGHT);
	pl_pencolor_r(plot->plotter, 0xcccc, 0xcccc, 0xcccc);
	pl_alabel_r(plot->plotter, 'l', 'l', label);

	/* Version */
	pl_ffontsize_r(plot->plotter, FONT_SIZE_3);
	pl_fmove_r(plot->plotter, config->plotResX/60, -1*config->plotResY/2+config->plotResY/100);
	pl_pencolor_r(plot->plotter, 0, 0xcccc, 0);
	pl_alabel_r(plot->plotter, 'l', 'l', "MDFourier "MDVERSION" for 240p Test Suite by Artemio Urbina");
	pl_ffontsize_r(plot->plotter, FONT_SIZE_2);

	/* Window */
	pl_fmove_r(plot->plotter, config->plotResX/20*19, -1*config->plotResY/2+config->plotResY/80);
	pl_pencolor_r(plot->plotter, 0xaaaa, 0xaaaa, 0xaaaa);
	switch(config->window)
	{
		case 'n':
			pl_alabel_r(plot->plotter, 'l', 'l', "Rectangle");
			break;
		case 't':
			pl_alabel_r(plot->plotter, 'l', 'l', "Tukey");
			break;
		case 'f':
			pl_alabel_r(plot->plotter, 'l', 'l', "Flattop");
			break;
		case 'h':
			pl_alabel_r(plot->plotter, 'l', 'l', "Hann");
			break;
		case 'm':
			pl_alabel_r(plot->plotter, 'l', 'l', "Hamming");
			break;
		default:
			pl_alabel_r(plot->plotter, 'l', 'l', "UNKNOWN");
			break;
	}

	/* Subpar Frequency domain nmormalization */
	if(config->frequencyNormalizationTries)
	{
		double width = 0;

		width = pl_flabelwidth_r(plot->plotter, "Rectangle ");
		pl_fmove_r(plot->plotter, config->plotResX/20*19+width, -1*config->plotResY/2+config->plotResY/80);
		pl_pencolor_r(plot->plotter, 0xaaaa, 0xaaaa, 0);
		sprintf(msg, "N%d", config->frequencyNormalizationTries);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
		if(config->frequencyNormalizationTolerant != 0)
		{
			pl_fmove_r(plot->plotter, config->plotResX/20*19+width, -1*config->plotResY/2+2*config->plotResY/80);
			sprintf(msg, "b:%g", config->frequencyNormalizationTolerant);
			pl_alabel_r(plot->plotter, 'c', 'l', msg);
		}
	}

	/* File information */
	if(config->labelNames)
	{
		double x = 0, y = 0;

		x = config->plotResX/2-config->plotResX/50*14;
		y = -1*config->plotResY/2+config->plotResY/80;
		pl_pencolor_r(plot->plotter, 0, 0xeeee, 0);

		if(type == PLOT_COMPARE)
		{
			if(config->referenceSignal)
			{
				sprintf(label, "Reference:   %5.5s %4dkHz %s %.110s%s",
					config->types.SyncFormat[config->videoFormatRef].syncName,
					config->referenceSignal->header.fmt.SamplesPerSec/1000,
					config->referenceSignal->AudioChannels == 2 ? "Stereo" : "Mono  ",
					basename(config->referenceFile),
					strlen(basename(config->referenceFile)) > 110 ? "\\.." : " ");
				pl_fmove_r(plot->plotter, x, y+config->plotResY/40);
				pl_alabel_r(plot->plotter, 'l', 'l', label);

				sprintf(label, "[%0.4fms %0.4fHz]", 
						config->referenceSignal->framerate, roundFloat(CalculateScanRate(config->referenceSignal)));
				pl_fmove_r(plot->plotter, config->plotResX/20*17, y+config->plotResY/40);
				pl_alabel_r(plot->plotter, 'l', 'l', label);
			}
			else
			{
				sprintf(label, "Reference:   %.115s", basename(config->referenceFile));
				pl_fmove_r(plot->plotter, x, y+config->plotResY/40);
				pl_alabel_r(plot->plotter, 'l', 'l', label);
			}
			if(config->comparisonSignal)
			{
				sprintf(label, "Comparison: %5.5s %4dkHz %s %.110s%s",
					config->types.SyncFormat[config->videoFormatCom].syncName,
					config->comparisonSignal->header.fmt.SamplesPerSec/1000,
					config->comparisonSignal->AudioChannels == 2 ? "Stereo" : "Mono  ",
					basename(config->comparisonFile),
					strlen(basename(config->comparisonFile)) > 110 ? "\\.." : " ");
				pl_fmove_r(plot->plotter, x, -1*config->plotResY/2+config->plotResY/80);
				pl_alabel_r(plot->plotter, 'l', 'l', label);

				sprintf(label, "[%0.4fms %0.4fHz]", 
						config->comparisonSignal->framerate, roundFloat(CalculateScanRate(config->comparisonSignal)));
				pl_fmove_r(plot->plotter, config->plotResX/20*17, y);
				pl_alabel_r(plot->plotter, 'l', 'l', label);
			}
			else
			{
				sprintf(label, "Comparison: %.115s", basename(config->comparisonFile));
				pl_fmove_r(plot->plotter, x, y);
				pl_alabel_r(plot->plotter, 'l', 'l', label);
			}
		}
		else
		{
			int format = 0;

			y += config->plotResY/60;
			if(type == PLOT_SINGLE_REF)
			{
				format = config->videoFormatRef;
				sprintf(label, "File: %5.5s %4dkHz %s %.110s%s",
					config->types.SyncFormat[format].syncName,
					config->referenceSignal->header.fmt.SamplesPerSec/1000,
					config->referenceSignal->AudioChannels == 2 ? "Stereo" : "Mono  ",
					basename(config->referenceFile),
					strlen(basename(config->referenceFile)) > 110 ? "\\.." : " ");
			}
			else
			{
				format = config->videoFormatCom;
				sprintf(label, "File: %5.5s %4dkHz %s %.110s%s",
					config->types.SyncFormat[format].syncName,
					config->comparisonSignal->header.fmt.SamplesPerSec/1000,
					config->comparisonSignal->AudioChannels == 2 ? "Stereo" : "Mono  ",
					basename(config->comparisonFile),
					strlen(basename(config->comparisonFile)) > 110 ? "\\.." : " ");
			}
	
			pl_fmove_r(plot->plotter, x, y);
			pl_alabel_r(plot->plotter, 'l', 'l', label);

			if(type == PLOT_SINGLE_REF)
				sprintf(label, "[%0.4fms %0.4fHz]", config->referenceSignal->framerate, roundFloat(CalculateScanRate(config->referenceSignal)));
			else
				sprintf(label, "[%0.4fms %0.4fHz]", config->comparisonSignal->framerate, roundFloat(CalculateScanRate(config->comparisonSignal)));
			pl_fmove_r(plot->plotter, config->plotResX/20*17, y);
			pl_alabel_r(plot->plotter, 'l', 'l', label);
		}
	}

	/* Warnings */
	// Add from top, change last multiplier for BAR HEIGHT

	if(config->compressToBlocks)
	{
		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/4, -1*config->plotResY/2+config->plotResY/20+12*BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
		pl_alabel_r(plot->plotter, 'l', 'l', "WARNING: Debug setting, blocks flattened");
	}

	if(!config->logScale)
	{
		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/4, -1*config->plotResY/2+config->plotResY/20+11*BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
		pl_alabel_r(plot->plotter, 'l', 'l', "WARNING: Log scale disabled");
	}

	if(config->channelBalance == 0 &&
		(config->referenceSignal->AudioChannels == 2 ||
		config->comparisonSignal->AudioChannels == 2))
	{
		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/4, -1*config->plotResY/2+config->plotResY/20+10*BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
		pl_alabel_r(plot->plotter, 'l', 'l', "WARNING: Audio channel balancing disabled");
	}

	if(config->ignoreFrameRateDiff)
	{
		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/4, -1*config->plotResY/2+config->plotResY/20+9*BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
		pl_alabel_r(plot->plotter, 'l', 'l', "WARNING: Ignored frame rate difference during analysis");
	}

	if(config->ignoreFloor)
	{
		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/4, -1*config->plotResY/2+config->plotResY/20+8*BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
		pl_alabel_r(plot->plotter, 'l', 'l', "WARNING: Noise floor was ignored during analysis");
	}

	if(config->noiseFloorTooHigh)
	{
		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/4, -1*config->plotResY/2+config->plotResY/20+7*BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
		pl_alabel_r(plot->plotter, 'l', 'l', "WARNING: Noise floor too high");
	}

	if(config->types.useWatermark && DetectWatermarkIssue(msg, config))
	{
		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/4, -1*config->plotResY/2+config->plotResY/20+6*BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if(config->AmpBarRange > BAR_DIFF_DB_TOLERANCE)
	{
		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/4, -1*config->plotResY/2+config->plotResY/20+5*BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
		pl_alabel_r(plot->plotter, 'l', 'l', "WARNING: Tolerance raised for matches");
	}

	if(config->smallFile)
	{
		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/4, -1*config->plotResY/2+config->plotResY/20+4*BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
		sprintf(msg, "WARNING: %s file%s shorter than expected", 
			config->smallFile == ROLE_REF ? "Reference" : config->smallFile == ROLE_COMP ? "Comparison" : "Both",
			config->smallFile == (ROLE_REF | ROLE_COMP) ? "s were" : " was");
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if(config->normType != max_frequency)
	{
		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/4, -1*config->plotResY/2+config->plotResY/20+3*BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
		if(config->normType == max_time)
			pl_alabel_r(plot->plotter, 'l', 'l', "Time domain normalization");
		if(config->normType == average)
			pl_alabel_r(plot->plotter, 'l', 'l', "Averaged normalization");
		if(config->normType == none)
			pl_alabel_r(plot->plotter, 'l', 'l', "WARNING: No Normalization, PLEASE DISREGARD");
	}

	if(config->noSyncProfile && type < PLOT_SINGLE_REF)
	{
		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/4, -1*config->plotResY/2+config->plotResY/20+2*BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
		sprintf(msg, "WARNING: No sync profile [%s], PLEASE DISREGARD", 
					config->noSyncProfileType == NO_SYNC_AUTO ? "Auto" : "Manual");
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if(config->syncTolerance)
	{
		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/4, -1*config->plotResY/2+config->plotResY/20+1*BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
		pl_alabel_r(plot->plotter, 'l', 'l', "WARNING: Sync tolerance enabled");
	}

	/* Top messages */

	pl_pencolor_r(plot->plotter, 0, 0xcccc, 0);
	{
		PLOT_COLUMN(1, 1);
		if(config->significantAmplitude > LOWEST_NOISEFLOOR_ALLOWED || config->ignoreFloor)
			pl_pencolor_r(plot->plotter, 0xcccc, 0xcccc, 0);
		sprintf(msg, "Significant: %0.1f dBFS", config->significantAmplitude);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	pl_pencolor_r(plot->plotter, 0, 0xcccc, 0);
	//if(config->startHz != START_HZ || config->endHz != END_HZ)
	{
		PLOT_COLUMN(1, 2);
		sprintf(msg, "Range: %g%s-%g%s", 
				config->startHz >= 1000 ? config->startHz/1000.0 : config->startHz,
				config->startHz >= 1000 ? "khz" : "hz",
				config->endHz >= 1000 ? config->endHz/1000.0 : config->endHz,
				config->endHz >= 1000 ? "khz" : "hz");
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	/* Noise floor */
	pl_pencolor_r(plot->plotter, 0xcccc, 0xcccc, 0xcccc);
	if(config->referenceSignal && (type == PLOT_COMPARE || type == PLOT_SINGLE_REF))
	{
		if(config->referenceSignal->gridAmplitude)
		{
			PLOT_COLUMN(3, 1);
			sprintf(msg, "Ref %0.1fhz:  %0.1fdBFS", 
					config->referenceSignal->gridFrequency,
					config->referenceSignal->gridAmplitude);
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}
	
		if(config->referenceSignal->scanrateAmplitude)
		{
			PLOT_COLUMN(3, 2);
			sprintf(msg, "Ref %0.1fkhz: %0.1fdBFS", 
					config->referenceSignal->scanrateFrequency/1000.0,
					config->referenceSignal->scanrateAmplitude);
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}
	}

	if(config->comparisonSignal && (type == PLOT_COMPARE || type == PLOT_SINGLE_COM))
	{
		if(config->comparisonSignal->gridAmplitude)
		{
			PLOT_COLUMN(2, 1);
			sprintf(msg, "Com %0.1fhz: %0.1fdBFS", 
					config->comparisonSignal->gridFrequency,
					config->comparisonSignal->gridAmplitude);
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}
	
		if(config->comparisonSignal->scanrateAmplitude)
		{
			PLOT_COLUMN(2, 2);
			sprintf(msg, "Com %0.1fkhz: %0.1fdBFS", 
					config->comparisonSignal->scanrateFrequency/1000.0,
					config->comparisonSignal->scanrateAmplitude);
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}
	}

	if(config->hasAddOnData)
	{
		PLOT_COLUMN(1, 3);
		if(config->useExtraData)
			pl_alabel_r(plot->plotter, 'l', 'l', "Extra Data: ON");
		else
			pl_alabel_r(plot->plotter, 'l', 'l', "Extra Data: OFF");
	}

	/* Aligned to 1hz */
	pl_pencolor_r(plot->plotter, 0, 0xcccc, 0xcccc);
	if(config->ZeroPad)
	{
		PLOT_COLUMN(2, 3);
		pl_alabel_r(plot->plotter, 'l', 'l', "1Hz Aligned");
	}

	if(config->outputFilterFunction != 3)
	{
		char * filer[6] = {"None", 	"Bright", "High", "Neutral", "Low", "Dimm" };

		PLOT_COLUMN(3, 3);
		sprintf(msg, "Color function: %s", filer[config->outputFilterFunction]);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if(config->channel != 's' && (config->referenceSignal->AudioChannels == 2 || config->comparisonSignal->AudioChannels == 2))
	{
		PLOT_COLUMN(4, 1);
		if(config->channel == 'l')
			pl_alabel_r(plot->plotter, 'l', 'l', "Left Channel");
		if(config->channel == 'r')
			pl_alabel_r(plot->plotter, 'l', 'l', "Right Channel");
	}

	if(config->MaxFreq != FREQ_COUNT)
	{
		PLOT_COLUMN(4, 2);
		sprintf(msg, "Frequencies/note: %d", config->MaxFreq);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if(!config->quantizeRound)
	{
		PLOT_COLUMN(4, 3);
		pl_alabel_r(plot->plotter, 'l', 'l', "Quantization: OFF");
	}

	if(config->channelWithLowFundamentals)
	{
		PLOT_COLUMN(5, 1);
		pl_alabel_r(plot->plotter, 'l', 'l', "Low Fundamentals present");
	}

	if(config->singleSyncUsed)
	{
		PLOT_COLUMN(5, 2);
		pl_alabel_r(plot->plotter, 'l', 'l', "Single precision sync used");
	}
	
	if(config->maxDbPlotZC != DB_HEIGHT)
	{
		PLOT_COLUMN(5, 3);
		pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
		pl_alabel_r(plot->plotter, 'l', 'l', "Vertical scale changed");
	}

	if(config->clkProcess == 'y')
	{
		pl_pencolor_r(plot->plotter, 0, 0xcccc, 0xcccc);
		if(type == PLOT_COMPARE)
		{
			PLOT_COLUMN(6, 1);
			sprintf(msg, "CLK R: %g Hz", CalculateClk(config->referenceSignal, config));
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
			PLOT_COLUMN(6, 2);
			sprintf(msg, "CLK C: %g Hz", CalculateClk(config->comparisonSignal, config));
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}
	}

	if(config->notVisible > 1)
	{
		PLOT_COLUMN(6, 3);
		if(config->notVisible > 5)
			pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
		else
			pl_pencolor_r(plot->plotter, 0, 0xcccc, 0xcccc);
		sprintf(msg, "Data \\ua\\da %gdBFS: %0.2f%%", config->maxDbPlotZC, config->notVisible);
		pl_alabel_r(plot->plotter, 'l', 'l', msg);
	}

	if(type == PLOT_COMPARE)
	{
		if(config->referenceSignal->balance)
		{
			PLOT_COLUMN(7, 1);
			sprintf(msg, "Imbalance R [%s]: %0.2f%%", 
				config->referenceSignal->balance > 0 ? "R" : "L", 
				fabs(config->referenceSignal->balance));
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}
		if(config->comparisonSignal->balance)
		{
			PLOT_COLUMN(7, 2);
			sprintf(msg, "Imbalance C [%s]: %0.2f%%", 
				config->comparisonSignal->balance > 0 ? "R" : "L", 
				fabs(config->comparisonSignal->balance));
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}
	}
	else
	{
		if(type == PLOT_SINGLE_REF)
		{
			PLOT_COLUMN(7, 1);
			sprintf(msg, "Imbalance [%s]: %0.2f%%", 
				config->referenceSignal->balance > 0 ? "R" : "L", 
				fabs(config->referenceSignal->balance));
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}
		else
		{
			PLOT_COLUMN(7, 2);
			sprintf(msg, "Imbalance [%s]: %0.2f%%", 
				config->comparisonSignal->balance > 0 ? "R" : "L", 
				fabs(config->comparisonSignal->balance));
			pl_alabel_r(plot->plotter, 'l', 'l', msg);
		}
	}

	pl_restorestate_r(plot->plotter);

#ifdef TESTWARNINGS
	*config = backup;
#endif
}

void DrawLabelsZeroToLimit(PlotFile *plot, double dBFS, double dbIncrement, double hz, double hzIncrement, int drawSignificant, parameters *config)
{
	double segments = 0;
	char label[100];

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0-X0BORDER*config->plotResX*plot->leftmargin, -1*config->plotResY-Y0BORDER*config->plotResY, config->plotResX+X1BORDER*config->plotResX, 0+Y1BORDER*config->plotResY);
	pl_pencolor_r(plot->plotter, 0, 0xaaaa, 0);
	pl_ffontsize_r(plot->plotter, FONT_SIZE_1);

	if(fabs(dBFS) < PCM_16BIT_MIN_AMPLITUDE)
		dbIncrement *= 2;

	pl_ffontname_r(plot->plotter, PLOT_FONT);
	segments = ceil(fabs(dBFS/dbIncrement));
	for(int i = 0; i <= segments; i ++)
	{
		pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, -1*i*config->plotResY/segments);
		sprintf(label, "%gdBFS", -1*i*dbIncrement);
		pl_alabel_r(plot->plotter, 'l', 'c', label);
	}

	if(drawSignificant)
	{
		double labelwidth = 0;

		labelwidth = pl_flabelwidth_r(plot->plotter, "\\ua XXXXXXXXX");

		pl_fmove_r(plot->plotter, -1*labelwidth-PLOT_SPACER, -1*config->plotResY/fabs(dBFS)*fabs(config->significantAmplitude));
		pl_pencolor_r (plot->plotter, 0x9999, 0x9999, 0);
		pl_alabel_r(plot->plotter, 'l', 'c', "Significant");

		pl_fmove_r(plot->plotter, -1*labelwidth-PLOT_SPACER, -1*config->plotResY/fabs(dBFS)*fabs(config->significantAmplitude)+1.5*BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0, 0xaaaa, 0);
		pl_alabel_r(plot->plotter, 'l', 'c', "\\ua Analyzed");

		pl_fmove_r(plot->plotter, -1*labelwidth-PLOT_SPACER, -1*config->plotResY/fabs(dBFS)*fabs(config->significantAmplitude)-1.5*BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0xaaaa, 0, 0);
		pl_alabel_r(plot->plotter, 'l', 'c', "\\da Discarded");

		pl_pencolor_r(plot->plotter, 0, 0xaaaa, 0);
	}

	if(config->logScale)
	{
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(10, config), 0);
		sprintf(label, "%dHz", 10);
		pl_alabel_r(plot->plotter, 'c', 'b', label);
	
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(100, config), 0);
		sprintf(label, "%dHz", 100);
		pl_alabel_r(plot->plotter, 'c', 'b', label);
	}

	pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(1000, config), 0);
	sprintf(label, "  %dHz", 1000);
	pl_alabel_r(plot->plotter, 'c', 'b', label);

	if(config->endHzPlot >= 10000)
	{
		for(int i = 10000; i < config->endHzPlot; i+= 10000)
		{
			pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(i, config), 0);
			sprintf(label, "%d%s", i/1000, i > 40000 ? "" : "khz");
			pl_alabel_r(plot->plotter, 'c', 'b', label);
		}
	}

	pl_restorestate_r(plot->plotter);
}

void DrawColorScale(PlotFile *plot, int type, int mode, double x, double y, double width, double height, double startDbs, double endDbs, double dbIncrement, parameters *config)
{
	double		segments = 0;
	char 		*label = NULL;
	int 		colorName = 0;
	long int	cnt = 0, cmp = 0;
	double		labelwidth = 0, maxlabel = 0;

	label = GetTypeDisplayName(config, type);
	colorName = MatchColor(GetTypeColor(config, type));

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0, 0, config->plotResX, config->plotResY);
	pl_filltype_r(plot->plotter, 1);

	segments = floor(fabs(endDbs/dbIncrement));
	for(double i = 0; i < segments; i ++)
	{
		long int intensity;

		intensity = CalculateWeightedError(i/segments, config)*0xffff;

		SetPenColor(colorName, intensity, plot);
		SetFillColor(colorName, intensity, plot);
		pl_fbox_r(plot->plotter, x, y+i*height/segments, x+width, y+i*height/segments+height/segments);
		pl_endsubpath_r(plot->plotter);
	}

	pl_pencolor_r(plot->plotter, 0xaaaa, 0xaaaa, 0xaaaa);
	pl_filltype_r(plot->plotter, 0);
	pl_fbox_r(plot->plotter, x, y, x+width, y+height);

	SetPenColor(colorName, 0xaaaa, plot);
	pl_ffontsize_r(plot->plotter, FONT_SIZE_2);
	pl_ffontname_r(plot->plotter, PLOT_FONT);

	/* dBFS label */

	pl_fmove_r(plot->plotter, x+width/2, y-FONT_SIZE_2);
	pl_alabel_r(plot->plotter, 'c', 'c', "dBFS");

	for(double i = 0; i < segments; i++)
	{
		char labeldbs[20];

		pl_fmove_r(plot->plotter, x+width+PLOT_SPACER, y+height-i*height/segments-height/segments/2);
		sprintf(labeldbs, "%c%g", fabs(startDbs) + i*dbIncrement != 0 ? '-' : ' ', fabs(startDbs) + i*dbIncrement);
		pl_alabel_r(plot->plotter, 'l', 'c', labeldbs);

		labelwidth = pl_flabelwidth_r(plot->plotter, label);
		if(maxlabel < labelwidth)
			maxlabel = labelwidth;	
	}

	x = x+width+maxlabel+FONT_SIZE_1/2;
	labelwidth = 0;
	maxlabel = 0;

	SetPenColor(colorName, 0xaaaa, plot);
	pl_fmove_r(plot->plotter, x, y);
	pl_alabel_r(plot->plotter, 'l', 'l', label);
	labelwidth = pl_flabelwidth_r(plot->plotter, label);

	if(mode != MODE_SPEC)
	{
		double bar_text_width = 0;

		if(mode == MODE_DIFF)
		{
			FindDifferenceTypeTotals(type, &cnt, &cmp, config);

			SetPenColor(COLOR_GRAY, 0xaaaa, plot);
			pl_fmove_r(plot->plotter, x, y+1.5*BAR_HEIGHT);
			pl_alabel_r(plot->plotter, 'l', 'l', BAR_DIFF);
		}
		if(mode == MODE_MISS)
		{
			FindMissingTypeTotals(type, &cnt, &cmp, config);
		}
		bar_text_width = DrawMatchBar(plot, colorName,
			x+labelwidth+BAR_WIDTH*0.2, y,
			BAR_WIDTH, BAR_HEIGHT, 
			(double)cnt, (double)cmp, config);
		if(mode == MODE_DIFF)
		{
			int 	x_offset = 0;
			char	header[100];

			x_offset = x+labelwidth+BAR_WIDTH + bar_text_width;

			FindDifferenceWithinInterval(type, &cnt, &cmp, config->AmpBarRange, config);

			if(config->AmpBarRange > BAR_DIFF_DB_TOLERANCE)
				SetPenColor(COLOR_YELLOW, 0xaaaa, plot);
			else
				SetPenColor(COLOR_GRAY, 0xaaaa, plot);
			pl_fmove_r(plot->plotter, 1.1*x_offset, y+1.5*BAR_HEIGHT);
			if(!config->drawPerfect)
				sprintf(header, BAR_WITHIN, config->AmpBarRange);
			else
				sprintf(header, BAR_WITHIN_PERFECT, config->AmpBarRange);
			pl_alabel_r(plot->plotter, 'l', 'l', header);

			SetPenColor(COLOR_GRAY, 0xaaaa, plot);
			pl_fmove_r(plot->plotter, x_offset, y+3*BAR_HEIGHT);
			if(!config->drawPerfect)
				pl_alabel_r(plot->plotter, 'c', 'c', BAR_HEADER);
			else
				pl_alabel_r(plot->plotter, 'l', 'c', BAR_HEADER);


			bar_text_width = DrawMatchBar(plot, colorName,
				1.1*x_offset, y,
				BAR_WIDTH, BAR_HEIGHT, 
				(double)cnt, (double)cmp, config);

			/* Perfect */
			if(config->drawPerfect)
			{
				x_offset = x_offset + BAR_WIDTH + bar_text_width;
	
				FindPerfectMatches(type, &cnt, &cmp, config);
	
				SetPenColor(COLOR_GRAY, 0xaaaa, plot);
				pl_fmove_r(plot->plotter, 1.1*x_offset, y+1.5*BAR_HEIGHT);
				sprintf(header, BAR_PERFECT);
				pl_alabel_r(plot->plotter, 'l', 'l', header);
	
				DrawMatchBar(plot, colorName,
					1.1*x_offset, y,
					BAR_WIDTH, BAR_HEIGHT, 
					(double)cnt, (double)cmp, config);
			}
		}
	}
	pl_restorestate_r(plot->plotter);
}

void DrawColorAllTypeScale(PlotFile *plot, int mode, double x, double y, double width, double height, double endDbs, double dbIncrement, int drawBars, parameters *config)
{
	double 	segments = 0, maxlabel = 0, maxbarwidth = 0;
	int		*colorName = NULL, *typeID = NULL;
	int		numTypes = 0, t = 0, maxMatch = 0;

	numTypes = GetActiveBlockTypesNoRepeat(config);
	segments = floor(fabs(endDbs/dbIncrement));

	width *= numTypes;
	colorName = (int*)malloc(sizeof(int)*numTypes);
	if(!colorName)
		return;
	typeID = (int*)malloc(sizeof(int)*numTypes);
	if(!typeID)
	{
		free(colorName);
		return;
	}

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0, 0, config->plotResX, config->plotResY);
	pl_filltype_r(plot->plotter, 1);

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type > TYPE_CONTROL && !config->types.typeArray[i].IsaddOnData)
		{
			colorName[t] = MatchColor(GetTypeColor(config, config->types.typeArray[i].type));
			typeID[t] = config->types.typeArray[i].type;
			t++;
		}
	}

	pl_ffontsize_r(plot->plotter, FONT_SIZE_2);
	pl_ffontname_r(plot->plotter, PLOT_FONT);

	if(drawBars == DRAW_BARS)
	{
		for(double i = 0; i < segments; i ++)
		{
			long int intensity;
	
			if(mode != MODE_TSDIFF)
				intensity = CalculateWeightedError(i/segments, config)*0xffff;
			else
				intensity = 0xffff*CalculateWeightedError(1.0 - i/segments, config);
	
			for(int t = 0; t < numTypes; t++)
			{
				double bx, by;
	
				bx = x+(double)t*width/(double)numTypes;
				by = y+i*height/segments;
				SetPenColor(colorName[t], intensity, plot);
				SetFillColor(colorName[t], intensity, plot);
				pl_fbox_r(plot->plotter, bx, by, bx+width/(double)numTypes, by+height/segments);
				pl_endsubpath_r(plot->plotter);
			}
		}

		pl_pencolor_r(plot->plotter, 0xaaaa, 0xaaaa, 0xaaaa);
		pl_filltype_r(plot->plotter, 0);
		pl_fbox_r(plot->plotter, x, y, x+width, y+height);
	
		SetPenColor(COLOR_GRAY, 0xaaaa, plot);

		/* dBFS label */
	
		pl_fmove_r(plot->plotter, x+width/2, y-FONT_SIZE_2);
		pl_alabel_r(plot->plotter, 'c', 'c', "dBFS");

		for(double i = 0; i < segments; i++)
		{
			char label[20];
			double	labelwidth = 0;
	
			pl_fmove_r(plot->plotter, x+width+PLOT_SPACER, y+height-i*height/segments-height/segments/2);
			if(mode != MODE_TSDIFF)
				sprintf(label, "%c%g", i*dbIncrement > 0 ? '-' : ' ', i*dbIncrement);
			else
				sprintf(label, "%s%g", i ? "\\+-" : "", fabs(i*dbIncrement));
			pl_alabel_r(plot->plotter, 'l', 'c', label);
	
			labelwidth = pl_flabelwidth_r(plot->plotter, label);
			if(maxlabel < labelwidth)
				maxlabel = labelwidth;	
		}
	}

	x = x+width+maxlabel+FONT_SIZE_1/2;
	maxlabel = 0;

	for(int t = 0; t < numTypes; t++)
	{
		double	labelwidth = 0;
		char 	*label = NULL;

		label = GetTypeDisplayName(config, typeID[t]);
		SetPenColor(colorName[t], 0xaaaa, plot);
		pl_fmove_r(plot->plotter, x, y+(numTypes-1)*config->plotResY/50-t*config->plotResY/50);
		pl_alabel_r(plot->plotter, 'l', 'l', label);

		labelwidth = pl_flabelwidth_r(plot->plotter, label);
		if(maxlabel < labelwidth)
			maxlabel = labelwidth;
	}

	if(mode != MODE_SPEC && mode != MODE_TSDIFF)
	{
		// Draw label if needed
		if(mode == MODE_DIFF)
		{
			SetPenColor(COLOR_GRAY, 0xaaaa, plot);
			pl_fmove_r(plot->plotter, x, y+(numTypes-1)*config->plotResY/50+1.5*BAR_HEIGHT);
			pl_alabel_r(plot->plotter, 'l', 'l', BAR_DIFF);
		}

		for(int t = 0; t < numTypes; t++)
		{
			double		barwidth = 0;
			long int	cnt = 0, cmp = 0;
	
			if(mode == MODE_DIFF)
				FindDifferenceTypeTotals(typeID[t], &cnt, &cmp, config);
			if(mode == MODE_MISS)
				FindMissingTypeTotals(typeID[t], &cnt, &cmp, config);
			barwidth = DrawMatchBar(plot, colorName[t],
					x+maxlabel+BAR_WIDTH*0.2, y+(numTypes-1)*config->plotResY/50-t*config->plotResY/50,
					BAR_WIDTH, BAR_HEIGHT, 
					(double)cnt, (double)cmp, config);
			if(barwidth > maxbarwidth)
				maxbarwidth = barwidth;
		}
	}

	// Draw Match within bars
	if(mode == MODE_DIFF)
	{
		long int	cnt = 0, cmp = 0;
		int 		x_offset = 0;
		char		header[100];

		x_offset = x+BAR_WIDTH+maxbarwidth + maxlabel;

		if(config->AmpBarRange > BAR_DIFF_DB_TOLERANCE)
			SetPenColor(COLOR_YELLOW, 0xaaaa, plot);
		else
			SetPenColor(COLOR_GRAY, 0xaaaa, plot);
		pl_fmove_r(plot->plotter, 1.1*x_offset, y+(numTypes-1)*config->plotResY/50+1.5*BAR_HEIGHT);
		if(!config->drawPerfect)
			sprintf(header, BAR_WITHIN, config->AmpBarRange);
		else
			sprintf(header, BAR_WITHIN_PERFECT, config->AmpBarRange);
		pl_alabel_r(plot->plotter, 'l', 'l', header);

		
		SetPenColor(COLOR_GRAY, 0xaaaa, plot);
		pl_fmove_r(plot->plotter, x_offset, y+(numTypes-1)*config->plotResY/50+3*BAR_HEIGHT);
		if(!config->drawPerfect)
			pl_alabel_r(plot->plotter, 'c', 'c', BAR_HEADER);
		else
			pl_alabel_r(plot->plotter, 'l', 'c', BAR_HEADER);

		for(int t = 0; t < numTypes; t++)
		{
			int localMax = 0;

			FindDifferenceWithinInterval(typeID[t], &cnt, &cmp, config->AmpBarRange, config);

			localMax = DrawMatchBar(plot, colorName[t],
				1.1*x_offset, y+(numTypes-1)*config->plotResY/50-t*config->plotResY/50,
				BAR_WIDTH, BAR_HEIGHT, 
				(double)cnt, (double)cmp, config);
			if(localMax > maxMatch)
				maxMatch = localMax;
		}

		if(config->drawPerfect)
		{
			x_offset = x_offset + BAR_WIDTH + maxMatch;

			SetPenColor(COLOR_GRAY, 0xaaaa, plot);
			pl_fmove_r(plot->plotter, 1.1*x_offset, y+(numTypes-1)*config->plotResY/50+1.5*BAR_HEIGHT);
			sprintf(header, BAR_PERFECT);
			pl_alabel_r(plot->plotter, 'l', 'l', header);

			for(int t = 0; t < numTypes; t++)
			{
				FindPerfectMatches(typeID[t], &cnt, &cmp, config);
				DrawMatchBar(plot, colorName[t],
					1.1*x_offset, y+(numTypes-1)*config->plotResY/50-t*config->plotResY/50,
					BAR_WIDTH, BAR_HEIGHT, 
					(double)cnt, (double)cmp, config);
			}
		}
	}

	free(colorName);
	colorName = NULL;
	free(typeID);
	typeID = NULL;

	pl_restorestate_r(plot->plotter);
}

double DrawMatchBar(PlotFile *plot, int colorName, double x, double y, double width, double height, double notFound, double total, parameters *config)
{
	char percent[40];
	double labelwidth = 0, maxlabel = 0;

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0, 0, config->plotResX, config->plotResY);

	// Back
	pl_filltype_r(plot->plotter, 1);
	SetPenColor(COLOR_GRAY, 0x0000, plot);
	SetFillColor(COLOR_GRAY, 0x0000, plot);
	pl_fbox_r(plot->plotter, x, y, x+width, y+height);

	// FG
	pl_filltype_r(plot->plotter, 1);
	SetPenColor(colorName, 0x8888, plot);
	SetFillColor(colorName, 0x8888, plot);
	if(total)
		pl_fbox_r(plot->plotter, x, y, x+(notFound*width/total), y+height);

	// Border
	pl_filltype_r(plot->plotter, 0);
	SetPenColor(COLOR_GRAY, 0x8888, plot);
	pl_fbox_r(plot->plotter, x, y, x+width, y+height);

	pl_filltype_r(plot->plotter, 0);

	// percent
	if(config->showPercent && total)
	{
		pl_ffontsize_r(plot->plotter, FONT_SIZE_2);
		pl_ffontname_r(plot->plotter, PLOT_FONT);

		sprintf(percent, "%5.2f%% of %ld", notFound*100.0/total, (long int)total);

		SetPenColor(colorName, 0x8888, plot);
		pl_fmove_r(plot->plotter, x+width*1.10, y);
		pl_alabel_r(plot->plotter, 'l', 'l', percent);
		labelwidth = pl_flabelwidth_r(plot->plotter, percent);
		if(labelwidth > maxlabel)
			maxlabel = labelwidth;
	}
	pl_restorestate_r(plot->plotter);

	return maxlabel;
}

void DrawNoiseLines(PlotFile *plot, double start, double end, AudioSignal *Signal, parameters *config)
{
	int harmonic = 0;

	pl_pencolor_r (plot->plotter, 0xAAAA, 0xAAAA, 0);
	pl_linemod_r(plot->plotter, "dotdashed");
	if(Signal->gridFrequency)
	{
		for(harmonic = 1; harmonic < 32; harmonic++)
		{
			pl_pencolor_r (plot->plotter, 0xAAAA-0x400*harmonic, 0xAAAA-0x400*harmonic, 0);
			pl_fline_r(plot->plotter, transformtoLog(Signal->gridFrequency*harmonic, config), start, transformtoLog(Signal->gridFrequency*harmonic, config), end);
		}
	}
	if(Signal->scanrateFrequency)
	{
		// Scanrate
		pl_pencolor_r (plot->plotter, 0xAAAA, 0xAAAA, 0);
		pl_fline_r(plot->plotter, transformtoLog(Signal->scanrateFrequency, config), start, transformtoLog(Signal->scanrateFrequency, config), end);
		// Crosstalk
		pl_pencolor_r (plot->plotter, 0xAAAA, 0x8888, 0);
		pl_fline_r(plot->plotter, transformtoLog(Signal->scanrateFrequency/2, config), start, transformtoLog(Signal->scanrateFrequency/2, config), end);
	}
	pl_linemod_r(plot->plotter, "solid");
}

void DrawLabelsNoise(PlotFile *plot, double hz, AudioSignal *Signal, parameters *config)
{
	char label[20];

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0-X0BORDER*config->plotResX*plot->leftmargin, -1*config->plotResY/2-Y0BORDER*config->plotResY, config->plotResX+X1BORDER*config->plotResX, config->plotResY/2+Y1BORDER*config->plotResY);

	pl_ffontname_r(plot->plotter, PLOT_FONT);
	pl_ffontsize_r(plot->plotter, FONT_SIZE_1);

	if(Signal->gridFrequency)
	{
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(Signal->gridFrequency, config), config->plotResY/2+FONT_SIZE_1);
		sprintf(label, "  %0.2fHz", Signal->gridFrequency);
		pl_alabel_r(plot->plotter, 'c', 'b', label);

		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(Signal->gridFrequency*2, config), config->plotResY/2+FONT_SIZE_1);
		sprintf(label, "  %0.2fHz", Signal->gridFrequency*2);
		pl_alabel_r(plot->plotter, 'c', 'b', label);
	}

	if(Signal->scanrateFrequency)
	{
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(Signal->scanrateFrequency, config), config->plotResY/2+FONT_SIZE_1);
		sprintf(label, "  %0.2fkHz", Signal->scanrateFrequency/1000);
		pl_alabel_r(plot->plotter, 'c', 'b', label);

		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(Signal->scanrateFrequency/2, config), config->plotResY/2+FONT_SIZE_1);
		sprintf(label, "  %0.2fkHz", Signal->scanrateFrequency/2000);
		pl_alabel_r(plot->plotter, 'c', 'b', label);
	}

	pl_restorestate_r(plot->plotter);
}

void SaveCSV(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config)
{
	FILE 		*csv = NULL;
	char		name[BUFFER_SIZE];

	if(!config)
		return;

	if(!amplDiff)
		return;

	sprintf(name, "%s.csv", filename);
	
	csv = fopen(name, "wb");
	if(!csv)
		return;
	fprintf(csv, "Type, Frequency(Hz), Diff(dbfs)\n");
	for(int a = 0; a < size; a++)
	{
		if(amplDiff[a].type > TYPE_CONTROL)
		{ 
			//long int intensity;

			// If channel is defined as noise, don't draw the lower than visible ones
			if(amplDiff[a].refAmplitude > config->significantAmplitude)
			{
				//intensity = CalculateWeightedError((fabs(config->significantAmplitude) - fabs(amplDiff[a].refAmplitude))/fabs(config->significantAmplitude), config)*0xffff;
	
				fprintf(csv, "%s, %g,%g\n", GetTypeName(config, amplDiff[a].type), amplDiff[a].hertz, amplDiff[a].diffAmplitude);
			}
		}
	}
	fclose(csv);
}

void PlotAllDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config)
{
	PlotFile	plot;
	char		name[BUFFER_SIZE];
	double		dBFS = config->maxDbPlotZC;

	if(!config)
		return;

	if(!amplDiff)
		return;

	sprintf(name, "DA__ALL_%s", filename);
	FillPlot(&plot, name, config->startHzPlot, -1*dBFS, config->endHzPlot, dBFS, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);

	for(int a = 0; a < size; a++)
	{
		if(amplDiff[a].type > TYPE_CONTROL && fabs(amplDiff[a].diffAmplitude) <= fabs(dBFS))
		{ 
			long int intensity;

			// If channel is defined as noise, don't draw the lower than visible ones
			if(amplDiff[a].refAmplitude > config->significantAmplitude)
			{
				intensity = CalculateWeightedError((fabs(config->significantAmplitude) - fabs(amplDiff[a].refAmplitude))/fabs(config->significantAmplitude), config)*0xffff;
	
				SetPenColor(amplDiff[a].color, intensity, &plot);
				pl_fpoint_r(plot.plotter, transformtoLog(amplDiff[a].hertz, config), amplDiff[a].diffAmplitude);
			}
		}
	}

	DrawColorAllTypeScale(&plot, MODE_DIFF, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, config->significantAmplitude, VERT_SCALE_STEP_BAR, DRAW_BARS, config);
	DrawLabelsMDF(&plot, DIFFERENCE_TITLE, ALL_LABEL, PLOT_COMPARE, config);

	ClosePlot(&plot);
}

int PlotEachTypeDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config)
{
	int 		i = 0, type = 0, types = 0, typeCount = 0;
	char		name[BUFFER_SIZE];

	typeCount = GetActiveBlockTypesNoRepeat(config);
	for(i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;

		if(type > TYPE_CONTROL && !config->types.typeArray[i].IsaddOnData)
		{
			char	*returnFolder = NULL;

			sprintf(name, "DA_%s_%02d%s", filename, 
				type, config->types.typeArray[i].typeName);
		
			if(typeCount > 1)
			{
				returnFolder = PushFolder(DIFFERENCE_FOLDER);
				if(!returnFolder)
					return 0;
			}
			PlotSingleTypeDifferentAmplitudes(amplDiff, size, type, name, config);
			logmsg(PLOT_ADVANCE_CHAR);

			if(typeCount > 1)
				ReturnToMainPath(&returnFolder);

			types ++;
		}
	}
	return types;
}

void PlotSingleTypeDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, int type, char *filename, parameters *config)
{
	PlotFile	plot;
	double		dBFS = config->maxDbPlotZC;

	if(!config)
		return;

	if(!amplDiff)
		return;

	FillPlot(&plot, filename, config->startHzPlot, -1*dBFS, config->endHzPlot, dBFS, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);

	for(int a = 0; a < size; a++)
	{
		if(amplDiff[a].hertz && amplDiff[a].type == type && fabs(amplDiff[a].diffAmplitude) <= fabs(dBFS))
		{ 
			long int intensity;

			intensity = CalculateWeightedError((fabs(config->significantAmplitude) - fabs(amplDiff[a].refAmplitude))/fabs(config->significantAmplitude), config)*0xffff;

			SetPenColor(amplDiff[a].color, intensity, &plot);
			pl_fpoint_r(plot.plotter, transformtoLog(amplDiff[a].hertz, config), amplDiff[a].diffAmplitude);
		}
	}

	DrawColorScale(&plot, type, MODE_DIFF,
		LEFT_MARGIN, HEIGHT_MARGIN, 
		config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15,
		0, config->significantAmplitude, VERT_SCALE_STEP_BAR, config);
	DrawLabelsMDF(&plot, DIFFERENCE_TITLE, GetTypeDisplayName(config, type), PLOT_COMPARE, config);
	ClosePlot(&plot);
}

int PlotNoiseDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config, AudioSignal *Signal)
{
	int 		i = 0, type = 0;
	char		name[BUFFER_SIZE];

	for(i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;
		if(type == TYPE_SILENCE)
		{
			sprintf(name, "NF_%s_%d_%02d%s", filename, i,
				type, config->types.typeArray[i].typeName);
			PlotSilenceBlockDifferentAmplitudes(amplDiff, size, type, name, config, Signal);
			logmsg(PLOT_ADVANCE_CHAR);
			return 1;  // we only plot once
		}
	}
	return 0;
}

void PlotSilenceBlockDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, int type, char *filename, parameters *config, AudioSignal *Signal)
{
	PlotFile	plot;
	double		dBFS = config->maxDbPlotZC;
	double		startAmplitude = config->referenceNoiseFloor, endAmplitude = PCM_16BIT_MIN_AMPLITUDE;

	if(!config)
		return;

	if(!amplDiff)
		return;

	// Find limits
	for(int a = 0; a < size; a++)
	{
		if(amplDiff[a].type == type)
		{
			if(fabs(amplDiff[a].diffAmplitude) > dBFS)
				dBFS = fabs(amplDiff[a].diffAmplitude);
			if(amplDiff[a].refAmplitude > startAmplitude)
				startAmplitude = amplDiff[a].refAmplitude;
			if(amplDiff[a].refAmplitude < endAmplitude)
				endAmplitude = amplDiff[a].refAmplitude;
		}
	}

	if(endAmplitude < NS_LOWEST_AMPLITUDE)
		endAmplitude = NS_LOWEST_AMPLITUDE;

	FillPlot(&plot, filename, config->startHzPlot, -1*dBFS, config->endHzPlot, dBFS, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);

	DrawNoiseLines(&plot, 0, endAmplitude, Signal, config);
	DrawLabelsNoise(&plot, config->endHzPlot, Signal, config);

	for(int a = 0; a < size; a++)
	{
		if(amplDiff[a].hertz && amplDiff[a].type == type && fabs(amplDiff[a].diffAmplitude) <= fabs(dBFS))
		{
			if(amplDiff[a].refAmplitude > endAmplitude)
			{
				long int intensity;
	
				intensity = CalculateWeightedError(1.0  -(fabs(amplDiff[a].refAmplitude)-fabs(startAmplitude))/(fabs(endAmplitude)-fabs(startAmplitude)), config)*0xffff;
				//intensity = 0xffff;

				SetPenColor(amplDiff[a].color, intensity, &plot);
				pl_fpoint_r(plot.plotter, transformtoLog(amplDiff[a].hertz, config), amplDiff[a].diffAmplitude);
			}
		}
	}

	DrawColorScale(&plot, type, MODE_DIFF,
		LEFT_MARGIN, HEIGHT_MARGIN, 
		config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15,
		(int)startAmplitude, (int)(endAmplitude-startAmplitude), VERT_SCALE_STEP_BAR, config);

	DrawLabelsMDF(&plot, NOISE_TITLE, GetTypeDisplayName(config, type), PLOT_COMPARE, config);
	ClosePlot(&plot);
}

/*
void PlotAllMissingFrequencies(FlatFrequency *freqDiff, long int size, char *filename, parameters *config)
{
	PlotFile	plot;
	char		name[BUFFER_SIZE];
	double		significant = 0, abs_significant = 0;

	if(!config)
		return;

	significant = config->significantAmplitude;
	abs_significant = fabs(significant);

	sprintf(name, "MIS__ALL_%s", filename);
	FillPlot(&plot, name, config->startHzPlot, significant, config->endHzPlot, 0.0, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroToLimit(&plot, significant, VERT_SCALE_STEP, config->endHzPlot, 1000, 0, config);
	DrawLabelsZeroToLimit(&plot, significant, VERT_SCALE_STEP, config->endHzPlot, 1000, 0, config);

	if(size)
	{
		for(int f = size-1; f >= 0 ; f--)
		{
			if(freqDiff[f].type > TYPE_CONTROL)
			{ 
				long int intensity;
				double x, y;
	
				x = transformtoLog(freqDiff[f].hertz, config);
				y = freqDiff[f].amplitude;
	
				intensity = CalculateWeightedError((abs_significant - fabs(freqDiff[f].amplitude))/abs_significant, config)*0xffff;
				
				SetPenColor(freqDiff[f].color, intensity, &plot);
				pl_fline_r(plot.plotter, x,	y, x, significant);
				pl_endpath_r(plot.plotter);
			}
		}
	}
	
	DrawColorAllTypeScale(&plot, MODE_MISS, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, significant, VERT_SCALE_STEP_BAR, DRAW_BARS, config);
	DrawLabelsMDF(&plot, MISSING_TITLE, ALL_LABEL, PLOT_COMPARE, config);
	ClosePlot(&plot);
}

int PlotEachTypeMissingFrequencies(FlatFrequency *freqDiff, long int size, char *filename, parameters *config)
{
	int 		i = 0, type = 0, types = 0, typeCount= 0;
	char		name[BUFFER_SIZE];

	typeCount = GetActiveBlockTypesNoRepeat(config);
	for(i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;
		if(type > TYPE_CONTROL && !config->types.typeArray[i].IsaddOnData)
		{
			char	*returnFolder = NULL;

			if(typeCount > 1)
			{
				returnFolder = PushFolder(MISSING_FOLDER);
				if(!returnFolder)
					return 0;
			}

			sprintf(name, "MISS_%s_%02d%s", filename, 
							config->types.typeArray[i].type, config->types.typeArray[i].typeName);
			PlotSingleTypeMissingFrequencies(freqDiff, size, type, name, config);
			logmsg(PLOT_ADVANCE_CHAR);

			if(typeCount > 1)
				ReturnToMainPath(&returnFolder);
			types ++;
		}
	}
	return types;
}

void PlotSingleTypeMissingFrequencies(FlatFrequency *freqDiff, long int size, int type, char *filename, parameters *config)
{
	PlotFile	plot;
	double		significant = 0, abs_significant = 0;

	if(!config || !freqDiff)
		return;

	significant = config->significantAmplitude;
	abs_significant = fabs(significant);

	FillPlot(&plot, filename, config->startHzPlot, significant, config->endHzPlot, 0.0, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroToLimit(&plot, significant, VERT_SCALE_STEP, config->endHzPlot, 1000, 0, config);
	DrawLabelsZeroToLimit(&plot, significant, VERT_SCALE_STEP, config->endHzPlot, 1000, 0, config);

	for(int f = 0; f < size; f++)
	{
		if(freqDiff[f].type == type)
		{
			long int intensity;
			double x, y;

			x = transformtoLog(freqDiff[f].hertz, config);;
			y = freqDiff[f].amplitude;
			intensity = CalculateWeightedError((abs_significant - fabs(y))/abs_significant, config)*0xffff;

			SetPenColor(freqDiff[f].color, intensity, &plot);
			pl_fline_r(plot.plotter, x,	y, x, significant);
			pl_endpath_r(plot.plotter);
		}
	}
	
	DrawColorScale(&plot, type, MODE_MISS, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, 0, significant, VERT_SCALE_STEP_BAR, config);
	DrawLabelsMDF(&plot, MISSING_TITLE, GetTypeDisplayName(config, type), PLOT_COMPARE, config);
	ClosePlot(&plot);
}
*/

void PlotAllSpectrogram(FlatFrequency *freqs, long int size, char *filename, int signal, parameters *config)
{
	PlotFile plot;
	char	 name[BUFFER_SIZE];
	double	 significant = 0, abs_significant = 0;

	if(!config)
		return;

	significant = config->significantAmplitude;
	abs_significant = fabs(significant);

	sprintf(name, "SP__ALL_%c_%s", signal == ROLE_REF ? 'A' : 'B', filename);
	FillPlot(&plot, name, config->startHzPlot, significant, config->endHzPlot, 0.0, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroToLimit(&plot, significant, VERT_SCALE_STEP, config->endHzPlot, 1000, 0, config);
	DrawLabelsZeroToLimit(&plot, significant, VERT_SCALE_STEP, config->endHzPlot, 1000, 0, config);

	if(size)
	{
		for(int f = size-1; f >= 0; f--)
		{
			if(freqs[f].type > TYPE_CONTROL)
			{ 
				long int intensity;
				double x, y;
	
				x = transformtoLog(freqs[f].hertz, config);
				y = freqs[f].amplitude;
				intensity = CalculateWeightedError((abs_significant - fabs(y))/abs_significant, config)*0xffff;
		
				SetPenColor(freqs[f].color, intensity, &plot);
				pl_fline_r(plot.plotter, x,	y, x, significant);
				pl_endpath_r(plot.plotter);
			}
		}
	}

	DrawColorAllTypeScale(&plot, MODE_SPEC, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, significant, VERT_SCALE_STEP_BAR, DRAW_BARS, config);
	DrawLabelsMDF(&plot, signal == ROLE_REF ? SPECTROGRAM_TITLE_REF : SPECTROGRAM_TITLE_COM, ALL_LABEL, signal == ROLE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);
	ClosePlot(&plot);
}

int PlotEachTypeSpectrogram(FlatFrequency *freqs, long int size, char *filename, int signal, parameters *config, AudioSignal *Signal)
{
	int 		i = 0, type = 0, types = 0, silence = 0, typeCount = 0;
	char		name[BUFFER_SIZE];

	typeCount = GetActiveBlockTypesNoRepeat(config);
	for(i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;
		if(type > TYPE_CONTROL && !config->types.typeArray[i].IsaddOnData)
		{
			char	*returnFolder = NULL;

			if(typeCount > 1)
			{
				returnFolder = PushFolder(SPECTROGRAM_FOLDER);
				if(!returnFolder)
					return 0;
			}
			sprintf(name, "SP_%c_%s_%02d%s", signal == ROLE_REF ? 'A' : 'B', filename, 
					config->types.typeArray[i].type, config->types.typeArray[i].typeName);
			PlotSingleTypeSpectrogram(freqs, size, type, name, signal, config);
			logmsg(PLOT_ADVANCE_CHAR);

			if(typeCount > 1)
				ReturnToMainPath(&returnFolder);
			types ++;
		}

		if(config->plotNoiseFloor && !silence && type == TYPE_SILENCE)
		{
			sprintf(name, "NF_SP_%c_%s_%02d%s", signal == ROLE_REF ? 'A' : 'B', filename, 
					config->types.typeArray[i].type, config->types.typeArray[i].typeName);
			PlotNoiseSpectrogram(freqs, size, type, name, signal, config, Signal);
			logmsg(PLOT_ADVANCE_CHAR);
			silence = 1;
		}
	}

	return types;
}

void PlotSingleTypeSpectrogram(FlatFrequency *freqs, long int size, int type, char *filename, int signal, parameters *config)
{
	PlotFile	plot;
	double		significant = 0, abs_significant = 0;

	if(!config)
		return;

	significant = config->significantAmplitude;
	abs_significant = fabs(significant);

	FillPlot(&plot, filename, config->startHzPlot, significant, config->endHzPlot, 0.0, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroToLimit(&plot, significant, VERT_SCALE_STEP,config->endHzPlot, 1000, 0, config);
	DrawLabelsZeroToLimit(&plot, significant, VERT_SCALE_STEP,config->endHzPlot, 1000, 0, config);

	for(int f = 0; f < size; f++)
	{
		if(freqs[f].type == type)
		{ 
			long int intensity;
			double x, y;

			x = transformtoLog(freqs[f].hertz, config);
			y = freqs[f].amplitude;
			intensity = CalculateWeightedError((abs_significant - fabs(y))/abs_significant, config)*0xffff;
	
			//pl_flinewidth_r(plot.plotter, 100*range_0_1);
			SetPenColor(freqs[f].color, intensity, &plot);
			pl_fline_r(plot.plotter, x,	y, x, significant);
			pl_endpath_r(plot.plotter);
		}
	}
	
	DrawColorScale(&plot, type, MODE_SPEC, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, 0, significant, VERT_SCALE_STEP,config);
	DrawLabelsMDF(&plot, signal == ROLE_REF ? SPECTROGRAM_TITLE_REF : SPECTROGRAM_TITLE_COM, GetTypeDisplayName(config, type), signal == ROLE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);
	ClosePlot(&plot);
}

void PlotNoiseSpectrogram(FlatFrequency *freqs, long int size, int type, char *filename, int signal, parameters *config, AudioSignal *Signal)
{
	PlotFile	plot;
	double		startAmplitude = config->significantAmplitude, endAmplitude = PCM_16BIT_MIN_AMPLITUDE;

	if(!config)
		return;

	// Find limits
	for(int f = 0; f < size; f++)
	{
		if(freqs[f].type == type)
		{
			if(freqs[f].amplitude > startAmplitude)
				startAmplitude = freqs[f].amplitude;
			if(freqs[f].amplitude < endAmplitude)
				endAmplitude = freqs[f].amplitude;
		}
	}

	if(endAmplitude < NS_LOWEST_AMPLITUDE)
		endAmplitude = NS_LOWEST_AMPLITUDE;

	if(Signal->role == ROLE_COMP)
	{
		if(config->refNoiseMax != 0)
			endAmplitude = config->refNoiseMax;
		else
			logmsg("WARNING: Noise Floor Reference values were not set\n");
	}

	if(Signal->role == ROLE_REF)
	{
		config->refNoiseMin = startAmplitude;
		config->refNoiseMax = endAmplitude;
	}

	FillPlot(&plot, filename, config->startHzPlot, endAmplitude, config->endHzPlot, 0.0, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroToLimit(&plot, endAmplitude, VERT_SCALE_STEP, config->endHzPlot, 1000, 1, config);
	DrawLabelsZeroToLimit(&plot, endAmplitude, VERT_SCALE_STEP, config->endHzPlot, 1000, 1, config);

	DrawNoiseLines(&plot, 0, endAmplitude, Signal, config);
	DrawLabelsNoise(&plot, config->endHzPlot, Signal, config);

	for(int f = 0; f < size; f++)
	{
		if(freqs[f].type == type && freqs[f].amplitude >= endAmplitude)
		{ 
			long int intensity;
			double x, y;

			x = transformtoLog(freqs[f].hertz, config);
			y = freqs[f].amplitude;
			intensity = CalculateWeightedError(((fabs(endAmplitude) - fabs(startAmplitude)) - (fabs(freqs[f].amplitude)- fabs(startAmplitude)))/(fabs(endAmplitude)-fabs(startAmplitude)), config)*0xffff;
			
			//pl_flinewidth_r(plot.plotter, 100*range_0_1);
			SetPenColor(freqs[f].color, intensity, &plot);
			pl_fline_r(plot.plotter, x, y, x, endAmplitude);
			pl_endpath_r(plot.plotter);
		}
	}
	
	DrawColorScale(&plot, type, MODE_SPEC, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, (int)startAmplitude, (int)(endAmplitude-startAmplitude), VERT_SCALE_STEP,config);
	DrawLabelsMDF(&plot, signal == ROLE_REF ? SPECTROGRAM_NOISE_REF : SPECTROGRAM_NOISE_COM, GetTypeDisplayName(config, type), signal == ROLE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);
	ClosePlot(&plot);
}

void VisualizeWindows(windowManager *wm, parameters *config)
{
	if(!wm)
		return;

	for(int i = 0; i < wm->windowCount; i++)
	{
		//logmsg("Factor len %ld: %g\n", wm->windowArray[i].frames,
			//CalculateCorrectionFactor(wm, wm->windowArray[i].frames));

		//for(long int j = 0; j < wm->windowArray[i].size; j++)
			//logmsg("Window %ld %g\n", j, wm->windowArray[i].window[j]);

		PlotWindow(&wm->windowArray[i], config);
	}
}

void PlotWindow(windowUnit *windowUnit, parameters *config)
{
	PlotFile plot;
	char	 name[BUFFER_SIZE];
	double 	 *window = NULL, frames;
	long int size;

	if(!config || !windowUnit)
		return;

	window = windowUnit->window;
	frames = windowUnit->frames;
	size = windowUnit->size;

	sprintf(name, "WindowPlot_%s", GetWindow(config->window));
	FillPlotExtra(&plot, name, 320, 384, 0, -0.1, 1, 1.1, 0.001, 0, config);

	if(!CreatePlotFile(&plot, config))
		return;

	// Frames Grid
	pl_pencolor_r (plot.plotter, 0, 0x3333, 0);
	for(long int i = 0; i < frames; i++)
		pl_fline_r(plot.plotter, (double)i*1/(double)frames, -0.1, (double)i*1/(double)frames, 1.1);

	// horizontal grid
	pl_pencolor_r (plot.plotter, 0, 0x5555, 0);
	pl_fline_r(plot.plotter, 0, 1, 1, 1);
	pl_fline_r(plot.plotter, 0, 0, 1, 0);
	pl_endpath_r(plot.plotter);

	pl_pencolor_r (plot.plotter, 0, 0xFFFF, 0);	
	for(int i = 0; i < size; i++)
		pl_fpoint_r(plot.plotter, (double)i/(double)size, window[i]);
	
	ClosePlot(&plot);
}

void PlotBetaFunctions(parameters *config)
{
	char	 name[BUFFER_SIZE];
	int		 type = 0;

	if(!config)
		return;

	for(type = 0; type <= 5; type ++)
	{
		PlotFile plot;

		config->outputFilterFunction = type;
		sprintf(name, "BetaFunctionPlot_%d", type);
		FillPlotExtra(&plot, name, 320, 384, 0, -0.1, 1, 1.1, 0.001, 0, config);
	
		if(!CreatePlotFile(&plot, config))
			return;
	
		pl_pencolor_r (plot.plotter, 0, 0x5555, 0);
		pl_fline_r(plot.plotter, 0, 1, 1, 1);
		pl_fline_r(plot.plotter, 0, 0, 1, 0);

		pl_pencolor_r (plot.plotter, 0, 0x3333, 0);
		pl_fline_r(plot.plotter, .5, -0.1, .5, 1.1);
		pl_fline_r(plot.plotter, .25, -0.1, .25, 1.1);
		pl_fline_r(plot.plotter, .75, -0.1, .75, 1.1);

		pl_fline_r(plot.plotter, 0, .5, 1, .5);
		pl_fline_r(plot.plotter, 0, .25, 1, .25);
		pl_fline_r(plot.plotter, 0, .75, 1, .75);
		pl_endpath_r(plot.plotter);
	
		pl_pencolor_r (plot.plotter, 0, 0xFFFF, 0);	
		for(int i = 0; i < 320; i++)
		{
			double x, y;
			long int color;

			x = (double)i/(double)320;
			y = CalculateWeightedError((double)i/(double)320, config);
			
			color = y*0xffff;
	
			SetPenColor(COLOR_AQUA, color, &plot);
			//logmsg("x: %g (%g) y: %g (%g) c:%ld\n", x, x*60, y, y*60, color);
			pl_fpoint_r(plot.plotter, x, y);
		}
		
		ClosePlot(&plot);
	}
}

int MatchColor(char *color)
{
	int		i = 0;
	char	colorcopy[640];

	strncpy(colorcopy, color, 512);

	for(i = 0; i < strlen(colorcopy); i++)
		colorcopy[i] = tolower(colorcopy[i]);

	if(strcmp(colorcopy, "red") == 0)
		return(COLOR_RED);
	if(strcmp(colorcopy, "green") == 0)
		return(COLOR_GREEN);
	if(strcmp(colorcopy, "blue") == 0)
		return(COLOR_BLUE);
	if(strcmp(colorcopy, "yellow") == 0)
		return(COLOR_YELLOW);
	if(strcmp(colorcopy, "magenta") == 0)
		return(COLOR_MAGENTA);
	if(strcmp(colorcopy, "aqua") == 0 || strcmp(color, "aquamarine") == 0)
		return(COLOR_AQUA);
	if(strcmp(colorcopy, "orange") == 0)
		return(COLOR_ORANGE);
	if(strcmp(colorcopy, "purple") == 0)
		return(COLOR_PURPLE);
	if(strcmp(colorcopy, "gray") == 0)
		return(COLOR_GRAY);
	if(strcmp(colorcopy, "null") == 0)
		return(COLOR_NULL);

	logmsg("Unmatched color %s, using green\n", color);
	return COLOR_GREEN;
}

void SetPenColorStr(char *colorName, long int color, PlotFile *plot)
{
	SetPenColor(MatchColor(colorName), color, plot);
}

void SetPenColor(int colorIndex, long int color, PlotFile *plot)
{
	switch(colorIndex)
	{
		case COLOR_RED:
			pl_pencolor_r(plot->plotter, color, 0, 0);
			break;
		case COLOR_GREEN:
			pl_pencolor_r(plot->plotter, 0, color, 0);
			break;
		case COLOR_BLUE:
			pl_pencolor_r(plot->plotter, 0, 0, color);
			break;
		case COLOR_YELLOW:
			pl_pencolor_r(plot->plotter, color, color, 0);
			break;
		case COLOR_AQUA:
			pl_pencolor_r(plot->plotter, 0, color, color);
			break;
		case COLOR_MAGENTA:
			pl_pencolor_r(plot->plotter, color, 0, color);
			break;
		case COLOR_PURPLE:
			pl_pencolor_r(plot->plotter, color/2, 0, color);
			break;
		case COLOR_ORANGE:
			pl_pencolor_r(plot->plotter, color, color/2, 0);
			break;
		case COLOR_GRAY:
			pl_pencolor_r(plot->plotter, color, color, color);
			break;
		case COLOR_NULL:
			pl_pencolor_r(plot->plotter, 0, 0, 0);
			break;
		default:
			pl_pencolor_r(plot->plotter, 0, color, 0);
			break;
	}
}

void SetFillColor(int colorIndex, long int color, PlotFile *plot)
{
	switch(colorIndex)
	{
		case COLOR_RED:
			pl_fillcolor_r(plot->plotter, color, 0, 0);
			break;
		case COLOR_GREEN:
			pl_fillcolor_r(plot->plotter, 0, color, 0);
			break;
		case COLOR_BLUE:
			pl_fillcolor_r(plot->plotter, 0, 0, color);
			break;
		case COLOR_YELLOW:
			pl_fillcolor_r(plot->plotter, color, color, 0);
			break;
		case COLOR_AQUA:
			pl_fillcolor_r(plot->plotter, 0, color, color);
			break;
		case COLOR_MAGENTA:
			pl_fillcolor_r(plot->plotter, color, 0, color);
			break;
		case COLOR_PURPLE:
			pl_fillcolor_r(plot->plotter, color/2, 0, color);
			break;
		case COLOR_ORANGE:
			pl_fillcolor_r(plot->plotter, color, color/2, 0);
			break;
		case COLOR_GRAY:
			pl_fillcolor_r(plot->plotter, color, color, color);
			break;
		case COLOR_NULL:
			pl_fillcolor_r(plot->plotter, 0, 0, 0);
			break;
		default:
			pl_fillcolor_r(plot->plotter, 0, color, 0);
			break;
	}
}


FlatAmplDifference *CreateFlatDifferences(parameters *config, long int *size, diffPlotType plotType)
{
	long int			count = 0;
	FlatAmplDifference	*ADiff = NULL;

	if(!config)
		return NULL;

	if(!size)
		return NULL;
	*size = 0;

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		int type = 0, doplot = 0;
		
		type = GetBlockType(config, b);
		if(plotType == normalPlot && type > TYPE_SILENCE)
			doplot = 1;
		if(plotType == floorPlot && type == TYPE_SILENCE)
			doplot = 1;
		if(doplot)
			count += config->Differences.BlockDiffArray[b].cntAmplBlkDiff;
	}

	ADiff = (FlatAmplDifference*)malloc(sizeof(FlatAmplDifference)*count);
	if(!ADiff)
		return NULL;
	memset(ADiff, 0, sizeof(FlatAmplDifference)*count);

	count = 0;
	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		int type = 0, doplot = 0;
		
		type = GetBlockType(config, b);
		if(plotType == normalPlot && type > TYPE_SILENCE)
			doplot = 1;
		if(plotType == floorPlot && type == TYPE_SILENCE)
			doplot = 1;
		if(doplot)
		{
			int color = 0;

			color = MatchColor(GetBlockColor(config, b));
			for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
			{
				ADiff[count].hertz = config->Differences.BlockDiffArray[b].amplDiffArray[a].hertz;
				ADiff[count].refAmplitude = config->Differences.BlockDiffArray[b].amplDiffArray[a].refAmplitude;
				ADiff[count].diffAmplitude = config->Differences.BlockDiffArray[b].amplDiffArray[a].diffAmplitude;
				ADiff[count].type = type;
				ADiff[count].color = color;
				count ++;
			}
		}
	}
	logmsg(PLOT_PROCESS_CHAR);
	AmplitudeDifferences_tim_sort(ADiff, count);
	logmsg(PLOT_PROCESS_CHAR);
	*size = count;
	return(ADiff);
}

/*
FlatFrequency *CreateFlatMissing(parameters *config, long int *size)
{
	long int	count = 0;
	FlatFrequency *FDiff = NULL;

	if(!config)
		return NULL;

	if(!size)
		return NULL;

	*size = 0;

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		if(GetBlockType(config, b) > TYPE_SILENCE)
			count += config->Differences.BlockDiffArray[b].cntFreqBlkDiff;
	}

	FDiff = (FlatFrequency*)malloc(sizeof(FlatFrequency)*count);
	if(!FDiff)
		return NULL;
	memset(FDiff, 0, sizeof(FlatFrequency)*count);

	count = 0;
	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		int type = 0, color = 0;
		
		type = GetBlockType(config, b);
		if(type > TYPE_SILENCE)
		{
			color = MatchColor(GetBlockColor(config, b));
			for(int f = 0; f < config->Differences.BlockDiffArray[b].cntFreqBlkDiff; f++)
			{
				FDiff[count].hertz = config->Differences.BlockDiffArray[b].freqMissArray[f].hertz;
				FDiff[count].amplitude = config->Differences.BlockDiffArray[b].freqMissArray[f].amplitude;
				FDiff[count].type = type;
				FDiff[count].color = color;
				count ++;
			}
		}
	}
	logmsg(PLOT_PROCESS_CHAR);
	FlatMissingDifferencesByAmplitude_tim_sort(FDiff, count);
	logmsg(PLOT_PROCESS_CHAR);

	*size = count;
	return(FDiff);
}
*/

int InsertElementInPlace(FlatFrequency *Freqs, FlatFrequency Element, long int currentsize)
{
	if(!currentsize)
	{
		Freqs[0] = Element;
		return 1;
	}

	for(long int j = 0; j < currentsize; j++)
	{
		if(Element.type == Freqs[j].type && Element.hertz == Freqs[j].hertz)
		{
			if(Freqs[j].amplitude > Element.amplitude)
				return 0;
			else
				Freqs[j].amplitude = Element.amplitude;
			return 0;
		}
	}

	Freqs[currentsize] = Element;
	return 1;
}

FlatFrequency *CreateFlatFrequencies(AudioSignal *Signal, long int *size, parameters *config)
{
	long int		block = 0, i = 0;
	long int		count = 0, counter = 0;
	FlatFrequency	*Freqs = NULL;
	double			significant = 0;

	if(!size || !Signal || !config)
		return NULL;

	*size = 0;

	significant = config->significantAmplitude;

	for(block = 0; block < config->types.totalBlocks; block++)
	{
		int 	type = TYPE_NOTYPE;

		type = GetBlockType(config, block);

		if(type >= TYPE_SILENCE)
		{
			for(i = 0; i < config->MaxFreq; i++)
			{
				int insert = 0;

				if(Signal->Blocks[block].freq[i].hertz == 0)
					break;

				if(type > TYPE_SILENCE && Signal->Blocks[block].freq[i].hertz && Signal->Blocks[block].freq[i].amplitude > significant)
					insert = 1;
				if(type == TYPE_SILENCE && Signal->Blocks[block].freq[i].hertz)
					insert = 1;

				if(insert)
					count ++;
				else
					break;
			}
		}
	}

	Freqs = (FlatFrequency*)malloc(sizeof(FlatFrequency)*count);
	if(!Freqs)
		return NULL;
	memset(Freqs, 0, sizeof(FlatFrequency)*count);

	for(block = 0; block < config->types.totalBlocks; block++)
	{
		int type = 0, color = 0;

		type = GetBlockType(config, block);
		if(type >= TYPE_SILENCE)
		{
			color = MatchColor(GetBlockColor(config, block));
	
			for(i = 0; i < config->MaxFreq; i++)
			{
				int insert = 0;

				if(type > TYPE_SILENCE && Signal->Blocks[block].freq[i].hertz && Signal->Blocks[block].freq[i].amplitude > significant)
					insert = 1;
				if(type == TYPE_SILENCE && Signal->Blocks[block].freq[i].hertz)
					insert = 1;

				if(insert)
				{
					FlatFrequency tmp;
	
					tmp.hertz = Signal->Blocks[block].freq[i].hertz;
					tmp.amplitude = Signal->Blocks[block].freq[i].amplitude;
					tmp.type = type;
					tmp.color = color;
	
					if(InsertElementInPlace(Freqs, tmp, counter))
						counter ++;
				}
				else
					break;
			}
		}
	}
	
	logmsg(PLOT_PROCESS_CHAR);
	FlatFrequenciesByAmplitude_tim_sort(Freqs, counter);
	logmsg(PLOT_PROCESS_CHAR);

	*size = counter;
	return(Freqs);
}


void PlotTest(char *filename, parameters *config)
{
	PlotFile	plot;
	char		name[BUFFER_SIZE];
	double		dBFS = config->maxDbPlotZC;

	if(!config)
		return;

	sprintf(name, "Test_%s", filename);
	FillPlot(&plot, name, config->startHzPlot, -1*dBFS, config->endHzPlot, dBFS, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);

	srand(time(NULL));

	DrawLabelsMDF(&plot, "PLOT TEST [%s]", "ZDBC", PLOT_COMPARE, config);
	DrawColorAllTypeScale(&plot, MODE_DIFF, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, config->significantAmplitude, VERT_SCALE_STEP_BAR, DRAW_BARS, config);

	ClosePlot(&plot);
}

void PlotTestZL(char *filename, parameters *config)
{
	PlotFile	plot;
	char		name[BUFFER_SIZE];

	if(!config)
		return;

	sprintf(name, "Test_ZL_%s", filename);
	FillPlot(&plot, name, config->startHzPlot, config->significantAmplitude, config->endHzPlot, 0.0, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroToLimit(&plot, config->significantAmplitude, VERT_SCALE_STEP, config->endHzPlot, 1000, 0, config);
	DrawLabelsZeroToLimit(&plot, config->significantAmplitude, VERT_SCALE_STEP, config->endHzPlot, 1000, 0, config);

	DrawColorScale(&plot, 1, MODE_SPEC, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, 0, SIGNIFICANT_VOLUME, VERT_SCALE_STEP_BAR, config);
	
	DrawLabelsMDF(&plot, "PLOT TEST [%s]", "GZL", PLOT_COMPARE, config);
	ClosePlot(&plot);
}

inline double transformtoLog(double coord, parameters *config)
{
	if(coord <= 0)
	{
		logmsg("WARNING: transformtoLog received %g\n", coord);
		return 0;
	}
	if(config->logScale)
		return(config->plotRatio*log10(coord));
	else
		return(coord);
}

/* =============Best Fit ================ */

long int movingAverage(AveragedFrequencies *data, AveragedFrequencies *averages, long int size, long int period)
{
	long int 			current_index = 0, i = 0, pos = 0;
	AveragedFrequencies *periodArray = NULL;

	periodArray = (AveragedFrequencies*)malloc(sizeof(AveragedFrequencies)*period);
	if(!periodArray)
		return 0;
	
	memset(periodArray, 0, sizeof(AveragedFrequencies)*period);

	for(i = 0; i < size; i++)
	{
		AveragedFrequencies	ma;

		ma.avgfreq = 0;
		ma.avgvol = 0;
		periodArray[current_index].avgfreq = data[i].avgfreq/(double)period;
		periodArray[current_index].avgvol = data[i].avgvol/(double)period;
 		for(long int j = 0; j < period; j++)
		{
			ma.avgfreq += periodArray[j].avgfreq;
			ma.avgvol += periodArray[j].avgvol;
		}
		if(i >= period)
			averages[pos++] = ma;
		current_index = (current_index + 1) % period;
	}

	free(periodArray);
	periodArray = NULL;
	return pos;
}

// parameters for the Average plot

#define	SMA_SIZE					4	// Size for the Simple Moving average period
#define	AVERAGE_CHUNKS				200	// How many chunks across the frequency spectrum

AveragedFrequencies *CreateFlatDifferencesAveraged(int matchType, long int *avgSize, int chunks, diffPlotType plotType, parameters *config)
{
	long int			count = 0, interval = 0, realResults = 0;
	FlatAmplDifference	*ADiff = NULL;
	AveragedFrequencies	*averaged = NULL, *averagedSMA = NULL;
	double				significant = 0, 	abs_significant = 0;

	if(!config)
		return NULL;

	if(!avgSize)
		return NULL;

	*avgSize = 0;

	significant = config->significantAmplitude;
	abs_significant = fabs(significant);

	// Count how many we have
	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		if(GetBlockType(config, b) == matchType)
			count += config->Differences.BlockDiffArray[b].cntAmplBlkDiff;
	}

	if(!count)
		return NULL;

	if(config->weightedAveragePlot)
		count *= 10;
	ADiff = (FlatAmplDifference*)malloc(sizeof(FlatAmplDifference)*count);
	if(!ADiff)
		return NULL;
	memset(ADiff, 0, sizeof(FlatAmplDifference)*count);

	count = 0;
	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		int type = 0;
		
		type = config->Differences.BlockDiffArray[b].type;
		if(type == matchType)
		{
			double		startAmplitude = config->referenceNoiseFloor, endAmplitude = PCM_16BIT_MIN_AMPLITUDE;

			if(plotType == floorPlot)
			{
				// Find limits
				for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
				{
					if(config->Differences.BlockDiffArray[b].amplDiffArray[a].hertz > 0)
					{
						if(config->Differences.BlockDiffArray[b].amplDiffArray[a].refAmplitude > startAmplitude)
							startAmplitude = config->Differences.BlockDiffArray[b].amplDiffArray[a].refAmplitude;
						if(config->Differences.BlockDiffArray[b].amplDiffArray[a].refAmplitude < endAmplitude)
							endAmplitude = config->Differences.BlockDiffArray[b].amplDiffArray[a].refAmplitude;
					}
				}

				if(endAmplitude < NS_LOWEST_AMPLITUDE)
					endAmplitude = NS_LOWEST_AMPLITUDE;
				significant = endAmplitude;
			}

			for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
			{
				if(config->Differences.BlockDiffArray[b].amplDiffArray[a].refAmplitude > significant)
				{
					if(config->weightedAveragePlot)
					{
						double intensity = 0, value = 0;
	
						if(plotType == normalPlot)
							value = (abs_significant - fabs(config->Differences.BlockDiffArray[b].amplDiffArray[a].refAmplitude))/
										abs_significant;
						else
							value = 1.0  -(fabs(config->Differences.BlockDiffArray[b].amplDiffArray[a].refAmplitude)-fabs(startAmplitude))/
										(fabs(endAmplitude)-fabs(startAmplitude));
						intensity = CalculateWeightedError(value, config);
						for(int i = 0; i < floor(intensity*10); i++)
						{
							ADiff[count].hertz = config->Differences.BlockDiffArray[b].amplDiffArray[a].hertz;
							ADiff[count].diffAmplitude = config->Differences.BlockDiffArray[b].amplDiffArray[a].diffAmplitude;
							count ++;
						}
					}
					else
					{
						ADiff[count].hertz = config->Differences.BlockDiffArray[b].amplDiffArray[a].hertz;
						ADiff[count].diffAmplitude = config->Differences.BlockDiffArray[b].amplDiffArray[a].diffAmplitude;
						count ++;
					}
				}
			}
		}
	}

	if(!count)
	{
		free(ADiff);
		return NULL;
	}
	logmsg(PLOT_PROCESS_CHAR);

	averaged = (AveragedFrequencies*)malloc(sizeof(AveragedFrequencies)*count);
	if(!averaged)
	{
		free(ADiff);
		return NULL;
	}
	memset(averaged, 0, sizeof(AveragedFrequencies)*count);

	AmplitudeDifferencesByFrequencyAveraged_tim_sort(ADiff, count);
	logmsg(PLOT_PROCESS_CHAR);

	interval = ceil((double)count/(double)chunks);
	if(interval < 1)
	{
		interval = 1;
		chunks = count;
	}

	for(long int a = 0; a < chunks; a++)
	{
		long int start, end, elements = 0;

		start = a*interval;
		end = start+interval;

		for(long int c = start; c < end; c++)
		{
			if(c == count)
				break;

			averaged[realResults].avgfreq += ADiff[c].hertz;
			averaged[realResults].avgvol += ADiff[c].diffAmplitude;
			elements++;
		}

		if(elements)
		{
			averaged[realResults].avgfreq /= elements;
			averaged[realResults].avgvol /= elements;

			realResults++;
		}

		if(end >= count)
			break;
	}

	free(ADiff);
	ADiff = NULL;

	averagedSMA = (AveragedFrequencies*)malloc(sizeof(AveragedFrequencies)*realResults);
	if(!averagedSMA)
	{
		free(averaged);
		return NULL;
	}

	memset(averagedSMA, 0, sizeof(AveragedFrequencies)*realResults);

	logmsg(PLOT_PROCESS_CHAR);

	*avgSize = movingAverage(averaged, averagedSMA, realResults, SMA_SIZE);

	free(averaged);
	averaged = NULL;

	return(averagedSMA);
}

int PlotDifferentAmplitudesAveraged(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config)
{
	int 				i = 0, type = 0, typeCount = 0, types = 0;
	char				name[BUFFER_SIZE];
	long int			*averagedSizes = NULL;
	AveragedFrequencies	**averagedArray = NULL;

	typeCount = GetActiveBlockTypesNoRepeat(config);

	averagedArray = (AveragedFrequencies**)malloc(sizeof(AveragedFrequencies*)*typeCount);
	if(!averagedArray)
		return 0;

	averagedSizes = (long int*)malloc(sizeof(long int)*typeCount);
	if(!averagedSizes)
		return 0;

	memset(averagedArray, 0, sizeof(AveragedFrequencies*)*typeCount);
	memset(averagedSizes, 0, sizeof(long int)*typeCount);

	for(i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;

		if(type > TYPE_CONTROL && !config->types.typeArray[i].IsaddOnData)
		{
			long int	chunks = AVERAGE_CHUNKS;

			if(typeCount == 1)
				sprintf(name, "DA__ALL_%s_AVG_", filename);
			else
				sprintf(name, "DA_%s_%02d%s_AVG_", filename, 
					config->types.typeArray[i].type, config->types.typeArray[i].typeName);

			averagedArray[types] = CreateFlatDifferencesAveraged(type, &averagedSizes[types], chunks, normalPlot, config);

			if(averagedArray[types])
			{
				char	*returnFolder = NULL;

				if(typeCount > 1)
				{
					returnFolder = PushFolder(DIFFERENCE_FOLDER);
					if(!returnFolder)
						return 0;
				}

				PlotSingleTypeDifferentAmplitudesAveraged(amplDiff, size, type, name, averagedArray[types], averagedSizes[types], config);
				logmsg(PLOT_ADVANCE_CHAR);

				if(typeCount > 1)
					ReturnToMainPath(&returnFolder);
			}

			types ++;
		}
	}

	if(types > 1 && averagedArray && averagedSizes)
	{
		sprintf(name, "DA__ALL_AVG_%s", filename);
		PlotAllDifferentAmplitudesAveraged(amplDiff, size, name, averagedArray, averagedSizes, config);
		logmsg(PLOT_ADVANCE_CHAR);
	}

	for(i = 0; i < typeCount; i++)
	{
		free(averagedArray[i]);
		averagedArray[i]= NULL;
	}

	free(averagedArray);
	averagedArray = NULL;
	free(averagedSizes);
	averagedSizes = NULL;

	return types;
}

int PlotNoiseDifferentAmplitudesAveraged(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config, AudioSignal *Signal)
{
	int 				i = 0;
	char				name[BUFFER_SIZE];
	long int			avgsize = 0;
	AveragedFrequencies	*averagedArray = NULL;

	for(i = 0; i < config->types.typeCount; i++)
	{
		int type = config->types.typeArray[i].type;
		if(type == TYPE_SILENCE)
		{
			long int chunks = AVERAGE_CHUNKS;

			sprintf(name, "NF__%s_%02d%s_AVG_", filename, 
					config->types.typeArray[i].type, config->types.typeArray[i].typeName);

			averagedArray = CreateFlatDifferencesAveraged(type, &avgsize, chunks, floorPlot, config);

			if(averagedArray)
			{
				PlotNoiseDifferentAmplitudesAveragedInternal(amplDiff, size, type, name, averagedArray, avgsize, config, Signal);
				logmsg(PLOT_ADVANCE_CHAR);
				free(averagedArray);
				averagedArray = NULL;
				return 1;
			}
		}
	}

	return 0;
}

void PlotNoiseDifferentAmplitudesAveragedInternal(FlatAmplDifference *amplDiff, long int size, int type, char *filename, AveragedFrequencies *averaged, long int avgsize, parameters *config, AudioSignal *Signal)
{
	PlotFile	plot;
	double		dbs = config->maxDbPlotZC, vertscale = VERT_SCALE_STEP;;
	int			color = 0;
	double		startAmplitude = config->referenceNoiseFloor, endAmplitude = PCM_16BIT_MIN_AMPLITUDE;

	if(!config)
		return;

	if(!amplDiff)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	// Find limits
	for(int a = 0; a < size; a++)
	{
		if(amplDiff[a].type == type)
		{
			if(fabs(amplDiff[a].diffAmplitude) > dbs)
				dbs = fabs(amplDiff[a].diffAmplitude);
			if(amplDiff[a].refAmplitude > startAmplitude)
				startAmplitude = amplDiff[a].refAmplitude;
			if(amplDiff[a].refAmplitude < endAmplitude)
				endAmplitude = amplDiff[a].refAmplitude;
		}
	}

	if(endAmplitude < NS_LOWEST_AMPLITUDE)
		endAmplitude = NS_LOWEST_AMPLITUDE;

	FillPlot(&plot, filename, config->startHzPlot, -1*dbs, config->endHzPlot, dbs, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	if(dbs > 90)
		vertscale *= 2;
	if(dbs > 200)
		vertscale *= 10;
	DrawGridZeroDBCentered(&plot, dbs, vertscale, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dbs, vertscale, config->endHzPlot, 1000, config);

	DrawNoiseLines(&plot, dbs, -1*dbs, Signal, config);
	DrawLabelsNoise(&plot, config->endHzPlot, Signal, config);

	for(long int a = 0; a < size; a++)
	{
		if(amplDiff[a].type == type)
		{ 
			if(amplDiff[a].refAmplitude > endAmplitude)
			{
				long int intensity;
	
				intensity = CalculateWeightedError(1.0  -(fabs(amplDiff[a].refAmplitude)-fabs(startAmplitude))/(fabs(endAmplitude)-fabs(startAmplitude)), config)*0xffff;
	
				SetPenColor(amplDiff[a].color, intensity, &plot);
				pl_fpoint_r(plot.plotter, transformtoLog(amplDiff[a].hertz, config), amplDiff[a].diffAmplitude);
			}
		}
	}

	color = MatchColor(GetTypeColor(config, type));
	pl_endpath_r(plot.plotter);

	if(averaged && avgsize > 1)
	{
		int first = 1;

		pl_flinewidth_r(plot.plotter, 50);
		SetPenColor(COLOR_GRAY, 0x0000, &plot);
		for(long int a = 0; a < avgsize; a++)
		{
			if(first)
			{
				pl_fline_r(plot.plotter, transformtoLog(averaged[a].avgfreq, config), averaged[a].avgvol,
							transformtoLog(averaged[a+1].avgfreq, config), averaged[a+1].avgvol);
				first = 0;
			}
			else
				pl_fcont_r(plot.plotter, transformtoLog(averaged[a].avgfreq, config), averaged[a].avgvol);
		}
		pl_endpath_r(plot.plotter);

		first = 1;
		pl_flinewidth_r(plot.plotter, plot.penWidth);
		SetPenColor(color, 0xFFFF, &plot);
		for(long int a = 0; a < avgsize; a++)
		{
			if(first)
			{
				pl_fline_r(plot.plotter, transformtoLog(averaged[a].avgfreq, config), averaged[a].avgvol,
							transformtoLog(averaged[a+1].avgfreq, config), averaged[a+1].avgvol);
				first = 0;
			}
			else
				pl_fcont_r(plot.plotter, transformtoLog(averaged[a].avgfreq, config), averaged[a].avgvol);
		}
		pl_endpath_r(plot.plotter);
	}

	DrawColorScale(&plot, type, MODE_DIFF, LEFT_MARGIN, HEIGHT_MARGIN, 
		config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15,
		(int)startAmplitude, (int)(endAmplitude-startAmplitude), VERT_SCALE_STEP_BAR, config);
	DrawLabelsMDF(&plot, NOISE_AVG_TITLE, GetTypeDisplayName(config, type), PLOT_COMPARE, config);
	ClosePlot(&plot);
}

void PlotSingleTypeDifferentAmplitudesAveraged(FlatAmplDifference *amplDiff, long int size, int type, char *filename, AveragedFrequencies *averaged, long int avgsize, parameters *config)
{
	PlotFile	plot;
	double		dbs = config->maxDbPlotZC;
	int			color = 0;

	if(!config)
		return;

	if(!amplDiff)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	FillPlot(&plot, filename, config->startHzPlot, -1*dbs, config->endHzPlot, dbs, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroDBCentered(&plot, dbs, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dbs, VERT_SCALE_STEP, config->endHzPlot, 1000, config);

	for(long int a = 0; a < size; a++)
	{
		if(amplDiff[a].type == type)
		{ 
			if(amplDiff[a].refAmplitude > config->significantAmplitude && fabs(amplDiff[a].diffAmplitude) <= fabs(dbs))
			{
				long int intensity;
	
				intensity = CalculateWeightedError((fabs(config->significantAmplitude) - fabs(amplDiff[a].refAmplitude))/fabs(config->significantAmplitude), config)*0xffff;
	
				SetPenColor(amplDiff[a].color, intensity, &plot);
				pl_fpoint_r(plot.plotter, transformtoLog(amplDiff[a].hertz, config), amplDiff[a].diffAmplitude);
			}
		}
	}

	color = MatchColor(GetTypeColor(config, type));
	pl_endpath_r(plot.plotter);

	if(averaged && avgsize > 1)
	{
		int first = 1;

		/*
		SetPenColor(color, 0xffff, &plot);
		for(long int a = 0; a < avgsize; a+=2)
		{
			if(a + 2 < avgsize)
			{
				pl_fbezier2_r(plot.plotter,
					transformtoLog(averaged[a].avgfreq, config), averaged[a].avgvol,
					transformtoLog(averaged[a+1].avgfreq, config), averaged[a+1].avgvol,
					transformtoLog(averaged[a+2].avgfreq, config), averaged[a+2].avgvol);
				//logmsg("Plot [%ld] %g->%g\n", a, averaged[a].avgfreq, averaged[a].avgvol);
			}
		}
		pl_endpath_r(plot.plotter);
		*/

		pl_flinewidth_r(plot.plotter, 50);
		SetPenColor(COLOR_GRAY, 0x0000, &plot);
		for(long int a = 0; a < avgsize; a++)
		{
			if(first)
			{
				pl_fline_r(plot.plotter, transformtoLog(averaged[a].avgfreq, config), averaged[a].avgvol,
							transformtoLog(averaged[a+1].avgfreq, config), averaged[a+1].avgvol);
				first = 0;
			}
			else
			{
				if(fabs(averaged[a].avgvol) <= fabs(dbs))
					pl_fcont_r(plot.plotter, transformtoLog(averaged[a].avgfreq, config), averaged[a].avgvol);
				else
					break;
			}
		}
		pl_endpath_r(plot.plotter);

		first = 1;
		pl_flinewidth_r(plot.plotter, plot.penWidth);
		SetPenColor(color, 0xFFFF, &plot);
		for(long int a = 0; a < avgsize; a++)
		{
			if(first)
			{
				pl_fline_r(plot.plotter, transformtoLog(averaged[a].avgfreq, config), averaged[a].avgvol,
							transformtoLog(averaged[a+1].avgfreq, config), averaged[a+1].avgvol);
				first = 0;
			}
			else
			{
				if(fabs(averaged[a].avgvol) <= fabs(dbs))
					pl_fcont_r(plot.plotter, transformtoLog(averaged[a].avgfreq, config), averaged[a].avgvol);
				else
					break;
			}
		}
		pl_endpath_r(plot.plotter);
	}

	DrawColorScale(&plot, type, MODE_DIFF, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, 0, config->significantAmplitude, VERT_SCALE_STEP_BAR, config);
	DrawLabelsMDF(&plot, DIFFERENCE_AVG_TITLE, GetTypeDisplayName(config, type), PLOT_COMPARE, config);
	ClosePlot(&plot);
}

void PlotAllDifferentAmplitudesAveraged(FlatAmplDifference *amplDiff, long int size, char *filename, AveragedFrequencies **averaged, long int *avgsize, parameters *config)
{
	PlotFile	plot;
	double		dBFS = config->maxDbPlotZC;
	int			currType = 0;

	if(!config)
		return;

	if(!amplDiff)
		return;

	if(!config->Differences.BlockDiffArray)
		return;
	
	FillPlot(&plot, filename, config->startHzPlot, -1*dBFS, config->endHzPlot, dBFS, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);

	for(long int a = 0; a < size; a++)
	{
		if(amplDiff[a].type > TYPE_CONTROL)
		{ 
			if(amplDiff[a].refAmplitude > config->significantAmplitude && fabs(amplDiff[a].diffAmplitude) <= fabs(dBFS))
			{
				long int intensity;
	
				intensity = CalculateWeightedError((fabs(config->significantAmplitude) - fabs(amplDiff[a].refAmplitude))/fabs(config->significantAmplitude), config)*0xffff;
	
				SetPenColor(amplDiff[a].color, intensity, &plot);
				pl_fpoint_r(plot.plotter, transformtoLog(amplDiff[a].hertz, config), amplDiff[a].diffAmplitude);
			}
		}
	}

	for(int t = 0; t < config->types.typeCount; t++)
	{
		int type = 0, color = 0;

		type = config->types.typeArray[t].type;
		if(type <= TYPE_CONTROL || config->types.typeArray[t].IsaddOnData)
			continue;

		color = MatchColor(GetTypeColor(config, type));
		pl_endpath_r(plot.plotter);
	
		if(averaged[currType] && avgsize[currType] > 1)
		{
			int first = 1;

			/*
			for(long int a = 0; a < avgsize[currType]; a+=2)
			{
				if(a + 2 < avgsize[currType])
				{
					pl_fbezier2_r(plot.plotter,
						transformtoLog(averaged[currType][a].avgfreq, config), averaged[currType][a].avgvol,
						transformtoLog(averaged[currType][a+1].avgfreq, config), averaged[currType][a+1].avgvol,
						transformtoLog(averaged[currType][a+2].avgfreq, config), averaged[currType][a+2].avgvol);
					//logmsg("Plot [%ld] %g->%g\n", a, averaged[currType][a].avgfreq, averaged[currType][a].avgvol);
				}
			}
			pl_endpath_r(plot.plotter);
			*/

			pl_flinewidth_r(plot.plotter, 50);
			SetPenColor(COLOR_GRAY, 0x0000, &plot);
			for(long int a = 0; a < avgsize[currType]; a++)
			{
				if(first)
				{
					pl_fline_r(plot.plotter, transformtoLog(averaged[currType][a].avgfreq, config), averaged[currType][a].avgvol,
								transformtoLog(averaged[currType][a+1].avgfreq, config), averaged[currType][a+1].avgvol);
					first = 0;
				}
				else
				{
					if(fabs(averaged[currType][a].avgvol) <= fabs(dBFS))
						pl_fcont_r(plot.plotter, transformtoLog(averaged[currType][a].avgfreq, config), averaged[currType][a].avgvol);
					else
						break;
				}
			}
			pl_endpath_r(plot.plotter);
	
			first = 1;
			pl_flinewidth_r(plot.plotter, plot.penWidth);
			SetPenColor(color, 0xffff, &plot);
			for(long int a = 0; a < avgsize[currType]; a++)
			{
				if(first)
				{
					pl_fline_r(plot.plotter, transformtoLog(averaged[currType][a].avgfreq, config), averaged[currType][a].avgvol,
								transformtoLog(averaged[currType][a+1].avgfreq, config), averaged[currType][a+1].avgvol);
					first = 0;
				}
				else
				{
					if(fabs(averaged[currType][a].avgvol) <= fabs(dBFS))
						pl_fcont_r(plot.plotter, transformtoLog(averaged[currType][a].avgfreq, config), averaged[currType][a].avgvol);
					else
						break;
				}
			}
			pl_endpath_r(plot.plotter);
		}
		currType++;
	}

	DrawColorAllTypeScale(&plot, MODE_DIFF, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, config->significantAmplitude, VERT_SCALE_STEP_BAR, DRAW_BARS, config);
	DrawLabelsMDF(&plot, DIFFERENCE_AVG_TITLE, ALL_LABEL, PLOT_COMPARE, config);

	ClosePlot(&plot);
}

void DrawFrequencyHorizontalGrid(PlotFile *plot, double hz, double hzIncrement, parameters *config)
{
	pl_pencolor_r (plot->plotter, 0, 0x5555, 0);
	for(int i = hzIncrement; i <= hz; i += hzIncrement)
	{
		double y = 0;

		if(config->logScaleTS)
			y = transformtoLog(i, config);
		else
			y = i;
		pl_fline_r(plot->plotter, 0, y, config->plotResX, y);
	}

	if(config->logScaleTS)
	{
		pl_fline_r(plot->plotter, 0, transformtoLog(10, config), config->plotResX, transformtoLog(10, config));
		pl_fline_r(plot->plotter, 0, transformtoLog(100, config), config->plotResX, transformtoLog(100, config));
	}

	pl_pencolor_r (plot->plotter, 0, 0x7777, 0);
	if(config->endHzPlot >= 10000)
	{
		for(int i = 10000; i < config->endHzPlot; i+= 10000)
		{
			double y = 0;
	
			if(config->logScaleTS)
				y = transformtoLog(i, config);
			else
				y = i;
			pl_fline_r(plot->plotter, 0, y, config->plotResX, y);
		}
	}

	pl_endpath_r(plot->plotter);
}

void DrawLabelsTimeSpectrogram(PlotFile *plot, int khz, int khzIncrement, parameters *config)
{
	double segments = 0, height = 0;
	char label[20];

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0-X0BORDER*config->plotResX*plot->leftmargin, -1*config->plotResY-Y0BORDER*config->plotResY, config->plotResX+X1BORDER*config->plotResX, 0+Y1BORDER*config->plotResY);
	pl_pencolor_r(plot->plotter, 0, 0xaaaa, 0);
	pl_ffontsize_r(plot->plotter, FONT_SIZE_1);

	if(!config->logScaleTS && khz >= 48 && khzIncrement == 1)
		khzIncrement = 2;

	pl_ffontname_r(plot->plotter, PLOT_FONT);
	segments = (double)khz/(double)khzIncrement;
	height = (double)config->plotResY/segments;
	for(int i = segments; i >= 0; i --)
	{
		int 	curkhz = 0;
		double	y = 0;

		curkhz = (int)floor(segments - i)*khzIncrement;
		y = -1*(double)i*height;

		if(config->logScaleTS && curkhz)
			y = -1*(config->plotResY-config->plotResY/(khz*1000)*transformtoLog(curkhz*1000, config));

		pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, y);
		sprintf(label, "%d%s", curkhz, curkhz ? "khz" : "hz");
		pl_alabel_r(plot->plotter, 'l', 'c', label);

		if(config->logScaleTS)
		{
			if(curkhz > 80)
				i -= 40;
			else if(curkhz > 40)
				i -= 20;
			else if(curkhz > 20)
				i -= 10;
			else if(curkhz > 6)
				i -= 2;
		}
	}

	if(config->logScaleTS)
	{
		double y = 0;

		y = -1*(config->plotResY-config->plotResY/(khz*1000)*transformtoLog(100, config));
		pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, y);
		pl_alabel_r(plot->plotter, 'l', 'c', "100hz");

		y = -1*(config->plotResY-config->plotResY/(khz*1000)*transformtoLog(10, config));
		pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, y);
		pl_alabel_r(plot->plotter, 'l', 'c', "10hz");
	}

	pl_restorestate_r(plot->plotter);
}

void DrawTimeCode(PlotFile *plot, double timecode, double x, double framerate, int color, double spaceAvailable, parameters *config)
{
	double	seconds = 0, labelwidth = 0;
	char	time[40];

	seconds = FramesToSeconds(timecode, framerate);

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0-X0BORDER*config->plotResX*plot->leftmargin, -1*config->plotResY/2-Y0BORDER*config->plotResY, config->plotResX+X1BORDER*config->plotResX, config->plotResY/2+Y1BORDER*config->plotResY);
	pl_ffontname_r(plot->plotter, PLOT_FONT);
	pl_ffontsize_r(plot->plotter, FONT_SIZE_2);
	SetPenColor(color, 0xFFFF, plot);
	pl_fmove_r(plot->plotter, x, config->plotResY/2);
	sprintf(time, "%0.1fs", seconds);
	labelwidth = pl_flabelwidth_r(plot->plotter, time);
	if(spaceAvailable >= labelwidth)
		pl_alabel_r(plot->plotter, 'l', 'b', time);
	pl_restorestate_r(plot->plotter);
}

void PlotTimeSpectrogram(AudioSignal *Signal, parameters *config)
{
	PlotFile	plot;
	double		significant = 0, x = 0, framewidth = 0, framecount = 0, tc = 0, abs_significant = 0;
	long int	block = 0, i = 0;
	int			lastType = TYPE_NOTYPE;
	char		filename[BUFFER_SIZE], name[BUFFER_SIZE/2];

	if(!Signal || !config)
		return;

	ShortenFileName(basename(Signal->SourceFile), name);
	sprintf(filename, "T_SP_%c_%s", Signal->role == ROLE_REF ? 'A' : 'B', name);

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type > TYPE_SILENCE)
			framecount += config->types.typeArray[i].elementCount*config->types.typeArray[i].frames;
	}

	if(!framecount)
		return;

	if(config->FullTimeSpectroScale)
		significant = PCM_16BIT_MIN_AMPLITUDE;
	else
		significant = config->significantAmplitude;

	abs_significant = fabs(significant);

	FillPlot(&plot, filename, 0, 0, config->plotResX, config->endHzPlot, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawFrequencyHorizontalGrid(&plot, config->endHzPlot, 1000, config);
	DrawLabelsTimeSpectrogram(&plot, floor(config->endHzPlot/1000), 1, config);

	framewidth = config->plotResX / framecount;
	for(block = 0; block < config->types.totalBlocks; block++)
	{
		int		type = 0;
		double	frames = 0;

		type = GetBlockType(config, block);
		frames = GetBlockFrames(config, block);
		if(type > TYPE_SILENCE && config->MaxFreq > 0)
		{
			int		color = 0;
			double	noteWidth = 0, xpos = 0;

			noteWidth = framewidth*frames;
			xpos = x+noteWidth;
			color = MatchColor(GetBlockColor(config, block));

			for(i = config->MaxFreq-1; i >= 0; i--)
			{
				if(Signal->Blocks[block].freq[i].hertz && Signal->Blocks[block].freq[i].amplitude > significant)
				{
					long int intensity;
					double y, amplitude;

					// x is fixed by block division
					y = Signal->Blocks[block].freq[i].hertz;
					if(config->logScaleTS)
						y = transformtoLog(y, config);
					amplitude = Signal->Blocks[block].freq[i].amplitude;
					
					intensity = CalculateWeightedError(fabs(abs_significant - fabs(amplitude))/abs_significant, config)*0xffff;
					SetPenColor(color, intensity, &plot);
					pl_fline_r(plot.plotter, x,	y, xpos, y);
					pl_endpath_r(plot.plotter);
				}
			}

			if(lastType != type)
			{
				double	spaceAvailable = 0;

				SetPenColor(color, 0x9999, &plot);
				pl_fline_r(plot.plotter, x, 0, x, config->endHzPlot);

				spaceAvailable = noteWidth*GetBlockElements(config, block);
				DrawTimeCode(&plot, tc, x, Signal->framerate, color, spaceAvailable, config);
				lastType = type;
			}
			x += noteWidth;
			tc += frames;
		}
	}

	DrawColorAllTypeScale(&plot, MODE_SPEC, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, significant, VERT_SCALE_STEP_BAR, DRAW_BARS, config);
	DrawLabelsMDF(&plot, Signal->role == ROLE_REF ? TSPECTROGRAM_TITLE_REF : TSPECTROGRAM_TITLE_COM, ALL_LABEL, Signal->role == ROLE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);

	ClosePlot(&plot);
}

void PlotTimeSpectrogramUnMatchedContent(AudioSignal *Signal, parameters *config)
{
	PlotFile	plot;
	double		significant = 0, x = 0, framewidth = 0, framecount = 0, tc = 0, abs_significant = 0;
	long int	block = 0, i = 0;
	int			lastType = TYPE_NOTYPE;
	char		filename[BUFFER_SIZE], name[BUFFER_SIZE/2];

	if(!Signal || !config)
		return;

	ShortenFileName(basename(Signal->SourceFile), name);
	if(Signal->role == ROLE_REF)
		sprintf(filename, "MISSING-A-T_SP_%s", name);
	else
		sprintf(filename, "MISSING-EXTRA_T_SP_%s", name);

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type > TYPE_SILENCE)
			framecount += config->types.typeArray[i].elementCount*config->types.typeArray[i].frames;
	}

	if(!framecount)
		return;

	if(config->FullTimeSpectroScale)
		significant = PCM_16BIT_MIN_AMPLITUDE;
	else
		significant = config->significantAmplitude;
	abs_significant = fabs(significant);

	FillPlot(&plot, filename, 0, 0, config->plotResX, config->endHzPlot, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawFrequencyHorizontalGrid(&plot, config->endHzPlot, 1000, config);
	DrawLabelsTimeSpectrogram(&plot, floor(config->endHzPlot/1000), 1, config);

	framewidth = config->plotResX / framecount;
	for(block = 0; block < config->types.totalBlocks; block++)
	{
		int		type = 0;
		double	frames = 0;

		type = GetBlockType(config, block);
		frames = GetBlockFrames(config, block);
		if(type > TYPE_SILENCE && config->MaxFreq > 0)
		{
			int		color = 0;
			double	noteWidth = 0, xpos = 0;

			noteWidth = framewidth*frames;
			xpos = x+noteWidth;
			color = MatchColor(GetBlockColor(config, block));

			for(i = config->MaxFreq-1; i >= 0; i--)
			{
				if(Signal->Blocks[block].freq[i].hertz && !Signal->Blocks[block].freq[i].matched
					&& Signal->Blocks[block].freq[i].amplitude > significant)
				{
					long int intensity;
					double y, amplitude;

					// x is fixed by block division
					y = Signal->Blocks[block].freq[i].hertz;
					if(config->logScaleTS)
						y = transformtoLog(y, config);
					amplitude = Signal->Blocks[block].freq[i].amplitude;
					
					intensity = CalculateWeightedError(fabs(abs_significant - fabs(amplitude))/abs_significant, config)*0xffff;
					SetPenColor(color, intensity, &plot);
					pl_fline_r(plot.plotter, x,	y, xpos, y);
					pl_endpath_r(plot.plotter);
				}
			}

			if(lastType != type)
			{
				double	spaceAvailable = 0;

				SetPenColor(color, 0x9999, &plot);
				pl_fline_r(plot.plotter, x, 0, x, config->endHzPlot);

				spaceAvailable = noteWidth*GetBlockElements(config, block);
				DrawTimeCode(&plot, tc, x, Signal->framerate, color, spaceAvailable, config);
				lastType = type;
			}
			x += noteWidth;
			tc += frames;
		}
	}

	DrawColorAllTypeScale(&plot, MODE_SPEC, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, significant, VERT_SCALE_STEP_BAR, DRAW_BARS, config);
	DrawLabelsMDF(&plot, Signal->role == ROLE_REF ? EXTRA_TITLE_TS_REF : EXTRA_TITLE_TS_COM, ALL_LABEL, PLOT_COMPARE, config);

	ClosePlot(&plot);
}

void PlotTimeDomainGraphs(AudioSignal *Signal, parameters *config)
{
	long int	plots = 0, i = 0;
	char		name[BUFFER_SIZE*2];

	if(config->plotAllNotes)
	{
		for(i = 0; i < config->types.totalBlocks; i++)
		{
			if(config->plotAllNotes || Signal->Blocks[i].type == TYPE_TIMEDOMAIN)
			{
				plots++;
				/*
				if(config->plotPhase)
					plots++;
				*/
				if(config->plotAllNotesWindowed && Signal->Blocks[i].audio.window_samples)
					plots++;
				if(Signal->Blocks[i].internalSyncCount)
					plots += Signal->Blocks[i].internalSyncCount;
			}
		}
		logmsg("\n  Creating %ld plots for %s:\n  ", plots, Signal->role == ROLE_REF ? "Reference" : "Comparison");
	}
	plots = 0;
	for(i = 0; i < config->types.totalBlocks; i++)
	{
		if(config->plotAllNotes || Signal->Blocks[i].type == TYPE_TIMEDOMAIN || (config->debugSync && Signal->Blocks[i].type == TYPE_SYNC))
		{
			sprintf(name, "TD_%05ld_%s_%s_%05d_%s", 
				i, Signal->role == ROLE_REF ? "1" : "2",
				GetBlockName(config, i), GetBlockSubIndex(config, i), config->compareName);

			PlotBlockTimeDomainGraph(Signal, i, name, WAVEFORM_GENERAL, 0, config);
			logmsg(PLOT_ADVANCE_CHAR);
			plots++;
			if(plots == 80)
			{
				plots = 0;
				logmsg("\n  ");
			}

			if(Signal->Blocks[i].internalSyncCount)
			{
				for(int slot = 0; slot < Signal->Blocks[i].internalSyncCount; slot++)
				{
					sprintf(name, "TD_%05ld_%s_%s_%05d_%s_%02d", 
									i, Signal->role == ROLE_REF ? "1" : "2",
									GetBlockName(config, i), GetBlockSubIndex(config, i), 
									config->compareName, slot);
					PlotBlockTimeDomainInternalSyncGraph(Signal, i, name, slot, config);
				}
			}

#ifdef INDIVPHASE
			/* These are phase graphs for each block */
			if(config->plotPhase)
			{
				sprintf(name, "TD_%05ld_%s_%s_%05d_%s", 
					i, Signal->role == ROLE_REF ? "5" : "6",
					GetBlockName(config, i), GetBlockSubIndex(config, i), config->compareName);
	
				PlotBlockPhaseGraph(Signal, i, name, config);
				logmsg(PLOT_ADVANCE_CHAR);
				plots++;
				if(plots == 80)
				{
					plots = 0;
					logmsg("\n  ");
				}
			}
#endif

			if(config->plotAllNotesWindowed && Signal->Blocks[i].audio.window_samples)
			{
				sprintf(name, "TD_%05ld_%s_%s_%05d_%s", 
					i, Signal->role == ROLE_REF ? "3" : "4",
					GetBlockName(config, i), GetBlockSubIndex(config, i), config->compareName);

				PlotBlockTimeDomainGraph(Signal, i, name, WAVEFORM_WINDOW, 0, config);

				logmsg(PLOT_ADVANCE_CHAR);
				plots++;
				if(plots == 80)
				{
					plots = 0;
					logmsg("\n  ");
				}
			}
		}
	}
	if(!config->plotAllNotes && plots > 40)
		logmsg("\n  ");
}

int ExecutePlotBlockTimeDomainGraph(int waveType, AudioSignal *Signal, long int block, double data, char *folder, parameters *config)
{
	char *returnFolder = NULL;
	char name[BUFFER_SIZE*2];

	returnFolder = PushFolder(folder);
	if(!returnFolder)
		return 0;
	
	sprintf(name, "TD_%05ld_%s_%s_%05d_%s", 
		block, Signal->role == ROLE_REF ? "1" : "2",
		GetBlockName(config, block), GetBlockSubIndex(config, block), config->compareName);

	PlotBlockTimeDomainGraph(Signal, block, name, waveType, data, config);

	ReturnToMainPath(&returnFolder);
	return 1;
}

void PlotTimeDomainHighDifferenceGraphs(AudioSignal *Signal, parameters *config)
{
	int		plots = 0;
	char	*returnFolder = NULL;

	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	returnFolder = PushFolder(WAVEFORMDIFF_FOLDER);
	if(!returnFolder)
		return;

	for(long int b = 0; b < config->types.totalBlocks; b++)
	{
		if(Signal->Blocks[b].type > TYPE_CONTROL)
		{
			double diff = 0;
	
			diff = Signal->Blocks[b].AverageDifference;
			if(diff > 0)
			{
				if(!ExecutePlotBlockTimeDomainGraph(WAVEFORM_AMPDIFF, Signal, b, diff, WAVEFORMDIR_AMPL, config))
				{
					ReturnToMainPath(&returnFolder);
					return;
				}
				logmsg(PLOT_ADVANCE_CHAR);
				plots++;
				if(plots == 80)
				{
					plots = 0;
					logmsg("\n  ");
				}
			}
			
			diff = Signal->Blocks[b].missingPercent;
			if(diff > 0)
			{
				if(!ExecutePlotBlockTimeDomainGraph(WAVEFORM_MISSING, Signal, b, diff, WAVEFORMDIR_MISS, config))
				{
					ReturnToMainPath(&returnFolder);
					return;
				}
				logmsg(PLOT_ADVANCE_CHAR);
				plots++;
				if(plots == 80)
				{
					plots = 0;
					logmsg("\n  ");
				}
			}
	
			diff = Signal->Blocks[b].extraPercent;
			if(diff > 0)
			{
				if(!ExecutePlotBlockTimeDomainGraph(WAVEFORM_EXTRA, Signal, b, diff, WAVEFORMDIR_EXTRA, config))
				{
					ReturnToMainPath(&returnFolder);
					return;
				}
				logmsg(PLOT_ADVANCE_CHAR);
				plots++;
				if(plots == 80)
				{
					plots = 0;
					logmsg("\n  ");
				}
			}
		}
	}
	logmsg("\n  ");
	ReturnToMainPath(&returnFolder);
}

void DrawVerticalFrameGrid(PlotFile *plot, AudioSignal *Signal, double frames, double frameIncrement, double MaxSamples, parameters *config)
{
	int		drawMS = 0;
	double	x = 0, xfactor = 0, segment = 0;

	if(frames <= 10)
		drawMS = 1;

	/* draw ms lines */
	if(drawMS)
	{
		SetPenColor(COLOR_GRAY, 0x4444, plot);
		for(int f = 0; f < frames; f++)
		{
			double offset, factor;

			factor = (double)Signal->header.fmt.SamplesPerSec/1000.0;
			offset = (double)f*Signal->framerate*factor;
			for(int i = 0; i <= Signal->framerate; i += 1)
			{
				x = offset+(double)i*factor;
				pl_fline_r(plot->plotter, x, MININT16, x, MAXINT16);
				pl_endpath_r(plot->plotter);
			}
		}
	}

	/* draw frame lines */
	SetPenColor(COLOR_GREEN, 0x7777, plot);
	for(int i = frameIncrement; i <= frames; i += frameIncrement)
	{
		x = FramesToSamples(i, Signal->header.fmt.SamplesPerSec, Signal->framerate);
		pl_fline_r(plot->plotter, x, MININT16, x, MAXINT16);
		pl_endpath_r(plot->plotter);
	}

	if(frames > 1) {
		if(frames <= 10)
			segment = frames/2;
		else {
			if(frames <= 100) segment = 10;
			else {
				if(frames <= 500) segment = 25;
				else segment = 200;
			}
		}
	}
	else segment = 1;

	SetPenColor(COLOR_GREEN, 0x9999, plot);
	for(int i = 0; i <= frames; i += segment)
	{
		x = FramesToSamples(i, Signal->header.fmt.SamplesPerSec, Signal->framerate);
		pl_fline_r(plot->plotter, x, MININT16, x, MAXINT16);
		pl_endpath_r(plot->plotter);
	}

	/* Draw the labels ¨*/
	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0-X0BORDER*config->plotResX*plot->leftmargin, -1*config->plotResY/2-Y0BORDER*config->plotResY, config->plotResX+X1BORDER*config->plotResX, config->plotResY/2+Y1BORDER*config->plotResY);
	pl_ffontsize_r(plot->plotter, FONT_SIZE_1);
	pl_ffontname_r(plot->plotter, PLOT_FONT);
	SetPenColor(COLOR_GREEN, 0x9999, plot);

	xfactor = config->plotResX/MaxSamples;
	for(int i = 0; i <= frames; i += segment)
	{
		char label[40];

		x = FramesToSamples(i, Signal->header.fmt.SamplesPerSec, Signal->framerate);
		sprintf(label, "Frame %d", i);
		pl_fmove_r(plot->plotter, x*xfactor, config->plotResY/2);
		pl_alabel_r(plot->plotter, 'c', 'b', label);
	}

	pl_restorestate_r(plot->plotter);
}

void DrawINT16DBFSLines(PlotFile *plot, double resx, parameters *config)
{
	char label[20];
	double factor = 0;
	int		dbfs = -3;

	// 0dbfs line
	SetPenColor(COLOR_GRAY, 0x5555, plot);
	pl_fline_r(plot->plotter, 0, 0, resx, 0);

	// 3dbfs lines
	SetPenColor(COLOR_GRAY, 0x3333, plot);
	for(int db = 2; db <= 256; db *= 2)
	{
		double height;

		height = INT16_03DB*2/db;
		pl_fline_r(plot->plotter, 0, height, resx, height);
		pl_fline_r(plot->plotter, 0, -1*height, resx, -1*height);

		height = MAXINT16/db;
		pl_fline_r(plot->plotter, 0, height, resx, height);
		pl_fline_r(plot->plotter, 0, -1*height, resx, -1*height);
	}
 
	pl_endpath_r(plot->plotter);

	/* Draw the labels ¨*/

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0-X0BORDER*config->plotResX*plot->leftmargin, -1*config->plotResY/2-Y0BORDER*config->plotResY, config->plotResX+X1BORDER*config->plotResX, config->plotResY/2+Y1BORDER*config->plotResY);
	pl_ffontsize_r(plot->plotter, FONT_SIZE_1);
	pl_ffontname_r(plot->plotter, PLOT_FONT);
	SetPenColor(COLOR_GRAY, 0x7777, plot);

	factor = config->plotResY/(2*MAXINT16);
	for(int db = 2; db <= 8; db *= 2)
	{
		double height;

		height = INT16_03DB*2/db;
		sprintf(label, "%ddBFS", dbfs);
		pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, height*factor);
		pl_alabel_r(plot->plotter, 'l', 'c', label);
		pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, -1*height*factor);
		pl_alabel_r(plot->plotter, 'l', 'c', label);
		dbfs -= 3;

		height = MAXINT16/db;
		sprintf(label, "%ddBFS", dbfs);
		pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, height*factor);
		pl_alabel_r(plot->plotter, 'l', 'c', label);
		pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, -1*height*factor);
		pl_alabel_r(plot->plotter, 'l', 'c', label);
		dbfs -= 3;
	}

	pl_restorestate_r(plot->plotter);
}

char *GetWFMTypeText(int wftype, char *buffer, double data, int role)
{
	buffer[0] = '\0';

	if(wftype ==  WAVEFORM_WINDOW)
		sprintf(buffer, " -- Windowed");
	if(wftype ==  WAVEFORM_AMPDIFF)
	{
		if(role == ROLE_REF)
			sprintf(buffer, " -- Average Differences in matching comparision: %g dBFS", data);
		else
			sprintf(buffer, " -- Average Differences here: %g dBFS", data);
	}
	if(wftype ==  WAVEFORM_MISSING)
	{
		if(role == ROLE_REF)
			sprintf(buffer, " -- Missing Frequencies in matching comparision: %g%%", data);
		else
			sprintf(buffer, " -- Missing Frequencies from here: %g%%", data);
	}
	if(wftype ==  WAVEFORM_EXTRA)
	{
		if(role == ROLE_REF)
			sprintf(buffer, " -- Extra Frequencies found in matching comparision: %g%%", data);
		else
			sprintf(buffer, " -- Extra Frequencies found here: %g%%", data);
	}
	return buffer;
}

void PlotBlockTimeDomainGraph(AudioSignal *Signal, int block, char *name, int wavetype, double data, parameters *config)
{
	char		title[BUFFER_SIZE/2], buffer[BUFFER_SIZE];
	PlotFile	plot;
	long int	color = 0, sample = 0, numSamples = 0, difference = 0, plotSize = 0;
	int16_t		*samples = NULL;

	if(!Signal || !config)
		return;

	if(block > config->types.totalBlocks)
		return;

	if(wavetype == WAVEFORM_WINDOW)
		samples = Signal->Blocks[block].audio.window_samples;
	else
		samples = Signal->Blocks[block].audio.samples;

	if(!samples)
		return;

	numSamples = Signal->Blocks[block].audio.size;
	difference = Signal->Blocks[block].audio.difference;
	if(difference < 0)
		plotSize += numSamples - difference;
	else
		plotSize = numSamples;

/*
	if(config->plotAllNotes)
	{
		int multiplier = 1;

		if(Signal->Blocks[block].type == TYPE_SYNC)
			multiplier *= 2;
		FillPlotExtra(&plot, name, multiplier*config->plotResX, config->plotResY, 0, MININT16, plotSize, MAXINT16, 1, 0.2, config);
	}
	else
*/
	FillPlot(&plot, name, 0, MININT16, plotSize, MAXINT16, 1, 0.2, config);

	if(!CreatePlotFile(&plot, config))
		return;

	// discarded samples box (difference)
	if(difference > 0 && Signal->Blocks[block].type != TYPE_SYNC)
	{
		pl_filltype_r(plot.plotter, 1);
		pl_pencolor_r(plot.plotter, 0x6666, 0, 0);
		pl_fillcolor_r(plot.plotter, 0x6666, 0, 0);
		pl_fbox_r(plot.plotter, numSamples-difference, MININT16, numSamples-1, MAXINT16);
		pl_filltype_r(plot.plotter, 0);
	}

	DrawVerticalFrameGrid(&plot, Signal, Signal->Blocks[block].frames, 1, plotSize, config);
	DrawINT16DBFSLines(&plot, numSamples, config);

	color = MatchColor(GetBlockColor(config, block));

	// Draw samples
	SetPenColor(color, 0xffff, &plot);
	for(sample = 0; sample < numSamples - 1; sample ++)
		pl_fline_r(plot.plotter, sample, samples[sample], sample+1, samples[sample+1]);
	pl_endpath_r(plot.plotter);

	sprintf(title, "%s# %d%s", GetBlockName(config, block), GetBlockSubIndex(config, block),
			GetWFMTypeText(wavetype, buffer, data, Signal->role));
	DrawLabelsMDF(&plot, Signal->role == ROLE_REF ? WAVEFORM_TITLE_REF : WAVEFORM_TITLE_COM, title, Signal->role == ROLE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);

	ClosePlot(&plot);
}

void PlotBlockTimeDomainInternalSyncGraph(AudioSignal *Signal, int block, char *name, int slot, parameters *config)
{
	char		title[1024];
	PlotFile	plot;
	long int	color = 0, sample = 0, numSamples = 0, difference = 0, plotSize = 0;
	int16_t		*samples = NULL;
	double		frames = 0;

	if(!Signal || !config)
		return;

	if(block > config->types.totalBlocks)
		return;

	if(!Signal->Blocks[block].internalSync)
		return;

	if(slot > Signal->Blocks[block].internalSyncCount - 1)
		return;

	samples = Signal->Blocks[block].internalSync[slot].samples;

	if(!samples)
		return;

	numSamples = Signal->Blocks[block].internalSync[slot].size;
	frames = SamplesToFrames(numSamples, Signal->header.fmt.SamplesPerSec, Signal->framerate);

	difference = Signal->Blocks[block].internalSync[slot].difference;
	if(difference < 0)
		plotSize += numSamples - difference;
	else
		plotSize = numSamples;

/*
	if(config->plotAllNotes)
	{
		int multiplier = 2;

		FillPlotExtra(&plot, name, multiplier*config->plotResX, config->plotResY, 0, MININT16, plotSize, MAXINT16, 1, 0.2, config);
	}
	else
*/
	FillPlot(&plot, name, 0, MININT16, plotSize, MAXINT16, 1, 0.2, config);

	if(!CreatePlotFile(&plot, config))
		return;

	// discarded samples box (difference)
	if(difference > 0 && Signal->Blocks[block].type != TYPE_SYNC)
	{
		pl_filltype_r(plot.plotter, 1);
		pl_pencolor_r(plot.plotter, 0x6666, 0, 0);
		pl_fillcolor_r(plot.plotter, 0x6666, 0, 0);
		pl_fbox_r(plot.plotter, numSamples-difference, MININT16, numSamples-1, MAXINT16);
		pl_filltype_r(plot.plotter, 0);
	}

	DrawVerticalFrameGrid(&plot, Signal, frames, 1, plotSize, config);

	DrawINT16DBFSLines(&plot, numSamples, config);

	color = MatchColor(GetBlockColor(config, block));

	// Draw samples
	SetPenColor(color, 0xffff, &plot);
	for(sample = 0; sample < numSamples - 1; sample ++)
		pl_fline_r(plot.plotter, sample, samples[sample], sample+1, samples[sample+1]);
	pl_endpath_r(plot.plotter);

	sprintf(title, "%s# %d-%d at %g", GetBlockName(config, block), GetBlockSubIndex(config, block),
			slot+1, Signal->framerate);
	DrawLabelsMDF(&plot, Signal->role == ROLE_REF ? WAVEFORM_TITLE_REF : WAVEFORM_TITLE_COM, title, Signal->role == ROLE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);

	ClosePlot(&plot);
}

#ifdef INDIVPHASE
void PlotBlockPhaseGraph(AudioSignal *Signal, int block, char *name, parameters *config)
{
	char		title[1024];
	PlotFile	plot;
	long int	color = 0, f = 0, count = 0;
	Frequency	*freqs = NULL;
	int			oldLog = 0;

	if(!Signal || !config)
		return;

	if(block > config->types.totalBlocks)
		return;

	freqs = Signal->Blocks[block].linFreq;
	count = Signal->Blocks[block].linFreqSize;
	if(!count || !freqs)
		return;

	if(Signal->Blocks[block].type == TYPE_SKIP)
		color = COLOR_RED;

	FillPlot(&plot, name, config->startHzPlot, -1*PHASE_ANGLE, config->endHzPlot, PHASE_ANGLE, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	oldLog = config->logScale;
	config->logScale = 0;

	DrawFrequencyHorizontal(&plot, PHASE_ANGLE, config->endHzPlot, 1000, config);

	// 0 phase line
	color = MatchColor(GetBlockColor(config, block));
	SetPenColor(COLOR_GRAY, 0x7777, &plot);
	pl_fline_r(plot.plotter, config->startHzPlot, 0, config->endHzPlot, 0);
	pl_endpath_r(plot.plotter);

	// Draw phase
	SetPenColor(color, 0xffff, &plot);
	for(f = 0; f < count - 1; f ++)
		pl_fline_r(plot.plotter, transformtoLog(freqs[f].hertz, config), freqs[f].phase, transformtoLog(freqs[f+1].hertz, config), freqs[f+1].phase);
	pl_endpath_r(plot.plotter);

	sprintf(title, "%s# %d at %g", GetBlockName(config, block), GetBlockSubIndex(config, block), Signal->framerate);
	DrawLabelsMDF(&plot, Signal->role == ROLE_REF ? WAVEFORM_TITLE_REF : WAVEFORM_TITLE_COM, title, Signal->role == ROLE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);

	ClosePlot(&plot);

	config->logScale = oldLog;
}
#endif

FlatPhase *CreatePhaseFlatDifferences(parameters *config, long int *size)
{
	long int 			count = 0;
	FlatPhase	*PDiff = NULL;

	if(!size)
		return NULL;

	*size = 0;
	PDiff = (FlatPhase*)malloc(sizeof(FlatPhase)*config->Differences.cntPhaseAudioDiff);
	if(!PDiff)
		return NULL;
	memset(PDiff, 0, sizeof(FlatPhase)*config->Differences.cntPhaseAudioDiff);

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		int type = 0;
		int color = 0;
		
		type = GetBlockType(config, b);
		color = MatchColor(GetBlockColor(config, b));

		for(int p = 0; p < config->Differences.BlockDiffArray[b].cntPhaseBlkDiff; p++)
		{
			PDiff[count].hertz = config->Differences.BlockDiffArray[b].phaseDiffArray[p].hertz;
			PDiff[count].phase = config->Differences.BlockDiffArray[b].phaseDiffArray[p].diffPhase;
			PDiff[count].type = type;
			PDiff[count].color = color;
			count ++;
		}
	}

	logmsg(PLOT_PROCESS_CHAR);
	PhaseDifferencesByFrequency_tim_sort(PDiff, count);
	logmsg(PLOT_PROCESS_CHAR);
	*size = count;
	return PDiff;
}

/*
void PlotPhaseFromSignal(AudioSignal *Signal, parameters *config)
{
	long int			size = 0;
	FlatPhase	*phaseDiff = NULL;
	
	phaseDiff = CreatePhaseFlatFromSignal(Signal, &size, config);
	if(!phaseDiff)
	{
		logmsg("Not enough memory for plotting\n");
		return;
	}

	if(PlotEachTypePhase(phaseDiff, size, config->compareName, Signal->role == ROLE_REF ? PHASE_REF : PHASE_COMP, config) > 1)
	{
		PlotAllPhase(phaseDiff, size, config->compareName, Signal->role == ROLE_REF ? PHASE_REF : PHASE_COMP, config);
		logmsg(PLOT_ADVANCE_CHAR);
	}
	
	free(phaseDiff);
	phaseDiff = NULL;
}
*/

void PlotPhaseDifferences(parameters *config)
{
	long int	size = 0;
	FlatPhase	*phaseDiff = NULL;
	
	phaseDiff = CreatePhaseFlatDifferences(config, &size);
	if(!phaseDiff)
	{
		logmsg("Not enough memory for plotting\n");
		return;
	}

	if(PlotEachTypePhase(phaseDiff, size, config->compareName, PHASE_DIFF, config) > 1)
	{
		PlotAllPhase(phaseDiff, size, config->compareName, PHASE_DIFF, config);
		logmsg(PLOT_ADVANCE_CHAR);
	}
	
	free(phaseDiff);
	phaseDiff = NULL;
}

void PlotAllPhase(FlatPhase *phaseDiff, long int size, char *filename, int pType, parameters *config)
{
	PlotFile	plot;
	char		name[BUFFER_SIZE];

	if(!config)
		return;

	if(!phaseDiff)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	if(pType == PHASE_DIFF)
		sprintf(name, "PHASE_DIFF__ALL_%s", filename);
	else
		sprintf(name, "PHASE__ALL_%c_%s", pType == PHASE_REF ? 'A' : 'B', filename);
	FillPlot(&plot, name, config->startHzPlot, -1*PHASE_ANGLE, config->endHzPlot, PHASE_ANGLE, 1, 0.5, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroAngleCentered(&plot, PHASE_ANGLE, 90, config->endHzPlot, 1000, config);
	DrawLabelsZeroAngleCentered(&plot, PHASE_ANGLE, 90, config->endHzPlot, 1000, config);

	for(int p = 0; p < size; p++)
	{
		if(phaseDiff[p].hertz && phaseDiff[p].type > TYPE_CONTROL)
		{ 
			SetPenColor(phaseDiff[p].color, 0xFFFF, &plot);
			pl_fpoint_r(plot.plotter, transformtoLog(phaseDiff[p].hertz, config), phaseDiff[p].phase);
		}
	}

	DrawColorAllTypeScale(&plot, MODE_SPEC, LEFT_MARGIN, HEIGHT_MARGIN, 0, 0, 0, VERT_SCALE_STEP_BAR, NO_DRAW_BARS, config);
	if(pType == PHASE_DIFF)
		DrawLabelsMDF(&plot, PHASE_DIFF_TITLE, ALL_LABEL, PLOT_COMPARE, config);
	else
		DrawLabelsMDF(&plot, pType == PHASE_REF ? PHASE_SIG_TITLE_REF : PHASE_SIG_TITLE_COM, ALL_LABEL, pType == PHASE_REF  ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);

	ClosePlot(&plot);
}

int PlotEachTypePhase(FlatPhase *phaseDiff, long int size, char *filename, int pType, parameters *config)
{
	int 		i = 0, type = 0, types = 0, typeCount = 0;
	char		name[BUFFER_SIZE];

	typeCount = GetActiveBlockTypesNoRepeat(config);
	for(i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;

		if(type > TYPE_CONTROL && !config->types.typeArray[i].IsaddOnData)
		{
			char	*returnFolder = NULL;

			if(typeCount > 1)
			{
				returnFolder = PushFolder(PHASE_FOLDER);
				if(!returnFolder)
					return 0;
			}

			if(pType == PHASE_DIFF)
				sprintf(name, "PHASE_DIFF_%s_%02d%s", filename, 
					type, config->types.typeArray[i].typeName);
			else
				sprintf(name, "PHASE_%c_%s_%02d%s", pType == PHASE_REF ? 'A' : 'B', filename, 
					type, config->types.typeArray[i].typeName);
		
			PlotSingleTypePhase(phaseDiff, size, type, name, pType, config);
			logmsg(PLOT_ADVANCE_CHAR);

			if(typeCount > 1)
				ReturnToMainPath(&returnFolder);

			types ++;
		}
	}
	return types;
}

void PlotSingleTypePhase(FlatPhase *phaseDiff, long int size, int type, char *filename, int pType, parameters *config)
{
	PlotFile	plot;

	if(!config)
		return;

	if(!phaseDiff)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	FillPlot(&plot, filename, config->startHzPlot, -1*PHASE_ANGLE, config->endHzPlot, PHASE_ANGLE, 1, 0.5, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroAngleCentered(&plot, PHASE_ANGLE, 90, config->endHzPlot, 1000, config);
	DrawLabelsZeroAngleCentered(&plot, PHASE_ANGLE, 90, config->endHzPlot, 1000, config);

	for(int p = 0; p < size; p++)
	{
		if(phaseDiff[p].hertz && phaseDiff[p].type == type)
		{ 
			SetPenColor(phaseDiff[p].color, 0xFFFF, &plot);
			pl_fpoint_r(plot.plotter, transformtoLog(phaseDiff[p].hertz, config), phaseDiff[p].phase);
		}
	}

	if(pType == PHASE_DIFF)
		DrawLabelsMDF(&plot, PHASE_DIFF_TITLE, GetTypeDisplayName(config, type), PLOT_COMPARE, config);
	else
		DrawLabelsMDF(&plot, pType == PHASE_REF ? PHASE_SIG_TITLE_REF : PHASE_SIG_TITLE_COM, GetTypeDisplayName(config, type), pType == PHASE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);
	ClosePlot(&plot);
}

void DrawGridZeroAngleCentered(PlotFile *plot, double maxAngle, double angleIncrement, double hz, double hzIncrement, parameters *config)
{
	pl_pencolor_r (plot->plotter, 0, 0xaaaa, 0);
	pl_fline_r(plot->plotter, 0, 0, hz, 0);
	pl_endpath_r(plot->plotter);

	pl_pencolor_r (plot->plotter, 0, 0x5555, 0);
	for(int i = angleIncrement; i < maxAngle; i += angleIncrement)
	{
		pl_fline_r(plot->plotter, 0, i, hz, i);
		pl_fline_r(plot->plotter, 0, -1*i, hz, -1*i);
	}
	pl_endpath_r(plot->plotter);

	DrawFrequencyHorizontal(plot, maxAngle, hz, 1000, config);

	pl_endpath_r(plot->plotter);
	pl_pencolor_r (plot->plotter, 0, 0xFFFF, 0);
}

void DrawLabelsZeroAngleCentered(PlotFile *plot, double maxAngle, double angleIncrement, double hz, double hzIncrement,  parameters *config)
{
	double segments = 0;
	char label[20];

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0-X0BORDER*config->plotResX*plot->leftmargin, -1*config->plotResY/2-Y0BORDER*config->plotResY, config->plotResX+X1BORDER*config->plotResX, config->plotResY/2+Y1BORDER*config->plotResY);

	pl_ffontname_r(plot->plotter, PLOT_FONT);
	pl_ffontsize_r(plot->plotter, FONT_SIZE_1);

	pl_pencolor_r(plot->plotter, 0, 0xffff	, 0);
	pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, config->plotResY/100);
	pl_alabel_r(plot->plotter, 'l', 't', "0\\de");

	pl_pencolor_r(plot->plotter, 0, 0xaaaa, 0);
	segments = fabs(maxAngle/angleIncrement);
	for(int i = 1; i < segments; i ++)
	{
		pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, i*config->plotResY/segments/2+config->plotResY/100);
		sprintf(label, " %g\\de", i*angleIncrement);
		pl_alabel_r(plot->plotter, 'l', 't', label);

		pl_fmove_r(plot->plotter, config->plotResX+PLOT_SPACER, -1*i*config->plotResY/segments/2+config->plotResY/100);
		sprintf(label, "-%g\\de", i*angleIncrement);
		pl_alabel_r(plot->plotter, 'l', 't', label);
	}

	if(config->logScale)
	{
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(10, config), config->plotResY/2);
		sprintf(label, "%dHz", 10);
		pl_alabel_r(plot->plotter, 'c', 'b', label);
	
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(100, config), config->plotResY/2);
		sprintf(label, "%dHz", 100);
		pl_alabel_r(plot->plotter, 'c', 'b', label);
	}

	pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(1000, config), config->plotResY/2);
	sprintf(label, "  %dHz", 1000);
	pl_alabel_r(plot->plotter, 'c', 'b', label);

	if(config->endHzPlot >= 10000)
	{
		for(int i = 10000; i < config->endHzPlot; i+= 10000)
		{
			pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(i, config), config->plotResY/2);
			sprintf(label, "%d%s", i/1000, i >= 40000  ? "" : "khz");
			pl_alabel_r(plot->plotter, 'c', 'b', label);
		}
	}

	pl_restorestate_r(plot->plotter);
}

/*
FlatPhase	*CreatePhaseFlatFromSignal(AudioSignal *Signal, long int *size, parameters *config)
{
	long int	block = 0, i = 0;
	long int 	count = 0;
	FlatPhase	*PhaseArray = NULL;

	if(!size || !Signal || !config)
		return NULL;

	*size = 0;

	for(block = 0; block < config->types.totalBlocks; block++)
	{
		int 	type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(type > TYPE_SILENCE)
		{
			for(i = 0; i < config->MaxFreq; i++)
			{
				if(Signal->Blocks[block].freq[i].hertz)
					count ++;
				else
					break;
			}
		}
	}

	PhaseArray = (FlatPhase*)malloc(sizeof(FlatPhase)*count);
	if(!PhaseArray)
		return NULL;
	memset(PhaseArray, 0, sizeof(FlatPhase)*count);

	count = 0;
	for(block = 0; block < config->types.totalBlocks; block++)
	{
		int type = 0;
		int color = 0;
		
		type = GetBlockType(config, block);
		color = MatchColor(GetBlockColor(config, block));

		if(type > TYPE_SILENCE)
		{
			for(i = 0; i < config->MaxFreq; i++)
			{
				if(Signal->Blocks[block].freq[i].hertz)
				{
					PhaseArray[count].hertz = Signal->Blocks[block].freq[i].hertz;
					PhaseArray[count].phase = Signal->Blocks[block].freq[i].phase;
					PhaseArray[count].type = type;
					PhaseArray[count].color = color;
					count ++;
				}
				else
					break;
			}
		}
	}

	logmsg(PLOT_PROCESS_CHAR);
	PhaseDifferencesByFrequency_tim_sort(PhaseArray, count);
	logmsg(PLOT_PROCESS_CHAR);
	*size = count;
	return PhaseArray;
}
*/

void PlotDifferenceTimeSpectrogram(parameters *config)
{
	PlotFile	plot;
	double		x = 0, framewidth = 0, framecount = 0, tc = 0, abs_significant = 0, steps = 0;
	double		outside = 0, maxDiff = 0;
	long int	block = 0, i = 0;
	int			lastType = TYPE_NOTYPE, type = 0;
	char		filename[2*BUFFER_SIZE];


	if(!config || !config->Differences.BlockDiffArray)
		return;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type > TYPE_SILENCE)
			framecount += config->types.typeArray[i].elementCount*config->types.typeArray[i].frames;
	}

	if(!framecount)
		return;

	/* This calculates the viewport in dBFS */
	steps = 1.0;
	abs_significant = fabs(config->maxDbPlotZC);
	outside = FindDifferencePercentOutsideViewPort(&maxDiff, &type, abs_significant, config);
	if(outside == 0 && fabs(maxDiff) && fabs(maxDiff) > abs_significant*2)
	{
		abs_significant = abs_significant/2;
		do
		{
			outside = FindDifferencePercentOutsideViewPort(&maxDiff, &type, abs_significant, config);
			if(outside == 0)
				abs_significant = abs_significant/2;
		}while(!outside && fabs(maxDiff) && fabs(maxDiff) > abs_significant*2);
	}

	if(outside >= 10)
		abs_significant = abs_significant*1.5;

	if(abs_significant > VERT_SCALE_STEP_BAR*10)
		steps = VERT_SCALE_STEP_BAR;
	else
		steps = abs_significant / 10;

	sprintf(filename, "DA__ALL_TS_%s", config->compareName);
	FillPlot(&plot, filename, 0, 0, config->plotResX, config->endHzPlot, 1, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawFrequencyHorizontalGrid(&plot, config->endHzPlot, 1000, config);
	DrawLabelsTimeSpectrogram(&plot, floor(config->endHzPlot/1000), 1, config);

	framewidth = config->plotResX / framecount;
	for(block = 0; block < config->types.totalBlocks; block++)
	{
		int		type = 0;
		double	frames = 0;

		type = GetBlockType(config, block);
		frames = GetBlockFrames(config, block);
		if(type > TYPE_SILENCE && config->MaxFreq > 0)
		{
			int				color = 0;
			long int		count = 0;
			double			noteWidth = 0, xpos = 0;

			noteWidth = framewidth*frames;
			xpos = x+noteWidth;
			color = MatchColor(GetBlockColor(config, block));
			count = config->Differences.BlockDiffArray[block].cntAmplBlkDiff;
			if(count)
			{
				AmplDifference 	*amplDiffArraySorted = NULL;

				amplDiffArraySorted = (AmplDifference*)malloc(sizeof(AmplDifference)*count);
				if(!amplDiffArraySorted)
				{
					logmsg("ERROR: Not enough memory (amplDiffArraySorted)\n");
					return;
				}

				memcpy(amplDiffArraySorted, config->Differences.BlockDiffArray[block].amplDiffArray, sizeof(AmplDifference)*count);
				AmplitudeDifferencesBlock_tim_sort(amplDiffArraySorted, count);

				for(i = count - 1; i >= 0; i--)
				{
					double refAmplitude = 0, amplitude = 0;
	
					refAmplitude = amplDiffArraySorted[i].refAmplitude;
					amplitude = amplDiffArraySorted[i].diffAmplitude;
					if(refAmplitude > config->significantAmplitude && fabs(amplitude) <= abs_significant)
					{
						long int intensity;
						double y;
		
						// x is fixed by block division
						y = amplDiffArraySorted[i].hertz;
						if(config->logScaleTS)
							y = transformtoLog(y, config);
						
						intensity = 0xffff*CalculateWeightedError(1.0-(fabs(abs_significant - fabs(amplitude))/abs_significant), config);
						/*
						if(1.0-(fabs(abs_significant - fabs(amplitude))/abs_significant) >= 0.5)
							logmsgFileOnly("%ghz: %g %g 0x%X [%g=1-(%g-%g)/%g]\n", y,
								amplitude, abs_significant, intensity, 1.0-(fabs(abs_significant - fabs(amplitude))/abs_significant), abs_significant, fabs(amplitude), abs_significant);
						*/
						SetPenColor(color, intensity, &plot);
						pl_fline_r(plot.plotter, x, y, xpos, y);
						pl_endpath_r(plot.plotter);
					}
				}

				free(amplDiffArraySorted);
			}

			if(lastType != type)
			{
				double	spaceAvailable = 0;

				SetPenColor(color, 0x9999, &plot);
				pl_fline_r(plot.plotter, x, 0, x, config->endHzPlot);

				spaceAvailable = noteWidth*GetBlockElements(config, block);
				DrawTimeCode(&plot, tc, x, config->smallerFramerate, color, spaceAvailable, config);
				lastType = type;
			}
			x += noteWidth;
			tc += frames;
		}
	}

	DrawColorAllTypeScale(&plot, MODE_TSDIFF, LEFT_MARGIN, HEIGHT_MARGIN, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, abs_significant, steps, DRAW_BARS, config);
	DrawLabelsMDF(&plot, TS_DIFFERENCE_TITLE, ALL_LABEL, PLOT_COMPARE, config);

	ClosePlot(&plot);
}

