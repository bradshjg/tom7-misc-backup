/* FCE Ultra - NES/Famicom Emulator
*
* Copyright notice for this file:
*  Copyright (C) 2002 Xodnizel
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef WIN32
#include <stdint.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <zlib.h>

#include "types.h"
#include "video.h"
#include "fceu.h"
#include "file.h"
#include "utils/memory.h"
#include "utils/crc32.h"
#include "state.h"
#include "palette.h"
#include "input.h"
#include "vsuni.h"
#include "driver.h"

uint8 *XBuf=NULL;
uint8 *XBackBuf=NULL;
int ClipSidesOffset=0;	//Used to move displayed messages when Clips left and right sides is checked
static uint8 *xbsave=NULL;

//for input display
extern int input_display;
extern uint32 cur_input_display;

bool oldInputDisplay = false;

void FCEU_KillVirtualVideo(void)
{
	//mbg merge TODO 7/17/06 temporarily removed
	//if(xbsave)
	//{
	// free(xbsave);
	// xbsave=0;
	//}
	//if(XBuf)
	//{
	//UnmapViewOfFile(XBuf);
	//CloseHandle(mapXBuf);
	//mapXBuf=NULL;
	//}
	//if(XBackBuf)
	//{
	// free(XBackBuf);
	// XBackBuf=0;
	//}
}

/**
* Return: Flag that indicates whether the function was succesful or not.
*
* TODO: This function is Windows-only. It should probably be moved.
**/
int FCEU_InitVirtualVideo(void)
{
	if(!XBuf)		/* Some driver code may allocate XBuf externally. */
		/* 256 bytes per scanline, * 240 scanline maximum, +16 for alignment,
		*/

		if(!(XBuf= (uint8*) (FCEU_malloc(256 * 256 + 16))) ||
			!(XBackBuf= (uint8*) (FCEU_malloc(256 * 256 + 16))))
		{
			return 0;
		}

		xbsave = XBuf;

		if( sizeof(uint8*) == 4 )
		{
			uintptr_t m = (uintptr_t)XBuf;
			m = ( 8 - m) & 7;
			XBuf+=m;
		}

		memset(XBuf,128,256*256); //*240);
		memset(XBackBuf,128,256*256);

		return 1;
}
