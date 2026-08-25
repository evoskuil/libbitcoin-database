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
#ifndef LIBBITCOIN_DATABASE_TYPES_UNSPENT_COIN_HPP
#define LIBBITCOIN_DATABASE_TYPES_UNSPENT_COIN_HPP

#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

/// A coin (unspent output) presented during a branch scan.
/// The buffers are reused across visits, copy to retain.
struct BCD_API unspent_coin
{
    /// First visited coin of the containing transaction.
    bool first{};

    /// Hash of the containing transaction.
    system::hash_digest txid{};

    /// Output index within the containing transaction.
    uint32_t index{};

    /// Confirmed height of the containing transaction.
    size_t height{};

    /// The containing transaction is coinbase.
    bool coinbase{};

    /// Output value.
    uint64_t value{};

    /// Serialized script.
    system::data_chunk script{};
};

} // namespace database
} // namespace libbitcoin

#endif
