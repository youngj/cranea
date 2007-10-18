#include "SourceOutput.h"
#include "SourceBase.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace std;

SourceOutput::SourceOutput(const string &filename) :  
    out_(filename.c_str(), ios::binary | ios::out | ios::trunc), encryptedStart_(0)
{
}

void SourceOutput::writePlaintext(const std::string &str)
{
    if (str.find('\0') != string::npos)
    {
        throw "Embedded NUL character in string";
    }

    if (!str.empty())
    {
        out_.write(str.c_str(), str.length());
    }
}

void SourceOutput::startEncryptedBlock()
{
    char buf[KEY_SIZE + (1 + NUM_GLOBAL_MAPS) * sizeof(off_t)];
    out_.put('\0');

    encryptedStart_ = out_.tellp();

    out_.write(buf, sizeof(buf));
}


void SourceOutput::setInitialLocation(SourceLocation *loc)
{
    initialLoc_ = loc;
}

bool SourceOutput::isRecorded(SourceBase *object)
{
    return offsetMap_.find(object->gid()) != offsetMap_.end();
}

void SourceOutput::recordObject(SourceBase *object)
{
    int gid = object->gid();
    offsetMap_[gid] = out_.tellp();
    if (object->isTopLevel())
    {
        gidMaps_[object->type()][object->keyhash()] = gid;
    }
}

ostream &SourceOutput::stream()
{
    return out_;
}

void SourceOutput::finalize()
{
    // writes the offsetMap and stores its offset at the beginning of the file

    off_t mapOffset = out_.tellp();

    writeVal<size_t>(offsetMap_.size());

    for (map<int, off_t>::const_iterator it = offsetMap_.begin(); it != offsetMap_.end(); ++it)
    {
        writeVal<int>(it->first);
        writeVal<off_t>(it->second);
    }
    writeValAndReturn<off_t>(mapOffset, encryptedStart_);

    // writes the gidMaps and stores their offset at the beginning of the file
    for (int i = 0; i < NUM_GLOBAL_MAPS; i++)
    {
        mapOffset = out_.tellp();
        
        map<byte *, int> &gidMap = gidMaps_[i];

        writeVal<size_t>(gidMap.size());
        
        for (map<byte *, int>::const_iterator it = gidMap.begin(); it != gidMap.end(); ++it)
        {
            byte *objectKeyHash = it->first;         

            out_.write((char *)objectKeyHash, KEYHASH_SIZE);
            writeVal<int>(it->second);
        }            

        writeValAndReturn<off_t>(mapOffset, encryptedStart_ + (i+1) * sizeof(off_t));
    }
    
    if (initialLoc_)
    {
        out_.seekp(encryptedStart_ + (1 + NUM_GLOBAL_MAPS) * sizeof(off_t));
        out_.write((char *)initialLoc_->key(), KEY_SIZE);
    }
}
    
off_t SourceOutput::pos()
{
    return out_.tellp();
}

void SourceOutput::close()
{
    out_.close();
}
