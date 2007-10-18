#include "GameBase.h"
#include "GameInput.h"

using namespace std;
using namespace CryptoPP;

#include "cryptlib.h"
#include "filters.h"
#include "files.h"
#include "modes.h"
#include "aes.h"
#include <fstream>
#include <map>

#ifdef WIN32
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN			
#include <windows.h> // for Sleep
#else

#include <unistd.h> // for usleep

void Sleep(unsigned int ms)
{
  unsigned int sec = ms / 1000;
  ms = ms % 1000;

  sleep(sec);
  usleep(1000 * ms);
}

#endif

#include <sys/stat.h> 


class AutoPumpFilter : public StreamTransformationFilter
{
public:
    AutoPumpFilter(StreamTransformation &c, Source &pumpee, size_t pumpSlack = AES::BLOCKSIZE) 
        : StreamTransformationFilter(c), pumpee_(&pumpee), pumpSlack_(pumpSlack)
    {
    }

    virtual ~AutoPumpFilter() 
    {
    }

    virtual size_t Get(byte &outByte)
    {
        return this->Get(&outByte, 1);
    }

    virtual size_t Get(byte *outString, size_t getMax)
    {
        size_t res = this->StreamTransformationFilter::Get(outString, getMax);

        if (res < getMax)
        {
            size_t left = getMax - res;
            pumpee_->Pump(left + pumpSlack_);
            res += this->StreamTransformationFilter::Get(outString + res, left);
        }
        return res;
    }

    Source &GetSource()
    {
        return *pumpee_;
    }
private:
    Source *pumpee_;
    size_t pumpSlack_;
};

GameContext::~GameContext()
{
    for (map<int, GameItem *>::iterator it = savedItems_.begin(); it != savedItems_.end(); ++it)
    {
        delete it->second;
    }

    for (map<int, GameLocation *>::iterator it = savedLocations_.begin(); it != savedLocations_.end(); ++it)
    {
        delete it->second;
    }
}

template <typename ElemType>
ElemType readVal(ifstream &in)
{
    ElemType val;
    in.read((char *)&val, sizeof(ElemType));
    canonicalizeEndianness(val);
    return val;
}

GameLocation *GameContext::readLocationChain(ifstream &in)
{
    size_t numAncestors = readVal<size_t>(in);

    GameLocation *loc = NULL;
    for (size_t i = 0; i < numAncestors; i++)
    {
        int gid = readVal<int>(in);
        byte locationKey[KEY_SIZE];
        in.read((char *)locationKey, KEY_SIZE);

        loc = getLocation(gid, locationKey, loc);
        assert(loc);
    }
    return loc;
}

void splitVariable(const string &var, string &first, string &rest)
{
    size_t dot = var.find('.');
    if (dot == string::npos)
    {
        first = var;
        rest = "";
    }
    else
    {
        first = var.substr(0, dot);
        rest = var.substr(dot + 1);
    }
}

void GameLocation::printVariable(const string &var, int expansionCount)
{
    if (var == "title")
    {
        ctx_->printExpansion(title, false, expansionCount);
    }
    else if (var == "desc")
    {
        ctx_->printExpansion(desc, false, expansionCount);
    }
    else if (var == "gid")
    {
        cout << gid();
    }
    else if (var == "prompt")
    {
        cout << getPrompt();
    }
}

void GameContext::printVariable(const string &var, int expansionCount)
{
    if (var == "d")
    {
        Sleep(500);
    }
    else if (var == "p")
    {
        string line;
        getline(cin, line);
    }
    else if (var == "cmd")
    {
        cout << lastCommand;
    }
    else if (var == "args")
    {
        cout << lastArgs;
    }
    else if (var == "s")
    {
        cout << " ";
    }
    else if (var == "br")
    {
        cout << endl;
    }
    else
    {
        string first, rest;
        splitVariable(var, first, rest);

        if (first == "location")
        {
            curLocation()->printVariable(rest, expansionCount);
        }
    }
}

bool GameContext::load(ifstream &in)
{
    if (in.fail())
        return false;

    GameContext backup(*this);

    locationStacks_.clear();
    savedLocations_.clear();
    savedItems_.clear();
    inventory_.clear();
    inventoryByTakeyHash_.clear();
    droppedItemLocations_.clear();
    originalItemLocations_.clear();

    size_t numLocationStacks = readVal<size_t>(in);

    //cout << "# location stacks: " << numLocationStacks << endl;

    for (size_t i = 0; i < numLocationStacks; i++)
    {
        locationStacks_.push_back(vector<GameLocation *>());
        vector<GameLocation *> &locationStack = locationStacks_.back();

        size_t locationsInStack = readVal<size_t>(in);

        //cout << "# locations in stack: " << locationsInStack << endl;

        GameLocation *parent = NULL;
        for (size_t j = 0; j < locationsInStack; j++)
        {
            int gid = readVal<int>(in);
            byte locationKey[KEY_SIZE];
            in.read((char *)locationKey, KEY_SIZE);

            GameLocation *loc = getLocation(gid, locationKey, parent);

            locationStack.push_back(loc);
            //cout << "pushed " << loc->title << " onto location stack" << endl;

            parent = loc;
        }
    }
    
    // items to take from original location (from inventory):
    // (gid, item key, item takey, (original loc gid, original loc key)+)+

    size_t inventorySize = readVal<size_t>(in);

    for (size_t i = 0; i < inventorySize; i++)
    {        
        int itemGid = readVal<int>(in);
        
        byte itemKey[KEY_SIZE];
        in.read((char *)itemKey, KEY_SIZE);

        byte itemTakey[KEY_SIZE];
        in.read((char *)itemTakey, KEY_SIZE);

        GameItem *item = getItem(itemGid, itemKey);

        GameLocation *originalLoc = readLocationChain(in);

        originalLoc->takeItem(item, itemTakey);
    }

    // items to take from some location and drop on a new location (from droppedItemLocations):
        // (gid, item key, item takey, (original loc gid, original loc key)+, (dropped loc gid, dropped loc key)+)+

    size_t numMovedItems = readVal<size_t>(in);

    for (size_t i = 0; i < numMovedItems; i++)
    {
        int itemGid = readVal<int>(in);
        byte itemKey[KEY_SIZE];
        in.read((char *)itemKey, KEY_SIZE);

        byte itemTakey[KEY_SIZE];
        in.read((char *)itemTakey, KEY_SIZE);

        GameItem *item = getItem(itemGid, itemKey);

        GameLocation *originalLoc = readLocationChain(in);

        originalLoc->takeItem(item, itemTakey);

        GameLocation *droppedLoc = readLocationChain(in);

        dropInventoryItem(item->takeyhash(), droppedLoc);
    }    

    bool success = !in.fail();
    if (!success)
    {
        *this = backup;

        /* prevent deleting items/locations when backup gets destructed 
         * (if success == true, we want to delete the old items/locations)
         */
        backup.savedLocations_.clear();
        backup.savedItems_.clear();
    }
    
    //cout << "current location: " << curLocation()->title << endl;

    return success;
}

template <typename ElemType>
void writeVal(ofstream &out, ElemType val)
{
    canonicalizeEndianness(val);
    out.write((char *)&val, sizeof(ElemType));
}

void GameContext::writeLocationChain(ofstream &out, GameLocation *loc)
{
    vector<GameLocation *> locAncestors;
    loc->getAncestors(locAncestors);
    size_t numAncestors = locAncestors.size();
    writeVal<size_t>(out, numAncestors);
    for (int i = (int)numAncestors - 1; i >= 0; i--) /* write locations starting from root */
    {
        GameLocation *ancestorLoc = locAncestors[i];
        writeVal<int>(out, ancestorLoc->gid());
        out.write((char *)ancestorLoc->key(), KEY_SIZE);
    }
}

bool GameContext::save(ofstream &out)
{
    if (out.fail())
        return false;

    // # locationStacks (# locations in stack, (gid, location key)+)+
    size_t numLocationStacks = locationStacks_.size();
    writeVal<size_t>(out, numLocationStacks);

    for (size_t i = 0; i < numLocationStacks; i++)
    {
        vector<GameLocation *> &locationStack = locationStacks_[i];
        size_t locationsInStack = locationStack.size();

        writeVal<size_t>(out, locationsInStack);

        for (size_t j = 0; j < locationsInStack; j++)
        {
            GameLocation *loc = locationStack[j];
            writeVal<int>(out, loc->gid());
            out.write((char *)loc->key(), KEY_SIZE);
        }
    }

    // items to take from original location (from inventory):
    // (gid, item key, item takey, (original loc gid, original loc key)+)+

    size_t inventorySize = inventory_.size();
    writeVal<size_t>(out, inventorySize);

    for (size_t i = 0; i < inventorySize; i++)
    {
        GameItem *item = inventory_[i];
        writeVal<int>(out, item->gid());
        out.write((char *)item->key(), KEY_SIZE);
        out.write((char *)item->takey(), KEY_SIZE);

        GameLocation *originalLoc = originalItemLocations_[item->gid()].second;

        writeLocationChain(out, originalLoc);
    }

    // items to take from some location and drop on a new location (from droppedItemLocations):
        // (gid, item key, item takey, (original loc gid, original loc key)+, (dropped loc gid, dropped loc key)+)+

    writeVal<size_t>(out, droppedItemLocations_.size());

    for (map<int, pair<GameItem *, GameLocation *> >::const_iterator it = droppedItemLocations_.begin(); 
            it != droppedItemLocations_.end(); ++it)
    {
        GameItem *item = it->second.first;
        GameLocation *droppedLoc = it->second.second;

        writeVal<int>(out, item->gid());
        out.write((char *)item->key(), KEY_SIZE);
        out.write((char *)item->takey(), KEY_SIZE);

        GameLocation *originalLoc = originalItemLocations_[item->gid()].second;

        writeLocationChain(out, originalLoc);
        writeLocationChain(out, droppedLoc);
    }

    return !out.fail();
}

std::string GameContext::getPrompt()
{
    return curLocation()->getPrompt();
}

void GameContext::doCommand(const string &cmd)
{
    curLocation()->doCommand(cmd);
}

GameItem *GameContext::getItem(int gid, const byte *key)
{
    map<int, GameItem *>::iterator it = savedItems_.find(gid);
    if (it != savedItems_.end())
    {   
        return it->second;
    }
    else
    {
        GameLocation *curLoc = curLocation();
        GameItem *item = GameItem::read(*this, gid, key, curLoc);    
        if (item)
        {
            savedItems_[gid] = item;
            originalItemLocations_[gid] = pair<GameItem *, GameLocation *>(item, curLoc);
        }
        return item;
    }
}

void GameContext::playGame()
{
    GameLocation *loc = getTopLevelLocation(in->initialKey());
   
    if (!loc)
    {
        cout << "invalid data file: no initial location" << endl;
        return;
    }

    pushLocation(loc);
    loc->enter();

    while (true)
    {
        printExpansion(getPrompt());

        string cmd;
        getline(cin, cmd);

        try 
        {
            doCommand(cmd);
        }
        catch (GameQuitException &ex)
        {
            ex;
            break;
        }        
    }
}

GameLocation *GameContext::getSavedLocation(int gid)
{
    std::map<int, GameLocation *>::iterator it = savedLocations_.find(gid);
    if (it != savedLocations_.end())
    {
        return it->second;
    }
    return NULL;
}

void trim(string &str)
{
    size_t start = str.find_first_not_of(' ');
    if (start != string::npos)
    {
        str = str.substr(start, str.find_last_not_of(' ') - start + 1);
    }
    else
    {
        str = "";
    }
}

void GameContext::addToInventory(GameItem *item)
{
    inventory_.push_back(item);
    inventoryByTakeyHash_[bytestring(item->takeyhash(), KEYHASH_SIZE)] = item;
}

bool GameContext::retakeItem(int gid)
{
    std::map<int, pair<GameItem *, GameLocation *> >::iterator it = droppedItemLocations_.find(gid);

    if (it != droppedItemLocations_.end())
    {
        GameItem *item = it->second.first;
        GameLocation *loc = it->second.second;
        loc->takeItem(item, item->takey());
        droppedItemLocations_.erase(it);
        return true;
    }
    else
    {
        return false;
    }
}

GameLocation *GameContext::getLocation(int gid, const byte *key, GameLocation *parent)
{
    GameLocation *loc = getSavedLocation(gid);
    if (loc)
    {
        return loc;
    }

    loc = GameLocation::read(*this, gid, key, parent);

    if (loc)
    {
        savedLocations_[loc->gid()] = loc;
    }

    return loc;
}
void GameContext::dropInventoryItem(const byte *takeyHash)
{
    dropInventoryItem(takeyHash, curLocation());
}

void GameContext::dropInventoryItem(const byte *takeyHash, GameLocation *whereToDrop)
{
    std::map<bytestring, GameItem *>::iterator it = inventoryByTakeyHash_.find(bytestring(takeyHash, KEYHASH_SIZE));

    if (it != inventoryByTakeyHash_.end())
    {
        GameItem *item = it->second;
        for (size_t i = 0; i < inventory_.size(); i++)
        {
            if (inventory_[i] == item)
            {
                inventory_.erase(inventory_.begin() + i);
                i--;
            }
        }

        whereToDrop->dropItem(item);
        droppedItemLocations_[item->gid()] = pair<GameItem *, GameLocation *>(item, whereToDrop);

        inventoryByTakeyHash_.erase(it);
    }
}

void GameContext::popLocationUntil(GameLocation *loc)
{
    vector<GameLocation *> &curLocationStack = locationStacks_.back();
    while (!curLocationStack.empty() && curLocationStack.back() != loc)
    {
        curLocationStack.pop_back();
    }
}

void GameContext::popLocation()
{ 
    locationStacks_.back().pop_back(); 
}

GameLocation *GameContext::curLocation()
{ 
    return locationStacks_.back().empty() ? NULL : locationStacks_.back().back(); 
}

void GameContext::pushLocation(GameLocation *loc)
{ 
    locationStacks_.back().push_back(loc); 
}

void GameContext::doCall()
{   
    locationStacks_.push_back(vector<GameLocation *>());
    
    // duplicate the previous location stack
    locationStacks_.back() = locationStacks_[locationStacks_.size() - 2];
}

void GameContext::doReturn()
{
    if (locationStacks_.size() > 1)
    {
        locationStacks_.pop_back();
    }
}

GameLocation *GameContext::getTopLevelLocation(const byte *key)
{
    int gid = in->getGid(ObjectTypeLocation, key);    
    return getLocation(gid, key, NULL);
}

void GameContext::printExpansion(const string &story, bool appendNewline, int expansionCount)
{
    string line;
	size_t start = 0; // where to start looking for open brackets { 
    size_t nextPrint = 0; // the first character we haven't printed yet
    size_t len = story.length();

	while (true)
	{
		size_t bracket = story.find("{", start);        

		if (bracket != string::npos)
		{
            size_t endBracket = string::npos;
            size_t cur = bracket + 1;            
            while (cur < len)
            {
                char ch = story[cur];
                if (ch == '}')
                {
                    endBracket = cur;
                    break;
                }
                if (ch == ' ' || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_' || ch == '.' || (ch >= '0' && ch <= '9'))
                {
                    cur++;
                }
                else
                {
                    break;
                }
            }

            if (endBracket != string::npos)
            {
                cout << story.substr(nextPrint, bracket - nextPrint);

                if (expansionCount > 0)
                {
                    string var = story.substr(bracket + 1, endBracket - bracket - 1);
                    trim(var);
                    transformLower(var);
                    printVariable(var, expansionCount - 1);
                }
                start = nextPrint = endBracket + 1;
            }
            else
            {
                start = bracket + 1;
            }
        }
        else // no more start brackets in the story
        {
            if (!story.empty())
            {
                cout << story.substr(nextPrint);
                if (appendNewline)
                {
                    cout << endl;
                }
            }
            return;
        }
    } // end while loop
}

string GameBase::decryptString(StreamTransformationFilter &decryptor)
{
    size_t len = decryptVal<int>(decryptor);
    byte * buf = new byte[len];
    if (decryptor.Get(buf, len) != len)
    {
        throw "WTF2 couldn't get enough bytes from decryptor";
    }
    string result((char*)buf, len);

    delete buf;

    return result;
}

GameBase *GameBase::read(GameContext &ctx, int gid, const byte *key, GameLocation *parent, DecryptFn decryptFn)
{
    if (!ctx.in->seekObject(gid))
    {
        return NULL;
    }

    byte iv[AES::BLOCKSIZE];
    ctx.in->stream().read((char *)iv, AES::BLOCKSIZE);

    CFB_Mode<AES>::Decryption decryption(key, KEY_SIZE, iv);

    FileSource source(ctx.in->stream(), false);

    AutoPumpFilter *decryptor = new AutoPumpFilter(decryption, source);

    source.Attach(decryptor);

    int magic = decryptVal<int>(*decryptor);

    if (magic != DEBUG_MAGIC)
    {
        cout << "UH OH! bad magic" << endl;
        return NULL;
    }

    GameBase *result = decryptFn(ctx, *decryptor, parent);

    if (result)
    {
        memcpy(result->key_, key, KEY_SIZE);
        result->gid_ = gid;
    }

    return result;
}

GameLocation::~GameLocation()
{ 
    if (startKey_) delete[] startKey_; 
}

void GameLocation::doAction(const byte *key)
{
    GameAction *action = this->getAction(key, true);
    if (action)
    {
        doAction(action);
        delete action;
    }
}

void GameLocation::doAction(GameAction *action, const std::vector<std::string> &args)
{
    string argStr;
    concatenateTokens(args, argStr);

    ctx_->lastArgs = argStr;

    action->doAction(args);
}

void GameLocation::doCommand(const std::string &cmd)
{
    ctx_->lastCommand = cmd;

    vector<string> tokens;
    tokenize(cmd, tokens);

    byte actionKey[KEY_SIZE];

    vector<string> reverseArgs;

    bool isExactCommand = true;
    while (true)
    {
        string partialCmd;
        concatenateTokens(tokens, partialCmd);

        commandKey(partialCmd, actionKey);

        GameAction *action = this->getAction(actionKey, isExactCommand);
        if (action != NULL)
        {
            vector <string> args;
            while (reverseArgs.size())
            {
                args.push_back(reverseArgs.back());
                reverseArgs.pop_back();
            }

            doAction(action, args);
            delete action;
            return;
        }

        if (!tokens.size())
            break;

        reverseArgs.push_back(tokens.back());
        tokens.pop_back();
        isExactCommand = false;        
    }

}

void GameLocation::getAncestors(vector<GameLocation *> &ancestors)
{
    GameLocation *cur = this;
    while (cur != NULL)
    {
        ancestors.push_back(cur);
        cur = cur->gameParent();
    }

}

GameBase *GameLocation::decrypt(GameContext &ctx, AutoPumpFilter &decryptor, GameLocation *parent)
{
    GameLocation *loc = new GameLocation(ctx, parent);

    char hasStart = decryptVal<char>(decryptor);

    if (hasStart != 0 && hasStart != 1)
    {
        throw "OMG WTF BBQ bad hasStart value";
    }

    if (hasStart)
    {
        // save somewhere
        loc->startKey_ = new byte[KEY_SIZE];
        decryptor.Get(loc->startKey_, KEY_SIZE);
    }

    loc->title = decryptString(decryptor);
    loc->desc = decryptString(decryptor);
    loc->prompt = decryptString(decryptor);
    size_t numIgnored = decryptVal<size_t>(decryptor);
    for (size_t i = 0; i < numIgnored; i++)
    {
        string ig = decryptString(decryptor);
        loc->ignoredSet_.insert(ig);
    }

    size_t numChildLocations = decryptVal<size_t>(decryptor);

    for (size_t i = 0; i < numChildLocations; i++)
    {
        byte keyhashBuf[KEYHASH_SIZE];
        decryptor.Get(keyhashBuf, KEYHASH_SIZE);
        
        int gid = decryptVal<int>(decryptor);

        loc->locationTable_[bytestring(keyhashBuf, KEYHASH_SIZE)] = gid;
    }

    size_t numItems = decryptVal<size_t>(decryptor);
    for (size_t i = 0; i < numItems; i++)
    {
        byte itemKey[KEY_SIZE];
        decryptor.Get(itemKey, KEY_SIZE);
        loc->itemKeys_.push_back(bytestring(itemKey, KEY_SIZE));
    }

    size_t numChildActions = decryptVal<size_t>(decryptor);

    for (size_t i = 0; i < numChildActions; i++)
    {
        byte cmdKeyHash[KEYHASH_SIZE];

        decryptor.Get(cmdKeyHash, KEYHASH_SIZE);

        byte encCommandBlock[3 * AES::BLOCKSIZE];
        decryptor.Get(encCommandBlock, 3 * AES::BLOCKSIZE);
        
        loc->actionTable_[bytestring(cmdKeyHash, KEYHASH_SIZE)].push_back(bytestring(encCommandBlock, 3 * AES::BLOCKSIZE));
    }

    return loc;
}

GameLocation *GameLocation::getChildByKey(const byte *key)
{
    byte keyHash[KEYHASH_SIZE];
    hashKey(key, keyHash);

    map<bytestring,int>::const_iterator it = locationTable_.find(bytestring(keyHash, KEYHASH_SIZE));

    if (it == locationTable_.end())
    {       
        throw "WTF bad location";
    }

    int startGid = it->second;

    return ctx_->getLocation(startGid, key, this);
}

std::string GameLocation::getPrompt()
{
    if (this->prompt.empty() && this->parent_)
    {
        return this->gameParent()->getPrompt();
    }
    else
    {
        return this->prompt;
    }
}

void GameLocation::dropItem(GameItem *item)
{
    itemKeys_.push_back(bytestring(item->key(), KEY_SIZE));
}

void GameLocation::enter()
{
    if (startKey_)
    {
        GameLocation *startLoc = getChildByKey(startKey_);
        if (startLoc)
        {
            ctx_->pushLocation(startLoc);
            startLoc->enter();
        }
    }
    else
    {
        if (!hasEnteredBefore_)
        {
            doAction(ctx_->firstVisitKey());
            hasEnteredBefore_ = true;
        }
        else
        {
            doAction(ctx_->returnVisitKey());
        }

        doAction(ctx_->forcedKey());
    }
}

GameAction *GameLocation::getAction(const byte *actionKey, bool isExactCommand, bool searchParents)
{
    byte keyHash[KEYHASH_SIZE];
    hashKey(actionKey, keyHash);
    bytestring keyHashStr = bytestring(keyHash, KEYHASH_SIZE);

    GameLocation *cur = this;
    while (cur != NULL)
    {
        GameAction *act = cur->getActionInternal(actionKey, keyHashStr, isExactCommand);
        if (act != NULL)
        {
            return act;
        }
        if (!searchParents)
        {
            break;
        }

        cur = cur->gameParent();
    }
    return NULL;
}

GameAction *GameLocation::getActionInternal(const byte *actionKey, const bytestring &actionKeyHash, bool isExactCommand)
{
    map<bytestring, vector<bytestring> >::const_iterator it = actionTable_.find(actionKeyHash);

    if (it != actionTable_.end())
    {
        vector<bytestring> actionBlocks = it->second;

        size_t numActionBlocks = actionBlocks.size();

        for (size_t i = 0; i < numActionBlocks; i++)
        {
            //H(CK(normalized cmd))->iv,E(CK(normalized cmd), gid + R(cmd))

            const byte *encryptedBlock = actionBlocks[i].c_str(); // 3x AES::BLOCKSIZE
            
            const byte *iv = encryptedBlock; // 1st block is IV
            const byte *encGid = encryptedBlock + AES::BLOCKSIZE;
            const byte *encKey = encGid + AES::BLOCKSIZE;
            
            CFB_Mode<AES>::Decryption decryption(actionKey, KEY_SIZE, iv);

            byte gidBlock[AES::BLOCKSIZE];
            byte actionKey[AES::BLOCKSIZE];
            decryption.ProcessData(gidBlock, encGid, AES::BLOCKSIZE);
            decryption.ProcessData(actionKey, encKey, AES::BLOCKSIZE);

            bool needsExactCommand = *gidBlock ? true : false; // byte 0 has exact bit

            if (needsExactCommand && !isExactCommand)
                continue;

            int gid = *((int *)gidBlock + 1); // bytes 4-7 are gid
            canonicalizeEndianness(gid);

            GameAction *action = GameAction::read(*ctx_, gid, actionKey, this);
            if (action)
            {
                return action;
            }
        }
    }
    return NULL;
}

void GameAction::doAction(const std::vector<std::string> &args)
{
    ctx_->printExpansion(this->desc, true);

    for (size_t i = 0; i < takesKeys_.size(); i++)
    {
        TakenItem &takenItem = takesKeys_[i];

        int gid = ctx_->in->getGid(ObjectTypeItem, takenItem.key);

        if (!ctx_->retakeItem(gid))
        {
            // the item may be in a different location, so we go to that location, take the item, 
            // and then return to where we were before.
            ctx_->doCall();

            followPath(takenItem.levelsUp, takenItem.locationKeysDown);
            
            GameLocation *itemLoc = ctx_->curLocation();

            GameItem *item = ctx_->getItem(gid, takenItem.key);

            itemLoc->takeItem(item, takenItem.takey);
            
            ctx_->doReturn();
        }
    }

    for (size_t i = 0; i < dropsTakeyHashes_.size(); i++)
    {
        ctx_->dropInventoryItem(dropsTakeyHashes_[i].c_str());
    }

    vector<GameFile *> filesToLaunch;
    for (size_t i = 0; i < files_.size(); i++)
    {
        const byte *fileKey = files_[i].key;
        int fileGid = ctx_->in->getGid(ObjectTypeFile, fileKey);
        GameFile *file = GameFile::read(*ctx_, fileGid, fileKey);
        if (files_[i].launch)
        {
            filesToLaunch.push_back(file);
        }
        else
        {
            delete file;
        }
    }
    for (size_t i = 0; i < filesToLaunch.size(); i++)
    {
        filesToLaunch[i]->launch();
        delete filesToLaunch[i];
    }

    if (this->changesLocation_)
    {
        followPath(levelsUp_, locationKeysDown_);
        ctx_->curLocation()->enter();
    }
}

void GameAction::followPath(size_t levelsUp, const vector<KeyBuffer> &locationKeysDown)
{
    ctx_->popLocationUntil(this->gameParent());

    for (size_t i = 0; i < levelsUp; i++)
    {
        ctx_->popLocation();
    }

    for (size_t i = 0; i < locationKeysDown.size(); i++)
    {
        const byte *locationKey = locationKeysDown[i].buf;

        GameLocation *cur = ctx_->curLocation();
        GameLocation *loc;

        if (cur)
        {
            loc = cur->getChildByKey(locationKey);
        }
        else
        {
            loc = ctx_->getTopLevelLocation(locationKey);
        }

        assert(loc);
        ctx_->pushLocation(loc);
    }
}

byte *GameAction::dokey()
{
    return dokey_;
}

#define NEW_ACTION_FOR_TYPE(type) case type: return new GameSpecializedAction<type>(ctx, parent);
GameAction *GameAction::newActionByType(int actionType, GameContext &ctx, GameLocation *parent)
{
    switch (actionType)
    {
        NEW_ACTION_FOR_TYPE(ActionTypeInventory);
        NEW_ACTION_FOR_TYPE(ActionTypeLook);
        NEW_ACTION_FOR_TYPE(ActionTypeQuit);
        NEW_ACTION_FOR_TYPE(ActionTypeCall);
        NEW_ACTION_FOR_TYPE(ActionTypeReturn);
        NEW_ACTION_FOR_TYPE(ActionTypeTake);
        NEW_ACTION_FOR_TYPE(ActionTypeDrop);
        NEW_ACTION_FOR_TYPE(ActionTypeSave);
        NEW_ACTION_FOR_TYPE(ActionTypeLoad);
        default:
        NEW_ACTION_FOR_TYPE(ActionTypeDefault);
    }
}

bool GameAction::getDokeyFromPredicate(GameContext &ctx, AutoPumpFilter &decryptor, byte *dokey)
{
    size_t predicateSize = decryptVal<size_t>(decryptor);
    if (predicateSize == 0)
    {
        decryptor.Get(dokey, KEY_SIZE);
        return true;
    }

    bool hasDokey = false;
    for (size_t i = 0; i < predicateSize; i++)
    {
        byte keyhashBuf[KEYHASH_SIZE];
        byte encDokey[KEY_SIZE];   
        byte iv[KEY_SIZE];

        vector<GameItem *> items;

        size_t conjunctionSize = decryptVal<size_t>(decryptor);

        bool hasAllItems = true;
        for (size_t j = 0; j < conjunctionSize; j++)
        {
            decryptor.Get(keyhashBuf, KEYHASH_SIZE);

            if (hasAllItems)
            {
                GameItem *item = ctx.getInventoryItem(keyhashBuf);
                if (item != NULL)
                {
                    items.push_back(item);
                }
                else
                {
                    hasAllItems = false;
                }
            }
        }

        decryptor.Get(iv, AES::BLOCKSIZE);
        decryptor.Get(encDokey, KEY_SIZE);

        if (hasAllItems && !hasDokey)
        {
            byte conjunctionKey[KEY_SIZE];
            makeConjunctionKey(items, conjunctionKey);
            
            CFB_Mode<AES>::Decryption dokeyDecryption(conjunctionKey, KEY_SIZE, iv);

            dokeyDecryption.ProcessData(dokey, encDokey, KEY_SIZE);
            hasDokey = true;
        }
    }

    return hasDokey;
}

GameBase *GameAction::decrypt(GameContext &ctx, AutoPumpFilter &decryptor, GameLocation *parent)
{
    byte dokey[KEY_SIZE];

    if (!getDokeyFromPredicate(ctx, decryptor, dokey))
    {
        return NULL;
    }

    byte iv[AES::BLOCKSIZE];
    decryptor.Get(iv, AES::BLOCKSIZE);
   
    CFB_Mode<AES>::Decryption innerDecryption(dokey, KEY_SIZE, iv);

    StreamTransformationFilter *innerDecryptorPtr = new AutoPumpFilter(innerDecryption, decryptor.GetSource(), 2 * AES::BLOCKSIZE);

    decryptor.Attach(innerDecryptorPtr);

    StreamTransformationFilter &innerDecryptor = *innerDecryptorPtr;

    int actionType = decryptVal<int>(innerDecryptor);

    GameAction *act = newActionByType(actionType, ctx, parent);

    act->desc = decryptString(innerDecryptor);

    size_t numFiles = decryptVal<size_t>(innerDecryptor);
    for (size_t i = 0; i < numFiles; i++)
    {
        act->files_.push_back(OpenedFile());
        OpenedFile &file = act->files_.back();
        innerDecryptor.Get(file.key, KEY_SIZE);
        file.launch = decryptVal<byte>(innerDecryptor) ? 1 : 0;
    }

    size_t numTakes = decryptVal<size_t>(innerDecryptor);

    for (size_t i = 0; i < numTakes; i++)
    {
        act->takesKeys_.push_back(TakenItem());

        TakenItem &takenItem = act->takesKeys_.back();
                
        innerDecryptor.Get(takenItem.key, KEY_SIZE);
        innerDecryptor.Get(takenItem.takey, KEY_SIZE);

        takenItem.levelsUp = decryptVal<size_t>(innerDecryptor);
        
        size_t numLocationsDown = decryptVal<size_t>(innerDecryptor);

        for (size_t i = 0; i < numLocationsDown; i++)
        {
            takenItem.locationKeysDown.push_back(KeyBuffer());
            KeyBuffer &keybuf = takenItem.locationKeysDown.back();
            innerDecryptor.Get(keybuf.buf, KEY_SIZE);
        }
    }

    size_t numDrops = decryptVal<size_t>(innerDecryptor);
    for (size_t i = 0; i < numDrops; i++)
    {
        byte itemTakeyHash[KEYHASH_SIZE];
        innerDecryptor.Get(itemTakeyHash, KEYHASH_SIZE);
        act->dropsTakeyHashes_.push_back(bytestring(itemTakeyHash, KEYHASH_SIZE));
    }
    
    act->changesLocation_ = decryptVal<byte>(innerDecryptor) ? true : false;
    
    if (act->changesLocation_)
    {
        act->levelsUp_ = decryptVal<size_t>(innerDecryptor);       

        size_t numLocationsDown = decryptVal<size_t>(innerDecryptor);

        for (size_t i = 0; i < numLocationsDown; i++)
        {
            act->locationKeysDown_.push_back(KeyBuffer());
            innerDecryptor.Get(act->locationKeysDown_.back().buf, KEY_SIZE);            
        }
    }
    else
    {
        act->levelsUp_ = 0;
    }
    
    size_t auxDataSize = decryptVal<size_t>(innerDecryptor);
    for (size_t i = 0; i < auxDataSize; i++)
    {
        string key = decryptString(innerDecryptor);
        act->auxData[key] = decryptString(innerDecryptor);
    }

    memcpy(act->dokey_, dokey, KEY_SIZE);

    return act;
}

template <ActionType actionType>
void GameSpecializedAction<actionType>::doAction(const std::vector<std::string> &args)
{
    this->GameAction::doAction(args);
}

void GameAction::iterateItems(const vector<GameItem *> &items)
{
    size_t numItems = items.size();
    if (numItems == 0)
    {
        ctx_->printExpansion(auxData["empty"], true);
    }
    else
    {
        ctx_->printExpansion(auxData["nonempty"], true);
    }

    for (size_t i = 0; i < numItems; i++)
    {
        ctx_->printExpansion(auxData["prefix"]);
        ctx_->printExpansion(items[i]->title);
        ctx_->printExpansion(auxData["suffix"]);
        cout << endl;
    }
}

template <>
void GameSpecializedAction<ActionTypeLook>::doAction(const std::vector<std::string> &args)
{
    this->GameAction::doAction(args);

    GameLocation *loc = ctx_->curLocation();

    vector<GameItem *> visibleItems;
    for (size_t i = 0; i < loc->numItems(); i++)
    {
        GameItem *item = loc->getItem(i);
        if (item->visible && !item->title.empty())
        {
            visibleItems.push_back(item);
        }
    }

    iterateItems(visibleItems);
}

template <>
void GameSpecializedAction<ActionTypeInventory>::doAction(const std::vector<std::string> &args)
{
    this->GameAction::doAction(args);

    vector<GameItem *> visibleItems;
    for (size_t i = 0; i < ctx_->inventorySize(); i++)
    {
        GameItem *item = ctx_->getInventoryItem(i);

        if (!item->title.empty())
        {
            visibleItems.push_back(item);
        }
    }
    iterateItems(visibleItems);
}

template <>
void GameSpecializedAction<ActionTypeQuit>::doAction(const std::vector<std::string> &args)
{
    this->GameAction::doAction(args);
    throw GameQuitException();
}

template <>
void GameSpecializedAction<ActionTypeCall>::doAction(const vector<string> &args)
{
    ctx_->doCall(); // call must go before action because we need to duplicate the previous location stack
    this->GameAction::doAction(args);
}

template <>
void GameSpecializedAction<ActionTypeReturn>::doAction(const vector<string> &args)
{
    this->GameAction::doAction(args);
    ctx_->doReturn();    
    ctx_->curLocation()->enter();
}

#define SAVE_FILE_EXTENSION ".crs"

bool isOkSaveName(const std::string &saveFile)
{
    for (size_t i = 0; i < saveFile.size(); i++)
    {
        if (!isOkFilenameChar(saveFile[i]))
        {
            return false;
        }
    }
    return true;
}

bool GameAction::getSaveFilename(const vector<string> &args, string &filename)
{
    filename = auxData["savename"];

    if (filename.empty())
    {
        if (!args.empty())
        {
            filename = args[0];
        }
        else
        {
            ctx_->printExpansion(auxData["prompt"]);
            getline(cin, filename);
        }
    }

    if (filename.empty())
    {
        ctx_->printExpansion(auxData["empty"], true);
        return false;
    }
    else if (!isOkSaveName(filename))
    {
        ctx_->printExpansion(auxData["invalid"], true);
        return false;
    }
    else
    {
        filename += SAVE_FILE_EXTENSION;
        return true;
    }
}

template <>
void GameSpecializedAction<ActionTypeLoad>::doAction(const vector<string> &args)
{
    string filename;
    if (getSaveFilename(args, filename))
    {
        ifstream in(filename.c_str(), ios::binary | ios::in);
        
        if (ctx_->load(in))
        {
            ctx_->printExpansion(auxData["success"], true);
        }
        else
        {
            ctx_->printExpansion(auxData["failure"], true);
        }
        in.close();

    }
    this->GameAction::doAction(args);
}

template <>
void GameSpecializedAction<ActionTypeSave>::doAction(const vector<string> &args)
{
    string filename;
    if (getSaveFilename(args, filename))
    {
        ofstream out(filename.c_str(), ios::binary | ios::out | ios::trunc);

        if (ctx_->save(out))
        {
            ctx_->printExpansion(auxData["success"], true);
        }
        else
        {
            ctx_->printExpansion(auxData["failure"], true);
        }

        out.close();
    }

    this->GameAction::doAction(args);
}

template <>
void GameSpecializedAction<ActionTypeTake>::doAction(const vector<string> &args)
{
    string itemTitle;
    concatenateTokens(args, itemTitle);

    GameLocation *curLocation = ctx_->curLocation();
    size_t numItems = curLocation->numItems();

    bool tookSomething = false;
    for (size_t i = 0; i < numItems; i++)
    {
        GameItem *item = curLocation->getItem(i);
        byte *takey = item->takey();
        if (takey && item->hasTitle(itemTitle))
        {
            curLocation->takeItem(item, takey);
            ctx_->printExpansion(auxData["success"], true);
            tookSomething = true;
            break;
        }
    }
    if (!tookSomething)
    {
        ctx_->printExpansion(auxData["failure"], true);
    }
    
    this->GameAction::doAction(args);
}

template<>
void GameSpecializedAction<ActionTypeDrop>::doAction(const vector<string> &args)
{
    string itemTitle;
    concatenateTokens(args, itemTitle);

    size_t inventorySize = ctx_->inventorySize();

    bool droppedSomething = false;
    for (size_t i = 0; i < inventorySize; i++)
    {
        GameItem *item = ctx_->getInventoryItem(i);
        if (item->hasTitle(itemTitle))
        {
            ctx_->dropInventoryItem(item->takeyhash());
            ctx_->printExpansion(auxData["success"], true);
            droppedSomething = true;
            break;
        }
    }
    if (!droppedSomething)
    {
        ctx_->printExpansion(auxData["failure"], true);
    }

    this->GameAction::doAction(args);
}

GameItem *GameLocation::getItem(size_t i)
{
    const byte *itemKey = itemKeys_[i].c_str();

    int gid = ctx_->in->getGid(ObjectTypeItem, itemKey);

    if (gid == -1)
    {
        throw "WTF";
    }

    return ctx_->getItem(gid, itemKey);
}

void GameLocation::takeItem(GameItem *item, const byte *takey)
{
    bytestring itemKeyStr = bytestring(item->key(), KEY_SIZE);

    for (size_t i = 0; i < itemKeys_.size(); i++)
    {
        if (itemKeys_[i] == itemKeyStr)
        {
            item->setTakey(takey);
            itemKeys_.erase(itemKeys_.begin() + i);
            ctx_->addToInventory(item);
            return;
        }   
    }
}

bool GameItem::hasTitle(const std::string &title)
{
    byte titleKey[KEY_SIZE];
    byte titleKeyHash[KEYHASH_SIZE];
    
    commandKey(title, titleKey);
    hashKey(titleKey, titleKeyHash);

    for (size_t i = 0; i < this->titles.size(); i++)
    {
        if (!memcmp(this->titles[i].buf, titleKeyHash, KEYHASH_SIZE))
        {
            return true;
        }
    }
    return false;
}

GameBase* GameItem::decrypt(GameContext &ctx, AutoPumpFilter &decryptor, GameLocation *parent)
{
    GameItem *item = new GameItem(ctx, parent);
    
    item->visible = decryptVal<byte>(decryptor) ? true : false;
    item->title = decryptString(decryptor);
    size_t numTitles = decryptVal<size_t>(decryptor);
    for (size_t i = 0; i < numTitles; i++)
    {
        item->titles.push_back(KeyHashBuffer());

        decryptor.Get(item->titles.back().buf, KEYHASH_SIZE);
    }
    item->desc = decryptString(decryptor);
    item->restrictTake = decryptVal<byte>(decryptor) ? true : false;
    if (!item->restrictTake)
    {
        item->takey_ = new byte[KEY_SIZE];
        decryptor.Get(item->takey_, KEY_SIZE);
    }
    return item;
}

void GameFile::launch()
{
#ifdef WIN32
	const string &cmd = dest;
#else
	string cmd = "open " + dest;
#endif
	
	if (system(("\"" + dest + "\"").c_str()) == -1)
	{
		cerr << "Your computer could not open " << dest << " automatically." << endl;
	}
}

#define FILEBUF_SIZE 2048
#define MIN(a,b) (((a)<(b))?(a):(b))

#define OUTPUT_DIR "extracted"

GameBase* GameFile::decrypt(GameContext &ctx, AutoPumpFilter &decryptor, GameLocation *parent)
{
    GameFile *file = new GameFile(ctx);

    file->dest = decryptString(decryptor);

    cleanFilename(file->dest, true);

    file->dest = OUTPUT_DIR"/" + file->dest;
    
    struct stat outputDirInfo;
    if (stat(OUTPUT_DIR, &outputDirInfo) != 0)
    {
        system("mkdir "OUTPUT_DIR);
    }
    
    struct stat fileInfo; 
    if (stat(file->dest.c_str(), &fileInfo) == 0) 
    { 
        cout << "file exists" << endl;
    }
    else
    {
        size_t len = decryptVal<size_t>(decryptor);
        fstream out(file->dest.c_str(), ios::out|ios::binary);

        if (out.fail())
        {   
            cerr << "ERROR: could not open " << file->dest << " for writing" << endl;
        }
        else
        {
            byte buf[FILEBUF_SIZE];
            
            // there's surely a way to do this with crypto++ filters...
            size_t left = len;
            while (true)
            {
                size_t len = decryptor.Get(buf, MIN(left,FILEBUF_SIZE));
                if (len == 0)
                {
                    break;
                }
                out.write((char *)buf,len);
                left -= len;
            }
            out.close();
        }
    }
    return file;
}
