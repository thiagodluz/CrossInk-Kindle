#include "RenderWorker.h"

#include <ctime>

#include "compat/Logging.h"

namespace kindle {
RenderWorker::RenderWorker(Handler handler, void* context, SessionHeartbeat* heartbeat)
    : handler_(handler), context_(context), heartbeat_(heartbeat) {
  pthread_condattr_t attributes;
  if (pthread_condattr_init(&attributes) != 0) return;
  if (pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC) == 0)
    conditionReady_ = pthread_cond_init(&condition_, &attributes) == 0;
  pthread_condattr_destroy(&attributes);
}
bool RenderWorker::start() {
  if (started_ || stopping_ || !conditionReady_) return false;
  pthread_attr_t attributes;
  pthread_attr_init(&attributes);
  // Native C++ font/layout calls need more stack than the ESP task shim.
  // This fixed 256-KB stack is bounded and small relative to the PW1's RAM.
  pthread_attr_setstacksize(&attributes, 256 * 1024);
  const int result = pthread_create(&thread_, &attributes, entry, this);
  pthread_attr_destroy(&attributes);
  if (result) {
    LOG_ERR("TASK", "Cannot start render worker: %d", result);
    return false;
  }
  started_ = true;
  return true;
}
bool RenderWorker::enqueue(const UiJob& job) {
  pthread_mutex_lock(&mutex_);
  if (stopping_ || count_ == 64) {
    pthread_mutex_unlock(&mutex_);
    return false;
  }
  queue_[(head_ + count_) % 64] = job;
  ++count_;
  pthread_cond_signal(&condition_);
  pthread_mutex_unlock(&mutex_);
  return true;
}
void RenderWorker::stop() {
  if (!started_) return;
  pthread_mutex_lock(&mutex_);
  stopping_ = true;
  pthread_cond_signal(&condition_);
  pthread_mutex_unlock(&mutex_);
  pthread_join(thread_, nullptr);
  started_ = false;
}
void* RenderWorker::entry(void* context) {
  static_cast<RenderWorker*>(context)->loop();
  return nullptr;
}
void RenderWorker::loop() {
  handler_(UiJob{UiJob::Kind::Start}, context_);
  while (!heartbeat_ || heartbeat_->pulse()) {
    pthread_mutex_lock(&mutex_);
    while (!count_ && !stopping_) {
      timespec until{};
      clock_gettime(CLOCK_MONOTONIC, &until);
      ++until.tv_sec;
      if (pthread_cond_timedwait(&condition_, &mutex_, &until) != 0) break;
    }
    if (stopping_) {
      pthread_mutex_unlock(&mutex_);
      break;
    }
    UiJob job;
    if (count_) {
      job = queue_[head_];
      head_ = (head_ + 1) % 64;
      --count_;
    }
    pthread_mutex_unlock(&mutex_);
    handler_(job, context_);
  }
  handler_(UiJob{UiJob::Kind::Stop}, context_);
}
}  // namespace kindle
