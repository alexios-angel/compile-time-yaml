.PHONY: default all clean grammar regrammar pch single-header single-header/ctyaml.hpp

default: all

CXX_STANDARD := 20

PYTHON := python3

# LL1q parser generator: https://github.com/alexios-angel/Tablewright
# (needs python3 with the lark package)
TABLEWRIGHT := tablewright

# Earley at compile time needs more constexpr budget than the defaults
CXX_IS_CLANG := $(shell $(CXX) --version 2>/dev/null | grep -qi clang && echo yes)
ifeq ($(CXX_IS_CLANG),yes)
CONSTEXPR_FLAGS := -fconstexpr-steps=500000000 -fconstexpr-depth=1024 -fbracket-depth=2048
else
CONSTEXPR_FLAGS := -fconstexpr-ops-limit=3000000000 -fconstexpr-loop-limit=10000000 -fconstexpr-depth=1024
endif

override CXXFLAGS := $(CXXFLAGS) -std=c++$(CXX_STANDARD) -Iinclude $(CONSTEXPR_FLAGS) -O2 -pedantic -Wall -Wextra -Werror -Wconversion

# precompiled header: parsing the YAML grammar text and compiling its
# tables happens once here instead of once per translation unit
ifeq ($(CXX_IS_CLANG),yes)
PCH := ctyaml.pch
PCH_USE = -include-pch $(PCH)
else
PCH := include/ctyaml.hpp.gch
PCH_USE =
endif

TESTS := $(wildcard tests/*.cpp)
OBJECTS := $(TESTS:%.cpp=%.o)
DEPENDENCY_FILES := $(OBJECTS:%.o=%.d)

all: $(OBJECTS)

$(OBJECTS): %.o: %.cpp $(PCH)
	$(CXX) $(CXXFLAGS) $(PCH_USE) -MMD -c $< -o $@

pch: $(PCH)

$(PCH): include/ctyaml.hpp
	$(CXX) $(CXXFLAGS) -x c++-header $< -o $@

-include $(DEPENDENCY_FILES)

clean:
	rm -f $(OBJECTS) $(DEPENDENCY_FILES) ctyaml.pch include/ctyaml.hpp.gch

# the only generated table left is ctlark's own grammar-of-grammars
grammar: include/ctlark/lark.hpp

regrammar:
	@rm -f include/ctlark/lark.hpp
	@$(MAKE) grammar

include/ctlark/lark.hpp: include/ctlark/lark.gram
	@echo "LL1q $<"
	@$(TABLEWRIGHT) --ll --q --input=include/ctlark/lark.gram --output=include/ctlark/ --generator=cpp_ctll_v2 --cfg:fname=lark.hpp --cfg:namespace=ctlark --cfg:guard=CTLARK__LARK__HPP --cfg:grammar_name=lark_grammar

# needs python3 with the quom package
single-header: single-header/ctyaml.hpp

single-header/ctyaml.hpp:
	$(PYTHON) -m quom include/ctyaml.hpp ctyaml.hpp.tmp
	echo "/*" > single-header/ctyaml.hpp
	cat LICENSE >> single-header/ctyaml.hpp
	echo "*/" >> single-header/ctyaml.hpp
	cat ctyaml.hpp.tmp >> single-header/ctyaml.hpp
	rm ctyaml.hpp.tmp
