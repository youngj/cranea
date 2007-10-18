
#include "cranea.h"
#include <string>
#include "SourceOutput.h"
#include "SourceBase.h"

#define XML_STATIC

#include "expat.h"
using namespace std;
#include <iostream>
#include <fstream>

#define OUTPUT_DIR "game"

#include <sys/stat.h> 

void startElementHandler(void *_ctx, const XML_Char *name, const XML_Char **atts)
{
    SourceContext *ctx = (SourceContext *)_ctx;

    map<string, string> attributes;

    const XML_Char **curAtt = atts;
    while (*curAtt)
    {
        attributes[curAtt[0]] = curAtt[1];
        curAtt += 2;
    }
    
    ctx->startElement(name, attributes);
}
void endElementHandler(void *_ctx, const XML_Char *name)
{
    SourceContext *ctx = (SourceContext *)_ctx;
    ctx->endElement();
}

void textHandler(void *_ctx, const XML_Char *s, int len)
{
    SourceContext *ctx = (SourceContext *)_ctx;

    ctx->handleText(string(s,len));
}

void printPos(XML_Parser parser)
{
    cout << " at line " << XML_GetCurrentLineNumber(parser);
    cout << ", column " << XML_GetCurrentColumnNumber(parser) << endl;
}

bool parse(XML_Parser parser, const char *buf, size_t length, bool last)
{
    bool parseResult;
    try 
    {
        parseResult = XML_Parse(parser, buf, (int)length, last) ? true : false;
    }
    catch (InvalidSourceException &ex)
    {
        cout << "Invalid XML: " << ex.message() << endl;
        printPos(parser);
        return false;
    }

    if (!parseResult)
    {
        XML_Error err = XML_GetErrorCode(parser);
        cout << XML_ErrorString(err);
        printPos(parser);
        return false;
    }
    return true;
}

bool parseXML(SourceContext &ctx, ifstream &xmlInput, XML_Parser parser)
{
    while (!xmlInput.eof())
    {
        string buf;
        getline(xmlInput, buf);
        buf += "\n";

        if (!parse(parser, buf.c_str(), buf.length(), false))
        {
            return false;
        }
    }

    if (!parse(parser, NULL, 0, true))
    {
        return false;
    }

    try
    {
        ctx.resolve();
    }
    catch (InvalidSourceException &ex)
    {
        cout << "Invalid XML: " << ex.message() << endl;
        return false;
    }
    return true;
}


bool parseInput(SourceContext &ctx, const string &filename)
{
    ifstream xmlInput(filename.c_str());
    if (xmlInput.fail())
    {
        cout << "could not open " << filename << endl;
        return false;
    }

    XML_Parser parser = XML_ParserCreate(NULL);
    XML_SetUserData(parser, &ctx);
    XML_SetElementHandler(parser, startElementHandler, endElementHandler);
    XML_SetCharacterDataHandler(parser, textHandler);

    bool success = parseXML(ctx, xmlInput, parser);

    XML_ParserFree(parser); 

    return success;
}

string promptDefault(const string &prompt, const string &def)
{
    cout << prompt << " [" << def << "]: ";
    string line;
    getline(cin, line);
    return line.empty() ? def : line;
}

int main(int argc, char* argv[])
{
    SourceContext ctx;

    string inputFile = promptDefault("input file", "adventure.xml");

    bool success = parseInput(ctx, inputFile);

    if (success)
    {
        string gamePrefix = OUTPUT_DIR"/" + ctx.gameName;
        string encryptedFilename = gamePrefix + ENCRYPTED_EXT;
        string playerFilename = gamePrefix + ".exe";

        struct stat outputDirInfo; 
        if (stat(OUTPUT_DIR, &outputDirInfo) != 0) 
        { 
            system("mkdir "OUTPUT_DIR);
        }

        system(("cp player.exe \"" + playerFilename + "\"").c_str());

        SourceOutput out(encryptedFilename);
        
        ctx.write(out);

        out.close();

        cout << "done!" << endl;
    }

    //string line;
    //getline(cin, line);

    return success ? 0 : 1;
}
