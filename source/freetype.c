/****************************************************************************
* libFreeType
*
* Needed to show the user what's the hell's going on !!
* These functions are generic, and do not contain any Memory Card specific 
* routines.
****************************************************************************/
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "freetype.h"

/*** Globals ***/
FT_Library ftlibrary;
FT_Face face;
FT_GlyphSlot slot;
FT_UInt glyph_index;

static int fonthi, fontlo;

/*** The actual TrueType Font ***/
extern char fontface[];		/*** From fontface.s ***/
extern int fontsize;			/*** From fontface.s ***/

/*** From video subsystem ***/
extern int screenheight;
extern u32 *xfb[2];
extern u32 whichfb;
extern GXRModeObj *vmode;

/*** File listings ***/
extern u8 filelist[1024][1024];
extern u32 maxfile;
#define PAGESIZE 18

/****************************************************************************
* FT_Init
*
* Initialise freetype library
****************************************************************************/
int FT_Init (){

  int err;

  err = FT_Init_FreeType (&ftlibrary);
  if (err)
    return 1;

  err =
    FT_New_Memory_Face (ftlibrary, (FT_Byte *) fontface, fontsize, 0, &face);
  if (err)
    return 1;

  setfontsize (16);
  setfontcolour (0xff, 0xff, 0xff);

  slot = face->glyph;

  return 0;

}

/****************************************************************************
* setfontsize
****************************************************************************/
void setfontsize (int pixelsize){
  int err;

  err = FT_Set_Pixel_Sizes (face, 0, pixelsize);

  if (err)
    printf ("Error setting pixel sizes!");
}

static void DrawCharacter (FT_Bitmap * bmp, FT_Int x, FT_Int y){
  FT_Int i, j, p, q;
  FT_Int x_max = x + bmp->width;
  FT_Int y_max = y + bmp->rows;
  int spos;
  unsigned int pixel;
  int c;

  for (i = x, p = 0; i < x_max; i++, p++)
    {
      for (j = y, q = 0; j < y_max; j++, q++)
	{
	  if (i < 0 || j < 0 || i >= 640 || j >= screenheight)
	    continue;

			/*** Convert pixel position to GC int sizes ***/
	  spos = (j * 320) + (i >> 1);

	  pixel = xfb[whichfb][spos];
	  c = bmp->buffer[q * bmp->width + p];

	  /*** Cool Anti-Aliasing doesn't work too well at hires on GC ***/
	  if (c > 128)
	    {
	      if (i & 1)
		pixel = (pixel & 0xffff0000) | fontlo;
	      else
		pixel = ((pixel & 0xffff) | fonthi);

	      xfb[whichfb][spos] = pixel;
	    }
	}
    }
}

/**
 * DrawText
 *
 * Place the font bitmap on the screen
 */
void DrawText (int x, int y, char *text){
  int px, n;
  int i;
  int err;
  int value, count;

  n = strlen (text);
  if (n == 0)
    return;

	/*** x == -1, auto centre ***/
  if (x == -1)
    {
      value = 0;
      px = 0;
    }
  else
    {
      value = 1;
      px = x;
    }

  for (count = value; count < 2; count++)
    {
		/*** Draw the string ***/
      for (i = 0; i < n; i++)
	{
	  err = FT_Load_Char (face, text[i], FT_LOAD_RENDER);

	  if (err)
	    {
	      printf ("Error %c %d\n", text[i], err);
	      continue;				/*** Skip unprintable characters ***/
	    }

	  if (count)
	    DrawCharacter (&slot->bitmap, px + slot->bitmap_left,
			   y - slot->bitmap_top);

	  px += slot->advance.x >> 6;
	}

      px = (640 - px) >> 1;

    }
}

/**
 * getcolour
 *
 * Simply converts RGB to Y1CbY2Cr format
 *
 * I got this from a pastebin, so thanks to whoever originally wrote it!
 */

static unsigned int getcolour (u8 r1, u8 g1, u8 b1){
  int y1, cb1, cr1, y2, cb2, cr2, cb, cr;
  u8 r2, g2, b2;

  r2 = r1;
  g2 = g1;
  b2 = b1;

  y1 = (299 * r1 + 587 * g1 + 114 * b1) / 1000;
  cb1 = (-16874 * r1 - 33126 * g1 + 50000 * b1 + 12800000) / 100000;
  cr1 = (50000 * r1 - 41869 * g1 - 8131 * b1 + 12800000) / 100000;

  y2 = (299 * r2 + 587 * g2 + 114 * b2) / 1000;
  cb2 = (-16874 * r2 - 33126 * g2 + 50000 * b2 + 12800000) / 100000;
  cr2 = (50000 * r2 - 41869 * g2 - 8131 * b2 + 12800000) / 100000;

  cb = (cb1 + cb2) >> 1;
  cr = (cr1 + cr2) >> 1;

  return ((y1 << 24) | (cb << 16) | (y2 << 8) | cr);
}

/**
 * setfontcolour
 *
 * Uses RGB triple values.
 */
void setfontcolour (u8 r, u8 g, u8 b){
  u32 fontcolour;

  fontcolour = getcolour (r, g, b);
  fonthi = fontcolour & 0xffff0000;
  fontlo = fontcolour & 0xffff;
}

/****************************************************************************
* ClearScreen
****************************************************************************/
void ClearScreen (){
  whichfb ^= 1;
  VIDEO_ClearFrameBuffer (vmode, xfb[whichfb], COLOR_BLACK);
}

/****************************************************************************
* ShowScreen
****************************************************************************/
void ShowScreen (){
  VIDEO_SetNextFramebuffer (xfb[whichfb]);
  VIDEO_Flush ();
  VIDEO_WaitVSync ();
}

/**
 * Show an action in progress
 */
void ShowAction (char *msg){
  int ypos = screenheight >> 1;
  ypos += 16;

  ClearScreen ();
  DrawText (-1, ypos, msg);
  ShowScreen ();
}

/****************************************************************************
* SelectMode
****************************************************************************/
int SelectMode (){
  int ypos;

  ypos = (screenheight >> 1);

  ClearScreen ();
  DrawText (-1, ypos, "Press A for SMB BACKUP mode");
  DrawText (-1, ypos + 20, "Press B for SMB RESTORE mode");
  DrawText (-1, ypos + 40, "Press Y for SD CARD BACKUP mode");
  DrawText (-1, ypos + 60, "Press X for SD CARD RESTORE mode");
  DrawText (-1, ypos + 80, "Press Z for SD/PSO Reload");
  ShowScreen ();

	/*** Clear any pending buttons ***/
  while (PAD_ButtonsHeld (0))
    VIDEO_WaitVSync ();

  for (;;){
     if (PAD_ButtonsHeld (0) & PAD_BUTTON_A){
	    while ((PAD_ButtonsDown (0) & PAD_BUTTON_A)){ VIDEO_WaitVSync (); }
	    return 100;
	 }
     if (PAD_ButtonsHeld (0) & PAD_BUTTON_B){
	    while ((PAD_ButtonsDown (0) & PAD_BUTTON_B)){ VIDEO_WaitVSync (); }	  
        return 200;
	 }
	 if (PAD_ButtonsHeld (0) & PAD_BUTTON_Y){
	    while ((PAD_ButtonsDown (0) & PAD_BUTTON_Y)){ VIDEO_WaitVSync (); }	  
        return 300;
	 }
	 if (PAD_ButtonsHeld (0) & PAD_BUTTON_X){
	    while ((PAD_ButtonsDown (0) & PAD_BUTTON_X)){ VIDEO_WaitVSync (); }	  
        return 400;
	 }
	 if (PAD_ButtonsHeld (0) & PAD_TRIGGER_Z){
	    while ((PAD_ButtonsDown (0) & PAD_TRIGGER_Z)){ VIDEO_WaitVSync (); }	  
        return 500;
	 }
     VIDEO_WaitVSync ();
  }
}

/**
 * Wait for user to press A
 */
void WaitButtonA (){
  while (PAD_ButtonsDown (0) & PAD_BUTTON_A);
  while (!(PAD_ButtonsDown (0) & PAD_BUTTON_A));
}

/**
 * Show a prompt
 */
void WaitPrompt (char *msg){
  int ypos = (screenheight) >> 1;

  ClearScreen ();
  DrawText (-1, ypos, msg);
  ypos += 20;
  DrawText (-1, ypos, "Press A to continue");
  ShowScreen ();
  WaitButtonA ();
}

void DrawLineFast (int startx, int endx, int y, u8 r, u8 g, u8 b){
  int width;
  u32 offset;
  u32 colour;
  int i;

  offset = (y * 320) + (startx >> 1);
  colour = getcolour (r, g, b);
  width = (endx - startx) >> 1;

  for (i = 0; i < width; i++)
    xfb[whichfb][offset++] = colour;
}

static void ShowFiles (int offset, int selection){
  int i, j;
  char text[80];
  int ypos;
  int w;

  ClearScreen ();

  setfontsize (16);

  ypos = (screenheight - (PAGESIZE * 20)) >> 1;
  ypos += 20;

  j = 0;
  for (i = offset; i < (offset + PAGESIZE) && (i < maxfile); i++){
      strcpy (text, filelist[i]);
      if (j == (selection - offset)){
      	  /*** Highlighted text entry ***/
	     for (w = 0; w < 20; w++){
	         DrawLineFast (30, 610, (j * 20) + (ypos - 16) + w, 0x00, 0x00, 0xC0);
         }
         setfontcolour (0xff, 0xff, 0xff);
         DrawText (-1, (j * 20) + ypos, text);
	     setfontcolour (0xc0, 0xc0, 0xc0);
      }
      else{
			/*** Normal entry ***/
	    DrawText (-1, (j * 20) + ypos, text);
	  }
      j++;
  }
  ShowScreen ();
}

/****************************************************************************
* ShowSelector
*
* Let's the user select a file to backup or restore.
* Returns index of file chosen.
****************************************************************************/
int ShowSelector (){
  short p;
  int redraw = 1;
  int quit = 0;
  int offset, selection;

  offset = selection = 0;

  while (quit == 0){
    if (redraw){
	  ShowFiles (offset, selection);
	  redraw = 0;
	}
    p = PAD_ButtonsDown (0);

    if (p & PAD_BUTTON_A){ 
          return selection;
    }
    if (p & PAD_BUTTON_DOWN) {
	   selection++;	   
	   if (selection == maxfile){ 
          selection = offset = 0;
       }	   
	   if ((selection - offset) >= PAGESIZE){ 
          offset += PAGESIZE;
       }	   
	   redraw = 1;
    }
    if (p & PAD_BUTTON_UP){
	  selection--;
	  if (selection < 0){
	      selection = maxfile - 1;
	      offset = selection - PAGESIZE + 1;
	  }
	  if (selection < offset){
	      offset -= PAGESIZE;
	  }
	  if (offset < 0){	    
         offset = 0;
      }
	  redraw = 1;
	}
  }
  return selection;
}
