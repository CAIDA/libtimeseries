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

typedef struct server {
  /* Identity frame that the server sent us */
  zframe_t *identity;

  /** Printable ID of server (for debugging and logging) */
  char *id;

  /** Time at which the server expires */
  uint64_t expiry;
} server_t;

static void reset_heartbeat_timer(tsmq_broker_t *broker,
				  uint64_t clock)
{
  broker->heartbeat_next = clock + broker->heartbeat_interval;
}

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

static server_t *server_init(tsmq_broker_t *broker, zframe_t *identity)
{
  server_t *server;

  if((server = malloc(sizeof(server_t))) == NULL)
    {
      return NULL;
    }

  server->identity = identity;
  server->id = zframe_strhex(identity);
  server->expiry = zclock_time() +
    (broker->heartbeat_interval * broker->heartbeat_liveness);

  return server;
}

static void server_free(server_t *server)
{
  if(server == NULL)
    {
      return;
    }

  if(server->identity != NULL)
    {
      zframe_destroy(&server->identity);
    }

  if(server->id != NULL)
    {
      free(server->id);
      server->id = NULL;
    }

  free(server);

  return;
}

static int server_ready(zlist_t *servers, server_t *server)
{
  server_t *s;

  /* first we have to see if we already have this server in the list */
  s = zlist_first(servers);
  while(s != NULL)
    {
      if(strcmp(server->id, s->id) == 0)
	{
	  /*fprintf(stderr, "DEBUG: Replacing existing server (%s)\n", s->id);*/
	  zlist_remove(servers, s);
	  server_free(s);
	  break;
	}

      s = zlist_next(servers);
    }

  /* now we add this nice shiny new server to the list */
  return zlist_append(servers, server);
}

static zframe_t *server_next (tsmq_broker_t *broker)
{
  server_t *server = NULL;
  zframe_t *frame = NULL;

  if((server = zlist_pop(broker->servers)) == NULL)
    {
      return NULL;
    }

  frame = server->identity;
  assert(frame != NULL);

  server->identity = NULL; /* otherwise server_free will try and free the id */
  server_free(server);

  return frame;
}

static void servers_purge (tsmq_broker_t *broker)
{
  server_t *server = zlist_first(broker->servers);

  while(server != NULL)
    {
      if(zclock_time () < server->expiry)
	{
	  break; /* Worker is alive, we're done here */
	}

      fprintf(stderr, "DEBUG: Removing dead server (%s)\n", server->id);
      zlist_remove(broker->servers, server);
      server_free(server);
      server = zlist_first(broker->servers);
    }
}

static void servers_free(tsmq_broker_t *broker)
{
  server_t *server;

  if(broker->servers == NULL)
    {
      return;
    }

  server = zlist_first(broker->servers);

  while(server != NULL)
    {
      zlist_remove(broker->servers, server);
      server_free(server);
      server = zlist_first(broker->servers);
    }
  zlist_destroy(&broker->servers);
}

static int handle_heartbeat_timer(zloop_t *loop, int timer_id, void *arg)
{
  tsmq_broker_t *broker = (tsmq_broker_t*)arg;
  uint64_t clock = zclock_time();
  uint8_t msg_type_p;
  server_t *server = NULL;

  /* time for heartbeats */
  assert(broker->heartbeat_next > 0);
  if(clock >= broker->heartbeat_next)
    {
      /** @todo optimize the server list */
      server = zlist_first(broker->servers);

      /* send a heartbeat to each server */
      while(server != NULL)
	{
          /** @todo replace server id with zmq_msg */
	  if(zframe_send(&server->identity, broker->server_socket,
			 ZFRAME_REUSE | ZFRAME_MORE) == -1)
	    {
	      tsmq_set_err(broker->tsmq, errno,
			   "Could not send heartbeat id to server %s",
			   server->id);
	      goto err;
	    }

	  msg_type_p = TSMQ_MSG_TYPE_HEARTBEAT;
	  if(zmq_send(broker->server_socket, &msg_type_p, 1, 0) == -1)
	    {
	      tsmq_set_err(broker->tsmq, errno,
			   "Could not send heartbeat msg to server %s",
			   server->id);
	      goto err;
	    }

	  server = zlist_next(broker->servers);
	}

      reset_heartbeat_timer(broker, clock);
    }

  /* remove dead servers */
  servers_purge(broker);

  return 0;

 err:
  return -1;
}

static int handle_server_msg(zloop_t *loop, zsock_t *reader, void *arg)
{
  tsmq_broker_t *broker = (tsmq_broker_t*)arg;
  tsmq_msg_type_t msg_type;
  zmsg_t *msg = NULL;
  zframe_t *frame = NULL;
  server_t *server = NULL;

  if((msg = zmsg_recv(broker->server_socket)) == NULL)
    {
      goto interrupt;
    }

  /* any kind of message from a server means that it is ready to
     be given a task */
  /* treat the first frame as an identity frame */
  if((frame = zmsg_pop(msg)) == NULL)
    {
      tsmq_set_err(broker->tsmq, TSMQ_ERR_PROTOCOL,
                   "Could not parse response from server");
      goto err;
    }

  /* create state for this server */
  if((server = server_init(broker, frame)) == NULL)
    {
      goto err;
    }
  /* frame is owned by server */

  /* add it to the queue of waiting and eager workers */
  server_ready(broker->servers, server);
  /* server is owned by broker->servers */

  /* now we validate the actual message and perhaps send it back to the
     client */
  msg_type = tsmq_msg_type(msg);

  if(msg_type == TSMQ_MSG_TYPE_READY)
    {
      /*fprintf(stderr, "DEBUG: Adding new server (%s)\n", server->id);*/
      /* ignore these as we already did the work */
      zmsg_destroy(&msg);
    }
  else if(msg_type == TSMQ_MSG_TYPE_HEARTBEAT)
    {
      /*fprintf(stderr, "DEBUG: Got a heartbeat from %s\n", server->id);*/
      /* ignore these */
      zmsg_destroy(&msg);
    }
  else if(msg_type == TSMQ_MSG_TYPE_REPLY)
    {
      /* DEBUG */
      /*fprintf(stderr, "DEBUG: Handing reply to client:\n");*/
      /*zmsg_print(msg);*/


      /* there must be at least two frames for a valid reply:
         1. client address 2. empty (3. reply body) */
      if(zmsg_size(msg) < 2)
        {
          tsmq_set_err(broker->tsmq, TSMQ_ERR_PROTOCOL,
                       "Malformed reply received from server");
          goto err;
        }

      /** @todo can we do more error checking here? */
      /* pass this message along to the client */
      if(zmsg_send(&msg, broker->client_socket) == -1)
        {
          tsmq_set_err(broker->tsmq, errno,
                       "Could not forward server reply to client");
          goto err;
        }

      /* msg is destroyed by zmsg_send */
    }
  else
    {
      tsmq_set_err(broker->tsmq, TSMQ_ERR_PROTOCOL,
                   "Invalid message type (%d) rx'd from server",
                   msg_type);
      goto err;
    }

  return 0;

 err:
  zmsg_destroy(&msg);
  zframe_destroy(&frame);
  return -1;

 interrupt:
  zmsg_destroy(&msg);
  zframe_destroy(&frame);
  tsmq_set_err(broker->tsmq, TSMQ_ERR_INTERRUPT, "Caught SIGINT");
  return -1;
}

static int handle_client_msg(zloop_t *loop, zsock_t *reader, void *arg)
{
  tsmq_broker_t *broker = (tsmq_broker_t*)arg;
  zmsg_t *msg = NULL;
  zframe_t *frame = NULL;

  if((msg = zmsg_recv(broker->client_socket)) == NULL)
    {
      goto interrupt;
    }

  if((frame = server_next(broker)) == NULL)
    {
      tsmq_set_err(broker->tsmq, TSMQ_ERR_PROTOCOL,
                   "No server available for client request");
      goto err;
    }

  if(zmsg_prepend(msg, &frame) != 0)
    {
      tsmq_set_err(broker->tsmq, TSMQ_ERR_MALLOC,
                   "Could not prepend frame to message");
      goto err;
    }

  if(zmsg_send(&msg, broker->server_socket) == -1)
    {
      tsmq_set_err(broker->tsmq, errno,
                   "Could not forward client request to server");
      goto err;
    }

  /* msg is free'd by zmsg_send */
  return 0;

 err:
  zmsg_destroy(&msg);
  zframe_destroy(&frame);
  return -1;

 interrupt:
  zmsg_destroy(&msg);
  zframe_destroy(&frame);
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
  if((broker->servers = zlist_new()) == NULL)
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

  /* seed the time for the next heartbeat sent to servers */
  reset_heartbeat_timer(broker, zclock_time());

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

  assert(broker->tsmq != NULL);

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
