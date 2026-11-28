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

// get_confirmed_balance

BC_AUTO_TEST_CASE(query_address__get_confirmed_balance__turbo_block1a_address0_confirmed_blocks__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));

    // block1a_address0 has 9 instances `script{ { { opcode::pick } } }`.
    // block1a_address0 has 4 confirmed instances (blocks 1a/2a/3a).
    // block1a (value: 0x18) is confirmed spent by block2a0.
    // block2a (value: 0x81) is confirmed unspent.
    // block2a (value: 0x81) is confirmed unspent.
    // block3a (value: 0x83) is confirmed unspent.
    BC_REQUIRE(test::setup_three_block_confirmed_address_store(query));

    const std::atomic_bool cancel{};
    uint64_t confirmed{};
    BC_REQUIRE(!query.get_confirmed_balance(cancel, confirmed, test::block1a_address0, max_size_t, true));
    BC_REQUIRE_EQUAL(confirmed, 389u);
}

BC_AUTO_TEST_CASE(query_address__get_confirmed_balance__turbo_block1a_address0_unconfirmed_blocks__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));

    // block1a_address0 has 6 instances `script{ { { opcode::pick } } }`.
    BC_REQUIRE(test::setup_three_block_unconfirmed_address_store(query));

    const std::atomic_bool cancel{};
    uint64_t confirmed{};
    BC_REQUIRE(!query.get_confirmed_balance(cancel, confirmed, test::block1a_address0, max_size_t, true));
    BC_REQUIRE_EQUAL(confirmed, 0u);
}

BC_AUTO_TEST_CASE(query_address__get_confirmed_balance__block1a_address1_confirmed_blocks__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));

    // block1a_address1 is unique to that output instance `script{ { { opcode::roll } } }`.
    // block1a_address1 is spent by block2a1, tx4-1, and (confirmed double spent by) block3a1.
    BC_REQUIRE(test::setup_three_block_confirmed_address_store(query));

    const std::atomic_bool cancel{};
    uint64_t confirmed{};
    BC_REQUIRE(!query.get_confirmed_balance(cancel, confirmed, test::block1a_address1, max_size_t));
    BC_REQUIRE_EQUAL(confirmed, 0u);
}

// duplicate instances (one tx archived twice, only one instance associated)

BC_AUTO_TEST_CASE(query_address__get_confirmed_balance__duplicate_instances__counted_once)
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
    const std::atomic_bool cancel{};
    uint64_t balance{};
    BC_REQUIRE_EQUAL(query.get_confirmed_balance(cancel, balance, test::block1a_address0, max_size_t, true), error::success);
    BC_REQUIRE_EQUAL(balance, 0x08u);
}

// get_address_totals

BC_AUTO_TEST_CASE(query_address__get_address_totals__confirmed_blocks__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(test::setup_three_block_confirmed_address_store(query));

    address_totals confirmed{};
    pool_receipts receipts{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_address_totals(cancel, confirmed, receipts, test::block1a_address0, max_size_t));
    BC_REQUIRE_EQUAL(confirmed.funded_count, 4u);
    BC_REQUIRE_EQUAL(confirmed.funded_value, 0x18u + 0x81u + 0x81u + 0x83u);
    BC_REQUIRE_EQUAL(confirmed.spent_count, 1u);
    BC_REQUIRE_EQUAL(confirmed.spent_value, 0x18u);
    BC_REQUIRE(receipts.empty());
}

BC_AUTO_TEST_CASE(query_address__get_address_totals__unconfirmed_blocks__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(test::setup_three_block_unconfirmed_address_store(query));

    address_totals confirmed{};
    pool_receipts receipts{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_address_totals(cancel, confirmed, receipts, test::block1a_address0, max_size_t));
    BC_REQUIRE_EQUAL(confirmed.funded_count, 0u);
    BC_REQUIRE_EQUAL(confirmed.spent_count, 0u);
    BC_REQUIRE(receipts.empty());
}

BC_AUTO_TEST_CASE(query_address__get_address_totals__confirmable__unconfirmed)
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

    address_totals confirmed{};
    pool_receipts receipts{};
    const std::atomic_bool cancel{};
    address_totals worst{};
    address_totals best{};
    BC_REQUIRE(!query.get_address_totals(cancel, confirmed, receipts, test::pay_address(), max_size_t));
    BC_REQUIRE(!query.get_unconfirmed_totals(worst, best, receipts, 8u));
    BC_REQUIRE_EQUAL(confirmed.funded_count, 0u);
    BC_REQUIRE_EQUAL(worst.funded_count, 2u);
    BC_REQUIRE_EQUAL(worst.funded_value, 3u);
    BC_REQUIRE_EQUAL(worst.spent_count, 1u);
    BC_REQUIRE_EQUAL(worst.spent_value, 2u);
}

BC_AUTO_TEST_CASE(query_address__get_address_totals__confirmed_spent_by_confirmable__unconfirmed_spend)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    store.set_pooling();
    const auto parent = test::to_pay(test::genesis_coinbase(), 3u);
    const auto child = test::to_pay(parent.hash(false), 1u);
    BC_REQUIRE(!test::archive_confirm(query, parent).is_terminal());
    BC_REQUIRE(test::archive_pool(query, child));

    address_totals confirmed{};
    address_totals worst{};
    address_totals best{};
    pool_receipts receipts{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_address_totals(cancel, confirmed, receipts, test::pay_address(), max_size_t));
    BC_REQUIRE(!query.get_unconfirmed_totals(worst, best, receipts, 8u));
    BC_REQUIRE_EQUAL(confirmed.funded_value, 3u);
    BC_REQUIRE_EQUAL(confirmed.spent_count, 0u);
    BC_REQUIRE_EQUAL(worst.funded_count, 1u);
    BC_REQUIRE_EQUAL(worst.funded_value, 1u);
    BC_REQUIRE_EQUAL(worst.spent_count, 1u);
    BC_REQUIRE_EQUAL(worst.spent_value, 3u);
}

BC_AUTO_TEST_CASE(query_address__get_address_totals__unconfirmable__confirmed_only)
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

    address_totals confirmed{};
    pool_receipts receipts{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_address_totals(cancel, confirmed, receipts, test::pay_address(), max_size_t));
    BC_REQUIRE_EQUAL(confirmed.funded_count, 1u);
    BC_REQUIRE_EQUAL(confirmed.funded_value, 3u);
    BC_REQUIRE_EQUAL(confirmed.spent_count, 0u);
    BC_REQUIRE(receipts.empty());
}

BC_AUTO_TEST_CASE(query_address__get_address_totals__duplicate_instances__counted_once)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));
    BC_REQUIRE(query.set(test::block1a, context{ 0, 1, 0 }, {}, false, false));
    BC_REQUIRE(query.set_strong(1));
    BC_REQUIRE(query.push_confirmed(1, false));
    BC_REQUIRE(query.set(test::tx4));
    BC_REQUIRE(query.set(test::block_spend_1a, context{ 0, 2, 0 }, {}, false, false));
    BC_REQUIRE(query.set_strong(2));
    BC_REQUIRE(query.push_confirmed(2, false));
    BC_REQUIRE_EQUAL(query.to_duplicates(test::tx4.hash(false)).size(), 2u);

    // block1a output 0 (0x18) is spent by tx4, whose 0x08 output is counted once.
    address_totals confirmed{};
    pool_receipts receipts{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_address_totals(cancel, confirmed, receipts, test::block1a_address0, max_size_t));
    BC_REQUIRE_EQUAL(confirmed.funded_count, 2u);
    BC_REQUIRE_EQUAL(confirmed.funded_value, 0x18u + 0x08u);
    BC_REQUIRE_EQUAL(confirmed.spent_count, 1u);
    BC_REQUIRE_EQUAL(confirmed.spent_value, 0x18u);
    BC_REQUIRE(receipts.empty());
}

BC_AUTO_TEST_CASE(query_address__get_address_totals__canceled__query_canceled)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(test::setup_three_block_confirmed_address_store(query));

    address_totals confirmed{};
    pool_receipts receipts{};
    const std::atomic_bool cancel{ true };
    BC_REQUIRE_EQUAL(query.get_address_totals(cancel, confirmed, receipts, test::block1a_address0, max_size_t), error::query_canceled);
}

BC_AUTO_TEST_CASE(query_address__get_unconfirmed_totals__spent_parent__funded_and_spent)
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

    address_totals worst{};
    address_totals best{};
    pool_receipts receipts{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_unconfirmed_receipts(cancel, receipts, test::pay_address(), max_size_t));
    BC_REQUIRE(!query.get_unconfirmed_totals(worst, best, receipts, 8u));
    BC_REQUIRE_EQUAL(receipts.size(), 2u);
    BC_REQUIRE_EQUAL(worst.funded_value, 3u);
    BC_REQUIRE_EQUAL(worst.spent_value, 2u);
    BC_REQUIRE_EQUAL(best.funded_value, 3u);
    BC_REQUIRE_EQUAL(best.spent_value, 2u);
}

BC_AUTO_TEST_CASE(query_address__get_unconfirmed_totals__rivals__least_and_greatest)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    store.set_pooling();
    const auto low = test::to_pay(test::genesis_coinbase(), 3u);
    const auto high = test::to_pay(test::genesis_coinbase(), 5u);
    BC_REQUIRE(test::archive_pool(query, low));
    BC_REQUIRE(test::archive_pool(query, high));

    address_totals worst{};
    address_totals best{};
    pool_receipts receipts{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_unconfirmed_receipts(cancel, receipts, test::pay_address(), max_size_t));
    BC_REQUIRE(!query.get_unconfirmed_totals(worst, best, receipts, 8u));
    BC_REQUIRE_EQUAL(receipts.size(), 2u);
    BC_REQUIRE_EQUAL(worst.funded_value, 3u);
    BC_REQUIRE_EQUAL(worst.spent_value, 0u);
    BC_REQUIRE_EQUAL(best.funded_value, 5u);
    BC_REQUIRE_EQUAL(best.spent_value, 0u);
}

BC_AUTO_TEST_CASE(query_address__get_unconfirmed_totals__rivals_over_limit__none_and_each)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    store.set_pooling();
    const auto low = test::to_pay(test::genesis_coinbase(), 3u);
    const auto high = test::to_pay(test::genesis_coinbase(), 5u);
    BC_REQUIRE(test::archive_pool(query, low));
    BC_REQUIRE(test::archive_pool(query, high));

    address_totals worst{};
    address_totals best{};
    pool_receipts receipts{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_unconfirmed_receipts(cancel, receipts, test::pay_address(), max_size_t));
    BC_REQUIRE(!query.get_unconfirmed_totals(worst, best, receipts, 1u));
    BC_REQUIRE_EQUAL(worst.funded_value, 0u);
    BC_REQUIRE_EQUAL(worst.spent_value, 0u);
    BC_REQUIRE_EQUAL(best.funded_value, 8u);
    BC_REQUIRE_EQUAL(best.spent_value, 0u);
}

BC_AUTO_TEST_CASE(query_address__get_unconfirmed_totals__rival_with_child__least_and_greatest)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    store.set_pooling();
    const auto low = test::to_pay(test::genesis_coinbase(), 3u);
    const auto high = test::to_pay(test::genesis_coinbase(), 5u);
    const auto child = test::to_pay(high.hash(false), 1u);
    BC_REQUIRE(test::archive_pool(query, low));
    BC_REQUIRE(test::archive_pool(query, high));
    BC_REQUIRE(test::archive_pool(query, child));

    address_totals worst{};
    address_totals best{};
    pool_receipts receipts{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_unconfirmed_receipts(cancel, receipts, test::pay_address(), max_size_t));
    BC_REQUIRE(!query.get_unconfirmed_totals(worst, best, receipts, 8u));
    BC_REQUIRE_EQUAL(receipts.size(), 3u);
    BC_REQUIRE_EQUAL(worst.funded_value, 6u);
    BC_REQUIRE_EQUAL(worst.spent_value, 5u);
    BC_REQUIRE_EQUAL(best.funded_value, 3u);
    BC_REQUIRE_EQUAL(best.spent_value, 0u);
}

BC_AUTO_TEST_CASE(query_address__get_unconfirmed_totals__rival_spenders__least_and_greatest)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    store.set_pooling();
    const auto parent = test::to_pay(test::genesis_coinbase(), 4u);
    const auto away = test::to_spend(parent.hash(false), 3u);
    const auto change = test::to_pay(parent.hash(false), 2u);
    BC_REQUIRE(test::archive_pool(query, parent));
    BC_REQUIRE(test::archive_pool(query, away));
    BC_REQUIRE(test::archive_pool(query, change));

    address_totals worst{};
    address_totals best{};
    pool_receipts receipts{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_unconfirmed_receipts(cancel, receipts, test::pay_address(), max_size_t));
    BC_REQUIRE(!query.get_unconfirmed_totals(worst, best, receipts, 8u));
    BC_REQUIRE_EQUAL(receipts.size(), 2u);
    BC_REQUIRE_EQUAL(worst.funded_value, 4u);
    BC_REQUIRE_EQUAL(worst.spent_value, 4u);
    BC_REQUIRE_EQUAL(best.funded_value, 6u);
    BC_REQUIRE_EQUAL(best.spent_value, 4u);
}

BC_AUTO_TEST_SUITE_END()
