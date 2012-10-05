#ifndef MADB__BUFFER_H
#define MADB__BUFFER_H

#include <string>
#include <fstream>
#include <iostream>

/* Internal import */
#include "slab.h"
#include "traits.h"

/* I am somewhat loathe to do this, but alas, I must */
#include <boost/filesystem.hpp>

namespace madb {
    template <typename D>
    class buffer {
    public:
        /* This file should only grow to 10MB before it gets rotated out */
        static const int32_t max_size = 5 * 1024 * 1024;

        /* To mode with which we open files */
        static const typename std::fstream::openmode open_mode =
            static_cast<std::fstream::openmode>(
                static_cast<int>(std::fstream::binary) |
                static_cast<int>(std::fstream::out)    |
                static_cast<int>(std::fstream::app)    |
                static_cast<int>(std::fstream::in));

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

        /* Default constructor
         *
         * Opens nothing, sits idle */
        buffer(): stream(), path(""), base(""), written(0) {}

        /* Open a file
         *
         * @param path -- the path to open
         * @param base -- base of the buffer directory to open */
        buffer(const std::string& path, const std::string& base):
            stream(path.c_str(), open_mode), path(path), base(base),
            written(0) {
            /* Read how far along in the file we are */
            written = stream.tellp();
        }

        /* Copy constructor */
        buffer(const buffer& other): stream(other.path.c_str()),
            path(other.path),base(other.base), written(other.written) {}

        /* Destructor */
        ~buffer() {
            close();
        }

        /* Make a new temporary buffer
         *
         * @param base -- path to the base directory to put the file in */
        void mktemp(const std::string& base_path) {
            /* Close the old file descriptor if necessary */
            close();

            /* Save out our new base path */
            base = base_path;

            /* Now, let's get a unique name */
            boost::filesystem::path new_path(base_path);
            /* Make sure the directory we need for the buffers exists, and then
             * generate a new, unique path */
            new_path /= "buffers";
            boost::filesystem::create_directories(new_path);
            new_path /= ".buffer.%%%%%%";
            new_path = boost::filesystem::unique_path(new_path);
            
            /* And then let's open up our stream */
            path = new_path.string();
            std::cout << "Opening up new path " << path << std::endl;
            stream.open(path.c_str(), open_mode);
            written = 0;
        }

        /* Take all the data out of the current buffer and write it out to all
         * of the files where they belong */
        int dump() {
            /* Make sure it's open */
            std::cout << "Dumping " << path << std::endl;
            if (!stream.is_open()) {
                std::cout << "Not open..." << std::endl;
                return 0;
            }

            /* Now, we should read in everything, iterate through it and write
             * out the contents to the files we should write out to */
            values_map_type results(read());
            typename values_map_type::iterator it(results.begin());
            for (; it != results.end(); ++it) {
                //std::cout << "Reading " << it->first << std::endl;
                slab<D>(base, it->first).insert(
                    it->second.begin(), it->second.end());
            }

            /* Afterwards, remove the file */
            if (!boost::filesystem::remove(path)) {
                perror("Failed to remove path");
                return -1;
            }

            /* Open up a new file descriptor */
            close();

            return 0;
        }

        /* Rotate out the current buffer file for a new one
         *
         * @returns 0 on success, else -1 */
        int rotate() {
            dump();
            mktemp(base);
            return 0;
        }

        /* Rotate out all the old buffer files in the provided path
         *
         * @param db_path -- path to the database's directory */
        static void rotate(const std::string& db_path) {
            boost::filesystem::path buffers_path(db_path);
            buffers_path /= "buffers";
            std::cout << "Rotate(" << buffers_path.string() << ")"
                << std::endl;

            if (boost::filesystem::is_directory(buffers_path)) {
                boost::filesystem::directory_iterator it(buffers_path);
                boost::filesystem::directory_iterator it_end;

                for (; it != it_end; ++it) {
                    buffer(it->path().string(), db_path).dump();
                }
            }
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
            int count = 0;
            while (count < written) {
                stream.read(reinterpret_cast<char*>(&len), sizeof(size_t));
                stream.read(key, len);
                stream.read(reinterpret_cast<char*>(&datum), sizeof(datum));
                results[std::string(key, len)].push_back(datum);
                count += (sizeof(size_t) + len + sizeof(datum));
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
            values_type results(slab<D>(base, name).get(start, end));

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
        void close() {
            path = "";
            stream.close();
        }
    };
}

#endif
