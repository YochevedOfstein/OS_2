
# Operating Systems: Assignment 2


Names: Lior Telman, Yocheved Ofstein  

------------

## The submission includes:
   - Folders: part_1 through part_6 
   - Source code files  
   - Individual makefiles for each question  
   - Screenshots
   - A main Makefile in the root directory  
   - This README.md file

### Main Makefile

Create a `Makefile` at the repository root to build and clean all exercises:

```makefile
# OS_1/Makefile

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

.PHONY: all clean
```

Run from the root:

```bash
cd OS_2
make all     # builds every exercise
make clean   # cleans every exercise
```