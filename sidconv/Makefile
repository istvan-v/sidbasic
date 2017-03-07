
PROGRAMS = sid_conv sid_dump
CXX = g++
CXXFLAGS = -Wall -O2 -mtune=generic
EP128FLAGS = -I../../ep128emu -I../../ep128emu/src -L../../ep128emu
EP128LIBS = ../../ep128emu/libepcompress.a ../../ep128emu/libep128emu.a
PLUS4FLAGS = -I../../plus4emu -I../../plus4emu/src -L../../plus4emu
PLUS4LIBS = ../../plus4emu/libplus4emu.a

all: $(PROGRAMS)

sid_conv: sid_conv.cpp $(EP128LIBS)
	$(CXX) $(CXXFLAGS) $(EP128FLAGS) $< -o $@ $(EP128LIBS) -lm -s

sid_dump: sid_dump.cpp $(PLUS4LIBS)
	$(CXX) $(CXXFLAGS) $(PLUS4FLAGS) $< -o $@ $(PLUS4LIBS) -lpthread -lm -s

$(EP128LIBS):
	cd ../../ep128emu && scons -j 4 libep128emu.a libepcompress.a

$(PLUS4LIBS):
	cd ../../plus4emu && scons -j 4 libplus4emu.a

clean:
	-rm $(PROGRAMS)

