#include <co_occurrence_graph.h>
#include <algorithm>
#include <iostream>
#include <sstream>

uint32_t StringDict::mcounter_ = 0;
//------------------------------------------------------------------------------
uint32_t StringDict::add( std::string w ) {
    std::transform(w.begin(), w.end(), w.begin(), ::tolower);
    auto p = dict_.insert( std::make_pair(w,++mcounter_) );

    if( p.second ) { // if it was inserted
        inverse_[ mcounter_ ] = w;
        return mcounter_;
    } else {
        --mcounter_;
        return p.first->second; // the code for the found entry
    }
}

//------------------------------------------------------------------------------
std::string StringDict::to_string() {
    std::stringstream ss;
    for( auto const &e : dict_ ) { // foreach entry in the dictionary
        ss << e.first << " (" << e.second << ")\n";
    }
    return ss.str();
}

//------------------------------------------------------------------------------
std::string CoOccurrenceGraph::to_string() {
    std::stringstream ss;
    for( auto const &e : graph_ ) {
        ss << dictionary_.ilookup( e.first ) << ":\t";
        for( auto const &d : e.second ) {
            ss << dictionary_.ilookup( d ) << ", ";
        }
        ss << "\n";
    }
    return ss.str(); 
}

//------------------------------------------------------------------------------
void CoOccurrenceGraph::add(const std::string &query) {
    if( query.empty() ) return;
    std::vector< uint32_t > splitquery;
    std::string sep(" +");
    size_t b = 0, e = 0;
    while( (e = query.find_first_of( sep, b )) != std::string::npos ) {
        splitquery.push_back( dictionary_.add( query.substr(b,e-b) ) );
        b = e+1;
    }

    if( b != query.size() ) {
        splitquery.push_back( dictionary_.add( query.substr(b) ) );
    }

    for( int i = 0; i < splitquery.size(); ++i ) {
        for( int j = i + 1; j < splitquery.size(); ++j ) {
            graph_[ splitquery[i] ].push_back( splitquery[j] );
        }
    }
}


