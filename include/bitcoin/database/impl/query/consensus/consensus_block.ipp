/**
 * Copyright (c) 2011-2026 libbitcoin developers
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
#ifndef LIBBITCOIN_DATABASE_QUERY_CONSENSUS_BLOCK_IPP
#define LIBBITCOIN_DATABASE_QUERY_CONSENSUS_BLOCK_IPP

#include <atomic>
#include <ranges>
#include <bitcoin/database/define.hpp>
#include <bitcoin/database/types/types.hpp>

namespace libbitcoin {
namespace database {

// Called by organizer under strict order to determine if block is confirmable.
// ----------------------------------------------------------------------------

TEMPLATE
code CLASS::block_confirmable(const header_link& link) NOEXCEPT
{
    constexpr auto parallel = poolstl::execution::par;
    constexpr auto relaxed = std::memory_order_relaxed;

    context ctx{};
    if (!get_context(ctx, link))
        return error::integrity_block_confirmable1;

    // bip30 coinbase check (see notes).
    ////code ec{};
    ////if ((ec = spent_duplicates(link, ctx)))
    ////    return ec;

    // Coinbase txs are not populated.
    const auto txs = to_transactions(link);
    if (txs.size() <= one)
        return error::success;

    // Get the records of all block txs (the coinbase for its outs range).
    stopper fault{};
    std::vector<table::transaction::record> records(txs.size());
    std::transform(parallel, txs.cbegin(), txs.cend(), records.begin(),
        [this, &fault](const tx_link& tx) NOEXCEPT
        {
            table::transaction::record record{};
            if (!store_.tx.get(tx, record))
                fault.store(true, relaxed);

            return record;
        });

    if (fault.load(relaxed))
        return error::integrity_block_confirmable2;

    // The outs ranges of the block's txs identify its internal spends.
    outs_ranges ranges(records.size());
    std::transform(records.cbegin(), records.cend(), ranges.begin(),
        [](const table::transaction::record& record) NOEXCEPT
        {
            return std::pair{ record.outs_fk, record.outs_fk + record.outs_count };
        });

    std::sort(ranges.begin(), ranges.end());

    // Code non-integral (no atomic), so codes must be system::error.
    std::atomic<system::error::transaction_error_t> consensus{};

    // Checks all spends for spendability (strong, unlocked, mature, unspent).
    const auto confirmable = std::all_of(parallel, std::next(records.cbegin()),
        records.cend(), [&](const table::transaction::record& record) NOEXCEPT
        {
            const auto index = &record - records.data();
            const tx_link spender{ txs.at(index) };
            std::vector<outs_link::integer> spent(record.ins_count);
            if (!store_.spend.get_run(spent, record.point_fk))
            {
                fault.store(true, relaxed);
                return false;
            }

            auto fk = record.point_fk;
            for (const auto& outs_fk: spent)
            {
                table::ins::get_input in{};
                if (!table::spend::is_spent(outs_fk) || !store_.ins.get(fk++, in))
                {
                    fault.store(true, relaxed);
                    return false;
                }

                if (const auto ec = spendable(spender, outs_fk, in.sequence,
                    record.version, ctx, ranges))
                {
                    consensus.store(ec, relaxed);
                    return false;
                }
            }

            return true;
        });

    if (fault.load(relaxed))
        return error::integrity_spendable;

    if (confirmable)
        return error::success;

    // Recast previous_output_null as database integrity error.
    const auto result = consensus.load(relaxed);
    if (result == system::error::previous_output_null)
        return error::integrity_spendable;

    return result;
}

// Called by validator for a current block, prior to block production.
// ----------------------------------------------------------------------------
// Each non-coinbase tx is validated under its pooled context, which is
// sufficient for the block (absolute locktime is monotonic under bip113). What
// remains is the coinbase, the block checks over all txs, and the prevouts
// record, from which block_confirmable performs the chain checks.

TEMPLATE
code CLASS::validate_pooled(const header_link& link, const chain_context& ctx,
    uint64_t subsidy_interval, uint64_t initial_subsidy) NOEXCEPT
{
    if (!store_.pool.enabled())
        return error::unvalidated;

    code ec{};
    pooled_block block{};
    if ((ec = get_pooled_block(block, link, context::from(ctx))) ||
        (ec = check_pooled_block(block)) ||
        (ec = accept_pooled_block(block, ctx, subsidy_interval,
            initial_subsidy)))
        return ec;

    return error::success;
}

// Tx and pool records (early, as insufficiency implies full validation).
TEMPLATE
code CLASS::get_pooled_block(pooled_block& out, const header_link& link,
    const context& ctx) const NOEXCEPT
{
    using namespace system;
    out.txs = to_transactions(link);
    if (out.txs.empty() || !get_block_sizes(out.light, out.heavy, link))
        return error::integrity;

    const auto count = out.txs.size();
    out.records.resize(count);
    for (size_t index{}; index < count; ++index)
        if (!store_.tx.get(out.txs.at(index), out.records.at(index)))
            return error::integrity;

    out.pooled.resize(count);
    for (size_t index = one; index < count; ++index)
    {
        auto& tx = out.pooled.at(index);
        tx.prevouts.resize(out.records.at(index).ins_count);
        if (const auto ec = get_pooled(tx, out.txs.at(index), ctx))
            return ec;

        out.fees = ceilinged_add(out.fees, tx.fee);
        out.sigops = ceilinged_add(out.sigops, tx.sigops);
    }

    return error::success;
}

// Block check: structure, and the points pass.
TEMPLATE
code CLASS::check_pooled_block(pooled_block& block) const NOEXCEPT
{
    using namespace system;
    const auto count = block.txs.size();

    if (block.light > chain::max_block_size)
        return system::error::block_size_limit;

    if (!block.records.front().coinbase)
        return system::error::first_not_coinbase;

    for (size_t index = one; index < count; ++index)
        if (block.records.at(index).coinbase)
            return system::error::extra_coinbases;

    block.positions.reserve(count);
    for (size_t index{}; index < count; ++index)
        block.positions.emplace(get_tx_key(block.txs.at(index)), index);

    // Pooled txs are bound, so points are read from their spent outputs.
    for (size_t index = one; index < count; ++index)
    {
        const auto& record = block.records.at(index);
        std::vector<outs_link::integer> spent(record.ins_count);
        if (!store_.spend.get_run(spent, record.point_fk))
            return error::integrity;

        for (const auto& outs_fk: spent)
        {
            if (!table::spend::is_spent(outs_fk))
                return error::integrity;

            const auto point = get_outpoint_key(outs_fk);
            if (!block.points.insert(point).second)
                return system::error::block_internal_double_spend;

            // An internal spend (by hash) precedes its spender and is not of
            // the coinbase.
            if (const auto it = block.positions.find(point.hash());
                it != block.positions.end())
            {
                if (it->second >= index)
                    return system::error::forward_reference;

                if (is_zero(it->second))
                    return system::error::coinbase_maturity;
            }
        }
    }

    return error::success;
}

// Block check (context) and accept: weight, coinbase, hash limit, claim, sigops.
TEMPLATE
code CLASS::accept_pooled_block(const pooled_block& block,
    const chain_context& ctx, uint64_t subsidy_interval,
    uint64_t initial_subsidy) const NOEXCEPT
{
    using namespace system;
    const auto bip16 = ctx.is_enabled(chain::flags::bip16_rule);
    const auto bip34 = ctx.is_enabled(chain::flags::bip34_rule);
    const auto bip42 = ctx.is_enabled(chain::flags::bip42_rule);
    const auto bip50 = ctx.is_enabled(chain::flags::bip50_rule);
    const auto bip141 = ctx.is_enabled(chain::flags::bip141_rule);

    const auto weight = ceilinged_add(
        ceilinged_multiply(chain::base_size_contribution, block.light),
        ceilinged_multiply(chain::total_size_contribution, block.heavy));
    if (bip141 && weight > chain::max_block_weight)
        return system::error::block_weight_limit;

    const auto coinbase = get_transaction(block.txs.front(), true);
    if (!coinbase)
        return error::integrity;

    if (const auto ec = coinbase->check())
        return ec;

    if (const auto ec = coinbase->check(ctx))
        return ec;

    const auto& ins = *coinbase->inputs_ptr();
    if (bip34 && !ins.empty() &&
        !chain::script::is_coinbase_pattern(ins.front()->script().ops(),
            ctx.height))
        return system::error::coinbase_height_mismatch;

    if (bip50)
    {
        std::unordered_set<hash_digest> hashes{};
        for (const auto& position: block.positions)
            hashes.insert(position.first);

        for (const auto& point: block.points)
            hashes.insert(point.hash());

        if (hashes.size() > chain::hash_limit)
            return system::error::temporary_hash_limit;
    }

    const auto subsidy = chain::block::subsidy(ctx.height, subsidy_interval,
        initial_subsidy, bip42);
    if (coinbase->spend() > ceilinged_add(block.fees, subsidy))
        return system::error::coinbase_value_limit;

    const auto sigops = ceilinged_add(block.sigops,
        coinbase->signature_operations(bip16, bip141));
    if (sigops > (bip141 ? chain::max_fast_sigops : chain::max_block_sigops))
        return system::error::block_sigop_limit;

    return error::success;
}

// utility
// ----------------------------------------------------------------------------
// All return codes must be system::error::transaction_error_t (see atomic).
//
// An input is bound to the row of the parent instance that its validation
// resolved. The parent is strong through that instance or another (strong is
// resolved by hash, as always), so there is one strong row per spent output.
// Spenders bound to the strong instance read their own row, which is complete
// for strong spenders: a strong spender bound to any other instance walked to
// the strong row at its own confirmation and marked itself there. A spender
// bound to a non-strong instance walks, checking every instance's row and
// marking the strong row. The mark is content (the tx does spend the output)
// so it is never cleared, and strong remains the only chain state. Two
// spenders bind to different instances only when an instance is archived
// between their bindings, a competing-branch instance under height-ordered
// validation posting, so this is a race on a reorg, and not routine.

TEMPLATE
system::error::transaction_error_t CLASS::spendable(const tx_link& spender,
    const outs_link& spent, uint32_t sequence, uint32_t version,
    const context& ctx, const outs_ranges& ranges) NOEXCEPT
{
    using namespace system;
    const auto bip68 = ctx.is_enabled(chain::flags::bip68_rule);

    // A spend internal to the block has zero age and a non-coinbase parent
    // (the block check rejects a forward reference or coinbase spend).
    const auto next = std::upper_bound(ranges.cbegin(), ranges.cend(),
        std::pair{ spent.value, max_uint32 });
    if ((next != ranges.cbegin()) && (spent.value < std::prev(next)->second))
    {
        if (bip68 &&
            transaction::is_relative_locktime_applied(false, version,
                sequence) &&
            input::is_relative_locked(sequence, ctx.height, ctx.mtp,
                ctx.height, ctx.mtp))
            return system::error::relative_time_locked;

        return system::error::transaction_success;
    }

    table::outs::get_parent out{};
    if (!store_.outs.get(spent, out))
        return system::error::previous_output_null;

    tx_link strong_fk{};
    const auto link = find_strong(strong_fk, out.parent_fk);
    if (link.is_terminal())
        return system::error::unconfirmed_spend;

    if (strong_fk == out.parent_fk)
    {
        if (const auto ec = contested(spender, spent))
            return ec;
    }
    else
    {
        table::transaction::get_output bound{};
        if (!store_.tx.get(out.parent_fk, bound))
            return system::error::previous_output_null;

        const auto offset = spent.value - bound.outs_fk;
        const auto index = possible_narrow_cast<uint32_t>(offset);
        for (const auto& instance: to_duplicates(get_tx_key(out.parent_fk)))
        {
            const auto row = to_output(tx_link{ instance }, index);
            if (row.is_terminal())
                return system::error::previous_output_null;

            if (const auto ec = contested(spender, row))
                return ec;

            if ((instance == strong_fk) && !mark(spender, row))
                return system::error::previous_output_null;
        }
    }

    // Avoids get_context call when relative locktime is not applicable.
    const auto relative = bip68 && transaction::is_relative_locktime_applied(
        out.coinbase, version, sequence);

    if (relative || out.coinbase)
    {
        context prevout{};
        if (!get_context(prevout, link))
            return system::error::previous_output_null;

        if (relative &&
            input::is_relative_locked(sequence, ctx.height, ctx.mtp,
                prevout.height, prevout.mtp))
            return system::error::relative_time_locked;

        if (out.coinbase &&
            transaction::is_coinbase_immature(prevout.height, ctx.height))
            return system::error::coinbase_maturity;
    }

    return system::error::transaction_success;
}

TEMPLATE
system::error::transaction_error_t CLASS::contested(const tx_link& spender,
    const outs_link& spent) const NOEXCEPT
{
    // Only a strong spender of another tx conflicts (instances are one tx).
    const auto conflicts = [&](const tx_link& other) NOEXCEPT
    {
        return (other != spender) && (get_tx_key(other) !=
            get_tx_key(spender)) && is_strong_tx(other);
    };

    const auto cell = store_.spender.at(spent.value).value;
    if (!table::spender::is_contested(cell))
        return table::spender::is_spent(cell) &&
            conflicts(table::spender::spender_fk(cell)) ?
            system::error::confirmed_double_spend :
            system::error::transaction_success;

    for (const auto& other: to_spender_txs(spent))
        if (conflicts(other))
            return system::error::confirmed_double_spend;

    return system::error::transaction_success;
}

// ****************************************************************************
// CONSENSUS: To reproduce the behavior of a UTXO accumulator when reorganizing
// a BIP30 exception block, the first instance of the reorganized coinbase tx
// must be set unstrong, despite its block being strong. This creates the odd
// situation where there is a confirmed block with unconfirmed txs. Otherwise
// the txs are spendable, but in the satoshi client their outputs no longer
// exist (de-accumulated). There is discussion about fixing this issue in the
// satoshi client, which would likely result in our behavior without this
// special handling. This is moot given the existence of checkpoints, so
// presently not consensus.
// ****************************************************************************

} // namespace database
} // namespace libbitcoin

#endif
