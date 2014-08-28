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

#include "tsmq_int.h"
#include "tsmq_md_broker.h"

#include "utils.h"

#define CTX broker->tsmq->ctx

#if 0
#define POP_EMPTY					\
  do {							\
  empty = zmsg_pop(msg);				\
  assert(empty != NULL && zframe_size(empty == 0));	\
  zframe_destroy(&empty);				\
  } while(0)
#endif

enum {
  POLL_ITEM_SERVER = 0,
  POLL_ITEM_CLIENT = 1,
  POLL_ITEM_CNT    = 2,
};

typedef struct server {
  /* Identity frame that the server sent us */
  zframe_t *identity;

  /** Printable ID of server (for debugging and logging) */
  char *id;

  /** Time at which the server expires */
  uint64_t expiry;
} server_t;

static server_t *server_init(tsmq_md_broker_t *broker, zframe_t *identity)
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
	  fprintf(stderr, "DEBUG: Replacing existing server (%s)\n", s->id);
	  zlist_remove(servers, s);
	  server_free(s);
	  break;
	}

      s = zlist_next(servers);
    }

  /* now we add this nice shiny new server to the list */
  return zlist_append(servers, server);
}

static zframe_t *server_next (tsmq_md_broker_t *broker)
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

static void servers_purge (tsmq_md_broker_t *broker)
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

/** @todo consider changing many of these return -1 to return 0 as they are not
    fatal. Be sure to clean up correctly however */
static int run_broker(tsmq_md_broker_t *broker)
{
  zmq_pollitem_t poll_items [] = {
    {broker->server_socket, 0, ZMQ_POLLIN, 0}, /* POLL_ITEM_SERVER */
    {broker->client_socket, 0, ZMQ_POLLIN, 0}, /* POLL_ITEM_CLIENT */
  };
  int poll_items_cnt = POLL_ITEM_CNT;
  int rc;

  zmsg_t *msg = NULL;
  zframe_t *frame = NULL;

  server_t *server = NULL;

  tsmq_msg_type_t msg_type;
  uint8_t msg_type_p;

  /*fprintf(stderr, "DEBUG: Beginning loop cycle\n");*/

  /* poll for client requests only if we have servers to handle them */
  if(zlist_size(broker->servers) == 0)
    {
      poll_items_cnt = 1;
    }

  if((rc = zmq_poll(poll_items, poll_items_cnt,
		    broker->heartbeat_interval * ZMQ_POLL_MSEC)) == -1)
    {
      goto interrupt;
    }

  /* handle message from a server */
  if(poll_items[POLL_ITEM_SERVER].revents & ZMQ_POLLIN)
    {
      if((msg = zmsg_recv(broker->server_socket)) == NULL)
	{
	  goto interrupt;
	}

      /*zmsg_dump(msg);*/

      /* any kind of message from a server means that it is ready to
	 be given a task */
      /* treat the first frame as an identity frame */
      if((frame = zmsg_pop(msg)) == NULL)
	{
	  tsmq_set_err(broker->tsmq, TSMQ_ERR_PROTOCOL,
		       "Could not parse response from server");
	  return -1;
	}

      /* create state for this server */
      if((server = server_init(broker, frame)) == NULL)
	{
	  return -1;
	}

      /* add it to the queue of waiting and eager workers */
      server_ready(broker->servers, server);

      /* now we validate the actual message and perhaps send it back to the
	 client */
      msg_type = tsmq_msg_type(msg);

      if(msg_type == TSMQ_MSG_TYPE_READY)
	{
	  fprintf(stderr, "DEBUG: Adding new server (%s)\n", server->id);
	  /* ignore these */
	}
      else if(msg_type == TSMQ_MSG_TYPE_HEARTBEAT)
	{
	  fprintf(stderr, "DEBUG: Got a heartbeat from %s\n", server->id);
	  /* ignore these */
	}
      else if(msg_type == TSMQ_MSG_TYPE_REPLY)
	{
	  /* there must be at least two frames for a valid reply:
	     1. client address 2. empty (3. reply body) */
	  if(zmsg_size(msg) < 2)
	    {
	      tsmq_set_err(broker->tsmq, TSMQ_ERR_PROTOCOL,
			   "Malformed reply received from server");
	      return -1;
	    }
	  /** @todo can we do more error checking here? */
	  /* pass this message along to the client */
	  if(zmsg_send(&msg, broker->client_socket) == -1)
	    {
	      tsmq_set_err(broker->tsmq, errno,
			   "Could not forward server reply to client");
	      return -1;
	    }
	}
      else
	{
	  tsmq_set_err(broker->tsmq, TSMQ_ERR_PROTOCOL,
		       "Invalid message type (%d) rx'd from server",
		       msg_type);
	  return -1;
	}

    }

  /* get next client request, route to next server */
  if(poll_items[POLL_ITEM_CLIENT].revents & ZMQ_POLLIN)
    {
      if((msg = zmsg_recv(broker->client_socket)) == NULL)
	{
	  goto interrupt;
	}

      if((frame = server_next(broker)) == NULL)
	{
	  tsmq_set_err(broker->tsmq, TSMQ_ERR_PROTOCOL,
		       "No server available for client request");
	  return -1;
	}

      if(zmsg_prepend(msg, &frame) != 0)
	{
	  tsmq_set_err(broker->tsmq, TSMQ_ERR_MALLOC,
		       "Could not prepend frame to message");
	  return -1;
	}

      if(zmsg_send(&msg, broker->server_socket) == -1)
	{
	  tsmq_set_err(broker->tsmq, errno,
		       "Could not forward client request to server");
	  return -1;
	}
    }

  /* time for heartbeats */
  assert(broker->heartbeat_next > 0);
  if(zclock_time() >= broker->heartbeat_next)
    {
      server = zlist_first(broker->servers);

      while(server != NULL)
	{
	  if(zframe_send(&server->identity, broker->server_socket,
			 ZFRAME_REUSE | ZFRAME_MORE) == -1)
	    {
	      tsmq_set_err(broker->tsmq, errno,
			   "Could not send heartbeat id to server %s",
			   server->id);
	      return -1;
	    }

	  msg_type_p = TSMQ_MSG_TYPE_HEARTBEAT;
	  if((frame = zframe_new(&msg_type_p, 1)) == NULL)
	    {
	      tsmq_set_err(broker->tsmq, TSMQ_ERR_MALLOC,
			   "Could not create new heartbeat frame");
	      return -1;
	    }

	  if(zframe_send(&frame, broker->server_socket, 0) == -1)
	    {
	      tsmq_set_err(broker->tsmq, errno,
			   "Could not send heartbeat msg to server %s",
			   server->id);
	      return -1;
	    }

	  server = zlist_next(broker->servers);
	}
      broker->heartbeat_next = zclock_time() + broker->heartbeat_interval;
    }
  servers_purge(broker);

  return 0;

 interrupt:
  /* we were interrupted */
  tsmq_set_err(broker->tsmq, TSMQ_ERR_INTERRUPT, "Caught SIGINT");
  return -1;
}

/* ---------- PUBLIC FUNCTIONS BELOW HERE ---------- */

TSMQ_ERR_FUNCS(md_broker)

tsmq_md_broker_t *tsmq_md_broker_init()
{
  tsmq_md_broker_t *broker;
  if((broker = malloc_zero(sizeof(tsmq_md_broker_t))) == NULL)
    {
      /* cannot set an err at this point */
      return NULL;
    }

  if((broker->tsmq = tsmq_init()) == NULL)
    {
      /* still cannot set an error */
      return NULL;
    }

  /* now we are ready to set errors... */

  broker->client_uri = strdup(TSMQ_MD_BROKER_CLIENT_URI_DEFAULT);

  broker->server_uri = strdup(TSMQ_MD_BROKER_SERVER_URI_DEFAULT);

  broker->heartbeat_interval = TSMQ_MD_HEARTBEAT_INTERVAL_DEFAULT;

  broker->heartbeat_liveness = TSMQ_MD_HEARTBEAT_LIVENESS_DEFAULT;

  /* establish an empty server list */
  if((broker->servers = zlist_new()) == NULL)
    {
      tsmq_set_err(broker->tsmq, TSMQ_ERR_INIT_FAILED,
		   "Could not create server list");
      tsmq_md_broker_free(broker);
      return NULL;
    }

  return broker;
}

int tsmq_md_broker_start(tsmq_md_broker_t *broker)
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

  /* seed the time for the next heartbeat sent to servers */
  broker->heartbeat_next = zclock_time() + broker->heartbeat_interval;

  /* start processing requests */
  while(run_broker(broker) == 0)

  tsmq_set_err(broker->tsmq, TSMQ_ERR_UNHANDLED, "Unhandled error");
  return -1;
}

void tsmq_md_broker_free(tsmq_md_broker_t *broker)
{
  assert(broker != NULL);
  assert(broker->tsmq != NULL);

  if(broker->client_uri != NULL)
    {
      free(broker->client_uri);
      broker->client_uri = NULL;
    }

  /* free'd by tsmq_free */
  broker->client_socket = NULL;

  if(broker->server_uri != NULL)
    {
      free(broker->server_uri);
      broker->server_uri = NULL;
    }

  /* free'd by tsmq_free */
  broker->server_socket = NULL;

  if(broker->servers != NULL)
    {
      zlist_destroy(&broker->servers);
    }

  /* will call zctx_destroy which will destroy our sockets too */
  tsmq_free(broker->tsmq);
  free(broker);

  return;
}

void tsmq_md_broker_set_client_uri(tsmq_md_broker_t *broker, const char *uri)
{
  assert(broker != NULL);

  /* remember, we set one by default */
  assert(broker->cient_uri != NULL);
  free(broker->client_uri);

  broker->client_uri = strdup(uri);
}

void tsmq_md_broker_set_server_uri(tsmq_md_broker_t *broker, const char *uri)
{
  assert(broker != NULL);

  /* remember, we set one by default */
  assert(broker->server_uri != NULL);
  free(broker->server_uri);

  broker->server_uri = strdup(uri);
}

void tsmq_md_broker_set_heartbeat_interval(tsmq_md_broker_t *broker,
					   uint64_t interval_ms)
{
  assert(broker != NULL);

  broker->heartbeat_interval = interval_ms;
}

void tsmq_md_broker_set_heartbeat_liveness(tsmq_md_broker_t *broker,
					   int beats)
{
  assert(broker != NULL);

  broker->heartbeat_liveness = beats;
}
