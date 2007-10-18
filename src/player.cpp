// player.cpp : Defines the entry point for the console application.
//

#include "cranea.h"
#include "GameBase.h"
#include "GameInput.h"
#include <iostream>
using namespace std;

int main(int argc, char* argv[])
{
    if (argc == 0)
    {
        cerr << "Need at least 1 argument to main!" << endl;
        return 1;
    }

    string executableName = baseFileName(argv[0]);
    
    size_t extPos = executableName.rfind('.');
    if (extPos != string::npos)
    {
        executableName = executableName.substr(0, extPos);
    }

    string encryptedFilename = executableName + ENCRYPTED_EXT;

    while (true)
    {
        GameInput in(encryptedFilename);        

        if (in.fail())
        {
            cerr << "Error opening data file " << encryptedFilename << "." << endl;
            cout << "Enter another file (blank to quit):";
            getline(cin, encryptedFilename);
            if (encryptedFilename.empty())
                break;
            else
                continue;
        }

        GameContext ctx(in);
        ctx.playGame();
        break;
    }

    return 0;
}

