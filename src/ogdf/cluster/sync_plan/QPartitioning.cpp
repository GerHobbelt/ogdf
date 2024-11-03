/** \file
 * \brief TODO Document
 *
 * \author Simon D. Fink <ogdf@niko.fink.bayern>
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
#include <ogdf/basic/Math.h>
#include <ogdf/cluster/sync_plan/QPartitioning.h>

using namespace ogdf;

bool QPartitioning::isQVertex(node n) const { return partitions[n] != NO_PARTITION; }

int QPartitioning::getPartitionOf(node n) const {
	OGDF_ASSERT(isQVertex(n));
	return partitions[n];
}

int QPartitioning::makeQVertex(node n, int p) {
	OGDF_ASSERT(!isQVertex(n));
	OGDF_ASSERT(n->degree() > 2);
	// this could actually be > 3, but then we have to manually preserve the edge bijection / order
	// OGDF_ASSERT(!pq.components.isCutVertex(n));
	if (p == NO_PARTITION) {
		p = partition_next_id;
		partition_next_id++;
	}
	int new_size = Math::nextPower2(MIN_TABLE_SIZE, partition_next_id);
	if (new_size != partition_table_size) {
		partition_table_size = new_size;
		resizeArrays();
	}
	partitioned_nodes[p].emplaceBack(n);
	partitions[n] = p;
	q_vertex_count++;
	return p;
}

void QPartitioning::releaseQVertex(node n) {
	OGDF_ASSERT(isQVertex(n));
	int p = partitions[n];
	partitions[n] = NO_PARTITION;
	// FIXME the list is very short in our case, but do we really need to do this in our undo operations?
	bool has_removed = partitioned_nodes[p].removeFirst(n);
	OGDF_ASSERT(has_removed);
	q_vertex_count--;
}

void QPartitioning::nodeDeleted(node v) {
	if (isQVertex(v)) {
		partitioned_nodes[getPartitionOf(v)].removeFirst(v);
		// partition might have turned empty, but we don't care
	}
}
