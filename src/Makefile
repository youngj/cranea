#
# Makefile for Cranea text adventure compiler and player.
# Adjust CRYPTOPP_DIR and EXPAT_DIR as necessary.
# It assumes that Crypto++ and Expat static libraries are already built and 
# can be found at EXPAT_LIB and CRYPT_LIB.
#

CRYPTOPP_DIR = cryptopp552
EXPAT_DIR = expat201

# use DBGFLAGS instead of RELFLAGS if you want debugging symbols

DBGFLAGS = -g
RELFLAGS = -O3 -DNDEBUG

CPPFLAGS = -Wall -I$(CRYPTOPP_DIR) -I$(EXPAT_DIR)/lib $(RELFLAGS)
CXX = g++
LDFLAGS = 

EXPAT_LIB = $(EXPAT_DIR)/.libs/libexpat.a 
CRYPT_LIB = $(CRYPTOPP_DIR)/libcryptopp.a

COMPILER_SRCS = compiler.cpp CraneaBase.cpp SourceBase.cpp SourceOutput.cpp
COMPILER_H = CraneaBase.h SourceBase.h SourceOutput.h
COMPILER_OBJS = $(COMPILER_SRCS:.cpp=.o)
COMPILER_EXECUTABLE = compiler.exe

PLAYER_SRCS = player.cpp GameBase.cpp CraneaBase.cpp GameInput.cpp
PLAYER_H = GameBase.h CraneaBase.h GameInput.h
PLAYER_OBJS = $(PLAYER_SRCS:.cpp=.o)
PLAYER_EXECUTABLE = player.exe

default: $(COMPILER_EXECUTABLE) $(PLAYER_EXECUTABLE)

compiler: $(COMPILER_EXECUTABLE)

player: $(PLAYER_EXECUTABLE)

compiler.exe: $(COMPILER_OBJS)
	$(CXX) -o $@ $(COMPILER_OBJS) $(EXPAT_LIB) $(CRYPT_LIB) $(LDFLAGS)

player.exe: $(PLAYER_OBJS)
	$(CXX) -o $@ $(PLAYER_OBJS) $(CRYPT_LIB) $(LDFLAGS)

clean:
	rm -f *~ *.o $(COMPILER_EXECUTABLE) $(PLAYER_EXECUTABLE)

