// A generic recursive visitor over any document: kind dispatch with
// `if constexpr`, iteration with for_each, and the whole document
// re-serialized to flow-style YAML - all resolvable at compile time,
// printed at runtime only to be seen.
//
// Build: make introspection

#include <ctyaml.hpp>
#include <iostream>
#include <string>

constexpr auto doc = ctyaml::parse<R"(
name: ctyaml
tags: [yaml, compile-time, 'c++']
stats:
  stars: ~
  numbers:
    - 0x10
    - -2.5
    - .inf
nested:
  - - deep
    - deeper
)">();

// count every scalar in the document, whatever its depth
template <typename Node> constexpr size_t count_scalars(Node node) {
	if constexpr (Node::type == ctyaml::kind::mapping) {
		size_t total = 0;
		ctyaml::for_each(node, [&](auto, auto value) { total += count_scalars(value); });
		return total;
	} else if constexpr (Node::type == ctyaml::kind::sequence) {
		size_t total = 0;
		ctyaml::for_each(node, [&](auto value) { total += count_scalars(value); });
		return total;
	} else {
		return 1;
	}
}
static_assert(count_scalars(doc) == 10);

// render an indented outline at runtime, dispatching on kind
template <typename Node> void outline(Node node, const std::string & indent = "") {
	if constexpr (Node::type == ctyaml::kind::mapping) {
		ctyaml::for_each(node, [&](auto key, auto value) {
			std::cout << indent << key.view() << ":\n";
			outline(value, indent + "  ");
		});
	} else if constexpr (Node::type == ctyaml::kind::sequence) {
		ctyaml::for_each(node, [&](auto value) {
			std::cout << indent << "-\n";
			outline(value, indent + "  ");
		});
	} else if constexpr (Node::type == ctyaml::kind::string) {
		std::cout << indent << '"' << node.view() << "\"\n";
	} else if constexpr (Node::type == ctyaml::kind::number) {
		std::cout << indent << node.view() << (node.is_integer() ? " (int)\n" : " (float)\n");
	} else if constexpr (Node::type == ctyaml::kind::boolean) {
		std::cout << indent << (node ? "true\n" : "false\n");
	} else {
		std::cout << indent << "null\n";
	}
}

int main() {
	outline(doc);
	std::cout << "\nscalars: " << count_scalars(doc) << "\n";
	std::cout << "flow:    " << ctyaml::serialize(doc) << "\n";
}
