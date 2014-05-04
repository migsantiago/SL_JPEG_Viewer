/****************************************************************************
* FreeType font
****************************************************************************/
#ifndef _FTFONTS_
#define _FTFONTS_

int FT_Init ();
void setfontsize (int pixelsize);
void setfontcolour (u8 r, u8 g, u8 b);
void DrawText (int x, int y, char *text);
void ClearScreen ();
void ShowScreen ();
void ShowAction (char *msg);
void WaitPrompt (char *msg);
int ShowSelector ();
int SelectMode ();

#endif
