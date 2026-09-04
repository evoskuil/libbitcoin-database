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
#ifndef LIBBITCOIN_DATABASE_UNSPENT_UNSPENT_WRITER_HPP
#define LIBBITCOIN_DATABASE_UNSPENT_UNSPENT_WRITER_HPP

#include <bitcoin/database/define.hpp>
#include <bitcoin/database/types/types.hpp>

namespace libbitcoin {
namespace database {

/// The unspent output commitment element (bitcoind coin serialization).
struct unspent_writer
{
    static constexpr auto point_size = system::hash_size + sizeof(uint32_t);
    static constexpr auto code_size = sizeof(uint32_t);
    static constexpr auto value_size = sizeof(uint64_t);
    static constexpr auto fixed_size = point_size + code_size + value_size;

    /// Write the element of the coin to the sink.
    static void write(system::writer& sink, const unspent_coin& coin) NOEXCEPT;

    /// The element size of the coin.
    static size_t size(const unspent_coin& coin) NOEXCEPT;

    /// The sha256 hash of the element of the coin (muhash element).
    static hash_digest hash(const unspent_coin& coin) NOEXCEPT;

    /// Add the coin to the totals.
    static void add(unspent_totals& out, const unspent_coin& coin) NOEXCEPT;

    /// Add the totals to the totals.
    static void add(unspent_totals& out, const unspent_totals& totals) NOEXCEPT;
};

} // namespace database
} // namespace libbitcoin

#include <bitcoin/database/impl/unspent/unspent_writer.ipp>

#endif
