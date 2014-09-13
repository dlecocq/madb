#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include <vector>
#include <sstream>
#include <iostream>

#include <boost/filesystem.hpp>

typedef struct datum_ {
    uint32_t count;
    float    avg;
    float    min;
    float    max;
    float    aux;
} datum;

#include <db.h>

using namespace madb;

/* This is is how many data points we need to make a few rotations to happen */
uint32_t count = 2 * (madb::buffer<datum>::max_size / sizeof(datum));

TEST_CASE("db", "works as advertised") {
    SECTION("buffer", "creates directories as needed") {
        REQUIRE(!boost::filesystem::exists("foo"));
        madb::db<datum> db("foo", 128);
        REQUIRE( boost::filesystem::exists("foo"));
        db.destroy();
        REQUIRE(!boost::filesystem::exists("foo"));
    }

    SECTION("slab directories", "creates directories as needed") {
        REQUIRE(!boost::filesystem::exists("foo/metrics"));
        madb::db<datum> db("foo", 128);

        /* Now, we're going to insert a very large number of data points under
         * a single metric, to the point where the buffer would get rotated out
         * and then make sure it created a data point */
        for(uint32_t i = 0; i < count; ++i) {
            datum d = {1, 1, 1, 1, 1};
            db.insert("testing", i, d);
        }

        /* Now, we should see a file for this metric */
        REQUIRE(boost::filesystem::exists("foo/metrics/testing/latest"));
        /* Lastly, we destroy the database */
        db.destroy();
    }

    SECTION("get", "can always get data that we expect to be there") {
        madb::db<datum> db("foo", 128);

        /* Now, we're going to insert a very large number of data points under
         * a single metric, to the point where the buffer would get rotated out
         * and then make sure it created a data point */
        for(uint32_t i = 0; i < count; ++i) {
            datum d = {1, 1, 1, 1, 1};
            db.insert("testing", i, d);
        }

        /* Now, we should try to get data out */
        madb::db<datum>::values_type results(db.get("testing", 0, count));

        // madb::db<datum>::values_type::iterator it(results.begin());
        // for (; it != results.end(); ++it) {
        //     std::cout << "Timestamp: " << it->time << std::endl;
        // }

        REQUIRE(results.size() == count);
        /* Lastly, we destroy the database */
        db.destroy();
    }

    SECTION("slab rotate", "can rotate out to new slabs at needed") {
        uint32_t timestamp = (madb::slab<datum>::max_size /
            sizeof(madb::db<datum>::data_type)) + 1;

        boost::filesystem::path p("foo/metrics/testing");
        std::stringstream ss;
        ss << timestamp;
        p /= ss.str();
        REQUIRE(!boost::filesystem::exists(p));
        madb::db<datum> db("foo", 128);
        
        for(uint32_t i = 0; i < count; ++i) {
            datum d = {1, 1, 1, 1, 1};
            db.insert("testing", i, d);
        }

        /* Now, we should see a file for this metric */
        std::cout << p.string() << std::endl;
        REQUIRE(boost::filesystem::exists(p));

        /* Lastly, we destroy the database */
        db.destroy();
    }

    SECTION("metrics", "can list all the metrics that we add") {
        madb::db<datum> db("foo", 128);

        typedef std::set<madb::db<datum>::key_type>    set_type;
        typedef std::vector<madb::db<datum>::key_type> vector_type;

        set_type metrics;
        metrics.insert("hello");
        metrics.insert("how");
        metrics.insert("are");
        metrics.insert("you");

        /* A datum do insert */
        datum d = {1, 1, 1, 1, 1};

        set_type::iterator it(metrics.begin());
        for (; it != metrics.end(); ++it) {    
            db.insert(*it, 1, d);
        }

        /* Now, let's list all the metrics, make suer it's complete */
        vector_type results(db.metrics());
        set_type result_set(results.begin(), results.end());
        REQUIRE(result_set == metrics);

        /* Now we should insert another, and make sure it's there, too */
        metrics.insert("today");
        db.insert("today", 1, d);
        results = db.metrics();
        result_set.clear();
        result_set.insert(results.begin(), results.end());
        REQUIRE(result_set == metrics);

        /* Clean up */
        db.destroy();
    }
}

int main(int argc, char* const argv[]) {    
    return Catch::Main(argc, argv);
}
