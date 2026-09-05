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
#ifndef LIBBITCOIN_DATABASE_UNSPENT_UNSPENT_READER_HPP
#define LIBBITCOIN_DATABASE_UNSPENT_UNSPENT_READER_HPP

#include <bitcoin/database/define.hpp>
#include <bitcoin/database/tables/tables.hpp>
#include <bitcoin/database/types/types.hpp>

namespace libbitcoin {
namespace database {

/// Set elements (outs table links).
using unspent_elements = std_vector<outs_link::integer>;

/// Materializes coins from set elements, each table in its own pass.
template <typename Store>
class unspent_reader
{
public:
    unspent_reader(const Store& store, const stopper& cancel) NOEXCEPT;

    /// flush(elements) for each batch of elements in the range.
    template <typename Flush>
    code elements(const difference_set& set, size_t begin, size_t end,
        const Flush& flush) const NOEXCEPT;

    /// handle(coin) for each coin (script copied) in the range, in tx order.
    template <typename Handler>
    code batch(const difference_set& set, size_t begin, size_t end,
        const Handler& handle) const NOEXCEPT;

    /// Coins (scripts not copied) and output links of the elements into out
    /// and puts from offset, tx state carried.
    code fill(unspent_coins& out, output_links& puts, size_t offset,
        tx_link::integer& previous, const unspent_elements& elements,
        size_t begin, size_t end) const NOEXCEPT;

    /// Values and scripts of the output links into out from offset.
    code read_scripts(unspent_coins& out, size_t offset,
        const output_links& puts, size_t count) const NOEXCEPT;

    /// Output links of the elements into out from offset.
    code read_puts(output_links& out, size_t offset,
        const unspent_elements& elements, size_t begin,
        size_t end) const NOEXCEPT;

    /// Parent tx links of the output links from offset.
    code read_parents(tx_links& out, const output_links& puts, size_t offset,
        size_t count) const NOEXCEPT;

    /// Hashes of the tx links.
    code read_hashes(system::hashes& out,
        const tx_links& parents) const NOEXCEPT;

private:
    static constexpr auto batch_size = 4096_size;

    code read_transactions(unspent_coins& out, size_t offset,
        tx_link::integer& previous, const tx_links& parents,
        const unspent_elements& elements, size_t begin) const NOEXCEPT;
    code read_blocks(header_links& out, const tx_links& parents) const NOEXCEPT;
    code read_heights(std_vector<size_t>& out,
        const header_links& blocks) const NOEXCEPT;
    code confirm(unspent_coins& out, size_t offset, const header_links& blocks,
        const std_vector<size_t>& heights) const NOEXCEPT;

    const Store& store_;
    const stopper& cancel_;
};

} // namespace database
} // namespace libbitcoin

#define TEMPLATE template <typename Store>
#define CLASS unspent_reader<Store>

#include <bitcoin/database/impl/unspent/unspent_reader.ipp>

#undef CLASS
#undef TEMPLATE

#endif
