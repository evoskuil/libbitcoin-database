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
#ifndef LIBBITCOIN_DATABASE_UNSPENT_UNSPENT_MUHASH_HPP
#define LIBBITCOIN_DATABASE_UNSPENT_UNSPENT_MUHASH_HPP

#include <bitcoin/database/define.hpp>
#include <bitcoin/database/types/types.hpp>
#include <bitcoin/database/unspent/unspent_reader.hpp>
#include <bitcoin/database/unspent/unspent_spans.hpp>

namespace libbitcoin {
namespace database {

/// The muhash commitment of a set (order invariant, partial sets combined).
template <typename Store>
class unspent_muhash
{
public:
    unspent_muhash(const Store& store, const stopper& cancel,
        bool turbo) NOEXCEPT;

    /// The totals and muhash of the set.
    code hash(unspent_totals& out, hash_digest& digest,
        const difference_set& set) const NOEXCEPT;

private:
    code span(unspent_totals& out, system::muhash3072& partial,
        const difference_set& set, size_t begin, size_t end) const NOEXCEPT;
    code insert(unspent_totals& out, system::muhash3072& partial,
        const unspent_coins& coins, const output_links& puts) const NOEXCEPT;

    const unspent_reader<Store> reader_;
    const unspent_spans<Store> spans_;
    const Store& store_;
};

} // namespace database
} // namespace libbitcoin

#define TEMPLATE template <typename Store>
#define CLASS unspent_muhash<Store>

#include <bitcoin/database/impl/unspent/unspent_muhash.ipp>

#undef CLASS
#undef TEMPLATE

#endif
