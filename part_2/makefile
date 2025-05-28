CXX = g++ 
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
TARGETS = molecule_supplier molecule_requester atom_supplier 

all: $(TARGETS)

molecule_requester: molecule_requester.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

molecule_supplier: molecule_supplier.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

atom_supplier: atom_supplier.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean