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
#ifndef LIBBITCOIN_DATABASE_QUERY_UNSPENT_IPP
#define LIBBITCOIN_DATABASE_QUERY_UNSPENT_IPP

#include <algorithm>
#include <atomic>
#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

// Unspent.
// ----------------------------------------------------------------------------

// The unspent set is the symmetric difference of creates and spends over
// the branch: paired events cancel and the residue is the utxo set.
static_assert(tx_link::bits <= bits<uint64_t> - difference_set<>::window_bits);

TEMPLATE
bool CLASS::is_bip30_exception(bool& out,
    const header_link& link) const NOEXCEPT
{
    using namespace system;

    static const chain::checkpoint first
    {
        "00000000000a4d0a398161ffc163c503763b1f4360639393e0e4c8e300e0caec",
        91842
    };

    static const chain::checkpoint second
    {
        "00000000000743f190a18c5577a3c2d2a1f610ae9601ac046a38084ccb7cd721",
        91880
    };

    out = false;
    size_t height{};
    if (!get_height(height, link))
        return false;

    if (height != first.height() && height != second.height())
        return true;

    // With the rule unconfigured duplicates are unconstrained (no exception).
    if (!script::is_enabled(initialized_forks(), chain::flags::bip30_rule))
        return true;

    const auto key = get_header_key(link);
    out = key == (height == first.height() ? first.hash() : second.hash());
    return true;
}

TEMPLATE
code CLASS::scan_unspent(difference_set<>& set, const stopper& cancel,
    const header_links& branch, bool turbo) const NOEXCEPT
{
    using namespace system;
    std::atomic_bool fail{};
    const auto parallel = poolstl::execution::par_if(turbo);

    // Reverse order in best effort attempt to minimize set size.
    std::for_each(parallel, branch.crbegin(), branch.crend(),
        [&](const auto& element) NOEXCEPT
        {
            if (cancel || fail)
                return;

            auto bip30_exception = false;
            const header_link link{ element };
            if (!is_bip30_exception(bip30_exception, link))
            {
                fail = true;
                return;
            }

            // The bip30 exception blocks are coinbase only.
            if (bip30_exception)
                return;

            auto coinbase = true;
            table::output::get_spendable value{};
            for (const auto& tx: to_transactions(link))
            {
                if (cancel || fail)
                    return;

                uint32_t index{};
                for (const auto& out: to_outputs(tx))
                {
                    if (!store_.output.get(out, value))
                    {
                        fail = true;
                        return;
                    }

                    if (!value.unspendable)
                        set.toggle(tx, index);

                    ++index;
                }

                if (coinbase)
                {
                    coinbase = false;
                    continue;
                }

                for (const auto& in: to_points(tx))
                {
                    const auto point = get_point_key(in);
                    auto it = store_.tx.it(point.hash());
                    if (!it)
                    {
                        fail = true;
                        return;
                    }

                    // Duplicate txs require the confirmed instance.
                    auto spent = *it;
                    if (++it && !is_confirmed_tx(spent))
                    {
                        for (spent = tx_link::terminal; it; ++it)
                        {
                            if (is_confirmed_tx(*it))
                            {
                                spent = *it;
                                break;
                            }
                        }

                        if (spent.is_terminal())
                        {
                            fail = true;
                            return;
                        }
                    }

                    set.toggle(spent, point.index());
                }
            }
        });

    if (fail)
        return error::integrity;

    if (cancel)
        return error::query_canceled;

    return error::success;
}

TEMPLATE
code CLASS::count_unspent(unspent_totals& out, const stopper& cancel,
    const difference_set<>::entries& survivors) const NOEXCEPT
{
    using namespace system;

    out = {};
    output_links puts{};
    auto previous = max_uint64;
    table::output::get_spendable value{};
    for (const auto& [key, mask]: survivors)
    {
        if (cancel)
            return error::query_canceled;

        const auto tx = difference_set<>::to_id(key);
        if (tx != previous)
        {
            previous = tx;
            ++out.transactions;
            puts = to_outputs(possible_narrow_cast<tx_link::integer>(tx));
        }

        const auto base = difference_set<>::to_index(key);
        for (auto bits = mask; !is_zero(bits);
            bits = bit_and(bits, sub1(bits)))
        {
            // A surviving spend with no create implies store inconsistency.
            const auto index = base + right_zeros(bits);
            if (index >= puts.size() ||
                !store_.output.get(puts[index], value))
                return error::integrity;

            ++out.outputs;
            out.value += value.value;
            out.script_bytes += value.script_size;
        }
    }

    return error::success;
}

TEMPLATE
code CLASS::get_unspent_totals(const stopper& cancel, unspent_totals& out,
    const header_links& branch, bool turbo) const NOEXCEPT
{
    difference_set<> set{};
    const auto ec = scan_unspent(set, cancel, branch, turbo);
    return ec ? ec : count_unspent(out, cancel, set.drain());
}

} // namespace database
} // namespace libbitcoin

#endif
