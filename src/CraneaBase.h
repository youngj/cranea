#ifndef _CRANEA_BASE_H_
#define _CRANEA_BASE_H_

class CraneaAction;
class CraneaLocation;

#if 0
class CraneaItem;
class CraneaFile;
#endif

#include "cranea.h"
#include <vector>
#include <string>
#include <set>
#include <map>
#include "filters.h"

#define ENCRYPTED_EXT ".cra"

std::string baseFileName(const std::string &path);

void cleanFilename(std::string &filename, bool allowSubdir = false);
bool isOkFilenameChar(char c);
bool isOkPathChar(char c);

void hashKey(const byte *key, byte *keyhash);

/* transformString(text,fn)
 * ========================
 * Transforms the text by calling fn on each of its characters.
 */
template<typename FnType>
void transformString(std::string &text, FnType fn)
{
	transform(text.begin(), text.end(), text.begin(), fn);
}

void transformLower(std::string &text);

void concatenateTokens(const std::vector<std::string> &tokens, std::string &str);

void commandKey(const std::string &cmd, byte *outputKey);

void debugBinary(const byte *buf, size_t len);

class CraneaBase
{
public:
    CraneaBase() : keyhash_(NULL) {}
    virtual ~CraneaBase() { if (keyhash_) delete[] keyhash_; }   

    virtual byte *key() = 0; // the key you need in order to decrypt this object (at the outermost level)
                            // some objects have multiple layers of encryption on parts of their content,
                            // see for instance the item takey and the action dokey.
    byte *keyhash(); // 1-way hash of the above key.      

    virtual int gid() = 0;

private:    

    byte *keyhash_;
};

class CraneaLocation 
{
public:
    CraneaLocation(CraneaLocation *parent = NULL) : parent_(parent) {}
    virtual ~CraneaLocation() {}

    std::string title;
    std::string desc;
    std::string prompt;

    bool isIgnored(const std::string &token);

    void tokenize(const std::string &cmd, std::vector<std::string> &tokens);
    void normalize(std::string &cmd);
    
protected:
    CraneaLocation *parent_;
    std::set<std::string> ignoredSet_;   
private:

    bool isIgnoredInternal(const std::string &token);

};

class CraneaAction
{
public:
    CraneaAction(CraneaLocation *parent) : parent_(parent) {}
    virtual ~CraneaAction() {}

    std::string desc;       
    std::map<std::string, std::string> auxData;

    virtual byte *dokey() = 0; // the key that you need to 'do' this action -- you get this by satisfying the predicate

protected:
    CraneaLocation *parent_;
};

class CraneaItem
{
public:
 CraneaItem(CraneaLocation *parent) :  restrictTake(true), visible(true), parent_(parent), takeyhash_(NULL) {}
    virtual ~CraneaItem() { if (takeyhash_) delete[] takeyhash_; }

    std::string title;
    std::string desc;
    bool restrictTake;    
    bool visible;

    virtual byte *takey() = 0; // the key you get when you are allowed to 'take' this item. 
                                // it may be included in the item itself, if the item is freely take-able.
                                // if the item cannot be freely taken, it is included in the 'gets' list 
                                // for the actions that are allowed to get this item.
    byte *takeyhash();

protected:
    CraneaLocation *parent_;
private:
    byte *takeyhash_;
};


template <typename ItemType>
void makeConjunctionKey(const std::vector<ItemType *> &conjunction, byte *outputKey)
{
    memset(outputKey, 0, KEY_SIZE);

    size_t conjunctionLen = conjunction.size();

    for (size_t i = 0; i < conjunctionLen; i++)
    {
        ItemType *requiredItem = conjunction[i];
        byte *itemTakey = requiredItem->takey();

        for (size_t j = 0; j < (size_t)KEY_SIZE; j++)
        {
            outputKey[j] ^= itemTakey[j];
        }
    }    
}

class CraneaFile
{
public:
    std::string dest;
};

template <typename ValType>
void swapEndianness(ValType &val)
{
    byte tmp;
    byte *start = (byte *) &val;
    byte *end = start + sizeof(ValType) - 1;
    for ( ; start < end; ++start, --end)
    {
        tmp = *start;
        *start = *end;
        *end = tmp;
    }
}

const short EndianTest = 0x0100;

#define CRANEA_BIG_ENDIAN (*(byte *) &EndianTest)

template <typename ValType>
inline void canonicalizeEndianness(ValType &val)
{
    if (CRANEA_BIG_ENDIAN)
    {
        swapEndianness(val);
    }
}



#endif 
