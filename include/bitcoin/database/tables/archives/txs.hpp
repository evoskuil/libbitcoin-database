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
#include <optional>
#include <bitcoin/database/define.hpp>
#include <bitcoin/database/memory/memory.hpp>
#include <bitcoin/database/primitives/primitives.hpp>
#include <bitcoin/database/tables/schema.hpp>

namespace libbitcoin {
namespace database {
namespace table {

// TODO: make heavy field variable size.

/// Txs is a slab arraymap of tx fks (first is count), indexed by header.fk.
struct txs
  : public array_map<schema::txs>
{
    using ct = linkage<schema::count_>;
    using tx = schema::transaction::link;
    using keys = std::vector<tx::integer>;
    using bytes = linkage<schema::size, sub1(to_bits(schema::size))>;
    using hash = std::optional<hash_digest>;
    using array_map<schema::txs>::arraymap;
    static constexpr auto offset = bytes::bits;
    static_assert(offset < to_bits(bytes::size));

    static constexpr bytes::integer merge(bool is_interval,
        bytes::integer light) NOEXCEPT
    {
        BC_ASSERT_MSG(!system::get_right(light, offset), "overflow");
        return system::set_right(light, offset, is_interval);
    }

    static constexpr bool is_interval(bytes::integer merged) NOEXCEPT
    {
        return system::get_right(merged, offset);
    }

    static constexpr bytes::integer to_light(bytes::integer merged) NOEXCEPT
    {
        return system::set_right(merged, offset, false);
    }

    // Intervals are optional based on store configuration.
    struct slab
      : public schema::txs
    {
        inline bool is_genesis() const NOEXCEPT
        {
            return !tx_fks.empty() && is_zero(tx_fks.front());
        }

        inline link count() const NOEXCEPT
        {
            return system::possible_narrow_cast<link::integer>(ct::size +
                bytes::size + bytes::size + tx::size * tx_fks.size() +
                (interval.has_value() ? schema::hash : zero) +
                to_int(is_genesis()));
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            using namespace system;
            tx_fks.resize(source.read_little_endian<ct::integer, ct::size>());
            const auto merged = source.read_little_endian<bytes::integer, bytes::size>();
            heavy = source.read_little_endian<bytes::integer, bytes::size>();
            std::for_each(tx_fks.begin(), tx_fks.end(), [&](auto& fk) NOEXCEPT
            {
                fk = source.read_little_endian<tx::integer, tx::size>();
            });

            light = to_light(merged);
            interval.reset();
            if (is_interval(merged)) interval = source.read_hash();
            depth = is_genesis() ? source.read_byte() : zero;
            BC_ASSERT(!source || source.get_read_position() == count());
            return source;
        }

        inline bool to_data(finalizer& sink) const NOEXCEPT
        {
            using namespace system;
            BC_ASSERT(tx_fks.size() < power2<uint64_t>(to_bits(ct::size)));

            const auto has_interval = interval.has_value();
            const auto merged = merge(has_interval, light);
            const auto fks = possible_narrow_cast<ct::integer>(tx_fks.size());
            sink.write_little_endian<ct::integer, ct::size>(fks);
            sink.write_little_endian<bytes::integer, bytes::size>(merged);
            sink.write_little_endian<bytes::integer, bytes::size>(heavy);
            std::for_each(tx_fks.begin(), tx_fks.end(),
                [&](const auto& fk) NOEXCEPT
                {
                    sink.write_little_endian<tx::integer, tx::size>(fk);
                });

            if (has_interval) sink.write_bytes(interval.value());
            if (is_genesis()) sink.write_byte(depth);
            BC_ASSERT(!sink || sink.get_write_position() == count());
            return sink;
        }

        inline bool operator==(const slab& other) const NOEXCEPT
        {
            return light == other.light
                && heavy == other.heavy
                && tx_fks == other.tx_fks
                && interval == other.interval
                && depth == other.depth;
        }

        bytes::integer light{}; // block.serialized_size(false)
        bytes::integer heavy{}; // block.serialized_size(true)
        keys tx_fks{};
        hash interval{};
        uint8_t depth{};
    };

    // put a contiguous set of tx identifiers.
    struct put_group
      : public schema::txs
    {
        inline bool is_genesis() const NOEXCEPT
        {
            return is_zero(tx_fk);
        }

        inline link count() const NOEXCEPT
        {
            return system::possible_narrow_cast<link::integer>(ct::size +
                bytes::size + bytes::size + tx::size * number +
                (interval.has_value() ? schema::hash : zero) +
                to_int(is_zero(tx_fk)));
        }

        // TODO: fks can instead be stored as a count and coinbase fk,
        // TODO: but will need to be disambiguated from compact blocks.
        inline bool to_data(finalizer& sink) const NOEXCEPT
        {
            BC_ASSERT(number < system::power2<uint64_t>(to_bits(ct::size)));

            const auto has_interval = interval.has_value();
            const auto merged = merge(has_interval, light);
            sink.write_little_endian<ct::integer, ct::size>(number);
            sink.write_little_endian<bytes::integer, bytes::size>(merged);
            sink.write_little_endian<bytes::integer, bytes::size>(heavy);

            for (auto fk = tx_fk; fk < (tx_fk + number); ++fk)
                sink.write_little_endian<tx::integer, tx::size>(fk);

            if (has_interval) sink.write_bytes(interval.value());
            if (is_genesis()) sink.write_byte(depth);
            BC_ASSERT(!sink || sink.get_write_position() == count());
            return sink;
        }

        ct::integer number{};
        bytes::integer light{};
        bytes::integer heavy{};
        tx::integer tx_fk{};
        hash interval{};
        uint8_t depth{};
    };

    struct get_interval
      : public schema::txs
    {
        inline link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            using namespace system;
            const auto number = source.read_little_endian<ct::integer, ct::size>();
            const auto merged = source.read_little_endian<bytes::integer, bytes::size>();
            source.skip_bytes(bytes::size + tx::size * number);
            if (is_interval(merged)) interval = source.read_hash();
            return source;
        }

        hash interval{};
    };

    // This reader is only applicable to the genesis block.
    struct get_genesis_depth
      : public schema::txs
    {
        inline link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        // Stored at end since only read once (at startup).
        inline bool from_data(reader& source) NOEXCEPT
        {
            const auto number = source.read_little_endian<ct::integer, ct::size>();
            const auto merged = source.read_little_endian<bytes::integer, bytes::size>();
            const auto skip_interval = is_interval(merged) ? schema::hash : zero;
            source.skip_bytes(bytes::size + tx::size * number + skip_interval);
            depth = source.read_byte();
            return source;
        }

        uint8_t depth{};
    };

    struct get_position
      : public schema::txs
    {
        inline link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            const auto number = source.read_little_endian<ct::integer, ct::size>();
            source.skip_bytes(bytes::size + bytes::size);
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
        inline link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            const auto number = source.read_little_endian<ct::integer, ct::size>();
            source.skip_bytes(bytes::size + bytes::size);
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

    struct get_tx
      : public schema::txs
    {
        inline link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            const auto number = source.read_little_endian<ct::integer, ct::size>();
            source.skip_bytes(bytes::size + bytes::size);
            if (number > position)
            {
                source.skip_bytes(position * tx::size);
                tx_fk = source.read_little_endian<tx::integer, tx::size>();
                return source;
            }

            source.invalidate();
            return source;
        }

        const size_t position{};
        tx::integer tx_fk{};
    };

    struct get_sizes
      : public schema::txs
    {
        static constexpr bytes::integer to_size(bytes::integer merged) NOEXCEPT
        {
            return system::set_right(merged, offset, false);
        }

        inline link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            source.skip_bytes(ct::size);
            light = to_size(source.read_little_endian<bytes::integer, bytes::size>());
            heavy = source.read_little_endian<bytes::integer, bytes::size>();
            return source;
        }

        bytes::integer light{};
        bytes::integer heavy{};
    };

    struct get_associated
      : public schema::txs
    {
        inline link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            // Read is cheap but unnecessary, since 0 is never set here.
            associated = to_bool(source.read_little_endian<ct::integer, ct::size>());
            return source;
        }

        bool associated{};
    };

    struct get_txs
      : public schema::txs
    {
        inline link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            tx_fks.resize(source.read_little_endian<ct::integer, ct::size>());
            source.skip_bytes(bytes::size + bytes::size);
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
        inline link count() const NOEXCEPT
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
            source.skip_bytes(bytes::size + bytes::size + tx::size);
            std::for_each(tx_fks.begin(), tx_fks.end(), [&](auto& fk) NOEXCEPT
            {
                fk = source.read_little_endian<tx::integer, tx::size>();
            });

            return source;
        }

        keys tx_fks{};
    };

    struct get_tx_count
      : public schema::txs
    {
        inline link count() const NOEXCEPT
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
        inline link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            number = source.read_little_endian<ct::integer, ct::size>();
            source.skip_bytes(bytes::size + bytes::size);

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
