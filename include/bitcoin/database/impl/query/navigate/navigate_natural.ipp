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
#ifndef LIBBITCOIN_DATABASE_QUERY_NAVIGATE_NATURAL_IPP
#define LIBBITCOIN_DATABASE_QUERY_NAVIGATE_NATURAL_IPP

#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

// Use natural key (entry).
// ----------------------------------------------------------------------------

TEMPLATE
inline header_link CLASS::to_candidate(size_t height) const NOEXCEPT
{
    return store_.candidate.at(height);
}

TEMPLATE
inline header_link CLASS::to_confirmed(size_t height) const NOEXCEPT
{
    return store_.confirmed.at(height);
}

TEMPLATE
inline header_link CLASS::to_top_candidate() const NOEXCEPT
{
    return store_.candidate.top();
}

TEMPLATE
inline header_link CLASS::to_top_confirmed() const NOEXCEPT
{
    return store_.confirmed.top();
}

TEMPLATE
inline header_link CLASS::to_header(const hash_digest& key) const NOEXCEPT
{
    return store_.header.first(key);
}

TEMPLATE
inline tx_link CLASS::to_tx(const hash_digest& key) const NOEXCEPT
{
    return store_.tx.first(key);
}

// A pooled tx by witness hash, otherwise an unsegregated tx (txid == wtxid).
// Candidates are verified against the pool following iterator disposal.
TEMPLATE
inline tx_link CLASS::to_witness_tx(const hash_digest& key) const NOEXCEPT
{
    std::vector<table::pool::link> candidates{};
    for (auto it = store_.wtxid.it({ key }); it; ++it)
        candidates.push_back(*it);

    for (const auto& fk: candidates)
    {
        const tx_link link{ store_.pool.get_key(fk) };
        if (store_.pool.first(link) == fk && get_pooled_wtxid(fk) == key)
            return link;
    }

    for (const auto& link: to_duplicates(key))
        if (!is_tx_segregated(link))
            return link;

    return {};
}

TEMPLATE
inline filter_link CLASS::to_filter(const header_link& key) const NOEXCEPT
{
    return store_.filter_tx.first(key);
}

TEMPLATE
outs_link CLASS::to_output(const point& prevout) const NOEXCEPT
{
    return to_output(prevout.hash(), prevout.index());
}

TEMPLATE
outs_link CLASS::to_output(const hash_digest& key,
    uint32_t input_index) const NOEXCEPT
{
    return to_output(to_tx(key), input_index);
}

// Obtain natural key (exit).
// ----------------------------------------------------------------------------

TEMPLATE
inline hash_digest CLASS::get_header_key(const header_link& link) const NOEXCEPT
{
    return store_.header.get_key(link);
}

TEMPLATE
inline hash_digest CLASS::get_tx_key(const tx_link& link) const NOEXCEPT
{
    return store_.tx.get_key(link);
}

TEMPLATE
inline ins_key CLASS::get_point_key(const ins_link& link) const NOEXCEPT
{
    outs_link spent{};
    ins_key key{};
    if (!get_spent_or_point(spent, key, link))
        return {};

    return spent.is_terminal() ? key : get_outpoint_key(spent);
}

// private
TEMPLATE
bool CLASS::get_spent_or_point(outs_link& spent, point& key,
    const ins_link& link) const NOEXCEPT
{
    return get_spent_or_point(spent, key, link, {});
}

// private
// An input resolves to its bound output, or to its point while unbound (a
// coinbase input is bound to no output and has the null point). A points miss
// implies a drain since the bindings read, so the binding is reread (a drained
// input is bound or unbound). Bindings are read through the caller's accessor
// if given.
TEMPLATE
bool CLASS::get_spent_or_point(outs_link& spent, point& key,
    const ins_link& link, const memory& spends) const NOEXCEPT
{
    outs_link bound{};
    const auto read = [&]() NOEXCEPT
    {
        bound = spends ? store_.spend.at(spends, link.value) :
            store_.spend.at(link.value);
        return !bound.is_terminal();
    };

    // A banked input's point is positioned at its ins less the bank base.
    table::points::get_composed row{};
    const auto banked = [&](const table::points& bank) NOEXCEPT
    {
        const auto base = bank.base().value;
        return link.value >= base &&
            bank.get(table::points::link{ link.value - base }, row);
    };

    if (!read())
    {
        if (banked(store_.points0) || banked(store_.points1))
        {
            spent = outs_link{};
            key = std::move(row.key);
            return true;
        }

        if (!read())
        {
            if (!get_unbound_point(key, link))
                return false;

            spent = outs_link{};
            return true;
        }
    }

    spent = bound;
    key = {};
    return true;
}

// private
TEMPLATE
bool CLASS::get_unbound_point(point& key, const ins_link& link) const NOEXCEPT
{
    table::unbound::record record{};
    const auto fk = store_.unbound.first(table::unbound::to_key(link));
    if (fk.is_terminal() || !store_.unbound.get(fk, record))
        return false;

    key = std::move(record.key);
    return true;
}

TEMPLATE
inline hash_digest CLASS::get_outs_key(const outs_link& link) const NOEXCEPT
{
    table::outs::get_output put{};
    if (!store_.outs.get(link, put))
        return {};

    table::output::get_script_hash output{};
    if (!store_.output.raw(store_.output.get_memory(), put.out_fk, output))
        return {};

    return output.hash;
}

TEMPLATE
typename CLASS::point CLASS::get_outpoint_key(
    const outs_link& link) const NOEXCEPT
{
    table::outs::get_parent out{};
    if (!store_.outs.get(link, out))
        return {};

    table::transaction::only_with_sk parent{};
    if (!store_.tx.get(out.parent_fk, parent))
        return {};

    const auto offset = link.value - parent.outs_fk;
    const auto index = system::possible_narrow_cast<uint32_t>(offset);
    return { std::move(parent.key), index };
}

TEMPLATE
inline hash_digest CLASS::get_wtxid_key(const pool_link& link) const NOEXCEPT
{
    return get_pooled_wtxid(link);
}

TEMPLATE
inline hash_digest CLASS::get_point_hash(const ins_link& link) const NOEXCEPT
{
    return get_point_key(link).hash();
}

} // namespace database
} // namespace libbitcoin

#endif
