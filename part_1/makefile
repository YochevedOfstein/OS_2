CXX = g++ 
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
TARGETS = atom_warehouse atom_supplier

all: $(TARGETS)

atom_warehouse: atom_warehouse.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

atom_supplier: atom_supplier.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean