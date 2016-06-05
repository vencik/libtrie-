#!/usr/bin/env python

import paths

import time
import random
import sys
import subprocess


##
#  \brief  Generate random string from an alphabet.
#
#  \param  alphabet  Alphabet
#  \param  min_len   Minimal length of the string
#  \param  max_len   Maximal length of the string
#
#  \return Random string
#
def rand_str(alphabet, min_len, max_len):
    return ''.join(map(
        lambda i: alphabet[random.randint(0, len(alphabet) - 1)],
        xrange(random.randint(min_len, max_len))))


##
#  \brief  Get list of TRIE paths as produced by the libtrie++ library
#
#  Provides list of TRIE paths (in DFS order) obrained by calling
#  the \c paths binary.
#
#  \param  keys  Key list
#
#  \return TRIE paths list as produced by the \c paths binary
#
def libtriexx_paths(keys):
    child = subprocess.Popen("./paths",
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)

    out, err = child.communicate(input='\n'.join(keys))

    if 0 != child.returncode:
        raise Error("paths binary failed: %d: %s" % (child.returncode, err))

    return out.splitlines()


class Error(Exception):
    def __init__(self, msg):
        return super(type(self)).__init__(msg)


#
# Main
#

if __name__ == "__main__":
    def next_arg(default=None):
        next_arg.no += 1

        retval = \
            sys.argv[next_arg.no] if len(sys.argv) > next_arg.no else default

        # Type conversions
        if type(default) in (int,): return int(retval)

        return retval

    next_arg.no = 0

    key_cnt = next_arg(4096)
    min_len = next_arg(0)
    max_len = next_arg(256)
    ab_size = next_arg(0)     # 0 means full alphabet

    rng_seed = next_arg(int(time.time()))

    # Seed RNG
    sys.stderr.write("RNG seeded by %s\n" % (rng_seed,))
    random.seed(rng_seed)

    alphabet  = ""
    alphabet += ''.join(map(chr, xrange(ord('a'), ord('z') + 1)))
    alphabet += ''.join(map(chr, xrange(ord('A'), ord('Z') + 1)))
    alphabet += ''.join(map(chr, xrange(ord('0'), ord('9') + 1)))

    if ab_size > 0:
        alphabet = alphabet[:ab_size]

    keys     = list()
    key_vals = dict()
    while len(key_vals) < key_cnt:
        key = rand_str(alphabet, min_len, max_len)

        # Ensure key uniqueness (for paths.key_vals2paths)
        if not key in key_vals:
            key_vals[key] = len(key_vals)
            keys.append(key)

    print("Generated keys (with quad-bit representations):")
    for key_no, key in enumerate(keys):
        print("%d: \"%s\": %s" %
            (key_no, key, ''.join(["%02x" % (ord(c),) for c in key])))

    paths = paths.keys2paths(keys)

    print("TRIE paths:")
    for path_no, path in enumerate(paths):
        print("%d: %s" % (path_no, path))

    libpaths = libtriexx_paths(keys)

    print("libtrie++ paths:")
    for path_no, path in enumerate(libpaths):
        print("%d: %s" % (path_no, path))

    # Test result
    err_cnt  = 0
    path_cnt = min(len(paths), len(libpaths))

    if len(paths) != len(libpaths):
        sys.stderr.write("TRIE paths count differs, "
              "expected %d, got %d\n" % (len(paths), len(libpaths)))
        err_cnt += 1

    path_no = 0
    for path, libpath in zip(paths[:path_cnt], libpaths[:path_cnt]):
        if path != libpath:
            sys.stderr.write("Path %d mismatch: "
                  "expected\n%s\ngot\n%s\n" % (path_no, path, libpath))
            err_cnt += 1

        path_no += 1

    sys.stderr.write("Exit code: %d\n" % (err_cnt,))
    sys.exit(err_cnt)
