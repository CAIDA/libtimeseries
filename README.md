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
 - TSK: Time Series Kafka (`kafka`)

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
backend is normally used in conjunction with TSK to handle time series
transport.

### Kafka Backend

TODO

## Requirements

 - wandio (http://research.wand.net.nz/software/libwandio.php)
 - DBATS (http://www.caida.org/tools/utilities/dbats)
 - librdkafka (https://github.com/edenhill/librdkafka)

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

 - `tsk-proxy`
    Server to proxy time series data from a TSK instance to any
    libtimeseries backend. This is usually used to write data from TSK
    into a DBATS database.

## License

libtimeseries is released under the GPL v3 license (see `COPYING` for more
information)
