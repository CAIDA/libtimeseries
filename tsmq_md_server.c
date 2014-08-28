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
#include <string.h>

#include "tsmq_int.h"
#include "tsmq_md_server.h"

#include "utils.h"

#define CTX server->tsmq->ctx

enum {
  POLL_ITEM_BROKER = 0,
  POLL_ITEM_CNT    = 1,
};

static int server_connect(tsmq_md_server_t *server)
{
  uint8_t msg_type_p;
  zframe_t *frame;

  /* connect to server socket */
  if((server->broker_socket = zsocket_new(CTX, ZMQ_DEALER)) == NULL)
    {
      tsmq_set_err(server->tsmq, TSMQ_ERR_START_FAILED,
		   "Failed to create broker connection");
      return -1;
    }

  if(zsocket_connect(server->broker_socket, "%s", server->broker_uri) < 0)
    {
      tsmq_set_err(server->tsmq, errno, "Could not connect to broker");
      return -1;
    }

  msg_type_p = TSMQ_MSG_TYPE_READY;
  if((frame = zframe_new(&msg_type_p, 1)) == NULL)
    {
      tsmq_set_err(server->tsmq, TSMQ_ERR_MALLOC,
		   "Could not create new server-ready frame");
      return -1;
    }

  if(zframe_send(&frame, server->broker_socket, 0) == -1)
    {
      tsmq_set_err(server->tsmq, errno,
		   "Could not send ready msg to broker");
      return -1;
    }

  fprintf(stderr, "DEBUG: server ready (%d)\n", msg_type_p);

  return 0;
}

static int run_server(tsmq_md_server_t *server)
{
  zmq_pollitem_t poll_items[] = {
    {server->broker_socket, 0, ZMQ_POLLIN, 0}, /* POLL_ITEM_BROKER */
  };
  int rc;

  zmsg_t *msg;
  zframe_t *frame;

  tsmq_msg_type_t msg_type;
  uint8_t msg_type_p;

  /*fprintf(stderr, "DEBUG: Beginning loop cycle\n");*/

  if((rc = zmq_poll(poll_items, POLL_ITEM_CNT,
		    server->heartbeat_interval * ZMQ_POLL_MSEC)) == -1)
    {
      goto interrupt;
    }

  if(poll_items[POLL_ITEM_BROKER].revents & ZMQ_POLLIN)
    {
      /*  Get message
       *  - 3-part: [client.id + empty + content] => request
       *  - 1-part: HEARTBEAT => heartbeat
       */
      if((msg = zmsg_recv(server->broker_socket)) == NULL)
	{
	  goto interrupt;
	}

      if(zmsg_size(msg) == 3)
	{
	  fprintf(stderr, "DEBUG: Got request from client (via broker)\n");

	  server->heartbeat_liveness_remaining = server->heartbeat_liveness;

	  fprintf(stderr, "DEBUG: Pretending to do some work\n");
	  sleep(1);
	  /* TODO: actually parse the request and lookup the metadata */
	  /* ... we should be given a libtimeseries instance to use for lookups */

	  /* send the reply back */
	  /* broker expects [id+empty(+response)] */
	  if(zmsg_send(&msg, server->broker_socket) == -1)
	    {
	      tsmq_set_err(server->tsmq, errno,
			   "Could not send reply to broker");
	      return -1;
	    }

	  if(zctx_interrupted != 0)
	    {
	      goto interrupt;
	    }
	}
      else if(zmsg_size(msg) == 1)
	{
	  /* When we get a heartbeat message from the queue, it means the queue
	     was (recently) alive, so we must reset our liveness indicator */
	  msg_type = tsmq_msg_type(msg);
	  if(msg_type == TSMQ_MSG_TYPE_HEARTBEAT)
	    {
	      server->heartbeat_liveness_remaining = server->heartbeat_liveness;
	    }
	  else
	    {
	      tsmq_set_err(server->tsmq, TSMQ_ERR_PROTOCOL,
			   "Invalid message type received from broker (%d)",
			   msg_type);
	      return -1;
	    }
	  zmsg_destroy(&msg);
	}
      else
	{
	  tsmq_set_err(server->tsmq, TSMQ_ERR_PROTOCOL,
		       "Invalid message received from broker");
	  return -1;
	}
      server->reconnect_interval_next =
	server->reconnect_interval_min;
    }
  else if(--server->heartbeat_liveness_remaining == 0)
    {
      fprintf(stderr, "WARN: heartbeat failure, can't reach broker\n");
      fprintf(stderr, "WARN: reconnecting in %"PRIu64" msec…\n",
	      server->reconnect_interval_next);

      zclock_sleep(server->reconnect_interval_next);

      if(server->reconnect_interval_next < server->reconnect_interval_max)
	{
	  server->reconnect_interval_next *= 2;
	}

      zsocket_destroy(CTX, server->broker_socket);
      server_connect(server);

      server->heartbeat_liveness_remaining = server->heartbeat_liveness;
    }

  /* send heartbeat to queue if it is time */
  if(zclock_time () > server->heartbeat_next)
    {
      server->heartbeat_next = zclock_time() + server->heartbeat_interval;
      fprintf(stderr, "DEBUG: Sending heartbeat to broker\n");

      msg_type_p = TSMQ_MSG_TYPE_HEARTBEAT;
      if((frame = zframe_new(&msg_type_p, 1)) == NULL)
	{
	  tsmq_set_err(server->tsmq, TSMQ_ERR_MALLOC,
		       "Could not create new heartbeat frame");
	  return -1;
	}

      if(zframe_send(&frame, server->broker_socket, 0) == -1)
	{
	  tsmq_set_err(server->tsmq, errno,
		       "Could not send heartbeat msg to broker");
	  return -1;
	}
    }

  return 0;

 interrupt:
  /* we were interrupted */
  tsmq_set_err(server->tsmq, TSMQ_ERR_INTERRUPT, "Caught interrupt");
  return -1;
}

/* ---------- PUBLIC FUNCTIONS BELOW HERE ---------- */

TSMQ_ERR_FUNCS(md_server)

tsmq_md_server_t *tsmq_md_server_init()
{
  tsmq_md_server_t *server;
  if((server = malloc_zero(sizeof(tsmq_md_server_t))) == NULL)
    {
      /* cannot set an err at this point */
      return NULL;
    }

  if((server->tsmq = tsmq_init()) == NULL)
    {
      /* still cannot set an error */
      return NULL;
    }

  /* now we are ready to set errors... */

  server->broker_uri = strdup(TSMQ_MD_SERVER_BROKER_URI_DEFAULT);

  server->heartbeat_interval = TSMQ_MD_HEARTBEAT_INTERVAL_DEFAULT;

  server->heartbeat_liveness_remaining = server->heartbeat_liveness =
    TSMQ_MD_HEARTBEAT_LIVENESS_DEFAULT;

  server->reconnect_interval_next = server->reconnect_interval_min =
    TSMQ_MD_RECONNECT_INTERVAL_MIN;

  server->reconnect_interval_max = TSMQ_MD_RECONNECT_INTERVAL_MAX;

  return server;
}

int tsmq_md_server_start(tsmq_md_server_t *server)
{
  /* connect to the server */
  if(server_connect(server) != 0)
    {
      return -1;
    }

  /* seed the time for the next heartbeat sent to the broker */
  server->heartbeat_next = zclock_time() + server->heartbeat_interval;

  /* start processing requests */
  while(run_server(server) == 0)

  tsmq_set_err(server->tsmq, TSMQ_ERR_UNHANDLED, "Unhandled error");
  return -1;
}

void tsmq_md_server_free(tsmq_md_server_t *server)
{
  assert(server != NULL);
  assert(server->tsmq != NULL);

  if(server->broker_uri != NULL)
    {
      free(server->broker_uri);
      server->broker_uri = NULL;
    }

  /* free'd by tsmq_free */
  server->broker_socket = NULL;

  /* will call zctx_destroy which will destroy our sockets too */
  tsmq_free(server->tsmq);
  free(server);

  return;
}

void tsmq_md_server_set_broker_uri(tsmq_md_server_t *server, const char *uri)
{
  assert(server != NULL);

  server->broker_uri = strdup(uri);
}

void tsmq_md_server_set_heartbeat_interval(tsmq_md_server_t *server,
					   uint64_t interval_ms)
{
  assert(server != NULL);

  server->heartbeat_interval = interval_ms;
}

void tsmq_md_server_set_heartbeat_liveness(tsmq_md_server_t *server,
					   int beats)
{
  assert(server != NULL);

  server->heartbeat_liveness = beats;
}

void tsmq_md_server_set_reconnect_interval_min(tsmq_md_server_t *server,
					       uint64_t reconnect_interval_min)
{
  assert(server != NULL);

  server->reconnect_interval_min = reconnect_interval_min;
}

void tsmq_md_server_set_reconnect_interval_max(tsmq_md_server_t *server,
					       uint64_t reconnect_interval_max)
{
  assert(server != NULL);

  server->reconnect_interval_max = reconnect_interval_max;
}
