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
#ifndef LIBBITCOIN_DATABASE_TABLES_ARCHIVES_SPEND_HPP
#define LIBBITCOIN_DATABASE_TABLES_ARCHIVES_SPEND_HPP

#include <bitcoin/database/define.hpp>
#include <bitcoin/database/memory/memory.hpp>
#include <bitcoin/database/primitives/primitives.hpp>
#include <bitcoin/database/tables/schema.hpp>

namespace libbitcoin {
namespace database {
namespace table {

/// Spend is a head-only array of spent outs fks indexed by ins.fk. A
/// terminal cell is unbound. Cells are content: set at bind, never cleared.
struct spend
  : public value_map<schema::spend>
{
    using outs = schema::outs::link;
    using value_map<schema::spend>::headmap;

    static constexpr bool is_spent(outs::integer outs_fk) NOEXCEPT
    {
        return outs_fk != outs::terminal;
    }

    /// Read the run of cells from first into out (sized to the run).
    inline bool get_run(std::vector<outs::integer>& out,
        size_t first) const NOEXCEPT
    {
        const auto ptr = get_memory();
        if (!ptr)
            return false;

        auto fk = first;
        for (auto& outs_fk: out)
            outs_fk = at(ptr, fk++).value;

        return true;
    }

    /// Set the spent outs fk if unbound, first is set if this binds the cell.
    struct bind
      : public schema::spend
    {
        inline bool from_data(reader& source) NOEXCEPT
        {
            holder = source.read_little_endian<outs::integer, cell>();
            return source;
        }

        inline bool to_data(flipper& sink) const NOEXCEPT
        {
            first = !is_spent(holder);
            sink.write_little_endian<outs::integer, cell>(first ? outs_fk :
                holder);
            return sink;
        }

        bool& first;
        outs::integer outs_fk{};
        outs::integer holder{};
    };
};

} // namespace table
} // namespace database
} // namespace libbitcoin

#endif
