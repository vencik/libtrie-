#!/usr/bin/env python3

from paths import val_keys2paths

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
        range(random.randint(min_len, max_len))))


##
#  \brief  Get list of TRIE paths as produced by the libtrie++ library
#
#  Provides list of TRIE paths (in DFS order) obrained by calling
#  the \c paths binary.
#
#  \param  val_keys  (value, key) tuple list
#  \param  rm_keys   (value, key) list (to be removed)
#
#  \return TRIE paths list as produced by the \c paths binary
#
def libtriexx_paths(val_keys, rm_keys):
    # Create input for libtriexx binary
    add_lines = ["A %d %s" % (val, key) for val, key in val_keys]
    rm_lines  = ["R %d %s" % (val, key) for val, key in rm_keys]

    # Randomly permutate additions
    for _i in range(int(len(add_lines) / 2)):
        i, j = (
            random.randint(0, len(add_lines) - 1),
            random.randint(0, len(add_lines) - 1))

        swp = add_lines[i]
        add_lines[i] = add_lines[j]
        add_lines[j] = swp

    # Randomly insert removals
    input_lines = add_lines
    for rm in rm_lines:
        input_lines.insert(random.randint(0, len(input_lines)), rm)

    # Append removals again (to make sure they all get removed)
    input_lines += rm_lines
    input_lines = '\n'.join(input_lines)

    print("libtree++ input lines:\n%s" % (input_lines,))

    # Run libtriexx binary
    child = subprocess.Popen("./paths",
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)

    out, err = child.communicate(
        input=bytes(input_lines, encoding="ascii"))
    out, err = (str(out, encoding="ascii"), str(err, encoding="ascii"))

    if 0 != child.returncode:
        raise Error("paths binary failed: %d: %s" % (child.returncode, err))

    return out.splitlines()


class Error(Exception):
    def __init__(self, msg):
        return super(type(self), self).__init__(msg)


##
#  \brief  Main routine
#
#  \param  argv  Command line arguments
#
#  \return Exit code
#
def main(argv):
    def next_arg(default=None):
        next_arg.no += 1

        retval = \
            argv[next_arg.no] if len(argv) > next_arg.no else default

        # Type conversions
        if type(default) in (int,):   return int(retval)
        if type(default) in (float,): return float(retval)

        return retval

    next_arg.no = 0

    key_cnt = next_arg(4096)
    min_len = next_arg(0)
    max_len = next_arg(256)
    ab_size = next_arg(0)     # 0 means full alphabet
    rm_rate = next_arg(0.0)

    rng_seed = next_arg(int(time.time()))

    # Seed RNG
    sys.stderr.write("RNG seeded by %s\n" % (rng_seed,))
    random.seed(rng_seed)

    alphabet  = ""
    alphabet += ''.join(map(chr, range(ord('a'), ord('z') + 1)))
    alphabet += ''.join(map(chr, range(ord('A'), ord('Z') + 1)))
    alphabet += ''.join(map(chr, range(ord('0'), ord('9') + 1)))

    if ab_size > 0:
        alphabet = alphabet[:ab_size]

    keys     = list()
    key_uniq = dict()
    while len(key_uniq) < key_cnt:
        key = rand_str(alphabet, min_len, max_len)

        # Ensure key uniqueness (for paths.key_vals2paths)
        if not key in key_uniq:
            key_uniq[key] = len(key_uniq)
            keys.append(key)

    val_keys = list(enumerate(keys))

    print("Generated keys (with quad-bit representations):")
    for key_no, key in val_keys:
        print("%d: \"%s\": %s" %
            (key_no, key, ''.join(["%02x" % (ord(c),) for c in key])))

    rm_pos  = len(val_keys) - int(len(val_keys) * rm_rate)
    rm_keys = val_keys[rm_pos:len(val_keys)]

    print("Last %d shall be removed." % (len(rm_keys),))

    paths = val_keys2paths(val_keys[:rm_pos])

    print("Final TRIE paths:")
    for path_no, path in enumerate(paths):
        print("%d: %s" % (path_no, path))

    libpaths = libtriexx_paths(val_keys, rm_keys)

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

    return err_cnt

if __name__ == "__main__":
    sys.exit(main(sys.argv))
