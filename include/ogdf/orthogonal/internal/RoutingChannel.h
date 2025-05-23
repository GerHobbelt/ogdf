/** \file
 * \brief Declaration of class RoutingChannel which maintains
 *        required size of routing channels and separation, cOverhang.
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
#include <ogdf/basic/GraphList.h>
#include <ogdf/basic/basic.h>
#include <ogdf/orthogonal/OrthoRep.h>

#include <algorithm>

namespace ogdf {

//! Maintains input sizes for constructive compaction (size
//! of routing channels, separation, cOverhang)
template<class ATYPE>
class RoutingChannel {
public:
	// constructor
	RoutingChannel(const Graph& G, ATYPE sep, double cOver)
		: m_channel(G), m_separation(sep), m_cOverhang(cOver) { }

	// size of routing channel of side dir of node v
	const ATYPE& operator()(node v, OrthoDir dir) const {
		return m_channel[v].rc[static_cast<int>(dir)];
	}

	ATYPE& operator()(node v, OrthoDir dir) { return m_channel[v].rc[static_cast<int>(dir)]; }

	// returns separation (minimum distance between vertices/edges)
	ATYPE separation() const { return m_separation; }

	// returns cOverhang (such that overhang = separation * cOverhang)
	double cOverhang() const { return m_cOverhang; }

	// returns overhang (distance between vertex corners and edges)
	ATYPE overhang() const { return ATYPE(m_cOverhang * m_separation); }

	void computeRoutingChannels(const OrthoRep& OR, bool align = false) {
		const Graph& G = OR;

		for (node v : G.nodes) {
			const OrthoRep::VertexInfoUML* pInfo = OR.cageInfo(v);

			if (pInfo) {
				const OrthoRep::SideInfoUML& sNorth =
						pInfo->m_side[static_cast<int>(OrthoDir::North)];
				const OrthoRep::SideInfoUML& sSouth =
						pInfo->m_side[static_cast<int>(OrthoDir::South)];
				const OrthoRep::SideInfoUML& sWest = pInfo->m_side[static_cast<int>(OrthoDir::West)];
				const OrthoRep::SideInfoUML& sEast = pInfo->m_side[static_cast<int>(OrthoDir::East)];

				(*this)(v, OrthoDir::North) = computeRoutingChannel(sNorth, sSouth, align);
				(*this)(v, OrthoDir::South) = computeRoutingChannel(sSouth, sNorth, align);
				(*this)(v, OrthoDir::West) = computeRoutingChannel(sWest, sEast, align);
				(*this)(v, OrthoDir::East) = computeRoutingChannel(sEast, sWest, align);
			}
		}
	}

private:
	// computes required size of routing channel at side si with opposite side siOpp
	int computeRoutingChannel(const OrthoRep::SideInfoUML& si, const OrthoRep::SideInfoUML& siOpp,
			bool align = false) {
		if (si.m_adjGen == nullptr) {
			int k = si.m_nAttached[0];
			if (k == 0 || ((k == 1 && siOpp.totalAttached() == 0) && !align)) {
				return 0;
			} else {
				return (k + 1) * m_separation;
			}

		} else {
			int m = max(si.m_nAttached[0], si.m_nAttached[1]);
			if (m == 0) {
				return 0;
			} else {
				return (m + 1) * m_separation;
			}
		}
	}

	struct vInfo {
		ATYPE rc[4];

		vInfo() { rc[0] = rc[1] = rc[2] = rc[3] = ATYPE(); }
	};

	NodeArray<vInfo> m_channel;
	ATYPE m_separation;
	double m_cOverhang;
};

}
