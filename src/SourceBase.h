#ifndef _SOURCEBASE_H
#define _SOURCEBASE_H

#include "CraneaBase.h"
#include "osrng.h"

class SourceLocation;
class SourceAction;
class SourceItem;
class SourceBase;
class SourceFile;
class SourceOutput;

class InvalidSourceException
{
public:
    InvalidSourceException(std::string msg) : msg(msg)
    {
    }
    const std::string &message() { return msg; }
private:
    std::string msg;
};

class SourceContext
{
public:
    SourceContext() : currentGid_(0) { }
    ~SourceContext();
    void makerand(byte *bytes, size_t len) { rng_.GenerateBlock(bytes, len); }
    int nextGid() { return currentGid_++; }

    void write(SourceOutput &out);

    /* xml parsing */
    void startElement(const std::string &_name, const std::map<std::string, std::string> &attributes);
    void endElement();
    void handleText(const std::string &str);    
    
    SourceLocation *getLocationById(const std::string &id);
    SourceItem *getItemById(const std::string &id);
    SourceFile *getFileById(const std::string &id);

    void resolve();

    std::string gameName;

private:
    CryptoPP::AutoSeededRandomPool rng_;
    int currentGid_;

    bool getBoolAttribute(const std::map<std::string, std::string> &attributes, const std::string &key, bool required, bool def);
    const std::string &getAttribute(const std::map<std::string, std::string> &attributes, const std::string &key, bool required = true, const std::string &def = "");
    
    std::string gameDesc_;
    
    std::string startId_;
    SourceLocation *start_;

    std::vector<std::string> elemStack_;
    std::vector<SourceBase *> objectStack_;

    std::vector<SourceBase *> topLevelObjects_;

    std::vector<SourceLocation *> allLocations_;
    std::vector<SourceItem *> allItems_;
    std::vector<SourceFile *> allFiles_;

    std::map<std::string, SourceLocation *> locationsMap_;
    std::map<std::string, SourceItem *> itemsMap_;
    std::map<std::string, SourceFile *> filesMap_;

    std::string curAuxKey_;
};

class SourceBase : public CraneaBase
{
public:
    virtual ~SourceBase() { if (key_) delete[] key_; }

    virtual bool isTopLevel() = 0;
    virtual ObjectType type() = 0;
    
    virtual byte *key();

    virtual void resolve() {} // resolves any named references to other objects

    virtual int gid() { return gid_; } 

    void write(SourceOutput &out);

protected:
    SourceBase(SourceContext &ctx) 
        : ctx_(&ctx), key_(NULL), gid_(ctx.nextGid()) {}    

    virtual void writeChildren(SourceOutput &out);
    virtual void encrypt(CryptoPP::StreamTransformationFilter &encryptor) = 0;

    void encryptString(CryptoPP::StreamTransformationFilter &encryptor, const std::string &str);

    template <typename T>
    void encryptVal(CryptoPP::StreamTransformationFilter &encryptor, T val);

    void generateKey(byte *outputKey);

    SourceContext *ctx_;
private:
    byte *key_;
    int gid_;
};

class SourceLocation : public SourceBase, public CraneaLocation
{
public:
    SourceLocation(SourceContext &ctx, SourceLocation *parent = NULL) : 
      SourceBase(ctx), CraneaLocation(parent), start(NULL) {}
    virtual ~SourceLocation();

    virtual bool isTopLevel() { return parent_ == NULL; }
    virtual ObjectType type() { return ObjectTypeLocation; }
    
    virtual void resolve();

    std::string id;
    
    std::string startId;
    SourceLocation *start;

    SourceLocation *newChildLocation();
    SourceAction *newChildAction();
    SourceItem *newChildItem();

    std::vector<std::set<std::string> > synonyms;

    std::set<std::string> *getSynonyms(const std::string &token);

    void addIgnored(const std::string &token);

    void getPath(SourceLocation *other, size_t &levelsUp, std::vector<SourceLocation *> &locationsDown);

    SourceLocation *sourceParent() 
    { 
        return (parent_) ? dynamic_cast<SourceLocation *>(parent_) : NULL;
    }

    void expandString(const std::string &str, std::vector<std::string> &expanded);    

protected:
    virtual void encrypt(CryptoPP::StreamTransformationFilter &encryptor);

    virtual void writeChildren(SourceOutput &out);

private:
    struct ExpandedAction
    {
        ExpandedAction(SourceAction *action, std::string cmd) : action(action), cmd(cmd) {}
        SourceAction *action;
        std::string cmd;
    };

    std::set<std::string> *getLocalSynonyms(const std::string &token);

    void getExpandedActions(std::vector<ExpandedAction> &expandedActions);

    void getAncestors(std::vector<SourceLocation *> &ancestors);

    void expandStringInternal(std::vector<std::string> &tokens, size_t i, std::vector<std::string> &commands);

    std::vector<SourceLocation *> locations_;
    std::vector<SourceAction *> actions_;
    std::vector<SourceItem *> items_;

    std::map<std::string, std::set<std::string> *> mergedSynonymsMap_;
};

typedef std::vector<SourceItem *> Conjunction;

typedef std::vector<std::string> UnresolvedConjunction;

class SourceAction : public SourceBase, public CraneaAction
{
public:
    SourceAction(SourceContext &ctx, SourceLocation *loc) :
  SourceBase(ctx), CraneaAction(loc), dest(NULL), exact(false), actionType(ActionTypeDefault), dokey_(NULL) {}
    virtual ~SourceAction() { if (dokey_) delete[] dokey_; }

    std::string destId;
    SourceLocation *dest;

    std::vector<std::pair<std::string, bool> > fileIds;
    std::vector<std::pair<SourceFile *, bool> > files;

    virtual void resolve();

    std::vector<Conjunction> predicate; /* in form ((item1^item2)v(item3^item4^item5)v(...)) */

    bool exact;

    std::vector<UnresolvedConjunction> predicateIds;

    std::vector<std::string> commands;

    Conjunction takes;
    UnresolvedConjunction takesIds;

    Conjunction drops;
    UnresolvedConjunction dropsIds;

    virtual void writeChildren(SourceOutput &out);

    ActionType actionType;

    virtual bool isTopLevel() { return false; }
    virtual ObjectType type() { return ObjectTypeAction; }

    virtual byte *dokey(); 

    SourceLocation *sourceParent() 
    { 
        return parent_ ? dynamic_cast<SourceLocation *>(parent_) : NULL;
    }

protected:
    virtual void encrypt(CryptoPP::StreamTransformationFilter &encryptor);

private:
    void encryptPath(CryptoPP::StreamTransformationFilter &innerEncryptor, SourceLocation *otherLoc);
    void resolveConjunction(const UnresolvedConjunction &ids, Conjunction &conj);
    byte *dokey_;

};

class SourceItem : public SourceBase, public CraneaItem
{
public:
    SourceItem(SourceContext &ctx, SourceLocation *parent) : 
      SourceBase(ctx), CraneaItem(parent), takey_(NULL) {}
    
    virtual ~SourceItem() { if (takey_) delete[] takey_; }

    virtual bool isTopLevel() { return true; }
    virtual ObjectType type() { return ObjectTypeItem; }

    virtual void resolve();
    virtual byte *takey();

    std::string id;
    std::vector<std::string> titles;

    SourceLocation *sourceParent() 
    { 
        return (parent_) ? dynamic_cast<SourceLocation *>(parent_) : NULL;
    }

protected:
    virtual void encrypt(CryptoPP::StreamTransformationFilter &encryptor);

private:
    byte *takey_;
};


class SourceFile : public SourceBase, public CraneaFile
{
public:
    SourceFile(SourceContext &ctx) : SourceBase(ctx), CraneaFile() {}

    std::string id;
    std::string src;

    virtual bool isTopLevel() { return true; }
    virtual ObjectType type() { return ObjectTypeFile; }
    virtual void resolve();

protected:
    virtual void encrypt(CryptoPP::StreamTransformationFilter &encryptor);

};

#endif 
