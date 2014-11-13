/*
 * tsmq
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of tsmq.
 *
 * tsmq is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * tsmq is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tsmq.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <assert.h>
#include <stdio.h>

#include "tsmq_broker_int.h"

#include "utils.h"

#define CTX broker->tsmq->ctx

static int handle_server_msg(zloop_t *loop, zsock_t *reader, void *arg);
static int handle_client_msg(zloop_t *loop, zsock_t *reader, void *arg);

struct tsmq_broker_server {

  /* Identity frame that the server sent us */
  zmq_msg_t identity;

  /** Printable ID of server (for debugging and logging) */
  char *id;

  /** Time at which the server expires */
  uint64_t expiry;

};

#if 0
static void reset_heartbeat_timer(tsmq_broker_t *broker,
				  uint64_t clock)
{
  broker->heartbeat_next = clock + broker->heartbeat_interval;
}
#endif

#if 0
static void reset_heartbeat_liveness(tsmq_broker_t *broker)
{
  broker->heartbeat_liveness_remaining = broker->heartbeat_liveness;
}
#endif

static int server_bind(tsmq_broker_t *broker)
{
  /* bind to server socket */
  if((broker->server_socket = zsocket_new(CTX, ZMQ_ROUTER)) == NULL)
    {
      tsmq_set_err(broker->tsmq, TSMQ_ERR_START_FAILED,
		   "Failed to create server socket");
      return -1;
    }

  zsocket_set_router_mandatory(broker->server_socket, 1);

  if(zsocket_bind(broker->server_socket, "%s", broker->server_uri) < 0)
    {
      tsmq_set_err(broker->tsmq, errno, "Could not bind to server socket");
      return -1;
    }

  /* add to reactor */
  if(zloop_reader(broker->loop, broker->server_socket,
                  handle_server_msg, broker) != 0)
    {
      tsmq_set_err(broker->tsmq, TSMQ_ERR_MALLOC,
                   "Could not add server socket to reactor");
      return -1;
    }

  return 0;
}

static int client_bind(tsmq_broker_t *broker)
{
  /* bind to client socket */
  if((broker->client_socket = zsocket_new(CTX, ZMQ_ROUTER)) == NULL)
    {
      tsmq_set_err(broker->tsmq, TSMQ_ERR_START_FAILED,
		   "Failed to create client socket");
      return -1;
    }

  zsocket_set_router_mandatory(broker->client_socket, 1);

  if(zsocket_bind(broker->client_socket, "%s", broker->client_uri) < 0)
    {
      tsmq_set_err(broker->tsmq, errno, "Could not bind to client socket");
      return -1;
    }

  /* add to reactor */
  if(zloop_reader(broker->loop, broker->client_socket,
                  handle_client_msg, broker) != 0)
    {
      tsmq_set_err(broker->tsmq, TSMQ_ERR_MALLOC,
                   "Could not add client socket to reactor");
      return -1;
    }

  return 0;
}

static char *msg_strhex(zmq_msg_t *msg)
{
    assert(msg != NULL);

    static const char hex_char [] = "0123456789ABCDEF";

    size_t size = zmq_msg_size(msg);
    byte *data = zmq_msg_data(msg);
    char *hex_str = (char *) malloc (size * 2 + 1);
    if(hex_str == NULL)
      {
	return NULL;
      }

    uint byte_nbr;
    for (byte_nbr = 0; byte_nbr < size; byte_nbr++) {
        hex_str [byte_nbr * 2 + 0] = hex_char [data [byte_nbr] >> 4];
        hex_str [byte_nbr * 2 + 1] = hex_char [data [byte_nbr] & 15];
    }
    hex_str [size * 2] = 0;
    return hex_str;
}

static void server_free(tsmq_broker_server_t **server_p)
{
  tsmq_broker_server_t *server;

  assert(server_p != NULL);
  server = *server_p;
  *server_p = NULL;

  if(server == NULL)
    {
      return;
    }

  zmq_msg_close(&server->identity);

  free(server->id);
  server->id = NULL;

  free(server);

  return;
}

/* because the hash calls with only the pointer, not the local ref */
static void server_free_wrap(tsmq_broker_server_t *server)
{
  server_free(&server);
}

static tsmq_broker_server_t *server_init(tsmq_broker_t *broker,
                                         zmq_msg_t *identity)
{
  tsmq_broker_server_t *server;
  int khret;
  khiter_t khiter;

  if((server = malloc_zero(sizeof(tsmq_broker_server_t))) == NULL)
    {
      return NULL;
    }

  if(zmq_msg_init(&server->identity) == -1 ||
     zmq_msg_copy(&server->identity, identity) == -1)
    {
      goto err;
    }
  zmq_msg_close(identity);

  server->id = msg_strhex(&server->identity);

  server->expiry = zclock_time() +
    (broker->heartbeat_interval * broker->heartbeat_liveness);

  /** @todo allow multiple servers */
  assert(kh_size(broker->servers) == 0);

  khiter = kh_put(str_server, broker->servers, server->id, &khret);
  if(khret == -1)
    {
      goto err;
    }
  kh_val(broker->servers, khiter) = server;

  return server;

 err:
  server_free(&server);
  return NULL;
}

static tsmq_broker_server_t *server_get(tsmq_broker_t *broker,
                                        zmq_msg_t *identity)
{
  tsmq_broker_server_t *server;
  khiter_t khiter;
  char *id;

  if((id = msg_strhex(identity)) == NULL)
    {
      return NULL;
    }

  if((khiter =
      kh_get(str_server, broker->servers, id)) == kh_end(broker->servers))
    {
      free(id);
      return NULL;
    }

  server = kh_val(broker->servers, khiter);
  /* we are already tracking this server, treat the msg as a heartbeat */
  /* touch the timeout */
  server->expiry = zclock_time() +
    (broker->heartbeat_interval * broker->heartbeat_liveness);
  free(id);
  return server;
}

static int server_send_headers(tsmq_broker_t *broker,
                               tsmq_broker_server_t *server,
                               tsmq_msg_type_t msg_type,
                               int sndmore)
{
  zmq_msg_t id_cpy;

  if(zmq_msg_init(&id_cpy) == -1 ||
     zmq_msg_copy(&id_cpy, &server->identity) == -1)
    {
      tsmq_set_err(broker->tsmq, TSMQ_ERR_MALLOC,
                   "Failed to duplicate server id");
      return -1;
    }
  if(zmq_msg_send(&id_cpy, broker->server_socket, ZMQ_SNDMORE) == -1)
    {
      zmq_msg_close(&id_cpy);
      tsmq_set_err(broker->tsmq, errno,
                   "Could not send server id to server %s",
                   server->id);
      return -1;
    }

  if(zmq_send(broker->server_socket, &msg_type, tsmq_msg_type_size_t,
              (sndmore != 0) ? ZMQ_SNDMORE : 0) == -1)
    {
      tsmq_set_err(broker->tsmq, errno,
                   "Could not send msg type (%d) to server %s",
                   msg_type, server->id);
      return -1;
    }

  return 0;
}

#if 0
static void server_remove(tsmq_broker_t *broker,
                          tsmq_broker_server_t *server)
{
  khiter_t khiter;
  if((khiter = kh_get(str_server, broker->servers, server->id))
     == kh_end(broker->servers))
    {
      /* already removed? */
      fprintf(stderr, "WARN: Removing non-existent server\n");
      return;
    }

  kh_del(str_server, broker->servers, khiter);
}
#endif

static int servers_purge(tsmq_broker_t *broker)
{
  khiter_t k;
  tsmq_broker_server_t *server;
  uint64_t clock = zclock_time();

  for(k = kh_begin(broker->servers); k != kh_end(broker->servers); ++k)
    {
      if(kh_exist(broker->servers, k) != 0)
	{
	  server = kh_val(broker->servers, k);

	  if(clock < server->expiry)
	    {
	      break; /* server is alive, we're done here */
	    }

	  fprintf(stderr, "INFO: Removing dead server (%s)\n", server->id);
	  fprintf(stderr, "INFO: Expiry: %"PRIu64" Time: %"PRIu64"\n",
		  server->expiry, clock);

	  /* the key string is actually owned by the server, dont free */
	  server_free(&server);
	  kh_del(str_server, broker->servers, k);
	}
    }

  return 0;
}

static void servers_free(tsmq_broker_t *broker)
{
  assert(broker != NULL);
  assert(broker->servers != NULL);

  kh_free_vals(str_server, broker->servers, server_free_wrap);
  kh_destroy(str_server, broker->servers);
  broker->servers = NULL;
}

static int handle_heartbeat_timer(zloop_t *loop, int timer_id, void *arg)
{
  tsmq_broker_t *broker = (tsmq_broker_t*)arg;
  tsmq_broker_server_t *server = NULL;
  khiter_t k;

  for(k = kh_begin(broker->servers); k != kh_end(broker->servers); ++k)
    {
      if(kh_exist(broker->servers, k) == 0)
        {
          continue;
        }

      server = kh_val(broker->servers, k);

      if(server_send_headers(broker, server, TSMQ_MSG_TYPE_HEARTBEAT, 0) != 0)
        {
          goto err;
        }
    }

  /* remove dead servers */
  return servers_purge(broker);

 err:
  return -1;
}

static int handle_server_msg(zloop_t *loop, zsock_t *reader, void *arg)
{
  tsmq_broker_t *broker = (tsmq_broker_t*)arg;
  tsmq_msg_type_t msg_type;

  zmq_msg_t server_id;
  zmq_msg_t proxy;
  tsmq_broker_server_t *server = NULL;
  int more = 0;

  /* get the server id frame */
  if(zmq_msg_init(&server_id) == -1)
    {
      tsmq_set_err(broker->tsmq, TSMQ_ERR_MALLOC,
                   "Failed to init msg");
      goto err;
    }

  if(zmq_msg_recv(&server_id, broker->server_socket, 0) == -1)
    {
      switch(errno)
	{
	case ETERM:
	case EINTR:
	  goto interrupt;
	  break;

	default:
	  tsmq_set_err(broker->tsmq, errno, "Could not recv from server");
	  goto err;
	  break;
	}
    }

  /* there has gotta be more to this message */
  if(zsocket_rcvmore(broker->server_socket) == 0)
    {
      tsmq_set_err(broker->tsmq, TSMQ_ERR_PROTOCOL,
                   "Invalid message received from server "
                   "(missing type)");
      goto err;
    }

  /* now, what is the message all about? */
  msg_type = tsmq_recv_type(broker->server_socket);

  /* check if this server is already registered */
  if((server = server_get(broker, &server_id)) == NULL)
    {
      if(msg_type == TSMQ_MSG_TYPE_READY)
	{
	  /* create state for this client */
	  if((server = server_init(broker, &server_id)) == NULL)
	    {
	      goto err;
	    }
	}
      else
	{
	  /* somehow the server state was lost but the server didn't
	     reconnect (i.e. send READY) */
	  tsmq_set_err(broker->tsmq, TSMQ_ERR_PROTOCOL,
                       "Unknown server found");
	  goto err;
	}
    }

  /* by here we have a server object and it is time to handle whatever message
     we were sent */
  switch(msg_type)
    {
    case TSMQ_MSG_TYPE_READY:
    case TSMQ_MSG_TYPE_HEARTBEAT:
      /* nothing, we already created state for this server */
      break;

    case TSMQ_MSG_TYPE_REPLY:
      if(zsocket_rcvmore(broker->server_socket) == 0)
        {
          tsmq_set_err(broker->tsmq, TSMQ_ERR_PROTOCOL,
                       "Empty received from server");
          goto err;
        }
      /* simply rx/tx the rest of the message */
      while(1)
        {
          if(zmq_msg_init(&proxy) != 0)
            {
              tsmq_set_err(broker->tsmq, TSMQ_ERR_MALLOC,
                           "Could not create proxy message");
              goto err;
            }
          if(zmq_msg_recv(&proxy, broker->server_socket, 0) == -1)
            {
              goto interrupt;
            }
          more = zsocket_rcvmore(broker->server_socket) != 0 ? ZMQ_SNDMORE : 0;
          /* tx the message */
          if(zmq_msg_send(&proxy, broker->client_socket, more) == -1)
            {
              zmq_msg_close(&proxy);
              tsmq_set_err(broker->tsmq, errno, "Could not send reply message");
              goto err;
            }
          if(more == 0)
            {
              break;
            }
        }
      break;

    default:
      tsmq_set_err(broker->tsmq, TSMQ_ERR_PROTOCOL,
                   "Invalid message type (%d) rx'd from server",
                   msg_type);
      goto err;
    }

  return 0;

 err:
  return -1;

 interrupt:
  /* we were interrupted */
  tsmq_set_err(broker->tsmq, TSMQ_ERR_INTERRUPT, "Caught SIGINT");
  return -1;
}

static int handle_client_msg(zloop_t *loop, zsock_t *reader, void *arg)
{
  tsmq_broker_t *broker = (tsmq_broker_t*)arg;
  tsmq_broker_server_t *server = NULL;
  khiter_t k;
  zmq_msg_t proxy;
  int more = 0;

  /** @todo handle no servers (queue) */

  /** @todo this is all HAX, FIXME */

  /* grab the first (only) connected server */
  for(k = kh_begin(broker->servers); k != kh_end(broker->servers); ++k)
    {
      if(kh_exist(broker->servers, k) == 0)
        {
          continue;
        }
      server = kh_val(broker->servers, k);
      break;
    }

  /* stuff will BREAK if there are no servers connected */
  assert(server != NULL);

  /* send the server id frame */
  if(server_send_headers(broker, server, TSMQ_MSG_TYPE_REQUEST, ZMQ_SNDMORE) != 0)
    {
      goto err;
    }

  while(1)
    {
      if(zmq_msg_init(&proxy) != 0)
        {
          tsmq_set_err(broker->tsmq, TSMQ_ERR_MALLOC,
                       "Could not create proxy message");
          goto err;
        }
      if(zmq_msg_recv(&proxy, broker->client_socket, 0) == -1)
        {
          goto interrupt;
        }
      more = zsocket_rcvmore(broker->client_socket) != 0 ? ZMQ_SNDMORE : 0;

      /** @todo for key lookup... somehow pick a server */
      /** @todo for key set, just pub it! */

      /* tx the message */
      if(zmq_msg_send(&proxy, broker->server_socket, more) == -1)
        {
          zmq_msg_close(&proxy);
          tsmq_set_err(broker->tsmq, errno,
                       "Could not send message to server");
          goto err;
        }

      if(more == 0)
        {
          break;
        }
    }

  return 0;

 err:
  zmq_msg_close(&proxy);
  return -1;

 interrupt:
  tsmq_set_err(broker->tsmq, TSMQ_ERR_INTERRUPT, "Caught SIGINT");
  return -1;
}

static int init_reactor(tsmq_broker_t *broker)
{
  /* set up the reactor */
  if((broker->loop = zloop_new()) == NULL)
    {
      tsmq_set_err(broker->tsmq, TSMQ_ERR_INIT_FAILED,
                   "Could not initialize reactor");
      return -1;
    }

  /* add a heartbeat timer */
  if((broker->timer_id = zloop_timer(broker->loop,
                                     broker->heartbeat_interval, 0,
                                     handle_heartbeat_timer, broker)) < 0)
    {
      tsmq_set_err(broker->tsmq, TSMQ_ERR_MALLOC,
                   "Could not add heartbeat timer reactor");
      return -1;
    }

  /* server and client sockets will be added by _start */

  return 0;
}

/* ---------- PUBLIC FUNCTIONS BELOW HERE ---------- */

TSMQ_ERR_FUNCS(broker)

tsmq_broker_t *tsmq_broker_init()
{
  tsmq_broker_t *broker;
  if((broker = malloc_zero(sizeof(tsmq_broker_t))) == NULL)
    {
      /* cannot set an err at this point */
      return NULL;
    }

  if((broker->tsmq = tsmq_init()) == NULL)
    {
      /* still cannot set an error */
      goto err;
    }

  /* now we are ready to set errors... */

  broker->client_uri = strdup(TSMQ_BROKER_CLIENT_URI_DEFAULT);

  broker->server_uri = strdup(TSMQ_BROKER_SERVER_URI_DEFAULT);

  broker->heartbeat_interval = TSMQ_HEARTBEAT_INTERVAL_DEFAULT;

  broker->heartbeat_liveness = TSMQ_HEARTBEAT_LIVENESS_DEFAULT;

  /* establish an empty server list */
  if((broker->servers = kh_init(str_server)) == NULL)
    {
      tsmq_set_err(broker->tsmq, TSMQ_ERR_INIT_FAILED,
		   "Could not create server list");
      goto err;
    }

  /* initialize the reactor */
  if(init_reactor(broker) != 0)
    {
      goto err;
    }

  return broker;

 err:
  tsmq_broker_free(&broker);
  return NULL;
}

int tsmq_broker_start(tsmq_broker_t *broker)
{

  if(server_bind(broker) != 0)
    {
      return -1;
    }

  if(client_bind(broker) != 0)
    {
      return -1;
    }

  /* start processing requests */
  zloop_start(broker->loop);

  return 0;
}

void tsmq_broker_free(tsmq_broker_t **broker_p)
{
  assert(broker_p != NULL);
  tsmq_broker_t *broker;

  broker = *broker_p;
  if(broker == NULL)
    {
      return;
    }
  *broker_p = NULL;

  zloop_destroy(&broker->loop);

  free(broker->client_uri);
  broker->client_uri = NULL;

  /* free'd by tsmq_free */
  broker->client_socket = NULL;

  free(broker->server_uri);
  broker->server_uri = NULL;

  /* free'd by tsmq_free */
  broker->server_socket = NULL;

  servers_free(broker);

  /* will call zctx_destroy which will destroy our sockets too */
  assert(broker->tsmq != NULL);
  tsmq_free(broker->tsmq);
  free(broker);

  return;
}

void tsmq_broker_set_client_uri(tsmq_broker_t *broker, const char *uri)
{
  assert(broker != NULL);

  /* remember, we set one by default */
  assert(broker->client_uri != NULL);
  free(broker->client_uri);

  broker->client_uri = strdup(uri);
}

void tsmq_broker_set_server_uri(tsmq_broker_t *broker, const char *uri)
{
  assert(broker != NULL);

  /* remember, we set one by default */
  assert(broker->server_uri != NULL);
  free(broker->server_uri);

  broker->server_uri = strdup(uri);
}

void tsmq_broker_set_heartbeat_interval(tsmq_broker_t *broker,
					uint64_t interval_ms)
{
  assert(broker != NULL);

  broker->heartbeat_interval = interval_ms;
}

void tsmq_broker_set_heartbeat_liveness(tsmq_broker_t *broker,
					int beats)
{
  assert(broker != NULL);

  broker->heartbeat_liveness = beats;
}
