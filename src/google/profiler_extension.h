#ifndef PROFILER_EXTENSION_H_
#define PROFILER_EXTENSION_H_

#include <signal.h>
#include <stdint.h>

// This header should be included to declare a custom profiler event source
// Currently this can only be done from C++

class ProfileHandler;

// The ProfileEventSource is a strategy pattern for producing events we want to correlate to code
// Currently: setitimer events for CPU profiling, interval events for wall-clock profiling
// We could in future have a strategy that uses hardware performance events (e.g. cache misses)
// It might even be possible to have usercode generated events (e.g. hashtable rehashes)
class ProfileEventSource {
public:
  ProfileEventSource() {}

  virtual ~ProfileEventSource() {
  }

  // Registers the current thread with the profile handler. On systems which
  // have a separate interval timer for each thread, this function starts the
  // timer for the current thread.
  //
  // The function also attempts to determine whether or not timers are shared by
  // all threads in the process.  (With LinuxThreads, and with NPTL on some
  // Linux kernel versions, each thread has separate timers.)
  //
  // Prior to determining whether timers are shared, this function will
  // unconditionally start the timer.  However, if this function determines
  // that timers are shared, then it will stop the timer if no callbacks are
  // currently registered.
  virtual void RegisterThread(int callback_count) = 0;

  // Called after a callback is registered / unregistered.
  // The underlying event source could be stopped / started
  // (but might not be, e.g. if we're don't yet know if timers are shared)
  virtual void RegisteredCallback(int new_callback_count) = 0;
  virtual void UnregisteredCallback(int new_callback_count) = 0;

  // Resets state to the default
  virtual void Reset() = 0;

  // Gets the signal that should be monitored, if one should be
  virtual int GetSignal() { return 0; }

  // Allow best-effort low-cost suppression of events.
  // The thread stops simply sending events; the timer technique doesn't do anything
  // Best-effort is OK because the caller is enabling/disabling the signal handler as well
  virtual void EnableEvents() { }
  virtual void DisableEvents() { }

  // Gets the number of ticks that have elapsed since the last call
  // When using wall-clock profiling, multiple signals might be sent during a syscall,
  // but the signal handler is only called once.
  // The tick count lets us know how many ticks took place.
  virtual uint32_t GetTicksSinceLastCall() {
    return 1;
  }
};

// Extension support functions
typedef void (*ProfileRecordCallback)(uint32_t count, void ** backtrace, uint32_t depth);
typedef ProfileEventSource*(*ProfilerHandlerExtensionFn)(int32_t frequency, const char * extension_spec, ProfileRecordCallback callback);

#endif /* PROFILER_EXTENSION_H_ */
