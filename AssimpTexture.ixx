module;
#include <string>
export module AssimpTexture;

using namespace std;

export struct AssimpTexture {
    unsigned int id;
    string type;
    string path;
};