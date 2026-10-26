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
#include "../test.hpp"
#include "../mocks/blocks.hpp"
#include "../mocks/map_store.hpp"

// these include the slow tests (mmap)

BOOST_FIXTURE_TEST_SUITE(store_tests, test::directory_setup_fixture)

// construct
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(store__construct__default_configuration__referenced)
{
    const settings configuration{};
    test::map_store instance{ configuration };
    BOOST_REQUIRE_EQUAL(&instance.configuration(), &configuration);
}

BOOST_AUTO_TEST_CASE(store__is_pooling__default__false)
{
    const settings configuration{};
    store<database::mmap> instance{ configuration };
    BOOST_REQUIRE(!instance.is_pooling());
}

BOOST_AUTO_TEST_CASE(store__set_pooling__initialized__is_pooling)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));
    BOOST_REQUIRE(!instance.is_pooling());
    instance.set_pooling();
    BOOST_REQUIRE(instance.is_pooling());
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__paths__default_configuration__expected)
{
    const settings configuration{};
    test::map_store instance{ configuration };

    /// Archive.
    BOOST_REQUIRE_EQUAL(instance.header_head_file(), "bitcoin/heads/archive_header.head");
    BOOST_REQUIRE_EQUAL(instance.header_body_file(), "bitcoin/archive_header.data");
    BOOST_REQUIRE_EQUAL(instance.input_head_file(), "bitcoin/heads/archive_input.head");
    BOOST_REQUIRE_EQUAL(instance.input_body_file(), "bitcoin/archive_input.data");
    BOOST_REQUIRE_EQUAL(instance.output_head_file(), "bitcoin/heads/archive_output.head");
    BOOST_REQUIRE_EQUAL(instance.output_body_file(), "bitcoin/archive_output.data");
    BOOST_REQUIRE_EQUAL(instance.ins_head_file(), "bitcoin/heads/archive_ins.head");
    BOOST_REQUIRE_EQUAL(instance.ins_body_file(), "bitcoin/archive_ins.data");
    BOOST_REQUIRE_EQUAL(instance.outs_head_file(), "bitcoin/heads/archive_outs.head");
    BOOST_REQUIRE_EQUAL(instance.outs_body_file(), "bitcoin/archive_outs.data");
    BOOST_REQUIRE_EQUAL(instance.spend_head_file(), "bitcoin/heads/archive_spend.head");
    BOOST_REQUIRE_EQUAL(instance.unbound_head_file(), "bitcoin/heads/archive_unbound.head");
    BOOST_REQUIRE_EQUAL(instance.unbound_body_file(), "bitcoin/archive_unbound.data");
    BOOST_REQUIRE_EQUAL(instance.tx_head_file(), "bitcoin/heads/archive_tx.head");
    BOOST_REQUIRE_EQUAL(instance.tx_body_file(), "bitcoin/archive_tx.data");
    BOOST_REQUIRE_EQUAL(instance.txs_head_file(), "bitcoin/heads/archive_txs.head");
    BOOST_REQUIRE_EQUAL(instance.txs_body_file(), "bitcoin/archive_txs.data");

    /// Index.
    BOOST_REQUIRE_EQUAL(instance.candidate_head_file(), "bitcoin/heads/index_candidate.head");
    BOOST_REQUIRE_EQUAL(instance.confirmed_head_file(), "bitcoin/heads/index_confirmed.head");
    BOOST_REQUIRE_EQUAL(instance.strong_head_file(), "bitcoin/heads/index_strong.head");
    BOOST_REQUIRE_EQUAL(instance.address_head_file(), "bitcoin/heads/index_address.head");
    BOOST_REQUIRE_EQUAL(instance.address_body_file(), "bitcoin/index_address.data");
    BOOST_REQUIRE_EQUAL(instance.spender_head_file(), "bitcoin/heads/index_spender.head");
    BOOST_REQUIRE_EQUAL(instance.spender_body_file(), "bitcoin/index_spender.data");
    BOOST_REQUIRE_EQUAL(instance.contested_head_file(), "bitcoin/heads/index_contested.head");
    BOOST_REQUIRE_EQUAL(instance.state_head_file(), "bitcoin/heads/index_state.head");
    BOOST_REQUIRE_EQUAL(instance.wtxid_head_file(), "bitcoin/heads/index_wtxid.head");
    BOOST_REQUIRE_EQUAL(instance.wtxid_body_file(), "bitcoin/index_wtxid.data");

    /// Cache.
    BOOST_REQUIRE_EQUAL(instance.points0_head_file(), "bitcoin/heads/cache_points0.head");
    BOOST_REQUIRE_EQUAL(instance.points0_body_file(), "bitcoin/heads/cache_points0.data");
    BOOST_REQUIRE_EQUAL(instance.points1_head_file(), "bitcoin/heads/cache_points1.head");
    BOOST_REQUIRE_EQUAL(instance.points1_body_file(), "bitcoin/heads/cache_points1.data");
    BOOST_REQUIRE_EQUAL(instance.pool_head_file(), "bitcoin/heads/cache_pool.head");
    BOOST_REQUIRE_EQUAL(instance.pool_body_file(), "bitcoin/cache_pool_context.data");

    /// Option.
    BOOST_REQUIRE_EQUAL(instance.filter_bk_head_file(), "bitcoin/heads/option_filter_bk.head");
    BOOST_REQUIRE_EQUAL(instance.filter_bk_body_file(), "bitcoin/option_filter_bk.data");
    BOOST_REQUIRE_EQUAL(instance.filter_tx_head_file(), "bitcoin/heads/option_filter_tx.head");
    BOOST_REQUIRE_EQUAL(instance.filter_tx_body_file(), "bitcoin/option_filter_tx.data");

    /// Lock.
    BOOST_REQUIRE_EQUAL(instance.flush_lock_file(), "bitcoin/flush.lock");
    BOOST_REQUIRE_EQUAL(instance.process_lock_file(), "bitcoin/process.lock");
}

// get_transactor
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(store__get_transactor__always__share_locked)
{
    const settings configuration{};
    test::map_store instance{ configuration };
    auto transactor = instance.get_transactor();
    BOOST_REQUIRE(transactor);
    BOOST_REQUIRE(!instance.transactor_mutex().try_lock());
    BOOST_REQUIRE(instance.transactor_mutex().try_lock_shared());
}

// set_pooling
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(store__set_pooling__created__pooling)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    test::map_store instance{ configuration };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(!instance.is_pooling());
    instance.set_pooling();
    BOOST_REQUIRE(instance.is_pooling());
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__set_pooling__twice__remains_pooling)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    test::map_store instance{ configuration };
    BOOST_REQUIRE(!instance.create(test::events));
    instance.set_pooling();
    instance.set_pooling();
    BOOST_REQUIRE(instance.is_pooling());
    BOOST_REQUIRE(!instance.close(test::events));
}

// set_current
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(store__set_current__created__not_current)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    test::map_store instance{ configuration };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(!instance.is_current_());
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__set_current__changed__applied_to_heads)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    test::map_store instance{ configuration };
    BOOST_REQUIRE(!instance.create(test::events));
    instance.set_current_(true);
    instance.set_current_(false);
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__set_current__unchanged__no_effect)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    test::map_store instance{ configuration };
    BOOST_REQUIRE(!instance.create(test::events));
    instance.set_current_(false);
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_SUITE_END()
