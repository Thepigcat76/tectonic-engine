#include "../include/tt_engine.h"
#include "lilc/alloc.h"
#include "lilc/array.h"
#include "lilc/deque.h"
#include "lilc/log.h"
#include <lilsockets.h>
#include <pthread.h>
#include <raylib.h>
#include <stdio.h>

void ttec_init(tt_engine_client_t *engine, tt_engine_init_t init_state) {
  engine->backend = tt_backend_make(&HEAP_ALLOCATOR, &engine->asset_manager);

  bump_init(&engine->event_bump, 16000);
  bump_allocator_init(&engine->event_alloc, &engine->event_bump);

  engine->events = array_new(ttb_event_t, &engine->event_alloc);

  SetTargetFPS(60);

  u32 window_flags = 0;

  const char *window_title = init_state.window_title;
  if (window_title == NULL) {
    window_title = "Tectonic Engine Example";
  }

  i32 window_width = init_state.window_width;
  if (window_width == 0) {
    window_width = 640;
  }

  i32 window_height = init_state.window_height;
  if (window_height == 0) {
    window_height = 480;
  }

  if (init_state.resizeable) {
    window_flags |= FLAG_WINDOW_RESIZABLE;
  }

  if (window_flags != 0) {
    SetConfigFlags(window_flags);
  }
  InitWindow(window_width, window_height, window_title);

  engine->asset_manager.asset_handles = NULL;

  engine->asset_manager.backend = engine->backend;

  tt_client_connection_init(&engine->client_connection);
}

void ttec_load_assets(tt_engine_client_t *engine) {
  tt_assets_load(&engine->asset_manager);
}

void ttec_reload_assets(tt_engine_client_t *engine) {
  tt_assets_unload(&engine->asset_manager, false);

  tt_assets_load(&engine->asset_manager);
}

void ttec_deinit(tt_engine_client_t *engine) {
  for (ttec_subprocess_e i = 0; i < _amount_ttec_subprocesses; i++) {
    if (engine->subproc_infos[i].running) {
      ttec_subproc_stop(engine, i);
    }
  }

  CloseWindow();

  tt_assets_unload(&engine->asset_manager, true);

  engine->asset_manager.asset_handles = NULL;

  bump_free(&engine->event_bump);

  tt_backend_destroy(engine->backend);
}

bool ttec_update(tt_engine_client_t *engine) {
  _ttb_cmds_render(engine->backend);

  return true;
}

bool ttec_connect(tt_engine_client_t *engine, const char *ipaddr, u32 port) {
  addr_t server_addr = sockets_connect_to_server(ipaddr, port);
  if (server_addr == -1) {
    return false;
  }

  pthread_rwlock_wrlock(&engine->client_connection.connection_rwlock);
  engine->client_connection.server_addr = server_addr;
  pthread_rwlock_unlock(&engine->client_connection.connection_rwlock);

  pthread_mutex_lock(&engine->client_connection.connected_mutex);
  engine->client_connection.connected = true;
  pthread_cond_signal(&engine->client_connection.connected_cond);
  pthread_mutex_unlock(&engine->client_connection.connected_mutex);

  return false;
}

static void ttec_packet_recv(tt_client_connection_t *connection, addr_t server_addr, tt_packet_t *packet) {
  packet->packet_id = -1;
}

static void *ttec_packet_receiver_run(void *arg) {
  tt_engine_client_t *engine_client = arg;
  tt_client_connection_t *connection = &engine_client->client_connection;

  addr_t server_addr = -1;

  bool running = true;
  pthread_rwlock_rdlock(&connection->connection_rwlock);
  {
    running = connection->client_running;
    bool connected = false;
    while (!connected && connection->client_running) {
      pthread_mutex_lock(&connection->connected_mutex);
      log_info("Waiting for server connection...");
      pthread_cond_wait(&connection->connected_cond, &connection->connected_mutex);
      connected = connection->connected;
      pthread_mutex_unlock(&connection->connected_mutex);
    }

    server_addr = connection->server_addr;
  }
  pthread_rwlock_unlock(&connection->connection_rwlock);

  // Listen for packets
  while (running) {
    log_debug("Listening for packets");
    tt_packet_t packet = {0};
    log_debug("Packet ptr: %p", packet.payload);
    ttec_packet_recv(connection, server_addr, &packet);
    log_debug("Packet: %zu", packet.packet_id);

    if (packet.packet_id == -1) {
      perror("Error packet on client");
      exit(1);
    }

    pthread_rwlock_rdlock(&connection->connection_rwlock);
    {
      log_debug("Adding packet to queue");
      deque_push_back(connection->packet_queue, packet);
      running = connection->client_running;
    }
    pthread_rwlock_unlock(&connection->connection_rwlock);
  }

  return NULL;
}

bool ttec_subproc_stop(tt_engine_client_t *engine, ttec_subprocess_e subproc) {
  tt_subproc_info_t *info = &engine->subproc_infos[subproc];

  if (!info->running)
    return false;

  pthread_join(info->thread_id, NULL);

  return true;
}

bool ttec_subproc_start(tt_engine_client_t *engine, ttec_subprocess_e subproc) {
  void *(*subproc_runner)(void *) = NULL;
  const char *subproc_name = NULL;
  void *arg = NULL;
  switch (subproc) {
  case TTEC_SUBPROC_PACKET_RECV: {
    subproc_runner = ttec_packet_receiver_run;
    subproc_name = "Client Packet Receiver";
    arg = engine;
  } break;
  default: {
    log_error("Handling for subproc %d not implemented yet", subproc);
    return false;
  } break;
  }

  if (engine->subproc_infos[subproc].running)
    return false;

  tt_subproc_info_t subproc_info = {
      .subproc_name = subproc_name,
  };

  if (pthread_create(&subproc_info.thread_id, NULL, subproc_runner, arg)) {
    log_error("Failed to start client subprocess %s", subproc_name);
    return false;
  }

  subproc_info.running = true;

  engine->subproc_infos[subproc] = subproc_info;

  return true;
}
