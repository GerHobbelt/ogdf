/** \file
 * \brief Declaration and implementation of the class PQInternalKey.
 *
 * \author Sebastian Leipert
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

#include <ogdf/basic/pqtree/PQBasicKey.h>

namespace ogdf {

/**
 * The class template PQInternalKey is a derived class of class template
 * PQBasicKey. PQInternalKey is a concrete class.
 * It is constructed to store any kind of information of internal nodes of the
 * PQ-tree. It may only be used for internal nodes as P- and
 * Q-nodes. This information is not allowed to leaves.
 *
 * The information is stored in #m_userStructInternal and
 * is assigned to a unique node in the PQ-tree. This
 * unique node can be identified with the #m_nodePointer of the
 * astract base class PQBasicKey. The
 * maintainance of this pointer is left to the user. By keeping the
 * responsibillity by the user, nodes with certain informations can
 * be identified and  accessed by him in constant time. This makes
 * the adaption of algorithms fast and easy.
 */
template<class T, class X, class Y>
class PQInternalKey : public PQBasicKey<T, X, Y> {
public:
	/**
	 * The class template PQInternalKey has only one public member: the
	 * #m_userStructInternal that has to be overloaded by the client. This
	 * element is kept public, since the user has to have the opportunity
	 * to manipulate the information that was stored by his algorithm at a
	 * node.
	 */
	Y m_userStructInternal;

	//Constructor
	explicit PQInternalKey(Y element) { m_userStructInternal = element; }

	//Destructor
	virtual ~PQInternalKey() { }

	//! Overloaded pure virtual function returning 0.
	virtual T userStructKey() { return 0; }

	//! Overloaded pure virtual function returning 0.
	virtual X userStructInfo() { return 0; }

	//! Overloaded pure virtual function returning #m_userStructInternal.
	virtual Y userStructInternal() { return m_userStructInternal; }
};

}
