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
 * Copyright 2016, Blender Foundation.
 * Contributor(s): Blender Institute
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/draw/modes/paint_weight_mode.c
 *  \ingroup draw
 */

#include "DRW_render.h"

/* If builtin shaders are needed */
#include "GPU_shader.h"

#include "draw_common.h"

#include "DNA_mesh_types.h"
#include "DNA_view3d_types.h"

#include "DEG_depsgraph_query.h"

extern char datatoc_common_world_clip_lib_glsl[];

extern char datatoc_paint_face_vert_glsl[];
extern char datatoc_paint_weight_vert_glsl[];
extern char datatoc_paint_weight_frag_glsl[];
extern char datatoc_paint_wire_vert_glsl[];
extern char datatoc_paint_wire_frag_glsl[];
extern char datatoc_paint_vert_frag_glsl[];
extern char datatoc_common_globals_lib_glsl[];

extern char datatoc_gpu_shader_uniform_color_frag_glsl[];

/* *********** LISTS *********** */

typedef struct PAINT_WEIGHT_PassList {
	struct DRWPass *weight_faces;
	struct DRWPass *wire_overlay;
	struct DRWPass *face_overlay;
	struct DRWPass *vert_overlay;
} PAINT_WEIGHT_PassList;

typedef struct PAINT_WEIGHT_StorageList {
	struct PAINT_WEIGHT_PrivateData *g_data;
} PAINT_WEIGHT_StorageList;

typedef struct PAINT_WEIGHT_Data {
	void *engine_type;
	DRWViewportEmptyList *fbl;
	DRWViewportEmptyList *txl;
	PAINT_WEIGHT_PassList *psl;
	PAINT_WEIGHT_StorageList *stl;
} PAINT_WEIGHT_Data;

typedef struct PAINT_WEIGHT_Shaders {
	struct GPUShader *weight_face;
	struct GPUShader *wire_overlay;
	struct GPUShader *face_overlay;
	struct GPUShader *vert_overlay;
} PAINT_WEIGHT_Shaders;

/* *********** STATIC *********** */

static struct {
	PAINT_WEIGHT_Shaders sh_data[DRW_SHADER_SLOT_LEN];

	int actdef;
} e_data = {NULL}; /* Engine data */

typedef struct PAINT_WEIGHT_PrivateData {
	DRWShadingGroup *fweights_shgrp;
	DRWShadingGroup *lwire_shgrp;
	DRWShadingGroup *face_shgrp;
	DRWShadingGroup *vert_shgrp;
} PAINT_WEIGHT_PrivateData;

/* *********** FUNCTIONS *********** */

static void PAINT_WEIGHT_engine_init(void *UNUSED(vedata))
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	PAINT_WEIGHT_Shaders *sh_data = &e_data.sh_data[draw_ctx->shader_slot];
	const bool is_clip = (draw_ctx->rv3d->rflag & RV3D_CLIPPING) != 0;

	if (is_clip) {
		DRW_state_clip_planes_set_from_rv3d(draw_ctx->rv3d);
	}

	if (!sh_data->weight_face) {
		const char *world_clip_lib_or_empty = is_clip ? datatoc_common_world_clip_lib_glsl : "";
		const char *world_clip_def_or_empty = is_clip ? "#define USE_WORLD_CLIP_PLANES\n" : "";

		sh_data->weight_face = DRW_shader_create_from_arrays({
		        .vert = (const char *[]){world_clip_lib_or_empty, datatoc_common_globals_lib_glsl, datatoc_paint_weight_vert_glsl, NULL},
		        .frag = (const char *[]){datatoc_common_globals_lib_glsl, datatoc_paint_weight_frag_glsl, NULL},
		        .defs = (const char *[]){world_clip_def_or_empty, NULL},
		});

		sh_data->wire_overlay = DRW_shader_create_from_arrays({
		        .vert = (const char *[]){world_clip_lib_or_empty, datatoc_common_globals_lib_glsl, datatoc_paint_wire_vert_glsl, NULL},
		        .frag = (const char *[]){datatoc_paint_wire_frag_glsl, NULL},
		        .defs = (const char *[]){world_clip_def_or_empty, "#define WEIGHT_MODE\n", NULL},
		});

		sh_data->face_overlay = DRW_shader_create_from_arrays({
		        .vert = (const char *[]){world_clip_lib_or_empty, datatoc_paint_face_vert_glsl, NULL},
		        .frag = (const char *[]){datatoc_gpu_shader_uniform_color_frag_glsl, NULL},
		        .defs = (const char *[]){world_clip_def_or_empty, NULL},
		});

		sh_data->vert_overlay = DRW_shader_create_from_arrays({
		        .vert = (const char *[]){world_clip_lib_or_empty, datatoc_common_globals_lib_glsl, datatoc_paint_wire_vert_glsl, NULL},
		        .frag = (const char *[]){datatoc_common_globals_lib_glsl, datatoc_paint_vert_frag_glsl, NULL},
		        .defs = (const char *[]){world_clip_def_or_empty, NULL},
		});
	}
}

static void PAINT_WEIGHT_cache_init(void *vedata)
{
	PAINT_WEIGHT_PassList *psl = ((PAINT_WEIGHT_Data *)vedata)->psl;
	PAINT_WEIGHT_StorageList *stl = ((PAINT_WEIGHT_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	const View3D *v3d = draw_ctx->v3d;
	RegionView3D *rv3d = draw_ctx->rv3d;
	PAINT_WEIGHT_Shaders *sh_data = &e_data.sh_data[draw_ctx->shader_slot];

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
	}

	{
		/* Create a pass */
		psl->weight_faces = DRW_pass_create(
		        "Weight Pass",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_MULTIPLY);

		stl->g_data->fweights_shgrp = DRW_shgroup_create(sh_data->weight_face, psl->weight_faces);

		DRW_shgroup_uniform_bool_copy(stl->g_data->fweights_shgrp, "drawContours", (v3d->overlay.wpaint_flag & V3D_OVERLAY_WPAINT_CONTOURS) != 0);

		DRW_shgroup_uniform_float(stl->g_data->fweights_shgrp, "opacity", &v3d->overlay.weight_paint_mode_opacity, 1);
		DRW_shgroup_uniform_texture(stl->g_data->fweights_shgrp, "colorramp", G_draw.weight_ramp);
		DRW_shgroup_uniform_block(stl->g_data->fweights_shgrp, "globalsBlock", G_draw.block_ubo);
		if (rv3d->rflag & RV3D_CLIPPING) {
			DRW_shgroup_world_clip_planes_from_rv3d(stl->g_data->fweights_shgrp, rv3d);
		}
	}

	{
		psl->wire_overlay = DRW_pass_create(
		        "Wire Pass",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_OFFSET_NEGATIVE);

		stl->g_data->lwire_shgrp = DRW_shgroup_create(sh_data->wire_overlay, psl->wire_overlay);
		DRW_shgroup_uniform_block(stl->g_data->lwire_shgrp, "globalsBlock", G_draw.block_ubo);
		if (rv3d->rflag & RV3D_CLIPPING) {
			DRW_shgroup_world_clip_planes_from_rv3d(stl->g_data->lwire_shgrp, rv3d);
		}
	}

	{
		psl->face_overlay = DRW_pass_create(
		        "Face Mask Pass",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND);

		stl->g_data->face_shgrp = DRW_shgroup_create(sh_data->face_overlay, psl->face_overlay);

		static float col[4] = {1.0f, 1.0f, 1.0f, 0.2f};
		DRW_shgroup_uniform_vec4(stl->g_data->face_shgrp, "color", col, 1);
		if (rv3d->rflag & RV3D_CLIPPING) {
			DRW_shgroup_world_clip_planes_from_rv3d(stl->g_data->face_shgrp, rv3d);
		}
	}

	{
		psl->vert_overlay = DRW_pass_create(
		        "Vert Mask Pass",
		        DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_OFFSET_NEGATIVE);

		stl->g_data->vert_shgrp = DRW_shgroup_create(sh_data->vert_overlay, psl->vert_overlay);
		DRW_shgroup_uniform_block(stl->g_data->vert_shgrp, "globalsBlock", G_draw.block_ubo);
		if (rv3d->rflag & RV3D_CLIPPING) {
			DRW_shgroup_world_clip_planes_from_rv3d(stl->g_data->vert_shgrp, rv3d);
		}
	}
}

static void PAINT_WEIGHT_cache_populate(void *vedata, Object *ob)
{
	PAINT_WEIGHT_StorageList *stl = ((PAINT_WEIGHT_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	const View3D *v3d = draw_ctx->v3d;

	if ((ob->type == OB_MESH) && (ob == draw_ctx->obact)) {
		const Mesh *me_orig = DEG_get_original_object(ob)->data;
		const bool use_wire = (v3d->overlay.paint_flag & V3D_OVERLAY_PAINT_WIRE) != 0;
		const bool use_surface = v3d->overlay.weight_paint_mode_opacity != 0.0f;
		const bool use_face_sel = (me_orig->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
		const bool use_vert_sel = (me_orig->editflag & ME_EDIT_PAINT_VERT_SEL) != 0;
		struct GPUBatch *geom;

		if (use_surface) {
			geom = DRW_cache_mesh_surface_weights_get(ob);
			DRW_shgroup_call_add(stl->g_data->fweights_shgrp, geom, ob->obmat);
		}

		if (use_face_sel || use_wire) {
			geom = DRW_cache_mesh_surface_edges_get(ob);
			DRW_shgroup_call_add(stl->g_data->lwire_shgrp, geom, ob->obmat);
		}

		if (use_face_sel) {
			geom = DRW_cache_mesh_surface_get(ob);
			DRW_shgroup_call_add(stl->g_data->face_shgrp, geom, ob->obmat);
		}

		if (use_vert_sel) {
			geom = DRW_cache_mesh_all_verts_get(ob);
			DRW_shgroup_call_add(stl->g_data->vert_shgrp, geom, ob->obmat);
		}
	}
}

static void PAINT_WEIGHT_draw_scene(void *vedata)
{
	PAINT_WEIGHT_PassList *psl = ((PAINT_WEIGHT_Data *)vedata)->psl;

	DRW_draw_pass(psl->weight_faces);
	DRW_draw_pass(psl->face_overlay);
	DRW_draw_pass(psl->wire_overlay);
	DRW_draw_pass(psl->vert_overlay);
}

static void PAINT_WEIGHT_engine_free(void)
{
	for (int sh_data_index = 0; sh_data_index < ARRAY_SIZE(e_data.sh_data); sh_data_index++) {
		PAINT_WEIGHT_Shaders *sh_data = &e_data.sh_data[sh_data_index];
		GPUShader **sh_data_as_array = (GPUShader **)sh_data;
		for (int i = 0; i < (sizeof(PAINT_WEIGHT_Shaders) / sizeof(GPUShader *)); i++) {
			DRW_SHADER_FREE_SAFE(sh_data_as_array[i]);
		}
	}
}

static const DrawEngineDataSize PAINT_WEIGHT_data_size = DRW_VIEWPORT_DATA_SIZE(PAINT_WEIGHT_Data);

DrawEngineType draw_engine_paint_weight_type = {
	NULL, NULL,
	N_("PaintWeightMode"),
	&PAINT_WEIGHT_data_size,
	&PAINT_WEIGHT_engine_init,
	&PAINT_WEIGHT_engine_free,
	&PAINT_WEIGHT_cache_init,
	&PAINT_WEIGHT_cache_populate,
	NULL,
	NULL,
	&PAINT_WEIGHT_draw_scene,
	NULL,
	NULL,
};
