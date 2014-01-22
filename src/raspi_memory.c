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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

// Use a page size of 4k
static const int page_size = 4*1024;
static const int alignment = 4*1024;

// Might be a define for this somewhere in the raspi userland headers somewhere?
#define MEMORY_ALLOCATE_FLAG 0x0c

/** map the specified address in to userspace
 *
 * @param base
 * @param size
 *
 * @return pointer to mapped memory, NULL if failed for any reason.
 *
 */
void *map_memory(unsigned int base, unsigned int size)
{
   int fd;
   unsigned int offset = base % page_size;
   void *memory;

   base = base - offset;

   if ((fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0)
   {
      return NULL;
   }

   memory = mmap(0,
                  size,
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED,
                  fd,
                  base);

   if (memory == MAP_FAILED)
   {
      return NULL;
   }

   close(mem_fd);

   return mem + offset;
}

/** Unmap previously mapped memory
 *
 * @param addr
 * @param size
 *
 */
void *unmap_memory(void *addr, unsigned int size)
{
   int s = munmap(addr, size);

   if (s != 0)
   {
      return NULL;
   }
}

/** Alloc memory on the Videocore via mailbox call
 *
 * @param fd
 *
 */
unsigned int memory_alloc(int fd, unsigned int size, unsigned int align, unsigned int flags)
{
   int i=0;
   unsigned int p[32];
   p[i++] = 0; // size. Filled in below
   p[i++] = 0x00000000;

   p[i++] = 0x3000c; // (the tag id)
   p[i++] = 12;      // (size of the buffer)
   p[i++] = 12;      // (size of the data)
   p[i++] = size;    // (num bytes? or pages?)
   p[i++] = align;   //
   p[i++] = flags;   // (MEM_FLAG_L1_NONALLOCATING)

   p[i++] = 0x00000000; // end tag
   p[0] = i*sizeof(*p); // actual size

   set_mailbox_property(fd, p);

   return p[5];
}

unsigned int memory_free(int file_desc, unsigned int handle)
{
   int i=0;
   unsigned int p[32];
   p[i++] = 0; // size
   p[i++] = 0x00000000; // process request

   p[i++] = 0x3000f; // (the tag id)
   p[i++] = 4; // (size of the buffer)
   p[i++] = 4; // (size of the data)
   p[i++] = handle;

   p[i++] = 0x00000000; // end tag
   p[0] = i*sizeof(*p); // actual size

   set_mailbox_property(file_desc, p);

   return p[5];
}

unsigned int memory_lock(int file_desc, unsigned int handle)
{
   int i=0;
   unsigned int p[32];
   p[i++] = 0; // size
   p[i++] = 0x00000000; // process request

   p[i++] = 0x3000d; // (the tag id)
   p[i++] = 4; // (size of the buffer)
   p[i++] = 4; // (size of the data)
   p[i++] = handle;

   p[i++] = 0x00000000; // end tag
   p[0] = i*sizeof(*p); // actual size

   set_mailbox_property(file_desc, p);

   return p[5];
}


VIDEOCORE_MEMORY_H videocore_alloc(int fd, int size)
{
   VIDEOCORE_MEMORY_H mem;

   // allocate memory on GPU, map it ready for use
   mem.handle = memory_alloc(fd, size, alignment, MEMORY_ALLOCATE_FLAG);
   mem.buffer = memory_lock(fd, mem.handle);
   mem.user = map_memory(mem.buffer, size);
   mem.size = size;

   return mem;
}

void videocore_free(int file_desc, VIDEOCORE_MEMORY_H mem)
{
   unmap_memory(mem.user, mem.size);
   memory_unlock(file_desc, mem.handle);
   memory_free(file_desc, mem.handle);
}
