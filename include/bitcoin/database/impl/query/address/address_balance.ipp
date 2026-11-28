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
#ifndef LIBBITCOIN_DATABASE_QUERY_ADDRESS_BALANCE_IPP
#define LIBBITCOIN_DATABASE_QUERY_ADDRESS_BALANCE_IPP

#include <atomic>
#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

// Address balance
// ----------------------------------------------------------------------------
// server/native
TEMPLATE
code CLASS::get_confirmed_balance(const stopper& cancel, uint64_t& out,
    const hash_digest& key, size_t limit, bool turbo) const NOEXCEPT
{
    outpoints outs{};
    if (const auto ec = get_confirmed_unspent_outpoints(cancel, outs, key,
        limit, turbo))
    {
        out = zero;
        return ec;
    }

    // Use of to_confirmed_unspent_outputs() provides necessary deduplication.
    out = std::accumulate(outs.cbegin(), outs.cend(), 0_u64,
        [](uint64_t total, const outpoint& out) NOEXCEPT
        {
            return system::ceilinged_add(total, out.value());
        });

    return error::success;
}

// Output spent by a confirmed tx is not a receipt.
TEMPLATE
code CLASS::get_unconfirmed_receipts(const stopper& cancel,
    pool_receipts& out, const hash_digest& key, size_t limit) const NOEXCEPT
{
    out.clear();
    output_links outs{};
    tx_links parents{};
    outs_link cursor{};
    if (const auto ec = to_address_outputs(cancel, cursor, outs, parents, key,
        limit))
        return ec;

    for (size_t index{}; index < outs.size(); ++index)
    {
        if (cancel)
            return error::query_canceled;

        const auto& link = outs.at(index);
        const auto& parent = parents.at(index);
        if (!is_confirmable_tx(parent))
            continue;

        auto spent = false;
        pool_receipt receipt{ to_pool_row(parent).value };
        for (const auto& in: to_spenders(link))
        {
            const auto spender = to_input_tx(in);
            const auto state = get_tx_state(spender);
            spent |= (state == tx_state::confirmed);
            if (state == tx_state::confirmable)
                receipt.spenders.push_back(to_pool_row(spender).value);
        }

        if (spent)
            continue;

        if (!get_value(receipt.value, link))
            return error::integrity;

        out.push_back(std::move(receipt));
    }

    return error::success;
}

// server/esplora
// Confirmed output spent by a confirmed tx is not a receipt.
TEMPLATE
code CLASS::get_address_totals(const stopper& cancel,
    address_totals& confirmed, pool_receipts& unconfirmed,
    const hash_digest& key, size_t limit) const NOEXCEPT
{
    using namespace system;
    confirmed = {};
    unconfirmed.clear();
    output_links outs{};
    tx_links parents{};
    outs_link cursor{};
    if (const auto ec = to_address_outputs(cancel, cursor, outs, parents, key,
        limit))
        return ec;

    for (size_t index{}; index < outs.size(); ++index)
    {
        if (cancel)
            return error::query_canceled;

        const auto& out = outs.at(index);
        const auto& parent = parents.at(index);

        tx_link strong{};
        const auto block = find_strong(strong, parent);
        const auto is_strong = !block.is_terminal();
        if (is_strong && strong != parent)
            continue;

        const auto chain = is_strong && is_confirmed_block(block);
        if (!chain && !is_confirmable_tx(parent))
            continue;

        auto spent = false;
        pool_receipt receipt{ pool_link::terminal };
        if (!chain)
            receipt.row = to_pool_row(parent).value;

        if (!get_value(receipt.value, out))
            return error::integrity;

        for (const auto& in: to_spenders(out))
        {
            const auto spender = to_input_tx(in);
            const auto state = get_tx_state(spender);
            spent |= (state == tx_state::confirmed);
            if (state == tx_state::confirmable)
                receipt.spenders.push_back(to_pool_row(spender).value);
        }

        if (chain)
        {
            ++confirmed.funded_count;
            confirmed.funded_value = ceilinged_add(confirmed.funded_value,
                receipt.value);
        }

        if (chain && spent)
        {
            ++confirmed.spent_count;
            confirmed.spent_value = ceilinged_add(confirmed.spent_value,
                receipt.value);
        }

        if (spent || (chain && receipt.spenders.empty()))
            continue;

        unconfirmed.push_back(std::move(receipt));
    }

    return error::success;
}

} // namespace database
} // namespace libbitcoin

#endif
