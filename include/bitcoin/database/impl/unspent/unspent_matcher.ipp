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
#ifndef LIBBITCOIN_DATABASE_UNSPENT_UNSPENT_MATCHER_IPP
#define LIBBITCOIN_DATABASE_UNSPENT_UNSPENT_MATCHER_IPP

#include <iterator>
#include <bitcoin/database/define.hpp>
#include <bitcoin/database/unspent/unspent_writer.hpp>

namespace libbitcoin {
namespace database {

TEMPLATE
CLASS::unspent_matcher(const Store& store, const stopper& cancel,
    bool turbo) NOEXCEPT
  : reader_(store, cancel),
    spans_(store, cancel, turbo)
{
}

TEMPLATE
code CLASS::match(unspent_coins& out, size_t& txouts, const keys& keys,
    const difference_set& set) const NOEXCEPT
{
    BC_ASSERT(out.empty());

    txouts = zero;
    std_vector<unspent_totals> totals(spans_.count());
    std_vector<unspent_coins> matches(spans_.count());
    const auto ec = spans_.for_each(set,
        [&](size_t index, size_t begin, size_t end) NOEXCEPT
        {
            return span(totals.at(index), matches.at(index), keys, set, begin,
                end);
        });

    if (ec)
        return ec;

    for (auto index = zero; index < matches.size(); ++index)
    {
        txouts += totals.at(index).outputs;
        auto& found = matches.at(index);
        out.insert(out.end(),
            std::make_move_iterator(found.begin()),
            std::make_move_iterator(found.end()));
    }

    return error::success;
}

TEMPLATE
code CLASS::span(unspent_totals& out, unspent_coins& matches,
    const keys& keys, const difference_set& set, size_t begin,
    size_t end) const NOEXCEPT
{
    using namespace system;
    return reader_.batch(set, begin, end,
        [&](const unspent_coin& coin) NOEXCEPT
        {
            unspent_writer::add(out, coin);
            if (keys.contains(sha256_hash(coin.script)))
                matches.push_back(coin);
        });
}

} // namespace database
} // namespace libbitcoin

#endif
