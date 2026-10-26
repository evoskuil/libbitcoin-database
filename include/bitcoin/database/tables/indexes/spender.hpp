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

/// Spender is a record hashmap of the spender tx fks of outputs, keyed by outs
/// fk. An output has a row for each tx bound to it. Rows are content: put at
/// bind, never removed.
struct spender
  : public hash_map<schema::spender>
{
    using tx = schema::transaction::link;
    using outs = schema::outs::link;
    using hash_map<schema::spender>::hashmap;

    static inline key to_key(const outs& outs_fk) NOEXCEPT
    {
        return outs_fk;
    }

    struct record
      : public schema::spender
    {
        inline bool from_data(reader& source) NOEXCEPT
        {
            spender_fk = source.read_little_endian<tx::integer, tx::size>();
            BC_ASSERT(!source || source.get_read_position() == minrow);
            return source;
        }

        inline bool to_data(finalizer& sink) const NOEXCEPT
        {
            sink.write_little_endian<tx::integer, tx::size>(spender_fk);
            BC_ASSERT(!sink || sink.get_write_position() == minrow);
            return sink;
        }

        inline bool operator==(const record&) const NOEXCEPT = default;

        tx::integer spender_fk{};
    };
};

} // namespace table
} // namespace database
} // namespace libbitcoin

#endif
