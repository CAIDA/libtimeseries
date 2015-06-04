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

#include "timeseries.h"
#include "timeseries_backend_int.h"

#include "tsmq_server_int.h"

#include "utils.h"

#define BULK_KEY_THRESHOLD 1

#define CTX server->tsmq->ctx

/* @todo: make this configurable, and generate a sane default. should be
   constant across restarts and define an instance of a timeseries
   backend. e.g. a dbats db, or a whisper store */
#define SERVER_ID "darknet.ucsd-nt"

static int server_connect(tsmq_server_t *server)
{
  uint8_t msg_type_p;

  /* connect to broker socket */
  if((server->broker_socket = zsocket_new(CTX, ZMQ_DEALER)) == NULL)
    {
      tsmq_set_err(server->tsmq, TSMQ_ERR_START_FAILED,
		   "Failed to create broker connection");
      return -1;
    }

  zsocket_set_rcvtimeo(server->broker_socket, server->heartbeat_interval);

  if(zsocket_connect(server->broker_socket, "%s", server->broker_uri) < 0)
    {
      tsmq_set_err(server->tsmq, errno, "Could not connect to broker");
      return -1;
    }

  msg_type_p = TSMQ_MSG_TYPE_READY;
  if(zmq_send(server->broker_socket, &msg_type_p, 1, 0) == -1)
    {
      tsmq_set_err(server->tsmq, errno,
		   "Could not send ready msg to broker");
      return -1;
    }

  fprintf(stderr, "DEBUG: server ready (%d)\n", msg_type_p);

  return 0;
}

static int handle_key_lookup_bulk(tsmq_server_t *server)
{
  char *key = NULL;     /* temp storage */
  uint32_t keys_cnt = 0; /* expected num to rx */
  uint32_t keys_rx = 0; /* num actually rx'd */
  int contig_alloc = 0;

  int i;

  /* receive the number of keys */
  if(zsocket_rcvmore(server->broker_socket) == 0)
    {
      tsmq_set_err(server->tsmq, TSMQ_ERR_PROTOCOL,
                   "Invalid 'key lookup' message (missing key cnt)");
      goto err;
    }
  if(zmq_recv(server->broker_socket, &keys_cnt, sizeof(uint32_t), 0)
     != sizeof(uint32_t))
    {
      tsmq_set_err(server->tsmq, TSMQ_ERR_PROTOCOL,
                   "Malformed 'key lookup' request (invalid key cnt)");
      goto err;
    }
  keys_cnt = ntohl(keys_cnt);

  if(keys_cnt > server->key_alloc_cnt)
    {
      /* do the mallocs */
      if(((server->keys =
           realloc(server->keys, sizeof(char*)*keys_cnt)) == NULL) ||
         ((server->key_ids =
           realloc(server->key_ids, sizeof(uint8_t*)*keys_cnt)) == NULL) ||
         ((server->key_id_lens =
           realloc(server->key_id_lens, sizeof(size_t)*keys_cnt)) == NULL))
        {
          tsmq_set_err(server->tsmq, TSMQ_ERR_MALLOC,
                       "Could not init key lookup arrays");
          goto err;
        }
      server->key_alloc_cnt = keys_cnt;
    }

  /* receive all the queries */
  while(1)
    {
      if(zsocket_rcvmore(server->broker_socket) == 0)
        {
          tsmq_set_err(server->tsmq, TSMQ_ERR_PROTOCOL,
                       "Invalid 'key lookup' message (missing key)");
          goto err;
        }

      /* grab the key from the message */
      if((key = tsmq_recv_str(server->broker_socket)) == NULL)
        {
          tsmq_set_err(server->tsmq, TSMQ_ERR_PROTOCOL,
                       "Malformed key lookup request (missing key)");
          goto err;
        }

      /* to support bulk lookup, we have to know if the next message is empty */
      if(key[0] == '\0')
        {
          /* no more keys */
          free(key);
          break;
        }

      assert(keys_rx+1 <= keys_cnt);
      server->keys[keys_rx++] = key;
    }

  if(zsocket_rcvmore(server->broker_socket) != 0)
    {
      tsmq_set_err(server->tsmq, TSMQ_ERR_PROTOCOL,
                   "Invalid 'key lookup' message (extra frames)");
      goto err;
    }

  /* did we get the correct number of keys? */
  if(keys_cnt != keys_rx)
    {
      tsmq_set_err(server->tsmq, TSMQ_ERR_PROTOCOL,
                   "Expecting %"PRIu32" keys to lookup, received %"PRIu32,
                   keys_cnt, keys_rx);
      goto err;
    }

  fprintf(stderr, "INFO: Resolving %"PRIu32" keys\n", keys_cnt);

  /* do the actual lookup */
  if(server->backend->resolve_key_bulk(server->backend, keys_cnt,
                                       (const char* const *)server->keys,
                                       server->key_ids, server->key_id_lens,
                                       &contig_alloc) != 0)
    {
      tsmq_set_err(server->tsmq, TSMQ_ERR_TIMESERIES,
                   "Bulk key lookup failed");
      goto err;
    }

  /* send each result */
  for(i=0; i<keys_cnt; i++)
    {
      assert(server->key_ids[i] != NULL);
      assert(server->key_id_lens[i] > 0);
      /* send the backend-specific key */
      if(zmq_send(server->broker_socket,
                  server->key_ids[i], server->key_id_lens[i], ZMQ_SNDMORE)
         != server->key_id_lens[i])
        {
          tsmq_set_err(server->tsmq, TSMQ_ERR_MALLOC,
                       "Failed to send server key id");
          goto err;
        }

      /* allocated by us */
      free(server->keys[i]);

      /* these things allocated by the backend */
      if(contig_alloc == 0)
        {
          free(server->key_ids[i]);
        }
    }

  /* end of stream */
  if(zmq_send(server->broker_socket, "", 0, 0) != 0)
    {
      tsmq_set_err(server->tsmq, TSMQ_ERR_MALLOC,
                   "Failed to send lookup completion message");
      goto err;
    }

  if(contig_alloc != 0)
    {
      free(server->key_ids[0]);
    }

  return 0;

 err:
  return -1;
}

static int recv_key_val(tsmq_server_t *server,
                        zmq_msg_t *key_msg_out,
                        tsmq_val_t *value_out)
{
  size_t len;

  /* get the value from the message */
  if(zsocket_rcvmore(server->broker_socket) == 0)
    {
      tsmq_set_err(server->tsmq, TSMQ_ERR_PROTOCOL,
                   "Invalid 'key/value' message (missing value)");
      goto err;
    }
  if((len = zmq_recv(server->broker_socket, value_out, sizeof(tsmq_val_t), 0))
     != sizeof(tsmq_val_t))
    {
      if(len == 0)
        {
          /* end of stream reached */
          return 0;
        }
      tsmq_set_err(server->tsmq, TSMQ_ERR_PROTOCOL,
                   "Malformed 'key/value' request (invalid value)");
      goto err;
    }
  *value_out = ntohll(*value_out);

  /* grab the key from the message */
  if(zsocket_rcvmore(server->broker_socket) == 0)
    {
      tsmq_set_err(server->tsmq, TSMQ_ERR_PROTOCOL,
                   "Invalid 'key/value' message (missing key)");
      goto err;
    }
  if(zmq_msg_init(key_msg_out) == -1)
    {
      tsmq_set_err(server->tsmq, TSMQ_ERR_MALLOC,
                   "Could not init key message");
      goto err;
    }
  if(zmq_msg_recv(key_msg_out, server->broker_socket, 0) == -1)
    {
      tsmq_set_err(server->tsmq, TSMQ_ERR_PROTOCOL,
                   "Malformed request (missing key id)");
      goto err;
    }

  return zmq_msg_size(key_msg_out);

 err:
  zmq_msg_close(key_msg_out);
  return -1;
}

static int handle_set_bulk(tsmq_server_t *server)
{
  tsmq_time_t time;
  uint32_t key_cnt;
  tsmq_val_t value;
  zmq_msg_t key_msg;
  int rc;

  /* ack the request immediately */
  /* send back a single empty message as an ack */
  if(zmq_send(server->broker_socket, "", 0, 0) == -1)
    {
      tsmq_set_err(server->tsmq, errno,
                   "Could not send set single reply");
      goto err;
    }

  /* get the time from the message */
  if(zsocket_rcvmore(server->broker_socket) == 0)
    {
      tsmq_set_err(server->tsmq, TSMQ_ERR_PROTOCOL,
                   "Invalid 'value set' message (missing time)");
      goto err;
    }
  if(zmq_recv(server->broker_socket, &time, sizeof(tsmq_time_t), 0)
     != sizeof(tsmq_time_t))
    {
      tsmq_set_err(server->tsmq, TSMQ_ERR_PROTOCOL,
                   "Malformed 'value set' request (invalid time)");
      goto err;
    }
  time = ntohl(time);

  /* get the number of keys in this message (to decide whether we should use
     set_single, or set_many */
  if(zsocket_rcvmore(server->broker_socket) == 0)
    {
      tsmq_set_err(server->tsmq, TSMQ_ERR_PROTOCOL,
                   "Invalid 'value set' message (missing key cnt)");
      goto err;
    }
  if(zmq_recv(server->broker_socket, &key_cnt, sizeof(uint32_t), 0)
     != sizeof(uint32_t))
    {
      tsmq_set_err(server->tsmq, TSMQ_ERR_PROTOCOL,
                   "Malformed 'value set' request (invalid key cnt)");
      goto err;
    }
  key_cnt = ntohl(key_cnt);

  if(key_cnt > BULK_KEY_THRESHOLD)
    {
      /* bulk set start */
      fprintf(stderr, "INFO: Setting %"PRIu32" values\n", key_cnt);
      if(server->backend->set_bulk_init(server->backend, key_cnt, time) != 0)
        {
          tsmq_set_err(server->tsmq, TSMQ_ERR_TIMESERIES,
                       "Set bulk init failed\n");
          goto err;
        }
    }

  while(1)
    {
      if((rc = recv_key_val(server, &key_msg, &value)) <= 0)
        {
          zmq_msg_close(&key_msg);
          break;
        }

      if(key_cnt <= BULK_KEY_THRESHOLD)
        {
          if(server->backend->set_single_by_id(server->backend,
                                               zmq_msg_data(&key_msg),
                                               zmq_msg_size(&key_msg),
                                               value, time) != 0)
            {
              tsmq_set_err(server->tsmq, TSMQ_ERR_TIMESERIES,
                           "Set single failed\n");
              goto err;
            }
        }
      else
        {
          if(server->backend->set_bulk_by_id(server->backend,
                                             zmq_msg_data(&key_msg),
                                             zmq_msg_size(&key_msg),
                                             value) != 0)
            {
              tsmq_set_err(server->tsmq, TSMQ_ERR_TIMESERIES,
                           "Set bulk failed\n");
              goto err;
            }
        }

      zmq_msg_close(&key_msg);
    }

  return rc;

 err:
  zmq_msg_close(&key_msg);
  return -1;
}

/* send reply message type, and turn around the client info frames */
static int send_reply_header(tsmq_server_t *server)
{
  tsmq_msg_type_t msg_type = TSMQ_MSG_TYPE_REPLY;
  int i;
  zmq_msg_t proxy;

  /* sends
    TSMQ_MSG_TYPE_REPLY
    CLIENT_ID (rx)
    EMPTY (rx)
    SEQ_NUM (rx)
  */

  if(zmq_send(server->broker_socket, &msg_type, tsmq_msg_type_size_t,
              ZMQ_SNDMORE) == -1)
    {
      tsmq_set_err(server->tsmq, errno,
                   "Could not send reply msg type to broker");
      return -1;
    }

  /* we need to receive the client id, the empty frame and the sequence
     number and send them back to the broker */
  /* <client-id>, <empty>, <seq no> */
  for(i=0; i<3; i++)
    {
      if(zsocket_rcvmore(server->broker_socket) == 0)
        {
          tsmq_set_err(server->tsmq, TSMQ_ERR_PROTOCOL,
                       "Invalid message from broker (%d)", i);
          return -1;
        }

      if(zmq_msg_init(&proxy) != 0)
        {
          tsmq_set_err(server->tsmq, TSMQ_ERR_MALLOC,
                       "Could not create proxy message");
          return -1;
        }

      if(zmq_msg_recv(&proxy, server->broker_socket, 0) == -1)
        {
          tsmq_set_err(server->tsmq, TSMQ_ERR_PROTOCOL,
                       "Could not receive message from client");
          return -1;
        }

      if(zmq_msg_send(&proxy, server->broker_socket, ZMQ_SNDMORE) == -1)
        {
          zmq_msg_close(&proxy);
          tsmq_set_err(server->tsmq, errno, "Could not send reply header");
          return -1;
        }
    }

  return 0;
}

static int handle_request(tsmq_server_t *server)
{
  tsmq_request_msg_type_t req_type;

  if(send_reply_header(server) != 0)
    {
      return -1;
    }

  /* grab the request type */
  req_type = tsmq_recv_request_type(server->broker_socket);

  /* send it back to the broker */
  if(zmq_send(server->broker_socket, &req_type,
              tsmq_request_msg_type_size_t, ZMQ_SNDMORE)
     != tsmq_request_msg_type_size_t)
    {
      tsmq_set_err(server->tsmq, TSMQ_ERR_MALLOC,
                   "Could not add reply type to message");
      return -1;
    }

  switch(req_type)
    {
    case TSMQ_REQUEST_MSG_TYPE_KEY_LOOKUP:
    case TSMQ_REQUEST_MSG_TYPE_KEY_LOOKUP_BULK:
      return handle_key_lookup_bulk(server);
      break;

    case TSMQ_REQUEST_MSG_TYPE_KEY_SET_SINGLE:
    case TSMQ_REQUEST_MSG_TYPE_KEY_SET_BULK:
      return handle_set_bulk(server);
      break;

    default:
      tsmq_set_err(server->tsmq, TSMQ_ERR_PROTOCOL,
                   "Unhandled request type (%d)", req_type);
      return -1;
      break;
    }
}

static void broker_reconnect(tsmq_server_t *server)
{
  fprintf(stderr, "WARN: heartbeat failure, can't reach broker\n");
  fprintf(stderr, "WARN: reconnecting in %"PRIu64" msec...\n",
          server->reconnect_interval_next);

  zclock_sleep(server->reconnect_interval_next);

  if(server->reconnect_interval_next <
     server->reconnect_interval_max)
    {
      server->reconnect_interval_next *= 2;
    }

  zsocket_destroy(CTX, server->broker_socket);
  server_connect(server);

  server->heartbeat_liveness_remaining = server->heartbeat_liveness;
}

static int run_server(tsmq_server_t *server)
{
  tsmq_msg_type_t msg_type;
  uint8_t msg_type_p;

  /*  Get message
   *  - 4-part: REQUEST + [client.id + empty + content] => request
   *  - 1-part: HEARTBEAT => heartbeat
   */

  msg_type = tsmq_recv_type(server->broker_socket);
  switch(msg_type)
    {
    case TSMQ_MSG_TYPE_REQUEST:
      if(handle_request(server) != 0)
        {
          goto err;
        }
      /* fall through to heartbeat */

    case TSMQ_MSG_TYPE_HEARTBEAT:
      server->heartbeat_liveness_remaining = server->heartbeat_liveness;
      server->reconnect_interval_next = server->reconnect_interval_min;
      break;

    default:
      switch(errno)
        {
       case EAGAIN:
         fprintf(stderr, "DEBUG: Heartbeat missed, %d lives remain\n",
                 server->heartbeat_liveness_remaining);
         if(--server->heartbeat_liveness_remaining == 0)
           {
             broker_reconnect(server);
           }
         /* by here we have reconnected */
         break;

        case ETERM:
        case EINTR:
          goto interrupt;
          break;

        default:
          tsmq_set_err(server->tsmq, TSMQ_ERR_PROTOCOL,
                       "Invalid message type received from broker (%d)",
                       msg_type);
          goto err;
          break;
        }
      break;
    }

  /* send heartbeat to queue if it is time */
  if(zclock_time() > server->heartbeat_next)
    {
      server->heartbeat_next = zclock_time() + server->heartbeat_interval;

      msg_type_p = TSMQ_MSG_TYPE_HEARTBEAT;
      if(zmq_send(server->broker_socket, &msg_type_p, 1, 0) == -1)
	{
	  tsmq_set_err(server->tsmq, errno,
		       "Could not send heartbeat msg to broker");
          goto err;
	}
    }

  return 0;

 err:
  return -1;

 interrupt:
  /* we were interrupted */
  tsmq_set_err(server->tsmq, TSMQ_ERR_INTERRUPT, "Caught interrupt");
  return -1;
}

/* ---------- PUBLIC FUNCTIONS BELOW HERE ---------- */

TSMQ_ERR_FUNCS(server)

tsmq_server_t *tsmq_server_init(timeseries_backend_t *ts_backend)
{
  tsmq_server_t *server;
  if((server = malloc_zero(sizeof(tsmq_server_t))) == NULL)
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

  assert(ts_backend != NULL);
  server->backend = ts_backend;

  server->broker_uri = strdup(TSMQ_SERVER_BROKER_URI_DEFAULT);

  server->heartbeat_interval = TSMQ_HEARTBEAT_INTERVAL_DEFAULT;

  server->heartbeat_liveness_remaining = server->heartbeat_liveness =
    TSMQ_HEARTBEAT_LIVENESS_DEFAULT;

  server->reconnect_interval_next = server->reconnect_interval_min =
    TSMQ_RECONNECT_INTERVAL_MIN;

  server->reconnect_interval_max = TSMQ_RECONNECT_INTERVAL_MAX;

  return server;
}

int tsmq_server_start(tsmq_server_t *server)
{
  /* connect to the server */
  if(server_connect(server) != 0)
    {
      return -1;
    }

  /* seed the time for the next heartbeat sent to the broker */
  server->heartbeat_next = zclock_time() + server->heartbeat_interval;

  /* start processing requests */
  while(1)
    {
      if(run_server(server) != 0)
        {
          break;
        }
    }

  return -1;
}

void tsmq_server_free(tsmq_server_t *server)
{
  assert(server != NULL);
  assert(server->tsmq != NULL);

  free(server->broker_uri);
  server->broker_uri = NULL;

  /* free'd by tsmq_free */
  server->broker_socket = NULL;

  free(server->keys);
  server->keys = NULL;

  free(server->key_ids);
  server->key_ids = NULL;

  free(server->key_id_lens);
  server->key_id_lens = NULL;

  server->key_alloc_cnt = 0;

  /* will call zctx_destroy which will destroy our sockets too */
  tsmq_free(server->tsmq);
  free(server);

  return;
}

void tsmq_server_set_broker_uri(tsmq_server_t *server, const char *uri)
{
  assert(server != NULL);

  free(server->broker_uri);

  server->broker_uri = strdup(uri);
}

void tsmq_server_set_heartbeat_interval(tsmq_server_t *server,
					uint64_t interval_ms)
{
  assert(server != NULL);

  server->heartbeat_interval = interval_ms;
}

void tsmq_server_set_heartbeat_liveness(tsmq_server_t *server,
					int beats)
{
  assert(server != NULL);

  server->heartbeat_liveness_remaining = server->heartbeat_liveness = beats;
}

void tsmq_server_set_reconnect_interval_min(tsmq_server_t *server,
					    uint64_t reconnect_interval_min)
{
  assert(server != NULL);

  server->reconnect_interval_min = reconnect_interval_min;
}

void tsmq_server_set_reconnect_interval_max(tsmq_server_t *server,
					    uint64_t reconnect_interval_max)
{
  assert(server != NULL);

  server->reconnect_interval_max = reconnect_interval_max;
}
