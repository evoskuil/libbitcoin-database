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
#ifndef LIBBITCOIN_DATABASE_PRIMITIVES_ARRAYHEAD_HPP
#define LIBBITCOIN_DATABASE_PRIMITIVES_ARRAYHEAD_HPP

#include <atomic>
#include <shared_mutex>
#include <bitcoin/system.hpp>
#include <bitcoin/database/define.hpp>
#include <bitcoin/database/primitives/manager.hpp>

namespace libbitcoin {
namespace database {

/// Dynamically expanding array map header.
template <class Link, bool Align>
class arrayhead
{
public:
    DEFAULT_COPY_MOVE_DESTRUCT(arrayhead);

    using bytes = typename Link::bytes;

    /// An array head is disabled if it has one or less buckets.
    arrayhead(storage& head, const Link& buckets) NOEXCEPT;

    /// Sizing is dynamic (thread safe).
    inline size_t size() const NOEXCEPT;
    inline size_t buckets() const NOEXCEPT;

    /// Configure initial buckets to zero to disable the table.
    bool enabled() const NOEXCEPT;

    /// Create from empty head file (not thread safe).
    bool create() NOEXCEPT;

    /// Clear the existing index of all links.
    bool clear() NOEXCEPT;

    /// False if head file size incorrect (not thread safe).
    bool verify() const NOEXCEPT;

    /// Unsafe if verify false (not thread safe).
    bool get_body_count(Link& count) const NOEXCEPT;
    bool set_body_count(const Link& count) NOEXCEPT;

    /// Convert natural key to head bucket index (unvalidated).
    inline Link index(size_t key) const NOEXCEPT;

    /// Unsafe if verify false.
    Link at(size_t key) const NOEXCEPT;

    /// Assign link value to bucket index.
    bool push(const Link& link, const Link& index) NOEXCEPT;

private:
    using link = Link::integer;
    using body = manager<Link, system::data_array<zero>, Link::size>;
    static_assert(std::atomic<link>::is_always_lock_free);
    static_assert(is_nonzero(Link::size));
    static constexpr auto bucket_size = Align ? sizeof(link) : Link::size;
    static constexpr bool aligned = (bucket_size == sizeof(link));

    template <size_t Bytes>
    INLINE static auto& to_array(memory::iterator it) NOEXCEPT
    {
        return system::unsafe_array_cast<uint8_t, Bytes>(it);
    }

    INLINE static constexpr Link position_to_link(size_t position) NOEXCEPT
    {
        using namespace system;
        const auto offset = floored_divide(position, bucket_size);
        return possible_narrow_cast<link>(floored_subtract(offset, one));
    }

    // Byte offset of bucket index within head file.
    // [body_size][[bucket[0]...bucket[buckets-1]]]
    INLINE static constexpr size_t link_to_position(const Link& index) NOEXCEPT
    {
        using namespace system;
        BC_ASSERT(!is_multiply_overflow<size_t>(index, bucket_size));
        BC_ASSERT(!is_add_overflow(bucket_size, index * bucket_size));
        return possible_narrow_cast<size_t>(add1(index) * bucket_size);
    }

    // These are thread safe.
    storage& file_;
    const Link initial_buckets_;
    mutable std::shared_mutex mutex_{};
};

} // namespace database
} // namespace libbitcoin

#define TEMPLATE template <class Link, bool Align>
#define CLASS arrayhead<Link, Align>

#include <bitcoin/database/impl/primitives/arrayhead.ipp>

#undef CLASS
#undef TEMPLATE

#endif
