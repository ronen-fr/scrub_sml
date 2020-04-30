#pragma once


#include <memory>
#include <queue>

#include "globals.h"
#include "scrubber.h"

class safe_delay;

class scrub_fsm_t {
 public:

  scrub_fsm_t(scrub_internals_t& si, sml_log_t& lg);
  ~scrub_fsm_t();

  scrub_internals_t& internals_;

  std::shared_ptr<safe_delay> delayer_;
  void kill_delayer();

  // the callbacks

  void pending_timer_entry();

  void newchunk_entry();
  void newchunk_epochchanged();
  void newchunk_busy();
  void search_log_for_updates();
  void wait_pushes_entry();
  bool active_pushes_exist() const;
  bool was_epoch_changed() const;
  bool were_all_updates_applied();
  void get_replicas_maps();
  void buildmap_step();
  void erase_myself_from_waited();
  bool got_all_replicas();
  void waitreplicas_when_done();
  void simulate_repl_msg();
  bool got_all_digest_updates();
  void this_chunk_is_done();

  bool left_preemptions();
  void adjust_preemption();
  void reset_preemption();

  void do_act1();
  void do_act2(eqe_t act);
  void qu_event_InternalError();

 //private:


  struct TheFsm;
  std::shared_ptr<TheFsm> fsm_;


  // and - for testing - a queue of events
  volatile bool do_die{ false };
  std::queue<eqe_t> eq;
  std::mutex	    eq_mutex;
  // and a thread to execute those:
  std::thread* eq_thread;
  void	       push_e(const eqe_t e)
  {
    std::scoped_lock lock(eq_mutex);
    eq.push(e);
  }
  std::optional<eqe_t> pop_e()
  {
    std::scoped_lock lock(eq_mutex);
    if (eq.empty())
      return std::nullopt;
    auto p = eq.front();
    eq.pop();
    return p;
  }
};


/*
   An auxiliary class to safely handle delayed activation.
   I am assuming that this will be replaced by regular services that are part
   of the existing infrastructure
 */
class safe_delay {

 public:

  static std::shared_ptr<safe_delay> make_delayer(milliseconds delay, scrub_fsm_t* scrbr, eqe_t cmd);

  void do_die()
  {
    is_alive.store(false);
    if (thr)
      thr->join();
  }

  // private: why is the ctor required to be public?

  safe_delay(milliseconds delay, scrub_fsm_t* scrbr, eqe_t actn) : act{ actn }, scrb{ scrbr } {}

 private:
  std::atomic_bool is_alive{ true };
  std::thread*	thr{ nullptr };
  eqe_t	act;
  scrub_fsm_t* scrb;

  void looper(std::shared_ptr<safe_delay> sp_to_hold, milliseconds delay)
  {
    static constexpr milliseconds max_d = 250ms;

    while (is_alive.load() && delay > max_d) {
      std::this_thread::sleep_for(max_d);
      delay -= max_d;
    }

    if (is_alive.load())
      std::this_thread::sleep_for(delay);

    if (is_alive.load())
      //(act)();
      scrb->push_e(act);
  }
};
