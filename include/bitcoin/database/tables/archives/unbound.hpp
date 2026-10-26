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
#ifndef LIBBITCOIN_DATABASE_TABLES_ARCHIVES_UNBOUND_HPP
#define LIBBITCOIN_DATABASE_TABLES_ARCHIVES_UNBOUND_HPP

#include <bitcoin/database/define.hpp>
#include <bitcoin/database/memory/memory.hpp>
#include <bitcoin/database/primitives/primitives.hpp>
#include <bitcoin/database/tables/schema.hpp>

namespace libbitcoin {
namespace database {
namespace table {

/// Unbound is a record hashmap of the points of inputs that were unbound when
/// the points cache was dropped, keyed by ins link.
struct unbound
  : public hash_map<schema::unbound>
{
    using in = schema::ins::link;
    using ix = linkage<schema::index>;
    using hash_map<schema::unbound>::hashmap;

    static inline key to_key(const in& ins_fk) NOEXCEPT
    {
        return ins_fk;
    }

    static constexpr uint32_t to_index(ix::integer value) NOEXCEPT
    {
        using namespace system;
        return value == ix::terminal ? chain::point::null_index : value;
    }

    static constexpr ix::integer from_index(uint32_t index) NOEXCEPT
    {
        using namespace system;
        return index == chain::point::null_index ? ix::terminal :
            possible_narrow_cast<ix::integer>(index);
    }

    struct record
      : public schema::unbound
    {
        inline bool from_data(reader& source) NOEXCEPT
        {
            key =
            {
                source.read_hash(),
                to_index(source.read_little_endian<ix::integer, ix::size>())
            };

            BC_ASSERT(!source || source.get_read_position() == minrow);
            return source;
        }

        inline bool to_data(finalizer& sink) const NOEXCEPT
        {
            sink.write_bytes(key.hash());
            sink.write_little_endian<ix::integer, ix::size>(from_index(key.index()));
            BC_ASSERT(!sink || sink.get_write_position() == minrow);
            return sink;
        }

        inline bool operator==(const record&) const NOEXCEPT = default;

        system::chain::point key{};
    };
};

} // namespace table
} // namespace database
} // namespace libbitcoin

#endif
