// The classic use: a YAML configuration baked into the binary at
// compile time. A typo in the YAML, a bad indent, a duplicate key or a
// missing setting is a build failure, and every lookup below compiles
// down to a constant.
//
// Build: make config

#include <ctyaml.hpp>
#include <iostream>

constexpr auto config = ctyaml::parse<R"(# demo service
service: demo
workers: 8
log:
  level: info
  color: true
endpoints:              # tried in order
  - host: a.example.com
    port: 443
    tls: true
  - host: b.example.com
    port: 8080
    tls: false
greeting: "hello & welcome"
)">();

// requirements checked at build time
static_assert(config.get<"workers">().is_integer());
static_assert(config.get<"endpoints">().size() >= 1);
static_assert(config.get<"endpoints">().get<0>().contains<"host">());
static_assert(config.get<"log">().get<"level">() == "info");

// values usable as constants
constexpr int workers = config.get<"workers">().to<int>();
int worker_slots[workers];

int main() {
	std::cout << "service:  " << config.get<"service">().view() << "\n";
	std::cout << "workers:  " << workers << " (slots: " << sizeof(worker_slots) / sizeof(int) << ")\n";
	std::cout << "greeting: " << config.get<"greeting">().view() << "\n";

	std::cout << "endpoints:\n";
	ctyaml::for_each(config.get<"endpoints">(), [](auto endpoint) {
		std::cout << "  " << endpoint.template get<"host">().view()
		          << ":" << endpoint.template get<"port">().template to<int>()
		          << (endpoint.template get<"tls">() ? " (tls)" : "")
		          << "\n";
	});
}
