#include "../include/tt_engine.h"
#include "lilc/array.h"
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
      i64 poll_res = sockets_server_poll_clients(server->poll_clients,
                                                 array_len(server->poll_clients), 100);

      i32 error = 0;
      if (!sockets_server_valid_poll(poll_res, &error)) {
        log_error("Failed to poll server");
        continue;
      }

      for (size_t i = 0; i < array_len(server->poll_clients); i++) {
        if (server->poll_clients[i].revents & (POLLIN | POLLRDNORM)) {
          int client_fd = server->poll_clients[i].fd;

          server_handle_client_connection(client_fd);
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
        pthread_rwlock_unlock(&server->server_lock);
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
  switch (subproc) {
  case TTES_SUBPROC_PACKET_RECV: {
    subproc_runner = ttes_packet_receiver_run;
    subproc_name = "Server Packet Receiver";
  } break;
  case TTES_SUBPROC_CLIENT_ACCEPT: {
    subproc_runner = ttes_client_acceptor_run;
    subproc_name = "Server Client Acceptor";
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

  if (pthread_create(&subproc_info.thread_id, NULL, subproc_runner, NULL)) {
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
