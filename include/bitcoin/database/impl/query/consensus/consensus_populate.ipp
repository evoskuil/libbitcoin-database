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
#ifndef LIBBITCOIN_DATABASE_QUERY_CONSENSUS_POPULATE_IPP
#define LIBBITCOIN_DATABASE_QUERY_CONSENSUS_POPULATE_IPP

#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

// Input population is not expected to be called publicly, tx population (chain
// true) is expected to be called (tx pool validation), and block population is
// called for full block validation/confirmation (chain false).

// populate_with_metadata
// ----------------------------------------------------------------------------
// These are used when performing confirmation, either directly (chain true) or
// caching metadata (chain false) in validation stage for later confirmation.

TEMPLATE
bool CLASS::populate_with_metadata(const block& block,
    bool chain) const NOEXCEPT
{
    const auto& txs = block.transactions_ptr();
    if (txs->empty())
        return false;

    return std::all_of(std::next(txs->begin()), txs->end(),
        [this, chain](const auto& tx) NOEXCEPT
        {
            return this->populate_with_metadata(*tx, chain);
        });
}

TEMPLATE
bool CLASS::populate_with_metadata(const transaction& tx, bool chain,
    bool pool) const NOEXCEPT
{
    // This override makes the public method safe for coinbase calling.
    return tx.is_coinbase() || populate_with_metadata_(tx, chain, pool);
}

// protected
TEMPLATE
bool CLASS::populate_with_metadata_(const transaction& tx, bool chain,
    bool pool) const NOEXCEPT
{
    BC_ASSERT(!tx.is_coinbase());

    const auto& ins = tx.inputs_ptr();
    return std::all_of(ins->begin(), ins->end(),
        [this, chain, pool](const auto& in) NOEXCEPT
        {
            return this->populate_with_metadata(*in, chain, pool);
        });
}

TEMPLATE
bool CLASS::populate_with_metadata(const input& input, bool chain,
    bool pool) const NOEXCEPT
{
    // Null point would return nullptr and be interpreted as missing.
    BC_ASSERT(!input.point().is_null());

    // If read via the store for store confirmation, then...
    // input.metadata.input_link is set earlier in get_input().
    // So this assertion no longer holds.
    ////BC_ASSERT(input.metadata.input_link == max_uint32);

    if (input.prevout)
        return true;

    const auto tx = to_tx(input.point().hash());
    const auto spent = to_output(tx, input.point().index());

    // Node and chain confirmation.
    input.prevout = get_output(spent);
    input.metadata.coinbase = is_coinbase(tx);

    // Node only (cheap so always include).
    input.metadata.output_link = spent.value;

    // This is used by node for unconfirmed tx validation (expensive).
    // There are three consensus-relevant heights. These are the height of the
    // presumed next block (pool), the height of the block containing previous
    // output's transaction, and the height of a block containing a previous
    // spender of the previous output. median-time-past is of the prevout.
    if (chain)
    {
        auto& metadata = input.metadata;

        context ctx{};
        if (get_context(ctx, find_strong(tx)))
        {
            // Confirmed previous output found at height (and with mtp).
            metadata.prevout_height = ctx.height;
            metadata.median_time_past = ctx.mtp;
        }
        else
        {
            // Confirmed previous output not found.
            metadata.prevout_height = max_uint32;
            metadata.median_time_past = max_uint32;
        }

        // The genesis coinbase is unspendable, so no confirmed spender is at
        // zero, which therefore distinguishes an unconfirmed spender.
        if (pool && !is_spent(input.point()))
        {
            // Any spender (including duplicate tx) not found.
            metadata.spender_height = max_uint32;
        }
        else if (const auto height = find_strong_spender_height(input.point());
            !height.is_terminal())
        {
            // Confirmed spender found at height.
            metadata.spender_height = system::possible_narrow_cast<uint32_t>(
                height.value);
        }
        else
        {
            // Confirmed spender not found.
            metadata.spender_height = pool ? 0_u32 : max_uint32;
        }
    }

    return !is_null(input.prevout);
}

TEMPLATE
code CLASS::populate_with_metadata(const header_link& link,
    block_view& block) const NOEXCEPT
{
    using namespace system;
    const auto& txs = block.views();
    const auto links = to_transactions(link);
    if (links.empty() || links.size() != txs.size())
        return error::integrity;

    data_chunk prevouts{};
    stream::out::data ostream(prevouts);
    write::bytes::ostream sink(ostream);

    auto found = true;
    for (auto index = one; index < txs.size(); ++index)
    {
        const auto& tx = txs.at(index);
        for (auto in = tx.inputs_begin(); in != tx.inputs_end(); ++in)
        {
            const auto spent = to_output(in->point_hash(), in->point_index());
            in->metadata.output_link = spent.value;
            if (spent.is_terminal())
            {
                found = false;
                continue;
            }

            if (is_null(in->prevout) && !tx.excluded &&
                !get_wire_output(sink, spent))
                return error::integrity;
        }
    }

    sink.flush();
    if (!sink)
        return error::integrity;

    if (found && !block.populate(std::move(prevouts)))
        return error::integrity;

    if (!found)
        return system::error::missing_previous_output;

    return error::success;
}

// populate_pooled
// ----------------------------------------------------------------------------
// This is used in validation of a block containing a tx validated in the pool,
// caching the pool's metadata for later confirmation. A block-internal spend
// is populated by the block, so its metadata remains that of internal spend.

TEMPLATE
code CLASS::populate_pooled(pooled_tx& out, const transaction& tx,
    const tx_link& link, const context& ctx) const NOEXCEPT
{
    const auto& ins = *tx.inputs_ptr();
    out.prevouts.resize(ins.size());
    if (const auto ec = get_pooled(out, link, ctx))
        return ec;

    auto prevout = out.prevouts.cbegin();
    for (const auto& in: ins)
    {
        const auto& pooled = *prevout++;
        if (in->prevout)
            continue;

        in->metadata.output_link = pooled.spent;
        in->metadata.coinbase = pooled.coinbase;
    }

    return error::success;
}

// populate_without_metadata
// ----------------------------------------------------------------------------
// These are used when not performing confirmation. This also implies that
// validation is not being performed, so is used for populating prevouts for
// the purpose of computing client filters in the validation stage. So these
// are not used for in consensus but are kept here for close similarity.

TEMPLATE
bool CLASS::populate_without_metadata(const block& block) const NOEXCEPT
{
    const auto& txs = block.transactions_ptr();
    if (txs->empty())
        return false;

    return std::all_of(std::next(txs->begin()), txs->end(),
        [this](const auto& tx) NOEXCEPT
        {
            return this->populate_without_metadata(*tx);
        });
}

TEMPLATE
bool CLASS::populate_without_metadata(const transaction& tx) const NOEXCEPT
{
    BC_ASSERT(!tx.is_coinbase());

    const auto& ins = tx.inputs_ptr();
    return std::all_of(ins->begin(), ins->end(),
        [this](const auto& in) NOEXCEPT
        {
            return this->populate_without_metadata(*in);
        });
}

TEMPLATE
bool CLASS::populate_without_metadata(const input& input) const NOEXCEPT
{
    // Null point would return nullptr and be interpreted as missing.
    BC_ASSERT(!input.point().is_null());

    if (input.prevout)
        return true;

    input.prevout = get_output(to_output(input.point()));
    return !is_null(input.prevout);
}

} // namespace database
} // namespace libbitcoin

#endif
