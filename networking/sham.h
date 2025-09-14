#ifndef SHAM_H
#define SHAM_H

#include <stdint.h>

struct sham_header {
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t flags;
    uint16_t window_size;
};

#define SYN 0x1
#define ACK 0x2
#define FIN 0x4
#define DATA 0x8

#endif