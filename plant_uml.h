#pragma once

#include <set>

template <typename>
struct is_sub_state_machine : std::false_type
{};

template <class T, class... Ts>
struct is_sub_state_machine<boost::sml::back::sm<boost::sml::back::sm_policy<T, Ts...>>>
  : std::true_type
{};

template <typename>
struct state_machine_impl : std::false_type
{};

using strset_t = std::set<std::string>;

template <class T>
void dump_transition(strset_t& substates_handled, int& starts) noexcept {


  auto src_state = std::string{sml::aux::string<typename T::src_state>{}.c_str()};
  auto dst_state = std::string{sml::aux::string<typename T::dst_state>{}.c_str()};
  if (dst_state == "X") {
    dst_state = "[*]";
  }
  if (dst_state == "terminate") {
    dst_state = "[*]";
  }

  if (T::initial) {
    fmt::print(std::cout, "{}[*] --> {}\n", (starts++ ? "--\n" : ""), src_state);
  }

  if constexpr (is_sub_state_machine<typename T::dst_state>::value) {

    auto [loc, suc] = substates_handled.insert(dst_state);

    if (suc) {
      fmt::print(std::cout, "\nstate {} {{\n", dst_state);
      int new_starts{0};
      dump_transitions(substates_handled, new_starts, typename T::dst_state::transitions{});
      fmt::print(std::cout, "}}\n");
    }
  }

  fmt::print(std::cout, "{} --> {}", src_state, dst_state);

  const auto has_event = !sml::aux::is_same<typename T::event, sml::anonymous>::value;
  const auto has_guard = !sml::aux::is_same<typename T::guard, sml::front::always>::value;
  const auto has_action = !sml::aux::is_same<typename T::action, sml::front::none>::value;

  if (has_event || has_guard || has_action) {
    fmt::print(std::cout, " :");
  }

  if (has_event) {
    fmt::print(std::cout, " {}", boost::sml::aux::get_type_name<typename T::event>());
  }

  if (has_guard) {
    fmt::print(std::cout, " [{}]", boost::sml::aux::get_type_name<typename T::guard::type>());
  }

  if (has_action) {
    fmt::print(std::cout, " / {}", boost::sml::aux::get_type_name<typename T::action::type>());
  }

  fmt::print(std::cout, "\n");
}

template <template <class...> class T, class... Ts>
void dump_transitions(strset_t& substates_handled, int& starts, const T<Ts...>&) noexcept {
  int _[]{0, (dump_transition<Ts>(substates_handled, starts), 0)...};
  (void)_;
}

template <class SM>
void dump(const SM&) noexcept {

  std::set<std::string> substates_handled;
  int starts{0};

  //std::cout << "@startuml" << std::endl << std::endl;

  dump_transitions(substates_handled, starts, typename SM::transitions{});

  //std::cout << std::endl << "@enduml" << std::endl;
}
