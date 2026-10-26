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
#ifndef LIBBITCOIN_DATABASE_PRIMITIVES_ARRAYMAP_HPP
#define LIBBITCOIN_DATABASE_PRIMITIVES_ARRAYMAP_HPP

#include <bitcoin/database/define.hpp>
#include <bitcoin/database/memory/memory.hpp>
#include <bitcoin/database/primitives/arrayhead.hpp>
#include <bitcoin/database/primitives/iterator.hpp>
#include <bitcoin/database/primitives/linkage.hpp>
#include <bitcoin/database/primitives/body.hpp>

namespace libbitcoin {
namespace database {
    
/// Caution: iterator/reader/finalizer hold body remap lock until disposed.
/// These handles should be used for serialization and immediately disposed.
/// Readers and writers are always prepositioned at data, and are limited to
/// the extent the record/slab size is known (limit can always be removed).
/// Streams are always initialized from first element byte up to file limit.
/// A based map keys its head less the base: keys below it are terminal, and
/// the drop empties the table and advances the base.
template <class Link, size_t RowSize, bool Align, bool Based = false>
class arraymap
{
public:
    DELETE_COPY_MOVE_DESTRUCT(arraymap);

    using link = Link;

    arraymap(storage& header, storage& body, const Link& buckets) NOEXCEPT;

    /// Setup, not thread safe.
    /// -----------------------------------------------------------------------

    bool create() NOEXCEPT;
    bool close() NOEXCEPT;
    bool clear() NOEXCEPT;
    bool backup(bool prune=false) NOEXCEPT;
    bool restore() NOEXCEPT;
    bool verify() const NOEXCEPT;

    /// Sizing.
    /// -----------------------------------------------------------------------

    /// The instance is enabled (has buckets).
    bool enabled() const NOEXCEPT;

    /// Hash table bucket count.
    size_t buckets() const NOEXCEPT;

    /// Head file bytes.
    size_t head_size() const NOEXCEPT;

    /// Body file bytes.
    size_t body_size() const NOEXCEPT;

    /// Capacity of body in bytes.
    size_t capacity() const NOEXCEPT;

    /// Count of body records (or bytes if slab).
    Link count() const NOEXCEPT;

    /// Increase count as necessary to specified.
    bool expand(const Link& count) NOEXCEPT;

    /// The key of the first bucket (zero unless based).
    Link base() const NOEXCEPT;

    /// Empty the table and advance the base (based only, not thread safe).
    bool drop(const Link& base) NOEXCEPT;

    /// Errors.
    /// -----------------------------------------------------------------------

    /// Get the fault condition.
    code get_fault() const NOEXCEPT;

    /// Get the space required to clear the disk full condition.
    size_t get_space() const NOEXCEPT;

    /// Resume from disk full condition.
    code reload() NOEXCEPT;

    /// Query interface, iterator is not thread safe.
    /// -----------------------------------------------------------------------

    /// Allocate count records or slab bytes at returned link (follow with put).
    inline Link allocate(const Link& count) NOEXCEPT;

    /// Return ptr for batch processing, holds shared lock on storage remap.
    inline memory get_memory() const NOEXCEPT;

    /// Head accessor (shared lock), for at(head, key) over many keys.
    inline memory get_head_memory() const NOEXCEPT;

    /// Return element link at key, terminal if not found/error (unverified).
    inline Link at(size_t key) const NOEXCEPT;

    /// Return element link at key using get_head_memory() ptr.
    inline Link at(const memory& head, size_t key) const NOEXCEPT;

    /// True if an instance of object with key exists.
    inline Link exists(size_t key) const NOEXCEPT;

    /// Get first element matching key, false if not found/error (unverified).
    template <typename Element, if_equal<Element::size, RowSize> = true>
    inline bool at(size_t key, Element& element) const NOEXCEPT;

    /// Get element at link, false if deserialize error.
    template <typename Element, if_equal<Element::size, RowSize> = true>
    inline bool get(const Link& link, Element& element) const NOEXCEPT;

    /// Get element at link using get_memory() ptr, false if deserialize error.
    template <typename Element, if_equal<Element::size, RowSize> = true>
    inline bool get(const memory& ptr, const Link& link,
        Element& element) const NOEXCEPT;

    /// Allocate, set, commit element to key (each record to its own key).
    /// Expands table AND HEADER as necessary.
    template <typename Element, if_equal<Element::size, RowSize> = true>
    bool put(size_t key, const Element& element) NOEXCEPT;

    /// Set/commit allocated element at link to key, using get_memory() ptr.
    /// Each record is committed to its own key, from key.
    template <typename Element, if_equal<Element::size, RowSize> = true>
    bool put(const memory& ptr, const Link& link, size_t key,
        const Element& element) NOEXCEPT;

private:
    static constexpr auto is_slab = (RowSize == max_size_t);
    using head = database::arrayhead<Link, Align, Based>;
    using body = database::body<Link, system::data_array<zero>, RowSize>;

    // Thread safe (index/top/push).
    // Not thread safe (create/open/close/backup/restore).
    head head_;

    // Thread safe.
    body body_;
};

template <typename Schema>
using array_map = arraymap<typename Schema::link, Schema::size, Schema::align>;

template <typename Schema>
using based_array_map = arraymap<typename Schema::link, Schema::size,
    Schema::align, true>;

} // namespace database
} // namespace libbitcoin

#define TEMPLATE template <class Link, size_t RowSize, bool Align, bool Based>
#define CLASS arraymap<Link, RowSize, Align, Based>
#define ELEMENT_CONSTRAINT template <class Element, \
    if_equal<Element::size, RowSize>>

#include <bitcoin/database/impl/primitives/arraymap.ipp>

#undef CLASS
#undef TEMPLATE

#endif
