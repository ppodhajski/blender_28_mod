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
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Joshua Leung
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/depsgraph_query_filter.cc
 *  \ingroup depsgraph
 *
 * Implementation of Graph Filtering API
 */

#include "MEM_guardedalloc.h"

extern "C" {
#include <string.h> // XXX: memcpy

#include "BLI_utildefines.h"
#include "BKE_idcode.h"
#include "BKE_main.h"
#include "BLI_listbase.h"
#include "BLI_ghash.h"

#include "BKE_action.h" // XXX: BKE_pose_channel_from_name
} /* extern "C" */

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "util/deg_util_foreach.h"

#include "intern/eval/deg_eval_copy_on_write.h"

#include "intern/depsgraph.h"
#include "intern/depsgraph_types.h"
#include "intern/depsgraph_intern.h"

#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_id.h"
#include "intern/nodes/deg_node_operation.h"


/* *************************************************** */
/* Graph Filtering Internals */

namespace DEG {

/* UserData for deg_add_retained_id_cb */
struct RetainedIdUserData {
	DEG_FilterQuery *query;
	GSet *set;
};

/* Helper for DEG_foreach_ancestor_id()
 * Keep track of all ID's encountered in a set
 */
void deg_add_retained_id_cb(ID *id, void *user_data)
{
	RetainedIdUserData *data = (RetainedIdUserData *)user_data;
	BLI_gset_add(data->set, (void *)id);
}

/* ------------------------------------------- */

/* Remove relations pointing to the given OperationDepsNode */
/* TODO: Make this part of OperationDepsNode? */
void deg_unlink_opnode(OperationDepsNode *op_node)
{
	/* Delete inlinks to this operation */
	for (DepsNode::Relations::const_iterator it_rel = op_node->inlinks.begin();
	     it_rel != op_node->inlinks.end();
	     )
	{
		DepsRelation *rel = *it_rel;
		rel->unlink();
		OBJECT_GUARDED_DELETE(rel, DepsRelation);
	}
	
	/* Delete outlinks from this operation */
	for (DepsNode::Relations::const_iterator it_rel = op_node->outlinks.begin();
	     it_rel != op_node->outlinks.end();
	     )
	{
		DepsRelation *rel = *it_rel;
		rel->unlink();
		OBJECT_GUARDED_DELETE(rel, DepsRelation);
	}
}

/* Remove and free given ID Node */
// XXX: Use id_cow or id_orig?
bool deg_filter_free_idnode(Depsgraph *graph, IDDepsNode *id_node,
                            const std::function <bool (ID_Type id_type)>& filter)
{
	if (id_node->done == 0) {
		/* This node has not been marked for deletion */
		return false;
	}
	else if (id_node->id_cow == NULL) {
		/* This means builder "stole" ownership of the copy-on-written
		 * datablock for her own dirty needs.
		 */
		return false;
	}
	else if (!deg_copy_on_write_is_expanded(id_node->id_cow)) {
		return false;
	}
	else {
		const ID_Type id_type = GS(id_node->id_cow->name);
		if (filter(id_type)) {
			id_node->destroy();
			return true;
		}
		else {
			return false;
		}
	}
}

/* Remove and free ID Nodes of a particular type from the graph
 *
 * See Depsgraph::clear_id_nodes() and Depsgraph::clear_id_nodes_conditional()
 * for more details about why we need these type filters
 */
void deg_filter_clear_ids_conditional(
        Depsgraph *graph,
        const std::function <bool (ID_Type id_type)>& filter)
{
	/* Based on Depsgraph::clear_id_nodes_conditional()... */
	for (Depsgraph::IDDepsNodes::const_iterator it = graph->id_nodes.begin();
	     it != graph->id_nodes.end();
	     )
	{
		IDDepsNode *id_node = *it;
		ID *id = id_node->id_orig;
		
		if (deg_filter_free_idnode(graph, id_node, filter)) {
			/* Node data got destroyed. Remove from collections, and free */
			BLI_ghash_remove(graph->id_hash, id, NULL, NULL);
			OBJECT_GUARDED_DELETE(id_node, IDDepsNode);
			it = graph->id_nodes.erase(it);
		}
		else {
			/* Node wasn't freed. Increment iterator */
			++it;
		}
	}
}

/* Remove every ID Node (and its associated subnodes, COW data) */
void deg_filter_remove_unwanted_ids(Depsgraph *graph, GSet *retained_ids)
{
	/* 1) First pass over ID nodes + their operations
	 * - Identify and tag ID's (via "done = 1") to be removed
	 * - Remove all links to/from operations that will be removed
	 */
	foreach (IDDepsNode *id_node, graph->id_nodes) {
		id_node->done = !BLI_gset_haskey(retained_ids, (void *)id_node->id_orig);
		if (id_node->done) {
			GHASH_FOREACH_BEGIN(ComponentDepsNode *, comp_node, id_node->components)
			{
				foreach (OperationDepsNode *op_node, comp_node->operations) {
					deg_unlink_opnode(op_node);
				}
			}
			GHASH_FOREACH_END();
		}
	}
	
	/* 2) Remove unwanted operations from graph->operations */
	for (Depsgraph::OperationNodes::const_iterator it_opnode = graph->operations.begin();
	     it_opnode != graph->operations.end();
	     )
	{
		OperationDepsNode *op_node = *it_opnode;
		IDDepsNode *id_node = op_node->owner->owner;
		if (id_node->done) {
			it_opnode = graph->operations.erase(it_opnode);
		}
		else {
			++it_opnode;
		}
	}
	
	/* Free ID nodes that are no longer wanted
	 * NOTE: See clear_id_nodes() for more details about what's happening here
	 *       (e.g. regarding the lambdas used for freeing order hacks)
	 */
	deg_filter_clear_ids_conditional(graph,  [](ID_Type id_type) { return id_type == ID_SCE; });
	deg_filter_clear_ids_conditional(graph,  [](ID_Type id_type) { return id_type != ID_PA; });
}

} //namespace DEG

/* *************************************************** */
/* Graph Filtering API */

/* Obtain a new graph instance that only contains the nodes needed */
Depsgraph *DEG_graph_filter(const Depsgraph *graph_src, Main *bmain, DEG_FilterQuery *query)
{
	const DEG::Depsgraph *deg_graph_src = reinterpret_cast<const DEG::Depsgraph *>(graph_src);
	if (deg_graph_src == NULL) {
		return NULL;
	}
	
	/* Construct a full new depsgraph based on the one we got */
	/* TODO: Improve the builders to not add any ID nodes we don't need later (e.g. ProxyBuilder?) */
	Depsgraph *graph_new = DEG_graph_new(deg_graph_src->scene,
	                                     deg_graph_src->view_layer,
	                                     DAG_EVAL_BACKGROUND);
	DEG_graph_build_from_view_layer(graph_new,
	                                bmain,
	                                deg_graph_src->scene,
	                                deg_graph_src->view_layer);
	
	/* Build a set of all the id's we want to keep */
	GSet *retained_ids = BLI_gset_ptr_new(__func__);
	DEG::RetainedIdUserData retained_id_data = {query, retained_ids};
	
	LISTBASE_FOREACH(DEG_FilterTarget *, target, &query->targets) {
		/* Target Itself */
		BLI_gset_add(retained_ids, (void *)target->id);
		
		/* Target's Ancestors (i.e. things it depends on) */
		DEG_foreach_ancestor_ID(graph_new,
		                        target->id,
		                        DEG::deg_add_retained_id_cb,
		                        &retained_id_data);
	}
	
	/* Remove everything we don't want to keep around anymore */
	DEG::Depsgraph *deg_graph_new = reinterpret_cast<DEG::Depsgraph *>(graph_new);
	if (BLI_gset_len(retained_ids) > 0) {
		DEG::deg_filter_remove_unwanted_ids(deg_graph_new, retained_ids);
	}
	// TODO: query->LOD filters
	
	/* Free temp data */
	BLI_gset_free(retained_ids, NULL);
	retained_ids = NULL;
	
	/* Return this new graph instance */
	return graph_new;
}

/* *************************************************** */
