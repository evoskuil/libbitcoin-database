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
#include <utility>
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
    if (!store_.envelope().forks.bip30)
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

            // Coin serialization is outpoint, height code, value, script.
            constexpr auto coin_overhead = hash_size + sizeof(uint32_t) +
                sizeof(uint32_t) + sizeof(uint64_t);

            ++out.outputs;
            out.value += value.value;
            out.script_bytes += value.script_size;
            out.coin_bytes += coin_overhead +
                variable_size(value.script_size) + value.script_size;
        }
    }

    return error::success;
}

TEMPLATE
template <typename Visitor>
code CLASS::visit_unspent(const Visitor& visit, const stopper& cancel,
    const difference_set<>::entries& survivors) const NOEXCEPT
{
    using namespace system;

    unspent_coin coin{};
    output_links puts{};
    auto previous = max_uint64;
    const auto bip30 = store_.envelope().forks.bip30;
    table::output::get_coin out{};
    for (const auto& [key, mask]: survivors)
    {
        if (cancel)
            return error::query_canceled;

        const auto tx = difference_set<>::to_id(key);
        coin.first = (tx != previous);
        if (coin.first)
        {
            previous = tx;
            const auto link = possible_narrow_cast<tx_link::integer>(tx);
            const auto at = get_confirmed_height(find_strong(link));
            coin.txid = get_tx_key(link);
            if (at.is_terminal() || coin.txid == null_hash)
                return error::integrity;

            coin.height = at.value;
            coin.coinbase = is_coinbase(link);

            // bitcoind retains duplicated coinbases at the overwriting
            // heights (bip30 exceptions), the store at the originals.
            if (bip30 && coin.coinbase)
                coin.height = (coin.height == 91812) ? 91842 :
                    (coin.height == 91722) ? 91880 : coin.height;

            puts = to_outputs(link);
        }

        const auto base = difference_set<>::to_index(key);
        for (auto bits = mask; !is_zero(bits);
            bits = bit_and(bits, sub1(bits)))
        {
            // A surviving spend with no create implies store inconsistency.
            const auto index = base + right_zeros(bits);
            if (index >= puts.size() || !store_.output.get(puts[index], out))
                return error::integrity;

            coin.index = possible_narrow_cast<uint32_t>(index);
            coin.value = out.value;
            std::swap(coin.script, out.script);
            visit(coin);
            coin.first = false;
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

TEMPLATE
template <typename Visitor>
code CLASS::get_unspent_coins(const stopper& cancel, const Visitor& visit,
    const header_links& branch, bool ordered, bool turbo) const NOEXCEPT
{
    using namespace system;

    difference_set<> set{};
    if (const auto ec = scan_unspent(set, cancel, branch, turbo))
        return ec;

    auto survivors = set.drain();
    if (ordered)
    {
        // Canonical order is (txid, index), as bitcoind's chainstate cursor.
        // The txid sort requires full materialization (memory expensive).
        using keyed_entry = std::pair<hash_digest, difference_set<>::entry>;
        std_vector<keyed_entry> keyed{};
        keyed.reserve(survivors.size());
        for (const auto& item: survivors)
        {
            if (cancel)
                return error::query_canceled;

            const auto tx = difference_set<>::to_id(item.first);
            auto txid = get_tx_key(possible_narrow_cast<tx_link::integer>(tx));
            if (txid == null_hash)
                return error::integrity;

            keyed.emplace_back(std::move(txid), item);
        }

        // Key order preserves window order within a transaction.
        std::sort(poolstl::execution::par_if(turbo), keyed.begin(),
            keyed.end(), [](const auto& left, const auto& right) NOEXCEPT
            {
                return (left.first == right.first) ?
                    (left.second.first < right.second.first) :
                    (left.first < right.first);
            });

        std::transform(keyed.begin(), keyed.end(), survivors.begin(),
            [](const auto& item) NOEXCEPT { return item.second; });
    }

    return visit_unspent(visit, cancel, survivors);
}

} // namespace database
} // namespace libbitcoin

#endif
