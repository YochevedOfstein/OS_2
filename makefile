CXX = g++ 
CXXFLAGS = -std=c++11 -Wall -Wextra -O2

all:
	$(MAKE) -C part_1
	$(MAKE) -C part_2
	$(MAKE) -C part_3
	$(MAKE) -C part_4
	$(MAKE) -C part_5

clean:
	$(MAKE) -C part_1 clean
	$(MAKE) -C part_2 clean
	$(MAKE) -C part_3 clean
	$(MAKE) -C part_4 clean
	$(MAKE) -C part_5 clean

.PHONY: all cleanS