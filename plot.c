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

#define SORT_NAME FlatMissingDifferencesByAmplitude
#define SORT_TYPE FlatFreqDifference
#define SORT_CMP(x, y)  ((x).amplitude < (y).amplitude ? -1 : ((x).amplitude == (y).amplitude ? 0 : 1))
#include "sort.h"  // https://github.com/swenson/sort/

#define SORT_NAME FlatFrequenciesByAmplitude
#define SORT_TYPE FlatFrequency
#define SORT_CMP(x, y)  ((x).amplitude < (y).amplitude ? -1 : ((x).amplitude == (y).amplitude ? 0 : 1))
#include "sort.h"  // https://github.com/swenson/sort/

#define DIFFERENCE_TITLE			"DIFFERENT AMPLITUDES [%s]"
#define MISSING_TITLE				"MISSING FREQUENCIES [%s]"
#define EXTRA_TITLE_TS_REF			"COMPARISON - MISSING FREQUENCIES - TIME SPECTROGRAM [%s] (Frequencies expected in Comparison file from the Reference template)"
#define EXTRA_TITLE_TS_COM			"COMPARISON - EXTRA FREQUENCIES - TIME SPECTROGRAM [%s] (Frequencies found in Comparison File and not in Reference file)"
#define SPECTROGRAM_TITLE_REF		"REFERENCE - SPECTROGRAM [%s]"
#define SPECTROGRAM_TITLE_COM		"COMPARISON - SPECTROGRAM [%s]"
#define TSPECTROGRAM_TITLE_REF		"REFERENCE - TIME SPECTROGRAM [%s]"
#define TSPECTROGRAM_TITLE_COM		"COMPARISON - TIME SPECTROGRAM [%s]"
#define DIFFERENCE_AVG_TITLE		"DIFFERENT AMPLITUDES AVERAGED [%s]"
#define NOISE_TITLE					"NOISE FLOOR AVERAGED"
#define NOISE_AVG_TITLE				"NOISE FLOOR AVERAGED"
#define SPECTROGRAM_NOISE_REF		"REFERENCE NOISE FLOOR - SPECTROGRAM [%s]"
#define SPECTROGRAM_NOISE_COM		"COMPARISON NOISE FLOOR - SPECTROGRAM [%s]"

#define BAR_HEADER					"Matched frequencies"
#define BAR_DIFF					"w/any amplitude difference"
#define BAR_WITHIN					"within %gdBFS"

#define ALL_LABEL					"ALL"

#define X0BORDER	200
#define Y0BORDER 	100
#define X1BORDER	80
#define Y1BORDER 	50

#if defined (WIN32)
	#include <direct.h>
	#define GetCurrentDir _getcwd
#else
	#include <unistd.h>
	#define GetCurrentDir getcwd
#endif

#define BAR_WIDTH		config->plotResX/40
#define	BAR_HEIGHT		config->plotResY/60

#define	VERT_SCALE_STEP			3
#define	VERT_SCALE_STEP_BAR		3

#define	COLOR_BARS_WIDTH_SCALE	120

char *GetCurrentPathAndChangeToResultsFolder(parameters *config)
{
	char 	*CurrentPath = NULL;

	CurrentPath = (char*)malloc(sizeof(char)*FILENAME_MAX);
	if(!CurrentPath)
		return NULL;

	if (!GetCurrentDir(CurrentPath, sizeof(char)*FILENAME_MAX))
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

void PlotResults(AudioSignal *ReferenceSignal, AudioSignal *ComparisonSignal, parameters *config)
{
	struct	timespec	start, end;
	char 	*CurrentPath = NULL;

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	CurrentPath = GetCurrentPathAndChangeToResultsFolder(config);

	if(config->plotDifferences || config->averagePlot)
	{
		struct	timespec	lstart, lend;

		if(config->clock)
			clock_gettime(CLOCK_MONOTONIC, &lstart);

		logmsg(" - Difference");
		PlotAmpDifferences(config);
		logmsg("\n");

		if(config->clock)
		{
			double	elapsedSeconds;
			clock_gettime(CLOCK_MONOTONIC, &lend);
			elapsedSeconds = TimeSpecToSeconds(&lend) - TimeSpecToSeconds(&lstart);
			logmsg(" - clk: Differences took %0.2fs\n", elapsedSeconds);
		}
	}

	if(config->plotMissing)
	{
		if(!config->FullTimeSpectroScale)
		{
			struct	timespec	lstart, lend;
	
			if(config->clock)
				clock_gettime(CLOCK_MONOTONIC, &lstart);
	
			logmsg(" - Missing and Extra Frequencies");
	
			// Old Missig plots, rendered obsolete?
			//PlotFreqMissing(config);
			PlotTimeSpectrogramUnMatchedContent(ReferenceSignal, config);
			logmsg(PLOT_ADVANCE_CHAR);
			PlotTimeSpectrogramUnMatchedContent(ComparisonSignal, config);
			logmsg(PLOT_ADVANCE_CHAR);
	
			logmsg("\n");
	
			if(config->clock)
			{
				double	elapsedSeconds;
				clock_gettime(CLOCK_MONOTONIC, &lend);
				elapsedSeconds = TimeSpecToSeconds(&lend) - TimeSpecToSeconds(&lstart);
				logmsg(" - clk: Missing took %0.2fs\n", elapsedSeconds);
			}
		}
		else
			logmsg(" X Skipped: Missing and Extra Frequencies, due to range\n");
	}

	if(config->plotSpectrogram)
	{
		struct	timespec	lstart, lend;

		if(config->clock)
			clock_gettime(CLOCK_MONOTONIC, &lstart);

		logmsg(" - Spectrograms");
		PlotSpectrograms(ReferenceSignal, config);
		PlotSpectrograms(ComparisonSignal, config);
		logmsg("\n");


		if(config->clock)
		{
			double	elapsedSeconds;
			clock_gettime(CLOCK_MONOTONIC, &lend);
			elapsedSeconds = TimeSpecToSeconds(&lend) - TimeSpecToSeconds(&lstart);
			logmsg(" - clk: Spectrogram took %0.2fs\n", elapsedSeconds);
		}
	}

	if(config->plotTimeSpectrogram)
	{
		struct	timespec	lstart, lend;

		if(config->clock)
			clock_gettime(CLOCK_MONOTONIC, &lstart);

		logmsg(" - Time Spectrogram");
		PlotTimeSpectrogram(ReferenceSignal, config);
		logmsg(PLOT_ADVANCE_CHAR);
		PlotTimeSpectrogram(ComparisonSignal, config);
		logmsg(PLOT_ADVANCE_CHAR);
		logmsg("\n");


		if(config->clock)
		{
			double	elapsedSeconds;
			clock_gettime(CLOCK_MONOTONIC, &lend);
			elapsedSeconds = TimeSpecToSeconds(&lend) - TimeSpecToSeconds(&lstart);
			logmsg(" - clk: Time Spectrogram took %0.2fs\n", elapsedSeconds);
		}
	}

	if(config->plotNoiseFloor)
	{
		if(!config->noSyncProfile && !config->ignoreFloor)
		{
			if(ReferenceSignal->hasFloor && ComparisonSignal->hasFloor)
			{
				struct	timespec	lstart, lend;
		
				if(config->clock)
					clock_gettime(CLOCK_MONOTONIC, &lstart);
		
				logmsg(" - Noise Floor");
				PlotNoiseFloor(ReferenceSignal, config);
				logmsg("\n");
		
		
				if(config->clock)
				{
					double	elapsedSeconds;
					clock_gettime(CLOCK_MONOTONIC, &lend);
					elapsedSeconds = TimeSpecToSeconds(&lend) - TimeSpecToSeconds(&lstart);
					logmsg(" - clk: Noise Floor took %0.2fs\n", elapsedSeconds);
				}
			}
			else
				logmsg(" X Noise Floor graphs ommited: no noise floor value found.\n");
		}
		else
			logmsg(" X Noise floor plots make no sense with current parameters.\n");
	}

	ReturnToMainPath(&CurrentPath);

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
		if(PlotEachTypeDifferentAmplitudes(amplDiff, size, config->compareName, config) > 1)
		{
			PlotAllDifferentAmplitudes(amplDiff, size, config->compareName, config);
			logmsg(PLOT_ADVANCE_CHAR);
		}
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
		CreateBaseName(config);
		PlotAllDifferentAmplitudes(amplDiff, size, config->compareName, config);
	}

	free(amplDiff);
	amplDiff = NULL;
}

void PlotFreqMissing(parameters *config)
{
	long int			size = 0;
	FlatFreqDifference	*freqDiff = NULL;
	
	freqDiff = CreateFlatMissing(config, &size);
	if(PlotEachTypeMissingFrequencies(freqDiff, size, config->compareName, config) > 1)
	{
		PlotAllMissingFrequencies(freqDiff, size, config->compareName, config);
		logmsg(PLOT_ADVANCE_CHAR);
	}

	free(freqDiff);
	freqDiff = NULL;
}

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

int FillPlot(PlotFile *plot, char *name, int sizex, int sizey, double x0, double y0, double x1, double y1, double penWidth, parameters *config)
{
	double dX = 0, dY = 0;

	plot->plotter = NULL;
	plot->plotter_params = NULL;
	plot->file = NULL;

	ComposeFileNameoPath(plot->FileName, name, ".png", config);

	plot->sizex = sizex;
	plot->sizey = sizey;

	plot->Rx0 = x0;
	plot->Rx1 = x1;
	plot->Ry0 = y0;
	plot->Ry1 = y1;

	// Commented for partial release
	//dX = X0BORDER*fabs(x0 - x1)/sizex;
	//dY = Y0BORDER*fabs(y0 - y1)/sizey;

	// Commented for partial release
	plot->x0 = x0-dX;
	plot->y0 = y0-dY;

	//dX = X1BORDER*fabs(x0 - x1)/sizex;
	//dY = Y1BORDER*fabs(y0 - y1)/sizey;

	plot->x1 = x1+dX;
	plot->y1 = y1+dY;

	plot->penWidth = penWidth;

	//logmsg("Plot %g %g -> %g %g\n", plot->x0, plot->y0, plot->x1, plot->y1);
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

void DrawGridZeroDBCentered(PlotFile *plot, double dBFS, double dbIncrement, double hz, double hzIncrement, parameters *config)
{
	pl_pencolor_r (plot->plotter, 0, 0xaaaa, 0);
	pl_fline_r(plot->plotter, 0, 0, hz, 0);

	pl_pencolor_r (plot->plotter, 0, 0x5555, 0);
	for(int i = dbIncrement; i < dBFS; i += dbIncrement)
	{
		pl_fline_r(plot->plotter, 0, i, hz, i);
		pl_fline_r(plot->plotter, 0, -1*i, hz, -1*i);
	}

	pl_pencolor_r (plot->plotter, 0, 0x5555, 0);
	for(int i = hzIncrement; i < hz; i += hzIncrement)
		pl_fline_r(plot->plotter, transformtoLog(i, config), -1*dBFS, transformtoLog(i, config), dBFS);

	pl_pencolor_r (plot->plotter, 0, 0x7777, 0);
	if(config->logScale)
	{
		pl_fline_r(plot->plotter, transformtoLog(10, config), -1*dBFS, transformtoLog(10, config), dBFS);
		pl_fline_r(plot->plotter, transformtoLog(100, config), -1*dBFS, transformtoLog(100, config), dBFS);
	}
	pl_fline_r(plot->plotter, transformtoLog(1000, config), -1*dBFS, transformtoLog(1000, config), dBFS);
	pl_fline_r(plot->plotter, transformtoLog(10000, config), -1*dBFS, transformtoLog(10000, config), dBFS);
	if(config->endHzPlot > 20000)
		pl_fline_r(plot->plotter, transformtoLog(20000, config), -1*dBFS, transformtoLog(20000, config), dBFS);

	if(config->averageLine != 0.0)
	{
		pl_pencolor_r (plot->plotter, 0xAAAA, 0xAAAA, 0);
		pl_linemod_r(plot->plotter, "dotdashed");
		pl_fline_r(plot->plotter, 0, -1.0*config->averageLine, hz, -1.0*config->averageLine);
		pl_linemod_r(plot->plotter, "solid");
	}

	pl_endpath_r(plot->plotter);
	pl_pencolor_r (plot->plotter, 0, 0xFFFF, 0);
}

void DrawGridZeroToLimit(PlotFile *plot, double dBFS, double dbIncrement, double hz, double hzIncrement, parameters *config)
{
	pl_pencolor_r (plot->plotter, 0, 0x5555, 0);
	for(int i = dbIncrement; i < fabs(dBFS); i += dbIncrement)
		pl_fline_r(plot->plotter, 0, -1*i, hz, -1*i);

	pl_pencolor_r (plot->plotter, 0, 0x5555, 0);
	for(int i = hzIncrement; i < hz; i += hzIncrement)
		pl_fline_r(plot->plotter, transformtoLog(i, config), dBFS, transformtoLog(i, config), 0);

	pl_pencolor_r (plot->plotter, 0, 0x7777, 0);
	if(config->logScale)
	{
		pl_fline_r(plot->plotter, transformtoLog(10, config), dBFS, transformtoLog(10, config), 0);
		pl_fline_r(plot->plotter, transformtoLog(100, config), dBFS, transformtoLog(100, config), 0);
	}
	pl_fline_r(plot->plotter, transformtoLog(1000, config), dBFS, transformtoLog(1000, config), 0);
	pl_fline_r(plot->plotter, transformtoLog(10000, config), dBFS, transformtoLog(10000, config), 0);	
	if(config->endHzPlot > 20000)
		pl_fline_r(plot->plotter, transformtoLog(20000, config), dBFS, transformtoLog(20000, config), 0);	

	pl_pencolor_r (plot->plotter, 0, 0xFFFF, 0);
	pl_flinewidth_r(plot->plotter, 1);
	pl_endpath_r(plot->plotter);
}

void DrawLabelsZeroDBCentered(PlotFile *plot, double dBFS, double dbIncrement, double hz, double hzIncrement,  parameters *config)
{
	double segments = 0;
	char label[20];

	pl_savestate_r(plot->plotter);
	pl_fspace_r(plot->plotter, 0, -1*config->plotResY/2, config->plotResX, config->plotResY/2);

	pl_ffontname_r(plot->plotter, "HersheySans");
	pl_ffontsize_r(plot->plotter, config->plotResY/60);

	pl_pencolor_r(plot->plotter, 0, 0xffff	, 0);
	pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/80, config->plotResY/100);
	pl_alabel_r(plot->plotter, 'c', 'c', "0dBFS");

	pl_pencolor_r(plot->plotter, 0, 0xaaaa, 0);
	segments = fabs(dBFS/dbIncrement);
	for(int i = 1; i < segments; i ++)
	{
		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/50, i*config->plotResY/segments/2+config->plotResY/100);
		sprintf(label, " %gdBFS", i*dbIncrement);
		pl_alabel_r(plot->plotter, 'c', 'c', label);

		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/50, -1*i*config->plotResY/segments/2+config->plotResY/100);
		sprintf(label, "-%gdBFS", i*dbIncrement);
		pl_alabel_r(plot->plotter, 'c', 'c', label);
	}

	if(config->logScale)
	{
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(10, config), config->plotResY/2-config->plotResY/100);
		sprintf(label, "%dHz", 10);
		pl_alabel_r(plot->plotter, 'c', 'c', label);
	
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(100, config), config->plotResY/2-config->plotResY/100);
		sprintf(label, "%dHz", 100);
		pl_alabel_r(plot->plotter, 'c', 'c', label);
	}

	pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(1000, config), config->plotResY/2-config->plotResY/100);
	sprintf(label, "  %dHz", 1000);
	pl_alabel_r(plot->plotter, 'c', 'c', label);

	pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(10000, config), config->plotResY/2-config->plotResY/100);
	sprintf(label, "%dkHz", 10);
	pl_alabel_r(plot->plotter, 'c', 'c', label);

	if(config->endHzPlot > 20000)
	{
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(20000, config), config->plotResY/2-config->plotResY/100);
		sprintf(label, "%dkHz", 20);
		pl_alabel_r(plot->plotter, 'c', 'c', label);
	}

	pl_restorestate_r(plot->plotter);
	//pl_fspace_r(plot->plotter, plot->x0, plot->y0, plot->x1, plot->y1);
}

void DrawLabelsMDF(PlotFile *plot, char *Gname, char *GType, int type, parameters *config)
{
	char	label[BUFFER_SIZE];

	pl_ffontsize_r(plot->plotter, config->plotResY/60);
	pl_ffontname_r(plot->plotter, "HersheySans");

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
	pl_fmove_r(plot->plotter, config->plotResX/45, -1*config->plotResY/2+config->plotResY/100);
	pl_pencolor_r(plot->plotter, 0, 0xcccc, 0);
	pl_alabel_r(plot->plotter, 'l', 'l', "MDFourier "MDVERSION" for 240p Test Suite by Artemio Urbina");

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
			pl_alabel_r(plot->plotter, 'l', 'l', "UNKOWN");
			break;
	}
	/* File information */
	if(config->labelNames)
	{
		if(type == PLOT_COMPARE)
		{
			pl_pencolor_r(plot->plotter, 0, 0xeeee, 0);
			if(config->referenceSignal)
			{
				sprintf(label, "Reference:   %.94s", basename(config->referenceFile));
				pl_fmove_r(plot->plotter, config->plotResX/2-config->plotResX/50*9, -1*config->plotResY/2+config->plotResY/80+config->plotResY/40);
				pl_alabel_r(plot->plotter, 'l', 'l', label);

				sprintf(label, "[%0.4fms %0.4fHz]", config->referenceSignal->framerate, roundFloat(CalculateScanRate(config->referenceSignal)));
				pl_fmove_r(plot->plotter, config->plotResX/20*17, -1*config->plotResY/2+config->plotResY/80+config->plotResY/40);
				pl_alabel_r(plot->plotter, 'l', 'l', label);
			}
			else
			{
				sprintf(label, "Reference:   %.94s", basename(config->referenceFile));
				pl_fmove_r(plot->plotter, config->plotResX/2-config->plotResX/10, -1*config->plotResY/2+config->plotResY/80+config->plotResY/40);
				pl_alabel_r(plot->plotter, 'l', 'l', label);
			}
			if(config->comparisonSignal)
			{
				sprintf(label, "Comparison: %s", basename(config->targetFile));
				pl_fmove_r(plot->plotter, config->plotResX/2-config->plotResX/50*9, -1*config->plotResY/2+config->plotResY/80);
				pl_alabel_r(plot->plotter, 'l', 'l', label);

				sprintf(label, "[%0.4fms %0.4fHz]", config->comparisonSignal->framerate, roundFloat(CalculateScanRate(config->comparisonSignal)));
				pl_fmove_r(plot->plotter, config->plotResX/20*17, -1*config->plotResY/2+config->plotResY/80);
				pl_alabel_r(plot->plotter, 'l', 'l', label);
			}
			else
			{
				sprintf(label, "Comparison: %s", basename(config->targetFile));
				pl_fmove_r(plot->plotter, config->plotResX/2-config->plotResX/10, -1*config->plotResY/2+config->plotResY/80);
				pl_alabel_r(plot->plotter, 'l', 'l', label);
			}
		}
		else
		{
			double width = 0, x = 0, y = 0;
	
			x = config->plotResX/2-config->plotResX/10;
			y = -1*config->plotResY/2+config->plotResY/80;
			if(type == PLOT_SINGLE_REF)
				sprintf(label, "File: %s", basename(config->referenceFile));
			else
				sprintf(label, "File: %s", basename(config->targetFile));
	
			pl_filltype_r(plot->plotter, 1);
			pl_pencolor_r(plot->plotter, 0, 0, 0);
			pl_fillcolor_r(plot->plotter, 0, 0, 0);
			width = pl_flabelwidth_r(plot->plotter, label);
			pl_fbox_r(plot->plotter, x, y, x+width, y+config->plotResY/80);
			pl_filltype_r(plot->plotter, 0);
	
			pl_pencolor_r(plot->plotter, 0, 0xeeee, 0);
			pl_fmove_r(plot->plotter, x, y);
			pl_alabel_r(plot->plotter, 'l', 'l', label);
		}
	}

	/* Warnings */
	// Add from top, change last multiplier for BAR HEIGHT

	if(config->AmpBarRange > BAR_DIFF_DB_TOLERANCE)
	{
		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/5, -1*config->plotResY/2+config->plotResY/100+10*BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
		pl_alabel_r(plot->plotter, 'l', 'l', "WARNING: Tolerance raised for matches");
	}

	if(config->smallFile)
	{
		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/5, -1*config->plotResY/2+config->plotResY/100+8*BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
		pl_alabel_r(plot->plotter, 'l', 'l', "WARNING: File was shorter than expected");
	}

	if(config->normType != max_frequency)
	{
		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/5, -1*config->plotResY/2+config->plotResY/100+6*BAR_HEIGHT);
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
		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/5, -1*config->plotResY/2+config->plotResY/20+4*BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
		pl_alabel_r(plot->plotter, 'l', 'l', "WARNING: NO SYNC profile, PLEASE DISREGARD");
	}

	if(config->syncTolerance)
	{
		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/5, -1*config->plotResY/2+config->plotResY/20+2*BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0xeeee, 0xeeee, 0);
		pl_alabel_r(plot->plotter, 'l', 'l', "WARNING: Sync tolerance enabled");
	}

	if(config->channel != 's')
	{
		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/10, config->plotResY/2-config->plotResY/30-BAR_HEIGHT);
		pl_pencolor_r(plot->plotter, 0xcccc, 0xcccc, 0xcccc);

		if(config->channel == 'l')
			pl_alabel_r(plot->plotter, 'l', 'l', "Left Channel");
		if(config->channel == 'r')
			pl_alabel_r(plot->plotter, 'l', 'l', "Right Channel");
	}
}

void DrawLabelsZeroToLimit(PlotFile *plot, double dBFS, double dbIncrement, double hz, double hzIncrement,  parameters *config)
{
	double segments = 0;
	char label[20];

	pl_fspace_r(plot->plotter, 0, -1*config->plotResY, config->plotResX, 0);
	pl_pencolor_r(plot->plotter, 0, 0xaaaa, 0);
	pl_ffontsize_r(plot->plotter, config->plotResY/60);

	pl_ffontname_r(plot->plotter, "HersheySans");
	segments = fabs(dBFS/dbIncrement);
	for(int i = 0; i < segments; i ++)
	{
		pl_fmove_r(plot->plotter, config->plotResX-config->plotResX/50, -1*i*config->plotResY/segments-config->plotResY/100);
		sprintf(label, "%gdBFS", -1*i*dbIncrement);
		pl_alabel_r(plot->plotter, 'c', 'c', label);
	}

	if(config->logScale)
	{
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(10, config), -config->plotResY/100);
		sprintf(label, "%dHz", 10);
		pl_alabel_r(plot->plotter, 'c', 'c', label);
	
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(100, config), -config->plotResY/100);
		sprintf(label, "%dHz", 100);
		pl_alabel_r(plot->plotter, 'c', 'c', label);
	}

	pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(1000, config), -config->plotResY/100);
	sprintf(label, "  %dHz", 1000);
	pl_alabel_r(plot->plotter, 'c', 'c', label);

	pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(10000, config), -config->plotResY/100);
	sprintf(label, "%dkHz", 10);
	pl_alabel_r(plot->plotter, 'c', 'c', label);

	if(config->endHzPlot > 20000)
	{
		pl_fmove_r(plot->plotter, config->plotResX/hz*transformtoLog(20000, config), config->plotResY/2-config->plotResY/100);
		sprintf(label, "%dkHz", 20);
		pl_alabel_r(plot->plotter, 'c', 'c', label);
	}

	pl_fspace_r(plot->plotter, plot->x0, plot->y0, plot->x1, plot->y1);
}

void DrawColorScale(PlotFile *plot, int type, int mode, double x, double y, double width, double height, double startDbs, double endDbs, double dbIncrement, parameters *config)
{
	double		segments = 0;
	char 		*label = NULL;
	int 		colorName = 0;
	long int	cnt = 0, cmp = 0;
	double		labelwidth = 0;

	label = GetTypeName(config, type);
	colorName = MatchColor(GetTypeColor(config, type));
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
	pl_ffontsize_r(plot->plotter, config->plotResY/60);
	pl_ffontname_r(plot->plotter, "HersheySans");

	for(double i = 0; i < segments; i++)
	{
		char labeldbs[20];

		pl_fmove_r(plot->plotter, x+width+config->plotResX/60, y+height-i*height/segments-height/segments/2);
		sprintf(labeldbs, " %c%gdBFS", fabs(startDbs) + i*dbIncrement != 0 ? '-' : ' ', fabs(startDbs) + i*dbIncrement);
		pl_alabel_r(plot->plotter, 'c', 'c', labeldbs);
	}

	SetPenColor(colorName, 0xaaaa, plot);
	pl_fmove_r(plot->plotter, x+4*width+config->plotResX/60, y);
	pl_alabel_r(plot->plotter, 'l', 'l', label);
	labelwidth = pl_flabelwidth_r(plot->plotter, label);

	if(mode != MODE_SPEC)
	{
		double bar_text_width = 0;

		if(mode == MODE_DIFF)
		{
			FindDifferenceTypeTotals(type, &cnt, &cmp, config);

			SetPenColor(COLOR_GRAY, 0xaaaa, plot);
			pl_fmove_r(plot->plotter, x+4*width+config->plotResX/60, y+1.5*BAR_HEIGHT);
			pl_alabel_r(plot->plotter, 'l', 'l', BAR_DIFF);
		}
		if(mode == MODE_MISS)
		{
			FindMissingTypeTotals(type, &cnt, &cmp, config);
		}
		bar_text_width = DrawMatchBar(plot, colorName,
			x+4*width+config->plotResX/60+labelwidth+BAR_WIDTH*0.2, y,
			BAR_WIDTH, BAR_HEIGHT, 
			(double)cnt, (double)cmp, config);
		if(mode == MODE_DIFF)
		{
			int 	x_offset = 0;
			char	header[100];

			x_offset = x+4*width+config->plotResX/60+labelwidth+BAR_WIDTH + bar_text_width;

			FindDifferenceWithinInterval(type, &cnt, &cmp, config->AmpBarRange, config);

			if(config->AmpBarRange > BAR_DIFF_DB_TOLERANCE)
				SetPenColor(COLOR_YELLOW, 0xaaaa, plot);
			else
				SetPenColor(COLOR_GRAY, 0xaaaa, plot);
			pl_fmove_r(plot->plotter, 1.1*x_offset, y+1.5*BAR_HEIGHT);
			sprintf(header, BAR_WITHIN, config->AmpBarRange);
			pl_alabel_r(plot->plotter, 'l', 'l', header);

			SetPenColor(COLOR_GRAY, 0xaaaa, plot);
			pl_fmove_r(plot->plotter, x_offset, y+3*BAR_HEIGHT);
			pl_alabel_r(plot->plotter, 'c', 'c', BAR_HEADER);

			DrawMatchBar(plot, colorName,
				1.1*x_offset, y,
				BAR_WIDTH, BAR_HEIGHT, 
				(double)cnt, (double)cmp, config);
		}
	}
}

void DrawColorAllTypeScale(PlotFile *plot, int mode, double x, double y, double width, double height, double endDbs, double dbIncrement, parameters *config)
{
	double 	segments = 0, maxlabel = 0, maxbarwidth = 0;
	int		*colorName = NULL, *typeID = NULL;
	int		numTypes = 0, t = 0;

	pl_fspace_r(plot->plotter, 0, 0, config->plotResX, config->plotResY);
	pl_filltype_r(plot->plotter, 1);

	numTypes = GetActiveBlockTypes(config);
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

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type > TYPE_CONTROL)
		{
			colorName[t] = MatchColor(GetTypeColor(config, config->types.typeArray[i].type));
			typeID[t] = config->types.typeArray[i].type;
			t++;
		}
	}

	for(double i = 0; i < segments; i ++)
	{
		long int intensity;

		intensity = CalculateWeightedError(i/segments, config)*0xffff;

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
	pl_ffontsize_r(plot->plotter, config->plotResY/60);
	pl_ffontname_r(plot->plotter, "HersheySans");

	for(double i = 0; i < segments; i++)
	{
		char label[20];

		pl_fmove_r(plot->plotter, x+width+config->plotResX/60, y+height-i*height/segments-height/segments/2);
		sprintf(label, " %c%gdBFS", i*dbIncrement > 0 ? '-' : ' ', i*dbIncrement);
		pl_alabel_r(plot->plotter, 'c', 'c', label);
	}

	for(int t = 0; t < numTypes; t++)
	{
		double	labelwidth = 0;
		char 	*label = NULL;

		label = GetTypeName(config, typeID[t]);
		SetPenColor(colorName[t], 0xaaaa, plot);
		pl_fmove_r(plot->plotter, x+2*width+config->plotResX/60, y+(numTypes-1)*config->plotResY/50-t*config->plotResY/50);
		pl_alabel_r(plot->plotter, 'l', 'l', label);

		labelwidth = pl_flabelwidth_r(plot->plotter, label);
		if(maxlabel < labelwidth)
			maxlabel = labelwidth;
	}

	if(mode != MODE_SPEC)
	{
		// Draw label if needed
		if(mode == MODE_DIFF)
		{
			SetPenColor(COLOR_GRAY, 0xaaaa, plot);
			pl_fmove_r(plot->plotter, x+2*width+config->plotResX/60, y+(numTypes-1)*config->plotResY/50+1.5*BAR_HEIGHT);
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
					x+2*width+config->plotResX/60+maxlabel+BAR_WIDTH*0.2, y+(numTypes-1)*config->plotResY/50-t*config->plotResY/50,
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

		x_offset = x+2*width+config->plotResX/60+BAR_WIDTH+maxbarwidth + maxlabel;

		if(config->AmpBarRange > BAR_DIFF_DB_TOLERANCE)
			SetPenColor(COLOR_YELLOW, 0xaaaa, plot);
		else
			SetPenColor(COLOR_GRAY, 0xaaaa, plot);
		pl_fmove_r(plot->plotter, 1.1*x_offset, y+(numTypes-1)*config->plotResY/50+1.5*BAR_HEIGHT);
		sprintf(header, BAR_WITHIN, config->AmpBarRange);
		pl_alabel_r(plot->plotter, 'l', 'l', header);

		SetPenColor(COLOR_GRAY, 0xaaaa, plot);
		pl_fmove_r(plot->plotter, x_offset, y+(numTypes-1)*config->plotResY/50+3*BAR_HEIGHT);
		pl_alabel_r(plot->plotter, 'c', 'c', BAR_HEADER);

		for(int t = 0; t < numTypes; t++)
		{
			FindDifferenceWithinInterval(typeID[t], &cnt, &cmp, config->AmpBarRange, config);

			DrawMatchBar(plot, colorName[t],
				1.1*x_offset, y+(numTypes-1)*config->plotResY/50-t*config->plotResY/50,
				BAR_WIDTH, BAR_HEIGHT, 
				(double)cnt, (double)cmp, config);
		}
	}

	free(colorName);
	colorName = NULL;
	free(typeID);
	typeID = NULL;
}

double DrawMatchBar(PlotFile *plot, int colorName, double x, double y, double width, double height, double notFound, double total, parameters *config)
{
	char percent[40];
	double labelwidth = 0, maxlabel = 0;

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
		pl_ffontsize_r(plot->plotter, config->plotResY/60);
		pl_ffontname_r(plot->plotter, "HersheySans");

		sprintf(percent, "%5.2f%% of %ld", notFound*100.0/total, (long int)total);

		SetPenColor(colorName, 0x8888, plot);
		pl_fmove_r(plot->plotter, x+width*1.10, y);
		pl_alabel_r(plot->plotter, 'l', 'l', percent);
		labelwidth = pl_flabelwidth_r(plot->plotter, percent);
		if(labelwidth > maxlabel)
			maxlabel = labelwidth;
	}
	return maxlabel;
}

void DrawNoiseLines(PlotFile *plot, double start, double end, AudioSignal *Signal, parameters *config)
{
	int harmonic = 0;

	pl_pencolor_r (plot->plotter, 0xAAAA, 0xAAAA, 0);
	pl_linemod_r(plot->plotter, "dotdashed");
	if(Signal->gridFrequency)
	{
		for(harmonic = 0; harmonic < 32; harmonic++)
		{
			pl_pencolor_r (plot->plotter, 0xAAAA-0x400*harmonic, 0xAAAA-0x400*harmonic, 0);
			pl_fline_r(plot->plotter, transformtoLog(Signal->gridFrequency*harmonic, config), start, transformtoLog(Signal->gridFrequency*harmonic, config), end);
		}
	}
	pl_pencolor_r (plot->plotter, 0xAAAA, 0xAAAA, 0);
	if(Signal->scanrateFrequency)
		pl_fline_r(plot->plotter, transformtoLog(Signal->scanrateFrequency, config), start, transformtoLog(Signal->scanrateFrequency, config), end);
	pl_linemod_r(plot->plotter, "solid");
}

void SaveCSV(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config)
{
	FILE 		*csv = NULL;
	char		name[BUFFER_SIZE];

	if(!config)
		return;

	if(!amplDiff)
		return;

	if(!config->Differences.BlockDiffArray)
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

	if(!config->Differences.BlockDiffArray)
		return;

	sprintf(name, "DA__ALL_%s", filename);
	FillPlot(&plot, name, config->plotResX, config->plotResY, config->startHzPlot, -1*dBFS, config->endHzPlot, dBFS, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);

	for(int a = 0; a < size; a++)
	{
		if(amplDiff[a].type > TYPE_CONTROL)
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

	DrawColorAllTypeScale(&plot, MODE_DIFF, config->plotResX/100, config->plotResY/15, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, config->significantAmplitude, VERT_SCALE_STEP_BAR, config);
	DrawLabelsMDF(&plot, DIFFERENCE_TITLE, ALL_LABEL, PLOT_COMPARE, config);

	ClosePlot(&plot);
}

int PlotEachTypeDifferentAmplitudes(FlatAmplDifference *amplDiff, long int size, char *filename, parameters *config)
{
	int 		i = 0, type = 0, types = 0;
	char		name[BUFFER_SIZE];

	for(i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;
		if(type > TYPE_CONTROL)
		{
			sprintf(name, "DA_%s_%02d%s_", filename, 
				type, config->types.typeArray[i].typeName);
		
			PlotSingleTypeDifferentAmplitudes(amplDiff, size, type, name, config);
			logmsg(PLOT_ADVANCE_CHAR);
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

	if(!config->Differences.BlockDiffArray)
		return;

	FillPlot(&plot, filename, config->plotResX, config->plotResY, config->startHzPlot, -1*dBFS, config->endHzPlot, dBFS, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);

	for(int a = 0; a < size; a++)
	{
		if(amplDiff[a].hertz && amplDiff[a].type == type)
		{ 
			long int intensity;

			intensity = CalculateWeightedError((fabs(config->significantAmplitude) - fabs(amplDiff[a].refAmplitude))/fabs(config->significantAmplitude), config)*0xffff;

			SetPenColor(amplDiff[a].color, intensity, &plot);
			pl_fpoint_r(plot.plotter, transformtoLog(amplDiff[a].hertz, config), amplDiff[a].diffAmplitude);
		}
	}

	DrawColorScale(&plot, type, MODE_DIFF,
		config->plotResX/100, config->plotResY/15, 
		config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15,
		0, config->significantAmplitude, VERT_SCALE_STEP_BAR, config);
	DrawLabelsMDF(&plot, DIFFERENCE_TITLE, GetTypeName(config, type), PLOT_COMPARE, config);
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
			sprintf(name, "NF_%s_%d_%02d%s_", filename, i,
				type, config->types.typeArray[i].typeName);
			PlotSilenceBlockDifferentAmplitudes(amplDiff, size, type, name, config, Signal);
			logmsg(PLOT_ADVANCE_CHAR);
			return 1;
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

	if(!config->Differences.BlockDiffArray)
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

	FillPlot(&plot, filename, config->plotResX, config->plotResY, config->startHzPlot, -1*dBFS, config->endHzPlot, dBFS, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);

	DrawNoiseLines(&plot, 0, endAmplitude, Signal, config);

	for(int a = 0; a < size; a++)
	{
		if(amplDiff[a].hertz && amplDiff[a].type == type)
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
		config->plotResX/100, config->plotResY/15, 
		config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15,
		(int)startAmplitude, (int)(endAmplitude-startAmplitude), VERT_SCALE_STEP_BAR, config);

	DrawLabelsMDF(&plot, NOISE_TITLE, GetTypeName(config, type), PLOT_COMPARE, config);
	ClosePlot(&plot);
}

void PlotAllMissingFrequencies(FlatFreqDifference *freqDiff, long int size, char *filename, parameters *config)
{
	PlotFile	plot;
	char		name[BUFFER_SIZE];
	double		significant = 0;

	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	significant = config->significantAmplitude;
	sprintf(name, "MIS__ALL_%s", filename);
	FillPlot(&plot, name, config->plotResX, config->plotResY, config->startHzPlot, significant, config->endHzPlot, 0.0, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroToLimit(&plot, significant, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroToLimit(&plot, significant, VERT_SCALE_STEP, config->endHzPlot, 1000, config);

	if(size)
	{
		for(int f = size; f >= 0 ; f--)
		{
			if(freqDiff[f].type > TYPE_CONTROL)
			{ 
				long int intensity;
				double x, y;
	
				x = transformtoLog(freqDiff[f].hertz, config);
				y = freqDiff[f].amplitude;
	
				intensity = CalculateWeightedError((fabs(significant) - fabs(freqDiff[f].amplitude))/fabs(significant), config)*0xffff;
				
				SetPenColor(freqDiff[f].color, intensity, &plot);
				pl_fline_r(plot.plotter, x,	y, x, significant);
				pl_endpath_r(plot.plotter);
			}
		}
	}
	
	DrawColorAllTypeScale(&plot, MODE_MISS, config->plotResX/100, config->plotResY/15, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, significant, VERT_SCALE_STEP_BAR, config);
	DrawLabelsMDF(&plot, MISSING_TITLE, ALL_LABEL, PLOT_COMPARE, config);
	ClosePlot(&plot);
}

int PlotEachTypeMissingFrequencies(FlatFreqDifference *freqDiff, long int size, char *filename, parameters *config)
{
	int 		i = 0, type = 0, types = 0;
	char		name[BUFFER_SIZE];

	for(i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;
		if(type > TYPE_CONTROL)
		{
			sprintf(name, "MISS_%s_%02d%s", filename, 
							config->types.typeArray[i].type, config->types.typeArray[i].typeName);
			PlotSingleTypeMissingFrequencies(freqDiff, size, type, name, config);
			logmsg(PLOT_ADVANCE_CHAR);
			types ++;
		}
	}
	return types;
}

void PlotSingleTypeMissingFrequencies(FlatFreqDifference *freqDiff, long int size, int type, char *filename, parameters *config)
{
	PlotFile	plot;
	double		significant = 0;

	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	significant = config->significantAmplitude;

	FillPlot(&plot, filename, config->plotResX, config->plotResY, config->startHzPlot, significant, config->endHzPlot, 0.0, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroToLimit(&plot, significant, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroToLimit(&plot, significant, VERT_SCALE_STEP, config->endHzPlot, 1000, config);

	for(int f = 0; f < size; f++)
	{
		if(freqDiff[f].type == type)
		{
			long int intensity;
			double x, y;

			x = transformtoLog(freqDiff[f].hertz, config);;
			y = freqDiff[f].amplitude;
			intensity = CalculateWeightedError((fabs(significant) - fabs(freqDiff[f].amplitude))/fabs(significant), config)*0xffff;

			SetPenColor(freqDiff[f].color, intensity, &plot);
			pl_fline_r(plot.plotter, x,	y, x, significant);
			pl_endpath_r(plot.plotter);
		}
	}
	
	DrawColorScale(&plot, type, MODE_MISS, config->plotResX/100, config->plotResY/15, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, 0, significant, VERT_SCALE_STEP_BAR, config);
	DrawLabelsMDF(&plot, MISSING_TITLE, GetTypeName(config, type), PLOT_COMPARE, config);
	ClosePlot(&plot);
}

void PlotAllSpectrogram(FlatFrequency *freqs, long int size, char *filename, int signal, parameters *config)
{
	PlotFile plot;
	char	 name[BUFFER_SIZE];
	double	 significant = 0;

	if(!config)
		return;

	significant = config->significantAmplitude;

	sprintf(name, "SP__ALL_%s", filename);
	FillPlot(&plot, name, config->plotResX, config->plotResY, config->startHzPlot, significant, config->endHzPlot, 0.0, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroToLimit(&plot, significant, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroToLimit(&plot, significant, VERT_SCALE_STEP, config->endHzPlot, 1000, config);

	if(size)
	{
		for(int f = size; f >= 0; f--)
		{
			if(freqs[f].type > TYPE_CONTROL)
			{ 
				long int intensity;
				double x, y;
	
				x = transformtoLog(freqs[f].hertz, config);
				y = freqs[f].amplitude;
				intensity = CalculateWeightedError((fabs(significant) - fabs(freqs[f].amplitude))/fabs(significant), config)*0xffff;
		
				SetPenColor(freqs[f].color, intensity, &plot);
				pl_fline_r(plot.plotter, x,	y, x, significant);
				pl_endpath_r(plot.plotter);
			}
		}
	}

	DrawColorAllTypeScale(&plot, MODE_SPEC, config->plotResX/100, config->plotResY/15, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, significant, VERT_SCALE_STEP_BAR, config);
	DrawLabelsMDF(&plot, signal == ROLE_REF ? SPECTROGRAM_TITLE_REF : SPECTROGRAM_TITLE_COM, ALL_LABEL, signal == ROLE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);
	ClosePlot(&plot);
}

int PlotEachTypeSpectrogram(FlatFrequency *freqs, long int size, char *filename, int signal, parameters *config, AudioSignal *Signal)
{
	int 		i = 0, type = 0, types = 0, silence = 0;
	char		name[BUFFER_SIZE];

	for(i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;
		if(type > TYPE_CONTROL)
		{
			sprintf(name, "SP_%c_%s_%02d%s", signal == ROLE_REF ? 'A' : 'B', filename, 
					config->types.typeArray[i].type, config->types.typeArray[i].typeName);
			PlotSingleTypeSpectrogram(freqs, size, type, name, signal, config);
			logmsg(PLOT_ADVANCE_CHAR);
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
	double		significant = 0;

	if(!config)
		return;

	significant = config->significantAmplitude;

	FillPlot(&plot, filename, config->plotResX, config->plotResY, config->startHzPlot, significant, config->endHzPlot, 0.0, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawLabelsZeroToLimit(&plot, significant, VERT_SCALE_STEP,config->endHzPlot, 1000, config);
	DrawGridZeroToLimit(&plot, significant, VERT_SCALE_STEP,config->endHzPlot, 1000, config);

	for(int f = 0; f < size; f++)
	{
		if(freqs[f].type == type)
		{ 
			long int intensity;
			double x, y;

			x = transformtoLog(freqs[f].hertz, config);
			y = freqs[f].amplitude;
			intensity = CalculateWeightedError((fabs(significant) - fabs(freqs[f].amplitude))/fabs(significant), config)*0xffff;
	
			//pl_flinewidth_r(plot.plotter, 100*range_0_1);
			SetPenColor(freqs[f].color, intensity, &plot);
			pl_fline_r(plot.plotter, x,	y, x, significant);
			pl_endpath_r(plot.plotter);
		}
	}
	
	DrawColorScale(&plot, type, MODE_SPEC, config->plotResX/100, config->plotResY/15, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, 0, significant, VERT_SCALE_STEP,config);
	DrawLabelsMDF(&plot, signal == ROLE_REF ? SPECTROGRAM_TITLE_REF : SPECTROGRAM_TITLE_COM, GetTypeName(config, type), signal == ROLE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);
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

	FillPlot(&plot, filename, config->plotResX, config->plotResY, config->startHzPlot, endAmplitude, config->endHzPlot, 0.0, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawLabelsZeroToLimit(&plot, endAmplitude, VERT_SCALE_STEP,config->endHzPlot, 1000, config);
	DrawGridZeroToLimit(&plot, endAmplitude, VERT_SCALE_STEP,config->endHzPlot, 1000, config);

	DrawNoiseLines(&plot, 0, endAmplitude, Signal, config);

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

			/*
			OutputFileOnlyStart();
			logmsg("Plot: %g %g %ld (%g,%g,%g,%g)\n", freqs[f].hertz, y, intensity,
					x, y, x, endAmplitude);
			OutputFileOnlyEnd();
			*/
		}
	}
	
	DrawColorScale(&plot, type, MODE_SPEC, config->plotResX/100, config->plotResY/15, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, (int)startAmplitude, (int)(endAmplitude-startAmplitude), VERT_SCALE_STEP,config);
	DrawLabelsMDF(&plot, signal == ROLE_REF ? SPECTROGRAM_NOISE_REF : SPECTROGRAM_NOISE_COM, GetTypeName(config, type), signal == ROLE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);
	ClosePlot(&plot);
}

void VisualizeWindows(windowManager *wm, parameters *config)
{
	if(!wm)
		return;

	for(int i = 0; i < wm->windowCount; i++)
	{
		logmsg("Factor len %ld: %g\n", wm->windowArray[i].frames,
			CalculateCorrectionFactor(wm, wm->windowArray[i].frames));

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
	FillPlot(&plot, name, 320, 384, 0, -0.1, 1, 1.1, 0.001, config);

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
		FillPlot(&plot, name, 320, 384, 0, -0.1, 1, 1.1, 0.001, config);
	
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

	ADiff = (FlatAmplDifference*)malloc(sizeof(FlatAmplDifference)*config->Differences.cntAmplAudioDiff+1000);
	if(!ADiff)
		return NULL;

	memset(ADiff, 0, sizeof(FlatAmplDifference)*config->Differences.cntAmplAudioDiff+1000);

	for(int b = 0; b < config->types.totalChunks; b++)
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

FlatFreqDifference *CreateFlatMissing(parameters *config, long int *size)
{
	long int	count = 0;
	FlatFreqDifference *FDiff = NULL;

	if(!config)
		return NULL;

	if(!size)
		return NULL;

	*size = 0;

	FDiff = (FlatFreqDifference*)malloc(sizeof(FlatFreqDifference)*config->Differences.cntFreqAudioDiff);
	if(!FDiff)
		return NULL;
	memset(FDiff, 0, sizeof(FlatFreqDifference)*config->Differences.cntFreqAudioDiff);

	for(int b = 0; b < config->types.totalChunks; b++)
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

int InsertElementInPlace(FlatFrequency *Freqs, FlatFrequency Element, long int currentsize)
{
	if(!currentsize)
	{
		Freqs[0] = Element;
		return 1;
	}

	// worst case scenario
	if(Freqs[currentsize-1].hertz >= Element.hertz)
	{
		Freqs[currentsize] = Element;
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

		if(Element.hertz > Freqs[j].hertz)
		{
			/* Move the previous values down the array */
			for(int k = currentsize; k > j; k--)
				Freqs[k] = Freqs[k - 1];
	
			Freqs[j] = Element;
			return 1;
		}
	}

	logmsg("WARNING InsertElementInPlace No match found!\n");
	return 0;
}

FlatFrequency *CreateFlatFrequencies(AudioSignal *Signal, long int *size, parameters *config)
{
	long int		block = 0, i = 0;
	long int		count = 0, counter = 0;
	FlatFrequency	*Freqs = NULL;

	if(!size || !Signal || !config)
		return NULL;

	*size = 0;

	for(block = 0; block < config->types.totalChunks; block++)
	{
		int 	type = TYPE_NOTYPE;
		double	significant = 0;

		type = GetBlockType(config, block);
		significant = config->significantAmplitude;

		if(type >= TYPE_SILENCE)
		{
			for(i = 0; i < config->MaxFreq; i++)
			{
				int insert = 0;

				if(type > TYPE_SILENCE && Signal->Blocks[block].freq[i].hertz && Signal->Blocks[block].freq[i].amplitude > significant)
					insert = 1;
				if(type == TYPE_SILENCE && Signal->Blocks[block].freq[i].hertz)
					insert = 1;

				if(insert)
					count ++;
				
				if(Signal->Blocks[block].freq[i].hertz == 0)
					break;
			}
		}
	}

	Freqs = (FlatFrequency*)malloc(sizeof(FlatFrequency)*count);
	if(!Freqs)
		return NULL;
	memset(Freqs, 0, sizeof(FlatFrequency)*count);

	for(block = 0; block < config->types.totalChunks; block++)
	{
		int type = 0, color = 0;
		double	significant = 0;

		type = GetBlockType(config, block);
		significant = config->significantAmplitude;
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
	FillPlot(&plot, name, config->plotResX, config->plotResY, config->startHzPlot, -1*dBFS, config->endHzPlot, dBFS, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);

	srand(time(NULL));

	DrawLabelsMDF(&plot, "PLOT TEST [%s]", "ZDBC", PLOT_COMPARE, config);
	DrawColorAllTypeScale(&plot, MODE_DIFF, config->plotResX/100, config->plotResY/15, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, config->significantAmplitude, VERT_SCALE_STEP_BAR, config);

	ClosePlot(&plot);
}

void PlotTestZL(char *filename, parameters *config)
{
	PlotFile	plot;
	char		name[BUFFER_SIZE];

	if(!config)
		return;

	sprintf(name, "Test_ZL_%s", filename);
	FillPlot(&plot, name, config->plotResX, config->plotResY, config->startHzPlot, config->significantAmplitude, config->endHzPlot, 0.0, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroToLimit(&plot, config->significantAmplitude, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroToLimit(&plot, config->significantAmplitude, VERT_SCALE_STEP, config->endHzPlot, 1000, config);

	DrawColorScale(&plot, 1, MODE_SPEC, config->plotResX/100, config->plotResY/15, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, 0, SIGNIFICANT_VOLUME, VERT_SCALE_STEP_BAR, config);
	
	DrawLabelsMDF(&plot, "PLOT TEST [%s]", "GZL", PLOT_COMPARE, config);
	ClosePlot(&plot);
}


inline double transformtoLog(double coord, parameters *config)
{
	if(config->logScale)
		return(config->endHzPlot*log10(coord)/log10(config->endHzPlot));
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
	double				significant = 0;

	if(!config)
		return NULL;

	if(!avgSize)
		return NULL;

	*avgSize = 0;

	significant = config->significantAmplitude;
	// Count how many we have
	for(int b = 0; b < config->types.totalChunks; b++)
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
	for(int b = 0; b < config->types.totalChunks; b++)
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
							value = (fabs(significant) - fabs(config->Differences.BlockDiffArray[b].amplDiffArray[a].refAmplitude))/
										fabs(significant);
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

	for(i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;
		if(type > TYPE_CONTROL)
			typeCount ++;
	}

	averagedArray = (AveragedFrequencies**)malloc(sizeof(AveragedFrequencies*)*typeCount);
	if(!averagedArray)
		return 0;

	averagedSizes = (long int*)malloc(sizeof(long int)*typeCount);
	if(!averagedSizes)
		return 0;

	for(i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;
		if(type > TYPE_CONTROL)
		{
			long int chunks = AVERAGE_CHUNKS;

			sprintf(name, "DA_%s_%02d%s_AVG_", filename, 
					config->types.typeArray[i].type, config->types.typeArray[i].typeName);

			averagedArray[types] = CreateFlatDifferencesAveraged(type, &averagedSizes[types], chunks, normalPlot, config);

			if(averagedArray[types])
			{
				PlotSingleTypeDifferentAmplitudesAveraged(amplDiff, size, type, name, averagedArray[types], averagedSizes[types], config);
				logmsg(PLOT_ADVANCE_CHAR);
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

			sprintf(name, "NF_%s_%02d%s_AVG_", filename, 
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

	FillPlot(&plot, filename, config->plotResX, config->plotResY, config->startHzPlot, -1*dbs, config->endHzPlot, dbs, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	if(dbs > 90)
		vertscale *= 2;
	if(dbs > 200)
		vertscale *= 10;
	DrawGridZeroDBCentered(&plot, dbs, vertscale, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dbs, vertscale, config->endHzPlot, 1000, config);

	DrawNoiseLines(&plot, dbs, -1*dbs, Signal, config);

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

	DrawColorScale(&plot, type, MODE_DIFF,
		config->plotResX/100, config->plotResY/15, 
		config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15,
		(int)startAmplitude, (int)(endAmplitude-startAmplitude), VERT_SCALE_STEP_BAR, config);
	DrawLabelsMDF(&plot, NOISE_AVG_TITLE, GetTypeName(config, type), PLOT_COMPARE, config);
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

	FillPlot(&plot, filename, config->plotResX, config->plotResY, config->startHzPlot, -1*dbs, config->endHzPlot, dbs, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroDBCentered(&plot, dbs, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dbs, VERT_SCALE_STEP, config->endHzPlot, 1000, config);

	for(long int a = 0; a < size; a++)
	{
		if(amplDiff[a].type == type)
		{ 
			if(amplDiff[a].refAmplitude > config->significantAmplitude)
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

	DrawColorScale(&plot, type, MODE_DIFF, config->plotResX/100, config->plotResY/15, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, 0, config->significantAmplitude, VERT_SCALE_STEP_BAR, config);
	DrawLabelsMDF(&plot, DIFFERENCE_AVG_TITLE, GetTypeName(config, type), PLOT_COMPARE, config);
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
	
	FillPlot(&plot, filename, config->plotResX, config->plotResY, config->startHzPlot, -1*dBFS, config->endHzPlot, dBFS, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, VERT_SCALE_STEP, config->endHzPlot, 1000, config);

	for(long int a = 0; a < size; a++)
	{
		if(amplDiff[a].type > TYPE_CONTROL)
		{ 
			if(amplDiff[a].refAmplitude > config->significantAmplitude)
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
		if(type <= TYPE_CONTROL)
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
					pl_fcont_r(plot.plotter, transformtoLog(averaged[currType][a].avgfreq, config), averaged[currType][a].avgvol);
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
					pl_fcont_r(plot.plotter, transformtoLog(averaged[currType][a].avgfreq, config), averaged[currType][a].avgvol);
			}
			pl_endpath_r(plot.plotter);
		}
		currType++;
	}

	DrawColorAllTypeScale(&plot, MODE_DIFF, config->plotResX/100, config->plotResY/15, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, config->significantAmplitude, VERT_SCALE_STEP_BAR, config);
	DrawLabelsMDF(&plot, DIFFERENCE_AVG_TITLE, ALL_LABEL, PLOT_COMPARE, config);

	ClosePlot(&plot);
}

void DrawFrequencyHorizontalGrid(PlotFile *plot, double hz, double hzIncrement, parameters *config)
{
	pl_pencolor_r (plot->plotter, 0, 0x5555, 0);
	for(int i = hzIncrement; i < hz; i += hzIncrement)
		pl_fline_r(plot->plotter, 0, i, config->plotResX, i);

	pl_pencolor_r (plot->plotter, 0, 0x7777, 0);
	pl_fline_r(plot->plotter, 0, 10000, config->plotResX, 10000);
	if(config->endHzPlot > 20000)
		pl_fline_r(plot->plotter, 0, 20000, config->plotResX, 20000);

	pl_endpath_r(plot->plotter);
}

void PlotTimeSpectrogram(AudioSignal *Signal, parameters *config)
{
	PlotFile	plot;
	double		significant = 0, x = 0, width = 0, blockcount = 0;
	long int	block = 0, i = 0;
	char		filename[BUFFER_SIZE], name[BUFFER_SIZE/2];

	if(!Signal || !config)
		return;

	ShortenFileName(basename(Signal->SourceFile), name);
	sprintf(filename, "T_SP_%c_%s", Signal->role == ROLE_REF ? 'A' : 'B', name);

	for(block = 0; block < config->types.totalChunks; block++)
	{
		int 	type = 0;

		type = GetBlockType(config, block);
		if(type > TYPE_SILENCE)
			blockcount++;
	}

	if(!blockcount)
		return;

	if(config->FullTimeSpectroScale)
		significant = PCM_16BIT_MIN_AMPLITUDE;
	else
		significant = config->significantAmplitude;

	FillPlot(&plot, filename, config->plotResX, config->plotResY, 
			0, 0, config->plotResX, config->endHzPlot, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawFrequencyHorizontalGrid(&plot, config->endHzPlot, 1000, config);

	width = config->plotResX / blockcount;
	for(block = 0; block < config->types.totalChunks; block++)
	{
		int 	type = 0, color = 0;

		type = GetBlockType(config, block);
		color = MatchColor(GetBlockColor(config, block));

		if(type > TYPE_SILENCE && config->MaxFreq > 0)
		{
			for(i = config->MaxFreq; i >= 0; i--)
			{
				if(Signal->Blocks[block].freq[i].hertz && Signal->Blocks[block].freq[i].amplitude > significant)
				{
					long int intensity;
					double y, amplitude;

					// x is fixed by block division
					y = Signal->Blocks[block].freq[i].hertz;
					amplitude = Signal->Blocks[block].freq[i].amplitude;
					
					intensity = CalculateWeightedError(fabs(fabs(significant) - fabs(amplitude))/fabs(significant), config)*0xffff;
					SetPenColor(color, intensity, &plot);
					pl_fline_r(plot.plotter, x,	y, x+width, y);
					pl_endpath_r(plot.plotter);
				}
			}
			x += width;
		}
	}

	DrawColorAllTypeScale(&plot, MODE_SPEC, config->plotResX/100, config->plotResY/15, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, significant, VERT_SCALE_STEP_BAR, config);
	DrawLabelsMDF(&plot, Signal->role == ROLE_REF ? TSPECTROGRAM_TITLE_REF : TSPECTROGRAM_TITLE_COM, ALL_LABEL, Signal->role == ROLE_REF ? PLOT_SINGLE_REF : PLOT_SINGLE_COM, config);

	ClosePlot(&plot);
}

void PlotTimeSpectrogramUnMatchedContent(AudioSignal *Signal, parameters *config)
{
	PlotFile	plot;
	double		significant = 0, x = 0, width = 0, blockcount = 0;
	long int	block = 0, i = 0;
	char		filename[BUFFER_SIZE], name[BUFFER_SIZE/2];

	if(!Signal || !config)
		return;

	ShortenFileName(basename(Signal->SourceFile), name);
	if(Signal->role == ROLE_REF)
		sprintf(filename, "MISSING-A-T_SP_%s", name);
	else
		sprintf(filename, "MISSING-EXTRA_T_SP_%s", name);

	for(block = 0; block < config->types.totalChunks; block++)
	{
		int 	type = 0;

		type = GetBlockType(config, block);
		if(type > TYPE_SILENCE)
			blockcount++;
	}

	if(!blockcount)
		return;

	if(config->FullTimeSpectroScale)
		significant = PCM_16BIT_MIN_AMPLITUDE;
	else
		significant = config->significantAmplitude;

	FillPlot(&plot, filename, config->plotResX, config->plotResY, 
			0, 0, config->plotResX, config->endHzPlot, 1, config);

	if(!CreatePlotFile(&plot, config))
		return;

	DrawFrequencyHorizontalGrid(&plot, config->endHzPlot, 1000, config);

	width = config->plotResX / blockcount;
	for(block = 0; block < config->types.totalChunks; block++)
	{
		int 	type = 0, color = 0;

		type = GetBlockType(config, block);
		color = MatchColor(GetBlockColor(config, block));

		if(type > TYPE_SILENCE && config->MaxFreq > 0)
		{
			for(i = config->MaxFreq; i >= 0; i--)
			{
				if(Signal->Blocks[block].freq[i].hertz && !Signal->Blocks[block].freq[i].matched
					&& Signal->Blocks[block].freq[i].amplitude > significant)
				{
					long int intensity;
					double y, amplitude;

					// x is fixed by block division
					y = Signal->Blocks[block].freq[i].hertz;
					amplitude = Signal->Blocks[block].freq[i].amplitude;
					
					intensity = CalculateWeightedError(fabs(fabs(significant) - fabs(amplitude))/fabs(significant), config)*0xffff;
					SetPenColor(color, intensity, &plot);
					pl_fline_r(plot.plotter, x,	y, x+width, y);
					pl_endpath_r(plot.plotter);
				}
			}
			x += width;
		}
	}

	DrawColorAllTypeScale(&plot, MODE_SPEC, config->plotResX/100, config->plotResY/15, config->plotResX/COLOR_BARS_WIDTH_SCALE, config->plotResY/1.15, significant, VERT_SCALE_STEP_BAR, config);
	DrawLabelsMDF(&plot, Signal->role == ROLE_REF ? EXTRA_TITLE_TS_REF : EXTRA_TITLE_TS_COM, ALL_LABEL, PLOT_COMPARE, config);

	ClosePlot(&plot);
}
