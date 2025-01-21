#include "cassandra.h"
#include "luajit-2.1/lua.h"

void export_consistency_enum(lua_State *L)
{
    lua_newtable(L);
    int top = lua_gettop(L);

#define XX(consistency, desc)                                                                                          \
    lua_pushstring(L, desc);                                                                                           \
    lua_pushinteger(L, consistency);
    CASS_CONSISTENCY_MAPPING(XX)
#undef XX
}

void export_protocol_version_enum(lua_State *L)
{
    lua_newtable(L);
    int top = lua_gettop(L);

#define XX(log_level, desc)                                                                                            \
    lua_pushstring(L, desc);                                                                                           \
    lua_pushinteger(L, log_level);
    CASS_LOG_LEVEL_MAPPING(XX)
#undef XX
}
