> **Attribution:** this library is built on the CTLL compile-time LL(1)
> parser from [CTRE](https://github.com/hanickadot/compile-time-regular-expressions)
> by Hana Dusíková, via the [notre](https://github.com/alexios-angel/notre)
> fork. Apache License 2.0 with LLVM Exceptions; see [NOTICE](NOTICE).

# ctyaml — compile-time YAML

YAML parsed while your code compiles. The document is a *type*: a typo,
a bad indent or a duplicate key is a compile error, lookups are
resolved at compile time, and every accessor is `constexpr` — usable in
`static_assert`, as template arguments, or at runtime with zero parsing
cost.

```c++
#include <ctyaml.hpp>

constexpr auto config = ctyaml::parse<R"(# deploy config
service: demo
workers: 8
log:
  level: info
  color: true
endpoints:
  - host: a.example.com
    port: 443
  - {host: b.example.com, port: 8080}
)">();

static_assert(config.get<"service">() == "demo");
static_assert(config.get<"workers">().to<int>() == 8);
static_assert(config.get<"log">().get<"color">());
static_assert(config.get<"endpoints">().get<1>().get<"port">().to<int>() == 8080);
static_assert(!config.contains<"missing">());

static_assert(ctyaml::is_valid<"a: 1\nb: [x, y]">);
static_assert(!ctyaml::is_valid<"a: 1\n a: bad indent">);
static_assert(!ctyaml::is_valid<"a: 1\na: duplicate">);
```

## The supported subset

YAML's full specification is famously large; ctyaml implements the
subset configuration files actually use — YAML 1.2 with the core
schema — and draws the line so that everything OUTSIDE the subset
**fails the parse loudly** instead of quietly meaning something else:

**Supported:** block mappings and block sequences nested by
indentation, the compact `- key: value` form (nested dashes `- - a`
included), a sequence at its mapping key's own indent, flow
collections `[a, b]` / `{k: v}` on one line (trailing commas, nesting),
plain scalars with YAML's context rules (`a:b` and `a#b` stay one
scalar, `key: value` splits, `# ...` comments need a space), single-
and double-quoted scalars with the full double-quote escape set
(`\xXX`, `\uXXXX`, `\UXXXXXXXX` decode to UTF-8 at parse time), core
schema resolution of plain values (`null`/`~`, `true`/`false`,
decimal/`0x`/`0o` integers, floats, `.inf`/`.nan`), comments, blank
lines, CRLF, and one optional leading `---` marker.

**Rejected, by design** (a compile error / `false` from `is_valid`,
never a misparse): anchors `&`, aliases `*`, tags `!`, directives `%`,
block scalars `|` and `>`, complex keys `? `, multi-document streams
(`---` mid-stream, `...`), multi-line plain or quoted scalars,
multi-line flow collections, and tabs anywhere outside quoted scalars
and comments.

**Documented divergences:** mapping keys are always strings (`get` is
spelled with the key's text: `get<"true">()`, `get<"80">()`); duplicate
keys are an error even though YAML merely frowns; a plain scalar
cannot start with `--`; and a scalar like `k:: v` (an even run of
colons directly before `: `) must be quoted.

## API

```c++
// validity as a bool (never a compile error):
template <ctll::fixed_string input> constexpr bool ctyaml::is_valid;

// the parsed document; invalid YAML fails the build:
template <ctll::fixed_string input> constexpr auto ctyaml::parse();
```

`parse` returns one of the document types, all in namespace `ctyaml`:

| Type | Accessors |
|------|-----------|
| `mapping<members...>` | `get<"key">()`, `["key"]`, `contains<"key">()`, `size()`, `empty()`, positional `key<N>()` / `value<N>()`, range-for over member views |
| `sequence<values...>` | `get<N>()`, `[N]`, `size()`, `empty()`, range-for over value views |
| `string<chars...>` | `view()`, `c_str()` (null-terminated), `size()`, `empty()`, `==` with `std::string_view` |
| `number<chars...>` | `to<T>()` for any arithmetic `T`, `is_integer()`, `view()` (raw spelling), `c_str()` |
| `boolean<B>` | `value`, `operator bool` |
| `null` | — |

Every type carries `static constexpr ctyaml::kind type` for
introspection (`kind::mapping`, `kind::sequence`, ...).

Two free functions round out the API:

```c++
// render any document value to single-line flow-style YAML, in static storage:
static_assert(ctyaml::serialize(ctyaml::parse<"a:\n  - 1\n  - two\n">()) == "{a: [1, two]}");

// compile-time iteration (elements, or key/value pairs):
ctyaml::for_each(config.get<"endpoints">(), [](auto value) { /* each has its own type */ });
ctyaml::for_each(config, [](auto key, auto value) { ... });
```

`serialize` writes strings plain when they read back as the same
string and double-quoted with escapes otherwise, and numbers with the
spelling they were parsed with — the output always re-parses to the
same document.

Brackets and iteration:

```c++
config["service"];                         // get, spelled the familiar way
config["endpoints"][1]["port"].to<int>(); // chains, to any depth
config.contains("service");                // runtime keys

// begin/end yield the same views from static storage, so range-for and
// algorithms work - in constexpr evaluation included:
for (const auto & m : config) {
    m.key;          // std::string_view
    m.value.type;   // ctyaml::kind
    m.value.text;   // string content, number spellings, flow-style containers
}
for (const auto & v : config["stages"]) { /* a view is a range */ }
for (const auto & m : config["notify"].items()) { /* key/value pairs */ }
```

Elements of one document all have different types, so `operator[]` with
a runtime key or index cannot return the element itself: it returns a
`value_view` — the kind, the text and the children, still fully
constexpr. A miss is a *null view* (`kind::null`), so chains are safe
and `contains` asks first; `get<...>()` remains the typed accessor and
`for_each` the type-preserving iteration. The records are `value_view`
and `member_view` ([`views.hpp`](include/ctyaml/views.hpp)), and
[`examples/iteration.cpp`](examples/iteration.cpp) is a runnable tour.

Details:

* String content is stored as UTF-8 bytes; double-quote escapes are
  decoded at parse time (`"\U0001F600"` becomes the four UTF-8 bytes
  of 😀), invalid escapes, surrogate code points and values beyond
  U+10FFFF are errors.
* Numbers keep their raw spelling; `to<T>()` converts on demand —
  hex, octal, exponents and `.inf`/`.nan` included (integral
  conversions truncate fractions, like a cast would).
* Resolution applies to plain VALUES only: `'true'` and `"1"` are
  strings, and `yes`/`no`/`on`/`off` (YAML 1.1 legacy) stay strings
  too.

## Debugging

When `is_valid` says `false`, the reason is one query away, computed at
compile time. Syntax failures carry the location and the expected
tokens:

```c++
constexpr auto info = ctyaml::error_info<"k: [1, 2">();
// info.kind (lex/parse/...), info.position, info.line, info.column

constexpr auto why = ctyaml::error_message<"k: [1, 2">();
//   ctlark: syntax error at line 1, column 9: unexpected end of input
//     k: [1, 2
//             ^
//   expected: _WS, ']', ','
```

Documents that PARSE can still fail the structural rules; the binder
names the rule and, where it survives the fold, the offending token:

```c++
ctyaml::bind_error<"a: 1\na: 2">();      // duplicate_key, where == "a"
ctyaml::bind_error<"k: \"bad \\q\"">();  // bad_escape, where == the raw scalar
ctyaml::bind_error<"a: 1\n...">();       // doc_end
ctyaml::bind_error<"a: 1\n  b: 2">();    // bad_indent (no location: use dump_tokens)
```

A failed `parse<>()` names the failing stage and the query to run in
its `static_assert` message. `ctyaml::debug` bundles the [ctlark
debugging toolbox](../compile-time-lark#debugging) with the YAML
grammar baked in: `traced_parse<input>()` (a recorded event log, also
runnable at runtime under a debugger), `parse_runtime(text)` (runtime
inputs against the compile-time tables), `dump_tokens<input>()` —
particularly useful here, since the NL tokens carry each line's
indentation — and `dump_grammar()`.

## C++17

With a pre-C++20 compiler, inputs and keys are `constexpr
ctll::fixed_string` variables with linkage instead of string literals:

```c++
static constexpr auto text = ctll::fixed_string{"n: 42\n"};
static constexpr ctll::fixed_string n_key = "n";

constexpr auto doc = ctyaml::parse<text>();
static_assert(doc.template get<n_key>().template to<int>() == 42);
```

## How it works

The grammar layer is
[ctlark](https://github.com/alexios-angel/compile-time-lark)
(compile-time Lark): the grammar is a *lark grammar string*
([`grammar.hpp`](include/ctyaml/grammar.hpp)) that ctlark parses and
compiles to constexpr tables while your code compiles, then runs its
contextual-lexing constexpr Earley parser over your document.

YAML's block structure is indentation, and indentation is not context
free — so the grammar does not try to say it. It is *line oriented*:
the `NL` terminal glues each newline to the run of spaces that follows
it, the `DASH` terminal keeps the spaces after `-`, spaces are never
`%ignore`d, and the scalar terminals encode YAML's context rules (a
`:` only binds into a plain scalar when not followed by a space, a `#`
only when not preceded by one) as run-and-terminator alternations,
since ctlark's regex subset has no lookarounds. The binder
([`bind.hpp`](include/ctyaml/bind.hpp)) then does what a YAML parser's
block-structure pass does, at the type level: flattens the parse tree
into (indent, content) lines and rebuilds the nesting by recursive
descent — re-entering `- key: value` as a synthetic line indented past
the dash — while resolving scalars, decoding escapes, and folding the
checks the grammar cannot express (indentation consistency, duplicate
keys, escape validity) into `bind<Tree>::ok`, which `is_valid`
includes.

Because that work happens in headers, a **precompiled header** makes
it a one-time cost: `make pch` (done automatically by the test build)
compiles `ctyaml.hpp` once — grammar parse, table build and all — and
every translation unit that includes it afterwards starts from the
baked result. The CMake tests and examples use
`target_precompile_headers` the same way (`CTYAML_PCH`, default ON).

An Earley parse needs a raised constexpr budget; the CMake interface
target carries the compiler-specific limit flags automatically
(`CTYAML_CONSTEXPR_LIMITS`, default ON) and the Makefiles set them:

```
clang:  -fconstexpr-steps=500000000 -fconstexpr-depth=1024 -fbracket-depth=2048
gcc:    -fconstexpr-ops-limit=3000000000 -fconstexpr-loop-limit=10000000 -fconstexpr-depth=1024
```

The only generated parse table left in the tree is ctlark's own
`lark.hpp` (the grammar of the Lark grammar language); regenerate it
after editing `lark.gram` with `make regrammar`.

## Building and integrating

Header-only. Pick whichever fits your project:

**CMake, as a subdirectory or via FetchContent:**

```cmake
add_subdirectory(compile-time-yaml)   # or FetchContent_MakeAvailable(ctyaml)
target_link_libraries(your-target PRIVATE ctyaml::ctyaml)
```

**CMake, installed** (`cmake -B build && cmake --install build`):

```cmake
find_package(ctyaml 0.1 REQUIRED)
target_link_libraries(your-target PRIVATE ctyaml::ctyaml)
```

The install also ships a `pkg-config` file (`ctyaml.pc`). Tests and
examples build only when ctyaml is the top-level project
(`CTYAML_BUILD_TESTS`, `CTYAML_BUILD_EXAMPLES`); `CTYAML_CXX_STANDARD`
selects the advertised standard (default 20). CPack can produce
TGZ/ZIP archives (plus DEB/RPM where the tooling exists), and
`-DCTYAML_MODULE=ON` builds `ctyaml.cppm` as a named C++ module
(experimental; needs CMake 3.30+, a modules-capable toolchain and
`import std`).

**No build system:** add `include/` to your include path, or copy the
amalgamated [`single-header/ctyaml.hpp`](single-header/ctyaml.hpp)
(regenerate with `make single-header`, which needs the
[quom](https://pypi.org/project/quom/) tool).

Requires C++17 (C++20 for the string-literal API). Runnable demos live
in [`examples/`](examples/).

Run the tests (compilation is the test — the suite is `static_assert`s):

```bash
make CXX=clang++                       # C++20
make CXX=clang++ CXX_STANDARD=17
# or through CMake/CTest:
cmake -B build && cmake --build build && ctest --test-dir build
```

## License

Apache License 2.0 with LLVM Exceptions (see [LICENSE](LICENSE)).
The CTLL parser is Hana Dusíková's work, via notre; see
[NOTICE](NOTICE).
