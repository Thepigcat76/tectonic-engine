#include "../include/tt_engine.h"

#include "lilc/alloc.h"
#include "lilc/array.h"
#include "lilc/log.h"
#include "lilsockets.h"

#include "tt_internal_shared.h"

#include <pthread.h>
#include <stdio.h>
#include <sys/cdefs.h>
#include <sys/poll.h>

void ttes_init(tt_engine_server_t *engine, bool integrated_srvr) {
  engine->backend = tt_backend_make(&HEAP_ALLOCATOR, NULL);

  tt_server_init(&engine->server, integrated_srvr);
}

void ttes_deinit(tt_engine_server_t *engine) {
  tt_backend_destroy(engine->backend);

  tt_server_deinit(&engine->server);
}

bool ttes_update(tt_engine_server_t *engine) {
  bool running = false;
  pthread_rwlock_rdlock(&engine->server.server_lock);
  running = engine->server.state != TT_SERVER_STOPPED;
  // TODO: Stop subprocesses
  pthread_rwlock_unlock(&engine->server.server_lock);
  return running;
}

bool ttes_server_host(tt_engine_server_t *engine, const char *ipaddr,
                      u32 port) {
  pthread_rwlock_wrlock(&engine->server.server_lock);

  if (!ttes_subproc_start(engine, TTES_SUBPROC_PACKET_RECV)) {
    log_error("Failed to start packet receiver on the server");
    return 1;
  }

  if (!ttes_subproc_start(engine, TTES_SUBPROC_CLIENT_ACCEPT)) {
    log_error("Failed to start client acceptor on the server");
    return 1;
  }
  engine->server.address = sockets_open_server(ipaddr, port);
  if (engine->server.address == -1) {
    log_error("Failed to open server");
    return false;
  }
  engine->server.state = TT_SERVER_OPEN;
  pthread_rwlock_unlock(&engine->server.server_lock);

  return true;
}

void ttes_server_stop(tt_engine_server_t *engine) {
  pthread_rwlock_wrlock(&engine->server.server_lock);
  engine->server.address = -1;
  engine->server.state = TT_SERVER_STOPPED;

  for (tt_subproc_id_t i = 0; i < _amount_ttes_subprocesses; i++) {
    if (engine->subproc_infos[i].running) {
      ttes_subproc_stop(engine, i);
    }
  }
  pthread_rwlock_unlock(&engine->server.server_lock);
}

void ttes_packet_init(tt_engine_server_t *engine, tt_packet_t *packet,
                      tt_packet_id_t packet_id, void *payload) {
  tt_packet_info_t packet_info = engine->server.packet_infos.infos[packet_id];

  packet->packet_id = packet_info.packet_id;

  pthread_rwlock_wrlock(&engine->server.server_lock);
  packet->payload = engine->server.packet_bump.alloc.alloc(
      &engine->server.packet_bump.alloc, packet_info.payload_size);
  memcpy(packet->payload, payload, packet_info.payload_size);
  pthread_rwlock_unlock(&engine->server.server_lock);
}

bool ttes_packet_pop(tt_engine_server_t *engine, tt_packet_t *packet,
                     tt_packet_context_t *packet_ctx, void *decode_ctx) {
  return tt_packet_queue_pop(
      &engine->server.packet_infos, &engine->server.packet_data_queue,
      &engine->server.packet_queue_mutex, packet, packet_ctx,
      &engine->server.packet_bump.alloc, decode_ctx);
}

// Server rwlock already locked
static tt_connection_status_e
tt_server_handle_client_connection(tt_server_t *server, u64 client_id) {
  tt_client_desc_t client_desc = server->connected_clients[client_id];

  struct tt_connection client_connection = {
      .client_id = client_id,
      .recv_addr = client_desc.address,
      .side = TT_SIDE_SERVER,
      .connection_packets = &server->packet_infos,
      .packet_data_queue = &server->packet_data_queue,
      .packet_queue_mutex = &server->packet_queue_mutex,
  };

  return tt_handle_connection(&client_connection);
}

static void *ttes_packet_receiver_run(void *arg) {
  tt_server_t *server = &((tt_engine_server_t *)arg)->server;

  addr_t server_addr = -1;
  tt_server_state_e server_state = TT_SERVER_RUNNING;

  bump_t temp_bump = {0};
  bump_init(&temp_bump, 4000);

  while (server_state == TT_SERVER_RUNNING || server_state == TT_SERVER_OPEN) {
    pthread_rwlock_rdlock(&server->server_lock);
    server_state = server->state;
    addr_t _server_addr = server->address;
    pthread_rwlock_unlock(&server->server_lock);

    switch (server_state) {
    case TT_SERVER_OPEN: {
      server_addr = _server_addr;
      if (server_addr == -1) {
        log_warn("Server open but has no address, stopping");
        pthread_rwlock_wrlock(&server->server_lock);
        server->state = TT_SERVER_RUNNING;
        pthread_rwlock_unlock(&server->server_lock);
      }

      pthread_mutex_lock(&server->poll_clients_mutex);
      usz server_poll_clients_len = array_len(server->poll_clients);
      i64 poll_res = sockets_server_poll_clients(server->poll_clients,
                                                 server_poll_clients_len, 100);

      i32 error = 0;
      if (!sockets_server_valid_poll(poll_res, &error)) {
        log_error("Failed to poll server");
        continue;
      }

      for (size_t i = 0; i < array_len(server->poll_clients); i++) {
        if (server->poll_clients[i].revents & (POLLIN | POLLRDNORM)) {
          pthread_rwlock_wrlock(&server->server_lock);
          tt_connection_status_e status =
              tt_server_handle_client_connection(server, i);
          switch (status) {
          case TT_CONNECTION_ILLEGAL_PACKET: {
            log_warn("Received illegal packet from: %zu", i);
          } break;
          case TT_CONNECTION_CLOSED: {
            log_info("Client connection closed, stopping");
            server->state = TT_SERVER_STOPPED;
          } break;
          default: {
          } break;
          }
          pthread_rwlock_unlock(&server->server_lock);
        }

        bool client_error =
            server->poll_clients[i].revents & (POLLERR | POLLHUP | POLLNVAL);
        if (client_error) {
          pthread_rwlock_wrlock(&server->server_lock);
          log_error("Client %d disconnected or error\n",
                    server->poll_clients[i].fd);
          sockets_close(server->poll_clients[i].fd);

          tt_client_desc_t client_desc = server->connected_clients[i];
          deque_push_back(server->removed_clients, client_desc);

          array_remove(server->poll_clients, i);
          array_remove(server->connected_clients, i);
          
          pthread_rwlock_unlock(&server->server_lock);
        }
      }

      pthread_mutex_unlock(&server->poll_clients_mutex);

    } break;
    case TT_SERVER_STOPPED: {
      server_addr = -1;
    } break;
    case TT_SERVER_RUNNING:
    default: {
      server_addr = -1;
      tt_wait(100);
    } break;
    }

    bump_reset(&temp_bump);
  }

  bump_free(&temp_bump);

  log_info("Server stopped receiving packets");

  return NULL;
}

static void *ttes_client_acceptor_run(void *arg) {
  tt_server_t *server = &((tt_engine_server_t *)arg)->server;

  addr_t server_addr = -1;
  tt_server_state_e server_state = TT_SERVER_RUNNING;

  while (server_state == TT_SERVER_RUNNING || server_state == TT_SERVER_OPEN) {
    pthread_rwlock_rdlock(&server->server_lock);
    server_state = server->state;
    pthread_rwlock_unlock(&server->server_lock);

    switch (server_state) {
    case TT_SERVER_OPEN: {
      if (server_addr == -1) {
        pthread_rwlock_rdlock(&server->server_lock);
        server_addr = server->address;
        pthread_rwlock_unlock(&server->server_lock);
      }

      i32 res = sockets_server_pending_client(server_addr);
      if (res > 0) {
        addr_t client_addr = sockets_server_accept_client(server_addr);
        pthread_mutex_lock(&server->poll_clients_mutex);

        pthread_rwlock_wrlock(&server->server_lock);
        usz connected_clients = array_len(server->connected_clients);

        tt_client_desc_t client_desc = {
            .client_id = connected_clients,
            .address = client_addr,
        };
        poll_client_t poll_client = {
            .fd = client_addr,
            .events = POLLIN | POLLRDNORM,
        };

        array_add(server->connected_clients, client_desc);
        array_add(server->poll_clients, poll_client);

        pthread_rwlock_unlock(&server->server_lock);

        pthread_mutex_unlock(&server->poll_clients_mutex);
      } else if (res < 0) {
        log_error("Error while checking for pending clients");
      }

    } break;
    case TT_SERVER_STOPPED: {
      server_addr = -1;
    } break;
    case TT_SERVER_RUNNING:
    default: {
      server_addr = -1;
      tt_wait(100);
    } break;
    }
  }

  log_info("Server stopped accepting clients");

  return NULL;
}

bool ttes_subproc_start(tt_engine_server_t *engine, ttes_subprocess_e subproc) {
  void *(*subproc_runner)(void *) = NULL;
  const char *subproc_name = NULL;
  void *arg;
  switch (subproc) {
  case TTES_SUBPROC_PACKET_RECV: {
    subproc_runner = ttes_packet_receiver_run;
    subproc_name = "Server Packet Receiver";
    arg = engine;
  } break;
  case TTES_SUBPROC_CLIENT_ACCEPT: {
    subproc_runner = ttes_client_acceptor_run;
    subproc_name = "Server Client Acceptor";
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
    log_error("Failed to start server subprocess %s", subproc_name);
    return false;
  }

  subproc_info.running = true;

  engine->subproc_infos[subproc] = subproc_info;

  return true;
}

bool ttes_subproc_stop(tt_engine_server_t *engine, ttes_subprocess_e subproc) {
  tt_subproc_info_t *info = &engine->subproc_infos[subproc];

  if (!info->running)
    return false;

  pthread_join(info->thread_id, NULL);

  return true;
}
