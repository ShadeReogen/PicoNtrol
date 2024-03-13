// SPDX-License-Identifier: Apache-2.0
// Copyright 2019 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_CIRCULAR_BUFFER_H
#define UNI_CIRCULAR_BUFFER_H

#include <stdint.h>

// UNI_CIRCULAR_BUFFER_SIZE represents how many packets can be queued
// Multiple gamepads could be connected at the same time, each queuing
// multiple packets: Think of 8 gamepads wanted to rumble at the same time.
#define UNI_CIRCULAR_BUFFER_SIZE 32
// UNI_CIRCULAR_BUFFER_DATA_SIZE represents the max size of each packet
#define UNI_CIRCULAR_BUFFER_DATA_SIZE 128

enum {
    UNI_CIRCULAR_BUFFER_ERROR_OK = 0,
    UNI_CIRCULAR_BUFFER_ERROR_BUFFER_FULL,
    UNI_CIRCULAR_BUFFER_ERROR_BUFFER_EMPTY,
    UNI_CIRCULAR_BUFFER_ERROR_BUFFER_TOO_BIG,
};

typedef struct uni_ciruclar_buffer_data_s {
    int16_t cid;
    uint8_t data[UNI_CIRCULAR_BUFFER_DATA_SIZE];
    int data_len;
} uni_circular_buffer_data_t;

typedef struct uni_circular_buffer_s {
    uni_circular_buffer_data_t buffer[UNI_CIRCULAR_BUFFER_SIZE];
    int16_t head_idx;
    int16_t tail_idx;
} uni_circular_buffer_t;

uint8_t uni_circular_buffer_put(uni_circular_buffer_t* b, int16_t cid, const void* data, int len);
uint8_t uni_circular_buffer_get(uni_circular_buffer_t* b, int16_t* cid, void** data, int* len);
uint8_t uni_circular_buffer_is_empty(uni_circular_buffer_t* b);
uint8_t uni_circular_buffer_is_full(uni_circular_buffer_t* b);
void uni_circular_buffer_reset(uni_circular_buffer_t* b);

#endif  // UNI_CIRCULAR_BUFFER_H
