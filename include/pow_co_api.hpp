#ifndef BOOSTMINER_POW_CO_API
#define BOOSTMINER_POW_CO_API

//#include <data/net/asio/session.hpp>
#include <data/net/HTTP_client.hpp>
#include <gigamonkey/boost/boost.hpp>

namespace net = data::net;
namespace Bitcoin = Gigamonkey::Bitcoin;
namespace Boost = Gigamonkey::Boost;
namespace encoding = data::encoding;

using uint32 = data::uint32;
using int64 = data::int64;

using string = data::string;

using UTF8 = data::UTF8;

template <typename X> using maybe = data::maybe<X>;
template <typename X> using ptr = data::ptr<X>;
template <typename X> using awaitable = data::awaitable<X>;

template <typename X> using list = data::list<X>;
template <typename K, typename V> using dispatch = data::dispatch<K, V>;

using JSON = data::JSON;

using bytes = data::bytes;

using digest256 = Gigamonkey::digest256;

struct inpoint : Bitcoin::outpoint {
    using Bitcoin::outpoint::outpoint;

    bool valid () const {
        return this->Digest.valid ();
    }

    inpoint (const Bitcoin::TxID &t, uint32 i) : outpoint {t, i} {}
};

struct pow_co : net::HTTP::client {

    net::asio::io_context &IO;
    ptr<net::HTTP::SSL> SSL;
    
    pow_co (net::asio::io_context &io, ptr<net::HTTP::SSL> ssl, string host = "pow.co") :
        net::HTTP::client {ssl, net::HTTP::REST {"https", host},
            data::rate_limiter {3, data::millisecond {1000}}},
        IO {io}, SSL {ssl} {}

    struct get_jobs_query {
        get_jobs_query &limit (uint32);
        get_jobs_query &content (digest256);
        get_jobs_query &tag (string);
        get_jobs_query &max_difficulty (double);
        get_jobs_query &min_difficulty (double);

        awaitable<list<Bitcoin::prevout>> operator () ();

        get_jobs_query (pow_co &pc) : PowCo {pc} {}

        maybe<uint32> Limit;
        maybe<string> Tag;
        maybe<digest256> Content;
        maybe<uint32> MaxDifficulty;
        maybe<uint32> MinDifficulty;

        pow_co &PowCo;
    };

    get_jobs_query jobs () {
        return get_jobs_query {*this};
    }
    
    awaitable<Bitcoin::prevout> job (const Bitcoin::TxID &);
    awaitable<Bitcoin::prevout> job (const Bitcoin::outpoint &);
    
    awaitable<inpoint> spends (const Bitcoin::outpoint &);
    
    awaitable<void> submit_proof (const bytes &);
    
    awaitable<bool> broadcast (const bytes &);

    struct get_work_query {
        get_work_query &limit (uint32);
        get_work_query &tag (string);
        get_work_query &offset (uint32);
        get_work_query &start (uint32);
        get_work_query &end (uint32);

        awaitable<JSON> operator () ();

        get_work_query (pow_co &pc) : PowCo {pc} {}

        maybe<uint32> Limit;
        maybe<string> Tag;
        maybe<uint32> Offset;
        maybe<uint32> Start;
        maybe<uint32> End;

        pow_co &PowCo;
    };

    get_work_query get_work () {
        return get_work_query {*this};
    }

    struct websockets_protocol_message {
        string Type;
        JSON Content;

        bool valid () const;

        operator JSON () const;
        websockets_protocol_message (const JSON &);
        websockets_protocol_message ();

        static JSON encode (string type, const JSON &content);
        static bool valid (const JSON &);

        static std::optional<Bitcoin::prevout> job_created (const JSON &);
        static std::optional<Bitcoin::outpoint> proof_created (const JSON &);
    };
/*
    struct websockets_protocol_handlers {
        virtual void job_created (const Bitcoin::prevout &) = 0;
        virtual ~websockets_protocol_handlers () {}
    };

    void connect (net::asio::error_handler error_handler, net::close_handler,
        function<ptr<websockets_protocol_handlers> (ptr<net::session<const JSON &>>)>);*/

    static string write (const Bitcoin::TxID &);
    static string write (const Bitcoin::outpoint &);

    static std::ostream &write (std::ostream &, const Bitcoin::TxID &);
    static std::ostream &write (std::ostream &, const Bitcoin::outpoint &);

    //void connect (net::asio::error_handler error_handler, net::close_handler, net::interaction<const JSON &>);
    
};

std::ostream inline &pow_co::write (std::ostream &o, const Bitcoin::TxID &txid) {
    return o << write (txid);
}

std::ostream inline &pow_co::write (std::ostream &o, const Bitcoin::outpoint &out) {
    return o << write (out.Digest) << "_v" << out.Index;
}


pow_co::get_jobs_query inline &pow_co::get_jobs_query::limit (uint32 x) {
    Limit = x;
    return *this;
}

pow_co::get_jobs_query inline &pow_co::get_jobs_query::content (digest256 d) {
    Content = d;
    return *this;
}

pow_co::get_jobs_query inline &pow_co::get_jobs_query::tag (string x) {
    Tag = x;
    return *this;
}

pow_co::get_jobs_query inline &pow_co::get_jobs_query::max_difficulty (double m) {
    MaxDifficulty = m;
    return *this;
}

pow_co::get_jobs_query inline &pow_co::get_jobs_query::min_difficulty (double m) {
    MinDifficulty = m;
    return *this;
}

pow_co::get_work_query inline &pow_co::get_work_query::limit (uint32 l) {
    Limit = l;
    return *this;
}

pow_co::get_work_query inline &pow_co::get_work_query::tag (string t) {
    Tag = t;
    return *this;
}

pow_co::get_work_query inline &pow_co::get_work_query::offset (uint32 o) {
    Offset = o;
    return *this;
}

pow_co::get_work_query inline &pow_co::get_work_query::start (uint32 s) {
    Start = s;
    return *this;
}

pow_co::get_work_query inline &pow_co::get_work_query::end (uint32 e) {
    End = e;
    return *this;
}

#endif
