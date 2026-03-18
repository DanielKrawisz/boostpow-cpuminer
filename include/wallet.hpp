#ifndef BOOSTMINER_WALLET
#define BOOSTMINER_WALLET

#include <gigamonkey/boost/boost.hpp>
#include <gigamonkey/wif.hpp>
#include <gigamonkey/schema/hd.hpp>
#include <gigamonkey/pay/extended.hpp>

using uint32 = data::uint32;

using uint64 = data::uint64;
using int64 = data::int64;

namespace HD = Gigamonkey::HD;
namespace secp256k1 = Gigamonkey::secp256k1;
namespace Bitcoin = Gigamonkey::Bitcoin;

using digest512 = Gigamonkey::digest512;

using bytes = data::bytes;

using string = data::string;

template <typename X> using list = data::list<X>;

struct p2pkh_prevout {
    
    Bitcoin::outpoint Outpoint;
    Bitcoin::satoshi Value;
    
    Gigamonkey::Bitcoin::secret Key;
    
    explicit operator Bitcoin::prevout () const;
    
};

std::ostream inline &operator << (std::ostream &o, const p2pkh_prevout &p) {
    return o << "p2pkh_prevout{" << p.Outpoint << ", Value: " << p.Value << ", Key: " << p.Key;
}

struct wallet {
    
    static constexpr double default_fee_rate = 0.5;
    
    list<p2pkh_prevout> Prevouts;
    HD::BIP_32::secret Master;
    data::uint32 Index;
    
    Bitcoin::satoshi value () const;
    
    struct spent;
    
    spent spend (Bitcoin::output to, double satoshis_per_byte = default_fee_rate) const;
    wallet insert (const p2pkh_prevout &) const;
    
    operator std::string () const;
    
    static wallet read (std::string_view);
    
};

std::ostream &operator << (std::ostream &, wallet &);

using extended_transaction = Gigamonkey::extended::transaction;

struct wallet::spent {
    wallet Wallet;
    extended_transaction Transaction;
};

std::ostream &write_JSON (std::ostream &, const wallet&);
wallet read_json (std::istream &);

std::ostream inline &operator << (std::ostream &o, wallet &w) {
    return write_JSON (o, w);
}

void write_to_file (const wallet &w, const std::string &filename);

wallet read_wallet_from_file (const std::string &filename);

wallet restore (const HD::BIP_32::secret &master, uint32 max_look_ahead);

#endif
