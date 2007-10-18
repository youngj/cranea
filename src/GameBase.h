#ifndef _GAMEBASE_H
#define _GAMEBASE_H

#include "CraneaBase.h"
#include "GameInput.h"
#include "cranea.h"

class AutoPumpFilter;
class GameLocation;
class GameAction;
class GameItem;
class GameFile;

typedef std::basic_string<byte> bytestring;


class GameQuitException
{
};

struct KeyBuffer
{
    byte buf[KEY_SIZE];
};

struct KeyHashBuffer
{
    byte buf[KEYHASH_SIZE];
};

class GameContext
{
public:
    GameContext(GameInput &in) : in(&in) 
    {
        commandKey(FORCED_COMMAND, forcedKey_);
        commandKey(FIRST_VISIT_COMMAND, firstVisitKey_);
        commandKey(RETURN_VISIT_COMMAND, returnVisitKey_);
        locationStacks_.push_back(std::vector<GameLocation *>());
    }
    ~GameContext();    

    GameInput *in;

    void pushLocation(GameLocation *loc);

    GameLocation *curLocation(); 
    
    bool load(std::ifstream &in);
    bool save(std::ofstream &out);

    void doCall();
    void doReturn();

    void popLocationUntil(GameLocation *loc);
    void popLocation();

    std::string lastCommand;
    std::string lastArgs;

    void printExpansion(const std::string &story, bool appendNewline = false, int expansionCount = 5);
    
    std::string getPrompt();
    void doCommand(const std::string &cmd);

    size_t inventorySize()
    {
        return inventory_.size();
    }

    GameItem *getInventoryItem(size_t i)
    {
        return inventory_[i];
    }

    void dropInventoryItem(const byte *takeyHash);
    void dropInventoryItem(const byte *takeyHash, GameLocation *whereToDrop);

    GameItem *getInventoryItem(const byte *takeyHash)
    {
        std::map<bytestring, GameItem *>::const_iterator it = inventoryByTakeyHash_.find(bytestring(takeyHash, KEYHASH_SIZE));

        if (it == inventoryByTakeyHash_.end())
        {
            return NULL;
        }

        return it->second;
    }

    const byte *forcedKey() { return forcedKey_; }
    const byte *firstVisitKey() { return firstVisitKey_; }
    const byte *returnVisitKey() { return returnVisitKey_; }

    void addToInventory(GameItem *item);

    // take an item that we have previously dropped. returns true on success.
    bool retakeItem(int gid);

    GameItem *getItem(int gid, const byte *key);
    GameLocation *getLocation(int gid, const byte *key, GameLocation *parent);
    GameLocation *getTopLevelLocation(const byte *key);

    void playGame();

private:

    void printVariable(const std::string &var, int expansionCount);

    void writeLocationChain(std::ofstream &out, GameLocation *loc);
    GameLocation *readLocationChain(std::ifstream &in);

    GameLocation *getSavedLocation(int gid);

    byte forcedKey_[KEY_SIZE];
    byte firstVisitKey_[KEY_SIZE];
    byte returnVisitKey_[KEY_SIZE];

    std::vector<std::vector<GameLocation *> > locationStacks_;
    std::map<int, GameLocation *> savedLocations_;
    std::map<int, GameItem *> savedItems_;
    std::vector<GameItem *> inventory_;
    std::map<bytestring, GameItem *> inventoryByTakeyHash_;
    std::map<int, std::pair<GameItem *, GameLocation *> > droppedItemLocations_;
    std::map<int, std::pair<GameItem *, GameLocation *> > originalItemLocations_;
};


class GameBase : public CraneaBase
{
public:
    GameBase(GameContext &ctx) : CraneaBase(), ctx_(&ctx) {}

    virtual byte *key() { return key_; }
    virtual int gid() { return gid_; }

protected:

    template <typename T>
    static T decryptVal(CryptoPP::StreamTransformationFilter &decryptor)
    {
        T result;
        if (decryptor.Get((byte *)&result, sizeof(T)) != sizeof(T))
        {
            throw "WTF couldn't get enough bytes from decryptor";
        }
        canonicalizeEndianness(result);
        return result;
    }

    typedef GameBase *(* DecryptFn)(GameContext &ctx, AutoPumpFilter &decryptor, GameLocation *parent);

    static GameBase *read(GameContext &ctx, int gid, const byte *key, GameLocation *parent, DecryptFn decryptFn);

    static std::string decryptString(CryptoPP::StreamTransformationFilter &decryptor);

    GameContext *ctx_;
private:
    static GameBase *decryptByType(GameContext &ctx, int objectType, CryptoPP::StreamTransformationFilter &decryptor, GameLocation *parent);
    int gid_;
    byte key_[KEY_SIZE];
};



class GameLocation : public GameBase, public CraneaLocation
{
public:
    GameLocation(GameContext &ctx, GameLocation *parent = NULL) 
      : GameBase(ctx), CraneaLocation(parent), hasEnteredBefore_(false), startKey_(NULL) {}
    virtual ~GameLocation();

    void doCommand(const std::string &cmd);
    GameAction *getAction(const byte *actionKey, bool isExactCommand, bool searchParents = true);

    size_t numItems() { return itemKeys_.size(); }
    GameItem *getItem(size_t i);
    void takeItem(GameItem *item, const byte *takey);
    void dropItem(GameItem *item);
    
    void doAction(const byte *key);

    void getAncestors(std::vector<GameLocation *> &ancestors);

    void enter();
    std::string getPrompt();

    static GameLocation *read(GameContext &ctx, int gid, const byte *key, GameLocation *parent)
    {
        return dynamic_cast<GameLocation *>(GameBase::read(ctx, gid, key, parent, &GameLocation::decrypt));
    }

    GameLocation *gameParent() 
    { 
        if (parent_)
        {
            GameLocation *res = dynamic_cast<GameLocation *>(parent_);
            if (!res)
            {
                throw "OMG WTF BBQ dynamic cast returned null (2)";
            }
            return res;
        }
        return NULL;
    }

    void printVariable(const std::string &var, int expansionCount = 5);

    GameLocation *getChildByKey(const byte *key);
   
protected:
    static GameBase *decrypt(GameContext &ctx, AutoPumpFilter &decryptor, GameLocation *parent);

private:

    void doAction(GameAction *action, const std::vector<std::string> &args = std::vector<std::string>());

    bool hasEnteredBefore_;

    GameAction *getActionInternal(const byte *actionKey, const bytestring &actionKeyHash, bool isExactCommand);

    byte *startKey_;

    std::map<bytestring, std::vector<bytestring> > actionTable_;
    std::vector<bytestring> itemKeys_;
    std::map<bytestring, int> locationTable_;

};


class GameAction : public GameBase, public CraneaAction
{
public:
    GameAction(GameContext &ctx, GameLocation *parent) : GameBase(ctx), CraneaAction(parent) {}

    GameLocation *gameParent() 
    { 
        return parent_ ? dynamic_cast<GameLocation *>(parent_) : NULL;
    }

    static GameAction *read(GameContext &ctx, int gid, const byte *key, GameLocation *parent)
    {
        return dynamic_cast<GameAction *>(GameBase::read(ctx, gid, key, parent, &GameAction::decrypt));
    }

    virtual void doAction(const std::vector<std::string> &args);

    virtual byte *dokey();    

    std::vector<KeyBuffer> locationKeysDown_;
protected:
    static GameBase* decrypt(GameContext &ctx, AutoPumpFilter &decryptor, GameLocation *parent);

    typedef std::pair<std::vector<std::string>, std::string> Predicate;

    bool getSaveFilename(const std::vector<std::string> &args, std::string &filename);

    void iterateItems(const std::vector<GameItem *> &items);
private:
    static GameAction *newActionByType(int actionType, GameContext &ctx, GameLocation *parent);
    static bool getDokeyFromPredicate(GameContext &ctx, AutoPumpFilter &decryptor, byte *dokey);
    void followPath(size_t levelsUp, const std::vector<KeyBuffer> &locationKeysDown);

    struct TakenItem
    {
        byte key[KEY_SIZE];
        byte takey[KEY_SIZE];
        size_t levelsUp;
        std::vector<KeyBuffer> locationKeysDown;
    };

    struct OpenedFile
    {
        byte key[KEY_SIZE];
        bool launch;
    };

    std::vector<TakenItem> takesKeys_;

    std::vector<bytestring> dropsTakeyHashes_;

    std::vector<OpenedFile> files_;

    bool changesLocation_;
    size_t levelsUp_;

    typedef std::pair<std::vector<bytestring>, bytestring> EncryptedPredicate;

    byte dokey_[KEY_SIZE];
};

template <ActionType actionType>
class GameSpecializedAction : public GameAction
{
public:
    GameSpecializedAction(GameContext &ctx, GameLocation *parent) : GameAction(ctx, parent){}
    virtual void doAction(const std::vector<std::string> &args);
};


class GameItem : public GameBase, public CraneaItem
{
public:
    GameItem(GameContext &ctx, GameLocation *parent) : GameBase(ctx), CraneaItem(parent), takey_(NULL) {}
    ~GameItem() { if (takey_) delete[] takey_; }

    std::vector<KeyHashBuffer> titles;

    void setTakey(const byte *takey)
    {
        if (!takey_) 
        {
            takey_ = new byte[KEY_SIZE];
            memcpy(takey_, takey, KEY_SIZE);
        }
    }

    bool hasTitle(const std::string &title);

    virtual byte *takey() { return takey_; }

    static GameItem *read(GameContext &ctx, int gid, const byte *key, GameLocation *parent)
    {
        return dynamic_cast<GameItem *>(GameBase::read(ctx, gid, key, parent, &GameItem::decrypt));
    }
protected:
    static GameBase* decrypt(GameContext &ctx, AutoPumpFilter &decryptor, GameLocation *parent);
private:
    byte *takey_;
};

class GameFile : public GameBase, public CraneaFile
{
public:
    GameFile(GameContext &ctx) : GameBase(ctx), CraneaFile() {}

    void launch();

    static GameFile *read(GameContext &ctx, int gid, const byte *key)
    {
        return dynamic_cast<GameFile *>(GameBase::read(ctx, gid, key, NULL, &GameFile::decrypt));
    }

protected:

    static GameBase* decrypt(GameContext &ctx, AutoPumpFilter &decryptor, GameLocation *parent);
};


#endif
