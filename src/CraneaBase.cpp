#include "CraneaBase.h"

using namespace std;

#include "sha.h"

using namespace CryptoPP;

#include <iostream>

void debugBinary(const byte *buf, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        cout << (int)buf[i] << " ";
    }
    cout << endl;
}

void transformLower(string &text)
{
    transformString(text, (int(*)(int))tolower);
}

bool isOkFilenameChar(char c)
{
    return isOkPathChar(c) && c != '\\' && c != '/';
}

bool isOkPathChar(char c)
{
    if (c < 32)
        return false;
    if (c == '*' || c == '<' || c == '>' || c == '"')
        return false;
    if (c == ':' || c == ';' || c == '?' || c == '|')
        return false;

    return true;
}

void cleanFilename(string &filename, bool allowSubdir)
{
    string cleaned; 
    for (size_t i = 0; i < filename.size(); i++)
    {
        char ch = filename[i];
        cleaned += (allowSubdir ? isOkPathChar(ch) : isOkFilenameChar(ch)) ? ch : '_';
    }
}

std::string baseFileName(const std::string &path)
{
    size_t slashPos = path.rfind('/');
    if (slashPos == string::npos)
    {
        slashPos = path.rfind('\\');
    }

    return (slashPos != string::npos) ? path.substr(slashPos+1) : path;
}

void concatenateTokens(const std::vector<std::string> &tokens, std::string &str)
{
    size_t numTokens = tokens.size();
    for (size_t j = 0; j < numTokens; j++)
    {
        str.append(tokens[j]);
        if (j + 1 < numTokens)
        {
            str += ' ';
        }
    }
}


void hashKey(const byte *key, byte *keyhash)
{
    SHA1().CalculateDigest(keyhash, key, KEY_SIZE);
}

void commandKey(const string &cmd, byte *outputKey)
{
    byte buf[SHA1::DIGESTSIZE];
    SHA1 sha1;
    std::string someStuff = "command!";
    sha1.Update((byte *)someStuff.c_str(), someStuff.length());
    sha1.Update((byte *)cmd.c_str(), cmd.length());
    sha1.Final(buf);
    memcpy(outputKey, buf, KEY_SIZE);
}

byte *CraneaBase::keyhash() 
{
    if (!keyhash_)
    {
        keyhash_ = new byte[KEYHASH_SIZE];
        hashKey(key(), keyhash_);
    }
    return keyhash_;
}   

bool CraneaLocation::isIgnoredInternal(const std::string &token)
{
    return ignoredSet_.find(token) != ignoredSet_.end();
}

bool CraneaLocation::isIgnored(const std::string &token)
{
    CraneaLocation *curLoc = this;
    while (curLoc)
    {
        if (curLoc->isIgnoredInternal(token))
        {
            return true;
        }
        curLoc = curLoc->parent_;
    }
    return false;
}

void CraneaLocation::tokenize(const string &cmd, vector<string> &tokens)
{
    string spaces = " \t\n\r";
    size_t start = 0;
    string token;

    string lowerCmd = cmd;

    transformLower(lowerCmd);    

    while (true)
    {
        size_t startWord = lowerCmd.find_first_not_of(spaces, start);
        if (startWord == string::npos)
        {
            break;
        }
        size_t endWord = lowerCmd.find_first_of(spaces, startWord);
        if (endWord != string::npos)
        {
            token = lowerCmd.substr(startWord, endWord - startWord);
        }
        else
        {
            token = lowerCmd.substr(startWord);
        }

        if (!isIgnored(token))
        {
            tokens.push_back(token);
        }
        
        if (endWord == string::npos)
        {
            break;
        }
        start = endWord + 1;
    }
}

void CraneaLocation::normalize(std::string &cmd)
{
    vector<string> tokens;
    tokenize(cmd, tokens);
    cmd = "";
    concatenateTokens(tokens, cmd);
}

byte *CraneaItem::takeyhash()
{
    if (!takeyhash_)
    {
        takeyhash_ = new byte[KEYHASH_SIZE];
        hashKey(takey(), takeyhash_);
    }
    return takeyhash_;
}
