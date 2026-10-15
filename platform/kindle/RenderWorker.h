#pragma once
#include <pthread.h>

#include "KindleBackends.h"
#include "SessionHeartbeat.h"

namespace kindle {
struct UiJob {
  enum class Kind { Start, Touch, Tick, Stop } kind = Kind::Tick;
  Touch touch{};
  uint64_t generation = 0;
};
// One joinable owner for UI state, drawing and filesystem actions. Producers
// only enqueue fixed-size jobs; there is no detached thread or per-tap allocation.
class RenderWorker {
 public:
  using Handler = void (*)(const UiJob&, void*);
  RenderWorker(Handler handler, void* context, SessionHeartbeat* heartbeat = nullptr);
  ~RenderWorker() {
    stop();
    if (conditionReady_) pthread_cond_destroy(&condition_);
    pthread_mutex_destroy(&mutex_);
  }
  bool start();
  bool enqueue(const UiJob& job);
  void stop();
  RenderWorker(const RenderWorker&) = delete;
  RenderWorker& operator=(const RenderWorker&) = delete;

 private:
  static void* entry(void* context);
  void loop();
  pthread_t thread_{};
  pthread_mutex_t mutex_ = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t condition_{};
  bool conditionReady_ = false;
  UiJob queue_[64]{};
  size_t head_ = 0, count_ = 0;
  bool started_ = false, stopping_ = false;
  Handler handler_;
  void* context_;
  SessionHeartbeat* heartbeat_;
};
}  // namespace kindle
