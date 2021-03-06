/*

Copyright (c) 2018, Steven Siloti
Copyright (c) 2015-2017, 2019-2020, Arvid Norberg
Copyright (c) 2016, Andrei Kurushin
Copyright (c) 2016, 2018, Alden Torres
All rights reserved.

You may use, distribute and modify this code under the terms of the BSD license,
see LICENSE file.
*/

#include "test.hpp"

#ifndef TORRENT_DISABLE_MUTABLE_TORRENTS

#include "libtorrent/torrent_info.hpp"
#include "libtorrent/resolve_links.hpp"
#include "libtorrent/aux_/path.hpp" // for combine_path
#include "libtorrent/hex.hpp" // to_hex
#include "libtorrent/create_torrent.hpp"

#include <functional>

using namespace lt;
using namespace std::placeholders;

struct test_torrent_t
{
	char const* filename1;
	char const* filename2;
	std::string::size_type expected_matches;
};

static test_torrent_t test_torrents[] = {
	// no match because shared file in test2 and test3 is not padded/aligned
	{ "test2", "test1_pad_files", 0},
	{ "test3", "test1_pad_files", 0},

	// in this case, test1 happens to have the shared file as the first one,
	// which makes it padded, however, the tail of it isn't padded, so it
	// still overlaps with the next file
	{ "test1", "test1_pad_files", 0},

	// test2 and test3 don't have the shared file aligned
	{ "test2", "test1_pad_files", 0},
	{ "test3", "test1_pad_files", 0},
	{ "test2", "test1_single", 0},

	// these are all padded. The first small file will accidentally also
	// match, even though it's not tail padded, the following file is identical
	{ "test2_pad_files", "test1_pad_files", 2},
	{ "test3_pad_files", "test1_pad_files", 2},
	{ "test3_pad_files", "test2_pad_files", 2},
	{ "test1_pad_files", "test2_pad_files", 2},
	{ "test1_pad_files", "test3_pad_files", 2},
	{ "test2_pad_files", "test3_pad_files", 2},

	// one might expect this to work, but since the tail of the single file
	// torrent is not padded, the last piece hash won't match
	{ "test1_pad_files", "test1_single", 0},

	// if it's padded on the other hand, it will work
	{ "test1_pad_files", "test1_single_padded", 1},

	// TODO: test files with different piece size (negative test)
};

// TODO: it would be nice to test resolving of more than just 2 files as well.
// like 3 single file torrents merged into one, resolving all 3 files.

TORRENT_TEST(resolve_links)
{
	std::string path = combine_path(parent_path(current_working_directory())
		, "mutable_test_torrents");

	for (int i = 0; i < int(sizeof(test_torrents)/sizeof(test_torrents[0])); ++i)
	{
		test_torrent_t const& e = test_torrents[i];

		std::string p = combine_path(path, e.filename1) + ".torrent";
		std::printf("loading %s\n", p.c_str());
		std::shared_ptr<torrent_info> ti1 = std::make_shared<torrent_info>(p);

		p = combine_path(path, e.filename2) + ".torrent";
		std::printf("loading %s\n", p.c_str());
		std::shared_ptr<torrent_info> ti2 = std::make_shared<torrent_info>(p);

		std::printf("resolving\n");
		resolve_links l(ti1);
		l.match(ti2, ".");

		aux::vector<resolve_links::link_t, file_index_t> const& links = l.get_links();

		auto const num_matches = std::size_t(std::count_if(links.begin(), links.end()
			, std::bind(&resolve_links::link_t::ti, _1)));

		// some debug output in case the test fails
		if (num_matches > e.expected_matches)
		{
			file_storage const& fs = ti1->files();
			for (file_index_t idx{0}; idx != links.end_index(); ++idx)
			{
				TORRENT_ASSERT(idx < file_index_t{fs.num_files()});
				std::printf("%*s --> %s : %d\n"
					, int(fs.file_name(idx).size())
					, fs.file_name(idx).data()
					, links[idx].ti
					? aux::to_hex(links[idx].ti->info_hash()).c_str()
					: "", static_cast<int>(links[idx].file_idx));
			}
		}

		TEST_EQUAL(num_matches, e.expected_matches);

	}
}

// this ensure that internally there is a range lookup
// since the zero-hash piece is in the second place
TORRENT_TEST(range_lookup_duplicated_files)
{
	file_storage fs1;
	file_storage fs2;

	fs1.add_file("test_resolve_links_dir/tmp1", 1024);
	fs1.add_file("test_resolve_links_dir/tmp2", 1024);
	fs2.add_file("test_resolve_links_dir/tmp1", 1024);
	fs2.add_file("test_resolve_links_dir/tmp2", 1024);

	lt::create_torrent t1(fs1, 1024, lt::create_torrent::v1_only);
	lt::create_torrent t2(fs2, 1024, lt::create_torrent::v1_only);

	t1.set_hash(piece_index_t{0}, sha1_hash::max());
	t1.set_hash(piece_index_t{1}, sha1_hash::max());
	t2.set_hash(piece_index_t{0}, sha1_hash::max());
	t2.set_hash(piece_index_t{1}, sha1_hash("01234567890123456789"));

	std::vector<char> tmp1;
	std::vector<char> tmp2;
	bencode(std::back_inserter(tmp1), t1.generate());
	bencode(std::back_inserter(tmp2), t2.generate());
	auto ti1 = std::make_shared<torrent_info>(tmp1, from_span);
	auto ti2 = std::make_shared<torrent_info>(tmp2, from_span);

	std::printf("resolving\n");
	resolve_links l(ti1);
	l.match(ti2, ".");

	aux::vector<resolve_links::link_t, file_index_t> const& links = l.get_links();

	auto const num_matches = std::count_if(links.begin(), links.end()
		, std::bind(&resolve_links::link_t::ti, _1));

	TEST_EQUAL(num_matches, 1);
}

#else
TORRENT_TEST(empty)
{
	TEST_CHECK(true);
}
#endif // TORRENT_DISABLE_MUTABLE_TORRENTS
