#include <ctyaml.hpp>
#include <string_view>

// C++17: inputs and keys are fixed_string variables with linkage
static constexpr auto doc_text = ctll::fixed_string{"server:\n  port: 8080\n  tags: [a, b]\n"};
static constexpr auto bad_text = ctll::fixed_string{"a: 1\n a: bad indent"};
static constexpr ctll::fixed_string server_key = "server";
static constexpr ctll::fixed_string port_key = "port";
static constexpr ctll::fixed_string tags_key = "tags";

static_assert(ctyaml::is_valid<doc_text>);
static_assert(!ctyaml::is_valid<bad_text>);

constexpr auto doc = ctyaml::parse<doc_text>();
static_assert(doc.template get<server_key>().template get<port_key>().template to<int>() == 8080);
static_assert(doc.template get<server_key>().template get<tags_key>().template get<1>() == std::string_view{"b"});
static_assert(ctyaml::serialize(doc) == std::string_view{"{server: {port: 8080, tags: [a, b]}}"});

void empty_symbol() { }

// operator[] takes plain keys and indexes, in any standard
static constexpr auto seq_text = ctll::fixed_string{"[10, 20, 30]"};
constexpr auto seq = ctyaml::parse<seq_text>();
static_assert(seq[1].to<int>() == 20);
static_assert(doc["server"]["port"].to<int>() == 8080);
static_assert(doc["server"]["tags"][1] == std::string_view{"b"});
static_assert(doc["server"]["missing"].type == ctyaml::kind::null);
static_assert(!doc.contains("missing"));

// iteration: uniform views, range-for, constexpr
static_assert([] {
	size_t n = 0;
	for (const auto & m : doc) {
		n += m.key.size();
	}
	return n;
}() == 6);

// diagnostics through the variable-form API
static_assert(ctyaml::error_info<doc_text>().ok());
static_assert(ctyaml::error_info<bad_text>().ok()); // bad_text PARSES; the binder rejects it
constexpr auto indent_err = ctyaml::bind_error<bad_text>();
static_assert(indent_err.reason == ctyaml::bind_reason::bad_indent);
static constexpr auto dup_text = ctll::fixed_string{"a: 1\na: 2"};
constexpr auto dup_err = ctyaml::bind_error<dup_text>();
static_assert(dup_err.reason == ctyaml::bind_reason::duplicate_key);
static_assert(dup_err.where == std::string_view{"a"});
static constexpr auto syntax_text = ctll::fixed_string{"k: [1, 2"};
static_assert(ctyaml::error_info<syntax_text>().kind == ctlark::error_kind::parse);
static_assert(!ctyaml::error_message<syntax_text>().empty());
constexpr auto traced = ctyaml::debug::traced_parse<syntax_text>();
static_assert(!traced.ok && traced.log.events > 0);
static_assert(!ctyaml::debug::dump_tokens<doc_text>().empty());
