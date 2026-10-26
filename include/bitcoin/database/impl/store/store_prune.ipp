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
#ifndef LIBBITCOIN_DATABASE_STORE_PRUNE_IPP
#define LIBBITCOIN_DATABASE_STORE_PRUNE_IPP

#include <chrono>
#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

// public
TEMPLATE
code CLASS::prune(const event_handler& handler) NOEXCEPT
{
    // Transactor lock generally only covers writes, but in this case pool
    // reads must also be guarded since the body shrinks and head is cleared.
    while (!transactor_mutex_.try_lock_for(std::chrono::seconds(1)))
    {
        handler(event_t::wait_lock, table_t::store);
    }

    code ec{ error::success };

    // Pool resettable if all candidates confirmed (fork is candidate top).
    if (!query<CLASS>{ *this }.is_coalesced())
    {
        ec = error::not_coalesced;
    }
    else
    {
        ec = compact(handler, [&]() NOEXCEPT -> code
        {
            handler(event_t::prune_table, table_t::pool_head);
            handler(event_t::prune_table, table_t::wtxid_head);

            // nullify table heads, set reference body counts to zero.
            // If snapshot from this state fails previous snapshot remains valid.
            // Batch tables drop at start and under lock after verify (not here).
            if (!pool.clear() || !wtxid.clear())
                return error::prune_table;

            // Snapshot with nullified head and zero body count.
            // The 'prune' parameter signals to not reset body count.
            // Success deletes the pre-prune snapshot (trim strands nothing).
            return snapshot(handler, true);
        });

        if (!ec)
        {
            // Reclaim disk space to logical extent.
            handler(event_t::unload_file, table_t::pool_body);
            if (!ec) ec = pool_body_.shrink();
            handler(event_t::load_file, table_t::pool_body);

            handler(event_t::unload_file, table_t::wtxid_body);
            if (!ec) ec = wtxid_body_.shrink();
            handler(event_t::load_file, table_t::wtxid_body);

            handler(event_t::unload_file, table_t::ecdsa0_body);
            if (!ec) ec = ecdsa0_body_.shrink();
            handler(event_t::load_file, table_t::ecdsa0_body);
            handler(event_t::unload_file, table_t::ecdsa1_body);
            if (!ec) ec = ecdsa1_body_.shrink();
            handler(event_t::load_file, table_t::ecdsa1_body);

            handler(event_t::unload_file, table_t::schnorr0_body);
            if (!ec) ec = schnorr0_body_.shrink();
            handler(event_t::load_file, table_t::schnorr0_body);
            handler(event_t::unload_file, table_t::schnorr1_body);
            if (!ec) ec = schnorr1_body_.shrink();
            handler(event_t::load_file, table_t::schnorr1_body);

            handler(event_t::unload_file, table_t::silent0_body);
            if (!ec) ec = silent0_body_.shrink();
            handler(event_t::load_file, table_t::silent0_body);
            handler(event_t::unload_file, table_t::silent1_body);
            if (!ec) ec = silent1_body_.shrink();
            handler(event_t::load_file, table_t::silent1_body);

            handler(event_t::unload_file, table_t::silent0_prefix_body);
            if (!ec) ec = silent0_prefix_body_.shrink();
            handler(event_t::load_file, table_t::silent0_prefix_body);
            handler(event_t::unload_file, table_t::silent1_prefix_body);
            if (!ec) ec = silent1_prefix_body_.shrink();
            handler(event_t::load_file, table_t::silent1_prefix_body);
        }
    }

    transactor_mutex_.unlock();
    return ec;
}

// protected
// The kept records are copied out while the pool head is intact (ancestors are
// found by key), and back after the reset, which snapshots pool and wtxid
// empty, so a fault before the next snapshot restores to the empty pool.
// Bodies are staged (settled records are read-only), so none is rewritten.
TEMPLATE
template <typename Reset>
code CLASS::compact(const event_handler& handler, const Reset& reset) NOEXCEPT
{
    const auto empty = [&]() NOEXCEPT
    {
        handler(event_t::prune_table, table_t::pool_body);
        handler(event_t::prune_table, table_t::wtxid_body);
        return pool_body_.truncate(0) && wtxid_body_.truncate(0);
    };

    const auto restart = [&]() NOEXCEPT -> code
    {
        if (const auto ec = reset())
            return ec;

        return empty() ? error::success : error::prune_table;
    };

    // A residual /compact (a fault during compaction) is cleared.
    const auto folder = configuration_.path / schema::dir::compact;
    if (const auto ec = file::clear_directory_ex(folder))
        return ec;

    if (!pool.enabled() || is_zero(pool.count().value))
    {
        if (const auto ec = file::remove_ex(folder))
            return ec;

        return restart();
    }

    Storage<one> pool_head{ head(folder, schema::caches::pool),
        head_settings(configuration_.pool), random };
    table::pool_storage<Storage> pool_body{ body(folder, schema::caches::pool),
        configuration_.pool, sequential, staged };

    using namespace system;
    using bucket = table::pool::link::integer;
    table::pool temp_pool{ pool_head, pool_body,
        possible_narrow_cast<bucket>(pool.buckets()),
        configuration_.pool.expected };

    code ec{ error::success };
    const auto create = [&](auto& storage, table_t table) NOEXCEPT
    {
        if (!ec)
        {
            handler(event_t::create_file, table);
            ec = storage.create();
        }
    };

    const auto open = [&](auto& storage, table_t table) NOEXCEPT
    {
        if (!ec)
        {
            handler(event_t::open_file, table);
            ec = storage.open();
        }
    };

    const auto load = [&](auto& storage, table_t table) NOEXCEPT
    {
        if (!ec)
        {
            handler(event_t::load_file, table);
            ec = storage.load();
        }
    };

    create(pool_head, table_t::pool_head);
    create(pool_body, table_t::pool_body);
    open(pool_head, table_t::pool_head);
    open(pool_body, table_t::pool_body);
    load(pool_head, table_t::pool_head);
    load(pool_body, table_t::pool_body);

    if (!ec)
    {
        handler(event_t::create_table, table_t::pool_table);
        if (!temp_pool.set_filter_k(pool.filter_k()) || !temp_pool.create())
            ec = error::create_table;
    }

    const auto retained = [&](bool& out, const tx_link& link) NOEXCEPT
    {
        return is_retained(out, link, temp_pool);
    };

    const auto unindexed = [](const table::pool::link&,
        const hash_digest&) NOEXCEPT
    {
        return true;
    };

    const auto all = [](bool& out, const tx_link&) NOEXCEPT
    {
        out = true;
        return true;
    };

    const auto indexed = [&](const table::pool::link& link,
        const hash_digest& hash) NOEXCEPT
    {
        return wtxid.put(link, hash);
    };

    // A failure before the reset leaves the live tables unchanged.
    if (!ec && !copy_pool(temp_pool, pool, retained, unindexed))
        ec = error::prune_table;

    if (!ec)
        ec = reset();

    if (!ec && (!empty() || !copy_pool(pool, temp_pool, all, indexed)))
        ec = error::prune_table;

    const auto close = [&](auto& storage, table_t table) NOEXCEPT
    {
        handler(event_t::unload_file, table);
        if (const auto error = storage.unload(); !ec) ec = error;
        handler(event_t::close_file, table);
        if (const auto error = storage.close(); !ec) ec = error;
    };

    close(pool_head, table_t::pool_head);
    close(pool_body, table_t::pool_body);
    if (const auto error = file::clear_directory_ex(folder); !ec) ec = error;
    if (const auto error = file::remove_ex(folder); !ec) ec = error;
    return ec;
}

// protected
// A tx is retained if it is confirmable and every unconfirmed ancestor is
// pooled. Ancestors are found by key, so link order is not assumed, and an
// ancestor already retained ends its walk. Parents are reached through the
// spends of the tx's inputs (pooled txs are spent at validation).
TEMPLATE
bool CLASS::is_retained(bool& out, const tx_link& link,
    table::pool& retained) NOEXCEPT
{
    const query<CLASS> reader{ *this };
    std::vector<tx_link::integer> pending{ link };
    std::unordered_set<tx_link::integer> visited{ link };

    out = false;
    if (!reader.is_confirmable_tx(link))
        return true;

    while (!pending.empty())
    {
        const tx_link next{ pending.back() };
        pending.pop_back();

        const auto fk = pool.first(next);
        if (fk.is_terminal())
            return true;

        table::transaction::only tx_record{};
        if (!tx.get(next, tx_record))
            return false;

        std::vector<table::spend::outs::integer> spent(tx_record.ins_count);
        if (!spend.get_run(spent, tx_record.point_fk))
            return false;

        for (const auto outs_fk: spent)
        {
            table::outs::get_parent value{};
            if (!outs.get(outs_fk, value))
                return false;

            const tx_link parent{ value.parent_fk };
            if (reader.is_confirmed_tx(parent) ||
                !retained.first(parent).is_terminal())
                continue;

            if (visited.insert(parent).second)
                pending.push_back(parent);
        }
    }

    out = true;
    return true;
}

// protected
TEMPLATE
template <typename Keep, typename Index>
bool CLASS::copy_pool(table::pool& to, table::pool& from, const Keep& keep,
    const Index& index) NOEXCEPT
{
    using namespace system;
    using word = table::pool_word;
    using lane_t = schema::pool::witness_lane;
    using lanes_t = std_array<lane_t, schema::pool::witness_lanes>;
    const auto end = from.count().value;
    for (pool_link::integer link{}; link < end; ++link)
    {
        const tx_link tx_fk{ from.get_key(link) };
        if (from.first(tx_fk) != link)
            continue;

        word id0{}, id1{}, id2{}, id3{};
        table::pool::record record{};
        if (!from.get(link, record) ||
            !from.id0.get(link, id0) || !from.id1.get(link, id1) ||
            !from.id2.get(link, id2) || !from.id3.get(link, id3))
            return false;

        bool kept{};
        if (!keep(kept, tx_fk))
            return false;

        if (!kept)
            continue;

        const auto target = to.allocate(1);
        if (target.is_terminal())
            return false;

        const auto words = to_little_endians(lanes_t
        {
            id0.word, id1.word, id2.word, id3.word
        });

        if (!index(target, array_cast<uint8_t>(words)))
            return false;

        // Column puts are unguarded, the accessor guards their rows.
        auto guard = to.get_memory();
        if (!guard ||
            !to.id0.put(target, id0) || !to.id1.put(target, id1) ||
            !to.id2.put(target, id2) || !to.id3.put(target, id3))
            return false;

        guard.reset();

        // Commit is deferred until the columns are set.
        if (!to.put(target, tx_fk, record))
            return false;
    }

    return true;
}

// protected
// An input of the bank without a binding is unbound.
TEMPLATE
bool CLASS::save_unbound(const table::points& bank, size_t end) NOEXCEPT
{
    using row_t = table::points::link::integer;
    std::vector<std::pair<table::ins::link::integer, row_t>> rows{};
    {
        const auto ptr = spend.get_memory();
        for (auto fk = bank.base().value; fk < end; ++fk)
            if (spend.at(ptr, fk).is_terminal())
                rows.emplace_back(fk, table::points::link::terminal);
    }

    {
        const auto head = bank.get_head_memory();
        for (auto& [fk, row]: rows)
            row = bank.at(head, fk).value;
    }

    std::vector<table::points::record> points(rows.size());
    {
        const auto body = bank.get_memory();
        for (size_t index{}; index < rows.size(); ++index)
        {
            const table::points::link row{ rows.at(index).second };
            if (!row.is_terminal() && !bank.get(body, row, points.at(index)))
                return false;
        }
    }

    for (size_t index{}; index < rows.size(); ++index)
    {
        if (table::points::link{ rows.at(index).second }.is_terminal())
            continue;

        const auto& point = points.at(index);
        const auto key = table::unbound::to_key(rows.at(index).first);
        if (unbound.first(key).is_terminal() && !unbound.put(key,
            table::unbound::record{ {}, { point.hash, point.index } }))
            return false;
    }

    return true;
}

// protected
// The keys of a bank end at the base of a later bank, or the ins count.
TEMPLATE
code CLASS::drain_points(table::points& bank,
    const table::points& other) NOEXCEPT
{
    const auto count = ins.count().value;
    const auto later = other.base().value;
    const auto end = later > bank.base().value ? later : count;
    if (!save_unbound(bank, end))
        return error::unbound_put;

    return bank.drop(count) ? error::success : error::prune_table;
}

// Block archives are not in flight at a flip or drop (the download chaser
// flips at its window gate) and pooled txs do not write points.
TEMPLATE
code CLASS::flip_points(const event_handler& handler) NOEXCEPT
{
    // ========================================================================
    const transactor scope{ transactor_mutex_ };

    const auto bank = !get_points_bank();
    handler(event_t::prune_table, bank ? table_t::points1_table :
        table_t::points0_table);

    if (const auto ec = drain_points(points_bank(bank), points_bank(!bank)))
        return ec;

    points_bank_.store(bank, std::memory_order_relaxed);
    return error::success;
    // ========================================================================
}

TEMPLATE
code CLASS::drop_points(const event_handler& handler) NOEXCEPT
{
    // ========================================================================
    const transactor scope{ transactor_mutex_ };

    handler(event_t::prune_table, table_t::points0_table);
    if (const auto ec = drain_points(points0, points1))
        return ec;

    handler(event_t::prune_table, table_t::points1_table);
    return drain_points(points1, points0);
    // ========================================================================
}

} // namespace database
} // namespace libbitcoin

#endif
