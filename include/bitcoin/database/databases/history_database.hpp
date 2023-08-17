/**
 * Copyright (c) 2011-2023 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_DATABASE_HISTORY_DATABASE_HPP
#define LIBBITCOIN_DATABASE_HISTORY_DATABASE_HPP

#include <memory>
#include <boost/filesystem.hpp>
#include <bitcoin/system.hpp>
#include <bitcoin/database/define.hpp>
#include <bitcoin/database/memory/memory_map.hpp>
#include <bitcoin/database/primitives/record_multimap.hpp>

namespace libbitcoin {
namespace database {

struct BCD_API history_statinfo
{
    /// Number of buckets used in the hashtable.
    /// load factor = addrs / buckets
    const size_t buckets;

    /// Total number of unique addresses in the database.
    const size_t addrs;

    /// Total number of rows across all addresses.
    const size_t rows;
};

/// This is a multimap where the key is the Bitcoin address hash,
/// which returns several rows giving the history for that address.
class BCD_API history_database
{
public:
    typedef boost::filesystem::path path;
    typedef std::shared_ptr<shared_mutex> mutex_ptr;

    /// Construct the database.
    history_database(const path& lookup_filename, const path& rows_filename,
        size_t buckets, size_t expansion, mutex_ptr mutex=nullptr);

    /// Close the database (all threads must first be stopped).
    ~history_database();

    /// Initialize a new history database.
    bool create();

    /// Call before using the database.
    bool open();

    /// Call to unload the memory map.
    bool close();

    /// Add an output row to the key. If key doesn't exist it will be created.
    void add_output(const short_hash& key, const chain::output_point& outpoint,
        size_t output_height, uint64_t value);

    /// Add an input to the key. If key doesn't exist it will be created.
    void add_input(const short_hash& key, const chain::output_point& inpoint,
        size_t input_height, const chain::input_point& previous);

    /// Delete the last row that was added to key.
    bool delete_last_row(const short_hash& key);

    /// Get the output and input points associated with the address hash.
    chain::history_compact::list get(const short_hash& key, size_t limit,
        size_t from_height) const;

    /// Commit latest inserts.
    void synchronize();

    /// Flush the memory maps to disk.
    bool flush() const;

    /// Return statistical info about the database.
    history_statinfo statinfo() const;

private:
    typedef record_hash_table<short_hash> record_map;
    typedef record_multimap<short_hash> record_multiple_map;

    // The starting size of the hash table, used by create.
    const size_t initial_map_file_size_;

    /// Hash table used for start index lookup for linked list by address hash.
    memory_map lookup_file_;
    record_hash_table_header lookup_header_;
    record_manager lookup_manager_;
    record_map lookup_map_;

    /// History rows.
    memory_map rows_file_;
    record_manager rows_manager_;
    record_multiple_map rows_multimap_;
};

} // namespace database
} // namespace libbitcoin

#endif
