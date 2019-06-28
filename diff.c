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

#include "diff.h"
#include "log.h"
#include "freq.h"

AmplDifference *CreateAmplDifferences(parameters *config)
{
	AmplDifference *ad = NULL;

	ad = (AmplDifference*)malloc(sizeof(AmplDifference)*config->MaxFreq);
	if(!ad)
	{
		logmsg("Insufficient memory for AmplDifference\n");
		return 0;
	}
	memset(ad, 0, sizeof(AmplDifference)*config->MaxFreq);
	return ad;
}

FreqDifference *CreateFreqDifferences(parameters *config)
{
	FreqDifference *fd = NULL;

	fd = (FreqDifference*)malloc(sizeof(FreqDifference)*config->MaxFreq);
	if(!fd)
	{
		logmsg("Insufficient memory for FreqDifference\n");
		return 0;
	}
	memset(fd, 0, sizeof(FreqDifference)*config->MaxFreq);
	return fd;
}

int CreateDifferenceArray(parameters *config)
{
	BlockDifference *BlockDiffArray = NULL;

	if(!config)
		return 0;

	BlockDiffArray = (BlockDifference*)malloc(sizeof(BlockDifference)*config->types.totalChunks);
	if(!BlockDiffArray)
	{
		logmsg("Insufficient memory for AudioDiffArray\n");
		return 0;
	}

	memset(BlockDiffArray, 0, sizeof(BlockDifference)*config->types.totalChunks);

	for(int i = 0; i < config->types.totalChunks; i++)
	{
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, i);
		if(type > TYPE_SILENCE)
		{
			BlockDiffArray[i].freqMissArray = CreateFreqDifferences(config);
			if(!BlockDiffArray[i].freqMissArray)
			{
				free(BlockDiffArray);
				return 0;
			}
	
			BlockDiffArray[i].amplDiffArray = CreateAmplDifferences(config);
			if(!BlockDiffArray[i].amplDiffArray)
			{
				free(BlockDiffArray);
				return 0;
			}
		}
		else
		{
			BlockDiffArray[i].freqMissArray = NULL;
			BlockDiffArray[i].amplDiffArray = NULL;
		}

		BlockDiffArray[i].type = type;
		BlockDiffArray[i].cntFreqBlkDiff = 0;
		BlockDiffArray[i].cmpFreqBlkDiff = 0;
		BlockDiffArray[i].weightedFreqBlkDiff = 0;
		BlockDiffArray[i].cntAmplBlkDiff = 0;
		BlockDiffArray[i].cmpAmplBlkDiff = 0;
		BlockDiffArray[i].weightedAmplBlkDiff = 0;
	}
	
	config->Differences.BlockDiffArray = BlockDiffArray;
	config->Differences.cntFreqAudioDiff = 0;
	config->Differences.cntAmplAudioDiff = 0;
	config->Differences.weightedFreqAudio = 0;
	config->Differences.weightedAmplAudio = 0;
	config->Differences.cntTotalCompared = 0;
	config->Differences.cntTotalAudioDiff = 0;
	config->Differences.weightedAudioDiff = 0;
	return 1;
}

void ReleaseDifferenceArray(parameters *config)
{
	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	for(int i = 0; i < config->types.totalChunks; i++)
	{
		if(config->Differences.BlockDiffArray[i].amplDiffArray)
		{
			free(config->Differences.BlockDiffArray[i].amplDiffArray);
			config->Differences.BlockDiffArray[i].amplDiffArray = NULL;
		}

		if(config->Differences.BlockDiffArray[i].freqMissArray)
		{
			free(config->Differences.BlockDiffArray[i].freqMissArray);
			config->Differences.BlockDiffArray[i].freqMissArray = NULL;
		}
	}

	free(config->Differences.BlockDiffArray);
	config->Differences.BlockDiffArray = NULL;
	config->Differences.cntFreqAudioDiff = 0;
	config->Differences.cntAmplAudioDiff = 0;
	config->Differences.weightedFreqAudio = 0;
	config->Differences.weightedAmplAudio = 0;
	config->Differences.cntTotalCompared = 0;
	config->Differences.cntTotalAudioDiff = 0;
	config->Differences.weightedAudioDiff = 0;
}

int IncrementCmpAmplDifference(int block, parameters *config)
{
	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	if(block > config->types.totalChunks)
		return 0;

	config->Differences.BlockDiffArray[block].cmpAmplBlkDiff ++;
	return 1;
}

int InsertAmplDifference(int block, double freq, double refAmplitude, double compAmplitude, double weighted, parameters *config)
{
	int position = 0;
	double diff = 0;

	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	if(block > config->types.totalChunks)
		return 0;
	
	diff = fabs(refAmplitude) - fabs(compAmplitude);
	position = config->Differences.BlockDiffArray[block].cntAmplBlkDiff;
	config->Differences.BlockDiffArray[block].amplDiffArray[position].hertz = freq;
	config->Differences.BlockDiffArray[block].amplDiffArray[position].refAmplitude = refAmplitude;
	config->Differences.BlockDiffArray[block].amplDiffArray[position].diffAmplitude = diff;
	config->Differences.BlockDiffArray[block].amplDiffArray[position].weight = weighted;

	config->Differences.BlockDiffArray[block].cntAmplBlkDiff ++;
	config->Differences.cntAmplAudioDiff ++;
	config->Differences.cntTotalAudioDiff ++;

	config->Differences.BlockDiffArray[block].weightedAmplBlkDiff += weighted;
	config->Differences.weightedAmplAudio += weighted;
	config->Differences.weightedAudioDiff += weighted;
	return 1;
}


int IncrementCompared(int block, parameters *config)
{
	if(!config)
		return 0;

	config->Differences.cntTotalCompared ++;
	if(!IncrementCmpAmplDifference(block, config))
		return 0;
	if(!IncrementCmpFreqNotFound(block, config))
		return 0;
	return 1;
}


int IncrementCmpFreqNotFound(int block, parameters *config)
{
	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	if(block > config->types.totalChunks)
		return 0;
	
	config->Differences.BlockDiffArray[block].cmpFreqBlkDiff ++;
	return 1;
}

int InsertFreqNotFound(int block, double freq, double amplitude, double weighted, parameters *config)
{
	int position = 0;

	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	if(block > config->types.totalChunks)
		return 0;

	position = config->Differences.BlockDiffArray[block].cntFreqBlkDiff;
	config->Differences.BlockDiffArray[block].freqMissArray[position].hertz = freq;
	config->Differences.BlockDiffArray[block].freqMissArray[position].amplitude = amplitude;
	config->Differences.BlockDiffArray[block].freqMissArray[position].weight = weighted;

	config->Differences.BlockDiffArray[block].cntFreqBlkDiff ++;
	config->Differences.cntFreqAudioDiff ++;
	config->Differences.cntTotalAudioDiff ++;

	config->Differences.BlockDiffArray[block].weightedFreqBlkDiff += weighted;
	config->Differences.weightedFreqAudio += weighted;
	config->Differences.weightedAudioDiff += weighted;
	return 1;
}

void PrintDifferentFrequencies(int block, parameters *config)
{
	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	OutputFileOnlyStart();

	if(config->Differences.BlockDiffArray[block].cntFreqBlkDiff)
		logmsg("Frequencies not found:\n");

	for(int f = 0; f < config->Differences.BlockDiffArray[block].cntFreqBlkDiff; f++)
	{
		logmsg("Frequency: %7g Hz\tAmplituide: %4.2f (%g)\n", 
			config->Differences.BlockDiffArray[block].freqMissArray[f].hertz,
			config->Differences.BlockDiffArray[block].freqMissArray[f].amplitude,
			config->Differences.BlockDiffArray[block].freqMissArray[f].weight);
	}

	OutputFileOnlyEnd();
}

void PrintDifferentAmplitudes(int block, parameters *config)
{
	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	OutputFileOnlyStart();

	if(config->Differences.BlockDiffArray[block].cntFreqBlkDiff)
		logmsg("\nDifferent Amplitudes:\n");

	for(int a = 0; a < config->Differences.BlockDiffArray[block].cntAmplBlkDiff; a++)
	{
		logmsg("Frequency: %7g Hz\tAmplitude: %4.2f dBFS\tAmplitude Difference: %4.2f dBFS (%g)\n",
			config->Differences.BlockDiffArray[block].amplDiffArray[a].hertz,
			config->Differences.BlockDiffArray[block].amplDiffArray[a].refAmplitude,
			config->Differences.BlockDiffArray[block].amplDiffArray[a].diffAmplitude,
			config->Differences.BlockDiffArray[block].amplDiffArray[a].weight);
	}

	OutputFileOnlyEnd();
}

void PrintDifferenceArray(parameters *config)
{
	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	logmsg("\nTotal Differences: %ld, Weighted: %g from %ld\nNonWeighted: %g%% Weighted: %g%%\n", 
				config->Differences.cntTotalAudioDiff,
				config->Differences.weightedAudioDiff,
				config->Differences.cntTotalCompared,
				(double)config->Differences.cntTotalAudioDiff*100.0/(double)config->Differences.cntTotalCompared,
				(double)config->Differences.weightedAudioDiff*100.0/(double)config->Differences.cntTotalCompared);
	logmsg("Total Frequencies not Found %ld Total Amplitunes not matched: %ld\n",
				config->Differences.cntFreqAudioDiff,
				config->Differences.cntAmplAudioDiff);
	logmsg("Total Weighted Frequencies not Found %g Total Amplitunes not matched: %g\n",
				config->Differences.weightedFreqAudio,
				config->Differences.weightedAmplAudio);

	for(int b = 0; b < config->types.totalChunks; b++)
	{
		if(GetBlockType(config, b) <= TYPE_CONTROL)
			continue;

		OutputFileOnlyStart();

		if(config->Differences.BlockDiffArray[b].cntAmplBlkDiff)
			logmsg("\n\nBlock: %s# %d (%d) Not Found: %d Differences: %d FW: %g AW: %g\n", 
					GetBlockName(config, b), GetBlockSubIndex(config, b), b,
					config->Differences.BlockDiffArray[b].cntFreqBlkDiff,
					config->Differences.BlockDiffArray[b].cntAmplBlkDiff,
					config->Differences.BlockDiffArray[b].weightedFreqBlkDiff,
					config->Differences.BlockDiffArray[b].weightedAmplBlkDiff);

		OutputFileOnlyEnd();

		PrintDifferentFrequencies(b, config);
		PrintDifferentAmplitudes(b, config);
	}
}