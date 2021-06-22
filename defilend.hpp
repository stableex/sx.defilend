#pragma once

#include <eosio/asset.hpp>
#include <sx.utils/utils.hpp>

namespace defilend {

    using namespace eosio;
    using namespace sx;

    const name id = "defilend"_n;
    const name code = "lend.defi"_n;
    const std::string description = "Lend.Defi Converter";
    const name token_code = "btoken.defi"_n;
    const name oracle_code = "oracle.defi"_n;
    const extended_symbol value_symbol { symbol{"USDT",4}, "tethertether"_n };

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
        uint128_t   current_variable_borrow_rate;
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
        uint128_t get_by_extsym() const { return static_cast<uint128_t>(contract.value) << 8 | sym.code().raw(); }
        uint64_t get_by_bsym() const { return bsym.code().raw(); }
    };
    typedef eosio::multi_index< "reserves"_n, reserves_row,
        indexed_by< "byextsym"_n, const_mem_fun<reserves_row, uint128_t, &reserves_row::get_by_extsym> >,
        indexed_by< "bybsym"_n, const_mem_fun<reserves_row, uint64_t, &reserves_row::get_by_bsym> >
    > reserves;

    struct [[eosio::table]] userconfigs_row {
        uint64_t    reserve_id;
        bool        use_as_collateral;

        uint64_t primary_key()const { return reserve_id; }
    };
    typedef eosio::multi_index< "userconfigs"_n, userconfigs_row > userconfigs;


    struct [[eosio::table]] oracle_row {
        uint64_t        id;
        name            contract;
        symbol_code     coin;
        uint8_t         precision;
        uint64_t        acc_price;
        uint64_t        last_price;
        uint64_t        avg_price;
        time_point_sec  last_update;

        uint64_t primary_key()const { return id; }
    };
    typedef eosio::multi_index< "prices"_n, oracle_row > prices;

    struct [[eosio::table]] userreserves_row {
        uint64_t        reserve_id;
        asset           principal_borrow_balance;
        asset           compounded_interest;
        uint128_t       last_variable_borrow_cumulative_index;
        uint128_t       stable_borrow_rate;
        time_point_sec  last_update_time;

        uint64_t primary_key()const { return reserve_id; }
    };
    typedef eosio::multi_index< "userreserves"_n, userreserves_row > userreserves;

    struct StOraclizedAsset {
        extended_asset tokens;
        double value;
        double ratioed;
    };

    static bool is_btoken( const symbol& sym ) {
        return utils::get_supply({ sym, token_code }).symbol.is_valid();
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

    /**
     * ## STATIC `unwrap`
     *
     * Given an input amount of tokens calculate amount B-tokens on deposit
     *
     * ### params
     *
     * - `{asset} quantity` - input amount
     *
     * ### example
     *
     * ```c++
     * // Inputs
     * const asset in = asset { 1000, "USDT" };
     *
     * // Calculation
     * const extended_asset out = defilend::wrap( in );
     * // => 100000 BUSDT@btoken.defi
     * ```
     */
    static extended_asset wrap( const asset& quantity ) {
        reserves reserves_tbl( code, code.value);
        for(const auto& row: reserves_tbl) {        //TODO: use secondary index (need to pass extended_asset to the method)
            if(row.sym == quantity.symbol) {
                const auto bsupply = utils::get_supply({ row.bsym, token_code });
                return { static_cast<int64_t>(static_cast<int128_t>(quantity.amount) * bsupply.amount / row.practical_balance.amount), extended_symbol{ bsupply.symbol, token_code } };
            }
        }
        check(false, "sx.defilend::wrap: Not lendable");
        return {};
    }

    /**
     * ## STATIC `unwrap`
     *
     * Given an input amount of B-tokens calculate amount redeemed tokens
     *
     * ### params
     *
     * - `{asset} quantity` - input amount
     *
     * ### example
     *
     * ```c++
     * // Inputs
     * const asset in = asset { 1000000, "BUSDT" };
     *
     * // Calculation
     * const extended_asset out = defilend::unwrap( in );
     * // => 10000 USDT@tethertether
     * ```
     */
    static extended_asset unwrap( const asset& quantity ) {
        reserves reserves_tbl( code, code.value);
        auto index = reserves_tbl.get_index<"bybsym"_n>();
        const auto it = index.lower_bound(quantity.symbol.code().raw());
        check(it != index.end() && it->bsym == quantity.symbol, "sx.defilend::unwrap: Not redeemable: " + quantity.to_string());

        const auto bsupply = utils::get_supply({ it->bsym, token_code });
        int64_t out_amount = static_cast<int128_t>(quantity.amount) * it->practical_balance.amount / bsupply.amount;

        if( it->practical_balance.amount < out_amount + it->utilization_rate * it->practical_balance.amount / 100000000000000 )
            out_amount = 0;

        return { out_amount, extended_symbol{ it->sym, it->contract }};
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
        if(is_btoken(out_sym)) {
            const auto out = wrap(quantity).quantity;
            if(out.symbol == out_sym) return out;
        }

        if(is_btoken(quantity.symbol)) {
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


    /**
     * ## STATIC `get_value`
     *
     * Given an input amount of an asset and Defibox oracle id, return USD value based on Defibox oracle
     *
     * ### params
     *
     * - `{extended_asset} in` - input tokens
     * - `{uint64_t} oracle_id` - oracle id
     *
     * ### example
     *
     * ```c++
     * // Inputs
     * const extended_asset in = asset { 10000, "EOS" };
     * const symbol oracle_id = 1;
     *
     * // Calculation
     * const asset out = defilend::get_value( in, oracle_id );
     * // => 4.0123
     * ```
     */
    static double get_value( const extended_asset in, const uint64_t oracle_id )
    {
        if(in.get_extended_symbol() == value_symbol)
            return static_cast<double>(in.quantity.amount) / pow(10, in.quantity.symbol.precision());

        prices prices_tbl( oracle_code, oracle_code.value);
        const auto row = prices_tbl.get(oracle_id, "defilend: no oracle");

        return static_cast<double>(in.quantity.amount)  / pow(10, in.quantity.symbol.precision()) * (static_cast<double>(row.avg_price) / pow(10, row.precision));
    }


    /**
     * ## STATIC `get_collaterals`
     *
     * Given an account name return collaterals and their values
     *
     * ### params
     *
     * - `{name} account` - account
     *
     * ### example
     *
     * ```c++
     * // Inputs
     * const name account = "myusername";
     *
     * // Calculation
     * const vector<StOraclizedAsset> collaterals = defilend::get_collaterals( account );
     * // => { {"400 USDT", 400, 300}, {"100 EOS", 500, 375} }
     * ```
     */
    static vector<StOraclizedAsset> get_collaterals( const name account )
    {
        vector<StOraclizedAsset> res;
        reserves reserves_tbl( code, code.value);
        userconfigs configs_tbl(code, account.value);
        for(const auto& row: configs_tbl) {
            if(!row.use_as_collateral) continue;
            const auto reserve = reserves_tbl.get(row.reserve_id, "defilend: no collateral reserve");
            const auto bdeposit = utils::get_balance({ reserve.bsym, token_code }, account ).quantity;
            if(bdeposit.amount == 0) continue;
            const auto supply = utils::get_supply({ reserve.bsym, token_code });
            const auto tokens = reserve.practical_balance / ((double) supply.amount / bdeposit.amount);
            const extended_asset ext_tokens = { tokens, reserve.contract };
            if( tokens.amount == 0) continue;
            const auto value = get_value(ext_tokens, reserve.oracle_price_id);
            res.push_back({ ext_tokens, value, value * reserve.liquidation_threshold / 10000 });
        }

        return res;
    }

    /**
     * ## STATIC `get_loans`
     *
     * Given an account name return user loans and their values
     *
     * ### params
     *
     * - `{name} account` - account
     *
     * ### example
     *
     * ```c++
     * // Inputs
     * const name account = "myusername";
     *
     * // Calculation
     * const vector<StOraclizedAsset> loans = defilend::get_loans( account );
     * // => { {"400 USDT", 400, 300}, {"100 EOS", 500, 375} }
     * ```
     */
    static vector<StOraclizedAsset> get_loans( const name account )
    {
        vector<StOraclizedAsset> res;
        reserves reserves_tbl( code, code.value);
        userreserves userreserves_tbl(code, account.value);
        const auto now = eosio::current_time_point().sec_since_epoch();
        for(const auto& row: userreserves_tbl) {
            const auto reserve = reserves_tbl.get(row.reserve_id, "defilend: no loan reserve");
            const auto secs = now - row.last_update_time.sec_since_epoch();
            const auto rate1 = reserve.current_variable_borrow_rate * secs / (365*24*60*60);
            const auto rate2 = rate1 * reserve.last_variable_borrow_cumulative_index / row.last_variable_borrow_cumulative_index;
            const auto accrued_amount = row.principal_borrow_balance.amount * interest2 / 100000000000000;
            const auto accrued = asset{ static_cast<int64_t>(accrued_amount), row.principal_borrow_balance.symbol };
            //print("\n  Principal: ", row.principal_borrow_balance, ", Seconds: ", secs, ", Rate1: ", rate1, ", Rate2: ", rate2, ", accrued: ", accrued);
            const extended_asset ext_tokens = { row.principal_borrow_balance + accrued, reserve.contract };
            const auto value = get_value(ext_tokens, reserve.oracle_price_id);
            res.push_back({ ext_tokens, value, value });
        }

        return res;
    }

    /**
     * ## STATIC `get_health_factor`
     *
     * Given an account name return user health factor
     *
     * ### params
     *
     * - `{name} account` - account
     *
     * ### example
     *
     * ```c++
     * // Inputs
     * const name account = "myusername";
     *
     * // Calculation
     * const double health_factor = defilend::get_health_factor( account );
     * // => 1.2345
     * ```
     */
    static double get_health_factor( const name account )
    {
        double deposited = 0, loaned = 0;

        const auto collaterals = get_collaterals(account);
        for(const auto coll: collaterals){
            deposited += coll.ratioed;
        }

        const auto loans = defilend::get_loans(account);
        for(const auto loan: loans){
            loaned += loan.value;
        }
        return loaned == 0 ? 0 : deposited / loaned;
    }



}