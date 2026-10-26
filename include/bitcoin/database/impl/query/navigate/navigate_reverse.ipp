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
#ifndef LIBBITCOIN_DATABASE_QUERY_NAVIGATE_REVERSE_IPP
#define LIBBITCOIN_DATABASE_QUERY_NAVIGATE_REVERSE_IPP

#include <atomic>
#include <ranges>
#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

// block->block[parent]
// ----------------------------------------------------------------------------
// Presumed to guaranee consistency as headers are not duplicated by the node.

TEMPLATE
header_link CLASS::to_parent(const header_link& link) const NOEXCEPT
{
    table::header::get_parent_fk header{};
    if (!store_.header.get(link, header))
        return {};

    // Terminal implies genesis (no parent).
    return header.parent_fk;
}

TEMPLATE
header_link CLASS::to_candidate_child(const header_link& link) const NOEXCEPT
{
    size_t height{};
    if (!get_height(height, link))
        return {};

    const auto child = to_candidate(add1(height));
    return (to_parent(child) == link) ? child : header_link{};
}

TEMPLATE
header_link CLASS::to_confirmed_child(const header_link& link) const NOEXCEPT
{
    size_t height{};
    if (!get_height(height, link))
        return {};

    const auto child = to_confirmed(add1(height));
    return (to_parent(child) == link) ? child : header_link{};
}

// address->outputs[receivers]
// ----------------------------------------------------------------------------
// There can be multiple spenders of the same output (due to conflicts) and
// multiple instances of an output under distinct links (due to tx dups).

TEMPLATE
code CLASS::to_touched_txs(tx_links& out,
    const outs_links& outputs) const NOEXCEPT
{
    const stopper cancel{};
    return to_touched_txs(cancel, out, outputs);
}

TEMPLATE
code CLASS::to_touched_txs(const stopper& cancel, tx_links& out,
    const outs_links& outputs) const NOEXCEPT
{
    tx_links parents{};
    parents.reserve(outputs.size());
    for (const auto& output: outputs)
    {
        if (cancel)
            return error::query_canceled;

        if (const auto tx = to_output_tx(output); tx.is_terminal())
            return error::integrity;
        else
            parents.push_back(tx);
    }

    return to_touched_txs(cancel, out, outputs, parents, false);
}

TEMPLATE
code CLASS::to_touched_txs(const stopper& cancel, tx_links& out,
    const outs_links& outputs, const tx_links& parents,
    bool turbo) const NOEXCEPT
{
    BC_ASSERT(outputs.size() == parents.size());
    const auto policy = poolstl::execution::par_if(turbo, parallel_floor);
    stopper fail{};

    // Each output resolves to its parent tx followed by its spending txs.
    std::vector<tx_links> touched(outputs.size());
    std::transform(policy, outputs.cbegin(), outputs.cend(), parents.cbegin(),
        touched.begin(), [this, &cancel, &fail](const outs_link& output,
            const tx_link& parent) NOEXCEPT
        {
            tx_links txs{};
            if (cancel || fail)
                return txs;

            txs.push_back(parent);
            for (const auto& input: std::views::reverse(to_spenders(parent, output)))
            {
                const auto tx = to_input_tx(input);
                if (tx.is_terminal())
                {
                    fail = true;
                    return txs;
                }

                txs.push_back(tx);
            }

            return txs;
        });

    if (fail)
        return error::integrity;

    if (cancel)
        return error::query_canceled;

    // Orders are reversed due to expected tx_links reversal, for faster sort.
    out.clear();
    out.reserve(two * outputs.size());
    for (const auto& txs: std::views::reverse(touched))
        out.insert(out.end(), txs.begin(), txs.end());

    return error::success;
}

TEMPLATE
code CLASS::to_address_outputs(outs_links& out,
    const hash_digest& key) const NOEXCEPT
{
    const stopper cancel{};
    return to_address_outputs(cancel, out, key);
}

TEMPLATE
code CLASS::to_address_outputs(const stopper& cancel, outs_links& out,
    const hash_digest& key) const NOEXCEPT
{
    outs_link cursor{};
    return to_address_outputs(cancel, cursor, out, key, max_size_t);
}

TEMPLATE
code CLASS::to_address_outputs(const stopper& cancel, outs_link& cursor,
    outs_links& out, const hash_digest& key, size_t limit) const NOEXCEPT
{
    tx_links parents{};
    return to_address_outputs(cancel, cursor, out, parents, key, limit);
}

TEMPLATE
code CLASS::to_address_outputs(const stopper& cancel, outs_link& cursor,
    outs_links& out, tx_links& parents, const hash_digest& key,
    size_t limit) const NOEXCEPT
{
    out.clear();
    parents.clear();

    // Limit bounds candidates, verified following iterator disposal.
    // The iterator guards the aggregate, so puts is read unguarded.
    code deferred{ error::success };
    outs_links candidates{};
    auto found = cursor.is_terminal();
    const auto end = cursor;
    auto it = store_.address.it({ key });
    for (cursor = it.get(); it; ++it)
    {
        if (cancel)
            return error::query_canceled;

        if (it.get() == end)
        {
            found = true;
            break;
        }

        // TODO: co-bucket rows consume the limit, so a collision with a large
        // TODO: address truncates an unrelated small one.
        if (is_zero(limit--))
        {
            deferred = error::depth_limited;
            break;
        }

        candidates.push_back(*it);
    }

    it.reset();
    if (!deferred && !found)
        deferred = error::invalid_cursor;

    // Verify candidates by hashing their scripts in place.
    // Parallelizable, though marshalling may exceed the benefit even if cold.
    const auto ptr = store_.output.get_memory();
    table::output::match_script_hash output{ {}, key };
    for (const auto& candidate: candidates)
    {
        if (cancel)
            return error::query_canceled;

        table::outs::record put{};
        if (!store_.outs.get(candidate, put) ||
            !store_.output.raw(ptr, put.out_fk, output))
            return error::integrity;

        if (output.match)
        {
            out.push_back(candidate);
            parents.push_back(put.parent_fk);
        }
    }

    return deferred;
}

// input|output|prevout->tx[parent]
// ----------------------------------------------------------------------------

TEMPLATE
tx_link CLASS::to_input_tx(const ins_link& link) const NOEXCEPT
{
    table::ins::get_parent ins{};
    if (!store_.ins.get(link, ins))
        return {};

    return ins.parent_fk;
}

TEMPLATE
tx_link CLASS::to_output_tx(const outs_link& link) const NOEXCEPT
{
    table::outs::get_parent out{};
    if (!store_.outs.get(link, out))
        return {};

    return out.parent_fk;
}

TEMPLATE
tx_link CLASS::to_prevout_tx(const ins_link& link) const NOEXCEPT
{
    return to_output_tx(to_spent(link));
}

// input->output[spent]
// ----------------------------------------------------------------------------
// A bound input is read from its spends row, otherwise the point is resolved
// through the tx table, as validation does before binding it.

TEMPLATE
outs_link CLASS::to_spent(const ins_link& link) const NOEXCEPT
{
    outs_link spent{};
    point key{};
    if (!get_spent_or_point(spent, key, link))
        return {};

    return spent.is_terminal() ? to_output(key) : spent;
}

// output->inputs[spenders]
// ----------------------------------------------------------------------------
// get_spenders(point) is deduped, but to_spenders(output) is not, it returns
// links to ALL spenders of the output including duplicates and double spends.
// This allows result to be navigated via to_block() to find any strong spend.
// An output has a row for each instance of its tx, so spenders of all of its
// rows are enumerated.

TEMPLATE
bool CLASS::has_spender(const outs_link& link) const NOEXCEPT
{
    using namespace system;
    const auto cell = store_.contested.at(
        table::contested::to_cell(link.value));
    return is_nonzero(bit_and<uint8_t>(cell,
        table::contested::to_spent(link.value)));
}

TEMPLATE
bool CLASS::is_contested(const outs_link& link) const NOEXCEPT
{
    using namespace system;
    const auto cell = store_.contested.at(
        table::contested::to_cell(link.value));
    return is_nonzero(bit_and<uint8_t>(cell,
        table::contested::to_contested(link.value)));
}

TEMPLATE
tx_links CLASS::to_spender_txs(const outs_link& link) const NOEXCEPT
{
    tx_links out{};
    if (!has_spender(link))
        return out;

    for (auto it = store_.spender.it(table::spender::to_key(link)); it; ++it)
    {
        table::spender::record record{};
        if (!store_.spender.get(*it, record))
            return {};

        out.push_back(record.spender_fk);
    }

    return out;
}

TEMPLATE
ins_link CLASS::to_spending_input(const tx_link& spender,
    const outs_link& link) const NOEXCEPT
{
    table::transaction::get_point tx{};
    if (!store_.tx.get(spender, tx))
        return {};

    const auto end = tx.points_fk + tx.number;
    for (auto fk = tx.points_fk; fk < end; ++fk)
        if (store_.spend.at(fk) == link)
            return fk;

    return {};
}

TEMPLATE
ins_links CLASS::to_spenders(const outs_link& link) const NOEXCEPT
{
    return to_spenders(get_outpoint_key(link));
}

TEMPLATE
ins_links CLASS::to_spenders(const tx_link& output_tx,
    uint32_t output_index) const NOEXCEPT
{
    return to_spenders(get_tx_key(output_tx), output_index);
}

TEMPLATE
ins_links CLASS::to_spenders(const tx_link& output_tx,
    const outs_link& link) const NOEXCEPT
{
    return to_spenders(output_tx, to_output_index(output_tx, link));
}

TEMPLATE
ins_links CLASS::to_spenders(const hash_digest& point_hash,
    uint32_t output_index) const NOEXCEPT
{
    return to_spenders({ point_hash, output_index });
}

TEMPLATE
ins_links CLASS::to_spenders(const point& point) const NOEXCEPT
{
    // Avoid returning spend links for coinbase inputs (not spenders).
    if (point.is_null())
        return {};

    ins_links spenders{};
    for (const auto& instance: to_duplicates(point.hash()))
    {
        const auto row = to_output(instance, point.index());
        if (row.is_terminal())
            continue;

        for (const auto& spender: to_spender_txs(row))
        {
            const auto in = to_spending_input(spender, row);
            if (!in.is_terminal())
                spenders.push_back(in);
        }
    }

    std::sort(spenders.begin(), spenders.end(), std::greater<>());
    return spenders;
}

// tx.hash->txs (all instances of same tx by hash)
// ----------------------------------------------------------------------------

TEMPLATE
tx_links CLASS::to_duplicates(const hash_digest& tx_hash) const NOEXCEPT
{
    tx_links out{};
    for (auto it = store_.tx.it(tx_hash); it; ++it)
        out.push_back(*it);

    return out;
}

// tx->block[strong]
// ----------------------------------------------------------------------------
// protected (logically hazardous)

// to_block() is ONLY the association from tx link to its associating block.
// This will be terminal when link is not of the associated instance of the
// "same" transaction. Tx links in the txs association are always consistent
// with the strong association, as strong is set using txs links.

TEMPLATE
header_link CLASS::to_block(const tx_link& link) const NOEXCEPT
{
    const auto strong = store_.strong.at(link);
    if (!table::strong::positive(strong))
        return {};

    return table::strong::header_fk(strong);
}

// utilities
// ----------------------------------------------------------------------------
// protected (presumed to not be externally useful)

TEMPLATE
uint32_t CLASS::to_input_index(const tx_link& parent_fk,
    const ins_link& point_fk) const NOEXCEPT
{
    uint32_t index{};
    for (const auto& in_fk: to_points(parent_fk))
    {
        if (in_fk == point_fk) return index;
        ++index;
    }

    return point::null_index;
}

TEMPLATE
uint32_t CLASS::to_output_index(const tx_link& parent_fk,
    const outs_link& outs_fk) const NOEXCEPT
{
    table::transaction::get_output tx{};
    if (!store_.tx.get(parent_fk, tx) || outs_fk.value < tx.outs_fk ||
        outs_fk.value >= tx.outs_fk + tx.number)
        return point::null_index;

    return system::possible_narrow_cast<uint32_t>(outs_fk.value - tx.outs_fk);
}

} // namespace database
} // namespace libbitcoin

#endif
