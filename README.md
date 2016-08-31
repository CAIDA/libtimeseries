# libtimeseries

libtimeseries is a C library that provides a high-performance abstraction layer
for efficiently writing to time series databases. It is optimized for writing
values for many time series simultaneously.

libtimeseries has two main components:
 - Key Package
 - Time Series Backend(s)

## Key Packages

The Key Package can be thought of as a table of key/value pairs, where the keys
are strings, and the values are 64 bit unsigned integers (support for other
value types is planned for a future release). Using the Key Package allows
libtimeseries to optimize writes by pre-fetching and caching internal key IDs. A
Key package is reused by updating the values for keys and then "flushing" to a
backend. The flush operation associates a timestamp with the key/values,
creating a "point" for each time series represented by the keys of the Key
Package.

## Backends

Time series backends are pluggable components that implement the libtimeseries
backend API and provide write support for a specific time series database
implementation.

Currently supported backends are:
 - Graphite ASCII format (`ascii`)
 - DBATS: DataBase of Aggregated Time Series (`dbats`)
 - TSMQ: Time Series Message Queue

### ASCII Backend
The ASCII backend simply writes the time series data to `stdout` in the Graphite
ASCII format (https://graphiteapp.org/quick-start-guides/feeding-metrics.html):

```
<key1> <value1> <epoch-seconds>
<key2> <value2> <epoch-seconds>
...
```

e.g.:
```
app.foo.my_counter 320 1472669180
app.foo.my_timer 100 1472669180
...
```

### DBATS Backend

The DBATS backend uses `libdbats` (http://www.caida.org/tools/utilities/dbats)
to write time series data to a DBATS database. Note that to use this backend,
the DBATS database must be on a locally-accessible file-system, and as such this
backend is normally used in conjunction with TSMQ (see below).

### TSMQ Backend

The TSMQ backend uses `libtsmq` (included in this release) to transmit time
series data to a (remote) server (`tsmq-server`) via a broker service
(`tsmq-broker`). TSMQ uses CZMQ (http://czmq.zeromq.org/) to provide distributed
messaging, normally via TCP.

#### tsmq-broker

The TSMQ broker (`tools/tsmq-broker`) is a service that uses CZMQ to listen for
clients on one port, a server on another, and then simply proxies messages
between the clients and the server. If multiple clients are connected, it will
read from each in a round-robin fashion. Note: currently only **one** server is
supported. Connecting additional servers will trigger an exception.

#### tsmq-server

The TSMQ server (`tools/tsmq-server`) uses CZMQ to connect to a broker service,
and then using libtimeseries writes timeseries data to configured
backend(s). A common usage configuration for TSMQ is as follows:

```
--------------------------------------------------[Analysis Machine]
                  <analysis-tool>
             libtimeseries (tsmq backend)
--------------------------------------------------
                       [TCP]
--------------------------------------------------[Broker Machine]
                     tsmq-broker
--------------------------------------------------
                       [TCP]
--------------------------------------------------[DB Machine]
   tsmq-server + libtimeseries (dbats backend)
                      DBATS DB
--------------------------------------------------
```

#### Caveats

Because the version of TSMQ included is an early alpha release, there must be a
one-to-one relationship between `tsmq-server` and `tsmq-broker`. Also, the
`tsmq-server` process *must* be started before the `tsmq-broker` process.


## Requirements

 - wandio (http://research.wand.net.nz/software/libwandio.php)
 - DBATS (http://www.caida.org/tools/utilities/dbats)
 - ZMQ/CZMQ (http://czmq.zeromq.org/)

## Building

To build and install libtimeseries
```
./configure [options]
make
make install
```

Most of the GNU standard configure options and make targets are also
available.

## API Documentation

See `lib/timeseries_pub.h`, `lib/timeseries_backend_pub.h` and
`lib/timeseries_kp_pub.h` for API documentation.

Also, `tools/timeseries-insert.c` provides a simple example of how to use the
API to write timeseries data.

## Programs

Run any program with the `-?` option for a list of options.

 - `timeseries-insert`
    Simple command-line tool to write time series data (input should be in the
    Graphite format described above).

 - `tsmq-broker`, `tsmq-server`
    See the description of TSMQ above.


## License

libtimeseries is released under the GPL v3 license (see `COPYING` for more
information)
