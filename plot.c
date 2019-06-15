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

#define ALL_LABEL					"ALL"
#define DIFFERENCE_TITLE			"DIFFERENT AMPLITUDES [%s]"
#define MISSING_TITLE				"MISSING FREQUENCIES [%s]"
#define SPECTROGRAM_TITLE_REF		"REFERENCE SPECTROGRAM [%s]"
#define SPECTROGRAM_TITLE_COM		"COMPARISON SPECTROGRAM [%s]"
#define DIFFERENCE_AVG_TITLE		"DIFFERENT AMPLITUDES AVERAGED [%s]"

#if defined (WIN32)
	#include <direct.h>
	#define GetCurrentDir _getcwd
#else
	#include <unistd.h>
	#define GetCurrentDir getcwd
#endif

#define BAR_HOR_POS_ALL	config->plotResX - config->plotResX/10
#define BAR_HOR_POS		config->plotResX - config->plotResX/13
#define	BAR_VERT_POS	config->plotResY/30
#define BAR_WIDTH		config->plotResX/150
#define	BAR_HEIGHT		config->plotResY/20
#define	BAR_LABEL_POS	config->plotResY/40

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

void PlotResults(AudioSignal *Signal, parameters *config)
{
	struct	timespec	start, end;
	char 	*CurrentPath = NULL;

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	CurrentPath = GetCurrentPathAndChangeToResultsFolder(config);

	logmsg(" -");
	if(config->plotDifferences || config->averagePlot)
	{
		logmsg(" Difference");
		PlotAmpDifferences(config);
	}
	if(config->plotMissing)
	{
		logmsg(" Missing");
		PlotFreqMissing(config);
	}
	if(config->plotSpectrogram)
	{
		logmsg(" Spectrogram");
		PlotSpectrograms(Signal, config);
	}
	logmsg("\n");

	ReturnToMainPath(&CurrentPath);

	if(config->clock)
	{
		double	elapsedSeconds;
		clock_gettime(CLOCK_MONOTONIC, &end);
		elapsedSeconds = TimeSpecToSeconds(&end) - TimeSpecToSeconds(&start);
		logmsg(" - Plotting PNGs took %0.2fs\n", elapsedSeconds);
	}
}

void PlotAmpDifferences(parameters *config)
{
	double average = 0;
	FlatAmplDifference	*amplDiff = NULL;
	
	average = FindDifferenceAverage(config);
	if(average > config->maxDbPlotZC)
	{
		config->maxDbPlotZC = average*1.5;

		logmsg("\n\nWARNING: The average difference is %g dBFS.\n", average);
		logmsg("\tThis is abnormal, signal might not be from the correct source\n");
		logmsg("\tStereo channels could also be inverted\n");
		logmsg("\tAdjusting viewport to %gdBFS for plots\n\n", config->maxDbPlotZC);
	}
	amplDiff = CreateFlatDifferences(config);
	if(!amplDiff)
	{
		logmsg("Not enough memory for plotting\n");
		return;
	}

	if(config->plotDifferences)
	{
		if(PlotEachTypeDifferentAmplitudes(amplDiff, config->compareName, config) > 1)
		{
			PlotAllDifferentAmplitudes(amplDiff, config->compareName, config);
			logmsg(PLOT_ADVANCE_CHAR);
		}
	}

	if(config->averagePlot)
		PlotDifferentAmplitudesAveraged(amplDiff, config->compareName, config);
	
	free(amplDiff);
	amplDiff = NULL;
}

void PlotDifferentAmplitudesWithBetaFunctions(parameters *config)
{
	FlatAmplDifference	*amplDiff = NULL;
	
	amplDiff = CreateFlatDifferences(config);
	if(!amplDiff)
	{
		logmsg("Not enough memory for plotting\n");
		return;
	}

	for(int o = 0; o < 6; o++)
	{
		config->outputFilterFunction = o;
		CreateBaseName(config);
		PlotAllDifferentAmplitudes(amplDiff, config->compareName, config);
	}

	free(amplDiff);
	amplDiff = NULL;
}

void PlotFreqMissing(parameters *config)
{
	FlatFreqDifference	*freqDiff = NULL;
	
	freqDiff = CreateFlatMissing(config);
	if(PlotEachTypeMissingFrequencies(freqDiff, config->compareName, config) > 1)
	{
		PlotAllMissingFrequencies(freqDiff, config->compareName, config);
		logmsg(PLOT_ADVANCE_CHAR);
	}

	free(freqDiff);
	freqDiff = NULL;
}

void PlotSpectrograms(AudioSignal *Signal, parameters *config)
{
	long int			size = 0;
	FlatFrequency		*frequencies = NULL;

	frequencies = CreateFlatFrequencies(Signal, &size, config);
	if(PlotEachTypeSpectrogram(frequencies, size, basename(Signal->SourceFile), Signal->role, config) > 1)
	{
		PlotAllSpectrogram(frequencies, size, basename(Signal->SourceFile), Signal->role, config);
		logmsg(PLOT_ADVANCE_CHAR);
	}

	free(frequencies);
	frequencies = NULL;
}

int FillPlot(PlotFile *plot, char *name, int sizex, int sizey, double x0, double y0, double x1, double y1, double penWidth, parameters *config)
{
	plot->plotter = NULL;
	plot->plotter_params = NULL;
	plot->file = NULL;

	ComposeFileNameoPath(plot->FileName, name, ".png", config);

	plot->sizex = sizex;
	plot->sizey = sizey;
	plot->x0 = x0;
	plot->y0 = y0;
	plot->x1 = x1;
	plot->y1 = y1;
	plot->penWidth = penWidth;
	return 1;
}

int CreatePlotFile(PlotFile *plot)
{
	char		size[20];

	plot->file = fopen(plot->FileName, "wb");
	if(!plot->file)
	{
		logmsg("Couldn't create Plot file %s\n%s\n", plot->FileName, strerror(errno));
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
	pl_bgcolor_r(plot->plotter, 0, 0, 0);
	pl_erase_r(plot->plotter);

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

	pl_pencolor_r (plot->plotter, 0, 0xFFFF, 0);
	pl_flinewidth_r(plot->plotter, 1);
}

void DrawLabelsZeroDBCentered(PlotFile *plot, double dBFS, double dbIncrement, double hz, double hzIncrement,  parameters *config)
{
	double segments = 0;
	char label[20];

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

	pl_fspace_r(plot->plotter, plot->x0, plot->y0, plot->x1, plot->y1);
}

void DrawLabelsMDF(PlotFile *plot, char *Gname, char *GType, parameters *config)
{
	char	label[BUFFER_SIZE];

	sprintf(label, Gname, GType);
	pl_fspace_r(plot->plotter, 0, -1*config->plotResY/2, config->plotResX, config->plotResY/2);

	pl_fmove_r(plot->plotter, config->plotResX/40, config->plotResY/2-config->plotResY/30);
	pl_pencolor_r(plot->plotter, 0xcccc, 0xcccc, 0xcccc);
	pl_alabel_r(plot->plotter, 'l', 'l', label);

	pl_fmove_r(plot->plotter, config->plotResX/40, -1*config->plotResY/2+config->plotResY/100);
	pl_pencolor_r(plot->plotter, 0, 0xcccc, 0);
	pl_alabel_r(plot->plotter, 'l', 'l', "MDFourier "MDVERSION" for 240p Test Suite by Artemio Urbina");
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

	pl_fspace_r(plot->plotter, plot->x0, plot->y0, plot->x1, plot->y1);
}

void DrawColorScale(PlotFile *plot, char *label, int colorName, double x, double y, double width, double height, double endDbs, double dbIncrement, parameters *config)
{
	double segments = 0;

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
		sprintf(labeldbs, " %c%gdBFS", i*dbIncrement > 0 ? '-' : ' ', i*dbIncrement);
		pl_alabel_r(plot->plotter, 'c', 'c', labeldbs);
	}

	SetPenColor(colorName, 0xaaaa, plot);
	pl_fmove_r(plot->plotter, x+width/2, y-config->plotResY/50);
	pl_alabel_r(plot->plotter, 'c', 'c', label);
}

void DrawColorAllTypeScale(PlotFile *plot, double x, double y, double width, double height, double endDbs, double dbIncrement, parameters *config)
{
	double 	segments = 0;
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
		SetPenColor(colorName[t], 0xaaaa, plot);
		pl_fmove_r(plot->plotter, x+1.8*width+config->plotResX/60, y+(numTypes-1)*config->plotResY/50-t*config->plotResY/50);
		pl_alabel_r(plot->plotter, 'l', 'l', GetTypeName(config, typeID[t]));
	}

	free(colorName);
	colorName = NULL;
	free(typeID);
	typeID = NULL;
}

void DrawMatchBar(PlotFile *plot, int colorName, double x, double y, double width, double height, double notFound, double total, parameters *config)
{
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
	pl_fbox_r(plot->plotter, x, y, x+width, y+(notFound*height/total));

	// Border
	pl_filltype_r(plot->plotter, 0);
	SetPenColor(COLOR_GRAY, 0xAAAA, plot);
	pl_fbox_r(plot->plotter, x, y, x+width, y+height);

	pl_filltype_r(plot->plotter, 0);
}

void DrawMatchBarLabel(PlotFile *plot, char *label, int colorName, double x, double y, parameters *config)
{
	pl_ffontsize_r(plot->plotter, config->plotResY/60);
	pl_ffontname_r(plot->plotter, "HersheySans");

	SetPenColor(colorName, 0xaaaa, plot);
	pl_fmove_r(plot->plotter, x, y);
	pl_alabel_r(plot->plotter, 'c', 'c', label);
}

void PlotAllDifferentAmplitudes(FlatAmplDifference *amplDiff, char *filename, parameters *config)
{
	PlotFile	plot;
	double		posx = 0;
	int			typeCount = 0;
	char		name[BUFFER_SIZE];
	double		dBFS = config->maxDbPlotZC;

	if(!config)
		return;

	if(!amplDiff)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	sprintf(name, "DA_ALL_%s", filename);
	FillPlot(&plot, name, config->plotResX, config->plotResY, config->startHzPlot, -1*dBFS, config->endHzPlot, dBFS, 1, config);

	if(!CreatePlotFile(&plot))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, 3, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, 3, config->endHzPlot, 1000, config);

	for(int a = 0; a < config->Differences.cntAmplAudioDiff; a++)
	{
		if(amplDiff[a].type > TYPE_CONTROL)
		{ 
			long int intensity;

			intensity = CalculateWeightedError((fabs(config->significantVolume) - fabs(amplDiff[a].refAmplitude))/fabs(config->significantVolume), config)*0xffff;

			SetPenColor(amplDiff[a].color, intensity, &plot);
			pl_fpoint_r(plot.plotter, transformtoLog(amplDiff[a].hertz, config), amplDiff[a].diffAmplitude);
		}
	}

	DrawLabelsMDF(&plot, DIFFERENCE_TITLE, ALL_LABEL, config);
	DrawColorAllTypeScale(&plot, config->plotResX/50, config->plotResY/15, config->plotResX/80, config->plotResY/1.15, config->significantVolume, 3, config);
	DrawMatchBar(&plot, COLOR_GRAY,	BAR_HOR_POS_ALL, BAR_VERT_POS, BAR_WIDTH, BAR_HEIGHT,
			config->Differences.cntAmplAudioDiff, config->Differences.cntTotalCompared, config);

	posx = BAR_HOR_POS_ALL+BAR_WIDTH*2;
	for(int i = 0; i < config->types.typeCount; i++)
	{
		int			type;
		long int	cntAmplBlkDiff, cmpAmplBlkDiff;

		type = config->types.typeArray[i].type;
		if(type > TYPE_CONTROL && FindDifferenceTypeTotals(type, &cntAmplBlkDiff, &cmpAmplBlkDiff, config))
		{
			DrawMatchBar(&plot, MatchColor(GetTypeColor(config, type)), 
				posx+BAR_WIDTH*typeCount, BAR_VERT_POS, 
				BAR_WIDTH, BAR_HEIGHT, 
				cntAmplBlkDiff, cmpAmplBlkDiff, config);
			typeCount++;
		}
	}

	DrawMatchBarLabel(&plot, "Percentage Found", COLOR_GRAY, BAR_HOR_POS_ALL+BAR_WIDTH*typeCount, BAR_LABEL_POS, config);

	ClosePlot(&plot);
}

int PlotEachTypeDifferentAmplitudes(FlatAmplDifference *amplDiff, char *filename, parameters *config)
{
	int 		i = 0, type = 0, types = 0;
	char		name[BUFFER_SIZE];

	for(i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;
		if(type > TYPE_CONTROL)
		{
			sprintf(name, "DA_%s_%02d%s_", filename, 
						config->types.typeArray[i].type, config->types.typeArray[i].typeName);
			PlotSingleTypeDifferentAmplitudes(amplDiff, type, name, config);
			logmsg(PLOT_ADVANCE_CHAR);
			types ++;
		}
	}
	return types;
}

void PlotSingleTypeDifferentAmplitudes(FlatAmplDifference *amplDiff, int type, char *filename, parameters *config)
{
	PlotFile	plot;
	long int	cntAmplBlkDiff = 0, cmpAmplBlkDiff = 0;
	double		dBFS = config->maxDbPlotZC;

	if(!config)
		return;

	if(!amplDiff)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	FillPlot(&plot, filename, config->plotResX, config->plotResY, config->startHzPlot, -1*dBFS, config->endHzPlot, dBFS, 1, config);

	if(!CreatePlotFile(&plot))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, 3, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, 3, config->endHzPlot, 1000, config);

	for(int a = 0; a < config->Differences.cntAmplAudioDiff; a++)
	{
		if(amplDiff[a].type == type)
		{ 
			long int intensity;

			intensity = CalculateWeightedError((fabs(config->significantVolume) - fabs(amplDiff[a].refAmplitude))/fabs(config->significantVolume), config)*0xffff;

			SetPenColor(amplDiff[a].color, intensity, &plot);
			pl_fpoint_r(plot.plotter, transformtoLog(amplDiff[a].hertz, config), amplDiff[a].diffAmplitude);
		}
	}

	DrawColorScale(&plot, GetTypeName(config, type), MatchColor(GetTypeColor(config, type)),
			config->plotResX/50, config->plotResY/15, 
			config->plotResX/80, config->plotResY/1.15,
			config->significantVolume, 3, config);
	if(FindDifferenceTypeTotals(type, &cntAmplBlkDiff, &cmpAmplBlkDiff, config))
	{
		DrawMatchBar(&plot, MatchColor(GetTypeColor(config, type)), 
			BAR_HOR_POS, BAR_VERT_POS, BAR_WIDTH, BAR_HEIGHT,
			cntAmplBlkDiff, cmpAmplBlkDiff, config);
		DrawMatchBarLabel(&plot, "Percentage Found", MatchColor(GetTypeColor(config, type)), BAR_HOR_POS+BAR_WIDTH, BAR_LABEL_POS, config);
	}
	DrawLabelsMDF(&plot, DIFFERENCE_TITLE, GetTypeName(config, type), config);
	ClosePlot(&plot);
}

void PlotAllMissingFrequencies(FlatFreqDifference *freqDiff, char *filename, parameters *config)
{
	PlotFile	plot;
	double		posx;
	int			typeCount = 0;
	char		name[BUFFER_SIZE];

	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	sprintf(name, "MIS_ALL_%s", filename);
	FillPlot(&plot, name, config->plotResX, config->plotResY, config->startHzPlot, config->significantVolume, config->endHzPlot, 0.0, 1, config);

	if(!CreatePlotFile(&plot))
		return;

	DrawGridZeroToLimit(&plot, config->significantVolume, 3, config->endHzPlot, 1000, config);
	DrawLabelsZeroToLimit(&plot, config->significantVolume, 3, config->endHzPlot, 1000, config);

	for(int f = 0; f < config->Differences.cntFreqAudioDiff; f++)
	{
		if(freqDiff[f].type > TYPE_CONTROL)
		{ 
			long int intensity;
			double x, y;

			x = transformtoLog(freqDiff[f].hertz, config);
			y = freqDiff[f].amplitude;

			intensity = CalculateWeightedError((fabs(config->significantVolume) - fabs(freqDiff[f].amplitude))/fabs(config->significantVolume), config)*0xffff;

			SetPenColor(freqDiff[f].color, intensity, &plot);
			pl_fline_r(plot.plotter, x,	y, x, config->significantVolume);
		}
	}
	
	DrawColorAllTypeScale(&plot, config->plotResX/50, config->plotResY/15, config->plotResX/80, config->plotResY/1.15, config->significantVolume, 3, config);
	DrawMatchBar(&plot, COLOR_GRAY,	BAR_HOR_POS_ALL, BAR_VERT_POS, BAR_WIDTH, BAR_HEIGHT,
			config->Differences.cntFreqAudioDiff, config->Differences.cntTotalCompared, config);

	posx = BAR_HOR_POS_ALL+BAR_WIDTH*2;
	for(int i = 0; i < config->types.typeCount; i++)
	{
		int			type;
		long int	cntFreqBlkDiff, cmpFreqBlkDiff;

		type = config->types.typeArray[i].type;
		if(type > TYPE_CONTROL && FindMissingTypeTotals(type, &cntFreqBlkDiff, &cmpFreqBlkDiff, config))
		{
			DrawMatchBar(&plot, MatchColor(GetTypeColor(config, type)), 
				posx+BAR_WIDTH*typeCount, BAR_VERT_POS, 
				BAR_WIDTH, BAR_HEIGHT, 
				cntFreqBlkDiff, cmpFreqBlkDiff, config);
			typeCount++;
		}
	}

	DrawMatchBarLabel(&plot, "Percentage Missing", COLOR_GRAY, BAR_HOR_POS_ALL+BAR_WIDTH*typeCount, BAR_LABEL_POS, config);
	DrawLabelsMDF(&plot, MISSING_TITLE, ALL_LABEL, config);
	ClosePlot(&plot);
}

int PlotEachTypeMissingFrequencies(FlatFreqDifference *freqDiff, char *filename, parameters *config)
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
			PlotSingleTypeMissingFrequencies(freqDiff, type, name, config);
			logmsg(PLOT_ADVANCE_CHAR);
			types ++;
		}
	}
	return types;
}

void PlotSingleTypeMissingFrequencies(FlatFreqDifference *freqDiff, int type, char *filename, parameters *config)
{
	PlotFile	plot;
	long int	cntFreqBlkDiff, cmpFreqBlkDiff;

	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	FillPlot(&plot, filename, config->plotResX, config->plotResY, config->startHzPlot, config->significantVolume, config->endHzPlot, 0.0, 1, config);

	if(!CreatePlotFile(&plot))
		return;

	DrawGridZeroToLimit(&plot, config->significantVolume, 3, config->endHzPlot, 1000, config);
	DrawLabelsZeroToLimit(&plot, config->significantVolume, 3, config->endHzPlot, 1000, config);

	for(int f = 0; f < config->Differences.cntFreqAudioDiff; f++)
	{
		if(freqDiff[f].type == type)
		{
			long int intensity;
			double x, y;

			x = transformtoLog(freqDiff[f].hertz, config);;
			y = freqDiff[f].amplitude;
			intensity = CalculateWeightedError((fabs(config->significantVolume) - fabs(freqDiff[f].amplitude))/fabs(config->significantVolume), config)*0xffff;


			SetPenColor(freqDiff[f].color, intensity, &plot);
			pl_fline_r(plot.plotter, x,	y, x, config->significantVolume);
		}
	}
	
	DrawColorScale(&plot, GetTypeName(config, type), MatchColor(GetTypeColor(config, type)), config->plotResX/50, config->plotResY/15, config->plotResX/80, config->plotResY/1.15, config->significantVolume, 3, config);
	if(FindMissingTypeTotals(type, &cntFreqBlkDiff, &cmpFreqBlkDiff, config))
	{
		DrawMatchBar(&plot, MatchColor(GetTypeColor(config, type)), 
			BAR_HOR_POS, BAR_VERT_POS, BAR_WIDTH, BAR_HEIGHT,
			cntFreqBlkDiff, cmpFreqBlkDiff, config);
		DrawMatchBarLabel(&plot, "Percentage Missing", MatchColor(GetTypeColor(config, type)), BAR_HOR_POS+BAR_WIDTH, BAR_LABEL_POS, config);
	}
	DrawLabelsMDF(&plot, MISSING_TITLE, GetTypeName(config, type), config);
	ClosePlot(&plot);
}

void PlotAllSpectrogram(FlatFrequency *freqs, long int size, char *filename, int signal, parameters *config)
{
	PlotFile plot;
	char	 name[BUFFER_SIZE];

	if(!config)
		return;

	sprintf(name, "SP_ALL_%s", filename);
	FillPlot(&plot, name, config->plotResX, config->plotResY, config->startHzPlot, config->significantVolume, config->endHzPlot, 0.0, 1, config);

	if(!CreatePlotFile(&plot))
		return;

	DrawGridZeroToLimit(&plot, config->significantVolume, 3, config->endHzPlot, 1000, config);
	DrawLabelsZeroToLimit(&plot, config->significantVolume, 3, config->endHzPlot, 1000, config);

	for(int f = 0; f < size; f++)
	{
		if(freqs[f].type > TYPE_CONTROL)
		{ 
			long int intensity;
			double x, y;

			x = transformtoLog(freqs[f].hertz, config);
			y = freqs[f].amplitude;
			intensity = CalculateWeightedError((fabs(config->significantVolume) - fabs(freqs[f].amplitude))/fabs(config->significantVolume), config)*0xffff;
	
			SetPenColor(freqs[f].color, intensity, &plot);
			pl_fline_r(plot.plotter, x,	y, x, config->significantVolume);
		}
	}

	DrawColorAllTypeScale(&plot, config->plotResX/50, config->plotResY/15, config->plotResX/80, config->plotResY/1.15, config->significantVolume, 3, config);
	DrawLabelsMDF(&plot, signal == ROLE_REF ? SPECTROGRAM_TITLE_REF : SPECTROGRAM_TITLE_COM, ALL_LABEL, config);
	ClosePlot(&plot);
}

int PlotEachTypeSpectrogram(FlatFrequency *freqs, long int size, char *filename, int signal, parameters *config)
{
	int 		i = 0, type = 0, types = 0;
	char		name[BUFFER_SIZE];

	for(i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;
		if(type > TYPE_CONTROL)
		{
			sprintf(name, "SP_%s_%02d%s", filename, 
					config->types.typeArray[i].type, config->types.typeArray[i].typeName);
			PlotSingleTypeSpectrogram(freqs, size, type, name, signal, config);
			logmsg(PLOT_ADVANCE_CHAR);
			types ++;
		}
	}
	return types;
}

void PlotSingleTypeSpectrogram(FlatFrequency *freqs, long int size, int type, char *filename, int signal, parameters *config)
{
	PlotFile plot;

	if(!config)
		return;

	FillPlot(&plot, filename, config->plotResX, config->plotResY, config->startHzPlot, config->significantVolume, config->endHzPlot, 0.0, 1, config);

	if(!CreatePlotFile(&plot))
		return;

	DrawLabelsZeroToLimit(&plot, config->significantVolume, 3, config->endHzPlot, 1000, config);
	DrawGridZeroToLimit(&plot, config->significantVolume, 3, config->endHzPlot, 1000, config);

	for(int f = 0; f < size; f++)
	{
		if(freqs[f].type == type)
		{ 
			long int intensity;
			double x, y;

			x = transformtoLog(freqs[f].hertz, config);
			y = freqs[f].amplitude;
			intensity = CalculateWeightedError((fabs(config->significantVolume) - fabs(freqs[f].amplitude))/fabs(config->significantVolume), config)*0xffff;
	
			//pl_flinewidth_r(plot.plotter, 100*range_0_1);
			SetPenColor(freqs[f].color, intensity, &plot);
			pl_fline_r(plot.plotter, x,	y, x, config->significantVolume);
		}
	}
	
	DrawColorScale(&plot, GetTypeName(config, type), MatchColor(GetTypeColor(config, type)), config->plotResX/50, config->plotResY/15, config->plotResX/80, config->plotResY/1.15, config->significantVolume, 3, config);
	DrawLabelsMDF(&plot, signal == ROLE_REF ? SPECTROGRAM_TITLE_REF : SPECTROGRAM_TITLE_COM, GetTypeName(config, type), config);
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

		PlotWindow(wm, wm->windowArray[i].frames, config);
	}
}

void PlotWindow(windowManager *wm, long int frames, parameters *config)
{
	PlotFile plot;
	char	 name[BUFFER_SIZE];
	double 	 *window = NULL;
	long int size;

	if(!config || !wm || !wm->windowArray)
		return;

	window = getWindowByLength(wm, frames);
	if(!window)
		return;

	size = getWindowSizeByLength(wm, frames);

	sprintf(name, "WindowPlot_%s", GetWindow(config->window));
	FillPlot(&plot, name, 320, 384, 0, -0.1, 1, 1.1, 0.001, config);

	if(!CreatePlotFile(&plot))
		return;

	// Frames Grid
	pl_pencolor_r (plot.plotter, 0, 0x3333, 0);
	for(long int i = 0; i < frames; i++)
		pl_fline_r(plot.plotter, (double)i*1/(double)frames, -0.1, (double)i*1/(double)frames, 1.1);

	// horizontal grid
	pl_pencolor_r (plot.plotter, 0, 0x5555, 0);
	pl_fline_r(plot.plotter, 0, 1, 1, 1);
	pl_fline_r(plot.plotter, 0, 0, 1, 0);

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
	
		if(!CreatePlotFile(&plot))
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

void SortFlatAmplitudeDifferencesByRefAmplitude(FlatAmplDifference *ADiff, long int size)
{
	long int 			i = 0, j = 0;
	FlatAmplDifference 	tmp;
	
	if(!ADiff)
		return;

	for (i = 1; i < size; i++)
	{
		tmp = ADiff[i];
		j = i - 1;
		while(j >= 0 && tmp.refAmplitude < ADiff[j].refAmplitude)
		{
			ADiff[j+1] = ADiff[j];
			j--;
		}
		ADiff[j+1] = tmp;
	}
}

FlatAmplDifference *CreateFlatDifferences(parameters *config)
{
	long int	count = 0;
	FlatAmplDifference *ADiff = NULL;

	if(!config)
		return NULL;

	ADiff = (FlatAmplDifference*)malloc(sizeof(FlatAmplDifference)*config->Differences.cntAmplAudioDiff);
	if(!ADiff)
		return NULL;

	for(int b = 0; b < config->types.totalChunks; b++)
	{
		int type = 0, color = 0;
		
		type = GetBlockType(config, b);
		if(type > TYPE_SILENCE)
		{
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
	SortFlatAmplitudeDifferencesByRefAmplitude(ADiff, config->Differences.cntAmplAudioDiff);
	logmsg(PLOT_PROCESS_CHAR);
	return(ADiff);
}

void SortFlatMissingDifferencesByAmplitude(FlatFreqDifference *FDiff, long int size)
{
	long int 			i = 0, j = 0;
	FlatFreqDifference 	tmp;
	
	if(!FDiff)
		return;

	for (i = 1; i < size; i++)
	{
		tmp = FDiff[i];
		j = i - 1;
		while(j >= 0 && tmp.amplitude < FDiff[j].amplitude)
		{
			FDiff[j+1] = FDiff[j];
			j--;
		}
		FDiff[j+1] = tmp;
	}
}

FlatFreqDifference *CreateFlatMissing(parameters *config)
{
	long int	count = 0;
	FlatFreqDifference *FDiff = NULL;

	if(!config)
		return NULL;

	FDiff = (FlatFreqDifference*)malloc(sizeof(FlatFreqDifference)*config->Differences.cntFreqAudioDiff);
	if(!FDiff)
		return NULL;

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
	SortFlatMissingDifferencesByAmplitude(FDiff, config->Differences.cntFreqAudioDiff);
	logmsg(PLOT_PROCESS_CHAR);
	return(FDiff);
}

void SortFlatFrequenciesByAmplitude(FlatFrequency *Freqs, long int size)
{
	long int 			i = 0, j = 0;
	FlatFrequency 	tmp;
	
	if(!Freqs)
		return;

	for (i = 1; i < size; i++)
	{
		tmp = Freqs[i];
		j = i - 1;
		while(j >= 0 && tmp.amplitude < Freqs[j].amplitude)
		{
			Freqs[j+1] = Freqs[j];
			j--;
		}
		Freqs[j+1] = tmp;
	}
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
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, block);
		if(type > TYPE_SILENCE)
		{
			for(i = 0; i < config->MaxFreq; i++)
			{
				if(Signal->Blocks[block].freq[i].hertz && Signal->Blocks[block].freq[i].amplitude > config->significantVolume)
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
	for(block = 0; block < config->types.totalChunks; block++)
	{
		int type = 0, color = 0;

		type = GetBlockType(config, block);
		if(type > TYPE_SILENCE)
		{
			color = MatchColor(GetBlockColor(config, block));
	
			for(i = 0; i < config->MaxFreq; i++)
			{
				if(Signal->Blocks[block].freq[i].hertz && Signal->Blocks[block].freq[i].amplitude > config->significantVolume)
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
	SortFlatFrequenciesByAmplitude(Freqs, count);
	logmsg(PLOT_PROCESS_CHAR);

	*size = counter;
	return(Freqs);
}


void PlotTest(char *filename, parameters *config)
{
	PlotFile	plot;
	int			typeCount = 0;
	char		name[BUFFER_SIZE];
	double		dBFS = config->maxDbPlotZC, posx = 0	;

	if(!config)
		return;

	sprintf(name, "Test_%s", filename);
	FillPlot(&plot, name, config->plotResX, config->plotResY, config->startHzPlot, -1*dBFS, config->endHzPlot, dBFS, 1, config);

	if(!CreatePlotFile(&plot))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, 3, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, 3, config->endHzPlot, 1000, config);

	srand(time(NULL));

	DrawLabelsMDF(&plot, "PLOT TEST [%s]", "ZDBC", config);
	DrawColorAllTypeScale(&plot, config->plotResX/50, config->plotResY/15, config->plotResX/80, config->plotResY/1.15, config->significantVolume, 3, config);
	DrawMatchBar(&plot, COLOR_GRAY, BAR_HOR_POS_ALL, BAR_VERT_POS, BAR_WIDTH, BAR_HEIGHT,
			rand() % 100, 100, config);

	posx = BAR_HOR_POS_ALL+BAR_WIDTH*2;
	for(int i = 0; i < config->types.typeCount; i++)
	{
		int			type;
		long int	cntAmplBlkDiff = rand() % 100, cmpAmplBlkDiff = 100;

		type = config->types.typeArray[i].type;
		if(type > TYPE_CONTROL)
		{
			DrawMatchBar(&plot, MatchColor(GetTypeColor(config, type)), 
				posx+BAR_WIDTH*typeCount, BAR_VERT_POS, 
				BAR_WIDTH, BAR_HEIGHT, 
				cntAmplBlkDiff, cmpAmplBlkDiff, config);
			typeCount++;
		}
	}

	DrawMatchBarLabel(&plot, "Percentage Found", COLOR_GRAY, BAR_HOR_POS_ALL+BAR_WIDTH*typeCount, BAR_LABEL_POS, config);

	ClosePlot(&plot);
}

void PlotTestZL(char *filename, parameters *config)
{
	PlotFile	plot;
	char		name[BUFFER_SIZE];

	if(!config)
		return;

	sprintf(name, "Test_ZL_%s", filename);
	FillPlot(&plot, name, config->plotResX, config->plotResY, config->startHzPlot, config->significantVolume, config->endHzPlot, 0.0, 1, config);

	if(!CreatePlotFile(&plot))
		return;

	DrawGridZeroToLimit(&plot, config->significantVolume, 3, config->endHzPlot, 1000, config);
	DrawLabelsZeroToLimit(&plot, config->significantVolume, 3, config->endHzPlot, 1000, config);

	DrawColorScale(&plot, "Test", COLOR_ORANGE, config->plotResX/50, config->plotResY/15, config->plotResX/80, config->plotResY/1.15, -60, 3, config);
	
	DrawLabelsMDF(&plot, "PLOT TEST [%s]", "GZL", config);
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

void SortFlatAmplitudeDifferencesByFrequencyAveraged(FlatAmplDifference *ADiff, long int size)
{
	long int 			i = 0, j = 0;
	FlatAmplDifference 	tmp;
	
	if(!ADiff)
		return;

	for (i = 1; i < size; i++)
	{
		tmp = ADiff[i];
		j = i - 1;
		while(j >= 0 && tmp.hertz < ADiff[j].hertz)
		{
			ADiff[j+1] = ADiff[j];
			j--;
		}
		ADiff[j+1] = tmp;
	}
}

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

AveragedFrequencies *CreateFlatDifferencesAveraged(int matchType, long int *avgSize, int chunks, parameters *config)
{
	long int			count = 0, interval = 0, realResults = 0;
	FlatAmplDifference	*ADiff = NULL;
	AveragedFrequencies	*averaged = NULL, *averagedSMA = NULL;

	if(!config)
		return NULL;

	if(!avgSize)
		return NULL;

	*avgSize = 0;

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
		
		type = GetBlockType(config, b);
		if(type == matchType)
		{
			for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
			{
				if(config->weightedAveragePlot)
				{
					double intensity = 0;

					intensity = CalculateWeightedError(
									(fabs(config->significantVolume) - fabs(config->Differences.BlockDiffArray[b].amplDiffArray[a].refAmplitude))/
									fabs(config->significantVolume), config);
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

	if(!count)
	{
		free(ADiff);
		return NULL;
	}

	averaged = (AveragedFrequencies*)malloc(sizeof(AveragedFrequencies)*count);
	if(!averaged)
	{
		free(ADiff);
		return NULL;
	}
	memset(averaged, 0, sizeof(AveragedFrequencies)*count);

	SortFlatAmplitudeDifferencesByFrequencyAveraged(ADiff, count);
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

	*avgSize = movingAverage(averaged, averagedSMA, realResults, SMA_SIZE);

	free(averaged);
	averaged = NULL;

	return(averagedSMA);
	
	//*avgSize = realResults;
	//return averaged;
}

int PlotDifferentAmplitudesAveraged(FlatAmplDifference *amplDiff, char *filename, parameters *config)
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

			averagedArray[types] = CreateFlatDifferencesAveraged(type, &averagedSizes[types], chunks, config);

			if(averagedArray[types])
			{
				PlotSingleTypeDifferentAmplitudesAveraged(amplDiff, type, name, averagedArray[types], averagedSizes[types], config);
				logmsg(PLOT_ADVANCE_CHAR);
			}

			types ++;
		}
	}

	if(types && averagedArray && averagedSizes)
	{
		sprintf(name, "DA_ALL_AVG_%s", filename);
		PlotAllDifferentAmplitudesAveraged(amplDiff, name, averagedArray, averagedSizes, config);
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

void PlotSingleTypeDifferentAmplitudesAveraged(FlatAmplDifference *amplDiff, int type, char *filename, AveragedFrequencies *averaged, long int avgsize, parameters *config)
{
	PlotFile	plot;
	long int	cntAmplBlkDiff, cmpAmplBlkDiff;
	double		dbs = config->maxDbPlotZC;
	int			color = 0;

	if(!config)
		return;

	if(!amplDiff)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	FillPlot(&plot, filename, config->plotResX, config->plotResY, config->startHzPlot, -1*dbs, config->endHzPlot, dbs, 1, config);

	if(!CreatePlotFile(&plot))
		return;

	DrawGridZeroDBCentered(&plot, dbs, 3, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dbs, 3, config->endHzPlot, 1000, config);

	for(long int a = 0; a < config->Differences.cntAmplAudioDiff; a++)
	{
		if(amplDiff[a].type == type)
		{ 
			long int intensity;

			intensity = CalculateWeightedError((fabs(config->significantVolume) - fabs(amplDiff[a].refAmplitude))/fabs(config->significantVolume), config)*0xffff;

			SetPenColor(amplDiff[a].color, intensity, &plot);
			pl_fpoint_r(plot.plotter, transformtoLog(amplDiff[a].hertz, config), amplDiff[a].diffAmplitude);
		}
	}

	color = MatchColor(GetTypeColor(config, type));
	pl_endpath_r(plot.plotter);

	if(averaged && avgsize > 1)
	{
		int first = 1;

		SetPenColor(color, 0xffff, &plot);

		/*
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

		SetPenColor(color, 0xffff, &plot);
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

	DrawColorScale(&plot, GetTypeName(config, type), MatchColor(GetTypeColor(config, type)), config->plotResX/50, config->plotResY/15, config->plotResX/80, config->plotResY/1.15, config->significantVolume, 3, config);
	if(FindDifferenceTypeTotals(type, &cntAmplBlkDiff, &cmpAmplBlkDiff, config))
	{
		DrawMatchBar(&plot, MatchColor(GetTypeColor(config, type)), 
			BAR_HOR_POS, BAR_VERT_POS, BAR_WIDTH, BAR_HEIGHT,
			cntAmplBlkDiff, cmpAmplBlkDiff, config);
		DrawMatchBarLabel(&plot, "Percentage Found", MatchColor(GetTypeColor(config, type)), BAR_HOR_POS+BAR_WIDTH, BAR_LABEL_POS, config);
	}
	DrawLabelsMDF(&plot, DIFFERENCE_AVG_TITLE, GetTypeName(config, type), config);
	ClosePlot(&plot);
}

void PlotAllDifferentAmplitudesAveraged(FlatAmplDifference *amplDiff, char *filename, AveragedFrequencies **averaged, long int *avgsize, parameters *config)
{
	PlotFile	plot;
	double		posx = 0;
	int			typeCount = 0;
	double		dBFS = config->maxDbPlotZC;
	int			currType = 0;

	if(!config)
		return;

	if(!amplDiff)
		return;

	if(!config->Differences.BlockDiffArray)
		return;
	
	FillPlot(&plot, filename, config->plotResX, config->plotResY, config->startHzPlot, -1*dBFS, config->endHzPlot, dBFS, 1, config);

	if(!CreatePlotFile(&plot))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, 3, config->endHzPlot, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, 3, config->endHzPlot, 1000, config);

	for(long int a = 0; a < config->Differences.cntAmplAudioDiff; a++)
	{
		if(amplDiff[a].type > TYPE_CONTROL)
		{ 
			long int intensity;

			intensity = CalculateWeightedError((fabs(config->significantVolume) - fabs(amplDiff[a].refAmplitude))/fabs(config->significantVolume), config)*0xffff;

			SetPenColor(amplDiff[a].color, intensity, &plot);
			pl_fpoint_r(plot.plotter, transformtoLog(amplDiff[a].hertz, config), amplDiff[a].diffAmplitude);
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

			SetPenColor(color, 0xffff, &plot);
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

				//logmsg("Plot [%ld] %g->%g\n", a, averaged[currType][a].avgfreq, averaged[currType][a].avgvol);
			}
			pl_endpath_r(plot.plotter);
		}
		currType++;
	}

	DrawLabelsMDF(&plot, DIFFERENCE_AVG_TITLE, ALL_LABEL, config);
	DrawColorAllTypeScale(&plot, config->plotResX/50, config->plotResY/15, config->plotResX/80, config->plotResY/1.15, config->significantVolume, 3, config);
	DrawMatchBar(&plot, COLOR_GRAY, BAR_HOR_POS_ALL, BAR_VERT_POS, BAR_WIDTH, BAR_HEIGHT,
			config->Differences.cntAmplAudioDiff, config->Differences.cntTotalCompared, config);

	posx = BAR_HOR_POS_ALL+BAR_WIDTH*2;
	for(int i = 0; i < config->types.typeCount; i++)
	{
		int			type;
		long int	cntAmplBlkDiff, cmpAmplBlkDiff;

		type = config->types.typeArray[i].type;
		if(type > TYPE_CONTROL && FindDifferenceTypeTotals(type, &cntAmplBlkDiff, &cmpAmplBlkDiff, config))
		{
			DrawMatchBar(&plot, MatchColor(GetTypeColor(config, type)), 
				posx+BAR_WIDTH*typeCount, BAR_VERT_POS, 
				BAR_WIDTH, BAR_HEIGHT, 
				cntAmplBlkDiff, cmpAmplBlkDiff, config);
			typeCount++;
		}
	}

	DrawMatchBarLabel(&plot, "Percentage Found", COLOR_GRAY, BAR_HOR_POS_ALL+BAR_WIDTH*typeCount, BAR_LABEL_POS, config);
	ClosePlot(&plot);
}