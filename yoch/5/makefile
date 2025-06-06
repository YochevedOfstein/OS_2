CXX = g++  
CXXFLAGS = -std=c++11 -Wall -Wextra -fprofile-arcs -ftest-coverage -O0
LDFLAGS = -lgcov
TARGETS = drinks_bar molecule_requester atom_supplier

all: $(TARGETS)

atom_supplier: atom_supplier.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $<

molecule_requester: molecule_requester.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $<

drinks_bar: drinks_bar.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS) *.gcda *.gcno *.gcov

.PHONY: all clean