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
#ifndef LIBBITCOIN_DATABASE_TABLES_ARCHIVES_TXS_HPP
#define LIBBITCOIN_DATABASE_TABLES_ARCHIVES_TXS_HPP

#include <algorithm>
#include <iterator>
#include <bitcoin/system.hpp>
#include <bitcoin/database/define.hpp>
#include <bitcoin/database/memory/memory.hpp>
#include <bitcoin/database/primitives/primitives.hpp>
#include <bitcoin/database/tables/schema.hpp>

namespace libbitcoin {
namespace database {
namespace table {

/// Txs is a slab arraymap of tx fks (first is count), indexed by header.fk.
struct txs
  : public array_map<schema::txs>
{
    using ct = linkage<schema::count_>;
    using tx = schema::transaction::link;
    using keys = std::vector<tx::integer>;
    using bytes = linkage<schema::size>;
    using array_map<schema::txs>::arraymap;

    struct slab
      : public schema::txs
    {
        link count() const NOEXCEPT
        {
            return system::possible_narrow_cast<link::integer>(ct::size +
                bytes::size + tx::size * tx_fks.size());
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            tx_fks.resize(source.read_little_endian<ct::integer, ct::size>());
            wire = source.read_little_endian<bytes::integer, bytes::size>();
            std::for_each(tx_fks.begin(), tx_fks.end(), [&](auto& fk) NOEXCEPT
            {
                fk = source.read_little_endian<tx::integer, tx::size>();
            });

            BC_ASSERT(!source || source.get_read_position() == count());
            return source;
        }

        inline bool to_data(finalizer& sink) const NOEXCEPT
        {
            BC_ASSERT(tx_fks.size() < system::power2<uint64_t>(to_bits(ct::size)));
            const auto fks = system::possible_narrow_cast<ct::integer>(tx_fks.size());

            sink.write_little_endian<ct::integer, ct::size>(fks);
            sink.write_little_endian<bytes::integer, bytes::size>(wire);
            std::for_each(tx_fks.begin(), tx_fks.end(), [&](const auto& fk) NOEXCEPT
            {
                sink.write_little_endian<tx::integer, tx::size>(fk);
            });

            BC_ASSERT(!sink || sink.get_write_position() == count());
            return sink;
        }

        inline bool operator==(const slab& other) const NOEXCEPT
        {
            return tx_fks == other.tx_fks;
        }

        // block.serialized_size(true)
        bytes::integer wire{};
        keys tx_fks{};
    };

    // put a contiguous set of tx identifiers.
    // TODO: this could be collapsed to a count and first using a sentinel.
    struct put_group
      : public schema::txs
    {
        link count() const NOEXCEPT
        {
            return system::possible_narrow_cast<link::integer>(ct::size +
                bytes::size + tx::size * number);
        }

        inline bool to_data(finalizer& sink) const NOEXCEPT
        {
            BC_ASSERT(number < system::power2<uint64_t>(to_bits(ct::size)));

            sink.write_little_endian<ct::integer, ct::size>(number);
            sink.write_little_endian<bytes::integer, bytes::size>(wire);

            for (auto fk = tx_fk; fk < (tx_fk + number); ++fk)
                sink.write_little_endian<tx::integer, tx::size>(fk);

            BC_ASSERT(!sink || sink.get_write_position() == count());
            return sink;
        }

        // block.serialized_size(true)
        bytes::integer wire{};
        ct::integer number{};
        tx::integer tx_fk{};
    };

    struct get_position
      : public schema::txs
    {
        link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            const auto number = source.read_little_endian<ct::integer, ct::size>();
            source.skip_bytes(bytes::size);
            for (position = zero; position < number; ++position)
                if (source.read_little_endian<tx::integer, tx::size>() == tx_fk)
                    return source;

            source.invalidate();
            return source;
        }

        const tx::integer tx_fk{};
        size_t position{};
    };

    struct get_coinbase
      : public schema::txs
    {
        link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            const auto number = source.read_little_endian<ct::integer, ct::size>();
            source.skip_bytes(bytes::size);
            if (is_nonzero(number))
            {
                coinbase_fk = source.read_little_endian<tx::integer, tx::size>();
                return source;
            }

            source.invalidate();
            return source;
        }

        tx::integer coinbase_fk{};
    };

    struct get_block_size
      : public schema::txs
    {
        link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            source.skip_bytes(ct::size);
            wire = source.read_little_endian<bytes::integer, bytes::size>();
            return source;
        }

        bytes::integer wire{};
    };

    struct get_associated
      : public schema::txs
    {
        link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            associated = to_bool(source.read_little_endian<ct::integer, ct::size>());
            return source;
        }

        bool associated{};
    };

    struct get_txs
      : public schema::txs
    {
        link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            tx_fks.resize(source.read_little_endian<ct::integer, ct::size>());
            source.skip_bytes(bytes::size);
            std::for_each(tx_fks.begin(), tx_fks.end(), [&](auto& fk) NOEXCEPT
            {
                fk = source.read_little_endian<tx::integer, tx::size>();
            });

            return source;
        }

        keys tx_fks{};
    };

    struct get_spending_txs
      : public schema::txs
    {
        link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            const auto number = source.read_little_endian<ct::integer, ct::size>();
            if (number <= one)
                return source;

            tx_fks.resize(sub1(number));
            source.skip_bytes(bytes::size + tx::size);
            std::for_each(tx_fks.begin(), tx_fks.end(), [&](auto& fk) NOEXCEPT
            {
                fk = source.read_little_endian<tx::integer, tx::size>();
            });

            return source;
        }

        keys tx_fks{};
    };

    struct get_tx_quantity
      : public schema::txs
    {
        link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            number = source.read_little_endian<ct::integer, ct::size>();
            return source;
        }

        size_t number{};
    };

    struct get_coinbase_and_count
      : public schema::txs
    {
        link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            number = source.read_little_endian<ct::integer, ct::size>();
            source.skip_bytes(bytes::size);

            if (is_zero(number))
            {
                source.invalidate();
            }
            else
            {
                coinbase_fk = source.read_little_endian<tx::integer, tx::size>();
            }

            return source;
        }

        size_t number{};
        tx::integer coinbase_fk{};
    };
};

} // namespace table
} // namespace database
} // namespace libbitcoin

#endif
