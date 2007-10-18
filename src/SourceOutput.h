#ifndef _SOURCE_OUTPUT_H
#define _SOURCE_OUTPUT_H

#include "cranea.h"
#include <map>
#include <string>
#include <fstream>

class SourceBase;
class SourceLocation;

class SourceOutput
{  
public:
    SourceOutput(const std::string &filename);
    void recordObject(SourceBase *object);
    bool isRecorded(SourceBase *object);
    std::ostream &stream();

    void writePlaintext(const std::string &str);

    void startEncryptedBlock();

    void finalize();    
    off_t pos();
    void close();

    void SourceOutput::setInitialLocation(SourceLocation *loc);

    template<typename T>
    void writeValAndReturn(T val, off_t loc)
    {
        off_t cur = out_.tellp();
        out_.seekp(loc);
        canonicalizeEndianness(val);
        out_.write((char *)&val, sizeof(T));
        out_.seekp(cur);
    }

    template<typename T>
    void writeVal(T val)
    {
        canonicalizeEndianness(val);
        out_.write((char *)&val, sizeof(T));
    }

private:

    std::map<byte *, int> gidMaps_[NUM_GLOBAL_MAPS];
    std::map<int, off_t> offsetMap_;
    std::ofstream out_;
    SourceLocation *initialLoc_;
    size_t encryptedStart_;
};

#endif
