///
/// @file connect.c
/// @brief Configuring and connecting to the cluster.
/// @author Erik Berkun-Drevnig<erik.berkundrevnig@lotusflare.com>
/// @copyright Copyright (c) 2023 LotusFlare, Inc.
///

#include "cassandra.h"
#include "errors.c"
#include "logs.c"
#include "luajit-2.1/lua.h"
#include "state.c"
#include <string.h>

LucasError *set_port(lua_State *L, int i, CassCluster *cluster)
{
    int port = 9042;
    lua_getfield(L, i, "port");
    if (!lua_isnil(L, lua_gettop(L)))
    {
        port = lua_tointeger(L, lua_gettop(L));
    }
    CassError err = cass_cluster_set_port(cluster, port);
    if (err != CASS_OK)
    {
        return lucas_new_errorf_from_cass_error(err, "could not set port to %d", port);
    }
    return NULL;
}

LucasError *set_num_threads_io(lua_State *L, int i, CassCluster *cluster)
{
    int num_threads_io = 1;
    lua_getfield(L, i, "num_threads_io");
    if (!lua_isnil(L, lua_gettop(L)))
    {
        num_threads_io = lua_tointeger(L, lua_gettop(L));
    }
    CassError err = cass_cluster_set_num_threads_io(cluster, num_threads_io);
    if (err != CASS_OK)
    {
        return lucas_new_errorf_from_cass_error(err, "could not set IO thread count to %d", num_threads_io);
    }
    return NULL;
}

LucasError *set_connect_timeout(lua_State *L, int i, CassCluster *cluster)
{
    int connect_timeout = 5000;
    lua_getfield(L, i, "connect_timeout");
    if (!lua_isnil(L, lua_gettop(L)))
    {
        connect_timeout = lua_tointeger(L, lua_gettop(L));
    }
    cass_cluster_set_connect_timeout(cluster, connect_timeout);
    return NULL;
}

LucasError *set_use_latency_aware_routing(lua_State *L, int i, CassCluster *cluster)
{
    int latency_aware_routing = false;
    lua_getfield(L, i, "use_latency_aware_routing");
    if (!lua_isnil(L, lua_gettop(L)))
    {
        latency_aware_routing = lua_toboolean(L, lua_gettop(L));
    }
    cass_cluster_set_latency_aware_routing(cluster, latency_aware_routing);
    return NULL;
}

LucasError *set_contact_points(lua_State *L, int i, CassCluster *cluster)
{
    lua_getfield(L, i, "contact_points");
    const char *contact_points = "127.0.0.1";
    if (!lua_isnil(L, lua_gettop(L)))
    {
        contact_points = lua_tostring(L, lua_gettop(L));
    }
    CassError err = cass_cluster_set_contact_points(cluster, contact_points);
    if (err != CASS_OK)
    {
        return lucas_new_errorf_from_cass_error(err, "could not set contact points %s", contact_points);
    }
    return NULL;
}

LucasError *set_application_name(lua_State *L, int i, CassCluster *cluster)
{
    lua_getfield(L, i, "application_name");
    const char *application_name = NULL;
    if (!lua_isnil(L, lua_gettop(L)))
    {
        application_name = lua_tostring(L, lua_gettop(L));
    }
    cass_cluster_set_application_name(cluster, application_name);
    return NULL;
}

LucasError *set_connection_heartbeat_interval(lua_State *L, int i, CassCluster *cluster)
{
    int heartbeat_interval = 1000;
    lua_getfield(L, i, "heartbeat_interval");
    if (!lua_isnil(L, lua_gettop(L)))
    {
        heartbeat_interval = lua_tointeger(L, lua_gettop(L));
    }
    cass_cluster_set_connection_heartbeat_interval(cluster, heartbeat_interval);
    return NULL;
}

LucasError *set_constant_reconnect(lua_State *L, int i, CassCluster *cluster)
{
    int constant_reconnect = 1000;
    lua_getfield(L, i, "constant_reconnect");
    if (!lua_isnil(L, lua_gettop(L)))
    {
        constant_reconnect = lua_tointeger(L, lua_gettop(L));
    }
    cass_cluster_set_constant_reconnect(cluster, constant_reconnect);
    return NULL;
}

LucasError *set_protocol_version(lua_State *L, int i, CassCluster *cluster)
{
    lua_getfield(L, i, "protocol_version");
    const int protocol_version_index = lua_gettop(L);
    if (lua_isnil(L, protocol_version_index))
    {
        return NULL;
    }
    const int protocol_version = lua_tointeger(L, protocol_version_index);
    CassError err = cass_cluster_set_protocol_version(cluster, protocol_version);
    if (err != CASS_OK)
    {
        return lucas_new_errorf_from_cass_error(err, "could not set protocol version to %d", protocol_version);
    }
    return NULL;
}

LucasError *set_ssl(lua_State *L, int i, CassCluster *cluster)
{
    LucasError *rc = NULL;
    CassError err = CASS_OK;
    CassSsl *ssl = cass_ssl_new();

    lua_getfield(L, i, "ssl");
    int ssl_index = lua_gettop(L);
    if (lua_isnil(L, ssl_index))
    {
        lucas_log(LOG_WARN, "ssl options not provided");
        goto cleanup;
    }

    lua_getfield(L, ssl_index, "certificate");
    if (lua_isnil(L, lua_gettop(L)))
    {
        rc = lucas_new_errorf("certificate is missing");
        goto cleanup;
    }
    const char *cert = lua_tostring(L, lua_gettop(L));
    lucas_log(LOG_DEBUG, "cert loaded: size=%d", strlen(cert));
    err = cass_ssl_set_cert(ssl, cert);
    if (err != CASS_OK)
    {
        rc = lucas_new_errorf_from_cass_error(err, "failed to set certificate");
        goto cleanup;
    }

    lua_getfield(L, ssl_index, "private_key");
    if (lua_isnil(L, lua_gettop(L)))
    {
        rc = lucas_new_errorf("private key is missing");
        goto cleanup;
    }
    const char *private_key = lua_tostring(L, lua_gettop(L));
    lucas_log(LOG_DEBUG, "key loaded, size=%d", strlen(private_key));

    lua_getfield(L, ssl_index, "password");
    const char *password = NULL;
    if (!lua_isnil(L, lua_gettop(L)))
    {
        password = lua_tostring(L, lua_gettop(L));
    }

    err = cass_ssl_set_private_key(ssl, private_key, password);
    if (err != CASS_OK)
    {
        rc = lucas_new_errorf_from_cass_error(err, "failed to set private key");
        goto cleanup;
    }

    cass_ssl_set_verify_flags(ssl, CASS_SSL_VERIFY_NONE);
    cass_cluster_set_ssl(cluster, ssl);
    lucas_log(LOG_INFO, "ssl configured");

cleanup:
    cass_ssl_free(ssl);
    return rc;
}

LucasError *set_authenticator(lua_State *L, int i, CassCluster *cluster)
{
    lua_getfield(L, i, "credentials");
    int credentials_index = lua_gettop(L);
    if (lua_isnil(L, credentials_index))
    {
        lucas_log(LOG_WARN, "credentials not provided");
        return NULL;
    }

    lua_getfield(L, credentials_index, "username");
    if (lua_isnil(L, lua_gettop(L)))
    {
        return lucas_new_errorf("username is missing");
    }
    const char *username = lua_tostring(L, lua_gettop(L));
    lucas_log(LOG_DEBUG, "setting username=%s", username);

    lua_getfield(L, credentials_index, "password");
    if (lua_isnil(L, lua_gettop(L)))
    {
        return lucas_new_errorf("password is missing");
    }
    const char *password = lua_tostring(L, lua_gettop(L));
    lucas_log(LOG_DEBUG, "setting password=%s", password);

    cass_cluster_set_credentials(cluster, username, password);
    lucas_log(LOG_INFO, "authenticator configured");

    return NULL;
}

LucasError *set_consistency(lua_State *L, int i, CassCluster *cluster)
{
    lua_getfield(L, i, "consistency");
    const int consistency_index = lua_gettop(L);
    if (lua_isnil(L, consistency_index))
    {
        return NULL;
    }
    const CassConsistency consistency = lua_tointeger(L, consistency_index);
    CassError err = cass_cluster_set_consistency(cluster, consistency);
    if (err != CASS_OK)
    {
        return lucas_new_errorf_from_cass_error(err, "could not set consistency to %d", consistency);
    }
    return NULL;
}

LucasError *set_log_level(lua_State *L, int i, CassCluster *cluster)
{
    lua_getfield(L, i, "log_level");
    const int log_level_index = lua_gettop(L);
    if (lua_isnil(L, log_level_index))
    {
        return NULL;
    }
    const CassLogLevel log_level = lua_tointeger(L, log_level_index);
    cass_log_set_level(log_level);
    return NULL;
}

typedef LucasError *(*ConfigCallback)(lua_State *, int, CassCluster *);

ConfigCallback functions[] = {
    set_contact_points,
    set_protocol_version,
    set_consistency,
    set_port,
    set_num_threads_io,
    set_ssl,
    set_authenticator,
    set_log_level,
    set_use_latency_aware_routing,
    set_connection_heartbeat_interval,
    set_constant_reconnect,
    set_connect_timeout,
    set_application_name,
};

static int connect(lua_State *L)
{
    const int ARG_OPTIONS = 1;
    CassError err = CASS_OK;
    CassFuture *future = NULL;
    CassCluster *cluster = NULL;
    LucasError *rc = NULL;

    if (session)
    {
        lucas_log(LOG_WARN, "already connected");
        cass_session_free(session);
        return 0;
    }

    session = cass_session_new();
    cluster = cass_cluster_new();
    lucas_log(LOG_INFO, "configuring cluster with provided options");

    for (int i = 0; i < sizeof(functions) / sizeof(ConfigCallback); i++)
    {
        rc = functions[i](L, ARG_OPTIONS, cluster);
        if (rc)
        {
            goto cleanup;
        }
    }

    lucas_log(LOG_INFO, "configuration done, attempting to connect");
    future = cass_session_connect(session, cluster);
    cass_future_wait(future);
    err = cass_future_error_code(future);
    if (err != CASS_OK)
    {
        rc = lucas_new_errorf_from_cass_error(err, "could not connect to cluster");
    }
    if (rc)
    {
        goto cleanup;
    }
    lucas_log(LOG_INFO, "session connect success");

cleanup:
    if (future)
    {
        cass_future_free(future);
    }
    if (cluster)
    {
        cass_cluster_free(cluster);
    }
    if (rc)
    {
        lucas_error_to_lua(L, rc);
    }
    return 0;
}
