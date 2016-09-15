#ifndef trie_hxx
#define trie_hxx

/**
 *  \file
 *  \brief  TRIE (aka reTRIEval tree)
 *
 *  \date   2016/02/10
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

#include <list>
#include <tuple>
#include <string>
#include <sstream>
#include <memory>
#include <cstdlib>
#include <iostream>
#include <iomanip>


// TODO: This should go to io:: namespace or somewhere...
/** Tuple serialiser */
template <class Tuple, int I>
class tuple_serialiser {
    private:

    tuple_serialiser<Tuple, I - 1> m_prefix;

    public:

    void operator () (
        std::ostream & out,
        const Tuple  & tuple,
        const char   * sep)
    const {
        m_prefix(out, tuple, ", ");
        out << std::get<I - 1>(tuple) << sep;
    }

};  // end of template class tuple_serialiser

/** Tuple serialiser (recursion fixed point) */
template <class Tuple>
class tuple_serialiser<Tuple, 0> {
    public:

    void operator () (
        std::ostream & out,
        const Tuple  & tuple,
        const char   * sep)
    const {
        out.put('(');
    }

};  // end of template class tuple_serialiser

/** Tuple serialisation operator */
template <typename ... Items>
inline std::ostream & operator << (
    std::ostream & out,
    const std::tuple<Items ...> & tuple)
{
    typedef std::tuple<Items ...> tuple_t;
    tuple_serialiser<tuple_t, std::tuple_size<tuple_t>::value> s;
    s(out, tuple, ")");
    return out;
}


namespace container {

namespace impl {

/** Identity */
template <typename T>
class identity {
    public:

    /** Pointer to \c inst bytes */
    inline const unsigned char * operator () (const T & inst) const {
        return reinterpret_cast<const unsigned char *>(&inst);
    }

};  // end of template class identity

/** Size in bytes */
template <typename T>
class size_of {
    public:

    /** Size of \c inst */
    inline size_t operator () (const T & inst) const { return sizeof(inst); }

};  // end of template class size_of

}  // end of namespace impl


/**
 *  \brief  TRIE
 *
 *  \tparam  T         Item type
 *  \tparam  KeyFn     Key getter type
 *  \tparam  KeyLenFn  Key length getter type
 */
template <
    typename T,
    class KeyFn    = impl::identity<T>,
    class KeyLenFn = impl::size_of<T> >
class trie {
    private:

    KeyFn    m_key_fn;      /**< Key getter        */
    KeyLenFn m_key_len_fn;  /**< Key length getter */

    typedef std::list<T> items_t;  /**< Item list */

    items_t m_items;  /**< Item list */

    /** TRIE node */
    struct node {
        typename items_t::iterator item;    /**< Item                     */
        const unsigned char *      key;     /**< Item key                 */
        size_t                     qlen;    /**< Key path quad-bit length */
        node *                     parent;  /**< Parent node              */

        /** Branches */
        std::unique_ptr<node> branches[1 << 4];

        /**
         *  \brief  Constructor
         *
         *  \param  _item    Item
         *  \param  _key     Item key
         *  \param  _qlen    Key path quad-bit length
         *  \param  _parent  Parent node
         */
        node(
            typename items_t::iterator _item,
            const unsigned char *      _key,
            size_t                     _qlen,
            node *                     _parent)
        :
            item   ( _item   ),
            key    ( _key    ),
            qlen   ( _qlen   ),
            parent ( _parent )
        {}

    };  // end of struct node

    node m_root;   /**< Root node */

    public:

    /**
     *  \brief  (Mis)match position specification
     *
     *  Entries:
     *  @0: TRIE node pointer
     *  @1: Matching key quad-bit length
     *  @2: End-of-path flag (\c true means complete key match)
     */
    typedef std::tuple<node *, size_t, bool> position_t;

    /** Position node (pointer) getter */
    inline static node * pos_node(const position_t & pos) {
        return std::get<0>(pos);
    }

    /** Position key quad-bit length getter */
    inline static size_t pos_qlen(const position_t & pos) {
        return std::get<1>(pos);
    }

    /** Position end-of-path (complete key match) flag getter */
    inline static bool pos_match(const position_t & pos) {
        return std::get<2>(pos);
    }

    private:

    /**
     *  \brief  Tracing miss action prototype
     *
     *  Action done when node is not found upon TRIE tracing.
     *
     *  \param  key   Key
     *  \param  len   Key length
     *  \param  nod   Current node
     *  \param  qlen  Quad-bit position of key mismatch
     *
     *  \return Position of the (original) mismatch
     */
    typedef position_t (trie::* trace_miss_t)(
        const unsigned char * key,
        size_t                len,
        node *                nod,
        size_t                qlen);

    /**
     *  \brief  Trace key
     *
     *  The function traces matching prefix of the key up to the point
     *  of mismatch (or end).
     *  TODO: describe the mismatch (branching) point
     *
     *
     *  \param  miss  Trace miss action
     *  \param  key   Item key
     *  \param  len   Item key length
     *
     *  \return Position of the (original) mismatch
     */
    position_t trace(
        trace_miss_t          miss,
        const unsigned char * key,
        size_t                len)
    const {
        const node * nod  = &m_root;
        size_t qlen = 0;

        for (size_t i = 0; i < len; ++i) {
            bool forward_branch = false;
            unsigned char byte = key[i];

            // Branching (qlen of 0 or 1 means high or low 1/2 byte)
            while (!(qlen & ~(size_t)0x1)) {
                const node * prev_nod = nod;
                nod = nod->branches[qlen ? byte & 0x0f : byte >> 4].get();

                if (NULL == nod) {  // no such branch
                    nod = prev_nod;

                    // Check high 1/2 byte, first, if the branch is on low
                    // 1/2 byte (i.e. amid a byte).
                    if (qlen && ((nod->key[i] ^ byte) >> 4))
                        return (const_cast<trie *>(this)->*miss)(
                            key, len, const_cast<node *>(nod->parent), i << 1);

                    return (const_cast<trie *>(this)->*miss)(
                        key, len, const_cast<node *>(nod), (i << 1) + qlen);
                }

                forward_branch = qlen > 0;  // branch 1/2 a byte ahead
                qlen = nod->qlen - (i << 1);
            }

            unsigned char mismatch = nod->key[i] ^ byte;
            if (mismatch) {
                const node * parent = nod->parent;
                if (forward_branch) parent = parent->parent;

                return (const_cast<trie *>(this)->*miss)(
                    key, len, const_cast<node *>(parent),
                    (i << 1) + !(mismatch & 0xf0));
            }

            qlen -= 2;
        }

        // Match
        if (0 == qlen)
            return position_t(
                const_cast<node *>(nod), len << 1, m_items.end() != nod->item);

        // Amid a branch
        return (const_cast<trie *>(this)->*miss)(
            key, len, const_cast<node *>(nod->parent), len << 1);
    }

    /**
     *  \brief  Get 1/2-byte from \c key at position \c qpos
     *
     *  \param  key   Key
     *  \param  qpos  1/2-byte position
     *
     *  \return 1/2 byte at position \c qpos
     */
    inline static size_t get_qpos(const unsigned char * key, size_t qpos) {
        unsigned char byte = key[qpos / 2];
        return qpos % 2 ? byte & 0x0f : byte >> 4;
    }

    /**
     *  \brief  Insert node for a key
     *
     *  \param  key   Key
     *  \param  len   Key length
     *  \param  nod   Current node
     *  \param  qlen  Quad-bit position of key mismatch
     *
     *  \return Position of inserted item
     */
    position_t insert_node(
        const unsigned char * key,
        size_t                len,
        node *                nod,
        size_t                qlen)
    {
        size_t br_ix   = get_qpos(key, nod->qlen);
        node * br_node = nod->branches[br_ix].get();

        // Create interim branch
        if (NULL != br_node) {
            size_t in_br_ix = get_qpos(br_node->key, qlen);
            node * in_node  = new node(m_items.end(), br_node->key, qlen, nod);

            in_node->branches[in_br_ix] = std::move(nod->branches[br_ix]);
            nod->branches[br_ix].reset(in_node);
            br_node->parent = in_node;

            // Interim node shall carry item
            if (qlen == (len << 1)) return position_t(in_node, qlen, false);

            br_ix = get_qpos(key, qlen);
            nod   = in_node;
        }

        // Create leaf
        // Note that the key SHAN'T be set, here.
        // The reason is that it may not (and probably won't) reside
        // at the same address after the item is inserted into the item list.
        qlen = len << 1;
        br_node = new node(m_items.end(), NULL, qlen, nod);
        nod->branches[br_ix].reset(br_node);
        return position_t(br_node, qlen, false);
    }

    /**
     *  \brief  Insert new item at \c nod position
     *
     *  Note that the function DOES NOT check if there's already
     *  an item set in the node.
     *
     *  \param  item  Item
     *  \param  nod   TRIE node
     */
    void insert_item(const T & item, node * nod) {
        m_items.push_back(item);
        nod->key  = key(m_items.back());
        nod->item = --m_items.end();
    }

    /**
     *  \brief  Report key search position
     *
     *  \param  key   Key
     *  \param  len   Key length
     *  \param  nod   Current node
     *  \param  qlen  Quad-bit position of key (mis)match
     *
     *  \return Node for the key specified
     */
    position_t search_position(
        const unsigned char * key,
        size_t                len,
        node *                nod,
        size_t                qlen)
    {
        // Hit is reported directly by trace
        return position_t(nod, qlen, false);
    }

    /** Iterator base */
    template <typename NodePtr>
    class iterator_base {
        friend class trie;

        public:

        /** Iterator dereference (tuple of {<key>, <key_size>, <value>}) */
        typedef std::tuple<const unsigned char *, size_t, T &> deref_t;

        protected:

        const trie & m_trie;  /**< Trie         */
        NodePtr      m_node;  /**< Current node */

        private:

        /** Move to the next valid node */
        void next() {
            for (size_t br_ix = 0; ;) {
                // Descend to depth
                for (;;) {
                    NodePtr nod = m_node->branches[br_ix].get();
                    if (NULL != nod) {
                        m_node = nod;
                        if (m_node->item != m_trie.m_items.end()) return;

                        br_ix = 0;
                        continue;
                    }

                    if (!(++br_ix < (1 << 4))) break;  // no more branches
                }

                // Ascend from depth
                do {
                    const unsigned char * key = m_node->key;
                    m_node = m_node->parent;
                    if (NULL == m_node) return;  // end

                    br_ix = get_qpos(key, m_node->qlen);

                } while (!(++br_ix < (1 << 4)));
            }
        }

        /**
         *  \brief  Constructor
         *
         *  The initial node may or may not bear an item.
         *  The constructor shall search to depth for a node that does
         *  if necessary.
         *
         *  \param  _trie  Trie
         *  \param  _node  Initial node
         */
        iterator_base(const trie & _trie, NodePtr _node):
            m_trie ( _trie ),
            m_node ( _node )
        {
            if (m_node->item == m_trie.m_items.end()) next();
        }

        /**
         *  \brief  End constructor
         *
         *  \param  _trie  Trie
         */
        iterator_base(const trie & _trie): m_trie(_trie), m_node(NULL) {}

        public:

        /** Dereference */
        inline deref_t operator * () const {
            return deref_t(m_node->key, m_node->qlen >> 1, *m_node->item);
        }

        /** Dereference */
        inline deref_t operator -> () const { return **this; }

        /** Pre-incrementation */
        inline iterator_base & operator ++ () {
            next();
            return *this;
        }

        /** Post-incrementation */
        inline iterator_base & operator ++ (int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        /** Comparison */
        inline bool operator == (const iterator_base & arg) const {
            return m_node == arg.m_node;
        }

        /** Comparison (negative) */
        inline bool operator != (const iterator_base & arg) const {
            return !(*this == arg);
        }

    };  // end of template class iterator_base

    /**
     *  \brief  Serialise (sub-)tree (implementation)
     *
     *  \param  out      Output stream
     *  \param  iostate  Original I/O manipulators state
     *  \param  nod      (Sub-)tree root node
     *  \param  indent   Indentation string
     */
    void serialise(
        std::ostream &      out,
        const std::ios &    iostate,
        const node *        nod,
        const std::string & indent)
    const {
        out << indent << "Node " << nod << " @"
            << std::dec << nod->qlen << ':' << std::endl
            << indent << "  Parent: " << nod->parent << std::endl
            << indent << "  Key: ";

        for (size_t i = 0; i < nod->qlen / 2; ++i)
            out << std::setw(2) << std::setfill('0') << std::hex
                << (unsigned int)nod->key[i];

        if (nod->qlen % 2)
            out << std::hex << (nod->key[nod->qlen / 2] >> 4);

        out << std::endl;

        // Serialise item
        if (nod->item != m_items.end()) {
            out.copyfmt(iostate);  // use original I/O manipulators

            out << indent << "  Item" << std::endl
                << indent << "    " << *nod->item << std::endl
                << indent << "  ItemEnd" << std::endl;
        }

        const size_t branches_cnt =
            sizeof(nod->branches) / sizeof(nod->branches[0]);

        // Serialise branches
        for (size_t i = 0; i < branches_cnt; ++i) {
            node * br_node = nod->branches[i].get();
            if (NULL == br_node) continue;

            out << indent << "  Branch " << std::hex << i << ':' << std::endl;
            serialise(out, iostate, br_node, indent + "    ");
            out << std::endl;
        }

        out << indent << "NodeEnd";
    }

    /**
     *  \brief  Serialise (sub-)tree
     *
     *  \param  out      Output stream
     *  \param  nod      (Sub-)tree root node
     *  \param  indent   Indentation string
     */
    void serialise(
        std::ostream &      out,
        const node *        nod,
        const std::string & indent = "")
    const {
        std::ios iostate(NULL);
        iostate.copyfmt(out);
        serialise(out, iostate, nod, indent);
        out.copyfmt(iostate);
    }

    /**
     *  \brief  Serialise (sub-)tree paths (implementation)
     *
     *  \param  out      Output stream
     *  \param  iostate  Original I/O manipulators state
     *  \param  nod      (Sub-)tree root node
     *  \param  qlen     Prefix quad-bit length
     *  \param  prefix   (Sub-)tree path prefix
     */
    void serialise_paths(
        std::ostream &      out,
        const std::ios &    iostate,
        const node *        nod,
        size_t              qlen,
        const std::string & prefix)
    const {
        std::stringstream prefix_ss; prefix_ss << prefix;

        // Serialise condensed part of path to the node
        if (qlen % 2)
            prefix_ss << std::hex << (nod->key[qlen++ / 2] & 0x0f);

        for (size_t i = qlen / 2; i < nod->qlen / 2; ++i)
            prefix_ss
                << std::setw(2) << std::setfill('0') << std::hex
                << (unsigned int)nod->key[i];

        if (nod->qlen % 2)
            prefix_ss << std::hex << (nod->key[nod->qlen / 2] >> 4);

        // Serialise node
        prefix_ss << '[';
        if (nod->item != m_items.end()) {
            prefix_ss.copyfmt(iostate);
            prefix_ss << *nod->item;
        }
        prefix_ss << ']';

        // Serialise branches
        const std::string branch_prefix(prefix_ss.str());

        const size_t branches_cnt =
            sizeof(nod->branches) / sizeof(nod->branches[0]);

        bool leaf = true;
        for (size_t i = 0; i < branches_cnt; ++i) {
            node * br_node = nod->branches[i].get();
            if (NULL == br_node) continue;

            leaf = false;
            serialise_paths(
                out, iostate, br_node, nod->qlen, branch_prefix);
        }

        // Leaf node, serialise path
        if (leaf) out << branch_prefix << std::endl;
    }

    /**
     *  \brief  Serialise (sub-)tree paths
     *
     *  \param  out      Output stream
     *  \param  nod      (Sub-)tree root node
     *  \param  qlen     Prefix quad-bit length
     *  \param  prefix   (Sub-)tree path prefix
     */
    void serialise_paths(
        std::ostream &      out,
        const node *        nod,
        size_t              qlen,
        const std::string & prefix)
    const {
        std::ios iostate(NULL);
        iostate.copyfmt(out);
        serialise_paths(out, iostate, nod, qlen, prefix);
        out.copyfmt(iostate);
    }

    public:

    /** Forward iterator */
    typedef iterator_base<node *> iterator;

    /** Const forward iterator */
    typedef iterator_base<const node *> const_iterator;

    /**
     *  \brief  Serialise tree
     *
     *  \param  out     Output stream
     *  \param  indent  Indentation string
     */
    inline void serialise(
        std::ostream      & out,
        const std::string & indent = "")
    const {
        serialise(out, &m_root, indent);
    }

    /**
     *  \brief  Serialise tree paths
     *
     *  \param  out     Output stream
     *  \param  indent  Indentation string
     */
    inline void serialise_paths(
        std::ostream      & out,
        const std::string & indent = "")
    const {
        serialise_paths(out, &m_root, 0, indent);
    }

    /** Key getter */
    inline const unsigned char * key(const T & inst) {
        return m_key_fn(inst);
    }

    /** Key length getter */
    inline size_t key_len(const T & inst) {
        return m_key_len_fn(inst);
    }

    /** Constructor (default key functors) */
    trie(): m_root(m_items.end(), NULL, 0, NULL) {}

    /**
     *  \brief  Constructor
     *
     *  \param  key_fn      Key functor
     *  \param  key_len_fn  Key length functor
     */
    trie(KeyFn key_fn, KeyLenFn key_len_fn):
        m_key_fn(key_fn), m_key_len_fn(key_len_fn),
        m_root(m_items.end(), NULL, 0, NULL)
    {}

    /** Begin iterator */
    inline iterator begin() { return iterator(*this, &m_root); }

    /** End iterator */
    inline iterator end() { return iterator(*this); }

    /** Begin const iterator */
    inline const_iterator begin() const {
        return const_iterator(*this, &m_root);
    }

    /** End const iterator */
    inline const_iterator end() const {
        return const_iterator(*this);
    }

    /**
     *  \brief  Insert item (unless already exists)
     *
     *  \param  item  Item
     *
     *  \return Item iterator
     */
    iterator insert(const T & item) {
        position_t pos = trace(&trie::insert_node, key(item), key_len(item));
        node *     nod = pos_node(pos);

        // Another item may already use the key
        if (!pos_match(pos)) insert_item(item, nod);

        return iterator(*this, nod);
    }

    /**
     *  \brief  Find item by key
     *
     *  \param  key  Item key
     *  \param  len  Item key length
     *
     *  \return Item iterator
     */
    const_iterator find(const unsigned char * key, size_t len) const {
        position_t pos = trace(&trie::search_position, key, len);

        // Key mismatch
        if (!pos_match(pos)) return end();

        return const_iterator(*this, pos_node(pos));
    }

    /**
     *  \brief  Find item (by its twin)
     *
     *  \param  item  Item
     *
     *  \return Item iterator
     */
    inline const_iterator find(const T & item) const {
        return find(key(item), key_len(item));
    }

    /**
     *  \brief  Transform position to iterator
     *
     *  \param  pos  Position
     *
     *  \return Iterator (end iterator if there's no item at position)
     */
    iterator pos2iterator(const position_t & pos) const {
        if (!pos_match(pos)) return end();

        return iterator(*this, pos_node(pos));
    }

    /**
     *  \brief  Lower bound
     *
     *  \param  key  Item key
     *  \param  len  Item key length
     *
     *  \return Lower bound position
     */
    inline position_t lower_bound(const unsigned char * key, size_t len) const {
        return trace(&trie::search_position, key, len);
    }

    /**
     *  \brief  Insert item at lower bound
     *
     *  \param  item  Item
     *
     *  \return Item iterator
     */
    iterator insert(const T & item, const position_t & pos) {
        node * nod  = pos_node(pos);
        size_t qlen = pos_qlen(pos);

        position_t item_pos = insert_node(key(item), key_len(item), nod, qlen);
        nod = pos_node(item_pos);
        insert_item(item, nod);

        return iterator(*this, nod);
    }

};  // end of template class trie


namespace impl {

/** Functions concatenation */
template <class Fn, class ... Fns>
class fn_concat {
    private:

    Fn                 m_fn;
    fn_concat<Fns ...> m_fns;

    public:

    template <typename ... Args>
    auto operator () (Args && ... args) ->
        decltype(m_fns.operator () (m_fn.operator () (args ...)))
    {
        return m_fns(m_fn(args ...));
    }

};  // end of template class fn_concat

/** Functions concatenation (recursion fixed point) */
template <class Fn>
class fn_concat<Fn> {
    private:

    Fn m_fn;

    public:

    template <typename ... Args>
    auto operator () (Args && ... args) ->
        decltype(m_fn.operator () (args ...))
    {
        return m_fn(args ...);
    }

};  // end of template class fn_concat


/** \c std::tuple item getter */
template <int I, class Tuple>
class get {
    public:

    typedef typename std::tuple_element<I, Tuple>::type result_t;

    const result_t & operator () (const Tuple & tuple) const {
        return std::get<I>(tuple);
    }

};  // end of template class get


/** \c std::string::data() caller */
class string_data {
    public:

    const unsigned char * operator () (const std::string & str) const {
        return (const unsigned char *)str.data();
    }

};  // end of class string_data

/** \c std::string.size() caller */
class string_size {
    public:

    inline size_t operator () (const std::string & str) const {
        return str.size();
    }

};  // end of class string_size

}  // end of namespace impl


/**
 *  \brief  String-keyed trie
 *
 *  \tparam  T  Value type
 */
template <typename T>
class string_trie: public trie<
    std::tuple<std::string, T>,
    impl::fn_concat<
        impl::get<0, std::tuple<std::string, T> >,
        impl::string_data>,
    impl::fn_concat<
        impl::get<0, std::tuple<std::string, T> >,
        impl::string_size> >
{};  // end of template class string_trie

}  // end of namespace container


// TODO: This should go to io:: namespace or somewhere...
/** Trie serialisation */
template <typename T, class KeyFn, class KeyLenFn>
std::ostream & operator << (
    std::ostream & out,
    const container::trie<T, KeyFn, KeyLenFn> & trie)
{
    trie.serialise(out);
    return out;
}

#endif  // end of #ifndef trie_hxx
