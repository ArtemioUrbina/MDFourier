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

#include "freq.h"
#include "log.h"

int IsCRTNoise(double freq)
{
	if(freq >= 15620 && freq <= 15710) //Peak around 15697-15698 usualy
		return 1;
	return 0;
}

void CleanAudio(GenesisAudio *Signal)
{
	if(!Signal)
		return;
	for(int n = 0; n < MAX_NOTES+1; n++)
	{
		for(int i = 0; i < MAX_FREQ_COUNT; i++)
		{
			Signal->Notes[n].freq[i].hertz = 0;
			Signal->Notes[n].freq[i].magnitude = 0;
			Signal->Notes[n].freq[i].amplitude = 0;
			Signal->Notes[n].freq[i].phase = 0;
			Signal->Notes[n].freq[i].indexFFT = -1;
			Signal->Notes[n].freq[i].matched = 0;
		}
	}
	Signal->SourceFile[0] = '\0';
	Signal->hasFloor = 0;
	Signal->floorFreq = 0.0;
	Signal->floorAmplitude = 0.0;	
}

void FindFloor(GenesisAudio *Signal, parameters *config)
{
	if(!Signal)
		return;
	
	if(!Signal->hasFloor)
		return;

	for(int i = 0; i < config->MaxFreq; i++)
	{
		if(!IsCRTNoise(Signal->Notes[MAX_NOTES].freq[i].hertz))
		{
			Signal->floorAmplitude = Signal->Notes[MAX_NOTES].freq[i].amplitude;
			Signal->floorFreq = Signal->Notes[MAX_NOTES].freq[i].hertz;
			logmsg("Found 'Silence' block: %g Hz at %0.4f.db\n",
				Signal->floorFreq, Signal->floorAmplitude);
			return;
		}
	}
	Signal->hasFloor = 0;  // revoke it if not found
}

void GlobalNormalize(GenesisAudio *Signal, parameters *config)
{
	double MaxMagnitude = 0;

	// Find global peak
	if(config->normalize == 'g' ||
	  (config->normalize == 'r' && config->relativeMaxMagnitude == 0.0))
	{
		for(int note = 0; note < MAX_NOTES+Signal->hasFloor; note++)
		{
			for(int i = 0; i < config->MaxFreq; i++)
			{
				if(!Signal->Notes[note].freq[i].hertz)
					break;
				if(Signal->Notes[note].freq[i].magnitude > MaxMagnitude)
					MaxMagnitude = Signal->Notes[note].freq[i].magnitude;
			}
		}

		if(config->normalize == 'r')
			config->relativeMaxMagnitude = MaxMagnitude;
	}

	if(config->normalize == 'r' && config->relativeMaxMagnitude != 0.0)
		MaxMagnitude = config->relativeMaxMagnitude;

	//Normalize and calculate Amplitude in dbs
	for(int note = 0; note < MAX_NOTES+Signal->hasFloor; note++)
	{
		for(int i = 0; i < config->MaxFreq; i++)
		{
			if(!Signal->Notes[note].freq[i].hertz)
				break;
			Signal->Notes[note].freq[i].amplitude = 
				20*log10(Signal->Notes[note].freq[i].magnitude / MaxMagnitude);
			Signal->Notes[note].freq[i].magnitude = 
				Signal->Notes[note].freq[i].magnitude * 100.0 / MaxMagnitude;
		}
	}
}

void CleanMatched(GenesisAudio *ReferenceSignal, GenesisAudio *TestSignal, parameters *config)
{
	for(int note = 0; note < MAX_NOTES+ReferenceSignal->hasFloor; note++)
	{
		for(int i = 0; i < config->MaxFreq; i++)
		{
			if(!ReferenceSignal->Notes[note].freq[i].hertz)
				break;
			ReferenceSignal->Notes[note].freq[i].matched = 0;	
		}
	}

	for(int note = 0; note < MAX_NOTES+TestSignal->hasFloor; note++)
	{
		for(int i = 0; i < config->MaxFreq; i++)
		{
			if(!TestSignal->Notes[note].freq[i].hertz)
				break;
			TestSignal->Notes[note].freq[i].matched = 0;
		}
	}
}

void PrintFrequencies(GenesisAudio *Signal, parameters *config)
{
	if(IsLogEnabled())
			DisableConsole();

	for(int note = 0; note < MAX_NOTES+Signal->hasFloor; note++)
	{
		logmsg("==================== %s# %d (%d) ===================\n", GetRange(note), GetSubIndex(note), note);
			if(config->spreadsheet)
				logmsg("Spreadsheet-%s#%d\n", GetRange(note), GetSubIndex(note));

		for(int j = 0; j < config->MaxFreq; j++)
		{
			if(Signal->Notes[note].freq[j].hertz)
			{
				logmsg("Frequency [%2d] %5.4g Hz [Amplitude: %g] [Phase: %g]",
					j, 
					Signal->Notes[note].freq[j].hertz,
					Signal->Notes[note].freq[j].amplitude,
					Signal->Notes[note].freq[j].phase);
				//detect CRT frequency
				if(IsCRTNoise(Signal->Notes[note].freq[j].hertz))
					logmsg(" *** CRT Noise ***");
				logmsg("\n");
			}
	
			if(config->spreadsheet)
			{
				logmsg("Spreadsheet-index-Hz-amplitude, %d, %g, %g\n",
					j, Signal->Notes[note].freq[j].hertz, Signal->Notes[note].freq[j].amplitude);
			}

			if(config->debugVerbose && j == 20)  // this is just for internal quick debugging
				exit(1);
		}
	}

	if(IsLogEnabled())
		EnableConsole();
}

void InsertMessageInBuffer(msgbuff *message, parameters *config)
{
	if(config->justResults)
		return;

	strcat(message->message, message->buffer);
	message->msgPos = strlen(message->message);
	if(message->msgPos > message->msgSize - 512)
	{
		message->message = (char*)realloc(message->message, (message->msgSize+4096)*(sizeof(char)));
		message->msgSize += 4096;
	}
}

void PrintComparedNotes(MaxFreq *ReferenceArray, MaxFreq *ComparedArray, parameters *config, GenesisAudio *Signal)
{
	if(IsLogEnabled())
		DisableConsole();

	// changed Magnitude->amplitude
	for(int j = 0; j < config->MaxFreq; j++)
	{
		if(ReferenceArray->freq[j].hertz)
		{
			int match = 0;

			logmsg("[%5d] Ref: %7g Hz %6.2fdb %6.2f ph [>%3d]", 
						j,
						ReferenceArray->freq[j].hertz,
						ReferenceArray->freq[j].amplitude,
						ReferenceArray->freq[j].phase,
						ReferenceArray->freq[j].matched - 1);

			if(ComparedArray->freq[j].hertz)
				logmsg("\tComp: %7g Hz %6.2fdb %6.2f ph [<%3d]", 
						ComparedArray->freq[j].hertz,
						ComparedArray->freq[j].amplitude,
						ComparedArray->freq[j].phase,
						ComparedArray->freq[j].matched - 1);
			else
				logmsg("\tCompared:\tNULL");
			match = ReferenceArray->freq[j].matched - 1;
			if(match != -1 &&
				ReferenceArray->freq[j].hertz != 
				ComparedArray->freq[match].hertz)
					logmsg("H");
			else
					logmsg(" ");
			if(match != -1 &&
				ReferenceArray->freq[j].amplitude != 
				ComparedArray->freq[match].amplitude)
					logmsg("W");
			logmsg("\n");
		}
	}
	logmsg("\n\n");

	if(IsLogEnabled())
		EnableConsole();
}
