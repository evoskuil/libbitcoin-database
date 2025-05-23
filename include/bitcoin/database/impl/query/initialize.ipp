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
#ifndef LIBBITCOIN_DATABASE_QUERY_INITIALIZE_IPP
#define LIBBITCOIN_DATABASE_QUERY_INITIALIZE_IPP

#include <algorithm>
#include <utility>
#include <bitcoin/system.hpp>
#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

// Initialization (natural-keyed).
// ----------------------------------------------------------------------------

TEMPLATE
inline bool CLASS::is_initialized() const NOEXCEPT
{
    return is_nonzero(store_.confirmed.count()) &&
        is_nonzero(store_.candidate.count());
}

TEMPLATE
inline size_t CLASS::get_top_candidate() const NOEXCEPT
{
    BC_ASSERT_MSG(is_nonzero(store_.candidate.count()), "empty");
    return sub1(store_.candidate.count());
}

TEMPLATE
inline size_t CLASS::get_top_confirmed() const NOEXCEPT
{
    BC_ASSERT_MSG(is_nonzero(store_.confirmed.count()), "empty");
    return sub1(store_.confirmed.count());
}

TEMPLATE
size_t CLASS::get_top_associated() const NOEXCEPT
{
    return get_top_associated_from(get_fork());
}

TEMPLATE
size_t CLASS::get_top_associated_from(size_t height) const NOEXCEPT
{
    if (height >= height_link::terminal)
        return max_size_t;

    while (is_associated(to_candidate(++height)));
    return --height;
}

TEMPLATE
associations CLASS::get_all_unassociated() const NOEXCEPT
{
    return get_unassociated_above(get_fork());
}

TEMPLATE
associations CLASS::get_unassociated_above(size_t height) const NOEXCEPT
{
    return get_unassociated_above(height, max_size_t);
}

TEMPLATE
associations CLASS::get_unassociated_above(size_t height,
    size_t count) const NOEXCEPT
{
    return get_unassociated_above(height, count, max_size_t);
}

TEMPLATE
associations CLASS::get_unassociated_above(size_t height,
    size_t count, size_t last) const NOEXCEPT
{
    association item{};
    associations out{};
    const auto top = std::min(get_top_candidate(), last);

    while (++height <= top && is_nonzero(count))
    {
        if (get_unassociated(item, to_candidate(height)))
        {
            out.insert(std::move(item));
            --count;
        }
    }

    return out;
}

TEMPLATE
size_t CLASS::get_unassociated_count() const NOEXCEPT
{
    return get_unassociated_count_above(get_fork());
}

TEMPLATE
size_t CLASS::get_unassociated_count_above(size_t height) const NOEXCEPT
{
    return get_unassociated_count_above(height, max_size_t);
}

TEMPLATE
size_t CLASS::get_unassociated_count_above(size_t height,
    size_t maximum) const NOEXCEPT
{
    size_t count{};
    const auto top = get_top_candidate();
    while (++height <= top && count < maximum)
        if (!is_associated(to_candidate(height)))
            ++count;

    return count;
}

} // namespace database
} // namespace libbitcoin

#endif
