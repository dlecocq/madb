#ifndef MADB__DB_H
#define MADB__DB_H

#include <string>
#include <vector>

/* C includes */
#include <dirent.h>

/* The default hash funciton */
#include "hash.h"
#include "buffer.h"
#include "traits.h"

#include <boost/filesystem.hpp>

namespace madb {
    /* The first template parameter must be a POD type that you wish to store
     * as the value type associated with each time for the metric. The second
     * argument may be a hash struct that implements:
     *
     *    uint32_t operator()(const char* data, size_t len) const
     */
    template <typename D, typename H=superfast>
    class db {
    public:
        /* Our traits */
        typedef data_traits<D> traits;

        /* "Inherited" traits */
        typedef typename traits::key_type        key_type;
        typedef typename traits::value_type      value_type;
        typedef typename traits::timestamp_type  timestamp_type;
        typedef typename traits::data_type       data_type;
        typedef typename traits::values_type     values_type;
        typedef typename traits::values_map_type values_map_type;

        /* Callback traits */
        typedef typename traits::insert_cb_type  insert_cb_type;
        typedef typename traits::read_cb_type    read_cb_type;
        typedef typename traits::get_cb_type     get_cb_type;

        /* Our hash type */
        typedef          H                       hash_type;

        /* Constructor
         *
         * Open up a database at the provided path with a certain number of
         * open file descriptors.
         *
         * @param base -- where to store the database
         * @param num_files -- how many open file descriptors to use */
        db(const std::string& base, uint32_t num_files):
            path(base), num_files(num_files), hasher(), buffers(num_files) {
            /* If the provided path doesn't end with a slash, it should */
            if (path.length() && path[path.length() - 1] != '/') {
                path = path + "/";
            }

            /* We should also make sure that the directory exists */

            /* Rotate out any existing buffers */
            buffer<D>::rotate(path);

            for (uint32_t i = 0; i < num_files; ++i) {
                buffers[i].mktemp(path);
            }
        }

        /* Destructor */
        ~db() {}

        /* Insert a datapoint synchronously
         *
         * @param name -- name of the metric
         * @param time -- timestamp for the data point
         * @param value -- data point to insert */
        void insert(const key_type& name, timestamp_type time,
            const value_type& value) {
            /* Figure out which buffer this needs to be mapped to */
            uint32_t hashed = hasher(
                name.c_str(), name.length()) % buffers.size();
            buffers[hashed].insert(name, time, value);
        }

        /* Insert a datapoint asynchronously
         *
         * @param name -- name of the metric
         * @param time -- timestamp for the data point
         * @param value -- data point to insert
         * @param cb -- callback to invoke upon completion
         * @param data -- user data to provide to the callback */
        void insert(const key_type& name, timestamp_type time,
            const value_type& value, insert_cb_type cb, void* data) {
            cb(data);
        }

        /* Get data synchronously
         *
         * @param name -- name of the metric
         * @param start -- beginning of the range, inclusive
         * @param end -- end of the range, inclusive */
        values_type get(const key_type& name, timestamp_type start,
            timestamp_type end) {
            uint32_t hashed = hasher(
                name.c_str(), name.length()) % buffers.size();
            return buffers[hashed].get(name, start, end);
        }

        /* Get data asynchronously
         *
         * @param name -- name of the metric
         * @param start -- beginning of the range, inclusive
         * @param end -- end of the range, inclusive
         * @param cb -- user callback
         * @param data -- user data to pass to the callback */
        void get(const key_type& name, timestamp_type start,
            timestamp_type end, get_cb_type cb, void* data) {
            cb(get(name, start, end), data);
        }

        /* Destroy this database */
        void destroy() {
            boost::filesystem::remove_all(path);
        }

        /* Return a list of all the metrics */
        std::vector<key_type> metrics() {
            return slab<value_type>::metrics(path);
        }

        /* Find all the metrics that match a pattern */
        std::vector<key_type> metrics(const std::string& pattern) {
            return slab<value_type>::metrics(path, pattern);
        }
    private:
        /* Private, unimplemented to prevent use */
        db();
        db(const db& other);
        const db& operator=(const db& other);

        /* Members */
        std::string path;        /* Path we're working from */
        uint32_t    num_files;   /* How many open file descriptors to use */
        hash_type   hasher;      /* Hashing function struct */
        std::vector<buffer<value_type> > buffers;
    };
}

#endif
