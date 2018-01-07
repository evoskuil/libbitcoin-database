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
#ifndef LIBBITCOIN_DATABASE_SLAB_MANAGER_IPP
#define LIBBITCOIN_DATABASE_SLAB_MANAGER_IPP

#include <cstddef>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/database/memory/memory.hpp>
#include <bitcoin/database/memory/storage.hpp>

/// -- file --
/// [ header ]
/// [ payload_size ] (includes self)
/// [ payload ]

/// -- header (hash table) --
/// [ count  ]
/// [ bucket ]
/// ...
/// [ bucket ]

/// -- payload (variable size records) --
/// [ slab ]
/// ...
/// [ slab ]

namespace libbitcoin {
namespace database {

// TODO: guard against overflows.

template <typename LinkType>
slab_manager<LinkType>::slab_manager(storage& file, size_t header_size)
  : file_(file),
    header_size_(header_size),
    payload_size_(sizeof(LinkType))
{
}

template <typename LinkType>
bool slab_manager<LinkType>::create()
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(mutex_);

    // Existing slabs size is incorrect for new file.
    if (payload_size_ != sizeof(LinkType))
        return false;

    // This currently throws if there is insufficient space.
    file_.resize(header_size_ + payload_size_);

    write_size();
    return true;
    ///////////////////////////////////////////////////////////////////////////
}

template <typename LinkType>
bool slab_manager<LinkType>::start()
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(mutex_);

    read_size();
    const auto minimum = header_size_ + payload_size_;

    // Slabs size exceeds file size.
    return minimum <= file_.size();
    ///////////////////////////////////////////////////////////////////////////
}

template <typename LinkType>
void slab_manager<LinkType>::sync() const
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(mutex_);

    write_size();
    ///////////////////////////////////////////////////////////////////////////
}

// protected
template <typename LinkType>
size_t slab_manager<LinkType>::payload_size() const
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    shared_lock lock(mutex_);

    return payload_size_;
    ///////////////////////////////////////////////////////////////////////////
}

// Return is offset by header but not size storage (embedded in data files).
// The file is thread safe, the critical section is to protect payload_size_.
template <typename LinkType>
LinkType slab_manager<LinkType>::new_slab(size_t size)
{
    // Critical Section
    ///////////////////////////////////////////////////////////////////////////
    unique_lock lock(mutex_);

    // Always write after the last slab.
    const auto next_slab_position = payload_size_;

    const size_t required_size = header_size_ + payload_size_ + size;

    if (!file_.reserve(required_size))
    {
        // TODO: return failure sentinel.
    }

    payload_size_ += size;

    return next_slab_position;
    ///////////////////////////////////////////////////////////////////////////
}

// Position is offset by header but not size storage (embedded in data files).
template <typename LinkType>
memory_ptr slab_manager<LinkType>::get(LinkType position) const
{
    // Ensure requested position is within the file.
    // We avoid a runtime error here to optimize out the payload_size lock.
    BITCOIN_ASSERT_MSG(position < payload_size(), "Read past end of file.");

    auto memory = file_.access();
    memory->increment(header_size_ + position);
    return memory;
}

// privates

// Read the size value from the first 64 bits of the file after the header.
template <typename LinkType>
void slab_manager<LinkType>::read_size()
{
    BITCOIN_ASSERT(header_size_ + sizeof(LinkType) <= file_.size());

    // The accessor must remain in scope until the end of the block.
    const auto memory = file_.access();
    const auto payload_size_address = memory->buffer() + header_size_;
    payload_size_ = from_little_endian_unsafe<LinkType>(
        payload_size_address);
}

// Write the size value to the first 64 bits of the file after the header.
template <typename LinkType>
void slab_manager<LinkType>::write_size() const
{
    BITCOIN_ASSERT(header_size_ + sizeof(LinkType) <= file_.size());

    // The accessor must remain in scope until the end of the block.
    const auto memory = file_.access();
    const auto payload_size_address = memory->buffer() + header_size_;
    auto serial = make_unsafe_serializer(payload_size_address);
    serial.write_little_endian<LinkType>(payload_size_);
}

} // namespace database
} // namespace libbitcoin

#endif