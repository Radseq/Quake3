#include "tr_world.hpp"
#include <cstddef>

static const byte *R_ClusterPVS(int cluster)
{
	if (!tr.world->vis || cluster < 0 || cluster >= tr.world->numClusters)
	{
		return tr.world->novis;
	}
	return tr.world->vis + cluster * tr.world->clusterBytes;
}

/*
=================
R_CullTriSurf_plus

Returns true if the grid is completely culled away.
Also sets the clipped hint bit in tess
=================
*/
static bool R_CullTriSurf_plus(srfTriangles_t *cv)
{
	int boxCull;

	boxCull = R_CullLocalBox_plus(cv->bounds);

	if (boxCull == CULL_OUT)
	{
		return true;
	}
	return false;
}

/*
=================
R_CullGrid_plus

Returns true if the grid is completely culled away.
Also sets the clipped hint bit in tess
=================
*/
static bool R_CullGrid_plus(srfGridMesh_t *cv)
{
	if (r_nocurves->integer)
	{
		return true;
	}

	int sphereCull;

	if (tr.currentEntityNum != REFENTITYNUM_WORLD)
	{
		sphereCull = R_CullLocalPointAndRadius_plus(cv->localOrigin, cv->meshRadius);
	}
	else
	{
		sphereCull = R_CullPointAndRadius(cv->localOrigin, cv->meshRadius);
	}

	// check for trivial reject
	if (sphereCull == CULL_OUT)
	{
		tr.pc.c_sphere_cull_patch_out++;
		return true;
	}
	// check bounding box if necessary
	else if (sphereCull == CULL_CLIP)
	{
		tr.pc.c_sphere_cull_patch_clip++;

		int boxCull = R_CullLocalBox_plus(cv->meshBounds);

		if (boxCull == CULL_OUT)
		{
			tr.pc.c_box_cull_patch_out++;
			return true;
		}
		else if (boxCull == CULL_IN)
		{
			tr.pc.c_box_cull_patch_in++;
		}
		else
		{
			tr.pc.c_box_cull_patch_clip++;
		}
	}
	else
	{
		tr.pc.c_sphere_cull_patch_in++;
	}

	return false;
}

/*
================
R_CullSurface_plus

Tries to back face cull surfaces before they are lighted or
added to the sorting list.

This will also allow mirrors on both sides of a model without recursion.
================
*/
static bool R_CullSurface_plus(const surfaceType_t *surface, shader_t *shader)
{
	srfSurfaceFace_t *sface;
	float d;

	if (r_nocull->integer)
	{
		return false;
	}

	if (*surface == SF_GRID)
	{
		return R_CullGrid_plus((srfGridMesh_t *)surface);
	}

	if (*surface == SF_TRIANGLES)
	{
		return R_CullTriSurf_plus((srfTriangles_t *)surface);
	}

	if (*surface != SF_FACE)
	{
		return false;
	}

	if (shader->cullType == CT_TWO_SIDED)
	{
		return false;
	}

	// face culling
	if (!r_facePlaneCull->integer)
	{
		return false;
	}

	sface = (srfSurfaceFace_t *)surface;
	d = DotProduct(tr.ort.viewOrigin, sface->plane.normal);

	// don't cull exactly on the plane, because there are levels of rounding
	// through the BSP, ICD, and hardware that may cause pixel gaps if an
	// epsilon isn't allowed here
	if (shader->cullType == CT_FRONT_SIDED)
	{
		if (d < sface->plane.dist - 8)
		{
			return true;
		}
	}
	else
	{
		if (d > sface->plane.dist + 8)
		{
			return true;
		}
	}

	return false;
}

static mnode_t *R_PointInLeaf(const vec3_t p)
{
	mnode_t *node;
	float d;
	const cplane_t *plane;

	if (!tr.world)
	{
		ri.Error(ERR_DROP, "R_PointInLeaf: bad model");
	}

	node = tr.world->nodes;
	while (true)
	{
		if (node->contents != CONTENTS_NODE)
		{
			break;
		}
		plane = node->plane;
		d = DotProduct(p, plane->normal) - plane->dist;
		if (d > 0)
		{
			node = node->children[0];
		}
		else
		{
			node = node->children[1];
		}
	}

	return node;
}

/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current
cluster
===============
*/
static void R_MarkLeaves_plus()
{
	const byte *vis;
	mnode_t *leaf, *parent;
	int i;
	int cluster;

	if (r_lockpvs->integer)
	{
		return;
	}

	leaf = R_PointInLeaf(tr.viewParms.pvsOrigin);
	cluster = leaf->cluster;

	if (tr.viewCluster == cluster && !tr.refdef.areamaskModified && !r_showcluster->modified)
	{
		return;
	}

	if (r_showcluster->modified || r_showcluster->integer)
	{
		r_showcluster->modified = false;
		if (r_showcluster->integer)
		{
			ri.Printf(PRINT_ALL, "cluster:%i  area:%i\n", cluster, leaf->area);
		}
	}

	tr.visCount++;
	tr.viewCluster = cluster;

	if (r_novis->integer || tr.viewCluster == -1)
	{
		for (i = 0; i < tr.world->numnodes; i++)
		{
			if (tr.world->nodes[i].contents != CONTENTS_SOLID)
			{
				tr.world->nodes[i].visframe = tr.visCount;
			}
		}
		return;
	}

	vis = R_ClusterPVS(tr.viewCluster);

	for (i = 0, leaf = tr.world->nodes; i < tr.world->numnodes; i++, leaf++)
	{
		cluster = leaf->cluster;
		if (cluster < 0 || cluster >= tr.world->numClusters)
		{
			continue;
		}

		if (!(vis[cluster >> 3] & (1 << (cluster & 7))))
		{
			continue;
		}

		if ((tr.refdef.areamask[leaf->area >> 3] & (1 << (leaf->area & 7))))
		{
			continue;
		}

		parent = leaf;
		do
		{
			if (parent->visframe == tr.visCount)
				break;
			parent->visframe = tr.visCount;
			parent = parent->parent;
		} while (parent);
	}
}

#ifdef USE_PMLIGHT
bool R_LightCullBounds(const dlight_t *dl, const vec3_t mins, const vec3_t maxs)
{
	if (dl->linear)
	{
		if (dl->transformed[0] - dl->radius > maxs[0] && dl->transformed2[0] - dl->radius > maxs[0])
			return true;
		if (dl->transformed[0] + dl->radius < mins[0] && dl->transformed2[0] + dl->radius < mins[0])
			return true;

		if (dl->transformed[1] - dl->radius > maxs[1] && dl->transformed2[1] - dl->radius > maxs[1])
			return true;
		if (dl->transformed[1] + dl->radius < mins[1] && dl->transformed2[1] + dl->radius < mins[1])
			return true;

		if (dl->transformed[2] - dl->radius > maxs[2] && dl->transformed2[2] - dl->radius > maxs[2])
			return true;
		if (dl->transformed[2] + dl->radius < mins[2] && dl->transformed2[2] + dl->radius < mins[2])
			return true;

		return false;
	}

	if (dl->transformed[0] - dl->radius > maxs[0])
		return true;
	if (dl->transformed[0] + dl->radius < mins[0])
		return true;

	if (dl->transformed[1] - dl->radius > maxs[1])
		return true;
	if (dl->transformed[1] + dl->radius < mins[1])
		return true;

	if (dl->transformed[2] - dl->radius > maxs[2])
		return true;
	if (dl->transformed[2] + dl->radius < mins[2])
		return true;

	return false;
}

static bool R_LightCullFace(const srfSurfaceFace_t *face, const dlight_t *dl)
{
	float d = DotProduct(dl->transformed, face->plane.normal) - face->plane.dist;
	if (dl->linear)
	{
		float d2 = DotProduct(dl->transformed2, face->plane.normal) - face->plane.dist;
		if ((d < -dl->radius) && (d2 < -dl->radius))
			return true;
		if ((d > dl->radius) && (d2 > dl->radius))
			return true;
	}
	else
	{
		if ((d < -dl->radius) || (d > dl->radius))
			return true;
	}

	return false;
}

static bool R_LightCullSurface_plus(const surfaceType_t *surface, const dlight_t *dl)
{
	switch (*surface)
	{
	case SF_FACE:
		return R_LightCullFace((const srfSurfaceFace_t *)surface, dl);
	case SF_GRID:
	{
		const srfGridMesh_t *grid = (const srfGridMesh_t *)surface;
		return R_LightCullBounds(dl, grid->meshBounds[0], grid->meshBounds[1]);
	}
	case SF_TRIANGLES:
	{
		const srfTriangles_t *tris = (const srfTriangles_t *)surface;
		return R_LightCullBounds(dl, tris->bounds[0], tris->bounds[1]);
	}
	default:
		return false;
	};
}

#endif // USE_PMLIGHT

#ifdef USE_LEGACY_DLIGHTS
static int R_DlightFace(srfSurfaceFace_t *face, int dlightBits)
{
	float d;
	int i;
	const dlight_t *dl;

	for (i = 0; i < tr.refdef.num_dlights; i++)
	{
		if (!(dlightBits & (1 << i)))
		{
			continue;
		}
		dl = &tr.refdef.dlights[i];
		d = DotProduct(dl->transformed, face->plane.normal) - face->plane.dist;
		if (d < -dl->radius || d > dl->radius)
		{
			// dlight doesn't reach the plane
			dlightBits &= ~(1 << i);
		}
	}

	if (!dlightBits)
	{
		tr.pc.c_dlightSurfacesCulled++;
	}

	face->dlightBits = dlightBits;
	return dlightBits;
}

static int R_DlightGrid(srfGridMesh_t *grid, int dlightBits)
{
	int i;
	const dlight_t *dl;

	for (i = 0; i < tr.refdef.num_dlights; i++)
	{
		if (!(dlightBits & (1 << i)))
		{
			continue;
		}
		dl = &tr.refdef.dlights[i];
		if (dl->origin[0] - dl->radius > grid->meshBounds[1][0] || dl->origin[0] + dl->radius < grid->meshBounds[0][0] || dl->origin[1] - dl->radius > grid->meshBounds[1][1] || dl->origin[1] + dl->radius < grid->meshBounds[0][1] || dl->origin[2] - dl->radius > grid->meshBounds[1][2] || dl->origin[2] + dl->radius < grid->meshBounds[0][2])
		{
			// dlight doesn't reach the bounds
			dlightBits &= ~(1 << i);
		}
	}

	if (!dlightBits)
	{
		tr.pc.c_dlightSurfacesCulled++;
	}

	grid->dlightBits = dlightBits;
	return dlightBits;
}

static int R_DlightTrisurf(srfTriangles_t *surf, int dlightBits)
{
	// FIXME: more dlight culling to trisurfs...
	surf->dlightBits = dlightBits;
	return dlightBits;
#if 0
	int			i;
	const dlight_t	*dl;

	for ( i = 0 ; i < tr.refdef.num_dlights ; i++ ) {
		if ( ! ( dlightBits & ( 1 << i ) ) ) {
			continue;
		}
		dl = &tr.refdef.dlights[i];
		if ( dl->origin[0] - dl->radius > grid->meshBounds[1][0]
			|| dl->origin[0] + dl->radius < grid->meshBounds[0][0]
			|| dl->origin[1] - dl->radius > grid->meshBounds[1][1]
			|| dl->origin[1] + dl->radius < grid->meshBounds[0][1]
			|| dl->origin[2] - dl->radius > grid->meshBounds[1][2]
			|| dl->origin[2] + dl->radius < grid->meshBounds[0][2] ) {
			// dlight doesn't reach the bounds
			dlightBits &= ~( 1 << i );
		}
	}

	if ( !dlightBits ) {
		tr.pc.c_dlightSurfacesCulled++;
	}

	grid->dlightBits = dlightBits;
	return dlightBits;
#endif
}

/*
====================
R_DlightSurface

The given surface is going to be drawn, and it touches a leaf
that is touched by one ort more dlights, so try to throw out
more dlights if possible.
====================
*/
static int R_DlightSurface(msurface_t *surf, int dlightBits)
{
	if (*surf->data == SF_FACE)
	{
		dlightBits = R_DlightFace((srfSurfaceFace_t *)surf->data, dlightBits);
	}
	else if (*surf->data == SF_GRID)
	{
		dlightBits = R_DlightGrid((srfGridMesh_t *)surf->data, dlightBits);
	}
	else if (*surf->data == SF_TRIANGLES)
	{
		dlightBits = R_DlightTrisurf((srfTriangles_t *)surf->data, dlightBits);
	}
	else
	{
		dlightBits = 0;
	}

	if (dlightBits)
	{
		tr.pc.c_dlightSurfaces++;
	}

	return dlightBits;
}
#endif // USE_LEGACY_DLIGHTS

/*
=============================================================

	WORLD MODEL

=============================================================
*/

static void R_AddWorldSurface_plus(msurface_t *surf, int dlightBits)
{
	if (surf->viewCount == tr.viewCount)
	{
		return; // already in this view
	}

	surf->viewCount = tr.viewCount;
	// FIXME: bmodel fog?

	// try to cull before dlighting or adding
	if (R_CullSurface_plus(surf->data, surf->shader))
	{
		return;
	}

#ifdef USE_PMLIGHT
#ifdef USE_LEGACY_DLIGHTS
	if (r_dlightMode->integer)
#endif
	{
		surf->vcVisible = tr.viewCount;
		R_AddDrawSurf_plus(surf->data, surf->shader, surf->fogIndex, 0);
		return;
	}
#endif // USE_PMLIGHT

#ifdef USE_LEGACY_DLIGHTS
	// check for dlighting
	if (dlightBits)
	{
		dlightBits = R_DlightSurface(surf, dlightBits);
		dlightBits = (dlightBits != 0);
	}

	R_AddDrawSurf_plus(surf->data, surf->shader, surf->fogIndex, dlightBits);
#endif // USE_LEGACY_DLIGHTS
}

static void R_RecursiveWorldNode_plus(mnode_t *node, unsigned int planeBits, unsigned int dlightBits)
{

	do
	{
		unsigned int newDlights[2];

		// if the node wasn't marked as potentially visible, exit
		if (node->visframe != tr.visCount)
		{
			return;
		}

		// if the bounding volume is outside the frustum, nothing
		// inside can be visible OPTIMIZE: don't do this all the way to leafs?

		if (!r_nocull->integer)
		{
			int r;

			if (planeBits & 1)
			{
				r = BoxOnPlaneSide_plus(node->mins, node->maxs, &tr.viewParms.frustum[0]);
				if (r == 2)
				{
					return; // culled
				}
				if (r == 1)
				{
					planeBits &= ~1; // all descendants will also be in front
				}
			}

			if (planeBits & 2)
			{
				r = BoxOnPlaneSide_plus(node->mins, node->maxs, &tr.viewParms.frustum[1]);
				if (r == 2)
				{
					return; // culled
				}
				if (r == 1)
				{
					planeBits &= ~2; // all descendants will also be in front
				}
			}

			if (planeBits & 4)
			{
				r = BoxOnPlaneSide_plus(node->mins, node->maxs, &tr.viewParms.frustum[2]);
				if (r == 2)
				{
					return; // culled
				}
				if (r == 1)
				{
					planeBits &= ~4; // all descendants will also be in front
				}
			}

			if (planeBits & 8)
			{
				r = BoxOnPlaneSide_plus(node->mins, node->maxs, &tr.viewParms.frustum[3]);
				if (r == 2)
				{
					return; // culled
				}
				if (r == 1)
				{
					planeBits &= ~8; // all descendants will also be in front
				}
			}
		}

		if (node->contents != CONTENTS_NODE)
		{
			break;
		}

		// node is just a decision point, so go down both sides
		// since we don't care about sort orders, just go positive to negative

		// determine which dlights are needed
		newDlights[0] = 0;
		newDlights[1] = 0;
#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
		if (!r_dlightMode->integer)
#endif
			if (dlightBits)
			{
				int i;

				for (i = 0; i < tr.refdef.num_dlights; i++)
				{
					const dlight_t *dl;
					float dist;

					if (dlightBits & (1 << i))
					{
						dl = &tr.refdef.dlights[i];
						dist = DotProduct(dl->origin, node->plane->normal) - node->plane->dist;

						if (dist > -dl->radius)
						{
							newDlights[0] |= (1 << i);
						}
						if (dist < dl->radius)
						{
							newDlights[1] |= (1 << i);
						}
					}
				}
			}
#endif // USE_LEGACY_DLIGHTS

		// recurse down the children, front side first
		R_RecursiveWorldNode_plus(node->children[0], planeBits, newDlights[0]);

		// tail recurse
		node = node->children[1];
#ifdef USE_LEGACY_DLIGHTS
		dlightBits = newDlights[1];
#endif
	} while (1);

	{
		// leaf node, so add mark surfaces
		int c;
		msurface_t *surf, **mark;

		tr.pc.c_leafs++;

		// add to z buffer bounds
		if (node->mins[0] < tr.viewParms.visBounds[0][0])
		{
			tr.viewParms.visBounds[0][0] = node->mins[0];
		}
		if (node->mins[1] < tr.viewParms.visBounds[0][1])
		{
			tr.viewParms.visBounds[0][1] = node->mins[1];
		}
		if (node->mins[2] < tr.viewParms.visBounds[0][2])
		{
			tr.viewParms.visBounds[0][2] = node->mins[2];
		}

		if (node->maxs[0] > tr.viewParms.visBounds[1][0])
		{
			tr.viewParms.visBounds[1][0] = node->maxs[0];
		}
		if (node->maxs[1] > tr.viewParms.visBounds[1][1])
		{
			tr.viewParms.visBounds[1][1] = node->maxs[1];
		}
		if (node->maxs[2] > tr.viewParms.visBounds[1][2])
		{
			tr.viewParms.visBounds[1][2] = node->maxs[2];
		}

		// add the individual surfaces
		mark = node->firstmarksurface;
		c = node->nummarksurfaces;
		while (c--)
		{
			// the surface may have already been added if it
			// spans multiple leafs
			surf = *mark;
			R_AddWorldSurface_plus(surf, dlightBits);
			mark++;
		}
	}
}

#ifdef USE_PMLIGHT
static void R_AddLitSurface_plus(msurface_t *surf, const dlight_t *light)
{
	// since we're not worried about offscreen lights casting into the frustum (ATM !!!)
	// only add the "lit" version of this surface if it was already added to the view
	// if ( surf->viewCount != tr.viewCount )
	//	return;

	// surfaces that were faceculled will still have the current viewCount in vcBSP
	// because that's set to indicate that it's BEEN vis tested at all, to avoid
	// repeated vis tests, not whether it actually PASSED the vis test or not
	// only light surfaces that are GENUINELY visible, as opposed to merely in a visible LEAF
	if (surf->vcVisible != tr.viewCount)
	{
		return;
	}

	if (surf->shader->lightingStage < 0)
	{
		return;
	}

	if (surf->lightCount == tr.lightCount)
		return;

	surf->lightCount = tr.lightCount;

	if (R_LightCullSurface_plus(surf->data, light))
	{
		tr.pc.c_lit_culls++;
		return;
	}

	R_AddLitSurf_plus(surf->data, surf->shader, surf->fogIndex);
}

static void R_RecursiveLightNode_plus(const mnode_t *node)
{
	bool children[2];
	msurface_t **mark;
	msurface_t *surf;
	float d;
	int c;
	do
	{
		// if the node wasn't marked as potentially visible, exit
		if (node->visframe != tr.visCount)
			return;

		if (node->contents != CONTENTS_NODE)
			break;

		children[0] = children[1] = false;

		d = DotProduct(tr.light->origin, node->plane->normal) - node->plane->dist;
		if (d > -tr.light->radius)
		{
			children[0] = true;
		}
		if (d < tr.light->radius)
		{
			children[1] = true;
		}

		if (tr.light->linear)
		{
			d = DotProduct(tr.light->origin2, node->plane->normal) - node->plane->dist;
			if (d > -tr.light->radius)
			{
				children[0] = true;
			}
			if (d < tr.light->radius)
			{
				children[1] = true;
			}
		}

		if (children[0] && children[1])
		{
			R_RecursiveLightNode_plus(node->children[0]);
			node = node->children[1];
		}
		else if (children[0])
		{
			node = node->children[0];
		}
		else if (children[1])
		{
			node = node->children[1];
		}
		else
		{
			return;
		}

	} while (1);

	tr.pc.c_lit_leafs++;

	// add the individual surfaces
	c = node->nummarksurfaces;
	mark = node->firstmarksurface;
	while (c--)
	{
		// the surface may have already been added if it spans multiple leafs
		surf = *mark;
		R_AddLitSurface_plus(surf, tr.light);
		mark++;
	}
}

#endif // USE_PMLIGHT

void R_AddWorldSurfaces_plus()
{
#ifdef USE_PMLIGHT
	dlight_t *dl;
	int i;
#endif

	if (!r_drawworld->integer)
	{
		return;
	}

	if (tr.refdef.rdflags & RDF_NOWORLDMODEL)
	{
		return;
	}

	tr.currentEntityNum = REFENTITYNUM_WORLD;
	tr.shiftedEntityNum = tr.currentEntityNum << QSORT_REFENTITYNUM_SHIFT;

	// determine which leaves are in the PVS / areamask
	R_MarkLeaves_plus();

	// clear out the visible min/max
	ClearBounds_plus(tr.viewParms.visBounds[0], tr.viewParms.visBounds[1]);

	// perform frustum culling and add all the potentially visible surfaces
	if (tr.refdef.num_dlights > MAX_DLIGHTS)
	{
		tr.refdef.num_dlights = MAX_DLIGHTS;
	}

	R_RecursiveWorldNode_plus(tr.world->nodes, 15, (1ULL << tr.refdef.num_dlights) - 1);

#ifdef USE_PMLIGHT
#ifdef USE_LEGACY_DLIGHTS
	if (!r_dlightMode->integer)
		return;
#endif // USE_LEGACY_DLIGHTS

	// "transform" all the dlights so that dl->transformed is actually populated
	// (even though HERE it's == dl->origin) so we can always use R_LightCullBounds
	// instead of having copypasted versions for both world and local cases

	R_TransformDlights_plus(tr.viewParms.num_dlights, tr.viewParms.dlights, &tr.viewParms.world);
	for (i = 0; i < tr.viewParms.num_dlights; i++)
	{
		dl = &tr.viewParms.dlights[i];
		dl->head = dl->tail = NULL;
		if (R_CullDlight_plus(dl) == CULL_OUT)
		{
			tr.pc.c_light_cull_out++;
			continue;
		}
		tr.pc.c_light_cull_in++;
		tr.lightCount++;
		tr.light = dl;
		R_RecursiveLightNode_plus(tr.world->nodes);
	}
#endif // USE_PMLIGHT
}

void R_AddBrushModelSurfaces_plus(trRefEntity_t *ent)
{
	bmodel_t *bmodel;
	int clip;
	const model_t *pModel;
	int i;

	pModel = R_GetModelByHandle_plus(ent->e.hModel);

	bmodel = pModel->bmodel;

	clip = R_CullLocalBox_plus(bmodel->bounds);
	if (clip == CULL_OUT)
	{
		return;
	}

#ifdef USE_PMLIGHT
#ifdef USE_LEGACY_DLIGHTS
	if (r_dlightMode->integer)
#endif
	{
		dlight_t *dl;
		int s;

		for (s = 0; s < bmodel->numSurfaces; s++)
		{
			R_AddWorldSurface_plus(bmodel->firstSurface + s, 0);
		}

		R_SetupEntityLighting_plus(&tr.refdef, ent);

		R_TransformDlights_plus(tr.viewParms.num_dlights, tr.viewParms.dlights, &tr.ort);

		for (i = 0; i < tr.viewParms.num_dlights; i++)
		{
			dl = &tr.viewParms.dlights[i];
			if (!R_LightCullBounds(dl, bmodel->bounds[0], bmodel->bounds[1]))
			{
				tr.lightCount++;
				tr.light = dl;
				for (s = 0; s < bmodel->numSurfaces; s++)
				{
					R_AddLitSurface_plus(bmodel->firstSurface + s, dl);
				}
			}
		}
		return;
	}
#endif // USE_PMLIGHT

#ifdef USE_LEGACY_DLIGHTS
	R_SetupEntityLighting_plus(&tr.refdef, ent);
	R_DlightBmodel_plus(bmodel);

	for (i = 0; i < bmodel->numSurfaces; i++)
	{
		R_AddWorldSurface_plus(bmodel->firstSurface + i, tr.currentEntity->needDlights);
	}
#endif
}

bool R_inPVS_plus( const vec3_t p1, const vec3_t p2 ) {
	const mnode_t *leaf;
	const byte	*vis;

	leaf = R_PointInLeaf( p1 );
	vis = ri.CM_ClusterPVS( leaf->cluster );
	leaf = R_PointInLeaf( p2 );

	if ( !(vis[leaf->cluster>>3] & (1<<(leaf->cluster&7))) ) {
		return false;
	}
	return true;
}