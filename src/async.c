#include "ngx_core.h"
#include "ngx_cycle.h"
#include "ngx_http.h"
#include "ngx_http_lua_api.h"
#include "ngx_string.h"
#include "ngx_thread_pool.h"
#include "ngx_time.h"
#include <luajit-2.1/lauxlib.h>
#include <luajit-2.1/lua.h>
#include <stdio.h>
#include <sys/resource.h>

/* Module context structure */
// static ngx_http_module_t ngx_http_mydb_module_ctx = {
//     NULL, /* Preconfiguration */
//     NULL, /* Postconfiguration */
//     NULL, /* Create main configuration */
//     NULL, /* Init main configuration */
//     NULL, /* Create server configuration */
//     NULL, /* Merge server configuration */
//     NULL, /* Create location configuration */
//     NULL  /* Merge location configuration */
// };

// /* Module definition */
// ngx_module_t ngx_http_mydb_module = {
//     NGX_MODULE_V1,
//     &ngx_http_mydb_module_ctx, /* Module context */
//     NULL,                      /* Module directives */
//     NGX_HTTP_MODULE,           /* Module type */
//     NULL,                      /* Init master */
//     NULL,                      /* Init module */
//     NULL,                      /* Init process */
//     NULL,                      /* Init thread */
//     NULL,                      /* Exit thread */
//     NULL,                      /* Exit process */
//     NULL,                      /* Exit master */
//     NGX_MODULE_V1_PADDING,
// };

//
// Per-request context.  This is effectively the "out-params" from the thread pool task.
//
typedef struct
{
    int msSleep;           // Time the task slept while doing background work, in milliseconds.
    ngx_http_request_t *r; // Http Request pointer, for the thread completion.
    lua_State *co;         // The Lua coroutine
} ngx_http_ericsten_ctx_t;

//
// Per-task context.  This is effectively the "in-params" to the thread pool task.
//
typedef struct
{
    ngx_http_lua_co_ctx_t *base; // Base coroutine context
    ngx_http_request_t *r; // Http Request pointer, for the thread completion.
    lua_State *co;         // The Lua coroutine
    int random_value;
} ngx_http_ericsten_task_ctx_t;

// static int ngx_http_lua_resume(lua_State *L, ngx_http_request_t *r)
// {
//     printf("resuming\n");

//     int status = lua_resume(L, 0);

//     if (status == LUA_YIELD)
//     {
//         ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "Coroutine yielded");
//         return status;
//     }
//     else if (status == LUA_OK)
//     {
//         ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "Coroutine completed successfully");
//         return status;
//     }
//     else
//     {
//         const char *err_msg = lua_tostring(L, -1);
//         ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "lua_resume failed: %s", err_msg);
//         lua_pop(L, 1); // Remove error message from stack
//         return status;
//     }
// }

static void my_completion_handler(ngx_event_t *ev)
{
    printf("calling completion handler\n");

    ngx_http_ericsten_task_ctx_t *ctx = ev->data;
    ngx_http_request_t *r = ctx->r;
    ngx_http_lua_co_ctx_t *base = ctx->base;

    printf("ctx = %p\n", ctx);
    printf("ctx->r = %p\n", ctx->r);
    printf("ctx->r->state = %lx\n", ctx->r->state);
    printf("ctx->base = %p\n", ctx->base);
    printf("ctx->co = %p\n", ctx->co);

    // ngx_http_lua_co_ctx_t *co_ctx = ngx_http_lua_get_cur_co_ctx(r);
    // printf("getting current context %p\n", co_ctx);
    // if (co_ctx == NULL)
    // {
    //     ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "Failed to get current coroutine context");
    //     return;
    // }



    // Set the status as a return value of the coroutine
    lua_pushstring(ctx->co, "test");
    printf("calling resume\n");
    ngx_http_lua_co_ctx_resume_helper(base, 1);
    printf("called resume\n");
    // ngx_http_lua_resume(ctx->co, r);
    // lua_pushstring(L, "sleep complete");
    // int status = ngx_http_lua_resume(L, r);
    // ngx_http_run_posted_requests(r->connection);

    // if (status != LUA_YIELD && status != LUA_OK)
    // {
    //     const char *err_msg = lua_tostring(L, -1);
    //     ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "lua_resume failed: %s", err_msg);
    //     lua_pop(L, 1); // Remove error message from stack
    //     ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
    // }
}

static void my_work_handler(void *data, ngx_log_t *log)
{
    printf("calling work handler\n");
    ngx_http_ericsten_task_ctx_t *ctx = data;
    printf("calling work handler\n");
    ngx_msleep(2000);
}

static int async_test(lua_State *L)
{
    printf("calling async_test\n");
    ngx_http_request_t *r = ngx_http_lua_get_request(L);
    if (!r)
    {
        return luaL_error(L, "failed to retrieve request");
    }
    printf("request = %p\n", r);

    ngx_http_ericsten_task_ctx_t *ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_ericsten_task_ctx_t));
    if (ctx == NULL)
    {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_http_lua_co_ctx_t *cur_co_ctx = ngx_http_lua_get_cur_co_ctx(r);
    printf("cur_co_ctx = %p\n", cur_co_ctx);

    ctx->r = r;
    ctx->co = L;
    ctx->base = cur_co_ctx;

    ngx_str_t pool_name = ngx_string("default"); // Use "default" thread pool
    ngx_thread_pool_t *thread_pool = ngx_thread_pool_get((ngx_cycle_t *)ngx_cycle, &pool_name);
    if (!thread_pool)
    {
        return luaL_error(L, "failed to get thread pool");
    }

    ngx_thread_task_t *task = ngx_thread_task_alloc(r->pool, sizeof(ngx_http_ericsten_task_ctx_t));
    if (!task)
    {
        return luaL_error(L, "failed to allocate task");
    }

    task->handler = my_work_handler;
    task->event.handler = my_completion_handler;
    task->event.log = r->connection->log;
    task->event.data = ctx;
    ngx_http_lua_set_cur_co_ctx(r, cur_co_ctx);

    if (ngx_thread_task_post(thread_pool, task) != NGX_OK)
    {
        return luaL_error(L, "failed to post task to thread pool");
    }

    return lua_yield(L, 0);
}
