<blockquote>
Midway upon the journey of our life
<br>
 I found myself within a forest dark,
<br>
 For the straightforward pathway had been lost.
<br>
                 Dante Alighieri
</blockquote>




### A summary of my attempt to use boost_experimental::sml instead of boost:state_chart

SML ("your scalable C++14 one header only State Machine Library with no dependencies") was created by
Kris Jusiak as a modern replacement for boost:MSM and boost::state_chart. As he explains (see for example
https://youtu.be/yZVby-PuXM0?t=2661), MSM while powerful uses dated technology (MPL) -- resulting in
long compilation times, bloated code, and cryptic error messages.

SML offers a convenient, easy to read, syntax - especially when compared with boost::state_charts. For example:
```C++
    return make_transition_table(
       *"idle"_s + event<e1> = "s1"_s
      , "s1"_s + event<e2> [ guard1 ] / action1 = "s2"_s
      , "s2"_s + event<e3> [ guard1 && ![] { return false;} ] / (action1, action2{}) = "s3"_s
      , "s3"_s + event<e4> [ !guard1 || guard2 ] / (action1, [] { std::cout << "action3" << std::endl; }) = "s4"_s
      , "s3"_s + event<e4> [ guard1 ] / ([] { std::cout << "action4" << std::endl; }, [this] { action4(); }) = "s5"_s
      , "s5"_s + event<e5> [ call(guard3) || guard2 ] / call(this, &actions_guards::action5) = X
    );
```

It also provides better (to my taste) support for dependencies separation (using DI - and there are some
YouTube videos of Mr. Jusiak detailing this aspect). BTW - using DI mandates some 'tricks' for enabling
the back-and-forth between the SM and the 'business logic'. E.g. see how scrub_fsm_t::TheFsm is declared
in scrub_fsm.h and scrub_fsm.cpp.

*I have previously introduced SML as a major component when working for HP Indigo. That code, unfortunately,
was part of a project that was canned - thus was never tested heavily.*


Alas, there are some drawbacks to using SML:

- SML is not maintained very actively. In 2020, for example, there had been one merged PR (fixing a misspelled example).
  And there *are* issues that should have been addressed:

- support for hierarchical SMs is lacking. There is, for example, no convenient way to query whether "we are in" a
  specific state, without making a distinction between the containing- and the sub- machine. Not to mention
  a strange design decision of having the nested SM being "in" its initial state event before that SM being
  entered.
  Things get much worse when combining hierarchical machines and orthogonal states. One cannot simply
  query 'being in' regarding one of the orthogonal paths, without specifying the other
  orthogonal states.
  <br>
  Or -
  take the 'example code' that dumps a SML state-machine as a PlantUML diagram source. That code handles neither
  hierarchical SFMs nor orthogonals.

*Note that these issues can mostly be worked around. There are discussion threads, on github and elsewhere, where
fixes are suggested. Specifically regarding the PlanUML dump - I have extended the code as required (see plant_uml.h).
Still - it is a major nuisance, and I had the feeling I am battling the SML library instead of concentrating on
my real tasks.*

- there is no easy replacement for state_chart's 'custom_reaction': say the incoming event triggers a process
  that selects one of N possible destination states. In SML this will require a workaround - either heavy use
  of guards, or - especially if the guards are expensive or not idempotent - an ungainly combination
  of (running the selection process, storing its result) + (generating an internal event) + (using guards that
  probe the stored results)

and worst of all:

- there seem to be no easy way to queue events from a sub-machine, if the code is not a direct part of the lambda inside the
  SM description (one can call 'process_event' from the passed dependency, but that call is synchronous - which is
  probably the wrong thing to do).


#### Bottom line:

I will be sticking with state_chart. At least for now.

But you can check this repository for my semi-valiant attempt. Feel free to show me how the problems
mentioned above can be solved easily.

