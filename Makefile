CXXFLAGS ?= -Wall -pedantic

d-demangle.o: d-demangle.cc d-demangle.h
	$(CXX) $(CXXFLAGS) -c -o d-demangle.o d-demangle.cc

d-demangle-main: d-demangle-main.cc d-demangle.o d-demangle.h
	$(CXX) $(CXXFLAGS) -o d-demangle d-demangle-main.cc d-demangle.o
