#ifndef MADB__BUFFER_H
#define MADB__BUFFER_H

#include <string>
#include <fstream>
#include <iostream>

/* For mkstemp */
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "traits.h"

namespace madb {
    template <typename D>
    class buffer {
    public:
        /* This file should only grow to 10MB before it gets rotated out */
        static const int32_t max_size = 10 * 1024 * 1024;

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

        /* Open a given path and return a buffer object
         *
         * @param path -- path to the file to open */
        static buffer<D> read(const std::string& pth) {
            buffer<D> buf;

            buf.stream.open(pth.c_str(), std::fstream::binary |
                std::fstream::out |
                std::fstream::in);

            buf.path = pth;

            return buf;
        }

        /* Make a new temporary buffer
         *
         * @param base -- path to the base directory to put the file in */
        void mktemp(const std::string& base_path) {
            /* Close the old file descriptor if necessary */
            close_();

            base = base_path;

            /* Construct a template argument to mkstemp. You can't use a string
             * constant argument, because it tries to replace the contents with
             * the new filename. */
            char* templ = reinterpret_cast<char*>(malloc(
                base.length() + 14));
            strncpy(templ                , base.c_str()   , base.length());
            strncpy(templ + base.length(), ".bufferXXXXXX", 14);
            
            int fd = 0;
            if ((fd = mkstemp(templ)) == -1) {
                std::cout << "Failed to make temporary file" << std::endl;
                free(templ);
                exit(1);
            }

            /* Save out our path, close file descriptor after opening path */
            path = templ;
            stream.open(templ, std::fstream::binary | std::fstream::out |
                std::fstream::in);
            free(templ);
            close(fd);
        }

        /* Default constructor
         *
         * Opens nothing, sits idle */
        buffer(): stream(), path(""), base(""), written(0) {}

        /* Copy constructor */
        buffer(const buffer& other): stream(), path(""), base(other.base),
            written(0) {
            /* Empty */
        }

        /* Destructor */
        ~buffer() {
            close_();
        }

        /* Rotate out the current buffer file for a new one
         *
         * @returns 0 on success, else -1 */
        int rotate() {
            if (!stream.is_open()) {
                return 0;
            }

            //std::cout << "Rotating " << path << std::endl;

            /* Afterwards, remove the file */
            if (remove(path.c_str()) != 0) {
                perror("Failed to remove path");
                return -1;
            }

            /* Open up a new file descriptor */
            close_();

            mktemp(base);

            return 0;
        }

        /* Write a data point to the file */
        int insert(const key_type& key, timestamp_type time,
            const value_type& val) {

            /* Length of string */
            size_t len = key.length();
            stream.write(reinterpret_cast<char*>(&len), sizeof(size_t));

            /* And then the key */
            stream << key;

            /* And now the time */
            data_type datum;
            datum.time = time;
            datum.value = val;
            stream.write(reinterpret_cast<char*>(&datum), sizeof(data_type));

            written += sizeof(size_t) + key.length() + sizeof(data_type);

            /* If this has exceeded our write size, then rotate... */
            if (written < max_size) {
                return 0;
            }

            return rotate();
        }

        /* Get all the data in this file synchronously
         *
         * @returns a mapping from metric names to their data points within the
         * buffer */
        values_map_type read() {
            values_map_type results;

            /* This is a buffer, and admittedly a weakpoint */
            char      key[1024];
            size_t    len = 0;
            data_type datum;

            /* Seek to the beginning of the file for reading */
            stream.seekg(0, std::fstream::beg);

            /* Now, while the file descriptor is good, keep reading */
            while (!stream.eof()) {
                stream.read(reinterpret_cast<char*>(&len), sizeof(size_t));
                stream.read(key, len);
                stream.read(reinterpret_cast<char*>(&datum), sizeof(datum));
                results[std::string(key, len)].push_back(datum);
            }

            return results;
        }

        /* Get all the data in this file asynchronously
         *
         * @param cb -- user callback
         * @param data -- user data to pass to the callback */
        void read(read_cb_type cb, void* data) {
            cb(read(), data);
        }

        /* Get data synchronously
         *
         * @param name -- name of the metric
         * @param start -- beginning of the range, inclusive
         * @param end -- end of the range, inclusive */
        values_type get(const key_type& name, timestamp_type start,
            timestamp_type end) {
            /* First, read in all the data points for this metric */
            values_type all(read()[name]);

            /* And then prepare the response */
            values_type results;

            typename values_type::iterator it(all.begin());
            for (; it != all.end(); ++it) {
                if (it->time <= end && it->time >= start) {
                    results.push_back(*it);
                }
            }
            return results;
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
    private:
        /* Private, unimplemented to avoid use */
        const buffer& operator=(const buffer& other);

        /* Members */
        std::fstream stream;    /* Our file stream */
        std::string  path;      /* Path of our filename */
        std::string  base;      /* Our base path */
        int          written;   /* How many bytes have been written */

        /* Close up our current file descriptor */
        void close_() {
            path = "";
            stream.close();
        }
    };
}

#endif
