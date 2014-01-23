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
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#include "xf86.h"
#include "xf86Cursor.h"
#include "cursorstr.h"

#include "sunxi_disp.h"
#include "fbdev_priv.h"
#include "raspi_hwcursor.h"
#include "raspi_memory.h"

// device parameters
#define MAILBOX_DEVICE_FILENAME "/dev/vc4mail"
#define MAJOR 100
#define MINOR 0
#define IOCTL_MBOX_PROPERTY   _IOWR(MAJOR, MINOR, char *)

#define MIN_RASPI_VERSION_NUMBER 1000


int set_mailbox_property(int file_desc, void *buf)
{
   int retval = ioctl(file_desc, IOCTL_MBOX_PROPERTY, buf);

   if (retval < 0)
   {
      printf("ioctl_set_msg failed:%d\n", retval);
   }
   return retval;
}

static unsigned int set_cursor_position(raspberry_cursor_state_s *state)
{
   int i=0;
   unsigned p[32];
   p[i++] = 0; // size
   p[i++] = 0x00000000; // process request
   p[i++] = 0x00008011; // set cursor state
   p[i++] = 12; // buffer size
   p[i++] = 12; // data size

   p[i++] = state->enabled;
   p[i++] = state->x;
   p[i++] = state->y;

   p[i++] = 0x00000000; // end tag
   p[0] = i*sizeof *p; // actual size

   set_mailbox_property(state->mailbox_fd, p);
   return p[5];
}

static unsigned int set_cursor_info(raspberry_cursor_state_s *state)
{
   int i=0;
   unsigned int p[32];
   p[i++] = 0; // size
   p[i++] = 0x00000000; // process request
   p[i++] = 0x00008010; // set cursor state
   p[i++] = 24; // buffer size
   p[i++] = 24; // data size

   p[i++] = state->width;
   p[i++] = state->height;
   p[i++] = state->format;
   p[i++] = (int)state->transfer_buffer.buffer;           // ptr to VC memory buffer
   p[i++] = state->hotspotx;
   p[i++] = state->hotspoty;

   p[i++] = 0x00000000; // end tag
   p[0] = i*sizeof(*p); // actual size

   set_mailbox_property(state->mailbox_fd, p);
   return p[5];

}

static void ShowCursor(ScrnInfoPtr pScrn)
{
    raspberry_cursor_state_s *state = RASPI_DISP_HWC(pScrn);

    state->enabled = 1;

    set_cursor_position(state);
}

static void HideCursor(ScrnInfoPtr pScrn)
{
   raspberry_cursor_state_s *state = RASPI_DISP_HWC(pScrn);

   state->enabled = 0;

   set_cursor_position(state);
}

static void SetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
   raspberry_cursor_state_s *state = RASPI_DISP_HWC(pScrn);

   state->x = x;
   state->y = y;

   set_cursor_position(state);
}

static void SetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
   // Can we support this?
}

static void LoadCursorImage(ScrnInfoPtr pScrn, unsigned char *bits)
{
   raspberry_cursor_state_s *state = RASPI_DISP_HWC(pScrn);
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

    set_cursor_position(state);

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

    set_cursor_info(state);
}

static unsigned get_version(int file_desc)
{
   int i=0;
   unsigned p[32];
   p[i++] = 0; // size
   p[i++] = 0x00000000; // process request

   p[i++] = 0x00000001; // get firmware version
   p[i++] = 0x00000004; // buffer size
   p[i++] = 0x00000000; // request size
   p[i++] = 0x00000000; // value buffer

   p[i++] = 0x00000000; // end tag
   p[0] = i*sizeof *p; // actual size

   set_mailbox_property(file_desc, p);
   return p[5];
}


raspberry_cursor_state_s *raspberry_cursor_init(ScreenPtr pScreen)
{
    xf86CursorInfoPtr InfoPtr;
    raspberry_cursor_state_s *state;
    int fd;
    struct stat stat_buf;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    unsigned int version;
    VIDEOCORE_MEMORY_H mem;
    int alloc_size;


    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "raspberry_cursor_init: Entered\n");

    // See if we have a device node, if not create one.
    if (stat(MAILBOX_DEVICE_FILENAME, &stat_buf) == -1)
    {
       // No node so attempt to create one.
       // Character device, readable by all
       if (mknod(MAILBOX_DEVICE_FILENAME, S_IFCHR | S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH, makedev(MAJOR, MINOR)) == -1)
          xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "raspberry_cursor_init: failed to create device node\n");
    }

    // First check to see if we have the mailbox char device
    fd = open(MAILBOX_DEVICE_FILENAME, 0);
    if (fd < 0)
    {
       xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "raspberry_cursor_init: no mailbox found\n");
       return NULL;
    }

    // Get the firmware number to ensure we have cursor support.
    version = get_version(fd);
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
    state->transfer_buffer = videocore_alloc(fd, state->transfer_buffer_size);

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
        videocore_free(state->mailbox_fd, state->transfer_buffer);
        xf86DestroyCursorInfoRec(state->InfoPtr);
        close(state->mailbox_fd);
    }
}

