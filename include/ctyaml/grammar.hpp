#ifndef CTYAML__GRAMMAR__HPP
#define CTYAML__GRAMMAR__HPP

#include "../ctlark.hpp"

// The grammar layer: the ctyaml YAML subset written in lark's grammar
// language and parsed by ctlark. YAML's block structure is not context
// free - nesting is INDENTATION - so this grammar does not try to say
// it. Instead it is line oriented: NL is a terminal that glues each
// newline to the run of spaces that follows it, so every token of
// indentation survives into the parse tree, and the binder (bind.hpp)
// rebuilds the mapping/sequence nesting from those widths. Everything
// that fits on one line - `key: value`, `- item`, flow collections,
// quoted scalars - is ordinary context-free structure below.
//
// The scalar terminals carry the spec load. A plain scalar may contain
// `:` only when not followed by a space and `#` only when not preceded
// by one; with no lookarounds in ctlark's regex subset, both rules are
// spelled as run-and-terminator alternations (`:+` glued to a plain
// character, a space run glued to a non-`#`), which is also what makes
// `key: value` split while `key:value` stays one scalar. FSCALAR is
// the same shape with the flow indicators `,[]{}` excluded, a
// candidate only inside flow collections (ctlark's lexing is
// contextual, like lark's). Spaces are never %ignore'd - every blank
// is either inside NL, inside a scalar, or an explicit _WS - which is
// what lets DASH carry its trailing spaces (the binder needs their
// width for `- key: value` nesting) and makes a tab anywhere a loud
// lex error, YAML's own rule for indentation.
//
// Unsupported YAML announces itself: anchors (&), aliases (*), tags
// (!), directives (%), block scalars (| >) and complex keys (? ) all
// begin with a character no terminal accepts, so they fail the parse
// rather than misparse. What the grammar cannot say - indentation
// consistency, duplicate keys, escape validity - the binder checks,
// and is_valid includes.

namespace ctyaml::detail {

inline constexpr ctll::fixed_string yaml_grammar = R"x(
start: _first (NL _line)*

_first: _DOCSTART (_WS _item)? _WS? _COMMENT?
      | _line

_line: _item? _WS? _COMMENT?

_item: pair
     | seqitem
     | value

pair: (SCALAR | DQ | SQ) _WS? ":" (_WS value)?

seqitem: DASH _item
       | DASH
       | "-"

?value: SCALAR -> plain
      | DQ -> dquoted
      | SQ -> squoted
      | flowseq
      | flowmap

?fvalue: FSCALAR -> plain
       | DQ -> dquoted
       | SQ -> squoted
       | flowseq
       | flowmap

flowseq: "[" _WS? ("]" | fvalue (_WS? "," _WS? fvalue)* _WS? ("," _WS?)? "]")
flowmap: "{" _WS? ("}" | fpair (_WS? "," _WS? fpair)* _WS? ("," _WS?)? "}")
fpair: (FSCALAR | DQ | SQ) _WS? (":" _WS? fvalue?)?

NL: /\r?\n[ ]*/
DASH: /\-[ ]+/
_DOCSTART: /\-\-\-/
_WS: /[ ]+/
_COMMENT: /#[^\r\n]*/
DQ: /"([^"\\\r\n]|\\[^\r\n])*"/
SQ: /'([^'\r\n]|'')*'/
SCALAR: /([^\t\r\n ,\[\]{}#&*!|>'"%@`\-?:]|:+[^\t\r\n :]|\?[^\t\r\n ]|\-[^\t\r\n \-])([^\t\r\n :]|:+[^\t\r\n :]|[ ]+[^\t\r\n #:]|[ ]+:+[^\t\r\n :])*/
FSCALAR: /([^\t\r\n ,\[\]{}#&*!|>'"%@`\-?:]|:+[^\t\r\n ,\[\]{}:]|\?[^\t\r\n ,\[\]{}]|\-[^\t\r\n ,\[\]{}\-])([^\t\r\n ,\[\]{}:]|:+[^\t\r\n ,\[\]{}:]|[ ]+[^\t\r\n ,\[\]{}#:]|[ ]+:+[^\t\r\n ,\[\]{}:])*/
)x";

inline constexpr ctll::fixed_string yaml_start = "start";

static_assert(ctlark::grammar_valid<yaml_grammar>,
              "ctyaml: internal error - the YAML grammar failed to compile");

} // namespace ctyaml::detail

#endif
