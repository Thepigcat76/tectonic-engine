#pragma once

#include "lilc/alloc.h"
#include "lilc/bump.h"
#include "lilsockets.h"
#include "tt_assets.h"
#include "tt_packets.h"
#include "tt_shared.h"
#include <pthread.h>

typedef struct tt_engine_client tt_engine_client_t;

typedef struct tt_engine_server tt_engine_server_t;

typedef struct tt_subproc_info {
  const char *subproc_name;
  pthread_t thread_id;
  bool running;
} tt_subproc_info_t;

// -- NETWORKING --

void tt_packet_send_client(tt_engine_server_t *srvr_engine, addr_t client_addr,
                           tt_packet_t packet);

void tt_packet_send_all_clients(tt_engine_server_t *srvr_engine,
                                tt_packet_t packet);

void tt_packet_send_server(tt_engine_client_t *client_engine,
                           tt_packet_t packet);

typedef struct tt_client_desc {
  u64 client_id;
  addr_t address;
} tt_client_desc_t;

typedef struct tt_server tt_server_t;

typedef enum tt_server_state {
  TT_SERVER_RUNNING, // Server is running but not yet hosten and open
  TT_SERVER_OPEN,    // Server is running and open to accept clients
  TT_SERVER_STOPPED, // Server has been stopped.
} tt_server_state_e;

#ifndef _CUSTOM_SERVER_IMPL
struct tt_server {
  pthread_rwlock_t server_lock;
  addr_t address;

  poll_client_t *poll_clients;

  tt_server_state_e state;

  bool server_running;

  bool integrated;

  tt_client_desc_t *connected_clients;

  tt_packet_t *packet_queue;
  pthread_mutex_t packet_queue_mutex;

  bump_t packet_bump;
  allocator_t packet_alloc;
};
#endif

void tt_server_init(tt_server_t *server, bool integrated);

void tt_server_deinit(tt_server_t *server);

typedef struct tt_client_connection tt_client_connection_t;

#ifndef _CUSTOM_CLIENT_CONNECTION_IMPL
struct tt_client_connection {
  addr_t server_addr;
  bool connected;
  pthread_cond_t connected_cond;
  pthread_mutex_t connected_mutex;

  pthread_rwlock_t connection_rwlock;

  bool client_running;

  tt_packet_t *packet_queue;
  pthread_mutex_t packet_queue_mutex;

  bump_t packet_bump;
  allocator_t packet_alloc;
};
#endif

// -- ENGINE --

// -- CLIENT-ENGINE --

typedef enum ttec_subprocess {
  TTEC_SUBPROC_PACKET_RECV,
  _amount_ttec_subprocesses,
} ttec_subprocess_e;

// prefix: ttec
struct tt_engine_client {
  ttb_event_array_t events;
  tt_backend_t *backend;

  tt_subproc_info_t subproc_infos[_amount_ttec_subprocesses];

  tt_asset_manager_t asset_manager;

  tt_client_connection_t client_connection;

  bump_t event_bump;
  allocator_t event_alloc;
};

typedef struct tt_engine_init {
  i32 window_width;
  i32 window_height;
  const char *window_title;
  bool resizeable;
} tt_engine_init_t;

void ttec_init(tt_engine_client_t *engine, tt_engine_init_t init_state);

void ttec_load_assets(tt_engine_client_t *engine);

void ttec_reload_assets(tt_engine_client_t *engine);

void ttec_deinit(tt_engine_client_t *engine);

bool ttec_update(tt_engine_client_t *engine);

bool ttec_connect(tt_engine_client_t *engine, const char *ipaddr, u32 port);

// -- CLIENT-ENGINE-SUBPROCESSES --

bool ttec_subproc_start(tt_engine_client_t *engine, ttec_subprocess_e subproc);

bool ttec_subproc_stop(tt_engine_client_t *engine, ttec_subprocess_e subproc);

// -- SERVER-ENGINE --
// Prefix: ttes_*

typedef enum ttes_subprocess {
  TTES_SUBPROC_PACKET_RECV,
  TTES_SUBPROC_CLIENT_ACCEPT,
  _amount_ttes_subprocesses,
} ttes_subprocess_e;

struct tt_engine_server {
  ttb_event_array_t events;
  tt_backend_t *backend;

  tt_subproc_info_t subproc_infos[_amount_ttes_subprocesses];

  tt_server_t server;
};

void ttes_init(tt_engine_server_t *engine, bool integrated);

void ttes_deinit(tt_engine_server_t *engine);

bool ttes_update(tt_engine_server_t *engine);

bool ttes_server_host(tt_engine_server_t *engine, const char *ipaddr, u32 port);

void ttes_server_stop(tt_engine_server_t *engine);

// -- SERVER-ENGINE-SUBPROCESSES --

bool ttes_subproc_start(tt_engine_server_t *engine, ttes_subprocess_e subproc);

bool ttes_subproc_stop(tt_engine_server_t *engine, ttes_subprocess_e subproc);
