#!/usr/bin/env python

import sys


##
#  \brief  Produce quad-bit string
#
#  \param  str  string
#
#  \return Quad-bit string
#
def str2qbit_str(str):
    qbit = map(lambda c: [ord(c) >> 4, ord(c) & 0xf], str)

    qstr = []
    for lo, hi in qbit:
        qstr += [lo, hi]

    return qstr


##
#  \brief  Build TRIE paths from list of (qkey, value) tuples
#
#  \param  qkey_vals  List of (qkey, value) tuples
#  \param  index      Current key index
#  \param  begin      Begin index of (qkey, value) tuple
#  \param  end        End index (one index too far)
#  \param  node       Default node indication
#
#  \return TRIE path list
#
def _build_paths(qkey_vals, index, begin, end, node):
    paths = []

    # Keys are sorted, empty must be the 1st one if any
    if not index < len(qkey_vals[begin][0]):
        node   = "[" + str(qkey_vals[begin][1]) + "]"  # value node
        begin += 1

    # Divide and conquer
    for i in xrange(begin + 1, end):
        if qkey_vals[i][0][index] != qkey_vals[begin][0][index]:
            if len(node) == 0: node = "[]"  # internal node

            paths += map(
                lambda path:
                    node + "%x" % qkey_vals[begin][0][index] + path,
                _build_paths(qkey_vals, index + 1, begin, i, ""))

            begin = i

    # Finish the above (this sub-step might be the only one)
    if begin < end:
        paths += map(
            lambda path:
                node + "%x" % qkey_vals[begin][0][index] + path,
            _build_paths(qkey_vals, index + 1, begin, end, ""))

    # End of single path case
    else:
        paths.append(node)

    return paths


## See _build_paths
def build_paths(qkey_vals):
    if len(qkey_vals) == 0: return ["[]"]

    return _build_paths(qkey_vals, 0, 0, len(qkey_vals), "[]")  # show root node


##
#  \brief  Provide TRIE paths for list of (key, value) tuples
#
#  \param  key_vals  (key, value) tuples
#
#  \return List of TRIE (quad-bit) paths
#
def key_vals2paths(key_vals):
    # Sort by keys lexicographically
    sorted_key_vals = sorted(key_vals, cmp, lambda k_v: k_v[0])

    # Keys in qbit strings
    qbit_key_vals = map(lambda k_v:
        (str2qbit_str(k_v[0]), k_v[1]), sorted_key_vals)

    # Build paths
    return build_paths(qbit_key_vals)


##
#  \brief  Provide TRIE paths for list of keys
#
#  Uses key index as value.
#
#  \param  keys  Key strings
#
#  \return List of TRIE (quad-bit) paths
#
def keys2paths(keys):
    return key_vals2paths(map(lambda no_line: (no_line[1], no_line[0]),
        enumerate(keys)))


#
# Main
#

if __name__ == "__main__":
    paths = keys2paths(sys.stdin.read().splitlines())

    for path in paths:
        print(path)
