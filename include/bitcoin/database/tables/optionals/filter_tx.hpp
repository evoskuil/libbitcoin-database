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
#ifndef LIBBITCOIN_DATABASE_TABLES_OPTIONALS_FILTER_TX_HPP
#define LIBBITCOIN_DATABASE_TABLES_OPTIONALS_FILTER_TX_HPP

#include <bitcoin/system.hpp>
#include <bitcoin/database/define.hpp>
#include <bitcoin/database/primitives/primitives.hpp>
#include <bitcoin/database/tables/schema.hpp>

namespace libbitcoin {
namespace database {
namespace table {
    
/// filter_tx is a slab of neutrino filters indexed by block link.
struct filter_tx
  : public array_map<schema::filter_tx>
{
    using array_map<schema::filter_tx>::arraymap;

    struct get_filter
      : public schema::filter_tx
    {
        link count() const NOEXCEPT
        {
            return system::possible_narrow_cast<link::integer>(
                variable_size(filter.size()) + filter.size());
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            filter = source.read_bytes(source.read_size());
            BC_ASSERT(!source || source.get_read_position() == count());
            return source;
        }

        system::data_chunk filter{};
    };

    struct put_ref
      : public schema::filter_tx
    {
        link count() const NOEXCEPT
        {
            return system::possible_narrow_cast<link::integer>(
                variable_size(filter.size()) + filter.size());
        }

        inline bool to_data(finalizer& sink) const NOEXCEPT
        {
            sink.write_variable(filter.size());
            sink.write_bytes(filter);
            BC_ASSERT(!sink || sink.get_write_position() == count());
            return sink;
        }

        const system::data_chunk& filter{};
    };
};

} // namespace table
} // namespace database
} // namespace libbitcoin

#endif
