#ifndef MADB__DB_H
#define MADB__DB_H

#include <string>
#include <vector>

/* C includes */
#include <dirent.h>

/* The default hash funciton */
#include "hash.h"
#include "buffer.h"

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
        /* Some helpful typedefs */
        typedef std::string key_type;
        typedef D           value_type;
        typedef uint32_t    timestamp_type;
        typedef H           hash_type;

        /* A single data point */
        typedef struct data_type_ {
            timestamp_type time;
            value_type     value;
        } data_type;

        /* A list of data points */
        typedef std::vector<data_type> values_type;

        /* Callback types */
        typedef void(* insert_cb_type)(void*);
        typedef void(*    get_cb_type)(const values_type&, void*);

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

            /* At this point, we should consider reading in any existing
             * buffers and rotating them out */
            DIR *dir;
            struct dirent *ent;
            dir = opendir(path.c_str());

            if (dir != NULL) {
                while ((ent = readdir(dir)) != NULL) {
                    std::string s(ent->d_name);
                    if (s.find(".buffer") == 0) {
                        s = path + s;
                        buffer<D>::read(s).rotate();
                        std::cout << s << std::endl;
                    }
                }
                closedir(dir);
            } else {
                std::cerr << "Couldn't read " << path << std::endl;
            }

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
            return values_type();
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
            cb(values_type(), data);
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
