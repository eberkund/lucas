#include "ngx_core.h"
#include "ngx_http_lua_api.h"
#include "ngx_thread_pool.h"
#include <luajit-2.1/lauxlib.h>
#include <luajit-2.1/lua.h>
#include <stdio.h>
#include <sys/resource.h>

typedef struct
{
    lua_State *co;               // The Lua coroutine
    ngx_http_request_t *request; // Associated HTTP request
} ngx_http_lua_ctx_t;

static ngx_int_t ngx_http_lua_resume(lua_State *L, ngx_http_request_t *r)
{
    int rc = lua_resume(L, 0);

    if (rc == LUA_YIELD)
    {
        // The coroutine yielded again, wait for the next event
        return NGX_DONE;
    }
    else if (rc != LUA_OK)
    {
        // An error occurred, log the error message
        const char *err_msg = lua_tostring(L, -1);
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "lua error: %s", err_msg);
        return NGX_ERROR;
    }

    // Retrieve the result from the coroutine
    if (lua_gettop(L) > 0)
    {
        const char *response = lua_tostring(L, -1);
        ngx_str_t resp;
        resp.data = (u_char *)response;
        resp.len = strlen(response);

        // Send the response back to the client
        ngx_buf_t *b = ngx_create_temp_buf(r->pool, resp.len);
        if (b == NULL)
        {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        ngx_memcpy(b->pos, resp.data, resp.len);
        b->last = b->pos + resp.len;
        b->last_buf = 1;

        ngx_chain_t out;
        out.buf = b;
        out.next = NULL;

        r->headers_out.status = NGX_HTTP_OK;
        r->headers_out.content_length_n = resp.len;

        ngx_http_send_header(r);
        ngx_http_output_filter(r, &out);
    }

    return NGX_OK;
}

static void my_event_handler(ngx_event_t *ev)
{
    printf("my handler\n");

    ngx_http_lua_ctx_t *ctx = ev->data;
    ngx_http_request_t *r = ctx->request;
    lua_State *co = ctx->co;

    // Check for errors or process the event result
    if (ev->timedout)
    {
        lua_pushnil(co);
        lua_pushstring(co, "timeout");
    }
    else
    {
        lua_pushboolean(co, 1);
        lua_pushstring(co, "event completed successfully");
    }

    // Resume the Lua coroutine
    // lua_resume(L, 0);
    ngx_http_lua_resume(co, r);
}

static int async_test(lua_State *L)
{
    ngx_http_request_t *r = ngx_http_lua_get_request(L);
    printf("calling async_test\n");

    if (!r)
    {
        printf("failed to retrieve request\n");
        return luaL_error(L, "failed to retrieve request");
    }

    // Allocate memory for your async operation
    // ngx_connection_t *c = ngx_pcalloc(r->pool, sizeof(ngx_connection_t));
    ngx_http_lua_ctx_t *ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_lua_ctx_t));

    if (!ctx)
    {
        printf("failed to allocate memory");
        return luaL_error(L, "failed to allocate memory");
    }

    // Create a new Lua coroutine for this request
    lua_State *co = lua_newthread(L);
    ctx->co = co;
    ctx->request = r;

    // Store the coroutine's reference in Lua registry to keep it alive
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushvalue(L, 1); // Push the function argument (callback)
    lua_xmove(L, co, 1);

    // Attach the context to the request
    // ngx_http_set_ctx(r, ctx, ngx_http_mydb_module);

    // Save Lua state and context for later resumption
    // c->data = L;

    // Register the event
    printf("registering event\n");
    ngx_thread_task_t *task = ngx_thread_task_alloc(r->pool, sizeof(ngx_thread_task_t));
    task->handler = my_event_handler;
    // ngx_event_t *ev = c->read;
    // ev->handler = my_event_handler; // Define the event callback
    // ev->data = c;
    // ngx_add_event(ev, NGX_READ_EVENT, 0);

    // Register the event

    ngx_event_t *ev = r->connection->read;
    ev->handler = my_event_handler;
    ev->data = ctx;
    ngx_add_event(ev, NGX_READ_EVENT, 0);

    // Suspend the coroutine
    printf("yielding\n");
    return lua_yield(L, 0);
}
