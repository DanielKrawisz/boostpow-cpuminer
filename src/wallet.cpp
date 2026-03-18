#include <wallet.hpp>
#include <whatsonchain.hpp>
#include <gigamonkey/fees.hpp>
#include <gigamonkey/script/pattern/pay_to_address.hpp>
#include <gigamonkey/script.hpp>
#include <gigamonkey/script/machine.hpp>
#include <math.h>
#include <data/io/wait_for_enter.hpp>
#include <data/lift.hpp>
#include <fstream>

std::ostream &write_JSON (std::ostream &o, const p2pkh_prevout &p) {
    return o << "{\"wif\": \"" << p.Key << "\", \"outpoint\": \"" << p.Outpoint.Digest <<
        "\", \"index\": " << p.Outpoint.Index << ", \"value\": " << int64 (p.Value) << "}";
}

std::ostream &write_JSON (std::ostream &o, const wallet &w) {
    
    o << "{\"prevouts\": [";
    
    list<p2pkh_prevout> p = w.Prevouts;
    
    if (!data::empty (p)) while (true) {
        write_JSON (o, data::first (p));
        p = data::rest (p);
        if (data::empty (p)) break;
        o << ", ";
    }
    
    return o << "], \"master\": \"" << w.Master << "\", \"index\": " << w.Index << "}";
    
}

namespace net = data::net;

using JSON = data::JSON;

using pay_to_address = Gigamonkey::pay_to_address;

using transaction_design = Gigamonkey::transaction_design;

p2pkh_prevout read_prevout (const JSON &j) {
    
    return p2pkh_prevout {
        
        Bitcoin::outpoint {Gigamonkey::digest256 {string (j["txid"])}, data::uint32 (j["index"])},
        Bitcoin::satoshi (int64 (j["value"])),
        
        Gigamonkey::Bitcoin::secret {string (j["wif"])}
    };
    
}

wallet read_JSON (std::istream &i) {
    JSON j = JSON::parse (i);
    
    if (j.size () != 3 || !j.contains ("prevouts") || !j.contains ("master") || !j.contains ("index"))
        throw std::string {"invalid wallet format"};
    
    auto pp = j["prevouts"];
    list<p2pkh_prevout> prevouts;
    for (const JSON p: pp) prevouts <<= read_prevout (p);
    
    return wallet {prevouts, HD::BIP_32::secret {string (j["master"])}, data::uint32 (j["index"])};
}

Bitcoin::satoshi wallet::value () const {
    return data::fold ([] (Bitcoin::satoshi x, const p2pkh_prevout &p) -> Bitcoin::satoshi {
        return x + p.Value;
    }, Bitcoin::satoshi {0}, Prevouts);
}

wallet wallet::insert (const p2pkh_prevout &p) const {
    return wallet {
        Prevouts << p, 
        Master, 
        Index
    };
}

p2pkh_prevout::operator Bitcoin::prevout () const {
    return Bitcoin::prevout {Outpoint, Bitcoin::output {Value, pay_to_address::script (Key.address ().Digest)}};
}

uint64 redeem_script_size (bool compressed_pubkey) {
    return (compressed_pubkey ? 33 : 65) + 2 + Bitcoin::signature::MaxSize;
}

constexpr uint64 p2sh_script_size = 24;

constexpr int64 dust = 500;

using transaction_design = Gigamonkey::transaction_design;

wallet::spent wallet::spend (Bitcoin::output to, double satoshis_per_byte) const {
    wallet w = *this;
    
    // generate change script
    auto new_secret = Bitcoin::secret (w.Master.derive ({w.Index}));
    w.Index++;
    
    bytes change_script = pay_to_address::script (new_secret.address ().Digest);
    
    // we create a transaction design without inputs and with an empty change output. 
    // we will figure out what these parameters are. 
    transaction_design tx {1, {}, {to, Bitcoin::output {0, change_script}}, 0};
    
    // the keys that we will use to sign the tx. 
    list<Bitcoin::secret> signing_keys {};
    
    Bitcoin::satoshi fee;
    
    // find sufficient funds 
    while (true) {
        // minimum fee required for this tx.
        fee = std::ceil (tx.expected_size () * satoshis_per_byte);
        
        // we have enough funds to cover the amount sent with fee without leaving a dust input. 
        if (tx.spent () - tx.sent () - fee > dust) break;
        
        if (data::empty (w.Prevouts)) throw std::string {"insufficient funds"};
        
        auto p = w.Prevouts.first ();
        w.Prevouts = w.Prevouts.rest ();
        
        tx.Inputs <<= transaction_design::input (Bitcoin::prevout (p), redeem_script_size (p.Key.Compressed));
        signing_keys <<= p.Key;
        
    }
    
    // randomize change index. 
    uint32 change_index = std::uniform_int_distribution<data::uint32> (0, 1) (data::random::get ());
    
    Bitcoin::satoshi change_value = tx.spent () - tx.sent () - fee;
    Bitcoin::output change {change_value, change_script};
    
    // replace outputs with a randomized list containing the correct change amount. 
    tx.Outputs = change_index == 0 ? list<Bitcoin::output> {change, to} : list<Bitcoin::output> {to, change};

    // generate incomplete transaction for signing.
    Bitcoin::incomplete::transaction incomplete (tx);

    // make all sighash documents for the incomplete transaction.
    list<Bitcoin::sighash::document> docs;

    data::for_each_by ([&] (size_t index, const Gigamonkey::transaction_design::input &i) {
        docs <<= Bitcoin::sighash::document {incomplete, index, i.value (), Bitcoin::decompile (i.script ())};
    }, tx.Inputs);
    
    auto completed = tx.complete (
        // get all redeem scripts
        lift (
            [] (
                const Bitcoin::secret &k,
                const Bitcoin::sighash::document &doc) {
                return pay_to_address::redeem (k.sign (doc), k.to_public ());
                },
                signing_keys, docs));

    return spent {
        w.insert (p2pkh_prevout {Bitcoin::outpoint {completed.id (), change_index}, change_value, new_secret}),
        completed};
}

bool broadcast (const Bitcoin::transaction &t) {
    
    WhatsOnChain::API API {};
    
    uint32 last_used = 0;
    
    try {
        return data::synced ([&] () { return API.transactions ().broadcast (bytes (t)); });
    } catch (net::HTTP::response response) {
        std::cout << "invalid HTTP response caught " << response.Status << std::endl;
    } 
    
    return false;
}

void write_to_file (const wallet &w, const std::string &filename) {
    std::fstream my_file;
    my_file.open (filename, std::ios::out);
    if (!my_file) throw std::string {"could not open file"};
    write_JSON (my_file, w);
    my_file.close ();
}

wallet read_wallet_from_file (const std::string &filename) {
    std::fstream my_file;
    my_file.open (filename, std::ios::in);
    if (!my_file) throw std::string {"could not open file"};
    return read_JSON (my_file);
}

wallet restore (const HD::BIP_32::secret &master, uint32 max_look_ahead) {
    
    wallet w {{}, master, 0};
    
    WhatsOnChain::API API {};
    
    uint32 last_used = 0;
    
    try {
        while (true) {
            
            Bitcoin::secret new_key = Bitcoin::secret (w.Master.derive ({w.Index}));
            w.Index += 1;
            
            Bitcoin::address new_addr = new_key.address ();
            std::cout << "testing address " << new_addr << std::endl;

            auto txs = data::synced ([&] () {return API.addresses ().get_unspent (new_addr); });
            
            if (txs.size () == 0) {
                if (last_used == max_look_ahead) break;
                
                last_used++;
                continue;
            }
            
            last_used = 0;
            
            for (const auto &prevout : txs) {
                std::cout << "  match found at " << prevout.Outpoint << " with value " << prevout.Value << std::endl;
                w = w.insert (p2pkh_prevout {Bitcoin::outpoint {prevout.Outpoint.Digest, prevout.Outpoint.Index}, prevout.Value, new_key});
            }
            
        }
    } catch (net::HTTP::response response) {
        std::cout << "invalid HTTP response caught " << response.Status << std::endl;
    } 
    
    return w;
    
}
