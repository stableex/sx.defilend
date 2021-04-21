#pragma once

#include <eosio/asset.hpp>

namespace defilend {

    using namespace eosio;

    const name id = "defilend"_n;
    const name code = "lend.defi"_n;
    const std::string description = "Lend.Defi Converter";
    const name token_code = "btoken.defi"_n;

    struct [[eosio::table]] reserves_row {
        uint64_t    id;
        name        contract;
        symbol      sym;
        symbol      bsym;
        uint128_t   last_liquidity_cumulative_index;
        uint128_t   last_variable_borrow_cumulative_index;
        asset       practical_balance;
        asset       total_borrows_stable;
        asset       total_borrows_variable;
        asset       minimum_borrows;
        asset       maximum_borrows;
        asset       minimum_deposit;
        asset       maximum_deposit;
        asset       maximum_total_deposit;
        uint128_t   overall_borrow_rate;
        uint128_t   current_liquidity_rate;
        uint128_t    current_variable_borrow_rate;
        uint128_t   current_stable_borrow_rate;
        uint128_t   current_avg_stable_borrow_rate;
        uint128_t   reserve_factor;
        asset       reserved_balance;
        uint64_t    base_ltv_as_collateral;
        uint64_t    liquidation_threshold;
        uint64_t    liquidation_forfeit;
        uint64_t    liquidation_bonus;
        uint128_t   utilization_rate;
        uint128_t   optimal_utilization_rate;
        uint128_t   base_variable_borrow_rate;
        uint128_t   variable_rate_slope1;
        uint128_t   variable_rate_slope2;
        uint128_t   base_stable_borrow_rate;
        uint128_t   stable_rate_slope1;
        uint128_t   stable_rate_slope2;
        bool        borrowing_enabled;
        bool        usage_as_collateral_enabled;
        bool        is_stable_borrow_rate_enabled;
        bool        is_active;
        bool        is_freezed;
        uint64_t    oracle_price_id;
        time_point_sec last_update_time;

        uint64_t primary_key() const { return id; }
    };
    typedef eosio::multi_index< "reserves"_n, reserves_row > reserves;

    struct [[eosio::table]] currency_stats {
        asset    supply;
        asset    max_supply;
        name     issuer;

        uint64_t primary_key()const { return supply.symbol.code().raw(); }
    };
    typedef eosio::multi_index< "stat"_n, currency_stats > stats;

    static asset get_supply( const symbol& sym ) {
        stats stats_tbl( token_code, sym_code.raw() );
        const auto it = stats_tbl.find( sym_code.raw() );
        return it != stats_tbl.end() && it->supply.symbol == sym ? it->supply : asset {};
    }

    static asset is_btoken( const symbol& sym ) {
        return get_supply(sym).symbol.is_valid();
    }

    static asset wrap( const asset& quantity, const asset& bsupply ) {
        reserves reserves_tbl( code, code.value);
        for(const auto& row: reserves_tbl) {
            if(row.bsym == bsupply.symbol) {
                check(row.sym == quantity.symbol, "sx.defilend::wrap: Wrong lendable pair");
                return { static_cast<int64_t>(static_cast<int128_t>(quantity.amount) * bsupply.amount / row.practical_balance.amount), bsupply.symbol };
            }
        }
        check(false, "sx.defilend::wrap: Not lendable");
        return {};
    }

    static asset unwrap( const asset& quantity, const asset& bsupply, const symbol& out_sym ) {
        reserves reserves_tbl( code, code.value);
        for(const auto& row: reserves_tbl) {
            if(row.bsym == bsupply.symbol) {
                check(row.sym == out_sym, "sx.defilend::unwrap: Wrong lendable pair");
                return { static_cast<int64_t>(static_cast<int128_t>(quantity.amount) * row.practical_balance.amount / bsupply.amount), out_sym };
            }
        }
        check(false, "sx.defilend::unwrap: Not lendable");
        return {};
    }
    /**
     * ## STATIC `get_amount_out`
     *
     * Given an input amount of an asset and pair id, returns the calculated return
     *
     * ### params
     *
     * - `{asset} in` - input amount
     * - `{symbol} out_sym` - out symbol
     *
     * ### example
     *
     * ```c++
     * // Inputs
     * const asset in = asset { 10000, "USDT" };
     * const symbol out_sym = symbol { "BUSDT,4" };
     *
     * // Calculation
     * const asset out = defilend::get_amount_out( in, out_sym );
     * // => 0.999612 BUSDT
     * ```
     */
    static asset get_amount_out( const asset quantity, const symbol out_sym )
    {
        auto bsupply = get_supply(out_sym);
        if(bsupply.symbol.is_valid()) return wrap(quantity, bsupply);

        bsupply = get_supply(quantity.symbol);
        if(bsupply.symbol.is_valid()) return unwrap(quantity, bsupply, out_sym);

        check(false, "sx.defilend: Not B-token");
        return {};
    }

}