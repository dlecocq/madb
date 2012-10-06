madb
====
The `Mostly Appends DB`

This is a database geared at storing time-series data. In particular, data that
is meant to be thinned out over time. For example, keeping 1-second resolution
for the last hour, 30-second resolution for the last day, and 5-minute
resolution for the last week.

There are a few assumptions about this problem space that inform the design:

- Inserts are almost always done in ascending order
- Higher sampling rates are more important for recent data points
- Most of the data that will get written to this database will never be read
- We need to support a very high write rate

Design
======
The database is header-only, and templated to accept a POD datatype as what's
stored in the time-series. The main class is `madb::db`:

    #include "db.h"

    typedef struct foo_ {
        uint32_t a;
        uint32_t b;
    } foo;

    /* Open up a database */
    madb::db<foo> db("path/to/database", 128);

The database contains a few important typedefs:

    /* The type associated with all the metric keys (std::string) */
    madb::db<foo>::key_type;

    /* Your provided template, essentially just 'foo' */
    madb::db<foo>::value_type;

    /* The format of the timestamp (uint32_t) */
    madb::db<foo>::timestamp_type;

    /* A struct with a timestamp and a value_type */
    madb::db<foo>::data_type;

The database keeps a number of `buffer`s open, and when a new data point is
added, the key for that metric is hashed to one of these open buffers and
inserted. Once the buffer is full enough, all the of the data points in that
file are placed into files dedicated to a particular metric. The underlying
directory structure looks something like this:

    # Where all the buffers are stored
    /path/to/database/
        buffer/
            ...
            .buffer.cd48d1
            .buffer.ce1802
            .buffer.cf1540
            ...
        metrics/
            foo/                        # A metric called 'foo'
                ...
                bar/                    # A metric called 'foo/bar'
                    ...
                baz/                    # A metric called 'foo/baz'
                    ...
            whiz/                       # A metric called 'whiz'
                latest                  # All the latest data points for whiz
                1349558019              # Data points before this timestamp
                1349472912              # Data points before this timestamp

When a `buffer` is `rotate`d out, all the data points for the metrics stored in
the buffer are written out to `/path/to/database/metrics/<metric_name>/latest`.
When that file gets sufficiently full, it is renamed to the highest timestamp
in that series of data points.

If a metric has files `time0`, `time1`, `time2`, `time3` and `latest`, then the
following must be true:

- A data point in `latest` may have any timestamp
- Otherwise, any data point occurring before or at `time0` appears there
- Otherwise, any data point between `time-k` and `time-k+1` appears in the file
    named `time-(k+1)`

Thus, in order to get all the datapoints for a time range for a metric, there
are just a few files in which we have to look: 1) the buffer that metric maps
to, 2) the `latest` file for that metric and 3) any relevant timestamp files.

Functionality Roadmap
=====================
The next few things I have on my docket for this:

- _quickly_ list all metrics in the database
- callbacks to subscribe to any updates for a metric
- callbacks to subscribe to any new metric names
- callbacks to subscribe to any deleted metric name
- alarm callbacks (if X goes above a certain threshold)
- stale callbacks (if X goes without a new data point for Y seconds)
- data pruning -- applying an expiration policy

Performance Roadmap
===================
Aside from the functionality I'd like to add, there are also performance
considerations. I'd consider it a success if:

    With 6M metrics, I can hit the 100k insertions per second rate. That
    means being able to support those 6M metrics at a rate of one data point
    per minute.

    This is mostly because I want to have very high limits on the number of
    discrete metrics collected, without leaving the 1 data point / minute range
    for performance. For some purposes, 100k metrics every second, or 1M
    metrics every 10 seconds might be more attractive. Sampling less than once
    per minute seems hard to make useful for most purposes, though.

My 'bonus goal' would be to hit the ~10M different metric mark with 1M data
points per second.

Some ideas that I've been toying with to reach this goal:

- Heavy use of `mmap` for the buffers before they get rotated out. That might
    help when reading the buffer back in before writing data out to the
    per-metric slabs.
- Better asynchronous support with `libuv`. For the time being, I'm using
    `std::fstream` because it is roughly fast enough. I'm maintining both a
    synchronous and asynchronous API so that asynchronous consumers using
    `libuv` can immediately take advantage of any performance boost that would
    come out of it. For the time being, both APIs run the same code.
- Using a thread-pool. This is one option to using full asynchronous support.
    The difficulty with using the asynchronous `libuv` API is that it quickly
    turns into callback and baton hell. Especially when doing things like
    iterating over directories, opening and closing files, etc. The thread pool
    would ease much of that pain, though. How much performance it would gain
    back remains to be seen.