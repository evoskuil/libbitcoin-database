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
#ifndef LIBBITCOIN_DATABASE_UNSPENT_UNSPENT_COUNTER_IPP
#define LIBBITCOIN_DATABASE_UNSPENT_UNSPENT_COUNTER_IPP

#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

TEMPLATE
CLASS::unspent_counter(const Store& store, const stopper& cancel,
    bool turbo) NOEXCEPT
  : store_(store),
    cancel_(cancel),
    reader_(store, cancel),
    spans_(store, cancel, turbo)
{
}

TEMPLATE
code CLASS::count(unspent_totals& out,
    const difference_set& set) const NOEXCEPT
{
    std_vector<unspent_totals> totals(spans_.count());
    const auto ec = spans_.for_each(set,
        [&](size_t index, size_t begin, size_t end) NOEXCEPT
        {
            return span(totals.at(index), set, begin, end);
        });

    if (ec)
        return ec;

    for (const auto& total: totals)
        unspent_writer::add(out, total);

    return error::success;
}

TEMPLATE
code CLASS::span(unspent_totals& out, const difference_set& set,
    size_t begin, size_t end) const NOEXCEPT
{
    output_links puts{};
    auto previous = tx_link::terminal;
    return reader_.elements(set, begin, end,
        [&](const unspent_elements& elements) NOEXCEPT
        {
            puts.resize(elements.size());
            if (const auto ec = reader_.read_puts(puts, zero, elements, zero,
                elements.size()))
                return ec;

            return read_totals(out, previous, puts);
        });
}

TEMPLATE
code CLASS::read_totals(unspent_totals& out, tx_link::integer& previous,
    const output_links& puts) const NOEXCEPT
{
    constexpr auto fixed = unspent_writer::fixed_size;

    const auto ptr = store_.output.get_memory();
    for (const auto& put: puts)
    {
        table::output::get_parent_coin output{};
        if (!store_.output.get(ptr, put, output))
            return error::integrity;

        if (output.parent_fk != previous)
        {
            previous = output.parent_fk;
            ++out.transactions;
        }

        const auto variable = variable_size(output.script_size);

        ++out.outputs;
        out.value += output.value;
        out.script_bytes += output.script_size;
        out.coin_bytes += fixed + variable + output.script_size;
    }

    return error::success;
}

} // namespace database
} // namespace libbitcoin

#endif
