/*
 * Copyright © 2014 James Hughes jnahughes@googlemail.com
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

#include <stdio.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>


#include "xf86.h"
#include "xf86Cursor.h"
#include "cursorstr.h"

#include "sunxi_disp.h"
#include "fbdev_priv.h"
#include "raspi_hwcursor.h"
#include "raspi_mailbox.h"


#define MIN_RASPI_VERSION_NUMBER 1000



static void ShowCursor(ScrnInfoPtr pScrn)
{
    raspberry_cursor_state_s *state = RASPI_DISP_HWC(pScrn);

    state->enabled = 1;

    mailbox_set_cursor_position(state->mailbox_fd, state->enabled, state->x, state->y);
}

static void HideCursor(ScrnInfoPtr pScrn)
{
   raspberry_cursor_state_s *state = RASPI_DISP_HWC(pScrn);

   state->enabled = 0;

   mailbox_set_cursor_position(state->mailbox_fd, state->enabled, state->x, state->y);
}

static void SetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
   raspberry_cursor_state_s *state = RASPI_DISP_HWC(pScrn);

   state->x = x;
   state->y = y;

   mailbox_set_cursor_position(state->mailbox_fd, state->enabled, state->x, state->y);
}

static void SetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
   // we only support the ARGB cursor on Raspi.
}

static void LoadCursorImage(ScrnInfoPtr pScrn, unsigned char *bits)
{
   // we only support the ARGB cursor on Raspi.
}



/* Called to turn on the ARGB HW cursor */

static Bool UseHWCursorARGB(ScreenPtr pScreen, CursorPtr pCurs)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];

    raspberry_cursor_state_s *state = RASPI_DISP_HWC(pScrn);

    /* VC4 supports ARGB cursors up to 64x64 */

    if (pCurs->bits->height <= MAX_ARGB_CURSOR_HEIGHT && pCurs->bits->width <= MAX_ARGB_CURSOR_WIDTH)
    {
         state->enabled = 1;
    }
    else
    {
         state->enabled = 0;
    }

    mailbox_set_cursor_position(state->mailbox_fd, state->enabled, state->x, state->y);

    return state->enabled ? TRUE : FALSE;
}

/* Load an ARGB8888 bitmap to the VC4 for use as cursor*/
static void LoadCursorARGB(ScrnInfoPtr pScrn, CursorPtr pCurs)
{
    raspberry_cursor_state_s *state = RASPI_DISP_HWC(pScrn);
    int copy_size;

    state->width  = pCurs->bits->width;
    state->height = pCurs->bits->height;
    state->format = 0;

    // It appears that the hotspot is already compensated for by X, so we dont need to pass it on.
    state->hotspotx = 0; // pCurs->bits->yhot;
    state->hotspoty = 0; // pCurs->bits->yhot;

    // Clear our transfer buffer up front
    memset(state->transfer_buffer.user, 0, state->transfer_buffer_size);

    // Copy cursor pixels to our VC memory
    copy_size = min(state->width * state->height * 4, state->transfer_buffer_size) ; // 4 bytes/pixel

    memcpy(state->transfer_buffer.user, pCurs->bits->argb, copy_size);

    mailbox_set_cursor_info(state->mailbox_fd, state->width, state->height, state->format, (void*)state->transfer_buffer.buffer, state->hotspotx, state->hotspoty);
}


raspberry_cursor_state_s *raspberry_cursor_init(ScreenPtr pScreen)
{
    xf86CursorInfoPtr InfoPtr;
    raspberry_cursor_state_s *state;
    int fd;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    unsigned int version;
    VIDEOCORE_MEMORY_H mem;
    int alloc_size;

    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "raspberry_cursor_init: Entered\n");

    fd = mailbox_init();

    if (fd == 0)
    {
       xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "raspberry_cursor_init: Failed to initialise mailbox\n");
       return NULL;
    }

    // Get the firmware number to ensure we have cursor support.
    version = mailbox_get_version(fd);

    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "raspberry_cursor_init: Detected firmware version %d)\n", version);

    if ( version < MIN_RASPI_VERSION_NUMBER)
    {
       xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "raspberry_cursor_init: No cursor support present in this firmware\n");
       return NULL;
    }

    if (!(InfoPtr = xf86CreateCursorInfoRec()))
    {
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "raspberry_cursor_init: xf86CreateCursorInfoRec() failed\n");
        return NULL;
    }

    InfoPtr->ShowCursor = ShowCursor;
    InfoPtr->HideCursor = HideCursor;
    InfoPtr->SetCursorPosition = SetCursorPosition;
    InfoPtr->SetCursorColors = SetCursorColors;
    InfoPtr->LoadCursorImage = LoadCursorImage;

    InfoPtr->MaxWidth = MAX_ARGB_CURSOR_WIDTH;
    InfoPtr->MaxHeight = MAX_ARGB_CURSOR_HEIGHT;
    InfoPtr->Flags = HARDWARE_CURSOR_ARGB;

    InfoPtr->UseHWCursorARGB = UseHWCursorARGB;
    InfoPtr->LoadCursorARGB = LoadCursorARGB;

    if (!xf86InitCursor(pScreen, InfoPtr))
    {
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "raspberry_cursor_init: xf86InitCursor(pScreen, InfoPtr) failed\n");
        xf86DestroyCursorInfoRec(InfoPtr);
        return NULL;
    }

    state = calloc(1, sizeof(raspberry_cursor_state_s));
    if (!state)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "raspberry_cursor_init: calloc failed\n");
        xf86DestroyCursorInfoRec(InfoPtr);
        return NULL;
    }

    // Get some videocore memory for pixel buffer when transferring cursor image to GPU
    // Allocate the max size we will need. Its not a huge amount anyway.
    state->transfer_buffer_size = MAX_ARGB_CURSOR_HEIGHT * MAX_ARGB_CURSOR_WIDTH * 4;// 4 bytes/pixel
    state->transfer_buffer = mailbox_videocore_alloc(fd, state->transfer_buffer_size);

    state->InfoPtr = InfoPtr;
    state->mailbox_fd = fd;

    return state;
}

void raspberry_cursor_close(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];

    raspberry_cursor_state_s *state = RASPI_DISP_HWC(pScrn);

    if (state)
    {
        // Get rid of cursor
        mailbox_set_cursor_position(state->mailbox_fd, 0, state->x, state->y);

        mailbox_videocore_free(state->mailbox_fd, state->transfer_buffer);
        xf86DestroyCursorInfoRec(state->InfoPtr);
        mailbox_deinit(state->mailbox_fd);
    }
}

