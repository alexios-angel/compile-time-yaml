#ifndef CTYAML__TYPES__HPP
#define CTYAML__TYPES__HPP

#include "../ctll/fixed_string.hpp"
#ifndef CTYAML_IN_A_MODULE
#include <cstddef>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>
#endif

// The document types a parse produces. The whole document is a TYPE -
// every scalar, key and nesting level is encoded in template
// parameters - so the values here are empty structs whose accessors are
// all constexpr and static.
//
// String content is stored as UTF-8 bytes (double-quote escapes,
// \uXXXX and \UXXXXXXXX included, are decoded during parsing); numbers
// keep their raw spelling - decimal, hex, octal, float or .inf/.nan -
// and convert on demand. Mapping keys are strings: YAML's tag
// resolution is applied to values, not keys, so `get<"true">()` finds
// the key spelled true.

namespace ctyaml {

CTLL_EXPORT enum class kind {
	mapping,
	sequence,
	string,
	number,
	boolean,
	null
};

// --- string

CTLL_EXPORT template <auto... Chars> struct string {
	static constexpr kind type = kind::string;

	// null-terminated so c_str()/data() work as C strings; size() excludes it
	static constexpr char storage[sizeof...(Chars) + 1]{static_cast<char>(Chars)..., '\0'};

	static constexpr const char * c_str() noexcept {
		return storage;
	}

	static constexpr size_t size() noexcept {
		return sizeof...(Chars);
	}
	static constexpr bool empty() noexcept {
		return sizeof...(Chars) == 0;
	}
	static constexpr std::string_view view() noexcept {
		return std::string_view{storage, sizeof...(Chars)};
	}
	constexpr operator std::string_view() const noexcept {
		return view();
	}
	template <auto... Rhs> constexpr bool operator==(string<Rhs...>) const noexcept {
		return view() == string<Rhs...>::view();
	}
	friend constexpr bool operator==(string, std::string_view rhs) noexcept {
		return view() == rhs;
	}
	friend constexpr bool operator==(std::string_view lhs, string) noexcept {
		return lhs == view();
	}
};

// --- number (raw spelling, converted on demand; YAML 1.2 core schema)

CTLL_EXPORT template <auto... Chars> struct number {
	static constexpr kind type = kind::number;

	// null-terminated so c_str() works as a C string; view() excludes it
	static constexpr char storage[sizeof...(Chars) + 1]{static_cast<char>(Chars)..., '\0'};

	static constexpr const char * c_str() noexcept {
		return storage;
	}

	static constexpr std::string_view view() noexcept {
		return std::string_view{storage, sizeof...(Chars)};
	}

	static constexpr bool is_integer() noexcept {
		constexpr std::string_view text = view();
		// hex and octal are integers whatever letters they contain
		if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'o')) {
			return true;
		}
		for (const char c : text) {
			if (c == '.' || c == 'e' || c == 'E') {
				return false;
			}
		}
		return true;
	}

	template <typename T> static constexpr T to() noexcept {
		constexpr std::string_view text = view();
		size_t i = 0;
		bool negative = false;
		if (text[0] == '+' || text[0] == '-') {
			negative = text[0] == '-';
			++i;
		}
		// .inf and .nan (the sign, if any, was already consumed)
		if (i < text.size() && text[i] == '.' && i + 1 < text.size()
		    && (text[i + 1] < '0' || text[i + 1] > '9')) {
			if constexpr (std::is_integral_v<T>) {
				return T{}; // no integral infinity; converting is the caller's choice
			} else {
				if (text[i + 1] == 'n' || text[i + 1] == 'N') {
					return std::numeric_limits<T>::quiet_NaN();
				}
				return negative ? -std::numeric_limits<T>::infinity()
				                : std::numeric_limits<T>::infinity();
			}
		}
		// hex and octal integers
		if (i + 1 < text.size() && text[i] == '0' && (text[i + 1] == 'x' || text[i + 1] == 'o')) {
			const unsigned base = text[i + 1] == 'x' ? 16u : 8u;
			unsigned long long value = 0;
			for (size_t k = i + 2; k < text.size(); ++k) {
				const char c = text[k];
				unsigned digit = 0;
				if (c >= '0' && c <= '9') {
					digit = static_cast<unsigned>(c - '0');
				} else if (c >= 'a' && c <= 'f') {
					digit = static_cast<unsigned>(c - 'a') + 10u;
				} else {
					digit = static_cast<unsigned>(c - 'A') + 10u;
				}
				value = value * base + digit;
			}
			return static_cast<T>(value);
		}
		// decimal: mantissa digits, fraction shifting the exponent down
		unsigned long long mantissa = 0;
		int exponent10 = 0;
		for (; i < text.size() && text[i] >= '0' && text[i] <= '9'; ++i) {
			mantissa = mantissa * 10 + static_cast<unsigned long long>(text[i] - '0');
		}
		if (i < text.size() && text[i] == '.') {
			++i;
			for (; i < text.size() && text[i] >= '0' && text[i] <= '9'; ++i) {
				mantissa = mantissa * 10 + static_cast<unsigned long long>(text[i] - '0');
				--exponent10;
			}
		}
		if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
			++i;
			bool exp_negative = false;
			if (text[i] == '+' || text[i] == '-') {
				exp_negative = text[i] == '-';
				++i;
			}
			int exp_value = 0;
			for (; i < text.size(); ++i) {
				exp_value = exp_value * 10 + (text[i] - '0');
			}
			exponent10 += exp_negative ? -exp_value : exp_value;
		}

		if constexpr (std::is_integral_v<T>) {
			long long value = static_cast<long long>(mantissa);
			for (; exponent10 > 0; --exponent10) {
				value *= 10;
			}
			for (; exponent10 < 0; ++exponent10) {
				value /= 10; // truncates fractions, like a cast would
			}
			return static_cast<T>(negative ? -value : value);
		} else {
			long double value = static_cast<long double>(mantissa);
			for (; exponent10 > 0; --exponent10) {
				value *= 10.0L;
			}
			for (; exponent10 < 0; ++exponent10) {
				value /= 10.0L;
			}
			return static_cast<T>(negative ? -value : value);
		}
	}
};

// --- boolean and null

CTLL_EXPORT template <bool Value> struct boolean {
	static constexpr kind type = kind::boolean;
	static constexpr bool value = Value;
	constexpr operator bool() const noexcept {
		return Value;
	}
};

CTLL_EXPORT struct null {
	static constexpr kind type = kind::null;
};

// --- sequence

CTLL_EXPORT template <typename... Values> struct sequence {
	static constexpr kind type = kind::sequence;

	static constexpr size_t size() noexcept {
		return sizeof...(Values);
	}
	static constexpr bool empty() noexcept {
		return sizeof...(Values) == 0;
	}

	template <size_t Index> static constexpr auto get() noexcept {
		static_assert(Index < sizeof...(Values), "ctyaml: sequence index out of range");
		return nth<Index, Values...>();
	}

	// get, spelled with brackets: the index rides in the argument's type
	// (ctyaml::literals: seq[1_i])
	template <size_t Index> constexpr auto operator[](std::integral_constant<size_t, Index>) const noexcept {
		return get<Index>();
	}

private:
	template <size_t Index, typename Head, typename... Tail> static constexpr auto nth() noexcept {
		if constexpr (Index == 0) {
			return Head{};
		} else {
			return nth<Index - 1, Tail...>();
		}
	}
};

// --- mapping

CTLL_EXPORT template <typename Key, typename Value> struct member {
	using key_type = Key;
	using value_type = Value;
};

CTLL_EXPORT template <typename... Members> struct mapping {
	static constexpr kind type = kind::mapping;

	static constexpr size_t size() noexcept {
		return sizeof...(Members);
	}
	static constexpr bool empty() noexcept {
		return sizeof...(Members) == 0;
	}

#if CTLL_CNTTP_COMPILER_CHECK
	template <ctll::fixed_string Name> static constexpr bool contains() noexcept {
		return (key_matches<Name, Members>() || ...);
	}

	// the value of the member with this key; a missing key is a
	// compile-time error (check contains<Name>() first when unsure)
	template <ctll::fixed_string Name> static constexpr auto get() noexcept {
		static_assert((key_matches<Name, Members>() || ...), "ctyaml: no member with this key");
		return find<Name, Members...>();
	}
#else
	// C++17: the key is a ctll::fixed_string variable with linkage
	template <const auto & Name> static constexpr bool contains() noexcept {
		return (key_matches<Name, Members>() || ...);
	}
	template <const auto & Name> static constexpr auto get() noexcept {
		static_assert((key_matches<Name, Members>() || ...), "ctyaml: no member with this key");
		return find<Name, Members...>();
	}
#endif

	// get, spelled with brackets: the key is a document string TYPE, so
	// this works with the "..."_k literal (C++20) and with any key a
	// for_each hands out, in any standard
	template <auto... Chars> constexpr auto operator[](string<Chars...>) const noexcept {
		static_assert((string_key_matches<string<Chars...>, Members>() || ...), "ctyaml: no member with this key");
		return find_string_key<string<Chars...>, Members...>();
	}

	// positional access, for iterating members
	template <size_t Index> static constexpr auto key() noexcept {
		static_assert(Index < sizeof...(Members), "ctyaml: member index out of range");
		return typename decltype(nth<Index, Members...>())::key_type{};
	}
	template <size_t Index> static constexpr auto value() noexcept {
		static_assert(Index < sizeof...(Members), "ctyaml: member index out of range");
		return typename decltype(nth<Index, Members...>())::value_type{};
	}

private:
#if CTLL_CNTTP_COMPILER_CHECK
	template <ctll::fixed_string Name, typename Member> static constexpr bool key_matches() noexcept {
#else
	template <const auto & Name, typename Member> static constexpr bool key_matches() noexcept {
#endif
		constexpr auto key_view = Member::key_type::view();
		if (Name.size() != key_view.size()) {
			return false;
		}
		for (size_t i = 0; i < key_view.size(); ++i) {
			if (static_cast<char32_t>(static_cast<unsigned char>(key_view[i])) != Name[i]) {
				return false;
			}
		}
		return true;
	}

#if CTLL_CNTTP_COMPILER_CHECK
	template <ctll::fixed_string Name, typename Head, typename... Tail> static constexpr auto find() noexcept {
#else
	template <const auto & Name, typename Head, typename... Tail> static constexpr auto find() noexcept {
#endif
		if constexpr (key_matches<Name, Head>()) {
			return typename Head::value_type{};
		} else {
			return find<Name, Tail...>();
		}
	}

	template <size_t Index, typename Head, typename... Tail> static constexpr auto nth() noexcept {
		if constexpr (Index == 0) {
			return Head{};
		} else {
			return nth<Index - 1, Tail...>();
		}
	}

	template <typename Key, typename Member> static constexpr bool string_key_matches() noexcept {
		return Key::view() == Member::key_type::view();
	}

	template <typename Key, typename Head, typename... Tail> static constexpr auto find_string_key() noexcept {
		if constexpr (string_key_matches<Key, Head>()) {
			return typename Head::value_type{};
		} else {
			return find_string_key<Key, Tail...>();
		}
	}
};

// compile-time iteration: the callable is invoked once per element
// (sequences) or once per key/value pair (mappings), each with its own type
CTLL_EXPORT template <typename F, typename... Values> constexpr void for_each(sequence<Values...>, F && f) {
	(f(Values{}), ...);
}

CTLL_EXPORT template <typename F, typename... Members> constexpr void for_each(mapping<Members...>, F && f) {
	(f(typename Members::key_type{}, typename Members::value_type{}), ...);
}

// --- literal suffixes: keys and indexes as types, for operator[]

namespace detail {

#if CTLL_CNTTP_COMPILER_CHECK
template <ctll::fixed_string S, size_t... I> constexpr auto lift_key(std::index_sequence<I...>) noexcept {
	return string<static_cast<char>(S[I])...>{};
}
#endif

template <char... Digits> constexpr size_t parse_index() noexcept {
	constexpr char digits[]{Digits...};
	size_t value = 0;
	for (const char c : digits) {
		if (c != '\'') { // the digit separator
			value = value * 10 + static_cast<size_t>(c - '0');
		}
	}
	return value;
}

} // namespace detail

// opt in with `using namespace ctyaml::literals`
namespace literals {

#if CTLL_CNTTP_COMPILER_CHECK
// "name"_k: the key as a document string type - doc["name"_k]
CTLL_EXPORT template <ctll::fixed_string S> constexpr auto operator""_k() noexcept {
	return detail::lift_key<S>(std::make_index_sequence<S.size()>{});
}
#endif

// 1_i: an index as an integral constant - seq[1_i]
CTLL_EXPORT template <char... Digits> constexpr auto operator""_i() noexcept {
	return std::integral_constant<size_t, detail::parse_index<Digits...>()>{};
}

} // namespace literals

} // namespace ctyaml

#endif
