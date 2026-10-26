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
#ifndef LIBBITCOIN_DATABASE_STORE_IPP
#define LIBBITCOIN_DATABASE_STORE_IPP

#include <bitcoin/database/define.hpp>

namespace libbitcoin {
namespace database {

TEMPLATE
CLASS::store(const settings& config) NOEXCEPT
  : configuration_(config),
    envelope_(config.envelope),

    // Archive.
    // ------------------------------------------------------------------------

    header_head_(head(config.path / schema::dir::heads, schema::archive::header), head_settings(config.header), random),
    header_body_(body(config.path, schema::archive::header), config.header, sequential, staged),

    input_head_(head(config.path / schema::dir::heads, schema::archive::input), head_settings(config.input), sequential),
    input_body_(body(config.path, schema::archive::input), config.input, sequential, staged),

    output_head_(head(config.path / schema::dir::heads, schema::archive::output), head_settings(config.output), sequential),
    output_body_(body(config.path, schema::archive::output), config.output, sequential, staged),

    ins_head_(head(config.path / schema::dir::heads, schema::archive::ins), head_settings(config.ins), sequential),
    ins_body_(body(config.path, schema::archive::ins), config.ins, sequential, staged),

    outs_head_(head(config.path / schema::dir::heads, schema::archive::outs), head_settings(config.outs), sequential),
    outs_body_(body(config.path, schema::archive::outs), config.outs, sequential, staged),

    spend_head_(head(config.path / schema::dir::heads, schema::archive::spend), head_settings<schema::spend::cell>(config.spend, advice::sequential), sequential, !staged, shared),
    unbound_head_(head(config.path / schema::dir::heads, schema::archive::unbound), head_settings(config.unbound), random),
    unbound_body_(body(config.path, schema::archive::unbound), config.unbound, sequential, staged),

    tx_head_(head(config.path / schema::dir::heads, schema::archive::tx), head_settings(config.tx), random),
    tx_body_(body(config.path, schema::archive::tx), config.tx, sequential, staged),

    txs_head_(head(config.path / schema::dir::heads, schema::archive::txs), head_settings(config.txs), random),
    txs_body_(body(config.path, schema::archive::txs), config.txs, sequential, staged),

    // Indexes.
    // ------------------------------------------------------------------------

    candidate_head_(head(config.path / schema::dir::heads, schema::indexes::candidate), head_settings<schema::height::cell>(config.candidate), sequential),
    confirmed_head_(head(config.path / schema::dir::heads, schema::indexes::confirmed), head_settings<schema::height::cell>(config.confirmed), sequential),

    strong_head_(head(config.path / schema::dir::heads, schema::indexes::strong), head_settings<schema::strong::cell>(config.strong, advice::sequential, zero), sequential, !staged, shared),
    address_head_(head(config.path / schema::dir::heads, schema::indexes::address), head_settings(config.address), random),
    address_body_(body(config.path, schema::indexes::address), config.address, sequential, staged),
    conflict_head_(head(config.path / schema::dir::heads, schema::indexes::conflict), head_settings(config.conflict), random),
    conflict_body_(body(config.path, schema::indexes::conflict), config.conflict, sequential, staged),
    spender_head_(head(config.path / schema::dir::heads, schema::indexes::spender), head_settings<schema::spender::cell>(config.spender, advice::sequential), sequential, !staged, shared),
    scan_head_(head(config.path / schema::dir::heads, schema::indexes::scan), head_settings(config.scan), random),
    scan_body_(body(config.path, schema::indexes::scan), config.scan, sequential, staged),
    scan_prefix_head_(head(config.path / schema::dir::heads, schema::indexes::scan_prefix), head_settings(config.scan), sequential),
    scan_prefix_body_(body(config.path, schema::indexes::scan_prefix), config.scan, sequential, staged),

    // Caches.
    // ------------------------------------------------------------------------

    // TODO: body not random, but keep in memory.
    ecdsa0_head_(head(config.path / schema::dir::heads, schema::caches::ecdsa0), head_settings(config.ecdsa), sequential),
    ecdsa0_body_(body(config.path, schema::caches::ecdsa0), config.ecdsa, sequential, staged),
    ecdsa1_head_(head(config.path / schema::dir::heads, schema::caches::ecdsa1), head_settings(config.ecdsa), sequential),
    ecdsa1_body_(body(config.path, schema::caches::ecdsa1), config.ecdsa, sequential, staged),

    // TODO: body not random, but keep in memory.
    schnorr0_head_(head(config.path / schema::dir::heads, schema::caches::schnorr0), head_settings(config.schnorr), sequential),
    schnorr0_body_(body(config.path, schema::caches::schnorr0), config.schnorr, sequential, staged),
    schnorr1_head_(head(config.path / schema::dir::heads, schema::caches::schnorr1), head_settings(config.schnorr), sequential),
    schnorr1_body_(body(config.path, schema::caches::schnorr1), config.schnorr, sequential, staged),

    silent0_head_(head(config.path / schema::dir::heads, schema::caches::silent0), head_settings(config.silent), sequential),
    silent0_body_(body(config.path, schema::caches::silent0), config.silent, sequential, staged),
    silent1_head_(head(config.path / schema::dir::heads, schema::caches::silent1), head_settings(config.silent), sequential),
    silent1_body_(body(config.path, schema::caches::silent1), config.silent, sequential, staged),
    silent0_prefix_head_(head(config.path / schema::dir::heads, schema::caches::silent0_prefix), head_settings(config.silent), sequential),
    silent0_prefix_body_(body(config.path, schema::caches::silent0_prefix), config.silent, sequential, staged),
    silent1_prefix_head_(head(config.path / schema::dir::heads, schema::caches::silent1_prefix), head_settings(config.silent), sequential),
    silent1_prefix_body_(body(config.path, schema::caches::silent1_prefix), config.silent, sequential, staged),

    envelope_head_(head(config.path / schema::dir::heads, schema::caches::envelope), head_settings(config.points), sequential),
    points0_head_(head(config.path / schema::dir::heads, schema::caches::points0), head_settings(config.points), sequential),
    points0_body_(body(config.path / schema::dir::heads, schema::caches::points0), config.points, sequential, staged),
    points1_head_(head(config.path / schema::dir::heads, schema::caches::points1), head_settings(config.points), sequential),
    points1_body_(body(config.path / schema::dir::heads, schema::caches::points1), config.points, sequential, staged),

    prevalid0_head_(head(config.path / schema::dir::heads, schema::caches::prevalid0), head_settings(config.prevalid), sequential),
    prevalid0_body_(body(config.path, schema::caches::prevalid0), config.prevalid, sequential, staged),
    prevalid1_head_(head(config.path / schema::dir::heads, schema::caches::prevalid1), head_settings(config.prevalid), sequential),
    prevalid1_body_(body(config.path, schema::caches::prevalid1), config.prevalid, sequential, staged),
    presilent0_head_(head(config.path / schema::dir::heads, schema::caches::presilent0), head_settings(config.presilent), sequential),
    presilent0_body_(body(config.path, schema::caches::presilent0), config.presilent, sequential, staged),
    presilent1_head_(head(config.path / schema::dir::heads, schema::caches::presilent1), head_settings(config.presilent), sequential),
    presilent1_body_(body(config.path, schema::caches::presilent1), config.presilent, sequential, staged),


    state_head_(head(config.path / schema::dir::heads, schema::indexes::state), head_settings<schema::state::cell>(config.state, advice::random, zero), random),

    pool_head_(head(config.path / schema::dir::heads, schema::caches::pool), head_settings(config.pool), random),
    pool_body_(body(config.path, schema::caches::pool), config.pool, sequential, staged),
    wtxid_head_(head(config.path / schema::dir::heads, schema::indexes::wtxid), head_settings(config.wtxid), random),
    wtxid_body_(body(config.path, schema::indexes::wtxid), config.wtxid, sequential, staged),

    // Optionals.
    // ------------------------------------------------------------------------

    filter_bk_head_(head(config.path / schema::dir::heads, schema::optionals::filter_bk), head_settings(config.filter_bk), random),
    filter_bk_body_(body(config.path, schema::optionals::filter_bk), config.filter_bk, sequential, staged),

    filter_tx_head_(head(config.path / schema::dir::heads, schema::optionals::filter_tx), head_settings(config.filter_tx), random),
    filter_tx_body_(body(config.path, schema::optionals::filter_tx), config.filter_tx, sequential, staged),

    // Locks.
    // ------------------------------------------------------------------------

    flush_lock_(lock(config.path, schema::locks::flush)),
    process_lock_(lock(config.path, schema::locks::process)),

    // Tables.
    // ------------------------------------------------------------------------

    header(header_head_, header_body_, config.header.buckets, config.header.expected),
    input(input_head_, input_body_),
    output(output_head_, output_body_),
    ins(ins_head_, ins_body_),
    outs(outs_head_, outs_body_),
    spend(spend_head_, config.spend.buckets),
    unbound(unbound_head_, unbound_body_, config.unbound.buckets),
    tx(tx_head_, tx_body_, config.tx.buckets, config.tx.expected),
    txs(txs_head_, txs_body_, config.txs.buckets),

    address(address_head_, address_body_, config.address.buckets, config.address.expected),
    candidate(candidate_head_),
    confirmed(confirmed_head_),
    conflict(conflict_head_, conflict_body_, config.conflict.buckets, config.conflict.expected),
    spender(spender_head_, config.spender.buckets),
    strong(strong_head_, config.strong.buckets),
    scan(scan_head_, scan_body_, config.scan.buckets),
    scan_prefix(scan_prefix_head_, scan_prefix_body_),

    ecdsa0(ecdsa0_head_, ecdsa0_body_),
    ecdsa1(ecdsa1_head_, ecdsa1_body_),
    schnorr0(schnorr0_head_, schnorr0_body_),
    schnorr1(schnorr1_head_, schnorr1_body_),
    silent0(silent0_head_, silent0_body_),
    silent1(silent1_head_, silent1_body_),
    silent0_prefix(silent0_prefix_head_, silent0_prefix_body_),
    silent1_prefix(silent1_prefix_head_, silent1_prefix_body_),
    envelope(envelope_head_),
    points0(points0_head_, points0_body_, config.points.buckets),
    points1(points1_head_, points1_body_, config.points.buckets),
    prevalid0(prevalid0_head_, prevalid0_body_),
    prevalid1(prevalid1_head_, prevalid1_body_),
    presilent0(presilent0_head_, presilent0_body_),
    presilent1(presilent1_head_, presilent1_body_),
    state(state_head_, config.state.buckets),
    pool(pool_head_, pool_body_, config.pool.buckets, config.pool.expected),
    wtxid(wtxid_head_, wtxid_body_, config.wtxid.buckets, config.wtxid.expected),

    filter_bk(filter_bk_head_, filter_bk_body_, config.filter_bk.buckets),
    filter_tx(filter_tx_head_, filter_tx_body_, config.filter_tx.buckets)
{
    envelope_.set(config);

    // Filter k derives from configured expected/buckets, envelope records it
    // at create and overrides it at open (stored values govern the store).
    using namespace system;
    envelope_.header_k = possible_narrow_cast<uint8_t>(header.filter_k());
    envelope_.address_k = possible_narrow_cast<uint8_t>(address.filter_k());
    envelope_.tx_k = possible_narrow_cast<uint8_t>(tx.filter_k());
    envelope_.conflict_k = possible_narrow_cast<uint8_t>(conflict.filter_k());
    envelope_.pool_k = possible_narrow_cast<uint8_t>(pool.filter_k());
    envelope_.wtxid_k = possible_narrow_cast<uint8_t>(wtxid.filter_k());
}

TEMPLATE
bool CLASS::turbo() const NOEXCEPT
{
    return configuration_.turbo;
}

TEMPLATE
size_t CLASS::gpu_scan_minimum() const NOEXCEPT
{
    return configuration_.gpu_scan_minimum;
}

TEMPLATE
bool CLASS::mark_unconfirmable() const NOEXCEPT
{
    return configuration_.mark_unconfirmable;
}

TEMPLATE
uint8_t CLASS::interval_depth() const NOEXCEPT
{
    // Configuration uses uint16_t because of boost parser bug for single byte.
    // But 2^255 is sufficient given that interval is limited by chain length.
    return system::limit<uint8_t>(envelope_.interval_depth);
}

TEMPLATE
const database::envelope& CLASS::get_envelope() const NOEXCEPT
{
    return envelope_;
}

TEMPLATE
size_t CLASS::get_scan_frontier() const NOEXCEPT
{
    return scan_frontier_.load(std::memory_order_relaxed);
}

TEMPLATE
size_t CLASS::get_latch_tx() const NOEXCEPT
{
    return latch_tx_.load(std::memory_order_relaxed);
}

TEMPLATE
bool CLASS::is_pooling() const NOEXCEPT
{
    return pooling_.load(std::memory_order_relaxed);
}

TEMPLATE
void CLASS::set_pooling() NOEXCEPT
{
    if (is_pooling())
        return;

    envelope_.pooling = true;
    envelope_.scan_frontier = scan.count().value;
    envelope_.latch_tx = tx.count().value;
    scan_frontier_.store(envelope_.scan_frontier, std::memory_order_relaxed);
    latch_tx_.store(envelope_.latch_tx, std::memory_order_relaxed);
    pooling_.store(true, std::memory_order_relaxed);

    code ec{};
    store_envelope(ec);
}

TEMPLATE
typename CLASS::transactor CLASS::get_transactor() NOEXCEPT
{
    return transactor{ transactor_mutex_ };
}

TEMPLATE
table::ecdsa<Storage>& CLASS::ecdsa_bank(bool bank) NOEXCEPT
{
    return bank ? ecdsa1 : ecdsa0;
}

TEMPLATE
table::silent<Storage>& CLASS::silent_bank(bool bank) NOEXCEPT
{
    return bank ? silent1 : silent0;
}

TEMPLATE
table::silent_prefix& CLASS::silent_prefix_bank(bool bank) NOEXCEPT
{
    return bank ? silent1_prefix : silent0_prefix;
}

TEMPLATE
table::schnorr<Storage>& CLASS::schnorr_bank(bool bank) NOEXCEPT
{
    return bank ? schnorr1 : schnorr0;
}

TEMPLATE
table::prevalid& CLASS::prevalid_bank(bool bank) NOEXCEPT
{
    return bank ? prevalid1 : prevalid0;
}

TEMPLATE
table::points& CLASS::points_bank(bool bank) NOEXCEPT
{
    return bank ? points1 : points0;
}

TEMPLATE
bool CLASS::get_points_bank() const NOEXCEPT
{
    return points_bank_.load(std::memory_order_relaxed);
}

TEMPLATE
table::presilent& CLASS::presilent_bank(bool bank) NOEXCEPT
{
    return bank ? presilent1 : presilent0;
}

// Current is coalesced (confirmed at candidate top) with a recent top header.
TEMPLATE
bool CLASS::is_current() const NOEXCEPT
{
    const auto count = candidate.count();
    if (is_zero(count) || (count != confirmed.count()))
        return false;

    table::header::get_timestamp top{};
    if (!header.get(candidate.at(sub1(count)), top))
        return false;

    const auto now = system::possible_narrow_sign_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    return (top.timestamp >= now) || ((now - top.timestamp) <= currency_seconds);
}

TEMPLATE
void CLASS::set_current(bool current) NOEXCEPT
{
    if (current_.exchange(current) == current)
        return;

    header_head_.current(current);
    input_head_.current(current);
    output_head_.current(current);
    ins_head_.current(current);
    outs_head_.current(current);
    spend_head_.current(current);
    unbound_head_.current(current);
    tx_head_.current(current);
    txs_head_.current(current);
    candidate_head_.current(current);
    confirmed_head_.current(current);
    strong_head_.current(current);
    address_head_.current(current);
    conflict_head_.current(current);
    spender_head_.current(current);
    scan_head_.current(current);
    scan_prefix_head_.current(current);
    ecdsa0_head_.current(current);
    ecdsa1_head_.current(current);
    schnorr0_head_.current(current);
    schnorr1_head_.current(current);
    silent0_head_.current(current);
    silent1_head_.current(current);
    silent0_prefix_head_.current(current);
    silent1_prefix_head_.current(current);
    envelope_head_.current(current);
    prevalid0_head_.current(current);
    prevalid1_head_.current(current);
    presilent0_head_.current(current);
    presilent1_head_.current(current);
    points0_head_.current(current);
    points1_head_.current(current);
    state_head_.current(current);
    pool_head_.current(current);
    wtxid_head_.current(current);
    filter_bk_head_.current(current);
    filter_tx_head_.current(current);
}

TEMPLATE
void CLASS::evaluate_currency() NOEXCEPT
{
    set_current(is_current());
}

} // namespace database
} // namespace libbitcoin

#endif
