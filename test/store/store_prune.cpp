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

// prune
// ----------------------------------------------------------------------------
// Empty store asserts so create and initialize.

BOOST_AUTO_TEST_CASE(store__prune__initialized__success)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));
    BOOST_REQUIRE(!instance.prune(test::events));

    // The retained snapshot is post-prune (the trim strands nothing).
    BOOST_REQUIRE(test::folder(configuration.path / schema::dir::primary));
    BOOST_REQUIRE(!test::folder(configuration.path / schema::dir::secondary));
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__prune__faulted_restore__success)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));
    BOOST_REQUIRE(!instance.prune(test::events));
    BOOST_REQUIRE(!instance.close(test::events));

    // Simulate power fault (flush lock persists), restore from post-prune.
    BOOST_REQUIRE(test::create(test::flush_lock_file(configuration.path)));
    BOOST_REQUIRE(!instance.restore(test::events));
    BOOST_REQUIRE(query_.is_initialized());
    BOOST_REQUIRE(!instance.close(test::events));
}

constexpr database::context validated_context
{
    system::chain::flags::bip113_rule, 8, 9
};

using spend_t = system::chain::transaction;

static spend_t spend(const system::hash_digest& hash, uint64_t value) NOEXCEPT
{
    const system::chain::point point{ hash, 0 };
    const system::chain::input input{ point, system::chain::script{}, max_uint32 };
    const system::chain::output output{ value, system::chain::script{} };
    return { 1u, system::chain::inputs{ input }, system::chain::outputs{ output }, 0u };
}

static const system::hash_digest& genesis_coinbase() NOEXCEPT
{
    static const auto hash = test::genesis.transactions_ptr()->front()->hash(false);
    return hash;
}

// Archive, bind and pool the tx under the validated context.
static bool pool(query<store<database::mmap>>& query_, const spend_t& tx) NOEXCEPT
{
    const auto stored = query_.set(tx) && test::bind(query_, tx);
    const auto link = query_.to_tx(tx.hash(false));
    return stored && query_.set_pooled(link, tx, validated_context, 0);
}

// Confirm a block of a coinbase and the tx above genesis (a pooled tx is
// associated by its pooled link, as the store is pooling).
static bool confirm(query<store<database::mmap>>& query_, const spend_t& tx) NOEXCEPT
{
    const system::chain::point null{ system::null_hash, system::chain::point::null_index };
    const system::chain::input input{ null, system::chain::script{}, max_uint32 };
    const system::chain::output output{ 42u, system::chain::script{} };
    const spend_t coinbase{ 1u, system::chain::inputs{ input }, system::chain::outputs{ output }, 0u };
    const system::chain::header header{ 1u, test::genesis.hash(), system::null_hash, 1u, 2u, 3u };
    const system::chain::block block{ header, { coinbase, tx } };
    const auto stored = query_.set(block, context{ 0, 1, 0 }, {}, false, true);
    const auto bound = stored && test::bind(query_, block);
    const auto link = query_.to_header(block.hash());
    const auto candidate = bound && query_.push_candidate(link);
    return candidate && query_.push_confirmed(link, true);
}

static code pooled(query<store<database::mmap>>& query_, const spend_t& tx) NOEXCEPT
{
    pooled_tx out{};
    out.prevouts.resize(one);
    return query_.get_pooled(out, query_.to_tx(tx.hash(false)), validated_context);
}

BOOST_AUTO_TEST_CASE(store__prune__pool__cleared)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));

    // A confirmed pooled tx is dropped.
    const auto tx = spend(genesis_coinbase(), 1u);
    BOOST_REQUIRE(pool(query_, tx));
    BOOST_REQUIRE(confirm(query_, tx));
    BOOST_REQUIRE(!instance.prune(test::events));
    BOOST_REQUIRE_EQUAL(query_.pool_body_size(), zero);
    BOOST_REQUIRE_EQUAL(pooled(query_, tx), error::unvalidated);
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__prune__faulted_restore__pool_cleared)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));

    const auto tx = spend(genesis_coinbase(), 1u);
    BOOST_REQUIRE(pool(query_, tx));
    BOOST_REQUIRE(confirm(query_, tx));
    BOOST_REQUIRE(!instance.prune(test::events));
    BOOST_REQUIRE(!instance.close(test::events));

    BOOST_REQUIRE(test::create(test::flush_lock_file(configuration.path)));
    BOOST_REQUIRE(!instance.restore(test::events));
    BOOST_REQUIRE_EQUAL(query_.pool_body_size(), zero);
    BOOST_REQUIRE_EQUAL(pooled(query_, tx), error::unvalidated);
    BOOST_REQUIRE(!instance.close(test::events));
}

// compaction
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(store__prune__unconfirmed_pooled__retained)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));
    instance.set_pooling();

    const auto unconfirmed = spend(genesis_coinbase(), 1u);
    BOOST_REQUIRE(pool(query_, unconfirmed));
    BOOST_REQUIRE(!instance.prune(test::events));
    BOOST_REQUIRE(!test::folder(configuration.path / schema::dir::compact));

    pooled_tx out{};
    out.prevouts.resize(one);
    BOOST_REQUIRE_EQUAL(query_.get_pooled(out, query_.to_tx(unconfirmed.hash(false)), validated_context), error::success);
    BOOST_REQUIRE_EQUAL(out.prevouts.front().spent, 0u);
    BOOST_REQUIRE(out.prevouts.front().coinbase);
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__prune__confirmed_pooled__dropped)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));
    instance.set_pooling();

    const auto confirmed = spend(genesis_coinbase(), 1u);
    BOOST_REQUIRE(pool(query_, confirmed));
    BOOST_REQUIRE(confirm(query_, confirmed));
    BOOST_REQUIRE(!instance.prune(test::events));
    BOOST_REQUIRE_EQUAL(pooled(query_, confirmed), error::unvalidated);
    BOOST_REQUIRE_EQUAL(query_.pool_body_size(), zero);
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__prune__conflicting_pooled__dropped)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));
    instance.set_pooling();

    const auto conflict = spend(genesis_coinbase(), 2u);
    BOOST_REQUIRE(pool(query_, conflict));
    BOOST_REQUIRE(confirm(query_, spend(genesis_coinbase(), 1u)));
    BOOST_REQUIRE(!instance.prune(test::events));
    BOOST_REQUIRE_EQUAL(pooled(query_, conflict), error::unvalidated);
    BOOST_REQUIRE_EQUAL(query_.pool_body_size(), zero);
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__prune__descendant_of_conflicting__dropped)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));
    instance.set_pooling();

    const auto conflict = spend(genesis_coinbase(), 2u);
    const auto descendant = spend(conflict.hash(false), 1u);
    BOOST_REQUIRE(pool(query_, conflict));
    BOOST_REQUIRE(pool(query_, descendant));
    BOOST_REQUIRE(confirm(query_, spend(genesis_coinbase(), 1u)));
    BOOST_REQUIRE(!instance.prune(test::events));
    BOOST_REQUIRE_EQUAL(pooled(query_, conflict), error::unvalidated);
    BOOST_REQUIRE_EQUAL(pooled(query_, descendant), error::unvalidated);
    BOOST_REQUIRE_EQUAL(query_.pool_body_size(), zero);
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__prune__descendant_of_retained__retained)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));
    instance.set_pooling();

    const auto parent = spend(genesis_coinbase(), 2u);
    const auto child = spend(parent.hash(false), 1u);
    BOOST_REQUIRE(pool(query_, parent));
    BOOST_REQUIRE(pool(query_, child));
    BOOST_REQUIRE(!instance.prune(test::events));
    BOOST_REQUIRE_EQUAL(pooled(query_, parent), error::success);

    pooled_tx out{};
    out.prevouts.resize(one);
    BOOST_REQUIRE_EQUAL(query_.get_pooled(out, query_.to_tx(child.hash(false)), validated_context), error::success);
    BOOST_REQUIRE_EQUAL(out.prevouts.front().spent, query_.to_output(query_.to_tx(parent.hash(false)), 0));
    BOOST_REQUIRE(!out.prevouts.front().coinbase);
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__prune__retained_and_dropped__only_retained)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));
    instance.set_pooling();

    const auto conflict = spend(genesis_coinbase(), 2u);
    const auto retained = spend(spend(genesis_coinbase(), 1u).hash(false), 3u);
    const auto confirmed = spend(genesis_coinbase(), 1u);
    BOOST_REQUIRE(pool(query_, conflict));
    BOOST_REQUIRE(pool(query_, confirmed));
    BOOST_REQUIRE(pool(query_, retained));
    BOOST_REQUIRE(confirm(query_, confirmed));
    BOOST_REQUIRE(!instance.prune(test::events));
    BOOST_REQUIRE_EQUAL(pooled(query_, conflict), error::unvalidated);
    BOOST_REQUIRE_EQUAL(pooled(query_, confirmed), error::unvalidated);
    BOOST_REQUIRE_EQUAL(pooled(query_, retained), error::success);
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__prune__child_pooled_before_parent__retained)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));
    instance.set_pooling();

    const auto parent = spend(genesis_coinbase(), 2u);
    const auto child = spend(parent.hash(false), 1u);
    BOOST_REQUIRE(query_.set(parent));
    BOOST_REQUIRE(test::bind(query_, parent));
    BOOST_REQUIRE(pool(query_, child));
    BOOST_REQUIRE(query_.set_pooled(query_.to_tx(parent.hash(false)), parent, validated_context, 0));
    BOOST_REQUIRE(!instance.prune(test::events));
    BOOST_REQUIRE_EQUAL(pooled(query_, parent), error::success);
    BOOST_REQUIRE_EQUAL(pooled(query_, child), error::success);
    BOOST_REQUIRE_EQUAL(query_.wtxid_records(), two);
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__prune__child_of_unpooled_parent__dropped)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));
    instance.set_pooling();

    const auto parent = spend(genesis_coinbase(), 2u);
    const auto child = spend(parent.hash(false), 1u);
    BOOST_REQUIRE(query_.set(parent));
    BOOST_REQUIRE(test::bind(query_, parent));
    BOOST_REQUIRE(pool(query_, child));
    BOOST_REQUIRE(!instance.prune(test::events));
    BOOST_REQUIRE_EQUAL(pooled(query_, child), error::unvalidated);
    BOOST_REQUIRE_EQUAL(query_.pool_body_size(), zero);
    BOOST_REQUIRE_EQUAL(query_.wtxid_records(), zero);
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__prune__retained__found_by_witness_hash)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));
    instance.set_pooling();

    const auto conflict = spend(genesis_coinbase(), 2u);
    const auto retained = spend(spend(genesis_coinbase(), 1u).hash(false), 3u);
    const auto confirmed = spend(genesis_coinbase(), 1u);
    BOOST_REQUIRE(pool(query_, conflict));
    BOOST_REQUIRE(pool(query_, confirmed));
    BOOST_REQUIRE(pool(query_, retained));
    BOOST_REQUIRE(confirm(query_, confirmed));
    BOOST_REQUIRE(!instance.prune(test::events));
    BOOST_REQUIRE_EQUAL(query_.wtxid_records(), one);
    BOOST_REQUIRE_EQUAL(query_.to_witness_tx(retained.hash(true)), query_.to_tx(retained.hash(false)));
    BOOST_REQUIRE(!query_.is_pooled(query_.to_tx(conflict.hash(false))));
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__prune__residual_compact_empty_pool__deleted)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));
    BOOST_REQUIRE(test::clear(configuration.path / schema::dir::compact));
    BOOST_REQUIRE(test::create(configuration.path / schema::dir::compact / "cache_pool.head"));
    BOOST_REQUIRE(!instance.prune(test::events));
    BOOST_REQUIRE(!test::folder(configuration.path / schema::dir::compact));
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__prune__residual_compact_pooled__deleted_retained)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));
    instance.set_pooling();

    const auto unconfirmed = spend(genesis_coinbase(), 1u);
    BOOST_REQUIRE(pool(query_, unconfirmed));
    BOOST_REQUIRE(test::clear(configuration.path / schema::dir::compact));
    BOOST_REQUIRE(test::create(configuration.path / schema::dir::compact / "cache_pool.head"));
    BOOST_REQUIRE(!instance.prune(test::events));
    BOOST_REQUIRE(!test::folder(configuration.path / schema::dir::compact));
    BOOST_REQUIRE_EQUAL(pooled(query_, unconfirmed), error::success);
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__prune__faulted_restore__retained_pool_empty)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));
    instance.set_pooling();

    const auto unconfirmed = spend(genesis_coinbase(), 1u);
    BOOST_REQUIRE(pool(query_, unconfirmed));
    BOOST_REQUIRE(!instance.prune(test::events));
    BOOST_REQUIRE_EQUAL(pooled(query_, unconfirmed), error::success);
    BOOST_REQUIRE(!instance.close(test::events));

    // The restore point is the prune snapshot, which records an empty pool.
    BOOST_REQUIRE(test::create(test::flush_lock_file(configuration.path)));
    BOOST_REQUIRE(!instance.restore(test::events));
    BOOST_REQUIRE_EQUAL(pooled(query_, unconfirmed), error::unvalidated);
    BOOST_REQUIRE_EQUAL(query_.pool_body_size(), zero);
    BOOST_REQUIRE_EQUAL(query_.wtxid_body_size(), zero);
    BOOST_REQUIRE(!query_.is_pooled(query_.to_tx(unconfirmed.hash(false))));
    BOOST_REQUIRE(!instance.close(test::events));
}

// points
// ----------------------------------------------------------------------------

// Archive a block of a coinbase and the tx (block txs have points).
static bool archive(query<store<database::mmap>>& query_, const spend_t& tx,
    uint32_t nonce) NOEXCEPT
{
    using namespace system::chain;
    const point null{ system::null_hash, point::null_index };
    const input input{ null, script{}, max_uint32 };
    const output output{ nonce, script{} };
    const spend_t coinbase{ 1u, inputs{ input }, outputs{ output }, 0u };
    const header header{ 1u, test::genesis.hash(), system::null_hash, 1u, 2u, nonce };
    const block block{ header, { coinbase, tx } };
    return query_.set(block, database::context{ 0, 1, 0 }, {}, false, false);
}

BOOST_AUTO_TEST_CASE(store__drop_points__genesis__dropped)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));
    BOOST_REQUIRE_EQUAL(query_.points0_body_size(), schema::points::minrow);
    BOOST_REQUIRE(!instance.drop_points(test::events));
    BOOST_REQUIRE_EQUAL(query_.points0_body_size(), zero);
    BOOST_REQUIRE_EQUAL(instance.points0.base(), one);
    BOOST_REQUIRE_EQUAL(instance.points1.base(), one);
    BOOST_REQUIRE(query_.get_point(0).is_null());
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__drop_points__unbound__dropped)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));

    // The binder keeps the point of a missing prevout across the drop.
    const auto tx = spend(system::one_hash, 1u);
    BOOST_REQUIRE(archive(query_, tx, 1u));
    BOOST_REQUIRE_EQUAL(query_.points0_body_size(), 3u * schema::points::minrow);
    BOOST_REQUIRE(query_.get_point(2) == system::chain::point(system::one_hash, 0));
    BOOST_REQUIRE_EQUAL(query_.bind(header_link{ 1 }), system::error::missing_previous_output);
    BOOST_REQUIRE_EQUAL(query_.unbound_records(), one);
    BOOST_REQUIRE(!instance.drop_points(test::events));
    BOOST_REQUIRE_EQUAL(instance.points0.base(), 3u);
    BOOST_REQUIRE_EQUAL(query_.points0_body_size(), zero);
    BOOST_REQUIRE_EQUAL(query_.unbound_records(), one);
    BOOST_REQUIRE(query_.get_point(2) == system::chain::point(system::one_hash, 0));
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__drop_points__pooled__dropped)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));

    // A pooled tx is bound at archive, its points are dropped with the bank.
    const auto tx = spend(genesis_coinbase(), 1u);
    BOOST_REQUIRE(query_.set(tx));
    BOOST_REQUIRE_EQUAL(query_.points0_body_size(), 2u * schema::points::minrow);
    BOOST_REQUIRE(query_.get_point(1) == system::chain::point(genesis_coinbase(), 0));
    BOOST_REQUIRE(!instance.drop_points(test::events));
    BOOST_REQUIRE_EQUAL(instance.points0.base(), 2u);
    BOOST_REQUIRE_EQUAL(query_.unbound_records(), zero);
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__drop_points__bound__dropped_and_resolved)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));

    const auto tx = spend(genesis_coinbase(), 1u);
    BOOST_REQUIRE(archive(query_, tx, 1u));
    BOOST_REQUIRE(test::bind(query_, tx));
    BOOST_REQUIRE(!instance.drop_points(test::events));
    BOOST_REQUIRE_EQUAL(instance.points0.base(), 3u);
    BOOST_REQUIRE_EQUAL(query_.points0_body_size(), zero);
    BOOST_REQUIRE_EQUAL(query_.unbound_records(), zero);

    // Points resolve through the bindings.
    BOOST_REQUIRE(query_.get_point(0).is_null());
    BOOST_REQUIRE(query_.get_point(1).is_null());
    BOOST_REQUIRE(query_.get_point(2) == system::chain::point(genesis_coinbase(), 0));
    BOOST_REQUIRE_EQUAL(query_.to_spent(2), query_.to_output(0, 0));
    BOOST_REQUIRE(query_.to_spent(1).is_terminal());

    // Later points are positioned from the new base.
    const auto child = spend(tx.hash(false), 1u);
    BOOST_REQUIRE(archive(query_, child, 2u));
    BOOST_REQUIRE_EQUAL(query_.points0_body_size(), 2u * schema::points::minrow);
    BOOST_REQUIRE(query_.get_point(4) == system::chain::point(tx.hash(false), 0));
    BOOST_REQUIRE(test::bind(query_, child));
    BOOST_REQUIRE(!instance.drop_points(test::events));
    BOOST_REQUIRE_EQUAL(instance.points0.base(), 5u);
    BOOST_REQUIRE_EQUAL(query_.unbound_records(), zero);
    BOOST_REQUIRE(query_.get_point(4) == system::chain::point(tx.hash(false), 0));
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__flip_points__archived__banks_alternate)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));
    BOOST_REQUIRE(!instance.get_points_bank());

    const auto tx = spend(genesis_coinbase(), 1u);
    BOOST_REQUIRE(archive(query_, tx, 1u));
    BOOST_REQUIRE_EQUAL(query_.points0_body_size(), 3u * schema::points::minrow);
    BOOST_REQUIRE(!instance.flip_points(test::events));
    BOOST_REQUIRE(instance.get_points_bank());
    BOOST_REQUIRE_EQUAL(instance.points1.base(), 3u);

    // The next block writes the other bank, both banks are read.
    const auto child = spend(tx.hash(false), 1u);
    BOOST_REQUIRE(archive(query_, child, 2u));
    BOOST_REQUIRE_EQUAL(query_.points0_body_size(), 3u * schema::points::minrow);
    BOOST_REQUIRE_EQUAL(query_.points1_body_size(), 2u * schema::points::minrow);
    BOOST_REQUIRE(query_.get_point(2) == system::chain::point(genesis_coinbase(), 0));
    BOOST_REQUIRE(query_.get_point(4) == system::chain::point(tx.hash(false), 0));

    // The flip drains the first bank, its bound point resolves through spend.
    BOOST_REQUIRE(test::bind(query_, tx));
    BOOST_REQUIRE(!instance.flip_points(test::events));
    BOOST_REQUIRE(!instance.get_points_bank());
    BOOST_REQUIRE_EQUAL(instance.points0.base(), 5u);
    BOOST_REQUIRE_EQUAL(query_.points0_body_size(), zero);
    BOOST_REQUIRE_EQUAL(query_.points1_body_size(), 2u * schema::points::minrow);
    BOOST_REQUIRE_EQUAL(query_.unbound_records(), zero);
    BOOST_REQUIRE(query_.get_point(2) == system::chain::point(genesis_coinbase(), 0));
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__restore__bound_points__restored)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));

    const auto tx = spend(genesis_coinbase(), 1u);
    BOOST_REQUIRE(archive(query_, tx, 1u));
    BOOST_REQUIRE(test::bind(query_, tx));
    BOOST_REQUIRE(!instance.snapshot(test::events));
    BOOST_REQUIRE(!instance.close(test::events));

    BOOST_REQUIRE(test::create(test::flush_lock_file(configuration.path)));
    BOOST_REQUIRE(!instance.restore(test::events));
    BOOST_REQUIRE_EQUAL(instance.points0.base(), zero);
    BOOST_REQUIRE_EQUAL(query_.points0_body_size(), 3u * schema::points::minrow);
    BOOST_REQUIRE(query_.get_point(2) == system::chain::point(genesis_coinbase(), 0));
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__drop_points__restore__base_retained)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));

    const auto tx = spend(genesis_coinbase(), 1u);
    BOOST_REQUIRE(archive(query_, tx, 1u));
    BOOST_REQUIRE(test::bind(query_, tx));
    BOOST_REQUIRE(!instance.drop_points(test::events));
    BOOST_REQUIRE(!instance.snapshot(test::events));
    BOOST_REQUIRE(!instance.close(test::events));

    BOOST_REQUIRE(test::create(test::flush_lock_file(configuration.path)));
    BOOST_REQUIRE(!instance.restore(test::events));
    BOOST_REQUIRE_EQUAL(instance.points0.base(), 3u);
    BOOST_REQUIRE_EQUAL(query_.points0_body_size(), zero);
    BOOST_REQUIRE(query_.get_point(2) == system::chain::point(genesis_coinbase(), 0));
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_CASE(store__drop_points__restore_to_prior_snapshot__points_restored)
{
    settings configuration{};
    configuration.path = TEST_DIRECTORY;
    store<database::mmap> instance{ configuration };
    query<store<database::mmap>> query_{ instance };
    BOOST_REQUIRE(!instance.create(test::events));
    BOOST_REQUIRE(query_.initialize(test::genesis));

    // Snapshot with points unbound, then drop (the snapshot holds the bank).
    const auto tx = spend(genesis_coinbase(), 1u);
    BOOST_REQUIRE(archive(query_, tx, 1u));
    BOOST_REQUIRE(!instance.snapshot(test::events));
    BOOST_REQUIRE(!instance.drop_points(test::events));
    BOOST_REQUIRE(!instance.close(test::events));

    BOOST_REQUIRE(test::create(test::flush_lock_file(configuration.path)));
    BOOST_REQUIRE(!instance.restore(test::events));
    BOOST_REQUIRE_EQUAL(instance.points0.base(), zero);
    BOOST_REQUIRE_EQUAL(query_.points0_body_size(), 3u * schema::points::minrow);
    BOOST_REQUIRE_EQUAL(query_.unbound_records(), zero);
    BOOST_REQUIRE(query_.get_point(2) == system::chain::point(genesis_coinbase(), 0));
    BOOST_REQUIRE(!instance.close(test::events));
}

BOOST_AUTO_TEST_SUITE_END()
