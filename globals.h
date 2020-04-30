#pragma once

#define DUMP_UML

#include "inplace.h"


using eqe_t = stdext::inplace_function<void()>;

#include "/home/rfriedma/try/sml/include/boost/sml.hpp"
namespace sml =  ::boost::sml;


#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/time.h>
#include <fmt/color.h>
#include <fmt/printf.h>
#include <string>

std::string m_s()
{
  std::time_t t = std::time(nullptr);
  auto so = fmt::format("{:%M:%S}", *std::localtime(&t));
  return so;
}


#include <cstdio>
#include <iostream>

inline bool prevent_logging{false};

namespace {
struct sml_log_t {
  template <class SM, class TEvent> void log_process_event(const TEvent&)
  {
    if (!prevent_logging)
      fmt::print(std::cerr,"[{}][process_event] {}\n", sml::aux::get_type_name<SM>(), sml::aux::get_type_name<TEvent>());
  }

  template <class SM, class TGuard, class TEvent> void log_guard(const TGuard&, const TEvent&, bool result)
  {
    if (!prevent_logging)
      fmt::print(std::cerr,"[{}][guard] {} {} {}\n", sml::aux::get_type_name<SM>(), sml::aux::get_type_name<TGuard>(),
	   sml::aux::get_type_name<TEvent>(), (result ? "[OK]" : "[Reject]"));
  }

  template <class SM, class TAction, class TEvent> void log_action(const TAction&, const TEvent&)
  {
    if (!prevent_logging)
      fmt::print(std::cerr,"[{}][action] {} {}\n", sml::aux::get_type_name<SM>(), sml::aux::get_type_name<TAction>(),
	   sml::aux::get_type_name<TEvent>());
  }

  template <class SM, class TSrcState, class TDstState>
  void log_state_change(const TSrcState& src, const TDstState& dst)
  {
    fmt::print(std::cerr,"\n##########\n[{}][transition] {} -> {}\n\n", sml::aux::get_type_name<SM>(), src.c_str(), dst.c_str());
  }
};

struct sml_nolog_t {
  template <class SM, class TEvent> void log_process_event(const TEvent&)
  {
    if (false)
      fmt::print(std::cerr,"[{}][process_event] {}\n", sml::aux::get_type_name<SM>(), sml::aux::get_type_name<TEvent>());
  }

  template <class SM, class TGuard, class TEvent> void log_guard(const TGuard&, const TEvent&, bool result)
  {
    if (false)
      fmt::print(std::cerr,"[{}][guard] {} {} {}\n", sml::aux::get_type_name<SM>(), sml::aux::get_type_name<TGuard>(),
		 sml::aux::get_type_name<TEvent>(), (result ? "[OK]" : "[Reject]"));
  }

  template <class SM, class TAction, class TEvent> void log_action(const TAction&, const TEvent&)
  {
    if (false)
      fmt::print(std::cerr,"[{}][action] {} {}\n", sml::aux::get_type_name<SM>(), sml::aux::get_type_name<TAction>(),
		 sml::aux::get_type_name<TEvent>());
  }

  template <class SM, class TSrcState, class TDstState>
  void log_state_change(const TSrcState& src, const TDstState& dst)
  {
    if (false)
    fmt::print(std::cerr,"\n##########\n[{}][transition] {} -> {}\n\n", sml::aux::get_type_name<SM>(), src.c_str(), dst.c_str());
  }
};
}
