/**
 * Copyright (c) 2011-2025 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_DATABASE_TABLES_ARCHIVES_PUTS_HPP
#define LIBBITCOIN_DATABASE_TABLES_ARCHIVES_PUTS_HPP

#include <algorithm>
#include <bitcoin/system.hpp>
#include <bitcoin/database/define.hpp>
#include <bitcoin/database/memory/memory.hpp>
#include <bitcoin/database/primitives/primitives.hpp>
#include <bitcoin/database/tables/schema.hpp>

namespace libbitcoin {
namespace database {
namespace table {

/// Puts is a record output fk records.
struct puts
  : public no_map<schema::puts>
{
    using out = linkage<schema::put>;
    using output_links = std::vector<out::integer>;
    using no_map<schema::puts>::nomap;

    struct record
      : public schema::puts
    {
        link count() const NOEXCEPT
        {
            return system::possible_narrow_cast<link::integer>(out_fks.size());
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            std::for_each(out_fks.begin(), out_fks.end(), [&](auto& fk) NOEXCEPT
            {
                fk = source.read_little_endian<out::integer, out::size>();
            });

            BC_ASSERT(!source || source.get_read_position() == count() * out::size);
            return source;
        }

        inline bool to_data(flipper& sink) const NOEXCEPT
        {
            std::for_each(out_fks.begin(), out_fks.end(), [&](const auto& fk) NOEXCEPT
            {
                sink.write_little_endian<out::integer, out::size>(fk);
            });

            BC_ASSERT(!sink || sink.get_write_position() == count() * out::size);
            return sink;
        }

        inline bool operator==(const record& other) const NOEXCEPT
        {
            return out_fks == other.out_fks;
        }

        output_links out_fks{};
    };

    struct get_output
      : public schema::puts
    {
        link count() const NOEXCEPT
        {
            return 1;
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            out_fk = source.read_little_endian<out::integer, out::size>();
            return source;
        }

        out::integer out_fk{};
    };
};

} // namespace table
} // namespace database
} // namespace libbitcoin

#endif
