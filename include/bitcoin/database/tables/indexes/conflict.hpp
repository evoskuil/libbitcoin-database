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
#ifndef LIBBITCOIN_DATABASE_TABLES_INDEXES_CONFLICT_HPP
#define LIBBITCOIN_DATABASE_TABLES_INDEXES_CONFLICT_HPP

#include <bitcoin/database/define.hpp>
#include <bitcoin/database/primitives/primitives.hpp>
#include <bitcoin/database/tables/schema.hpp>

namespace libbitcoin {
namespace database {
namespace table {

/// Conflict is a record hashmap of the spender tx fks of contested outputs
/// beyond the first (which is in the spender head), keyed by outs fk.
struct conflict
  : public hash_map<schema::conflict>
{
    using tx = schema::transaction::link;
    using outs = schema::outs::link;
    using hash_map<schema::conflict>::hashmap;

    static inline key to_key(const outs& outs_fk) NOEXCEPT
    {
        return outs_fk;
    }

    struct record
      : public schema::conflict
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
