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
#ifndef LIBBITCOIN_DATABASE_UNSPENT_UNSPENT_MATCHER_HPP
#define LIBBITCOIN_DATABASE_UNSPENT_UNSPENT_MATCHER_HPP

#include <unordered_set>
#include <bitcoin/database/define.hpp>
#include <bitcoin/database/types/types.hpp>
#include <bitcoin/database/unspent/unspent_reader.hpp>
#include <bitcoin/database/unspent/unspent_spans.hpp>

namespace libbitcoin {
namespace database {

/// Coins of a set with scripts hashing to a key set.
template <typename Store>
class unspent_matcher
{
public:
    using keys = std::unordered_set<hash_digest>;

    unspent_matcher(const Store& store, const stopper& cancel,
        bool turbo) NOEXCEPT;

    /// The matching coins and the size of the set.
    code match(unspent_coins& out, size_t& txouts, const keys& keys,
        const difference_set& set) const NOEXCEPT;

private:
    code span(unspent_totals& out, unspent_coins& matches, const keys& keys,
        const difference_set& set, size_t begin, size_t end) const NOEXCEPT;

    const unspent_reader<Store> reader_;
    const unspent_spans<Store> spans_;
};

} // namespace database
} // namespace libbitcoin

#define TEMPLATE template <typename Store>
#define CLASS unspent_matcher<Store>

#include <bitcoin/database/impl/unspent/unspent_matcher.ipp>

#undef CLASS
#undef TEMPLATE

#endif
