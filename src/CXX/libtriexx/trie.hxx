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


/** TRIE key tracing modes (see \ref trie class documentation) */
enum {
    TRIE_KEY_TRACING_STRICT = 0,  /**< Strict TRIE key tracing */
    TRIE_KEY_TRACING_SLOBBY = 1,  /**< Slobby TRIE key tracing */
};  // end of enum


/**
 *  \brief  TRIE
 *
 *  \tparam  T           Item type
 *  \tparam  KeyFn       Key getter type
 *  \tparam  KeyLenFn    Key length getter type
 *  \tparam  KeyTracing  Key tracing mode (see below)
 *
 *  IMPLEMENTATION NOTES:
 *  Note that the \c KeyFn and \c KeyLenFn functors are mutable.
 *  That means that potentially, these functors may keep some variable
 *  state (not sure if that's necessary, but it seems acceptable and
 *  solves potential problem with generic functor templates like \c fn_concat,
 *  which would have to define both const and non-const operator \c () etc).
 *
 *  Also note that the TRIE implements optional "slobby" mode of key tracing.
 *  If enabled, key tail tracing (last step of the key path from penultimate
 *  note to leaf) is skipped and the leaf value is produced whether the key
 *  tail matches or not.
 *  This mode of operation is designed specifically for the case of using
 *  the TRIE structure in a hash "table" implementation (keys would be
 *  the hash strings).
 *  Using a suitable hashing function, one may assume that if (substantially
 *  long) prefix of the hash strings matches, the rest of it will likely
 *  match too.
 *  Unless perfect hashing is used (collisions are impossible), the item keys
 *  must be compared, revealing the mismatch eventually, anyway.
 *  Note that this mode is controlled by a template argument to the class;
 *  since the extra code is encapsulated in a conditional statement with
 *  the condition possible to evaluate at compile time, reasonable compilers
 *  will omit the code altogether if slobby mode isn't used.
 *  The default is strict key tracing.
 */
template <
    typename T,
    class KeyFn      = impl::identity<T>,
    class KeyLenFn   = impl::size_of<T>,
    int   KeyTracing = TRIE_KEY_TRACING_STRICT>
class trie {
    private:

    mutable KeyFn    m_key_fn;      /**< Key getter        */
    mutable KeyLenFn m_key_len_fn;  /**< Key length getter */

    typedef std::list<T> items_t;  /**< Item list */

    items_t m_items;  /**< Item list */

    /** TRIE node */
    struct node {
        typename items_t::iterator item;    /**< Item                     */
        const unsigned char *      key;     /**< Item key                 */
        size_t                     qlen;    /**< Key path quad-bit length */
        node *                     parent;  /**< Parent node              */

        typedef uint32_t br_attrs_t;  /**< Type of branch attributes */

        /** Branches */
        std::unique_ptr<node> branches[1 << 4];
        br_attrs_t br_attrs;  /**< Branch attributes */

        /**
         *  \brief  Branch attributes initialiser
         *
         *  Sets node's own branch index and 1st and last branch indices.
         *  If the 1st son's index is greater than the last, meaning
         *  that the node is a leaf (it has no children).
         *
         *  \param  _br_own   Node's own branch index
         *  \param  _br_1st   Node's 1st son branch index
         *  \param  _br_last  Node's last son branch index
         *
         *  \return Branch attributes (for constructor)
         */
        inline static br_attrs_t br_attrs_init(
            size_t _br_own,
            size_t _br_1st,
            size_t _br_last)
        {
            return
                ((br_attrs_t)_br_own  << 8) |
                ((br_attrs_t)_br_last << 4) |
                ((br_attrs_t)_br_1st      );
        }

        /**
         *  \brief  Constructor
         *
         *  With the default values for 1st and last branch indices,
         *  a leaf node is constructed.
         *
         *  \param  _item     Item
         *  \param  _key      Item key
         *  \param  _qlen     Key path quad-bit length
         *  \param  _parent   Parent node
         *  \param  _br_own   Node's own branch index
         *  \param  _br_1st   Node's 1st son branch index
         *  \param  _br_last  Node's last son branch index
         */
        node(
            typename items_t::iterator _item,
            const unsigned char *      _key,
            size_t                     _qlen,
            node *                     _parent,
            size_t                     _br_own,
            size_t                     _br_1st  = 1,
            size_t                     _br_last = 0)
        :
            item     ( _item   ),
            key      ( _key    ),
            qlen     ( _qlen   ),
            parent   ( _parent ),
            br_attrs ( br_attrs_init(_br_own, _br_1st, _br_last) )
        {}

        /** First branch index getter */
        inline size_t br_1st() const { return 0xf & (br_attrs); }

        /** First branch index setter */
        inline void br_1st(size_t ix) {
            br_attrs &= ~(br_attrs_t)0xf;
            br_attrs |= (br_attrs_t)ix;
        }

        /** Last branch index getter */
        inline size_t br_last() const { return 0xf & (br_attrs >> 4); }

        /** Last branch index setter */
        inline void br_last(size_t ix) {
            br_attrs &= ~(br_attrs_t)0xf0;
            br_attrs |= (br_attrs_t)ix << 4;
        }

        /**
         *  \brief  Set 1st and last branch indices at once
         *
         *  \param  ix_1st   1st branch index
         *  \param  ix_last  Last branch index
         */
        inline void br_set(size_t ix_1st, size_t ix_last) {
            br_attrs &= ~(br_attrs_t)0xff;
            br_attrs |= ((br_attrs_t)ix_last << 4) | (br_attrs_t)ix_1st;
        }

        /** Node branch index getter */
        inline size_t br_own() const { return 0xf & (br_attrs >> 8); }

        /** Node branch index setter */
        inline void br_own(size_t ix) {
            br_attrs &= ~(br_attrs_t)0xf00;
            br_attrs |= (br_attrs_t)ix << 8;
        }

        /** Node is leaf */
        inline bool is_leaf() const { return br_1st() > br_last(); }

        /** Node has exactly one child */
        inline bool has_only_son() const { return br_1st() == br_last(); }

    };  // end of struct node

    node m_root;  /**< Root node */

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
     *  \param  slob  Use slobby key tracing (if enabled)
     *
     *  \return Position of the (original) mismatch
     */
    position_t trace(
        trace_miss_t          miss,
        const unsigned char * key,
        size_t                len,
        bool                  slob = false)
    const {
        const node * nod = &m_root;
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

                if (TRIE_KEY_TRACING_SLOBBY == KeyTracing && slob &&
                    nod->is_leaf())
                {
                    return position_t(const_cast<node *>(nod), len << 1, true);
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
            node * in_node  = new node(
                m_items.end(), br_node->key, qlen, nod,
                br_ix, in_br_ix, in_br_ix);

            in_node->branches[in_br_ix] = std::move(nod->branches[br_ix]);
            in_node->branches[in_br_ix]->br_own(in_br_ix);
            nod->branches[br_ix].reset(in_node);
            br_node->parent = in_node;

            // Interim node shall carry item
            if (qlen == (len << 1)) return position_t(in_node, qlen, false);

            br_ix = get_qpos(key, qlen);
            nod   = in_node;
        }

        // Set 1st & last branch indices correctly (leaf has dummy indices)
        if (nod->is_leaf())
            nod->br_set(br_ix, br_ix);
        else {
            if (nod->br_1st()  > br_ix) nod->br_1st(br_ix);
            if (nod->br_last() < br_ix) nod->br_last(br_ix);
        }

        // Create new leaf
        // Note that the key SHAN'T be set, here.
        // The reason is that it may not (and probably won't) reside
        // at the same address after the item is inserted into the item list.
        qlen = len << 1;
        br_node = new node(m_items.end(), NULL, qlen, nod, br_ix);
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
    template <class Trie, typename Node>
    class iterator_base {
        public:

        typedef Trie trie_t;  /**< Base trie type      */
        typedef Node node_t;  /**< Base trie node type */

        protected:

        trie_t & m_trie;  /**< Trie         */
        node_t * m_node;  /**< Current node */

        private:

        /** Move to the next valid node */
        void next() {
            const auto items_end = m_trie.m_items.end();
            size_t br_ix = m_node->br_1st();

            for (;;) {
                // Descend to depth
                while (br_ix <= m_node->br_last()) {
                    node_t * nod = m_node->branches[br_ix].get();

                    if (NULL != nod) {
                        m_node = nod;
                        if (m_node->item != items_end) return;  // got next

                        // Interim node must have a child
                        br_ix = m_node->br_1st();
                        continue;
                    }

                    ++br_ix;  // next branch
                }

                // Ascend from depth
                do {
                    br_ix = m_node->br_own() + 1;

                    m_node = m_node->parent;
                    if (NULL == m_node) return;  // end

                } while (br_ix > m_node->br_last());
            }
        }

        protected:

        /**
         *  \brief  Constructor
         *
         *  The initial node may or may not bear an item.
         *  The constructor shall search to depth for a node that does
         *  if necessary.
         *
         *  \param  _trie  Trie
         *  \param  _node  Initial node (\c NULL means end iterator)
         */
        iterator_base(trie_t & _trie, node_t * _node):
            m_trie ( _trie ),
            m_node ( _node )
        {
            if (NULL != m_node && m_node->item == m_trie.m_items.end())
                next();
        }

        /** End iterator check */
        inline bool is_end() const { return NULL == m_node; }

        public:

        /** Pre-incrementation */
        inline iterator_base & operator ++ () {
            next();
            return *this;
        }

        /** Post-incrementation */
        inline iterator_base operator ++ (int) {
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

    public:

    /** Forward iterator (forward declaration) */
    class iterator;

    /** Const forward iterator */
    class const_iterator: public iterator_base<const trie, const node> {
        friend class trie;

        public:

        /** Iterator dereference (tuple of {<key>, <key_size>, <value>}) */
        typedef std::tuple<const unsigned char *, size_t, const T &> deref_t;

        private:

        /** Constructor (see \c iterator_base) */
        const_iterator(const trie & _trie, const node * _node = NULL):
            iterator_base<const trie, const node>(_trie, _node)
        {}

        /** Node getter */
        const node * get_node() const { return this->m_node; }

        public:

        /** Dereference */
        inline deref_t operator * () const {
            return deref_t(
                this->m_node->key,
                this->m_node->qlen >> 1,
                *(this->m_node->item));
        }

        /** Dereference */
        inline deref_t operator -> () const { return **this; }

        /**
         *  \brief  Non-const conversion
         *
         *  NOTE: Conversion to non-const iterator casts \c const from
         *  the \c trie reference and \c trie::node pointer.
         *  Unless the object is really accessible, using the resulting
         *  iterator will result in undefined behaviour.
         */
        operator iterator() const {
            return iterator(
                const_cast<trie &>(this->m_trie),
                const_cast<node *>(this->m_node));
        }

    };  // end of class const_iterator

    /** Forward iterator */
    class iterator: public iterator_base<trie, node> {
        friend class trie;

        public:

        /** Iterator dereference (tuple of {<key>, <key_size>, <value>}) */
        typedef std::tuple<const unsigned char *, size_t, T &> deref_t;

        private:

        /** Constructor (see \c iterator_base) */
        iterator(trie & _trie, node * _node = NULL):
            iterator_base<trie, node>(_trie, _node)
        {}

        /** Node getter */
        node * get_node() const { return this->m_node; }

        public:

        /** Dereference */
        inline deref_t operator * () const {
            return deref_t(
                this->m_node->key,
                this->m_node->qlen >> 1,
                *(this->m_node->item));
        }

        /** Dereference */
        inline deref_t operator -> () const { return **this; }

        /** Const conversion */
        operator const_iterator() const {
            return const_iterator(this->m_trie, this->m_node);
        }

    };  // end of class iterator

    private:

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
            << indent << "  Self     @" << nod->br_own() << std::endl
            << indent << "  1st  son @" << nod->br_1st() << std::endl
            << indent << "  Last son @" << nod->br_last() << std::endl
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

            static const char * br_ok = "  Branch ";
            static const char * br_ko = "  FAULTY BRANCH ";
            const char * br_ind =
                nod->br_1st() <= i && i <= nod->br_last()
                ? br_ok : br_ko;

            out << indent << br_ind << std::hex << i << ':' << std::endl;
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
    inline const unsigned char * key(const T & inst) const {
        return m_key_fn(inst);
    }

    /** Key length getter */
    inline size_t key_len(const T & inst) const {
        return m_key_len_fn(inst);
    }

    /** Constructor (default key functors) */
    trie(): m_root(m_items.end(), NULL, 0, NULL, 0) {}

    /**
     *  \brief  Constructor
     *
     *  \param  key_fn      Key functor
     *  \param  key_len_fn  Key length functor
     */
    trie(KeyFn key_fn, KeyLenFn key_len_fn):
        m_key_fn(key_fn), m_key_len_fn(key_len_fn),
        m_root(m_items.end(), NULL, 0, NULL, 0)
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
     *  NOTE: Uses slobby key tracing if enabled.
     *
     *  \param  key  Item key
     *  \param  len  Item key length
     *
     *  \return Item iterator
     */
    const_iterator find(const unsigned char * key, size_t len) const {
        position_t pos = trace(&trie::search_position, key, len, true);

        // Key mismatch
        if (!pos_match(pos)) return end();

        return const_iterator(*this, pos_node(pos));
    }

    /**
     *  \brief  Find item (by its twin)
     *
     *  NOTE: Uses slobby key tracing if enabled.
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
     *  If item with the key already exists, an exception is thrown.
     *
     *  \param  item  Item
     *
     *  \return Item iterator
     */
    iterator insert(const T & item, const position_t & pos) {
        if (pos_match(pos))
            throw std::logic_error(
                "libtrie++: insert to already occupied position");

        node * nod = pos_node(pos);
        const size_t qlen = pos_qlen(pos);

        if (qlen != nod->qlen)
            nod = pos_node(insert_node(key(item), key_len(item), nod, qlen));

        insert_item(item, nod);
        return iterator(*this, nod);
    }

    /**
     *  \brief  Erase item
     *
     *  Increments the iterator.
     *
     *  \param  iter  Item iterator
     */
    void erase(iterator & iter) {
        // Check the iterator
        if (iter.is_end())
            throw std::logic_error(
                "libtrie++: attempted erase at end iterator");

        const auto items_end = m_items.end();
        node * nod = iter.get_node();

        ++iter;  // iterator is incremented

        // Remove item from item list
        m_items.erase(nod->item);
        nod->item = items_end;

        // Empty leaf node shall be removed
        if (nod->is_leaf() && nod != &m_root) {
            const size_t br_ix  = nod->br_own();
            node *       parent = nod->parent;
            parent->branches[br_ix].reset(NULL);

            // Just removed parent's only son
            if (parent->has_only_son()) parent->br_set(1, 0);

            // There were more than 1 child
            else {
                // Removed the 1st son
                if (parent->br_1st() == br_ix) {
                    size_t ix = br_ix + 1;
                    for (; NULL == parent->branches[ix].get(); ++ix);
                    parent->br_1st(ix);
                }

                // Removed the last son
                else if (parent->br_last() == br_ix) {
                    size_t ix = br_ix - 1;
                    for (; NULL == parent->branches[ix].get(); --ix);
                    parent->br_last(ix);
                }
            }

            nod = parent;
        }

        // Interim node with only son shall be removed
        if (nod->has_only_son() && items_end == nod->item && nod != &m_root) {
            node * parent = nod->parent;
            size_t br_ix  = nod->br_own();
            auto & branch = parent->branches[br_ix];
            branch = std::move(nod->branches[nod->br_1st()]);
            branch->parent = parent;
            branch->br_own(br_ix);
            nod = parent;
        }

        // Interim node without value uses key of its descendant (any will do)
        if (!nod->is_leaf() && items_end == nod->item && nod != &m_root) {
            const unsigned char * key = nod->branches[nod->br_1st()]->key;
            while (items_end == nod->item && nod != &m_root) {
                nod->key = key;
                nod = nod->parent;
            }
        }
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
 *  \tparam  T           Value type
 *  \tparam  KeyTracing  Key tracing mode
 */
template <typename T, int KeyTracing = TRIE_KEY_TRACING_STRICT>
class string_trie: public trie<
    std::tuple<std::string, T>,
    impl::fn_concat<
        impl::get<0, std::tuple<std::string, T> >,
        impl::string_data>,
    impl::fn_concat<
        impl::get<0, std::tuple<std::string, T> >,
        impl::string_size>,
    KeyTracing>
{};  // end of template class string_trie

}  // end of namespace container


// TODO: This should go to io:: namespace or somewhere...
/** Trie serialisation */
template <typename T, class KeyFn, class KeyLenFn, int KeyTracing>
std::ostream & operator << (
    std::ostream & out,
    const container::trie<T, KeyFn, KeyLenFn, KeyTracing> & trie)
{
    trie.serialise(out);
    return out;
}

#endif  // end of #ifndef trie_hxx
