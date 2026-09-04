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
#ifndef LIBBITCOIN_DATABASE_UNSPENT_UNSPENT_MUHASH_IPP
#define LIBBITCOIN_DATABASE_UNSPENT_UNSPENT_MUHASH_IPP

#include <bitcoin/database/define.hpp>
#include <bitcoin/database/unspent/unspent_writer.hpp>

namespace libbitcoin {
namespace database {

TEMPLATE
CLASS::unspent_muhash(const Store& store, const stopper& cancel,
    bool turbo) NOEXCEPT
  : reader_(store, cancel), spans_(store, cancel, turbo), store_(store)
{
}

TEMPLATE
code CLASS::hash(unspent_totals& out, hash_digest& digest,
    const difference_set& set) const NOEXCEPT
{
    using namespace system;
    std_vector<unspent_totals> totals(spans_.count());
    std_vector<muhash3072> partials(spans_.count());
    const auto ec = spans_.for_each(set,
        [&](size_t index, size_t begin, size_t end) NOEXCEPT
        {
            return span(totals.at(index), partials.at(index), set, begin, end);
        });

    if (ec)
        return ec;

    muhash3072 muhash{};
    for (size_t index{}; index < partials.size(); ++index)
    {
        unspent_writer::add(out, totals.at(index));
        muhash *= partials.at(index);
    }

    digest = muhash.flush();
    return error::success;
}

TEMPLATE
code CLASS::span(unspent_totals& out, system::muhash3072& partial,
    const difference_set& set, size_t begin, size_t end) const NOEXCEPT
{
    output_links puts{};
    unspent_coins coins{};
    auto previous = tx_link::terminal;

    return reader_.elements(set, begin, end,
        [&](const unspent_elements& at) NOEXCEPT
        {
            puts.resize(at.size());
            coins.resize(at.size());
            auto ec = reader_.fill(coins, puts, zero, previous, at, zero,
                at.size());

            if (!ec)
                ec = insert(out, partial, coins, puts);

            return ec;
        });
}

TEMPLATE
code CLASS::insert(unspent_totals& out, system::muhash3072& partial,
    const unspent_coins& coins, const output_links& puts) const NOEXCEPT
{
    using namespace system;
    const auto ptr = store_.output.get_memory();
    for (size_t at{}; at < coins.size(); ++at)
    {
        hash_digest digest{};
        stream::out::fast stream{ digest };
        hash::sha256::fast sink{ stream };
        const auto& coin = coins.at(at);
        unspent_writer::write(sink, coin);
        table::output::write_script tail{ {}, sink };
        if (!store_.output.raw(ptr, puts.at(at), tail))
            return error::integrity;

        sink.flush();
        partial.insert_hash(digest);
        unspent_writer::add(out, coin.first, tail.value, tail.length);
    }

    return error::success;
}

} // namespace database
} // namespace libbitcoin

#endif
