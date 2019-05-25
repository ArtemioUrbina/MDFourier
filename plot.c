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

#define PLOT_RES_X 1600.0
#define PLOT_RES_Y 800.0


void PlotResults(AudioSignal *Signal, parameters *config)
{
	struct	timespec	start, end;

	if(config->clock)
		clock_gettime(CLOCK_MONOTONIC, &start);

	PlotAmpDifferences(config);
	PlotFreqMissing(config);
	PlotSpectrograms(Signal, config);

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
	FlatAmplDifference	*amplDiff = NULL;
	
	amplDiff = CreateFlatDifferences(config);
	if(!amplDiff)
	{
		logmsg("Not enough memory for plotting\n");
		return;
	}

	if(PlotEachTypeDifferentAmplitudes(amplDiff, config->compareName, config) > 1)
		PlotAllDifferentAmplitudes(amplDiff, config->compareName, config);
	
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
		PlotAllMissingFrequencies(freqDiff, config->compareName, config);

	free(freqDiff);
	freqDiff = NULL;
}

void PlotSpectrograms(AudioSignal *Signal, parameters *config)
{
	long int			size = 0;
	FlatFrequency		*frequencies = NULL;

	frequencies = CreateFlatFrequencies(Signal, &size, config);
	if(PlotEachTypeSpectrogram(frequencies, size, basename(Signal->SourceFile), config) > 1)
		PlotAllSpectrogram(frequencies, size, basename(Signal->SourceFile), config);

	free(frequencies);
	frequencies = NULL;
}

int FillPlot(PlotFile *plot, char *name, int sizex, int sizey, double x0, double y0, double x1, double y1, double penWidth, parameters *config)
{
	plot->plotter = NULL;
	plot->plotter_params = NULL;
	plot->file = NULL;

	ComposeFileName(plot->FileName, name, ".png", config);

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
		logmsg("Couldn't create Plot file %s\n", plot->FileName);
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
	pl_pencolor_r (plot->plotter, 0, 0xcccc, 0);
	pl_fline_r(plot->plotter, 0, 0, hz, 0);

	pl_pencolor_r (plot->plotter, 0, 0x5555, 0);
	for(int i = dbIncrement; i < dBFS; i += dbIncrement)
	{
		pl_fline_r(plot->plotter, 0, i, hz, i);
		pl_fline_r(plot->plotter, 0, -1*i, hz, -1*i);
	}

	pl_pencolor_r (plot->plotter, 0, 0x5555, 0);
	pl_fline_r(plot->plotter, transformtoLog(10, hz, config), -1*dBFS, transformtoLog(10, hz, config), dBFS);
	pl_fline_r(plot->plotter, transformtoLog(100, hz, config), -1*dBFS, transformtoLog(100, hz, config), dBFS);
	for(int i = hzIncrement; i < hz; i += hzIncrement)
		pl_fline_r(plot->plotter, transformtoLog(i, hz, config), -1*dBFS, transformtoLog(i, hz, config), dBFS);

	pl_pencolor_r (plot->plotter, 0, 0xFFFF, 0);
}

void DrawGridZeroToLimit(PlotFile *plot, double dBFS, double dbIncrement, double hz, double hzIncrement, parameters *config)
{
	pl_pencolor_r (plot->plotter, 0, 0x7777, 0);
	for(int i = dbIncrement; i < fabs(dBFS); i += dbIncrement)
		pl_fline_r(plot->plotter, 0, -1*i, hz, -1*i);

	pl_pencolor_r (plot->plotter, 0, 0x7777, 0);
	pl_fline_r(plot->plotter, transformtoLog(10, hz, config), dBFS, transformtoLog(10, hz, config), 0);
	pl_fline_r(plot->plotter, transformtoLog(100, hz, config), dBFS, transformtoLog(100, hz, config), 0);
	for(int i = hzIncrement; i < hz; i += hzIncrement)
		pl_fline_r(plot->plotter, transformtoLog(i, hz, config), dBFS, transformtoLog(i, hz, config), 0);

	pl_pencolor_r (plot->plotter, 0, 0xFFFF, 0);
	pl_flinewidth_r(plot->plotter, 1);
}

void DrawLabelsZeroDBCentered(PlotFile *plot, double dBFS, double dbIncrement, double hz, double hzIncrement,  parameters *config)
{
	double segments = 0;
	char label[20];

	pl_fspace_r(plot->plotter, 0, -1*PLOT_RES_Y/2, PLOT_RES_X, PLOT_RES_Y/2);

	pl_pencolor_r(plot->plotter, 0, 0xaaaa, 0);
	pl_ffontsize_r(plot->plotter, PLOT_RES_Y/60);

	pl_fmove_r(plot->plotter, PLOT_RES_X-PLOT_RES_X/80, PLOT_RES_Y/100);
	pl_alabel_r(plot->plotter, 'c', 'c', "0dBFS");

	pl_ffontname_r(plot->plotter, "HersheySans");
	segments = fabs(dBFS/dbIncrement);
	for(int i = 1; i < segments; i ++)
	{
		pl_fmove_r(plot->plotter, PLOT_RES_X-PLOT_RES_X/50, i*PLOT_RES_Y/segments/2+PLOT_RES_Y/100);
		sprintf(label, " %gdBFS", i*dbIncrement);
		pl_alabel_r(plot->plotter, 'c', 'c', label);

		pl_fmove_r(plot->plotter, PLOT_RES_X-PLOT_RES_X/50, -1*i*PLOT_RES_Y/segments/2+PLOT_RES_Y/100);
		sprintf(label, "-%gdBFS", i*dbIncrement);
		pl_alabel_r(plot->plotter, 'c', 'c', label);
	}

	if(config->logScale)
	{
		pl_fmove_r(plot->plotter, PLOT_RES_X/hz*transformtoLog(10, hz, config), PLOT_RES_Y/2-PLOT_RES_Y/100);
		sprintf(label, "%dHz", 10);
		pl_alabel_r(plot->plotter, 'c', 'c', label);
	
		pl_fmove_r(plot->plotter, PLOT_RES_X/hz*transformtoLog(100, hz, config), PLOT_RES_Y/2-PLOT_RES_Y/100);
		sprintf(label, "%dHz", 100);
		pl_alabel_r(plot->plotter, 'c', 'c', label);
	}

	pl_fmove_r(plot->plotter, PLOT_RES_X/hz*transformtoLog(1000, hz, config), PLOT_RES_Y/2-PLOT_RES_Y/100);
	sprintf(label, "  %dHz", 1000);
	pl_alabel_r(plot->plotter, 'c', 'c', label);

	pl_fmove_r(plot->plotter, PLOT_RES_X/hz*transformtoLog(10000, hz, config), PLOT_RES_Y/2-PLOT_RES_Y/100);
	sprintf(label, "%dkHz", 10);
	pl_alabel_r(plot->plotter, 'c', 'c', label);

	pl_fspace_r(plot->plotter, plot->x0, plot->y0, plot->x1, plot->y1);
}

void DrawLabelsMDF(PlotFile *plot)
{
	pl_fspace_r(plot->plotter, 0, -1*PLOT_RES_Y/2, PLOT_RES_X, PLOT_RES_Y/2);

	pl_fmove_r(plot->plotter, PLOT_RES_X/40, -1*PLOT_RES_Y/2+PLOT_RES_Y/100);
	pl_pencolor_r(plot->plotter, 0, 0xcccc, 0);
	pl_alabel_r(plot->plotter, 'l', 'l', "MDFourier "MDVERSION" for 240p Test Suite by Artemio Urbina");
}

void DrawLabelsZeroToLimit(PlotFile *plot, double dBFS, double dbIncrement, double hz, double hzIncrement,  parameters *config)
{
	double segments = 0;
	char label[20];

	pl_fspace_r(plot->plotter, 0, -1*PLOT_RES_Y, PLOT_RES_X, 0);
	pl_pencolor_r(plot->plotter, 0, 0xaaaa, 0);
	pl_ffontsize_r(plot->plotter, PLOT_RES_Y/60);

	pl_ffontname_r(plot->plotter, "HersheySans");
	segments = fabs(dBFS/dbIncrement);
	for(int i = 0; i < segments; i ++)
	{
		pl_fmove_r(plot->plotter, PLOT_RES_X-PLOT_RES_X/50, -1*i*PLOT_RES_Y/segments-PLOT_RES_Y/100);
		sprintf(label, "%gdBFS", -1*i*dbIncrement);
		pl_alabel_r(plot->plotter, 'c', 'c', label);
	}

	if(config->logScale)
	{
		pl_fmove_r(plot->plotter, PLOT_RES_X/hz*transformtoLog(10, hz, config), -PLOT_RES_Y/100);
		sprintf(label, "%dHz", 10);
		pl_alabel_r(plot->plotter, 'c', 'c', label);
	
		pl_fmove_r(plot->plotter, PLOT_RES_X/hz*transformtoLog(100, hz, config), -PLOT_RES_Y/100);
		sprintf(label, "%dHz", 100);
		pl_alabel_r(plot->plotter, 'c', 'c', label);
	}

	pl_fmove_r(plot->plotter, PLOT_RES_X/hz*transformtoLog(1000, hz, config), -PLOT_RES_Y/100);
	sprintf(label, "  %dHz", 1000);
	pl_alabel_r(plot->plotter, 'c', 'c', label);

	pl_fmove_r(plot->plotter, PLOT_RES_X/hz*transformtoLog(10000, hz, config), -PLOT_RES_Y/100);
	sprintf(label, "%dkHz", 10);
	pl_alabel_r(plot->plotter, 'c', 'c', label);

	pl_fspace_r(plot->plotter, plot->x0, plot->y0, plot->x1, plot->y1);
}

void DrawColorScale(PlotFile *plot, char *label, int colorName, double x, double y, double width, double height, double endDbs, double dbIncrement, parameters *config)
{
	double segments = 0;

	pl_fspace_r(plot->plotter, 0, 0, PLOT_RES_X, PLOT_RES_Y);
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
	pl_ffontsize_r(plot->plotter, PLOT_RES_Y/60);
	pl_ffontname_r(plot->plotter, "HersheySans");

	for(double i = 0; i < segments; i++)
	{
		char labeldbs[20];

		pl_fmove_r(plot->plotter, x+width+PLOT_RES_X/60, y+height-i*height/segments-height/segments/2);
		sprintf(labeldbs, " %c%gdBFS", i*dbIncrement > 0 ? '-' : ' ', i*dbIncrement);
		pl_alabel_r(plot->plotter, 'c', 'c', labeldbs);
	}

	SetPenColor(colorName, 0xaaaa, plot);
	pl_fmove_r(plot->plotter, x+width/2, y+height+PLOT_RES_Y/100);
	pl_alabel_r(plot->plotter, 'c', 'c', label);
}

void DrawColorAllTypeScale(PlotFile *plot, double x, double y, double width, double height, double endDbs, double dbIncrement, parameters *config)
{
	double 	segments = 0;
	int		*colorName = NULL, *typeID = NULL;
	int		numTypes = 0, t = 0;

	pl_fspace_r(plot->plotter, 0, 0, PLOT_RES_X, PLOT_RES_Y);
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
	pl_ffontsize_r(plot->plotter, PLOT_RES_Y/60);
	pl_ffontname_r(plot->plotter, "HersheySans");

	for(double i = 0; i < segments; i++)
	{
		char label[20];

		pl_fmove_r(plot->plotter, x+width+PLOT_RES_X/60, y+height-i*height/segments-height/segments/2);
		sprintf(label, " %c%gdBFS", i*dbIncrement > 0 ? '-' : ' ', i*dbIncrement);
		pl_alabel_r(plot->plotter, 'c', 'c', label);
	}

	for(int t = 0; t < numTypes; t++)
	{
		SetPenColor(colorName[t], 0xaaaa, plot);
		pl_fmove_r(plot->plotter, x+2*width+PLOT_RES_X/60, y+(numTypes-1)*PLOT_RES_Y/50-t*PLOT_RES_Y/50);
		pl_alabel_r(plot->plotter, 'l', 'l', GetTypeName(config, typeID[t]));
	}

	free(colorName);
	colorName = NULL;
	free(typeID);
	typeID = NULL;
}

void PlotAllDifferentAmplitudes(FlatAmplDifference *amplDiff, char *filename, parameters *config)
{
	PlotFile	plot;
	char		name[2048];
	double		dBFS = DB_HEIGHT;

	if(!config)
		return;

	if(!amplDiff)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	sprintf(name, "DifferentAmplitudes_ALL_%s", filename);
	FillPlot(&plot, name, PLOT_RES_X, PLOT_RES_Y, 0, -1*dBFS, TOP_FREQUENCY, dBFS, 1, config);

	if(!CreatePlotFile(&plot))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, 3, TOP_FREQUENCY, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, 3, TOP_FREQUENCY, 1000, config);

	for(int a = 0; a < config->Differences.cntAmplAudioDiff; a++)
	{
		if(amplDiff[a].type > TYPE_CONTROL)
		{ 
			long int intensity;

			intensity = CalculateWeightedError((fabs(config->significantVolume) - fabs(amplDiff[a].refAmplitude))/fabs(config->significantVolume), config)*0xffff;

			SetPenColor(amplDiff[a].color, intensity, &plot);
			pl_fpoint_r(plot.plotter, transformtoLog(amplDiff[a].hertz, TOP_FREQUENCY, config), amplDiff[a].diffAmplitude);
		}
	}

	DrawLabelsMDF(&plot);
	DrawColorAllTypeScale(&plot, PLOT_RES_X/50, PLOT_RES_Y/15, PLOT_RES_X/80, PLOT_RES_Y/1.15, config->significantVolume, 3, config);
	ClosePlot(&plot);
}

int PlotEachTypeDifferentAmplitudes(FlatAmplDifference *amplDiff, char *filename, parameters *config)
{
	int 		i = 0, type = 0, types = 0;
	char		name[2048];

	for(i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;
		if(type > TYPE_CONTROL)
		{
			sprintf(name, "DifferentAmplitudes_%s_%02d%s_", filename, 
						config->types.typeArray[i].type, config->types.typeArray[i].typeName);
			PlotSingleTypeDifferentAmplitudes(amplDiff, type, name, config);
			
			if(config->averagePlot)
			{
				long int chunks = 30, bestFitSize = 0;
				AveragedFrequencies	*bestFit = NULL;
	
				sprintf(name, "DifferentAmplitudes_%s_%02d%s_AVG_", filename, 
						config->types.typeArray[i].type, config->types.typeArray[i].typeName);

				bestFit = BestFit_CreateFlatDifferencesBestFit(type, &bestFitSize, chunks, config);

				if(bestFit)
					PlotSingleTypeDifferentAmplitudesBestFit(amplDiff, type, name, bestFit, bestFitSize, config);
		
				if(bestFit)
					free(bestFit);
			}
			
			types ++;
		}
	}
	return types;
}

void PlotSingleTypeDifferentAmplitudes(FlatAmplDifference *amplDiff, int type, char *filename, parameters *config)
{
	PlotFile	plot;
	double		dBFS = DB_HEIGHT;

	if(!config)
		return;

	if(!amplDiff)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	FillPlot(&plot, filename, PLOT_RES_X, PLOT_RES_Y, 0, -1*dBFS, TOP_FREQUENCY, dBFS, 1, config);

	if(!CreatePlotFile(&plot))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, 3, TOP_FREQUENCY, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, 3, TOP_FREQUENCY, 1000, config);

	for(int a = 0; a < config->Differences.cntAmplAudioDiff; a++)
	{
		if(amplDiff[a].type == type)
		{ 
			long int intensity;

			intensity = CalculateWeightedError((fabs(config->significantVolume) - fabs(amplDiff[a].refAmplitude))/fabs(config->significantVolume), config)*0xffff;

			SetPenColor(amplDiff[a].color, intensity, &plot);
			pl_fpoint_r(plot.plotter, transformtoLog(amplDiff[a].hertz, TOP_FREQUENCY, config), amplDiff[a].diffAmplitude);
		}
	}

	DrawColorScale(&plot, GetTypeName(config, type), MatchColor(GetTypeColor(config, type)), PLOT_RES_X/50, PLOT_RES_Y/15, PLOT_RES_X/80, PLOT_RES_Y/1.15, config->significantVolume, 3, config);
	DrawLabelsMDF(&plot);
	ClosePlot(&plot);
}

void PlotAllMissingFrequencies(FlatFreqDifference *freqDiff, char *filename, parameters *config)
{
	PlotFile plot;
	char	 name[2048];

	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	sprintf(name, "MissingFrequencies_ALL_%s", filename);
	FillPlot(&plot, name, PLOT_RES_X, PLOT_RES_Y, 0, config->significantVolume, TOP_FREQUENCY, 0.0, 1, config);

	if(!CreatePlotFile(&plot))
		return;

	DrawGridZeroToLimit(&plot, config->significantVolume, 3, TOP_FREQUENCY, 1000, config);
	DrawLabelsZeroToLimit(&plot, config->significantVolume, 3, TOP_FREQUENCY, 1000, config);

	for(int f = 0; f < config->Differences.cntFreqAudioDiff; f++)
	{
		if(freqDiff[f].type > TYPE_CONTROL)
		{ 
			long int intensity;
			double x, y;

			x = transformtoLog(freqDiff[f].hertz, TOP_FREQUENCY, config);
			y = freqDiff[f].amplitude;

			intensity = CalculateWeightedError((fabs(config->significantVolume) - fabs(freqDiff[f].amplitude))/fabs(config->significantVolume), config)*0xffff;

			SetPenColor(freqDiff[f].color, intensity, &plot);
			pl_fline_r(plot.plotter, x,	y, x, config->significantVolume);
		}
	}
	
	DrawColorAllTypeScale(&plot, PLOT_RES_X/50, PLOT_RES_Y/15, PLOT_RES_X/80, PLOT_RES_Y/1.15, config->significantVolume, 3, config);
	DrawLabelsMDF(&plot);
	ClosePlot(&plot);
}

int PlotEachTypeMissingFrequencies(FlatFreqDifference *freqDiff, char *filename, parameters *config)
{
	int 		i = 0, type = 0, types = 0;
	char		name[2048];

	for(i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;
		if(type > TYPE_CONTROL)
		{
			sprintf(name, "MissingFrequencies_%s_%02d%s", filename, 
							config->types.typeArray[i].type, config->types.typeArray[i].typeName);
			PlotSingleTypeMissingFrequencies(freqDiff, type, name, config);
			types ++;
		}
	}
	return types;
}

void PlotSingleTypeMissingFrequencies(FlatFreqDifference *freqDiff, int type, char *filename, parameters *config)
{
	PlotFile plot;

	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	FillPlot(&plot, filename, PLOT_RES_X, PLOT_RES_Y, 0, config->significantVolume, TOP_FREQUENCY, 0.0, 1, config);

	if(!CreatePlotFile(&plot))
		return;

	DrawGridZeroToLimit(&plot, config->significantVolume, 3, TOP_FREQUENCY, 1000, config);
	DrawLabelsZeroToLimit(&plot, config->significantVolume, 3, TOP_FREQUENCY, 1000, config);

	for(int f = 0; f < config->Differences.cntFreqAudioDiff; f++)
	{
		if(freqDiff[f].type == type)
		{
			long int intensity;
			double x, y;

			x = transformtoLog(freqDiff[f].hertz, TOP_FREQUENCY, config);;
			y = freqDiff[f].amplitude;
			intensity = CalculateWeightedError((fabs(config->significantVolume) - fabs(freqDiff[f].amplitude))/fabs(config->significantVolume), config)*0xffff;


			SetPenColor(freqDiff[f].color, intensity, &plot);
			pl_fline_r(plot.plotter, x,	y, x, config->significantVolume);
		}
	}
	
	DrawColorScale(&plot, GetTypeName(config, type), MatchColor(GetTypeColor(config, type)), PLOT_RES_X/50, PLOT_RES_Y/15, PLOT_RES_X/80, PLOT_RES_Y/1.15, config->significantVolume, 3, config);
	DrawLabelsMDF(&plot);
	ClosePlot(&plot);
}

void PlotAllSpectrogram(FlatFrequency *freqs, long int size, char *filename, parameters *config)
{
	PlotFile plot;
	char	 name[2048];

	if(!config)
		return;

	sprintf(name, "Spectrogram_ALL_%s", filename);
	FillPlot(&plot, name, PLOT_RES_X, PLOT_RES_Y, 0, config->significantVolume, TOP_FREQUENCY, 0.0, 1, config);

	if(!CreatePlotFile(&plot))
		return;

	DrawGridZeroToLimit(&plot, config->significantVolume, 3, TOP_FREQUENCY, 1000, config);
	DrawLabelsZeroToLimit(&plot, config->significantVolume, 3, TOP_FREQUENCY, 1000, config);

	for(int f = 0; f < size; f++)
	{
		if(freqs[f].type > TYPE_CONTROL)
		{ 
			long int intensity;
			double x, y;

			x = transformtoLog(freqs[f].hertz, TOP_FREQUENCY, config);
			y = freqs[f].amplitude;
			intensity = CalculateWeightedError((fabs(config->significantVolume) - fabs(freqs[f].amplitude))/fabs(config->significantVolume), config)*0xffff;
	
			SetPenColor(freqs[f].color, intensity, &plot);
			pl_fline_r(plot.plotter, x,	y, x, config->significantVolume);
		}
	}

	DrawColorAllTypeScale(&plot, PLOT_RES_X/50, PLOT_RES_Y/15, PLOT_RES_X/80, PLOT_RES_Y/1.15, config->significantVolume, 3, config);
	DrawLabelsMDF(&plot);
	ClosePlot(&plot);
}

int PlotEachTypeSpectrogram(FlatFrequency *freqs, long int size, char *filename, parameters *config)
{
	int 		i = 0, type = 0, types = 0;
	char		name[2048];

	for(i = 0; i < config->types.typeCount; i++)
	{
		type = config->types.typeArray[i].type;
		if(type > TYPE_CONTROL)
		{
			sprintf(name, "Spectrogram_%s_%02d%s", filename, 
					config->types.typeArray[i].type, config->types.typeArray[i].typeName);
			PlotSingleTypeSpectrogram(freqs, size, type, name, config);
			types ++;
		}
	}
	return types;
}

void PlotSingleTypeSpectrogram(FlatFrequency *freqs, long int size, int type, char *filename, parameters *config)
{
	PlotFile plot;

	if(!config)
		return;

	FillPlot(&plot, filename, PLOT_RES_X, PLOT_RES_Y, 0, config->significantVolume, TOP_FREQUENCY, 0.0, 1, config);

	if(!CreatePlotFile(&plot))
		return;

	DrawLabelsZeroToLimit(&plot, config->significantVolume, 3, TOP_FREQUENCY, 1000, config);
	DrawGridZeroToLimit(&plot, config->significantVolume, 3, TOP_FREQUENCY, 1000, config);

	for(int f = 0; f < size; f++)
	{
		if(freqs[f].type == type)
		{ 
			long int intensity;
			double x, y;

			x = transformtoLog(freqs[f].hertz, TOP_FREQUENCY, config);
			y = freqs[f].amplitude;
			intensity = CalculateWeightedError((fabs(config->significantVolume) - fabs(freqs[f].amplitude))/fabs(config->significantVolume), config)*0xffff;
	
			//pl_flinewidth_r(plot.plotter, 100*range_0_1);
			SetPenColor(freqs[f].color, intensity, &plot);
			pl_fline_r(plot.plotter, x,	y, x, config->significantVolume);
		}
	}
	
	DrawColorScale(&plot, GetTypeName(config, type), MatchColor(GetTypeColor(config, type)), PLOT_RES_X/50, PLOT_RES_Y/15, PLOT_RES_X/80, PLOT_RES_Y/1.15, config->significantVolume, 3, config);
	DrawLabelsMDF(&plot);
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
	char	 name[2048];
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
	char	 name[2048];
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
	if(strcmp(colorcopy, "gray") == 0 || strcmp(color, "white") == 0)
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
	SortFlatAmplitudeDifferencesByRefAmplitude(ADiff, config->Differences.cntAmplAudioDiff);
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
	SortFlatMissingDifferencesByAmplitude(FDiff, config->Differences.cntFreqAudioDiff);
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
			for(int k = currentsize-1; k > j; k--)
				Freqs[k] = Freqs[k - 1];
	
			Freqs[j] = Element;
			return 1;
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

	if(!size || !Signal || !config)
		return NULL;

	*size = 0;

	for(block = 0; block < config->types.totalChunks; block++)
		for(i = 0; i < config->MaxFreq; i++)
		{
			if(Signal->Blocks[block].freq[i].hertz && Signal->Blocks[block].freq[i].amplitude > config->significantVolume)
				count ++;
			else
				break;
		}

	Freqs = (FlatFrequency*)malloc(sizeof(FlatFrequency)*count);
	if(!Freqs)
		return NULL;

	memset(Freqs, 0, sizeof(FlatFrequency)*count);
	for(block = 0; block < config->types.totalChunks; block++)
	{
		int type = 0, color = 0;

		type = GetBlockType(config, block);
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
	
	SortFlatFrequenciesByAmplitude(Freqs, count);

	*size = counter;
	return(Freqs);
}


void PlotTest(char *filename, parameters *config)
{
	PlotFile	plot;
	char		name[2048];
	double		dBFS = DB_HEIGHT;

	if(!config)
		return;

	sprintf(name, "Test_%s", filename);
	FillPlot(&plot, name, PLOT_RES_X, PLOT_RES_Y, 0, -1*dBFS, TOP_FREQUENCY, dBFS, 1, config);

	if(!CreatePlotFile(&plot))
		return;

	DrawGridZeroDBCentered(&plot, dBFS, 3, TOP_FREQUENCY, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dBFS, 3, TOP_FREQUENCY, 1000, config);
	DrawLabelsMDF(&plot);
	DrawColorScale(&plot, "Test", COLOR_ORANGE, PLOT_RES_X/50, PLOT_RES_Y/15, PLOT_RES_X/80, PLOT_RES_Y/1.15, -60, 3, config);
	

	ClosePlot(&plot);
}

void PlotTestZL(char *filename, parameters *config)
{
	PlotFile	plot;
	char		name[2048];

	if(!config)
		return;

	sprintf(name, "Test_ZL_%s", filename);
	FillPlot(&plot, name, PLOT_RES_X, PLOT_RES_Y, 0, config->significantVolume, TOP_FREQUENCY, 0.0, 1, config);

	if(!CreatePlotFile(&plot))
		return;

	DrawGridZeroToLimit(&plot, config->significantVolume, 3, TOP_FREQUENCY, 1000, config);
	DrawLabelsZeroToLimit(&plot, config->significantVolume, 3, TOP_FREQUENCY, 1000, config);

	DrawColorScale(&plot, "Test", COLOR_ORANGE, PLOT_RES_X/50, PLOT_RES_Y/15, PLOT_RES_X/80, PLOT_RES_Y/1.15, -60, 3, config);
	
	DrawLabelsMDF(&plot);
	ClosePlot(&plot);
}


inline double transformtoLog(double coord, double top, parameters *config)
{
	if(config->logScale)
		return(top*log10(coord)/log10(top));
	else
		return(coord);
}

/* =============Best Fit ================ */

void BestFit_SortFlatAmplitudeDifferencesByFrequency(FlatAmplDifference *ADiff, long int size)
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

AveragedFrequencies *BestFit_CreateFlatDifferencesBestFit(int matchType, long int *avgSize, int chunks, parameters *config)
{
	long int			count = 0, size = 0, interval = 0;
	FlatAmplDifference	*ADiff = NULL;
	AveragedFrequencies	*averaged = NULL;

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

	interval = ceil((double)count/(double)chunks);
	ADiff = (FlatAmplDifference*)malloc(sizeof(FlatAmplDifference)*count);
	if(!ADiff)
		return NULL;

	size = ceil((double)count/(double)interval);
	averaged = (AveragedFrequencies*)malloc(sizeof(AveragedFrequencies)*size);
	if(!averaged)
	{
		free(ADiff);
		return NULL;
	}

	memset(ADiff, 0, sizeof(FlatAmplDifference)*count);
	memset(averaged, 0, sizeof(AveragedFrequencies)*size);
	count = 0;

	for(int b = 0; b < config->types.totalChunks; b++)
	{
		int type = 0;
		
		type = GetBlockType(config, b);
		if(type == matchType)
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

	BestFit_SortFlatAmplitudeDifferencesByFrequency(ADiff, count);

	OutputFileOnlyStart();
	logmsg("===================\n");
	logmsg("Averaged Size %ld Difference Size %ld INterval %d\n", size, count, interval);

	for(long int a = 0; a < size; a++)
	{
		long int 	start, end, elements = 0;

		start = a*interval;
		end = start+interval;

		for(long int c = start; c < end; c++)
		{
			if(c == count)
				break;

			averaged[a].avgfreq += ADiff[c].hertz;
			averaged[a].avgvol += ADiff[c].diffAmplitude;
			elements++;
		}
		averaged[a].avgfreq /= elements;
		averaged[a].avgvol /= elements;

		logmsg("averaged[%ld]: %ghz %gdbfs (elems:%ld)\n", a, averaged[a].avgfreq, averaged[a].avgvol, elements);
	}

	OutputFileOnlyEnd();

	free(ADiff);
	*avgSize = size;

	return(averaged);
}

void PlotSingleTypeDifferentAmplitudesBestFit(FlatAmplDifference *amplDiff, int type, char *filename, AveragedFrequencies *averaged, long int avgsize, parameters *config)
{
	PlotFile	plot;
	double		dbs = DB_HEIGHT;
	int			color = 0;

	if(!config)
		return;

	if(!amplDiff)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	FillPlot(&plot, filename, PLOT_RES_X, PLOT_RES_Y, 0, -1*dbs, TOP_FREQUENCY, dbs, 1, config);

	if(!CreatePlotFile(&plot))
		return;

	DrawGridZeroDBCentered(&plot, dbs, 3, TOP_FREQUENCY, 1000, config);
	DrawLabelsZeroDBCentered(&plot, dbs, 3, TOP_FREQUENCY, 1000, config);

	for(long int a = 0; a < config->Differences.cntAmplAudioDiff; a++)
	{
		if(amplDiff[a].type == type)
		{ 
			long int intensity;

			intensity = CalculateWeightedError((fabs(config->significantVolume) - fabs(amplDiff[a].refAmplitude))/fabs(config->significantVolume), config)*0xffff;

			SetPenColor(amplDiff[a].color, intensity, &plot);
			pl_fpoint_r(plot.plotter, transformtoLog(amplDiff[a].hertz, TOP_FREQUENCY, config), amplDiff[a].diffAmplitude);
		}
	}

	color = MatchColor(GetTypeColor(config, type));
	pl_endpath_r(plot.plotter);

	if(averaged && avgsize > 1)
	{
		SetPenColor(color, 0xffff, &plot);
		for(long int a = 0; a < avgsize; a+=2)
		{
			if(a + 2 < avgsize)
			{
				pl_fbezier2_r(plot.plotter,
					transformtoLog(averaged[a].avgfreq, 20000, config), averaged[a].avgvol,
					transformtoLog(averaged[a+1].avgfreq, 20000, config), averaged[a+1].avgvol,
					transformtoLog(averaged[a+2].avgfreq, 20000, config), averaged[a+2].avgvol);
				logmsg("Plot [%ld] %g->%g\n", a, averaged[a].avgfreq, averaged[a].avgvol);
			}
		}
		pl_endpath_r(plot.plotter);

		/*
		SetPenColor(color, 0xffff, &plot);
		for(long int a = 0; a < avgsize; a++)
		{
			if(a == 0)
				pl_fline_r(plot.plotter, transformtoLog(averaged[a].avgfreq, 20000, config), averaged[a].avgvol,
							transformtoLog(averaged[a+1].avgfreq, 20000, config), averaged[a+1].avgvol);
			else
				pl_fcont_r(plot.plotter, transformtoLog(averaged[a].avgfreq, 20000, config), averaged[a].avgvol);
		}
		pl_endpath_r(plot.plotter);
		*/
	}

	DrawColorScale(&plot, GetTypeName(config, type), MatchColor(GetTypeColor(config, type)), PLOT_RES_X/50, PLOT_RES_Y/15, PLOT_RES_X/80, PLOT_RES_Y/1.15, config->significantVolume, 3, config);
	DrawLabelsMDF(&plot);
	ClosePlot(&plot);
}