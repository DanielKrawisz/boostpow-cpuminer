#ifndef BOOSTMINER_JOBS
#define BOOSTMINER_JOBS

#include <gigamonkey/boost/boost.hpp>
#include <gigamonkey/schema/keysource.hpp>
#include <random.hpp>

using uint32 = data::uint32;

using int64 = data::int64;

using uint160 = Gigamonkey::uint160;
using uint256 = Gigamonkey::uint256;

using digest160 = Gigamonkey::digest160;
using digest256 = Gigamonkey::digest256;

using JSON = data::JSON;

using string = data::string;

using bytes = data::bytes;

template <typename X> using ptr = data::ptr<X>;

template <typename X> using maybe = data::maybe<X>;

template <typename X> using stack = data::stack<X>;
template <typename X> using list = data::list<X>;
template <typename X> using set = data::set<X>;
template <typename K, typename V> using map = data::map<K, V>;

namespace BoostPOW {

    namespace Boost = Gigamonkey::Boost;
    namespace Bitcoin = Gigamonkey::Bitcoin;
    
    struct working : Boost::candidate {
        list<int> Workers;
        working (const Boost::candidate &x): Boost::candidate {x}, Workers {} {}
        working (): Boost::candidate {}, Workers {} {}
        
        // used to select random jobs. 
        double weight (double minimum_profitability, double tilt) const;
    };
    
    struct jobs {
        std::map<digest256, working> Jobs;
        std::map<Bitcoin::outpoint, digest256> Scripts;
        
        digest256 add_script (const bytes &output_script);
        void add_prevout (const Bitcoin::prevout &u);

        uint32 remove (std::function<bool (const working &)>);

        std::map<digest256, working>::iterator random_select (random &r, double minimum_profitability);
        
        explicit operator JSON () const;
        
    };

    using key_source = Gigamonkey::key_source;

    // keep track of keys that have been used so that we can find a key from an address later.
    struct map_key_database final : key_source {
        ptr<key_source> Keys;
        uint32 MaxLookAhead;

        map<digest160, Bitcoin::secret> Past;
        list<Bitcoin::secret> Next;

        explicit map_key_database (ptr<key_source> keys, uint32 max_look_ahead = 0) :
            Keys {keys}, MaxLookAhead {max_look_ahead}, Past {}, Next {} {}

        Bitcoin::secret next () override;

        Bitcoin::secret operator [] (const digest160 &addr);

    };
    
    string write (const Bitcoin::TxID &);
    string write (const Bitcoin::outpoint &);
    JSON to_JSON (const Boost::candidate::prevout &);
    JSON to_JSON (const Boost::candidate &);
}

#endif
