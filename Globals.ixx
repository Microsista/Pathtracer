module;
#include <string>
export module Globals;

extern "C" {
#include "Lua542/include/lua.h"
#include "Lua542/include/lauxlib.h"
#include "Lua542/include/lualib.h"
}

import DXSampleHelper;

using namespace std;

export {
    bool CheckLua(lua_State* L, int r) {
        if (r != LUA_OK) {
            std::string errormsg = lua_tostring(L, -1);
            print(errormsg);
            return false;
        }
        return true;
    }

    int lua_HostFunction(lua_State* L) {
        float a = (float)lua_tonumber(L, 1);
        float b = (float)lua_tonumber(L, 2);
        print("[C++] Hostfunction(");
        print(a);
        print(", ");
        print(b);
        print(") called");
        float c = a * b;
        lua_pushnumber(L, c);
        return 1;
    }
}