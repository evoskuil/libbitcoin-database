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
#include "../../test.hpp"
#include "../../mocks/blocks.hpp"
#include "../../mocks/chunk_store.hpp"
#include "../../mocks/pooling.hpp"

BC_FIXTURE_TEST_SUITE(query_address_tests, test::directory_setup_fixture)

// get_history

BC_AUTO_TEST_CASE(query_address__get_history__genesis__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    histories out{};
    const std::atomic_bool cancel{};
    const auto& hash = test::genesis_address0;
    BC_CHECK(!query.get_unconfirmed_history(cancel, out, hash));
    BC_CHECK_EQUAL(out.size(), 0u);

    out.clear();
    height_link cursor{};
    BC_CHECK(!query.get_confirmed_history(cancel, cursor, out, hash));
    BC_REQUIRE_EQUAL(out.size(), 1u);
    BC_CHECK_EQUAL(out.at(0).fee, history::missing_prevout);
    BC_CHECK_EQUAL(out.at(0).position, 0u);
    BC_CHECK_EQUAL(out.at(0).tx.height(), 0u);
    BC_CHECK_EQUAL(out.at(0).tx.hash(), test::genesis.transactions_ptr()->at(0)->hash(false));

    out.clear();
    cursor = {};
    BC_CHECK(!query.get_history(cancel, cursor, out, hash));
    BC_REQUIRE_EQUAL(out.size(), 1u);
    BC_CHECK_EQUAL(out.at(0).fee, history::missing_prevout);
    BC_CHECK_EQUAL(out.at(0).position, 0u);
    BC_CHECK_EQUAL(out.at(0).tx.height(), 0u);
    BC_CHECK_EQUAL(out.at(0).tx.hash(), test::genesis.transactions_ptr()->at(0)->hash(false));
}

BC_AUTO_TEST_CASE(query_address__get_history__turbo_block1a_address0__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    histories out{};
    height_link cursor{};
    const std::atomic_bool cancel{};
    BC_CHECK(!query.get_history(cancel, cursor, out, test::block1a_address0, max_size_t, true));
    BC_REQUIRE_EQUAL(out.size(), 4u);

    // Identities (not part of sort).
    BC_CHECK_EQUAL(out.at(0).tx.hash(), test::block1a.transactions_ptr()->at(0)->hash(false)); // tx1/tx1 (same)
    BC_CHECK_EQUAL(out.at(1).tx.hash(), test::block2a.transactions_ptr()->at(0)->hash(false)); // tx2/tx2 (same)
    BC_CHECK_EQUAL(out.at(2).tx.hash(), test::block2a.transactions_ptr()->at(1)->hash(false)); // tx3/tx3 (same)
    BC_CHECK_EQUAL(out.at(3).tx.hash(), test::block3a.transactions_ptr()->at(0)->hash(false)); // tx6/tx6 (same)

    // Confirmed by height ascending, unknown excluded.
    BC_CHECK_EQUAL(out.at(0).tx.height(), 1u);                             // tx1
    BC_CHECK_EQUAL(out.at(1).tx.height(), 2u);                             // tx2
    BC_CHECK_EQUAL(out.at(2).tx.height(), 2u);                             // tx3
    BC_CHECK_EQUAL(out.at(3).tx.height(), 3u);                             // tx6

    // Confirmed height by block position.
    BC_CHECK_EQUAL(out.at(0).position, 0u);                                // tx1
    BC_CHECK_EQUAL(out.at(1).position, 0u);                                // tx2
    BC_CHECK_EQUAL(out.at(2).position, 1u);                                // tx3
    BC_CHECK_EQUAL(out.at(3).position, 0u);                                // tx6

    // Fee (not part of sort).
    BC_CHECK_EQUAL(out.at(0).fee, history::missing_prevout);               // tx1
    BC_CHECK_EQUAL(out.at(1).fee, history::missing_prevout);               // tx2
    BC_CHECK_EQUAL(out.at(2).fee, history::missing_prevout);               // tx3
    BC_CHECK_EQUAL(out.at(3).fee, history::missing_prevout);               // tx6
}

BC_AUTO_TEST_CASE(query_address__get_history__limited__depth_limited_empty)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    histories out{};
    height_link cursor{};
    const std::atomic_bool cancel{};
    const auto& hash = test::block1a_address0;

    // Limited returned when limit precludes reaching the address cursor (or terminal).
    BC_CHECK_EQUAL(query.get_history(cancel, cursor, out, hash, 0, true), error::depth_limited);
    BC_CHECK(out.empty());

    cursor = {};
    BC_CHECK_EQUAL(query.get_history(cancel, cursor, out, hash, 8, true), error::depth_limited);
    BC_CHECK(out.empty());

    cursor = {};
    BC_CHECK_EQUAL(query.get_history(cancel, cursor, out, hash, 9, true), error::success);
    BC_CHECK_EQUAL(out.size(), 4u);

    cursor = {};
    BC_CHECK_EQUAL(query.get_history(cancel, cursor, out, hash, 100, true), error::success);
    BC_CHECK_EQUAL(out.size(), 4u);
}

BC_AUTO_TEST_CASE(query_address__get_history__start__excludes_prior)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    histories out{};
    height_link cursor{ 42 };
    const std::atomic_bool cancel{};
    const auto& hash = test::block1a_address0;

    // Limited returned when limit precludes reaching the address cursor (or terminal).
    BC_CHECK_EQUAL(query.get_history(cancel, cursor, out, hash, 0, true), error::depth_limited);
    BC_CHECK(out.empty());

    cursor = 42;
    BC_CHECK_EQUAL(query.get_history(cancel, cursor, out, hash, 8, true), error::depth_limited);
    BC_CHECK(out.empty());

    // None at or above start.
    cursor = 42;
    BC_CHECK_EQUAL(query.get_history(cancel, cursor, out, hash, 9, true), error::success);
    BC_CHECK(out.empty());
        
    // Only excludes genesis.
    cursor = 1;
    BC_CHECK_EQUAL(query.get_history(cancel, cursor, out, hash, 100, true), error::success);
    BC_CHECK_EQUAL(out.size(), 4u);

    // Excludes block1a which has one confirmed touching tx.
    cursor = 2;
    BC_CHECK_EQUAL(query.get_history(cancel, cursor, out, hash, 100, true), error::success);
    BC_CHECK_EQUAL(out.size(), 3u);
}

// ----------------------------------------------------------------------------

// block1a_address0 has 9 instances `script{ { { opcode::pick } } }`.
// 7 outputs are in individual txs, 2 are in tx bk1b0.
// 4 owning txs are confirmed (bk1a/bk2a/bk3a).
// 4 owning txs are unconfirmed (bk1b/bk2b/tx4/tx5).
// 6 outputs are spent, (bk2a/bk3a/tx4/tx5/bk2b00/bk2b01).
// 2 spend txs are confirmed, (bk2a/bk3a).
// 3 spend txs are unconfirmed, (tx4/tx5/bk2b0(2)).
//
// One owner owns two (one owner removed by deduplication).
// All spenders are also owners (all spends removed by deduplication).
// 4 owns + 2 spend txs are confirmed.
// 4 owns + 2 spend txs are confirmed.
//
// 4 unconfirmed:
// tx4     spends 1u + owns 1u (+1u) [unconfirmed rooted].
// tx5     spends 1u + owns 1u (+1u) [unconfirmed rooted].
// bk1btx0             owns 2u (+1u) [unconfirmed unrooted].
// bk2btx0 spends 2u + owns 1u (+1u) [unconfirmed unrooted].
//
// 4 confirmed:
// bk1atx0             owns 1c (+1c) [confirmed].
// bk2atx0 spends 1c + owns 1c (+1c) [confirmed].
// bk2atx1             owns 1c (+1c) [confirmed].
// bk3atx0 spends 1c + owns 1c (+1c) [confirmed].

// get_unconfirmed_history

BC_AUTO_TEST_CASE(query_address__get_unconfirmed_history__unknown__empty)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    histories out{};
    const std::atomic_bool cancel{};
    BC_CHECK(!query.get_unconfirmed_history(cancel, out, test::block1a_address0, max_size_t, true));
    BC_CHECK(out.empty());
}

BC_AUTO_TEST_CASE(query_address__get_unconfirmed_history__confirmable__rooted_before_unrooted)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    store.set_pooling();
    const auto parent = test::to_pay(test::genesis_coinbase(), 2u);
    const auto child = test::to_pay(parent.hash(false), 1u);
    BC_REQUIRE(test::archive_pool(query, parent));
    BC_REQUIRE(test::archive_pool(query, child));

    histories out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_unconfirmed_history(cancel, out, test::pay_address(), max_size_t, true));
    BC_REQUIRE_EQUAL(out.size(), 2u);
    BC_REQUIRE_EQUAL(out.at(0).tx.hash(), parent.hash(false));
    BC_REQUIRE_EQUAL(out.at(0).tx.height(), history::rooted_height);
    BC_REQUIRE_EQUAL(out.at(0).position, history::unconfirmed_position);
    BC_REQUIRE_EQUAL(out.at(1).tx.hash(), child.hash(false));
    BC_REQUIRE_EQUAL(out.at(1).tx.height(), history::unrooted_height);
    BC_REQUIRE_EQUAL(out.at(1).position, history::unconfirmed_position);
}

BC_AUTO_TEST_CASE(query_address__get_unconfirmed_history__unconfirmable__empty)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    store.set_pooling();
    const auto parent = test::to_pay(test::genesis_coinbase(), 2u);
    const auto child = test::to_pay(parent.hash(false), 1u);
    BC_REQUIRE(test::archive_pool(query, parent));
    BC_REQUIRE(test::archive_pool(query, child));
    BC_REQUIRE(!test::archive_confirm(query, test::to_pay(test::genesis_coinbase(), 3u)).is_terminal());

    histories out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_unconfirmed_history(cancel, out, test::pay_address(), max_size_t, true));
    BC_REQUIRE(out.empty());
}

BC_AUTO_TEST_CASE(query_address__get_history__unconfirmable__confirmed_only)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    store.set_pooling();
    const auto parent = test::to_pay(test::genesis_coinbase(), 2u);
    const auto child = test::to_pay(parent.hash(false), 1u);
    BC_REQUIRE(test::archive_pool(query, parent));
    BC_REQUIRE(test::archive_pool(query, child));
    BC_REQUIRE(!test::archive_confirm(query, test::to_pay(test::genesis_coinbase(), 3u)).is_terminal());

    histories out{};
    height_link cursor{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_history(cancel, cursor, out, test::pay_address(), max_size_t, true));
    BC_REQUIRE_EQUAL(out.size(), 1u);
    BC_REQUIRE_EQUAL(out.front().tx.height(), 1u);
    BC_REQUIRE_EQUAL(out.front().position, 1u);
}

BC_AUTO_TEST_CASE(query_address__get_unconfirmed_history__limited__depth_limited_empty)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    histories out{};
    const std::atomic_bool cancel{};
    const auto& hash = test::block1a_address0;

    // Limited returned when limit precludes reaching the address cursor (or terminal).
    BC_CHECK_EQUAL(query.get_unconfirmed_history(cancel, out, hash, 0, true), error::depth_limited);
    BC_CHECK(out.empty());

    BC_CHECK_EQUAL(query.get_unconfirmed_history(cancel, out, hash, 8, true), error::depth_limited);
    BC_CHECK(out.empty());

    BC_CHECK_EQUAL(query.get_unconfirmed_history(cancel, out, hash, 9, true), error::success);
    BC_CHECK(out.empty());

    BC_CHECK_EQUAL(query.get_unconfirmed_history(cancel, out, hash, 100, true), error::success);
    BC_CHECK(out.empty());
}

// get_confirmed_history

BC_AUTO_TEST_CASE(query_address__get_confirmed_history__turbo_block1a_address0__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    histories out{};
    height_link cursor{};
    const std::atomic_bool cancel{};
    BC_CHECK(!query.get_confirmed_history(cancel, cursor, out, test::block1a_address0, max_size_t, true));
    BC_REQUIRE_EQUAL(out.size(), 4u);

    // Identities (not part of sort).
    BC_CHECK_EQUAL(out.at(0).tx.hash(), test::block1a.transactions_ptr()->at(0)->hash(false)); // tx1/tx1 (same)
    BC_CHECK_EQUAL(out.at(1).tx.hash(), test::block2a.transactions_ptr()->at(0)->hash(false)); // tx2/tx2 (same)
    BC_CHECK_EQUAL(out.at(2).tx.hash(), test::block2a.transactions_ptr()->at(1)->hash(false)); // tx3/tx3 (same)
    BC_CHECK_EQUAL(out.at(3).tx.hash(), test::block3a.transactions_ptr()->at(0)->hash(false)); // tx6/tx6 (same)

    // Confirmed by height ascending.
    BC_CHECK_EQUAL(out.at(0).tx.height(), 1u);                             // tx1
    BC_CHECK_EQUAL(out.at(1).tx.height(), 2u);                             // tx2
    BC_CHECK_EQUAL(out.at(2).tx.height(), 2u);                             // tx3
    BC_CHECK_EQUAL(out.at(3).tx.height(), 3u);                             // tx6

    // Confirmed height by block position.
    BC_CHECK_EQUAL(out.at(0).position, 0u);                                // tx1
    BC_CHECK_EQUAL(out.at(1).position, 0u);                                // tx2
    BC_CHECK_EQUAL(out.at(2).position, 1u);                                // tx3
    BC_CHECK_EQUAL(out.at(3).position, 0u);                                // tx6

    // Fee (not part of sort).
    BC_CHECK_EQUAL(out.at(0).fee, history::missing_prevout);               // tx1
    BC_CHECK_EQUAL(out.at(1).fee, history::missing_prevout);               // tx2
    BC_CHECK_EQUAL(out.at(2).fee, history::missing_prevout);               // tx3
    BC_CHECK_EQUAL(out.at(3).fee, history::missing_prevout);               // tx6
}

BC_AUTO_TEST_CASE(query_address__get_confirmed_history__limited__depth_limited_empty)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    histories out{};
    height_link cursor{};
    const std::atomic_bool cancel{};
    const auto& hash = test::block1a_address0;

    // Limited returned when limit precludes reaching address cursor (or terminal).
    BC_CHECK_EQUAL(query.get_confirmed_history(cancel, cursor, out, hash, 0, true), error::depth_limited);
    BC_CHECK(out.empty());

    cursor = {};
    BC_CHECK_EQUAL(query.get_confirmed_history(cancel, cursor, out, hash, 8, true), error::depth_limited);
    BC_CHECK(out.empty());

    cursor = {};
    BC_CHECK_EQUAL(query.get_confirmed_history(cancel, cursor, out, hash, 9, true), error::success);
    BC_CHECK_EQUAL(out.size(), 4u);

    cursor = {};
    BC_CHECK_EQUAL(query.get_confirmed_history(cancel, cursor, out, hash, 100, true), error::success);
    BC_CHECK_EQUAL(out.size(), 4u);
}

BC_AUTO_TEST_CASE(query_address__get_confirmed_history__start__excludes_prior)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    histories out{};
    height_link cursor{ 42 };
    const std::atomic_bool cancel{};
    const auto& hash = test::block1a_address0;

    // Limited returned when limit precludes reaching address cursor (or terminal).
    BC_CHECK_EQUAL(query.get_confirmed_history(cancel, cursor, out, hash, 0, true), error::depth_limited);
    BC_CHECK(out.empty());

    // Only unconfirmed.
    cursor = 42;
    BC_CHECK_EQUAL(query.get_confirmed_history(cancel, cursor, out, hash, 8, true), error::depth_limited);
    BC_CHECK(out.empty());

    // Only excludes genesis.
    cursor = 1;
    BC_CHECK_EQUAL(query.get_confirmed_history(cancel, cursor, out, hash, 9, true), error::success);
    BC_CHECK_EQUAL(out.size(), 4u);

    // Excludes block1a which has one confirmed touching tx.
    cursor = 2;
    BC_CHECK_EQUAL(query.get_confirmed_history(cancel, cursor, out, hash, 100, true), error::success);
    BC_CHECK_EQUAL(out.size(), 3u);
}

// get_tx_history

BC_AUTO_TEST_CASE(query_address__get_tx_history__genesis__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    const auto hash = test::genesis.transactions_ptr()->at(0)->hash(false);
    auto history = query.get_tx_history(0);
    BC_CHECK(history.valid());
    BC_CHECK_EQUAL(history.fee, history::missing_prevout);
    BC_CHECK_EQUAL(history.position, 0u);
    BC_CHECK_EQUAL(history.tx.height(), 0u);
    BC_CHECK_EQUAL(history.tx.hash(), hash);
}

BC_AUTO_TEST_CASE(query_address__get_tx_history__confirmed__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    const auto hash = test::block2a.transactions_ptr()->at(0)->hash(false);
    const auto history = query.get_tx_history(query.to_tx(hash));
    BC_CHECK(history.valid());
    BC_CHECK_EQUAL(history.fee, history::missing_prevout); // spend > value
    BC_CHECK_EQUAL(history.position, 0u);
    BC_CHECK_EQUAL(history.tx.height(), 2u);
    BC_CHECK_EQUAL(history.tx.hash(), hash);
}

BC_AUTO_TEST_CASE(query_address__get_tx_history__confirmed_missing_prevout__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    const auto hash = test::block2a.transactions_ptr()->at(1)->hash(false);
    const auto history = query.get_tx_history(query.to_tx(hash));
    BC_CHECK(history.valid());
    BC_CHECK_EQUAL(history.fee, history::missing_prevout); // missing prevout
    BC_CHECK_EQUAL(history.position, 1u);
    BC_CHECK_EQUAL(history.tx.height(), 2u);
    BC_CHECK_EQUAL(history.tx.hash(), hash);
}

BC_AUTO_TEST_CASE(query_address__get_tx_history__unknown__invalid)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    const auto history = query.get_tx_history(query.to_tx(test::tx4.hash(false)));
    BC_CHECK(!history.valid());
    BC_CHECK_EQUAL(history.position, history::unconfirmed_position);
}

BC_AUTO_TEST_CASE(query_address__get_tx_history__rooted__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    store.set_pooling();
    const auto parent = test::to_pay(test::genesis_coinbase(), 2u);
    const auto child = test::to_pay(parent.hash(false), 1u);
    BC_REQUIRE(test::archive_pool(query, parent));
    BC_REQUIRE(test::archive_pool(query, child));

    const auto history = query.get_tx_history(query.to_tx(parent.hash(false)));
    BC_REQUIRE(history.valid());
    BC_REQUIRE_EQUAL(history.position, history::unconfirmed_position);
    BC_REQUIRE_EQUAL(history.tx.height(), history::rooted_height);
    BC_REQUIRE_EQUAL(history.tx.hash(), parent.hash(false));
}

BC_AUTO_TEST_CASE(query_address__get_tx_history__unrooted__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    store.set_pooling();
    const auto parent = test::to_pay(test::genesis_coinbase(), 2u);
    const auto child = test::to_pay(parent.hash(false), 1u);
    BC_REQUIRE(test::archive_pool(query, parent));
    BC_REQUIRE(test::archive_pool(query, child));

    const auto history = query.get_tx_history(query.to_tx(child.hash(false)));
    BC_REQUIRE(history.valid());
    BC_REQUIRE_EQUAL(history.position, history::unconfirmed_position);
    BC_REQUIRE_EQUAL(history.tx.height(), history::unrooted_height);
    BC_REQUIRE_EQUAL(history.tx.hash(), child.hash(false));
}

BC_AUTO_TEST_CASE(query_address__get_tx_history__unconfirmable__invalid)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    store.set_pooling();
    const auto parent = test::to_pay(test::genesis_coinbase(), 2u);
    const auto child = test::to_pay(parent.hash(false), 1u);
    BC_REQUIRE(test::archive_pool(query, parent));
    BC_REQUIRE(test::archive_pool(query, child));
    BC_REQUIRE(!test::archive_confirm(query, test::to_pay(test::genesis_coinbase(), 3u)).is_terminal());

    BC_REQUIRE(!query.get_tx_history(query.to_tx(parent.hash(false))).valid());
    BC_REQUIRE(!query.get_tx_history(query.to_tx(child.hash(false))).valid());
}

// get_spenders_history1
// get_spenders_history2

BC_AUTO_TEST_CASE(query_address__get_spenders_history__bogus__empty)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    auto histories = query.get_spenders_history({ 0x42 }, 0);
    BC_CHECK(histories.empty());

    histories = query.get_spenders_history({ { 0x42 }, 0 });
    BC_CHECK(histories.empty());
}

BC_AUTO_TEST_CASE(query_address__get_spenders_history__genesis__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    const auto hash = test::genesis.transactions_ptr()->at(0)->hash(false);
    auto histories = query.get_spenders_history(hash, 0);
    BC_CHECK(histories.empty());

    histories = query.get_spenders_history({ hash, 0 });
    BC_CHECK(histories.empty());
}

BC_AUTO_TEST_CASE(query_address__get_spenders_history__confirmed__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    using namespace system;
    const auto hash = test::block1a.transactions_ptr()->at(0)->hash(false);
    const auto histories = query.get_spenders_history(hash, 0);
    BC_CHECK_EQUAL(histories.size(), 2u);

    BC_CHECK(histories.at(0).valid());
    BC_CHECK_EQUAL(histories.at(0).fee, history::missing_prevout); // spend > value
    BC_CHECK_EQUAL(histories.at(0).position, 0u);
    BC_CHECK_EQUAL(histories.at(0).tx.height(), 2u);
    BC_CHECK_EQUAL(histories.at(0).tx.hash(), test::block2a.transactions_ptr()->at(0)->hash(false));

    BC_CHECK(histories.at(1).valid());
    BC_CHECK_EQUAL(histories.at(1).fee, history::missing_prevout); // spend > value
    BC_CHECK_EQUAL(histories.at(1).position, 0u);
    BC_CHECK_EQUAL(histories.at(1).tx.height(), 3u);
    BC_CHECK_EQUAL(histories.at(1).tx.hash(), test::block3a.transactions_ptr()->at(0)->hash(false));
}

BC_AUTO_TEST_CASE(query_address__get_spenders_history__confirmable__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    store.set_pooling();
    const auto parent = test::to_pay(test::genesis_coinbase(), 2u);
    const auto child = test::to_pay(parent.hash(false), 1u);
    BC_REQUIRE(test::archive_pool(query, parent));
    BC_REQUIRE(test::archive_pool(query, child));

    const auto histories = query.get_spenders_history(parent.hash(false), 0);
    BC_REQUIRE_EQUAL(histories.size(), 1u);
    BC_REQUIRE_EQUAL(histories.front().tx.hash(), child.hash(false));
    BC_REQUIRE_EQUAL(histories.front().position, history::unconfirmed_position);
}

BC_AUTO_TEST_CASE(query_address__get_spenders_history__unconfirmable__empty)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    store.set_pooling();
    const auto parent = test::to_pay(test::genesis_coinbase(), 2u);
    const auto child = test::to_pay(parent.hash(false), 1u);
    BC_REQUIRE(test::archive_pool(query, parent));
    BC_REQUIRE(test::archive_pool(query, child));
    BC_REQUIRE(!test::archive_confirm(query, test::to_pay(test::genesis_coinbase(), 3u)).is_terminal());

    BC_REQUIRE(query.get_spenders_history(parent.hash(false), 0).empty());
}

// duplicate instances (one tx archived twice, only one instance associated)

BC_AUTO_TEST_CASE(query_address__get_tx_history__duplicate_instances__same_entry)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(!store.is_pooling());
    BC_REQUIRE(query.initialize(test::genesis));
    BC_REQUIRE(query.set(test::block1a, context{ 0, 1, 0 }, {}, false, false));
    BC_REQUIRE(query.set_strong(1));
    BC_REQUIRE(query.push_confirmed(1, false));
    BC_REQUIRE(query.set(test::tx4));
    const auto loose = query.to_tx(test::tx4.hash(false));
    BC_REQUIRE(query.set(test::block_spend_1a, context{ 0, 2, 0 }, {}, false, false));
    BC_REQUIRE(query.set_strong(2));
    BC_REQUIRE(query.push_confirmed(2, false));
    const auto stored = query.get_position_tx(query.to_header(test::block_spend_1a.hash()), 0);
    BC_REQUIRE(stored != loose);

    const auto expected = query.get_tx_history(stored);
    BC_REQUIRE(expected.valid());
    BC_REQUIRE(expected.confirmed());
    BC_REQUIRE_EQUAL(expected.tx.height(), 2u);
    BC_REQUIRE_EQUAL(expected.position, 0u);

    const auto duplicate = query.get_tx_history(loose);
    BC_REQUIRE(!duplicate.fault());
    BC_REQUIRE(duplicate.valid());
    BC_REQUIRE(duplicate == expected);
    BC_REQUIRE_EQUAL(duplicate.tx.height(), 2u);
    BC_REQUIRE_EQUAL(duplicate.position, 0u);

    const auto confirmed = query.get_tx_confirmed_history(loose);
    BC_REQUIRE(!confirmed.fault());
    BC_REQUIRE(confirmed == expected);
}

BC_AUTO_TEST_CASE(query_address__get_history__duplicate_instances__deduplicated)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(!store.is_pooling());
    BC_REQUIRE(query.initialize(test::genesis));
    BC_REQUIRE(query.set(test::block1a, context{ 0, 1, 0 }, {}, false, false));
    BC_REQUIRE(query.set_strong(1));
    BC_REQUIRE(query.push_confirmed(1, false));
    BC_REQUIRE(query.set(test::tx4));
    BC_REQUIRE(query.set(test::block_spend_1a, context{ 0, 2, 0 }, {}, false, false));
    BC_REQUIRE(query.set_strong(2));
    BC_REQUIRE(query.push_confirmed(2, false));
    BC_REQUIRE_EQUAL(query.to_duplicates(test::tx4.hash(false)).size(), 2u);

    histories out{};
    height_link cursor{};
    const std::atomic_bool cancel{};
    BC_REQUIRE_EQUAL(query.get_history(cancel, cursor, out, test::block1a_address0, max_size_t, true), error::success);

    // block1a coinbase (funds address0) and tx4 (spends it, pays address0), once each.
    BC_REQUIRE_EQUAL(out.size(), 2u);
    BC_REQUIRE_EQUAL(out.at(0).tx.hash(), test::block1a.transactions_ptr()->at(0)->hash(false));
    BC_REQUIRE_EQUAL(out.at(0).tx.height(), 1u);
    BC_REQUIRE_EQUAL(out.at(1).tx.hash(), test::tx4.hash(false));
    BC_REQUIRE_EQUAL(out.at(1).tx.height(), 2u);
    BC_REQUIRE_EQUAL(out.at(1).position, 0u);

    histories confirmed{};
    height_link confirmed_cursor{};
    BC_REQUIRE_EQUAL(query.get_confirmed_history(cancel, confirmed_cursor, confirmed, test::block1a_address0, max_size_t, true), error::success);
    BC_REQUIRE_EQUAL(confirmed.size(), 2u);
    BC_REQUIRE_EQUAL(confirmed.at(1).tx.hash(), test::tx4.hash(false));
}

// get_confirmed_touches

BC_AUTO_TEST_CASE(query_address__get_confirmed_touches__block1a_address0__block_order_flagged)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    touches out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE_EQUAL(query.get_confirmed_touches(cancel, out, test::block1a_address0), error::success);

    // tx1 funds, tx2 spends tx1 and funds, tx3 funds, tx6 spends tx1 and funds.
    BC_REQUIRE_EQUAL(out.size(), 4u);
    BC_CHECK_EQUAL(out.at(0).height, 1u);
    BC_CHECK_EQUAL(out.at(0).position, 0u);
    BC_CHECK_EQUAL(query.get_tx_key(out.at(0).link), test::block1a.transactions_ptr()->at(0)->hash(false));
    BC_CHECK(out.at(0).funds);
    BC_CHECK(!out.at(0).spends);
    BC_CHECK_EQUAL(out.at(1).height, 2u);
    BC_CHECK_EQUAL(out.at(1).position, 0u);
    BC_CHECK_EQUAL(query.get_tx_key(out.at(1).link), test::block2a.transactions_ptr()->at(0)->hash(false));
    BC_CHECK(out.at(1).funds);
    BC_CHECK(out.at(1).spends);
    BC_CHECK_EQUAL(out.at(2).height, 2u);
    BC_CHECK_EQUAL(out.at(2).position, 1u);
    BC_CHECK_EQUAL(query.get_tx_key(out.at(2).link), test::block2a.transactions_ptr()->at(1)->hash(false));
    BC_CHECK(out.at(2).funds);
    BC_CHECK(!out.at(2).spends);
    BC_CHECK_EQUAL(out.at(3).height, 3u);
    BC_CHECK_EQUAL(out.at(3).position, 0u);
    BC_CHECK_EQUAL(query.get_tx_key(out.at(3).link), test::block3a.transactions_ptr()->at(0)->hash(false));
    BC_CHECK(out.at(3).funds);
    BC_CHECK(out.at(3).spends);
}

BC_AUTO_TEST_CASE(query_address__get_confirmed_touches__turbo__same)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    touches out{};
    touches turbo{};
    const std::atomic_bool cancel{};
    BC_REQUIRE_EQUAL(query.get_confirmed_touches(cancel, out, test::block1a_address0), error::success);
    BC_REQUIRE_EQUAL(query.get_confirmed_touches(cancel, turbo, test::block1a_address0, zero, max_size_t, max_size_t, true), error::success);
    BC_REQUIRE_EQUAL(turbo.size(), 4u);
    BC_CHECK(turbo == out);
}

BC_AUTO_TEST_CASE(query_address__get_confirmed_touches__height_range__excludes_outside)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    touches out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE_EQUAL(query.get_confirmed_touches(cancel, out, test::block1a_address0, 2, 2), error::success);
    BC_REQUIRE_EQUAL(out.size(), 2u);
    BC_CHECK_EQUAL(out.at(0).height, 2u);
    BC_CHECK_EQUAL(out.at(0).position, 0u);
    BC_CHECK_EQUAL(out.at(1).height, 2u);
    BC_CHECK_EQUAL(out.at(1).position, 1u);
}

BC_AUTO_TEST_CASE(query_address__get_confirmed_touches__limited__depth_limited_empty)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    touches out{};
    const std::atomic_bool cancel{};
    BC_CHECK_EQUAL(query.get_confirmed_touches(cancel, out, test::block1a_address0, zero, max_size_t, 8), error::depth_limited);
    BC_CHECK(out.empty());
}

BC_AUTO_TEST_CASE(query_address__get_confirmed_touches__canceled__query_canceled)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    touches out{};
    const std::atomic_bool cancel{ true };
    BC_CHECK_EQUAL(query.get_confirmed_touches(cancel, out, test::block1a_address0), error::query_canceled);
    BC_CHECK(out.empty());
}

BC_AUTO_TEST_CASE(query_address__get_confirmed_touches__prevout__confirmed_spenders)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    // tx2 (block2a) and tx6 (block3a) spend tx1:0.
    touches out{};
    const system::chain::point prevout{ test::block1a.transactions_ptr()->at(0)->hash(false), 0 };
    BC_REQUIRE_EQUAL(query.get_confirmed_touches(out, prevout), error::success);
    BC_REQUIRE_EQUAL(out.size(), 2u);
    BC_CHECK_EQUAL(out.at(0).height, 2u);
    BC_CHECK_EQUAL(out.at(0).position, 0u);
    BC_CHECK(!out.at(0).funds);
    BC_CHECK(out.at(0).spends);
    BC_CHECK_EQUAL(out.at(1).height, 3u);
    BC_CHECK_EQUAL(out.at(1).position, 0u);
    BC_CHECK(out.at(1).spends);
}

BC_AUTO_TEST_CASE(query_address__get_confirmed_touches__prevout_height_range__excludes_outside)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_CHECK(!store.create(test::events_handler));
    BC_CHECK(test::setup_three_block_confirmed_address_store(query));

    touches out{};
    const system::chain::point prevout{ test::block1a.transactions_ptr()->at(0)->hash(false), 0 };
    BC_REQUIRE_EQUAL(query.get_confirmed_touches(out, prevout, 3, 3), error::success);
    BC_REQUIRE_EQUAL(out.size(), 1u);
    BC_CHECK_EQUAL(out.at(0).height, 3u);
}

BC_AUTO_TEST_SUITE_END()
