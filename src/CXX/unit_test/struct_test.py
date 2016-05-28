#!/usr/bin/env python

import paths

import time
import random
import sys


def rand_str(alphabet, min_len, max_len):
    """
    Generate random string from an alphabet.
    :param alphabet: Alphabet
    :param min_len: Minimal length of the string
    :param max_len: Maximal length of the string
    :return: Random string
    """
    return ''.join(map(
        lambda i: alphabet[random.randint(0, len(alphabet) - 1)],
        xrange(random.randint(min_len, max_len))))


#
# Main
#

if __name__ == "__main__":
    key_cnt  = 10
    min_len  = 0
    max_len  = 255
    rng_seed = int(time.time())

    # Seed RNG
    sys.stderr.write("RNG seeded by %s\n" % (rng_seed,))
    random.seed(rng_seed)

    alphabet  = ""
    alphabet += ''.join(map(chr, xrange(ord('a'), ord('z') + 1)))
    alphabet += ''.join(map(chr, xrange(ord('A'), ord('Z') + 1)))
    alphabet += ''.join(map(chr, xrange(ord('0'), ord('9') + 1)))
    #alphabet += ''.join(map(chr, xrange(ord('A'), ord('H') + 1)))

    key_vals = dict()
    while len(key_vals) < key_cnt:
        key_vals[rand_str(alphabet, min_len, max_len)] = len(key_vals)

    for key, val in key_vals.iteritems():
        print "%s %d" % (key, val)

    paths = paths.key_vals2paths(key_vals.iteritems())

    for path in paths:
        print path
