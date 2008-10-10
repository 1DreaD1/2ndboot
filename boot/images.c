#include "images.h"
#include "buffers.h"
#include "memory.h"
#include "stdio.h"

struct buffer_handle {
  struct abstract_buffer abstract;
  void *rest;
  addr_t dest;
  uint32_t reserved[3];
};

struct buffer_handle buffers_list[IMG_LAST_TAG+1] = {
  [IMG_LINUX] = {
    .dest = 0x90008000,
  },
  [IMG_INITRAMFS] = {
    .dest = 0x90800000,
  },
  [IMG_MOTFLATTREE] = {
    .dest = 0x91000000,
  },
  [IMG_CMDLINE] = {
    .dest = 0x91100000,
  },
  [IMG_USBFW] = {
    .dest = 0x91110000,
  },
};

struct memory_image *image_find(uint8_t tag, struct memory_image *dest) {
  if (tag > IMG_LAST_TAG) {
    return NULL;
  }
  if (buffers_list[tag].abstract.state == B_STAT_COMPLETED) {
    dest->data = (void*)buffers_list[tag].dest;
    dest->size = (size_t)buffers_list[tag].abstract.size;
    return dest;
  }
  return NULL;
}

void image_complete() {
  int i;
  struct abstract_buffer *ab;

  for (i = 1; i <= IMG_LAST_TAG; ++i) {
    ab = &buffers_list[i].abstract;

    if (ab->state == B_STAT_CREATED) {
    }
    if ((ab->state == B_STAT_COMPLETED) &&
	(ab->attrs & B_ATTR_VERIFY)) {
      if (ab->checksum != crc32(buffers_list[i].dest, (size_t)ab->size)) {
	ab->state = B_STAT_ERROR;
      }
    }
  }
}

void image_dump_stats() {
  int i;
  struct abstract_buffer *ab;

  printf("tag  addr     size\n");
  for (i = 1; i <= IMG_LAST_TAG; ++i) {
    int c;

    if (buffers_list[i].dest == 0) {
      continue;
    }
    ab = &buffers_list[i].abstract;

    switch (ab->state) {
    case B_STAT_NONE:
      c = '-'; break;
    case B_STAT_CREATED:
      c = '*'; break;
    case B_STAT_COMPLETED:
      c = '+'; break;
    case B_STAT_ERROR:
      c = '!'; break;
    default:
      c = '?'; break;
    }
    printf("%04d %08x %08x %c\n", (uint32_t)i, buffers_list[i].dest, ab->size, (char)c);
  }
}