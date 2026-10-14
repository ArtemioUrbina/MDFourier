/* 
 * MDFourier
 * A Fourier Transform analysis tool to compare game console audio
 * http://junkerhq.net/MDFourier/
 *
 * Copyright (C)2019-2026 Artemio Urbina
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

#include "profile.h"
#include "log.h"
#include "plot.h"
#include "freq.h"

int LoadProfile(parameters *config)
{
	FILE 	*file;
	char	lineBuffer[LINE_BUFFER_SIZE];
	char	buffer[PARAM_BUFFER_SIZE];

	if(!config)
		return 0;

	file = fopen(config->profileFile, "r");
	if(!file)
	{
		logmsg("ERROR: Could not load profile configuration file: \"%s\" [errno %d]\n", config->profileFile, errno);
		return 0;
	}
	
	readLine(lineBuffer, file);
	sscanf(lineBuffer, "%s ", buffer);
	if(strcmp(buffer, "MDFourierAudioBlockFile") == 0)
	{
		sscanf(lineBuffer, "%*s %s\n", buffer);
		if(atof(buffer) < PROFILE_VER)
		{
			logmsg("ERROR: Please update your profile files to version %g\n", PROFILE_VER);
			fclose(file);
			return 0;
		}
		if(atof(buffer) > PROFILE_VER)
		{
			logmsg("ERROR: This executable can parse \"MDFourierAudioBlockFile %g\" files only\n", PROFILE_VER);
			fclose(file);
			return 0;
		}
		return(LoadAudioBlockStructure(file, config));
	}

	if(strcmp(buffer, "MDFourierNoSyncProfile") == 0)
	{
		sscanf(lineBuffer, "%*s %s\n", buffer);
		if(atof(buffer) != PROFILE_VER)
		{
			logmsg("ERROR: This executable can parse \"MDFourierNoSyncProfile %g\" files only\n", PROFILE_VER);
			fclose(file);
			return 0;
		}
		return(LoadAudioNoSyncProfile(file, config));
	}

	logmsg("ERROR: Not an MD Fourier Audio Profile File\n");
	fclose(file);

	return 0;
}

void FlattenProfile(parameters *config)
{
	int last = 0, total = 0;

	if(!config)
		return;

	// Match Stereo Balance block
	for(int i = 0; i < config->types.typeCount; i++)
	{
		total += config->types.typeArray[i].elementCount;
		if(total >= config->stereoBalanceBlock && last <= config->stereoBalanceBlock)
		{
			config->stereoBalanceBlock = i;
			break;
		}
		last = total;
	}

	for(int i = 0; i < config->types.typeCount; i++)
	{
		total = config->types.typeArray[i].elementCount * config->types.typeArray[i].frames;
		config->types.typeArray[i].elementCount = 1;
		config->types.typeArray[i].frames = total;
	}

	config->types.regularBlocks = GetActiveAudioBlocks(config);
	config->types.totalBlocks = GetTotalAudioBlocks(config);
	logmsg("Audio Blocks flattened\n");
}

int MatchVideoFormat(parameters* config, char* format)
{
	if (!config)
		return -1;

	for (int s = 0; s < config->types.syncCount; s++)
	{
		if (strcmp(format, config->types.SyncFormat[s].syncName) == 0)
			return s;
	}

	return -1;
}

void listFormats(parameters* config)
{
	if (!config)
		return;

	for (int s = 0; s < config->types.syncCount; s++)
	{
		logmsg("%d/%s", s, config->types.SyncFormat[s].syncName);
		if (s != config->types.syncCount - 1)
			logmsg(", ");
	}

	return;
}

int CheckSyncFormats(parameters *config)
{
	if(config->noSyncProfile && !config->types.syncCount)
		return 1;

	if(config->videoFormatRef < 0 || config->videoFormatRef >= config->types.syncCount)
	{
		logmsg("\tERROR: Invalid format '%d' for Reference, profile defines %d types\n\t[", 
				config->videoFormatRef, config->types.syncCount);
		for(int s = 0; s < config->types.syncCount; s++)
		{
			logmsg("%d:%s", s, config->types.SyncFormat[s].syncName);
			if(s != config->types.syncCount - 1)
				logmsg(", ");
		}
		logmsg("]\n");
		return 0;
	}

	if(config->videoFormatCom < 0|| config->videoFormatCom >= config->types.syncCount)
	{
		logmsg("\tERROR: Invalid format '%d' for Comparison, profile defines %d types:\n\t[", 
				config->videoFormatCom, config->types.syncCount);
		listFormats(config);
		logmsg("]\n");
		return 0;
	}
	return 1;
}

int HasOptionalClocks(parameters *config)
{
	for(int c = 0; c < config->clockCount; c++)
	{
		if(config->clocks[c].optional)
			return 1;
	}
	return 0;
}

int EndProfileLoad(parameters *config)
{
	logmsg("* Using profile [%s]\n", config->types.Name);	
	if(config->compressToBlocks)
		FlattenProfile(config);

	if(config->doClkAdjust)
	{
		if(config->clkMeasure)
		{
			if(ZEROPAD_FACTOR == ZEROPAD_FACTOR_CLK)
				logmsg(" - Adjusting rates, align to 1/%dhz enabled (Zero padding)\n", ZEROPAD_FACTOR);
			else
				logmsg(" - Adjusting rates, align to 1/%dhz & CLK 1/%dhz enabled (Zero padding)\n",
					ZEROPAD_FACTOR, ZEROPAD_FACTOR_CLK);
			config->ZeroPad = 1;
			config->ZeroPadFactor = ZEROPAD_FACTOR;

			if(!ValidateCLKArray(config))
				return 0;
		}
		else
		{
			logmsg(" - Ignoring -j since no CLK rates were found in profile\n");
			config->doClkAdjust = 0;  // no current need to, but anyway...
		}
	}
	else
		if(HasOptionalClocks(config))	// their type lists are used without -j
			if(!ValidateCLKArray(config))
				return 0;

	CheckSilenceOverride(config);
	PrintAudioBlocks(config);
	if(!CheckSyncFormats(config))
		return 0;

	if(!CheckProfileBaseLength(config))
		return 0;

	return 1;
}

int CheckChannel(char *channel, parameters *config)
{
	if(*channel == CHANNEL_MONO)
		return 1;
	if(*channel == CHANNEL_STEREO)
		return 1;
	if(*channel == CHANNEL_PSTEREO)
	{
		config->allowStereoVsMono = 1;
		*channel = CHANNEL_STEREO;
		return 1;
	}
	if(*channel == CHANNEL_NOISE)
		return 1;
	return 0;
}

int IsCLKSeparator(char c)
{
	if(c == ' ' || c == '\t' || c == '\r' || c == '\n')
		return 1;
	return 0;
}

int SkipCLKSeparators(char *lineBuffer, int pos)
{
	while(lineBuffer[pos] != '\0' && IsCLKSeparator(lineBuffer[pos]))
		pos++;
	return pos;
}

int SkipCLKToken(char *lineBuffer, int pos)
{
	while(lineBuffer[pos] != '\0' && !IsCLKSeparator(lineBuffer[pos]))
		pos++;
	return pos;
}

int CreateCLKArray(char *lineBuffer, ClockDef *clock)
{
	/* 	If no blocks are specified, or  '*' found apply to all.
			Otherwise apply to explicitly mentioned blocks */

	int pos = 0, fields = 0, listStart = 0, params = 0, start = 0, count = 0, read = 0;

	clock->blocksAdjust = NULL;
	clock->blkAdjustNum = CLK_ADJUST_ALL; /* apply to all by default */

	/* We start by catching up to the already parsed position */
	/* "%*s %*c %d %lf %lf" */
	pos = SkipCLKSeparators(lineBuffer, pos);
	while(lineBuffer[pos] != '\0' && fields < 5)
	{
		pos = SkipCLKToken(lineBuffer, pos);
		pos = SkipCLKSeparators(lineBuffer, pos);
		fields++;
	}

	if(fields < 5)
		return 0;

	if(lineBuffer[pos] == '\0')
		return 1; /* apply to all */

	listStart = pos;

	/* count parameter list */
	while(lineBuffer[pos] != '\0')
	{
		start = pos;
		pos = SkipCLKToken(lineBuffer, pos);
		if(pos - start == 1 && lineBuffer[start] == '*')
			return 1; /* apply to all */
		params++;
		pos = SkipCLKSeparators(lineBuffer, pos);
	}

	clock->blocksAdjust = (int*)malloc(sizeof(int)*params);
	if(!clock->blocksAdjust)
		return 0;

	pos = listStart;
	while(lineBuffer[pos] != '\0')
	{
		start = pos;
		pos = SkipCLKToken(lineBuffer, pos);
		/* The whole token must be a number */
		if(sscanf(lineBuffer+start, "%d%n", &clock->blocksAdjust[count], &read) != 1 || start + read != pos)
		{
			logmsg("ERROR: Invalid block type in CLK adjust list: %s\n", lineBuffer+start);
			free(clock->blocksAdjust);
			clock->blocksAdjust = NULL;
			return 0;
		}
		count++;
		pos = SkipCLKSeparators(lineBuffer, pos);
	}

	clock->blkAdjustNum = count;
	return 1;
}

int LoadClockLine(char *lineBuffer, int *measured, parameters *config)
{
	char		name[20], type = '\0';
	ClockDef	*clock = NULL;

	*measured = 0;
	if(sscanf(lineBuffer, "%19s %c", name, &type) != 2)
	{
		logmsg("ERROR: Invalid MD Fourier Audio Blocks File (CLK):\n%s\n", lineBuffer);
		return 0;
	}

	if(type == CLKID_DISABLE)
		return 1;

	if(type != CLKID_ENABLE && type != CLKID_OPTIONAL)
	{
		logmsg("ERROR: Clock must be enabled with 'y', 'o' (optional) or 'n':\n%s\n", lineBuffer);
		return 0;
	}

	if(config->clockCount == MAX_CLOCKS)
	{
		logmsg("ERROR: Only %d clocks can be defined in a profile:\n%s\n", MAX_CLOCKS, lineBuffer);
		return 0;
	}

	// file names and labels so they must be unique 
	for(int c = 0; c < config->clockCount; c++)
	{
		if(strcmp(config->clocks[c].name, name) == 0) 
		{
			logmsg("ERROR: Clock name '%s' is defined twice in the profile\n", name);
			return 0;
		}
	}
	
	clock = &config->clocks[config->clockCount];
	memset(clock, 0, sizeof(ClockDef));  
	clock->refID = CLKID_NONE;
	clock->comID = CLKID_NONE;	
	sprintf(clock->name, "%s", name);	
	clock->optional = type == CLKID_OPTIONAL;

	if(sscanf(lineBuffer, "%*s %*c %d %lf %lf", 
		&clock->block,
		&clock->freq,	
		&clock->ratio) != 3)
	{
		logmsg("ERROR: Invalid MD Fourier Audio Blocks File (CLK):\n%s\n", lineBuffer);
		return 0;
	}

	if(clock->block <= 0 || clock->freq <= 0 || clock->ratio <= 0)  
	{
		logmsg("ERROR: Invalid MD Fourier Audio Blocks File (CLK Values):\n%s\n", lineBuffer);
		return 0; 
	}

	if(!CreateCLKArray(lineBuffer, clock))
	{
		logmsg("ERROR: Invalid MD Fourier Audio Blocks File (CLK Block Array):\n%s\n", lineBuffer);
		return 0;		
	}
	
	config->clockCount++;
	config->clkMeasure = 1;
	*measured = 1;
	return 1;
}

int GetClockForType(int type, parameters *config)
{
	int all = -1;

	for(int c = 0; c < config->clockCount; c++)
	{
		if(config->clocks[c].blkAdjustNum == CLK_ADJUST_ALL)
			all = c;
		else
		{
			for(int i = 0; i < config->clocks[c].blkAdjustNum; i++)
			{
				if(config->clocks[c].blocksAdjust[i] == type)
					return c;
			}
		}
	}
	return all;
}

int ValidateCLKArray(parameters *config)
{
	int i = 0, t = 0, clkType = 0, allCount = 0;

	for(int c = 0; c < config->clockCount; c++)
	{
		ClockDef *clock = &config->clocks[c];

		if(clock->blkAdjustNum == CLK_ADJUST_ALL)
		{
			if(clock->optional)
			{
				logmsg("ERROR: Optional clock %s must list its block types, otherwise all would be skipped\n", clock->name);
				return 0;
			}
			allCount++;
			if(config->clockCount == 1)
				logmsg(" - %s clock adjustment applies to all block types\n", clock->name);
			else
				logmsg(" - %s clock adjustment applies to the types other clocks don't list\n", clock->name);
			continue;
		}

		logmsg(" - %s clock adjustment limited to types:", clock->name);
		for(i = 0; i < clock->blkAdjustNum; i++)
		{
			int found = 0;

			for(t = 0; t < config->types.typeCount; t++)
			{
				if(config->types.typeArray[t].type == clock->blocksAdjust[i] &&
					config->types.typeArray[t].type > TYPE_SILENCE)
				{
					logmsg(" %s", config->types.typeArray[t].typeDisplayName);
					found = 1;
					break;
				}
			}
			if(!found)
			{
				logmsg("\nERROR: %s adjust list has type %d, which is not an audio block type in this profile\n",
					clock->name, clock->blocksAdjust[i]);
				return 0;
			}

			// a type can only follow one clock
			for(int o = 0; o < c; o++)
			{
				for(int j = 0; j < config->clocks[o].blkAdjustNum; j++)
				{
					if(config->clocks[o].blocksAdjust[j] == clock->blocksAdjust[i])
					{
						logmsg("\nERROR: Type %d is in the adjust list of both %s and %s clocks\n",
							clock->blocksAdjust[i], config->clocks[o].name, clock->name);
						return 0;
					}
				}
			}
		}
		logmsg("\n");
	}

	if(allCount > 1)
	{
		logmsg("ERROR: Only one clock can adjust all block types, list the types for the others\n");
		return 0;
	}

	// The block used to measure each clock must be adjusted by that clock
	for(int c = 0; c < config->clockCount; c++)
	{
		clkType = GetBlockType(config, config->clocks[c].block);
		if(GetClockForType(clkType, config) != c)
		{
			logmsg("ERROR: %s block %s# %d (type %d) is not adjusted by the %s clock\n",
				config->clocks[c].name,
				GetBlockName(config, config->clocks[c].block),
				GetBlockSubIndex(config, config->clocks[c].block), clkType,
				config->clocks[c].name);
			return 0;
		}
	}
	return 1;
}

int ParseCLKTolerance(char *token, CLKIdentity *clkID)
{
	int		read = 0;
	char	*unit = NULL;

	if(sscanf(token, "%lf%n", &clkID->tolerance, &read) != 1)
		return 0;

	unit = token + read;
	if(strcmp(unit, "ppm") == 0)
		clkID->unit = CLKID_UNIT_PPM;
	else if(strcmp(unit, "c") == 0 || strcmp(unit, "cents") == 0)
		clkID->unit = CLKID_UNIT_CENTS;
	else if(strcmp(unit, "Hz") == 0 || strcmp(unit, "hz") == 0)
		clkID->unit = CLKID_UNIT_HZ;
	else
		return 0;

	if(clkID->tolerance <= 0)
		return 0;
	return 1;
}

void CalculateCLKIdentityRange(CLKIdentity *clkID)
{
	switch(clkID->unit)
	{
		case CLKID_UNIT_PPM:
			clkID->minHz = clkID->nominal*(1.0 - clkID->tolerance/1000000.0);
			clkID->maxHz = clkID->nominal*(1.0 + clkID->tolerance/1000000.0);
			break;
		case CLKID_UNIT_CENTS:
			clkID->minHz = clkID->nominal*pow(2.0, -clkID->tolerance/1200.0);
			clkID->maxHz = clkID->nominal*pow(2.0, clkID->tolerance/1200.0);
			break;
		case CLKID_UNIT_HZ:
		default:
			clkID->minHz = clkID->nominal - clkID->tolerance;
			clkID->maxHz = clkID->nominal + clkID->tolerance;
			break;
	}
}

char *GetCLKToleranceUnit(CLKIdentity *clkID)
{
	switch(clkID->unit)
	{
		case CLKID_UNIT_PPM:
			return "ppm";
		case CLKID_UNIT_CENTS:
			return "cents";
		default:
			return "Hz";
	}
}

/*  [CLKID] N
 *  Name	Nominal	Tolerance(ppm|c|cents|Hz)
 */
int LoadCLKIdentities(char *lineBuffer, FILE *file, ClockDef *clock)
{
	int		i = 0, j = 0, count = 0;
	char	tolerance[PARAM_BUFFER_SIZE];

	if(sscanf(lineBuffer, "[CLKID] %d", &count) != 1 || count <= 0)
	{
		logmsg("ERROR: Invalid Line '%s'\nExpected [CLKID] N\n", lineBuffer);
		return 0;
	}

	if(clock->ids)
	{
		free(clock->ids);
		clock->ids = NULL;
		clock->idCount = 0;
	}

	clock->ids = (CLKIdentity*)malloc(sizeof(CLKIdentity)*count);
	if(!clock->ids)
	{
		logmsg("ERROR: Not enough memory for CLK identities\n");
		return 0;
	}
	memset(clock->ids, 0, sizeof(CLKIdentity)*count);

	for(i = 0; i < count; i++)
	{
		readLine(lineBuffer, file);
		if(sscanf(lineBuffer, "%127s %lf %s",  
			clock->ids[i].name, 
			&clock->ids[i].nominal,
			tolerance) != 3)
		{
			logmsg("ERROR: Invalid CLKID line, expected 'Name Nominal Tolerance(ppm|c|Hz)':\n%s\n", lineBuffer);
			free(clock->ids);
			clock->ids = NULL;
			return 0;
		}

		if(clock->ids[i].nominal <= 0 || !ParseCLKTolerance(tolerance, &clock->ids[i]))
		{
			logmsg("ERROR: Invalid CLKID values, nominal must be > 0 and tolerance end in ppm, c or Hz:\n%s\n", lineBuffer);
			free(clock->ids);
			clock->ids = NULL;
			return 0;
		}

		CleanName(clock->ids[i].name, clock->ids[i].displayName);
		CalculateCLKIdentityRange(&clock->ids[i]);	
	}
	clock->idCount = count;

	/* find closest nominal */
	for(i = 0; i < count; i++)
	{
		for(j = i + 1; j < count; j++)
		{
			if(clock->ids[i].minHz <= clock->ids[j].maxHz &&
				clock->ids[j].minHz <= clock->ids[i].maxHz)
			{
				logmsg(" - WARNING: %s CLKID ranges for '%s' and '%s' overlap\n",
					clock->name, clock->ids[i].displayName, clock->ids[j].displayName);
			}
		}
	}
	return 1;
}

void ReleaseClock(ClockDef *clock)
{
	if(clock->blocksAdjust)
	{
		free(clock->blocksAdjust);
		clock->blocksAdjust = NULL;
	}
	clock->blkAdjustNum = 0;

	if(clock->ids)
	{
		free(clock->ids);
		clock->ids = NULL;
	}
	clock->idCount = 0;
	clock->refID = CLKID_NONE;
	clock->comID = CLKID_NONE;
}

void ReleaseCLKData(parameters *config)
{
	for(int c = 0; c < config->clockCount; c++)
		ReleaseClock(&config->clocks[c]);
	config->clockCount = 0;
	config->clkMeasure = 0;
}

int LoadAudioBlockStructure(FILE *file, parameters *config)
{
	int		insideInternal = 0, i = 0, syncCount = 0, lineCount = 7, clockLines = 0;
	int		hadSilenceOverride = 0, internalCount = 0;
	char	lineBuffer[LINE_BUFFER_SIZE];
	char	buffer[PARAM_BUFFER_SIZE], buffer2[PARAM_BUFFER_SIZE], buffer3[PARAM_BUFFER_SIZE];

	config->noSyncProfile = 0;

	/* Line 2: Profile Name */
	readLine(lineBuffer, file);
	if(sscanf(lineBuffer, "%255[^\n]\n", config->types.Name) != 1)
	{
		logmsg("ERROR: Invalid Name '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}

	/* Line 3: Profile Frame rates numbers */
	readLine(lineBuffer, file);
	if(sscanf(lineBuffer, "[VideoModes] %s\n", buffer) != 1)
	{
		logmsg("ERROR: Invalid Line '%s'\nExpected [VideoModes] N\n", lineBuffer);
		fclose(file);
		return 0;
	}
	config->types.syncCount = atoi(buffer);
	if(!config->types.syncCount)
	{
		logmsg("ERROR: Invalid Sync count\n'%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}
	if(config->types.syncCount > 10)
	{
		logmsg("ERROR: Invalid Sync count\n'%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}
	lineCount += config->types.syncCount;
	/* Lines 4 to 4+config->types.syncCount:sync types*/
	for(i = 0; i < config->types.syncCount; i++)
	{
		readLine(lineBuffer, file);
		if(sscanf(lineBuffer, "%255s %s %s %d %d %d\n", 
								config->types.SyncFormat[i].syncName, buffer2, buffer3,
								&config->types.SyncFormat[i].pulseSyncFreq,
								&config->types.SyncFormat[i].pulseFrameLen,
								&config->types.SyncFormat[i].pulseCount) != 6)
		{
			logmsg("ERROR: Invalid Frame Rate Adjustment\n'%s'\n", lineBuffer);
			fclose(file);
			return 0;
		}
		config->types.SyncFormat[i].MSPerFrame = strtod(buffer2, NULL);
		if(!config->types.SyncFormat[i].MSPerFrame)
		{
			logmsg("ERROR: Invalid MS per frame  Adjustment\n'%s'\n", lineBuffer);
			fclose(file);
			return 0;
		}
		config->types.SyncFormat[i].LineCount = strtod(buffer3, NULL);
		if(config->types.SyncFormat[i].LineCount < 0)
		{
			logmsg("ERROR: Invalid line count Adjustment\n'%s'\n", lineBuffer);
			fclose(file);
			return 0;
		}
	
		if(!config->types.SyncFormat[i].pulseSyncFreq)
		{
			logmsg("ERROR: Invalid Pulse Sync Frequency:\n%s\n", lineBuffer);
			fclose(file);
			return 0;
		}
	
		if(!config->types.SyncFormat[i].pulseFrameLen)
		{
			logmsg("ERROR: Invalid Pulse Length:\n%s\n", lineBuffer);
			fclose(file);
			return 0;
		}
	
		if(!config->types.SyncFormat[i].pulseCount)
		{
			logmsg("ERROR: Invalid Pulse Count value:\n%s\n", lineBuffer);
			fclose(file);
			return 0;
		}
	}

	/* Clocks: CLK lines, each one optionally followed by its [CLKID] , until [MonoBalanceBlock] */
	readLine(lineBuffer, file);
	clockLines = 0;
	while(strncmp(lineBuffer, "[MonoBalanceBlock]", 18) != 0)
	{
		int measured = 0;

		if(!LoadClockLine(lineBuffer, &measured, config))
		{
			logmsg("ERROR: Invalid Clock Line (expected 'Name Enable Block Freq Mult):\n%s\n", lineBuffer);
			fclose(file);
			return 0;
		}
		if(clockLines++)
			lineCount++;

		readLine(lineBuffer, file);
		if(strncmp(lineBuffer, "[CLKID]", 7) == 0)
		{
			ClockDef	ignored, *clock = NULL;

			if(measured)
				clock = &config->clocks[config->clockCount - 1];	
			else
			{
				memset(&ignored, 0, sizeof(ClockDef)); 
				clock = &ignored;
			}

			if(!LoadCLKIdentities(lineBuffer, file, clock))
			{
				fclose(file);
				return 0;
			}
			lineCount += clock->idCount + 1;	

			if(!measured)
			{
				logmsg(" - WARNING: [CLKID] ignored, CLK measurement is disabled for that clock\n");
				ReleaseClock(&ignored);
			}
			readLine(lineBuffer, file);
		}
	}

	/* Stereo Balancing, line was already read above */
	if(sscanf(lineBuffer, "[MonoBalanceBlock] %s\n", buffer) != 1)
	{
		logmsg("ERROR: Invalid Line '%s'\nExpected [MonoBalanceBlock] N\n", lineBuffer);
		fclose(file);
		return 0;
	}
	config->stereoBalanceBlock = atoi(buffer);

	/* Type count */
	readLine(lineBuffer, file);
	if(sscanf(lineBuffer, "[ToneLines] %s\n", buffer) != 1)
	{
		logmsg("ERROR: Invalid Line '%s'\nExpected [ToneLines] N\n", lineBuffer);
		fclose(file);
		return 0;
	}
	config->types.typeCount = atoi(buffer);
	if(!config->types.typeCount)
	{
		logmsg("ERROR: Invalid type count '%s'\n", buffer);
		fclose(file);
		return 0;
	}
	config->types.typeArray = (AudioBlockType*)malloc(sizeof(AudioBlockType)*config->types.typeCount);
	if(!config->types.typeArray)
	{
		logmsg("ERROR: Not enough memory\n");
		fclose(file);
		return 0;
	}
	memset(config->types.typeArray, 0, sizeof(AudioBlockType)*config->types.typeCount);

	/* types */
	for(i = 0; i < config->types.typeCount; i++)
	{
		char 	type = 0;

		readLine(lineBuffer, file);
		if(sscanf(lineBuffer, "%128s ", config->types.typeArray[i].typeName) != 1)
		{
			logmsg("ERROR: Invalid Block Name %s\n", config->types.typeArray[i].typeName);
			fclose(file);
			return 0;
		}
		CleanName(config->types.typeArray[i].typeName, config->types.typeArray[i].typeDisplayName);

		if(sscanf(lineBuffer, "%*s %c ", &type) != 1)
		{
			if(type)
				logmsg("ERROR: Invalid Block Type %c\n", type);
			else
				logmsg("ERROR: Unexpected profile line %d\n%s\n", lineCount, lineBuffer);
			fclose(file);
			return 0;
		}

		switch(type)
		{
			case TYPE_SILENCE_C:
				config->types.typeArray[i].type = TYPE_SILENCE;
				break;
			case TYPE_SYNC_C:
				config->types.typeArray[i].type = TYPE_SYNC;
				if(config->timeDomainSync)
					config->hasTimeDomain++;
				syncCount++;
				break;
			case TYPE_INTERNAL_KNOWN_C:
				config->types.typeArray[i].type = TYPE_INTERNAL_KNOWN;
				break;
			case TYPE_INTERNAL_UNKNOWN_C:
				config->types.typeArray[i].type = TYPE_INTERNAL_UNKNOWN;
				break;
			case TYPE_SKIP_C:
				config->types.typeArray[i].type = TYPE_SKIP;
				break;
			case TYPE_TIMEDOMAIN_C:
				config->types.typeArray[i].type = TYPE_TIMEDOMAIN;
				config->hasTimeDomain++;
				break;
			case TYPE_SILENCE_OVER_C:
				config->types.typeArray[i].type = TYPE_SILENCE_OVERRIDE;
				hadSilenceOverride = 1;
				break;
			case TYPE_WATERMARK_C:
				config->types.typeArray[i].type = TYPE_WATERMARK;
				config->types.useWatermark = 1;
				break;
			default:
				if(sscanf(lineBuffer, "%*s %d ", &config->types.typeArray[i].type) != 1)
				{
					logmsg("ERROR: Invalid MD Fourier Block ID\n%s\n", lineBuffer);
					fclose(file);
					return 0;
				}
				break;
		}
		
		if(config->types.typeArray[i].type == TYPE_INTERNAL_KNOWN ||
			config->types.typeArray[i].type == TYPE_INTERNAL_UNKNOWN)
		{
			if(insideInternal)
			{
				insideInternal = 0;
				internalCount++;
			}
			else
				insideInternal = 1;

			if(sscanf(lineBuffer, "%*s %*s %d %d %20s %c %d %lf\n", 
				&config->types.typeArray[i].elementCount,
				&config->types.typeArray[i].frames,
				&config->types.typeArray[i].color[0],
				&config->types.typeArray[i].channel,
				&config->types.typeArray[i].syncTone,
				&config->types.typeArray[i].syncLen) != 6)
			{
				logmsg("ERROR: Invalid MD Fourier Audio Blocks File (line %d)\n(Display_name, id, element count, frames, color, channel): %s\n", lineCount, lineBuffer);
				fclose(file);
				return 0;
			}
		}
		else if(config->types.typeArray[i].type == TYPE_WATERMARK)
		{
			if(sscanf(lineBuffer, "%*s %*s %d %d %20s %c %d %d %128s\n", 
				&config->types.typeArray[i].elementCount,
				&config->types.typeArray[i].frames,
				&config->types.typeArray[i].color[0],
				&config->types.typeArray[i].channel,
				&config->types.watermarkValidFreq,
				&config->types.watermarkInvalidFreq,
				config->types.watermarkDisplayName) != 7)
			{
				logmsg("ERROR: Invalid MD Fourier Audio Blocks File (line %d)\n(Display_name, id, element count, frames, color, channel, WMValid, WMFail, Name): %s\n", lineCount, lineBuffer);
				fclose(file);
				return 0;
			}

			if(!config->types.watermarkValidFreq || !config->types.watermarkInvalidFreq)
			{
				logmsg("ERROR: Invalid Watermark values: %s\n", lineBuffer);
				return 0;
			}
		}
		else  // All regular lines
		{
			if(sscanf(lineBuffer, "%*s %*s %d %d %d %20s %c\n", 
				&config->types.typeArray[i].elementCount,
				&config->types.typeArray[i].frames,
				&config->types.typeArray[i].cutFrames,
				&config->types.typeArray[i].color[0],
				&config->types.typeArray[i].channel) != 5)
			{
				logmsg("ERROR: Invalid MD Fourier Audio Blocks File (line %d)\n(Element Count, frames, skip, color, channel): %s\n", lineCount, lineBuffer);
				fclose(file);
				return 0;
			}

			if(config->types.typeArray[i].cutFrames != 0 && config->types.typeArray[i].frames - abs(config->types.typeArray[i].cutFrames) <= 0)
			{
				logmsg("ERROR: Invalid MD Fourier Audio Blocks File (line %d): %s, Skip bigger than element\n", lineCount, lineBuffer);
				fclose(file);
				return 0;
			}
			config->types.typeArray[i].cutFrames = abs(config->types.typeArray[i].cutFrames);

			if(!CheckChannel(&config->types.typeArray[i].channel, config))
			{
				logmsg("ERROR: Invalid MD Fourier Audio Blocks File\n(Element Count, frames, skip, color, *channel*): %s\n", lineBuffer);
				fclose(file);
				return 0;
			}

			if(config->types.typeArray[i].channel == CHANNEL_STEREO)
			{
				if(config->types.typeArray[i].type < TYPE_CONTROL &&   // Allow Silence blocks to be stereo
					config->types.typeArray[i].type != TYPE_TIMEDOMAIN &&
					config->types.typeArray[i].type != TYPE_SKIP)
				{
					logmsg("ERROR: Only regular blocks can be analyzed as stereo\n %s\n", lineBuffer);
					fclose(file);
					return 0;
				}
				config->usesStereo = 1;
			}

			// Check for masking extensions, will probably be forced in profile 2.4. They are optional now
			if(sscanf(lineBuffer, "%*s %*s %*d %*d %*d %*s %*c %c\n", 
				&config->types.typeArray[i].maskType) == 1)
			{
				if(config->types.typeArray[i].maskType == ';')
					config->types.typeArray[i].maskType = MASK_NONE;

				if(config->types.typeArray[i].maskType != MASK_USE_WINDOW && config->types.typeArray[i].maskType != MASK_NONE)
				{
					logmsg("ERROR: Mask type can only be 'w' or 'n' for windowed or none (windowed was the original default)\n %s (0x%02X)\n", lineBuffer,
						config->types.typeArray[i].maskType);
					fclose(file);
					return 0;
				}
			}
			else
				config->types.typeArray[i].maskType = MASK_DEFAULT;
		}

		if(!config->types.typeArray[i].elementCount)
		{
			logmsg("ERROR: Element Count must have a value > 0\n%s\n", lineBuffer);
			fclose(file);
			return 0;
		}

		if(!config->types.typeArray[i].frames)
		{
			logmsg("ERROR: Frames must have a value > 0\n%s\n", lineBuffer);
			fclose(file);
			return 0;
		}
	
		if(MatchColor(config->types.typeArray[i].color) == COLOR_NONE)
		{
			logmsg("ERROR: Unrecognized color \"%s\" aborting\n", 
				config->types.typeArray[i].color);
			listColors();
			fclose(file);
			return 0;
		}

		config->types.typeArray[i].IsaddOnData = MatchesPreviousType(i, config->types.typeArray[i].type, config);
		// make silent if duplicate and not silence block
		if(!config->useExtraData && config->types.typeArray[i].IsaddOnData)
		{
			if(config->types.typeArray[i].type != TYPE_SILENCE)
				config->types.typeArray[i].type = TYPE_TIMEDOMAIN;  // TYPE_SKIP
		}
		if(config->useExtraData && config->types.typeArray[i].IsaddOnData)
		{
			int parent = 0;

			config->hasAddOnData ++;

			parent = MatchesExtraDataColor(i, config->types.typeArray[i].type, config);
			if(parent)
			{
				logmsg("Extra Data \"%s\" color %s does not match parent category (%d:%s) color %s. Aborting.\n", 
						config->types.typeArray[i].typeDisplayName,
						config->types.typeArray[i].color,
						config->types.typeArray[parent].type,
						config->types.typeArray[parent].typeDisplayName,
						config->types.typeArray[parent].color);
				fclose(file);
				return 0;
			}
		}

		lineCount++;
	}

	if(insideInternal)
	{
		logmsg("ERROR: Internal sync detection block didn't have a closing section\n");
		return 0;
	}

	if(internalCount > 1 && hadSilenceOverride)
	{
		logmsg("ERROR: More than one Internal sync plus Noise floor override are not supported together\n");
		return 0;
	}

	if(syncCount != 2)
	{
		logmsg("ERROR: There must be two Sync lines (%d found)\n", syncCount);
		return 0;
	}

	config->types.regularBlocks = GetActiveAudioBlocks(config);
	config->types.totalBlocks = GetTotalAudioBlocks(config);
	if(!config->types.totalBlocks)
	{
		logmsg("ERROR: Total Audio Blocks should be at least 1\n");
		return 0;
	}

	fclose(file);

	return 1;
}

int LoadAudioNoSyncProfile(FILE *file, parameters *config)
{
	char	type = '\0';
	char	lineBuffer[LINE_BUFFER_SIZE];
	char	buffer[PARAM_BUFFER_SIZE];

	config->noSyncProfile = 1;
	if(config->plotDifferences)
		config->averagePlot = 1;

	readLine(lineBuffer, file);
	if(sscanf(lineBuffer, "%255[^\n]\n", config->types.Name) != 1)
	{
		logmsg("ERROR: Invalid Name '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}

	readLine(lineBuffer, file);
	if(sscanf(lineBuffer, "%s\n", buffer) != 1)
	{
		logmsg("ERROR: Invalid Reference Frame Rate Adjustment '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}
	config->types.SyncFormat[0].MSPerFrame = strtod(buffer, NULL);
	if(!config->types.SyncFormat[0].MSPerFrame)
	{
		logmsg("ERROR: Invalid Reference Frame Rate Adjustment '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}

	readLine(lineBuffer, file);
	if(sscanf(lineBuffer, "%s\n", buffer) != 1)
	{
		logmsg("ERROR: Invalid Comparison Frame Rate Adjustment '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}
	config->types.SyncFormat[1].MSPerFrame = strtod(buffer, NULL);
	if(!config->types.SyncFormat[1].MSPerFrame)
	{
		logmsg("ERROR: Invalid Comparison Frame Rate Adjustment '%s'\n", lineBuffer);
		fclose(file);
		return 0;
	}

	/* Read type of Sync */

	readLine(lineBuffer, file);
	sscanf(lineBuffer, "%c\n", &type);

	switch(type)
	{
		case NO_SYNC_AUTO_C:
			config->noSyncProfileType = NO_SYNC_AUTO;
			break;
		case NO_SYNC_MANUAL_C:
			config->noSyncProfileType = NO_SYNC_MANUAL;
			break;
		case NO_SYNC_LENGTH_C:
			config->noSyncProfileType = NO_SYNC_LENGTH;
			config->normType = none;
			break;
			break;
		case NO_SYNC_DIGITAL_C:
			config->noSyncProfileType = NO_SYNC_DIGITAL;
			break;
		default:
			logmsg("ERROR: Invalid Free profile type '%c'. Use '%c', '%c', '%c' or '%c'\n", 
				type, NO_SYNC_AUTO_C, NO_SYNC_MANUAL_C, NO_SYNC_LENGTH_C, NO_SYNC_DIGITAL_C);
			fclose(file);
			return 0;
			break;
	}
	/*  Read blocks */
	readLine(lineBuffer, file);
	sscanf(lineBuffer, "%s\n", buffer);
	config->types.typeCount = atoi(buffer);
	if(!config->types.typeCount)
	{
		logmsg("ERROR: Invalid type count:\n'%s'\n", buffer);
		fclose(file);
		return 0;
	}
	config->types.typeArray = (AudioBlockType*)malloc(sizeof(AudioBlockType)*config->types.typeCount);
	if(!config->types.typeArray)
	{
		logmsg("Not enough memory\n");
		fclose(file);
		return 0;
	}
	memset(config->types.typeArray, 0, sizeof(AudioBlockType)*config->types.typeCount);

	for(int t = 0; t < config->types.typeCount; t++)
	{
		char typesc = 0;

		readLine(lineBuffer, file);
		if(sscanf(lineBuffer, "%128s ", config->types.typeArray[t].typeName) != 1)
		{
			logmsg("ERROR: Invalid Block Name\n%s\n", lineBuffer);
			fclose(file);
			return 0;
		}
		CleanName(config->types.typeArray[t].typeName, config->types.typeArray[t].typeDisplayName);

		if(sscanf(lineBuffer, "%*s %c ", &typesc) != 1)
		{
			logmsg("ERROR: Invalid Block Type %s\n", lineBuffer);
			fclose(file);
			return 0;
		}

		switch(typesc)
		{
			case TYPE_SILENCE_C:
				config->types.typeArray[t].type = TYPE_SILENCE;
				break;
			case TYPE_SKIP_C:
				config->types.typeArray[t].type = TYPE_SKIP;
				break;
			case TYPE_TIMEDOMAIN_C:
				config->types.typeArray[t].type = TYPE_TIMEDOMAIN;
				config->hasTimeDomain++;
				break;
			case TYPE_SILENCE_OVER_C:
				config->types.typeArray[t].type = TYPE_SILENCE_OVERRIDE;
				break;
			case TYPE_WATERMARK_C:
				config->types.typeArray[t].type = TYPE_WATERMARK;
				config->types.useWatermark = 1;
				break;
			default:
				if(sscanf(lineBuffer, "%*s %d ", &config->types.typeArray[t].type) != 1)
				{
					logmsg("ERROR: Invalid MD Fourier Block ID %d\n%s", config->types.typeArray[t].type, lineBuffer);
					fclose(file);
					return 0;
				}
				break;
		}
		
		if(config->types.typeArray[t].type == TYPE_WATERMARK)
		{
			if(sscanf(lineBuffer, "%*s %*s %d %d %20s %c %d %d %128s\n", 
				&config->types.typeArray[t].elementCount,
				&config->types.typeArray[t].frames,
				&config->types.typeArray[t].color[0],
				&config->types.typeArray[t].channel,
				&config->types.watermarkValidFreq,
				&config->types.watermarkInvalidFreq,
				config->types.watermarkDisplayName) != 7)
			{
				logmsg("ERROR: Invalid MD Fourier Audio Blocks File (Element Count, frames, color, channel, WMValid, WMFail, Name): %s\n", lineBuffer);
				fclose(file);
				return 0;
			}

			if(!config->types.watermarkValidFreq || !config->types.watermarkInvalidFreq)
			{
				logmsg("ERROR: Invalid Watermark values: %s\n", lineBuffer);
				return 0;
			}
		}
		else
		{
			if(sscanf(lineBuffer, "%*s %*s %d %d %s %c\n", 
				&config->types.typeArray[t].elementCount,
				&config->types.typeArray[t].frames,
				&config->types.typeArray[t].color [0],
				&config->types.typeArray[t].channel) != 4)
			{
				logmsg("ERROR: Invalid MD Fourier Audio Blocks File (Element Count, frames, color, channel): %s\n", lineBuffer);
				fclose(file);
				return 0;
			}
		}

		if(!config->types.typeArray[t].elementCount)
		{
			logmsg("Element Count must have a value > 0\n");
			fclose(file);
			return 0;
		}

		if(!config->types.typeArray[t].frames)
		{
			logmsg("Frames must have a value > 0\n");
			fclose(file);
			return 0;
		}
	
		if(MatchColor(config->types.typeArray[t].color) == COLOR_NONE)
		{
			logmsg("ERROR: Unrecognized color \"%s\" aborting\n", 
				config->types.typeArray[t].color);
			listColors();
			fclose(file);
			return 0;
		}

		config->types.typeArray[t].IsaddOnData = MatchesPreviousType(t, config->types.typeArray[t].type, config);
		// make silent if duplicate and not silence block
		if(!config->useExtraData && config->types.typeArray[t].IsaddOnData)
		{
			if(config->types.typeArray[t].type != TYPE_SILENCE)
				config->types.typeArray[t].type = TYPE_TIMEDOMAIN;  // TYPE_SKIP
		}

		if(config->useExtraData && config->types.typeArray[t].IsaddOnData)
		{
			int parent = 0;

			parent = MatchesExtraDataColor(t, config->types.typeArray[t].type, config);
			if(parent)
			{
				logmsg("Extra Data \"%s\" color %s does not match parent category (%d:%s) color %s. Aborting.\n", 
						config->types.typeArray[t].typeDisplayName,
						config->types.typeArray[t].color,
						config->types.typeArray[parent].type,
						config->types.typeArray[parent].typeDisplayName,
						config->types.typeArray[parent].color);
				fclose(file);
				return 0;
			}
		}

		if(config->useExtraData && config->types.typeArray[t].IsaddOnData)
			config->hasAddOnData ++;
	}

	config->types.regularBlocks = GetActiveAudioBlocks(config);
	config->types.totalBlocks = GetTotalAudioBlocks(config);
	if(!config->types.totalBlocks)
	{
		logmsg("Total Audio Blocks should be at least 1\n");
		return 0;
	}

	/*
	if (config->significantAmplitude != SIGNIFICANT_AMPLITUDE)
	{
		if(config->noSyncProfileType != NO_SYNC_LENGTH)
		{
			config->significantAmplitude = SIGNIFICANT_AMPLITUDE;
			logmsg(" - Free sync profiles ignore -p. \n");
		}
	}
	*/
	
	fclose(file);
	
	return 1;
}

void PrintAudioBlocks(parameters *config)
{
	long int frames = 0, count = 0, MinFrame = 10000, MaxFrame = 0;
	double TotalSeconds = 0, averageFrameAnalisis = 0;

	if(!config)
		return;

	logmsgFileOnly("\n======== PROFILE ========\n");
	for(int i = 0; i < config->types.typeCount; i++)
	{
		char	type[20], t;
		double	seconds = 0, StartSeconds = 0;

		t = GetTypeProfileName(config->types.typeArray[i].type);
		if(t == TYPE_NULLTYPE_C)
			sprintf(type, "%d", config->types.typeArray[i].type);
		else
			sprintf(type, "%c", t);

		StartSeconds = TotalSeconds;
		seconds = FramesToSeconds(config->types.typeArray[i].frames, config->types.SyncFormat[0].MSPerFrame);
		seconds *= config->types.typeArray[i].elementCount;
		TotalSeconds += seconds;
		frames += config->types.typeArray[i].elementCount * config->types.typeArray[i].frames;

		logmsgFileOnly("Frames: %0.5ld/%0.5d | length: %6.3f [%10.3f to %10.3f] | %c%s %s %d %d %d %s %c %s", 
			(long int)config->types.typeArray[i].elementCount*config->types.typeArray[i].frames,
			frames,
			seconds, 
			StartSeconds,
			TotalSeconds,
			config->types.typeArray[i].type == TYPE_SKIP ? '\t' : ' ',
			config->types.typeArray[i].typeName,
			type,
			config->types.typeArray[i].elementCount,
			config->types.typeArray[i].frames,
			config->types.typeArray[i].cutFrames*-1,
			config->types.typeArray[i].color,
			config->types.typeArray[i].channel,
			config->types.typeArray[i].IsaddOnData ? "(ExtraData)" : " ");

		if(config->types.typeArray[i].type == TYPE_WATERMARK)
		{
			logmsgFileOnly("%d %d %s", 
				config->types.watermarkValidFreq,
				config->types.watermarkInvalidFreq,
				config->types.watermarkDisplayName);
		}

		logmsgFileOnly("\n");

		if(config->types.typeArray[i].type != TYPE_SYNC && 
			config->types.typeArray[i].type >= TYPE_SILENCE &&
			config->types.typeArray[i].type != TYPE_SKIP)
		{
			averageFrameAnalisis += config->types.typeArray[i].frames;
			count++;

			if (config->types.typeArray[i].frames < MinFrame)
				MinFrame = config->types.typeArray[i].frames;
			if (config->types.typeArray[i].frames > MaxFrame)
				MaxFrame = config->types.typeArray[i].frames;
		}
	}
	logmsgFileOnly("Total frames: %ld\n", frames);
	if (count)
	{
		double standardDeviation = 0;

		averageFrameAnalisis = averageFrameAnalisis / count;
		count = 0;

		logmsgFileOnly("Average frame length analysis: %g\n", averageFrameAnalisis);
		logmsgFileOnly("Minimum frame count for analysis: %ld\n", MinFrame);
		logmsgFileOnly("Maximum frame count for analysis: %ld\n", MaxFrame);
		for (int i = 0; i < config->types.typeCount; i++)
		{
			if (config->types.typeArray[i].type >= TYPE_SILENCE && config->types.typeArray[i].type != TYPE_SKIP)
			{
				standardDeviation += pow((double)config->types.typeArray[i].frames - averageFrameAnalisis, 2);
				count++;
			}
		}
		if (count)
		{
			standardDeviation = sqrt(standardDeviation / (count - 1));
			logmsgFileOnly("Frame length standard deviation: %g\n", standardDeviation);
		}
	}

	/*
	for (int i = 0; i < config->types.typeCount; i++)
	{
		if (config->types.typeArray[i].type >= TYPE_SILENCE && config->types.typeArray[i].type != TYPE_SKIP)
		{
			char	type[20], t;

			t = GetTypeProfileName(config->types.typeArray[i].type);
			if (t == TYPE_NULLTYPE_C)
				sprintf(type, "%d", config->types.typeArray[i].type);
			else
				sprintf(type, "%c", t);

			logmsgFileOnly("%c%s %s %d %d %d %s %c %s\n",
				config->types.typeArray[i].frames == averageFrameAnalisis ? ' ' : '*',
				config->types.typeArray[i].typeDisplayName,
				type,
				config->types.typeArray[i].elementCount,
				config->types.typeArray[i].frames,
				config->types.typeArray[i].cutFrames * -1,
				config->types.typeArray[i].color,
				config->types.typeArray[i].channel);
		}
	}
	*/
	logmsgFileOnly("================\n");
}

int CheckProfileBaseLength(parameters *config)
{
	long int longest = 0, different = 0;

	if(!config)
		return 0;

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type >= TYPE_CONTROL)
		{
			if(config->types.typeArray[i].frames > longest)
				longest = config->types.typeArray[i].frames;

			if(config->ZeroPad)
			{
				double	seconds = 0;

				seconds = FramesToSeconds(config->types.typeArray[i].frames, config->types.SyncFormat[0].MSPerFrame);
				if(seconds > 1.0)
				{
					logmsg("ERROR: Block %d (%s) is %g seconds. Cannot apply Zero Padding\n",
							i, config->types.typeArray[i].typeDisplayName, seconds);
					return 0;
				}
			}
		}
	}

	if(!longest)
	{
		logmsg("ERROR: Invalid Profile, no valid lengths\n");
		return 0;
	}

	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type >= TYPE_CONTROL)
		{
			if(config->types.typeArray[i].frames != longest)
				different++;
		}
	}

	config->maxBlockFrameCount = longest;

	if(different)
		config->padBlockSizes = 1;

	return 1;
}

// Must be called after sync has been detected
void SelectSilenceProfile(parameters *config)
{
	if(!config)
		return;

	if(!config->hasSilenceOverRide)
		return;

	// Convert silence padding to skip blocks
	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type == TYPE_SILENCE)
			config->types.typeArray[i].type = TYPE_SKIP;
	}

	// Convert Silence Override to silence blocks
	for(int i = 0; i < config->types.typeCount; i++)
	{
		if(config->types.typeArray[i].type == TYPE_SILENCE_OVERRIDE)
			config->types.typeArray[i].type = TYPE_SILENCE;
	}
}

char *getRoleText(AudioSignal *Signal)
{
	if(!Signal)
		return("Invalid Signal");
	if(Signal->role == ROLE_REF)
		return("Reference");
	if(Signal->role == ROLE_COMP)
		return("Comparison");
	return("Unknown Role");
}
