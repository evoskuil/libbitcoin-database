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
#ifndef LIBBITCOIN_DATABASE_TABLES_ARCHIVES_OUTS_HPP
#define LIBBITCOIN_DATABASE_TABLES_ARCHIVES_OUTS_HPP

#include <bitcoin/database/define.hpp>
#include <bitcoin/database/memory/memory.hpp>
#include <bitcoin/database/primitives/primitives.hpp>
#include <bitcoin/database/tables/schema.hpp>

namespace libbitcoin {
namespace database {
namespace table {

/// Outs is a record nomap of output fk and parent tx fk rows (one per output,
/// navigated from the tx record). The output fk carries the excluded bit and
/// the parent fk the coinbase bit, each reducing its domain.
struct outs
  : public no_map<schema::outs>
{
    using ix = linkage<schema::index>;
    using out = schema::output::link;
    using tx = schema::transaction::link;
    using output_links = std::vector<out::integer>;
    using excluded_outputs = std::vector<std::pair<ix::integer, out::integer>>;
    using no_map<schema::outs>::nomap;
    static constexpr auto excluded_offset = out::bits;
    static constexpr auto coinbase_offset = tx::bits;
    static_assert(excluded_offset < to_bits(out::size));
    static_assert(coinbase_offset < to_bits(tx::size));

    static constexpr out::integer merge(bool excluded,
        out::integer out_fk) NOEXCEPT
    {
        BC_ASSERT_MSG(!system::get_right(out_fk, excluded_offset), "overflow");
        return system::set_right(out_fk, excluded_offset, excluded);
    }

    static constexpr out::integer to_out_fk(out::integer merged) NOEXCEPT
    {
        return system::set_right(merged, excluded_offset, false);
    }

    static constexpr bool is_excluded(out::integer merged) NOEXCEPT
    {
        return system::get_right(merged, excluded_offset);
    }

    static inline bool is_excluded(const system::chain::output& out) NOEXCEPT
    {
        using namespace system;
        const auto& script = out.script();
        const auto size = script.serialized_size(false);
        return (size > chain::max_script_size) || (!is_zero(size) &&
            (script.ops().front().code() == chain::opcode::op_return));
    }

    static inline bool peek_excluded(auto& source, size_t bytes) NOEXCEPT
    {
        using namespace system;
        constexpr auto op_return = to_value(chain::opcode::op_return);
        return (bytes > chain::max_script_size) ||
            (!is_zero(bytes) && (source.peek_byte() == op_return));
    }

    static constexpr tx::integer merge_parent(bool coinbase,
        tx::integer parent_fk) NOEXCEPT
    {
        BC_ASSERT_MSG(!system::get_right(parent_fk, coinbase_offset), "overflow");
        return system::set_right(parent_fk, coinbase_offset, coinbase);
    }

    static constexpr tx::integer to_parent_fk(tx::integer merged) NOEXCEPT
    {
        return system::set_right(merged, coinbase_offset, false);
    }

    static constexpr bool is_coinbase(tx::integer merged) NOEXCEPT
    {
        return system::get_right(merged, coinbase_offset);
    }

    struct record
      : public schema::outs
    {
        inline bool from_data(reader& source) NOEXCEPT
        {
            const auto out = source.read_little_endian<out::integer, out::size>();
            const auto parent = source.read_little_endian<tx::integer, tx::size>();
            out_fk = to_out_fk(out);
            excluded = is_excluded(out);
            parent_fk = to_parent_fk(parent);
            coinbase = is_coinbase(parent);
            BC_ASSERT(!source || source.get_read_position() == minrow);
            return source;
        }

        inline bool to_data(flipper& sink) const NOEXCEPT
        {
            sink.write_little_endian<out::integer, out::size>(merge(excluded, out_fk));
            sink.write_little_endian<tx::integer, tx::size>(merge_parent(coinbase, parent_fk));
            BC_ASSERT(!sink || sink.get_write_position() == minrow);
            return sink;
        }

        inline bool operator==(const record&) const NOEXCEPT = default;

        out::integer out_fk{};
        tx::integer parent_fk{};
        bool excluded{};
        bool coinbase{};
    };

    struct get_output
      : public schema::outs
    {
        inline bool from_data(reader& source) NOEXCEPT
        {
            out_fk = to_out_fk(source.read_little_endian<out::integer, out::size>());
            return source;
        }

        out::integer out_fk{};
    };

    struct get_parent
      : public schema::outs
    {
        inline bool from_data(reader& source) NOEXCEPT
        {
            source.skip_bytes(out::size);
            const auto parent = source.read_little_endian<tx::integer, tx::size>();
            parent_fk = to_parent_fk(parent);
            coinbase = is_coinbase(parent);
            return source;
        }

        tx::integer parent_fk{};
        bool coinbase{};
    };

    struct get_excluded
      : public schema::outs
    {
        inline link count() const NOEXCEPT
        {
            return system::possible_narrow_cast<link::integer>(number);
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            excluded.clear();
            for (ix::integer index{}; index < number; ++index)
            {
                const auto merged = source.read_little_endian<out::integer,
                    out::size>();
                source.skip_bytes(tx::size);
                if (is_excluded(merged))
                    excluded.emplace_back(index, to_out_fk(merged));
            }

            return source;
        }

        // Output count is assigned by the caller, from the tx record.
        ix::integer number{};
        excluded_outputs excluded{};
    };

    struct put_ref
      : public schema::outs
    {
        inline link count() const NOEXCEPT
        {
            return system::possible_narrow_cast<link::integer>(tx_.outputs());
        }

        inline bool to_data(flipper& sink) const NOEXCEPT
        {
            using namespace system;
            const auto parent = merge_parent(coinbase, parent_fk);
            auto out_fk = output_fk;
            const auto& outs = *tx_.outputs_ptr();
            std::ranges::for_each(outs, [&](const auto& out) NOEXCEPT
            {
                sink.write_little_endian<out::integer, out::size>(merge(is_excluded(*out), out_fk));
                sink.write_little_endian<tx::integer, tx::size>(parent);

                // Calculate next corresponding output fk from serialized size.
                // (variable_size(value) + (value + script)) - value
                out_fk += (variable_size(out->value()) +
                    out->serialized_size() - sizeof(uint64_t));
            });

            BC_ASSERT(!sink || sink.get_write_position() == count() * minrow);
            return sink;
        }

        const out::integer output_fk{};
        const tx::integer parent_fk{};
        const bool coinbase{};
        const system::chain::transaction& tx_{};
    };

    struct put_view
      : public schema::outs
    {
        inline link count() const NOEXCEPT
        {
            return system::possible_narrow_cast<link::integer>(tx_.outputs());
        }

        inline bool to_data(flipper& sink) const NOEXCEPT
        {
            using namespace system;
            const auto parent = merge_parent(coinbase, parent_fk);
            auto out_fk = output_fk;
            auto stream = tx_.get_outputs_stream();
            read::bytes::fast source{ stream };

            for (size_t out{}; out < tx_.outputs(); ++out)
            {
                const auto value = source.read_8_bytes_little_endian();
                const auto bytes = source.read_size();
                const auto excluded = peek_excluded(source, bytes);
                source.skip_bytes(bytes);
                sink.write_little_endian<out::integer, out::size>(merge(excluded, out_fk));
                sink.write_little_endian<tx::integer, tx::size>(parent);
                out_fk += variable_size(value) + variable_size(bytes) + bytes;
            }

            BC_ASSERT(source);
            BC_ASSERT(!sink || sink.get_write_position() == count() * minrow);
            return sink && source;
        }

        const out::integer output_fk{};
        const tx::integer parent_fk{};
        const bool coinbase{};
        const system::chain::view::transaction& tx_;
    };
};

} // namespace table
} // namespace database
} // namespace libbitcoin

#endif
