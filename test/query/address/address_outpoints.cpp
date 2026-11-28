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

// get_confirmed_unspent_outpoints

BC_AUTO_TEST_CASE(query_address__get_unconfirmed_unspent_outpoints__turbo_genesis__empty)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_unconfirmed_unspent_outpoints(cancel, out, test::genesis_address0, max_size_t, true));
    BC_REQUIRE(out.empty());
}

BC_AUTO_TEST_CASE(query_address__get_unconfirmed_unspent_outpoints__unknown__empty)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(test::setup_three_block_confirmed_address_store(query));

    outpoints out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_unconfirmed_unspent_outpoints(cancel, out, test::block1a_address0, max_size_t, true));
    BC_REQUIRE(out.empty());
}

BC_AUTO_TEST_CASE(query_address__get_unconfirmed_unspent_outpoints__confirmable__unspent_only)
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

    outpoints out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_unconfirmed_unspent_outpoints(cancel, out, test::pay_address(), max_size_t, true));
    BC_REQUIRE_EQUAL(out.size(), 1u);
    BC_REQUIRE_EQUAL(out.front().value(), 1u);
    BC_REQUIRE_EQUAL(out.front().point().hash(), child.hash(false));
}

BC_AUTO_TEST_CASE(query_address__get_unconfirmed_unspent_outpoints__unconfirmable__empty)
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

    outpoints out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_unconfirmed_unspent_outpoints(cancel, out, test::pay_address(), max_size_t, true));
    BC_REQUIRE(out.empty());
}

BC_AUTO_TEST_CASE(query_address__get_address_outpoints__unknown__excluded)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(test::setup_three_block_confirmed_address_store(query));

    outpoints out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_address_outpoints(cancel, out, test::block1a_address0, max_size_t, true));
    BC_REQUIRE_EQUAL(out.size(), 4u);
}

BC_AUTO_TEST_CASE(query_address__get_address_outpoints__confirmable__included)
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

    outpoints out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_address_outpoints(cancel, out, test::pay_address(), max_size_t, true));
    BC_REQUIRE_EQUAL(out.size(), 2u);
}

BC_AUTO_TEST_CASE(query_address__get_confirmed_unspent_outpoints__turbo_genesis__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_confirmed_unspent_outpoints(cancel, out, test::genesis_address0, max_size_t, true));
    BC_REQUIRE_EQUAL(out.size(), 1u);
    BC_REQUIRE(*out.cbegin() == query.get_outpoint(query.to_output(0, 0)));
}

BC_AUTO_TEST_CASE(query_address__get_confirmed_unspent_outpoints__genesis__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_confirmed_unspent_outpoints(cancel, out, test::genesis_address0, max_size_t));
    BC_REQUIRE_EQUAL(out.size(), 1u);
    BC_REQUIRE(*out.cbegin() == query.get_outpoint(query.to_output(0, 0)));
}

// get_minimum_unspent_outpoints

BC_AUTO_TEST_CASE(query_address__get_minimum_unspent_outpoints__turbo_above__excluded)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_minimum_unspent_outpoints(cancel, out, test::genesis_address0, 5000000001, max_size_t, true));
    BC_REQUIRE(out.empty());
}

BC_AUTO_TEST_CASE(query_address__get_minimum_unspent_outpoints__above__excluded)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_minimum_unspent_outpoints(cancel, out, test::genesis_address0, 5000000001, max_size_t));
    BC_REQUIRE(out.empty());
}

BC_AUTO_TEST_CASE(query_address__get_minimum_unspent_outpoints__at__included)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_minimum_unspent_outpoints(cancel, out, test::genesis_address0, 5000000000, max_size_t));
    BC_REQUIRE_EQUAL(out.size(), 1u);
    BC_REQUIRE(*out.cbegin() == query.get_outpoint(query.to_output(0, 0)));
}

BC_AUTO_TEST_CASE(query_address__get_minimum_unspent_outpoints__below__included)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_minimum_unspent_outpoints(cancel, out, test::genesis_address0, 0, max_size_t));
    BC_REQUIRE_EQUAL(out.size(), 1u);
    BC_REQUIRE(*out.cbegin() == query.get_outpoint(query.to_output(0, 0)));

    BC_REQUIRE(!query.get_minimum_unspent_outpoints(cancel, out, test::genesis_address0, 4999999999, max_size_t));
    BC_REQUIRE_EQUAL(out.size(), 1u);
    BC_REQUIRE(*out.cbegin() == query.get_outpoint(query.to_output(0, 0)));
}

// get_address_outpoints1

BC_AUTO_TEST_CASE(query_address__get_address_outpoints1__turbo_genesis__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_address_outpoints(cancel, out, test::genesis_address0, max_size_t, true));
    BC_REQUIRE_EQUAL(out.size(), 1u);
    BC_REQUIRE(*out.cbegin() == query.get_outpoint(query.to_output(0, 0)));
}

BC_AUTO_TEST_CASE(query_address__get_address_outpoints1__genesis__expected)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE(!query.get_address_outpoints(cancel, out, test::genesis_address0, max_size_t));
    BC_REQUIRE_EQUAL(out.size(), 1u);
    BC_REQUIRE(*out.cbegin() == query.get_outpoint(query.to_output(0, 0)));
}

BC_AUTO_TEST_CASE(query_address__get_address_outpoints1__cancel__query_canceled_false)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    std::atomic_bool cancel{ true };
    BC_REQUIRE_EQUAL(query.get_address_outpoints(cancel, out, test::genesis_address0, max_size_t), error::query_canceled);
    BC_REQUIRE(out.empty());
}

BC_AUTO_TEST_CASE(query_address__get_address_outpoints1__zero_limit__depth_limited)
{
    settings settings{ test::database_settings() };
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BC_REQUIRE(!store.create(test::events_handler));
    BC_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    const std::atomic_bool cancel{};
    BC_REQUIRE_EQUAL(query.get_address_outpoints(cancel, out, test::genesis_address0, 0), error::depth_limited);
    BC_REQUIRE(out.empty());
}

BC_AUTO_TEST_SUITE_END()
