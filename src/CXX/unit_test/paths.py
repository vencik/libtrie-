#!/usr/bin/env python3

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
#  \brief  Build TRIE paths from list of (value, qkey) tuples
#
#  \param  val_qkeys  List of (value, qkey) tuples
#  \param  index      Current key index
#  \param  begin      Begin index of (qkey, value) tuple
#  \param  end        End index (one index too far)
#  \param  node       Default node indication
#
#  \return TRIE path list
#
def _build_paths(val_qkeys, index, begin, end, node):
    paths = []

    # Keys are sorted, empty must be the 1st one if any
    if not index < len(val_qkeys[begin][1]):
        node   = "[" + str(val_qkeys[begin][0]) + "]"  # value node
        begin += 1

    # Divide and conquer
    for i in range(begin + 1, end):
        if val_qkeys[i][1][index] != val_qkeys[begin][1][index]:
            if len(node) == 0: node = "[]"  # internal node

            paths += list(map(
                lambda path:
                    node + "%x" % val_qkeys[begin][1][index] + path,
                _build_paths(val_qkeys, index + 1, begin, i, "")))

            begin = i

    # Finish the above (this sub-step might be the only one)
    if begin < end:
        paths += list(map(
            lambda path:
                node + "%x" % val_qkeys[begin][1][index] + path,
            _build_paths(val_qkeys, index + 1, begin, end, "")))

    # End of single path case
    else:
        paths.append(node)

    return paths


## See _build_paths
def build_paths(val_qkeys):
    if len(val_qkeys) == 0: return ["[]"]

    return _build_paths(val_qkeys, 0, 0,
        len(val_qkeys), "[]")  # show root


##
#  \brief  Provide TRIE paths for list of (key, value) tuples
#
#  \param  val_keys  (value, key) tuples
#
#  \return List of TRIE (quad-bit) paths
#
def val_keys2paths(val_keys):
    # Sort by keys lexicographically
    sorted_val_keys = sorted(val_keys, key=lambda k_v: k_v[1])

    # Keys in qbit strings
    val_qkeys = list(map(lambda v_k:
        (v_k[0], str2qbit_str(v_k[1])), sorted_val_keys))

    # Build paths
    return build_paths(val_qkeys)


##
#  \brief  Main routine
#
#  \param  argv  Command line arguments
#
#  \return Exit code
#
def main(argv):
    paths = val_keys2paths(
        list(map(lambda line: line.split(' '),
            sys.stdin.read().splitlines())))

    for path in paths:
        print(path)

    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))
