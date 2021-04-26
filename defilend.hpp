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

    struct [[eosio::table]] userconfigs_row {
        uint64_t    reserve_id;
        bool        use_as_collateral;

        uint64_t primary_key()const { return reserve_id; }
    };
    typedef eosio::multi_index< "userconfigs"_n, userconfigs_row > userconfigs;

    static asset get_supply( const symbol_code& symcode ) {
        stats stats_tbl( token_code, symcode.raw() );
        const auto it = stats_tbl.begin();
        return it != stats_tbl.end() ? it->supply : asset {};
    }

    static bool is_btoken( const symbol_code& symcode ) {
        return get_supply(symcode).symbol.is_valid();
    }

    //get first b-token based on symcode (could be wrong if there are multiple wrapped tokens with the same symbol code)
    static extended_symbol get_btoken( const symbol_code& symcode) {
        reserves reserves_tbl( code, code.value);
        for(const auto& row: reserves_tbl) {
            if(row.sym.code() == symcode) {
                return { row.bsym, token_code };
            }
        }
        return {};
    }

    static extended_asset wrap( const asset& quantity ) {
        reserves reserves_tbl( code, code.value);
        for(const auto& row: reserves_tbl) {
            if(row.sym == quantity.symbol) {
                const auto bsupply = get_supply(row.bsym.code());
                return { static_cast<int64_t>(static_cast<int128_t>(quantity.amount) * bsupply.amount / row.practical_balance.amount), extended_symbol{ bsupply.symbol, token_code } };
            }
        }
        check(false, "sx.defilend::wrap: Not lendable");
        return {};
    }

    static extended_asset unwrap( const asset& quantity ) {
        reserves reserves_tbl( code, code.value);
        for(const auto& row: reserves_tbl) {
            if(row.bsym == quantity.symbol) {
                const auto bsupply = get_supply(row.bsym.code());
                return { static_cast<int64_t>(static_cast<int128_t>(quantity.amount) * row.practical_balance.amount / bsupply.amount), extended_symbol{ row.sym, row.contract }};
            }
        }
        check(false, "sx.defilend::unwrap: Not lendable: " + quantity.to_string());
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
        if(is_btoken(out_sym.code())) {
            const auto out = wrap(quantity).quantity;
            if(out.symbol == out_sym) return out;
        }

        if(is_btoken(quantity.symbol.code())) {
            const auto out = unwrap(quantity).quantity;
            if(out.symbol == out_sym) return out;
        }

        check(false, "sx.defilend: Not B-token");
        return {};
    }

    static void unstake( const name authorizer, const name owner, const symbol_code sym)
    {
        reserves reserves_tbl( code, code.value);
        userconfigs configs(code, owner.value);
        for(const auto& row: configs) {
            const auto pool = reserves_tbl.get(row.reserve_id, "defilend: no reserve");
            if(pool.bsym.code() == sym) return;     //already unstaked
        }

        action(
            permission_level{authorizer,"active"_n},
            defilend::code,
            "unstake"_n,
            std::make_tuple(owner, sym)
        ).send();
    }

}