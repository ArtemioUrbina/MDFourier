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

#include "diff.h"
#include "log.h"
#include "freq.h"

#define STEREO_DIFF_SIZE	2*config->MaxFreq
#define MONO_DIFF_SIZE		config->MaxFreq

long int GetSilenceBlockSize(int block, parameters *config)
{
	long int s1, s2, size = 0;

	s1 = config->referenceSignal->Blocks[block].SilenceSizeLeft + config->referenceSignal->Blocks[block].SilenceSizeRight;
	s2 = config->comparisonSignal->Blocks[block].SilenceSizeLeft + config->comparisonSignal->Blocks[block].SilenceSizeRight;
	if(s1 > s2)
		size = s1;
	else
		size = s2;
	return size;
}

AmplDifference *CreateAmplDifferences(int block, parameters *config)
{
	AmplDifference	*ad = NULL;
	long int		size = 0;

	if(GetBlockType(config, block) == TYPE_SILENCE)
		size = GetSilenceBlockSize(block, config);
	else
	{
		if(GetBlockChannel(config, block) == CHANNEL_STEREO)
			size = STEREO_DIFF_SIZE;
		else
			size = MONO_DIFF_SIZE;
	}
	ad = (AmplDifference*)malloc(sizeof(AmplDifference)*size);
	if(!ad)
	{
		logmsg("Insufficient memory for AmplDifference (%ld bytes)\n", sizeof(AmplDifference)*size);
		return 0;
	}
	memset(ad, 0, sizeof(AmplDifference)*size);
	return ad;
}

FreqDifference *CreateFreqDifferences(int block, parameters *config)
{
	FreqDifference *fd = NULL;
	long int		size = 0;

	if(GetBlockType(config, block) == TYPE_SILENCE)
		size = GetSilenceBlockSize(block, config);
	else
	{
		if(GetBlockChannel(config, block) == CHANNEL_STEREO)
			size = STEREO_DIFF_SIZE;
		else
			size = MONO_DIFF_SIZE;
	}
	fd = (FreqDifference*)malloc(sizeof(FreqDifference)*size);
	if(!fd)
	{
		logmsg("Insufficient memory for FreqDifference (%ld bytes)\n", sizeof(sizeof(FreqDifference)*size));
		return 0;
	}
	memset(fd, 0, sizeof(FreqDifference)*size);
	return fd;
}

PhaseDifference *CreatePhaseDifferences(int block, parameters *config)
{
	PhaseDifference *pd = NULL;
	long int		size = 0;

	if(GetBlockType(config, block) == TYPE_SILENCE)
		size = GetSilenceBlockSize(block, config);
	else
	{
		if(GetBlockChannel(config, block) == CHANNEL_STEREO)
			size = STEREO_DIFF_SIZE;
		else
			size = MONO_DIFF_SIZE;
	}
	pd = (PhaseDifference*)malloc(sizeof(PhaseDifference)*size);
	if(!pd)
	{
		logmsg("Insufficient memory for FreqDifference (%ld bytes)\n", sizeof(sizeof(PhaseDifference)*size));
		return 0;
	}
	memset(pd, 0, sizeof(PhaseDifference)*size);
	return pd;
}

int CreateDifferenceArray(parameters *config)
{
	BlockDifference *BlockDiffArray = NULL;

	if(!config)
		return 0;

	BlockDiffArray = (BlockDifference*)malloc(sizeof(BlockDifference)*config->types.totalBlocks);
	if(!BlockDiffArray)
	{
		logmsg("Insufficient memory for AudioDiffArray(%ld bytes)\n", sizeof(sizeof(BlockDifference)*config->types.totalBlocks));
		return 0;
	}

	memset(BlockDiffArray, 0, sizeof(BlockDifference)*config->types.totalBlocks);

	for(int i = 0; i < config->types.totalBlocks; i++)
	{
		int type = TYPE_NOTYPE;

		type = GetBlockType(config, i);
		if(type >= TYPE_SILENCE)
		{
			BlockDiffArray[i].freqMissArray = CreateFreqDifferences(i, config);
			if(!BlockDiffArray[i].freqMissArray)
			{
				free(BlockDiffArray);
				return 0;
			}
	
			BlockDiffArray[i].amplDiffArray = CreateAmplDifferences(i, config);
			if(!BlockDiffArray[i].amplDiffArray)
			{
				free(BlockDiffArray);
				return 0;
			}

			BlockDiffArray[i].phaseDiffArray = CreatePhaseDifferences(i, config);
			if(!BlockDiffArray[i].phaseDiffArray)
			{
				free(BlockDiffArray);
				return 0;
			}
		}
		else
		{
			BlockDiffArray[i].freqMissArray = NULL;
			BlockDiffArray[i].amplDiffArray = NULL;
			BlockDiffArray[i].phaseDiffArray = NULL;
		}

		BlockDiffArray[i].type = type;

		BlockDiffArray[i].cntFreqBlkDiff = 0;
		BlockDiffArray[i].cmpFreqBlkDiff = 0;

		BlockDiffArray[i].cntAmplBlkDiff = 0;
		BlockDiffArray[i].cmpAmplBlkDiff = 0;
		BlockDiffArray[i].perfectAmplMatch = 0;

		// Left
		BlockDiffArray[i].cntAmplBlkDiffLeft = 0;
		BlockDiffArray[i].cmpAmplBlkDiffLeft = 0;
		BlockDiffArray[i].perfectAmplMatchLeft = 0;

		// Right
		BlockDiffArray[i].cntAmplBlkDiffRight = 0;
		BlockDiffArray[i].cmpAmplBlkDiffRight = 0;
		BlockDiffArray[i].perfectAmplMatchRight = 0;

		BlockDiffArray[i].cntPhaseBlkDiff = 0;
		BlockDiffArray[i].cmpPhaseBlkDiff = 0;

		BlockDiffArray[i].channel = GetBlockChannel(config, i);
	}
	
	config->Differences.BlockDiffArray = BlockDiffArray;

	config->Differences.cntFreqAudioDiff = 0;
	config->Differences.cntAmplAudioDiff = 0;
	config->Differences.cntPhaseAudioDiff = 0;
	config->Differences.cmpPhaseAudioDiff = 0;

	config->Differences.cntPerfectAmplMatch = 0;
	config->Differences.cntTotalCompared = 0;
	config->Differences.cntTotalAudioDiff = 0;

	// Left
	config->Differences.cntFreqAudioDiffLeft = 0;
	config->Differences.cntAmplAudioDiffLeft = 0;
	config->Differences.cntPhaseAudioDiffLeft = 0;
	config->Differences.cmpPhaseAudioDiffLeft = 0;

	config->Differences.cntPerfectAmplMatchLeft = 0;
	config->Differences.cntTotalComparedLeft = 0;
	config->Differences.cntTotalAudioDiffLeft = 0;

	// Right
	config->Differences.cntFreqAudioDiffRight = 0;
	config->Differences.cntAmplAudioDiffRight = 0;
	config->Differences.cntPhaseAudioDiffRight = 0;
	config->Differences.cmpPhaseAudioDiffRight = 0;

	config->Differences.cntPerfectAmplMatchRight = 0;
	config->Differences.cntTotalComparedRight = 0;
	config->Differences.cntTotalAudioDiffRight = 0;
	return 1;
}

void ReleaseDifferenceArray(parameters *config)
{
	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	for(int i = 0; i < config->types.totalBlocks; i++)
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

		if(config->Differences.BlockDiffArray[i].phaseDiffArray)
		{
			free(config->Differences.BlockDiffArray[i].phaseDiffArray);
			config->Differences.BlockDiffArray[i].phaseDiffArray = NULL;
		}
	}

	free(config->Differences.BlockDiffArray);
	config->Differences.BlockDiffArray = NULL;

	config->Differences.cntFreqAudioDiff = 0;
	config->Differences.cntAmplAudioDiff = 0;
	config->Differences.cntPhaseAudioDiff = 0;

	config->Differences.cntTotalCompared = 0;
	config->Differences.cntTotalAudioDiff = 0;
}

int IncrementCmpAmpl(int block, char channel, parameters *config)
{
	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	if(block > config->types.totalBlocks)
		return 0;

	config->Differences.BlockDiffArray[block].cmpAmplBlkDiff ++;

	if(channel == CHANNEL_LEFT || channel == CHANNEL_MONO)
		config->Differences.BlockDiffArray[block].cmpAmplBlkDiffLeft ++;

	if(channel == CHANNEL_RIGHT)
		config->Differences.BlockDiffArray[block].cmpAmplBlkDiffRight ++;
	return 1;
}

int IncrementCmpPhase(int block, parameters *config)
{
	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	if(block > config->types.totalBlocks)
		return 0;

	config->Differences.BlockDiffArray[block].cmpPhaseBlkDiff ++;
	config->Differences.cmpPhaseAudioDiff ++;
	return 1;
}

int InsertAmplDifference(int block, Frequency ref, Frequency comp, char channel, parameters *config)
{
	long int	position = 0;
	double		diffAmpl = 0;

	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	if(!config->Differences.BlockDiffArray[block].amplDiffArray)
		return 0;

	if(block > config->types.totalBlocks)
		return 0;

	if(ref.amplitude == NO_AMPLITUDE || comp.amplitude == NO_AMPLITUDE)
	{
		logmsg("WARNING: Received NO_AMPLITUDE for Amplitude difference\n");
		return 0;
	}

	diffAmpl = fabs(ref.amplitude) - fabs(comp.amplitude);
	position = config->Differences.BlockDiffArray[block].cntAmplBlkDiff;

	if(GetBlockType(config, block) != TYPE_SILENCE)
	{
		if(GetBlockChannel(config, block) == CHANNEL_STEREO)
		{
			if(position >= STEREO_DIFF_SIZE)
				return 0;
		}
		else
		{
			if(position >= MONO_DIFF_SIZE)
				return 0;
		}
	}
	else
	{
		if(position >= GetSilenceBlockSize(block, config))
			return 0;
	}

	config->Differences.BlockDiffArray[block].amplDiffArray[position].hertz = ref.hertz;
	config->Differences.BlockDiffArray[block].amplDiffArray[position].refAmplitude = ref.amplitude;
	config->Differences.BlockDiffArray[block].amplDiffArray[position].diffAmplitude = diffAmpl;
	config->Differences.BlockDiffArray[block].amplDiffArray[position].channel = channel;

	config->Differences.BlockDiffArray[block].cntAmplBlkDiff ++;
	config->Differences.cntAmplAudioDiff ++;
	config->Differences.cntTotalAudioDiff ++;

	if(channel == CHANNEL_LEFT || channel == CHANNEL_MONO)
	{
		config->Differences.BlockDiffArray[block].cntAmplBlkDiffLeft ++;
		config->Differences.cntAmplAudioDiffLeft ++;
		config->Differences.cntTotalAudioDiffLeft ++;
	}

	if(channel == CHANNEL_RIGHT)
	{
		config->Differences.BlockDiffArray[block].cntAmplBlkDiffRight ++;
		config->Differences.cntAmplAudioDiffRight ++;
		config->Differences.cntTotalAudioDiffRight ++;
	}
	
	return 1;
}

int InsertPhaseDifference(int block, Frequency ref, Frequency comp, char channel, parameters *config)
{
	int		position = 0;
	double	diffPhase = 0;

	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	if(!config->Differences.BlockDiffArray[block].phaseDiffArray)
		return 0;

	if(block > config->types.totalBlocks)
		return 0;

	if(ref.amplitude == NO_AMPLITUDE || comp.amplitude == NO_AMPLITUDE)
	{
		logmsg("WARNING: Received NO_AMPLITUDE for Phase difference\n");
		return 0;
	}
	
	diffPhase = comp.phase - ref.phase;
	//diffPhase = roundFloat(diffPhase);
	if(diffPhase > 180.0)
		diffPhase -= 360;
	if(diffPhase <= -180.0)  // -180 is the same as 180, hence the =
		diffPhase += 360;
	if(diffPhase == 0) diffPhase = 0; // remove -0.0 for plots.

	position = config->Differences.BlockDiffArray[block].cntPhaseBlkDiff;

	if(GetBlockType(config, block) != TYPE_SILENCE)
	{
		if(GetBlockChannel(config, block) == CHANNEL_STEREO)
		{
			if(position >= STEREO_DIFF_SIZE)
				return 0;
		}
		else
		{
			if(position >= MONO_DIFF_SIZE)
				return 0;
		}
	}
	else
	{
		if(position >= GetSilenceBlockSize(block, config))
			return 0;
	}

	config->Differences.BlockDiffArray[block].phaseDiffArray[position].hertz = ref.hertz;
	config->Differences.BlockDiffArray[block].phaseDiffArray[position].diffPhase = diffPhase;
	config->Differences.BlockDiffArray[block].phaseDiffArray[position].channel = channel;

	config->Differences.BlockDiffArray[block].cntPhaseBlkDiff ++;
	config->Differences.cntPhaseAudioDiff ++;
	//config->Differences.cntTotalAudioDiff ++;
	
	return 1;
}

int IncrementCompared(int block, char channel, parameters *config)
{
	if(!config)
		return 0;

	config->Differences.cntTotalCompared ++;
	if(channel == CHANNEL_LEFT || channel == CHANNEL_MONO)
		config->Differences.cntTotalComparedLeft++;
	if(channel == CHANNEL_RIGHT)
		config->Differences.cntTotalComparedRight++;
	if(!IncrementCmpAmpl(block, channel, config))
		return 0;
	if(!IncrementCmpFreq(block, config))
		return 0;
	if(!IncrementCmpPhase(block, config))
		return 0;
	return 1;
}

int IncrementPerfectMatch(int block, char channel, parameters *config)
{
	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	if(block > config->types.totalBlocks)
		return 0;

	config->Differences.BlockDiffArray[block].perfectAmplMatch ++;
	config->Differences.cntPerfectAmplMatch ++;

	if(channel == CHANNEL_LEFT || channel == CHANNEL_MONO)
	{
		config->Differences.BlockDiffArray[block].perfectAmplMatchLeft ++;
		config->Differences.cntPerfectAmplMatchLeft ++;
	}

	if(channel == CHANNEL_RIGHT)
	{
		config->Differences.BlockDiffArray[block].perfectAmplMatchRight ++;
		config->Differences.cntPerfectAmplMatchRight ++;
	}
	
	return 1;
}

int IncrementCmpFreq(int block, parameters *config)
{
	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	if(block > config->types.totalBlocks)
		return 0;
	
	config->Differences.BlockDiffArray[block].cmpFreqBlkDiff ++;
	return 1;
}

int InsertFreqNotFound(int block, double freq, double amplitude, char channel, parameters *config)
{
	int position = 0;

	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	if(!config->Differences.BlockDiffArray[block].freqMissArray)
		return 0;

	if(block > config->types.totalBlocks)
		return 0;

	if(amplitude == NO_AMPLITUDE)
	{
		logmsg("WARNING: Received NO_AMPLITUDE for Frequency not found\n");
		return 0;
	}

	position = config->Differences.BlockDiffArray[block].cntFreqBlkDiff;
	config->Differences.BlockDiffArray[block].freqMissArray[position].hertz = freq;
	config->Differences.BlockDiffArray[block].freqMissArray[position].amplitude = amplitude;
	config->Differences.BlockDiffArray[block].freqMissArray[position].channel = channel;

	config->Differences.BlockDiffArray[block].cntFreqBlkDiff ++;
	config->Differences.cntFreqAudioDiff ++;
	config->Differences.cntTotalAudioDiff ++;

	return 1;
}

void PrintDifferentFrequencies(int block, parameters *config)
{
	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	if(config->Differences.BlockDiffArray[block].cntFreqBlkDiff)
		logmsgFileOnly("Frequencies not found:\n");

	for(int f = 0; f < config->Differences.BlockDiffArray[block].cntFreqBlkDiff; f++)
	{
		logmsgFileOnly("Frequency: %7g Hz\tAmplituide: %4.2f\tChannel: %c\n", 
			config->Differences.BlockDiffArray[block].freqMissArray[f].hertz,
			config->Differences.BlockDiffArray[block].freqMissArray[f].amplitude,
			config->Differences.BlockDiffArray[block].freqMissArray[f].channel);
	}
}

void PrintDifferentAmplitudes(int block, parameters *config)
{
	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	if(config->Differences.BlockDiffArray[block].cntAmplBlkDiff)
		logmsgFileOnly("\nDifferent Amplitudes:\n");

	for(int a = 0; a < config->Differences.BlockDiffArray[block].cntAmplBlkDiff; a++)
	{
		logmsgFileOnly("Frequency: %7.4g Hz\tAmplitude: %4.6g dBFS\tAmplitude Difference: %4.6g dB\tChannel: %c\n",
			config->Differences.BlockDiffArray[block].amplDiffArray[a].hertz,
			config->Differences.BlockDiffArray[block].amplDiffArray[a].refAmplitude,
			config->Differences.BlockDiffArray[block].amplDiffArray[a].diffAmplitude,
			config->Differences.BlockDiffArray[block].amplDiffArray[a].channel);
	}
}

void PrintDifferentPhases(int block, parameters *config)
{
	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	if(config->Differences.BlockDiffArray[block].cntPhaseBlkDiff)
		logmsgFileOnly("\nDifferent Phase:\n");

	for(int a = 0; a < config->Differences.BlockDiffArray[block].cntPhaseBlkDiff; a++)
	{
		logmsgFileOnly("Frequency: %7g Hz\tPhase Difference: %3.2f\tChannel: %c\n",
			config->Differences.BlockDiffArray[block].phaseDiffArray[a].hertz,
			config->Differences.BlockDiffArray[block].phaseDiffArray[a].diffPhase,
			config->Differences.BlockDiffArray[block].phaseDiffArray[a].channel);
	}
}

void PrintDifferenceArray(parameters *config)
{
	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	if(!config->Differences.cntTotalCompared)
	{
		logmsg("ERROR: Compared equals zero\n");
		return;
	}

	logmsgFileOnly("\n\n==========DIFFERENCE ARRAY==========\n");
	logmsgFileOnly("\nPerfect Frequency/Amplitude Matches: %ld\n",
				config->Differences.cntPerfectAmplMatch);
	logmsgFileOnly("Total Differences: %ld from %ld\nTotal: %g%%\n", 
				config->Differences.cntTotalAudioDiff,
				config->Differences.cntTotalCompared,
				(double)config->Differences.cntTotalAudioDiff*100.0/(double)config->Differences.cntTotalCompared);
	logmsgFileOnly("Total Frequencies not Found: %ld\n",
				config->Differences.cntFreqAudioDiff);
	logmsgFileOnly("Total Amplitudes not matched: %ld\n",
				config->Differences.cntAmplAudioDiff);
	logmsgFileOnly("Total Phases not matched: %ld from %ld (%g%%)\n",
				config->Differences.cntPhaseAudioDiff, config->Differences.cmpPhaseAudioDiff, 
				config->Differences.cmpPhaseAudioDiff > 0 ? (double)config->Differences.cntPhaseAudioDiff*100.0/(double)config->Differences.cmpPhaseAudioDiff : 0);

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		if(GetBlockType(config, b) < TYPE_CONTROL)
			continue;

		//if(config->Differences.BlockDiffArray[b].cntAmplBlkDiff)
		logmsgFileOnly("\n\nBlock: %s# %d (%d) Not Found: %ld Differences: %ld Phase Diff: %ld\n", 
				GetBlockName(config, b), GetBlockSubIndex(config, b), b,
				config->Differences.BlockDiffArray[b].cntFreqBlkDiff,
				config->Differences.BlockDiffArray[b].cntAmplBlkDiff,
				config->Differences.BlockDiffArray[b].cntPhaseBlkDiff);

		PrintDifferentFrequencies(b, config);
		PrintDifferentAmplitudes(b, config);
		PrintDifferentPhases(b, config);
	}
}

double FindDifferenceAverage(parameters *config)
{
	double		AvgDifAmp = 0;
	long int	count = 0;

	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		if(config->Differences.BlockDiffArray[b].type <= TYPE_CONTROL)
			continue;

		for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
		{
			AvgDifAmp += config->Differences.BlockDiffArray[b].amplDiffArray[a].diffAmplitude;
			count ++;
		}
	}

	if(count)
		AvgDifAmp /= count;

	return AvgDifAmp;
}

void SubstractDifferenceAverageFromResults(parameters *config)
{
	if(!config)
		return;

	if(!config->Differences.BlockDiffArray)
		return;

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		if(config->Differences.BlockDiffArray[b].type <= TYPE_CONTROL)
			continue;

		for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
			config->Differences.BlockDiffArray[b].amplDiffArray[a].diffAmplitude -= config->averageDifference;
	}
}

double FindDifferencePercentOutsideViewPort(double *maxAmpl, int *type, double threshold, parameters *config)
{
	int		typeMax = 0, currentType = TYPE_NOTYPE;
	double	maxPercent = 0, maxFromType = 0;
	double	count = 0, outside = 0, localMax = 0;

	if(!config || !config->Differences.BlockDiffArray || !maxAmpl || !type)
	{
		logmsg("WARNING: Invalid data to find difference percent outside view port\n");
		return 0;
	}

	*maxAmpl = 0;
	*type = TYPE_NOTYPE;
	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		int seltype = 0;

		seltype = config->Differences.BlockDiffArray[b].type;
		if(seltype <= TYPE_CONTROL)
			continue;

		if(seltype != currentType)
		{
			if(count)
			{	
				double percent = 0;
	
				percent = outside/count*100.0;
				if(percent > maxPercent)
				{
					maxPercent = percent;
					maxFromType = localMax;
					typeMax = currentType;
				}
			}

			count = 0;
			outside = 0;
			localMax = 0;

			currentType = seltype;
		}

		for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
		{
			double ampl = 0;

			ampl = fabs(config->Differences.BlockDiffArray[b].amplDiffArray[a].diffAmplitude);
			if(ampl >= threshold)
			{
				if(localMax < ampl)
					localMax = ampl;
				outside ++;
			}
			count ++;
		}
	}

	if(count)
	{	
		double percent = 0;

		percent = outside/count*100.0;
		if(percent > maxPercent)
		{
			maxPercent = percent;
			maxFromType = localMax;
			typeMax = currentType;
		}
	}

	if(maxFromType)
	{
		*maxAmpl = maxFromType;
		*type = typeMax;
	}
	return(maxPercent);
}

double FindVisibleInViewPortWithinStandardDeviation(double *maxAmpl, double *outside, int type, int numstd, parameters *config)
{
	double	mean = 0, standard = 0, threshold = 0, count = 0;

	if(!config)
		return -1;

	if(!config->Differences.BlockDiffArray || !maxAmpl || !outside)
		return -1;

	if(numstd < 1)
		numstd = 1;

	if(numstd > 2)
		numstd = 2;

	*maxAmpl = 0;
	*outside = 0;

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		int currentType = 0;

		currentType = config->Differences.BlockDiffArray[b].type;
		if(currentType <= TYPE_CONTROL)
			continue;

		if(type == currentType)
		{
			for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
			{
				mean +=  fabs(config->Differences.BlockDiffArray[b].amplDiffArray[a].diffAmplitude);
				count ++;
			}
		}
	}

	if(!count)
		return -1;

	mean = mean/count;
	count = 0;
	// STD DEV
	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		int currentType = 0;

		currentType = config->Differences.BlockDiffArray[b].type;
		if(currentType <= TYPE_CONTROL)
			continue;

		if(type == currentType)
		{
			for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
			{
				double ampl = 0;
		
				ampl = fabs(config->Differences.BlockDiffArray[b].amplDiffArray[a].diffAmplitude);
				standard += pow(ampl - mean, 2);
				count++;
			}
		}
	}
	if(!count)
		return -1;

	standard = sqrt(standard/(count-1));
	count = 0;

	threshold = mean + numstd*standard;
	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		int currentType = 0;

		currentType = config->Differences.BlockDiffArray[b].type;
		if(currentType <= TYPE_CONTROL)
			continue;

		if(type == currentType)
		{
			for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
			{
				double ampl = 0;
		
				ampl = fabs(config->Differences.BlockDiffArray[b].amplDiffArray[a].diffAmplitude);
				if(ampl >= threshold)
				{
					if(*maxAmpl < ampl)
						*maxAmpl = ampl;
					(*outside) ++;
				}
				count ++;
			}
		}
	}	
	
	if(count)
		*outside = *outside/count*100.0;
	return(threshold);
}

int FindExtraFrequenciesByType(int type, long int *inside, long int *count, char channel, parameters *config)
{
	long int	extraCount = 0, extraTotal = 0;

	if(!config || !inside || !count)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	*inside = 0;
	*count = 0;
	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		if(config->Differences.BlockDiffArray[b].type == type)
		{
			/* Extra */
			if(channel == CHANNEL_LEFT || channel == CHANNEL_STEREO || channel == CHANNEL_MONO)
			{
				for(long int i = config->MaxFreq-1; i >= 0; i--)
				{
					AudioSignal *Signal	= NULL;

					Signal = config->comparisonSignal;
					if(Signal && Signal->Blocks[b].freq[i].hertz
						&& Signal->Blocks[b].freq[i].amplitude > config->significantAmplitude)
					{
						extraTotal++;
						if(!Signal->Blocks[b].freq[i].matched)
							extraCount++;
					}
				}
			}

			if((channel == CHANNEL_RIGHT || channel == CHANNEL_STEREO)
				&& config->referenceSignal && config->referenceSignal->Blocks[b].freqRight)
			{
				for(int i = config->MaxFreq-1; i >= 0; i--)
				{
					AudioSignal *Signal	= NULL;
	
					Signal = config->comparisonSignal;
					if(Signal && Signal->Blocks[b].freqRight[i].hertz
						&& Signal->Blocks[b].freqRight[i].amplitude > config->significantAmplitude)
					{
						extraTotal++;
						if(!Signal->Blocks[b].freqRight[i].matched)
							extraCount++;
					}
				}
			}
		}
	}
	
	*inside = extraCount;
	*count = extraTotal;
	return 1;
}

int FindMissingFrequenciesByType(int type, long int *inside, long int *count, char channel, parameters *config)
{
	long int	extraCount = 0, extraTotal = 0;

	if(!config || !inside || !count)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	*inside = 0;
	*count = 0;
	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		if(config->Differences.BlockDiffArray[b].type == type)
		{
			/* Missing */
			if(channel == CHANNEL_LEFT || channel == CHANNEL_STEREO || channel == CHANNEL_MONO)
			{
				for(long int i = config->MaxFreq-1; i >= 0; i--)
				{
					AudioSignal *Signal	= NULL;

					Signal = config->referenceSignal;
					if(Signal && Signal->Blocks[b].freq[i].hertz
						&& Signal->Blocks[b].freq[i].amplitude > config->significantAmplitude)
					{
						extraTotal++;
						if(!Signal->Blocks[b].freq[i].matched)
							extraCount++;
					}
				}
			}

			if((channel == CHANNEL_RIGHT || channel == CHANNEL_STEREO)
				&& config->referenceSignal && config->referenceSignal->Blocks[b].freqRight)
			{
				for(int i = config->MaxFreq-1; i >= 0; i--)
				{
					AudioSignal *Signal	= NULL;
	
					Signal = config->referenceSignal;
					if(Signal && Signal->Blocks[b].freqRight[i].hertz
						&& Signal->Blocks[b].freqRight[i].amplitude > config->significantAmplitude)
					{
						extraTotal++;
						if(!Signal->Blocks[b].freqRight[i].matched)
							extraCount++;
					}
				}
			}
		}
	}
	
	*inside = extraCount;
	*count = extraTotal;
	return 1;
}

long int FindDifferenceAveragesperBlock(double thresholdAmplitude, double thresholdMissing, double thresholdExtra, parameters *config)
{
	long int total = 0;

	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		double		average = 0, missing = 0, extra = 0;
		long int	missingCount = 0, missingTotal = 0;
		long int	extraCount = 0, extraTotal = 0;
		long int	count = 0;

		if(config->Differences.BlockDiffArray[b].type <= TYPE_CONTROL)
			continue;

		/* Amplitude Difference */
		for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
		{
			// Check if this compare is needed
			if(config->Differences.BlockDiffArray[b].amplDiffArray[a].refAmplitude > config->significantAmplitude)
			{
				average += fabs(config->Differences.BlockDiffArray[b].amplDiffArray[a].diffAmplitude);
				count ++;
			}
		}

		if(count)
		{
			average = average/(double)count;
			if(average >= thresholdAmplitude)
			{
				config->referenceSignal->Blocks[b].AverageDifference = average;
				config->comparisonSignal->Blocks[b].AverageDifference = average;
				total ++;
			}
		}

		/* Missing & Extra */
		for(long int i = config->MaxFreq-1; i >= 0; i--)
		{
			AudioSignal *Signal	= NULL;

			Signal = config->referenceSignal;
			if(Signal && Signal->Blocks[b].freq[i].hertz)
			{
				missingTotal++;
				if(!Signal->Blocks[b].freq[i].matched
					&& Signal->Blocks[b].freq[i].amplitude > config->significantAmplitude)
					missingCount++;
			}

			Signal = config->comparisonSignal;
			if(Signal && Signal->Blocks[b].freq[i].hertz)
			{
				extraTotal++;
				if(!Signal->Blocks[b].freq[i].matched
					&& Signal->Blocks[b].freq[i].amplitude > config->significantAmplitude)
					extraCount++;
			}
		}

		if(config->referenceSignal && config->referenceSignal->Blocks[b].freqRight)
		{
			for(int i = config->MaxFreq-1; i >= 0; i--)
			{
				AudioSignal *Signal	= NULL;
	
				Signal = config->referenceSignal;
				if(Signal && Signal->Blocks[b].freqRight[i].hertz)
				{
					missingTotal++;
					if(!Signal->Blocks[b].freqRight[i].matched
						&& Signal->Blocks[b].freqRight[i].amplitude > config->significantAmplitude)
						missingCount++;
				}
	
				Signal = config->comparisonSignal;
				if(Signal && Signal->Blocks[b].freqRight[i].hertz)
				{
					extraTotal++;
					if(!Signal->Blocks[b].freqRight[i].matched
						&& Signal->Blocks[b].freqRight[i].amplitude > config->significantAmplitude)
						extraCount++;
				}
			}
		}
		
		if(missingTotal)
		{
			missing = (double)missingCount/(double)missingTotal*100.0;
			if(missing > thresholdMissing)
			{
				config->referenceSignal->Blocks[b].missingPercent = missing;
				config->comparisonSignal->Blocks[b].missingPercent = missing;  // set to 0 to not show both
				total ++;
			}
		}

		if(extraTotal)
		{
			extra = (double)extraCount/(double)extraTotal*100.0;
			if(extra > thresholdExtra)
			{
				config->referenceSignal->Blocks[b].extraPercent = extra;	// set to 0 to not show both
				config->comparisonSignal->Blocks[b].extraPercent = extra;
				total ++;
			}
		}
	}

	return total;
}


int FindDifferenceTypeTotals(int type, long int *cntAmplBlkDiff, long int *cmpAmplBlkDiff, char channel, parameters *config)
{
	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	if(!cntAmplBlkDiff || !cmpAmplBlkDiff)
		return 0;

	*cntAmplBlkDiff = 0;
	*cmpAmplBlkDiff = 0;

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		if(config->Differences.BlockDiffArray[b].type < TYPE_SILENCE)
			continue;

		if(type == config->Differences.BlockDiffArray[b].type)
		{
			if(channel == CHANNEL_STEREO)
			{
				*cntAmplBlkDiff += config->Differences.BlockDiffArray[b].cntAmplBlkDiff;
				*cmpAmplBlkDiff += config->Differences.BlockDiffArray[b].cmpAmplBlkDiff;
			}

			if(channel == CHANNEL_LEFT || channel == CHANNEL_MONO)
			{
				*cntAmplBlkDiff += config->Differences.BlockDiffArray[b].cntAmplBlkDiffLeft;
				*cmpAmplBlkDiff += config->Differences.BlockDiffArray[b].cmpAmplBlkDiffLeft;
			}
			
			if(channel == CHANNEL_RIGHT)
			{
				*cntAmplBlkDiff += config->Differences.BlockDiffArray[b].cntAmplBlkDiffRight;
				*cmpAmplBlkDiff += config->Differences.BlockDiffArray[b].cmpAmplBlkDiffRight;
			}
		}
	}

	return 1;
}

int FindDifferenceWithinInterval(int type, long int *inside, long int *count, double MaxInterval, char channel, parameters *config)
{
	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	if(!inside || !count)
		return 0;

	*inside = 0;
	*count = 0;

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		if(config->Differences.BlockDiffArray[b].type < TYPE_SILENCE)
			continue;

		if(type == config->Differences.BlockDiffArray[b].type)
		{
			if(channel == CHANNEL_STEREO)
			{
				for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
				{
					if(fabs(config->Differences.BlockDiffArray[b].amplDiffArray[a].diffAmplitude) <= MaxInterval)
						(*inside)++;
				
				}
				if(!config->drawPerfect)
					(*inside) += config->Differences.BlockDiffArray[b].perfectAmplMatch;
				(*count) += config->Differences.BlockDiffArray[b].cmpAmplBlkDiff;
			}

			if(channel == CHANNEL_LEFT || channel == CHANNEL_MONO)
			{
				for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
				{
					if(config->Differences.BlockDiffArray[b].amplDiffArray[a].channel == CHANNEL_LEFT &&
						fabs(config->Differences.BlockDiffArray[b].amplDiffArray[a].diffAmplitude) <= MaxInterval)
							(*inside)++;
				
				}
				if(!config->drawPerfect)
					(*inside) += config->Differences.BlockDiffArray[b].perfectAmplMatchLeft;
				(*count) += config->Differences.BlockDiffArray[b].cmpAmplBlkDiffLeft;
			}

			if(channel == CHANNEL_RIGHT)
			{
				for(int a = 0; a < config->Differences.BlockDiffArray[b].cntAmplBlkDiff; a++)
				{
					if(config->Differences.BlockDiffArray[b].amplDiffArray[a].channel == CHANNEL_RIGHT &&
						fabs(config->Differences.BlockDiffArray[b].amplDiffArray[a].diffAmplitude) <= MaxInterval)
							(*inside)++;
				
				}
				if(!config->drawPerfect)
					(*inside) += config->Differences.BlockDiffArray[b].perfectAmplMatchRight;
				(*count) += config->Differences.BlockDiffArray[b].cmpAmplBlkDiffRight;
			}
		}
	}

	return 1;
}

int FindPerfectMatches(int type, long int *inside, long int *count, char channel, parameters *config)
{
	if(!config)
		return 0;

	if(!config->Differences.BlockDiffArray)
		return 0;

	if(!inside || !count)
		return 0;

	*inside = 0;
	*count = 0;

	for(int b = 0; b < config->types.totalBlocks; b++)
	{
		if(config->Differences.BlockDiffArray[b].type <= TYPE_SILENCE)
			continue;

		if(type == config->Differences.BlockDiffArray[b].type)
		{
			if(channel == CHANNEL_STEREO)
			{
				(*inside) += config->Differences.BlockDiffArray[b].perfectAmplMatch;
				(*count) += config->Differences.BlockDiffArray[b].cmpAmplBlkDiff;
			}

			if(channel == CHANNEL_LEFT || channel == CHANNEL_MONO)
			{
				(*inside) += config->Differences.BlockDiffArray[b].perfectAmplMatchLeft;
				(*count) += config->Differences.BlockDiffArray[b].cmpAmplBlkDiffLeft;
			}

			if(channel == CHANNEL_RIGHT)
			{
				(*inside) += config->Differences.BlockDiffArray[b].perfectAmplMatchRight;
				(*count) += config->Differences.BlockDiffArray[b].cmpAmplBlkDiffRight;
			}
		}
	}

	return 1;
}
