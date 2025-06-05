CXX = g++ 
CXXFLAGS = -std=c++11 -Wall -Wextra -fprofile-arcs -ftest-coverage -O0
TARGETS = molecule_supplier molecule_requester

all: $(TARGETS)

molecule_requester: molecule_requester.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

molecule_supplier: molecule_supplier.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS) *.gcda *.gcno *.gcov

.PHONY: all clean