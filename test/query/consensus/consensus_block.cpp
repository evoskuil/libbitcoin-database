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

BOOST_FIXTURE_TEST_SUITE(query_consensus_tests, test::directory_setup_fixture)

// block_confirmable

constexpr auto bip68 = system::chain::flags::bip68_rule;

BOOST_AUTO_TEST_CASE(query_consensus__block_confirmable__bad_link__integrity_block_confirmable1)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1, context{ bip68, 1, 0 }, {}, false, false));
    BOOST_REQUIRE_EQUAL(query.block_confirmable(2), error::integrity_block_confirmable1);
}

// These pass but are limited by vectors with mostly coinbases.
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(query_consensus__block_confirmable__null_points__success)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1, context{ bip68 }, {}, false, false));
    BOOST_REQUIRE(query.set(test::block2, context{ bip68 }, {}, false, false));
    BOOST_REQUIRE(query.set(test::block3, context{ bip68 }, {}, false, false));

    // ALL COINBASE TXS
    // block1/2/3 at links 1/2/3 confirming at heights 1/2/3.
    // blocks have only coinbase txs, all txs should be set strong before calling
    // confirmable, but these are bip30 default configuration.
    BOOST_REQUIRE_EQUAL(query.block_confirmable(1), error::success);
    BOOST_REQUIRE_EQUAL(query.block_confirmable(2), error::success);
    BOOST_REQUIRE_EQUAL(query.block_confirmable(3), error::success);
}

BOOST_AUTO_TEST_CASE(query_consensus__block_confirmable__missing_prevouts__integrity)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1a, context{ bip68, 1, 0 }, {}, false, false));
    BOOST_REQUIRE(test::bind(query, test::block1a));

    // ONLY COINBASE TXS
    // block1a is missing all three input prevouts.
    BOOST_REQUIRE(query.set_strong(1));
    BOOST_REQUIRE_EQUAL(query.block_confirmable(1), error::success);
}

BOOST_AUTO_TEST_CASE(query_consensus__block_confirmable__spend_gensis__coinbase_maturity)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    // block_spend_genesis spends the genesis output.
    BOOST_REQUIRE(query.set(test::block_spend_genesis, context{ 0, 101, 0 }, {}, false, false));
    BOOST_REQUIRE(test::bind(query, test::block_spend_genesis));
    BOOST_REQUIRE(query.set_strong(1));

    // COINBASE TX
    // 1 + 100 = 101 (maturity, except genesis)
    BOOST_REQUIRE_EQUAL(query.block_confirmable(1), error::success);
}

BOOST_AUTO_TEST_CASE(query_consensus__block_confirmable__immature_prevouts__coinbase_maturity)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    // block1b has only a coinbase tx.
    BOOST_REQUIRE(query.set(test::block1b, context{ bip68, 1, 0 }, {}, false, false));
    BOOST_REQUIRE(test::bind(query, test::block1b));
    BOOST_REQUIRE(query.set_strong(1));
    BOOST_REQUIRE_EQUAL(query.block_confirmable(1), error::success);

    // COINBASE TX
    // block2b prematurely spends block1b's coinbase outputs.
    BOOST_REQUIRE(query.set(test::block2b, context{ 0, 100, 0 }, {}, false, false));
    BOOST_REQUIRE(test::bind(query, test::block2b));
    BOOST_REQUIRE(query.set_strong(2));
    BOOST_REQUIRE_EQUAL(query.block_confirmable(2), error::success);
}

BOOST_AUTO_TEST_CASE(query_consensus__block_confirmable__mature_prevouts__success)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    // block1b has only a coinbase tx.
    BOOST_REQUIRE(query.set(test::block1b, context{ bip68, 1, 0 }, {}, false, false));
    BOOST_REQUIRE(test::bind(query, test::block1b));
    BOOST_REQUIRE(query.set_strong(1));
    BOOST_REQUIRE_EQUAL(query.block_confirmable(1), error::success);

    // COINBASE TX
    // block2b spends block1b's coinbase outputs.
    BOOST_REQUIRE(query.set(test::block2b, context{ 0, 101, 0 }, {}, false, false));
    BOOST_REQUIRE(test::bind(query, test::block2b));
    BOOST_REQUIRE(query.set_strong(2));
    BOOST_REQUIRE_EQUAL(query.block_confirmable(2), error::success);
}

BOOST_AUTO_TEST_CASE(query_consensus__block_confirmable__spend_non_coinbase__success)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    // block1a has non-coinbase tx/outputs.
    BOOST_REQUIRE(query.set(test::block1a, context{ 0, 1, 0 }, {}, false, false));
    BOOST_REQUIRE(test::bind(query, test::block1a));
    BOOST_REQUIRE(query.set_strong(1));

    // block_spend_1a spends both block1a outputs.
    BOOST_REQUIRE(query.set(test::block_spend_1a, context{ 0, 2, 0 }, {}, false, false));
    BOOST_REQUIRE(test::bind(query, test::block_spend_1a));
    BOOST_REQUIRE(query.set_strong(2));

    // COINBASE TX
    // Maturity applies only to coinbase prevouts.
    BOOST_REQUIRE_EQUAL(query.block_confirmable(2), error::success);
}

static bool set_internal_spend(test::query_accessor& query,
    const system::chain::block& block) NOEXCEPT
{
    return query.set(test::block1a, context{ 0, 1, 0 }, {}, false, false)
        && query.set_strong(1)
        && query.set(block, context{ bip68, 2, 0 }, {}, false, false)
        && test::bind(query, block)
        && query.set_strong(2);
}

BOOST_AUTO_TEST_CASE(query_consensus__block_confirmable__internal_relative_locked__relative_time_locked)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(set_internal_spend(query, test::block_spend_internal_locked_1a));
    BOOST_REQUIRE(query.block_confirmable(2) == system::error::relative_time_locked);
}

BOOST_AUTO_TEST_CASE(query_consensus__block_confirmable__internal_relative_disabled__success)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(set_internal_spend(query, test::block_spend_internal_disabled_1a));
    BOOST_REQUIRE_EQUAL(query.block_confirmable(2), error::success);
}

BOOST_AUTO_TEST_CASE(query_consensus__block_confirmable__internal_relative_zero__success)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(set_internal_spend(query, test::block_spend_internal_unlocked_1a));
    BOOST_REQUIRE_EQUAL(query.block_confirmable(2), error::success);
}

// These pass but vectors need to be updated to create clear test conditions.
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(query_consensus__block_confirmable__spend_coinbase_and_internal_immature__success)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    // block1b has coinbase tx/outputs.
    BOOST_REQUIRE(query.set(test::block1b, context{ 0, 1, 0 }, {}, false, false));
    BOOST_REQUIRE(test::bind(query, test::block1b));
    BOOST_REQUIRE(query.set_strong(1));

    // block_spend_internal_2b spends first block1b output and first own output.
    // The block1b coinbase is also the block's own coinbase, so both spends
    // bind to the block's instance and are internal (no maturity).
    BOOST_REQUIRE(query.set(test::block_spend_internal_2b, context{ 0, 100, 0 }, {}, false, false));
    BOOST_REQUIRE(test::bind(query, test::block_spend_internal_2b));
    BOOST_REQUIRE_EQUAL(query.block_confirmable(2), error::success);
    BOOST_REQUIRE(query.set_strong(2));
    BOOST_REQUIRE_EQUAL(query.block_confirmable(2), error::success);
}

BOOST_AUTO_TEST_CASE(query_consensus__block_confirmable__spend_coinbase_and_internal_mature__success)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    // block1b has coinbase tx/outputs.
    BOOST_REQUIRE(query.set(test::block1b, context{ 0, 1, 0 }, {}, false, false));
    BOOST_REQUIRE(test::bind(query, test::block1b));
    BOOST_REQUIRE(query.set_strong(1));

    BOOST_REQUIRE(query.set(test::block_spend_internal_2b, context{ 0, 101, 0 }, {}, false, false));
    BOOST_REQUIRE(test::bind(query, test::block_spend_internal_2b));

    // The block1b coinbase is mature, the internal spend has no maturity.
    BOOST_REQUIRE_EQUAL(query.block_confirmable(2), error::success);
    BOOST_REQUIRE(query.set_strong(2));
    BOOST_REQUIRE_EQUAL(query.block_confirmable(2), error::success);
}

BOOST_AUTO_TEST_CASE(query_consensus__block_confirmable__confirmed_double_spend__confirmed_double_spend)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    // block1a has non-coinbase tx/outputs.
    BOOST_REQUIRE(query.set(test::block1a, context{ 0, 1, 0 }, {}, false, false));
    BOOST_REQUIRE(test::bind(query, test::block1a));
    BOOST_REQUIRE(query.set_strong(1));

    // block2a spends both block1a outputs (though not itself confirmable is set strong).
    BOOST_REQUIRE(query.set(test::block2a, context{ 0, 2, 0 }, {}, false, false));
    BOOST_REQUIRE(test::bind(query, test::block2a));
    BOOST_REQUIRE(query.set_strong(2));

    // block_coinbase_spend_1a (also) spends both block1a outputs.
    BOOST_REQUIRE(query.set(test::block_coinbase_spend_1a, context{ bip68, 3, 0 }, {}, false, false));
    BOOST_REQUIRE(test::bind(query, test::block_coinbase_spend_1a));
    BOOST_REQUIRE(query.set_strong(3));

    // Both spenders are bound to the block1a output and strong.
    const auto spent = query.to_output(1, 0);
    const auto first = query.to_tx(test::block2a.transactions_ptr()->front()->hash(false));
    const auto second = query.to_tx(test::block_coinbase_spend_1a.transactions_ptr()->back()->hash(false));
    BOOST_REQUIRE_EQUAL(query.to_spender_txs(spent), (tx_links{ first, second }));
    BOOST_REQUIRE(query.is_strong_tx(first));
    BOOST_REQUIRE(query.is_strong_tx(second));

    // Not confirmable because the strong block2a spender is a double spend.
    BOOST_REQUIRE_EQUAL(query.block_confirmable(3), system::error::confirmed_double_spend);
}

BOOST_AUTO_TEST_CASE(query_consensus__block_confirmable__unconfirmed_double_spend__success)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    // block1a has non-coinbase tx/outputs.
    BOOST_REQUIRE(query.set(test::block1a, context{ 0, 1, 0 }, {}, false, false));
    BOOST_REQUIRE(test::bind(query, test::block1a));
    BOOST_REQUIRE(query.set_strong(1));

    // tx5 spends first block1a output.
    BOOST_REQUIRE(query.set(test::tx5));
    BOOST_REQUIRE(test::bind(query, test::tx5));

    // block_spend_1a (also) spends both block1a outputs.
    BOOST_REQUIRE(query.set(test::block_spend_1a, context{ 0, 2, 0 }, {}, false, false));
    BOOST_REQUIRE(test::bind(query, test::block_spend_1a));
    BOOST_REQUIRE(query.set_strong(2));

    // Confirmable because of intervening tx5 is unconfirmed double spend.
    BOOST_REQUIRE_EQUAL(query.block_confirmable(2), error::success);
}

// validate_pooled
// ----------------------------------------------------------------------------

using namespace system::chain;
constexpr auto pooled_flags = flags::bip113_rule;
constexpr uint64_t pooled_interval = 210'000;
constexpr uint64_t pooled_subsidy = 5'000'000'000;
const database::context pooled_context{ pooled_flags, 8, 9 };
const system::chain::context block_context{ pooled_flags, 0, 9, 8, 0, 0, 0 };
const auto genesis_hash = test::genesis.transactions_ptr()->front()->hash(false);

static transaction make_coinbase(uint64_t value) NOEXCEPT
{
    const script nops{ { { opcode::nop }, { opcode::nop } } };
    return
    {
        1,
        inputs{ input{ point{}, nops, witness{}, max_uint32 } },
        outputs{ output{ value, script{} } },
        0
    };
}

static transaction make_spend(const hash_digest& hash, uint32_t index,
    uint64_t value) NOEXCEPT
{
    return
    {
        1,
        inputs{ input{ point{ hash, index }, script{}, witness{}, max_uint32 } },
        outputs{ output{ value, script{} } },
        0
    };
}

static block make_block(const transactions& txs) NOEXCEPT
{
    const header head{ 1, test::block0_hash, hash_digest{ 0x42 }, 0, 0, 0 };
    return { head, transactions{ txs } };
}

static bool store_block(test::query_accessor& query, const block& block) NOEXCEPT
{
    const auto stored = query.set(block, pooled_context, {}, false, false);
    return stored && test::bind(query, block);
}

static bool pool_tx(test::query_accessor& query, const transaction& tx,
    uint64_t prevout, const database::context& ctx) NOEXCEPT
{
    const auto link = query.to_tx(tx.hash(false));
    for (const auto& in: *tx.inputs_ptr())
        in->prevout = system::to_shared<output>(prevout, script{});

    return query.set_pooled(link, tx, ctx, 0);
}

BOOST_AUTO_TEST_CASE(query_consensus__validate_pooled__pooled__success)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto parent = make_spend(genesis_hash, 0, 4'000'000'000);
    const auto child = make_spend(parent.hash(false), 0, 3'000'000'000);
    const auto block = make_block({ make_coinbase(1), parent, child });
    BOOST_REQUIRE(store_block(query, block));

    parent.inputs_ptr()->front()->metadata.coinbase = true;
    BOOST_REQUIRE(pool_tx(query, parent, 5'000'000'000, pooled_context));
    BOOST_REQUIRE(pool_tx(query, child, 4'000'000'000, pooled_context));

    const auto link = query.to_header(block.hash());
    BOOST_REQUIRE_EQUAL(query.validate_pooled(link, block_context, pooled_interval, pooled_subsidy), error::success);
}

BOOST_AUTO_TEST_CASE(query_consensus__validate_pooled__insufficient__unvalidated)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto parent = make_spend(genesis_hash, 0, 4'000'000'000);
    const auto block = make_block({ make_coinbase(1), parent });
    BOOST_REQUIRE(store_block(query, block));

    BOOST_REQUIRE(pool_tx(query, parent, 5'000'000'000, database::context{ pooled_flags, 9, 9 }));

    const auto link = query.to_header(block.hash());
    BOOST_REQUIRE_EQUAL(query.validate_pooled(link, block_context, pooled_interval, pooled_subsidy), error::unvalidated);
}

BOOST_AUTO_TEST_CASE(query_consensus__validate_pooled__not_pooled__unvalidated)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto block = make_block({ make_coinbase(1), make_spend(genesis_hash, 0, 1) });
    BOOST_REQUIRE(store_block(query, block));

    const auto link = query.to_header(block.hash());
    BOOST_REQUIRE_EQUAL(query.validate_pooled(link, block_context, pooled_interval, pooled_subsidy), error::unvalidated);
}

BOOST_AUTO_TEST_CASE(query_consensus__validate_pooled__overclaim__coinbase_value_limit)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    // Fee is 1'000'000'000, so claim may not exceed 6'000'000'000.
    const auto parent = make_spend(genesis_hash, 0, 4'000'000'000);
    const auto block = make_block({ make_coinbase(6'000'000'001), parent });
    BOOST_REQUIRE(store_block(query, block));

    BOOST_REQUIRE(pool_tx(query, parent, 5'000'000'000, pooled_context));

    const auto link = query.to_header(block.hash());
    BOOST_REQUIRE_EQUAL(query.validate_pooled(link, block_context, pooled_interval, pooled_subsidy), system::error::coinbase_value_limit);
}

BOOST_AUTO_TEST_CASE(query_consensus__validate_pooled__forward_reference__forward_reference)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto parent = make_spend(genesis_hash, 0, 4'000'000'000);
    const auto child = make_spend(parent.hash(false), 0, 3'000'000'000);
    const auto block = make_block({ make_coinbase(1), child, parent });
    BOOST_REQUIRE(store_block(query, block));

    BOOST_REQUIRE(pool_tx(query, parent, 5'000'000'000, pooled_context));
    BOOST_REQUIRE(pool_tx(query, child, 4'000'000'000, pooled_context));

    const auto link = query.to_header(block.hash());
    BOOST_REQUIRE_EQUAL(query.validate_pooled(link, block_context, pooled_interval, pooled_subsidy), system::error::forward_reference);
}

BOOST_AUTO_TEST_CASE(query_consensus__validate_pooled__internal_double_spend__block_internal_double_spend)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto spend1 = make_spend(genesis_hash, 0, 4'000'000'000);
    const auto spend2 = make_spend(genesis_hash, 0, 3'000'000'000);
    const auto block = make_block({ make_coinbase(1), spend1, spend2 });
    BOOST_REQUIRE(store_block(query, block));

    BOOST_REQUIRE(pool_tx(query, spend1, 5'000'000'000, pooled_context));
    BOOST_REQUIRE(pool_tx(query, spend2, 5'000'000'000, pooled_context));

    const auto link = query.to_header(block.hash());
    BOOST_REQUIRE_EQUAL(query.validate_pooled(link, block_context, pooled_interval, pooled_subsidy), system::error::block_internal_double_spend);
}

BOOST_AUTO_TEST_CASE(query_consensus__validate_pooled__internal_coinbase_spend__coinbase_maturity)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto coinbase = make_coinbase(1);
    const auto spend = make_spend(coinbase.hash(false), 0, 1);
    const auto block = make_block({ coinbase, spend });
    BOOST_REQUIRE(store_block(query, block));

    BOOST_REQUIRE(pool_tx(query, spend, 1, pooled_context));

    const auto link = query.to_header(block.hash());
    BOOST_REQUIRE_EQUAL(query.validate_pooled(link, block_context, pooled_interval, pooled_subsidy), system::error::coinbase_maturity);
}

BOOST_AUTO_TEST_CASE(query_consensus__validate_pooled__mature__block_confirmable)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    // Genesis is unspendable, so spend a strong coinbase at height one.
    const auto source = make_coinbase(5'000'000'000);
    const block first{ header{ 1, test::block0_hash, hash_digest{ 0x01 }, 0, 0, 0 }, transactions{ source } };
    BOOST_REQUIRE(query.set(first, database::context{ pooled_flags, 1, 0 }, {}, false, false));
    BOOST_REQUIRE(query.set_strong(query.to_header(first.hash())));

    const database::context mature_context{ pooled_flags, 200, 9 };
    const system::chain::context mature_block{ pooled_flags, 0, 9, 200, 0, 0, 0 };
    const auto parent = make_spend(source.hash(false), 0, 4'000'000'000);
    const auto child = make_spend(parent.hash(false), 0, 3'000'000'000);
    const auto block = make_block({ make_coinbase(1), parent, child });
    BOOST_REQUIRE(query.set(block, mature_context, {}, false, false));
    BOOST_REQUIRE(test::bind(query, block));

    parent.inputs_ptr()->front()->metadata.coinbase = true;
    BOOST_REQUIRE(pool_tx(query, parent, 5'000'000'000, mature_context));
    BOOST_REQUIRE(pool_tx(query, child, 4'000'000'000, mature_context));

    const auto link = query.to_header(block.hash());
    BOOST_REQUIRE_EQUAL(query.validate_pooled(link, mature_block, pooled_interval, pooled_subsidy), error::success);
    BOOST_REQUIRE_EQUAL(query.block_confirmable(link), error::success);
}

BOOST_AUTO_TEST_CASE(query_consensus__validate_pooled__disabled__unvalidated)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    settings.pool.buckets = 0;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE_EQUAL(query.validate_pooled(0, block_context, pooled_interval, pooled_subsidy), error::unvalidated);
}

// block_confirmable requires bound inputs.
// ----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(query_consensus__block_confirmable__unbound__integrity_spendable)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1a, database::context{ 0, 1, 0 }, {}, false, false));
    BOOST_REQUIRE(query.set_strong(1));
    BOOST_REQUIRE(query.set(test::block_coinbase_spend_1a, database::context{ 0, 2, 0 }, {}, false, false));
    BOOST_REQUIRE(query.set_strong(2));
    BOOST_REQUIRE_EQUAL(query.block_confirmable(2), error::integrity_spendable);
}

BOOST_AUTO_TEST_CASE(query_consensus__block_confirmable__bound__success)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1a, database::context{ 0, 1, 0 }, {}, false, false));
    BOOST_REQUIRE(query.set_strong(1));
    BOOST_REQUIRE(query.set(test::block_coinbase_spend_1a, database::context{ 0, 2, 0 }, {}, false, false));
    BOOST_REQUIRE(test::bind(query, test::block_coinbase_spend_1a));
    BOOST_REQUIRE(query.set_strong(2));
    BOOST_REQUIRE_EQUAL(query.block_confirmable(2), error::success);
}

BOOST_AUTO_TEST_SUITE_END()
