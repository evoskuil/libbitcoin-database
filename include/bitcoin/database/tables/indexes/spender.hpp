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
#ifndef LIBBITCOIN_DATABASE_TABLES_INDEXES_SPENDER_HPP
#define LIBBITCOIN_DATABASE_TABLES_INDEXES_SPENDER_HPP

#include <bitcoin/database/define.hpp>
#include <bitcoin/database/primitives/primitives.hpp>
#include <bitcoin/database/tables/schema.hpp>

namespace libbitcoin {
namespace database {
namespace table {

/// Spender is a head-only array of spender tx fks indexed by outs.fk, each
/// merged with a contested bit set when another tx has spent the output (its
/// spenders beyond the first are in the conflict table). A terminal cell is
/// unspent. Cells are content: set at spend, never cleared.
struct spender
  : public value_map<schema::spender>
{
    using tx = schema::transaction::link;
    using value = schema::spender::value;
    using value_map<schema::spender>::headmap;
    static constexpr auto offset = tx::bits;
    static_assert(offset < to_bits(value::size));

    static constexpr value::integer merge(bool contested,
        tx::integer spender_fk) NOEXCEPT
    {
        BC_ASSERT_MSG(!system::get_right(spender_fk, offset), "overflow");
        return system::set_right(spender_fk, offset, contested);
    }

    static constexpr bool is_spent(value::integer merged) NOEXCEPT
    {
        return merged != value::terminal;
    }

    static constexpr bool is_contested(value::integer merged) NOEXCEPT
    {
        return system::get_right(merged, offset);
    }

    static constexpr tx::integer spender_fk(value::integer merged) NOEXCEPT
    {
        return system::set_right(merged, offset, false);
    }

    /// Set the spender if the cell is unspent, holder is the prior cell.
    struct spend
      : public schema::spender
    {
        inline bool from_data(reader& source) NOEXCEPT
        {
            holder = source.read_little_endian<value::integer, cell>();
            return source;
        }

        inline bool to_data(flipper& sink) const NOEXCEPT
        {
            sink.write_little_endian<value::integer, cell>(is_spent(holder) ?
                holder : merge(false, spender_fk));
            return sink;
        }

        value::integer& holder;
        tx::integer spender_fk{};
    };

    /// Set the contested bit, retaining the spender.
    struct contest
      : public schema::spender
    {
        inline bool from_data(reader& source) NOEXCEPT
        {
            current = source.read_little_endian<value::integer, cell>();
            return source;
        }

        inline bool to_data(flipper& sink) const NOEXCEPT
        {
            sink.write_little_endian<value::integer, cell>(
                system::set_right(current, offset, true));
            return sink;
        }

        value::integer current{};
    };
};

} // namespace table
} // namespace database
} // namespace libbitcoin

#endif
