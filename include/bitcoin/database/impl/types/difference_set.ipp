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
#ifndef LIBBITCOIN_DATABASE_TYPES_DIFFERENCE_SET_IPP
#define LIBBITCOIN_DATABASE_TYPES_DIFFERENCE_SET_IPP

#include <algorithm>
#include <mutex>
#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

TEMPLATE
void CLASS::toggle(uint64_t id, uint32_t index) NOEXCEPT
{
    using namespace system;
    constexpr auto golden = 0x9e3779b97f4a7c15_u64;
    const auto key = to_key(id, index);
    auto& shard = shards_[shift_right(key * golden, key_bits - shard_bits)];
    const std::lock_guard<std::mutex> lock{ shard.mutex };
    auto& mask = shard.map[key];
    mask ^= bit_right<Mask>(bit_and(index, index_mask));
    if (is_zero(mask))
        shard.map.erase(key);
}

TEMPLATE
typename CLASS::entries CLASS::drain() NOEXCEPT
{
    size_t count{};
    for (const auto& shard: shards_)
        count += shard.map.size();

    entries out{};
    out.reserve(count);
    for (auto& shard: shards_)
    {
        for (const auto& entry: shard.map)
            out.emplace_back(entry.first, entry.second);

        shard.map.clear();
    }

    std::sort(out.begin(), out.end());
    return out;
}

BC_POP_WARNING()

} // namespace database
} // namespace libbitcoin

#endif
