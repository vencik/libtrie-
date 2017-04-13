/**
 *  \file
 *  \brief  TRIE paths
 *
 *  \date   2016/05/29
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

#include <algorithm>
#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <exception>
#include <stdexcept>
#include <regex>


/** Print TRIE paths */
static int trie_paths() {
    int error_cnt = 0;

    std::cerr << "TRIE paths BEGIN" << std::endl;

    // Read action/value/key input
    std::vector<std::tuple<char, int, std::string> > input;

    for (bool eof = false; !eof; ) {
        std::string line;
        std::getline(std::cin, line);
        eof = std::cin.eof();

        // Also include last line of a file with missing final EoL
        if (eof && line.empty()) break;

        // Parse input line
        static const std::regex line_regex(
            "^[ \\t]*([AR])[ \\t]+(\\d+)[ \\t]+([^ \\t]*)");

        std::smatch bref;
        if (!std::regex_match(line, bref, line_regex)) {
            std::cerr << "Syntax error: '" << line << '\'' << std::endl;
            throw std::runtime_error("Syntax error");
        }

        // Value (numeric)
        int val;
        std::stringstream val_ss(bref[2]);
        val_ss >> val;

        input.emplace_back(bref[1].str()[0], val, bref[3]);
    }

    std::cerr << "Creating TRIE..." << std::endl;

    // Create TRIE
    std::map<int, const std::string &> keymap;
    std::for_each(input.begin(), input.end(),
    [&keymap](const std::tuple<char, int, std::string> & i) {
        keymap.emplace(std::get<1>(i), std::get<2>(i));
    });

    auto _key = [&keymap](int i) -> const std::string & {
        const auto iter = keymap.find(i);
        if (keymap.end() == iter)
            throw std::logic_error(
                "INTERNAL ERROR: item key not found");

        return iter->second;
    };

    auto key = [&_key](int i) -> const unsigned char * {
        return (const unsigned char *)_key(i).data();
    };

    auto key_len = [&_key](int i) -> size_t {
        return _key(i).size();
    };

    container::trie<int, decltype(key), decltype(key_len)> trie(key, key_len);

    std::cerr << "Building TRIE..." << std::endl;

    // Build TRIE structure
    std::for_each(input.begin(), input.end(),
    [&trie](const std::tuple<char, int, std::string> & i) {
        switch (std::get<0>(i)) {
            // Add new key
            case 'A':
                trie.insert(std::get<1>(i));
                break;

            // Remove key
            case 'R': {
                decltype(trie)::iterator iter = trie.find(std::get<1>(i));
                if (trie.end() == iter) break;  // item not inserted, yet

                trie.erase(iter);
                break;
            }

            default:  // unknown action
                throw std::logic_error(
                    "INTERNAL ERROR: action not implemented");
        }

        //std::cerr << trie << std::endl;
    });

    std::cerr << "TRIE paths:" << std::endl;

    // Print TRIE paths
    trie.serialise_paths(std::cout);

    std::cerr << "TRIE paths END" << std::endl;

    return error_cnt;
}


/** Main routine (implementation) */
static int main_impl(int argc, char * const argv[]) {
    int exit_code = 64;  // pessimistic assumption

    do {  // pragmatic do ... while (0) loop allowing for breaks
        exit_code = trie_paths();
        if (0 != exit_code) break;

    } while (0);  // end of pragmatic loop

    return exit_code;
}

/** Main routine (exception-safe wrapper) */
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

    std::cerr
        << "Exit code: " << exit_code
        << std::endl;

    return exit_code;
}
