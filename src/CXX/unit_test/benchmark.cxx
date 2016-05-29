/**
 *  \file
 *  \brief  TRIE benchmark
 *
 *  TRIE operations complexity benchmark.
 *
 *  \date   2016/04/06
 *  \author Vaclav Krpec  <vencik@razdva.cz>
 *
 *
 *  LEGAL NOTICE
 *
 *  Copyright (c) 2016, Vaclav Krpec
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the distribution.
 *
 *  3. Neither the name of the copyright holder nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
 *  OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <libtrie++/trie.hxx>

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <iostream>
#include <exception>
#include <stdexcept>
#include <cassert>
#include <cstdlib>

extern "C" {
#include <time.h>
#include <getopt.h>
}


/**
 *  \brief  Print TRIE (as key -> value table)
 *
 *  \param  out   Output stream
 *  \param  trie  TRIE
 */
template <typename T, class KeyFn, class KeyLenFn>
static void print_trie(
    std::ostream & out,
    const container::trie<T, KeyFn, KeyLenFn> & trie)
{
    out << "Trie dump:" << std::endl << trie << std::endl;

    typedef container::trie<T, KeyFn, KeyLenFn> trie_t;

    out << "Key -> value pairs in key order:" << std::endl;
    std::for_each(trie.begin(), trie.end(),
    [&out](const typename trie_t::const_iterator::deref_t & d) {
        std::ios iostate(NULL);
        iostate.copyfmt(out);

        out << std::setw(2) << std::setfill('0') << std::hex;
        for (size_t i = 0; i < std::get<1>(d); ++i) {
            unsigned char byte = std::get<0>(d)[i];
            byte = (byte << 4) | (byte >> 4);
            out << (unsigned int)byte;
        }

        out.copyfmt(iostate);

        out << " -> " << std::get<2>(d) << std::endl;
    });
}

/**
 *  \brief  Random integer withing bounds
 *
 *  \param  lo  Lower bound
 *  \param  hi  Upper bound
 *
 *  \return Random integer from interval [lo, hi]
 */
static int rand_int(int lo, int hi) {
    double x = ::rand() / RAND_MAX;  // 0 <= x <= 1
    x *= hi - lo;                    // 0 <= x <= hi - lo

    return lo + (int)x;  // lo <= x <= hi
}


/**
 *  \brief  Generate string
 *
 *  \param  alphabet  Alphabet
 *
 *  \return Random string of characters from alphabet specified
 */
static std::string generate_string(
    const std::string & alphabet,
    size_t              len_min,
    size_t              len_max)
{
    assert(len_min <= len_max);

    size_t len = rand_int(len_min, len_max);
    std::string str; str.reserve(len);

    for (; len; --len) {
        size_t i = rand_int(0, alphabet.size() - 1);
        str.push_back(alphabet[i]);
    }

    return str;
}


/**
 *  \brief  Get timestamp
 *
 *  Uses monotonic clock to obtain high-precision timestamp.
 *
 *  \return Timestamp (in seconds)
 */
inline static double timestamp() {
    struct timespec tspec;
    assert(-1 != clock_gettime(CLOCK_MONOTONIC_RAW, &tspec));

    return (double)tspec.tv_sec + (double)tspec.tv_nsec / 1000000000.0;
}


/**
 *  \brief  Benchmark result
 *
 *  \param  test       Test identification
 *  \param  loops      Number of loops
 *  \param  trie_time  TRIE total time
 *  \param  map_time   \c std::map total time
 */
static void result(
    const std::string & test,
    size_t              loops,
    double              trie_time,
    double              map_time)
{
    std::cerr
        << test << ":" << std::endl
        << "container::trie time: " << trie_time << " s ("
        << trie_time / loops << " s per insert avg)" << std::endl
        << "std::map time: " << map_time << " s ("
        << map_time / loops << " s per insert avg)" << std::endl;

    double time_ratio = map_time / trie_time;
    bool trie_faster = time_ratio > 1.0;
    if (!trie_faster) time_ratio = 1.0 / time_ratio;
    double percent_diff = (int)(1000.0 * (time_ratio - 1.0)) / 10.0;

    std::cerr
        << "TRIE is " << time_ratio << " times "
        << (trie_faster ? "FASTER" : "SLOWER")
        << " than map (that's about " << percent_diff << "%)"
        << std::endl;
}


/**
 *  \brief  String-keyed TRIE benchmark
 *
 *  \param  n              Number of test keys generated
 *  \param  prefix_cnt     Number of common key prefixes generated
 *  \param  prefix_min     Common key prefix minimal length
 *  \param  prefix_max     Common key prefix maximal length
 *  \param  key_min        Key minimal length
 *  \param  key_max        Key maximal length
 *  \param  misses_per100  Key search miss percentage (random key used)
 *
 *  \return Error count
 */
static int string_trie_benchmark(
    size_t n,
    size_t prefix_cnt,
    size_t prefix_min,
    size_t prefix_max,
    size_t key_min,
    size_t key_max,
    int    misses_per100)
{
    int error_cnt = 0;

    std::cerr << "String TRIE benchmark BEGIN" << std::endl;

    // Alphabet
    size_t alphabet_size = 64;
    std::string alphabet; alphabet.reserve(alphabet_size);
    for (size_t i = 0; i < alphabet_size; ++i)
        alphabet.push_back('A' + i);

    // Prefixes
    std::vector<std::string> prefixes; prefixes.reserve(prefix_cnt);
    for (size_t i = 0; i < prefix_cnt; ++i)
        prefixes.push_back(
            generate_string(alphabet, prefix_min, prefix_max));

    // Key generator
    auto generate_key =
    [&alphabet, &prefixes](size_t min, size_t max) -> std::string {
        size_t prefix_ix = rand_int(0, prefixes.size() - 1);
        const std::string & prefix = prefixes[prefix_ix];
        return prefix + generate_string(alphabet,
            min < prefix.size() ? 0 : min - prefix.size(),
            max < prefix.size() ? 0 : max - prefix.size());
    };

    // Containers
    std::vector<std::string>    keys; keys.reserve(n);
    container::string_trie<int> trie;
    std::map<std::string, int>  map;

    // Insert benchmark
    double trie_time = 0.0;
    double map_time  = 0.0;

    for (size_t i = 0; i < n; ++i) {
        const std::string key = generate_key(key_min, key_max);
        keys.push_back(key);

        trie_time -= timestamp();
        trie.insert(std::make_tuple(key, (int)i));
        trie_time += timestamp();

        map_time -= timestamp();
        map.emplace(key, i);
        map_time += timestamp();
    }

    result("Insert", n, trie_time, map_time);

    //print_trie(std::cerr, trie);

    // Find benchmark
    trie_time = 0.0;
    map_time  = 0.0;

    for (size_t i = 0; i < n; ++i) {
        int key_ix = -1;
        const std::string key = rand_int(0, 99) < misses_per100
            ? generate_key(key_min, key_max)
            : keys[key_ix = rand_int(0, keys.size() - 1)];

        trie_time -= timestamp();
        trie.find((const unsigned char *)key.data(), key.size());
        trie_time += timestamp();

        map_time -= timestamp();
        map.find(key);
        map_time += timestamp();
    }

    result("Search", n, trie_time, map_time);

    std::cerr << "String TRIE benchmark END" << std::endl;

    return error_cnt;
}


/** Benchmark */
static int main_impl(int argc, char * const argv[]) {
    int exit_code = 64;  // pessimistic assumption

    unsigned rng_seed = 0;  // random number generator seed

    // Default benchmark parameters
    size_t n             = 1000000;
    size_t prefix_cnt    = 12;
    size_t prefix_min    = 8;
    size_t prefix_max    = 64;
    size_t key_min       = 12;
    size_t key_max       = 256;
    int    misses_per100 = 15;

    // Usage
    auto usage = [&argv,
        n, prefix_cnt, prefix_min, prefix_max,
        key_min, key_max,
        misses_per100]()
    { std::cerr <<
"Usage: " << argv[0] << " [OPTIONS]\n\n"
"OPTIONS:\n"
"    -h, --help                 show help and exit\n"
"    -s, --rng-seed <seed>      RNG seed (0 means current time)\n"
"\n"
"    -n, --loop-count   <cnt>   Number of generated keys\n"
"                               default: " << n << "\n"
"    -c, --prefix-count <cnt>   Number of pre-generated key prefixes\n"
"                               default: " << prefix_cnt << "\n"
"    -p, --prefix-min   <min>   Key prefix min. length\n"
"                               default: " << prefix_min << "\n"
"    -P, --prefix-max   <max>   Key prefix max. length\n"
"                               default: " << prefix_max << "\n"
"    -k, --key-min      <min>   Key min. length\n"
"                               default: " << key_min << "\n"
"    -K, --key-max      <max>   Key max. length\n"
"                               default: " << key_max << "\n"
"    -m, --misses-per100  <%>   Find key misses (in %)\n"
"                               default: " << misses_per100 << "\n"
"\n"; };

    // Options
    static const struct option long_opts[] {
        { "help",     no_argument,       NULL, 'h' },
        { "rng-seed", required_argument, NULL, 's' },

        // Benchmark parameters
        { "loop-count",     required_argument, NULL, 'n' },
        { "prefix-count",   required_argument, NULL, 'c' },
        { "prefix-min",     required_argument, NULL, 'p' },
        { "prefix-max",     required_argument, NULL, 'P' },
        { "key-min",        required_argument, NULL, 'k' },
        { "key-max",        required_argument, NULL, 'K' },
        { "misses-per100",  required_argument, NULL, 'm' },

        //{ "", required|no_argument, NULL, '' },

        { NULL, 0, NULL, '\0' }  // terminator
    };

    for (;;) {
        int long_opt_ix;
        int opt = getopt_long(argc, argv,
            ":hs:n:c:p:P:k:K:m:",
            long_opts, &long_opt_ix);

        if (-1 == opt) break;  // no more options

        switch (opt) {
            case 'h':  // help
                usage();
                ::exit(0);
                break;

            case 's':  // RNG seed
                rng_seed = ::atoi(optarg);
                break;

            // Benchmark parameters
            case 'n':  // loop count
                n = ::atoi(optarg);
                break;

            case 'c':  // prefix count
                prefix_cnt = ::atoi(optarg);
                break;

            case 'p':  // prefix min. length
                prefix_min = ::atoi(optarg);
                break;

            case 'P':  // prefix max. length
                prefix_max = ::atoi(optarg);
                break;

            case 'k':  // key min. length
                key_min = ::atoi(optarg);
                break;

            case 'K':  // key max. length
                key_max = ::atoi(optarg);
                break;

            case 'm':  // find benchmark misses [%]
                misses_per100 = ::atoi(optarg);
                break;

            case '?':  // unknown option
            case ':':  // missing argument
                usage();
                ::exit(1);

            default:  // internal error (forgotten option)
                std::cerr
                    << "INTERNAL ERROR: forgotten option " << (char)opt
                    << std::endl;
                ::abort();
        }
    }

    // Seed RNG
    if (0 == rng_seed) rng_seed = (unsigned)::time(NULL);
    ::srand(rng_seed);
    std::cerr << "RNG seeded with " << rng_seed << std::endl;

    exit_code = string_trie_benchmark(n,
        prefix_cnt, prefix_min, prefix_max,
        key_min, key_max,
        misses_per100);

    std::cerr
        << "Exit code: " << exit_code
        << std::endl;

    return exit_code;
}

/** Exception-safe wrapper */
int main(int argc, char * const argv[]) {
    int exit_code = 128;

    try {
        exit_code = main_impl(argc, argv);
    }
    catch (const std::exception & x) {
        std::cerr
            << "Standard exception caught: "
            << x.what()
            << std::endl;
    }
    catch (...) {
        std::cerr
            << "Unhandled non-standard exception caught"
            << std::endl;
    }

    return exit_code;
}
