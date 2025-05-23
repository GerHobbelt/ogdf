/** \file
 * \brief Declaration of class SPQRTree
 *
 * \author Carsten Gutwenger
 *
 * \par License:
 * This file is part of the Open Graph Drawing Framework (OGDF).
 *
 * \par
 * Copyright (C)<br>
 * See README.md in the OGDF root directory for details.
 *
 * \par
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * Version 2 or 3 as published by the Free Software Foundation;
 * see the file LICENSE.txt included in the packaging of this file
 * for details.
 *
 * \par
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * \par
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see
 * http://www.gnu.org/copyleft/gpl.html
 */

#pragma once

#include <ogdf/basic/Graph.h>
#include <ogdf/basic/List.h>
#include <ogdf/basic/SList.h>
#include <ogdf/basic/basic.h>
#include <ogdf/decomposition/PertinentGraph.h>
#include <ogdf/decomposition/Skeleton.h>

namespace ogdf {

/**
 * \brief Linear-time implementation of static SPQR-trees.
 *
 * @ingroup decomp
 *
 * The class SPQRTree maintains the arrangement of the triconnected
 * components of a biconnected multi-graph \a G [Hopcroft, Tarjan 1973]
 * as a so-called SPQR tree \a T [Di Battista, Tamassia, 1996]. We
 * call \a G the original graph of \a T.
 *
 * Each node of the tree has an associated type (represented by
 * SPQRTree::NodeType), which is either SNode, PNode, or
 * RNode, and a skeleton (represented by the class Skeleton).
 * The skeletons of the nodes of \a T are in one-to-one
 * correspondence to the triconnected components of \a G, i.e.,
 * S-nodes correspond to polygons, P-nodes to bonds, and
 * R-nodes to triconnected graphs.
 *
 * In our representation of SPQR-trees, Q-nodes are omitted. Instead,
 * the skeleton S of a node \a v in \a T contains two types of edges:
 * real edges, which correspond to edges in \a G, and virtual edges, which
 * correspond to edges in \a T having \a v as an endpoint.
 * There is a special edge \a er in G at which \a T is rooted, i.e., the
 * root node of \a T is the node whose skeleton contains the real edge
 * corresponding to \a er.
 *
 * The reference edge of the skeleton of the root node is \a er, the
 * reference edge of the skeleton \a S of a non-root node \a v is the virtual
 * edge in \a S that corresponds to the tree edge (parent(\a v),\a v).
 */
class OGDF_EXPORT SPQRTree {
public:
	//! The type of a tree node in T.
	enum class NodeType { SNode, PNode, RNode };

	// destructor
	virtual ~SPQRTree() { }

	SPQRTree() {};
	SPQRTree(const SPQRTree& copy) = delete;
	SPQRTree(SPQRTree&& move) = delete;
	SPQRTree& operator=(const SPQRTree& copy) = delete;
	SPQRTree& operator=(SPQRTree&& move) = delete;

	//! \name Access operations
	//! @{

	//! Returns a reference to the original graph \a G.
	virtual const Graph& originalGraph() const = 0;

	//! Returns a reference to the tree \a T.
	virtual const Graph& tree() const = 0;

	//! Returns the edge of \a G at which \a T is rooted.
	virtual edge rootEdge() const = 0;

	//! Returns the root node of \a T.
	virtual node rootNode() const = 0;

	//! Returns the number of S-nodes in \a T.
	virtual int numberOfSNodes() const = 0;

	//! Returns the number of P-nodes in \a T.
	virtual int numberOfPNodes() const = 0;

	//! Returns the number of R-nodes in \a T.
	virtual int numberOfRNodes() const = 0;

	/**
	 * \brief Returns the type of node \p v.
	 * \pre \p v is a node in \a T
	 */
	virtual NodeType typeOf(node v) const = 0;

	//! Returns the list of all nodes with type \p t.
	virtual List<node> nodesOfType(NodeType t) const = 0;

	/**
	 * \brief Returns the skeleton of node \p v.
	 * \pre \p v is a node in \a T
	 */
	virtual Skeleton& skeleton(node v) const = 0;

	/**
	 * \brief Returns the skeleton that contains the real edge \p e.
	 * \pre \p e is an edge in \a G
	 */
	virtual const Skeleton& skeletonOfReal(edge e) const = 0;

	/**
	 * \brief Returns the skeleton edge that corresponds to the real edge \p e.
	 * \pre \p e is an edge in \a G
	 */
	virtual edge copyOfReal(edge e) const = 0;

	/**
	 * \brief Returns the pertinent graph of tree node \p v in \p Gp.
	 * \pre \p v is a node in \a T
	 */
	void pertinentGraph(node v, PertinentGraph& Gp) const {
		if (m_cpV == nullptr) {
			m_cpV = new NodeArray<node>(originalGraph(), nullptr);
		}
		NodeArray<node>& cpV = *m_cpV;

		Gp.init(v);
		cpRec(v, Gp);

		const Skeleton& S = skeleton(v);

		edge e = Gp.m_skRefEdge = S.referenceEdge();
		if (e != nullptr) {
			e = Gp.m_P.newEdge(cpV[S.original(e->source())], cpV[S.original(e->target())]);
		}
		Gp.m_vEdge = e;

		while (!m_cpVAdded.empty()) {
			cpV[m_cpVAdded.popFrontRet()] = nullptr;
		}
	}

	//! @}
	//! \name Update operations
	//! @{

	/**
	 * \brief Roots \a T at edge \p e and returns the new root node of \a T.
	 * \pre \p e is an edge in \a G
	 */
	virtual node rootTreeAt(edge e) = 0;

	/**
	 * \brief Roots \a T at node \p v and returns \p v.
	 * \pre \p v is a node in \a T
	 */
	virtual node rootTreeAt(node v) = 0;

	void directSkEdge(node vT, edge e, node src) {
		OGDF_ASSERT(e != nullptr);
		OGDF_ASSERT(src == e->source() || src == e->target());

		if (e->source() != src) {
			skeleton(vT).getGraph().reverseEdge(e);
		}
	}

	void replaceSkEdgeByPeak(node vT, edge e) {
		Graph& M = skeleton(vT).getGraph();
		M.reverseEdge(M.split(e));
	}

	// !@}

protected:
	/**
	 * \brief Recursively performs the task of adding edges (and nodes)
	 * to the pertinent graph \p Gp for each involved skeleton graph.
	 */
	virtual void cpRec(node v, PertinentGraph& Gp) const = 0;

	//! Add an edge to \p Gp corresponding to \p eOrig.
	edge cpAddEdge(edge eOrig, PertinentGraph& Gp) const {
		edge eP = Gp.m_P.newEdge(cpAddNode(eOrig->source(), Gp), cpAddNode(eOrig->target(), Gp));
		Gp.m_origE[eP] = eOrig;
		return eP;
	}

	//! Add a node to \p Gp corresponding to \p vOrig if required.
	node cpAddNode(node vOrig, PertinentGraph& Gp) const {
		node& vP = (*m_cpV)[vOrig];
		if (vP == nullptr) {
			m_cpVAdded.pushBack(vOrig);
			Gp.m_origV[vP = Gp.m_P.newNode()] = vOrig;
		}
		return vP;
	}

	// auxiliary members used for computing pertinent graphs
	mutable NodeArray<node>* m_cpV; //!< node in pertinent graph corresponding to an original node (auxiliary member)
	mutable SList<node> m_cpVAdded; //!< list of added nodes (auxiliary member)
};

}
