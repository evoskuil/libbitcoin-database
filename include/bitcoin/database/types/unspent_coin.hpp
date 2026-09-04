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
#include <bitcoin/database/types/type.hpp>

namespace libbitcoin {
namespace database {

/// An unspent output, used by bitcoind.
struct BCD_API unspent_coin
{
    /// First coin of the containing transaction, in presentation order.
    bool first{};

    /// The output (value carried).
    outpoint out{};

    /// Confirmed height of the containing transaction.
    size_t height{};

    /// The containing transaction is coinbase.
    bool coinbase{};

    /// Serialized script.
    system::data_chunk script{};
};

using unspent_coins = std::vector<unspent_coin>;

} // namespace database
} // namespace libbitcoin

#endif
