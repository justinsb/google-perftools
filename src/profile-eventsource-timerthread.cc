// This file is included from inside profile-handler.cc

// Support for wall-clock profiling.
// A background thread fires events into all registered threads at regular (wall-clock) intervals.

#include "base/atomicops.h"

class ThreadProfileEventSource : public ProfileEventSource {
public:
  typedef Atomic32 timer_count_t;

  ThreadProfileEventSource(int32 frequency) :
    thread_(0), thread_stop_(false),
        events_enabled_(false), frequency_(frequency) {
  }

  void RegisterThread(int callback_count);
  uint32_t GetTicksSinceLastCall();

  void EnableEvents() {
    events_enabled_ = true;
  }
  void DisableEvents() {
    events_enabled_ = true;
  }

  virtual int GetSignal() { return SIGPROF; }

  void RegisteredCallback(int new_callback_count);
  void UnregisteredCallback(int new_callback_count);

  void Reset();

private:
  SpinLock lock_;

  void StartTimerThread();
  void StopTimerThread();

  pthread_t thread_;
  bool thread_stop_;
  bool events_enabled_;
  int32 frequency_;

  // Keep track of "time"
  static __thread timer_count_t thread_last_tick;
  static timer_count_t current_tick_;

  typedef list<pthread_t> ThreadList;
  typedef ThreadList::iterator ThreadListIterator;
  ThreadList threads_ GUARDED_BY(lock_);

  // Timer thread body
  static void * TimerThreadMain(void * arg);
};


ThreadProfileEventSource::timer_count_t ThreadProfileEventSource::current_tick_ = 0;
__thread ThreadProfileEventSource::timer_count_t ThreadProfileEventSource::thread_last_tick = 0;

void ThreadProfileEventSource::RegisterThread(int callback_count) {
  SpinLockHolder sl(&lock_);
  threads_.push_back(pthread_self());
}

void ThreadProfileEventSource::StartTimerThread() {
  if (thread_) {
    RAW_LOG(FATAL, "Timer already running");
  }

  pthread_attr_t attr;
  if (pthread_attr_init(&attr)) {
    RAW_LOG(FATAL, "Cannot initialize pthread_attr_t");
  }

  int scheduler;
  if (-1 == (scheduler = sched_getscheduler(getpid()))) {
    RAW_LOG(FATAL, "Cannot get current scheduler");
  }

  int max_priority;
  if (-1 == (max_priority = sched_get_priority_max(scheduler))) {
    RAW_LOG(FATAL, "Cannot get max priority");
  }

  sched_param sched;
  sched.__sched_priority = max_priority;
  if (pthread_attr_setschedparam(&attr, &sched)) {
    RAW_LOG(FATAL, "Cannot set timer thread priority");
  }

  thread_stop_ = false;

  pthread_t thread;
  if (pthread_create(&thread, &attr, TimerThreadMain, this)) {
    RAW_LOG(FATAL, "Cannot create timer thread");
  }

  pthread_attr_destroy(&attr);

  thread_ = thread;
}

void * ThreadProfileEventSource::TimerThreadMain(void * arg) {
  ThreadProfileEventSource * instance = (ThreadProfileEventSource*) arg;

  __useconds_t sleep_us = 1000000 / instance->frequency_;

  RAW_CHECK(instance->GetSignal() == SIGPROF, "Registered signal was not SIGPROF");

  while (!instance->thread_stop_) {
    ::base::subtle::Barrier_AtomicIncrement(
        &ThreadProfileEventSource::current_tick_, 1);

    if (instance->events_enabled_) {
      {
        SpinLockHolder sl(&instance->lock_);

        ThreadList& threads = instance->threads_;

        for (ThreadListIterator it = threads.begin(); it != threads.end(); ) {
          int err = pthread_kill(*it, SIGPROF);
          if (err) {
            switch (err) {
            case ESRCH:
              //RAW_LOG(INFO, "Removing finished thread %ld", *it);

              //TODO: Exit when no threads remain??
              it = threads.erase(it);
              continue;

            case EINVAL:
              // Shouldn't happen
              RAW_LOG(WARNING, "Error sending signal: EINVAL");
              break;

            default:
              // _Really_ shouldn't happen
              RAW_LOG(WARNING, "Unknown error sending signal: %d", err);
              break;
            }
          }

          ++it;
        }
      }
    }

    usleep(sleep_us);
  }

  //RAW_LOG(INFO, "TimerThread exiting");

  return 0;
}

void ThreadProfileEventSource::StopTimerThread() {
  if (thread_) {
    thread_stop_ = true;
    if (pthread_join(thread_, NULL)) {
      RAW_LOG(FATAL, "Cannot stop timer thread %d", errno);
    }
    thread_ = 0;
  }
  return;
}

void ThreadProfileEventSource::Reset() {
  StopTimerThread();
}

uint32_t ThreadProfileEventSource::GetTicksSinceLastCall() {
  timer_count_t system_time = ::base::subtle::Acquire_Load(&ThreadProfileEventSource::current_tick_);
  timer_count_t thread_time =  ::base::subtle::Acquire_Load(&thread_last_tick);

  uint32_t ticks;
  if (thread_time != 0) {
    ticks = (system_time > thread_time)
        ? (system_time - thread_time)
        : (thread_time - system_time);
  } else {
    // We (likely) have just started this thread
    ticks = 1;
  }
  //  if (ticks == 0) {
  //     RAW_LOG(WARNING, "SignalHandler ticks == 0 %ld %ld", system_time, thread_time);
  //  } else if (ticks != 1) {
  //     RAW_LOG(WARNING, "SignalHandler ticks != 1 %ld", ticks);
  //  }
  ::base::subtle::Release_Store(&thread_last_tick, system_time);

  return ticks;
}


void ThreadProfileEventSource::RegisteredCallback(int new_callback_count) {
  // Start the timer if timer is shared and this is a first callback.
  if (new_callback_count == 1) {
    StartTimerThread();
  }
}

void ThreadProfileEventSource::UnregisteredCallback(int new_callback_count) {
  if (new_callback_count == 0) {
    StopTimerThread();
  }
}

