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

SONGLIST songlist;

#ifdef ARM
// compare two songs to determine the list order
int compar_song(SONGFILE *a, SONGFILE *b) {
  SONGINFO songinfo;
  char str_a[30], str_b[30];
  int offset;
  
  memset(str_a, 0, sizeof(str_a));
  memset(str_b, 0, sizeof(str_b));
  
  memset(&songinfo, 0, sizeof(songinfo));
  read_song_info_for_song(a, &songinfo);
  
  if(strstr(songinfo.artist, "The ") == NULL) {
    offset = 0;
  } else {
    offset = 4;
  }
  strncpy(str_a, songinfo.artist + offset, sizeof(str_a) - offset);
  
  memset(&songinfo, 0, sizeof(songinfo));
  read_song_info_for_song(b, &songinfo);
  
  if(strstr(songinfo.artist, "The ") == NULL) {
    offset = 0;
  } else {
    offset = 4;
  }
  strncpy(str_b, songinfo.artist + offset, sizeof(str_b) - offset);
  
  //iprintf("comparing %s <-> %s: %d\n", str_a, str_b, strncasecmp(str_a, str_b, MIN(sizeof(str_a), sizeof(str_b))));
  
  return strncasecmp(str_a, str_b, MIN(sizeof(str_a), sizeof(str_b)));
}

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

int read_song_info_for_song(SONGFILE *song, SONGINFO *songinfo)
{
  static FIL _file;
  memset(&_file, 0, sizeof(_file));
  assert(f_open(&_file, get_full_filename(song->filename), FA_OPEN_EXISTING|FA_READ) == FR_OK);
  assert(read_song_info(&_file, songinfo) == 0);
  assert(f_close(&_file) == FR_OK);
  return 0;
}
#endif

int read_song_info(FIL *file, SONGINFO *songinfo)
{
	BYTE id3buffer[3000];
	WORD bytes_read;

  // try ID3v2
  #ifdef ARM
	assert(f_read(file, id3buffer, sizeof(id3buffer), &bytes_read) == FR_OK);
	#else
	fread(id3buffer, 1, sizeof(id3buffer), file);
	#endif
	
	memset(songinfo, 0, sizeof(*songinfo));
	
	if (strncmp("ID3", id3buffer, 3) == 0) {
		DWORD tag_size, frame_size;
		BYTE version_major, version_release, extended_header;
		int frame_header_size;
		DWORD i;
		
		tag_size = ((DWORD)id3buffer[6] << 21)|((DWORD)id3buffer[7] << 14)|((WORD)id3buffer[8] << 7)|id3buffer[9];
		songinfo->data_start = tag_size;
		version_major = id3buffer[3];
		version_release = id3buffer[4];
		extended_header = id3buffer[5] & (1<<6);
		//iprintf("found ID3 version 2.%x.%x, length %lu, extended header: %i\n", version_major, version_release, tag_size, extended_header);
		if (version_major >= 3) {
		  frame_header_size = 10;
	  } else {
	    frame_header_size = 6;
	  }
		i = 10;
		// iterate through frames
		while (i < MIN(tag_size, sizeof(id3buffer))) {
			puts(id3buffer + i);
			if (version_major >= 3) {
			  frame_size = ((DWORD)id3buffer[i + 4] << 24)|((DWORD)id3buffer[i + 5] << 16)|((WORD)id3buffer[i + 6] << 8)|id3buffer[i + 7];
			} else {
			  frame_size = ((DWORD)id3buffer[i + 3] << 14)|((WORD)id3buffer[i + 4] << 7)|id3buffer[i + 5];
			}
			iprintf("frame size: %lu\n", frame_size);
			if (strncmp("TT2", id3buffer + i, 3) == 0 || strncmp("TIT2", id3buffer + i, 4) == 0) {
				strncpy(songinfo->title, id3buffer + i + frame_header_size + 1, MIN(frame_size - 1, sizeof(songinfo->title) - 1));
			} else if (strncmp("TP1", id3buffer + i, 3) == 0 || strncmp("TPE1", id3buffer + i, 4) == 0) {
				strncpy(songinfo->artist, id3buffer + i + frame_header_size + 1, MIN(frame_size - 1, sizeof(songinfo->artist) - 1));
			} else if (strncmp("TAL", id3buffer + i, 3) == 0) {
				strncpy(songinfo->album, id3buffer + i + frame_header_size + 1, MIN(frame_size - 1, sizeof(songinfo->album) - 1));
			}
			i += frame_size + frame_header_size;
			
			/*
			doesn't work when frame is too large
			if (sizeof(id3buffer) - i < 500)
			{
			  puts("refilling buffer");
			  memmove(id3buffer, id3buffer + i, sizeof(id3buffer) - i);
			  #ifdef ARM
      	assert(f_read(file, id3buffer + i, sizeof(id3buffer) - i, &bytes_read) == FR_OK);
      	#else
      	fread(id3buffer + i, 1, sizeof(id3buffer) - i, file);
      	#endif
      	i = 0;
			}
			*/
		}
	} else {
		// try ID3v1
		#ifdef ARM
		// TODO: test this. seems to fail with 12BEHI~1.M4A
		assert(f_lseek(file, file->fsize - 128) == FR_OK);
		assert(f_read(file, id3buffer, 128, &bytes_read) == FR_OK);
		#endif
		
		if (strncmp("TAG", id3buffer, 3) == 0) {
			strncpy(songinfo->title, id3buffer + 3, MIN(30, sizeof(songinfo->title) - 1));
			strncpy(songinfo->artist, id3buffer + 3 + 30, MIN(30, sizeof(songinfo->artist) - 1));
			strncpy(songinfo->album, id3buffer + 3 + 60, MIN(30, sizeof(songinfo->album) - 1));
			//iprintf("found ID3 version 1\n");
		}
		
		songinfo->data_start = 0;
	}

	return 0;
}

char * get_full_filename(char * filename)
{
	static char full_filename[14];
	
	full_filename[0] = '/';
	strncpy(full_filename + 1, filename, 12);
	full_filename[13] = '\0';
	
	return full_filename;
}

#ifndef ARM
int main(int argc, char *argv[])
{
	FILE           *fp;
	SONGINFO        songinfo;

  if (argc != 2) {
    puts("usage: program filename");
    return 1;
  }

	memset(&songinfo, 0, sizeof(songinfo));
	fp = fopen(argv[1], "r");
	read_song_info(fp, &songinfo);
	puts(songinfo.title);
	puts(songinfo.artist);
	puts(songinfo.album);

	return 0;
}
#endif
