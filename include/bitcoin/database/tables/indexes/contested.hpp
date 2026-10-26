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
#ifndef LIBBITCOIN_DATABASE_TABLES_INDEXES_CONTESTED_HPP
#define LIBBITCOIN_DATABASE_TABLES_INDEXES_CONTESTED_HPP

#include <bitcoin/database/define.hpp>
#include <bitcoin/database/primitives/primitives.hpp>
#include <bitcoin/database/tables/schema.hpp>

namespace libbitcoin {
namespace database {
namespace table {

/// Contested is a head-only array of bits indexed by outs.fk, eight to a
/// cell. A set bit marks an output spent by more than one tx (its spenders
/// beyond the first are in the conflict table). Bits are content: set at
/// spend, never cleared.
struct contested
  : public value_map<schema::contested>
{
    using outs = schema::outs::link;
    using value_map<schema::contested>::headmap;

    static constexpr size_t to_cell(outs::integer outs_fk) NOEXCEPT
    {
        return outs_fk / byte_bits;
    }

    static constexpr uint8_t to_mask(outs::integer outs_fk) NOEXCEPT
    {
        return system::bit_right<uint8_t>(outs_fk % byte_bits);
    }

    /// Merge the bit of an output into its cell.
    struct contest
      : public schema::contested
    {
        inline bool from_data(reader& source) NOEXCEPT
        {
            current = source.read_byte();
            return source;
        }

        inline bool to_data(flipper& sink) const NOEXCEPT
        {
            sink.write_byte(system::bit_or<value>(current, mask));
            return sink;
        }

        value mask{};
        value current{};
    };
};

} // namespace table
} // namespace database
} // namespace libbitcoin

#endif
