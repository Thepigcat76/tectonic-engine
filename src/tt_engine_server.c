#include "../include/tt_engine.h"
#include "lilc/array.h"
#include "lilc/deque.h"
#include "lilc/log.h"
#include "lilsockets.h"
#include <pthread.h>
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

bool ttes_update(tt_engine_server_t *engine) { return true; }

bool ttes_server_host(tt_engine_server_t *engine, const char *ipaddr,
                      u32 port) {
  pthread_rwlock_wrlock(&engine->server.server_lock);
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
  pthread_rwlock_unlock(&engine->server.server_lock);
}

extern bool tt_packet_queue_pop(tt_packet_info_array_t *packet_infos, tt_byte_buf_t *packet_queue,
                         pthread_mutex_t *packet_queue_mutex,
                         tt_packet_t *out_packet, allocator_t *packet_alloc,
                         void *decode_ctx);

void ttes_packet_init(tt_engine_server_t *engine, tt_packet_t *packet, tt_packet_handle_t packet_handle, void *payload) {
  tt_packet_info_t packet_info = engine->server.packet_infos.infos[packet_handle];

  packet->packet_handle = packet_handle;
  packet->packet_id = packet_info.packet_id;

  pthread_rwlock_wrlock(&engine->server.server_lock);
  packet->payload = engine->server.packet_alloc.alloc(&engine->server.packet_alloc, packet_info.payload_size);
  pthread_rwlock_unlock(&engine->server.server_lock);
}

bool ttes_packet_pop(tt_engine_server_t *engine, tt_packet_t *packet,
                     void *decode_ctx) {
  return tt_packet_queue_pop(&engine->server.packet_infos, engine->server.packet_data_queue,
                             &engine->server.packet_queue_mutex, packet,
                             &engine->server.packet_alloc, decode_ctx);
}

// Server rwlock already locked
static bool tt_server_handle_client_connection(tt_server_t *server,
                                               u64 client_id) {
  u8 len_buf[8];
  i64 len_res = sockets_receive(client_id, len_buf, 8);
  if (len_res != 4) {
    return false;
  }

  u32 len = (len_buf[0] << 24) | (len_buf[1] << 16) | (len_buf[2] << 8) |
            (len_buf[3] << 0);

  tt_byte_buf_t bytebuf = {0};
  tt_byte_buf_init(&bytebuf, &server->packet_alloc, len);

  i64 payload_res = sockets_receive(client_id, bytebuf.bytes, len);
  if (payload_res == -1) {
    return false;
  }

  pthread_mutex_lock(&server->packet_queue_mutex);
  deque_push_back(server->packet_data_queue, bytebuf);
  pthread_mutex_unlock(&server->packet_queue_mutex);

  return true;
}

static void *ttes_packet_receiver_run(void *arg) {
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

      pthread_rwlock_rdlock(&server->server_lock);

      // do stuff ...
      i64 poll_res = sockets_server_poll_clients(
          server->poll_clients, array_len(server->poll_clients), 100);

      i32 error = 0;
      if (!sockets_server_valid_poll(poll_res, &error)) {
        log_error("Failed to poll server");
        continue;
      }

      for (size_t i = 0; i < array_len(server->poll_clients); i++) {
        if (server->poll_clients[i].revents & (POLLIN | POLLRDNORM)) {
          if (!tt_server_handle_client_connection(server, i)) {
            log_error("Failed to handle client connection");
          }
        }

        if (server->poll_clients[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
          log_error("Client %d disconnected or error\n",
                    server->poll_clients[i].fd);
          sockets_close(server->poll_clients[i].fd);
          // TODO: You may want to mark the player slot as disconnected
          array_remove(server->poll_clients, i);
          array_remove(server->connected_clients, i);
        }
        server_addr = server->address;
      }

      pthread_rwlock_unlock(&server->server_lock);

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

        pthread_rwlock_wrlock(&server->server_lock);

        tt_client_desc_t client_desc = {
            .client_id = array_len(server->connected_clients),
            .address = client_addr,
        };
        poll_client_t poll_client = {
            .fd = client_addr,
            .events = POLLIN | POLLRDNORM,
        };

        array_add(server->connected_clients, client_desc);
        array_add(server->poll_clients, poll_client);

        pthread_rwlock_unlock(&server->server_lock);
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
