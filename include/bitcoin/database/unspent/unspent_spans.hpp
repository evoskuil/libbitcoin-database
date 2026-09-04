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
#ifndef LIBBITCOIN_DATABASE_UNSPENT_UNSPENT_SPANS_HPP
#define LIBBITCOIN_DATABASE_UNSPENT_UNSPENT_SPANS_HPP

#include <bitcoin/database/define.hpp>
#include <bitcoin/database/types/types.hpp>

namespace libbitcoin {
namespace database {

/// Parallel iteration of a set in tx-aligned element ranges.
template <typename Store>
class unspent_spans
{
public:
    unspent_spans(const Store& store, const stopper& cancel,
        bool turbo) NOEXCEPT;

    /// The span index bound.
    size_t count() const NOEXCEPT;

    /// span(index, begin, end) for each element range, in parallel.
    template <typename Span>
    code for_each(const difference_set& set, const Span& span) const NOEXCEPT;

private:
    static constexpr auto per_thread = 32_size;

    bool to_tx_base(size_t& out, size_t element) const NOEXCEPT;
    code bounds(std_vector<size_t>& out,
        const difference_set& set) const NOEXCEPT;

    const Store& store_;
    const stopper& cancel_;
    const bool turbo_;
};

} // namespace database
} // namespace libbitcoin

#define TEMPLATE template <typename Store>
#define CLASS unspent_spans<Store>

#include <bitcoin/database/impl/unspent/unspent_spans.ipp>

#undef CLASS
#undef TEMPLATE

#endif
