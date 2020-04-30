#pragma once

#include "/home/rfriedma/try/sml/include/boost/sml.hpp"

#include <cstdio>
#include <string>
#include <string_view>
#include <chrono>
#include <memory>
#include <sys/types.h>	// for killing threads
#include <signal.h>	// for killing threads

// only for testing: -------------------------------

#include <queue>
#include <thread>
#include <mutex>
#include <optional>
#include <atomic>
#include <stdlib.h>
#include "globals.h"

#define CATCH_CONFIG_MAIN
#include "/home/rfriedma/src/Catch2/catch.hpp"

using namespace std::chrono;
using namespace std::chrono_literals;

// --- end of testing-only code

#include "scrub_fsm.h"

namespace sml = boost::sml;


// clang-format off
struct EpochChanged{};
struct EntryEvent{};
struct StartScrub {};
struct SchedScrub {};
struct InternalSchedScrub {};
struct ChunkIsBusy {};
struct SelectedChunkFree {};
struct AdjustPreemption {};
struct TmpEvent_1 {};
struct ActivePushesUpd{};
struct ActivePushesHandled{};
struct UpdatesApplied{};
struct DoStep{};
struct InternalError{};
struct IntLocalMapDone{};
struct GotReplicas{};
struct DigestUpdate{};
struct NoMoreChunks{};
struct ToNextChunk{};

struct EnablePreemption {};
struct DisablePreemption {};
struct ResetPreemption {};
struct PreemptionRequest{};
struct HandlePreemption {};
// clang-format on


// just for SML issues:
// moved: struct EntryEvent{};
// clang-format off
#define EvEpochChanged 		::sml::event<EpochChanged>
#define EvStartScrub 		::sml::event<StartScrub>
#define EvSchedScrub 		::sml::event<SchedScrub>
#define EvInternalSchedScrub 	::sml::event<InternalSchedScrub>
#define EvChunkIsBusy 		::sml::event<ChunkIsBusy>
#define EvSelectedChunkFree 	::sml::event<SelectedChunkFree>
#define EvAdjustPreemption 	::sml::event<AdjustPreemption>
#define EvTmpEvent_1 		::sml::event<TmpEvent_1>

#define EvEntryEvent 		::sml::event<EntryEvent>
#define EvActivePushesUpd 	::sml::event<ActivePushesUpd>
#define EvActivePushesHandled 	::sml::event<ActivePushesHandled>
#define EvUpdatesApplied	::sml::event<UpdatesApplied>
#define EvDoStep		::sml::event<DoStep>
#define EvInternalError		::sml::event<InternalError>
#define EvIntLocalMapDone	::sml::event<IntLocalMapDone>
#define EvGotReplicas		::sml::event<GotReplicas>
#define EvDigestUpdate		::sml::event<DigestUpdate>
#define EvNoMoreChunks		::sml::event<NoMoreChunks>
#define EvToNextChunk		::sml::event<ToNextChunk>

#define EvEnablePreemption 	::sml::event<EnablePreemption>
#define EvDisablePreemption 	::sml::event<DisablePreemption>
#define EvResetPreemption 	::sml::event<ResetPreemption>
#define EvPreemptionRequest 	::sml::event<PreemptionRequest>
#define EvHandlePreemption 	::sml::event<HandlePreemption>
// clang-format on



auto active_updates_guard = [](scrub_fsm_t* si) -> bool {
  fmt::print(std::cerr, "active_updates_guard: {}\n", si->active_pushes_exist());
  return si->active_pushes_exist();
};

auto same_epoch_guard = [](scrub_fsm_t* si) -> bool {
  fmt::print(std::cerr, "same_epoch_guard: {}\n", !si->was_epoch_changed());
  return !si->was_epoch_changed();
};

auto all_updates_applied = [](scrub_fsm_t* si) -> bool {
  fmt::print(std::cerr, "all_updates_applied guard: {}\n", si->were_all_updates_applied());
  return si->were_all_updates_applied();
};

// the handling of 'were preempted' is kludgy, due to SML issues with 'in()' for nested orthogonal
// states
static bool in_preempted_state{false};

auto were_preempted = []() -> bool {
  fmt::print(std::cerr, "were we preempted guard: {}\n", in_preempted_state);
  return in_preempted_state;
};

auto got_all_replicas = [](scrub_fsm_t* si) -> bool {
  fmt::print(std::cerr, "got data from all replicas? {}\n", si->got_all_replicas());
  return si->got_all_replicas();
};

auto got_all_digest_updates= [](scrub_fsm_t* si) -> bool {
  fmt::print(std::cerr, "got dig updates? {}\n", si->got_all_digest_updates());
  return si->got_all_digest_updates();
};


struct fsm_impl {

  static inline auto actnq_epoch = [](scrub_fsm_t* si) -> void {};

  //  static inline auto newchunk_entry = [](scrub_fsm_t* si) -> void {
  //    fmt::print(std::cerr, "newc entry @{}\n", m_s());
  //  };

  static inline auto newchunk_epoch2 = [](scrub_fsm_t* si) -> void {};

  struct newchunk_epoch {
    void operator()(scrub_fsm_t* si);
  };

  template <typename T> struct pendingtimer_entry {
    // void operator()(const scrub_fsm_t& si, auto& sm);

    // void operator()(scrub_internals_t& si) {}

    // void operator()(scrub_fsm_t* si, sml::back::process<T> p) { si->pending_timer_entry(p); }
    void operator()(scrub_fsm_t* si) { si->pending_timer_entry(); }

    // void operator()(ScrubMachine* sm);
  };

  struct Active {

    auto operator()() const
    {

      using namespace ::boost::sml;

      // clang-format off
      return make_transition_table(

	*"ActiveNQueued"_s + EvEpochChanged  / actnq_epoch
	,"ActiveNQueued"_s + EvSchedScrub                                          = "PendingTimer"_s
	,"ActiveNQueued"_s + on_entry<_>     /  []() ->void {fprintf(stderr, "anq: eventry\n");}

	,"PendingTimer"_s  + on_entry<_>     / [](scrub_fsm_t* si) mutable -> void {
	                  fmt::print(std::cerr,"{}: b4 'process'\n", __PRETTY_FUNCTION__);
			  si->do_act1();
			  //process(EntryEvent{});

	                  fmt::print(std::cerr, "{}: af 'process'\n", __PRETTY_FUNCTION__);
			} // RRR
	,"PendingTimer"_s  + EvEntryEvent	 / [](scrub_fsm_t* si) -> void {
	                  fmt::print(std::cerr, "PendT eventry\n");
			  si->pending_timer_entry();
			}

	,"PendingTimer"_s  + EvInternalSchedScrub                                  = "NewChunk"_s
	,"PendingTimer"_s  + EvSelectedChunkFree   / [](){abort(); } // arrived too early. FSM implementation bug
	,"PendingTimer"_s  + sml::on_exit<_>       / [](scrub_fsm_t* si) -> void { si->kill_delayer(); }

	,"NewChunk"_s      + on_entry<_>	/  [](scrub_fsm_t* si)
							 -> void { si->newchunk_entry(); }

	,"NewChunk"_s      + EvEpochChanged  / [](scrub_fsm_t* si)
	  				-> void { si->newchunk_epochchanged(); }

	,"NewChunk"_s      + EvChunkIsBusy   / [](scrub_fsm_t* si) -> void { si->newchunk_busy(); }  = "ActiveNQueued"_s

	,"NewChunk"_s      + EvSelectedChunkFree / [](scrub_fsm_t* si) -> void {
					si->search_log_for_updates(); }  = "WaitPushes"_s

	,"WaitPushes"_s	   + on_entry<_>        / process(ActivePushesUpd{})
	,"WaitPushes"_s	   + EvActivePushesUpd  [  active_updates_guard ]   = "WaitPushesWaitState"_s
	,"WaitPushes"_s	   + EvActivePushesUpd  [ !active_updates_guard ]   = "WaitLastUpdate"_s

	,"WaitPushesWaitState"_s + EvSchedScrub [  same_epoch_guard ]	     = "WaitLastUpdate"_s
	,"WaitPushesWaitState"_s + EvSchedScrub [ !same_epoch_guard ]	     = X

	,"WaitLastUpdate"_s + on_entry<_>        / process(UpdatesApplied{})
	,"WaitLastUpdate"_s + EvUpdatesApplied   [  all_updates_applied && !same_epoch_guard ]	/
				process(EpochChanged{})

	,"WaitLastUpdate"_s + EvUpdatesApplied   [  all_updates_applied && same_epoch_guard ]	/
				[](scrub_fsm_t* si) -> void { si->get_replicas_maps(); }  = "BuildMap"_s

	,"BuildMap"_s   + on_entry<_>        / process(DoStep{})
	,"BuildMap"_s   + sml::on_exit<_>    / [](scrub_fsm_t* si) -> void { si->kill_delayer(); }
	,"BuildMap"_s   + EvDoStep [ !same_epoch_guard ] / process(EpochChanged{})
	,"BuildMap"_s   + EvDoStep [ same_epoch_guard && were_preempted ]           = "DrainReplMaps"_s
	,"BuildMap"_s   + EvDoStep [ same_epoch_guard && !were_preempted ] /
				[](scrub_fsm_t* si) -> void { si->buildmap_step(); }
	,"BuildMap"_s   + EvIntLocalMapDone /
				[](scrub_fsm_t* si) -> void { si->erase_myself_from_waited(); }  = "WaitReplicas"_s

	,"DrainReplMaps"_s + EvGotReplicas [ got_all_replicas ]                = "NewChunk"_s

	// -------------- WaitReplicas --------------

	// note: a possible race regarding the multiple calls to got_all_replicas(). Todo: latch on entry.
	,"WaitReplicas"_s + on_entry<_>        / process(GotReplicas{})

	,"WaitReplicas"_s + sml::on_exit<_>    / [](scrub_fsm_t* si) -> void { si->kill_delayer(); }

	,"WaitReplicas"_s + EvGotReplicas [ !got_all_replicas ]   /
			      [](scrub_fsm_t* si) -> void { si->simulate_repl_msg(); }

	,"WaitReplicas"_s + EvGotReplicas [ got_all_replicas && !same_epoch_guard ]   = "ActiveNQueued"_s

	,"WaitReplicas"_s + EvGotReplicas [ got_all_replicas && same_epoch_guard ] /
			      [](scrub_fsm_t* si) -> void { si->waitreplicas_when_done(); } = "WaitDigestUpdate"_s

	// -------------- WaitDigestUpdate --------------

	,"WaitDigestUpdate"_s + on_entry<_>                               / process(DigestUpdate{})

	,"WaitDigestUpdate"_s + EvDigestUpdate [ got_all_digest_updates ] /
				  [](scrub_fsm_t* si) -> void { si->this_chunk_is_done(); }

	,"WaitDigestUpdate"_s + EvToNextChunk			= "ActiveNQueued"_s
	,"WaitDigestUpdate"_s + EvNoMoreChunks			= X

	,

	// -------------- the preemption sub-machine --------------

	*"NonPreemptable"_s   + event<EnablePreemption>  [ ([](scrub_fsm_t* si) -> bool
				{return si->left_preemptions();} )] = "Preemptable"_s
	,"NonPreemptable"_s   + EvDisablePreemption   / []{/*discard*/}
	,"NonPreemptable"_s   + EvResetPreemption     / []{  fprintf(stderr,"pr-reset\n");  }
	,"NonPreemptable"_s   + EvAdjustPreemption    / [](scrub_fsm_t* si) -> void {si->adjust_preemption();}

	// the 'internal error' event must be handled in the context of the external SM. And SML does not
	// support that
	//,"Preemptable"_s  + EvEnablePreemption   / process(InternalError{})  // a bug
	,"Preemptable"_s  + EvEnablePreemption   / [](scrub_fsm_t* si) -> void { si->qu_event_InternalError(); }
	//,"Preemptable"_s  + EvInternalError                                             = "NonPreemptable"_s


	,"Preemptable"_s  + EvDisablePreemption                                         = "NonPreemptable"_s
	,"Preemptable"_s  + EvResetPreemption    / [](scrub_fsm_t* si) -> void {si->reset_preemption();}
	                                                                                = "NonPreemptable"_s
	,"Preemptable"_s  + EvAdjustPreemption                                          = X // verify
	,"Preemptable"_s  + EvPreemptionRequest                                         = "Preempted"_s
	,"Preempted"_s    + sml::on_entry<_>    / []() -> void { in_preempted_state = true; }
	,"Preempted"_s    + sml::on_exit<_>     / []() -> void { in_preempted_state = false; }
	,"Preempted"_s    + EvEnablePreemption  / []{/*discard*/}
	,"Preempted"_s    + EvDisablePreemption / []{/*discard*/}
	,"Preempted"_s    + EvResetPreemption   / [](scrub_fsm_t* si) -> void {si->reset_preemption();}
	                                                                                = "NonPreemptable"_s
	,"Preempted"_s    + EvAdjustPreemption  / [](scrub_fsm_t* si) -> void {si->adjust_preemption();}
											= "NonPreemptable"_s
	,"Preempted"_s    + EvPreemptionRequest / []{/*discard*/}

      );
      // clang-format on
    }
  };

  auto operator()() const
  {

    using namespace sml;

    // clang-format off
    return make_transition_table(
      *"NotActive"_s + event<StartScrub>                         = "ReservationsMade"_s
      ,"ReservationsMade"_s + event<TmpEvent_1>			= state<fsm_impl::Active>
      ,"ReservationsMade"_s + on_entry<_> / []() ->void {fprintf(stderr, "Reserv: eventry\n");}

      //"ActiveScrubbing"_s + event<HandlePreemption> / []{},
      //"ActiveScrubbing"_s + EvAdjustPreemption / []{},

      ,state<fsm_impl::Active> + EvInternalError	                = X
      ,state<fsm_impl::Active> + event<ResetPreemption>	                = X
    );
    // clang-format on
  }
};

#ifdef DUMP_UML
#include "plant_uml.h"
#endif

