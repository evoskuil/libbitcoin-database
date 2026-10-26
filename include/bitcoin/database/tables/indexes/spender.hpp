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

/// Spender is a head-only array of spender tx fks indexed by outs.fk (an
/// output with other spenders is contested, its spenders beyond the first are
/// in the conflict table). A terminal cell is unspent. Cells are content: set
/// at spend, never cleared.
struct spender
  : public value_map<schema::spender>
{
    using tx = schema::transaction::link;
    using value = schema::spender::value;
    using value_map<schema::spender>::headmap;

    static constexpr bool is_spent(value::integer spender_fk) NOEXCEPT
    {
        return spender_fk != value::terminal;
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
                holder : spender_fk);
            return sink;
        }

        value::integer& holder;
        tx::integer spender_fk{};
    };
};

} // namespace table
} // namespace database
} // namespace libbitcoin

#endif
