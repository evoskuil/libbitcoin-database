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
#ifndef LIBBITCOIN_DATABASE_TABLES_ARCHIVES_OUTPUT_HPP
#define LIBBITCOIN_DATABASE_TABLES_ARCHIVES_OUTPUT_HPP

#include <bitcoin/database/define.hpp>
#include <bitcoin/database/memory/memory.hpp>
#include <bitcoin/database/primitives/primitives.hpp>
#include <bitcoin/database/tables/schema.hpp>

namespace libbitcoin {
namespace database {
namespace table {
    
BC_PUSH_WARNING(NO_NEW_OR_DELETE)
BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

/// Output is a blob (set of non-searchable slabs).
/// Output can be obtained by fk navigation (eg from tx/index). 
struct output
  : public no_map<schema::output>
{
    using no_map<schema::output>::nomap;

    struct slab
      : public schema::output
    {
        inline link count() const NOEXCEPT
        {
            return system::possible_narrow_cast<link::integer>(
                variable_size(value) + script.serialized_size(true));
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            using namespace system;
            value = source.read_variable();
            script = chain::script{ source, true };
            BC_ASSERT(!source || source.get_read_position() == count());
            return source;
        }

        inline bool to_data(flipper& sink) const NOEXCEPT
        {
            sink.write_variable(value);
            script.to_data(sink, true);
            BC_ASSERT(!sink || sink.get_write_position() == count());
            return sink;
        }

        inline bool operator==(const slab&) const NOEXCEPT = default;

        uint64_t value{};
        system::chain::script script{};
    };

    // Cannot use output{ sink } because database output.value is varint.
    struct only
      : public schema::output
    {
        inline link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            using namespace system;

            // Required step to control order of stream read.
            const auto prefix = source.read_variable();
            output = std::make_shared<const chain::output>
            (
                prefix,
                std::make_shared<const chain::script>(source, true)
            );

            return source;
        }

        system::chain::output::cptr output{};
    };

    struct get_script
      : public schema::output
    {
        inline link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            using namespace system;
            source.skip_variable();
            script = std::make_shared<const chain::script>(source, true);
            return source;
        }

        system::chain::script::cptr script{};
    };

    /// Unstreamed, hashes the script in place and compares to the key.
    struct match_script_hash
      : public schema::output
    {
        inline bool from_data(memory::iterator start) NOEXCEPT
        {
            using namespace system;

            // Skip value, and read the script size.
            const auto* position = start;
            unsafe_from_variable(position);
            const auto scrypt_size = unsafe_from_variable(position);
            const auto bytes = possible_narrow_cast<size_t>(scrypt_size);
            match = (key == accumulator<sha256>::hash(bytes, position));
            return true;
        }

        const system::hash_digest& key;
        bool match{};
    };

    /// Unstreamed, hashes the script in place.
    struct get_script_hash
      : public schema::output
    {
        inline bool from_data(memory::iterator start) NOEXCEPT
        {
            using namespace system;

            // Skip value, and read the script size.
            const auto* position = start;
            unsafe_from_variable(position);
            const auto script_size = unsafe_from_variable(position);
            const auto bytes = possible_narrow_cast<size_t>(script_size);
            hash = accumulator<sha256>::hash(bytes, position);
            return true;
        }

        system::hash_digest hash{};
    };

    /// Unstreamed, writes full output directly to the sink.
    struct write_script
      : public schema::output
    {
        inline bool from_data(memory::iterator start) NOEXCEPT
        {
            using namespace system;

            // Read the value and script size.
            const auto* position = start;
            value = unsafe_from_variable(position);
            const auto script_size = unsafe_from_variable(position);
            length = possible_narrow_cast<size_t>(script_size);

            sink.write_8_bytes_little_endian(value);
            sink.write_variable(length);
            sink.write_bytes(position, length);
            return true;
        }

        bytewriter& sink;
        uint64_t value{};
        size_t length{};
    };

    /// Reuse an instance across gets to amortize the script allocation.
    struct get_coin
      : public schema::output
    {
        inline link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            using namespace system;
            value = source.read_variable();
            script.resize(possible_narrow_cast<size_t>(
                source.read_variable()));
            source.read_bytes(script.data(), script.size());
            return source;
        }

        uint64_t value{};
        system::data_chunk script{};
    };

    struct get_spendable
      : public schema::output
    {
        inline link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            using namespace system;
            value = source.read_variable();
            script_size = possible_narrow_cast<size_t>(
                source.read_variable());

            // bitcoind's unspent set exclusion (op_return lead or oversized).
            constexpr auto op_return = to_value(chain::opcode::op_return);
            unspendable = (script_size > chain::max_script_size) ||
                (!is_zero(script_size) && (source.read_byte() == op_return));
            return source;
        }

        uint64_t value{};
        size_t script_size{};
        bool unspendable{};
    };

    struct get_value_size
      : public schema::output
    {
        inline link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            using namespace system;
            value = source.read_variable();
            script_size = possible_narrow_cast<size_t>(
                source.read_variable());
            return source;
        }

        uint64_t value{};
        size_t script_size{};
    };

    struct get_value
      : public schema::output
    {
        inline link count() const NOEXCEPT
        {
            BC_ASSERT(false);
            return {};
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            value = source.read_variable();
            return source;
        }

        uint64_t value{};
    };

    struct put_ref
      : public schema::output
    {
        inline link count() const NOEXCEPT
        {
            using namespace system;
            const auto& outs = *tx_.outputs_ptr();
            const auto outputs = std::accumulate(outs.cbegin(), outs.cend(),
                zero, [](size_t total, const auto& out) NOEXCEPT
                {
                    const auto output_size = out->serialized_size();
                    return total + variable_size(out->value()) + output_size;
                });

            // Converts value from fixed size wire encoding to variable.
            // (variable_size(value) + (value + script)) - value
            return possible_narrow_cast<link::integer>(outputs -
                (outs.size() * sizeof(uint64_t)));
        }

        inline bool to_data(flipper& sink) const NOEXCEPT
        {
            const auto& outs = *tx_.outputs_ptr();
            std::ranges::for_each(outs, [&](const auto& out) NOEXCEPT
            {
                sink.write_variable(out->value());
                out->script().to_data(sink, true);
            });

            BC_ASSERT(!sink || sink.get_write_position() == count());
            return sink;
        }

        const system::chain::transaction& tx_{};
    };

    struct put_view
      : public schema::output
    {
        inline link count() const NOEXCEPT
        {
            using namespace system;
            return possible_narrow_cast<link::integer>(
                tx_.output_table_size());
        }

        inline bool to_data(flipper& sink) const NOEXCEPT
        {
            using namespace system;
            auto stream = tx_.get_outputs_stream();
            read::bytes::fast source{ stream };

            for (size_t out{}; out < tx_.outputs(); ++out)
            {
                // tx view output writer not used due to variable value.
                sink.write_variable(source.read_8_bytes_little_endian());
                const auto bytes = source.read_size();
                sink.write_variable(bytes);
                sink.write_bytes(source.read_bytes(bytes));
            }

            BC_ASSERT(source);
            BC_ASSERT(!sink || sink.get_write_position() == count());
            return sink;
        }

        const system::chain::view::transaction& tx_;
    };

    struct wire_script
      : public schema::output
    {
        inline bool from_data(reader& source) NOEXCEPT
        {
            // value (translates from variable to fixed width)
            sink.write_8_bytes_little_endian(source.read_variable());

            // script (prefixed)
            const auto length = source.read_size();
            sink.write_variable(length);
            sink.write_bytes(source, length);
            return source;
        }

        bytewriter& sink;
    };
};

BC_POP_WARNING()
BC_POP_WARNING()

} // namespace table
} // namespace database
} // namespace libbitcoin

#endif
