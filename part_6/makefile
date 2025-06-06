CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -fprofile-arcs -ftest-coverage

TARGETS = atom_supplier molecule_supplier_stage6 molecule_requester drinks_bar

all: $(TARGETS)

atom_supplier: atom_supplier.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

molecule_supplier_stage6: molecule_supplier_stage6.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

molecule_requester: molecule_requester.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

drinks_bar: drinks_bar.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS) *.gcov *.gcno *.gcda
