#ifndef CTYAML__SERIALIZE__HPP
#define CTYAML__SERIALIZE__HPP

#include "types.hpp"
#include "bind.hpp"
#ifndef CTYAML_IN_A_MODULE
#include <array>
#include <cstddef>
#include <string_view>
#endif

// Compile-time serialization: ctyaml::serialize(doc) renders any
// document value back to single-line, flow-style YAML in static
// storage and returns a std::string_view of it - nothing happens at
// runtime.
//
//   constexpr auto doc = ctyaml::parse<"a:\n  - 1\n  - two\n">();
//   static_assert(ctyaml::serialize(doc) == "{a: [1, two]}");
//
// Strings are written plain when that reads back as the same string -
// no character YAML could mistake for structure, and not a spelling
// that would resolve to null, a boolean or a number - and
// double-quoted with escapes otherwise. Numbers keep the spelling they
// were parsed with, so the output always re-parses to the same
// document.

namespace ctyaml {

namespace detail {

// safe to emit without quotes, inside flow collections included
constexpr bool plain_safe(std::string_view v) noexcept {
	if (v.empty()) {
		return false;
	}
	// a spelling the reader would resolve into a non-string
	if (plain_null(v) || plain_true(v) || plain_false(v) || plain_int(v) || plain_float(v)) {
		return false;
	}
	if (v == "...") {
		return false;
	}
	if (v.front() == ' ' || v.back() == ' ') {
		return false;
	}
	constexpr std::string_view first_forbidden = ",[]{}#&*!|>'\"%@`-?:";
	for (const char c : first_forbidden) {
		if (v[0] == c) {
			return false;
		}
	}
	for (const char c : v) {
		if (static_cast<unsigned char>(c) < 0x20 || c == 0x7F) {
			return false;
		}
		// flow indicators, and the two characters whose meaning depends
		// on a neighbouring space; quoting is cheaper than the analysis
		if (c == ',' || c == '[' || c == ']' || c == '{' || c == '}' || c == ':' || c == '#') {
			return false;
		}
	}
	return true;
}

constexpr bool yaml_needs_escape(char c) noexcept {
	return c == '"' || c == '\\' || static_cast<unsigned char>(c) < 0x20;
}

constexpr size_t yaml_escaped_size(char c) noexcept {
	if (!yaml_needs_escape(c)) {
		return 1;
	}
	switch (c) {
		case '"': case '\\': case '\0': case '\a': case '\b': case '\t':
		case '\n': case '\v': case '\f': case '\r': case '\x1b':
			return 2;
		default:
			return 6; // \u00XX
	}
}

constexpr char * yaml_write_escaped(char * out, char c) noexcept {
	if (!yaml_needs_escape(c)) {
		*out++ = c;
		return out;
	}
	*out++ = '\\';
	switch (c) {
		case '"': *out++ = '"'; return out;
		case '\\': *out++ = '\\'; return out;
		case '\0': *out++ = '0'; return out;
		case '\a': *out++ = 'a'; return out;
		case '\b': *out++ = 'b'; return out;
		case '\t': *out++ = 't'; return out;
		case '\n': *out++ = 'n'; return out;
		case '\v': *out++ = 'v'; return out;
		case '\f': *out++ = 'f'; return out;
		case '\r': *out++ = 'r'; return out;
		case '\x1b': *out++ = 'e'; return out;
		default: {
			constexpr char hex[] = "0123456789abcdef";
			*out++ = 'u';
			*out++ = '0';
			*out++ = '0';
			*out++ = hex[(static_cast<unsigned char>(c) >> 4) & 0xF];
			*out++ = hex[static_cast<unsigned char>(c) & 0xF];
			return out;
		}
	}
}

// --- size pass
//
// declared up front: sequences hold mappings and mappings hold
// sequences, and the recursive calls are not found by ADL (the
// document types live in ctyaml, these helpers in ctyaml::detail)

template <auto... Cs> constexpr size_t serialized_size(string<Cs...>) noexcept;
template <auto... Cs> constexpr size_t serialized_size(number<Cs...>) noexcept;
constexpr size_t serialized_size(boolean<true>) noexcept;
constexpr size_t serialized_size(boolean<false>) noexcept;
constexpr size_t serialized_size(null) noexcept;
template <typename... Values> constexpr size_t serialized_size(sequence<Values...>) noexcept;
template <typename... Members> constexpr size_t serialized_size(mapping<Members...>) noexcept;

template <auto... Cs> constexpr char * serialize_to(char * out, string<Cs...>) noexcept;
template <auto... Cs> constexpr char * serialize_to(char * out, number<Cs...>) noexcept;
constexpr char * serialize_to(char * out, boolean<true>) noexcept;
constexpr char * serialize_to(char * out, boolean<false>) noexcept;
constexpr char * serialize_to(char * out, null) noexcept;
template <typename... Values> constexpr char * serialize_to(char * out, sequence<Values...>) noexcept;
template <typename... Members> constexpr char * serialize_to(char * out, mapping<Members...>) noexcept;

template <auto... Cs> constexpr size_t serialized_size(string<Cs...>) noexcept {
	constexpr auto v = string<Cs...>::view();
	if (plain_safe(v)) {
		return v.size();
	}
	size_t total = 2; // the quotes
	((total += yaml_escaped_size(static_cast<char>(Cs))), ...);
	return total;
}

template <auto... Cs> constexpr size_t serialized_size(number<Cs...>) noexcept {
	return sizeof...(Cs);
}

constexpr size_t serialized_size(boolean<true>) noexcept { return 4; }
constexpr size_t serialized_size(boolean<false>) noexcept { return 5; }
constexpr size_t serialized_size(null) noexcept { return 4; }

template <typename... Values> constexpr size_t serialized_size(sequence<Values...>) noexcept {
	size_t total = 2; // the brackets
	((total += serialized_size(Values{})), ...);
	if constexpr (sizeof...(Values) > 1) {
		total += (sizeof...(Values) - 1) * 2; // the ", " separators
	}
	return total;
}

template <typename... Members> constexpr size_t serialized_size(mapping<Members...>) noexcept {
	size_t total = 2; // the braces
	((total += serialized_size(typename Members::key_type{}) + 2 + serialized_size(typename Members::value_type{})), ...);
	if constexpr (sizeof...(Members) > 1) {
		total += (sizeof...(Members) - 1) * 2; // the ", " separators
	}
	return total;
}

// --- write pass

constexpr char * yaml_write_literal(char * out, std::string_view text) noexcept {
	for (const char c : text) {
		*out++ = c;
	}
	return out;
}

template <auto... Cs> constexpr char * serialize_to(char * out, string<Cs...>) noexcept {
	constexpr auto v = string<Cs...>::view();
	if (plain_safe(v)) {
		return yaml_write_literal(out, v);
	}
	*out++ = '"';
	((out = yaml_write_escaped(out, static_cast<char>(Cs))), ...);
	*out++ = '"';
	return out;
}

template <auto... Cs> constexpr char * serialize_to(char * out, number<Cs...>) noexcept {
	((*out++ = static_cast<char>(Cs)), ...);
	return out;
}

constexpr char * serialize_to(char * out, boolean<true>) noexcept { return yaml_write_literal(out, "true"); }
constexpr char * serialize_to(char * out, boolean<false>) noexcept { return yaml_write_literal(out, "false"); }
constexpr char * serialize_to(char * out, null) noexcept { return yaml_write_literal(out, "null"); }

template <typename... Values> constexpr char * serialize_to(char * out, sequence<Values...>) noexcept {
	*out++ = '[';
	size_t index = 0;
	(((index++ != 0 ? (void)(out = yaml_write_literal(out, ", ")) : void()), out = serialize_to(out, Values{})), ...);
	*out++ = ']';
	return out;
}

template <typename... Members> constexpr char * serialize_to(char * out, mapping<Members...>) noexcept {
	*out++ = '{';
	size_t index = 0;
	(((index++ != 0 ? (void)(out = yaml_write_literal(out, ", ")) : void()),
	  out = serialize_to(out, typename Members::key_type{}),
	  out = yaml_write_literal(out, ": "),
	  out = serialize_to(out, typename Members::value_type{})), ...);
	*out++ = '}';
	return out;
}

// the rendered document lives in static storage, one array per type
template <typename Node> struct serialized_storage {
	static constexpr size_t length = serialized_size(Node{});
	// one extra element keeps the rendering null-terminated
	static constexpr std::array<char, length + 1> compute() noexcept {
		std::array<char, length + 1> out{};
		serialize_to(out.data(), Node{});
		return out;
	}
	static constexpr std::array<char, length + 1> content = compute();
};

} // namespace detail

// flow-style YAML for any document value, in static storage
CTLL_EXPORT template <typename Node> constexpr std::string_view serialize(Node = Node{}) noexcept {
	using storage = detail::serialized_storage<Node>;
	return std::string_view{storage::content.data(), storage::length};
}

} // namespace ctyaml

#endif
