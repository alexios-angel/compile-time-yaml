// Brackets and iteration: operator[] is get spelled the familiar way -
// the key or index rides in the argument's TYPE - and begin/end give
// every container uniform views (kind + text) out of static storage,
// so range-for and <algorithm> work over a document whose elements all
// have different types.
//
// Build: make iteration

#include <ctyaml.hpp>
#include <algorithm>
#include <iostream>

using namespace ctyaml::literals;

constexpr auto pipeline = ctyaml::parse<R"(# build pipeline
name: ctyaml
timeout: 90
cache: true
stages: [lint, build, test]
notify:
  channel: '#builds'
  on_failure: true
)">();

// --- operator[]: get, spelled with brackets, chains included

static_assert(pipeline["name"_k] == "ctyaml");
static_assert(pipeline["stages"_k][2_i] == "test");
static_assert(pipeline["notify"_k]["channel"_k] == "#builds");
static_assert(pipeline["timeout"_k].to<int>() == 90);

// --- iteration: uniform views make a document an ordinary range

static_assert(std::count_if(begin(pipeline), end(pipeline),
    [](const ctyaml::member_view & m) { return m.value.type == ctyaml::kind::boolean; }) == 1);

constexpr auto longest = *std::max_element(begin(pipeline), end(pipeline),
    [](const ctyaml::member_view & a, const ctyaml::member_view & b) { return a.key.size() < b.key.size(); });
static_assert(longest.key == "timeout");

// range-for in constant evaluation: a named constexpr function (gcc 10
// mishandles this loop inside a constexpr lambda)
constexpr size_t stage_chars() noexcept {
	size_t total = 0;
	for (const auto & v : pipeline["stages"_k]) {
		total += v.text.size(); // strings view their content
	}
	return total;
}
static_assert(stage_chars() == 4 + 5 + 4);

constexpr const char * kind_names[]{"mapping", "sequence", "string", "number", "boolean", "null"};

int main() {
	// dump any document as a table: views are plain kinds and string_views
	for (const auto & m : pipeline) {
		std::cout << m.key << " (" << kind_names[static_cast<int>(m.value.type)] << "): "
		          << m.value.text << "\n";
	}

	std::cout << "\nstages:";
	for (const auto & v : pipeline["stages"_k]) {
		std::cout << ' ' << v.text;
	}
	std::cout << "\n";
}
