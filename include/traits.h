#ifndef MADB__TRAITS_H
#define MADB__TRAITS_H

#include <string>
#include <vector>
#include <tr1/unordered_map>

namespace madb {
    template <typename D>
    class data_traits {
    public:
        /* Some helpful typedefs */
        typedef std::string key_type;
        typedef D           value_type;
        typedef uint32_t    timestamp_type;

        /* A single data point */
        typedef struct data_type_ {
            timestamp_type time;
            value_type     value;

            bool operator<(const data_type_& other) const {
                return time < other.time;
            }
        } data_type;

        /* A list of data points */
        typedef std::vector<data_type> values_type;

        /* A mapping of names to values type */
        typedef std::tr1::unordered_map<key_type, values_type> values_map_type;

        /* Callback types */
        typedef void(* insert_cb_type)(void*);
        typedef void(*   read_cb_type)(const values_map_type&, void*);
        typedef void(*    get_cb_type)(const values_type&, void*);
    };
}

#endif
