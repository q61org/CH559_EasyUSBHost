#ifndef __RINGBUF_H__
#define __RINGBUF_H__

#include <stdint.h>

#define RINGBUF_SIZE  128
#define RINGBUF_SIZE_MASK  0x7f

struct ringbuf_t {
    uint8_t buf[RINGBUF_SIZE];
    uint8_t ri;
    uint8_t wi;
};
typedef struct ringbuf_t __xdata RingBuf;

void ringbuf_init(RingBuf *rb);
uint8_t ringbuf_write(RingBuf *rb, uint8_t b);
uint8_t ringbuf_available(RingBuf *rb);
uint8_t ringbuf_read(RingBuf *rb, uint8_t *dst);
uint8_t ringbuf_pop(RingBuf *rb);

#endif
