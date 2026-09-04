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
#ifndef LIBBITCOIN_DATABASE_UNSPENT_UNSPENT_COUNTER_HPP
#define LIBBITCOIN_DATABASE_UNSPENT_UNSPENT_COUNTER_HPP

#include <bitcoin/database/define.hpp>
#include <bitcoin/database/types/types.hpp>
#include <bitcoin/database/unspent/unspent_writer.hpp>
#include <bitcoin/database/unspent/unspent_reader.hpp>
#include <bitcoin/database/unspent/unspent_spans.hpp>

namespace libbitcoin {
namespace database {

/// Totals of a set.
template <typename Store>
class unspent_counter
{
public:
    unspent_counter(const Store& store, const stopper& cancel,
        bool turbo) NOEXCEPT;

    /// The totals of the set.
    code count(unspent_totals& out, const difference_set& set) const NOEXCEPT;

private:
    code span(unspent_totals& out, const difference_set& set, size_t begin,
        size_t end) const NOEXCEPT;
    code read_totals(unspent_totals& out, tx_link::integer& previous,
        const output_links& puts) const NOEXCEPT;

    const Store& store_;
    const stopper& cancel_;
    const unspent_reader<Store> reader_;
    const unspent_spans<Store> spans_;
};

} // namespace database
} // namespace libbitcoin

#define TEMPLATE template <typename Store>
#define CLASS unspent_counter<Store>

#include <bitcoin/database/impl/unspent/unspent_counter.ipp>

#undef CLASS
#undef TEMPLATE

#endif
