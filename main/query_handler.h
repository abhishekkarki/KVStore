#ifndef QUERY_HANDLER_H
#define QUERY_HANDLER_H
// ─────────────────────────────────────────────────────────────────────────────
#include <stdint.h>
#include <stdbool.h>
#include "measurement.h"
// ─────────────────────────────────────────────────────────────────────────────
void process_query_message(const char *message);
// ─────────────────────────────────────────────────────────────────────────────
//static int unify_and_respond(uint32_t start_timestamp, uint32_t end_timestamp, const char *resp_topic);
// ─────────────────────────────────────────────────────────────────────────────
void send_measurements_response(Measurement *measurements, int count, const char *response_topic);
void send_error_response_range(uint32_t start_timestamp, uint32_t end_timestamp, const char *response_topic);
// ─────────────────────────────────────────────────────────────────────────────
#endif // QUERY_HANDLER_H