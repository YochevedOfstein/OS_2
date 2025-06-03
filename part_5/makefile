CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17

TARGETS = uds_server uds_client

all: $(TARGETS)

uds_server: uds_server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

uds_client: uds_client.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)
