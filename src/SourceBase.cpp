#include "SourceBase.h"
#include "SourceOutput.h"

using namespace std;
using namespace CryptoPP;

#include "cryptlib.h"
#include "filters.h"
#include "files.h"
#include "modes.h"
#include "aes.h"
#include <fstream>

/* xml parsing */
static const string validNestings[][2] = {
    /* {child, parent} */
    { "s" , "synonyms" },
    { "synonyms" , "location" },
    { "ignored" , "location" },
    { "action" , "location" },
    { "item" , "location" },
    { "location" , "location" },
    { "location" , "adventure" },
    { "cleartext" , "adventure" },
    { "file" , "adventure" },
    { "takes" , "action" },
    { "needs" , "action" },
    { "drops" , "action" },
    { "extracts" , "action" },
    { "aux" , "action" },
    { "alt", "action" },
    { "or" , "action" },
    { "needs" , "or" },
    { "alt", "item" },
};

struct ActionTypePair
{
    string str;
    ActionType val;
};

static const ActionTypePair validActionTypes[] = {
    { "", ActionTypeDefault },
    { "default", ActionTypeDefault },
    { "look", ActionTypeLook },
    { "take", ActionTypeTake },
    { "drop", ActionTypeDrop },
    { "call", ActionTypeCall },
    { "return", ActionTypeReturn },
    { "inventory", ActionTypeInventory },
    { "quit", ActionTypeQuit },
    { "save", ActionTypeSave },
    { "load", ActionTypeLoad },
};

SourceContext::~SourceContext()
{
    for (size_t i = 0; i < topLevelObjects_.size(); i++)
    {
        delete topLevelObjects_[i];
    }
}

bool SourceContext::getBoolAttribute(const std::map<std::string, std::string> &attributes, const std::string &key, bool required, bool def)
{
    string str = getAttribute(attributes, key, required, "");
    if (str.empty())
    {
        return def;
    }
    else
    {
        transformLower(str);
        if (str == "false" || str == "0" || str == "no") 
            return false;
        else if (str == "true" || str == "1" || str == "yes") 
            return true;
        else 
            throw InvalidSourceException(string() + "'" + str + "' is not a valid boolean value.");
    }
}

const std::string &SourceContext::getAttribute(const std::map<std::string, std::string> &attributes, const std::string &key, bool required, const std::string &def)
{
    map<string, string>::const_iterator it = attributes.find(key);
    if (it != attributes.end())
    {
        return it->second;
    }
    else
    {
        if (required)
        {
            throw InvalidSourceException(string() + "Missing required attribute '" + key + "' for element '" + elemStack_.back() + "'");
        }

        return def;
    }
}

void SourceContext::startElement(const string &_name, const map<string, string> &_attributes)
{
    string name = _name;
    transformLower(name);
    
    map<string,string> attributes;
    for (map<string,string>::const_iterator it = _attributes.begin(); it != _attributes.end(); ++it)
    {
        string key = it->first;
        transformLower(key);
        attributes[key] = it->second;
    }

    string parentElem;

    if (elemStack_.empty())
    {
        if (name != "adventure")
        {
            throw InvalidSourceException("The root element was not named 'adventure'");
        }
    }
    else
    {
        parentElem = elemStack_.back();
        bool isValidNesting = false;
        for (size_t i = 0; i < sizeof(validNestings)/sizeof(validNestings[0]); i++)
        {
            if (validNestings[i][0] == name && validNestings[i][1] == parentElem)
            {
                isValidNesting = true;
                break;
            }
        }
        if (!isValidNesting)
        {
            throw InvalidSourceException(string() + "The '" + name + "' tag cannot appear under a '"+ parentElem +"' tag.");
        }
    }

    elemStack_.push_back(name);

    SourceBase *topObject = objectStack_.empty() ? NULL : objectStack_.back();

    SourceLocation *parentLoc = topObject ? dynamic_cast<SourceLocation *>(topObject) : NULL;
    SourceItem *parentItem = topObject ? dynamic_cast<SourceItem *>(topObject) : NULL;
    SourceAction *parentAction = topObject ? dynamic_cast<SourceAction *>(topObject) : NULL;

    if (name == "location")
    {
        SourceLocation *newLoc;
        if (parentLoc)
        {
            newLoc = parentLoc->newChildLocation();            
        }
        else
        {
            newLoc = new SourceLocation(*this);
            topLevelObjects_.push_back(newLoc);
        }

        newLoc->id = getAttribute(attributes, "id");
        newLoc->prompt = getAttribute(attributes, "prompt", false);
        newLoc->startId = getAttribute(attributes, "start", false);
        newLoc->title = getAttribute(attributes, "title", false);

        objectStack_.push_back(newLoc);
        allLocations_.push_back(newLoc);
    }
    else if (name == "synonyms")
    {
        assert(parentLoc);
        parentLoc->synonyms.push_back(set<string>());
    }
    else if (name == "s")
    {
        // nothing to do here, wait for text
    }
    else if (name == "adventure")
    {
        gameName = getAttribute(attributes, "name");
        startId_ = getAttribute(attributes, "start", false);
    }
    else if (name == "ignored")
    {
        const std::string &token = getAttribute(attributes, "token");
        assert(parentLoc);
        parentLoc->addIgnored(token);
    }
    else if (name == "action")
    {
        assert(parentLoc);
        SourceAction *action = parentLoc->newChildAction();       

        bool allowBlankCommand = true;
        if (getBoolAttribute(attributes, "forced", false, false))
        {
            action->commands.push_back(FORCED_COMMAND);
            allowBlankCommand = false;
        }

        if (getBoolAttribute(attributes, "firstvisit", false, false))
        {
            action->commands.push_back(FIRST_VISIT_COMMAND);
            allowBlankCommand = false;
        }

        if (getBoolAttribute(attributes, "returnvisit", false, false))
        {
            action->commands.push_back(RETURN_VISIT_COMMAND);
            allowBlankCommand = false;
        }

        const string &cmd = getAttribute(attributes, "cmd", false);

        if (allowBlankCommand || !cmd.empty())
        {
            action->commands.push_back(cmd);
        }

        action->exact = getBoolAttribute(attributes, "exact", false, false);
        action->destId = getAttribute(attributes, "dest", false);

        string actionTypeStr = getAttribute(attributes, "type", false);
        transformLower(actionTypeStr);

        bool validActionType = false;
        for (size_t i = 0; i < sizeof(validActionTypes)/sizeof(validActionTypes[0]); i++)
        {
            if (validActionTypes[i].str == actionTypeStr)
            {
                action->actionType = validActionTypes[i].val;
                validActionType = true;
            }
        }
        if (!validActionType)
        {
            throw InvalidSourceException(string() + "'" + actionTypeStr + "' is not a valid action type.");            
        }

        // make a conjunction for all the "needs" tags that aren't inside an "or" tag
        action->predicateIds.push_back(UnresolvedConjunction());

        objectStack_.push_back(action);
    }
    else if (name == "or")
    {
        assert(parentAction);

        parentAction->predicateIds.push_back(UnresolvedConjunction());
    }
    else if (name == "aux")
    {
        assert(parentAction);

        curAuxKey_ = getAttribute(attributes, "key");
    }
    else if (name == "needs")
    {
        assert(parentAction);

        UnresolvedConjunction &conj = (parentElem == "action") 
            ? parentAction->predicateIds[0] : parentAction->predicateIds.back();

        conj.push_back(getAttribute(attributes, "item"));
    }
    else if (name == "takes")
    {
        assert(parentAction);

        parentAction->takesIds.push_back(getAttribute(attributes, "item"));
    }
    else if (name == "drops")
    {
        assert(parentAction);

        parentAction->dropsIds.push_back(getAttribute(attributes, "item"));
    }
    else if (name == "item")
    {
        assert(parentLoc);
    
        SourceItem *item = parentLoc->newChildItem();       

        item->id = getAttribute(attributes, "id");

        item->title = getAttribute(attributes, "title", false);

        if (!item->title.empty())
        {
            item->titles.push_back(item->title);
        }
        item->restrictTake = getBoolAttribute(attributes, "restricttake", false, true);
        item->visible = getBoolAttribute(attributes, "visible", false, true);

        objectStack_.push_back(item);
        allItems_.push_back(item);
    }
    else if (name == "alt")
    {
        if (parentElem == "item")
        {
            assert(parentItem);
            parentItem->titles.push_back(getAttribute(attributes, "title"));
        }
        else if (parentElem == "action")
        {
            assert(parentAction);
            parentAction->commands.push_back(getAttribute(attributes, "cmd"));
        }
    }
    else if (name == "file")
    {
        SourceFile *file = new SourceFile(*this);

        file->id = getAttribute(attributes, "id");
        file->src = getAttribute(attributes, "src");
        file->dest = getAttribute(attributes, "dest", false);

        topLevelObjects_.push_back(file);
        objectStack_.push_back(file);
        allFiles_.push_back(file);
    }   
    else if (name == "extracts")
    {
        assert(parentAction);

        const string &fileId = getAttribute(attributes, "file");
        bool launch = getBoolAttribute(attributes, "launch", false, true);
        
        parentAction->fileIds.push_back(pair<string, bool>(fileId, launch));
    }
}

template <typename SourceType>
SourceType *resolveId(const map<string, SourceType *> &map, const string &typeDesc, const string &id)
{
    if (id.empty())
    {  
        return NULL;
    }

    // typename map<string, SourceType *>::const_iterator it = map.find(id); // why doesn't this compile?
    if (map.find(id) == map.end())
    {
        throw InvalidSourceException(string() + "Could not resolve reference to "+typeDesc+" '"+id+"'");
    }
    return map.find(id)->second;
}

SourceLocation *SourceContext::getLocationById(const std::string &id)
{
    return resolveId(locationsMap_, "location", id);
}

SourceItem *SourceContext::getItemById(const std::string &id)
{
    return resolveId(itemsMap_, "item", id);
}

SourceFile *SourceContext::getFileById(const std::string &id)
{
    return resolveId(filesMap_, "file", id);
}

void SourceContext::endElement()
{
    string &elemName = elemStack_.back();

    if (elemName == "location" || elemName == "action" || elemName == "item" || elemName == "file")
    {
        objectStack_.pop_back();
    }

    elemStack_.pop_back();
}

void trimTabs(string &str)
{
    // split lines.

    vector<string> lines;

    size_t start = 0;
    while (true)
    {
        size_t newLine = str.find_first_of("\n", start);

        if (newLine == string::npos)
        {
            lines.push_back(str.substr(start));
            break;
        }

        lines.push_back(str.substr(start, newLine - start + 1));        
        start = newLine + 1;
    }

    // count # of whitespace characters at the start of each line that contains non-whitespace. 
    // find the minimum of these counts.

    bool dedent = false;
    size_t minSpaces = 999999;
    for (size_t i = 0; i < lines.size(); i++)
    {
        size_t numSpaces = lines[i].find_first_not_of("\t\r\n ");
        if (numSpaces != string::npos)
        {
            if (numSpaces < minSpaces)
            {
                dedent = true;
                minSpaces = numSpaces;
            }
        }
    }

    // trim that number of characters off the beginning of each line, and any whitespace from the end.
    str = "";
    for (size_t i = 0; i < lines.size(); i++)
    {
        size_t lastNonWhitespace = lines[i].find_last_not_of(" \r\n\t");

        if (lastNonWhitespace != string::npos && lastNonWhitespace >= minSpaces)
        {
            str.append(lines[i].substr(minSpaces, lastNonWhitespace - minSpaces + 1));
        }
        str.append("\n");
    }

    // strip newlines at beginning and end
    size_t lastNonNewLine = str.find_last_not_of('\n');
    if (lastNonNewLine != string::npos)
    {
        str.erase(lastNonNewLine + 1);
        
        size_t firstNonNewLine = str.find_first_not_of('\n');
        if (firstNonNewLine != string::npos)
        {
            str.erase(0, firstNonNewLine);
        }
    }
    else 
    {
        str = "";
    }
}

bool isWhitespace(const string &str)
{
    return str.find_first_not_of("\t\r\n ") == string::npos;
}

void SourceContext::handleText(const string &str)
{
    const string &text = str;
    
    if (elemStack_.empty())
    {        
        if (isWhitespace(text))
        {
            return;
        }
        else
        {
            throw InvalidSourceException(string() + "Unexpected text outside of 'adventure' tag: '"+text+"'");
        }
    }

    string &parentElem = elemStack_.back();

    SourceBase *topObject = objectStack_.empty() ? NULL : objectStack_.back();
    SourceLocation *parentLoc = topObject ? dynamic_cast<SourceLocation *>(topObject) : NULL;

    if (parentElem == "cleartext")
    {
        this->gameDesc_.append(text);
    }
    else if (parentElem == "s")
    {
        assert(parentLoc);
        
        parentLoc->synonyms.back().insert(text);
    }
    else if (parentElem == "action")
    {
        SourceAction *parentAction = topObject ? dynamic_cast<SourceAction *>(topObject) : NULL;

        assert(parentAction);
        parentAction->desc.append(text);
    }
    else if (parentElem == "location")
    {
        assert(parentLoc);

        parentLoc->desc.append(text);
    }
    else if (parentElem == "item")
    {
        SourceItem *parentItem = topObject ? dynamic_cast<SourceItem *>(topObject) : NULL;
        assert(parentItem);

        parentItem->desc.append(text);
    }
    else if (parentElem == "aux")
    {
        SourceAction *parentAction = topObject ? dynamic_cast<SourceAction *>(topObject) : NULL;
        assert(parentAction);

        parentAction->auxData[curAuxKey_].append(text);
    }
    else if (!isWhitespace(text))
    {
        throw InvalidSourceException(string() + "Unexpected text inside '"+parentElem+"' tag: '"+text+"'");
    }
}

template <typename SourceType>
void insertInIdMap(const vector<SourceType *> &objs, map<string, SourceType *> &map)
{
    map.clear();
    for (size_t i = 0; i < objs.size(); i++)
    {
        SourceType *obj = objs[i];
        const string &id = obj->id;

        if (map.find(id) != map.end())
        {
            throw InvalidSourceException(string() + "Duplicate id: " + id);
        }
        map[id] = obj;
    }
}

void SourceContext::write(SourceOutput &out)
{
    out.writePlaintext(this->gameDesc_);

    out.startEncryptedBlock();

    for (size_t i = 0; i < topLevelObjects_.size(); i++)
    {
        topLevelObjects_[i]->write(out);
    }

    out.setInitialLocation(start_);
    out.finalize();
}


bool isOkGameNameChar(char c)
{
    return isOkFilenameChar(c) && c != '.';
}

void SourceContext::resolve()
{
    if (allLocations_.empty())
    {
        throw InvalidSourceException("The adventure requires at least one location.");
    }    

    insertInIdMap(allLocations_, locationsMap_);
    insertInIdMap(allItems_, itemsMap_);
    insertInIdMap(allFiles_, filesMap_);

    for (size_t i = 0; i < gameName.size(); i++)
    {
        if (!isOkGameNameChar(gameName[i]))
            throw InvalidSourceException(string() + "Invalid character in adventure name: " + gameName[i]);
    }

    start_ = getLocationById(startId_);

    bool startIsTopLevel = false;

    for (size_t i = 0; i < topLevelObjects_.size(); i++)
    {
        SourceLocation *loc = dynamic_cast<SourceLocation *>(topLevelObjects_[i]);

        if (loc)
        {
            if (!start_)
            {
                start_ = loc;
            }

            if (loc == start_)
            {
                startIsTopLevel = true;
                break;
            }
        }
    }

    if (!startIsTopLevel)
    {
        throw InvalidSourceException(string() + "Start location " +startId_+ " is not a direct child of the <adventure> tag");
    }

    trimTabs(gameDesc_);
    for (size_t i = 0; i < topLevelObjects_.size(); i++)
    {
        topLevelObjects_[i]->resolve();
    }    
}

byte *SourceBase::key()
{
    if (!key_)
    {
        key_ = new byte[KEY_SIZE];
        generateKey(key_);
    }
    return key_;
}

void SourceBase::write(SourceOutput &out)
{
    if (out.isRecorded(this))
    {
        return;
    }
    out.recordObject(this);

    byte iv[AES::BLOCKSIZE];
   
    ctx_->makerand(iv, AES::BLOCKSIZE);

    //cout << "writing object with gid " << this->gid_ << " at offset " << out.stream().tellp() << endl;

    out.stream().write((char *)iv, AES::BLOCKSIZE);   

    //cout << "write iv: ";
    //debugBinary(iv, AES::BLOCKSIZE);

    //cout << "write key: ";
    //debugBinary(this->key(), KEY_SIZE);

    CFB_Mode<AES>::Encryption cfbEncryption(this->key(), KEY_SIZE, iv);

    StreamTransformationFilter encryptor(cfbEncryption, new FileSink(out.stream()));

    encryptVal<int>(encryptor, DEBUG_MAGIC);

    this->encrypt(encryptor);

    encryptor.MessageEnd();

    writeChildren(out);
}

template <typename T>
void SourceBase::encryptVal(StreamTransformationFilter &encryptor, T val)
{
    canonicalizeEndianness(val);
    encryptor.Put((byte *)&val, sizeof(T));
}

void SourceBase::encryptString(StreamTransformationFilter &encryptor, const string &str)
{
    size_t len = str.length();
    encryptVal<size_t>(encryptor, len);
    encryptor.Put((byte*)str.c_str(), len);
}

void SourceBase::writeChildren(SourceOutput &out)
{
}


void SourceBase::generateKey(byte *outputKey)
{    
    ctx_->makerand(outputKey, KEY_SIZE); 
    // we hope that this key and its hash are unique for this game (TODO: verify this)
}

SourceLocation::~SourceLocation()
{
    for (size_t i = 0; i < locations_.size(); i++)
    {
        delete locations_[i];
    }
    for (size_t i = 0; i < actions_.size(); i++)
    {
        delete actions_[i];
    }
    for (map<string, set<string> *>::iterator it = mergedSynonymsMap_.begin(); it != mergedSynonymsMap_.end(); ++it)
    {
        delete it->second;
    }
}

SourceLocation *SourceLocation::newChildLocation()
{
    SourceLocation *loc = new SourceLocation(*ctx_, this);
    locations_.push_back(loc);
    return loc;
}

SourceAction *SourceLocation::newChildAction()
{
    SourceAction *act = new SourceAction(*ctx_, this);
    actions_.push_back(act);
    return act;
}

SourceItem *SourceLocation::newChildItem()
{
    SourceItem *item = new SourceItem(*ctx_, this);
    items_.push_back(item);
    return item;
}

set<string> *SourceLocation::getLocalSynonyms(const string &token)
{
    size_t numSynonymGroups = synonyms.size();
    for (size_t i = 0; i < numSynonymGroups; i++)
    {
        set<string> &synonymGroup = synonyms[i];
        if (synonymGroup.find(token) != synonymGroup.end())
        {
            return &synonymGroup;
        }
    }
    return NULL;
}

set<string> *SourceLocation::getSynonyms(const string &token)
{
    map<string, set<string> *>::iterator it = mergedSynonymsMap_.find(token);
    if (it != mergedSynonymsMap_.end())
    {
        return it->second;
    }

    set<string> *localSynonyms = getLocalSynonyms(token);
    SourceLocation *parentLoc = this->sourceParent();

    set<string> *parentSynonyms = parentLoc ? parentLoc->getSynonyms(token) : NULL;
    if (parentSynonyms && localSynonyms)
    {
        set<string> *mergedSynonyms = new set<string>(*localSynonyms);
        for (set<string>::const_iterator pit = parentSynonyms->begin(); pit != parentSynonyms->end(); ++pit)
        {
            mergedSynonyms->insert(*pit);
        }

        mergedSynonymsMap_[token] = mergedSynonyms;

        return mergedSynonyms;
    }
    else if (parentSynonyms)
    {
        return parentSynonyms;
    }
    else
    {
        return localSynonyms;
    }
}

void SourceLocation::resolve()
{
    trimTabs(desc);
    for (size_t i = 0; i < synonyms.size(); i++)
    {
        set<string> &synonymsSet = synonyms[i];
        set<string> synonymsCopy = synonymsSet;
        synonymsSet.clear();
        for (set<string>::const_iterator it = synonymsCopy.begin(); it != synonymsCopy.end(); ++it)
        {
            string syn = *it;
            trimTabs(syn);
            transformLower(syn);
            synonymsSet.insert(syn);
        }
    }

    start = ctx_->getLocationById(startId);
    if (start)
    {
        bool startAtImmediateChild = false;
        for (size_t i = 0; i < locations_.size(); i++)
        {
            if (start == locations_[i])
            {
                startAtImmediateChild = true;
                break;
            }
        }
        if (!startAtImmediateChild)
        {
            throw InvalidSourceException(string() + " start id '" + startId + "' is not an immediate child of '" +id+ "'");
        }
    }

    for (size_t i = 0; i < items_.size(); i++)
    {
        items_[i]->resolve();
    }
    for (size_t i = 0; i < actions_.size(); i++)
    {
        actions_[i]->resolve();
    }
    for (size_t i = 0; i < locations_.size(); i++)
    {
        locations_[i]->resolve();
    }
}

void SourceLocation::addIgnored(const string &token)
{
    string lowerToken = token;
    // TODO: normalize spaces
    transformLower(lowerToken);
    ignoredSet_.insert(lowerToken);
}


void SourceLocation::encrypt(StreamTransformationFilter &encryptor)
{
    char hasStart = (this->start) ? 1 : 0;
    encryptor.Put(hasStart);

    if (hasStart)
    {
        byte *key = this->start->key();
        encryptor.Put(key, KEY_SIZE);
    }
    encryptString(encryptor, title);
    encryptString(encryptor, desc);
    encryptString(encryptor, prompt);
    
    size_t numIgnored = ignoredSet_.size();
    encryptVal<size_t>(encryptor, numIgnored);
    for (set<string>::const_iterator it = ignoredSet_.begin(); it != ignoredSet_.end(); ++it)
    {
        encryptString(encryptor, *it);
    }

    size_t numChildLocations = locations_.size();
    encryptVal<size_t>(encryptor, numChildLocations);
    for (size_t i = 0; i < numChildLocations; i++)
    {
        //cout << "location " << this->gid() << " has child with keyhash ";
        //debugBinary(locations_[i]->keyhash(), KEYHASH_SIZE);

        encryptor.Put(locations_[i]->keyhash(), KEYHASH_SIZE);
        encryptVal<int>(encryptor, locations_[i]->gid());
    }

    size_t numItems = items_.size();
    encryptVal<size_t>(encryptor, numItems);
    for (size_t i = 0; i < numItems; i++)
    {
        encryptor.Put(items_[i]->key(), KEY_SIZE);
    }

    vector<ExpandedAction> expandedActions;
    this->getExpandedActions(expandedActions);
    size_t numExpandedActions = expandedActions.size();
    encryptVal<size_t>(encryptor, numExpandedActions);

    for (size_t i = 0; i < numExpandedActions; i++)
    {
        ExpandedAction &expAction = expandedActions[i];
        byte cmdKey[KEY_SIZE];
        byte cmdKeyHash[KEYHASH_SIZE];
        commandKey(expAction.cmd, cmdKey);

        //cout << expAction.cmd << endl;

        hashKey(cmdKey, cmdKeyHash);
        encryptor.Put(cmdKeyHash, KEYHASH_SIZE);

        byte iv[AES::BLOCKSIZE];
        ctx_->makerand(iv, AES::BLOCKSIZE);
        
        encryptor.Put(iv, AES::BLOCKSIZE);

        CFB_Mode<AES>::Encryption cfbEncryption(cmdKey, KEY_SIZE, iv);

        byte gidBlock[AES::BLOCKSIZE];
        memset(gidBlock, 0, AES::BLOCKSIZE);
        int gid = expAction.action->gid();
        canonicalizeEndianness(gid);

        byte exact = expAction.action->exact ? 1 : 0;

        *gidBlock = exact;
        *((int *)gidBlock + 1) = gid;        
        
        byte encBuf[AES::BLOCKSIZE];
        cfbEncryption.ProcessData(encBuf, gidBlock, AES::BLOCKSIZE);
        encryptor.Put(encBuf, AES::BLOCKSIZE);

        cfbEncryption.ProcessData(encBuf, expAction.action->key(), AES::BLOCKSIZE);
        encryptor.Put(encBuf, AES::BLOCKSIZE);
    }
}

void SourceLocation::writeChildren(SourceOutput &out)
{
    for (size_t i = 0; i < locations_.size(); i++)
    {
        locations_[i]->write(out);
    }

    for (size_t i = 0; i < items_.size(); i++)
    {
        items_[i]->write(out);
    }
    
    for (size_t i = 0; i < actions_.size(); i++)
    {
        actions_[i]->write(out);
    }
}

void SourceLocation::getPath(SourceLocation *other, size_t &levelsUp, vector<SourceLocation *> &locationsDown)
{
    vector<SourceLocation *> thisAncestors, otherAncestors;
    this->getAncestors(thisAncestors);
    other->getAncestors(otherAncestors);

    while (thisAncestors.size() && otherAncestors.size())
    {
        SourceLocation *thisAncestor = thisAncestors.back();
        SourceLocation *otherAncestor = otherAncestors.back();

        if (thisAncestor != otherAncestor)
            break;

        thisAncestors.pop_back();
        otherAncestors.pop_back();
    }

    levelsUp = thisAncestors.size();

    while (otherAncestors.size())
    {
        locationsDown.push_back(otherAncestors.back());
        otherAncestors.pop_back();
    }
}

void SourceLocation::expandStringInternal(vector<string> &tokens, size_t i, vector<string> &commands)
{
    if (i >= tokens.size())
    {
        // concatenate tokens, joined by space, and append to commands array
        
        string cmd;
        concatenateTokens(tokens, cmd);
        commands.push_back(cmd);
    }
    else
    {
        string &token = tokens[i];

        set<string> *synonyms = getSynonyms(token);
        if (synonyms)
        {
            string origToken = tokens[i];
            
            for (set<string>::const_iterator it = synonyms->begin(); it != synonyms->end(); ++it)
            {
                tokens[i] = *it;
                expandStringInternal(tokens, i+1, commands);
            }
            tokens[i] = origToken;
        }
        else
        {
            expandStringInternal(tokens, i+1, commands);
        }
    }
}

void SourceLocation::expandString(const string &str, vector<string> &expanded)
{
    vector<string> tokens;
    tokenize(str, tokens);
    expandStringInternal(tokens, 0, expanded);
}


void SourceLocation::getExpandedActions(std::vector<ExpandedAction> &expandedActions)
{
    size_t numActions = actions_.size();
    for (size_t i = 0; i < numActions; i++)
    {
        SourceAction *action = actions_[i];

        vector<string> &actionCommands = action->commands;
        for (size_t k = 0; k < actionCommands.size(); k++)
        {
            vector<string> expandedCommands;            
            expandString(actionCommands[k], expandedCommands);

            for (size_t j = 0; j < expandedCommands.size(); j++)
            {
                expandedActions.push_back(ExpandedAction(action, expandedCommands[j]));
            }
        }
    }
}

void SourceLocation::getAncestors(vector<SourceLocation *> &ancestors)
{
    SourceLocation *cur = this;
    while (cur != NULL)
    {
        ancestors.push_back(cur);
        cur = cur->sourceParent();
    }
}

void SourceAction::resolveConjunction(const UnresolvedConjunction &ids, Conjunction &conj)
{
    conj.clear();
    for (size_t i = 0; i < ids.size(); i++)
    {
        conj.push_back(ctx_->getItemById(ids[i]));
    }
}

void SourceAction::resolve()
{
    trimTabs(desc);

    for (map<string,string>::iterator it = auxData.begin(); it != auxData.end(); ++it)
    {
        trimTabs(it->second);
    }

    dest = ctx_->getLocationById(destId);

    predicate.clear();
    for (size_t i = 0; i < predicateIds.size(); i++)
    {
        UnresolvedConjunction &conjIds = predicateIds[i];
        if (conjIds.size())
        {
            predicate.push_back(Conjunction());
            resolveConjunction(conjIds, predicate.back());
        }
    }

    files.clear();
    for (size_t i = 0; i < fileIds.size(); i++)
    {
        files.push_back(pair<SourceFile *, bool>(ctx_->getFileById(fileIds[i].first), fileIds[i].second));
    }

    resolveConjunction(takesIds, takes);
    resolveConjunction(dropsIds, drops);
}



void SourceAction::writeChildren(SourceOutput &out)
{
    /*
    if (file)
    {
        file->write(out);
    }
    */
}

byte *SourceAction::dokey() // the key that you get when you are allowed to take the object
{
    if (!dokey_)
    {
        dokey_ = new byte[KEY_SIZE];
        generateKey(dokey_);
    }
    return dokey_;
}

void SourceAction::encrypt(StreamTransformationFilter &encryptor)
{
    size_t predicateSize = predicate.size();
    encryptVal<size_t>(encryptor, predicateSize);

    // if there are no predicates in order to do this action, just give them the dokey directly
    if (predicateSize == 0)
    {
        encryptor.Put(dokey(), KEY_SIZE);
    }

    byte iv[AES::BLOCKSIZE];

    for (size_t i = 0; i < predicateSize; i++)
    {
        Conjunction &conjunction = predicate[i];
        size_t conjunctionSize = conjunction.size();
        encryptVal<size_t>(encryptor, conjunctionSize);
   
        // record the takey hash for each item used in this conjunction
        for (size_t j = 0; j < conjunctionSize; j++)
        {
            SourceItem *requiredItem = conjunction[j];
            encryptor.Put(requiredItem->takeyhash(), KEYHASH_SIZE);
        }

        byte conjunctionKey[KEY_SIZE];
        makeConjunctionKey(conjunction, conjunctionKey);

        // record the dokey for this action, encrypted by the conjunction key
        ctx_->makerand(iv, AES::BLOCKSIZE);
        
        encryptor.Put(iv, AES::BLOCKSIZE);

        if (KEY_SIZE != AES::BLOCKSIZE)
        {
            throw "key size not equal to block size... : (";
        }

        CFB_Mode<AES>::Encryption cfbEncryption(conjunctionKey, KEY_SIZE, iv);

        byte outputBlock[KEY_SIZE];

        cfbEncryption.ProcessData(outputBlock, dokey(), KEY_SIZE);

        encryptor.Put(outputBlock, KEY_SIZE);
    }

    // now finally, the payload for this action, encrypted by the dokey
    
    ctx_->makerand(iv, AES::BLOCKSIZE);
    encryptor.Put(iv, AES::BLOCKSIZE);

    CFB_Mode<AES>::Encryption innerEncryption(dokey(), KEY_SIZE, iv);

    StreamTransformationFilter innerEncryptor(innerEncryption);

    encryptVal<int>(innerEncryptor, this->actionType);
    encryptString(innerEncryptor, this->desc);
    
    size_t numFiles = files.size();
    encryptVal<size_t>(innerEncryptor, numFiles);
    for (size_t i = 0; i < numFiles; i++)
    {        
        pair<SourceFile *, bool> &filePair = files[i];
        innerEncryptor.Put(filePair.first->key(), KEY_SIZE);
        encryptVal<byte>(innerEncryptor, filePair.second ? 1 : 0); // should the game open this file after decrypting it?
    }

    size_t numTakes = this->takes.size();
    encryptVal<size_t>(innerEncryptor, numTakes);
    for (size_t i = 0; i < numTakes; i++)
    {
        SourceItem *takesItem = this->takes[i];
        innerEncryptor.Put(takesItem->key(), KEY_SIZE);
        innerEncryptor.Put(takesItem->takey(), KEY_SIZE);
        
        encryptPath(innerEncryptor, takesItem->sourceParent());        
    }

    size_t numDrops = this->drops.size();
    encryptVal<size_t>(innerEncryptor, numDrops);
    for (size_t i = 0; i < numDrops; i++)
    {
        innerEncryptor.Put(drops[i]->takeyhash(), KEYHASH_SIZE);
    }

    encryptVal<byte>(innerEncryptor, this->dest ? 1 : 0);
    
    if (this->dest)
    {
        encryptPath(innerEncryptor, this->dest);
    }

    encryptVal<size_t>(innerEncryptor, auxData.size());
    for (map<string,string>::const_iterator it = auxData.begin(); it != auxData.end(); ++it)
    {
        encryptString(innerEncryptor, it->first);
        encryptString(innerEncryptor, it->second);
    }

    innerEncryptor.MessageEnd();
    innerEncryptor.TransferAllTo(encryptor);
}

void SourceAction::encryptPath(StreamTransformationFilter &innerEncryptor, SourceLocation *otherLoc)
{    
    vector<SourceLocation *> locationsDown;
    size_t levelsUp; 
    sourceParent()->getPath(otherLoc, levelsUp, locationsDown);
    encryptVal<size_t>(innerEncryptor, levelsUp);

    size_t numLocationsDown = locationsDown.size();
    encryptVal<size_t>(innerEncryptor, numLocationsDown);
    for (size_t i = 0; i < numLocationsDown; i++)
    {
        innerEncryptor.Put(locationsDown[i]->key(), KEY_SIZE);
    }
}

byte *SourceItem::takey() // the key that you get when you are allowed to take the object
{
    if (!takey_)
    {
        takey_ = new byte[KEY_SIZE];
        generateKey(takey_);
    }
    return takey_;
}

void SourceItem::encrypt(StreamTransformationFilter &encryptor)
{
    encryptVal<byte>(encryptor, visible ? 1 : 0);
    encryptString(encryptor, title);
    size_t numTitles = titles.size();
    encryptVal<size_t>(encryptor, numTitles);
    for (size_t i = 0; i < numTitles; i++)
    {
        byte titleKey[KEY_SIZE];
        byte titleKeyHash[KEYHASH_SIZE];

        commandKey(titles[i], titleKey);
        hashKey(titleKey, titleKeyHash);

        encryptor.Put(titleKeyHash, KEYHASH_SIZE);
    }
    encryptString(encryptor, desc);
    encryptVal<byte>(encryptor, restrictTake ? 1 : 0);
    if (!restrictTake)
    {
        encryptor.Put(takey(), KEY_SIZE);
    }    
}

void SourceItem::resolve()
{
    trimTabs(desc);

    SourceLocation *parentLoc = this->sourceParent();
    vector<string> oldTitles = this->titles;
    this->titles.clear();
    for (size_t i = 0; i < oldTitles.size(); i++)
    {
        vector<string> expandedTitles;
        parentLoc->expandString(oldTitles[i], expandedTitles);    
        for (size_t j = 0; j < expandedTitles.size(); j++)
        {
            this->titles.push_back(expandedTitles[j]);
        }
    }
}

void SourceFile::resolve()
{
    if (dest.empty())
    {
        dest = baseFileName(src);
    }

    ifstream in(src.c_str());
    if (in.fail())
    {
        throw InvalidSourceException(string() + "could not open " + src);
    }
    in.close();
}

void SourceFile::encrypt(StreamTransformationFilter &encryptor)
{
    encryptString(encryptor, dest);

    fstream in(src.c_str(), ios::in|ios::binary);

    if (in.fail())
    {
        throw InvalidSourceException(string() + "could not open " + src);
    }
    else
    {
    	in.seekg(0, ios::end);
	    size_t len = in.tellg();
        encryptVal<size_t>(encryptor, len);
        in.seekg(0);

        FileSource f(in, true);
        f.TransferAllTo(encryptor);

        in.close();
    }
}
