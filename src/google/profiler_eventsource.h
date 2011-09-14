#ifndef PROFILER_EVENTSOURCE_H_
#define PROFILER_EVENTSOURCE_H_

/**
 * ProfileEventSource is the base class for strategies around different
 * profiling sampling techniques.
 *
 * To provide a custom sampling technique for the CPU profiler, one can
 * implement the ProfileEventSource interface and then make sure it is
 * created in ProfileHandler::BuildEventSource.
 *
 * Currently these strategies exist:
 *   TimerProfileEventSource:
 *     Sample using setitimer, i.e. sample on regular CPU-clock
 *     intervals or wall-clock intervals.
 *
 * Currently an event must raise a signal when it fires; ProfileHandler
 * then calls each of the registered callbacks.  In future we may remove
 * the requirement for an intermediate signal.
 *
 * ProfileEventSource objects should only be called by ProfileHandler;
 * calling methods directly can cause unexpected behaviour.
 *
 *
 * Background:
 *
 * The CPU profiler is a sampling profiler; "sampling" because instead
 * of trying to record everything that happens, instead we periodically
 * take "samples" that represent the state of the program.  We expect
 * that statistically these samples will approximate the full behaviour
 * of the program.  Normally these samples are stack traces of the
 * program at a particular moment in time.
 *
 * We can consider the behaviour of a program in different ways.  Often
 * we want to know where the program is spending most of its CPU time;
 * to measure this we sample based on regular intervals of CPU time;
 * the samples should then converge to an accurate representation of CPU
 * time usage.  The "setitimer" syscall gives us this behaviour, and
 * this is the traditional way in which google-perftools CPU profiler
 * has been used, as implemented in TimerProfileEventSource.
 *
 * However, in the case of an I/O bound program, knowing where we're
 * spending CPU time often isn't as important as knowing where we're
 * waiting on I/O. In this case, we usually want to know where the
 * program is spending its wall-clock time, rather than CPU time.
 * So we want to sample based on regular wall-clock intervals.
 *
 * Generalizing, there are a large number of ways to profile a program.
 * Modern CPUs include a number of hardware performance events (cache
 * misses, page faults, branches etc); the linux "Perf" system exposes a
 * number of OS events (run "perf list" to see a list).  By sampling on
 * regular intervals of these events we can get an idea of which code is
 * producing these events, just as CPU profiling shows us which code is
 * consuming the most CPU.
 *
 * User-code might also want to sample based on its own events
 * (e.g. mallocs, hashtable rehashes, RPC calls, any expensive function
 * or unexpected event.)
 *
 * Because there are a large number of different ways in which to
 * sample, we use a strategy pattern to separate out the sampling event
 * (the "when") from the sampling action (the "what").  ProfileHandler
 * does the "what" (currently recording stack traces), an instance of
 * ProfileEventSource says "when" to do it.
 */
#include <signal.h>
#include <stdint.h>

class ProfileHandler;

class ProfileEventSource {
public:
  ProfileEventSource() { }

  virtual ~ProfileEventSource() { }

  // Registers the current thread with the event source.
  // Any per-thread actions can be done here; it's also a good place for
  // any one-off initialization.
  // Called automatically at initialization by ProfileHandlerInitializer,
  // or explicitly by a call to ProfileHandlerRegisterThread.
  virtual void RegisterThread(int callback_count) = 0;

  // Called after a sampler callback is registered / unregistered.
  // If the event source is high-impact, start or stop it here based on
  // the new callback count.
  virtual void RegisteredCallback(int new_callback_count) = 0;
  virtual void UnregisteredCallback(int new_callback_count) = 0;

  // Resets any internal state to the initial state;
  // Called when ProfileHandlerReset is called.
  // e.g. Stop a timer if you started one.
  virtual void Reset() = 0;

  // Returns the signal that should be monitored for our events.
  // Currently all profiling events are raised through signals.
  // Return NO_SIGNAL if no signal should be monitored.
  static const int NO_SIGNAL = 0;
  virtual int GetSignal() { return NO_SIGNAL; }

  // To avoid complex threading issues, the profiler suppresses events when
  // its internal state is changing (e.g. when adding or removing callbacks)
  // After a call to EnableEvents, no more events should be raised until
  // a call to DisableEvents.
  // The profiler also enables/disables its signal handler, so an event
  // source does not have to support these functions.  These functions
  // should normally have an inexpensive implementation (suppress events
  // rather than disable the event source), because these functions are
  // called whenever a callback is registered.  Disabling the event
  // source should instead be done when the last callback is
  // unregistered (see UnregisteredCallback above)
  virtual void EnableEvents() { }
  virtual void DisableEvents() { }
};

#endif /* PROFILER_EVENTSOURCE_H_ */
