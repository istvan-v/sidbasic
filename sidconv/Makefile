
PROGRAMS = sid_conv sid_dump
CXX = g++
CXXFLAGS = -Wall -O2 -mtune=generic
EP128FLAGS = -I./epcompress
EP128LIBS = ./epcompress/compress.cpp ./epcompress/compress2.cpp	\
	    ./epcompress/decompress2.cpp ./epcompress/comprlib.cpp	\
	    ./epcompress/decompm2.cpp
PLUS4FLAGS = -I../../plus4emu -I../../plus4emu/src -L../../plus4emu
PLUS4LIBS = ../../plus4emu/libplus4emu.a

all: $(PROGRAMS)

sid_conv: sid_conv.cpp $(EP128LIBS)
	$(CXX) $(CXXFLAGS) $(EP128FLAGS) $< -o $@ $(EP128LIBS) -lm -s

sid_dump: sid_dump.cpp $(PLUS4LIBS)
	$(CXX) $(CXXFLAGS) $(PLUS4FLAGS) $< -o $@ $(PLUS4LIBS) -lpthread -lm -s

$(PLUS4LIBS):
	cd ../../plus4emu && scons -j 4 libplus4emu.a

clean:
	-rm $(PROGRAMS)

