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
#ifndef LIBBITCOIN_DATABASE_QUERY_ARCHIVE_WIRE_WRITER_IPP
#define LIBBITCOIN_DATABASE_QUERY_ARCHIVE_WIRE_WRITER_IPP

#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

// set transaction_view
// ----------------------------------------------------------------------------
// This is the only multitable write query (except initialize/genesis).
// The caller holds the transactor and all table accessors, with all table rows
// preallocated by the block writer. No allocation may occur under accessors.

TEMPLATE
code CLASS::set_code(const accessors& ptrs, const allocation& fks,
    const transaction_view& tx, bool prune) NOEXCEPT
{
    using namespace system;
    using ix = linkage<schema::index>;

    // View parser treats empty as invalid, so there is no empty state.
    // As a result this condition should never be hit (deserialization fails).
    if (!tx.is_valid())
        return error::tx_empty;

    const auto inputs = possible_narrow_cast<ix::integer>(tx.inputs());
    const auto outputs = possible_narrow_cast<ix::integer>(tx.outputs());
    const auto coinbase = tx.is_coinbase();

    // Contiguously store inputs (preallocated).
    if (!store_.input.put(ptrs.input, fks.in_fk,
        table::input::put_view{ {}, tx, prune }))
        return error::tx_input_put;

    // Contiguously store outputs (preallocated).
    if (!store_.output.put(ptrs.output, fks.out_fk,
        table::output::put_view{ {}, tx }))
        return error::tx_output_put;

    // Contiguously store ins (preallocated).
    if (!store_.ins.put(ptrs.ins, fks.ins_fk,
        table::ins::put_view{ {}, fks.in_fk, fks.tx_fk, tx }))
        return error::tx_ins_put;

    // Contiguously store points (expanded, positioned with ins).
    if (!store_.points_bank(fks.bank).put(ptrs.points, fks.points_fk,
        table::points::put_view{ {}, tx }))
        return error::tx_points_put;

    // Contiguously store outs (preallocated).
    if (!store_.outs.put(ptrs.outs, fks.outs_fk,
        table::outs::put_view{ {}, fks.out_fk, fks.tx_fk, coinbase, tx }))
        return error::tx_outs_put;

    // Create tx record (preallocated).
    // Commit is deferred for address index consistency.
    if (!store_.tx.set(ptrs.tx, fks.tx_fk, tx.hash(false),
        table::transaction::put_view
        {
            {},
            tx,
            inputs,
            outputs,
            fks.ins_fk,
            fks.outs_fk
        }))
    {
        return error::tx_tx_set;
    }

    // Commit address index records (hashmap, rows positioned with outs).
    if (address_enabled())
    {
        auto ad_fk = fks.outs_fk;
        auto outs = tx.get_outputs_stream();
        read::bytes::fast osource{ outs };

        for (size_t out{}; out < outputs; ++out)
        {
            osource.skip_bytes(sizeof(uint64_t));
            const auto bytes = osource.read_size();

            if (!store_.address.push(ptrs.address, ad_fk++,
                { sha256_hash(osource.read_bytes(bytes)) }))
                return error::tx_address_put;
        }

        store_.address.complete(fks.outs_fk.value, outputs);

        BC_ASSERT(osource);
        if (!osource)
            return error::tx_address_put;
    }

    // Commit tx to search (hashmap).
    return store_.tx.commit(ptrs.tx, fks.tx_fk, tx.hash(false)) ?
        error::success : error::tx_tx_commit;
}

TEMPLATE
tx_link CLASS::to_pooled(const transaction_view& tx) const NOEXCEPT
{
    if (tx.is_coinbase())
        return {};

    // One tx with distinct witnesses can both validate unconfirmed, so guard.
    for (const auto& link: to_duplicates(tx.hash(false)))
        if (is_witness_match(link, tx))
            return link;

    return {};
}

// set txs from block
// ----------------------------------------------------------------------------
// This sets only the txs of a block with header/context already archived.
// Block MUST be kept in scope until all transactions are written. ~block()
// releases all memory for parts of itself, due to the custom allocator.

TEMPLATE
code CLASS::set_code(const block_view& block, bool strong, bool bypass,
    bool prune) NOEXCEPT
{
    header_link unused{};
    return set_code(unused, block, strong, bypass, prune);
}

TEMPLATE
code CLASS::set_code(header_link& out_fk, const block_view& block,
    bool strong, bool bypass, bool prune) NOEXCEPT
{
    out_fk = to_header(block.hash());
    if (out_fk.is_terminal())
        return error::txs_header;

    size_t height{};
    if (!get_height(height, out_fk))
        return error::txs_height;

    return set_code(block, out_fk, strong, bypass, height, prune);
}

TEMPLATE
code CLASS::set_code(const block_view& block, const header_link& key,
    bool strong, bool, size_t height, bool prune) NOEXCEPT
{
    using namespace system;
    using in_t = input_link::integer;
    using out_t = output_link::integer;
    using ins_t = ins_link::integer;
    using outs_t = outs_link::integer;

    if (key.is_terminal())
        return error::txs_header;

    auto txs = block.transactions();
    if (is_zero(txs))
        return error::txs_empty;

    // Sum full block allocation for each table from cached view metadata.
    size_t points{};
    size_t outputs{};
    size_t input_bytes{};
    size_t output_bytes{};
    const auto pooling = store_.is_pooling();
    const auto& views = block.views();
    tx_links links(txs, tx_link::terminal);
    auto it = links.begin();
    for (const auto& tx: views)
    {
        if (pooling)
            *it = to_pooled(tx);

        if (*it++ != tx_link::terminal)
        {
            --txs;
            continue;
        }

        points += tx.inputs();
        outputs += tx.outputs();
        input_bytes += tx.input_table_size(prune);
        output_bytes += tx.output_table_size();
    }

    // Optional hash, only has value on height intervals.
    auto interval = create_interval(key, height);
    
    using count = linkage<schema::count_>::integer;
    const auto light = block.serialized_size(false);
    const auto heavy = block.serialized_size(true);

    // ========================================================================
    const auto scope = get_transactor();

    // Allocate all block rows for each table (one allocation lock each).
    allocation fks{};
    fks.tx_fk = store_.tx.allocate(possible_narrow_cast<count>(txs));
    if (fks.tx_fk.is_terminal())
        return error::tx_tx_allocate;

    fks.in_fk = store_.input.allocate(
        possible_narrow_cast<in_t>(input_bytes));
    if (fks.in_fk.is_terminal())
        return error::tx_input_put;

    fks.out_fk = store_.output.allocate(
        possible_narrow_cast<out_t>(output_bytes));
    if (fks.out_fk.is_terminal())
        return error::tx_output_put;

    // The bank is selected with the ins allocation, excluding a bank swap.
    {
        const auto selector = store_.get_selector();
        fks.bank = store_.get_points_bank();
        fks.ins_fk = store_.ins.allocate(
            possible_narrow_cast<ins_t>(points));
    }

    if (fks.ins_fk.is_terminal())
        return error::tx_ins_put;

    auto& bank = store_.points_bank(fks.bank);

    // Points rows are positioned with ins less the bank base, by expansion.
    const auto base = bank.base().value;
    if (fks.ins_fk.value < base)
        return error::tx_points_put;

    fks.points_fk = fks.ins_fk.value - base;
    if (!bank.expand(fks.points_fk.value +
        possible_narrow_cast<ins_t>(points)))
        return error::tx_points_put;

    fks.outs_fk = store_.outs.allocate(
        possible_narrow_cast<outs_t>(outputs));
    if (fks.outs_fk.is_terminal())
        return error::tx_outs_put;

    // Address rows are positioned with outs by expansion.
    const auto address = address_enabled();
    if (address && !store_.address.expand(fks.outs_fk +
        possible_narrow_cast<outs_t>(outputs)))
        return error::tx_address_put;

    // Guard all tables against remap for the duration of the block write.
    // No table may be allocated while any of these accessors are held.
    accessors ptrs{};
    ptrs.tx = store_.tx.get_memory();
    ptrs.ins = store_.ins.get_memory();
    if (is_nonzero(points))
        ptrs.points = bank.get_memory();

    ptrs.outs = store_.outs.get_memory();
    ptrs.input  = store_.input.get_memory();
    ptrs.output = store_.output.get_memory();
    if (address)
        ptrs.address = store_.address.get_memory();

    if (!ptrs.input ||
        !ptrs.output ||
        !ptrs.ins ||
        (is_nonzero(points) && !ptrs.points) ||
        !ptrs.outs ||
        !ptrs.tx ||
        (address && !ptrs.address))
        return error::unloaded_file;

    // Write all txs into their preallocated rows (write order preserved).
    code ec{};
    it = links.begin();
    for (const auto& tx: views)
    {
        if (*it == tx_link::terminal)
        {
            if ((ec = set_code(ptrs, fks, tx, prune)))
                return ec;

            *it = fks.tx_fk++;
            fks.points_fk += possible_narrow_cast<ins_t>(tx.inputs());
            fks.ins_fk    += possible_narrow_cast<ins_t>(tx.inputs());
            fks.outs_fk   += possible_narrow_cast<outs_t>(tx.outputs());
            fks.in_fk     += possible_narrow_cast<in_t>(tx.input_table_size(prune));
            fks.out_fk    += possible_narrow_cast<out_t>(tx.output_table_size());
        }

        ++it;
    }

    // Release all accessors (subsequent writes allocate).
    ptrs.tx.reset();
    ptrs.ins.reset();
    ptrs.points.reset();
    ptrs.outs.reset();
    ptrs.input.reset();
    ptrs.output.reset();
    ptrs.address.reset();

    return set_txs(key, std::move(links), light, heavy, std::move(interval),
        strong);
    // ========================================================================
}

} // namespace database
} // namespace libbitcoin

#endif
