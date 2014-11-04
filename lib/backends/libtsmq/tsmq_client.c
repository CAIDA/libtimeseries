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

static int broker_connect(tsmq_client_t *client)
{
  /* connect to broker socket */
  if((client->broker_socket = zsocket_new(CTX, ZMQ_REQ)) == NULL)
    {
      tsmq_set_err(client->tsmq, TSMQ_ERR_START_FAILED,
		   "Failed to create broker connection");
      return -1;
    }

  if(zsocket_connect(client->broker_socket, "%s", client->broker_uri) < 0)
    {
      tsmq_set_err(client->tsmq, errno, "Could not connect to broker on %s",
		   client->broker_uri);
      return -1;
    }

  return 0;
}

/* the caller no longer owns the request_body msg */
/* but they *will* own the returned msg */
static zmsg_t *execute_request(tsmq_client_t *client,
			       tsmq_request_msg_type_t type,
			       zmsg_t **request_body_p)
{
  zmsg_t *request_body = *request_body_p;
  *request_body_p = NULL;

  int retries_remaining = client->request_retries;

  zmsg_t *req_cpy;

  uint8_t type_b = type;
  int expect_reply = 1;

  zmsg_t *msg = NULL;
  zframe_t *frame = NULL;

  uint64_t seq;
  tsmq_request_msg_type_t rtype;

  while(retries_remaining > 0 && !zctx_interrupted)
    {
      /* request format:
	 SEQUENCE
	 REQUEST_TYPE
	 REQUEST_BODY
	 ...
      */

      /* first (working backward), we prepend the request type */
      if(zmsg_pushmem(request_body, &type_b, sizeof(uint8_t)) != 0)
	{
	  tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
		       "Could not add request type to message");
          goto err;
	}

      /* now prepend the sequence number */
      if(zmsg_pushmem(request_body, &client->sequence_num,
		      sizeof(uint64_t)) != 0)
	{
	  tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
		       "Could not add sequence number to message");
          goto err;
	}

      /* create a copy of the message in case we need to resend */
      if((req_cpy = zmsg_dup(request_body)) == NULL)
	{
	  tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
		       "Could not duplicate request message");
          goto err;
	}

      /* now fire off the request and wait for a reply */
      if(zmsg_send(&req_cpy, client->broker_socket) != 0)
	{
	  tsmq_set_err(client->tsmq, errno,
		       "Could not send request to broker");
          goto err;
	}

      while(expect_reply != 0)
	{
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
	      /* as soon as we get a response, we know that we will not resend,
		 so destroy the original message */
	      zmsg_destroy(&request_body);

	      /* got a reply from the broker, must match the sequence number */
	      if((msg = zmsg_recv(client->broker_socket)) == NULL)
		{
		  goto interrupt;
		}

	      /* pop off the first frame, it should be a seq no */
	      if((frame = zmsg_pop(msg)) == NULL)
		{
		  tsmq_set_err(client->tsmq, TSMQ_ERR_PROTOCOL,
			       "Missing sequence number in reply");
                  goto err;
		}

	      /* check the sequence number against what we have */
	      if((seq = *zframe_data(frame)) != client->sequence_num)
		{
		  tsmq_set_err(client->tsmq, TSMQ_ERR_PROTOCOL,
			       "Invalid sequence number received."
			       " Got %"PRIu64", expecting %"PRIu64,
			       seq, client->sequence_num);
                  goto err;
		  zframe_destroy(&frame);
		  zmsg_destroy(&msg);
		  return NULL;
		}
	      zframe_destroy(&frame);

	      /* check the request type against what we sent */
	      if((rtype = tsmq_request_msg_type(msg)) != type)
		{
		  tsmq_set_err(client->tsmq, TSMQ_ERR_PROTOCOL,
			       "Invalid request type in response."
			       " Got %d, expecting %d",
			       rtype, type);
                  goto err;

		}

	      /* call this an acceptable response */
	      return msg;
	    }

	  /* recv timed out */
	  if(--retries_remaining == 0)
	    {
	      tsmq_set_err(client->tsmq, TSMQ_ERR_PROTOCOL,
			   "No response received after %d retries.",
			   client->request_retries);
              goto err;
	    }
	  else
	    {
	      /* send the request again */
	      fprintf(stderr, "WARN: No response, retrying...\n");
	      zsocket_destroy(CTX, client->broker_socket);
	      fprintf(stderr, "DEBUG: Reconnecting to broker\n");
	      if(broker_connect(client) != 0)
		{
		  tsmq_set_err(client->tsmq, TSMQ_ERR_START_FAILED,
			       "Failed to connect to broker");
                  goto err;
		}
	      /* create a copy of the message in case we need to resend */
	      if((req_cpy = zmsg_dup(request_body)) == NULL)
		{
		  tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
			       "Could not duplicate request message");
                  goto err;
		}

	      fprintf(stderr, "DEBUG: Re-sending request to broker\n");
	      /* now fire off the request and wait for a reply */
	      if(zmsg_send(&req_cpy, client->broker_socket) != 0)
		{
		  tsmq_set_err(client->tsmq, errno,
			       "Could not send request to broker");
                  goto err;
		}
	    }
	}
    }

  zframe_destroy(&frame);
  zmsg_destroy(&msg);
  return NULL;

 err:
  zframe_destroy(&frame);
  zmsg_destroy(&msg);
  return NULL;

 interrupt:
  /* we were interrupted */
  zframe_destroy(&frame);
  zmsg_destroy(&msg);
  tsmq_set_err(client->tsmq, TSMQ_ERR_INTERRUPT, "Caught SIGINT");
  return NULL;
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
  zmsg_t *msg = NULL;
  zframe_t*frame = NULL;
  tsmq_client_key_t *response = NULL;

  if((msg = zmsg_new()) == NULL)
    {
      tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
		   "Failed to malloc key lookup message");
      return NULL;
    }

  if(zmsg_addstr(msg, key) != 0)
    {
      tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
		   "Failed to add key to lookup message");
      zmsg_destroy(&msg);
      return NULL;
    }

  /*fprintf(stderr, "DEBUG: Sending request!\n");*/
  /* now hand our message to the execute_request function which will return us a
     message with the response from the appropriate server */
  if((msg =
      execute_request(client, TSMQ_REQUEST_MSG_TYPE_KEY_LOOKUP, &msg)) == NULL)
    {
      /* err will already be set */
      return NULL;
    }

  /* create a new key info structure */
  if((response = malloc(sizeof(tsmq_client_key_t))) == NULL)
    {
      tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
		   "Failed to malloc key response");
      goto err;
    }

  /* decode the message into a new tsmq_client_key_t message */
  /* we expect two frames, one with the server id and one with the key id */
  if((frame = zmsg_pop(msg)) == NULL)
    {
      tsmq_set_err(client->tsmq, TSMQ_ERR_PROTOCOL,
		   "Malformed request response (missing server id)");
      goto err;
    }

  response->server_id_len = zframe_size(frame);
  if((response->server_id = malloc(response->server_id_len)) == NULL)
    {
      tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
		   "Failed to malloc server id");
      goto err;
    }
  memcpy(response->server_id, zframe_data(frame), response->server_id_len);
  zframe_destroy(&frame);

  /* now there should be one more frame with the key id */
  if((frame = zmsg_pop(msg)) == NULL)
    {
      tsmq_set_err(client->tsmq, TSMQ_ERR_PROTOCOL,
		   "Malformed request response (missing key id)");
      goto err;
    }

  response->server_key_id_len = zframe_size(frame);
  if((response->server_key_id = malloc(response->server_key_id_len)) == NULL)
    {
      tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
		   "Failed to malloc server key id");
      goto err;
    }
  memcpy(response->server_key_id, zframe_data(frame),
	 response->server_key_id_len);

  zframe_destroy(&frame);
  zmsg_destroy(&msg);

  return response;

 err:
  tsmq_client_key_free(&response);
  zframe_destroy(&frame);
  zmsg_destroy(&msg);
  return NULL;
}

void tsmq_client_key_free(tsmq_client_key_t **key_p)
{
  tsmq_client_key_t *key = *key_p;

  if(key == NULL)
    {
      return;
    }

  free(key->server_id);
  key->server_id = NULL;
  key->server_id_len = 0;

  free(key->server_key_id);
  key->server_key_id = NULL;
  key->server_key_id_len = 0;

  free(key);

  *key_p = NULL;
}

int tsmq_client_key_set_single(tsmq_client_t *client,
			       tsmq_client_key_t *key,
			       tsmq_val_t value, tsmq_time_t time)
{
  zmsg_t *msg;

  /* body structure will be:
     TIME          (4)
     SERVER_ID     (?)
     SERVER_KEY_ID (?)
     VALUE         (8)
  */

  /* for set multiple...
     TIME
     SERVER_ID
     SERVER_KEY_ID
     VALUE
     SERVER_ID
     SERVER_KEY_ID
     VALUE
     SERVER_ID
     SERVER_KEY_ID
     VALUE
     ... */

  if((msg = zmsg_new()) == NULL)
    {
      tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
		   "Failed to malloc set value message");
      return -1;
    }

  /* add the time */
  if(zmsg_addmem(msg, &time, sizeof(tsmq_time_t)) != 0)
    {
      tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
		   "Failed to add time to message");
      goto err;
    }

  /* add the value */
  if(zmsg_addmem(msg, &value, sizeof(tsmq_val_t)) != 0)
    {
      tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
		   "Failed to add value to message");
      goto err;
    }

  /* add the key */
  if(zmsg_addmem(msg, key->server_key_id, key->server_key_id_len) != 0)
    {
      tsmq_set_err(client->tsmq, TSMQ_ERR_MALLOC,
		   "Failed to add key id to message");
      goto err;
    }

  /*fprintf(stderr, "DEBUG: Sending request!\n");*/
  if((msg = execute_request(client,
                            TSMQ_REQUEST_MSG_TYPE_KEY_SET_SINGLE,
                            &msg)) == NULL)
    {
      /* err will already be set */
      goto err;
    }

  /* the message is empty. is it?? */
  assert(zmsg_size(msg) == 0);
  zmsg_destroy(&msg);
  return 0;

 err:
  zmsg_destroy(&msg);
  return -1;
}
