// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

// /home/rfriedma/src/asok_2020/ceph/build/boost/include
// clang++ /*-I/home/rfriedma/src/v_2006/ceph/build/boost/include*/ --std=c++17 g.cc
// clang++ --std=c++17 sml.cc -ggdb -pthread

//
// Created by rfriedma on 23/04/2020.
//

#pragma once
#define WITH_SML


#include "/home/rfriedma/try/sml/include/boost/sml.hpp"


#include <cstdio>
#include <string>
#include <string_view>
#include <chrono>
#include <memory>
//#include <sys/types.h>	// for killing threads
//#include <signal.h>	// for killing threads

// only for testing: -------------------------------

#include <queue>
#include <thread>
#include <mutex>
#include <optional>
#include <atomic>
#include <stdlib.h>
#include <cassert>
#include "inplace.h"


using namespace std::chrono;
using namespace std::chrono_literals;

bool simulation_this_chunk_is_last{ true };

// --- end of testing-only code


namespace sml = boost::sml;


struct PG {
  int info_history_same_interval_since;

  struct recovery_state_t {

    int get_last_update_applied() { return xx; }
    int xx{ -1 };
  };

  recovery_state_t recovery_state;

  void run_callbacks() { fprintf(stderr, "PG::run_callbacks()\n"); }

  void requeue_writes() { fprintf(stderr, "PG::requeue_writes()\n"); }

  // state flags that are relevant to scrubbing
};

/*!
  The part of scrubber_t that's not a state-machine wiring.

  Note: scrubber_t inherits from us (and from the state machine itself). Thus -
  most internal data is "protected".

  Why the separation? I wish to move to a different FSM implementation. Thus I
  am forced to strongly decouple the state machine implementation details from
  the actual scrubbing code.
*/
class scrub_internals_t {
 public:
  scrub_internals_t(PG* pg) : pg_{ pg } {}

  bool was_epoch_changed() { return (epoch_start != pg_->info_history_same_interval_since); }

  /*
    When exiting 'active' state (on error, interval changed, (done?))
    - reset
    - mark scrubbing as not active
    - unreserve - local and remote
  */
  void cleanup()
  {
    //
  }

  // note - true if the range is available
  bool dbg_select_range{ true };
  bool select_range()
  {
    /* the whole block (circa l. 2710) that sets:
      - subset_last_update
      - max_end
      - end?
      - start


      By:
      - setting tentative range based on conf and divisor
      - requesting a partial list of elements from the backend;
      - handling some head/clones issues
      -
     */
    return dbg_select_range;
  }

  // walk the log to find the latest update that affects our chunk
  void search_log_for_updates()
  {
    search_log_for_updates_done = true;
    /*
    scrubber.subset_last_update = eversion_t();
    for (auto p = projected_log.log.rbegin(); p != projected_log.log.rend();
	 ++p) {
      if (p->soid >= scrubber.start && p->soid < scrubber.end) {
	scrubber.subset_last_update = p->version;
	break;
      }
    }
    if (scrubber.subset_last_update == eversion_t()) {
      // (there was no relevant update entry in the log)
      for (list<pg_log_entry_t>::const_reverse_iterator p =
	     recovery_state.get_pg_log().get_log().log.rbegin();
	   p != recovery_state.get_pg_log().get_log().log.rend(); ++p) {
	if (p->soid >= scrubber.start && p->soid < scrubber.end) {
	  scrubber.subset_last_update = p->version;
	  break;
	}
      }
    }
    */
  }
  bool search_log_for_updates_done{ false };  // a unit-test aux

  int dbg_build_scrub_map_chunk{ -EINPROGRESS };
  int build_scrub_map_chunk() { return dbg_build_scrub_map_chunk; }

  void get_replicas_maps()
  {
    assert(search_log_for_updates_done);
    /*
	// ask replicas to scan
	scrubber.waiting_on_whom.insert(pg_whoami);

	// request maps from replicas
	for (set<pg_shard_t>::iterator i = get_acting_recovery_backfill().begin();
	     i != get_acting_recovery_backfill().end();
	     ++i) {
	  if (*i == pg_whoami) continue;
	  _request_scrub_map(*i, scrubber.subset_last_update,
			     scrubber.start, scrubber.end, scrubber.deep,
			     scrubber.preempt_left > 0);
	  scrubber.waiting_on_whom.insert(*i);
	}
	dout(10) << __func__ << " waiting_on_whom " << scrubber.waiting_on_whom
		 << dendl;

	scrubber.state = PG::Scrubber::BUILD_MAP;
	scrubber.primary_scrubmap_pos.reset();
     */
  }

  // protected:

  PG*  pg_;
  int  epoch_start;
  bool active_{ false };
  int  active_pushes{ 1 };  // is it part of the PG?
  int  subset_last_update{ -1 };

  int num_digest_updates_pending{ 1 };

  std::vector<int> waiting_on_whom;

  void scrub_compare_maps() { fprintf(stderr, "scrub_compare_maps()\n"); }

  std::chrono::milliseconds sleep_needed{ 10ms };

  struct preemption_data_t {
    int	   left_{ 1 };	// actually - to be fetched from conf
    size_t size_divisor{ 1 };
    bool   try_preempt()
    {
      if (left_ > 0) {
	--left_;
	size_divisor *= 2;
	return left_ > 0;
      }
      return false;
    }

    bool adjust()
    {
      if (left_ > 0) {
	--left_;
	size_divisor *= 2;
	return left_ > 0;
      }
      return false;
    }

    size_t chunk_divisor() const { return size_divisor; }

    void reset() { *this = preemption_data_t{}; }
  };
  preemption_data_t preemption_data;
};

using eqe_t = stdext::inplace_function<void()>;




