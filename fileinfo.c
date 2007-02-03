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
#include <assert.h>

#ifdef ARM

#include "ff.h"

#else

#include "fatfs/integer.h"
#define FIL FILE
#define iprintf printf

#endif

#include "fileinfo.h"

#define MIN(x,y) ( ((x) < (y)) ? (x) : (y) )

enum filetypes get_filetype(char * filename)
{
	char *extension;
	
	extension = strrchr(filename, '.') + 1;
	
	if(strncasecmp(extension, "MP3", 3) == 0) {
		return MP3;
	} else if (strncasecmp(extension, "MP4", 3) == 0 ||
	           strncasecmp(extension, "M4A", 3) == 0) {
		return MP4;
	} else if (strncasecmp(extension, "AAC", 3) == 0) {
		return AAC;
	} else if (strncasecmp(extension, "WAV", 3) == 0 ||
	           strncasecmp(extension, "RAW", 3) == 0) {
		return WAV;
	} else {
		return UNKNOWN;
	}
}

int read_song_info(FIL *file, SONGINFO *songinfo)
{
	char id3buffer[1000];
	WORD bytes_read;
	
  // try ID3v2
  #ifdef ARM
	assert(f_read(file, id3buffer, sizeof(id3buffer), &bytes_read) == FR_OK);
	#else
	fread(id3buffer, 1, sizeof(id3buffer), file);
	#endif
	
	if (strncmp("ID3", id3buffer, 3) == 0) {
		DWORD tag_size, frame_size;
		BYTE version_major, version_release, extended_header;
		int i;
		
		tag_size = ((DWORD)id3buffer[6] << 21)|((DWORD)id3buffer[7] << 14)|((WORD)id3buffer[8] << 7)|id3buffer[9];
		songinfo->data_start = tag_size;
		version_major = id3buffer[3];
		version_release = id3buffer[4];
		extended_header = id3buffer[5] & (1<<6);
		iprintf("found ID3 version 2.%x.%x, length %lu, extended header: %i\n", version_major, version_release, tag_size, extended_header);
		i = 10;
		// iterate thorugh frames
		while (i < MIN(tag_size, sizeof(id3buffer))) {
			//puts(id3buffer + i);
			frame_size = ((DWORD)id3buffer[i + 3] << 14)|((WORD)id3buffer[i + 4] << 7)|id3buffer[i + 5];
			if (strncmp("TT2", id3buffer + i, 3) == 0) {
				strncpy(songinfo->title, id3buffer + i + 7, MIN(frame_size - 1, sizeof(songinfo->title) - 1));
			} else if (strncmp("TP1", id3buffer + i, 3) == 0) {
				strncpy(songinfo->artist, id3buffer + i + 7, MIN(frame_size - 1, sizeof(songinfo->artist) - 1));
			} else if (strncmp("TAL", id3buffer + i, 3) == 0) {
				strncpy(songinfo->album, id3buffer + i + 7, MIN(frame_size - 1, sizeof(songinfo->album) - 1));
			}
			i += frame_size + 6;
		}
		//f_lseek(&file, tag_size);
	} else {
		// try ID3v1
		f_lseek(file, file->fsize - 128);
		
		assert(f_read(file, id3buffer, 128, &bytes_read) == FR_OK);
		if (strncmp("TAG", id3buffer, 3) == 0) {
			strncpy(songinfo->title, id3buffer + 3 + 30, MIN(30, sizeof(songinfo->title) - 1));
			strncpy(songinfo->artist, id3buffer + 3 + 30, MIN(30, sizeof(songinfo->artist) - 1));
			strncpy(songinfo->album, id3buffer + 3 + 60, MIN(30, sizeof(songinfo->album) - 1));
			iprintf("found ID3 version 1\n");
		}
		
		songinfo->data_start = 0;
		
		//f_lseek(&file, 0);
	}
	
	return 0;
}

char * get_full_filename(unsigned char * filename)
{
	static char full_filename[14];
	
	full_filename[0] = '/';
	strncpy(full_filename + 1, filename, 12);
	full_filename[13] = '\0';
	
	return full_filename;
}

#ifndef ARM
int main(void)
{
	FILE           *fp;
	SONGINFO        songinfo;

	memset(&songinfo, 0, sizeof(songinfo));
	fp = fopen("/media/Music/Journey/Essential Journey/Good Morning Girl.mp3", "r");
	read_song_info(fp, &songinfo);
	puts(songinfo.title);
	puts(songinfo.artist);
	puts(songinfo.album);

	return 0;
}
#endif