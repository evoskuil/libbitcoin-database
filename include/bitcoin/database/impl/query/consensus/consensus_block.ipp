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

// Each pass holds the accessor of one table, read by all of its tasks.
TEMPLATE
code CLASS::block_confirmable(const header_link& link, bool parallel) NOEXCEPT
{
    using namespace system;
    constexpr auto relaxed = std::memory_order_relaxed;
    const auto policy = poolstl::execution::par_if(parallel, parallel_floor);

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
    std::atomic_bool fault{};
    std::vector<table::transaction::record> records(txs.size());
    {
        const auto ptr = store_.tx.get_memory();
        std::transform(policy, txs.cbegin(), txs.cend(), records.begin(),
            [&](const auto& tx) NOEXCEPT
            {
                table::transaction::record record{};
                if (!store_.tx.get(ptr, tx, record))
                    fault.store(true, relaxed);

                return record;
            });
    }

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

    size_t inputs{};
    for (auto record = std::next(records.cbegin()); record != records.cend();
        ++record)
        inputs += record->ins_count;

    confirm_spends spends{};
    spends.reserve(inputs);
    for (size_t index = one; index < records.size(); ++index)
    {
        const auto& record = records.at(index);
        const auto end = record.point_fk + record.ins_count;
        for (auto ins = record.point_fk; ins < end; ++ins)
            spends.push_back({ index, ins });
    }

    {
        const auto ptr = store_.spend.get_memory();
        std::for_each(policy, spends.begin(), spends.end(),
            [&](confirm_spend& spend) NOEXCEPT
            {
                spend.outs = store_.spend.at(ptr, spend.ins).value;
                if (!table::spend::is_spent(spend.outs))
                    fault.store(true, relaxed);
            });
    }

    {
        const auto ptr = store_.ins.get_memory();
        std::for_each(policy, spends.begin(), spends.end(),
            [&](confirm_spend& spend) NOEXCEPT
            {
                table::ins::get_input in{};
                if (!store_.ins.get(ptr, spend.ins, in))
                    fault.store(true, relaxed);

                spend.sequence = in.sequence;
            });
    }

    if (fault.load(relaxed))
        return error::integrity_spendable;

    // A spend internal to the block has zero age and a non-coinbase parent
    // (the block check rejects a forward reference or coinbase spend).
    for (auto& spend: spends)
    {
        const auto next = std::upper_bound(ranges.cbegin(), ranges.cend(),
            std::pair{ spend.outs, max_uint32 });
        spend.internal = (next != ranges.cbegin()) &&
            (spend.outs < std::prev(next)->second);
    }

    {
        const auto ptr = store_.outs.get_memory();
        std::for_each(policy, spends.begin(), spends.end(),
            [&](confirm_spend& spend) NOEXCEPT
            {
                table::outs::get_parent out{};
                if (spend.internal)
                    return;

                if (!store_.outs.get(ptr, spend.outs, out))
                    fault.store(true, relaxed);

                spend.parent = out.parent_fk;
                spend.coinbase = out.coinbase;
            });
    }

    if (fault.load(relaxed))
        return error::integrity_spendable;

    // A parent strong in its own instance (others resolve by duplicates).
    {
        const auto ptr = store_.strong.get_memory();
        std::for_each(policy, spends.begin(), spends.end(),
            [&](confirm_spend& spend) NOEXCEPT
            {
                if (spend.internal)
                    return;

                const auto strong = store_.strong.at(ptr, spend.parent);
                if (table::strong::positive(strong))
                    spend.block = table::strong::header_fk(strong);
            });
    }

    {
        const auto ptr = store_.contested.get_memory();
        std::for_each(policy, spends.begin(), spends.end(),
            [&](confirm_spend& spend) NOEXCEPT
            {
                if (spend.internal ||
                    header_link{ spend.block }.is_terminal())
                    return;

                const auto cell = store_.contested.at(ptr,
                    table::contested::to_cell(spend.outs));
                spend.contested = is_nonzero(bit_and<uint8_t>(cell,
                    table::contested::to_contested(spend.outs)));
            });
    }

    // Checks all spends for spendability (strong, unlocked, mature, unspent).
    for (const auto& spend: spends)
    {
        const auto ec = spendable(spend, txs.at(spend.tx),
            records.at(spend.tx).version, ctx);

        // Recast previous_output_null as database integrity error.
        if (ec == system::error::previous_output_null)
            return error::integrity_spendable;

        if (ec)
            return ec;
    }

    return error::success;
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
system::error::transaction_error_t CLASS::spendable(const confirm_spend& spend,
    const tx_link& spender, uint32_t version, const context& ctx) NOEXCEPT
{
    using namespace system;
    const auto bip68 = ctx.is_enabled(chain::flags::bip68_rule);
    const auto sequence = spend.sequence;
    const outs_link spent{ spend.outs };

    if (spend.internal)
    {
        if (bip68 &&
            transaction::is_relative_locktime_applied(false, version,
                sequence) &&
            input::is_relative_locked(sequence, ctx.height, ctx.mtp,
                ctx.height, ctx.mtp))
            return system::error::relative_time_locked;

        return system::error::transaction_success;
    }

    const tx_link parent{ spend.parent };
    auto strong_fk = parent;
    header_link link{ spend.block };
    if (link.is_terminal())
        link = find_strong(strong_fk, get_tx_key(parent));

    if (link.is_terminal())
        return system::error::unconfirmed_spend;

    if (strong_fk == parent)
    {
        if (spend.contested)
            if (const auto ec = contested(spender, spent))
                return ec;
    }
    else
    {
        table::transaction::get_output bound{};
        if (!store_.tx.get(parent, bound))
            return system::error::previous_output_null;

        const auto offset = spent.value - bound.outs_fk;
        const auto index = possible_narrow_cast<uint32_t>(offset);
        for (const auto& instance: to_duplicates(get_tx_key(parent)))
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
        spend.coinbase, version, sequence);

    if (relative || spend.coinbase)
    {
        context prevout{};
        if (!get_context(prevout, link))
            return system::error::previous_output_null;

        if (relative &&
            input::is_relative_locked(sequence, ctx.height, ctx.mtp,
                prevout.height, prevout.mtp))
            return system::error::relative_time_locked;

        if (spend.coinbase &&
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
    for (const auto& other: to_spender_txs(spent))
        if ((other != spender) && (get_tx_key(other) != get_tx_key(spender)) &&
            is_strong_tx(other))
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
