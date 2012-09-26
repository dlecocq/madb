#include <iostream>

typedef struct datum_ {
    uint32_t count;
    float    avg;
    float    min;
    float    max;
    float    aux;
} datum;

#include <db.h>

int main(int argc, char* argv[]) {
    madb::db<datum> db("foo/", 128);
    return 0;
}