
CXXFLAGS=-std=c++17 -Wall -Wextra -pedantic -I..
# CXXFLAGS=-std=c++17 -Werror -Wall -Wextra -Wshadow=local -Wold-style-cast \
	 -Wpedantic -Wsign-conversion -Wcast-align=strict -I..

all: lc3al.exe lc3.exe

%.exe: %.cpp
	$(LINK.cpp) $^ $(LDFLAGS) -o $@

.PHONY: clean lc3 lc3al

clean:
	del lc3al.exe lc3.exe
lc3: lc3.exe
lc3al: lc3al.exe
