/** \file
 * \brief Utility macros for declaring copy and move constructors and assignment operations.
 *
 * \author Simon D. Fink
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

//! Explicitly disables (deletes) copy construction and assignment for class \p cls.
#define OGDF_NO_COPY(cls)          \
	cls(const cls& copy) = delete; \
	cls& operator=(const cls& copy) = delete;

//! Explicitly disables (deletes) move construction and assignment for class \p cls.
#define OGDF_NO_MOVE(cls)     \
	cls(cls&& move) = delete; \
	cls& operator=(cls&& move) = delete;

//! Explicitly provides default copy construction and assignment for class \p cls.
#define OGDF_DEFAULT_COPY(cls)      \
	cls(const cls& copy) = default; \
	cls& operator=(const cls& copy) = default;

//! Explicitly provides default move construction and assignment for class \p cls.
#define OGDF_DEFAULT_MOVE(cls) \
	cls(cls&& move) = default; \
	cls& operator=(cls&& move) = default;

//! Declares the copy constructor for class \p cls.
#define OGDF_COPY_CONSTR(cls) cls(const cls& copy)
//! Declares the copy assignment operation for class \p cls.
#define OGDF_COPY_OP(cls) cls& operator=(const cls& copy)
//! Declares the move constructor for class \p cls.
#define OGDF_MOVE_CONSTR(cls) cls(cls&& move) noexcept : cls()
//! Declares the move assignment operation for class \p cls.
#define OGDF_MOVE_OP(cls) cls& operator=(cls&& move) noexcept
//! Declares the swap function for class \p cls.
#define OGDF_SWAP_OP(cls) friend void swap(cls& first, cls& second) noexcept

//! Provide move construct/assign and copy assign given there is a copy constructor and swap.
/**
 * Only requires custom implementations of OGDF_COPY_CONSTR and OGDF_SWAP_OP
 * and automatically provides OGDF_COPY_OP, OGDF_MOVE_CONSTR, and OGDF_MOVE_OP.
 *
 * See https://stackoverflow.com/a/3279550 for more details on this idiom.
 */
#define OGDF_COPY_MOVE_BY_SWAP(cls)     \
	cls& operator=(cls copy_by_value) { \
		using std::swap;                \
		swap(*this, copy_by_value);     \
	}                                   \
	OGDF_MOVE_CONSTR(cls) {             \
		using std::swap;                \
		swap(*this, move);              \
	}                                   \
	OGDF_MOVE_OP(cls) {                 \
		using std::swap;                \
		swap(*this, move);              \
	}
