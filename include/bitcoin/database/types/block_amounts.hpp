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
#ifndef LIBBITCOIN_DATABASE_TYPES_BLOCK_AMOUNTS_HPP
#define LIBBITCOIN_DATABASE_TYPES_BLOCK_AMOUNTS_HPP

#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

/// Coin amounts moved by a single block.
struct BCD_API block_amounts
{
    /// Total value of the prevouts spent by the block.
    uint64_t prevouts{};

    /// Total value of the coinbase outputs.
    uint64_t coinbase{};

    /// Total value of the non-coinbase outputs.
    uint64_t outputs{};

    /// Total value of the provably unspendable outputs created.
    uint64_t unspendable{};

    /// Total value of the coinbase outputs overwritten by bip30 duplication.
    uint64_t bip30{};
};

} // namespace database
} // namespace libbitcoin

#endif
