/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Joshua Leung
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/node/deg_node_factory.cc
 *
 *  \ingroup depsgraph
 */

#include "intern/node/deg_node_factory.h"

namespace DEG {

/* Global type registry */
static DepsNodeFactory *
node_typeinfo_registry[static_cast<int>(NodeType::NUM_TYPES)] = {NULL};

void register_node_typeinfo(DepsNodeFactory *factory)
{
	BLI_assert(factory != NULL);
	const int type_as_int = static_cast<int>(factory->type());
	node_typeinfo_registry[type_as_int] = factory;
}

DepsNodeFactory *type_get_factory(const NodeType type)
{
	/* Look up type - at worst, it doesn't exist in table yet, and we fail. */
	const int type_as_int = static_cast<int>(type);
	return node_typeinfo_registry[type_as_int];
}

}  // namespace DEG
