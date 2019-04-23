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
		logmsg("Couldn't create file\n");
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

	if (pl_openpl_r(plot->plotter) < 0)
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
	if (pl_closepl_r(plot->plotter) < 0)
	{
		logmsg("Couldn't close Plotter\n");
		return 0;
	}
	
	if (pl_deletepl_r(plot->plotter) < 0)
	{
		logmsg("Couldn't delete Plotter\n");
		return 0;
	}

	fclose(plot->file);
	return 1;
}

void PlotDifferentBlockAmplitudes(int block, parameters *config)
{
	PlotFile plot;
	char	 name[1024];

	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	sprintf(name, "Block%3d", block);
	FillPlot(&plot, name, 1000, 500, 0, -10.0, 20000, 10.0, 0.1, config);

	CreatePlotFile(&plot);
	for(int a = 0; a < config->Differences.BlockDiffArray[block].cntAmplBlkDiff; a++)
	{
		pl_fpoint_r(plot.plotter, config->Differences.BlockDiffArray[block].amplDiffArray[a].hertz, 
				config->Differences.BlockDiffArray[block].amplDiffArray[a].diffAmplitude);
	}
	ClosePlot(&plot);
}

void PlotAllDifferentAmplitudes(char *filename, parameters *config)
{
	PlotFile plot;
	char	 name[2048];

	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	sprintf(name, "DifferentAmplitudes_%s", filename);
	FillPlot(&plot, name, 2000, 1000, 0, -12.0, 20000, 12.0, 0.1, config);

	CreatePlotFile(&plot);

	pl_pencolor_r (plot.plotter, 0, 0xcccc, 0);
	pl_fline_r(plot.plotter, 0, 0, 20000, 0);

	pl_pencolor_r (plot.plotter, 0, 0x5555, 0);
	for(int i = 3; i < 12; i += 3)
	{
		pl_fline_r(plot.plotter, 0, i, 19500, i);
		pl_fline_r(plot.plotter, 0, -1*i, 19500, -1*i);
	}

	pl_pencolor_r (plot.plotter, 0, 0x5555, 0);
	for(int i = 0; i < 20000; i += 1000)
		pl_fline_r(plot.plotter, i, -12, i, 12);

	pl_pencolor_r (plot.plotter, 0, 0xFFFF, 0);
	for(int b = 0; b < config->types.totalChunks; b++)
	{
		int type;

		type = GetBlockType(config, b);
		if(type > TYPE_SILENCE)
		{ 
			pl_pencolorname_r(plot.plotter, GetBlockColor(config, b));
			for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
			{
				pl_fpoint_r(plot.plotter, config->Differences.BlockDiffArray[b].amplDiffArray[a].hertz, 
						config->Differences.BlockDiffArray[b].amplDiffArray[a].diffAmplitude);
			}
		}
	}
	ClosePlot(&plot);
}

void PlotAllMissingFrequencies(char *filename, parameters *config)
{
	PlotFile plot;
	char	 name[2048];

	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	sprintf(name, "MissingFrequencies_%s", filename);
	FillPlot(&plot, name, 2000, 1000, 0, config->significantVolume, 20000, 0.0, 0.1, config);

	CreatePlotFile(&plot);

	pl_pencolor_r (plot.plotter, 0, 0xaaaa, 0);
	pl_fline_r(plot.plotter, 0, 0, 20000, 0);

	pl_pencolor_r (plot.plotter, 0, 0x7777, 0);
	for(int i = 3; i < fabs(config->significantVolume); i += 3)
		pl_fline_r(plot.plotter, 0, -1*i, 20000, -1*i);

	pl_pencolor_r (plot.plotter, 0, 0x7777, 0);
	for(int i = 0; i < 20000; i += 1000)
		pl_fline_r(plot.plotter, i, config->significantVolume, i, 0);

	pl_pencolor_r (plot.plotter, 0, 0xFFFF, 0);
	for(int b = 0; b < config->types.totalChunks; b++)
	{
		int type;

		type = GetBlockType(config, b);
		if(type > TYPE_SILENCE)
		{ 
			pl_pencolorname_r(plot.plotter, GetBlockColor(config, b));
			for(int f = 0; f < config->Differences.BlockDiffArray[b].cntFreqBlkDiff; f++)
			{
				pl_fpoint_r(plot.plotter, config->Differences.BlockDiffArray[b].freqMissArray[f].hertz, 
						config->Differences.BlockDiffArray[b].freqMissArray[f].amplitude);
			}
		}
	}
	ClosePlot(&plot);
}

void PlotSpectrogram(char *filename, AudioSignal *Signal, int block, parameters *config)
{
	PlotFile plot;
	char	 name[2048];

	if(!config)
		return;

	SortFrequencies(Signal, config);

	sprintf(name, "Spectrogram_%s_block%3d", filename, block);
	FillPlot(&plot, name, 2000, 1000, 0, config->significantVolume, 20000, 0.0, 0.1, config);

	CreatePlotFile(&plot);

	pl_pencolor_r (plot.plotter, 0, 0xbbbb, 0);
	pl_fline_r(plot.plotter, 0, 0, 20000, 0);

	pl_pencolor_r (plot.plotter, 0, 0x5555, 0);
	for(int i = 3; i < fabs(config->significantVolume); i += 3)
		pl_fline_r(plot.plotter, 0, -1*i, 20000, -1*i);

	pl_pencolor_r (plot.plotter, 0, 0x5555, 0);
	for(int i = 0; i < 20000; i += 1000)
		pl_fline_r(plot.plotter, i, config->significantVolume, i, 0);

	pl_pencolor_r (plot.plotter, 0, 0xFFFF, 0);
	
	pl_fline_r(plot.plotter, 0, -1*config->significantVolume, 1, -1*config->significantVolume);
	for(int i = 0; i < config->MaxFreq; i++)
	{
		pl_fcont_r(plot.plotter, Signal->Blocks[block].freq[i].hertz, 
				Signal->Blocks[block].freq[i].amplitude);
	}
	ClosePlot(&plot);
}

void PlotAllSpectrogram(char *filename, AudioSignal *Signal, parameters *config)
{
	PlotFile plot;
	char	 name[2048];

	if(!config)
		return;

	sprintf(name, "Spectrogram_%s", filename);
	FillPlot(&plot, name, 2000, 1000, 0, config->significantVolume, 20000, 0.0, 0.1, config);

	CreatePlotFile(&plot);

	pl_pencolor_r (plot.plotter, 0, 0xbbbb, 0);
	pl_fline_r(plot.plotter, 0, 0, 20000, 0);

	pl_pencolor_r (plot.plotter, 0, 0x5555, 0);
	for(int i = 3; i < fabs(config->significantVolume); i += 3)
		pl_fline_r(plot.plotter, 0, -1*i, 20000, -1*i);

	pl_pencolor_r (plot.plotter, 0, 0x5555, 0);
	for(int i = 0; i < 20000; i += 1000)
		pl_fline_r(plot.plotter, i, config->significantVolume, i, 0);

	pl_pencolor_r (plot.plotter, 0, 0xFFFF, 0);
	
	for(int b = 0; b < config->types.totalChunks; b++)
	{
		int type;

		type = GetBlockType(config, b);
		if(type > TYPE_SILENCE)
		{ 
			pl_pencolorname_r(plot.plotter, GetBlockColor(config, b));
			for(int i = 0; i < config->MaxFreq; i++)
			{
				pl_fpoint_r(plot.plotter, Signal->Blocks[b].freq[i].hertz, 
						Signal->Blocks[b].freq[i].amplitude);
			}
		}
	}
	ClosePlot(&plot);
}

void PlotWindow(windowManager *wm, parameters *config)
{
	PlotFile plot;
	char	 name[2048];
	float 	 *window = NULL;
	long int size;

	if(!config || !wm || !wm->windowArray)
		return;

	window = getWindowByLength(wm, 1.0);
	if(!window)
		return;

	size = getWindowSizeByLength(wm, 1.0);

	sprintf(name, "WindowPlot_%s", GetWindow(config->window));
	FillPlot(&plot, name, 512, 544, 0, -0.1, 1, 1.1, 0.01, config);

	CreatePlotFile(&plot);

	pl_pencolor_r (plot.plotter, 0, 0x5555, 0);
	pl_fline_r(plot.plotter, 0, 1, 1, 1);
	pl_fline_r(plot.plotter, 0, 0, 1, 0);

	pl_pencolor_r (plot.plotter, 0, 0xFFFF, 0);	
	for(int i = 0; i < size; i++)
		pl_fpoint_r(plot.plotter, (double)i/(double)size, window[i]);
	
	ClosePlot(&plot);
}