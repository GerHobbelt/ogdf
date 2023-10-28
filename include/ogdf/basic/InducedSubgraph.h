#pragma once


#include <ogdf/basic/GraphObserver.h>

namespace ogdf {

namespace internal {
template<class it>
typename std::iterator_traits<it>::difference_type guess_dist(it begin, it end,
		std::input_iterator_tag) {
	return 0;
}

template<class it>
typename std::iterator_traits<it>::difference_type guess_dist(it begin, it end,
		std::random_access_iterator_tag) {
	return end - begin;
}
}

#ifdef OGDF_HAS_CONCEPTS
template<std::forward_iterator BaseIterator>
#else
template<typename BaseIterator>
#endif
struct filtered_iterator {
	using filter_type = std::function<bool(const typename BaseIterator::value_type&)>;
	using value_type = typename BaseIterator::value_type;
	using difference_type = std::ptrdiff_t;

	filtered_iterator() : _cur(), _end(), _filter() {};

	filtered_iterator(filter_type filter, BaseIterator base, BaseIterator end = {})
		: _cur(base), _end(end), _filter(filter) {
		while (_cur != _end && !_filter(*_cur)) {
			++_cur;
		}
	}

	filtered_iterator begin() const { return *this; }

	filtered_iterator end() const { return {_filter, _end, _end}; }

	typename BaseIterator::value_type operator*() const { return *_cur; }

	filtered_iterator& operator++() {
		do {
			++_cur;
		} while (_cur != _end && !_filter(*_cur));
		return *this;
	}

	filtered_iterator operator++(int) {
		filtered_iterator copy = *this;
		++*this;
		return copy;
	}

	bool operator==(const filtered_iterator& rhs) const {
		return _cur == rhs._cur && _end == rhs._end
				&& _filter.target_type() == rhs._filter.target_type();
	}

	bool operator!=(const filtered_iterator& rhs) const { return !(rhs == *this); }

private:
	BaseIterator _cur;
	BaseIterator _end;
	filter_type _filter;
};

OGDF_CHECK_CONCEPT(OGDF_NODE_ITER<filtered_iterator<Graph::node_iterator>>);

template<typename BaseIterator>
filtered_iterator<BaseIterator> make_filtered_iterator(
		typename filtered_iterator<BaseIterator>::filter_type filter, BaseIterator base,
		BaseIterator end = {}) {
	return {filter, base, end};
}

template<OGDF_NODE_ITER NI, OGDF_EDGE_ITER EI, bool copyEmbedding, bool copyIDs, bool notifyObservers>
std::pair<int, int> Graph::insert(const NI& nodesBegin, const NI& nodesEnd, const EI& edgesBegin,
		const EI& edgesEnd, NodeArray<node>& nodeMap, EdgeArray<edge>& edgeMap) {
	OGDF_ASSERT(nodeMap.valid());
	OGDF_ASSERT(edgeMap.valid());
	OGDF_ASSERT(nodeMap.graphOf() == edgeMap.graphOf());
	int newNodes = 0, newEdges = 0;
	void* cbData = preInsert(copyEmbedding, copyIDs, notifyObservers, nodeMap, edgeMap, &newNodes,
			&newEdges);
	if (nodesBegin == nodesEnd) {
		postInsert(cbData, newNodes, newEdges);
		return {newNodes, newEdges};
	}

	int guessedNodes = internal::guess_dist(nodesBegin, nodesEnd,
			typename std::iterator_traits<NI>::iterator_category());
	if (guessedNodes > 0) {
		m_regNodeArrays.reserveSpace(guessedNodes);
	}

	for (auto it = nodesBegin; it != nodesEnd; ++it) {
		node vG = *it;
		if (copyIDs) {
			m_nodeIdCount = max(m_nodeIdCount, vG->index() + 1);
		}
		node v = nodeMap[vG] = pureNewNode(copyIDs ? vG->index() : m_nodeIdCount++);
		newNodes++;
		if (notifyObservers) {
			m_regNodeArrays.keyAdded(v);
			nodeInserted(cbData, vG, v);
			for (GraphObserver* obs : getObservers()) {
				obs->nodeAdded(v);
			}
		}
	}

	if (edgesBegin == edgesEnd) {
		postInsert(cbData, newNodes, newEdges);
		return {newNodes, newEdges};
	}

	if (!copyEmbedding) {
		int guessedEdges = internal::guess_dist(edgesBegin, edgesEnd,
				typename std::iterator_traits<EI>::iterator_category());
		if (guessedEdges > 0) {
			m_regEdgeArrays.reserveSpace(guessedEdges);
			m_regAdjArrays.reserveSpace(guessedEdges); // registry adds factor 2 in calculateArraySize
		}
	}

	for (auto it = edgesBegin; it != edgesEnd; ++it) {
		edge eG = *it;
		node src = nodeMap[eG->source()];
		node tgt = nodeMap[eG->target()];
		if (src == nullptr || tgt == nullptr) {
			continue;
		}
		if (copyIDs) {
			m_edgeIdCount = max(m_edgeIdCount, eG->index() + 1);
		}
		edge e = edgeMap[eG] = pureNewEdge(src, tgt, copyIDs ? eG->index() : m_edgeIdCount++);
		newEdges++;
		if (!copyEmbedding) {
			src->adjEntries.pushBack(e->m_adjSrc);
			tgt->adjEntries.pushBack(e->m_adjTgt);
			if (notifyObservers) {
				m_regEdgeArrays.keyAdded(e);
				m_regAdjArrays.keyAdded(e->adjSource());
				edgeInserted(cbData, eG, e);
				for (GraphObserver* obs : getObservers()) {
					obs->edgeAdded(e);
				}
			}
		}
	}

	if (!copyEmbedding) {
#ifdef OGDF_HEAVY_DEBUG
		consistencyCheck();
#endif
		postInsert(cbData, newNodes, newEdges);
		return {newNodes, newEdges};
	}

	m_regEdgeArrays.reserveSpace(newEdges);
	m_regAdjArrays.reserveSpace(newEdges); // registry adds factor 2 in calculateArraySize

	for (auto it = nodesBegin; it != nodesEnd; ++it) {
		node vG = *it;
		node v = nodeMap[vG];
		for (adjEntry adjG : vG->adjEntries) {
			edge eG = adjG->m_edge;
			edge e = edgeMap[eG];
			// edgeMap[eG] might be an old value, so check whether the edge was overwritten this round
			if (nodeMap[eG->source()] == nullptr || nodeMap[eG->target()] == nullptr) {
				continue;
			}
			OGDF_ASSERT(e != nullptr);
			adjEntry adj = adjG->isSource() ? e->adjSource() : e->adjTarget();
			v->adjEntries.pushBack(adj);
		}
	}

	// notify observers of added edges after adjEntries are initialized
	if (notifyObservers) {
		for (auto it = edgesBegin; it != edgesEnd; ++it) {
			edge eG = *it;
			edge e = edgeMap[eG];
			if (nodeMap[eG->source()] == nullptr || nodeMap[eG->target()] == nullptr) {
				continue;
			}
			OGDF_ASSERT(e != nullptr);
			m_regEdgeArrays.keyAdded(e);
			m_regAdjArrays.keyAdded(e->adjSource());
			edgeInserted(cbData, eG, e);
			for (GraphObserver* obs : getObservers()) {
				obs->edgeAdded(e);
			}
		}
	}

#ifdef OGDF_HEAVY_DEBUG
	consistencyCheck();
#endif

	postInsert(cbData, newNodes, newEdges);
	return {newNodes, newEdges};
}

template<OGDF_NODE_ITER NI, OGDF_EDGE_FILTER EF, bool copyEmbedding, bool copyIDs, bool notifyObservers>
std::pair<int, int> Graph::insert(const NI& nodesBegin, const NI& nodesEnd, const EF& edgeFilter,
		NodeArray<node>& nodeMap, EdgeArray<edge>& edgeMap) {
	OGDF_ASSERT(nodeMap.valid());
	OGDF_ASSERT(edgeMap.valid());
	OGDF_ASSERT(nodeMap.graphOf() == edgeMap.graphOf());
	int newNodes = 0, newEdges = 0;
	void* cbData = preInsert(copyEmbedding, copyIDs, notifyObservers, nodeMap, edgeMap, &newNodes,
			&newEdges);
	if (nodesBegin == nodesEnd) {
		postInsert(cbData, newNodes, newEdges);
		return {newNodes, newEdges};
	}

	int guessedNodes = internal::guess_dist(nodesBegin, nodesEnd,
			typename std::iterator_traits<NI>::iterator_category());
	if (guessedNodes > 0 && notifyObservers) {
		m_regNodeArrays.reserveSpace(guessedNodes);
	}

	for (auto it = nodesBegin; it != nodesEnd; ++it) {
		node vG = *it;
		if (copyIDs) {
			m_nodeIdCount = max(m_nodeIdCount, vG->index() + 1);
		}
		node v = nodeMap[vG] = pureNewNode(copyIDs ? vG->index() : m_nodeIdCount++);
		newNodes++;
		if (notifyObservers) {
			m_regNodeArrays.keyAdded(v);
			nodeInserted(cbData, vG, v);
			for (GraphObserver* obs : getObservers()) {
				obs->nodeAdded(v);
			}
		}

		for (adjEntry adjG : vG->adjEntries) {
			edge eG = adjG->m_edge;
			if (!edgeFilter(eG)) {
				continue;
			}
			edge e = edgeMap[eG];
			if (e == nullptr) {
				node twin = nodeMap[adjG->twinNode()];
				if (twin == nullptr) {
					continue;
				}
				if (copyIDs) {
					m_edgeIdCount = max(m_edgeIdCount, eG->index() + 1);
				}
				if (adjG->isSource()) {
					e = edgeMap[eG] = pureNewEdge(v, twin, copyIDs ? eG->index() : m_edgeIdCount++);
					v->adjEntries.pushBack(e->m_adjSrc);
				} else {
					e = edgeMap[eG] = pureNewEdge(twin, v, copyIDs ? eG->index() : m_edgeIdCount++);
					v->adjEntries.pushBack(e->m_adjTgt);
				}
				newEdges++;
			} else {
				adjEntry adj = adjG->isSource() ? e->adjSource() : e->adjTarget();
				v->adjEntries.pushBack(adj);
				// at this point, other edges might still be incomplete, so we cannot call observers
			}
		}
	}

	// notify observers of added edges after all adjEntries are initialized
	if (notifyObservers) {
		m_regEdgeArrays.reserveSpace(newEdges);
		m_regAdjArrays.reserveSpace(newEdges); // registry adds factor 2 in calculateArraySize

		for (auto it = nodesBegin; it != nodesEnd; ++it) {
			node vG = *it;
			for (adjEntry adjG : vG->adjEntries) {
				edge eG = adjG->m_edge;
				edge e = edgeMap[eG];
				if (e == nullptr) {
					continue;
				}
				m_regEdgeArrays.keyAdded(e);
				m_regAdjArrays.keyAdded(e->adjSource());
				edgeInserted(cbData, eG, e);
				for (GraphObserver* obs : getObservers()) {
					obs->edgeAdded(e);
				}
			}
		}
	}

#ifdef OGDF_HEAVY_DEBUG
	consistencyCheck();
#endif

	postInsert(cbData, newNodes, newEdges);
	return {newNodes, newEdges};
}

template<OGDF_NODE_FILTER NF, OGDF_EDGE_FILTER EF, bool copyEmbedding, bool copyIDs, bool notifyObservers>
std::pair<int, int> Graph::insert(const Graph& G, const NF& nodeFilter, const EF& edgeFilter,
		NodeArray<node>& nodeMap, EdgeArray<edge>& edgeMap) {
	if (!nodeMap.registeredAt()) {
		nodeMap.init(G);
	}
	OGDF_ASSERT(nodeMap.registeredAt()->graphOf() == &G);
	if (!edgeMap.registeredAt()) {
		edgeMap.init(G);
	}
	OGDF_ASSERT(edgeMap.registeredAt()->graphOf() == &G);
	filtered_iterator<node_iterator> nodes_it {nodeFilter, G.nodes.begin(), G.nodes.end()};
	return insert<filtered_iterator<node_iterator>, EF, copyEmbedding, copyIDs, notifyObservers>(G,
			nodes_it, nodes_it.end(), edgeFilter, nodeMap, edgeMap);
}


}
