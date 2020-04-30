//
// Created by rfriedma on 23/04/2020.
//

#include "globals.h"
#include "scrub_fsm.h"
#include "fsm_impl.h"
#include <memory>
#include <queue>
#include <deque>

#define CATCH_CONFIG_MAIN
#include "/home/rfriedma/src/Catch2/catch.hpp"

namespace sml = boost::sml;
using namespace sml;

using chosen_t = sml::sm<fsm_impl,
			 sml::logger<sml_log_t>,
			 sml::defer_queue<std::deque>,
			 sml::process_queue<std::queue>,
			 sml::dispatch<sml::back::policies::branch_stm> >;

struct scrub_fsm_t::TheFsm : public chosen_t {
  explicit TheFsm(scrub_fsm_t* ctrl, sml_log_t& lg) : chosen_t(static_cast<scrub_fsm_t*>(ctrl), lg) {}
};

scrub_fsm_t::scrub_fsm_t(scrub_internals_t& si, sml_log_t& lg) : internals_{ si },
    fsm_{ std::make_shared<TheFsm>(this, lg) }
{
  eq_thread = new std::thread([&, this]() {
    while (!do_die) {
      // a very simplistic implementation:
      auto e = pop_e();
      if (e) {
	// to do: check activation number
	fmt::print(std::cerr, "!!!!!!!!!!!!!! vvvvvv{}v\n", m_s());
	(*e)();
	fmt::print(std::cerr, "!!!!!!!!!!!!!! ^^^^^^{}^\n", m_s());
      } else {
	std::this_thread::sleep_for(2ms);
      }
    }
  });
}

scrub_fsm_t::~scrub_fsm_t()
{

  do_die = true;
  eq_thread->join();
}

void scrub_fsm_t::pending_timer_entry(/*poster_EntryEvent p*/)
{
  //  if we are required to sleep:
  //	arrange a callback sometimes later.
  //	be sure to be able to identify a stale callback

  if (auto sleep_time = internals_.sleep_needed; sleep_time.count()) {
    // schedule a transition sleep_time in the future
    fmt::print(std::cerr, "zzzzzzzzz... at {} for {}ms\n", m_s(), sleep_time.count());
    internals_.sleep_needed = 0ms;

    eqe_t cmnd{ [this]() {
      fmt::print(std::cerr, "{}: sending InterSchedScrub ({})\n", __PRETTY_FUNCTION__, m_s());
      // fmt::print(std::cerr, "%s: sending InterSchedScrub\n", __PRETTY_FUNCTION__);
      fsm_->process_event(InternalSchedScrub{});
      fmt::print(std::cerr, "{}: after InterSchedScrub ({})\n", __PRETTY_FUNCTION__, m_s());
    } };

    delayer_ = safe_delay::make_delayer(sleep_time, this, cmnd);

  } else {

    fmt::print(std::cerr, "{}: immediate InterSchedScrub ({})\n", __PRETTY_FUNCTION__, m_s());
    fsm_->process_event(InternalSchedScrub{});
  }
}

void scrub_fsm_t::newchunk_entry()
{
  fmt::print(std::cerr, "newc entry @{}\n", m_s());
  fsm_->process_event(EnablePreemption{});

  bool got_a_chunk = internals_.select_range();
  fmt::print(std::cerr, "\t\t{}: selection {}\n", __FUNCTION__, got_a_chunk ? "(OK)" : "(busy)");
  if (got_a_chunk) {
    fsm_->process_event(SelectedChunkFree{});
  } else {
    fmt::print(std::cerr, "\t{}: selected chunk is busy\n", __FUNCTION__);
    // wait until we are available
    fsm_->process_event(ChunkIsBusy{});
  }
}

void scrub_fsm_t::newchunk_epochchanged(/*poster p*/) {}

void scrub_fsm_t::newchunk_busy() {}

//void scrub_fsm_t::do_act1() { fsm_->process_event(EntryEvent{}); }
void scrub_fsm_t::do_act1() {
  	push_e(eqe_t{[this]() { fsm_->process_event(EntryEvent{}); }});
}

void scrub_fsm_t::qu_event_InternalError() {
  push_e(eqe_t{[this]() { fsm_->process_event(InternalError{}); }});
}

void scrub_fsm_t::search_log_for_updates()
{
  internals_.search_log_for_updates();
}

void scrub_fsm_t::wait_pushes_entry()
{

}

bool scrub_fsm_t::active_pushes_exist() const { return false; }

void scrub_fsm_t::do_act2(eqe_t act)
{
  push_e(std::move(act));
}


bool scrub_fsm_t::was_epoch_changed() const { return false; }

bool scrub_fsm_t::were_all_updates_applied()
{
  auto last_applied = internals_.pg_->recovery_state.get_last_update_applied();
  return (last_applied >= internals_.subset_last_update);
}

void scrub_fsm_t::get_replicas_maps()
{
  internals_.get_replicas_maps();
}

void scrub_fsm_t::buildmap_step()
{
  auto ret = internals_.build_scrub_map_chunk();

  if (ret == -EINPROGRESS) {
    // must wait for the backend to finish. No specific event provided.

    // in the simulation (and in the actual code, too), we must allow the FSM
    // to continue receiving external events while waiting. sm_process_events() must
    // return before being requeued.

    prevent_logging = true;

    eqe_t cmnd{ [this]() {
      fmt::print(std::cerr, "buildmap_step: sending DoStep ({}) {}\n", m_s(), (uint64_t)(fsm_.get()));
      internals_.dbg_build_scrub_map_chunk = 3;
      fsm_->process_event(DoStep{});
      fmt::print(std::cerr, "{}: after DoStep ({})\n", __PRETTY_FUNCTION__, m_s());
    } };

    delayer_ = safe_delay::make_delayer(800ms, this, cmnd);

  } else if (ret < 0) {

    prevent_logging = false;
    fprintf(stderr, "\n%s: Error! Aborting.\n", __FUNCTION__);
    internals_.cleanup();
    fsm_->process_event(InternalError{});

  } else {

    // the local map was created
    prevent_logging = false;
    fsm_->process_event(IntLocalMapDone{});
  }
}

void scrub_fsm_t::kill_delayer()
{
  if (delayer_) {
    delayer_->do_die();
    delayer_.reset();
  }
}

void scrub_fsm_t::erase_myself_from_waited() {}

bool scrub_fsm_t::got_all_replicas()
{
  return internals_.waiting_on_whom.empty();
}

void scrub_fsm_t::waitreplicas_when_done()
{
  prevent_logging = false;
  kill_delayer();
  internals_.scrub_compare_maps();
  internals_.pg_->run_callbacks();
  internals_.pg_->requeue_writes();
}

void scrub_fsm_t::simulate_repl_msg()
{
  prevent_logging = true;

  eqe_t cmnd{ [this]() {
    fmt::print(std::cerr, "wait-replicas: sending 'got repl' ({}) {}\n", m_s(), (uint64_t)(fsm_.get()));
    fsm_->process_event(GotReplicas{});
    fmt::print(std::cerr, "{}: after GotReplicas ({})\n", __PRETTY_FUNCTION__, m_s());
  } };

  delayer_ = safe_delay::make_delayer(800ms, this, cmnd);
}

bool scrub_fsm_t::got_all_digest_updates()
{
  return internals_.num_digest_updates_pending == 0;
}

void scrub_fsm_t::this_chunk_is_done()
{
  // should result in either a transit to 'not-active'
  // or to 'active-n-queued'

  if (simulation_this_chunk_is_last) {

    // scrub_finish();

    // may handle trim

    fsm_->process_event(NoMoreChunks{});

  } else {

    // go get a new chunk (view "requeue")
    fsm_->process_event(ResetPreemption{});
    fsm_->process_event(ToNextChunk{});
  }
}

bool scrub_fsm_t::left_preemptions()
{
  return internals_.preemption_data.left_ > 0;
}

void scrub_fsm_t::adjust_preemption()
{
  internals_.preemption_data.adjust();
}

void scrub_fsm_t::reset_preemption()
{
  internals_.preemption_data.reset();
}


// /////////////////////////////////////// safe_delay

std::shared_ptr<safe_delay> safe_delay::make_delayer(milliseconds delay, scrub_fsm_t* scrbr, eqe_t cmd)
{
  auto myslf = std::make_shared<safe_delay>(delay, scrbr, cmd);
  myslf->thr = new std::thread([=]() mutable -> void { myslf->looper(myslf, delay); });
  return myslf;
}

// /////////////////////////////////////// unit tests

volatile int sync1{ 0 };

#define sm_process_event_int(A)                           \
  {                                                       \
    fmt::print(std::cerr, "<< Ev:{}:------------\n", #A); \
    smc.fsm_->process_event(A);                           \
    fmt::print(std::cerr, "Ev:{}>>\n\n", #A);             \
  }

#define sm_process_event_int2(A, N)                               \
  {                                                               \
    fmt::print(std::cerr, "<< Ev:{}:--${}----------\n", #A, (N)); \
    smc.fsm_->process_event(A);                                   \
    fmt::print(std::cerr, "Ev:{}>>\n\n", #A);                     \
  }

#define sm_process_event_s(E, S)   \
  {                                \
    smc.push_e([&] {               \
      sm_process_event_int2(E, S); \
      sync1 = S;                   \
    });                            \
  }

#define SYNC_ON(N)                                              \
  {                                                             \
    fmt::print(std::cerr, "\nSync point {}....{}\n", N, m_s()); \
    sm_process_event_s(TmpEvent_1{}, N);                        \
    while (N != sync1) {                                        \
      std::this_thread::sleep_for(22ms);                        \
    }                                                           \
    fmt::print(std::cerr, "  ...sync'd {}\n", m_s());           \
  }

// 'is_in' does not work correctly for orthogonal regions
#define IS_IN(ST)	\
  (smc.fsm_->is(ST))

#define IS_IN_AC(ST,PR)	\
  (smc.fsm_->is<decltype(state<fsm_impl::Active>)>(ST,PR))



TEST_CASE("sml1", "[sml]")
{
  using namespace sml;

  PG		    pg2;
  scrub_internals_t sint{ &pg2 };
  sml_log_t logg{};

  scrub_fsm_t smc{ sint, logg };

  REQUIRE(IS_IN("NotActive"_s));
  sint.sleep_needed = 2s;

  sm_process_event_s(StartScrub{}, 1);
  SYNC_ON(1);

  sm_process_event_s(TmpEvent_1{}, 766);
  SYNC_ON(766);

  sm_process_event_s(SchedScrub{}, 777);
  SYNC_ON(777);
  sleep(5);

  REQUIRE(smc.fsm_->is(state<fsm_impl::Active>));
  sm_process_event_s(HandlePreemption{}, 788);
  SYNC_ON(788);
}


#define SYNC_AND_STATE(N, ST)              \
  {                                        \
    sm_process_event_s(TmpEvent_1{}, (N)); \
    while ((N) != sync1) {                 \
      std::this_thread::sleep_for(50ms);   \
    }                                      \
    REQUIRE(IS_IN(ST));                    \
  }

#define SYNC_AND_STATES(N, ST, PR_ST)      \
  {                                        \
    sm_process_event_s(TmpEvent_1{}, (N)); \
    while ((N) != sync1) {                 \
      std::this_thread::sleep_for(50ms);   \
    }                                      \
    REQUIRE(IS_IN(ST));                    \
    REQUIRE(IS_IN(PR_ST));                 \
  }

#define WAIT_AND_STATES(N, ST, PR_ST)               \
  {                                                 \
    sm_process_event_s(TmpEvent_1{}, (N));          \
    while ((N) != sync1) {                          \
      std::this_thread::sleep_for(10ms);            \
    }                                               \
    while (!IS_IN(ST)) {                            \
      std::this_thread::sleep_for(10ms);            \
    }                                               \
    sm_process_event_s(TmpEvent_1{}, (1000 + (N))); \
    while ((1000 + (N)) != sync1) {                 \
      std::this_thread::sleep_for(10ms);            \
    }                                               \
    REQUIRE(IS_IN(PR_ST));                          \
  }

#define SYNC_AND_CHECK(N, ST)              \
  {                                        \
    sm_process_event_s(TmpEvent_1{}, (N)); \
    while ((N) != sync1) {                 \
      std::this_thread::sleep_for(10ms);   \
    }                                      \
    if (IS_IN(ST{}))                         \
      return;                              \
  }

#define SYNC_AND_CHECK_AC(N, ST)           \
  {                                        \
    sm_process_event_s(TmpEvent_1{}, (N)); \
    while ((N) != sync1) {                 \
      std::this_thread::sleep_for(10ms);   \
    }                                      \
    if (IS_IN_AC(ST{}))                      \
      return;                              \
  }


#if 0
// an auxliary test function that gets you to the state to be tested:
template <typename TARGET_STATE> void get_to_state_aux(scrub_fsm_t& smc)
{
  REQUIRE(smc.fsm_->is("NotActive"_s));
  sm_process_event_s(StartScrub{}, 999);
  SYNC_AND_CHECK(18, TARGET_STATE);

  smc.internals_.pg_->recovery_state.xx = -1;
  smc.internals_.subset_last_update	    = 10;  // cond for WaitLastUpdate->BuildMap

  if constexpr (std::is_same<TARGET_STATE, decltype("PendingTimer"_s) >::value)
    smc.internals_.sleep_needed = 1000s;
  else
    smc.internals_.sleep_needed = 0s;

  // no way to stop at NewChunk
  smc.internals_.dbg_select_range = true;
  smc.internals_.active_pushes = 10;

  smc.internals_.dbg_build_scrub_map_chunk = -EINPROGRESS;

  sm_process_event_s(SchedScrub(), 91);

  SYNC_AND_CHECK(2, TARGET_STATE);  // the sub-wait of WAITPUSHES
  smc.internals_.waiting_on_whom.push_back(17);
  smc.internals_.active_pushes = 0;
  sm_process_event_s(ActivePushesUpd{}, 92);	// to notice the active_pushes==0
  sm_process_event_s(SchedScrub(), 93);

  SYNC_AND_CHECK(5, TARGET_STATE);

  smc.internals_.pg_->recovery_state.xx = 10;
  sm_process_event_s(UpdatesApplied{}, 94);  // buildmap
  SYNC_AND_CHECK(5, TARGET_STATE);
  while (!IS_IN("BuildMap"_s)) {
    std::this_thread::sleep_for(50ms);
  }
  SYNC_AND_CHECK(7, TARGET_STATE);

  // go to wait replicas
  smc.internals_.dbg_build_scrub_map_chunk = 1;
  SYNC_AND_CHECK(8, TARGET_STATE);

  // wait digest update
  smc.internals_.waiting_on_whom.erase(smc.internals_.waiting_on_whom.begin());
  sm_process_event_s(GotReplicas{}, 76);
  SYNC_AND_CHECK(10, TARGET_STATE);

  // std::this_thread::sleep_for(100ms);
  // drain - dealt with separately
  // while (1);
  abort();
}

// NewChunk is transitory
template <> void get_to_state_aux<decltype("NewChunk"_s) >(scrub_fsm_t& smc) { abort(); }

template <typename TARGET_STATE> void get_to_state(scrub_fsm_t& smc)
{
  get_to_state_aux<TARGET_STATE>(smc);
  //REQUIRE(IS_IN(TARGET_STATE{}) || IS_IN_AC(TARGET_STATE{}));
}

#ifdef NOT_YET
template <> void get_to_state_aux<decltype("DrainReplMaps"_s) >(scrub_fsm_t& smc)
{
  get_to_state<WaitLastUpdate>(sm);

  // now - preempt before BuildMap
  sm_process_event_s(PreemptionRequest{}, 37);
  sm_process_event_s(InternalAllUpdates{}, 39);
  SYNC_AND_STATES(6, DrainReplMaps, Preempted);
}
#endif


// //////////////////////////////////////////////////////////////// //

TEST_CASE("old_main", "[basic]")
{
  using namespace sml;

  PG		    pg1;
  scrub_internals_t sint{ &pg1 };
  sml_log_t logg{};
  scrub_fsm_t sm{ sint, logg };

  pg1.info_history_same_interval_since = 17;

  SECTION("to_actvnqueued")
  {
    get_to_state<decltype("ActvNQueued"_s)>(sm);
    //sm.terminate();
  }

  SECTION("to_pending_timer")
  {
    get_to_state<decltype("PendingTimer"_s)>(sm);
    //sm.terminate();
    //std::this_thread::sleep_for(1s);  // allow some time to finish, to see what will happen
  }

  SECTION("to_wait_pushes_waitstate")
  {
    get_to_state<decltype("WaitPushesWaitState"_s)>(sm);
    //sm.terminate();
  }
#if 0
  SECTION("to_wait_last_update")
  {
    get_to_state<WaitLastUpdate>(sm);
    sm.terminate();
  }

  SECTION("to_build_map")
  {
    get_to_state<BuildMap>(sm);
    sm.terminate();
  }

  SECTION("to_waitreplicas")
  {
    get_to_state<WaitReplicas>(sm);
    sm.terminate();
  }

  SECTION("to_wait_digest_update")
  {
    get_to_state<WaitDigestUpdate>(sm);
    sm.terminate();
  }

  SECTION("to_drain")
  {
    get_to_state<DrainReplMaps>(sm);
    sm.terminate();
  }
#endif
}



#endif


#ifdef DUMP_UML
//
//  this "test case" just dumps an "almost usable" PlantUML
//  description of the state machine.
//  The Sed script 'fixuml' makes the output presentable.
//
TEST_CASE("dump_uml","[.]")
{
  using namespace sml;
  prevent_logging = true;

  PG		    pg2;
  scrub_internals_t sint{ &pg2 };
  sml_log_t logg{};

  std::cout << "@startuml" << std::endl << std::endl;
  std::cout << "hide empty description" << std::endl;
  scrub_fsm_t smc{ sint, logg };
  dump(*smc.fsm_);

#if 0
  sml_nolog_t nologg{};
  sm<fsm_impl::Active,
     sml::logger<sml_nolog_t>,
    sml::defer_queue<std::deque>,
    sml::process_queue<std::queue>,
    sml::dispatch<sml::back::policies::branch_stm> > sma{sint, nologg};

  dump(sma, "Active");
#endif
  std::cout << std::endl << "@enduml" << std::endl;
}


#endif
#if 0
TEST_CASE("sml1", "[sml]")
{
  using namespace sml;

  PG		    pg2;
  scrub_internals_t sint{ &pg2 };
  sm<ScrubMachine>  sm{ sint };

  REQUIRE(sm.is("NotActive"_s));
  sm.process_event(StartScrub{});

  sm.process_event(TmpEvent_1{});
  // auto x = sm.is<decltype(state<ScrubMachine::Active>)>;

  REQUIRE(sm.is(state<ScrubMachine::Active>));
  REQUIRE(sm.is<decltype(state<ScrubMachine::Active>)>("ActiveNQueued"_s, "NonPreemptable"_s));	 // works

  // REQUIRE(sm.is<decltype(state<ScrubMachine::Active>)>( "NonPreemptable"_s, "A"_s ) );

  // REQUIRE(is_any_state_in(sm, state<ScrubMachine::Active>::"NonPreemptable"_s ) ); // fails
  // REQUIRE(is_any_state_in(sm,  decltype(ScrubMachine::Active{}) /*"Active"_s*/ ) ); // fails

  sm.process_event(SchedScrub{});
  sm.process_event(EnablePreemption{});

  // REQUIRE(sm.is<decltype(state<ScrubMachine::Active>)>( "B"_s ) );
  // REQUIRE(sm.is<decltype(state<ScrubMachine::Active>)>( "Preemptable"_s ) );

  sm.process_event(DoStep{});
}
#endif
