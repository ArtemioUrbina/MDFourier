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
#include <errno.h>

#include <string.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>
#include <libgen.h>

#include "incbeta.h"

#define MDVERSION "0.971"

#define MAX_FREQ_COUNT		100000 	/* Number of frequencies to compare(MAX) */
#define FREQ_COUNT			2000	/* Number of frequencies to compare(default) */

#define SIGNIFICANT_VOLUME	-60.0
#define NS_SIGNIFICANT_VOLUME	-66.0
#define NS_LOWEST_AMPLITUDE		-120
#define	PCM_16BIT_MIN_AMPLITUDE	-96.0

#define TYPE_SILENCE			0
#define TYPE_SYNC				-1
#define TYPE_NOTYPE				-2
#define TYPE_INTERNAL_KNOWN		-3
#define TYPE_INTERNAL_UNKNOWN	-4
#define TYPE_SKIP				-5
#define TYPE_CONTROL			TYPE_SILENCE

#define	BAR_DIFF_DB_TOLERANCE	1.0

#define NO_INDEX 		-100
#define	NO_AMPLITUDE	-1000

/* This is the difference allowed between reference and compared */
/* amplitudes to match, in dbs */
#define DBS_TOLERANCE	0

/* Width of each peak */
#define HERTZ_WIDTH			0.0

/* +/- Tolerance in frequency Difference to be the same one */
#define HERTZ_DIFF			0.0

#define START_HZ	20.0
#define END_HZ		20000.0

#define DB_HEIGHT	18.0
#define DB_DIFF		12.0

#define PLOT_RES_X 1600.0
#define PLOT_RES_Y 800.0

#define PLOT_RES_X_HIGH 1920.0
#define PLOT_RES_Y_HIGH 1080.0

#define PLOT_RES_X_LOW 800.0
#define PLOT_RES_Y_LOW 400.0

#define BUFFER_SIZE		4096
#define T_BUFFER_SIZE	BUFFER_SIZE*2+256

#define NO_ROLE		0
#define	ROLE_REF	1
#define	ROLE_COMP	2

#define CHANNEL_NONE	'-'
#define	CHANNEL_MONO	'm'
#define	CHANNEL_STEREO	's'
#define	CHANNEL_NOISE	'n'

#if defined (WIN32)
	#define MAX_FOLDER_NAME	50
	#define MAX_FILE_NAME	25
#endif

//#define FFTSIZEDEBUG	1

enum normalize
{
	max_time,
	max_frequency,
	average,
	none
};

#define PROFILE_FILE	"profiles\\mdfblocksGEN.mfn"

typedef struct max_vol {
	int16_t		magnitude;
	uint32_t	offset;
	uint32_t	samplerate;
	double		framerate;
} MaxVolum;

typedef struct max_mag {
	double		magnitude;
	double		hertz;
	long int	block;
} MaxMagn;

#define NTSC	0
#define PAL		1

typedef struct abt_st {
	char		typeName[128];
	int 		type;
	int			elementCount;
	int			frames;
	char		color[20];
	char		channel;
	int			syncTone;
	double		syncLen;
} AudioBlockType;

typedef struct sync_st {
	double			MSPerFrame;
	double			LineCount;
	int				pulseSyncFreq;
	int				pulseFrameLen;
	int				pulseCount;
} VideoBlockDef;

typedef struct abd_st {
	char			Name[256];
	int				totalChunks;
	int				regularChunks;
	VideoBlockDef	SyncFormat[2];

	AudioBlockType	*typeArray;
	int				typeCount;
} AudioBlockDef;

/********************************************************/
/* WAV data structures */

typedef struct	RIFF_HEADER
{
	/* RIFF Chunk Descriptor */
	uint8_t 		RIFF[4];		/* RIFF Header Magic header */
	uint32_t		ChunkSize;		/* RIFF Chunk Size */
	uint8_t 		WAVE[4];		/* WAVE Header */
} riff_hdr;

typedef struct	SYB_CHUNK
{
	/* sub-chunk */
	uint8_t 		chunkID[4]; 	/* Chunk ID */
	uint32_t		Size;			/* Size of the SubChunk */
} sub_chunk;

#define	WAVE_FORMAT_PCM			0x0001
#define	WAVE_FORMAT_IEEE_FLOAT	0x0003
#define	WAVE_FORMAT_ALAW		0x0006
#define	WAVE_FORMAT_MULAW		0x0007
#define WAVE_FORMAT_EXTENSIBLE	0xFFFE

typedef struct	FMT_HEADER
{
	/* "fmt" sub-chunk */
	uint8_t 		fmt[4]; 		/* FMT header */
	uint32_t		Subchunk1Size;	/* Size of the fmt chunk */
	uint16_t		AudioFormat;	/* Audio format 1=PCM,6=mulaw,7=alaw,	  257=IBM Mu-Law, 258=IBM A-Law, 259=ADPCM */
	uint16_t		NumOfChan;		/* Number of channels 1=Mono 2=Sterio */
	uint32_t		SamplesPerSec;	/* Sampling Frequency in Hz */
	uint32_t		bytesPerSec;	/* bytes per second */
	uint16_t		blockAlign; 	/* 2=16-bit mono, 4=16-bit stereo */
	uint16_t		bitsPerSample;	/* Number of bits per sample */
} fmt_hdr;

typedef struct	DATA_HEADER
{
	/* "data" sub-chunk */
	uint8_t 		DataID[4]; /* "data"  string */
	uint32_t		DataSize;	/* Sampled data length */
} data_hdr;

typedef struct	WAV_HEADER
{
	riff_hdr	riff;
	fmt_hdr		fmt;
	data_hdr	data;
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
	char		SourceFile[BUFFER_SIZE];
	int			role;

	int 		hasFloor;
	double		floorFreq;
	double		floorAmplitude;

	char 		*Samples;
	double		framerate;
	wav_hdr		header;

	long int	startOffset;
	long int	endOffset;

	MaxMagn		MaxMagnitude;
	double		MinAmplitude;

	double		gridFrequency;
	double		scanrateFrequency;
	double		SilenceBinSize;

	int			nyquistLimit;
	double		startHz;
	double		endHz;

	AudioBlocks *Blocks;
}  AudioSignal;

/********************************************************/

typedef struct window_unit_st {
	double		*window;
	long int	frames;
	double		seconds;
	long int	size;
} windowUnit;

typedef struct window_st {
	windowUnit	*windowArray;
	int windowCount;
	int MaxWindow;
	int SamplesPerSec;
	char winType;
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
	long int		cntFreqBlkDiff;
	long int		cmpFreqBlkDiff;
	long int		perfectAmplMatch;
	double			weightedFreqBlkDiff;

	AmplDifference *amplDiffArray;
	long int		cntAmplBlkDiff;
	long int		cmpAmplBlkDiff;
	double			weightedAmplBlkDiff;

	int				type;
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
	char			referenceFile[BUFFER_SIZE];
	char			targetFile[BUFFER_SIZE];
	char			folderName[BUFFER_SIZE+128];
	char			baseName[BUFFER_SIZE];
	char			compareName[BUFFER_SIZE];
	char			profileFile[BUFFER_SIZE];
	double			tolerance;
	double			startHz, endHz;
	double			startHzPlot, endHzPlot;
	double			maxDbPlotZC;
	int				showAll;
	int				extendedResults;
	int				justResults;
	int				verbose;
	char			window;
	char			channel;
	int				MaxFreq;
	int				clock;
	int				ignoreFloor;
	int				useOutputFilter;
	int				outputFilterFunction;
	AudioBlockDef	types;
	AudioDifference	Differences;
	double			origSignificantAmplitude;
	double			significantAmplitude;
	double			referenceNoiseFloor;
	double			smallerFramerate;
	double			referenceFramerate;
	int				logScale;
	int				debugSync;
	int				reverseCompare;
	int				ZeroPad;
	enum normalize	normType;
	int				channelBalance;
	int				showPercent;
	int				noSyncProfile;
	double			NoSyncTotalFrames;
	int				ignoreFrameRateDiff;
	int				labelNames;

	int				plotDifferences;
	int				plotMissing;
	int				plotSpectrogram;
	int				plotTimeSpectrogram;
	int				plotNoiseFloor;
	int				averagePlot;
	int				weightedAveragePlot;
	int				drawWindows;
	int				averageIgnore;
	double			averageLine;
	int				outputCSV;
	int				whiteBG;
	int				smallFile;
	int				syncTolerance;
	double			AmpBarRange;

	double 			plotResX;
	double			plotResY;

	fftw_plan		sync_plan;
	fftw_plan		model_plan;
	fftw_plan		reverse_plan;

	double			refNoiseMin;
	double			refNoiseMax;

	int				videoFormatRef;
	int				videoFormatCom;
	int				nyquistLimit;

#ifdef MDWAVE
	int				maxBlanked;
	int				invert;
	int				chunks;
	int				useCompProfile;
#endif

#ifdef	FFTSIZEDEBUG
	long			fftsize;
#endif
	AudioSignal		*referenceSignal;
	AudioSignal		*comparisonSignal;
} parameters;


#endif
