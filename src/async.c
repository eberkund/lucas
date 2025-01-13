#include "ngx_core.h"
#include "ngx_cycle.h"
#include "ngx_http.h"
#include "ngx_http_lua_api.h"
#include "ngx_thread_pool.h"
#include <luajit-2.1/lauxlib.h>
#include <luajit-2.1/lua.h>
#include <stdio.h>
#include <sys/resource.h>

/* Module context structure */
static ngx_http_module_t ngx_http_mydb_module_ctx = {
    NULL, /* Preconfiguration */
    NULL, /* Postconfiguration */
    NULL, /* Create main configuration */
    NULL, /* Init main configuration */
    NULL, /* Create server configuration */
    NULL, /* Merge server configuration */
    NULL, /* Create location configuration */
    NULL  /* Merge location configuration */
};

/* Module directives */
static ngx_command_t ngx_http_mydb_commands[] = {
    {ngx_string("mydb_directive"), NGX_HTTP_LOC_CONF | NGX_CONF_NOARGS, NULL, 0, 0, NULL}, ngx_null_command};

/* Module definition */
ngx_module_t ngx_http_mydb_module = {
    NGX_MODULE_V1,
    &ngx_http_mydb_module_ctx, /* Module context */
    ngx_http_mydb_commands,    /* Module directives */
    NGX_HTTP_MODULE,           /* Module type */
    NULL,                      /* Init master */
    NULL,                      /* Init module */
    NULL,                      /* Init process */
    NULL,                      /* Init thread */
    NULL,                      /* Exit thread */
    NULL,                      /* Exit process */
    NULL,                      /* Exit master */
    NGX_MODULE_V1_PADDING,
};

typedef struct
{
    lua_State *co;               // The Lua coroutine
    ngx_http_request_t *request; // Associated HTTP request
    u_char *result;                // Result of the async operation
    int error;                   // Error flag (0 = success, 1 = error)
} ngx_http_lua_ctx_t;

static ngx_int_t ngx_http_lua_resume(lua_State *L, ngx_http_request_t *r)
{
    printf("calling ngx_http_lua_resume\n");
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

static void my_completion_handler(ngx_event_t *ev)
{
    printf("completion handler\n");

    ngx_thread_task_t *task = (ngx_thread_task_t *)ev->data;
    ngx_http_lua_ctx_t *ctx = (ngx_http_lua_ctx_t *)task->ctx;
    lua_State *co = ctx->co;

    // Push result or error onto Lua stack
    if (ctx->error)
    {
        lua_pushnil(co);
        lua_pushstring(co, "operation failed");
    }
    else
    {
        lua_pushstring(co, "no error");
        lua_pushnil(co); // No error
    }

    // Resume the Lua coroutine
    // ngx_http_lua_co_ctx_resume_helper(co, 0);
    ngx_http_lua_resume(co, ctx->request);
}

static void my_work_handler(void *data, ngx_log_t *log)
{
    // ngx_http_lua_ctx_t *ctx = data;
    printf("calling work handler\n");
    // Simulate a blocking operation (e.g., database query)
    ngx_msleep(1000); // Replace with actual blocking code

    // // Set the result of the operation
    // ctx->result = ngx_pstrdup(ctx->request->pool, "query result");
    // ctx->error = 0; // No error
}

static int async_test(lua_State *L)
{
    printf("calling async_test\n");
    ngx_http_request_t *r = ngx_http_lua_get_request(L);
    if (!r)
    {
        return luaL_error(L, "failed to retrieve request");
    }

    // Allocate custom context
    ngx_http_lua_ctx_t *ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_lua_ctx_t));
    if (!ctx)
    {
        return luaL_error(L, "failed to allocate memory for context");
    }

    // Create a new Lua coroutine
    lua_State *co = lua_newthread(L);
    ctx->co = co;
    ctx->request = r;

    // Move the Lua callback function into the coroutine
    lua_pushvalue(L, 1); // Push the callback function
    lua_xmove(L, co, 1);

    // Save the coroutine in the Lua registry
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    // Attach the context to the request
    ngx_http_set_ctx(r, ctx, ngx_http_mydb_module);

    // Create and schedule the thread pool task
    ngx_thread_task_t *task = ngx_thread_task_alloc(r->pool, sizeof(ngx_thread_task_t));
    if (!task)
    {
        return luaL_error(L, "failed to allocate task");
    }

    task->handler = my_work_handler;             // Task handler
    task->event.handler = my_completion_handler; // Completion handler
    task->event.data = ctx;
    task->ctx = ctx;

    // ngx_thread_pool_t *thread_pool = ngx_thread_pool_get((ngx_str_t *)&ngx_thread_pool_name, r->connection->log);

    // Get the NGINX thread pool
    ngx_str_t pool_name = ngx_string("default"); // Use "default" thread pool
    ngx_thread_pool_t *thread_pool = ngx_thread_pool_get(ngx_cycle, &pool_name);
    if (!thread_pool)
    {
        return luaL_error(L, "failed to get thread pool");
    }

    // ngx_thread_pool_add(ngx_conf_t * cf, ngx_str_t * name)

    if (ngx_thread_task_post(thread_pool, task) != NGX_OK)
    {
        return luaL_error(L, "failed to post task to thread pool");
    }

    // Yield the coroutine and return
    return lua_yield(L, 0);
}
