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
// the spent (and contested) bits of the output and puts its spender row.
// Binding is content, so rebinding is idempotent and nothing here is written
// or cleared by confirmation or reorganization (see consensus_block.ipp).
// Each pass holds the accessor of one table.
// ----------------------------------------------------------------------------

// protected
TEMPLATE
code CLASS::get_spends(block_spends& out,
    const header_link& link) const NOEXCEPT
{
    const auto txs = to_transactions(link);
    if (txs.empty())
        return error::integrity;

    // The coinbase spends no output.
    std::vector<table::transaction::get_puts> puts(sub1(txs.size()));
    {
        const auto ptr = store_.tx.get_memory();
        for (size_t index{}; index < puts.size(); ++index)
            if (!store_.tx.get(ptr, txs.at(add1(index)), puts.at(index)))
                return error::integrity;
    }

    size_t inputs{};
    for (const auto& put: puts)
        inputs += put.ins_count;

    out.clear();
    out.reserve(inputs);
    for (size_t index{}; index < puts.size(); ++index)
    {
        const auto& put = puts.at(index);
        const auto end = put.points_fk + put.ins_count;
        for (auto ins = put.points_fk; ins < end; ++ins)
            out.push_back({ txs.at(add1(index)), ins });
    }

    return get_spent_outs(out);
}

// protected
TEMPLATE
code CLASS::get_spends(block_spends& out,
    const header_links& links) const NOEXCEPT
{
    std::vector<std::pair<size_t, tx_link::integer>> txs{};
    for (size_t block{}; block < links.size(); ++block)
    {
        const auto fks = to_transactions(links.at(block));
        if (fks.empty())
            return error::integrity;

        // The coinbase spends no output.
        for (auto fk = std::next(fks.cbegin()); fk != fks.cend(); ++fk)
            txs.emplace_back(block, *fk);
    }

    std::vector<table::transaction::get_puts> puts(txs.size());
    {
        const auto ptr = store_.tx.get_memory();
        for (size_t index{}; index < txs.size(); ++index)
            if (!store_.tx.get(ptr, txs.at(index).second, puts.at(index)))
                return error::integrity;
    }

    size_t inputs{};
    for (const auto& put: puts)
        inputs += put.ins_count;

    out.clear();
    out.reserve(inputs);
    for (size_t index{}; index < txs.size(); ++index)
    {
        const auto& [block, tx] = txs.at(index);
        const auto& put = puts.at(index);
        const auto end = put.points_fk + put.ins_count;
        for (auto ins = put.points_fk; ins < end; ++ins)
        {
            out.push_back({ tx, ins });
            out.back().block = block;
        }
    }

    return get_spent_outs(out);
}

// protected
TEMPLATE
code CLASS::get_spent_outs(block_spends& out) const NOEXCEPT
{
    // Pooled txs of the block are bound.
    {
        const auto ptr = store_.spend.get_memory();
        for (auto& spend: out)
            spend.outs = store_.spend.at(ptr, spend.ins).value;
    }

    // A banked input's point is positioned at its ins less the bank base.
    for (const auto bank: { false, true })
    {
        const auto& points = store_.points_bank(bank);
        const auto base = points.base().value;
        const auto body = points.get_memory();
        for (auto& spend: out)
        {
            if (!outs_link{ spend.outs }.is_terminal() || spend.ins < base ||
                !table::points::link{ spend.row }.is_terminal())
                continue;

            table::points::get_composed point{};
            const table::points::link row{ spend.ins - base };
            if (points.get(body, row, point))
            {
                spend.row = row.value;
                spend.key = std::move(point.key);
            }
        }
    }

    // Inputs neither bound nor banked are unbound (drained).
    for (auto& spend: out)
    {
        if (!outs_link{ spend.outs }.is_terminal() ||
            !table::points::link{ spend.row }.is_terminal())
            continue;

        outs_link spent{};
        if (!get_spent_or_point(spent, spend.key, spend.ins))
            return error::integrity;

        spend.outs = spent.value;
    }

    {
        const auto ptr = store_.tx.get_memory();
        for (auto& spend: out)
        {
            if (!outs_link{ spend.outs }.is_terminal())
                continue;

            const auto tx = store_.tx.first(ptr, spend.key.hash());
            if (tx.is_terminal())
                continue;

            table::transaction::get_output output{ {}, spend.key.index() };
            if (!store_.tx.get(ptr, tx, output))
                return error::integrity;

            spend.outs = output.outs_fk;
        }
    }

    // A bucket top archived after the accessor ends its walk, so retry misses.
    for (auto& spend: out)
        if (outs_link{ spend.outs }.is_terminal())
            spend.outs = to_output(spend.key).value;

    return error::success;
}

// writers
// ----------------------------------------------------------------------------

// protected
TEMPLATE
bool CLASS::set_spends(block_spends& spends) NOEXCEPT
{
    {
        const auto ptr = store_.spend.get_memory();
        for (auto& spend: spends)
            spend.pending = !outs_link{ spend.outs }.is_terminal() &&
                !store_.spend.set(ptr, spend.ins,
                    table::spend::bind{ {}, spend.first, spend.outs });
    }

    // Cells beyond the accessor extend the count.
    for (auto& spend: spends)
        if (spend.pending && !store_.spend.set(spend.ins,
            table::spend::bind{ {}, spend.first, spend.outs }))
            return false;

    {
        const auto ptr = store_.contested.get_memory();
        for (auto& spend: spends)
            spend.pending = spend.first && !store_.contested.set(ptr,
                table::contested::to_cell(spend.outs),
                table::contested::spend{ {}, spend.outs });
    }

    // Cells beyond the accessor extend the count.
    for (const auto& spend: spends)
        if (spend.pending && !store_.contested.set(
            table::contested::to_cell(spend.outs),
            table::contested::spend{ {}, spend.outs }))
            return false;

    if (!set_unbanked(spends, true))
        return false;

    // Rows of first bindings are allocated together (one claim per call).
    size_t rows{};
    for (const auto& spend: spends)
        rows += to_int<size_t>(spend.first);

    if (is_zero(rows))
        return true;

    using count = table::spender::link::integer;
    auto fk = store_.spender.allocate(
        system::possible_narrow_cast<count>(rows));
    if (fk.is_terminal())
        return false;

    const auto ptr = store_.spender.get_memory();
    for (const auto& spend: spends)
        if (spend.first && !store_.spender.put(ptr, fk++,
            table::spender::to_key(spend.outs),
            table::spender::record{ {}, spend.tx }))
            return false;

    return true;
}

// protected
// A banked input without a binding keeps its point across the bank drop.
TEMPLATE
bool CLASS::set_unbound(const block_spends& spends) NOEXCEPT
{
    for (const auto& spend: spends)
    {
        if (!outs_link{ spend.outs }.is_terminal())
            continue;

        const auto key = table::unbound::to_key(spend.ins);
        if (store_.unbound.first(key).is_terminal() && !store_.unbound.put(key,
            table::unbound::record{ {}, spend.key }))
            return false;
    }

    return set_unbanked(spends, false);
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
// The bits of the bound (or unbound) inputs are cleared, a bit beyond the
// count is not banked.
TEMPLATE
bool CLASS::set_unbanked(const block_spends& spends, bool bound) NOEXCEPT
{
    const auto ptr = store_.banked.get_memory();
    const auto cells = store_.banked.count().value;
    for (const auto& spend: spends)
    {
        const auto cell = table::banked::to_cell(spend.ins);
        if ((outs_link{ spend.outs }.is_terminal() == bound) || (cell >= cells))
            continue;

        if (!store_.banked.set(ptr, cell,
            table::banked::unbank{ {}, table::banked::to_bit(spend.ins) }))
            return false;
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
    block_spends spends{ { to_input_tx(link).value, link.value } };
    spends.front().outs = spent.value;

    // ========================================================================
    const auto scope = get_transactor();
    return set_spends(spends);
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
    block_spends spends{};
    for (auto index = one; index < txs.size(); ++index)
    {
        const auto& tx = txs.at(index);
        for (auto in = tx.inputs_begin(); in != tx.inputs_end(); ++in)
        {
            spends.push_back({ links.at(index), in->metadata.input_link });
            spends.back().outs = in->metadata.output_link;
            if (outs_link{ in->metadata.output_link }.is_terminal())
            {
                spends.back().key = { in->point_hash(), in->point_index() };
                found = false;
            }
        }
    }

    // ========================================================================
    const auto scope = get_transactor();
    if (!set_spends(spends) || !set_unbound(spends))
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
    block_spends spends{};
    spends.reserve(record.ins_count);
    auto fk = record.points_fk;
    for (const auto& in: *tx.inputs_ptr())
    {
        const outs_link spent{ in->metadata.output_link };
        spends.push_back({ link.value, fk++ });
        spends.back().outs = (spent.is_terminal() ?
            to_output(in->point()) : spent).value;
        found &= !outs_link{ spends.back().outs }.is_terminal();
    }

    // ========================================================================
    const auto scope = get_transactor();
    if (!set_spends(spends))
        return error::integrity;
    // ========================================================================

    if (!found)
        return system::error::missing_previous_output;

    return error::success;
}

TEMPLATE
code CLASS::bind(std::vector<code>& out, const header_links& links) NOEXCEPT
{
    block_spends spends{};
    if (const auto ec = get_spends(spends, links))
        return ec;

    out.assign(links.size(), error::success);
    for (const auto& spend: spends)
        if (outs_link{ spend.outs }.is_terminal())
            out.at(spend.block) = system::error::missing_previous_output;

    // ========================================================================
    const auto scope = get_transactor();
    if (!set_spends(spends) || !set_unbound(spends))
        return error::integrity;
    // ========================================================================

    return error::success;
}

TEMPLATE
code CLASS::bind(const header_link& link) NOEXCEPT
{
    block_spends spends{};
    if (const auto ec = get_spends(spends, link))
        return ec;

    auto found = true;
    for (const auto& spend: spends)
        found &= !outs_link{ spend.outs }.is_terminal();

    // ========================================================================
    const auto scope = get_transactor();
    if (!set_spends(spends) || !set_unbound(spends))
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
    block_spends spends{};
    spends.reserve(record.ins_count);
    const auto end = record.points_fk + record.ins_count;
    for (auto fk = record.points_fk; fk < end; ++fk)
    {
        outs_link spent{};
        point key{};
        if (!get_spent_or_point(spent, key, fk))
            return error::integrity;

        spends.push_back({ link.value, fk });
        spends.back().outs = (spent.is_terminal() ? to_output(key) :
            spent).value;
        found &= !outs_link{ spends.back().outs }.is_terminal();
    }

    // ========================================================================
    const auto scope = get_transactor();
    if (!set_spends(spends))
        return error::integrity;
    // ========================================================================

    if (!found)
        return system::error::missing_previous_output;

    return error::success;
}

} // namespace database
} // namespace libbitcoin

#endif
