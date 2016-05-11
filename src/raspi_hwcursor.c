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
#include "servermd.h"

#include "sunxi_disp.h"
#include "fbdev_priv.h"
#include "raspi_hwcursor.h"
#include "raspi_mailbox.h"


#define MIN_RASPI_VERSION_NUMBER 1390809622

/* Show/Enable the cursor
 *
 */
static void ShowCursor(ScrnInfoPtr pScrn)
{
    raspberry_cursor_state_s *state = RASPI_DISP_HWC(pScrn);

    state->enabled = 1;

    mailbox_set_cursor_position(state->mailbox_fd, state->enabled, state->x, state->y, 1);
}

/* Hide/Disable the cursor
 *
 */
static void HideCursor(ScrnInfoPtr pScrn)
{
   raspberry_cursor_state_s *state = RASPI_DISP_HWC(pScrn);

   state->enabled = 0;

   mailbox_set_cursor_position(state->mailbox_fd, state->enabled, state->x, state->y, 1);
}

/* Set cursor position on display
 *
 */
static void SetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
   raspberry_cursor_state_s *state = RASPI_DISP_HWC(pScrn);

   state->x = x;
   state->y = y;

   mailbox_set_cursor_position(state->mailbox_fd, state->enabled, state->x, state->y, 1);
}

/* Set the cursor colours.
 */
static void SetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
   raspberry_cursor_state_s *state = RASPI_DISP_HWC(pScrn);

   state->background_colour = bg;
   state->foreground_colour = fg;

   // Ought to regenerate the ARGB cursor here using the new colours...but current version
   // doesn't support this. Something for another day...
}

/* Load a cursor image.
 * Loads the supplied cursor bits to VC4
 */
static void LoadCursorImage(ScrnInfoPtr pScrn, unsigned char *bits)
{
   raspberry_cursor_state_s *state = RASPI_DISP_HWC(pScrn);
   int size = state->realised_width * state->realised_height * 4;

   memcpy(state->transfer_buffer.user, bits, size);

   // Our cursor needs a minimum width/height of 16 each
   mailbox_set_cursor_info(state->mailbox_fd, state->realised_width,
                           state->realised_height, 0,
                           state->transfer_buffer.buffer,
                           state->hotspotx, state->hotspotx);
}

/* Called to generate a ARGB cursor from the X definition passed in
 *
 * Incoming data is 1bpp, with the rows padded to BITMAP_SCANLINE_PAD bits.
 *
 */
static unsigned char* RealiseCursor(xf86CursorInfoPtr info, CursorPtr pCurs)
{
    raspberry_cursor_state_s *state = RASPI_DISP_HWC(info->pScrn);
    char *mem;
    // Round up our width/height to 16 as minimum requirements for VC cursor
    int dest_width = pCurs->bits->width < 16 ? 16 : pCurs->bits->width;
    int dest_height = pCurs->bits->height < 16 ? 16 : pCurs->bits->height;

    // Also round up to an even number, I suspect the VC HW cursor dispmanx code requires this also.
    dest_width = (dest_width + 1) & ~1;
    dest_height = (dest_height + 1) & ~1;

    int dest_size = dest_width * dest_height * 4; // 4 bpp;
    int dest_pitch = dest_width; // this is in uint32_t's so no need pump up by bpp

    mem = calloc(1, dest_size);

    if (!mem)
        return NULL;

    // Passed in colours are 16 bit device independent values. We want 8 bit components in one uint32_t
    state->foreground_colour = (pCurs->foreRed & 0xFF00) << 8 | (pCurs->foreGreen & 0xFF00) |
                                 (pCurs->foreBlue & 0xff00) >> 8;
    state->background_colour = (pCurs->backRed & 0xFF00) << 8 | (pCurs->backGreen & 0xFF00) |
                                 (pCurs->backBlue & 0xff00) >> 8;

    if (pCurs->bits->argb)
    {
        memcpy(mem, pCurs->bits->argb, dest_size);
    }
    else
    {
        int x,y,bit;
        typedef uint8_t PIX_TYPE;
        #define PIX_TYPE_SIZE (sizeof(PIX_TYPE) * 8)

        PIX_TYPE  *src = (PIX_TYPE *)pCurs->bits->source;
        PIX_TYPE  *mask =  (PIX_TYPE *)pCurs->bits->mask;
        PIX_TYPE  *current_src, *current_mask;

        uint32_t *dst, pixel;

        // Pitch may not be the width.
        // Pad up to the BITMAP_SCANLINE_PAD to give pixels, then work out the number of PIX types in the width
        // at 1bpp
        int src_pitch = ( (pCurs->bits->width +(BITMAP_SCANLINE_PAD - 1)) & ~(BITMAP_SCANLINE_PAD - 1)) / PIX_TYPE_SIZE;

        dst = (uint32_t*)mem;

        // For every scanline in source
        for (y=0;y<pCurs->bits->height;y++)
        {
            uint32_t *dst_p = dst;
            current_src = src;
            current_mask = mask;

           // For each PIX in scanline
            for (x=0;x<pCurs->bits->width;x+=PIX_TYPE_SIZE)
            {
                // For each bit in the incoming PIX, @ 1bits per pixel
                for (bit=0;bit<PIX_TYPE_SIZE;bit++)
                {
                    pixel = ((*current_src >> bit) & 0x01) ? state->foreground_colour : state->background_colour;

                    pixel |= ((*current_mask >> bit) & 0x01) ? 0xff000000 : 0;

                    *dst_p++ = pixel;
                }

               current_src++;
               current_mask++;
           }

           src += src_pitch;
           mask += src_pitch;
           dst += dest_pitch;
        }

        state->realised_height = dest_height;
        state->realised_width = dest_width;
        state->hotspotx = 0;
        state->hotspoty = 0; // pCurs->bits->height;
    }

    return mem;
}

/* Called to turn on the ARGB HW cursor
 *
 */
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

    mailbox_set_cursor_position(state->mailbox_fd, state->enabled, state->x, state->y, 1);

    return state->enabled ? TRUE : FALSE;
}

/* Load an ARGB8888 bitmap to the VC4 for use as cursor
 *
 */
static void LoadCursorARGB(ScrnInfoPtr pScrn, CursorPtr pCurs)
{
    raspberry_cursor_state_s *state = RASPI_DISP_HWC(pScrn);
    int copy_size;

    state->width  = pCurs->bits->width;
    state->height = pCurs->bits->height;
    state->format = 0;

    // It appears that the hotspot is already compensated for by X, so we dont need to pass it on to VC4.
    state->hotspotx = 0;
    state->hotspoty = 0;

    // Clear our transfer buffer up front
    memset(state->transfer_buffer.user, 0, state->transfer_buffer_size);

    // Copy cursor pixels to our VC memory
    copy_size = min(state->width * state->height * 4, state->transfer_buffer_size) ; // 4 bytes/pixel

    memcpy(state->transfer_buffer.user, pCurs->bits->argb, copy_size);

    mailbox_set_cursor_info(state->mailbox_fd, state->width, state->height,
                state->format, state->transfer_buffer.buffer,
                state->hotspotx, state->hotspoty);
}


/* Initialise the Raspberry Pi HW cursor system
 *
 */
raspberry_cursor_state_s *raspberry_cursor_init(ScreenPtr pScreen)
{
    xf86CursorInfoPtr InfoPtr;
    raspberry_cursor_state_s *state;
    int fd;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    unsigned int version;
    VIDEOCORE_MEMORY_H mem;
    int alloc_size, dummy;

    fd = mailbox_init();

    if (fd == 0)
    {
       xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "raspberry_cursor_init: Failed to initialise mailbox\n");
       return NULL;
    }

    // Get the firmware number to ensure we have cursor support.
    version = mailbox_get_version(fd);

    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "raspberry_cursor_init: Detected firmware version %d\n", version);

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
    InfoPtr->RealizeCursor = RealiseCursor;

    InfoPtr->MaxWidth = MAX_ARGB_CURSOR_WIDTH;
    InfoPtr->MaxHeight = MAX_ARGB_CURSOR_HEIGHT;
    InfoPtr->Flags = HARDWARE_CURSOR_ARGB | HARDWARE_CURSOR_UPDATE_UNHIDDEN ;

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

    state->hotspotx = 0;
    state->hotspoty = 0;

    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "raspberry_cursor_init: Complete\n");

    return state;
}

/* Close down the Raspberry Pi cursor system and release any resources
 *
 */
void raspberry_cursor_close(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];

    raspberry_cursor_state_s *state = RASPI_DISP_HWC(pScrn);

    if (state)
    {
        // Get rid of cursor from display
        mailbox_set_cursor_position(state->mailbox_fd, 0, state->x, state->y, 1);

        mailbox_videocore_free(state->mailbox_fd, state->transfer_buffer);
        xf86DestroyCursorInfoRec(state->InfoPtr);
        mailbox_deinit(state->mailbox_fd);
    }
}

