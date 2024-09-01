#pragma once

#include <chrono>

// -------- MACRO ---------

#define TIMER_START(name) \
  auto name##_begin = std::chrono::high_resolution_clock::now();

#define TIMER_END(name) \
  auto name##_end = std::chrono::high_resolution_clock::now();

#define TIMER_PRINT(name)                                                  \
  auto name##_elapse = name##_end - name##_begin;                          \
  double name##_ms =                                                       \
      std::chrono::duration_cast<std::chrono::milliseconds>(name##_elapse) \
          .count();                                                        \
  SPDLOG_INFO("[P{}](TIMER) {} need {} ms (or {} s)", rank,                \
              std::string(#name), name##_ms, name##_ms / 1000);

#define COMM_START(name)                                      \
  auto name##_st_start = lctx->GetStats();                    \
  int64_t name##_send_bytes = name##_st_start->sent_bytes;    \
  int64_t name##_send_action = name##_st_start->sent_actions; \
  int64_t name##_recv_bytes = name##_st_start->recv_bytes;    \
  int64_t name##_recv_action = name##_st_start->recv_actions;

#define COMM_END(name)                                                   \
  auto name##_st_end = lctx->GetStats();                                 \
  name##_send_bytes = name##_st_end->sent_bytes - name##_send_bytes;     \
  name##_send_action = name##_st_end->sent_actions - name##_send_action; \
  name##_recv_bytes = name##_st_end->recv_bytes - name##_recv_bytes;     \
  name##_recv_action = name##_st_end->recv_actions - name##_recv_action;

#define COMM_PRINT(name)                                                     \
  SPDLOG_INFO(                                                               \
      "[P{}](COMM) {} send bytes: {} && send actions: {} && recv bytes: {} " \
      "&& "                                                                  \
      "recv actions: {}",                                                    \
      rank, std::string(#name), name##_send_bytes, name##_send_action,       \
      name##_recv_bytes, name##_recv_action);

#define TIMER_N_COMM_START(name) \
  COMM_START(name);              \
  TIMER_START(name);

#define TIMER_N_COMM_END(name) \
  TIMER_END(name);             \
  COMM_END(name);

#define TIMER_N_COMM_PRINT(name) \
  TIMER_PRINT(name);             \
  COMM_PRINT(name);

#define TIMER_N_COMM_END_PRINT(name) \
  TIMER_N_COMM_END(name);            \
  TIMER_N_COMM_PRINT(name);
