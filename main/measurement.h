#ifndef MEASUREMENT_H
#define MEASUREMENT_H
// ─────────────────────────────────────────────────────────────────────────────
#include <stdint.h>
// ─────────────────────────────────────────────────────────────────────────────
typedef struct __attribute__((packed)) {
    uint32_t timestamp;   // 4 bytes    
    float temperature;    // 4 bytes
    uint8_t dirty_bit;    // 1 byte
} Measurement;
// ─────────────────────────────────────────────────────────────────────────────
// Dirty bit definitions
#define DIRTY_BIT_BUFFER_ONLY 0
#define DIRTY_BIT_IN_FLASH 1
#define DIRTY_BIT_SENT_TO_EDGE 2
// ─────────────────────────────────────────────────────────────────────────────
#endif // MEASUREMENT_H