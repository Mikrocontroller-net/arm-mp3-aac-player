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
#include "keys.h"
#include "ir.h"

static unsigned char buf[2048];
//static SONGINFO songinfo;
extern FATFS fs;

enum playing_states {START, PLAY, STOP};
enum user_commands {CMD_START, CMD_STOP, CMD_NEXT};
static enum playing_states state = STOP;
static FIL file;
static SONGLIST songlist;
static SONGINFO songinfo;
static int current_song_index = -1;

enum user_commands get_command(void)
{
  long key0 = get_key_press( 1<<KEY0 );
	long key1 = get_key_rpt( 1<<KEY1 ) || get_key_press( 1<<KEY1 );
  int ir_cmd = ir_get_cmd();
	
	if ((key0 && state == STOP) || ir_cmd == 0x35) {
    return CMD_START;
	} else if ((key0 && state == PLAY) || ir_cmd == 0x36) {
    return CMD_STOP;
	} else if (key1 || ir_cmd == 0x34) {
    return CMD_NEXT;
	}
	
  return -1;
}

void next(void)
{
  current_song_index++;
  if (current_song_index >= songlist.size) {
  	current_song_index = 0;
  }
  iprintf("selected file: %.12s\n", songlist.list[current_song_index].filename);

  memset(&songinfo, 0, sizeof(SONGINFO));
  read_song_info_for_song(&(songlist.list[current_song_index]), &songinfo);
  assert(f_open( &file, get_full_filename(songlist.list[current_song_index].filename), FA_OPEN_EXISTING|FA_READ) == FR_OK);
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
    read_song_info_for_song(&(songlist.list[i]), &songinfo);
    iprintf("%s\n", songinfo.artist);
  }
}

void play(void)
{
  enum user_commands cmd;
	
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
	
  get_command();
	
	while(1)
	{
	
		switch(state) {
		case STOP:
      cmd = get_command();
    
      if (cmd == CMD_START) {
				state = START;
			} else if (cmd == CMD_NEXT) {
				f_close( &file );
				puts("File closed.");
				next();
				state = STOP;
			}

		break;
		
		case START:
			if (songinfo.type != UNKNOWN) {
				//assert(f_open( &file, get_full_filename(fileinfo.fname), FA_OPEN_EXISTING|FA_READ) == FR_OK);
				//puts("File opened.");
				
				mp3_free();
				aac_free();
				dac_reset();
				
				switch(songinfo.type) {
				case AAC:
					aac_alloc();
					aac_reset();
				break;
				
				case MP4:
					aac_alloc();
					aac_reset();
					aac_setup_raw(); // TODO: do this only when the file is first opened
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
      cmd = get_command();
			
			switch(songinfo.type) {
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
		
			if (cmd == CMD_STOP) {
				state = STOP;
				iprintf("underruns: %u\n", underruns);
			} else if (cmd == CMD_NEXT) {
				iprintf("underruns: %u\n", underruns);
				next();
				state = START;
			}
		break;
			
		}
	}
	
}
