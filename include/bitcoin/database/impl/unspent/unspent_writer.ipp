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
#ifndef LIBBITCOIN_DATABASE_UNSPENT_UNSPENT_WRITER_IPP
#define LIBBITCOIN_DATABASE_UNSPENT_UNSPENT_WRITER_IPP

#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

inline void unspent_writer::write(system::writer& sink,
    const unspent_coin& coin) NOEXCEPT
{
    using namespace system;
    const auto coinbase = to_int<uint32_t>(coin.coinbase);
    const auto height = possible_narrow_cast<uint32_t>(coin.height);

    sink.write_bytes(coin.out.point().hash());
    sink.write_4_bytes_little_endian(coin.out.point().index());
    sink.write_4_bytes_little_endian(bit_or(shift_left(height), coinbase));
}

inline size_t unspent_writer::size(size_t script) NOEXCEPT
{
    return fixed_size + variable_size(script) + script;
}

inline void unspent_writer::add(unspent_totals& out, bool first,
    uint64_t value, size_t script) NOEXCEPT
{
    if (first)
        ++out.transactions;

    ++out.outputs;
    out.value += value;
    out.script_bytes += script;
    out.coin_bytes += size(script);
}

inline void unspent_writer::add(unspent_totals& out,
    const unspent_coin& coin) NOEXCEPT
{
    add(out, coin.first, coin.out.value(), coin.script.size());
}

inline void unspent_writer::add(unspent_totals& out,
    const unspent_totals& totals) NOEXCEPT
{
    out.transactions += totals.transactions;
    out.outputs += totals.outputs;
    out.value += totals.value;
    out.script_bytes += totals.script_bytes;
    out.coin_bytes += totals.coin_bytes;
}

} // namespace database
} // namespace libbitcoin

#endif
