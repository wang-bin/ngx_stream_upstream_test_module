
/*
 * Copyright (C) WangBin
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_stream.h>

static char *ngx_stream_upstream_test(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);


static ngx_command_t  ngx_stream_upstream_test_commands[] = {

    { ngx_string("test"),
      NGX_STREAM_UPS_CONF|NGX_CONF_NOARGS,
      ngx_stream_upstream_test,
      0,
      0,
      NULL },

      ngx_null_command
};


static ngx_stream_module_t  ngx_stream_upstream_test_module_ctx = {
    NULL,                                    /* preconfiguration */
    NULL,                                    /* postconfiguration */

    NULL,                                    /* create main configuration */
    NULL,                                    /* init main configuration */

    NULL,                                    /* create server configuration */ // TODO: create ngx_stream_upstream_check_srv_conf_t
    NULL                                     /* merge server configuration */
};


ngx_module_t  ngx_stream_upstream_test_module = {
    NGX_MODULE_V1,
    &ngx_stream_upstream_test_module_ctx, /* module context */
    ngx_stream_upstream_test_commands, /* module directives */
    NGX_STREAM_MODULE,                       /* module type */
    NULL,                                    /* init master */
    NULL,                                    /* init module */
    NULL,                                    /* init process */
    NULL,                                    /* init thread */
    NULL,                                    /* exit thread */
    NULL,                                    /* exit process */
    NULL,                                    /* exit master */
    NGX_MODULE_V1_PADDING
};

typedef struct init_upstream_chain_s  init_upstream_chain_t;

struct init_upstream_chain_s {
    ngx_stream_upstream_init_pt init_upstream;
    init_upstream_chain_t* next;
};


static ngx_int_t
ngx_stream_upstream_init_test(ngx_conf_t *cf, ngx_stream_upstream_srv_conf_t *us)
{
    // us->peer.data now is peers(ngx_stream_upstream_rr_peers_t*)
// store peers->peer to set peer->down later. add peer + check_index
    return NGX_OK;
}

static ngx_int_t
ngx_stream_upstream_init_test_all(ngx_conf_t *cf, ngx_stream_upstream_srv_conf_t *us)
{
    ngx_log_debug0(NGX_LOG_DEBUG_STREAM, cf->log, 0, "init test");

    init_upstream_chain_t* init_ups = us->peer.data;

    init_upstream_chain_t* i = init_ups;
    while (i) {
        if (i->init_upstream)
            i->init_upstream(cf, us);
        i = i->next;
        // free i if not from pool
    }
    return NGX_OK;
}


static char *
ngx_stream_upstream_test(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
/*!
    call in ngx_command_t.set(). init_upstream was set in one of upstream load balance command
    alternatives:
    - push {uscf, init_upstream} to a global array. can be used in multiple modules like a callback chain
    - uscf->peer.data = prev_init_upstream? peer.data is used in rr init_upstream later. can be used only in 1 module if data is not allocated as a struct {func, next}
 */
    ngx_stream_upstream_srv_conf_t  *uscf;
    init_upstream_chain_t* this_init = NULL;
    init_upstream_chain_t* init = ngx_pcalloc(cf->pool, sizeof(init_upstream_chain_t)); // ngx_alloc?

    uscf = ngx_stream_conf_get_module_srv_conf(cf, ngx_stream_upstream_module);
    if (uscf->peer.data) {
        // set init_upstream_chain_s.next: real works
        this_init = init;
        init = uscf->peer.data;
        while (init->next) {
            init = init->next;
        }
    } else {
        // alloc init_upstream_chain_s as uscf->peer.data.
        // init_upstream_chain_s.init_upstream = uscf->peer.init_upstream(never null?)
        // uscf->peer.init_upstream: data = uscf->peer.data(will be reset in ngx_stream_upstream_init_round_robin), loop init_upstream_chain_t and call init, free data at last
        init->init_upstream = uscf->peer.init_upstream;
        uscf->peer.data = init;
        uscf->peer.init_upstream = ngx_stream_upstream_init_test_all;
        // add next init_upstream_chain_s.init_upstream: real works
        this_init = ngx_pcalloc(cf->pool, sizeof(init_upstream_chain_t));
    }
    this_init->init_upstream = ngx_stream_upstream_init_test;
    init->next = this_init;

    return NGX_CONF_OK;
}
