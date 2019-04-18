/* 
 * MDFourier
 * A Fourier Transform analysis tool to compare different 
 * Sega Genesis/Mega Drive audio hardware revisions.
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
 * Compile with: 
 *	  gcc -Wall -std=gnu99 -o mdfourier mdfourier.c -lfftw3 -lm
 */

#ifndef MDFOURIER_FREQ_H
#define MDFOURIER_FREQ_H

#include "cline.h"

typedef struct FrequencySt {
	double hertz;
	double magnitude;
	double amplitude;
	double phase;
	long   indexFFT;
	int    matched;
} Frequency;

typedef struct MaxFreqSt {
	Frequency	freq[MAX_FREQ_COUNT];
	int 		index;
	int 		type;
} MaxFreq;

typedef struct GenesisAudioSt {
	MaxFreq 	Notes[MAX_NOTES+1]; 
	char		SourceFile[1024];
	int 		hasFloor;
	double		floorFreq;
	double		floorAmplitude;
}  GenesisAudio;


// WAV data structures
typedef struct	WAV_HEADER
{
	/* RIFF Chunk Descriptor */
	uint8_t 		RIFF[4];		// RIFF Header Magic header
	uint32_t		ChunkSize;		// RIFF Chunk Size
	uint8_t 		WAVE[4];		// WAVE Header
	/* "fmt" sub-chunk */
	uint8_t 		fmt[4]; 		// FMT header
	uint32_t		Subchunk1Size;	// Size of the fmt chunk
	uint16_t		AudioFormat;	// Audio format 1=PCM,6=mulaw,7=alaw,	  257=IBM Mu-Law, 258=IBM A-Law, 259=ADPCM
	uint16_t		NumOfChan;		// Number of channels 1=Mono 2=Sterio
	uint32_t		SamplesPerSec;	// Sampling Frequency in Hz
	uint32_t		bytesPerSec;	// bytes per second
	uint16_t		blockAlign; 	// 2=16-bit mono, 4=16-bit stereo
	uint16_t		bitsPerSample;	// Number of bits per sample
	/* "data" sub-chunk */
	uint8_t 		Subchunk2ID[4]; // "data"  string
	uint32_t		Subchunk2Size;	// Sampled data length
} wav_hdr;


typedef struct msg_buffer_st {
	char *message;
	char buffer[1024];
	int msgPos;
	int msgSize;
} msgbuff;

void CleanAudio(GenesisAudio *Signal);
void CleanMatched(GenesisAudio *ReferenceSignal, GenesisAudio *TestSignal, parameters *config);
void PrintFrequencies(GenesisAudio *Signal, parameters *config);
void GlobalNormalize(GenesisAudio *Signal, parameters *config);
void FindFloor(GenesisAudio *Signal, parameters *config);
int IsCRTNoise(double freq);

void PrintComparedNotes(MaxFreq *ReferenceArray, MaxFreq *ComparedArray, parameters *config, GenesisAudio *Signal);
void InsertMessageInBuffer(msgbuff *message, parameters *config);

#endif