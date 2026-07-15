# CLAUDE.md — compile-time-yaml (ctyaml)

Compile-time YAML parser (supported YAML 1.2 core-schema subset). The document
is a constexpr **type**: bad indent / duplicate key / bad escape is a compile
error, all accessors are `constexpr`. Namespace `ctyaml`. Umbrella header
`include/ctyaml.hpp`. Compile-time ONLY — no runtime `load`/`dumps`
(`serialize()` and `debug::parse_runtime` exist, but there is no runtime loader).
Header-only, C++20 default (C++17 supported). Repo: github.com/alexios-angel/compile-time-yaml, work on `main`. Prefer `rg` over grep.

## Build & test (compiling the tests IS the test)
`make` compiles every `tests/*.cpp` to a `.o`; each is a `static_assert` suite,
so a clean compile is a pass. A PCH of the umbrella header is built once.
```bash
make                      # C++20, builds PCH + all tests
make CXX=clang++          # clang (uses -include-pch ctyaml.pch)
make CXX_STANDARD=17      # C++17
make clean
cmake -B build && cmake --build build && ctest --test-dir build
```
Flags (in Makefile): `-std=c++20 -Iinclude -O2 -pedantic -Wall -Wextra -Werror -Wconversion`. Keep code warning-clean under `-Werror -Wconversion`.

## Layout
- `include/ctyaml.hpp` — umbrella; the public API (`is_valid`, `parse`, `error_info`, `error_message`, `bind_error`, `debug::`).
- `include/ctyaml/grammar.hpp` — the YAML subset as a **Lark grammar string** (data; line-oriented, `NL`/`DASH` keep leading spaces because indentation is not context-free).
- `include/ctyaml/bind.hpp` — MOST complex file (~600 lines): the indentation/block-structure pass. Flattens the ctlark parse tree into (indent, content) lines, rebuilds nesting, resolves scalars, decodes escapes, folds checks the grammar can't express into `bind<Tree>::ok`.
- `include/ctyaml/types.hpp` — document types (`mapping`, `sequence`, `string`, `number`, `boolean`, `null`, `kind`).
- `include/ctyaml/serialize.hpp`, `views.hpp` — `serialize()`, `value_view`/`member_view`, `for_each`.
- `external/compile-time-lark/` — git **SUBMODULE** providing `ctlark` + `ctll` (see Gotchas). Do NOT edit here.
- `single-header/ctyaml.hpp` (generated), `ctyaml.cppm` (C++ module, `import std`), `examples/`, `tests/`.

## Public API (namespace ctyaml)
- `is_valid<input>` — bool, never a compile error.
- `parse<input>()` — returns the document type; invalid YAML fails to compile.
- `error_info<input>()` / `error_message<input>()` — syntax failure location + rendered caret (from ctlark).
- `bind_error<input>()` — why a document that PARSES fails structure. Reasons (`bind_reason`): `bad_escape`, `duplicate_key`, `doc_end` (`...` marker), `bad_indent` (NOTE: indentation errors lose their token location — use `debug::dump_tokens`).
- `serialize(doc)`, `for_each(doc, fn)`.
- `ctyaml::debug::` — `traced_parse<input>()`, `dump_tokens<input>()`, `dump_grammar()`, `parse_runtime(text)`.

## Conventions
- **C++17/20 CNTTP split**: `CTLL_CNTTP_COMPILER_CHECK` gates `CTYAML_STRING_INPUT` — string-literal NTTP on C++20, `const auto&` `ctll::fixed_string` variables (with linkage) on C++17.
- Debug macros: `CTLARK_VERBOSE_ERRORS`, `CTLARK_DEBUG`, `CTLARK_CONSTEXPR_ASSERT`.

## GOTCHAS
- **ctlark and ctll are a git SUBMODULE, never edit here**:
  `external/compile-time-lark` — run `git submodule update --init` once after
  cloning. **compile-time-lark is the source of truth**: edit the core THERE,
  then bump by checking out a new commit inside the submodule and committing
  the gitlink; then regenerate `single-header/`. The build adds
  `<sub>/include` AND `<sub>/include/ctlark` / `<sub>/include/ctll` to the
  include path so the headers' relative `"../ctlark.hpp"`-style includes
  resolve via the quoted-include fallback; the CMake install flattens
  everything back to include/{ctyaml,ctlark,ctll}.
- **Huge constexpr budget** (Earley at compile time): Makefile sets, per compiler —
  gcc: `-fconstexpr-ops-limit=3000000000 -fconstexpr-loop-limit=10000000 -fconstexpr-depth=1024`;
  clang: `-fconstexpr-steps=500000000 -fconstexpr-depth=1024 -fbracket-depth=2048`.
  CMake attaches them too (`-DCTYAML_CONSTEXPR_LIMITS=OFF` to opt out). Hitting the compiler's own step cap is a distinct failure mode from the library's queryable overflow/depth errors.
- **Grammar is generated**: the only generated table is ctlark's own `lark.hpp`, which lives in the submodule (`external/compile-time-lark/include/ctlark/lark.hpp`), produced from `lark.gram` by **Tablewright** — regenerate it in compile-time-lark (`make regrammar` THERE), then bump the submodule. Don't hand-edit `lark.hpp`.
- **Single header**: `make single-header` (needs `python3 -m quom`); prepends `LICENSE`.
- **Attribution** (preserve): CTLL is from CTRE (Hana Dusíková) via notre; the Lark grammar language is lark-parser's (Erez Shinan); Tablewright descends from Desatomat. Keep `NOTICE`/`LICENSE` (Apache-2.0 + LLVM Exceptions) intact.

## CMake options
`CTYAML_BUILD_TESTS`, `CTYAML_BUILD_EXAMPLES` (ON when top-level), `CTYAML_PCH` (ON), `CTYAML_CONSTEXPR_LIMITS` (ON), `CTYAML_MODULE` (OFF; forces C++23+), `CTYAML_CXX_STANDARD` (default 20).
