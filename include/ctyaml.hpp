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

// what failed and where, when it does not: kind, byte offset, line,
// column and the expected terminals (kind none = the syntax is fine)
CTLL_EXPORT template <CTYAML_STRING_INPUT input> constexpr ctlark::error_info_t error_info() noexcept {
	return ctlark::error_info<detail::yaml_grammar, input, detail::yaml_start>();
}

// the rendered diagnostic - location, snippet with a caret, expected
// terminals - as a static string ("" when the syntax is fine)
CTLL_EXPORT template <CTYAML_STRING_INPUT input> constexpr std::string_view error_message() noexcept {
	return ctlark::error_message<detail::yaml_grammar, input, detail::yaml_start>();
}

// why the binder rejected a document that PARSES - bad escapes,
// duplicate keys, the '...' marker, or inconsistent indentation;
// reason none when the document is valid or the syntax already failed
CTLL_EXPORT template <CTYAML_STRING_INPUT input> constexpr bind_error_t bind_error() noexcept {
	if constexpr (!ctlark::is_valid<detail::yaml_grammar, input, detail::yaml_start>) {
		return bind_error_t{};
	} else {
		using tree_t = decltype(ctlark::parse<detail::yaml_grammar, input, detail::yaml_start>());
		return detail::doc_fail<detail::bind<tree_t>, tree_t>();
	}
}

// parse the input into its document value; invalid YAML fails to compile
CTLL_EXPORT template <CTYAML_STRING_INPUT input> constexpr auto parse() noexcept {
#ifdef CTLARK_VERBOSE_ERRORS
	(void)ctlark::verbose_report<detail::yaml_grammar, input, detail::yaml_start>();
#endif
	static_assert(ctlark::is_valid<detail::yaml_grammar, input, detail::yaml_start>,
	              "ctyaml: the input is not valid YAML syntax (within the supported subset) - print "
	              "ctyaml::error_message<input>() for the location and the expected tokens");
	static_assert(!ctlark::is_valid<detail::yaml_grammar, input, detail::yaml_start> || is_valid<input>,
	              "ctyaml: the input parses but fails a structural rule (indentation, duplicate key, "
	              "escape, or '...') - print ctyaml::bind_error<input>() for the reason");
	if constexpr (is_valid<input>) {
		using bound = detail::bind<decltype(ctlark::parse<detail::yaml_grammar, input, detail::yaml_start>())>;
		return typename bound::type{};
	} else {
		return null{};
	}
}

// the ctlark debugging toolbox with the YAML grammar baked in: traced
// parses (also runnable at runtime under a debugger), runtime inputs
// against the compile-time tables, token and grammar dumps
namespace debug {

CTLL_EXPORT template <CTYAML_STRING_INPUT input, size_t Cap = 4096> constexpr auto traced_parse() noexcept {
	return ctlark::debug::traced_parse<detail::yaml_grammar, input, detail::yaml_start, Cap>();
}

CTLL_EXPORT template <CTYAML_STRING_INPUT input> constexpr std::string_view dump_tokens() noexcept {
	return ctlark::debug::dump_tokens<detail::yaml_grammar, input, detail::yaml_start>();
}

CTLL_EXPORT constexpr std::string_view dump_grammar() noexcept {
	return ctlark::debug::dump_grammar<detail::yaml_grammar>();
}

CTLL_EXPORT template <size_t MaxTokens = 1024>
ctlark::debug::runtime_result parse_runtime(std::string_view in) {
	return ctlark::debug::parse_runtime<detail::yaml_grammar, MaxTokens>(in, "start");
}

} // namespace debug

} // namespace ctyaml

#endif
