#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>

using namespace std;

/* PORTS ASSIGNED TO OUR GROUP
        10034-10037
*/

void usage();
vector<string> getUserInput(void);

int main(int argc, char *argv[])
{
    float fDamaged = 0;
    float fLost = 0;
    bool bContinue = true;
    string userInput;

    for(int i=1;i < argc; i+= 2)
    {
        switch (argv[i] [1])
        {
            case 'd':
            {
                fDamaged = atof(argv[i+1]);
            };
            break;

            case 'l':
            {
                fLost = atof(argv[i+1]);
            };
            break;

            case 'h':
            {
                usage();
                return 0;
            }
        }
    }

    while( bContinue ) {
        vector<string> userInput = getUserInput();

        if( userInput.front().compare("put") == 0 ) {
            cout << "Putting " << userInput[1] << endl;
        } else if( userInput.front().compare("quit") == 0 ) {
            cout << "Good-bye!\n";
            bContinue = false;
        } else {
            cout << "Unrecognized\n";
        }
    }

    return 0;
}

void usage() {
    cout << "Use the following syntax: \nproject1 -l <lost packets> -d <damaged packets>" << endl;
}

vector<string> getUserInput(void) {
    vector<string> result;
    string sLine;

    cout << "> ";

    getline( cin, sLine );

    // Parse words into a vector
    string word;
    istringstream iss(sLine);

    while( iss >> word ) result.push_back(word);

    return result;
}
