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
#include <bitcoin/database/primitives/linked_list_iterable.hpp>
#include <bitcoin/database/primitives/record_manager.hpp>

namespace libbitcoin {
namespace database {

// static
template <typename Key, typename Index, typename Link>
size_t recordset_hash_table<Key, Index, Link>::size(size_t value_size)
{
    return sizeof(Link) + value_size;
}

template <typename Key, typename Index, typename Link>
recordset_hash_table<Key, Index, Link>::recordset_hash_table(
    record_hash_table<Key, Index, Link>& map,
    record_manager<Link>& manager)
  : map_(map), manager_(manager)
{
}

template <typename Key, typename Index, typename Link>
void recordset_hash_table<Key, Index, Link>::store(const Key& key,
    write_function write)
{
    // Allocate and populate new unlinked row.
    row_manager record(manager_);
    const auto begin = record.create(write);

    // Critical Section.
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(create_mutex_);

    const auto roots = find(key);

    if (roots.empty())
    {
        record.link(row_manager::not_found);

        map_.store(key, [=](serializer<uint8_t*>& serial)
        {
            //*****************************************************************
            serial.template write_little_endian<Link>(begin);
            //*****************************************************************
        });
    }
    else
    {
        record.link(roots.front());

        map_.update(key, [=](serializer<uint8_t*>& serial)
        {
            // Critical Section
            ///////////////////////////////////////////////////////////////////
            unique_lock lock(update_mutex_);
            serial.template write_little_endian<Link>(begin);
            ///////////////////////////////////////////////////////////////////
        });
    }
    ///////////////////////////////////////////////////////////////////////////
}

template <typename Key, typename Index, typename Link>
linked_list_iterable<record_manager<Link>, Link>
recordset_hash_table<Key, Index, Link>::find(const Key& key) const
{
    const auto begin_address = map_.find(key);

    if (!begin_address)
        return { manager_, row_manager::not_found };

    const auto memory = begin_address->buffer();

    // Critical Section.
    ///////////////////////////////////////////////////////////////////////////
    shared_lock lock(update_mutex_);
    return { manager_, from_little_endian_unsafe<Link>(memory) };
    ///////////////////////////////////////////////////////////////////////////
}

/// Get a remap safe address pointer to the indexed data.
template <typename Key, typename Index, typename Link>
memory_ptr recordset_hash_table<Key, Index, Link>::get(Link index) const
{
    return row_manager(manager_, index).data();
}

// Unlink is not safe for concurrent write.
template <typename Key, typename Index, typename Link>
bool recordset_hash_table<Key, Index, Link>::unlink(const Key& key)
{
    const auto roots = find(key);

    if (roots.empty())
        return false;

    row_manager record(manager_, roots.front());
    const auto next_index = record.next();

    // Remove the hash table entry, which delinks the single row.
    if (next_index == row_manager::not_found)
        return map_.unlink(key);

    // Update the hash table entry, which skips the first of multiple rows.
    map_.update(key, [&](serializer<uint8_t*>& serial)
    {
        // Critical Section.
        ///////////////////////////////////////////////////////////////////////
        unique_lock lock(update_mutex_);
        serial.template write_little_endian<Link>(next_index);
        ///////////////////////////////////////////////////////////////////////
    });

    return true;
}

} // namespace database
} // namespace libbitcoin

#endif