#include <iostream>
#include <fstream>
using namespace std;

#include "CraneaBase.h"
#include "GameInput.h"

bool GameInput::seekObject(int gid)
{
    map<int, off_t>::iterator pos = offsetMap_.find(gid);
    if (pos == offsetMap_.end())
    {
        return false;
    }
    off_t offset = pos->second;
    in_.seekg(offset);
    return true;
}

int GameInput::getGid(ObjectType objectType, const byte *key)
{
    byte keyhash[KEYHASH_SIZE];

    hashKey(key, keyhash);

    string keyStr((char*)keyhash, KEYHASH_SIZE);
    map<string, int>::iterator pos = gidMaps_[objectType].find(keyStr);
    if (pos == gidMaps_[objectType].end())
    {
        return -1;
    }
    return pos->second;
}

byte *GameInput::initialKey()
{
    return initialKey_;
}

GameInput::GameInput(const string &infile)
    : in_(infile.c_str(), ios::in | ios::binary)
{
    fail_ = in_.fail();
    
    if (!fail_)
    {
        // read until first nul byte
        char ch;
        do
        {
            ch = in_.get();
        }
        while (ch != '\0' && !in_.eof());

        off_t offsetTableOffset = readVal<off_t>();

        off_t gidTableOffsets[NUM_GLOBAL_MAPS];
        for (size_t i = 0; i < NUM_GLOBAL_MAPS; i++)
        {
            gidTableOffsets[i] = readVal<off_t>();
        }

        // read the initial location key
        in_.read((char *)initialKey_, KEY_SIZE);

        // read the offset table (gid -> file offset)
        in_.seekg(offsetTableOffset);
        size_t numEntries = readVal<size_t>();

        //cout << numEntries << " entries in the global offset table" << endl;

        for (size_t i = 0; i < numEntries; i++)
        {
            int gid = readVal<int>();
            off_t offset = readVal<off_t>();
            offsetMap_[gid] = offset;
        }

        // read the tables of top-level objects (keyhash -> gid)
        for (size_t i = 0; i < NUM_GLOBAL_MAPS; i++)
        {
            map<string, int> &gidMap = gidMaps_[i];

            in_.seekg(gidTableOffsets[i]);
            numEntries = readVal<size_t>();

            //cout << numEntries << " entries in map " << i << endl;

            for (size_t j = 0; j < numEntries; j++)
            {
                char keyhash[KEYHASH_SIZE];
                in_.read(keyhash, KEYHASH_SIZE);
                string keyhashStr(keyhash, KEYHASH_SIZE);

                int gid = readVal<int>();
                gidMap[keyhashStr] = gid;                
            }
        }
    }
}

ifstream &GameInput::stream()
{
    return in_;
}

template <typename T>
T GameInput::readVal()
{
    T result;
    in_.read((char *)&result, sizeof(T));
    canonicalizeEndianness(result);
    return result;
}

bool GameInput::fail()
{
    return fail_;
}
