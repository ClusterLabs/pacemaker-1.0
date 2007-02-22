/* 
 * ccmbitmap.c: functions that manipulate bitmaps
 *
 * Copyright (C) 2001 Aatash Patel <aatashp@yahoo.com>
 * Copyright (C) 2001 Dr Xu <lifangxu@yahoo.com>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
/* bitmap.c  */
/*	Routines to manage a bitmap -- an array of bits each of which */
/*	can be either on or off.  Represented as an array of integers. */
#include <lha_internal.h>
#include <ccm.h>

#ifndef TRUE
#	define TRUE 1
#	define FALSE 0
#endif



/* 	Initialize a bitmap with "nitems" bits, so that every bit is clear. */
/*	it can be added somewhere on a list. */
int
bitmap_create(char **map, int numBits)
{ 
	int i, numBytes;

	if (numBits%BitsInByte == 0) {
	  numBytes =  numBits/BitsInByte;
	} else {
	  numBytes =  numBits/BitsInByte+1;
	}
  *map = g_malloc(sizeof(char)*numBytes);
  assert(*map);
  for ( i = 0 ; i < numBytes; i++ )
    (*map)[i] = 0;
  return(numBytes);
}

/* return the number of bytes required to represent numBits */
int
bitmap_size(int numBits)
{ 
  int numBytes;
	if (numBits%BitsInByte == 0) {
	  numBytes =  numBits/BitsInByte;
	} else {
	  numBytes =  numBits/BitsInByte+1;
	}
  	return(numBytes);
}


/*	delete bitmap */
void
bitmap_delete(char *map) {
  g_free(map);
}

/*	mark the which bit as set */
void 
bitmap_mark(int which, char *map, int numBits) {
  assert(which >= 0 && which < numBits);
  map[which / BitsInByte] |= 1 << (which % BitsInByte);
}


/* 	Clear the "which" bit in a bitmap. */
void 
bitmap_clear(int which, char *map, int numBits) {
  assert(which >= 0 && which < numBits);
  map[which / BitsInByte] &= ~(1 << (which % BitsInByte));
}


/* 	Return TRUE if the "which" bit is set. */
int 
bitmap_test(int which, const char *map, int numBits)
{
  assert(which >= 0 && which < numBits);
  if (map[which / BitsInByte] & (1 << (which % BitsInByte)))
    return(TRUE);
  else
    return(FALSE);
}

/* 	Return total number of bits already set  */
int 
bitmap_count(const char *map, int numBits) 
{
  int count, i;

  count = 0;
  for (i = 0; i < numBits; i++)
    if (bitmap_test(i, map, numBits)) 
      count++;
  return count;
}


/* 	Print the contents of the bitmap, for debugging. */
void
bitmap_print(char *map, int numBits, char * comments)
{
  int i;

  fprintf(stderr, "%s\n", comments); 
  for (i = 0; i < numBits; i++)
    if (bitmap_test(i, map, numBits))
      fprintf(stderr, "%d, ", i);
  fprintf(stderr, "\n"); 
}

/*	Reset the bitmap. */
void
bitmap_reset(char *map, int numBits)
{
  int i;

  for(i=0; i<numBits; i++) {
    bitmap_clear(i, map, numBits);
  }
}
