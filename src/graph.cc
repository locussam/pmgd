#include <stddef.h>
#include "graph.h"
#include "node.h"
#include "edge.h"
#include "allocator.h"
#include "iterator.h"
#include "os.h"

class Jarvis::Graph::GraphImpl {
    typedef FixedAllocator NodeTable;
    typedef FixedAllocator EdgeTable;

    static const size_t BASE_ADDRESS = 0x10000000000;
    static const size_t REGION_SIZE = 0x10000000000;
    static const unsigned INDEX_SIZE = 4096;
    static const unsigned NODE_SIZE = 64;
    static const unsigned EDGE_SIZE = 32;

    static constexpr char index_name[] = "index.jdb";

    static constexpr AllocatorInfo default_allocators[] = {
        { "nodes.jdb", BASE_ADDRESS + REGION_SIZE, REGION_SIZE, NODE_SIZE },
        { "edges.jdb", BASE_ADDRESS + 2*REGION_SIZE, REGION_SIZE, EDGE_SIZE },
    };

    struct GraphIndex {
        uint64_t version;

        // node_table, edge_table, property_chunks
        AllocatorInfo node_info;
        AllocatorInfo edge_info;

        // Transaction info
        // Lock table info
    };

    class GraphInit {
        bool _create;
        os::MapRegion _index_map;
        GraphIndex *_index;

    public:
        GraphInit(const char *name, int options);
        bool create() { return _create; }
        const AllocatorInfo &node_info() { return _index->node_info; }
        const AllocatorInfo &edge_info() { return _index->edge_info; }
    };

    // ** Order here is important: GraphInit MUST be first
    GraphInit _init;

    NodeTable _node_table;
    EdgeTable _edge_table;
    // Other Fixed ones
    // Variable allocator

    // Transactions
    // Lock manager

public:
    GraphImpl(const char *name, int options)
        : _init(name, options),
          _node_table(name, _init.node_info(), _init.create()),
          _edge_table(name, _init.edge_info(), _init.create())
        { }
    NodeTable &node_table() { return _node_table; }
    EdgeTable &edge_table() { return _edge_table; }
};

Jarvis::Graph::Graph(const char *name, int options)
    : _impl(new GraphImpl(name, options))
{
}

Jarvis::Graph::~Graph()
{
    delete _impl;
}

Jarvis::Node &Jarvis::Graph::add_node(StringID tag)
{
    Node *node = (Node *)_impl->node_table().alloc();
    node->init(tag);
    return *node;
}

Jarvis::Edge &Jarvis::Graph::add_edge(Node &src, Node &dest, StringID tag)
{
    Edge *edge = (Edge *)_impl->edge_table().alloc();
    edge->init(src, dest, tag);
    return *edge;
}


constexpr char Jarvis::Graph::GraphImpl::index_name[];
constexpr Jarvis::AllocatorInfo Jarvis::Graph::GraphImpl::default_allocators[];

Jarvis::Graph::GraphImpl::GraphInit::GraphInit(const char *name, int options)
    : _create(options & Create),
      _index_map(name, index_name, BASE_ADDRESS, INDEX_SIZE, _create, false),
      _index(reinterpret_cast<GraphIndex *>(BASE_ADDRESS))
{
    // _create was modified by _index_map constructor
    // depending on whether the file existed or not

    // Set up the info structure
    if (_create) {
        // Version info
        _index->version = 1;

        // TODO replace static indexing
        _index->node_info = default_allocators[0];
        _index->edge_info = default_allocators[1];

        // Other information
    }
}


namespace Jarvis {
    template <typename T>
    class Graph_Iterator : public T {
        const FixedAllocator &table;
        void _next();
        void _skip();

    protected:
        void *_cur;

    public:
        Graph_Iterator(const FixedAllocator &);
        operator bool() const { return _cur != NULL; }
        const typename T::Ref_type &operator*() const { return *(typename T::Ref_type *)_cur; }
        const typename T::Ref_type *operator->() const { return (typename T::Ref_type *)_cur; }
        typename T::Ref_type &operator*() { return *(typename T::Ref_type *)_cur; }
        typename T::Ref_type *operator->() { return (typename T::Ref_type *)_cur; }
        bool next();
    };

    class Graph_EdgeIterator : public Graph_Iterator<EdgeIteratorImpl> {
        friend class EdgeRef;
        Edge *get_edge() const { return (Edge *)_cur; }
        StringID get_tag() const { return get_edge()->get_tag(); }
        Node &get_source() const { return get_edge()->get_source(); }
        Node &get_destination() const { return get_edge()->get_destination(); }
    public:
        Graph_EdgeIterator(const FixedAllocator &a) : Graph_Iterator<EdgeIteratorImpl>(a) {}
    };
};

template <typename T>
Jarvis::Graph_Iterator<T>::Graph_Iterator(const FixedAllocator &n)
    : table(n)
{
    _cur = table.begin();
    _next();
}

template <typename T>
bool Jarvis::Graph_Iterator<T>::next()
{
    _skip();
    _next();
    return _cur != NULL;
}

template <typename T>
void Jarvis::Graph_Iterator<T>::_next()
{
    while (_cur < table.end() && table.is_free(_cur))
        _skip();

    if (_cur >= table.end())
        _cur = NULL;
}

template <typename T>
void Jarvis::Graph_Iterator<T>::_skip()
{
    _cur = table.next(_cur);
}

Jarvis::NodeIterator Jarvis::Graph::get_nodes()
{
    return NodeIterator(new Graph_Iterator<NodeIteratorImpl>(_impl->node_table()));
}

Jarvis::EdgeIterator Jarvis::Graph::get_edges()
{
    return EdgeIterator(new Graph_EdgeIterator(_impl->edge_table()));
}

Jarvis::NodeID Jarvis::Graph::get_id(const Node &node) const
{
    return _impl->node_table().get_id(&node);
}

Jarvis::EdgeID Jarvis::Graph::get_id(const Edge &edge) const
{
    return _impl->edge_table().get_id(&edge);
}
