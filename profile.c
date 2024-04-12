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

int EndProfileLoad(parameters *config)
{
	logmsg("* Using profile [%s]\n", config->types.Name);	
	if(config->compressToBlocks)
		FlattenProfile(config);

	if(config->doClkAdjust)
	{
		if(config->clkMeasure)
		{
			logmsg(" - Adjusting CLK rates, align to 1hz enabled (Zero padding)\n");
			config->ZeroPad = 1;
		}
		else
		{
			logmsg(" - Ignoring -j since no CLK rates were found in profile\n");
			config->doClkAdjust = 0;  // no current need to, but anyway...
		}
	}

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

int LoadAudioBlockStructure(FILE *file, parameters *config)
{
	int		insideInternal = 0, i = 0, syncCount = 0, lineCount = 7;
	int		hadSilenceOverride = 0, internalCount = 0;
	char	lineBuffer[LINE_BUFFER_SIZE], tmp = '\0';
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

	/* CLK estimation */
	readLine(lineBuffer, file);
	if(sscanf(lineBuffer, "%s %c", config->clkName, &tmp) != 2)
	{
		logmsg("ERROR: Invalid MD Fourier Audio Blocks File (CLK):\n%s\n", lineBuffer);
		fclose(file);
		return 0;
	}
	config->clkMeasure = (tmp == 'y');
	if(config->clkMeasure)
	{
		if(sscanf(lineBuffer, "%*s %*c %d %d %d\n", 
			&config->clkBlock,
			&config->clkFreq,
			&config->clkRatio) != 3)
		{
			logmsg("ERROR: Invalid MD Fourier Audio Blocks File (CLK):\n%s\n", lineBuffer);
			fclose(file);
			return 0;
		}

		if(config->clkBlock <= 0 || config->clkFreq <= 0 || config->clkRatio <= 0)
		{
			logmsg("ERROR: Invalid MD Fourier Audio Blocks File (CLK):\n%s\n", lineBuffer);
			fclose(file);
			return 0;
		}
	}

	/* Stereo Balancing */
	readLine(lineBuffer, file);
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

			// Check for masking extensions, will probably be dforced in profile 2.4. They are optional now
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
	memset(config->types.typeArray, 0, sizeof(AudioBlockType));

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
			logmsg("Unrecognized color \"%s\" aborting\n", 
				config->types.typeArray[t].color);
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

		logmsgFileOnly("%c%s %s %d %d %d %s %c %s | Frames: %ld/%d | Seconds: %g [%g to %g]\n", 
			config->types.typeArray[i].type == TYPE_SKIP ? '\t' : ' ',
			config->types.typeArray[i].typeDisplayName,
			type,
			config->types.typeArray[i].elementCount,
			config->types.typeArray[i].frames,
			config->types.typeArray[i].cutFrames*-1,
			config->types.typeArray[i].color,
			config->types.typeArray[i].channel,
			config->types.typeArray[i].IsaddOnData ? "(ExtraData)" : " ",
			(long int)config->types.typeArray[i].elementCount*config->types.typeArray[i].frames,
			frames,
			seconds, 
			StartSeconds,
			TotalSeconds);

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
					logmsg("ERROR: Block %d is %g seconds. Cannot apply Zero Padding\n", i, seconds);
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
