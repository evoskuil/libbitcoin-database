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

/// Contested is a head-only array of bit pairs indexed by outs.fk, four to a
/// cell. The low bit of a pair marks an output spent (bound by any tx), the
/// high bit marks it spent by more than one tx (all of its spenders are in
/// the spender table). Bits are content: set at bind, never cleared.
struct contested
  : public value_map<schema::contested>
{
    using outs = schema::outs::link;
    using value_map<schema::contested>::headmap;
    static constexpr size_t pairs = byte_bits / two;

    static constexpr size_t to_cell(outs::integer outs_fk) NOEXCEPT
    {
        return outs_fk / pairs;
    }

    static constexpr uint8_t to_spent(outs::integer outs_fk) NOEXCEPT
    {
        return system::bit_right<uint8_t>(two * (outs_fk % pairs));
    }

    static constexpr uint8_t to_contested(outs::integer outs_fk) NOEXCEPT
    {
        return system::bit_right<uint8_t>(add1(two * (outs_fk % pairs)));
    }

    /// Merge the spent bit of an output into its cell, and the contested bit
    /// if the spent bit is already set.
    struct spend
      : public schema::contested
    {
        inline bool from_data(reader& source) NOEXCEPT
        {
            current = source.read_byte();
            return source;
        }

        inline bool to_data(flipper& sink) const NOEXCEPT
        {
            using namespace system;
            const auto spent = to_spent(outs_fk);
            const auto merged = is_zero(bit_and<value>(current, spent)) ?
                spent : bit_or<value>(spent, to_contested(outs_fk));
            sink.write_byte(bit_or<value>(current, merged));
            return sink;
        }

        outs::integer outs_fk{};
        value current{};
    };
};

} // namespace table
} // namespace database
} // namespace libbitcoin

#endif
