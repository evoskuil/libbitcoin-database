/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBBITCOIN_DATABASE_QUERY_CONSENSUS_IPP
#define LIBBITCOIN_DATABASE_QUERY_CONSENSUS_IPP

#include <algorithm>
#include <atomic>
#include <bitcoin/system.hpp>
#include <bitcoin/database/define.hpp>
#include <bitcoin/database/error.hpp>

namespace libbitcoin {
namespace database {

// unspent_duplicates (bip30)
// ----------------------------------------------------------------------------

// private/static
TEMPLATE
inline bool CLASS::contains(const maybe_strongs& pairs,
    const header_link& block) NOEXCEPT
{
    return std::any_of(pairs.begin(), pairs.end(), [&](const auto& pair) NOEXCEPT
    {
        return block == pair.block;
    });
}

// private/static
TEMPLATE
inline tx_links CLASS::strong_only(const maybe_strongs& pairs) NOEXCEPT
{
    tx_links links{};
    for (const auto& pair: pairs)
        if (pair.strong)
            links.push_back(pair.tx);

    // Reduced to the subset of strong duplicate (by hash) tx records.
    return links;
}

// protected
// Required for bip30 processing.
// The top of the strong_tx table will reflect the current state of only one
// block association. This scans the multimap for the first instance of each
// block that is associated by the tx.link and returns that set of block links.
// Return distinct set of txs by link where each is strong by block.
TEMPLATE
tx_links CLASS::get_strong_txs(const tx_link& link) const NOEXCEPT
{
    auto it = store_.strong_tx.it(link);
    if (!it)
        return {};

    // Obtain all first (by block) duplicate (by link) tx records.
    maybe_strongs pairs{};
    do
    {
        table::strong_tx::record strong{};
        if (store_.strong_tx.get(it, strong) &&
            !contains(pairs, strong.header_fk))
        {
            pairs.emplace_back(strong.header_fk, it.self(), strong.positive);
        }

    }
    while (it.advance());
    it.reset();

    return strong_only(pairs);
}

// protected
// Required for bip30 processing.
// Return distinct set of txs by link for hash where each is strong by block.
TEMPLATE
tx_links CLASS::get_strong_txs(const hash_digest& tx_hash) const NOEXCEPT
{
    // TODO: avoid nested iterators. accumulate set of tx_links and iterate set
    // TODO: after releasing the initial iterator.

    // TODO: deadlock.
    auto it = store_.tx.it(tx_hash);
    if (!it)
        return {};

    tx_links links{};
    do
    {
        // TODO: deadlock.
        for (const auto& tx: get_strong_txs(it.self()))
            links.push_back(tx);
    }
    while (it.advance());
    return links;
}

// protected
TEMPLATE
bool CLASS::is_spent_prevout(const point_link& link, index index) const NOEXCEPT
{
    table::point::get_stub stub{};
    if (!store_.point.get(link, stub))
        return false;

    // Prevout is spent by any confirmed transaction.
    return spent_prevout(link, index, stub.value, tx_link::terminal) ==
        error::confirmed_double_spend;
}

TEMPLATE
bool CLASS::is_spent_coinbase(const tx_link& link) const NOEXCEPT
{
    // TODO: identify spends by stub.
    ////// All outputs of the tx are confirmed spent.
    ////const auto point_fk = to_point(get_tx_key(link));
    ////for (index index{}; index < output_count(link); ++index)
    ////    if (!is_spent_prevout(point_fk, index))
    ////        return false;

    return true;
}

// Duplicate tx instances (with the same hash) may result from a write race.
// Duplicate cb tx instances are allowed by consensus. Apart from two bip30
// exceptions, duplicate cb txs are allowed only if previous are fully spent.
TEMPLATE
code CLASS::unspent_duplicates(const header_link& link,
    const context& ctx) const NOEXCEPT
{
    // This is generally going to be disabled.
    if (!ctx.is_enabled(system::chain::flags::bip30_rule))
        return error::success;

    // Get the block's first tx link.
    const auto cb = to_coinbase(link);
    if (cb.is_terminal())
        return error::integrity1;

    // TODO: deadlock risk.
    // Iterate all strong records for each tx link of the same hash.
    // The same link may be the coinbase for more than one block.
    // Distinct links may be the coinbase for independent blocks.
    // Duplicate instances of a tx (including cb) may exist because of a race.
    // Strong records for a link may be reorganized and again organized.
    auto it = store_.tx.it(get_tx_key(cb));
    if (!it)
        return error::integrity2;

    // TODO: avoid nested iterators. accumulate set of tx_links and iterate set
    // TODO: after releasing the initial iterator.
    tx_links coinbases{};
    do
    {
        for (const auto& tx: get_strong_txs(it.self()))
            coinbases.push_back(tx);
    }
    while (it.advance());
    it.reset();
    
    if (prevout_enabled())
    {
        // Current block is not set strong, so zero is expected.
        if (is_zero(coinbases.size()))
            return error::success;
    }
    else
    {
        // Current block is set strong, so self is expected.
        if (is_one(coinbases.size()))
            return error::success;
    
        // Remove self (will be not found if current block is not set_strong).
        const auto self = std::find(coinbases.begin(), coinbases.end(), link);
        if (self == coinbases.end() || coinbases.erase(self) == coinbases.end())
            return error::integrity3;
    }
    
    // bip30: all outputs of all previous duplicate coinbases must be spent.
    // BUGBUG: with !prevout_enabled() this can locate spends in current block.
    // BUGBUG: with multiple previous duplicates there must be same number of
    // BUGBUG: spends of each coinbase output, but this will identify only one.
    for (const auto coinbase: coinbases)
        if (!is_spent_coinbase(coinbase))
            return error::unspent_coinbase_collision;

    return error::success;
}

// spent_prevout
// ----------------------------------------------------------------------------

// SEARCHES SPEND [37%] (small POINT read [.24%], tiny STRONG_TX search [0%]).
// protected
TEMPLATE
error::error_t CLASS::spent_prevout(const point_link& link, index index,
    const point_stub& stub, const tx_link& self) const NOEXCEPT // 37.32%
{
    // TODO: reuse to_spenders() and check for confirmed.
    // Prevout is spent by any confirmed transaction. "self" tx is excluded and
    // presumed to not be confirmed (block under evaluation is not strong).

    auto it = store_.spend.it(table::spend::compose(stub, index));
    if (!it)
        return self.is_terminal() ? error::success : error::integrity4;

    std::vector<table::spend::get_parent_point> spenders{};
    do
    {
        table::spend::get_parent_point spend{};
        if (!store_.spend.get(it, spend))
            return error::integrity5;

        // Exclude self from strong_tx search.
        if (spend.parent_fk != self)
            spenders.push_back(spend);
    }
    while (it.advance()); // 25.74% (iteration)
    it.reset();

    if (spenders.empty())
        return error::success;

    // link is the validated prevout, but may not be the confirmed one.
    // Hash is only read in the case of conflict(s) or double spend.
    const auto point_hash = get_point_key(link);  // 0.24% (total)

    // Find a confirmed spending tx after excluding hashmap conflicts.
    for (const auto& spender: spenders)
        if ((get_point_key(spender.point_fk) == point_hash) &&  // above
            is_strong_tx(spender.parent_fk))                    // 0% (no conflicts)
            return error::confirmed_double_spend;

    return error::success;
}

// unspendable_prevout
// ----------------------------------------------------------------------------

// SEARCHES STRONG_TX [33%] (small HEADER read [<10%]).
// protected
TEMPLATE
error::error_t CLASS::unspendable_prevout(uint32_t sequence, bool coinbase,
    const tx_link& prevout_tx, uint32_t version,
    const context& ctx) const NOEXCEPT // 43.83%
{
    // TODO: If to_block(prevout_tx) is terminal, may be a duplicate tx, so
    // TODO: perform search for each tx instance of same hash as prevout_tx
    // TODO: until block associated (otherwise missing/unconfirmed prevout).
    const auto strong = to_block(prevout_tx); // 33.29%
    if (strong.is_terminal())
        return error::unconfirmed_spend;

    const auto bip68 = ctx.is_enabled(system::chain::flags::bip68_rule) &&
        (version >= system::chain::relative_locktime_min_version);

    // The bip68 condition reduces get_context in 295001-419328 to ~0%.
    if (bip68 || coinbase)
    {
        context out{};
        if (!get_context(out, strong)) // 10.31% (before above condition)
            return error::integrity6;

        if (bip68 &&
            input::is_locked(sequence, ctx.height, ctx.mtp, out.height, out.mtp))
            return error::relative_time_locked;

        if (coinbase &&
            !transaction::is_coinbase_mature(out.height, ctx.height))
            return error::coinbase_maturity;
    }

    return error::success;
}

// get_spend_set
// ----------------------------------------------------------------------------

// READS TX / PUTS / SPEND TABLES [15%]
// protected
TEMPLATE
bool CLASS::get_spend_set(spend_set& set, const tx_link& link) const NOEXCEPT // 14.78%
{
    table::transaction::get_version_inputs tx{};
    if (!store_.tx.get(link, tx)) // 2.24%
        return false;

    table::puts::get_spends puts{};
    puts.spend_fks.resize(tx.ins_count);
    if (!store_.puts.get(tx.puts_fk, puts)) // 9.2%
        return false;

    set.tx = link;
    set.version = tx.version;
    set.spends.reserve(puts.spend_fks.size());
    const auto ptr = store_.spend.get_memory();

    // This is not concurrent because get_spend_sets is concurrent (by tx).
    for (const auto& spend_fk: puts.spend_fks)
    {
        table::spend::get_spend_set_value spend{};
        if (!store_.spend.get(ptr, spend_fk, spend)) // 0.55%
            return false;

        set.spends.push_back(std::move(spend.value));
    }

    return true;
}

TEMPLATE
bool CLASS::populate_prevouts(spend_sets& sets, size_t spends,
    const header_link& link) const NOEXCEPT
{
    table::prevout::record_get prevouts{};
    prevouts.values.resize(spends);
    if (!store_.prevout.at(link, prevouts))
        return false;

    // This technique stores internal spends as null points in order to
    // maintain spend positions relative to spends.
    size_t index{};
    for (auto& set: sets)
        for (auto& spend: set.spends)
        {
            // These are generated during validation and stored into a single
            // arraymap allocation at that time (no search).
            spend.coinbase = prevouts.coinbase(index);
            spend.prevout_tx = prevouts.output_tx_fk(index++);
        }

    return true;
}

TEMPLATE
bool CLASS::populate_prevouts(spend_sets& sets) const NOEXCEPT
{
    // This technique does not benefit from skipping internal spends, and
    // therefore also requires set_strong before query, and self removal.
    for (auto& set: sets)
        for (auto& spend: set.spends)
        {
            // TODO: is_coinbase and to_tx can be combined.
            // These are the queries avoided by the points table.
            spend.prevout_tx = to_tx(get_point_key(spend.point_fk));
            spend.coinbase = is_coinbase(spend.prevout_tx);

            // Garud against because terminal is excluded in common code.
            if (spend.prevout_tx == table::prevout::tx::terminal)
                return false;
        }

    return true;
}

// block_confirmable
// ----------------------------------------------------------------------------
// C++17 incomplete on GCC/CLang, so std fns presently parallel only on MSVC++.

TEMPLATE
code CLASS::block_confirmable(const header_link& link) const NOEXCEPT
{
    constexpr auto parallel = poolstl::execution::par;

    context ctx{};
    if (!get_context(ctx, link))
        return error::integrity7;

    // bip30 coinbase check.
    ////code ec{};
    ////if ((ec = unspent_duplicates(link, ctx)))
    ////    return ec;

    // Empty block is success.
    const auto txs = to_spending_transactions(link);
    if (txs.empty())
        return error::success;

    // One spend set per tx.
    spend_sets sets(txs.size());
    std::atomic<size_t> spends{ zero };
    std::atomic<error::error_t> failure{ error::success };

    const auto to_set = [this, &spends, &failure](const auto& tx) NOEXCEPT
    {
        spend_set set{};
        if (!get_spend_set(set, tx))
            failure.store(error::integrity8);

        spends.fetch_add(set.spends.size(), std::memory_order_relaxed);
        return set;
    };

    // 14.99%
    std::transform(parallel, txs.begin(), txs.end(), sets.begin(), to_set);
    if (failure)
        return { failure.load() };

    if (!(prevout_enabled() ? populate_prevouts(sets, spends, link) :
        populate_prevouts(sets)))
        return error::integrity9;

    const auto is_unspendable = [this, &ctx, &failure](const auto& set) NOEXCEPT
    {
        error::error_t ec{};
        for (const auto& spend: set.spends)
            if ((spend.prevout_tx != table::prevout::tx::terminal) &&
                ((ec = unspendable_prevout(spend.sequence, spend.coinbase,
                    spend.prevout_tx, set.version, ctx))))
                failure.store(ec);

        return failure != error::success;
    };

    const auto is_spent = [this, &failure](const auto& set) NOEXCEPT
    {
        error::error_t ec{};
        for (const auto& spend: set.spends)
            if ((spend.prevout_tx != table::prevout::tx::terminal) &&
                ((ec = spent_prevout(spend.point_fk, spend.point_index,
                    spend.point_stub, set.tx))))
                failure.store(ec);

        return failure != error::success;
    };

    // 43.83%
    if (std::any_of(parallel, sets.begin(), sets.end(), is_unspendable))
        return { failure.load() };

    // 37.55%
    if (std::any_of(parallel, sets.begin(), sets.end(), is_spent))
        return { failure.load() };

    return error::success;
}

// setters
// ----------------------------------------------------------------------------

// protected
// This is also invoked from block.txs archival (when checkpoint/milestone).
TEMPLATE
bool CLASS::set_strong(const header_link& link, const tx_links& txs,
    bool positive) NOEXCEPT
{
    using namespace system;
    using link_t = table::strong_tx::link;
    using element_t = table::strong_tx::record;

    // Preallocate all strong_tx records for the block and reuse memory ptr.
    const auto records = possible_narrow_cast<link_t::integer>(txs.size());
    auto record = store_.strong_tx.allocate(records);
    const auto ptr = store_.strong_tx.get_memory();

    for (const auto tx: txs)
        if (!store_.strong_tx.put(ptr, record++, link_t{ tx }, element_t
        {
            {},
            link,
            positive
        })) return false;

    return true;
}

TEMPLATE
bool CLASS::set_strong(const header_link& link) NOEXCEPT
{
    const auto txs = to_transactions(link);
    if (txs.empty())
        return false;

    // ========================================================================
    const auto scope = store_.get_transactor();

    // Clean allocation failure (e.g. disk full).
    return set_strong(link, txs, true);
    // ========================================================================
}

TEMPLATE
bool CLASS::set_unstrong(const header_link& link) NOEXCEPT
{
    const auto txs = to_transactions(link);
    if (txs.empty())
        return false;

    // ========================================================================
    const auto scope = store_.get_transactor();

    // Clean allocation failure (e.g. disk full).
    return set_strong(link, txs, false);
    // ========================================================================
}

TEMPLATE
bool CLASS::set_prevouts(const header_link& link, const block& block) NOEXCEPT
{
    if (!prevout_enabled())
        return true;

    // Empty or coinbase only implies no spends.
    if (block.transactions() <= one)
        return true;

    // ========================================================================
    const auto scope = store_.get_transactor();

    // Clean single allocation failure (e.g. disk full).
    const table::prevout::record_put_ref prevouts{ {}, block };
    return store_.prevout.put(link, prevouts);
    // ========================================================================
}

} // namespace database
} // namespace libbitcoin

#endif
