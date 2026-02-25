/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_DATABASE_QUERY_ESTIMATE_IPP
#define LIBBITCOIN_DATABASE_QUERY_ESTIMATE_IPP

#include <atomic>
#include <algorithm>
#include <iterator>
#include <memory>
#include <numeric>
#include <utility>
#include <bitcoin/database/define.hpp>

// virtual_size
// ----------------------------------------------------------------------------

namespace libbitcoin {
namespace database {

TEMPLATE
bool CLASS::get_tx_virtual_size(size_t& out,
    const tx_link& link) const NOEXCEPT
{
    size_t light{}, heavy{};
    if (!get_tx_sizes(light, heavy, link))
        return false;

    out = system::chain::virtual_size(light, heavy);
    return true;
}

TEMPLATE
bool CLASS::get_block_virtual_size(size_t& out,
    const header_link& link) const NOEXCEPT
{
    size_t light{}, heavy{};
    if (!get_block_sizes(light, heavy, link))
        return false;

    out = system::chain::virtual_size(light, heavy);
    return true;
}

// tx/block/branch fee rates
// ----------------------------------------------------------------------------

TEMPLATE
bool CLASS::get_tx_fees(fee_rate& out, const tx_link& link) const NOEXCEPT
{
#if defined(SLOW_FEES)
    const auto tx = get_transaction(link, false);
    if (!tx || tx->is_coinbase() || !populate_without_metadata(*tx))
        return false;

    out.bytes = tx->virtual_size();
    out.fee = tx->fee();
    return true;
#else
    table::transaction::get_coinbase tx{};
    if (!store_.tx.get(link, tx) || tx.coinbase)
        return false;

    return get_tx_virtual_size(out.bytes, link) && get_tx_fee(out.fee, link);
#endif // SLOW_FEES
}
    
TEMPLATE
bool CLASS::get_block_fees(fee_rates& out,
    const header_link& link) const NOEXCEPT
{
#if defined(SLOW_FEES)
    out.clear();
    const auto block = get_block(link, false);
    if (!block)
        return false;

    block->populate();
    if (!populate_without_metadata(*block))
        return false;

    const auto& txs = *block->transactions_ptr();
    if (txs.empty())
        return false;

    out.reserve(txs.size());
    for (auto tx = std::next(txs.begin()); tx != txs.end(); ++tx)
        out.emplace_back((*tx)->virtual_size(), (*tx)->fee());

    return true;
#else // FAST_FEES|FASTER_FEES
    out.clear();
    table::txs::get_txs txs{};
    if (!store_.txs.at(to_txs(link), txs) || (txs.tx_fks.size() < one))
        return false;

    std::atomic_bool fail{};
    out.resize(sub1(txs.tx_fks.size()));
    const auto begin = std::next(txs.tx_fks.begin());
    constexpr auto parallel = poolstl::execution::par;
    constexpr auto relaxed = std::memory_order_relaxed;

    std::transform(parallel, begin, txs.tx_fks.end(), out.begin(),
        [&](const auto& tx_fk) NOEXCEPT
        {
            fee_rate rate{};
            if (!fail.load(relaxed) && !get_tx_fees(rate, tx_fk))
                fail.store(true, relaxed);

            return rate;
        });

    return !fail.load(relaxed);
#endif // SLOW_FEES
}

TEMPLATE
bool CLASS::get_branch_fees(std::atomic_bool& cancel, fee_rate_sets& out,
    size_t start, size_t count) const NOEXCEPT
{
    out.clear();
    if (is_zero(count))
        return true;

    if (system::is_add_overflow(start, sub1(count)))
        return false;

    const auto last = start + sub1(count);
    if (last > get_top_confirmed())
        return false;

    out.resize(count);
    std::vector<size_t> offsets(count);
    std::iota(offsets.begin(), offsets.end(), zero);

    std::atomic_bool fail{};
    constexpr auto relaxed = std::memory_order_relaxed;
    std::for_each(poolstl::execution::par, offsets.begin(), offsets.end(),
        [&](const size_t& offset) NOEXCEPT
        {
            if (fail.load(relaxed))
                return;

            if (cancel.load(relaxed) || !get_block_fees(out.at(offset),
                to_confirmed(start + offset)))
                fail.store(true, relaxed);
        });

    const auto failed = fail.load(relaxed);
    if (failed) out.clear();
    return !failed;
}

} // namespace database
} // namespace libbitcoin

#endif
