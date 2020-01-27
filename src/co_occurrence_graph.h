#ifndef _CO_OCCURRENCE_GRAPH_H_
#define _CO_OCCURRENCE_GRAPH_H_
#include <vector>
#include <map>
#include <set>
#include <string>
#include <stdint.h>

template <class Vector, class T>
void insert_into_vector(Vector& v, const T& t) {
    typename Vector::iterator i = std::lower_bound(v.begin(), v.end(), t);
    if (i == v.end() || t < *i)
    v.insert(i, t);
}

class StringDict {
public:
    StringDict() {}
    std::string ilookup( uint32_t x ) {
        auto it = inverse_.find(x);
        return it == inverse_.end() ? "" : it->second;
    }

    uint32_t add( std::string );
    bool contains( const std::string &s ) {
        return false; 
    }

    std::string to_string();

private:
    std::map< std::string, uint32_t > dict_;
    std::map< uint32_t, std::string > inverse_;
    static uint32_t mcounter_;
};

class CoOccurrenceGraph {
public:
    CoOccurrenceGraph() {}
    void add(const std::string &query);
    std::string to_string();
    std::string dictionary() { return dictionary_.to_string(); }
    
private:
    StringDict dictionary_;
    std::map< uint32_t, std::vector<uint32_t> > graph_;
};

#endif

