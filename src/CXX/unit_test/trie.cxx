/**
 *  \file
 *  \brief  TRIE unit test
 *
 *  \date   2016/02/28
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

#include <vector>
#include <algorithm>
#include <iostream>
#include <exception>
#include <stdexcept>


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
    out << "TRIE dump:" << std::endl << trie << std::endl;

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

    out << "TRIE paths dump:" << std::endl;
    trie.serialise_paths(out);
    out << std::endl;
}


/** TRIE unit test */
static int trie_test() {
    int error_cnt = 0;

    std::cerr << "TRIE test BEGIN" << std::endl;

    std::vector<std::string> str_list;
    str_list.push_back("\x01\x02\x03");  // 0
    str_list.push_back("\x01\x12\x03");  // 1
    str_list.push_back("\x02\x12\x03");  // 2
    str_list.push_back("\x10\x12\x03");  // 3
    str_list.push_back("\x10\x12");      // 4
    str_list.push_back("\x10\x13\x11");  // 5

    auto key = [&str_list](int i) -> const unsigned char * {
        return (const unsigned char *)str_list[i].data();
    };

    auto key_len = [&str_list](int i) -> size_t {
        return str_list[i].size();
    };

    container::trie<int, decltype(key), decltype(key_len)> trie(key, key_len);

    for (size_t i = 0; i < str_list.size(); ++i) {
        trie.insert(i);
        print_trie(std::cerr, trie);
    }

    std::cerr << "TRIE test END" << std::endl;

    return error_cnt;
}


/** String-keyed TRIE unit test */
static int string_trie_test() {
    int error_cnt = 0;

    std::cerr << "String TRIE test BEGIN" << std::endl;

    container::string_trie<int> trie;

    trie.insert(std::make_tuple<std::string, int>("abc", 13));
    trie.insert(std::make_tuple<std::string, int>("aBCDE", 25));
    trie.insert(std::make_tuple<std::string, int>("acde", 34));
    trie.insert(std::make_tuple<std::string, int>("abd", 43));
    trie.insert(std::make_tuple<std::string, int>("ab", 52));
    trie.insert(std::make_tuple<std::string, int>("abda", 64));

    print_trie(std::cerr, trie);

    std::cerr << "String TRIE test END" << std::endl;

    return error_cnt;
}


/** Unit test */
static int main_impl(int argc, char * const argv[]) {
    int exit_code = 64;  // pessimistic assumption

    do {  // pragmatic do ... while (0) loop allowing for breaks
        exit_code = trie_test();
        if (0 != exit_code) break;

        exit_code = string_trie_test();
        if (0 != exit_code) break;

    } while (0);  // end of pragmatic loop

    std::cerr
        << "Exit code: " << exit_code
        << std::endl;

    return exit_code;
}

/** Unit test exception-safe wrapper */
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
