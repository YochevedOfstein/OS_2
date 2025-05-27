CXX = g++ 
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
TARGETS = atom_warehouse atom_supplier molecule_supplier molecule_requestor

all: $(TARGETS)

molecule_requestor: molecule_requestor.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

molecule_supplier: molecule_supplier.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

atom_warehouse: atom_warehouse.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

atom_supplier: atom_supplier.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean