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

struct store_setup_fixture
{
    DELETE4(store_setup_fixture);
    BC_PUSH_WARNING(NO_THROW_IN_NOEXCEPT)

        store_setup_fixture() NOEXCEPT
    {
        BOOST_REQUIRE(test::clear(test::directory));
    }

    ~store_setup_fixture() NOEXCEPT
    {
        BOOST_REQUIRE(test::clear(test::directory));
    }

    BC_POP_WARNING()
};

BOOST_FIXTURE_TEST_SUITE(store_tests, store_setup_fixture)

class access
  : public store
{
public:
    using path = std::filesystem::path;
    using store::store;

    // backup internals

    code backup_() NOEXCEPT
    {
        return store::backup();
    }

    code dump_() NOEXCEPT
    {
        return store::dump();
    }

    code restore_() NOEXCEPT
    {
        return store::restore();
    }

    const settings& configuration() const NOEXCEPT
    {
        return configuration_;
    }

    const path& header_head_file() const NOEXCEPT
    {
        return header_head_.file();
    }

    const path& header_body_file() const NOEXCEPT
    {
        return header_body_.file();
    }

    const path& point_head_file() const NOEXCEPT
    {
        return point_head_.file();
    }

    const path& point_body_file() const NOEXCEPT
    {
        return point_body_.file();
    }

    const path& input_head_file() const NOEXCEPT
    {
        return input_head_.file();
    }

    const path& input_body_file() const NOEXCEPT
    {
        return input_body_.file();
    }

    const path& output_body_file() const NOEXCEPT
    {
        return output_body_.file();
    }

    const path& puts_body_file() const NOEXCEPT
    {
        return puts_body_.file();
    }

    const path& tx_head_file() const NOEXCEPT
    {
        return tx_head_.file();
    }

    const path& tx_body_file() const NOEXCEPT
    {
        return tx_body_.file();
    }

    const path& txs_head_file() const NOEXCEPT
    {
        return txs_head_.file();
    }

    const path& txs_body_file() const NOEXCEPT
    {
        return txs_body_.file();
    }

    const path& flush_lock_file() const NOEXCEPT
    {
        return flush_lock_.file();
    }

    const path& process_lock_file() const NOEXCEPT
    {
        return process_lock_.file();
    }

    boost::upgrade_mutex& transactor_mutex() NOEXCEPT
    {
        return transactor_mutex_;
    }
};

// publics

BOOST_AUTO_TEST_CASE(store__create__default__success)
{
    settings configuration{};
    configuration.dir = TEST_DIRECTORY;
    store instance{ configuration };
    BOOST_REQUIRE_EQUAL(instance.create(), error::success);
}

BOOST_AUTO_TEST_CASE(store__open__default__success)
{
    settings configuration{};
    configuration.dir = TEST_DIRECTORY;
    store instance{ configuration };
    BOOST_REQUIRE_EQUAL(instance.create(), error::success);
    BOOST_REQUIRE_EQUAL(instance.open(), error::success);
    instance.close();
}

BOOST_AUTO_TEST_CASE(store__snapshot__always__expected)
{
    BOOST_REQUIRE(true);
}

BOOST_AUTO_TEST_CASE(store__close__default__success)
{
    settings configuration{};
    configuration.dir = TEST_DIRECTORY;
    store instance{ configuration };
    BOOST_REQUIRE_EQUAL(instance.create(), error::success);
    BOOST_REQUIRE_EQUAL(instance.open(), error::success);
    BOOST_REQUIRE_EQUAL(instance.close(), error::success);
}

BOOST_AUTO_TEST_CASE(store__get_transactor__always__share_locked)
{
    const settings configuration{};
    access instance{ configuration };
    auto transactor = instance.get_transactor();
    BOOST_REQUIRE(transactor);
    BOOST_REQUIRE(!instance.transactor_mutex().try_lock());
    BOOST_REQUIRE(instance.transactor_mutex().try_lock_shared());
}

// protecteds

BOOST_AUTO_TEST_CASE(store__backup__unloaded__expected_error)
{
    const settings configuration{};
    access instance{ configuration };
    BOOST_REQUIRE_EQUAL(instance.backup_(), error::unloaded_file);
}

BOOST_AUTO_TEST_CASE(store__dump__unloaded__expected_error)
{
    const settings configuration{};
    access instance{ configuration };
    BOOST_REQUIRE_EQUAL(instance.dump_(), error::unloaded_file);
}

BOOST_AUTO_TEST_CASE(store__restore__missing_backup__expected_error)
{
    const settings configuration{};
    access instance{ configuration };
    BOOST_REQUIRE_EQUAL(instance.restore_(), error::missing_backup);
}

BOOST_AUTO_TEST_CASE(store__construct__default_configuration__referenced)
{
    const settings configuration{};
    access instance{ configuration };
    BOOST_REQUIRE_EQUAL(&instance.configuration(), &configuration);
}

BOOST_AUTO_TEST_CASE(store__paths__default_configuration__expected)
{
    const settings configuration{};
    access instance{ configuration };
    BOOST_REQUIRE_EQUAL(instance.header_head_file(), "bitcoin/index/archive_header.idx");
    BOOST_REQUIRE_EQUAL(instance.header_body_file(), "bitcoin/archive_header.dat");
    BOOST_REQUIRE_EQUAL(instance.point_head_file(), "bitcoin/index/archive_point.idx");
    BOOST_REQUIRE_EQUAL(instance.point_body_file(), "bitcoin/archive_point.dat");
    BOOST_REQUIRE_EQUAL(instance.input_head_file(), "bitcoin/index/archive_input.idx");
    BOOST_REQUIRE_EQUAL(instance.input_body_file(), "bitcoin/archive_input.dat");
    BOOST_REQUIRE_EQUAL(instance.output_body_file(), "bitcoin/archive_output.dat");
    BOOST_REQUIRE_EQUAL(instance.puts_body_file(), "bitcoin/archive_puts.dat");
    BOOST_REQUIRE_EQUAL(instance.tx_head_file(), "bitcoin/index/archive_tx.idx");
    BOOST_REQUIRE_EQUAL(instance.tx_body_file(), "bitcoin/archive_tx.dat");
    BOOST_REQUIRE_EQUAL(instance.txs_head_file(), "bitcoin/index/archive_txs.idx");
    BOOST_REQUIRE_EQUAL(instance.txs_body_file(), "bitcoin/archive_txs.dat");
    BOOST_REQUIRE_EQUAL(instance.flush_lock_file(), "bitcoin/flush.lock");
    BOOST_REQUIRE_EQUAL(instance.process_lock_file(), "bitcoin/process.lock");
}

BOOST_AUTO_TEST_SUITE_END()
