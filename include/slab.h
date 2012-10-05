#ifndef MADB__SLAB_H
#define MADB__SLAB_H

#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>

/* Internal imports */
#include "traits.h"

/* I am somewhat loathe to do this, but alas, I must */
#include <boost/filesystem.hpp>

namespace madb {
    template <typename D>
    class slab {
    public:
        /* Each slab should only grow to this size before it's rotated out */
        static const int32_t max_size = 1 * 1024 * 1024;

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

        /* This is a one-off functor for filtering results */
        struct range_filter {
            timestamp_type start;
            timestamp_type end;

            range_filter(timestamp_type start, timestamp_type end):
                start(start), end(end) {};

            bool operator()(const data_type& d) {
                /* Should we remove it? */
                return (d.time < start) || (d.time > end);
            }
        };

        /* Constructor
         *
         * @param base -- base path for storing the database
         * @param name -- name of the metric */
        slab(const std::string& base, const std::string& name):
            base(base), name(name), stream() {
            /* First, we have to make sure that directory we're going to be
             * writing to exists, and then open a stream to it for reading and
             * writing */
            boost::filesystem::create_directories(directory());
            stream.open(latest_path().c_str(), open_mode);
        }

        ~slab() {
            stream.close();
        }

        /* Write a data point to the file
         *
         * @param datum -- piece of data to write out */
        int insert(const data_type& datum) {
            stream.write(reinterpret_cast<char*>(const_cast<data_type*>(&datum)), sizeof(data_type));
            /* Increment written, check if we need to rotate to new slab */
            written += sizeof(data_type);
            if (written < max_size) {
                return 0;
            }
            return rotate();
        }

        /* Write a data point to the file
         *
         * @param time -- timestamp associated with the data point
         * @param val -- actual value for that timestamp */
        int insert(timestamp_type time, const value_type& val) {
            /* Insert the new data point to the latest slab, and then check to
             * see if it needs to be rotated out */
            data_type datum;
            datum.time  = time;
            datum.value = val;
            return insert(datum);
        }

        /* Insert a whole range of values into the slab
         *
         * @param start -- beginning of range
         * @param end -- end of range */
        int insert(typename values_type::iterator start,
            typename values_type::iterator end) {
            typename values_type::iterator it(start);
            for (; it != end; ++it) {
                insert(*it);
            }
            return 0;
        }

        /* Get data synchronously
         *
         * @param start -- beginning of the range, inclusive
         * @param end -- end of the range, inclusive */
        values_type get(timestamp_type start, timestamp_type end) {
            /* First thing we have to do is to iterate through the directory
             * to try to find all the slabs within it, and read any that fit
             * in the time range */
            std::vector<timestamp_type> slabs_(slabs());

            /* Aggregate all of our results */
            values_type results(read());

            /* And now each of the slabs */
            typename std::vector<timestamp_type>::iterator it(slabs_.begin());
            for (; it != slabs_.end(); ++it) {
                values_type tmp(read(timestamp_path(*it)));
                results.insert(results.end(), tmp.begin(), tmp.end());
            }

            std::remove_if(
                results.begin(), results.end(), range_filter(start, end));

            std::sort(results.begin(), results.end());
            
            return results;
        }

        /* Get data asynchronously
         *
         * @param start -- beginning of the range, inclusive
         * @param end -- end of the range, inclusive
         * @param cb -- user callback
         * @param data -- user data to pass to the callback */
        void get(timestamp_type start, timestamp_type end, get_cb_type cb,
            void* data) {
            cb(get(name, start, end), data);
        }

        /* Get the path associated with this metric */
        std::string directory() const {
            boost::filesystem::path p(base);
            p /= "metrics";
            p /= name;
            return p.string();
        }

        /* Get the path to the latest slab */
        std::string latest_path() const {
            boost::filesystem::path p(directory());
            p /= "latest";
            return p.string();
        }

        /* Get the path for a particular timestamp */
        std::string timestamp_path(timestamp_type time) const {
            boost::filesystem::path p(directory());
            std::stringstream ss;
            ss << time;
            p /= ss.str();
            return p.string();
        }

        /* Get a sorted vector of the slabs inside this */
        std::vector<timestamp_type> slabs() const {
            std::vector<timestamp_type> results;

            boost::filesystem::path dir(directory());
            if (!boost::filesystem::is_directory(dir)) {
                return results;
            }

            boost::filesystem::directory_iterator it(dir);
            boost::filesystem::directory_iterator it_end;
            for (; it != it_end; ++it) {
                if (!boost::filesystem::is_regular_file(it->path())) {
                    continue;
                }

                if (it->path().filename().string() == "latest") {
                    continue;
                }
                
                /* If it's a real file... */
                std::stringstream ss(it->path().filename().string());
                timestamp_type time;
                ss >> time;
                results.push_back(time);
            }

            return results;
        }
    private:
        /* Private, unimplemented to avoid use */
        slab();
        slab(const slab& other);
        const slab& operator=(const slab& other);

        /* Members */
        std::string  base;      /* Base of the database */
        std::string  name;      /* Name of the metric */
        std::fstream stream;    /* Stream to the latest slice */
        int          written;   /* How many bytes have been written to the
                                 * latest slab */

        /* Read all the values in from the current slice */
        values_type read() {
            /* Storage, and a return type */
            data_type datum;
            values_type results;

            /* Seek to the beginning and prepare to read */
            stream.seekg(0, std::fstream::beg);

            /* Lastly, read in the stream for the latest data points */
            while (true) {
                stream.read(reinterpret_cast<char*>(&datum), sizeof(datum));
                if (!stream.eof()) {
                    results.push_back(datum);
                } else {
                    break;
                }
            }

            return results;
        }

        /* Read all the values from a provided path */
        values_type read(const std::string& path) {
            /* Storage, and a return type */
            data_type datum;
            values_type results;
            std::fstream stream_(path.c_str(), open_mode);

            /* Seek to the beginning and prepare to read */
            stream_.seekg(0, std::fstream::beg);

            /* Lastly, read in the stream for the latest data points */
            while (true) {
                stream_.read(reinterpret_cast<char*>(&datum), sizeof(datum));
                if (!stream_.eof()) {
                    results.push_back(datum);
                } else {
                    break;
                }
            }

            return results;
        }

        /* Rotate out the current buffer file for a new one
         *
         * @returns 0 on success, else -1 */
        int rotate() {
            /* For the full implementation, we actually need to see what the
             * latest slab's max timestamp was, and anything that should be in
             * another slab should be moved to the appropriate one. */
            values_type all(read());
            data_type maximum(*std::max_element(all.begin(), all.end()));

            stream.flush();
            stream.close();

            boost::filesystem::rename(
                latest_path(), timestamp_path(maximum.time));
            std::cout << "Rotating slab to " << timestamp_path(maximum.time) << std::endl;

            stream.open(latest_path().c_str(), open_mode);

            written = 0;
            
            return 0;
        }
    };
}

#endif
