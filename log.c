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

#include "log.h"
#include "mdfourier.h"

int do_log = 0;
char log_file[LOG_NAME_LEN];
FILE *logfile = NULL;

void DisableConsole() { do_log = 2; }
void EnableConsole() { do_log = 1; }
int IsLogEnabled() { return do_log; }

void logmsg(char *fmt, ... )
{
	va_list arguments;

	va_start(arguments, fmt);
	if(do_log != 2)
	{
		vprintf(fmt, arguments);
#if defined (WIN32)
		fflush(stdout);  // output to Front end ASAP
#endif
	}

	if(do_log && logfile)
		vfprintf(logfile, fmt, arguments);

	va_end(arguments);
	return;
}

int setLogName(char *name)
{
	sprintf(log_file, "%s", name);

	remove(log_file);

	logfile = fopen(log_file, "w");
	if(!logfile)
	{
		printf("Could not create log file %s\n", log_file);
		return 0;
	}

	printf("\tLog enabled to file: %s\n", log_file);
	return 1;
}

void endLog()
{
	if(do_log && logfile)
	{
		fclose(logfile);
		logfile = NULL;
	}
}

