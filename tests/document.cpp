#include <ctyaml.hpp>

void empty_symbol_17() { }

// the string-literal API needs C++20 class-type template parameters;
// tests/cxx17.cpp covers the C++17 variable form
#if CTLL_CNTTP_COMPILER_CHECK

#include <string_view>
using namespace std::literals;

// --- basics
static_assert(ctyaml::is_valid<"a: 1">);
static_assert(ctyaml::is_valid<"- a\n- b">);
static_assert(ctyaml::is_valid<"plain scalar">);
static_assert(ctyaml::is_valid<"[1, 2, 3]">);
static_assert(ctyaml::is_valid<"{a: 1, b: 2}">);
static_assert(ctyaml::is_valid<"">);           // an empty document is null
static_assert(ctyaml::is_valid<"# just a comment\n\n">);
static_assert(ctyaml::is_valid<"--- doc\n">);  // one document marker is fine

// --- THE feature: structure errors are compile-time properties
static_assert(!ctyaml::is_valid<"a: 1\n a: deeper than its mapping">);
static_assert(!ctyaml::is_valid<"a: 1\na: duplicate key">);
static_assert(!ctyaml::is_valid<"a: 1\n- mixed pair and dash">);
static_assert(!ctyaml::is_valid<"- a\nb: mixed dash and pair">);
static_assert(!ctyaml::is_valid<"a: scalar\n  extra: deeper line after a value">);
static_assert(!ctyaml::is_valid<"root\nsecond root line">);
static_assert(!ctyaml::is_valid<"\tindent: with a tab">);
static_assert(!ctyaml::is_valid<"[1, 2">);          // unclosed flow
static_assert(!ctyaml::is_valid<"[1,\n 2]">);       // flow is single-line
static_assert(!ctyaml::is_valid<"a: [1, 2] junk">); // trailing junk

// --- unsupported YAML fails the parse instead of misparsing
static_assert(!ctyaml::is_valid<"k: &anchor v">);   // anchors
static_assert(!ctyaml::is_valid<"k: *alias">);      // aliases
static_assert(!ctyaml::is_valid<"k: !!str tagged">); // tags
static_assert(!ctyaml::is_valid<"%YAML 1.2\n---\nk: v">); // directives
static_assert(!ctyaml::is_valid<"k: |\n  literal block">); // block scalars
static_assert(!ctyaml::is_valid<"k: >\n  folded block">);
static_assert(!ctyaml::is_valid<"? complex\n: key">); // complex keys
static_assert(!ctyaml::is_valid<"a: 1\n---\nb: 2">); // multi-document streams
static_assert(!ctyaml::is_valid<"a: 1\n...">);       // document-end marker

// --- a real document
constexpr auto doc = ctyaml::parse<R"(
# server configuration
server:
  host: example.com
  ports: [80, 443]
  tls: true
  retry: ~        # core schema null
limits:
  - 10
  - 2.5e3
  - 0x1F
  - 0o17
users:
  - name: hana
    admin: true
  - name: alex
    admin: false
motd: "tab\tnewline\n\xe9 é \U0001F600"
quoted: 'it''s flow-safe: yes'
)">();

static_assert(doc.type == ctyaml::kind::mapping);
static_assert(doc.size() == 5);
static_assert(doc.contains<"server">());
static_assert(!doc.contains<"missing">());
static_assert(doc.key<0>() == "server");   // positional access

static_assert(doc.get<"server">().get<"host">() == "example.com"sv);
static_assert(doc.get<"server">().get<"ports">().size() == 2);
static_assert(doc.get<"server">().get<"ports">().get<1>().to<int>() == 443);
static_assert(doc.get<"server">().get<"tls">());
static_assert(doc.get<"server">().get<"retry">().type == ctyaml::kind::null);

// numbers keep their spelling and convert on demand (core schema)
static_assert(doc.get<"limits">().get<0>().is_integer());
static_assert(doc.get<"limits">().get<1>().to<double>() == 2500.0);
static_assert(!doc.get<"limits">().get<1>().is_integer());
static_assert(doc.get<"limits">().get<2>().to<int>() == 31);   // hex
static_assert(doc.get<"limits">().get<2>().is_integer());
static_assert(doc.get<"limits">().get<3>().to<int>() == 15);   // octal
static_assert(doc.get<"limits">().get<2>().view() == "0x1F"sv);

// a sequence of mappings, the compact `- key: value` form
static_assert(doc.get<"users">().size() == 2);
static_assert(doc.get<"users">().get<0>().get<"name">() == "hana"sv);
static_assert(doc.get<"users">().get<1>().get<"admin">() == false);

// double-quote escapes decode to UTF-8 at parse time
static_assert(doc.get<"motd">() == "tab\tnewline\n\xc3\xa9 \xc3\xa9 \xf0\x9f\x98\x80"sv);
// single quotes: '' is the only escape, everything else is literal
static_assert(doc.get<"quoted">() == "it's flow-safe: yes"sv);

// --- scalar resolution (YAML 1.2 core schema, values only)
static_assert(ctyaml::parse<"v: true">().get<"v">().type == ctyaml::kind::boolean);
static_assert(ctyaml::parse<"v: True">().get<"v">().type == ctyaml::kind::boolean);
static_assert(ctyaml::parse<"v: yes">().get<"v">().type == ctyaml::kind::string); // 1.1 legacy stays a string
static_assert(ctyaml::parse<"v: null">().get<"v">().type == ctyaml::kind::null);
static_assert(ctyaml::parse<"v: ~">().get<"v">().type == ctyaml::kind::null);
static_assert(ctyaml::parse<"v:">().get<"v">().type == ctyaml::kind::null);       // missing value
static_assert(ctyaml::parse<"v: -12">().get<"v">().to<int>() == -12);
static_assert(ctyaml::parse<"v: +12">().get<"v">().to<int>() == 12);
static_assert(ctyaml::parse<"v: .5">().get<"v">().to<double>() == 0.5);
static_assert(ctyaml::parse<"v: .inf">().get<"v">().to<double>() > 1e308);
static_assert(ctyaml::parse<"v: -.inf">().get<"v">().to<double>() < -1e308);
static_assert(ctyaml::parse<"v: .nan">().get<"v">().to<double>() != ctyaml::parse<"v: .nan">().get<"v">().to<double>());
static_assert(ctyaml::parse<"v: 1.2.3">().get<"v">().type == ctyaml::kind::string); // not a number
static_assert(ctyaml::parse<"v: 0x">().get<"v">().type == ctyaml::kind::string);
static_assert(ctyaml::parse<"v: 'true'">().get<"v">().type == ctyaml::kind::string); // quoted never resolves
static_assert(ctyaml::parse<R"(v: "1")">().get<"v">().type == ctyaml::kind::string);

// keys are strings, whatever they would resolve to as values
static_assert(ctyaml::parse<"true: 1">().contains<"true">());
static_assert(ctyaml::parse<"0: zero">().get<"0">() == "zero"sv);
static_assert(ctyaml::parse<R"("a b": quoted key)">().get<"a b">() == "quoted key"sv);
static_assert(ctyaml::parse<"std::vector: 3">().get<"std::vector">().to<int>() == 3);
// a duplicate is a duplicate however it is spelled
static_assert(!ctyaml::is_valid<R"(a: 1
"a": 2)">);

// --- plain scalars: YAML's context rules
static_assert(ctyaml::parse<"v: a:b">().get<"v">() == "a:b"sv);        // no space: one scalar
static_assert(ctyaml::parse<"v: a#b">().get<"v">() == "a#b"sv);        // # after non-space
static_assert(ctyaml::parse<"v: a b  c">().get<"v">() == "a b  c"sv);  // inner spaces kept
static_assert(ctyaml::parse<"v: -1x">().get<"v">() == "-1x"sv);        // dash-led scalar
static_assert(ctyaml::parse<"v: x # c">().get<"v">() == "x"sv);        // comment cut off

// --- block structure edges
// nested sequences, compact form; siblings sit past the dash
constexpr auto nested = ctyaml::parse<"- - a\n  - b\n- k:\n  - 1\n">();
static_assert(nested.get<0>().size() == 2);
static_assert(nested.get<0>().get<1>() == "b"sv);
static_assert(nested.get<1>().get<"k">().get<0>().to<int>() == 1);

// a sequence may sit at its mapping key's own indent
constexpr auto atkey = ctyaml::parse<"key:\n- 1\n- 2\nother: x\n">();
static_assert(atkey.get<"key">().size() == 2);
static_assert(atkey.get<"other">() == "x"sv);

// lone dashes are null items; a dash's block may start on the next line
constexpr auto dashes = ctyaml::parse<"- \n-\n- v\n-\n  k: v\n">();
static_assert(dashes.size() == 4);
static_assert(dashes.get<0>().type == ctyaml::kind::null);
static_assert(dashes.get<2>() == "v"sv);
static_assert(dashes.get<3>().get<"k">() == "v"sv);

// the dash's width decides where its mapping's siblings belong
static_assert(ctyaml::is_valid<"-  k: v\n   j: w">);
static_assert(!ctyaml::is_valid<"-  k: v\n  j: w">);

// blank lines and comment lines never affect structure
constexpr auto sparse = ctyaml::parse<"a: 1\n\n   \n# note\nb: 2\n">();
static_assert(sparse.size() == 2);

// CRLF input parses like LF input
static_assert(ctyaml::parse<"a: 1\r\nb: 2\r\n">().size() == 2);

// a document may open past column 0 only after a blank first line
static_assert(ctyaml::is_valid<"\n  a: 1\n  b: 2">);
static_assert(!ctyaml::is_valid<"  a: 1">);

// --- flow collections
constexpr auto flow = ctyaml::parse<"[a, {b: 1, c}, [2], 'd e', ]">();
static_assert(flow.size() == 4);   // the trailing comma adds nothing
static_assert(flow.get<1>().get<"b">().to<int>() == 1);
static_assert(flow.get<1>().get<"c">().type == ctyaml::kind::null);
static_assert(flow.get<2>().get<0>().to<int>() == 2);
static_assert(flow.get<3>() == "d e"sv);
static_assert(ctyaml::parse<"[]">().empty());
static_assert(ctyaml::parse<"{}">().empty());
static_assert(ctyaml::parse<"{a: }">().get<"a">().type == ctyaml::kind::null);
static_assert(ctyaml::parse<"[a:b]">().get<0>() == "a:b"sv); // glued colon: a scalar
static_assert(!ctyaml::is_valid<"{a: 1, a: 2}">);            // duplicates in flow too

// --- escapes that must fail
static_assert(!ctyaml::is_valid<R"(k: "\q")">);        // unknown escape
static_assert(!ctyaml::is_valid<R"(k: "\ud800")">);    // surrogate code point
static_assert(!ctyaml::is_valid<R"(k: "\U00110000")">); // beyond Unicode
static_assert(!ctyaml::is_valid<R"(k: "\x9")">);       // truncated hex
static_assert(ctyaml::is_valid<R"(k: "\x41A")">);

// --- iteration: one call per element / pair, each with its own type
constexpr int sum = [] {
	int total = 0;
	ctyaml::for_each(ctyaml::parse<"[1, 2, 3]">(), [&](auto v) { total += v.template to<int>(); });
	return total;
}();
static_assert(sum == 6);

constexpr size_t key_chars = [] {
	size_t total = 0;
	ctyaml::for_each(ctyaml::parse<"aa: 1\nbbb: 2">(), [&](auto k, auto) { total += k.size(); });
	return total;
}();
static_assert(key_chars == 5);

// --- serialization: back to (flow-style) YAML, in static storage
static_assert(ctyaml::serialize(atkey) == "{key: [1, 2], other: x}"sv);
static_assert(ctyaml::serialize(ctyaml::parse<"a:\n  b: [1, .5]\n  c: ~\n">()) == "{a: {b: [1, .5], c: null}}"sv);
static_assert(ctyaml::serialize(ctyaml::parse<"'has: colon'">()) == R"("has: colon")"sv);
static_assert(ctyaml::serialize(ctyaml::parse<"'123'">()) == R"("123")"sv); // quoted so it re-reads as a string
static_assert(ctyaml::serialize(ctyaml::parse<R"(k: "a\nb")">()) == R"({k: "a\nb"})"sv);
// the output always re-parses to the same document
static_assert(ctyaml::is_valid<"{key: [1, 2], other: x}">);
static_assert(std::is_same_v<
    decltype(ctyaml::parse<"{key: [1, 2], other: x}">()),
    std::remove_const_t<decltype(atkey)>>);

// --- document markers
static_assert(ctyaml::parse<"--- scalar doc">() == "scalar doc"sv);
static_assert(ctyaml::parse<"---\na: 1\n">().get<"a">().to<int>() == 1);
static_assert(ctyaml::parse<"--- a: 1\n">().get<"a">().to<int>() == 1);

#endif

// --- operator[] and iterators (see include/ctyaml/views.hpp)

#if CTLL_CNTTP_COMPILER_CHECK

namespace bracket_tests {

constexpr auto d = ctyaml::parse<"name: Hana\ntags: [regex, ct]\nn: 42\n">();

// [] navigates with plain keys and indexes, returning uniform views
static_assert(d["name"] == "Hana"sv);
static_assert(d["tags"][1] == "ct");
static_assert(d["n"].to<int>() == 42);
static_assert(d["tags"].size() == 2);
static_assert(d["tags"][0].type == ctyaml::kind::string);
static_assert(d[0] == "Hana"); // mappings index positionally, like value<N>()

// misses are null views, so chains are safe; contains() asks first
static_assert(d["missing"].type == ctyaml::kind::null);
static_assert(d["missing"]["deep"][7].type == ctyaml::kind::null);
static_assert(d.contains("name"));
static_assert(!d.contains("missing"));

// keys a for_each hands out convert to string_view and work in []
static_assert([] {
	size_t named = 0;
	ctyaml::for_each(d, [&](auto key, auto value) {
		if (d[key].type == decltype(value)::type) {
			++named;
		}
	});
	return named;
}() == 3);

// begin/end yield the same views from static storage: range-for works,
// in constexpr evaluation included
static_assert([] {
	size_t key_chars = 0;
	for (const auto & m : d) {
		key_chars += m.key.size();
	}
	return key_chars;
}() == 4 + 4 + 1);

static_assert([] {
	for (const auto & m : d) {
		if (m.key == "tags") {
			// nested containers view their flow-style serialization
			return m.value.type == ctyaml::kind::sequence && m.value.text == "[regex, ct]";
		}
	}
	return false;
}());

// a view is itself a range (gcc 10 wants this loop in a named function
// rather than a constexpr lambda)
constexpr size_t tag_chars() noexcept {
	size_t total = 0;
	for (const auto & v : d["tags"]) {
		total += v.text.size(); // strings view their content
	}
	return total;
}
static_assert(tag_chars() == 5 + 2);

// a mapping view iterates with items()
constexpr size_t item_chars() noexcept {
	constexpr auto nested = ctyaml::parse<"outer:\n  a: 1\n  bb: 2\n">();
	size_t total = 0;
	for (const auto & m : nested["outer"].items()) {
		total += m.key.size();
	}
	return total;
}
static_assert(item_chars() == 1 + 2);

// scalar views: numbers keep their spelling, booleans and null their literals
static_assert(ctyaml::parse<"[0x1F, .inf, true, ~]">()[0].text == "0x1F"sv);
static_assert(ctyaml::parse<"[0x1F, .inf, true, ~]">()[1].to<double>() > 1e308);
static_assert(ctyaml::parse<"[0x1F, .inf, true, ~]">()[3].type == ctyaml::kind::null);

// empty containers iterate zero times
static_assert(ctyaml::begin(ctyaml::parse<"[]">()) == ctyaml::end(ctyaml::parse<"[]">()));

} // namespace bracket_tests

// --- diagnostics: error_info, error_message, bind_error, debug tools

// valid documents report nothing
static_assert(ctyaml::error_info<"a: 1\nb: 2">().ok());
static_assert(ctyaml::error_message<"a: 1\nb: 2">() == ""sv);
static_assert(ctyaml::bind_error<"a: 1\nb: 2">().ok());

// an unterminated flow sequence: kind, offset, line, column, expected
constexpr auto unterminated = ctyaml::error_info<"k: [1, 2">();
static_assert(unterminated.kind == ctlark::error_kind::parse);
static_assert(unterminated.position == 8 && unterminated.line == 1 && unterminated.column == 9);
static_assert(ctyaml::error_message<"k: [1, 2">() ==
              "ctlark: syntax error at line 1, column 9: unexpected end of input\n"
              "  k: [1, 2\n"
              "          ^\n"
              "expected: _WS, ']', ','"sv);

// structural failures name the rule and (where possible) the token
constexpr auto dup_key = ctyaml::bind_error<"a: 1\na: 2">();
static_assert(dup_key.reason == ctyaml::bind_reason::duplicate_key);
static_assert(dup_key.where == "a"sv);
constexpr auto nested_dup = ctyaml::bind_error<"m:\n  x: 1\n  x: 2">();
static_assert(nested_dup.reason == ctyaml::bind_reason::duplicate_key && nested_dup.where == "x"sv);
constexpr auto flow_dup = ctyaml::bind_error<"m: {k: 1, k: 2}">();
static_assert(flow_dup.reason == ctyaml::bind_reason::duplicate_key && flow_dup.where == "k"sv);
constexpr auto bad_esc = ctyaml::bind_error<"k: \"bad \\q\"">();
static_assert(bad_esc.reason == ctyaml::bind_reason::bad_escape);
static_assert(bad_esc.where == "\"bad \\q\""sv);
constexpr auto doc_end = ctyaml::bind_error<"a: 1\n...">();
static_assert(doc_end.reason == ctyaml::bind_reason::doc_end && doc_end.where == "..."sv);
constexpr auto bad_indent = ctyaml::bind_error<"a: 1\n  b: 2">();
static_assert(bad_indent.reason == ctyaml::bind_reason::bad_indent);

// the ctlark debugging toolbox with the YAML grammar baked in: the
// token dump shows the line structure (NL tokens carry the indent)
static_assert(ctyaml::debug::dump_tokens<"a: 1\n- x">() ==
              "SCALAR 'a' @0..1\n"
              "COLON ':' @1..2\n"
              "_WS ' ' @2..3\n"
              "SCALAR '1' @3..4\n"
              "NL '\n' @4..5\n"
              "DASH '- ' @5..7\n"
              "SCALAR 'x' @7..8\n"sv);
constexpr auto traced = ctyaml::debug::traced_parse<"k: [1, 2">();
static_assert(!traced.ok && traced.error.kind == ctlark::error_kind::parse);
static_assert(traced.log.events > 0);
static_assert(ctyaml::debug::dump_grammar().find("terminal DASH") != std::string_view::npos);

#endif
