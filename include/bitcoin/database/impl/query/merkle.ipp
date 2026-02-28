/**
 * Copyright (c) 2011-2026 libbitcoin developers (see AUTHORS)
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
#ifndef LIBBITCOIN_DATABASE_QUERY_MERKLE_IPP
#define LIBBITCOIN_DATABASE_QUERY_MERKLE_IPP

#include <algorithm>
#include <ranges>
#include <utility>
#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

// merkle
// ----------------------------------------------------------------------------

// protected
TEMPLATE
CLASS::hash_option CLASS::create_interval(header_link link,
    size_t height) const NOEXCEPT
{
    // Interval ends at nth block where n is a multiple of span.
    // One is a functional but undesirable case (no optimization).
    const auto span = interval_span();
    BC_ASSERT(!is_zero(span));

    // If valid link is provided then empty return implies non-interval height.
    if (link.is_terminal() || !system::is_multiple(add1(height), span))
        return {};

    // Generate the leaf nodes for the span.
    hashes leaves(span);
    for (auto& leaf: std::views::reverse(leaves))
    {
        leaf = get_header_key(link);
        link = to_parent(link);
    }

    // Generate the merkle root of the interval ending on link header.
    return system::merkle_root(std::move(leaves));
}

// protected
TEMPLATE
CLASS::hash_option CLASS::get_confirmed_interval(size_t height) const NOEXCEPT
{
    const auto span = interval_span();
    BC_ASSERT(!is_zero(span));

    if (!system::is_multiple(add1(height), span))
        return {};

    table::txs::get_interval txs{};
    if (!store_.txs.at(to_confirmed(height), txs))
        return {};

    return txs.interval;
}

// static/protected
TEMPLATE
void CLASS::merge_merkle(hashes& to, hashes&& from, size_t first) NOEXCEPT
{
    // from is either even or has one additional element of reserved space.
    if (!is_one(from.size()) && is_odd(from.size()))
        from.push_back(from.back());

    using namespace system;
    for (const auto& row: block::merkle_branch(first, from.size()))
    {
        BC_ASSERT(add1(row.sibling) * row.width <= from.size());
        const auto it = std::next(from.begin(), row.sibling * row.width);
        const auto mover = std::make_move_iterator(it);
        to.push_back(merkle_root({ mover, std::next(mover, row.width) }));
    }
}

// protected
TEMPLATE
code CLASS::get_merkle_proof(hashes& proof, hashes roots, size_t target,
    size_t waypoint) const NOEXCEPT
{
    const auto span = interval_span();
    BC_ASSERT(!is_zero(span));

    const auto first = (target / span) * span;
    const auto last = std::min(sub1(first + span), waypoint);
    auto other = get_confirmed_hashes(first, add1(last - first));
    if (other.empty())
        return error::merkle_proof;

    using namespace system;
    proof.reserve(ceilinged_log2(other.size()) + ceilinged_log2(roots.size()));
    merge_merkle(proof, std::move(other), target % span);
    merge_merkle(proof, std::move(roots), target / span);
    return error::success;
}

// static/private
TEMPLATE
hash_digest CLASS::merkle_subroot(hashes&& tree, size_t span) NOEXCEPT
{
    using namespace system;

    // Tree cannot be empty or exceed span (span is power of two).
    if (tree.empty() || tree.size() > span)
        return {};

    // zero depth implies single tree element, which is the root.
    const auto depth = ceilinged_log2(span);
    if (is_zero(depth))
        return tree.front();

    // Merkle root treats a single hash as top/complete, but for a non-zero
    // depth subtree, any odd leaf requires duplication including a single.
    if (is_odd(tree.size()))
        tree.push_back(tree.back());

    // Log2 of the evened breadth gives the elevation by merkle_root.
    const auto partial = ceilinged_log2(tree.size());

    // Partial cannot exceed depth, since tree.size() <= span (a power of 2).
    if (is_subtract_overflow(depth, partial))
        return {};

    // Elevate hashes to partial level.
    auto hash = merkle_root(std::move(tree));

    // Elevate hashes from partial to depth level.
    for (size_t level{}; level < (depth - partial); ++level)
        hash = sha256::double_hash(hash, hash);

    return hash;
}

// protected
TEMPLATE
code CLASS::get_merkle_subroots(hashes& roots, size_t waypoint) const NOEXCEPT
{
    const auto span = interval_span();
    BC_ASSERT(!is_zero(span));

    const auto range = add1(waypoint);
    roots.reserve(system::ceilinged_divide(range, span));
    for (size_t first{}; first < range; first += span)
    {
        const auto last = std::min(sub1(first + span), waypoint);
        const auto size = add1(last - first);

        if (size == span)
        {
            auto interval = get_confirmed_interval(last);
            if (!interval.has_value()) return error::merkle_interval;
            roots.push_back(std::move(interval.value()));
        }
        else
        {
            auto partial = get_confirmed_hashes(first, size);
            if (partial.empty()) return error::merkle_hashes;
            roots.push_back(merkle_subroot(std::move(partial), span));
        }
    }

    return error::success;
}

TEMPLATE
code CLASS::get_merkle_root_and_proof(hash_digest& root, hashes& proof,
    size_t target, size_t waypoint) const NOEXCEPT
{
    if (target > waypoint)
        return error::invalid_argument;

    if (waypoint > get_top_confirmed())
        return error::not_found;

    hashes tree{};
    if (const auto ec = get_merkle_subroots(tree, waypoint))
        return ec;

    proof.clear();
    if (const auto ec = get_merkle_proof(proof, tree, target, waypoint))
        return ec;

    root = system::merkle_root(std::move(tree));
    return {};
}

TEMPLATE
hash_digest CLASS::get_merkle_root(size_t height) const NOEXCEPT
{
    hashes tree{};
    if (const auto ec = get_merkle_subroots(tree, height))
        return {};

    return system::merkle_root(std::move(tree));
}

} // namespace database
} // namespace libbitcoin

#endif
