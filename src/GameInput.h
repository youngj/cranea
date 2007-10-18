#ifndef _GAME_INPUT_H_
#define _GAME_INPUT_H_

#include <iostream>
#include <fstream>
#include <string>
#include "cranea.h"

class CraneaBase;

class GameInput
{
public:
    GameInput(const std::string &infile);
    bool fail();
    
    std::ifstream &stream();

    int getGid(ObjectType objectType, const byte *key);

    bool seekObject(int gid);

    byte *initialKey();

    template <typename T>
    T readVal();

private:
    std::ifstream in_;
    bool fail_;
    byte initialKey_[KEY_SIZE];
    std::map<std::string, int> gidMaps_[NUM_GLOBAL_MAPS];
    std::map<int, off_t> offsetMap_;    
};

#endif
