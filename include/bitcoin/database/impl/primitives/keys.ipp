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
#ifndef LIBBITCOIN_DATABASE_PRIMITIVES_KEYS_IPP
#define LIBBITCOIN_DATABASE_PRIMITIVES_KEYS_IPP

#include <algorithm>
#include <bitcoin/database.hpp>

namespace libbitcoin {
namespace database {
namespace keys {

INLINE constexpr uint64_t fnv1a_combine(uint64_t left, uint64_t right)
{
    using namespace system;
    constexpr uint64_t fnv_prime = 0x100000001b3;
    constexpr uint64_t fnv_offset = 0xcbf29ce484222325;

    auto hash = fnv_offset;
    hash = bit_xor(hash, left);
    hash *= fnv_prime;
    hash = bit_xor(hash, right);
    hash *= fnv_prime;
    return hash;
}

template <class Key>
INLINE constexpr size_t size() NOEXCEPT
{
    using namespace system;
    if constexpr (is_same_type<Key, chain::point>)
    {
        // Index is truncated to three bytes.
        return sub1(chain::point::serialized_size());
    }
    else if constexpr (is_std_array<Key>)
    {
        return array_count<Key>;
    }
}

template <class Key, class Integral>
INLINE Integral bucket(const Key& key, Integral buckets) NOEXCEPT
{
    using namespace system;
    if constexpr (is_same_type<Key, chain::point>)
    {
        // If and only if coinbase the bucket is zero.
        if (key.index() == chain::point::null_index)
            return { 0 };

        // Push other zeros into bucket one.
        // Cast after modulo to avoid amplifying index weakness in lower bits.
        const auto bucket = possible_narrow_cast<Integral>(hash(key) % buckets);
        return is_zero(bucket) ? Integral{ 1 } : bucket;
    }
    else
    {
        return possible_narrow_cast<Integral>(hash(key)) % buckets;
    }
}

template <class Key>
INLINE uint64_t hash(const Key& key) NOEXCEPT
{
    using namespace system;
    if constexpr (is_same_type<Key, chain::point>)
    {
        // Both produce an 85% Poisson distribution.
        return fnv1a_combine(hash(key.hash()), key.index());
        ////return bit_xor(hash(key.hash()), shift_left<uint64_t>(key.index()));
    }
    else if constexpr (is_std_array<Key>)
    {
        // Produces an almost perfect 100% Poisson distribution for sha256.
        // Assumes sufficient uniqueness in low order bytes (ok for all).
        // sequentially-valued keys should have no more buckets than values.
        constexpr auto bytes = std::min(size<Key>(), sizeof(uint64_t));

        uint64_t hash{};
        std::copy_n(key.begin(), bytes, byte_cast(hash).begin());
        return hash;
    }
}

template <class Key>
INLINE uint64_t thumb(const Key& key) NOEXCEPT
{
    using namespace system;
    if constexpr (is_same_type<Key, system::chain::point>)
    {
        // spread point.index across point.hash extraction, as otherwise the
        // unlikely bucket collisions of points of the same hash will not be
        // differentiated by filters. This has a very small impact on false
        // positives (-5,789 out of ~2.6B) but w/o material computational cost.
        return fnv1a_combine(thumb(key.hash()), key.index());
        ////return thumb(key.hash());
    }
    else if constexpr (is_std_array<Key>)
    {
        // Assumes sufficient uniqueness in second-low order bytes (ok for all).
        // The thumb(key) starts at the next byte following hash(key).
        // If key is to short then thumb aligns to the end of the key.
        // This is intended to minimize overlap to the extent possible, while
        // also avoiding the high order bits in the case of block hashes.
        constexpr auto bytes = std::min(size<Key>(), sizeof(uint64_t));
        constexpr auto offset = std::min(size<Key>() - bytes, bytes);

        uint64_t hash{};
        const auto start = std::next(key.begin(), offset);
        std::copy_n(start, bytes, byte_cast(hash).begin());
        return hash;
    }
}

template <class Key>
INLINE void write(writer& sink, const Key& key) NOEXCEPT
{
    using namespace system;
    if constexpr (is_same_type<Key, chain::point>)
    {
        sink.write_bytes(key.hash());
        sink.write_3_bytes_little_endian(key.index());
    }
    else if constexpr (is_std_array<Key>)
    {
        sink.write_bytes(key);
    }
}

template <class Array, class Key>
INLINE bool compare(const Array& bytes, const Key& key) NOEXCEPT
{
    using namespace system;
    static_assert(size<Key>() <= array_count<Array>);
    if constexpr (is_same_type<Key, chain::point>)
    {
        // Index is truncated to three bytes.
        const auto index = key.index();
        return compare(array_cast<uint8_t, hash_size>(bytes), key.hash())
            && bytes.at(hash_size + 0) == byte<0>(index)
            && bytes.at(hash_size + 1) == byte<1>(index)
            && bytes.at(hash_size + 2) == byte<2>(index);
    }
    else if constexpr (is_std_array<Key>)
    {
        return std::equal(bytes.begin(), bytes.end(), key.begin());
    }
}

} // namespace keys
} // namespace database
} // namespace libbitcoin

#endif
