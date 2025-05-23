/** \file
 * \brief Regression test for upward crossing minimization (currently only the heuristics)
 *
 * \author Markus Chimani
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

#include <ogdf/basic/Module.h>
#include <ogdf/upward/SubgraphUpwardPlanarizer.h>
#include <ogdf/upward/UpwardPlanRep.h>
#include <ogdf/upward/UpwardPlanarity.h>

#include <functional>
#include <set>
#include <string>

#include <graphs.h>

#include <testing.h>

namespace ogdf {
class Graph;
} // namespace ogdf

go_bandit([] {
	describe("SubgraphUpwardPlanarizer", [] {
		forEachGraphItWorks({GraphProperty::acyclic, GraphProperty::connected,
									GraphProperty::simple, GraphProperty::sparse},
				[](Graph& G, const std::string& graphName, const std::set<GraphProperty>& props) {
					// Init module
					UpwardPlanRep U;
					U.setOriginalGraph(G);
					SubgraphUpwardPlanarizer sup;
					Module::ReturnType s = sup.call(U, nullptr, nullptr);
					AssertThat(Module::isSolution(s), IsTrue());
					AssertThat(UpwardPlanarity::isUpwardPlanar(U), IsTrue());
				});
	});
});
