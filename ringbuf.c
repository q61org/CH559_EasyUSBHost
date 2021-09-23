#include "sdcc_keywords.h"
#include "ringbuf.h"

void ringbuf_init(RingBuf *rb)
{
    rb->ri = rb->wi = 0;
}

uint8_t ringbuf_write(RingBuf *rb, uint8_t b)
{
    if (((rb->wi + 1) & RINGBUF_SIZE_MASK) == rb->ri) {
        // buffer full
        return 0;
    }
    rb->buf[rb->wi++] = b;
    rb->wi &= RINGBUF_SIZE_MASK;
    return 1;
}

uint8_t ringbuf_available(RingBuf *rb)
{
    return (rb->ri != rb->wi);
}

uint8_t ringbuf_read(RingBuf *rb, uint8_t *dst)
{
    if (!ringbuf_available(rb)) return 0;
    *dst = rb->buf[rb->ri++];
    rb->ri &= RINGBUF_SIZE_MASK;
    return 1;
}

uint8_t ringbuf_pop(RingBuf *rb)
{
    uint8_t r;
    if (ringbuf_read(rb, &r) == 0) {
        return 255;
    }
    return r;
}
