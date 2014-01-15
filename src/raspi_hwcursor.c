/*
 * Copyright © 2013 James Hughes jnahughes@googlemail.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "xf86Cursor.h"
#include "cursorstr.h"

#include "raspi_hwcursor.h"


static void ShowCursor(ScrnInfoPtr pScrn)
{
    raspberry_cursor_state_s *state = SUNXI_DISP_HWC(pScrn);

    state->state.enabled = 1;

    set_cursor_position(&state->state);
}

static void HideCursor(ScrnInfoPtr pScrn)
{
   raspberry_cursor_state_s *state = SUNXI_DISP_HWC(pScrn);

   state->state.enabled = 0;

   set_cursor_position(&state->state);
}

static void SetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
   raspberry_cursor_state_s *state = SUNXI_DISP_HWC(pScrn);

   state->state.x = x;
   state->state.y = y;

   set_cursor_position(&state->state);
}

static void SetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
   // Can we support this?
}

static void LoadCursorImage(ScrnInfoPtr pScrn, unsigned char *bits)
{
   raspberry_cursor_state_s *state = SUNXI_DISP_HWC(pScrn);
}



/* Called to turn on the ARGB HW cursor */

static Bool UseHWCursorARGB(ScreenPtr pScreen, CursorPtr pCurs)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];

    raspberry_cursor_state_s *state = SUNXI_DISP_HWC(pScrn);

    /* VC4 supports ARGB cursors up to 64x64 */

    if (pCurs->bits->height <= MAX_ARGB_CURSOR_HEIGHT && pCurs->bits->width <= MAX_ARGB_CURSOR_WIDTH)
    {
         state->state.enabled = 1;
    }
    else
    {
         state->state.enabled = 0;
    }

    set_cursor_position(&state->state);

    return state->state.enabled ? TRUE : FALSE;
}

/* Load an ARGB8888 bitmap to the VC4 for use as cursor*/
static void LoadCursorARGB(ScrnInfoPtr pScrn, CursorPtr pCurs)
{
    raspberry_cursor_state_s *disp = SUNXI_DISP(pScrn);

    int           width  = pCurs->bits->width;
    int           height = pCurs->bits->height;



}

raspberry_cursor_state_s *raspberry_cursor_init(ScreenPtr pScreen)
{
    xf86CursorInfoPtr InfoPtr;

    raspberry_cursor_state_s *state;

    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];

    sunxi_disp_t *disp = SUNXI_DISP(pScrn);

    if (!disp)
        return NULL;

    if (!(InfoPtr = xf86CreateCursorInfoRec()))
    {
        ErrorF("raspberry_cursor_init: xf86CreateCursorInfoRec() failed\n");
        return NULL;
    }

    InfoPtr->ShowCursor = ShowCursor;
    InfoPtr->HideCursor = HideCursor;
    InfoPtr->SetCursorPosition = SetCursorPosition;
    InfoPtr->SetCursorColors = SetCursorColors;
    InfoPtr->LoadCursorImage = LoadCursorImage;
    InfoPtr->MaxWidth = InfoPtr->MaxHeight = 64;
    InfoPtr->Flags = HARDWARE_CURSOR_ARGB;

    InfoPtr->UseHWCursorARGB = UseHWCursorARGB;
    InfoPtr->LoadCursorARGB = LoadCursorARGB;

    if (!xf86InitCursor(pScreen, InfoPtr))
    {
        ErrorF("raspberry_cursor_init: xf86InitCursor(pScreen, InfoPtr) failed\n");
        xf86DestroyCursorInfoRec(InfoPtr);
        return NULL;
    }

    state = calloc(1, sizeof(raspberry_cursor_state_s));
    if (!state)
    {
        ErrorF("raspberry_cursor_init: calloc failed\n");
        xf86DestroyCursorInfoRec(InfoPtr);
        return NULL;
    }

    state->hwcursor = InfoPtr;

    return state;
}

void raspberry_cursor_close(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];

    raspberry_cursor_state_s *state = SUNXI_DISP_HWC(pScrn);

    if (state)
    {
        xf86DestroyCursorInfoRec(state->hwcursor);
    }
}

