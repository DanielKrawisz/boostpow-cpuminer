#ifndef BOOSTMINER_NETWORK
#define BOOSTMINER_NETWORK

#include <gigamonkey/pay/MAPI.hpp>
#include <pow_co_api.hpp>
#include <whatsonchain.hpp>
#include <jobs.hpp>
#include <ctime>

using uint64 = data::uint64;
using satoshis_per_byte = Gigamonkey::satoshis_per_byte;

namespace BoostPOW {

    using UTXO = WhatsOnChain::UTXO;

    using Gigamonkey::SHA2_256;

    struct network {
        net::asio::io_context IO;
        ptr<net::HTTP::SSL> SSL;
        WhatsOnChain::API WhatsOnChain;
        pow_co PowCo;
        Gigamonkey::MAPI::client Gorilla;
        net::HTTP::client CoinGecko;
        
        network (string api_host = "pow.co") : IO {}, SSL {std::make_shared<net::HTTP::SSL> (net::HTTP::SSL::tlsv12_client)},
            WhatsOnChain {SSL}, PowCo {IO, SSL, api_host},
            Gorilla {net::HTTP::REST {"https", "mapi.gorillapool.io"}},
            CoinGecko {net::HTTP::REST {"https", "api.coingecko.com"}, data::rate_limiter {1, data::millisecond {10}}} {
            SSL->set_default_verify_paths ();
            SSL->set_verify_mode (net::asio::ssl::verify_peer);
        }
        
        awaitable<BoostPOW::jobs> jobs (uint32 limit = 10, double max_difficulty = -1, int64 min_value = 1);
        
        awaitable<bytes> get_transaction (const Bitcoin::TxID &);
        
        awaitable<satoshis_per_byte> mining_fee ();
        
        awaitable<Boost::candidate> job (const Bitcoin::outpoint &);
        
        struct broadcast_error {
            enum error {
                none, 
                unknown, 
                network_connection_fail, 
                insufficient_fee, 
                invalid_transaction
            };
            
            error Error;
            
            broadcast_error (error e): Error{e} {}
            
            operator bool () {
                return Error == none;
            }
        };
        
        awaitable<broadcast_error> broadcast (const bytes &tx);

        awaitable<broadcast_error> broadcast_solution (const bytes &tx);

        awaitable<double> price (tm);
        
    };
    
    struct fees {
        virtual double get () = 0;
        virtual ~fees () {}
    };
    
    struct given_fees : fees {
        double FeeRate;
        given_fees (double f) : FeeRate {f} {}
        double get () final override {
            return FeeRate;
        }
    };
    
    struct network_fees : fees {
        network &Net;
        double Default;
        network_fees (network &n, double d = .05) : Net {n}, Default {d} {}
        double get () final override {
            try {
                return double (data::synced (&network::mining_fee, Net));
            } catch (std::exception &e) {
                std::cout << "Warning! Exception caught while trying to get a fee quote: " << e.what () << std::endl;
                return Default;
            }
        }
    };

    awaitable<network::broadcast_error> inline network::broadcast_solution (const bytes &tx) {
        co_await PowCo.submit_proof (tx);
        co_return co_await broadcast (tx);
    }
    
}

#endif
