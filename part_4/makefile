CXX = g++ 
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
TARGETS = drinks_bar molecule_requester

all: $(TARGETS)

molecule_requester: molecule_requester.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

drinks_bar: drinks_bar.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean