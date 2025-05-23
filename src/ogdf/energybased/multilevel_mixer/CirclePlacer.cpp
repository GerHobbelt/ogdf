/** \file
 * \brief Places nodes on a circle around the barycenter of its neighbors.
 *
 * \author Gereon Bartel
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

#include <ogdf/basic/Graph.h>
#include <ogdf/basic/GraphList.h>
#include <ogdf/basic/Math.h>
#include <ogdf/basic/basic.h>
#include <ogdf/basic/geometry.h>
#include <ogdf/energybased/multilevel_mixer/BarycenterPlacer.h>
#include <ogdf/energybased/multilevel_mixer/CirclePlacer.h>
#include <ogdf/energybased/multilevel_mixer/MultilevelGraph.h>

#include <cmath>
#include <map>

namespace ogdf {

CirclePlacer::CirclePlacer()
	: m_circleSize(0.0f), m_fixedRadius(false), m_nodeSelection(NodeSelection::New) { }

void CirclePlacer::setRadiusFixed(bool fixed) { m_fixedRadius = fixed; }

void CirclePlacer::setCircleSize(float sizeIncrease) { m_circleSize = sizeIncrease; }

void CirclePlacer::setNodeSelection(NodeSelection nodeSel) { m_nodeSelection = nodeSel; }

void CirclePlacer::placeOneLevel(MultilevelGraph& MLG) {
	DPoint center(0.0, 0.0);
	double radius = 0.0;

	std::map<node, bool> oldNodes;
	Graph& G = MLG.getGraph();
	double n = G.numberOfNodes();
	if (n > 0) {
		for (node v : G.nodes) {
			oldNodes[v] = true;
			center = center + DPoint(MLG.x(v), MLG.y(v));
		}
		center = DPoint(center.m_x / n, center.m_y / n);
		for (node v : G.nodes) {
			double r = sqrt(MLG.x(v) * MLG.x(v) + MLG.y(v) * MLG.y(v));
			if (r > radius) {
				radius = r;
			}
		}
		radius += m_circleSize;
	} else {
		radius = 0.0f + m_circleSize;
	}

	BarycenterPlacer BP;
	BP.placeOneLevel(MLG);

	for (node v : G.nodes) {
		if (!m_fixedRadius) {
			radius = (float)center.distance(DPoint(MLG.x(v), MLG.y(v))) + m_circleSize;
		}
		if (m_nodeSelection == NodeSelection::All
				|| (m_nodeSelection == NodeSelection::New && oldNodes[v])
				|| (m_nodeSelection == NodeSelection::Old && !oldNodes[v])) {
			float angle =
					(float)(atan2(MLG.x(v) - center.m_x, -MLG.y(v) + center.m_y) - 0.5 * Math::pi);
			MLG.x(v, cos(angle) * radius + ((m_randomOffset) ? (float)randomDouble(-1.0, 1.0) : 0.f));
			MLG.y(v, sin(angle) * radius + ((m_randomOffset) ? (float)randomDouble(-1.0, 1.0) : 0.f));
		}
	}
}

}
