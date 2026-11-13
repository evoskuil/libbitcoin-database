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
#ifndef LIBBITCOIN_DATABASE_QUERY_POOL_IPP
#define LIBBITCOIN_DATABASE_QUERY_POOL_IPP

#include <bit>
#include <unordered_map>
#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

// Transaction pool.
// ----------------------------------------------------------------------------

TEMPLATE
bool CLASS::is_pooled(const tx_link& link) const NOEXCEPT
{
    return store_.pool.enabled() && !store_.pool.first(link).is_terminal();
}

TEMPLATE
bool CLASS::get_pooled_time(uint32_t& out, const tx_link& link) const NOEXCEPT
{
    if (!store_.pool.enabled())
        return false;

    const auto fk = store_.pool.first(link);
    if (fk.is_terminal())
        return false;

    table::pool::get_time pooled{};
    if (!store_.pool.get(fk, pooled))
        return false;

    out = pooled.time;
    return true;
}

TEMPLATE
bool CLASS::is_confirmable_tx(const tx_link& link) const NOEXCEPT
{
    confirmables memo{};
    return is_confirmable_tx(memo, link);
}

TEMPLATE
bool CLASS::is_confirmable_spent(const output_link& link) const NOEXCEPT
{
    for (const auto& in: to_spenders(link))
    {
        const auto tx = to_input_tx(in);
        if (is_pooled(tx) && is_confirmable_tx(tx))
            return true;
    }

    return false;
}

TEMPLATE
ins_link CLASS::find_confirmable_spender(const point& prevout) const NOEXCEPT
{
    for (const auto& in: to_spenders(prevout))
    {
        const auto tx = to_input_tx(in);
        if (is_pooled(tx) && is_confirmable_tx(tx))
            return in;
    }

    return {};
}

TEMPLATE
code CLASS::get_pooled(pooled_tx& out, const tx_link& link,
    const context& ctx) const NOEXCEPT
{
    using prevout = table::prevout::slab_get;
    if (!store_.pool.enabled())
        return error::unvalidated;

    const auto fk = store_.pool.first(link);
    if (fk.is_terminal())
        return error::unvalidated;

    table::pool::record valid{};
    if (!store_.pool.get(fk, valid))
        return error::integrity;

    if (!is_sufficient(ctx, valid.ctx))
        return error::unvalidated;

    table::pool_edge edge{};
    table::pool_total total{};
    if (!store_.pool.totals.get(fk, total) || !store_.pool.edges.get(fk, edge))
        return error::integrity;

    table::spends::get_refs::parents parents(out.prevouts.size());
    table::spends::get_refs run{ {}, parents };
    if (!store_.spends.get(edge.spends_fk, run))
        return error::integrity;

    out.fee = total.fee;
    out.sigops = total.sigops;
    std::ranges::transform(parents, out.prevouts.begin(),
        [](auto merged) NOEXCEPT
        {
            return pooled_tx::prevout
            {
                prevout::output_tx_fk(merged), prevout::coinbase(merged)
            };
        });

    return error::success;
}

// A terminal cursor starts at the first record, and the cursor is set to end
// once the records are exhausted.
TEMPLATE
code CLASS::get_confirmable_txs(pool_link& cursor, tx_links& out,
    const pool_link& end, size_t limit) const NOEXCEPT
{
    out.clear();
    auto fk = cursor.is_terminal() ? pool_link::integer{} : cursor.value;
    if (fk > end)
        return error::invalid_cursor;

    if (!store_.pool.enabled())
    {
        cursor = end;
        return error::success;
    }

    confirmables memo{};
    const auto stop = std::min(end.value, store_.pool.count().value);
    for (; fk < stop && out.size() < limit; ++fk)
    {
        const auto link = to_pool_tx(fk);
        if (!link.is_terminal() && is_confirmable_tx(memo, link))
            out.push_back(link);
    }

    cursor = fk == stop ? end : pool_link{ fk };
    return error::success;
}

TEMPLATE
code CLASS::get_confirmable_txs(tx_links& out) const NOEXCEPT
{
    out.clear();
    if (!store_.pool.enabled())
        return error::success;

    pool_link cursor{};
    return get_confirmable_txs(cursor, out, store_.pool.count(), max_size_t);
}

// An unconfirmed parent of a confirmable tx is itself confirmable.
TEMPLATE
tx_links CLASS::to_confirmable_parents(const tx_link& link) const NOEXCEPT
{
    tx_links out{};
    for (const auto& point: to_points(link))
    {
        const auto parent = to_tx(get_point_key(point).hash());
        if (!parent.is_terminal() && !is_confirmed_tx(parent) &&
            is_pooled(parent) && !system::contains(out, parent.value))
            out.push_back(parent.value);
    }

    return out;
}

TEMPLATE
tx_links CLASS::to_confirmable_children(const tx_link& link) const NOEXCEPT
{
    tx_links out{};
    for (const auto& output: to_outputs(link))
    {
        for (const auto& in: to_spenders(output))
        {
            const auto child = to_input_tx(in);
            if (!child.is_terminal() && is_pooled(child) &&
                is_confirmable_tx(child) &&
                !system::contains(out, child.value))
                out.push_back(child.value);
        }
    }

    return out;
}

TEMPLATE
tx_links CLASS::to_confirmable_ancestors(const tx_link& link) const NOEXCEPT
{
    return to_confirmable_relatives(link, true);
}

TEMPLATE
tx_links CLASS::to_confirmable_descendants(const tx_link& link) const NOEXCEPT
{
    return to_confirmable_relatives(link, false);
}

TEMPLATE
bool CLASS::get_ancestor_totals(fee_totals& out,
    const tx_link& link) const NOEXCEPT
{
    return get_confirmable_totals(out, link, true);
}

TEMPLATE
bool CLASS::get_descendant_totals(fee_totals& out,
    const tx_link& link) const NOEXCEPT
{
    return get_confirmable_totals(out, link, false);
}

TEMPLATE
bool CLASS::get_confirmable_fees(fee_rates& out) const NOEXCEPT
{
    out.clear();
    tx_links links{};
    if (get_confirmable_txs(links))
        return false;

    out.resize(links.size());
    auto rate = out.begin();
    for (const auto& link: links)
        if (!get_tx_fees(*rate++, link))
            return false;

    return true;
}

// protected
// The first record of a tx is its last committed, so the tx of any earlier or
// uncommitted record (its key does not resolve to it) is terminal.
TEMPLATE
tx_link CLASS::to_pool_tx(const pool_link& link) const NOEXCEPT
{
    const tx_link tx{ store_.pool.get_key(link) };
    return store_.pool.first(tx) == link ? tx : tx_link{};
}

TEMPLATE
bool CLASS::get_pooled_fee(uint64_t& out, const tx_link& link) const NOEXCEPT
{
    size_t height{};
    return get_pooled_fee(out, height, link);
}

TEMPLATE
bool CLASS::get_pooled_fee(uint64_t& fee, size_t& height,
    const tx_link& link) const NOEXCEPT
{
    size_t size{};
    return get_pooled_fee(fee, size, height, link);
}

TEMPLATE
bool CLASS::get_pooled_fee(uint64_t& fee, size_t& size, size_t& height,
    const tx_link& link) const NOEXCEPT
{
    if (!store_.pool.enabled())
        return false;

    const auto fk = store_.pool.first(link);
    if (fk.is_terminal())
        return false;

    table::pool_total total{};
    table::pool::get_height pooled{};
    if (!store_.pool.get(fk, pooled) || !store_.pool.totals.get(fk, total))
        return false;

    fee = total.fee;
    size = total.bytes;
    height = pooled.height;
    return true;
}

TEMPLATE
hash_digest CLASS::get_pooled_wtxid(const table::pool::link& link) const NOEXCEPT
{
    using namespace system;
    table::pool_word id0{}, id1{}, id2{}, id3{};
    if (!store_.pool.id0.get(link, id0) ||
        !store_.pool.id1.get(link, id1) ||
        !store_.pool.id2.get(link, id2) ||
        !store_.pool.id3.get(link, id3))
        return {};

    using lane_t = schema::pool::witness_lane;
    using lanes_t = std_array<lane_t, schema::pool::witness_lanes>;
    const auto words = to_little_endians(lanes_t
    {
        id0.word, id1.word, id2.word, id3.word
    });

    return array_cast<uint8_t>(words);
}

// A walk that finds no confirmed spend proves each tx visited confirmable.
TEMPLATE
bool CLASS::is_confirmable_tx(confirmables& memo,
    const tx_link& link) const NOEXCEPT
{
    if (const auto it = memo.find(link.value); it != memo.cend())
        return it->second;

    const auto unconfirmable = [&]() NOEXCEPT
    {
        memo.emplace(link.value, false);
        return false;
    };

    if (is_confirmed_tx(link))
        return unconfirmable();

    std::vector<tx_link::integer> pending{ link.value };
    std::unordered_set<tx_link::integer> visited{ link.value };
    while (!pending.empty())
    {
        const tx_link next{ pending.back() };
        pending.pop_back();

        if (const auto it = memo.find(next.value); it != memo.cend())
        {
            if (!it->second)
                return unconfirmable();

            continue;
        }

        for (const auto& point: to_points(next))
        {
            const auto prevout = get_point_key(point);
            if (!find_confirmed_spender(prevout).is_terminal())
                return unconfirmable();

            const auto parent = to_tx(prevout.hash());
            if (!parent.is_terminal() && !is_confirmed_tx(parent) &&
                visited.insert(parent.value).second)
                pending.push_back(parent.value);
        }
    }

    for (const auto tx: visited)
        memo.emplace(tx, true);

    return true;
}

TEMPLATE
bool CLASS::get_confirmable_totals(fee_totals& out, const tx_link& link,
    bool ancestors) const NOEXCEPT
{
    using namespace system;
    auto links = to_confirmable_relatives(link, ancestors);
    links.push_back(link.value);

    out = {};
    for (const auto fk: links)
    {
        fee_rate rate{};
        if (!get_tx_fees(rate, fk))
            return false;

        // A confirmed relative is not pooled (and has no sigops total).
        table::pool_total total{};
        const auto row = store_.pool.first(tx_link{ fk });
        if (!row.is_terminal() && !store_.pool.totals.get(row, total))
            return false;

        ++out.count;
        out.bytes = ceilinged_add(out.bytes, rate.bytes);
        out.sigops = ceilinged_add(out.sigops, total.sigops);
        out.fee = ceilinged_add(out.fee, rate.fee);
    }

    return true;
}

TEMPLATE
tx_links CLASS::to_confirmable_relatives(const tx_link& link,
    bool ancestors) const NOEXCEPT
{
    tx_links out{};
    std::vector<tx_link::integer> pending{ link.value };
    std::unordered_set<tx_link::integer> visited{ link.value };
    while (!pending.empty())
    {
        const tx_link next{ pending.back() };
        pending.pop_back();

        const auto links = ancestors ? to_confirmable_parents(next) :
            to_confirmable_children(next);

        for (const auto fk: links)
        {
            if (visited.insert(fk).second)
            {
                out.push_back(fk);
                pending.push_back(fk);
            }
        }
    }

    return out;
}

// writers
// ----------------------------------------------------------------------------

TEMPLATE
bool CLASS::set_pooled(const tx_link& link, const transaction& tx,
    const chain_context& ctx, uint32_t time) NOEXCEPT
{
    return set_pooled(link, tx, context::from(ctx), time);
}

TEMPLATE
bool CLASS::set_pooled(const tx_link& link, const transaction& tx,
    const context& ctx, uint32_t time) NOEXCEPT
{
    if (!store_.pool.enabled())
        return true;

    using namespace system;
    using algorithm = system::pool::algorithm;
    using link_t = algorithm::link_t;
    constexpr auto none = algorithm::none;
    const auto& ins = *tx.inputs_ptr();
    const auto inputs = ins.size();
    auto conflicted = false;
    tx_links prevouts(inputs);
    pool_links mates(inputs, none);
    pool_links parents(inputs, none);

    for (size_t at{}; at < inputs; ++at)
    {
        const auto& in = *ins.at(at);
        const auto& point = in.point();
        tx_link parent{ in.metadata.parent_tx };
        auto coinbase = in.metadata.coinbase;

        // Prevouts populated from the tx's own package are not yet linked.
        if (in.metadata.parent_tx == max_uint32)
        {
            if ((parent = to_tx(point.hash())).is_terminal())
                return false;

            coinbase = is_coinbase(parent);
        }

        prevouts.at(at) = table::prevout::merge(coinbase, parent);
        const auto row = store_.pool.first(parent);
        if (!row.is_terminal())
            parents.at(at) = possible_narrow_cast<link_t>(row.value);

        bool conflict{};
        auto& mate = mates.at(at);
        if (!get_pooled_spender(mate, conflict, point, link, true))
            return false;

        conflicted |= conflict;
    }

    const auto bip16 = ctx.is_enabled(chain::flags::bip16_rule);
    const auto bip141 = ctx.is_enabled(chain::flags::bip141_rule);
    const auto sigops = tx.signature_operations(bip16, bip141);
    using lane_t = schema::pool::witness_lane;
    const auto hash = tx.get_hash(true);
    const auto words = from_little_endians(array_cast<lane_t>(hash));

    // ========================================================================
    const auto scope = get_transactor();

    // Clean single allocation failure (e.g. disk full).
    table::spends::link first{};
    const table::spends::put_refs spent{ {}, prevouts };
    if (!store_.spends.put_link(first, spent))
        return false;

    auto& vtx = store_.pool;
    const auto fk = vtx.allocate(1);
    if (fk.is_terminal())
        return false;

    if (!store_.wtxid.put(fk, hash))
        return false;

    // Column puts are unguarded, the accessor guards their rows against remap.
    using word = table::pool_word;
    const table::pool_total total
    {
        {},
        tx.fee(),
        possible_narrow_cast<uint32_t>(tx.virtual_size()),
        possible_narrow_cast<uint32_t>(sigops)
    };
    const table::pool_edge edge
    {
        {},
        first,
        possible_narrow_cast<uint32_t>(inputs)
    };

    auto guard = vtx.get_memory();
    if (!guard ||
        !vtx.id0.put(fk, word{ {}, std::get<0>(words) }) ||
        !vtx.id1.put(fk, word{ {}, std::get<1>(words) }) ||
        !vtx.id2.put(fk, word{ {}, std::get<2>(words) }) ||
        !vtx.id3.put(fk, word{ {}, std::get<3>(words) }) ||
        !vtx.totals.put(fk, total) ||
        !vtx.edges.put(fk, edge))
        return false;

    guard.reset();

    // Commit is deferred until the columns are set.
    if (!vtx.put(fk, link, table::pool::record{ {}, ctx, time }))
        return false;

    const auto row = possible_narrow_cast<link_t>(fk.value);
    const auto start = possible_narrow_cast<link_t>(first.value);
    if (!set_pool_links(row, start, parents, mates) ||
        !set_pool_adoption(row, tx))
        return false;

    // A conflict with a confirmed spender remains absent.
    if (conflicted)
        return true;

    pool_guards guards{};
    algorithm::columns_t columns{};
    if (!get_pool_columns(columns, guards))
        return false;

    algorithm::admit(columns, row);
    return true;
    // ========================================================================
}

// The pool index is rebuilt from the compacted pool. Retained rows are
// confirmable, so all are present.
TEMPLATE
bool CLASS::set_pool_index() NOEXCEPT
{
    if (!store_.pool.enabled())
        return true;

    using namespace system;
    using algorithm = system::pool::algorithm;
    using link_t = algorithm::link_t;
    constexpr auto none = algorithm::none;

    if (!store_.pool_cells.truncate(0) ||
        !store_.pool_children.truncate(0) ||
        !store_.spends_rows.truncate(0) ||
        !store_.spends_siblings.truncate(0) ||
        !store_.spends_rivals.truncate(0))
        return false;

    const auto count = store_.pool.count().value;
    for (pool_link::integer row{}; row < count; ++row)
    {
        const tx_link tx{ store_.pool.get_key(row) };
        table::pool_edge edge{};
        if (!store_.pool.edges.get(row, edge))
            return false;

        const auto inputs = edge.inputs;
        table::spends::get_refs::parents prevouts(inputs);
        table::spends::get_refs spent{ {}, prevouts };
        if (!store_.spends.get(edge.spends_fk, spent))
            return false;

        pool_links mates(inputs, none);
        pool_links parents(inputs, none);
        for (uint32_t at{}; at < inputs; ++at)
        {
            using prevout = table::prevout::slab_get;
            const tx_link parent{ prevout::output_tx_fk(prevouts.at(at)) };
            const auto parent_row = store_.pool.first(parent);
            if (!parent_row.is_terminal())
                parents.at(at) = possible_narrow_cast<link_t>(parent_row.value);

            bool conflict{};
            auto& mate = mates.at(at);
            const auto point = get_point_key(to_point(tx, at));
            if (!get_pooled_spender(mate, conflict, point, tx, true))
                return false;
        }

        if (!set_pool_links(row, edge.spends_fk, parents, mates))
            return false;
    }

    pool_guards guards{};
    algorithm::columns_t columns{};
    if (!get_pool_columns(columns, guards))
        return false;

    pool_links scratch(columns.rows.size());
    algorithm::initialize(columns);
    while (algorithm::propagate(columns, scratch));
    return true;
}

// protected
// The row's cell and children head, and each input's parent row, sibling
// (joining the list of inputs spending its parent) and rival (joining the
// ring of inputs spending its point at a pooled spender of it).
TEMPLATE
bool CLASS::set_pool_links(system::pool::algorithm::link_t row,
    system::pool::algorithm::link_t start,
    const pool_links& parents, const pool_links& mates) NOEXCEPT
{
    using namespace system;
    using link_t = system::pool::algorithm::link_t;
    constexpr auto none = system::pool::algorithm::none;
    constexpr auto absent = system::pool::algorithm::absent;
    const auto put = [](auto& head, link_t index, link_t value) NOEXCEPT
    {
        using replace = typename std::decay_t<decltype(head)>::replace;
        return head.set(index, replace{ {}, value });
    };

    if (!put(store_.pool_cells, row, absent) ||
        !put(store_.pool_children, row, none))
        return false;

    const auto inputs = parents.size();
    for (size_t at{}; at < inputs; ++at)
    {
        const auto input = start + possible_narrow_cast<link_t>(at);
        const auto parent = parents.at(at);
        const auto mate = mates.at(at);
        auto sibling = none;
        auto rival = none;

        if (parent != none)
        {
            sibling = store_.pool_children.at(parent);
            if (!put(store_.pool_children, parent, input))
                return false;
        }

        if (mate != none)
        {
            const auto next = store_.spends_rivals.at(mate);
            rival = (next == none) ? mate : next;
            if (!put(store_.spends_rivals, mate, input))
                return false;
        }

        if (!put(store_.spends_rows, input, parent) ||
            !put(store_.spends_siblings, input, sibling) ||
            !put(store_.spends_rivals, input, rival))
            return false;
    }

    return true;
}

// protected
// Pooled spenders of the tx (the children of a reorganized parent) take it
// as their parent row and join its children list.
TEMPLATE
bool CLASS::set_pool_adoption(system::pool::algorithm::link_t row,
    const transaction& tx) NOEXCEPT
{
    using namespace system;
    using link_t = system::pool::algorithm::link_t;
    constexpr auto none = system::pool::algorithm::none;
    const auto put = [](auto& head, link_t index, link_t value) NOEXCEPT
    {
        using replace = typename std::decay_t<decltype(head)>::replace;
        return head.set(index, replace{ {}, value });
    };

    const auto outputs = tx.outputs_ptr()->size();
    const auto txid = tx.get_hash(false);
    for (uint32_t index{}; index < outputs; ++index)
    {
        for (const auto& in: to_spenders(point{ txid, index }))
        {
            const auto child = to_input_tx(in);
            const auto child_row = store_.pool.first(child);
            if (child_row.is_terminal())
                continue;

            table::pool_edge edge{};
            if (!store_.pool.edges.get(child_row, edge))
                return false;

            const auto position = in - to_point(child, zero).value;
            const auto offset = possible_narrow_cast<link_t>(position);
            const auto input = edge.spends_fk + offset;

            if (store_.spends_rows.at(input) != none)
                continue;

            const auto sibling = store_.pool_children.at(row);
            if (!put(store_.spends_rows, input, row) ||
                !put(store_.spends_siblings, input, sibling) ||
                !put(store_.pool_children, row, input))
                return false;
        }
    }

    return true;
}

// index
// ----------------------------------------------------------------------------

// protected
// The index heads hold no count cell, so each is a plain array of cells.
TEMPLATE
bool CLASS::get_pool_columns(system::pool::algorithm::columns_t& out,
    pool_guards& guards) const NOEXCEPT
{
    using namespace system;
    using algorithm = system::pool::algorithm;
    using link_t = algorithm::link_t;
    using edges_t = algorithm::edges_t;
    using totals_t = algorithm::totals_t;
    static_assert(std::endian::native == std::endian::little);

    const auto rows = std::min
    ({
        possible_narrow_cast<size_t>(store_.pool.count().value),
        possible_narrow_cast<size_t>(store_.pool_cells.count().value),
        possible_narrow_cast<size_t>(store_.pool_children.count().value)
    });

    const auto inputs = std::min
    ({
        possible_narrow_cast<size_t>(store_.spends.count().value),
        possible_narrow_cast<size_t>(store_.spends_rows.count().value),
        possible_narrow_cast<size_t>(store_.spends_siblings.count().value),
        possible_narrow_cast<size_t>(store_.spends_rivals.count().value)
    });

    if (!guards.pool && is_nonzero(rows))
        guards.pool = store_.pool.get_memory();

    if (!guards.cells && is_nonzero(rows))
        guards.cells = store_.pool_cells.get_memory();

    if (!guards.children && is_nonzero(rows))
        guards.children = store_.pool_children.get_memory();

    if (!guards.rows && is_nonzero(inputs))
        guards.rows = store_.spends_rows.get_memory();

    if (!guards.siblings && is_nonzero(inputs))
        guards.siblings = store_.spends_siblings.get_memory();

    if (!guards.rivals && is_nonzero(inputs))
        guards.rivals = store_.spends_rivals.get_memory();

    if (is_nonzero(rows) &&
        (!guards.pool || !guards.cells || !guards.children))
        return false;

    if (is_nonzero(inputs) &&
        (!guards.rows || !guards.siblings || !guards.rivals))
        return false;

    // A null accessor (empty table) has no data.
    const auto data = [](const memory& guard) NOEXCEPT
    {
        link_t* cells{};
        if (guard)
            cells = pointer_cast<link_t>(guard.data());

        return cells;
    };

    const auto raw_edges = store_.pool.edges.get_raw();
    const auto raw_totals = store_.pool.totals.get_raw();
    const auto edges = pointer_cast<const edges_t>(raw_edges);
    const auto totals = pointer_cast<const totals_t>(raw_totals);
    out =
    {
        { data(guards.cells), rows },
        { data(guards.children), rows },
        { edges, rows },
        { totals, rows },
        { data(guards.rows), inputs },
        { data(guards.siblings), inputs },
        { data(guards.rivals), inputs }
    };

    return true;
}

// protected
// A point archived with one spender is not in the duplicate table.
TEMPLATE
bool CLASS::get_pooled_spender(system::pool::algorithm::link_t& out,
    bool& conflicted, const point& point, const tx_link& self,
    bool archived) const NOEXCEPT
{
    using namespace system;
    using link_t = system::pool::algorithm::link_t;
    constexpr auto none = system::pool::algorithm::none;
    out = none;
    conflicted = false;

    if (archived && !store_.duplicate.exists(point))
        return true;

    for (const auto& in: to_spenders(point))
    {
        const auto tx = to_input_tx(in);
        if (tx == self)
            continue;

        if (is_confirmed_tx(tx))
        {
            conflicted = true;
            continue;
        }

        if (out != none)
            continue;

        const auto row = store_.pool.first(tx);
        if (row.is_terminal())
            continue;

        table::pool_edge edge{};
        if (!store_.pool.edges.get(row, edge))
            return false;

        const auto position = in - to_point(tx, zero).value;
        out = edge.spends_fk + possible_narrow_cast<link_t>(position);
    }

    return true;
}

// protected
// The conflicts of the block's txs that are not pooled are in its prevout
// cache, which is absent when the block was confirmed by bypass.
TEMPLATE
bool CLASS::set_pool_block(const header_link& link, const tx_links& txs,
    bool confirmed) NOEXCEPT
{
    using namespace system;
    using algorithm = system::pool::algorithm;
    using link_t = algorithm::link_t;
    if (!store_.pool.enabled())
        return true;

    pool_guards guards{};
    algorithm::columns_t columns{};
    if (!get_pool_columns(columns, guards))
        return false;

    const auto rows = columns.cells.size();
    if (is_zero(rows))
        return true;

    for (const auto fk: txs)
    {
        const auto row = store_.pool.first(tx_link{ fk });
        if (row.is_terminal() || row.value >= rows)
            continue;

        const auto at = possible_narrow_cast<link_t>(row.value);
        if (confirmed)
            algorithm::confirm(columns, at);
        else
            algorithm::unconfirm(columns, at);
    }

    table::prevout::slab_get cache{};
    if (!store_.prevout.at(to_prevout(link), cache))
        return true;

    for (const auto conflict: cache.conflicts)
    {
        const auto row = store_.pool.first(tx_link{ conflict });
        if (row.is_terminal() || row.value >= rows)
            continue;

        const auto at = possible_narrow_cast<link_t>(row.value);
        if (confirmed)
            algorithm::exclude(columns, at);
        else
            algorithm::revive(columns, at);
    }

    return true;
}

// coverage
// ----------------------------------------------------------------------------

TEMPLATE
code CLASS::get_pool_coverage(bool& out,
    const system::chain::transaction_cptrs& txs,
    const std::vector<bool>& stored, const context& ctx, uint64_t rate,
    uint32_t bytes_per_sigop, const pool_totals& limit,
    size_t maximum_outcomes) const NOEXCEPT
{
    out = false;
    if (!store_.pool.enabled())
    {
        out = true;
        return error::success;
    }

    pool_graph graph{};
    if (const auto ec = get_pool_graph(graph, txs, stored, ctx))
        return ec;

    return get_pool_coverage_(out, graph, rate, bytes_per_sigop, limit,
        maximum_outcomes);
}

// protected
// Candidates are rows and inputs numbered from the table counts. A parent in
// the package is found by hash (parents precede children), a pooled parent by
// link, and any other parent is none. An input joins the ring of a pooled
// spender of its point by a bridge from that input, or a ring of candidate
// spenders if there is none.
TEMPLATE
code CLASS::get_pool_graph(pool_graph& out,
    const system::chain::transaction_cptrs& txs,
    const std::vector<bool>& stored, const context& ctx) const NOEXCEPT
{
    using namespace system;
    using algorithm = system::pool::algorithm;
    using link_t = algorithm::link_t;
    constexpr auto none = algorithm::none;

    algorithm::columns_t table{};
    if (!get_pool_columns(table, out.guards))
        return error::integrity;

    const auto rows = possible_narrow_cast<link_t>(table.cells.size());
    const auto inputs = possible_narrow_cast<link_t>(table.rows.size());
    const auto bip16 = ctx.is_enabled(chain::flags::bip16_rule);
    const auto bip141 = ctx.is_enabled(chain::flags::bip141_rule);
    std::unordered_map<hash_digest, link_t> positions{};
    std::unordered_map<point, link_t> points{};
    auto input = inputs;
    auto row = rows;
    for (size_t index{}; index < txs.size(); ++index)
    {
        if (stored.at(index))
            continue;

        const auto& tx = *txs.at(index);
        const auto& ins = *tx.inputs_ptr();
        const auto count = possible_narrow_cast<link_t>(ins.size());
        const auto sigops = tx.signature_operations(bip16, bip141);
        positions.emplace(tx.get_hash(false), row);
        out.candidates.push_back(row);
        out.cells.push_back(algorithm::absent);
        out.children.push_back(none);
        out.edges.push_back({ input, count });
        out.totals.push_back(
        {
            tx.fee(),
            possible_narrow_cast<uint32_t>(tx.virtual_size()),
            possible_narrow_cast<uint32_t>(sigops)
        });

        for (const auto& in: ins)
        {
            const auto& point = in->point();
            auto parent = none;
            if (in->metadata.parent_tx == max_uint32)
            {
                const auto it = positions.find(point.hash());
                if (it != positions.end())
                    parent = it->second;
            }
            else
            {
                const tx_link parent_tx{ in->metadata.parent_tx };
                const auto fk = store_.pool.first(parent_tx);
                if (!fk.is_terminal())
                    parent = possible_narrow_cast<link_t>(fk.value);
            }

            out.rows.push_back(parent);
            out.siblings.push_back(none);
            out.rivals.push_back(none);

            if (parent != none && parent < rows)
            {
                out.child_bridges.push_back({ parent, input });
            }
            else if (parent != none)
            {
                auto& head = out.children.at(parent - rows);
                out.siblings.back() = head;
                head = input;
            }

            link_t mate{};
            bool conflict{};
            if (!get_pooled_spender(mate, conflict, point, {}, false))
                return error::integrity;

            out.conflicted |= conflict;
            if (mate != none)
            {
                out.rivals.back() = mate;
                out.rival_bridges.push_back({ mate, input });
            }
            else if (const auto it = points.find(point); it == points.end())
            {
                points.emplace(point, input);
            }
            else
            {
                auto& ring = out.rivals.at(it->second - inputs);
                out.rivals.back() = (ring == none) ? it->second : ring;
                ring = input;
            }

            ++input;
        }

        ++row;
    }

    out.graph =
    {
        table,
        {
            out.cells, out.children, out.edges, out.totals,
            out.rows, out.siblings, out.rivals
        },
        out.child_bridges,
        out.rival_bridges
    };

    return error::success;
}

// protected
TEMPLATE
code CLASS::get_pool_coverage_(bool& out, const pool_graph& graph,
    uint64_t rate, uint32_t bytes_per_sigop, const pool_totals& limit,
    size_t maximum_outcomes) const NOEXCEPT
{
    using namespace system;
    using algorithm = system::pool::algorithm;
    using link_t = algorithm::link_t;
    using mask_t = algorithm::mask_t;
    constexpr auto none = algorithm::none;
    const auto& g = graph.graph;
    const auto rows = possible_narrow_cast<link_t>(g.table.cells.size());
    const auto inputs = possible_narrow_cast<link_t>(g.table.rows.size());
    out = false;

    // A point of the package spent by a confirmed tx cannot be confirmed.
    if (graph.conflicted)
        return error::success;

    // A candidate with an absent parent cannot be confirmed.
    for (const auto candidate: graph.candidates)
    {
        const auto& slice = graph.edges.at(candidate - rows);
        for (auto at = slice.first; at < slice.first + slice.count; ++at)
        {
            const auto parent = graph.rows.at(at - inputs);
            if (parent != none && parent < rows &&
                g.table.cells[parent] == algorithm::absent)
                return error::success;
        }
    }

    algorithm::initialize(g, graph.candidates);
    uint64_t added{};
    uint64_t bytes{};
    std::vector<algorithm::component_t> components{};
    for (const auto candidate: graph.candidates)
    {
        const std::span<const link_t> self{ &candidate, one };
        const auto& totals = graph.totals.at(candidate - rows);
        const auto size = algorithm::chargeable(totals, bytes_per_sigop);
        const auto seen = [&](const auto& component) NOEXCEPT
        {
            return is_nonzero(algorithm::members(component, self));
        };

        bytes = ceilinged_add(bytes, size);
        if (std::ranges::any_of(components, seen))
            continue;

        algorithm::component_t component{};
        if (!algorithm::collect(component, g, candidate) ||
            !algorithm::relate(component, g))
            return error::pool_limit;

        pool_totals sum{};
        if (is_nonzero(component.contests) &&
            !algorithm::ancestry(sum, g, component, limit))
            return error::pool_limit;

        const auto outcomes = algorithm::outcomes(component);
        if (outcomes > maximum_outcomes)
            return error::pool_limit;

        std::vector<mask_t> masks(outcomes);
        for (size_t index{}; index < outcomes; ++index)
            masks[index] = algorithm::outcome(component, index);

        uint64_t best{};
        uint64_t best_without{};
        const auto members = algorithm::members(component, graph.candidates);
        algorithm::evaluate(best, best_without, masks, members, component, g);

        added = ceilinged_add(added, floored_subtract(best, best_without));
        components.push_back(component);
    }

    out = algorithm::covered(added, zero, bytes, rate);
    return error::success;
}

// template
// ----------------------------------------------------------------------------

// A tx pooled more than once is taken at its first row, which precedes the
// children of each of its rows.
TEMPLATE
code CLASS::get_pool_template(tx_links& out, const pool_totals& budget,
    size_t depth, uint32_t bytes_per_sigop,
    size_t maximum_outcomes) const NOEXCEPT
{
    using algorithm = system::pool::algorithm;
    out.clear();
    if (!store_.pool.enabled())
        return error::success;

    pool_guards guards{};
    algorithm::columns_t columns{};
    if (!get_pool_columns(columns, guards))
        return error::integrity;

    std::vector<algorithm::mark> marks(columns.cells.size());
    pool_links rows{};
    algorithm::resolve(marks, columns, maximum_outcomes);
    algorithm::pack(rows, marks, columns, budget, depth, bytes_per_sigop);

    std::unordered_set<tx_link::integer> taken{};
    out.reserve(rows.size());
    for (const auto row: rows)
    {
        const tx_link tx{ store_.pool.get_key(pool_link{ row }) };
        if (tx.is_terminal())
            return error::integrity;

        if (taken.insert(tx.value).second)
            out.push_back(tx);
    }

    return error::success;
}

} // namespace database
} // namespace libbitcoin

#endif
