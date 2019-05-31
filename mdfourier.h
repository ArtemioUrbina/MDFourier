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

#ifndef MDFOURIER_H
#define MDFOURIER_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>

#include <string.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>
#include <libgen.h>

#include "incbeta.h"

#define MDVERSION "0.913"


#define MAX_FREQ_COUNT		100000 	/* Number of frequencies to compare(MAX) */
#define FREQ_COUNT			2000	/* Number of frequencies to compare(default) */
#define SIGNIFICANT_VOLUME	-96		/* ~20.0f * log10f( 1.0f / 65536.0f ) */
#define	REFRESH_RATE_NOISE_DETECT_TOLERANCE	2.0

#define TYPE_SILENCE	0
#define TYPE_SYNC		-1
#define TYPE_NOTYPE		-2
#define TYPE_CONTROL	TYPE_SILENCE

#define NO_INDEX 		-100
#define	NO_AMPLITUDE	-1000

/* This is the difference allowed between reference and compared */
/* amplitudes to match, in dbs */
#define DBS_TOLERANCE	0

/* Width of each peak */
#define HERTZ_WIDTH			0.0

/* +/- Tolerance in frequency Difference to be the same one */
#define HERTZ_DIFF			0.0

#define START_HZ	5
#define END_HZ		22050

typedef struct max_vol {
	int16_t		magnitude;
	uint32_t	offset;
	uint32_t	samplerate;
	double		framerate;
} MaximumVolume;

typedef struct max_mag {
	double		magnitude;
	double		hertz;
	long int	block;
} MaximumMagnitude;

typedef struct abt_st {
	char		typeName[128];
	int 		type;
	int			elementCount;
	int			frames;
	char		color[20];
} AudioBlockType;

typedef struct abd_st {
	char			Name[128];
	int				totalChunks;
	int				regularChunks;
	double			platformMSPerFrame;
	int				pulseSyncFreq;
	int				pulseMinVol;
	int				pulseVolDiff;
	int				pulseFrameMinLen;
	int				pulseFrameMaxLen;
	int				pulseCount;

	AudioBlockType	*typeArray;
	int				typeCount;
} AudioBlockDef;

/********************************************************/
/* WAV data structures */
typedef struct	WAV_HEADER
{
	/* RIFF Chunk Descriptor */
	uint8_t 		RIFF[4];		/* RIFF Header Magic header */
	uint32_t		ChunkSize;		/* RIFF Chunk Size */
	uint8_t 		WAVE[4];		/* WAVE Header */
	/* "fmt" sub-chunk */
	uint8_t 		fmt[4]; 		/* FMT header */
	uint32_t		Subchunk1Size;	/* Size of the fmt chunk */
	uint16_t		AudioFormat;	/* Audio format 1=PCM,6=mulaw,7=alaw,	  257=IBM Mu-Law, 258=IBM A-Law, 259=ADPCM */
	uint16_t		NumOfChan;		/* Number of channels 1=Mono 2=Sterio */
	uint32_t		SamplesPerSec;	/* Sampling Frequency in Hz */
	uint32_t		bytesPerSec;	/* bytes per second */
	uint16_t		blockAlign; 	/* 2=16-bit mono, 4=16-bit stereo */
	uint16_t		bitsPerSample;	/* Number of bits per sample */
	/* "data" sub-chunk */
	uint8_t 		Subchunk2ID[4]; /* "data"  string */
	uint32_t		Subchunk2Size;	/* Sampled data length */
} wav_hdr;
/********************************************************/

typedef struct FrequencySt {
	double	hertz;
	double	magnitude;
	double	amplitude;
	double	phase;
	int		matched;
} Frequency;

typedef struct fftw_spectrum_st {
	fftw_complex  	*spectrum;
	size_t			size;
	double			seconds;
} FFTWSpectrum;

typedef struct AudioBlock_st {
	Frequency		*freq;
	FFTWSpectrum	fftwValues;
	int				index;
	int				type;
} AudioBlocks;

typedef struct AudioSt {
	char		SourceFile[1024];

	int 		hasFloor;
	double		floorFreq;
	double		floorAmplitude;

	char 		*Samples;
	double		framerate;
	wav_hdr		header;

	long int	startOffset;
	long int	endOffset;

	double		MaxMagnitude;
	double		MinAmplitude;

	double		RefreshNoise;
	double		CRTLow;
	double		CRTHigh;

	AudioBlocks *Blocks; 
}  AudioSignal;

/********************************************************/

typedef struct window_unit_st {
	double		*window;
	long int	frames;
	long int	size;
} windowUnit;

typedef struct window_st {
	windowUnit	*windowArray;
	int windowCount;
} windowManager;

/********************************************************/

typedef struct freq_diff_st {
	double	hertz;
	double	amplitude;
	double	weight;
} FreqDifference;

typedef struct ampl_diff_st {
	double	hertz;
	double	refAmplitude;
	double	diffAmplitude;
	double	weight;
} AmplDifference;

typedef struct blk_diff_st {
	FreqDifference *freqMissArray;
	int			cntFreqBlkDiff;
	double		weightedFreqBlkDiff;

	AmplDifference *amplDiffArray;
	int			cntAmplBlkDiff;
	double		weightedAmplBlkDiff;
} BlockDifference;

typedef struct block_diff_st {
	BlockDifference	*BlockDiffArray;

	long int 		cntFreqAudioDiff;
	long int		cntAmplAudioDiff;
	double			weightedFreqAudio;
	double			weightedAmplAudio;

	long int		cntTotalCompared;
	long int		cntTotalAudioDiff;
	double			weightedAudioDiff;
} AudioDifference;

/********************************************************/

typedef struct parameters_st {
	char			referenceFile[1024];
	char			targetFile[1024];
	char			folderName[2512];
	char			baseName[2512];
	char			compareName[2512];
	double			tolerance;
	int				startHz, endHz;
	int				showAll;
	int				extendedResults;
	int				justResults;
	int				verbose;
	char			window;
	char			channel;
	int				MaxFreq;
	int				clock;
	char			normalize;
	int				ignoreFloor;
	int				useOutputFilter;
	int				outputFilterFunction;
	AudioBlockDef	types;
	AudioDifference	Differences;
	double			origSignificantVolume;
	double			significantVolume;
	double			smallerFramerate;
	int				logScale;
	int				debugSync;
	int				reverseCompare;
	int				ZeroPad;
	int				timeDomainNormalize;
	int				averagePlot;
	int				weightedAveragePlot;

	fftw_plan		sync_plan;
	fftw_plan		model_plan;
	fftw_plan		reverse_plan;

#ifdef MDWAVE
	int				maxBlanked;
	int				invert;
	int				chunks;
	double			floorAmplitude;
#endif
} parameters;


#endif
