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
#ifndef LIBBITCOIN_DATABASE_RECORD_ROW_HPP
#define LIBBITCOIN_DATABASE_RECORD_ROW_HPP

#include <bitcoin/bitcoin.hpp>
#include <bitcoin/database/define.hpp>
#include <bitcoin/database/memory/memory.hpp>
#include <bitcoin/database/primitives/record_manager.hpp>

namespace libbitcoin {
namespace database {

/**
 * A linked list with comparable key field.
 */
template <typename KeyType>
class record_row
{
public:
    typedef byte_serializer::functor write_function;

    static BC_CONSTEXPR size_t key_start = 0;
    static BC_CONSTEXPR size_t key_size = std::tuple_size<KeyType>::value;
    static BC_CONSTEXPR size_t index_size = sizeof(array_index);
    static BC_CONSTEXPR file_offset prefix_size = key_size + index_size;

    record_row(record_manager& manager, array_index index=0);

    /// Allocate unlinked item for the given key.
    array_index create(const KeyType& key, write_function write);

    /// Link allocated/populated item.
    void link(array_index next);

    /// Does this match?
    bool compare(const KeyType& key) const;

    /// The actual user data.
    memory_ptr data() const;

    /// The file offset of the user data.
    file_offset offset() const;

    /// Position of next item in the chained list.
    array_index next_index() const;

    /// Write a new next index.
    void write_next_index(array_index next);

private:
    memory_ptr raw_data(file_offset offset) const;

    array_index index_;
    record_manager& manager_;
    mutable shared_mutex mutex_;
};

} // namespace database
} // namespace libbitcoin

#include <bitcoin/database/impl/record_row.ipp>

#endif
