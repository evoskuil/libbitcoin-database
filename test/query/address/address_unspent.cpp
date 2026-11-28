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

// get_unconfirmed_unspent
// get_confirmed_unspent
// get_unspent

BC_AUTO_TEST_CASE(query_address__get_unspent__genesis__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(test::setup_three_block_confirmed_address_store(query));

    unspent_outputs out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_unconfirmed_unspent(cancel, out, test::genesis_address0, max_size_t));
    BC_REQUIRE_EQUAL(out.size(), 0u);

    out.clear();
    BC_REQUIRE(!query.get_confirmed_unspent(cancel, out, test::genesis_address0, max_size_t));
    BC_REQUIRE_EQUAL(out.size(), 1u);
    BC_REQUIRE_EQUAL(out.at(0).height, 0u);
    BC_REQUIRE_EQUAL(out.at(0).position, 0u);
    BC_REQUIRE_EQUAL(out.at(0).out.value(), 5'000'000'000u);
    BC_REQUIRE_EQUAL(out.at(0).out.point().index(), 0u);
    BC_REQUIRE_EQUAL(out.at(0).out.point().hash(), test::genesis.transactions_ptr()->at(0)->hash(false));

    out.clear();
    BC_REQUIRE(!query.get_unspent(cancel, out, test::genesis_address0, max_size_t));
    BC_REQUIRE_EQUAL(out.size(), 1u);
    BC_REQUIRE_EQUAL(out.at(0).height, 0u);
    BC_REQUIRE_EQUAL(out.at(0).position, 0u);
    BC_REQUIRE_EQUAL(out.at(0).out.value(), 5'000'000'000u);
    BC_REQUIRE_EQUAL(out.at(0).out.point().index(), 0u);
    BC_REQUIRE_EQUAL(out.at(0).out.point().hash(), test::genesis.transactions_ptr()->at(0)->hash(false));
}

BC_AUTO_TEST_CASE(query_address__get_unspent__turbo_genesis__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(test::setup_three_block_confirmed_address_store(query));

    unspent_outputs out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_unconfirmed_unspent(cancel, out, test::block1a_address0, max_size_t, true));

    // Neither tx4/5 nor the block1b/2b txs are confirmed or pooled.
    BC_CHECK(out.empty());

    out.clear();
    BC_REQUIRE(!query.get_confirmed_unspent(cancel, out, test::block1a_address0, max_size_t, true));

    // block1a is confirmed spent by block2a, block2a/3a are confirmed and contain 3 unspent.
    BC_CHECK_EQUAL(out.size(), 3u);

    BC_CHECK_EQUAL(out.at(0).height, 2u);
    BC_CHECK_EQUAL(out.at(0).position, 0u);
    BC_CHECK_EQUAL(out.at(0).out.value(), 0x81u);
    BC_CHECK_EQUAL(out.at(0).out.point().index(), 0u);
    BC_CHECK_EQUAL(out.at(0).out.point().hash(), test::block2a.transactions_ptr()->at(0)->hash(false));

    BC_CHECK_EQUAL(out.at(1).height, 2u);
    BC_CHECK_EQUAL(out.at(1).position, 1u);
    BC_CHECK_EQUAL(out.at(1).out.value(), 0x81u);
    BC_CHECK_EQUAL(out.at(1).out.point().index(), 0u);
    BC_CHECK_EQUAL(out.at(1).out.point().hash(), test::block2a.transactions_ptr()->at(1)->hash(false));

    BC_CHECK_EQUAL(out.at(2).height, 3u);
    BC_CHECK_EQUAL(out.at(2).position, 0u);
    BC_CHECK_EQUAL(out.at(2).out.value(), 0x83u);
    BC_CHECK_EQUAL(out.at(2).out.point().index(), 0u);
    BC_CHECK_EQUAL(out.at(2).out.point().hash(), test::block3a.transactions_ptr()->at(0)->hash(false));

    out.clear();
    BC_REQUIRE(!query.get_unspent(cancel, out, test::block1a_address0, max_size_t, true));

    // Same as get_confirmed_unspent().
    BC_CHECK_EQUAL(out.size(), 3u);

    BC_CHECK_EQUAL(out.at(0).height, 2u);
    BC_CHECK_EQUAL(out.at(0).position, 0u);
    BC_CHECK_EQUAL(out.at(0).out.value(), 0x81u);
    BC_CHECK_EQUAL(out.at(0).out.point().index(), 0u);
    BC_CHECK_EQUAL(out.at(0).out.point().hash(), test::block2a.transactions_ptr()->at(0)->hash(false));

    BC_CHECK_EQUAL(out.at(1).height, 2u);
    BC_CHECK_EQUAL(out.at(1).position, 1u);
    BC_CHECK_EQUAL(out.at(1).out.value(), 0x81u);
    BC_CHECK_EQUAL(out.at(1).out.point().index(), 0u);
    BC_CHECK_EQUAL(out.at(1).out.point().hash(), test::block2a.transactions_ptr()->at(1)->hash(false));

    BC_CHECK_EQUAL(out.at(2).height, 3u);
    BC_CHECK_EQUAL(out.at(2).position, 0u);
    BC_CHECK_EQUAL(out.at(2).out.value(), 0x83u);
    BC_CHECK_EQUAL(out.at(2).out.point().index(), 0u);
    BC_CHECK_EQUAL(out.at(2).out.point().hash(), test::block3a.transactions_ptr()->at(0)->hash(false));
}

BC_AUTO_TEST_CASE(query_address__get_unspent__confirmable__unspent_only)
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

    unspent_outputs out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_unspent(cancel, out, test::pay_address(), max_size_t, true));
    BC_REQUIRE_EQUAL(out.size(), 1u);
    BC_REQUIRE_EQUAL(out.front().out.point().hash(), child.hash(false));
    BC_REQUIRE_EQUAL(out.front().position, unspent_output::unconfirmed_position);

    BC_REQUIRE(!query.get_unconfirmed_unspent(cancel, out, test::pay_address(), max_size_t, true));
    BC_REQUIRE_EQUAL(out.size(), 1u);
    BC_REQUIRE_EQUAL(out.front().out.point().hash(), child.hash(false));
}

BC_AUTO_TEST_CASE(query_address__get_unspent__unknown_spender__unspent)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));
    BC_REQUIRE(query.set(test::to_pay(test::genesis_coinbase(), 2u)));

    unspent_outputs out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_unspent(cancel, out, test::genesis_address0, max_size_t, true));
    BC_REQUIRE_EQUAL(out.size(), 1u);
    BC_REQUIRE_EQUAL(out.front().height, 0u);
}

BC_AUTO_TEST_CASE(query_address__get_unspent__confirmable_spender__spent)
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

    unspent_outputs out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_unspent(cancel, out, test::genesis_address0, max_size_t, true));
    BC_REQUIRE(out.empty());
}

BC_AUTO_TEST_CASE(query_address__get_unspent__unconfirmable__confirmed_only)
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

    unspent_outputs out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_unspent(cancel, out, test::pay_address(), max_size_t, true));
    BC_REQUIRE_EQUAL(out.size(), 1u);
    BC_REQUIRE_EQUAL(out.front().height, 1u);
    BC_REQUIRE_EQUAL(out.front().out.value(), 3u);
}

// duplicate instances (one tx archived twice, only one instance associated)

BC_AUTO_TEST_CASE(query_address__get_unspent__duplicate_instances__deduplicated)
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

    // block1a output 0 is spent by tx4, whose 0x08 output to the address is unspent.
    unspent_outputs out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE_EQUAL(query.get_unspent(cancel, out, test::block1a_address0, max_size_t, true), error::success);
    BC_REQUIRE_EQUAL(out.size(), 1u);
    BC_REQUIRE_EQUAL(out.at(0).out.point().hash(), test::tx4.hash(false));
    BC_REQUIRE_EQUAL(out.at(0).out.point().index(), 0u);
    BC_REQUIRE_EQUAL(out.at(0).out.value(), 0x08u);
    BC_REQUIRE_EQUAL(out.at(0).height, 2u);
    BC_REQUIRE_EQUAL(out.at(0).position, 0u);

    unspent_outputs confirmed{};
    BC_REQUIRE_EQUAL(query.get_confirmed_unspent(cancel, confirmed, test::block1a_address0, max_size_t, true), error::success);
    BC_REQUIRE_EQUAL(confirmed.size(), 1u);
    BC_REQUIRE_EQUAL(confirmed.at(0).out.point().hash(), test::tx4.hash(false));

    unspent_outputs unconfirmed{};
    BC_REQUIRE_EQUAL(query.get_unconfirmed_unspent(cancel, unconfirmed, test::block1a_address0, max_size_t, true), error::success);
    BC_REQUIRE(unconfirmed.empty());
}

BC_AUTO_TEST_SUITE_END()
