CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17

TARGETS = uds_server uds_client molecule_supplier_stage5

all: $(TARGETS)

uds_server: uds_server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

uds_client: uds_client.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

molecule_supplier_stage5: molecule_supplier_stage5.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)
