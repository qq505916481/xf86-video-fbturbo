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

#include "raspi_mailbox.h"

#ifndef RASPI_CURSOR_H_
#define RASPI_CURSOR_H_

typedef struct
{
   int enabled;
   int x;
   int y;

   int width;
   int height;
   int format; // Not used
   int hotspotx;
   int hotspoty;

   uint32_t foreground_colour;
   uint32_t background_colour;

   xf86CursorInfoPtr InfoPtr;
   int mailbox_fd;

   VIDEOCORE_MEMORY_H transfer_buffer;
   int transfer_buffer_size;

} raspberry_cursor_state_s;

#define MAX_ARGB_CURSOR_HEIGHT 64
#define MAX_ARGB_CURSOR_WIDTH  64

extern raspberry_cursor_state_s *raspberry_cursor_init(ScreenPtr pScreen);
extern void raspberry_cursor_close(ScreenPtr pScreen);

#endif /* RASPI_CURSOR_H_ */
