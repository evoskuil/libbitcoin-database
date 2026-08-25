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
#ifndef LIBBITCOIN_DATABASE_TYPES_DIFFERENCE_SET_HPP
#define LIBBITCOIN_DATABASE_TYPES_DIFFERENCE_SET_HPP

#include <array>
#include <mutex>
#include <utility>
#include <boost/unordered/unordered_flat_map.hpp>
#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

/// Multiset symmetric difference, exact and enumerable. Each presentation of
/// an element toggles its membership, so even multiplicities cancel. Elements
/// are (id, index) pairs, held as Mask-width index windows of an id, with the
/// id and window number packed into a 64 bit key. Thread safe.
template <typename Mask = uint64_t, size_t WindowBits = 16>
class difference_set
{
public:
    DELETE_COPY_MOVE(difference_set);

    using entry = std::pair<uint64_t, Mask>;
    using entries = std::vector<entry>;

    static constexpr auto key_bits = bits<uint64_t>;
    static constexpr auto mask_bits = bits<Mask>;
    static constexpr auto window_bits = WindowBits;
    static constexpr auto index_bits = system::floored_log2(mask_bits);
    static_assert(window_bits < key_bits);

    static constexpr uint64_t to_key(uint64_t id, uint32_t index) NOEXCEPT
    {
        using namespace system;
        return bit_or(shift_left(id, window_bits),
            shift_right<uint64_t>(index, index_bits));
    }

    static constexpr uint64_t to_id(uint64_t key) NOEXCEPT
    {
        return system::shift_right(key, window_bits);
    }

    static constexpr uint64_t to_index(uint64_t key) NOEXCEPT
    {
        using namespace system;
        return shift_left(bit_and(key, unmask_right<uint64_t>(window_bits)),
            index_bits);
    }

    difference_set() NOEXCEPT = default;
    ~difference_set() = default;

    /// Toggle element membership (thread safe).
    void toggle(uint64_t id, uint32_t index) NOEXCEPT;

    /// Sorted odd-multiplicity elements, emptying the set (not thread safe).
    entries drain() NOEXCEPT;

private:
    static constexpr auto shard_bits = 4_size;
    static constexpr auto index_mask =
        system::possible_narrow_cast<uint32_t>(sub1(mask_bits));

    struct shard
    {
        std::mutex mutex{};
        boost::unordered_flat_map<uint64_t, Mask> map{};
    };

    std::array<shard, system::power2(shard_bits)> shards_{};
};

} // namespace database
} // namespace libbitcoin

#define TEMPLATE template <typename Mask, size_t WindowBits>
#define CLASS difference_set<Mask, WindowBits>

#include <bitcoin/database/impl/types/difference_set.ipp>

#undef CLASS
#undef TEMPLATE

#endif
