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

#ifndef MDFPROFILE_H
#define MDFPROFILE_H

#include "mdfourier.h"

#define LINE_BUFFER_SIZE	1024
#define PARAM_BUFFER_SIZE	512

#define	PROFILE_VER		2.3

#define readLine(buffer, file) if(fgets(buffer, LINE_BUFFER_SIZE, file) == NULL) { logmsg("Invalid Profile file (File ended prematurely)\n"); return 0; } else { int j = 0; for(j = 0; j < LINE_BUFFER_SIZE; j++) { if(buffer[j] == '\r' || buffer[j] == '\n' || buffer[j] == '\0') { buffer[j] = '\0'; break; } } }
int LoadProfile(parameters *config);
int EndProfileLoad(parameters *config);

void SelectSilenceProfile(parameters *config);

char *getRoleText(AudioSignal *Signal);
int CheckProfileBaseLength(parameters *config);

int MatchVideoFormat(parameters * config, char* format);
void listFormats(parameters * config);

#endif
