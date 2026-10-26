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
// resolved, by writing its spends row and the spender of the output. Binding
// is content, so rebinding is idempotent and nothing here is written or
// cleared by confirmation or reorganization (see consensus_block.ipp).
// ----------------------------------------------------------------------------

// protected
TEMPLATE
code CLASS::get_binds(tx_binds& out, const header_link& link) const NOEXCEPT
{
    const auto links = to_transactions(link);
    if (links.empty())
        return error::integrity;

    out.clear();
    out.reserve(sub1(links.size()));
    for (auto index = one; index < links.size(); ++index)
    {
        const tx_link tx{ links.at(index) };
        binds values{};
        if (const auto ec = get_binds(values, tx))
            return ec;

        out.emplace_back(tx.value, std::move(values));
    }

    return error::success;
}

// protected
TEMPLATE
code CLASS::get_binds(binds& out, const tx_link& link) const NOEXCEPT
{
    table::transaction::get_puts record{};
    if (!store_.tx.get(link, record))
        return error::integrity;

    out.clear();
    out.reserve(record.ins_count);
    const auto end = record.points_fk + record.ins_count;
    for (auto fk = record.points_fk; fk < end; ++fk)
    {
        outs_link spent{};
        point key{};
        if (!get_spent_or_point(spent, key, fk))
            return error::integrity;

        out.emplace_back(fk, (spent.is_terminal() ? to_output(key) : spent).value);
    }

    return error::success;
}

// writers
// ----------------------------------------------------------------------------

// protected
TEMPLATE
code CLASS::bind(const tx_link& link, const binds& values) NOEXCEPT
{
    auto found = true;
    for (const auto& [ins, outs]: values)
    {
        if (outs == outs_link::terminal)
        {
            found = false;
            continue;
        }

        if (!spend(ins, outs, link))
            return error::integrity;
    }

    if (!found)
        return system::error::missing_previous_output;

    return error::success;
}

// protected
TEMPLATE
bool CLASS::spend(const ins_link& link, const outs_link& spent,
    const tx_link& spender) NOEXCEPT
{
    if (!store_.spend.set(link.value, table::spend::bind{ {}, spent }))
        return false;

    return mark(spender, spent);
}

// protected
TEMPLATE
bool CLASS::mark(const tx_link& spender, const outs_link& spent) NOEXCEPT
{
    if (spender.is_terminal())
        return false;

    table::spender::value::integer holder{};
    if (!store_.spender.set(spent.value,
        table::spender::spend{ {}, holder, spender }))
        return false;

    // The first spender of the output.
    if (!table::spender::is_spent(holder))
        return true;

    // The same tx (any instance) is not a conflict.
    const tx_link other{ table::spender::spender_fk(holder) };
    if ((other == spender) || (get_tx_key(other) == get_tx_key(spender)))
        return true;

    // A contested output, each additional spender is recorded once.
    const auto key = table::conflict::to_key(spent);
    for (auto it = store_.conflict.it(key); it; ++it)
    {
        table::conflict::record conflict{};
        if (!store_.conflict.get(*it, conflict))
            return false;

        if (conflict.spender_fk == spender)
            return true;
    }

    return store_.conflict.put(key, table::conflict::record{ {}, spender }) &&
        store_.spender.set(spent.value, table::spender::contest{});
}

TEMPLATE
bool CLASS::spend(const ins_link& link, const outs_link& spent) NOEXCEPT
{
    // ========================================================================
    const auto scope = get_transactor();
    return spend(link, spent, to_input_tx(link));
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
    binds values{};

    // ========================================================================
    const auto scope = get_transactor();

    for (auto index = one; index < txs.size(); ++index)
    {
        const auto& tx = txs.at(index);
        values.clear();
        for (auto in = tx.inputs_begin(); in != tx.inputs_end(); ++in)
            values.emplace_back(in->metadata.input_link,
                in->metadata.output_link);

        const auto ec = bind(tx_link{ links.at(index) }, values);
        if (ec == system::error::missing_previous_output)
            found = false;
        else if (ec)
            return ec;
    }
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

    binds values{};
    values.reserve(record.ins_count);
    auto fk = record.points_fk;
    for (const auto& in: *tx.inputs_ptr())
    {
        const auto& point = in->point();
        const outs_link spent{ in->metadata.output_link };
        values.emplace_back(fk++, spent.is_terminal() ?
            to_output(to_tx(point.hash()), point.index()).value : spent.value);
    }

    // ========================================================================
    const auto scope = get_transactor();
    return bind(link, values);
    // ========================================================================
}

TEMPLATE
code CLASS::bind(const header_link& link) NOEXCEPT
{
    tx_binds values{};
    if (const auto ec = get_binds(values, link))
        return ec;

    auto found = true;

    // ========================================================================
    const auto scope = get_transactor();

    for (const auto& [tx, spends]: values)
    {
        const auto ec = bind(tx_link{ tx }, spends);
        if (ec == system::error::missing_previous_output)
            found = false;
        else if (ec)
            return ec;
    }
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

    binds values{};
    if (const auto ec = get_binds(values, link))
        return ec;

    // ========================================================================
    const auto scope = get_transactor();
    return bind(link, values);
    // ========================================================================
}

} // namespace database
} // namespace libbitcoin

#endif
