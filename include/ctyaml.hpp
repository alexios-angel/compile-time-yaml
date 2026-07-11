#ifndef CTYAML__HPP
#define CTYAML__HPP

#include "ctlark.hpp"
#include "ctyaml/grammar.hpp"
#include "ctyaml/types.hpp"
#include "ctyaml/bind.hpp"
#include "ctyaml/serialize.hpp"
#include "ctyaml/views.hpp"

// ctyaml: compile-time YAML.
//
//   constexpr auto doc = ctyaml::parse<R"(
//   server:
//     host: example.com
//     ports: [80, 443]
//     tls: true
//   )">();
//
//   static_assert(doc.get<"server">().get<"host">() == "example.com");
//   static_assert(doc.get<"server">().get<"ports">().get<1>().to<int>() == 443);
//   static_assert(doc.get<"server">().get<"tls">());
//   static_assert(ctyaml::is_valid<"a: 1\nb: 2">);
//   static_assert(!ctyaml::is_valid<"a: 1\n a: bad indent">);
//
// The document is parsed while your code compiles - malformed YAML is
// a compile error (or `false` from is_valid) - and the result is a
// TYPE whose accessors are all constexpr. The grammar layer is ctlark
// (compile-time Lark): the YAML subset's grammar is a lark grammar
// string (grammar.hpp) written line-by-line, because YAML's block
// structure is indentation and indentation is not context free - the
// NL terminal keeps each line's leading spaces, and bind.hpp rebuilds
// the mapping/sequence nesting from those widths while resolving
// scalars (YAML 1.2 core schema), decoding escapes and enforcing what
// the grammar cannot say. See the README for the supported subset;
// what is outside it fails the parse rather than misparsing.

namespace ctyaml {

#if CTLL_CNTTP_COMPILER_CHECK
#define CTYAML_STRING_INPUT ctll::fixed_string
#else
// C++17: pass a constexpr ctll::fixed_string variable with linkage
#define CTYAML_STRING_INPUT const auto &
#endif

namespace detail {

// grammar validity is a given (static_assert in grammar.hpp); input
// validity is the parse plus the binder's structure and escape checks
template <CTYAML_STRING_INPUT input> constexpr bool valid_document() noexcept {
	if constexpr (!ctlark::is_valid<yaml_grammar, input, yaml_start>) {
		return false;
	} else {
		return bind<decltype(ctlark::parse<yaml_grammar, input, yaml_start>())>::ok;
	}
}

} // namespace detail

// does the input parse as YAML (within the supported subset)?
CTLL_EXPORT template <CTYAML_STRING_INPUT input> constexpr bool is_valid =
	detail::valid_document<input>();

// parse the input into its document value; invalid YAML fails to compile
CTLL_EXPORT template <CTYAML_STRING_INPUT input> constexpr auto parse() noexcept {
	static_assert(is_valid<input>, "ctyaml: the input is not valid YAML (within the supported subset)");
	if constexpr (is_valid<input>) {
		using bound = detail::bind<decltype(ctlark::parse<detail::yaml_grammar, input, detail::yaml_start>())>;
		return typename bound::type{};
	} else {
		return null{};
	}
}

} // namespace ctyaml

#endif
