#ifndef CTYAML__BIND__HPP
#define CTYAML__BIND__HPP

#include "grammar.hpp"
#include "types.hpp"
#ifndef CTYAML_IN_A_MODULE
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>
#endif

// Lowering a ctlark parse tree into ctyaml's document types. The
// grammar is line oriented, so this binder does what a YAML parser's
// block-structure pass does: it first flattens the tree into a list of
// (indent, content) lines - the indent read off each NL token, which
// carries its line's leading spaces - then rebuilds the nesting by
// recursive descent over that list. A mapping consumes pair lines at
// one indent, a sequence consumes dash lines at one indent, a `key:`
// with nothing after it takes the deeper block that follows (or a
// sequence at ITS OWN indent - YAML's one deliberate irregularity),
// and `- key: value` re-enters the machinery as a synthetic line
// indented past the dash, whose width the DASH token kept for exactly
// this purpose.
//
// Scalars resolve here too, YAML 1.2 core schema: plain `true`, `~`,
// `0x1F` or `12.5` become boolean/null/number values, quoted scalars
// are always strings, and mapping KEYS are always strings (tag
// resolution applies to values; `get<"true">()` should find the key
// spelled true). Double-quote escapes decode to UTF-8, \xXX \uXXXX
// \UXXXXXXXX included. What the grammar cannot check, bind<Tree>::ok
// folds over the whole document - consistent indentation, no duplicate
// keys, valid escapes, code points below 0x110000 and no surrogates,
// no `...` document-end marker (multi-document streams are not
// supported) - and is_valid includes it.

namespace ctyaml::detail {

// tree data and token type names, as they appear in the parse tree
using bt_start = ctlark::text<'s', 't', 'a', 'r', 't'>;
using bt_pair = ctlark::text<'p', 'a', 'i', 'r'>;
using bt_seqitem = ctlark::text<'s', 'e', 'q', 'i', 't', 'e', 'm'>;
using bt_plain = ctlark::text<'p', 'l', 'a', 'i', 'n'>;
using bt_dquoted = ctlark::text<'d', 'q', 'u', 'o', 't', 'e', 'd'>;
using bt_squoted = ctlark::text<'s', 'q', 'u', 'o', 't', 'e', 'd'>;
using bt_flowseq = ctlark::text<'f', 'l', 'o', 'w', 's', 'e', 'q'>;
using bt_flowmap = ctlark::text<'f', 'l', 'o', 'w', 'm', 'a', 'p'>;
using bt_fpair = ctlark::text<'f', 'p', 'a', 'i', 'r'>;
using bt_NL = ctlark::text<'N', 'L'>;
using bt_DASH = ctlark::text<'D', 'A', 'S', 'H'>;
using bt_SCALAR = ctlark::text<'S', 'C', 'A', 'L', 'A', 'R'>;
using bt_FSCALAR = ctlark::text<'F', 'S', 'C', 'A', 'L', 'A', 'R'>;
using bt_DQ = ctlark::text<'D', 'Q'>;
using bt_SQ = ctlark::text<'S', 'Q'>;

constexpr int bind_hexval(char c) noexcept {
	if (c >= '0' && c <= '9') { return c - '0'; }
	if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
	return c - 'A' + 10;
}

constexpr bool bind_ishex(char c) noexcept {
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// --- plain scalar resolution (YAML 1.2 core schema)

constexpr bool plain_null(std::string_view v) noexcept {
	return v == "~" || v == "null" || v == "Null" || v == "NULL";
}
constexpr bool plain_true(std::string_view v) noexcept {
	return v == "true" || v == "True" || v == "TRUE";
}
constexpr bool plain_false(std::string_view v) noexcept {
	return v == "false" || v == "False" || v == "FALSE";
}

constexpr bool plain_int(std::string_view v) noexcept {
	if (v.size() > 2 && v[0] == '0' && v[1] == 'x') {
		for (size_t i = 2; i < v.size(); ++i) {
			if (!bind_ishex(v[i])) { return false; }
		}
		return true;
	}
	if (v.size() > 2 && v[0] == '0' && v[1] == 'o') {
		for (size_t i = 2; i < v.size(); ++i) {
			if (v[i] < '0' || v[i] > '7') { return false; }
		}
		return true;
	}
	size_t i = (v[0] == '+' || v[0] == '-') ? 1 : 0;
	if (i == v.size()) { return false; }
	for (; i < v.size(); ++i) {
		if (v[i] < '0' || v[i] > '9') { return false; }
	}
	return true;
}

constexpr bool plain_float(std::string_view v) noexcept {
	const size_t from = (v[0] == '+' || v[0] == '-') ? 1 : 0;
	const std::string_view b = v.substr(from);
	if (b == ".inf" || b == ".Inf" || b == ".INF") { return true; }
	if (from == 0 && (b == ".nan" || b == ".NaN" || b == ".NAN")) { return true; }
	// [0-9]* (. [0-9]*)? ([eE][+-]?[0-9]+)? with a digit somewhere and a
	// dot or an exponent to tell it from an int
	bool digit = false;
	bool dot = false;
	bool exp = false;
	size_t i = from;
	for (; i < v.size() && v[i] >= '0' && v[i] <= '9'; ++i) { digit = true; }
	if (i < v.size() && v[i] == '.') {
		dot = true;
		++i;
		for (; i < v.size() && v[i] >= '0' && v[i] <= '9'; ++i) { digit = true; }
	}
	if (!digit) { return false; }
	if (i < v.size() && (v[i] == 'e' || v[i] == 'E')) {
		exp = true;
		++i;
		if (i < v.size() && (v[i] == '+' || v[i] == '-')) { ++i; }
		if (i == v.size()) { return false; }
		for (; i < v.size(); ++i) {
			if (v[i] < '0' || v[i] > '9') { return false; }
		}
	}
	return i == v.size() && (dot || exp);
}

// lifting a ctlark text into document types
template <typename Text> struct make_string_t;
template <auto... Cs> struct make_string_t<ctlark::text<Cs...>> {
	using type = ctyaml::string<Cs...>;
};

template <typename Text> struct resolve_plain;
template <auto... Cs> struct resolve_plain<ctlark::text<Cs...>> {
	static constexpr auto pick() noexcept {
		constexpr std::string_view v = ctlark::text<Cs...>::view();
		if constexpr (plain_null(v)) {
			return ctyaml::null{};
		} else if constexpr (plain_true(v)) {
			return ctyaml::boolean<true>{};
		} else if constexpr (plain_false(v)) {
			return ctyaml::boolean<false>{};
		} else if constexpr (plain_int(v) || plain_float(v)) {
			return ctyaml::number<Cs...>{};
		} else {
			return ctyaml::string<Cs...>{};
		}
	}
	using type = decltype(pick());
};

// --- decoding a raw DQ token (still quoted, escapes intact)

template <typename Text> struct decode_dq {
	struct out_t {
		// \L and \P grow two input chars into three UTF-8 bytes
		char buf[Text::size() * 2 + 1]{};
		size_t len = 0;
		bool ok = true;
	};

	static constexpr void put_code_point(out_t & o, unsigned long cp) noexcept {
		// \u names a code point directly: surrogates have no meaning here
		if ((cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) {
			o.ok = false;
			return;
		}
		if (cp < 0x80) {
			o.buf[o.len++] = static_cast<char>(cp);
		} else if (cp < 0x800) {
			o.buf[o.len++] = static_cast<char>(0xC0 | (cp >> 6));
			o.buf[o.len++] = static_cast<char>(0x80 | (cp & 0x3F));
		} else if (cp < 0x10000) {
			o.buf[o.len++] = static_cast<char>(0xE0 | (cp >> 12));
			o.buf[o.len++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
			o.buf[o.len++] = static_cast<char>(0x80 | (cp & 0x3F));
		} else {
			o.buf[o.len++] = static_cast<char>(0xF0 | (cp >> 18));
			o.buf[o.len++] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
			o.buf[o.len++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
			o.buf[o.len++] = static_cast<char>(0x80 | (cp & 0x3F));
		}
	}

	static constexpr out_t compute() noexcept {
		out_t o{};
		constexpr std::string_view raw = Text::view();
		size_t i = 1;                      // the grammar guarantees the
		const size_t end = raw.size() - 1; // surrounding quotes
		while (i < end) {
			const char c = raw[i];
			if (c != '\\') {
				o.buf[o.len++] = c;
				++i;
				continue;
			}
			const char e = raw[i + 1]; // the grammar guarantees a follower
			size_t hex_digits = 0;
			switch (e) {
				case '0': o.buf[o.len++] = '\0'; break;
				case 'a': o.buf[o.len++] = '\a'; break;
				case 'b': o.buf[o.len++] = '\b'; break;
				case 't': case '\t': o.buf[o.len++] = '\t'; break;
				case 'n': o.buf[o.len++] = '\n'; break;
				case 'v': o.buf[o.len++] = '\v'; break;
				case 'f': o.buf[o.len++] = '\f'; break;
				case 'r': o.buf[o.len++] = '\r'; break;
				case 'e': o.buf[o.len++] = '\x1b'; break;
				case ' ': o.buf[o.len++] = ' '; break;
				case '"': o.buf[o.len++] = '"'; break;
				case '/': o.buf[o.len++] = '/'; break;
				case '\\': o.buf[o.len++] = '\\'; break;
				case 'N': put_code_point(o, 0x85); break;
				case '_': put_code_point(o, 0xA0); break;
				case 'L': put_code_point(o, 0x2028); break;
				case 'P': put_code_point(o, 0x2029); break;
				case 'x': hex_digits = 2; break;
				case 'u': hex_digits = 4; break;
				case 'U': hex_digits = 8; break;
				default: o.ok = false; return o;
			}
			if (hex_digits == 0) {
				if (!o.ok) { return o; }
				i += 2;
				continue;
			}
			if (i + 2 + hex_digits > end) {
				o.ok = false;
				return o;
			}
			unsigned long cp = 0;
			for (size_t k = i + 2; k < i + 2 + hex_digits; ++k) {
				if (!bind_ishex(raw[k])) {
					o.ok = false;
					return o;
				}
				cp = cp * 16 + static_cast<unsigned long>(bind_hexval(raw[k]));
			}
			put_code_point(o, cp);
			if (!o.ok) { return o; }
			i += 2 + hex_digits;
		}
		return o;
	}

	static constexpr out_t data = compute();
	static constexpr bool ok = data.ok;

	template <size_t... I> static constexpr auto lift(std::index_sequence<I...>) noexcept {
		return ctyaml::string<data.buf[I]...>{};
	}
	using type = decltype(lift(std::make_index_sequence<data.len>{}));
};

// --- decoding a raw SQ token ('' is the only escape)

template <typename Text> struct decode_sq {
	struct out_t {
		char buf[Text::size() + 1]{};
		size_t len = 0;
	};

	static constexpr out_t compute() noexcept {
		out_t o{};
		constexpr std::string_view raw = Text::view();
		size_t i = 1;
		const size_t end = raw.size() - 1;
		while (i < end) {
			o.buf[o.len++] = raw[i];
			i += raw[i] == '\'' ? 2 : 1;
		}
		return o;
	}

	static constexpr out_t data = compute();

	template <size_t... I> static constexpr auto lift(std::index_sequence<I...>) noexcept {
		return ctyaml::string<data.buf[I]...>{};
	}
	using type = decltype(lift(std::make_index_sequence<data.len>{}));
};

// --- mapping keys: always strings, raw for plain and decoded for quoted

template <typename Token> struct bind_key;
template <typename V> struct bind_key<ctlark::token<bt_SCALAR, V>> {
	using type = typename make_string_t<V>::type;
	static constexpr bool ok = true;
};
template <typename V> struct bind_key<ctlark::token<bt_FSCALAR, V>> {
	using type = typename make_string_t<V>::type;
	static constexpr bool ok = true;
};
template <typename V> struct bind_key<ctlark::token<bt_DQ, V>> {
	using decoded = decode_dq<V>;
	using type = typename decoded::type;
	static constexpr bool ok = decoded::ok;
};
template <typename V> struct bind_key<ctlark::token<bt_SQ, V>> {
	using type = typename decode_sq<V>::type;
	static constexpr bool ok = true;
};

// equal content is equal type, so uniqueness is type inequality
template <typename Key, typename... Ms> constexpr size_t key_count() noexcept {
	return (static_cast<size_t>(std::is_same_v<Key, typename Ms::key_type>) + ... + 0);
}
template <typename... Ms> constexpr bool members_unique() noexcept {
	return ((key_count<typename Ms::key_type, Ms...>() == 1) && ... && true);
}

// --- single-line values: scalars and flow collections bind directly

template <typename Node> struct bind_value;
template <typename Node> struct bind_fpair;

template <typename Tok> struct bind_value<ctlark::tree<bt_plain, Tok>> {
	using type = typename resolve_plain<typename Tok::value_type>::type;
	static constexpr bool ok = true;
};
template <typename Tok> struct bind_value<ctlark::tree<bt_dquoted, Tok>> {
	using decoded = decode_dq<typename Tok::value_type>;
	using type = typename decoded::type;
	static constexpr bool ok = decoded::ok;
};
template <typename Tok> struct bind_value<ctlark::tree<bt_squoted, Tok>> {
	using type = typename decode_sq<typename Tok::value_type>::type;
	static constexpr bool ok = true;
};
template <typename... Vs> struct bind_value<ctlark::tree<bt_flowseq, Vs...>> {
	using type = ctyaml::sequence<typename bind_value<Vs>::type...>;
	static constexpr bool ok = (bind_value<Vs>::ok && ... && true);
};
template <typename... Ps> struct bind_value<ctlark::tree<bt_flowmap, Ps...>> {
	using type = ctyaml::mapping<typename bind_fpair<Ps>::type...>;
	static constexpr bool ok = (bind_fpair<Ps>::ok && ... && true)
	    && members_unique<typename bind_fpair<Ps>::type...>();
};

template <typename K> struct bind_fpair<ctlark::tree<bt_fpair, K>> {
	using key = bind_key<K>;
	using type = ctyaml::member<typename key::type, ctyaml::null>;
	static constexpr bool ok = key::ok;
};
template <typename K, typename V> struct bind_fpair<ctlark::tree<bt_fpair, K, V>> {
	using key = bind_key<K>;
	using val = bind_value<V>;
	using type = ctyaml::member<typename key::type, typename val::type>;
	static constexpr bool ok = key::ok && val::ok;
};

// --- flattening the parse tree into (indent, content) lines

template <size_t Indent, typename Content> struct ln { };

template <typename Node> struct node_is_nl : std::false_type { };
template <typename V> struct node_is_nl<ctlark::token<bt_NL, V>> : std::true_type { };

template <typename Node> struct node_is_pair : std::false_type { };
template <typename... Cs> struct node_is_pair<ctlark::tree<bt_pair, Cs...>> : std::true_type { };

template <typename Node> struct node_is_seqitem : std::false_type { };
template <typename... Cs> struct node_is_seqitem<ctlark::tree<bt_seqitem, Cs...>> : std::true_type { };

// `...` at column 0 is the document-end marker: not supported, and too
// easily meant as one to let it silently become a scalar
template <typename Node> struct node_is_doc_end : std::false_type { };
template <typename V> struct node_is_doc_end<ctlark::tree<bt_plain, ctlark::token<bt_SCALAR, V>>> {
	static constexpr bool value = V::view() == std::string_view{"..."};
};

// the indentation a NL token carries: everything after its newline
template <typename Tok> constexpr size_t nl_indent() noexcept {
	constexpr std::string_view v = Tok::value_type::view();
	size_t i = 0;
	while (v[i] != '\n') { ++i; }
	return v.size() - i - 1;
}

template <bool Ok, typename Lines> struct gathered {
	static constexpr bool ok = Ok;
	using lines = Lines;
};

template <size_t Cur, bool Ok, typename... Ls>
constexpr auto gather(std::integral_constant<size_t, Cur>, gathered<Ok, ctll::list<Ls...>> g) noexcept {
	return g;
}
template <size_t Cur, bool Ok, typename... Ls, typename Head, typename... Rest>
constexpr auto gather(std::integral_constant<size_t, Cur> cur, gathered<Ok, ctll::list<Ls...>>, Head,
                      Rest... rest) noexcept {
	if constexpr (node_is_nl<Head>::value) {
		return gather(std::integral_constant<size_t, nl_indent<Head>()>{}, gathered<Ok, ctll::list<Ls...>>{},
		              rest...);
	} else {
		constexpr bool doc_end = Cur == 0 && node_is_doc_end<Head>::value;
		return gather(cur, gathered<Ok && !doc_end, ctll::list<Ls..., ln<Cur, Head>>>{}, rest...);
	}
}

// --- block structure: recursive descent over the line list
//
// Every parse returns a blk: the bound type, the folded ok, and the
// lines it did not consume (its caller decides whether those are the
// next entry at ITS indent or an indentation error).

template <typename Type, bool Ok, typename Rest> struct blk {
	using type = Type;
	static constexpr bool ok = Ok;
	using rest = Rest;
};

template <typename Lines> struct parse_block; // head line starts the block
template <size_t I, bool SameIndentSeq, typename Lines> struct nested_value;
template <size_t I, typename Ms, bool Ok, typename Lines> struct map_loop;
template <size_t I, typename Ms, bool Ok, typename Pair, typename RestList> struct map_entry;
template <size_t I, typename Is, bool Ok, typename Lines> struct seq_loop;
template <size_t I, typename Is, bool Ok, typename Item, typename RestList> struct seq_entry;

template <size_t J, typename C, typename... Rest> struct parse_block<ctll::list<ln<J, C>, Rest...>> {
	static constexpr auto make() noexcept {
		if constexpr (node_is_pair<C>::value) {
			return map_loop<J, ctll::list<>, true, ctll::list<ln<J, C>, Rest...>>::make();
		} else if constexpr (node_is_seqitem<C>::value) {
			return seq_loop<J, ctll::list<>, true, ctll::list<ln<J, C>, Rest...>>::make();
		} else {
			using v = bind_value<C>;
			return blk<typename v::type, v::ok, ctll::list<Rest...>>{};
		}
	}
};

// the value of a `key:` or `-` with nothing after it: the deeper block
// that follows, a sequence at the same indent (mapping values only), or
// null when neither is there
template <size_t I, bool S> struct nested_value<I, S, ctll::list<>> {
	static constexpr auto make() noexcept {
		return blk<ctyaml::null, true, ctll::list<>>{};
	}
};
template <size_t I, bool S, size_t J, typename C, typename... Rest>
struct nested_value<I, S, ctll::list<ln<J, C>, Rest...>> {
	static constexpr auto make() noexcept {
		if constexpr (J > I) {
			return parse_block<ctll::list<ln<J, C>, Rest...>>::make();
		} else if constexpr (S && J == I && node_is_seqitem<C>::value) {
			return seq_loop<I, ctll::list<>, true, ctll::list<ln<J, C>, Rest...>>::make();
		} else {
			return blk<ctyaml::null, true, ctll::list<ln<J, C>, Rest...>>{};
		}
	}
};

// a mapping: pair lines at one indent
template <size_t I, typename... Ms, bool Ok> struct map_loop<I, ctll::list<Ms...>, Ok, ctll::list<>> {
	static constexpr auto make() noexcept {
		return blk<ctyaml::mapping<Ms...>, Ok && members_unique<Ms...>(), ctll::list<>>{};
	}
};
template <size_t I, typename... Ms, bool Ok, size_t J, typename C, typename... Rest>
struct map_loop<I, ctll::list<Ms...>, Ok, ctll::list<ln<J, C>, Rest...>> {
	static constexpr auto make() noexcept {
		if constexpr (J > I) {
			// a deeper line where a key was expected: the previous value
			// already ended, so this indentation belongs to nothing
			return blk<ctyaml::null, false, ctll::list<>>{};
		} else if constexpr (J < I || !node_is_pair<C>::value) {
			// the block ends here; whether what follows is legal is the
			// caller's question (a root-level leftover is an error there)
			return blk<ctyaml::mapping<Ms...>, Ok && members_unique<Ms...>(), ctll::list<ln<J, C>, Rest...>>{};
		} else {
			return map_entry<I, ctll::list<Ms...>, Ok, C, ctll::list<Rest...>>::make();
		}
	}
};

template <size_t I, typename... Ms, bool Ok, typename K, typename RestList>
struct map_entry<I, ctll::list<Ms...>, Ok, ctlark::tree<bt_pair, K>, RestList> {
	using key = bind_key<K>;
	static constexpr auto make() noexcept {
		using nested = decltype(nested_value<I, true, RestList>::make());
		return map_loop<I, ctll::list<Ms..., ctyaml::member<typename key::type, typename nested::type>>,
		                Ok && key::ok && nested::ok, typename nested::rest>::make();
	}
};
template <size_t I, typename... Ms, bool Ok, typename K, typename V, typename RestList>
struct map_entry<I, ctll::list<Ms...>, Ok, ctlark::tree<bt_pair, K, V>, RestList> {
	using key = bind_key<K>;
	using val = bind_value<V>;
	static constexpr auto make() noexcept {
		return map_loop<I, ctll::list<Ms..., ctyaml::member<typename key::type, typename val::type>>,
		                Ok && key::ok && val::ok, RestList>::make();
	}
};

// a sequence: dash lines at one indent
template <size_t I, typename... Is, bool Ok> struct seq_loop<I, ctll::list<Is...>, Ok, ctll::list<>> {
	static constexpr auto make() noexcept {
		return blk<ctyaml::sequence<Is...>, Ok, ctll::list<>>{};
	}
};
template <size_t I, typename... Is, bool Ok, size_t J, typename C, typename... Rest>
struct seq_loop<I, ctll::list<Is...>, Ok, ctll::list<ln<J, C>, Rest...>> {
	static constexpr auto make() noexcept {
		if constexpr (J > I) {
			return blk<ctyaml::null, false, ctll::list<>>{};
		} else if constexpr (J < I || !node_is_seqitem<C>::value) {
			// ends here: a sequence at its mapping's indent stops at the
			// next `key:` line, which continues the mapping
			return blk<ctyaml::sequence<Is...>, Ok, ctll::list<ln<J, C>, Rest...>>{};
		} else {
			return seq_entry<I, ctll::list<Is...>, Ok, C, ctll::list<Rest...>>::make();
		}
	}
};

// `-` alone: the item is the deeper block that follows, or null
template <size_t I, typename... Is, bool Ok, typename RestList>
struct seq_entry<I, ctll::list<Is...>, Ok, ctlark::tree<bt_seqitem>, RestList> {
	static constexpr auto make() noexcept {
		using nested = decltype(nested_value<I, false, RestList>::make());
		return seq_loop<I, ctll::list<Is..., typename nested::type>, Ok && nested::ok,
		                typename nested::rest>::make();
	}
};
template <size_t I, typename... Is, bool Ok, typename DashV, typename RestList>
struct seq_entry<I, ctll::list<Is...>, Ok, ctlark::tree<bt_seqitem, ctlark::token<bt_DASH, DashV>>, RestList> {
	static constexpr auto make() noexcept {
		using nested = decltype(nested_value<I, false, RestList>::make());
		return seq_loop<I, ctll::list<Is..., typename nested::type>, Ok && nested::ok,
		                typename nested::rest>::make();
	}
};
// `- content`: re-enter as a synthetic line indented past the dash (the
// DASH token kept its trailing spaces so the width is exact), which is
// what makes `- key: value` a mapping whose siblings sit at that column
template <size_t I, typename... Is, bool Ok, typename DashV, typename Inner, typename... Rest>
struct seq_entry<I, ctll::list<Is...>, Ok, ctlark::tree<bt_seqitem, ctlark::token<bt_DASH, DashV>, Inner>,
                 ctll::list<Rest...>> {
	static constexpr size_t effective = I + DashV::size();
	static constexpr auto make() noexcept {
		using inner = decltype(parse_block<ctll::list<ln<effective, Inner>, Rest...>>::make());
		return seq_loop<I, ctll::list<Is..., typename inner::type>, Ok && inner::ok,
		                typename inner::rest>::make();
	}
};

// --- the document binder

template <typename Node> struct bind;
template <typename... Segs> struct bind<ctlark::tree<bt_start, Segs...>> {
	using gathered_t =
	    decltype(gather(std::integral_constant<size_t, 0>{}, gathered<true, ctll::list<>>{}, Segs{}...));

	static constexpr auto make() noexcept {
		using lines = typename gathered_t::lines;
		if constexpr (std::is_same_v<lines, ctll::list<>>) {
			// an empty document (or only comments and blank lines) is null
			return blk<ctyaml::null, gathered_t::ok, ctll::list<>>{};
		} else {
			using body = decltype(parse_block<lines>::make());
			constexpr bool consumed = std::is_same_v<typename body::rest, ctll::list<>>;
			return blk<typename body::type, gathered_t::ok && body::ok && consumed, ctll::list<>>{};
		}
	}

	using result = decltype(make());
	using type = typename result::type;
	static constexpr bool ok = result::ok;
};

} // namespace ctyaml::detail

#endif
