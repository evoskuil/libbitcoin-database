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
#ifndef LIBBITCOIN_DATABASE_QUERY_POOL_CONFIRMABLE_IPP
#define LIBBITCOIN_DATABASE_QUERY_POOL_CONFIRMABLE_IPP

#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

TEMPLATE
bool CLASS::is_confirmable_spent(const output_link& link) const NOEXCEPT
{
    for (const auto& in: to_spenders(link))
        if (is_confirmable_tx(to_input_tx(in)))
            return true;

    return false;
}

TEMPLATE
ins_link CLASS::find_confirmable_spender(const point& prevout) const NOEXCEPT
{
    for (const auto& in: to_spenders(prevout))
        if (is_confirmable_tx(to_input_tx(in)))
            return in;

    return {};
}

// An unconfirmed parent of a confirmable tx is itself confirmable.
TEMPLATE
tx_links CLASS::to_confirmable_parents(const tx_link& link) const NOEXCEPT
{
    tx_links out{};
    for (const auto& point: to_points(link))
    {
        const auto parent = to_tx(get_point_key(point).hash());
        if (!parent.is_terminal() && is_confirmable_tx(parent) &&
            !system::contains(out, parent.value))
            out.push_back(parent.value);
    }

    return out;
}

TEMPLATE
tx_links CLASS::to_confirmable_children(const tx_link& link) const NOEXCEPT
{
    tx_links out{};
    for (const auto& output: to_outputs(link))
    {
        for (const auto& in: to_spenders(output))
        {
            const auto child = to_input_tx(in);
            if (!child.is_terminal() && is_confirmable_tx(child) &&
                !system::contains(out, child.value))
                out.push_back(child.value);
        }
    }

    return out;
}

TEMPLATE
tx_links CLASS::to_confirmable_ancestors(const tx_link& link) const NOEXCEPT
{
    return to_confirmable_relatives(link, true);
}

TEMPLATE
tx_links CLASS::to_confirmable_descendants(const tx_link& link) const NOEXCEPT
{
    return to_confirmable_relatives(link, false);
}

TEMPLATE
bool CLASS::get_ancestor_totals(fee_totals& out,
    const tx_link& link) const NOEXCEPT
{
    return get_confirmable_totals(out, link, true);
}

TEMPLATE
bool CLASS::get_descendant_totals(fee_totals& out,
    const tx_link& link) const NOEXCEPT
{
    return get_confirmable_totals(out, link, false);
}

TEMPLATE
code CLASS::get_confirmable_txs(tx_links& out) const NOEXCEPT
{
    using namespace system;
    using algorithm = system::pool::algorithm;
    using link_t = algorithm::link_t;
    out.clear();
    if (!store_.pool.enabled())
        return error::success;

    pool_guards guards{};
    algorithm::columns_t columns{};
    if (!get_pool_columns(columns, guards))
        return error::integrity;

    const auto rows = possible_narrow_cast<link_t>(columns.cells.size());
    for (link_t row{}; row < rows; ++row)
    {
        const auto cell = algorithm::from_column(columns.cells[row]);
        if (!algorithm::is_present(cell))
            continue;

        out.push_back(tx_link{ store_.pool.get_key(pool_link{ row }) });
    }

    return error::success;
}

TEMPLATE
code CLASS::get_pooled_ancestry(pooled_rows& out,
    const tx_links& links) const NOEXCEPT
{
    using namespace system;
    using algorithm = system::pool::algorithm;
    using link_t = algorithm::link_t;
    out.clear();
    if (!store_.pool.enabled())
        return error::success;

    pool_guards guards{};
    algorithm::columns_t columns{};
    if (!get_pool_columns(columns, guards))
        return error::integrity;

    std::vector<link_t> pending{};
    pending.reserve(links.size());
    for (const tx_link link: links)
    {
        const auto row = store_.pool.first(link);
        if (row.is_terminal())
            return error::integrity;

        pending.push_back(possible_narrow_cast<link_t>(row.value));
    }

    std::unordered_set<link_t> seen{};
    while (!pending.empty())
    {
        const auto row = pending.back();
        pending.pop_back();
        if (!seen.insert(row).second)
            continue;

        const auto edges = algorithm::from_column(columns.edges[row]);
        const auto end = edges.first + edges.count;
        for (auto input = edges.first; input < end; ++input)
        {
            const auto parent = algorithm::from_column(columns.rows[input]);
            if (parent == algorithm::none)
                continue;

            const auto cell = algorithm::from_column(columns.cells[parent]);
            if (!algorithm::is_boundary(cell))
                pending.push_back(parent);
        }

        const tx_link tx{ store_.pool.get_key(pool_link{ row }) };
        out.push_back({ row, tx.value,
            algorithm::from_column(columns.totals[row]) });
    }

    std::ranges::sort(out, {}, &pooled_row::row);
    return error::success;
}

TEMPLATE
code CLASS::get_confirmable_fees(fee_rates& out) const NOEXCEPT
{
    using namespace system;
    using algorithm = system::pool::algorithm;
    using link_t = algorithm::link_t;
    out.clear();
    if (!store_.pool.enabled())
        return error::success;

    pool_guards guards{};
    algorithm::columns_t columns{};
    if (!get_pool_columns(columns, guards))
        return error::integrity;

    const auto rows = possible_narrow_cast<link_t>(columns.cells.size());
    for (link_t row{}; row < rows; ++row)
    {
        const auto cell = algorithm::from_column(columns.cells[row]);
        if (!algorithm::is_present(cell))
            continue;

        const auto totals = algorithm::from_column(columns.totals[row]);
        const size_t height = algorithm::from_column(columns.heights[row]);
        const auto bytes = ceilinged_divide<size_t>(totals.weight,
            chain::light_weight_factor);
        out.push_back({ bytes, totals.fee, height });
    }

    return error::success;
}

// A receipt is counted in an outcome that includes its tx and no spender of it.
// Its spenders are rivals, so at most one is included, and the sum of an
// outcome is that of a weight of each member: what it receives less what it
// spends. Rival spenders are of one component, as is a row with its
// descendants, so a receipt is of at most one. Component over the limit counts
// none, or each.
TEMPLATE
code CLASS::get_unconfirmed_range(uint64_t& worst, uint64_t& best,
    const pool_receipts& receipts, size_t maximum_outcomes) const NOEXCEPT
{
    using namespace system;
    using algorithm = system::pool::algorithm;
    using link_t = algorithm::link_t;
    constexpr auto none = algorithm::none;
    worst = zero;
    best = zero;
    if (!store_.pool.enabled())
        return error::success;

    pool_guards guards{};
    algorithm::columns_t columns{};
    if (!get_pool_columns(columns, guards))
        return error::integrity;

    // Label of the row if present, labeled false if not in a component.
    const auto rows = columns.cells.size();
    const auto present = [&](link_t& label, bool& labeled,
        pool_link::integer row) NOEXCEPT
    {
        if (row >= rows)
            return false;

        label = algorithm::from_column(columns.cells[row]);
        labeled = algorithm::is_labeled(label);
        return algorithm::is_present(label);
    };

    struct item { link_t row; uint64_t value; pool_links spenders; };
    std::map<link_t, std::vector<item>> components{};
    for (const auto& receipt: receipts)
    {
        link_t label{};
        bool labeled{};
        if (!present(label, labeled, receipt.row))
            continue;

        auto spent = false;
        auto component = labeled ? label : none;
        item next
        {
            labeled ? possible_narrow_cast<link_t>(receipt.row) : none,
            receipt.value,
            {}
        };

        for (const auto spender: receipt.spenders)
        {
            if (!present(label, labeled, spender))
                continue;

            spent = true;
            if (!labeled)
                continue;

            component = label;
            next.spenders.push_back(possible_narrow_cast<link_t>(spender));
        }

        if (component != none)
        {
            components[component].push_back(std::move(next));
        }
        else if (!spent)
        {
            worst = ceilinged_add(worst, receipt.value);
            best = ceilinged_add(best, receipt.value);
        }
    }

    std::vector<algorithm::mask_t> outcomes{};
    const algorithm::graph_t graph{ columns };
    for (const auto& [label, items]: components)
    {
        algorithm::component_t component{};
        const auto exact = algorithm::collect(component, graph, label) &&
            algorithm::relate(component, graph) &&
            algorithm::outcomes(component) <= maximum_outcomes;

        if (!exact)
        {
            for (const auto& next: items)
                best = ceilinged_add(best, next.value);

            continue;
        }

        // Receipt of a row not in component is counted unless spent.
        uint64_t base{};
        algorithm::weights_t weights{};
        const auto member = [&](link_t row) NOEXCEPT
        {
            const std::array one_row{ row };
            return right_zeros(algorithm::members(component, one_row));
        };

        for (const auto& next: items)
        {
            if (next.row == none)
                base += next.value;
            else
                weights[member(next.row)] += next.value;

            for (const auto spender: next.spenders)
                weights[member(spender)] -= next.value;
        }

        int64_t least{};
        int64_t greatest{};
        algorithm::leaves(outcomes, component, graph, maximum_outcomes, {}, {});
        algorithm::range(least, greatest, outcomes, component, weights);
        worst = ceilinged_add(worst, base + to_unsigned(least));
        best = ceilinged_add(best, base + to_unsigned(greatest));
    }

    return error::success;
}

TEMPLATE
bool CLASS::get_confirmable_totals(fee_totals& out, const tx_link& link,
    bool ancestors) const NOEXCEPT
{
    using namespace system;
    auto links = to_confirmable_relatives(link, ancestors);
    links.push_back(link.value);

    out = {};
    for (const auto fk: links)
    {
        fee_rate rate{};
        if (!get_tx_fees(rate, fk))
            return false;

        // A confirmed relative is not pooled (and has no sigops total).
        table::pool_total total{};
        const auto row = store_.pool.first(tx_link{ fk });
        if (!row.is_terminal() && !store_.pool.totals.get(row, total))
            return false;

        ++out.count;
        out.bytes = ceilinged_add(out.bytes, rate.bytes);
        out.sigops = ceilinged_add(out.sigops, total.sigops);
        out.fee = ceilinged_add(out.fee, rate.fee);
    }

    return true;
}

TEMPLATE
tx_links CLASS::to_confirmable_relatives(const tx_link& link,
    bool ancestors) const NOEXCEPT
{
    tx_links out{};
    std::vector<tx_link::integer> pending{ link.value };
    std::unordered_set<tx_link::integer> visited{ link.value };
    while (!pending.empty())
    {
        const tx_link next{ pending.back() };
        pending.pop_back();

        const auto links = ancestors ? to_confirmable_parents(next) :
            to_confirmable_children(next);

        for (const auto fk: links)
        {
            if (visited.insert(fk).second)
            {
                out.push_back(fk);
                pending.push_back(fk);
            }
        }
    }

    return out;
}

} // namespace database
} // namespace libbitcoin

#endif
