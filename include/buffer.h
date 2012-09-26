#ifndef MADB__BUFFER_H
#define MADB__BUFFER_H

#include <string>
#include <iostream>

/* For mkstemp */
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace madb {
    template <typename D>
    class buffer {
    public:
        /* Open a given path and return a buffer object
         *
         * @param path -- path to the file to open */
        static buffer<D> read(const std::string& pth) {
            buffer<D> buf;

            if ((buf.fd = open(pth.c_str(), O_RDWR)) == -1) {
                std::cout << "Failed to open " << pth << std::endl;
                exit(1);
            }

            /* Figure out how many bytes have been written */
            buf.written = lseek(buf.fd, 0, SEEK_END);
            buf.path = pth;

            return buf;
        }

        /* Make a new temporary buffer
         *
         * @param base -- path to the base directory to put the file in */
        void mktemp(const std::string& base) {
            /* Close the old file descriptor if necessary */
            if (fd > 0) {
                close(fd);
            }

            /* Construct a template argument to mkstemp. You can't use a string
             * constant argument, because it tries to replace the contents with
             * the new filename. */
            char* templ = reinterpret_cast<char*>(malloc(
                base.length() + 14));
            strncpy(templ                , base.c_str()   , base.length());
            strncpy(templ + base.length(), ".bufferXXXXXX", 14);
            
            if ((fd = mkstemp(templ)) == -1) {
                std::cout << "Failed to make temporary file" << std::endl;
                free(templ);
                exit(1);
            }

            /* Save out our path */
            path = templ;
            free(templ);

            std::cout << "Opened " << path << " | " << fd << std::endl;
        }

        /* Default constructor
         *
         * Opens nothing, sits idle */
        buffer(): fd(-1), written(0), path("") {}

        // /* Copy constructor */
        // buffer(buffer& other): fd(other.fd), written(other.written),
        //     path(other.path) {
        //     other.fd = -1;
        //     other.written = 0;
        //     other.path = "";
        // }

        ~buffer() {
            close_();
        }

        /* Rotate out the current buffer file for a new one */
        void rotate() {
            if (!path.length() || !fd) {
                return;
            }

            std::cout << "Rotating " << path << std::endl;

            /* Open up a new file descriptor */
            close_();

            /* Afterwards, remove the file */
            if (remove(path.c_str()) != 0) {
                perror("Failed to remove path");
            }
        }
    private:
        /* Private, unimplemented to avoid use */
        const buffer& operator=(const buffer& other);

        /* Members */
        int         fd;         /* File descriptor to write to */
        uint32_t    written;    /* How many bytes have been written */
        std::string path;       /* Path of our filename */

        /* Close up */
        void close_() {
            if (fd > 0) {
                std::cout << "Closing " << path << " | " << fd << std::endl;
                if (close(fd) != 0) {
                    perror("Failed to close file");
                }
                fd = -1;
                written = 0;
                path = "";
            }
        }
    };
}

#endif
