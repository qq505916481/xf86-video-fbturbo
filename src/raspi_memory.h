 /*
 * Copyright © 2014 James Hughes jnahughes@googlemail.com
 * Based on some code copyright Herman Hermitage
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

#ifndef RASPI_MEMORY_H_
#define RASPI_MEMORY_H_

typedef struct
{
  unsigned int handle;
  unsigned int buffer;
  void *user;
  unsigned int size;
} VIDEOCORE_MEMORY_H;

unsigned memory_alloc(int file_desc, unsigned size, unsigned align, unsigned flags);
unsigned memory_free(int file_desc, unsigned handle);
unsigned memory_lock(int file_desc, unsigned handle);
unsigned memory_unlock(int file_desc, unsigned handle);

VIDEOCORE_MEMORY_H videocore_alloc(int file_desc, int size);
void videocore_free(int file_desc, VIDEOCORE_MEMORY_H mem);

#endif /* RASPI_MEMORY_H_ */
