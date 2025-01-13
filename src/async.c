#include "ngx_core.h"
#include "ngx_cycle.h"
#include "ngx_http.h"
#include "ngx_http_lua_api.h"
#include "ngx_string.h"
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
    int callback_ref;            // Lua registry reference for the callback function
} ngx_http_lua_ctx_t;

static void ngx_http_lua_resume(lua_State *co, ngx_http_request_t *r)
{
    printf("resuming\n");
    int status;
    int nresults;

    // Resume the coroutine with arguments already on the stack
    status = lua_resume(co, lua_gettop(co) - 1);
    printf("status %d", status);
    printf("top = %s", lua_typename(co, -1));
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "top = %s", lua_typename(co, lua_gettop(co)));

    return;

    // if (status == LUA_OK)
    // { // LUA_OK in Lua 5.1
    //     // Coroutine finished successfully
    //     if (lua_gettop(co) >= 1 && lua_isstring(co, -1))
    //     {
    //         // Get the result string from the Lua stack
    //         const char *result = lua_tostring(co, -1);
    //         lua_pop(co, 1); // Remove the result from the stack

    //         ngx_buf_t *b;
    //         ngx_chain_t out;

    //         // Allocate a buffer for the response
    //         b = ngx_create_temp_buf(r->pool, ngx_strlen(result));
    //         if (b == NULL)
    //         {
    //             ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Failed to allocate buffer for Lua result");
    //             ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
    //             return;
    //         }

    //         // Copy the result into the buffer
    //         ngx_memcpy(b->pos, result, ngx_strlen(result));
    //         b->last = b->pos + ngx_strlen(result);
    //         b->last_buf = 1;

    //         out.buf = b;
    //         out.next = NULL;

    //         // Send the response to the client
    //         r->headers_out.status = NGX_HTTP_OK;
    //         r->headers_out.content_length_n = ngx_strlen(result);
    //         ngx_http_send_header(r);
    //         ngx_http_output_filter(r, &out);
    //         ngx_http_finalize_request(r, NGX_DONE);
    //     }
    //     else
    //     {
    //         // No result on the Lua stack
    //         ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Lua coroutine completed without a valid result");
    //         ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
    //     }
    // }
    // else if (status == LUA_YIELD)
    // {
    //     // Coroutine yielded; do nothing (will be resumed later)
    //     return;
    // }
    // else
    // {
    //     // Coroutine errored
    //     const char *err = lua_tostring(co, -1);
    //     if (err)
    //     {
    //         ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Lua coroutine error: %s", err);
    //     }
    //     else
    //     {
    //         ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Lua coroutine error (unknown)");
    //     }
    //     lua_pop(co, 1); // Remove the error message from the stack

    //     ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
    // }
}

static void my_completion_handler(ngx_event_t *ev)
{
    printf("completion handler\n");
    printf("preparing 1/3\n");
    ngx_thread_task_t *task = ev->data;
    printf("preparing 2/3\n");
    ngx_http_lua_ctx_t *ctx = task->ctx;
    printf("preparing 3/3, %p\n", ctx);
    lua_State *co = ctx->co;

    // Push result or error onto Lua stack
    if (ctx->error)
    {
        printf("error\n");
        lua_pushnil(co);
        lua_pushstring(co, "operation failed");
    }
    else
    {
        printf("no error\n");
        lua_pushstring(co, "no error");
        lua_pushnil(co); // No error
    }

    // Resume the Lua coroutine
    ngx_http_lua_resume(co, ctx->request);
}

static void my_work_handler(void *data, ngx_log_t *log)
{
    ngx_http_lua_ctx_t *ctx = data;
    printf("calling work handler\n");
    // Simulate a blocking operation (e.g., database query)
    ngx_msleep(1000); // Replace with actual blocking code

    // Set the result of the operation
    ngx_str_t result = ngx_string("result");
    ctx->result = ngx_pstrdup(ctx->request->pool, &result);
    ctx->error = 0; // No error
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
    printf("new thread created %p\n", co);
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

// Initialize the task
    task->handler = my_work_handler;            // Task handler
    task->event.handler = my_completion_handler; // Completion handler
    task->ctx = ctx;                            // Store the context in the task
    task->event.data = task;
    task->event.log = r->connection->log;       // Log for error messages

    // Get the NGINX thread pool
    ngx_str_t pool_name = ngx_string("default"); // Use "default" thread pool
    ngx_thread_pool_t *thread_pool = ngx_thread_pool_get(ngx_cycle, &pool_name);
    if (!thread_pool)
    {
        return luaL_error(L, "failed to get thread pool");
    }

    if (ngx_thread_task_post(thread_pool, task) != NGX_OK)
    {
        return luaL_error(L, "failed to post task to thread pool");
    }

    // Yield the coroutine and return
    return lua_yield(L, 0);
}
