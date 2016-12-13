/*
 * libtimeseries
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of libtimeseries.
 *
 * libtimeseries is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libtimeseries is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libtimeseries.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "config.h"

#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <db.h> // for DB_LOCK_DEADLOCK
#include <dbats.h>

#include "utils.h"
#include "wandio_utils.h"

#include "timeseries_backend_int.h"
#include "timeseries_kp_int.h"
#include "timeseries_log_int.h"
#include "timeseries_backend_dbats.h"

#define BACKEND_NAME "dbats"

/* map the dbats flags to strings */
#define FLAG_UNCOMPRESSED     "uncompressed"
#define FLAG_EXCLUSIVE        "exclusive"
#define FLAG_NO_TXN           "no-txn"
#define FLAG_UPDATABLE        "updatable"

#define STATE(provname)				\
  (TIMESERIES_BACKEND_STATE(dbats, provname))

/** The basic fields that every instance of this backend have in common */
static timeseries_backend_t timeseries_backend_dbats = {
  TIMESERIES_BACKEND_ID_DBATS,
  BACKEND_NAME,
  TIMESERIES_BACKEND_GENERATE_PTRS(dbats)
};

/** Holds the state for an instance of this backend */
typedef struct timeseries_backend_dbats_state {
  /** Path to the DBATS database we are writing to */
  char *dbats_path;

  /** Pointer to a DBATS handler object */
  dbats_handler *dbats_handler;

  /** Flags to be passed to dbats_open */
  uint32_t dbats_flags;

  /** The number of values received for the current bulk set */
  uint32_t bulk_cnt;

  /** The time for the current bulk set */
  uint32_t bulk_time;

  /** The expected number of values in the current bulk set */
  uint32_t bulk_expect;

  /** The snapshot for the current bulk set */
  dbats_snapshot *bulk_snap;

} timeseries_backend_dbats_state_t;

/** Print usage information to stderr */
static void usage(timeseries_backend_t *backend)
{
  fprintf(stderr,
	  "backend usage: %s [-f flag [-f flag]] -p path\n"
	  "       -f <flag>     flag(s) to use when opening database\n"
	  "                       - "FLAG_UNCOMPRESSED"\n"
	  "                       - "FLAG_EXCLUSIVE"\n"
	  "                       - "FLAG_NO_TXN"\n"
	  "                       - "FLAG_UPDATABLE"\n"
	  "                       (see DBATS documentation for more info)\n"
	  "       -p <path>     path to an existing DBATS database directory\n",
	  backend->name);
}


/** Parse the arguments given to the backend */
static int parse_args(timeseries_backend_t *backend, int argc, char **argv)
{
  timeseries_backend_dbats_state_t *state = STATE(backend);
  int opt;

  assert(argc > 0 && argv != NULL);

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  /* remember the argv strings DO NOT belong to us */

  while((opt = getopt(argc, argv, ":f:p:?")) >= 0)
    {
      switch(opt)
	{

	case 'f':
	  if(strncmp(optarg, FLAG_UNCOMPRESSED, strlen(FLAG_UNCOMPRESSED)) == 0)
	    {
	      state->dbats_flags |= DBATS_UNCOMPRESSED;
	    }
	  else if(strncmp(optarg, FLAG_EXCLUSIVE, strlen(FLAG_EXCLUSIVE)) == 0)
	    {
	      state->dbats_flags |= DBATS_EXCLUSIVE;
	    }
	  else if(strncmp(optarg, FLAG_NO_TXN, strlen(FLAG_NO_TXN)) == 0)
	    {
	      state->dbats_flags |= DBATS_NO_TXN;
	    }
	  else if(strncmp(optarg, FLAG_UPDATABLE, strlen(FLAG_UPDATABLE)) == 0)
	    {
	      state->dbats_flags |= DBATS_UPDATABLE;
	    }
	  else
	    {
	      fprintf(stderr, "ERROR: Invalid DBATS flag specified (%s)\n",
		      optarg);
	      usage(backend);
	      return -1;
	    }
	  break;

	case 'p':
	  state->dbats_path = strdup(optarg);
	  break;

	case '?':
	case ':':
	default:
	  usage(backend);
	  return -1;
	}
    }

  if(state->dbats_path == NULL)
    {
      fprintf(stderr, "ERROR: DBATS path must be specified with -p\n");
      usage(backend);
      return -1;
    }

  return 0;
}

/* ===== PUBLIC FUNCTIONS BELOW THIS POINT ===== */

timeseries_backend_t *timeseries_backend_dbats_alloc()
{
  return &timeseries_backend_dbats;
}

int timeseries_backend_dbats_init(timeseries_backend_t *backend,
				 int argc, char ** argv)
{
  timeseries_backend_dbats_state_t *state;

  /* allocate our state */
  if((state = malloc_zero(sizeof(timeseries_backend_dbats_state_t)))
     == NULL)
    {
      timeseries_log(__func__,
		  "could not malloc timeseries_backend_dbats_state_t");
      return -1;
    }
  timeseries_backend_register_state(backend, state);

  /* parse the command line args */
  if(parse_args(backend, argc, argv) != 0)
    {
      return -1;
    }

  /* can we open the dbats db now?? */
  /* the two parameters that we specify with 0's are only used when creating a
     DB */
  if(dbats_open(&state->dbats_handler, state->dbats_path,
		0, 0, state->dbats_flags, 0644) != 0)
    {
      fprintf(stderr, "ERROR: failed to open DBATS database (%s)\n",
	      state->dbats_path);
      usage(backend);
      return -1;
    }

  /* we have no config to do, so go ahead and commit the dbats_open txn */
  if(dbats_commit_open(state->dbats_handler) != 0)
    {
      fprintf(stderr, "ERROR: failed to open DBATS database (%s)\n",
	      state->dbats_path);
      usage(backend);
      return -1;
    }

  /* ready to rock n roll */

  return 0;
}

void timeseries_backend_dbats_free(timeseries_backend_t *backend)
{
  timeseries_backend_dbats_state_t *state = STATE(backend);
  if(state != NULL)
    {
      if(state->dbats_path != NULL)
	{
	  free(state->dbats_path);
	  state->dbats_path = NULL;
	}

      if(state->bulk_snap != NULL)
	{
	  dbats_abort_snap(state->bulk_snap);
	  state->bulk_snap = NULL;
	}

      if(state->dbats_handler != NULL)
	{
	  dbats_close(state->dbats_handler);
	  state->dbats_handler = NULL;
	}

      state->dbats_flags = 0;

      timeseries_backend_free_state(backend);
    }
  return;
}

int timeseries_backend_dbats_kp_init(timeseries_backend_t *backend,
                                     timeseries_kp_t *kp,
                                     void **kp_state_p)
{
  /* we do not need any state */
  assert(kp_state_p != NULL);
  *kp_state_p = NULL;
  return 0;
}

void timeseries_backend_dbats_kp_free(timeseries_backend_t *backend,
                                      timeseries_kp_t *kp,
                                      void *kp_state)
{
  /* we did not allocate any state */
  assert(kp_state == NULL);
  return;
}

int timeseries_backend_dbats_kp_ki_update(timeseries_backend_t *backend,
					  timeseries_kp_t *kp)
{
  timeseries_backend_dbats_state_t *state = STATE(backend);
  timeseries_kp_ki_t *ki = NULL;
  int id;
  uint32_t *dbats_id;

  /* foreach KI, if the backend state is null, get the key id */
  TIMESERIES_KP_FOREACH_KI(kp, ki, id)
    {
      if(timeseries_kp_ki_enabled(ki) == 0 ||
         timeseries_kp_ki_get_backend_state(ki, TIMESERIES_BACKEND_ID_DBATS)
         != NULL)
        {
          continue;
        }

      if((dbats_id = malloc(sizeof(uint32_t))) == NULL)
        {
          timeseries_log(__func__, "Could not allocate DBATS Key");
	  return -1;
        }

      /* lookup this key */
      /** @todo bulk key lookup */
      if(dbats_get_key_id(state->dbats_handler, NULL,
                          timeseries_kp_ki_get_key(ki),
                          dbats_id, DBATS_CREATE) != 0)
        {
          timeseries_log(__func__, "Could not resolve DBATS key ID");
	  return -1;
        }

      timeseries_kp_ki_set_backend_state(ki, TIMESERIES_BACKEND_ID_DBATS,
                                         dbats_id);
    }
  return 0;
}

void timeseries_backend_dbats_kp_ki_free(timeseries_backend_t *backend,
                                       timeseries_kp_t *kp,
				       timeseries_kp_ki_t *ki,
                                       void *ki_state)
{
  /* ki_state is a (uint32_t*) */
  free(ki_state);
  return;
}

int timeseries_backend_dbats_kp_flush(timeseries_backend_t *backend,
				      timeseries_kp_t *kp,
				      uint32_t time)
{
  timeseries_backend_dbats_state_t *state = STATE(backend);
  dbats_snapshot *snapshot;
  dbats_value val;
  int rc;
  timeseries_kp_ki_t *ki = NULL;
  int id;
  uint32_t *dbats_id;

  /* we re-enter here if the set deadlocks */
 retry:

  /* select the snapshot */
  if(dbats_select_snap(state->dbats_handler, &snapshot, time, 0) != 0)
    {
      timeseries_log(__func__, "dbats_select_snap failed");
      return -1;
    }

  TIMESERIES_KP_FOREACH_KI(kp, ki, id)
    {
      if(timeseries_kp_ki_enabled(ki) == 0)
        {
          continue;
        }

      dbats_id = (uint32_t*)
        timeseries_kp_ki_get_backend_state(ki, TIMESERIES_BACKEND_ID_DBATS);

      val.u64 = timeseries_kp_ki_get_value(ki);
      if((rc = dbats_set(snapshot, *dbats_id, &val)) != 0)
	{
	  dbats_abort_snap(snapshot);
	  if(rc == DB_LOCK_DEADLOCK)
	    {
	      timeseries_log(__func__, "deadlock in dbats_set");
	      goto retry;
	    }
	  timeseries_log(__func__, "dbats_set failed");
	  return -1;
	}
    }

  /* now we commit the snapshot */
  if((rc = dbats_commit_snap(snapshot)) != 0)
    {
      if(rc == DB_LOCK_DEADLOCK)
	{
	  timeseries_log(__func__, "deadlock in dbats_commit_snap");
	  goto retry;
	}
      timeseries_log(__func__, "dbats_commit_snap failed");
      return -1;
    }

  return 0;
}

int timeseries_backend_dbats_set_single(timeseries_backend_t *backend,
					const char *key,
					uint64_t value,
					uint32_t time)
{
  timeseries_backend_dbats_state_t *state = STATE(backend);
  dbats_snapshot *snapshot;
  dbats_value val;
  int rc;

  /* we re-enter here if the set deadlocks */
 retry:

  /* select the snapshot */
  if(dbats_select_snap(state->dbats_handler, &snapshot, time, 0) != 0)
    {
      timeseries_log(__func__, "dbats_select_snap failed");
      return -1;
    }

  /* set the value */
  if((rc = dbats_set_by_key(snapshot, key, &val, DBATS_CREATE)) != 0)
    {
      dbats_abort_snap(snapshot);
      if(rc == DB_LOCK_DEADLOCK)
	{
	  timeseries_log(__func__, "deadlock in dbats_set_by_key");
	  goto retry;
	}
      timeseries_log(__func__, "dbats_set_by_key failed");
      return -1;
    }

  /* commit the snapshot */
  if((rc = dbats_commit_snap(snapshot)) != 0)
    {
      if(rc == DB_LOCK_DEADLOCK)
	{
	  timeseries_log(__func__, "deadlock in dbats_commit_snap");
	  goto retry;
	}
      timeseries_log(__func__, "dbats_commit_snap failed");
      return -1;
    }

  return 0;
}

int timeseries_backend_dbats_set_single_by_id(timeseries_backend_t *backend,
                                              uint8_t *id, size_t id_len,
                                              uint64_t value, uint32_t time)
{
  timeseries_backend_dbats_state_t *state = STATE(backend);
  dbats_snapshot *snapshot;
  uint32_t dbats_id;
  dbats_value val;
  int rc;

  /* we re-enter here if the set deadlocks */
 retry:

  /* select the snapshot */
  if(dbats_select_snap(state->dbats_handler, &snapshot, time, 0) != 0)
    {
      timeseries_log(__func__, "dbats_select_snap failed");
      return -1;
    }

  /* set the value */
  assert(id_len == sizeof(uint32_t));
  memcpy(&dbats_id, id, sizeof(uint32_t));
  val.u64 = value;
  if((rc = dbats_set(snapshot, dbats_id, &val)) != 0)
    {
      dbats_abort_snap(snapshot);
      if(rc == DB_LOCK_DEADLOCK)
	{
	  timeseries_log(__func__, "deadlock in dbats_set_by_key");
	  goto retry;
	}
      timeseries_log(__func__, "dbats_set_by_key failed");
      return -1;
    }

  /* commit the snapshot */
  if((rc = dbats_commit_snap(snapshot)) != 0)
    {
      if(rc == DB_LOCK_DEADLOCK)
	{
	  timeseries_log(__func__, "deadlock in dbats_commit_snap");
	  goto retry;
	}
      timeseries_log(__func__, "dbats_commit_snap failed");
      return -1;
    }

  return 0;
}

int timeseries_backend_dbats_set_bulk_init(timeseries_backend_t *backend,
                                           uint32_t key_cnt, uint32_t time)
{
  timeseries_backend_dbats_state_t *state = STATE(backend);
  assert(state->bulk_expect == 0 && state->bulk_cnt == 0 &&
	 state->bulk_snap == NULL);

  state->bulk_expect = key_cnt;
  state->bulk_time = time;

  /* select the snapshot */
  if(dbats_select_snap(state->dbats_handler, &state->bulk_snap, time, 0) != 0)
    {
      timeseries_log(__func__, "dbats_select_snap failed");
      return -1;
    }

  return 0;
}

int timeseries_backend_dbats_set_bulk_by_id(timeseries_backend_t *backend,
                                            uint8_t *id, size_t id_len,
                                            uint64_t value)
{
  timeseries_backend_dbats_state_t *state = STATE(backend);
  uint32_t *dbats_id;
  dbats_value val;
  int rc;

  assert(state->bulk_expect > 0);

  assert(id_len == sizeof(uint32_t));
  dbats_id = (uint32_t*)id;

  val.u64 = value;
  if((rc = dbats_set(state->bulk_snap, *dbats_id, &val)) != 0)
    {
      dbats_abort_snap(state->bulk_snap);
      if(rc == DB_LOCK_DEADLOCK)
	{
	  timeseries_log(__func__, "deadlock in dbats_set");
          /* can't retry, just fail */
	}
      timeseries_log(__func__, "dbats_set failed");
      return -1;
    }

  if(++state->bulk_cnt == state->bulk_expect)
    {
      /* commit the snapshot */
      if((rc = dbats_commit_snap(state->bulk_snap)) != 0)
	{
	  if(rc == DB_LOCK_DEADLOCK)
	    {
	      timeseries_log(__func__, "deadlock in dbats_commit_snap");
              /* can't retry, just fail */
	    }
	  timeseries_log(__func__, "dbats_commit_snap failed");
	  return -1;
	}

      state->bulk_cnt = 0;
      state->bulk_time = 0;
      state->bulk_expect = 0;
      state->bulk_snap = NULL;
    }
  return 0;
}

size_t timeseries_backend_dbats_resolve_key(timeseries_backend_t *backend,
                                            const char *key,
                                            uint8_t **backend_key)
{
  timeseries_backend_dbats_state_t *state = STATE(backend);
  uint32_t *dbats_id = NULL;

  if((dbats_id = malloc(sizeof(uint32_t))) == NULL)
    {
      timeseries_log(__func__, "Could not allocate DBATS Key");
      return -1;
    }

  if(dbats_get_key_id(state->dbats_handler, NULL, key,
		      dbats_id, DBATS_CREATE) != 0)
    {
      timeseries_log(__func__, "Could not resolve DBATS key ID");
      return -1;
    }

  *backend_key = (uint8_t*)dbats_id;

  return sizeof(uint32_t);
}

int timeseries_backend_dbats_resolve_key_bulk(timeseries_backend_t *backend,
                                              uint32_t keys_cnt,
                                              const char * const *keys,
                                              uint8_t **backend_keys,
                                              size_t *backend_key_lens,
                                              int *contig_alloc)
{
  timeseries_backend_dbats_state_t *state = STATE(backend);
  int i;

  uint32_t *dbats_ids = NULL;
  uint32_t dbats_keys_cnt = keys_cnt;

  int retries = 60; // hax until we get deadlock retries into DBATS
  int rc;

  /** allocate an array of uint32s to store the result from dbats */
  if((dbats_ids = malloc(sizeof(uint32_t)*keys_cnt)) == NULL)
    {
      timeseries_log(__func__, "Could not allocate DBATS Key array");
      return -1;
    }

  /* ask dbats to do the lookup */
  do {
    rc = dbats_bulk_get_key_id(state->dbats_handler, NULL, &dbats_keys_cnt, keys,
                             dbats_ids, DBATS_CREATE);
    if(rc != 0)
      {
        retries--;
        if(retries == 0)
          {
            timeseries_log(__func__,
                           "Could not resolve DBATS key IDs after 60 retries");
            return -1;
          }
        else
          {
            timeseries_log(__func__,
                           "Retrying key lookup for %"PRIu32" keys");
          }
      }
  } while (rc != 0);

  assert(dbats_keys_cnt == keys_cnt);

  for(i=0; i<keys_cnt; i++)
    {
      backend_keys[i] = (uint8_t*)(&dbats_ids[i]);
      backend_key_lens[i] = sizeof(uint32_t);
    }

  assert(contig_alloc != NULL);
  *contig_alloc = 1;

  return 0;
}
