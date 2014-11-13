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

#include "tsmq_client_int.h"

#include "utils.h"

#define CTX client->tsmq->ctx

enum {
  REPLY_ERROR   = -1,
  REPLY_SUCCESS =  0,
  REPLY_TIMEOUT =  1,
};

#define SEND_REQUEST(req_type)                                  \
  int retries_remaining = client->request_retries;              \
  int reply_rx = -1;                                            \
  while(retries_remaining > 0 && zctx_interrupted == 0)         \
    {                                                           \
  /* send the message headers */                                \
  if(send_request_headers(client, req_type, ZMQ_SNDMORE) != 0)  \
    {                                                           \
      /* err will already be set */                             \
      goto err;                                                 \
    }

#define HANDLE_REQUEST_REPLY(req_type)                                  \
  /* wait for a reply */                                                \
  if((reply_rx = recv_reply_headers(client, req_type)) == REPLY_ERROR)  \
    {                                                                   \
      goto err;                                                         \
    }                                                                   \
  if(reply_rx == REPLY_SUCCESS)                                         \
    {                                                                   \
      break;                                                            \
    }                                                                   \
  if(reconnect_broker(client) != 0)                                     \
    {                                                                   \
      goto err;                                                         \
    }                                                                   \
  retries_remaining--;                                                  \
  }                                                                     \
  /* increment the seq number regardless of the outcome */              \
  client->sequence_num++;                                               \
  /* we didn't get a reply, and we ran out of retries */                \
   do {                                                                 \
     if(reply_rx == REPLY_TIMEOUT)                                      \
       {                                                                \
         tsmq_set_err(client->tsmq, TSMQ_ERR_PROTOCOL,                  \
                      "No response received after %d retries.",         \
                      client->request_retries);                         \
         goto err;                                                      \
       }                                                                \
   } while(0)

static int broker_connect(tsmq_client_t *client)
{
  /* connect to broker socket */
  if((client->broker_socket = zsocket_new(CTX, ZMQ_REQ)) == NULL)
    {
      tsmq_set_err(client->tsmq, TSMQ_ERR_START_FAILED,
		   "Failed to create broker connection");
      return -1;
    }

  /** @todo enable this instead of zmq_poll */
  /*zsocket_set_rcvtimeo(client->server_socket, client->request_timeout);*/

  if(zsocket_connect(client->broker_socket, "%s", client->broker_uri) < 0)
    {
      tsmq_set_err(client->tsmq, errno, "Could not connect to broker on %s",
		   client->broker_uri);
      return -1;
    }

  return 0;
}

static int reconnect_broker(tsmq_client_t *client)
{
  fprintf(stderr, "WARN: No response, retrying...\n");
  zsocket_destroy(CTX, client->broker_socket);
  fprintf(stderr, "DEBUG: Reconnecting to broker\n");
  if(broker_connect(client) != 0)
    {
      tsmq_set_err(client->tsmq, TSMQ_ERR_START_FAILED,
                   "Failed to connect to broker");
      return -1;
    }
  return 0;
}

static int send_request_headers(tsmq_client_t *client,
                                tsmq_request_msg_type_t req_type,
                                int sndmore)
{
  uint64_t seq_num;
  /* request format:
     SEQUENCE
     REQUEST_TYPE
     REQUEST_BODY
     ...
  */

  seq_num = htonll(client->sequence_num);
  /* send the sequence number */
  if(zmq_send(client->broker_socket, &seq_num, sizeof(uint64_t),
              ZMQ_SNDMORE) != sizeof(uint64_t))
    {
      tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
                   "Could send sequence number message");
      return -1;
    }

  /* send the request type */
  if(zmq_send(client->broker_socket, &req_type, tsmq_msg_type_size_t,
              (sndmore != 0) ? ZMQ_SNDMORE : 0)
     != tsmq_msg_type_size_t)
    {
      tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
                   "Could not send request type message");
      return -1;
    }

  return 0;
}

/** Block waiting for a reply
 *
 * @param client
 * @param req_type      expected type of the reply
 * @return 0 if the headers were received correctly, 1 if not (needs re-tx),
 *         -1 if an error occurred
 */
static int recv_reply_headers(tsmq_client_t *client,
                              tsmq_request_msg_type_t req_type)
{
  uint64_t rx_seq_num;
  tsmq_request_msg_type_t rx_req_type;

  zmq_pollitem_t poll_items[] = {
    { client->broker_socket, 0, ZMQ_POLLIN, 0 },
  };
  if(zmq_poll(poll_items, 1,
              client->request_timeout * ZMQ_POLL_MSEC) == -1)
    {
      goto interrupt;
    }

  /* process a server reply and exit our loop if the reply is valid */
  if(poll_items[0].revents & ZMQ_POLLIN)
    {
      /* Msg Format:
         SEQ_NUM
         REQ_TYPE
         PAYLOAD
      */

      /* got a reply from the broker, must match the sequence number */
      if(zmq_recv(client->broker_socket, &rx_seq_num, sizeof(uint64_t), 0)
         != sizeof(uint64_t))
        {
          tsmq_set_err(client->tsmq, TSMQ_ERR_PROTOCOL,
                       "Malformed request reply (missing seq num)");
          goto err;
        }
      rx_seq_num = ntohll(rx_seq_num);

      /* check the sequence number against what we have */
      if(rx_seq_num != client->sequence_num)
        {
          tsmq_set_err(client->tsmq, TSMQ_ERR_PROTOCOL,
                       "Invalid sequence number received."
                       " Got %"PRIu64", expecting %"PRIu64,
                       rx_seq_num, client->sequence_num);
          goto err;
        }

      if(zsocket_rcvmore(client->broker_socket) == 0)
        {
          tsmq_set_err(client->tsmq, TSMQ_ERR_PROTOCOL,
                       "Invalid reply message (missing request type)");
          goto err;
        }

      /* check the request type against what we sent */
      if((rx_req_type = tsmq_recv_request_type(client->broker_socket))
         != req_type)
        {
          tsmq_set_err(client->tsmq, TSMQ_ERR_PROTOCOL,
                       "Invalid request type in response."
                       " Got %d, expecting %d",
                       rx_req_type, req_type);
          goto err;

        }

      /* all that is left is the payload (there MUST be payload) */
      if(zsocket_rcvmore(client->broker_socket) == 0)
        {
          tsmq_set_err(client->tsmq, TSMQ_ERR_PROTOCOL,
                       "Invalid reply message (missing payload)");
          goto err;
        }
      return REPLY_SUCCESS;
    }

  /* no luck, perhaps retry the request */
  return REPLY_TIMEOUT;

 err:
  return REPLY_ERROR;

 interrupt:
  tsmq_set_err(client->tsmq, TSMQ_ERR_INTERRUPT, "Caught SIGINT");
  return REPLY_ERROR;
}

static tsmq_client_key_t *key_init()
{
  tsmq_client_key_t *key = NULL;

  if((key = malloc(sizeof(tsmq_client_key_t))) == NULL)
    {
      goto err;
    }

  if(zmq_msg_init(&key->server_id) == -1)
    {
      goto err;
    }

  if(zmq_msg_init(&key->server_key_id) == -1)
    {
      goto err;
    }

  return key;

 err:
  tsmq_client_key_free(&key);
  return NULL;
}

static int key_recv(tsmq_client_t *client, tsmq_client_key_t *key)
{
  /* we expect two messages, one with the server id and one with the key id */

  if(zmq_msg_recv(&key->server_id, client->broker_socket, 0) == -1)
    {
      tsmq_set_err(client->tsmq, TSMQ_ERR_PROTOCOL,
                   "Malformed reply (missing server id)");
      return -1;
    }

  /* now there should be one more frame with the key id */
  if(zmq_msg_recv(&key->server_key_id, client->broker_socket, 0) == -1)
    {
      tsmq_set_err(client->tsmq, TSMQ_ERR_PROTOCOL,
                   "Malformed reply (missing server key id)");
      return -1;
    }

  return 0;
}

/* ---------- PUBLIC FUNCTIONS BELOW HERE ---------- */

TSMQ_ERR_FUNCS(client)

tsmq_client_t *tsmq_client_init()
{
  tsmq_client_t *client;
  if((client = malloc_zero(sizeof(tsmq_client_t))) == NULL)
    {
      /* cannot set an err at this point */
      return NULL;
    }

  if((client->tsmq = tsmq_init()) == NULL)
    {
      /* still cannot set an error */
      return NULL;
    }

  /* now we are ready to set errors... */

  client->broker_uri = strdup(TSMQ_CLIENT_BROKER_URI_DEFAULT);

  client->request_timeout = TSMQ_CLIENT_REQUEST_TIMEOUT;

  client->request_retries = TSMQ_CLIENT_REQUEST_RETRIES;

  return client;
}

int tsmq_client_start(tsmq_client_t *client)
{
  return broker_connect(client);
}

void tsmq_client_free(tsmq_client_t **client_p)
{
  assert(client_p != NULL);
  tsmq_client_t *client = *client_p;
  if(client == NULL)
    {
      return;
    }
  assert(client->tsmq != NULL);

  free(client->broker_uri);
  client->broker_uri = NULL;

  /* will call zctx_destroy which will destroy our sockets too */
  tsmq_free(client->tsmq);

  /* free'd by tsmq_free */
  client->broker_socket = NULL;

  free(client);

  *client_p = NULL;
  return;
}

void tsmq_client_set_broker_uri(tsmq_client_t *client, const char *uri)
{
  assert(client != NULL);

  free(client->broker_uri);

  client->broker_uri = strdup(uri);
  assert(client->broker_uri != NULL);
}

void tsmq_client_set_request_timeout(tsmq_client_t *client,
				     uint64_t timeout_ms)
{
  assert(client != NULL);

  client->request_timeout = timeout_ms;
}

void tsmq_client_set_request_retries(tsmq_client_t *client,
				     int retry_cnt)
{
  assert(client != NULL);

  client->request_retries = retry_cnt;
}

tsmq_client_key_t *tsmq_client_key_lookup(tsmq_client_t *client,
					  const char *key)
{
  tsmq_client_key_t *key_info = NULL;
  size_t key_len = strlen(key);

  SEND_REQUEST(TSMQ_REQUEST_MSG_TYPE_KEY_LOOKUP)
  {
    /* send the key */
    if(zmq_send_const(client->broker_socket, key, key_len, 0) != key_len)
      {
        tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
                     "Failed to add key to lookup message");
        goto err;
      }
  }
  HANDLE_REQUEST_REPLY(TSMQ_REQUEST_MSG_TYPE_KEY_LOOKUP);

  /* there is a payload waiting on the socket, grab it */

  /* create a new key info structure */
  if((key_info = key_init()) == NULL)
    {
      tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
                     "Failed to init key");
      goto err;
    }

  if(key_recv(client, key_info) != 0)
    {
      goto err;
    }

  return key_info;

 err:
  tsmq_client_key_free(&key_info);
  return NULL;
}

int tsmq_client_key_set_single(tsmq_client_t *client,
			       tsmq_client_key_t *key,
			       tsmq_val_t value, tsmq_time_t time)
{
  zmq_msg_t msg_cpy;

  /* payload structure will be:
     TIME          (4)
     VALUE         (8)
     SERVER_ID     (?) //TODO
     SERVER_KEY_ID (?)
  */

  SEND_REQUEST(TSMQ_REQUEST_MSG_TYPE_KEY_SET_SINGLE)
  {
    /* add the time */
    if(zmq_send_const(client->broker_socket, &time, sizeof(tsmq_time_t),
                      ZMQ_SNDMORE) != sizeof(tsmq_time_t))
      {
        tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
                     "Failed to send time in set single");
        goto err;
      }

    /* add the value */
    if(zmq_send_const(client->broker_socket, &value, sizeof(tsmq_val_t),
                      ZMQ_SNDMORE) != sizeof(tsmq_val_t))
      {
        tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
                     "Failed to send value in set single");
        goto err;
      }

    /* add the key */
    if(zmq_msg_init(&msg_cpy) == -1 ||
       zmq_msg_copy(&msg_cpy, &key->server_key_id) == -1)
      {
        tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
                     "Failed to copy key id in set single");
        goto err;
      }
    if(zmq_msg_send(&msg_cpy, client->broker_socket, 0) == -1)
      {
        tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
                     "Failed to send key id in set single");
        goto err;
      }
  }
  HANDLE_REQUEST_REPLY(TSMQ_REQUEST_MSG_TYPE_KEY_SET_SINGLE);

  /* there should be a single empty message, just pop it */
  if(zmq_recv(client->broker_socket, NULL, 0, 0) != 0)
    {
      tsmq_set_err(client->tsmq, TSMQ_ERR_PROTOCOL,
                   "Malformed set single reply");
      goto err;
    }

  return 0;

 err:
  return -1;
}

void tsmq_client_key_free(tsmq_client_key_t **key_p)
{
  tsmq_client_key_t *key;

  assert(key_p != NULL);
  key = *key_p;
  if(key == NULL)
    {
      return;
    }

  zmq_msg_close(&key->server_id);
  zmq_msg_close(&key->server_key_id);

  free(key);

  *key_p = NULL;
  return;
}
