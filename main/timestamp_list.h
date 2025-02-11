#ifndef TIMESTAMP_LIST_H
#define TIMESTAMP_LIST_H
// ─────────────────────────────────────────────────────────────────────────────
#include <stdint.h>
#include <stddef.h>
// ─────────────────────────────────────────────────────────────────────────────
#define TIMESTAMP_LIST_KEY "timestamp_list"
// ─────────────────────────────────────────────────────────────────────────────
void append_timestamp_to_list(uint32_t timestamp);
void remove_timestamps_from_list(size_t count);
void get_timestamps_from_list(size_t count, uint32_t *timestamps, size_t *out_count);
// ─────────────────────────────────────────────────────────────────────────────
#endif // TIMESTAMP_LIST_H