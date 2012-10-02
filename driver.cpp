#include <cstdio>
#include <vector>
#include <iostream>

typedef struct datum_ {
    uint32_t count;
    float    avg;
    float    min;
    float    max;
    float    aux;
} datum;

#include <db.h>
#include <hash.h>

int main(int argc, char* argv[]) {
    madb::db<datum> db("foo/", 128);

    //*
    uint32_t i = 0;
    std::vector<std::string> metrics;
    for (i = 0; i < 100; ++i) {
        metrics.push_back(tmpnam(NULL));
    }

    for (i = 0; i < 100; ++i) {
        std::vector<std::string>::iterator it = metrics.begin();
        for (; it != metrics.end(); ++it) {
            datum d = {1, 1, 1, 1, 1};
            db.insert(*it, i, d);
        }
    }
    //*/

    /* Now, let's read some results */
    std::cout << "Fetching " << metrics[0] << std::endl;
    typedef madb::db<datum>::values_type values_type;
    values_type results = db.get(metrics[0], 100, 200);
    values_type::iterator it(results.begin());
    for(; it != results.end(); ++it) {
        std::cout << it->time << " | " << it->value.avg << std::endl;
    }

    return 0;
}