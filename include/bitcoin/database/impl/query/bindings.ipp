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
#ifndef LIBBITCOIN_DATABASE_QUERY_BINDINGS_IPP
#define LIBBITCOIN_DATABASE_QUERY_BINDINGS_IPP

#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

// Binding.
// An input is bound to the output it spends when its prevout is first
// resolved, by writing its spend cell, and the first binding of the cell sets
// the spent (and contested) bits of the output and puts its spender row. An
// input that is not resolved remains banked, to be swept to the unbound table
// by the drain of its bank. Binding is content, so rebinding is idempotent and
// nothing here is written or cleared by confirmation or reorganization (see
// consensus_block.ipp). Each pass holds the accessor of one table.
// ----------------------------------------------------------------------------

// protected
TEMPLATE
code CLASS::get_point_sets(point_sets& out,
    const header_link& link) const NOEXCEPT
{
    const auto txs = to_transactions(link);
    if (txs.empty())
        return error::integrity;

    // The coinbase spends no output.
    out.resize(sub1(txs.size()));
    {
        const auto ptr = store_.tx.get_memory();
        for (size_t index{}; index < out.size(); ++index)
        {
            auto& set = out.at(index);
            table::transaction::get_puts puts{};
            set.tx = txs.at(add1(index));
            if (!store_.tx.get(ptr, set.tx, puts))
                return error::integrity;

            set.ins = puts.points_fk;
            set.points.resize(puts.ins_count);
        }
    }

    return get_spent_outs(out);
}

// protected
// An unbound input's point is read in place from its bank (else from the
// unbound table) and queried by tx hash. Blocks bind in parallel, so the txs
// of a block bind in sequence.
TEMPLATE
code CLASS::get_spent_outs(point_sets& sets) const NOEXCEPT
{
    constexpr auto policy = poolstl::execution::seq;

    // Pooled txs of the block are bound.
    {
        const auto ptr = store_.spend.get_memory();
        for (auto& set: sets)
        {
            auto ins = set.ins;
            for (auto& point: set.points)
                point.outs = store_.spend.at(ptr, ins++).value;
        }
    }

    {
        std::atomic_bool fault{};
        const auto& points0 = store_.points0;
        const auto& points1 = store_.points1;
        const auto base0 = points0.base().value;
        const auto base1 = points1.base().value;
        const auto rows0 = points0.get_memory();
        const auto rows1 = points1.get_memory();
        const auto txs = store_.tx.get_memory();
        std::for_each(policy, sets.begin(), sets.end(),
            [&](point_set& set) NOEXCEPT
            {
                auto ins = set.ins;
                for (auto& point: set.points)
                {
                    const auto fk = ins++;
                    if (!outs_link{ point.outs }.is_terminal())
                        continue;

                    table::points::get_composed row{};
                    if (!(fk >= base0 && points0.get(rows0,
                        table::points::link{ fk - base0 }, row)) &&
                        !(fk >= base1 && points1.get(rows1,
                        table::points::link{ fk - base1 }, row)))
                    {
                        outs_link spent{};
                        if (!get_spent_or_point(spent, row.key, fk))
                        {
                            fault.store(true, std::memory_order_relaxed);
                            return;
                        }

                        if (!spent.is_terminal())
                        {
                            point.outs = spent.value;
                            continue;
                        }
                    }

                    const auto tx = store_.tx.first(txs, row.key.hash());
                    if (tx.is_terminal())
                        continue;

                    table::transaction::get_output output{ {},
                        row.key.index() };
                    if (!store_.tx.get(txs, tx, output))
                    {
                        fault.store(true, std::memory_order_relaxed);
                        return;
                    }

                    point.outs = output.outs_fk;
                }
            });

        if (fault.load(std::memory_order_relaxed))
            return error::integrity;
    }

    // A bucket top archived after the accessor ends its walk, so retry misses.
    for (auto& set: sets)
    {
        auto ins = set.ins;
        for (auto& point: set.points)
        {
            const auto fk = ins++;
            if (!outs_link{ point.outs }.is_terminal())
                continue;

            outs_link spent{};
            system::chain::point key{};
            if (!get_spent_or_point(spent, key, fk))
                return error::integrity;

            point.outs = (spent.is_terminal() ? to_output(key) : spent).value;
        }
    }

    return error::success;
}

// writers
// ----------------------------------------------------------------------------

// protected
TEMPLATE
bool CLASS::set_spends(point_sets& sets) NOEXCEPT
{
    {
        const auto ptr = store_.spend.get_memory();
        for (auto& set: sets)
        {
            auto ins = set.ins;
            for (auto& point: set.points)
                point.pending = !outs_link{ point.outs }.is_terminal() &&
                    !store_.spend.set(ptr, ins++,
                        table::spend::bind{ {}, point.first, point.outs });
        }
    }

    // Cells beyond the accessor extend the count.
    for (auto& set: sets)
    {
        auto ins = set.ins;
        for (auto& point: set.points)
        {
            const auto fk = ins++;
            if (point.pending && !store_.spend.set(fk,
                table::spend::bind{ {}, point.first, point.outs }))
                return false;
        }
    }

    {
        const auto ptr = store_.contested.get_memory();
        for (auto& set: sets)
            for (auto& point: set.points)
                point.pending = point.first && !store_.contested.set(ptr,
                    table::contested::to_cell(point.outs),
                    table::contested::spend{ {}, point.outs });
    }

    // Cells beyond the accessor extend the count.
    for (const auto& set: sets)
        for (const auto& point: set.points)
            if (point.pending && !store_.contested.set(
                table::contested::to_cell(point.outs),
                table::contested::spend{ {}, point.outs }))
                return false;

    if (!set_unbanked(sets))
        return false;

    // Rows of first bindings are allocated together (one claim per call).
    size_t rows{};
    for (const auto& set: sets)
        for (const auto& point: set.points)
            rows += to_int<size_t>(point.first);

    if (is_zero(rows))
        return true;

    using count = table::spender::link::integer;
    auto fk = store_.spender.allocate(
        system::possible_narrow_cast<count>(rows));
    if (fk.is_terminal())
        return false;

    const auto ptr = store_.spender.get_memory();
    for (const auto& set: sets)
        for (const auto& point: set.points)
            if (point.first && !store_.spender.put(ptr, fk++,
                table::spender::to_key(point.outs),
                table::spender::record{ {}, set.tx }))
                return false;

    return true;
}

// protected
// The bits of an archived range of inputs are set, cell by cell.
TEMPLATE
bool CLASS::set_banked(ins_link::integer first, size_t count) NOEXCEPT
{
    using namespace system;
    const auto end = first + possible_narrow_cast<ins_link::integer>(count);
    for (auto ins = first; ins < end;)
    {
        uint8_t mask{};
        const auto cell = table::banked::to_cell(ins);
        for (; ins < end && table::banked::to_cell(ins) == cell; ++ins)
            mask = bit_or<uint8_t>(mask, table::banked::to_bit(ins));

        if (!store_.banked.set(cell, table::banked::bank{ {}, mask }))
            return false;
    }

    return true;
}

// protected
// The bits of the bound inputs are cleared, a bit beyond the count is not
// banked.
TEMPLATE
bool CLASS::set_unbanked(const point_sets& sets) NOEXCEPT
{
    const auto ptr = store_.banked.get_memory();
    const auto cells = store_.banked.count().value;
    for (const auto& set: sets)
    {
        auto ins = set.ins;
        for (const auto& point: set.points)
        {
            const auto fk = ins++;
            const auto cell = table::banked::to_cell(fk);
            if (outs_link{ point.outs }.is_terminal() || (cell >= cells))
                continue;

            if (!store_.banked.set(ptr, cell,
                table::banked::unbank{ {}, table::banked::to_bit(fk) }))
                return false;
        }
    }

    return true;
}

// protected
TEMPLATE
bool CLASS::mark(const tx_link& spender, const outs_link& spent) NOEXCEPT
{
    if (spender.is_terminal())
        return false;

    // Each spender of an output is recorded once.
    const auto key = table::spender::to_key(spent);
    for (auto it = store_.spender.it(key); it; ++it)
    {
        table::spender::record record{};
        if (!store_.spender.get(*it, record))
            return false;

        if (record.spender_fk == spender)
            return true;
    }

    return store_.contested.set(table::contested::to_cell(spent.value),
        table::contested::spend{ {}, spent.value }) &&
        store_.spender.put(key, table::spender::record{ {}, spender });
}

TEMPLATE
bool CLASS::spend(const ins_link& link, const outs_link& spent) NOEXCEPT
{
    point_sets sets{ { to_input_tx(link).value, link.value, { {} } } };
    sets.front().points.front().outs = spent.value;

    // ========================================================================
    const auto scope = get_transactor();
    return set_spends(sets);
    // ========================================================================
}

TEMPLATE
code CLASS::bind(const header_link& link, const block_view& block) NOEXCEPT
{
    const auto& txs = block.views();
    const auto links = to_transactions(link);
    if (links.empty() || links.size() != txs.size())
        return error::integrity;

    auto found = true;
    point_sets sets(sub1(txs.size()));
    for (auto index = one; index < txs.size(); ++index)
    {
        const auto& tx = txs.at(index);
        auto& set = sets.at(sub1(index));
        set.tx = links.at(index);
        set.points.resize(tx.inputs());

        auto point = set.points.begin();
        for (auto in = tx.inputs_begin(); in != tx.inputs_end(); ++in)
        {
            if (in == tx.inputs_begin())
                set.ins = in->metadata.input_link;

            point++->outs = in->metadata.output_link;
            found &= !outs_link{ in->metadata.output_link }.is_terminal();
        }
    }

    // ========================================================================
    const auto scope = get_transactor();
    if (!set_spends(sets))
        return error::integrity;
    // ========================================================================

    if (!found)
        return system::error::missing_previous_output;

    return error::success;
}

TEMPLATE
code CLASS::bind(const tx_link& link, const transaction& tx) NOEXCEPT
{
    table::transaction::get_puts record{};
    if (!store_.tx.get(link, record))
        return error::integrity;

    auto found = true;
    point_sets sets{ { link.value, record.points_fk, {} } };
    auto& points = sets.front().points;
    points.resize(record.ins_count);

    auto point = points.begin();
    for (const auto& in: *tx.inputs_ptr())
    {
        const outs_link spent{ in->metadata.output_link };
        point->outs = (spent.is_terminal() ? to_output(in->point()) :
            spent).value;
        found &= !outs_link{ point++->outs }.is_terminal();
    }

    // ========================================================================
    const auto scope = get_transactor();
    if (!set_spends(sets))
        return error::integrity;
    // ========================================================================

    if (!found)
        return system::error::missing_previous_output;

    return error::success;
}

TEMPLATE
code CLASS::bind(const header_link& link) NOEXCEPT
{
    point_sets sets{};
    if (const auto ec = get_point_sets(sets, link))
        return ec;

    auto found = true;
    for (const auto& set: sets)
        for (const auto& point: set.points)
            found &= !outs_link{ point.outs }.is_terminal();

    // ========================================================================
    const auto scope = get_transactor();
    if (!set_spends(sets))
        return error::integrity;
    // ========================================================================

    if (!found)
        return system::error::missing_previous_output;

    return error::success;
}

TEMPLATE
code CLASS::bind(const tx_link& link) NOEXCEPT
{
    if (is_coinbase(link))
        return error::success;

    table::transaction::get_puts record{};
    if (!store_.tx.get(link, record))
        return error::integrity;

    auto found = true;
    point_sets sets{ { link.value, record.points_fk, {} } };
    auto& points = sets.front().points;
    points.resize(record.ins_count);

    auto fk = record.points_fk;
    for (auto& point: points)
    {
        outs_link spent{};
        system::chain::point key{};
        if (!get_spent_or_point(spent, key, fk++))
            return error::integrity;

        point.outs = (spent.is_terminal() ? to_output(key) : spent).value;
        found &= !outs_link{ point.outs }.is_terminal();
    }

    // ========================================================================
    const auto scope = get_transactor();
    if (!set_spends(sets))
        return error::integrity;
    // ========================================================================

    if (!found)
        return system::error::missing_previous_output;

    return error::success;
}

} // namespace database
} // namespace libbitcoin

#endif
