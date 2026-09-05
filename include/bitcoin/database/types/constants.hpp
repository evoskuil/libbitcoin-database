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
#ifndef LIBBITCOIN_DATABASE_TYPES_CONSTANTS_HPP
#define LIBBITCOIN_DATABASE_TYPES_CONSTANTS_HPP

#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

/// The two bip30 exceptions, coinbases duplicating (overwriting) an original.
struct bip30
{
    static constexpr size_t first_original = 91722;
    static constexpr size_t first_exception = 91880;
    static constexpr size_t second_original = 91812;
    static constexpr size_t second_exception = 91842;

    static inline const system::chain::checkpoint first
    {
        "00000000000743f190a18c5577a3c2d2a1f610ae9601ac046a38084ccb7cd721",
        first_exception
    };

    static inline const system::chain::checkpoint second
    {
        "00000000000a4d0a398161ffc163c503763b1f4360639393e0e4c8e300e0caec",
        second_exception
    };
};

} // namespace database
} // namespace libbitcoin

#endif
