/* 
 * MDFourier
 * A Fourier Transform analysis tool to compare different 
 * Sega Genesis/Mega Drive audio hardware revisions, and
 * other hardware in the future
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

#define MDVERSION "0.992rc3"

#if INTPTR_MAX == INT64_MAX
#define	BITS_MDF "64-bit"
#elif INTPTR_MAX == INT32_MAX
#define	BITS_MDF "32-bit"
#else
#error Unknown pointer size or missing size macros!
#endif

#define MAX_FREQ_COUNT		40000 	/* Number of frequencies to compare(MAX) */
#define FREQ_COUNT			2000	/* Number of frequencies to compare(default) */

#define SIGNIFICANT_VOLUME			-60.0
#define NS_SIGNIFICANT_VOLUME		-66.0
#define NS_LOWEST_AMPLITUDE			-200
#define	PCM_16BIT_MIN_AMPLITUDE		-96.0
#define LOWEST_NOISEFLOOR_ALLOWED	-40.0

#define TYPE_NOTYPE				-1000
#define TYPE_SILENCE			-1
#define TYPE_SYNC				-2
#define TYPE_INTERNAL_KNOWN		-4
#define TYPE_INTERNAL_UNKNOWN	-5
#define TYPE_SKIP				-6
#define TYPE_TIMEDOMAIN			-7
#define	TYPE_SILENCE_OVERRIDE	-8
#define	TYPE_WATERMARK			-9
#define TYPE_CONTROL			TYPE_SILENCE


#define TYPE_NULLTYPE_C			'\0'
#define TYPE_SILENCE_C			'n'
#define TYPE_SYNC_C				's'
#define TYPE_NOTYPE_C			'-'
#define TYPE_INTERNAL_KNOWN_C	'i'
#define TYPE_INTERNAL_UNKNOWN_C	'I'
#define TYPE_SKIP_C				'k'
#define TYPE_TIMEDOMAIN_C		't'
#define TYPE_SILENCE_OVER_C		'N'
#define TYPE_WATERMARK_C		'W'

#define	BAR_DIFF_DB_TOLERANCE	1.0

#define NO_INDEX 		-100
#define	NO_AMPLITUDE	-1000

/* Width of each peak */
#define HERTZ_WIDTH			0.0

/* +/- Tolerance in frequency Difference to be the same one */
#define HERTZ_DIFF			0.0

#define START_HZ	20.0
#define END_HZ		20000.0
#define MAX_HZ		192000.0

#define START_HZ_PLOT	0

#define DB_HEIGHT	18.0
#define DB_DIFF		DB_HEIGHT/2.0

#define	MAXINT16		 32768.0
#define	MININT16		-32767.0

#define INT16_03DB		23197.0   // 0x5AD9

#define SIG_CLK_DIFF 0.15
// Use -3dbfs scale
#define WAVEFORM_SCALE	INT16_03DB
// Use Max Scale
//#define WAVEFORM_SCALE	(MAXINT16-1)

#define BUFFER_SIZE		4096
#define T_BUFFER_SIZE	BUFFER_SIZE*2+256

#define NO_ROLE		0
#define	ROLE_REF	1
#define	ROLE_COMP	2

#define CHANNEL_NONE	'-'
#define	CHANNEL_MONO	'm'
#define	CHANNEL_STEREO	's'
#define	CHANNEL_NOISE	'n'

#define NO_SYNC_AUTO_C		'A'
#define NO_SYNC_MANUAL_C	'M'

#define NO_SYNC_AUTO		0
#define NO_SYNC_MANUAL		1

#define	INVALID_CHANNELS	-1

#define	WATERMARK_NONE				0
#define	WATERMARK_VALID				1
#define	WATERMARK_INVALID			2
#define	WATERMARK_INDETERMINATE		3

#define	AMPL_HIDIFF		6.0
#define	MISS_HIDIFF		10.0
#define	EXTRA_HIDIFF	10.0

#define	NO_CLK			-1

#if defined (WIN32)
	#define MAX_FOLDER_NAME	50
	#define MAX_FILE_NAME	25
#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
	#define FOLDERCHAR	'\\'
#else 
	#define FOLDERCHAR	'/'
#endif

#if INTPTR_MAX == INT64_MAX
#define	BITS_MDF "64-bit"
#elif INTPTR_MAX == INT32_MAX
#define	BITS_MDF "32-bit"
#else
#error Unknown pointer size or missing size macros!
#endif

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif

#define	PROFILE_VER		2.1
#define	MAX_SYNC		10

#define	FREQDOMTRIES	10
#define	FREQDOMRATIO	40.0

#define	DELAYCOUNT		10

enum normalize
{
	max_time,
	max_frequency,
	average,
	none
};

#define OUTPUT_FOLDER		"MDFResults"
#define OUTPUT_FOLDER_MDW	"MDWResults"


typedef struct max_vol {
	int16_t		maxSample;
	uint32_t	offset;
	uint32_t	samplerate;
	double		framerate;
} MaxSample;

typedef struct max_mag {
	double		magnitude;
	double		hertz;
	long int	block;
} MaxMagn;

typedef struct abt_st {
	char		typeName[128];
	char		typeDisplayName[128];
	int 		type;
	int			elementCount;
	int			frames;
	int			cutFrames;
	char		color[20];
	char		channel;
	int			syncTone;
	double		syncLen;
	int			IsaddOnData;
} AudioBlockType;

typedef struct sync_st {
	char			syncName[255];
	double			MSPerFrame;
	double			LineCount;
	int				pulseSyncFreq;
	int				pulseFrameLen;
	int				pulseCount;
} VideoBlockDef;

typedef struct abd_st {
	char			Name[256];
	int				totalBlocks;
	int				regularBlocks;

	VideoBlockDef	SyncFormat[MAX_SYNC];
	int				syncCount;

	AudioBlockType	*typeArray;
	int				typeCount;

	int				useWatermark;
	int				watermarkValidFreq;
	int				watermarkInvalidFreq;
	char			watermarkDisplayName[128];
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
	short	matched;
} Frequency;

typedef struct fftw_spectrum_st {
	fftw_complex  	*spectrum;
	size_t			size;
} FFTWSpectrum;

typedef struct samples_st {
	int16_t			*samples;
	int16_t			*window_samples;
	long int		size;
	long int		difference;
} BlockSamples;

typedef struct AudioBlock_st {
	Frequency		*freq;
	FFTWSpectrum	fftwValues;
	BlockSamples	audio;
	BlockSamples	*internalSync;
	int				internalSyncCount;
	int				index;
	int				type;
	int				frames;
	double 			seconds;

#ifdef INDIVPHASE
	Frequency		*linFreq;
	long int		linFreqSize;
#endif

	double			AverageDifference;
	double			missingPercent;
	double			extraPercent;
} AudioBlocks;

typedef struct AudioSt {
	char		SourceFile[BUFFER_SIZE];
	int			AudioChannels;
	int			role;

	int 		hasSilenceBlock;
	double		floorFreq;
	double		floorAmplitude;

	char 		*Samples;
	long int	SamplesStart;
	double		framerate;
	wav_hdr		header;

	long int	startOffset;
	long int	endOffset;

	MaxMagn		MaxMagnitude;
	double		MinAmplitude;

	double		gridFrequency;
	double		gridAmplitude;
	double		scanrateFrequency;
	double		scanrateAmplitude;
	double		crossFrequency;
	double		crossAmplitude;
	double		SilenceBinSize;

	int			nyquistLimit;
	int			watermarkStatus;

	double		startHz;
	double		endHz;

	double		delayArray[DELAYCOUNT];
	int			delayElemCount;

	double		balance;
	AudioBlocks	clkFrequencies;
	double		originalCLK;
	double		EstimatedSR_CLK;
	int			originalSR_CLK;

	double		EstimatedSR;
	int			originalSR;
	double		originalFrameRate;

	AudioBlocks *Blocks;
}  AudioSignal;

/********************************************************/

typedef struct window_unit_st {
	double		*window;
	long int	frames;
	double		seconds;
	long int	size;
	long int	sizePadding;
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
} FreqDifference;

typedef struct ampl_diff_st {
	double	hertz;
	double	refAmplitude;
	double	diffAmplitude;
} AmplDifference;

typedef struct phase_diff_st {
	double	hertz;
	double	diffPhase;
} PhaseDifference;

typedef struct blk_diff_st {
	FreqDifference *freqMissArray;
	long int		cntFreqBlkDiff;
	long int		cmpFreqBlkDiff;

	AmplDifference *amplDiffArray;
	long int		cntAmplBlkDiff;
	long int		cmpAmplBlkDiff;

	long int		perfectAmplMatch;

	PhaseDifference *phaseDiffArray;
	long int		cntPhaseBlkDiff;
	long int		cmpPhaseBlkDiff;

	int				type;
} BlockDifference;

typedef struct block_diff_st {
	BlockDifference	*BlockDiffArray;

	long int		cntPerfectAmplMatch;
	long int 		cntFreqAudioDiff;
	long int		cntAmplAudioDiff;
	long int		cntPhaseAudioDiff;
	long int		cmpPhaseAudioDiff;

	long int		cntTotalCompared;
	long int		cntTotalAudioDiff;
} AudioDifference;

/********************************************************/

typedef struct parameters_st {
	char			referenceFile[BUFFER_SIZE];
	char			comparisonFile[BUFFER_SIZE];
	char			folderName[BUFFER_SIZE*2];
	char			compareName[BUFFER_SIZE];
	char			profileFile[BUFFER_SIZE];
	char			outputFolder[BUFFER_SIZE];
	char			outputPath[BUFFER_SIZE];
	char			tmpPath[BUFFER_SIZE];
	double			startHz, endHz;
	double			startHzPlot, endHzPlot;
	double			maxDbPlotZC;
	int				showAll;
	int				extendedResults;
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
	int				logScaleTS;
	int				debugSync;
	int				ZeroPad;
	enum normalize	normType;
	int				channelBalance;
	int				showPercent;
	int				noSyncProfile;
	int				noSyncProfileType;
	double			NoSyncTotalFrames;
	int				ignoreFrameRateDiff;
	int				labelNames;

	double			thresholdAmplitudeHiDif;
	double			thresholdMissingHiDif;
	double			thresholdExtraHiDif;

	int				plotDifferences;
	int				plotMissing;
	int				plotSpectrogram;
	int				plotTimeSpectrogram;
	int				plotNoiseFloor;
	int				plotTimeDomain;
	int				plotAllNotes;
	int				plotAllNotesWindowed;
	int				plotTimeDomainHiDiff;
	int				plotPhase;
	double			plotRatio;
	int				averagePlot;
	int				weightedAveragePlot;
	int				drawWindows;
	int				outputCSV;
	int				whiteBG;
	int				smallFile;
	int				syncTolerance;
	double			AmpBarRange;
	int				FullTimeSpectroScale;
	int				hasTimeDomain;
	int				hasSilenceOverRide;
	int				hasAddOnData;
	int				frequencyNormalizationTries;
	double			frequencyNormalizationTolerant;
	int				noiseFloorAutoAdjust;
	int				noiseFloorTooHigh;
	int				noiseFloorBigDifference;
	int				channelWithLowFundamentals;
	double			notVisible;
	int				SRNoMatch;
	int				diffClkNoMatch;
	int				changedCLKFrom;
	double			centsDifferenceCLK;
	double			RefCentsDifferenceSR;
	double			ComCentsDifferenceSR;

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
	int				useExtraData;
	int				compressToBlocks;
	int				drawPerfect;

/* Values only used for clock frequency */
	char		clkName[20];
	int			clkMeasure;
	int			clkBlock;
	int			clkFreq;
	int			clkRatio;

	int			doClkAdjust;
	int			doSamplerateAdjust;

	AudioSignal		*referenceSignal;
	AudioSignal		*comparisonSignal;

#ifdef MDWAVE
	int				maxBlanked;
	int				invert;
	int				chunks;
	int				useCompProfile;
	int				executefft;
#endif

} parameters;


#endif
