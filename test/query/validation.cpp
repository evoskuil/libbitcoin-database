/**
 * Copyright (c) 2011-2022 libbitcoin developers (see AUTHORS)
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
#include "../test.hpp"
#include "../mocks/blocks.hpp"
#include "../mocks/chunk_store.hpp"

struct query_validation_setup_fixture
{
    DELETE4(query_validation_setup_fixture);
    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

    query_validation_setup_fixture() NOEXCEPT
    {
        BOOST_REQUIRE(test::clear(test::directory));
    }

    ~query_validation_setup_fixture() NOEXCEPT
    {
        BOOST_REQUIRE(test::clear(test::directory));
    }

    BC_POP_WARNING()
};

BOOST_FIXTURE_TEST_SUITE(query_validation_tests, query_validation_setup_fixture)

BOOST_AUTO_TEST_CASE(query_validation__get_context__genesis__default)
{
    settings settings{};
    settings.dir = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE_EQUAL(store.create(), error::success);
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.get_context(0) == context{});
}

BOOST_AUTO_TEST_CASE(query_validation__get_context__invalid__default)
{
    settings settings{};
    settings.dir = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE_EQUAL(store.create(), error::success);
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.get_context(header_link::terminal) == context{});
    BOOST_REQUIRE(query.get_context(1) == context{});
}

BOOST_AUTO_TEST_CASE(query_validation__get_context__block1__expected)
{
    settings settings{};
    settings.dir = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE_EQUAL(store.create(), error::success);
    BOOST_REQUIRE(query.initialize(test::genesis));

    const context expected{ 12, 34, 56 };
    BOOST_REQUIRE(query.set(test::block1, expected));
    BOOST_REQUIRE(query.get_context(1) == expected);
}

BOOST_AUTO_TEST_CASE(query_validation__get_block_state__invalid_link__unassociated)
{
    settings settings{};
    settings.dir = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE_EQUAL(store.create(), error::success);
    BOOST_REQUIRE(query.initialize(test::genesis));

    uint64_t fees{};
    BOOST_REQUIRE_EQUAL(query.get_block_state(1), error::unassociated);
    BOOST_REQUIRE_EQUAL(query.get_block_state(fees, 1), error::unassociated);
    BOOST_REQUIRE_EQUAL(fees, 0u);
}

BOOST_AUTO_TEST_CASE(query_validation__get_block_state__unassociated_link__unassociated)
{
    settings settings{};
    settings.dir = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE_EQUAL(store.create(), error::success);
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1.header(), {}));
    BOOST_REQUIRE(query.set(*test::block1.transactions_ptr()->front()));

    uint64_t fees{};
    BOOST_REQUIRE_EQUAL(query.get_block_state(1), error::unassociated);
    BOOST_REQUIRE_EQUAL(query.get_block_state(fees, 1), error::unassociated);
    BOOST_REQUIRE_EQUAL(fees, 0u);
}

BOOST_AUTO_TEST_CASE(query_validation__get_block_state__unvalidated_link__unvalidated)
{
    settings settings{};
    settings.dir = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE_EQUAL(store.create(), error::success);
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1, {}));

    uint64_t fees{};
    BOOST_REQUIRE_EQUAL(query.get_block_state(1), error::unvalidated);
    BOOST_REQUIRE_EQUAL(query.get_block_state(fees, 1), error::unvalidated);
    BOOST_REQUIRE_EQUAL(fees, 0u);
}

BOOST_AUTO_TEST_CASE(query_validation__get_block_state__confirmable__block_confirmable)
{
    settings settings{};
    settings.dir = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE_EQUAL(store.create(), error::success);
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1, {}));

    uint64_t fees{};
    BOOST_REQUIRE_EQUAL(query.get_block_state(0), error::block_confirmable);
    BOOST_REQUIRE_EQUAL(query.get_block_state(fees, 0), error::block_confirmable);
    BOOST_REQUIRE_EQUAL(fees, 0u);

    BOOST_REQUIRE(query.set_block_confirmable(1, 42));
    BOOST_REQUIRE_EQUAL(query.get_block_state(1), error::block_confirmable);
    BOOST_REQUIRE_EQUAL(query.get_block_state(fees, 1), error::block_confirmable);
    BOOST_REQUIRE_EQUAL(fees, 42u);
}

BOOST_AUTO_TEST_CASE(query_validation__get_block_state__preconfirmable__block_preconfirmable)
{
    settings settings{};
    settings.dir = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE_EQUAL(store.create(), error::success);
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1, {}));

    uint64_t fees{};
    BOOST_REQUIRE(query.set_block_preconfirmable(1));
    BOOST_REQUIRE_EQUAL(query.get_block_state(1), error::block_preconfirmable);
    BOOST_REQUIRE_EQUAL(query.get_block_state(fees, 1), error::block_preconfirmable);
    BOOST_REQUIRE_EQUAL(fees, 0u);
}

BOOST_AUTO_TEST_CASE(query_validation__get_block_state__unconfirmable__block_unconfirmable)
{
    settings settings{};
    settings.dir = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE_EQUAL(store.create(), error::success);
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1, {}));

    uint64_t fees{};
    BOOST_REQUIRE(query.set_block_unconfirmable(1));
    BOOST_REQUIRE_EQUAL(query.get_block_state(1), error::block_unconfirmable);
    BOOST_REQUIRE_EQUAL(query.get_block_state(fees, 1), error::block_unconfirmable);
    BOOST_REQUIRE_EQUAL(fees, 0u);
}

BOOST_AUTO_TEST_CASE(query_validation__get_tx_state__invalid_link__unvalidated)
{
    settings settings{};
    settings.dir = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE_EQUAL(store.create(), error::success);
    BOOST_REQUIRE(query.initialize(test::genesis));

    uint64_t fee{};
    size_t sigops{};
    BOOST_REQUIRE_EQUAL(query.get_tx_state(1, {}), error::unvalidated);
    BOOST_REQUIRE_EQUAL(query.get_tx_state(fee, sigops, 1, {}), error::unvalidated);
    BOOST_REQUIRE_EQUAL(fee, 0u);
}

BOOST_AUTO_TEST_CASE(query_validation__get_tx_state__unvalidated__unvalidated)
{
    settings settings{};
    settings.dir = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE_EQUAL(store.create(), error::success);
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1, {}));

    uint64_t fee{};
    size_t sigops{};
    BOOST_REQUIRE_EQUAL(query.get_tx_state(1, {}), error::unvalidated);
    BOOST_REQUIRE_EQUAL(query.get_tx_state(fee, sigops, 1, {}), error::unvalidated);
    BOOST_REQUIRE_EQUAL(fee, 0u);
}

BOOST_AUTO_TEST_CASE(query_validation__get_tx_state__connected_out_of_context__unvalidated)
{
    settings settings{};
    settings.dir = TEST_DIRECTORY;
    settings.validated_tx_buckets = 1;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE_EQUAL(store.create(), error::success);
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1, {}));
    BOOST_REQUIRE(query.set(test::block2, {}));
    BOOST_REQUIRE(query.set(test::block3, {}));

    uint64_t fee{};
    size_t sigops{};
    constexpr context ctx{ 7, 8, 9 };

    // Set a context which does not match ctx.
    BOOST_REQUIRE(query.set_tx_connected(0, { 1, 5, 9 }, 0, 0));
    BOOST_REQUIRE(query.set_tx_connected(1, { 2, 6, 0 }, 0, 0));
    BOOST_REQUIRE(query.set_tx_connected(2, { 3, 7, 1 }, 0, 0));
    BOOST_REQUIRE(query.set_tx_connected(3, { 4, 8, 2 }, 0, 0));
    BOOST_REQUIRE_EQUAL(query.get_tx_state(1, ctx), error::unvalidated);
    BOOST_REQUIRE_EQUAL(query.get_tx_state(fee, sigops, 1, ctx), error::unvalidated);
    BOOST_REQUIRE_EQUAL(fee, 0u);
    BOOST_REQUIRE_EQUAL(sigops, 0u);
}

BOOST_AUTO_TEST_CASE(query_validation__get_tx_state__connected_in_context__tx_connected)
{
    settings settings{};
    settings.dir = TEST_DIRECTORY;
    settings.validated_tx_buckets = 1;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE_EQUAL(store.create(), error::success);
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1, {}));
    BOOST_REQUIRE(query.set(test::block2, {}));
    BOOST_REQUIRE(query.set(test::block3, {}));

    uint64_t fee{};
    size_t sigops{};
    constexpr uint64_t expected_fee = 42;
    constexpr size_t expected_sigops = 24;
    constexpr context ctx{ 7, 8, 9 };
    BOOST_REQUIRE(query.set_tx_connected(0, ctx, 11, 12));
    BOOST_REQUIRE(query.set_tx_connected(1, ctx, 13, 14));
    BOOST_REQUIRE(query.set_tx_connected(2, ctx, expected_fee, expected_sigops));
    BOOST_REQUIRE(query.set_tx_connected(2, { 1, 5, 9 }, 15, 16));
    BOOST_REQUIRE(query.set_tx_connected(2, { 2, 6, 0 }, 17, 18));
    BOOST_REQUIRE(query.set_tx_connected(3, ctx, 19, 20));
    BOOST_REQUIRE_EQUAL(query.get_tx_state(2, ctx), error::tx_connected);
    BOOST_REQUIRE_EQUAL(query.get_tx_state(fee, sigops, 2, ctx), error::tx_connected);
    BOOST_REQUIRE_EQUAL(fee, expected_fee);
    BOOST_REQUIRE_EQUAL(sigops, expected_sigops);
}

BOOST_AUTO_TEST_CASE(query_validation__get_tx_state__connected_in_context__tx_preconnected)
{
    settings settings{};
    settings.dir = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE_EQUAL(store.create(), error::success);
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1, {}));
    BOOST_REQUIRE(query.set(test::block2, {}));
    BOOST_REQUIRE(query.set(test::block3, {}));

    uint64_t fee{};
    size_t sigops{};
    constexpr context ctx{ 7, 8, 9 };
    BOOST_REQUIRE(query.set_tx_preconnected(3, ctx));
    BOOST_REQUIRE_EQUAL(query.get_tx_state(3, ctx), error::tx_preconnected);
    BOOST_REQUIRE_EQUAL(query.get_tx_state(fee, sigops, 3, ctx), error::tx_preconnected);
    BOOST_REQUIRE_EQUAL(fee, 0u);
    BOOST_REQUIRE_EQUAL(sigops, 0u);
}

BOOST_AUTO_TEST_CASE(query_validation__get_tx_state__connected_in_context__tx_disconnected)
{
    settings settings{};
    settings.dir = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE_EQUAL(store.create(), error::success);
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1, {}));
    BOOST_REQUIRE(query.set(test::block2, {}));
    BOOST_REQUIRE(query.set(test::block3, {}));

    uint64_t fee{};
    size_t sigops{};
    constexpr context ctx{ 7, 8, 9 };
    BOOST_REQUIRE(query.set_tx_disconnected(4, ctx));
    BOOST_REQUIRE_EQUAL(query.get_tx_state(4, ctx), error::tx_disconnected);
    BOOST_REQUIRE_EQUAL(query.get_tx_state(fee, sigops, 4, ctx), error::tx_disconnected);
    BOOST_REQUIRE_EQUAL(fee, 0u);
    BOOST_REQUIRE_EQUAL(sigops, 0u);
}

BOOST_AUTO_TEST_SUITE_END()
