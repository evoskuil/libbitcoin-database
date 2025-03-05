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
#ifndef LIBBITCOIN_DATABASE_TABLES_OPTIONALS_BUFFER_HPP
#define LIBBITCOIN_DATABASE_TABLES_OPTIONALS_BUFFER_HPP

#include <utility>
#include <bitcoin/system.hpp>
#include <bitcoin/database/define.hpp>
#include <bitcoin/database/primitives/primitives.hpp>
#include <bitcoin/database/tables/schema.hpp>

namespace libbitcoin {
namespace database {
namespace table {

/// prevout is an array map index of previous outputs by block link.
/// The coinbase flag is merged into the tx field, reducing it's domain.
/// Masking is from the right in order to accomodate integral tx domain.
struct prevout
  : public array_map<schema::prevout>
{
    using tx = linkage<schema::tx>;
    using ix = linkage<schema::index>;
    using pt = linkage<schema::ins_>;
    using header = linkage<schema::block>;
    using array_map<schema::prevout>::arraymap;
    static constexpr size_t offset = sub1(to_bits(tx::size));

    struct slab_put_ref
      : public schema::prevout
    {
        inline link count() const NOEXCEPT
        {
            // TODO: assert overflow.
            using namespace system;
            const auto& txs = *block.transactions_ptr();
            BC_ASSERT_MSG(!is_zero(txs.size()), "empty block");

            const auto base = variable_size(sub1(txs.size()));
            return std::accumulate(std::next(txs.begin()), txs.end(), base,
                [](size_t total, const auto& tx) NOEXCEPT
                {
                    total += (variable_size(tx->version()) +
                        variable_size(tx->inputs()));

                    const auto& ins = *tx->inputs_ptr();
                    return std::accumulate(ins.begin(), ins.end(), total,
                        [](size_t sum, const auto& in) NOEXCEPT
                        {
                            return sum + tx::size + sizeof(uint32_t) +
                                variable_size(in->point().index());
                        });
                });
        }

        static constexpr tx::integer merge(bool coinbase,
            tx::integer output_tx_fk) NOEXCEPT
        {
            using namespace system;
            BC_ASSERT_MSG(!get_right(output_tx_fk, offset), "overflow");
            return set_right(output_tx_fk, offset, coinbase);
        }

        inline bool to_data(finalizer& sink) const NOEXCEPT
        {
            using namespace system;
            const auto& txs = *block.transactions_ptr();
            BC_ASSERT_MSG(!is_zero(txs.size()), "empty block");

            // Write block section heading (spending tx count).
            sink.write_variable(sub1(txs.size()));

            std::for_each(std::next(txs.begin()), txs.end(),
                [&](const auto& tx) NOEXCEPT
                {
                    // Write tx section heading (tx input count and version).
                    sink.write_variable(tx->version());
                    sink.write_variable(tx->inputs());

                    const auto& ins = *tx->inputs_ptr();
                    return std::for_each(ins.begin(), ins.end(),
                        [&](const auto& in) NOEXCEPT
                        {
                            // Sets terminal sentinel for block-internal spends.
                            const auto value = in->metadata.inside ? tx::terminal :
                                merge(in->metadata.coinbase, in->metadata.parent);

                            sink.write_little_endian<tx::integer, tx::size>(value);
                            sink.write_variable(in->point().index());
                            sink.write_little_endian<uint32_t>(in->sequence());
                        });
                });

            BC_ASSERT(!sink || (sink.get_write_position() == count()));
            return sink;
        }

        const system::chain::block& block{};
    };

    struct slab_get
      : public schema::prevout
    {
        static inline bool coinbase(tx::integer spend) NOEXCEPT
        {
            // Inside are always reflected as coinbase.
            return system::get_right(spend, offset);
        }

        static inline tx::integer output_tx_fk(tx::integer spend) NOEXCEPT
        {
            // Inside spends are mapped to terminal.
            using namespace system;
            return spend == tx::terminal ? spend : set_right(spend, offset, false);
        }

        inline bool from_data(reader& source) NOEXCEPT
        {
            using namespace system;

            // Read block section heading (spending tx count).
            sets.resize(possible_narrow_cast<size_t>(source.read_variable()));

            std::for_each(sets.begin(), sets.end(), [&](auto& set) NOEXCEPT
            {
                // Read tx section heading (tx input count and version).
                set.version = possible_narrow_cast<uint32_t>(source.read_variable());
                set.points.resize(possible_narrow_cast<size_t>(source.read_variable()));
                std::for_each(set.points.begin(), set.points.end(),[&](auto& value) NOEXCEPT
                {
                    const auto tx = source.read_little_endian<tx::integer, tx::size>();
                    value.tx = output_tx_fk(tx);
                    value.coinbase = coinbase(tx);
                    value.index = system::narrow_cast<uint32_t>(source.read_variable());
                    value.sequence = source.read_little_endian<uint32_t>();
                });
            });

            ////BC_ASSERT(!source || source.get_read_position() == count());
            return source;
        }

        point_sets sets{};
    };
};

} // namespace table
} // namespace database
} // namespace libbitcoin

#endif
