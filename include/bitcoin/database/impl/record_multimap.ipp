/**
 * Copyright (c) 2011-2017 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_DATABASE_RECORD_MULTIMAP_IPP
#define LIBBITCOIN_DATABASE_RECORD_MULTIMAP_IPP

#include <bitcoin/database/memory/memory.hpp>
#include <bitcoin/database/primitives/record_row.hpp>

namespace libbitcoin {
namespace database {

template <typename KeyType>
record_multimap<KeyType>::record_multimap(record_hash_table_type& map,
    record_manager& manager)
  : map_(map), manager_(manager)
{
}

template <typename KeyType>
void record_multimap<KeyType>::store(const KeyType& key,
    write_function write)
{
    // TODO: create keyless (non-template) record_row.
    const auto& todo = key;

    // Allocate and populate new unlinked row.
    row record(manager_);
    const auto begin = record.create(todo, write);

    // TODO: store does not require the update lock as it links after write.
    const auto write_index = [&](serializer<uint8_t*>& serial)
    {
        // Critical Section
        ///////////////////////////////////////////////////////////////////////
        unique_lock lock(update_mutex_);
        serial.template write_little_endian<array_index>(begin);
        ///////////////////////////////////////////////////////////////////////
    };

    // Critical Section.
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(mutex_);

    const auto old_begin = find(key);
    record.link(old_begin);

    if (old_begin == map_.not_found)
        map_.store(key, write_index);
    else
        map_.update(key, write_index);
    ///////////////////////////////////////////////////////////////////////////
}

// TODO: differentiate or consolidate map.not_found and record.not_found.
template <typename KeyType>
array_index record_multimap<KeyType>::find(const KeyType& key) const
{
    const auto begin_address = map_.find(key);

    if (!begin_address)
        return map_.not_found;

    const auto memory = REMAP_ADDRESS(begin_address);

    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    update_mutex_.lock_shared();
    const auto begin = from_little_endian_unsafe<array_index>(memory);
    update_mutex_.unlock_shared();
    ///////////////////////////////////////////////////////////////////////////

    BITCOIN_ASSERT(begin != map_.not_found);
    return begin;
}

// Unlink is not safe for concurrent write.
template <typename KeyType>
bool record_multimap<KeyType>::unlink(const KeyType& key)
{
    const auto begin = find(key);

    // No rows exist.
    if (begin == map_.not_found)
        return false;

    const row begin_item(manager_, begin);
    const auto next_index = begin_item.next_index();

    // Remove the hash table entry, which delinks the single row.
    if (next_index == map_.not_found)
        return map_.unlink(key);

    const auto write_index = [&](serializer<uint8_t*>& serial)
    {
        // Critical Section.
        ///////////////////////////////////////////////////////////////////////////
        unique_lock lock(update_mutex_);
        serial.template write_little_endian<array_index>(next_index);
        ///////////////////////////////////////////////////////////////////////////
    };

    // Update the hash table entry, which skips over the first of multiple rows.
    map_.update(key, write_index);
    return true;
}

} // namespace database
} // namespace libbitcoin

#endif
