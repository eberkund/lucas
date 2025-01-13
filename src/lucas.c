///
/// @file lucas.c
/// @brief Lua module entrypoint.
/// @author Erik Berkun-Drevnig<erik.berkundrevnig@lotusflare.com>
/// @copyright Copyright (c) 2023 LotusFlare, Inc.
///

#include "async.c"
#include "batch.c"
#include "compatible.c"
#include "connect.c"
#include "logs.c"
#include "luajit-2.1/lauxlib.h"
#include "luajit-2.1/lua.h"
#include "metrics.c"
#include "query.c"
#include "types.c"

int version(lua_State *L)
{
    lua_pushstring(L, VERSION);
    return 1;
}

int luaopen_lucas(lua_State *L)
{
    luaL_Reg lucas[] = {
        {"version", version},
        {"connect", lucas_connect},
        {"query", query},
        {"batch", batch},
        {"logger", logger},
        {"metrics", metrics},
        {"speculative_execution_metrics", speculative_execution_metrics},

        {"ascii", type_ascii},
        {"bigint", type_bigint},
        {"blob", type_blob},
        {"boolean", type_boolean},
        {"counter", type_counter},
        {"decimal", type_decimal},
        {"double", type_double},
        {"float", type_float},
        {"int", type_int},
        {"text", type_text},
        {"timestamp", type_timestamp},
        {"uuid", type_uuid},
        {"varchar", type_varchar},
        {"varint", type_varint},
        {"timeuuid", type_timeuuid},
        {"inet", type_inet},
        {"date", type_date},
        {"time", type_time},
        {"smallint", type_smallint},
        {"tinyint", type_tinyint},
        {"duration", type_duration},
        {"list", type_list},
        {"map", type_map},
        {"set", type_set},
        {"udt", type_udt},
        {"tuple", type_tuple},
        {"null", type_null},
        {"unset", type_unset},

        {NULL, NULL},
    };
    luaL_Reg lucas_compatible[] = {
        {"convert", convert},

        {NULL, NULL},
    };
    luaL_Reg lucas_resty[] = {
        {"async_test", async_test},

        {NULL, NULL},
    };

    // luaL_openlib(L, "lucas", lucas, 0);
    // luaL_openlib(L, "lucas.compatible", lucas_compatible, 0);
    // luaL_openlib(L, "lucas.resty", lucas_resty, 0);

    lua_newtable(L);
    const int table = lua_gettop(L);
    luaL_setfuncs(L, lucas, 0);

    lua_pushstring(L, "compatible");
    luaL_newlib(L, lucas_compatible);
    lua_settable(L, table);

    lua_pushstring(L, "resty");
    luaL_newlib(L, lucas_resty);
    lua_settable(L, table);

    cass_log_set_level(CASS_LOG_DISABLED);
    return 1;
}
