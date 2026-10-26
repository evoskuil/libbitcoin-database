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
#ifndef LIBBITCOIN_DATABASE_TABLES_OPTIONALS_ADDRESS_HPP
#define LIBBITCOIN_DATABASE_TABLES_OPTIONALS_ADDRESS_HPP

#include <bitcoin/database/define.hpp>
#include <bitcoin/database/primitives/primitives.hpp>
#include <bitcoin/database/tables/schema.hpp>

namespace libbitcoin {
namespace database {
namespace table {

/// Address is a record hashmap of outs rows searched by output script hash,
/// with no stored key or value (rows aligned with outs, by expand and commit).
/// The index is disabled when it has no buckets, in which case nothing is
/// written to its body.
struct address
  : public hash_map<schema::address>
{
    using hash_map<schema::address>::hashmap;

    struct record
      : public schema::address
    {
        inline bool from_data(reader& source) NOEXCEPT
        {
            BC_ASSERT(!source || is_zero(source.get_read_position()));
            return source;
        }

        inline bool to_data(finalizer& sink) const NOEXCEPT
        {
            BC_ASSERT(!sink || is_zero(sink.get_write_position()));
            return sink;
        }
    };
};

} // namespace table
} // namespace database
} // namespace libbitcoin

#endif
