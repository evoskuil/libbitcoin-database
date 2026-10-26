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
#ifndef LIBBITCOIN_DATABASE_TABLES_CACHES_POINTS_HPP
#define LIBBITCOIN_DATABASE_TABLES_CACHES_POINTS_HPP

#include <bitcoin/database/define.hpp>
#include <bitcoin/database/memory/memory.hpp>
#include <bitcoin/database/primitives/primitives.hpp>
#include <bitcoin/database/tables/schema.hpp>

namespace libbitcoin {
namespace database {
namespace table {

/// Points is a based record arraymap of input points (hash:index) keyed by
/// ins link. Records are read only until their inputs are bound, after which
/// the table is dropped and its base advanced to the ins count.
struct points
  : public based_array_map<schema::points>
{
    using ix = linkage<schema::index>;
    using based_array_map<schema::points>::arraymap;

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
      : public schema::points
    {
        inline bool from_data(reader& source) NOEXCEPT
        {
            hash = source.read_hash();
            index = to_index(source.read_little_endian<ix::integer, ix::size>());
            BC_ASSERT(!source || source.get_read_position() == minrow);
            return source;
        }

        inline bool to_data(flipper& sink) const NOEXCEPT
        {
            sink.write_bytes(hash);
            sink.write_little_endian<ix::integer, ix::size>(from_index(index));
            BC_ASSERT(!sink || sink.get_write_position() == minrow);
            return sink;
        }

        inline bool is_null() const NOEXCEPT
        {
            return index == system::chain::point::null_index;
        }

        inline bool operator==(const record&) const NOEXCEPT = default;

        hash_digest hash{};
        uint32_t index{};
    };

    struct get_composed
      : public schema::points
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

        system::chain::point key{};
    };

    struct put_ref
      : public schema::points
    {
        inline link count() const NOEXCEPT
        {
            return system::possible_narrow_cast<link::integer>(tx_.inputs());
        }

        inline bool to_data(flipper& sink) const NOEXCEPT
        {
            const auto& ins = *tx_.inputs_ptr();
            std::ranges::for_each(ins, [&](const auto& in) NOEXCEPT
            {
                const auto& point = in->point();
                sink.write_bytes(point.hash());
                sink.write_little_endian<ix::integer, ix::size>(from_index(point.index()));
            });

            BC_ASSERT(!sink || sink.get_write_position() == count() * minrow);
            return sink;
        }

        const system::chain::transaction& tx_{};
    };

    struct put_view
      : public schema::points
    {
        inline link count() const NOEXCEPT
        {
            return system::possible_narrow_cast<link::integer>(tx_.inputs());
        }

        inline bool to_data(flipper& sink) const NOEXCEPT
        {
            using namespace system;
            auto stream = tx_.get_inputs_stream();
            read::bytes::fast source{ stream };

            for (size_t in{}; in < tx_.inputs(); ++in)
            {
                sink.write_bytes(source.read_hash());
                const auto index = source.read_4_bytes_little_endian();
                sink.write_little_endian<ix::integer, ix::size>(from_index(index));
                source.skip_bytes(source.read_size());
                source.skip_bytes(sizeof(uint32_t));
            }

            BC_ASSERT(source);
            BC_ASSERT(!sink || sink.get_write_position() == count() * minrow);
            return sink && source;
        }

        const system::chain::view::transaction& tx_;
    };
};

} // namespace table
} // namespace database
} // namespace libbitcoin

#endif
