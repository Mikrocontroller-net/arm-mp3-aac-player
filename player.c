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

/*

This is the controller of the player.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>

#include "player.h"
#include "ff.h"
#include "fileinfo.h"
#include "dac.h"
#include "play_wav.h"
#include "play_mp3.h"
#include "play_aac.h"
#include "control.h"

static unsigned char buf[2048];
//static SONGINFO songinfo;
extern FATFS fs;
extern BYTE fbuff[512];

enum playing_states {START, PLAY, STOP};
static enum playing_states state = STOP;
static enum filetypes infile_type = UNKNOWN;
static WORD bytes_read;
static FIL file;
static SONGLIST songlist;
static SONGINFO songinfo;
static int current_song_index = -1;

char *memstr(char *haystack, char *needle, int size)
{
	char *p;
	char needlesize = strlen(needle);

	for (p = haystack; p <= (haystack-needlesize+size); p++)
	{
		if (memcmp(p, needle, needlesize) == 0)
			return p; /* found */
	}
	return NULL;
}

void next(void) {
  current_song_index++;
  if (current_song_index >= songlist.size) {
  	current_song_index = 0;
  }
  iprintf("selected file: %.12s\n", songlist.list[current_song_index].filename);

  assert(f_open( &file, get_full_filename(songlist.list[current_song_index].filename), FA_OPEN_EXISTING|FA_READ) == FR_OK);

  memset(&songinfo, 0, sizeof(songinfo));
  read_song_info(&file, &songinfo);

  iprintf("title: %s\n", songinfo.title);
  iprintf("artist: %s\n", songinfo.artist);
  iprintf("album: %s\n", songinfo.album);
  iprintf("skipping: %i\n", songinfo.data_start);
  f_lseek(&file, songinfo.data_start);
}

void player_init(void)
{
  dac_init();
  songlist_build(&songlist);
  songlist_sort(&songlist);
  
  for (int i = 0; i < songlist.size; i++)
  {
    read_song_info_for_song(songlist.list[i].filename, &songinfo);
    iprintf("%.12s\n", songinfo.artist);
  }
}

void play(void)
{
	long key0=0, key1=0;
	
	dac_init();
	wav_init(buf, sizeof(buf));
	mp3_init(buf, sizeof(buf));
	aac_init(buf, sizeof(buf));

	//dac_enable_dma();
	
	
	//iprintf("f_open: %i\n", f_open(&file, "/04TUYY~1.MP3", FA_OPEN_EXISTING|FA_READ));
	//infile_type = MP3;
	
	next();
	state = STOP;
	
	//mp3_alloc();
	
	get_key_press( 1<<KEY0 );
	get_key_press( 1<<KEY1 );
	
	while(1)
	{
	
		switch(state) {
		case STOP:
			key0 = get_key_press( 1<<KEY0 );
			key1 = get_key_rpt( 1<<KEY1 ) || get_key_press( 1<<KEY1 );
			
			if (key0) {
				state = START;
			} else if (key1) {
				f_close( &file );
				puts("File closed.");
				next();
				state = STOP;
			}

		break;
		
		case START:
			infile_type = get_filetype(songlist.list[current_song_index].filename);
			if (infile_type != UNKNOWN) {
				//assert(f_open( &file, get_full_filename(fileinfo.fname), FA_OPEN_EXISTING|FA_READ) == FR_OK);
				//puts("File opened.");
				
				mp3_free();
				aac_free();
				dac_reset();
				
				switch(infile_type) {
				case AAC:
					aac_alloc();
					aac_reset();
				break;
				
				case MP4:
					// skip MP4 header
					{
						char buffer[4000];
						char *p;
						long data_offset = -1;
						
						for(int n=0; n<100; n++) {
							assert(f_read(&file, buffer, sizeof(buffer), &bytes_read) == FR_OK);
							p = memstr(buffer, "mdat", sizeof(buffer));
							if(p != NULL) {
								data_offset = (p - buffer) + file.fptr - bytes_read + 4;
								iprintf("found mdat atom data at 0x%lx\n", data_offset);
								break;
							} else {
								// seek backwards
								assert(f_lseek(&file, file.fptr - 4) == FR_OK);
							}
						}
						
						assert(data_offset > 0);
						assert(f_lseek(&file, data_offset) == FR_OK);
					}
					aac_alloc();
					aac_reset();
					aac_setup_raw();
				break;
					
				case MP3:
					mp3_alloc();
					mp3_reset();
				break;
				}
				puts("playing");
				malloc_stats();
				state = PLAY;
			} else {
				puts("unknown file type");
				state = STOP;
			}
		break;
		
		case PLAY:
			key0 = get_key_press( 1<<KEY0 );
			key1 = get_key_rpt( 1<<KEY1 ) || get_key_press( 1<<KEY1 );
			
			switch(infile_type) {
			case MP3:
				if(mp3_process(&file) != 0) {
					next();
					state = START;
				}
			break;
			
			case MP4:
				if (aac_process(&file, 1) != 0) {
				  next();
					state = START;
				}
			break;
			
			case AAC:
				if (aac_process(&file, 0) != 0) {
				  next();
					state = START;
				}
			break;
			
			case WAV:
				if (wav_process(&file) != 0) {
				  next();
					state = START;
				}
			break;
			
			default:
				state = STOP;
			break;
			}
		
			if (key0) {
				state = STOP;
				iprintf("underruns: %u\n", underruns);
			} else if (key1) {
				iprintf("underruns: %u\n", underruns);
				next();
				state = START;
			}
		break;
			
		}
	}
	
}
