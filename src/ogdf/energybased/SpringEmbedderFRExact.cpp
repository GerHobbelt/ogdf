/** \file
 * \brief Implementation of ogdf::SpringEmbedderFRExact.
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


#include <ogdf/basic/Array.h>
#include <ogdf/basic/Graph.h>
#include <ogdf/basic/GraphAttributes.h>
#include <ogdf/basic/GraphList.h>
#include <ogdf/basic/LayoutStandards.h>
#include <ogdf/basic/Math.h>
#include <ogdf/basic/SList.h>
#include <ogdf/basic/System.h>
#include <ogdf/basic/basic.h>
#include <ogdf/basic/geometry.h>
#include <ogdf/basic/simple_graph_alg.h>
#include <ogdf/energybased/SpringEmbedderFRExact.h>
#include <ogdf/packing/TileToRowsCCPacker.h>

#include <cmath>


#ifdef _OPENMP
#	include <omp.h>
#endif

#include <ogdf/basic/internal/intrinsics.h>

#include <algorithm>

namespace ogdf {

SpringEmbedderFRExact::ArrayGraph::ArrayGraph(GraphAttributes& ga)
	: m_ga(&ga), m_mapNode(ga.constGraph()) {
	const Graph& G = ga.constGraph();
	m_numNodes = m_numEdges = 0;

	m_orig = nullptr;
	m_src = m_tgt = nullptr;
	m_x = m_y = nullptr;
	m_nodeWeight = nullptr;
	m_useNodeWeight = false;

	// compute connected components of G
	NodeArray<int> component(G);
	m_numCC = connectedComponents(G, component);

	m_nodesInCC.init(m_numCC);

	for (node v : G.nodes) {
		m_nodesInCC[component[v]].pushBack(v);
	}
}

SpringEmbedderFRExact::ArrayGraph::~ArrayGraph() {
	System::alignedMemoryFree(m_orig);
	System::alignedMemoryFree(m_src);
	System::alignedMemoryFree(m_tgt);
	System::alignedMemoryFree(m_x);
	System::alignedMemoryFree(m_y);
	System::alignedMemoryFree(m_nodeWeight);
}

void SpringEmbedderFRExact::ArrayGraph::initCC(int i) {
	System::alignedMemoryFree(m_orig);
	System::alignedMemoryFree(m_src);
	System::alignedMemoryFree(m_tgt);
	System::alignedMemoryFree(m_x);
	System::alignedMemoryFree(m_y);
	System::alignedMemoryFree(m_nodeWeight);

	m_numNodes = m_nodesInCC[i].size();
	m_numEdges = 0;

	m_orig = (node*)System::alignedMemoryAlloc16(m_numNodes * sizeof(node));
	m_x = (double*)System::alignedMemoryAlloc16(m_numNodes * sizeof(double));
	m_y = (double*)System::alignedMemoryAlloc16(m_numNodes * sizeof(double));
	m_nodeWeight = (double*)System::alignedMemoryAlloc16(m_numNodes * sizeof(double));

	int j = 0;
	for (node v : m_nodesInCC[i]) {
		m_orig[j] = v;
		m_mapNode[v] = j;

		m_x[j] = m_ga->x(v);
		m_y[j] = m_ga->y(v);

		if (m_useNodeWeight) {
			m_nodeWeight[j] = m_ga->has(GraphAttributes::nodeWeight) ? m_ga->weight(v) : 1.0;
		} else {
			m_nodeWeight[j] = 1.0;
		}
		for (adjEntry adj : v->adjEntries) {
			if (v->index() < adj->twinNode()->index()) {
				++m_numEdges;
			}
		}
		++j;
	}

	m_src = (int*)System::alignedMemoryAlloc16(m_numEdges * sizeof(int));
	m_tgt = (int*)System::alignedMemoryAlloc16(m_numEdges * sizeof(int));

	j = 0;
	int srcId = 0;
	for (node v : m_nodesInCC[i]) {
		for (adjEntry adj : v->adjEntries) {
			node w = adj->twinNode();
			if (v->index() < w->index()) {
				m_src[j] = srcId;
				m_tgt[j] = m_mapNode[w];
				++j;
			}
		}
		++srcId;
	}
}

SpringEmbedderFRExact::SpringEmbedderFRExact() {
	// default parameters
	m_iterations = 1000;
	m_noise = true;
	m_coolingFunction = CoolingFunction::Factor;

	m_coolFactor_x = 0.9;
	m_coolFactor_y = 0.9;

	double def_nw = LayoutStandards::defaultNodeWidth();
	double def_nh = LayoutStandards::defaultNodeHeight();
	m_idealEdgeLength =
			LayoutStandards::defaultNodeSeparation() + sqrt(def_nw * def_nw + def_nh * def_nh);

	m_minDistCC = LayoutStandards::defaultCCSeparation();
	m_pageRatio = 1.0;
	m_useNodeWeight = false;
	m_checkConvergence = true;
	m_convTolerance = 0.01; //fraction of ideal edge length below which convergence is achieved
}

void SpringEmbedderFRExact::call(GraphAttributes& AG) {
	const Graph& G = AG.constGraph();
	if (G.empty()) {
		return;
	}

	// all edges straight-line
	AG.clearAllBends();

	ArrayGraph component(AG);
	component.m_useNodeWeight = m_useNodeWeight;

	EdgeArray<edge> auxCopy(G);
	Array<DPoint> boundingBox(component.numberOfCCs());

	int i;
	const bool haveSSE3 = System::cpuSupports(CPUFeature::SSE3);
	for (i = 0; i < component.numberOfCCs(); ++i) {
		component.initCC(i);

		if (component.numberOfNodes() >= 2) {
			initialize(component);

			if (haveSSE3) {
				mainStep_sse3(component);
			} else {
				mainStep(component);
			}
		}

		double minX, maxX, minY, maxY;
		minX = maxX = component.m_x[0];
		minY = maxY = component.m_y[0];

		for (int vCopy = 0; vCopy < component.numberOfNodes(); ++vCopy) {
			node v = component.original(vCopy);
			AG.x(v) = component.m_x[vCopy];
			AG.y(v) = component.m_y[vCopy];

			if (AG.x(v) - AG.width(v) / 2 < minX) {
				minX = AG.x(v) - AG.width(v) / 2;
			}
			if (AG.x(v) + AG.width(v) / 2 > maxX) {
				maxX = AG.x(v) + AG.width(v) / 2;
			}
			if (AG.y(v) - AG.height(v) / 2 < minY) {
				minY = AG.y(v) - AG.height(v) / 2;
			}
			if (AG.y(v) + AG.height(v) / 2 > maxY) {
				maxY = AG.y(v) + AG.height(v) / 2;
			}
		}

		minX -= m_minDistCC;
		minY -= m_minDistCC;

		for (int vCopy = 0; vCopy < component.numberOfNodes(); ++vCopy) {
			node v = component.original(vCopy);
			AG.x(v) -= minX;
			AG.y(v) -= minY;
		}

		boundingBox[i] = DPoint(maxX - minX, maxY - minY);
	}

	Array<DPoint> offset(component.numberOfCCs());
	TileToRowsCCPacker packer;
	packer.call(boundingBox, offset, m_pageRatio);

	// The arrangement is given by offset to the origin of the coordinate
	// system. We still have to shift each node and edge by the offset
	// of its connected component.

	for (i = 0; i < component.numberOfCCs(); ++i) {
		const SList<node>& nodes = component.nodesInCC(i);

		const double dx = offset[i].m_x;
		const double dy = offset[i].m_y;

		// iterate over all nodes in ith CC
		for (node v : nodes) {
			AG.x(v) += dx;
			AG.y(v) += dy;
		}
	}
}

void SpringEmbedderFRExact::initialize(ArrayGraph& component) {
	// compute bounding box of current layout
	double xmin, xmax, ymin, ymax;
	xmin = xmax = component.m_x[0];
	ymin = ymax = component.m_y[0];

	for (int v = 0; v < component.numberOfNodes(); ++v) {
		Math::updateMin(xmin, component.m_x[v]);
		Math::updateMax(xmax, component.m_x[v]);
		Math::updateMin(ymin, component.m_y[v]);
		Math::updateMax(ymax, component.m_y[v]);
	}

	double w = xmax - xmin + m_idealEdgeLength;
	double h = ymax - ymin + m_idealEdgeLength;

	// scale such that the area is about n*k^2  (k = ideal edge length)
	double ratio = h / w;
	double W = sqrt(component.numberOfNodes() / ratio) * m_idealEdgeLength;
	double H = ratio * W;

	double fx = W / w;
	double fy = H / h;

	// check: was outcommented in paper version
	for (int v = 0; v < component.numberOfNodes(); ++v) {
		component.m_x[v] = (component.m_x[v] - xmin) * fx;
		component.m_y[v] = (component.m_y[v] - ymin) * fy;
	}

	// 1/20 of all nodes in row
	//m_txNull = m_tyNull = m_tx = m_ty = 0.05 * G.numberOfNodes() * m_idealEdgeLength;
	//m_txNull = 3*m_idealEdgeLength;
	//m_tyNull = 3*m_idealEdgeLength;
	m_txNull = W / 8.0;
	m_tyNull = H / 8.0;
}

void SpringEmbedderFRExact::cool(double& tx, double& ty, int& cF) {
	switch (m_coolingFunction) {
	case CoolingFunction::Factor:
		tx *= m_coolFactor_x;
		ty *= m_coolFactor_y;
		break;

	case CoolingFunction::Logarithmic:
		double cFLog = mylog2(cF);
		if (cFLog != 0) {
			tx = m_txNull / cFLog;
			ty = m_tyNull / cFLog;
		}
		cF++;
		break;
	}
}

void SpringEmbedderFRExact::mainStep(ArrayGraph& C) {
	const int n = C.numberOfNodes();
	const double k = m_idealEdgeLength;
	const double kSquare = k * k;
	const double c_rep = 0.052 * kSquare; // factor for repulsive forces as suggested by Walshaw = 0.2

	const double minDist = 10e-6; //100*DBL_EPSILON;
	const double minDistSquare = minDist * minDist;

	double* disp_x = (double*)System::alignedMemoryAlloc16(n * sizeof(double));
	double* disp_y = (double*)System::alignedMemoryAlloc16(n * sizeof(double));

	double tx = m_txNull;
	double ty = m_tyNull;
	int cF = 1;

	bool converged = (m_iterations == 0);
	int itCount = 1;

	// Loop until either maximum number of iterations reached or
	// movement falls under convergence threshold
	while (!converged) {
		if (m_checkConvergence) {
			converged = true;
		}
		// repulsive forces

#pragma omp parallel for
		for (int v = 0; v < n; ++v) {
			disp_x[v] = disp_y[v] = 0;

			for (int u = 0; u < n; ++u) {
				if (u == v) {
					continue;
				}

				double delta_x = C.m_x[v] - C.m_x[u];
				double delta_y = C.m_y[v] - C.m_y[u];

				double distSquare = max(minDistSquare, delta_x * delta_x + delta_y * delta_y);

				double t = C.m_nodeWeight[u] / distSquare;
				disp_x[v] += delta_x * t;
				disp_y[v] += delta_y * t;
			}

			disp_x[v] *= c_rep;
			disp_y[v] *= c_rep;
		}

		// attractive forces

		for (int e = 0; e < C.numberOfEdges(); ++e) {
			int v = C.m_src[e];
			int u = C.m_tgt[e];

			double delta_x = C.m_x[v] - C.m_x[u];
			double delta_y = C.m_y[v] - C.m_y[u];

			double dist = max(minDist, sqrt(delta_x * delta_x + delta_y * delta_y));

			disp_x[v] -= delta_x * dist / k;
			disp_y[v] -= delta_y * dist / k;

			disp_x[u] += delta_x * dist / k;
			disp_y[u] += delta_y * dist / k;
		}

		// limit the maximum displacement to the temperature (m_tx,m_ty)

#pragma omp parallel for
		for (int v = 0; v < n; ++v) {
			double dist = max(minDist, sqrt(disp_x[v] * disp_x[v] + disp_y[v] * disp_y[v]));
			double xdisplace = disp_x[v] / dist * min(dist, tx);
			double ydisplace = disp_y[v] / dist * min(dist, ty);
			double eucdistsq = xdisplace * xdisplace + ydisplace * ydisplace;
			double threshold = m_convTolerance * m_idealEdgeLength;
			if (eucdistsq > threshold * threshold) {
				converged = false;
			}
			C.m_x[v] += xdisplace;
			C.m_y[v] += ydisplace;
		}

		cool(tx, ty, cF);
		//}
		itCount++;
		converged = (itCount > m_iterations || converged);
	}

	System::alignedMemoryFree(disp_x);
	System::alignedMemoryFree(disp_y);
}

void SpringEmbedderFRExact::mainStep_sse3(ArrayGraph& C) {
#ifdef OGDF_SSE3_EXTENSIONS
	const int n = C.numberOfNodes();

#	ifdef _OPENMP
	const int work = 256;
	const int nThreadsRep = min(omp_get_max_threads(), 1 + n * n / work);
	const int nThreadsPrev = min(omp_get_max_threads(), 1 + n / work);
#	endif

	const double k = m_idealEdgeLength;
	const double kSquare = k * k;
	const double c_rep = 0.052 * kSquare; // 0.2 = factor for repulsive forces as suggested by Warshal

	const double minDist = 10e-6; //100*DBL_EPSILON;
	const double minDistSquare = minDist * minDist;

	double* disp_x = (double*)System::alignedMemoryAlloc16(n * sizeof(double));
	double* disp_y = (double*)System::alignedMemoryAlloc16(n * sizeof(double));

#	ifdef OGDF_SPRINGEMBEDDERFREXACT_USE_KSQUARE
	__m128d mm_kSquare = _mm_set1_pd(kSquare);
#	endif
	__m128d mm_minDist = _mm_set1_pd(minDist);
	__m128d mm_minDistSquare = _mm_set1_pd(minDistSquare);
	__m128d mm_c_rep = _mm_set1_pd(c_rep);

#	pragma omp parallel num_threads(nThreadsRep)
	{
		double tx = m_txNull;
		double ty = m_tyNull;
		int cF = 1;

		for (int i = 1; i <= m_iterations; i++) {
			// repulsive forces

#	pragma omp for
			for (int v = 0; v < n; ++v) {
				__m128d mm_disp_xv = _mm_setzero_pd();
				__m128d mm_disp_yv = _mm_setzero_pd();

				__m128d mm_xv = _mm_set1_pd(C.m_x[v]);
				__m128d mm_yv = _mm_set1_pd(C.m_y[v]);

				auto compute_pd = [&](int u) {
					__m128d mm_delta_x = _mm_sub_pd(mm_xv, _mm_load_pd(&C.m_x[u]));
					__m128d mm_delta_y = _mm_sub_pd(mm_yv, _mm_load_pd(&C.m_y[u]));

					__m128d mm_xSquare = _mm_mul_pd(mm_delta_x, mm_delta_x);
					__m128d mm_ySquare = _mm_mul_pd(mm_delta_y, mm_delta_y);
					__m128d mm_distSquare =
							_mm_max_pd(mm_minDistSquare, _mm_add_pd(mm_xSquare, mm_ySquare));

					__m128d mm_t =
#	ifndef OGDF_SPRINGEMBEDDERFREXACT_USE_KSQUARE
							_mm_div_pd(_mm_load_pd(&C.m_nodeWeight[u]), mm_distSquare);
#	else
							_mm_div_pd(mm_kSquare, mm_distSquare);
#	endif
					mm_disp_xv = _mm_add_pd(mm_disp_xv, _mm_mul_pd(mm_delta_x, mm_t));
					mm_disp_yv = _mm_add_pd(mm_disp_yv, _mm_mul_pd(mm_delta_y, mm_t));
				};
				auto compute_sd = [&](int u) {
					__m128d mm_delta_x = _mm_sub_sd(mm_xv, _mm_load_sd(&C.m_x[u]));
					__m128d mm_delta_y = _mm_sub_sd(mm_yv, _mm_load_sd(&C.m_y[u]));

					__m128d mm_xSquare = _mm_mul_sd(mm_delta_x, mm_delta_x);
					__m128d mm_ySquare = _mm_mul_sd(mm_delta_y, mm_delta_y);
					__m128d mm_distSquare =
							_mm_max_sd(mm_minDistSquare, _mm_add_sd(mm_xSquare, mm_ySquare));

					__m128d mm_t =
#	ifndef OGDF_SPRINGEMBEDDERFREXACT_USE_KSQUARE
							_mm_div_sd(_mm_load_sd(&C.m_nodeWeight[u]), mm_distSquare);
#	else
							_mm_div_sd(mm_kSquare, mm_distSquare);
#	endif
					mm_disp_xv = _mm_add_sd(mm_disp_xv, _mm_mul_sd(mm_delta_x, mm_t));
					mm_disp_yv = _mm_add_sd(mm_disp_yv, _mm_mul_sd(mm_delta_y, mm_t));
				};

				int u;
				for (u = 0; u + 1 < v; u += 2) {
					compute_pd(u);
				}
				int uStart = u + 2;
				if (u == v) {
					++u;
				}
				if (u < n) {
					compute_sd(u);
				}

				// TODO do we need u+1 here?
				//      GCC's leak sanitizer reports a heap buffer overflow when using just u.
				for (u = uStart; u + 1 < n; u += 2) {
					compute_pd(u);
				}
				if (u < n) {
					compute_sd(u);
				}

				mm_disp_xv = _mm_hadd_pd(mm_disp_xv, mm_disp_xv);
				mm_disp_yv = _mm_hadd_pd(mm_disp_yv, mm_disp_yv);

				_mm_store_sd(&disp_x[v], _mm_mul_sd(mm_disp_xv, mm_c_rep));
				_mm_store_sd(&disp_y[v], _mm_mul_sd(mm_disp_yv, mm_c_rep));
			}

			// attractive forces

#	pragma omp single
			for (int e = 0; e < C.numberOfEdges(); ++e) {
				int v = C.m_src[e];
				int u = C.m_tgt[e];

				double delta_x = C.m_x[v] - C.m_x[u];
				double delta_y = C.m_y[v] - C.m_y[u];

				double dist = max(minDist, sqrt(delta_x * delta_x + delta_y * delta_y));

				disp_x[v] -= delta_x * dist / k;
				disp_y[v] -= delta_y * dist / k;

				disp_x[u] += delta_x * dist / k;
				disp_y[u] += delta_y * dist / k;
			}

			// limit the maximum displacement to the temperature (m_tx,m_ty)

			__m128d mm_tx = _mm_set1_pd(tx);
			__m128d mm_ty = _mm_set1_pd(ty);

#	pragma omp for nowait
			for (int v = 0; v < n - 1; v += 2) {
				__m128d mm_disp_xv = _mm_load_pd(&disp_x[v]);
				__m128d mm_disp_yv = _mm_load_pd(&disp_y[v]);

				__m128d mm_dist = _mm_max_pd(mm_minDist,
						_mm_sqrt_pd(_mm_add_pd(_mm_mul_pd(mm_disp_xv, mm_disp_xv),
								_mm_mul_pd(mm_disp_yv, mm_disp_yv))));

				_mm_store_pd(&C.m_x[v],
						_mm_add_pd(_mm_load_pd(&C.m_x[v]),
								_mm_mul_pd(_mm_div_pd(mm_disp_xv, mm_dist),
										_mm_min_pd(mm_dist, mm_tx))));
				_mm_store_pd(&C.m_y[v],
						_mm_add_pd(_mm_load_pd(&C.m_y[v]),
								_mm_mul_pd(_mm_div_pd(mm_disp_yv, mm_dist),
										_mm_min_pd(mm_dist, mm_ty))));
			}
#	pragma omp single nowait
			{
				if (n % 2) {
					int v = n - 1;
					double dist = max(minDist, sqrt(disp_x[v] * disp_x[v] + disp_y[v] * disp_y[v]));

					C.m_x[v] += disp_x[v] / dist * min(dist, tx);
					C.m_y[v] += disp_y[v] / dist * min(dist, ty);
				}
			}

			cool(tx, ty, cF);

#	pragma omp barrier
		}
	}

	System::alignedMemoryFree(disp_x);
	System::alignedMemoryFree(disp_y);

#else
	mainStep(C);
#endif
}

}
