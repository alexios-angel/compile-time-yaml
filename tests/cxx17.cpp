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

// operator[] needs no C++20: keys from for_each and _i indexes are types
using namespace ctyaml::literals;

static constexpr auto seq_text = ctll::fixed_string{"[10, 20, 30]"};
constexpr auto seq = ctyaml::parse<seq_text>();
static_assert(seq[1_i].template to<int>() == 20);

static_assert([] {
	int hits = 0;
	ctyaml::for_each(doc, [&](auto key, auto) {
		if (doc[key].type == ctyaml::kind::mapping) {
			++hits;
		}
	});
	return hits;
}() == 1);

// iteration: uniform views, range-for, constexpr
static_assert([] {
	size_t n = 0;
	for (const auto & m : doc) {
		n += m.key.size();
	}
	return n;
}() == 6);
