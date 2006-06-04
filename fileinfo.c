/*
  Copyright (C) 2006 Andreas Schwarz <andreas@andreas-s.net>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include <stdio.h>
#include <string.h>

#include "fileinfo.h"

enum filetypes get_filetype(char * filename)
{
	char *extension;
	
	extension = strrchr(filename, '.') + 1;
	
	if(strncasecmp(extension, "MP3", 3) == 0) {
		return MP3;
	} else if (strncasecmp(extension, "MP4", 3) == 0 ||
	           strncasecmp(extension, "M4A", 3) == 0 ||
	           strncasecmp(extension, "AAC", 3) == 0) {
		return AAC;
	} else if (strncasecmp(extension, "WAV", 3) == 0) {
		return WAV;
	} else {
		return UNKNOWN;
	}
}

char * get_full_filename(unsigned char * filename)
{
	static char full_filename[14];
	
	full_filename[0] = '/';
	strncpy(full_filename + 1, filename, 12);
	full_filename[13] = '\0';
	
	return full_filename;
}