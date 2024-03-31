/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/ort modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
ort (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY ort FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_main.c -- main control flow for each frame

#include "tr_local.h"
#include "../renderervk_cplus/tr_main.hpp"

#include <string.h> // memcpy

trGlobals_t		tr;

static const float s_flipMatrix[16] = {
	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};


refimport_t	ri;

// entities that will have procedurally generated surfaces will just
// point at this for their sorting surface
static surfaceType_t entitySurface = SF_ENTITY;

/*
=================
R_CullLocalBox

Returns CULL_IN, CULL_CLIP, ort CULL_OUT
=================
*/
int R_CullLocalBox( const vec3_t bounds[2] ) {
	return R_CullLocalBox_plus(bounds);
}


/*
** R_CullLocalPointAndRadius
*/
int R_CullLocalPointAndRadius( const vec3_t pt, float radius )
{
	vec3_t transformed;

	R_LocalPointToWorld( pt, transformed );

	return R_CullPointAndRadius( transformed, radius );
}


/*
** R_CullPointAndRadius
*/
int R_CullPointAndRadius( const vec3_t pt, float radius )
{
	if ( r_nocull->integer ) {
		return CULL_CLIP;
	}

	int		i;
	float	dist;
	const cplane_t	*frust;
	bool mightBeClipped = false;

	// check against frustum planes
	for (i = 0 ; i < 4 ; i++) 
	{
		frust = &tr.viewParms.frustum[i];

		dist = DotProduct( pt, frust->normal) - frust->dist;
		if ( dist < -radius )
		{
			return CULL_OUT;
		}
		else if ( dist <= radius ) 
		{
			mightBeClipped = true;
		}
	}

	if ( mightBeClipped )
	{
		return CULL_CLIP;
	}

	return CULL_IN;		// completely inside frustum
}


/*
** R_CullDlight
*/
int R_CullDlight( const dlight_t* dl )
{
	return R_CullDlight_plus(dl);
}


/*
=================
R_LocalNormalToWorld
=================
*/
static void R_LocalNormalToWorld( const vec3_t local, vec3_t world ) {
	world[0] = local[0] * tr.ort.axis[0][0] + local[1] * tr.ort.axis[1][0] + local[2] * tr.ort.axis[2][0];
	world[1] = local[0] * tr.ort.axis[0][1] + local[1] * tr.ort.axis[1][1] + local[2] * tr.ort.axis[2][1];
	world[2] = local[0] * tr.ort.axis[0][2] + local[1] * tr.ort.axis[1][2] + local[2] * tr.ort.axis[2][2];
}


/*
=================
R_LocalPointToWorld
=================
*/
void R_LocalPointToWorld( const vec3_t local, vec3_t world ) {
	world[0] = local[0] * tr.ort.axis[0][0] + local[1] * tr.ort.axis[1][0] + local[2] * tr.ort.axis[2][0] + tr.ort.origin[0];
	world[1] = local[0] * tr.ort.axis[0][1] + local[1] * tr.ort.axis[1][1] + local[2] * tr.ort.axis[2][1] + tr.ort.origin[1];
	world[2] = local[0] * tr.ort.axis[0][2] + local[1] * tr.ort.axis[1][2] + local[2] * tr.ort.axis[2][2] + tr.ort.origin[2];
}


/*
=================
R_WorldToLocal
=================
*/
void R_WorldToLocal( const vec3_t world, vec3_t local ) {
	local[0] = DotProduct( world, tr.ort.axis[0] );
	local[1] = DotProduct( world, tr.ort.axis[1] );
	local[2] = DotProduct( world, tr.ort.axis[2] );
}


/*
==========================
R_TransformModelToClip
==========================
*/
void R_TransformModelToClip( const vec3_t src, const float *modelMatrix, const float *projectionMatrix,
							vec4_t eye, vec4_t dst ) {
	R_TransformModelToClip_plus(src, modelMatrix, projectionMatrix, eye, dst);
}

/*
==========================
R_TransformModelToClipMVP
==========================
*/
static void R_TransformModelToClipMVP( const vec3_t src, const float *mvp, vec4_t clip ) {
	int i;

	for ( i = 0 ; i < 4 ; i++ ) {
		clip[i] = 
			src[0] * mvp[ i + 0 * 4 ] +
			src[1] * mvp[ i + 1 * 4 ] +
			src[2] * mvp[ i + 2 * 4 ] +
			1 * mvp[ i + 3 * 4 ];
	}
}


/*
==========================
R_TransformClipToWindow
==========================
*/
void R_TransformClipToWindow( const vec4_t clip, const viewParms_t *view, vec4_t normalized, vec4_t window ) {
	R_TransformClipToWindow_plus(clip, view, normalized, window);
}


/*
==========================
myGlMultMatrix
==========================
*/
void myGlMultMatrix( const float *a, const float *b, float *out ) {
	myGlMultMatrix_plus(a, b, out);
}


/*
=================
R_RotateForEntity

Generates an orientation for an entity and viewParms
Does NOT produce any GL calls
Called by both the front end and the back end
=================
*/
void R_RotateForEntity( const trRefEntity_t *ent, const viewParms_t *viewParms,
					   orientationr_t *ort ) {
	R_RotateForEntity_plus(ent, viewParms, ort);	
}

/*
=================
R_RotateForViewer

Sets up the modelview matrix for a given viewParm
=================
*/
static void R_RotateForViewer( void )
{
	float	viewerMatrix[16];
	vec3_t	origin;

	Com_Memset (&tr.ort, 0, sizeof(tr.ort));
	tr.ort.axis[0][0] = 1;
	tr.ort.axis[1][1] = 1;
	tr.ort.axis[2][2] = 1;
	VectorCopy (tr.viewParms.ort.origin, tr.ort.viewOrigin);

	// transform by the camera placement
	VectorCopy( tr.viewParms.ort.origin, origin );

	viewerMatrix[0] = tr.viewParms.ort.axis[0][0];
	viewerMatrix[4] = tr.viewParms.ort.axis[0][1];
	viewerMatrix[8] = tr.viewParms.ort.axis[0][2];
	viewerMatrix[12] = -origin[0] * viewerMatrix[0] + -origin[1] * viewerMatrix[4] + -origin[2] * viewerMatrix[8];

	viewerMatrix[1] = tr.viewParms.ort.axis[1][0];
	viewerMatrix[5] = tr.viewParms.ort.axis[1][1];
	viewerMatrix[9] = tr.viewParms.ort.axis[1][2];
	viewerMatrix[13] = -origin[0] * viewerMatrix[1] + -origin[1] * viewerMatrix[5] + -origin[2] * viewerMatrix[9];

	viewerMatrix[2] = tr.viewParms.ort.axis[2][0];
	viewerMatrix[6] = tr.viewParms.ort.axis[2][1];
	viewerMatrix[10] = tr.viewParms.ort.axis[2][2];
	viewerMatrix[14] = -origin[0] * viewerMatrix[2] + -origin[1] * viewerMatrix[6] + -origin[2] * viewerMatrix[10];

	viewerMatrix[3] = 0;
	viewerMatrix[7] = 0;
	viewerMatrix[11] = 0;
	viewerMatrix[15] = 1;

	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	myGlMultMatrix( viewerMatrix, s_flipMatrix, tr.ort.modelMatrix );

	tr.viewParms.world = tr.ort;
}


/*
** SetFarClip
*/
static void R_SetFarClip( void )
{
	float	farthestCornerDistance;
	int		i;

	// if not rendering the world (icons, menus, etc)
	// set a 2k far clip plane
	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		tr.viewParms.zFar = 2048;
		return;
	}

	//
	// set far clipping planes dynamically
	//
	farthestCornerDistance = 0;
	for ( i = 0; i < 8; i++ )
	{
		vec3_t v;
		vec3_t vecTo;
		float distance;

		v[0] = tr.viewParms.visBounds[(i>>0)&1][0];
		v[1] = tr.viewParms.visBounds[(i>>1)&1][1];
		v[2] = tr.viewParms.visBounds[(i>>2)&1][2];

		VectorSubtract( v, tr.viewParms.ort.origin, vecTo );

		distance = DotProduct( vecTo, vecTo );

		if ( distance > farthestCornerDistance )
		{
			farthestCornerDistance = distance;
		}
	}

	tr.viewParms.zFar = sqrt( farthestCornerDistance );
}


/*
=================
R_SetupFrustum

Set up the culling frustum planes for the current view using the results we got from computing the first two rows of
the projection matrix.
=================
*/
static void R_SetupFrustum( viewParms_t *dest, float xmin, float xmax, float ymax, float zProj, float stereoSep )
{
	vec3_t ofsorigin;
	float oppleg, adjleg, length;
	int i;
	
	if(stereoSep == 0 && xmin == -xmax)
	{
		// symmetric case can be simplified
		VectorCopy(dest->ort.origin, ofsorigin);

		length = sqrt(xmax * xmax + zProj * zProj);
		oppleg = xmax / length;
		adjleg = zProj / length;

		VectorScale(dest->ort.axis[0], oppleg, dest->frustum[0].normal);
		VectorMA(dest->frustum[0].normal, adjleg, dest->ort.axis[1], dest->frustum[0].normal);

		VectorScale(dest->ort.axis[0], oppleg, dest->frustum[1].normal);
		VectorMA(dest->frustum[1].normal, -adjleg, dest->ort.axis[1], dest->frustum[1].normal);
	}
	else
	{
		// In stereo rendering, due to the modification of the projection matrix, dest->ort.origin is not the
		// actual origin that we're rendering so offset the tip of the view pyramid.
		VectorMA(dest->ort.origin, stereoSep, dest->ort.axis[1], ofsorigin);
	
		oppleg = xmax + stereoSep;
		length = sqrt(oppleg * oppleg + zProj * zProj);
		VectorScale(dest->ort.axis[0], oppleg / length, dest->frustum[0].normal);
		VectorMA(dest->frustum[0].normal, zProj / length, dest->ort.axis[1], dest->frustum[0].normal);

		oppleg = xmin + stereoSep;
		length = sqrt(oppleg * oppleg + zProj * zProj);
		VectorScale(dest->ort.axis[0], -oppleg / length, dest->frustum[1].normal);
		VectorMA(dest->frustum[1].normal, -zProj / length, dest->ort.axis[1], dest->frustum[1].normal);
	}

	length = sqrt(ymax * ymax + zProj * zProj);
	oppleg = ymax / length;
	adjleg = zProj / length;

	VectorScale(dest->ort.axis[0], oppleg, dest->frustum[2].normal);
	VectorMA(dest->frustum[2].normal, adjleg, dest->ort.axis[2], dest->frustum[2].normal);

	VectorScale(dest->ort.axis[0], oppleg, dest->frustum[3].normal);
	VectorMA(dest->frustum[3].normal, -adjleg, dest->ort.axis[2], dest->frustum[3].normal);
	
	for (i=0 ; i<4 ; i++) {
		dest->frustum[i].type = PLANE_NON_AXIAL;
		dest->frustum[i].dist = DotProduct (ofsorigin, dest->frustum[i].normal);
		SetPlaneSignbits( &dest->frustum[i] );
	}

	// near clipping plane
	VectorCopy( dest->ort.axis[0], dest->frustum[4].normal );
	dest->frustum[4].type = PLANE_NON_AXIAL;
	dest->frustum[4].dist = DotProduct( ofsorigin, dest->frustum[4].normal ) + r_znear->value;
	SetPlaneSignbits( &dest->frustum[4] );
}


/*
===============
R_SetupProjection
===============
*/
void R_SetupProjection( viewParms_t *dest, float zProj, bool computeFrustum )
{
	R_SetupProjection_plus(dest, zProj, computeFrustum);
}

/*
===============
R_SetupProjectionZ

Sets the z-component transformation part in the projection matrix
===============
*/
static void R_SetupProjectionZ( viewParms_t *dest )
{
	const float zNear = r_znear->value;
	const float zFar = dest->zFar;
	const float depth = zFar - zNear;

	dest->projectionMatrix[2] = 0;
	dest->projectionMatrix[6] = 0;
#ifdef USE_REVERSED_DEPTH
	dest->projectionMatrix[10] = zNear / depth;
	dest->projectionMatrix[14] = zFar * zNear / depth;
#else
	dest->projectionMatrix[10] = - zFar / depth;
	dest->projectionMatrix[14] = - zFar * zNear / depth;
#endif
	if ( dest->portalView != PV_NONE )
	{
		float	plane[4];
		float	plane2[4];
		vec4_t q, c;

#ifdef USE_REVERSED_DEPTH
		dest->projectionMatrix[10] = - zFar / depth;
		dest->projectionMatrix[14] = - zFar * zNear / depth;
#endif

		// transform portal plane into camera space
		plane[0] = dest->portalPlane.normal[0];
		plane[1] = dest->portalPlane.normal[1];
		plane[2] = dest->portalPlane.normal[2];
		plane[3] = dest->portalPlane.dist;

		plane2[0] = -DotProduct( dest->ort.axis[1], plane );
		plane2[1] =  DotProduct( dest->ort.axis[2], plane );
		plane2[2] = -DotProduct( dest->ort.axis[0], plane );
		plane2[3] =  DotProduct( plane, dest->ort.origin) - plane[3];

		// Lengyel, Eric. "Modifying the Projection Matrix to Perform Oblique Near-plane Clipping".
		// Terathon Software 3D Graphics Library, 2004. http://www.terathon.com/code/oblique.html
		q[0] = (SGN(plane2[0]) + dest->projectionMatrix[8]) / dest->projectionMatrix[0];
		q[1] = (SGN(plane2[1]) + dest->projectionMatrix[9]) / dest->projectionMatrix[5];
		q[2] = -1.0f;
		q[3] = - dest->projectionMatrix[10] / dest->projectionMatrix[14];
		VectorScale4( plane2, 2.0f / DotProduct4(plane2, q), c );

		dest->projectionMatrix[2]  = c[0];
		dest->projectionMatrix[6]  = c[1];
		dest->projectionMatrix[10] = c[2];
		dest->projectionMatrix[14] = c[3];

#ifdef USE_REVERSED_DEPTH
		dest->projectionMatrix[2] = -dest->projectionMatrix[2];
		dest->projectionMatrix[6] = -dest->projectionMatrix[6];
		dest->projectionMatrix[10] = -(dest->projectionMatrix[10] + 1.0);
		dest->projectionMatrix[14] = -dest->projectionMatrix[14];
#endif
	}
}


/*
=================
R_MirrorPoint
=================
*/
static void R_MirrorPoint( const vec3_t in, const orientation_t *surface, const orientation_t *camera, vec3_t out ) {
	int		i;
	vec3_t	local;
	vec3_t	transformed;
	float	d;

	VectorSubtract( in, surface->origin, local );

	VectorClear( transformed );
	for ( i = 0 ; i < 3 ; i++ ) {
		d = DotProduct( local, surface->axis[i] );
		VectorMA( transformed, d, camera->axis[i], transformed );
	}

	VectorAdd( transformed, camera->origin, out );
}


static void R_MirrorVector( const vec3_t in, const orientation_t *surface, const orientation_t *camera, vec3_t out ) {
	int		i;
	float	d;

	VectorClear( out );
	for ( i = 0 ; i < 3 ; i++ ) {
		d = DotProduct(in, surface->axis[i]);
		VectorMA( out, d, camera->axis[i], out );
	}
}


/*
=============
R_PlaneForSurface
=============
*/
static void R_PlaneForSurface( const surfaceType_t *surfType, cplane_t *plane ) {
	srfTriangles_t	*tri;
	srfPoly_t		*poly;
	drawVert_t		*v1, *v2, *v3;
	vec4_t			plane4;

	if (!surfType) {
		Com_Memset (plane, 0, sizeof(*plane));
		plane->normal[0] = 1;
		return;
	}
	switch (*surfType) {
	case SF_FACE:
		*plane = ((srfSurfaceFace_t *)surfType)->plane;
		return;
	case SF_TRIANGLES:
		tri = (srfTriangles_t *)surfType;
		v1 = tri->verts + tri->indexes[0];
		v2 = tri->verts + tri->indexes[1];
		v3 = tri->verts + tri->indexes[2];
		PlaneFromPoints( plane4, v1->xyz, v2->xyz, v3->xyz );
		VectorCopy( plane4, plane->normal ); 
		plane->dist = plane4[3];
		return;
	case SF_POLY:
		poly = (srfPoly_t *)surfType;
		PlaneFromPoints( plane4, poly->verts[0].xyz, poly->verts[1].xyz, poly->verts[2].xyz );
		VectorCopy( plane4, plane->normal ); 
		plane->dist = plane4[3];
		return;
	default:
		Com_Memset (plane, 0, sizeof(*plane));
		plane->normal[0] = 1;
		return;
	}
}


/*
=================
R_GetPortalOrientation

entityNum is the entity that the portal surface is a part of, which may
be moving and rotating.

Returns true if it should be mirrored
=================
*/
static bool R_GetPortalOrientations( const drawSurf_t *drawSurf, int entityNum,
							 orientation_t *surface, orientation_t *camera,
							 vec3_t pvsOrigin, portalView_t *portalView ) {
	int			i;
	cplane_t	originalPlane, plane;
	trRefEntity_t	*e;
	float		d;
	vec3_t		transformed;

	// create plane axis for the portal we are seeing
	R_PlaneForSurface( drawSurf->surface, &originalPlane );

	// rotate the plane if necessary
	if ( entityNum != REFENTITYNUM_WORLD ) {
		tr.currentEntityNum = entityNum;
		tr.currentEntity = &tr.refdef.entities[entityNum];

		// get the orientation of the entity
		R_RotateForEntity( tr.currentEntity, &tr.viewParms, &tr.ort );

		// rotate the plane, but keep the non-rotated version for matching
		// against the portalSurface entities
		R_LocalNormalToWorld( originalPlane.normal, plane.normal );
		plane.dist = originalPlane.dist + DotProduct( plane.normal, tr.ort.origin );

		// translate the original plane
		originalPlane.dist = originalPlane.dist + DotProduct( originalPlane.normal, tr.ort.origin );
	} else {
		plane = originalPlane;
	}

	VectorCopy( plane.normal, surface->axis[0] );
	PerpendicularVector( surface->axis[1], surface->axis[0] );
	CrossProduct( surface->axis[0], surface->axis[1], surface->axis[2] );

	// locate the portal entity closest to this plane.
	// origin will be the origin of the portal, origin2 will be
	// the origin of the camera
	for ( i = 0 ; i < tr.refdef.num_entities ; i++ ) {
		e = &tr.refdef.entities[i];
		if ( e->e.reType != RT_PORTALSURFACE ) {
			continue;
		}

		d = DotProduct( e->e.origin, originalPlane.normal ) - originalPlane.dist;
		if ( d > 64 || d < -64) {
			continue;
		}

		// get the pvsOrigin from the entity
		VectorCopy( e->e.oldorigin, pvsOrigin );

		// if the entity is just a mirror, don't use as a camera point
		if ( e->e.oldorigin[0] == e->e.origin[0] && 
			e->e.oldorigin[1] == e->e.origin[1] && 
			e->e.oldorigin[2] == e->e.origin[2] ) {
			VectorScale( plane.normal, plane.dist, surface->origin );
			VectorCopy( surface->origin, camera->origin );
			VectorSubtract( vec3_origin, surface->axis[0], camera->axis[0] );
			VectorCopy( surface->axis[1], camera->axis[1] );
			VectorCopy( surface->axis[2], camera->axis[2] );

			*portalView = PV_MIRROR;
			return true;
		}

		// project the origin onto the surface plane to get
		// an origin point we can rotate around
		d = DotProduct( e->e.origin, plane.normal ) - plane.dist;
		VectorMA( e->e.origin, -d, surface->axis[0], surface->origin );
			
		// now get the camera origin and orientation
		VectorCopy( e->e.oldorigin, camera->origin );
		AxisCopy( e->e.axis, camera->axis );
		VectorSubtract( vec3_origin, camera->axis[0], camera->axis[0] );
		VectorSubtract( vec3_origin, camera->axis[1], camera->axis[1] );

		// optionally rotate
		if ( e->e.oldframe ) {
			// if a speed is specified
			if ( e->e.frame ) {
				// continuous rotate
				d = (tr.refdef.time/1000.0f) * e->e.frame;
				VectorCopy( camera->axis[1], transformed );
				RotatePointAroundVector( camera->axis[1], camera->axis[0], transformed, d );
				CrossProduct( camera->axis[0], camera->axis[1], camera->axis[2] );
			} else {
				// bobbing rotate, with skinNum being the rotation offset
				d = sin( tr.refdef.time * 0.003f );
				d = e->e.skinNum + d * 4;
				VectorCopy( camera->axis[1], transformed );
				RotatePointAroundVector( camera->axis[1], camera->axis[0], transformed, d );
				CrossProduct( camera->axis[0], camera->axis[1], camera->axis[2] );
			}
		}
		else if ( e->e.skinNum ) {
			d = e->e.skinNum;
			VectorCopy( camera->axis[1], transformed );
			RotatePointAroundVector( camera->axis[1], camera->axis[0], transformed, d );
			CrossProduct( camera->axis[0], camera->axis[1], camera->axis[2] );
		}

		*portalView = PV_PORTAL;
		return true;
	}

	// if we didn't locate a portal entity, don't render anything.
	// We don't want to just treat it as a mirror, because without a
	// portal entity the server won't have communicated a proper entity set
	// in the snapshot

	// unfortunately, with local movement prediction it is easily possible
	// to see a surface before the server has communicated the matching
	// portal surface entity, so we don't want to print anything here...

	//ri.Printf( PRINT_ALL, "Portal surface without a portal entity\n" );

	return false;
}


static bool IsMirror( const drawSurf_t *drawSurf, int entityNum )
{
	int			i;
	cplane_t	originalPlane, plane;
	trRefEntity_t	*e;
	float		d;

	// create plane axis for the portal we are seeing
	R_PlaneForSurface( drawSurf->surface, &originalPlane );

	// rotate the plane if necessary
	if ( entityNum != REFENTITYNUM_WORLD )
	{
		tr.currentEntityNum = entityNum;
		tr.currentEntity = &tr.refdef.entities[entityNum];

		// get the orientation of the entity
		R_RotateForEntity( tr.currentEntity, &tr.viewParms, &tr.ort );

		// rotate the plane, but keep the non-rotated version for matching
		// against the portalSurface entities
		R_LocalNormalToWorld( originalPlane.normal, plane.normal );
		plane.dist = originalPlane.dist + DotProduct( plane.normal, tr.ort.origin );

		// translate the original plane
		originalPlane.dist = originalPlane.dist + DotProduct( originalPlane.normal, tr.ort.origin );
	}
	else
	{
		plane = originalPlane;
	}

	// locate the portal entity closest to this plane.
	// origin will be the origin of the portal, origin2 will be
	// the origin of the camera
	for ( i = 0 ; i < tr.refdef.num_entities ; i++ ) 
	{
		e = &tr.refdef.entities[i];
		if ( e->e.reType != RT_PORTALSURFACE ) {
			continue;
		}

		d = DotProduct( e->e.origin, originalPlane.normal ) - originalPlane.dist;
		if ( d > 64 || d < -64) {
			continue;
		}

		// if the entity is just a mirror, don't use as a camera point
		if ( e->e.oldorigin[0] == e->e.origin[0] && 
			e->e.oldorigin[1] == e->e.origin[1] && 
			e->e.oldorigin[2] == e->e.origin[2] ) 
		{
			return true;
		}

		return false;
	}
	return false;
}


/*
** SurfIsOffscreen
**
** Determines if a surface is completely offscreen.
*/
static bool SurfIsOffscreen( const drawSurf_t *drawSurf, bool *isMirror ) {
	float shortest = 100000000;
	int entityNum;
	int numTriangles;
	shader_t *shader;
	int		fogNum;
	int dlighted;
	vec4_t clip, eye;
	int i;
	unsigned int pointAnd = (unsigned int)~0;

	*isMirror = false;

	R_RotateForViewer();

	R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted );
	RB_BeginSurface_plus( shader, fogNum );
#ifdef USE_VBO
	tess.allowVBO = false;
#endif
#ifdef USE_TESS_NEEDS_NORMAL
	tess.needsNormal = true;
#endif
	rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );

	for ( i = 0; i < tess.numVertexes; i++ )
	{
		int j;
		unsigned int pointFlags = 0;

		R_TransformModelToClip( tess.xyz[i], tr.ort.modelMatrix, tr.viewParms.projectionMatrix, eye, clip );

		for ( j = 0; j < 3; j++ )
		{
			if ( clip[j] >= clip[3] )
			{
				pointFlags |= (1 << (j*2));
			}
			else if ( clip[j] <= -clip[3] )
			{
				pointFlags |= ( 1 << (j*2+1));
			}
		}
		pointAnd &= pointFlags;
	}

	// trivially reject
	if ( pointAnd )
	{
		tess.numIndexes = 0;
		return true;
	}

	// determine if this surface is backfaced and also determine the distance
	// to the nearest vertex so we can cull based on portal range.  Culling
	// based on vertex distance isn't 100% correct (we should be checking for
	// range to the surface), but it's good enough for the types of portals
	// we have in the game right now.
	numTriangles = tess.numIndexes / 3;

	for ( i = 0; i < tess.numIndexes; i += 3 )
	{
		vec3_t normal;
		float len;

		VectorSubtract( tess.xyz[tess.indexes[i]], tr.viewParms.ort.origin, normal );

		len = VectorLengthSquared( normal );			// lose the sqrt
		if ( len < shortest )
		{
			shortest = len;
		}

		if ( DotProduct( normal, tess.normal[tess.indexes[i]] ) >= 0 )
		{
			numTriangles--;
		}
	}
	tess.numIndexes = 0;
	if ( !numTriangles )
	{
		return true;
	}

	// mirrors can early out at this point, since we don't do a fade over distance
	// with them (although we could)
	if ( IsMirror( drawSurf, entityNum ) )
	{
		*isMirror = true;
		return false;
	}

	if ( shortest > (tess.shader->portalRange*tess.shader->portalRange) )
	{
		return true;
	}

	return false;
}


/*
================
R_GetModelViewBounds
================
*/
static void R_GetModelViewBounds( int *mins, int *maxs )
{
	float			minn[2];
	float			maxn[2];
	float			norm[2];
	float			mvp[16];
	float			dist[4];
	vec4_t			clip;
	int				i,j;

	minn[0] = minn[1] =  1.0;
	maxn[0] = maxn[1] = -1.0;

	// premultiply
	myGlMultMatrix( tr.ort.modelMatrix, tr.viewParms.projectionMatrix, mvp );

	for ( i = 0; i < tess.numVertexes; i++ ) {
		R_TransformModelToClipMVP( tess.xyz[i], mvp, clip );
		if ( clip[3] <= 0.0 ) {
			dist[0] = DotProduct( tess.xyz[i], tr.viewParms.frustum[0].normal ) - tr.viewParms.frustum[0].dist; // right
			dist[1] = DotProduct( tess.xyz[i], tr.viewParms.frustum[1].normal ) - tr.viewParms.frustum[1].dist; // left
			dist[2] = DotProduct( tess.xyz[i], tr.viewParms.frustum[2].normal ) - tr.viewParms.frustum[2].dist; // bottom
			dist[3] = DotProduct( tess.xyz[i], tr.viewParms.frustum[3].normal ) - tr.viewParms.frustum[3].dist; // top
			if ( dist[0] <= 0 && dist[1] <= 0 ) {
				if ( dist[0] < dist[1] ) {
					maxn[0] =  1.0f;
				} else {
					minn[0] = -1.0f;
				}
			} else {
				if ( dist[0] <= 0 ) maxn[0] =  1.0f;
				if ( dist[1] <= 0 ) minn[0] = -1.0f;
			}
			if ( dist[2] <= 0 && dist[3] <= 0 ) {
				if ( dist[2] < dist[3] )
					minn[1] = -1.0f;
				else
					maxn[1] =  1.0f;
			} else {
				if ( dist[2] <= 0 ) minn[1] = -1.0f;
				if ( dist[3] <= 0 ) maxn[1] =  1.0f;
			}
		} else {
			for ( j = 0; j < 2; j++ ) {
				if ( clip[j] >  clip[3] ) clip[j] =  clip[3]; else
				if ( clip[j] < -clip[3] ) clip[j] = -clip[3];
			}
			norm[0] = clip[0] / clip[3];
			norm[1] = clip[1] / clip[3];
			for ( j = 0; j < 2; j++ ) {
				if ( norm[j] < minn[j] ) minn[j] = norm[j];
				if ( norm[j] > maxn[j] ) maxn[j] = norm[j];
			}
		}
	}

	mins[0] = (int)(-0.5 + 0.5 * ( 1.0 + minn[0] ) * tr.viewParms.viewportWidth);
	mins[1] = (int)(-0.5 + 0.5 * ( 1.0 + minn[1] ) * tr.viewParms.viewportHeight);
	maxs[0] = (int)(0.5 + 0.5 * ( 1.0 + maxn[0] ) * tr.viewParms.viewportWidth);
	maxs[1] = (int)(0.5 + 0.5 * ( 1.0 + maxn[1] ) * tr.viewParms.viewportHeight);
}


/*
========================
R_MirrorViewBySurface

Returns true if another view has been rendered
========================
*/
extern int r_numdlights;
static bool R_MirrorViewBySurface( const drawSurf_t *drawSurf, int entityNum ) {
	viewParms_t		newParms;
	viewParms_t		oldParms;
	orientation_t	surface, camera;
	bool		isMirror;

	// don't recursively mirror
	if ( tr.viewParms.portalView != PV_NONE ) {
		ri.Printf( PRINT_DEVELOPER, "WARNING: recursive mirror/portal found\n" );
		return false;
	}

	if ( r_noportals->integer > 1 /*|| r_fastsky->integer == 1 */ ) {
		return false;
	}

	// trivially reject portal/mirror
	if ( SurfIsOffscreen( drawSurf, &isMirror ) ) {
		return false;
	}

	if ( !isMirror && r_noportals->integer ) {
		return false;
	}

	// save old viewParms so we can return to it after the mirror view
	oldParms = tr.viewParms;

	newParms = tr.viewParms;
	newParms.portalView = PV_NONE;

	if ( !R_GetPortalOrientations( drawSurf, entityNum, &surface, &camera, 
		newParms.pvsOrigin, &newParms.portalView ) ) {
		return false;		// bad portal, no portalentity
	}

#ifdef USE_PMLIGHT
	// create dedicated set for each view
	if ( r_numdlights + oldParms.num_dlights <= ARRAY_LEN( backEndData->dlights ) ) {
		int i;
		newParms.dlights = oldParms.dlights + oldParms.num_dlights;
		newParms.num_dlights = oldParms.num_dlights;
		r_numdlights += oldParms.num_dlights;
		for ( i = 0; i < oldParms.num_dlights; i++ )
			newParms.dlights[i] = oldParms.dlights[i];
	}
#endif

	if ( tess.numVertexes > 2 && r_fastsky->integer && vk.fastSky ) {
		int mins[2], maxs[2];
		R_GetModelViewBounds( mins, maxs );
		newParms.scissorX = newParms.viewportX + mins[0];
		newParms.scissorY = newParms.viewportY + mins[1];
		newParms.scissorWidth = maxs[0] - mins[0];
		newParms.scissorHeight = maxs[1] - mins[1];
	}

	R_MirrorPoint( oldParms.ort.origin, &surface, &camera, newParms.ort.origin );

	VectorSubtract( vec3_origin, camera.axis[0], newParms.portalPlane.normal );
	newParms.portalPlane.dist = DotProduct( camera.origin, newParms.portalPlane.normal );

	R_MirrorVector (oldParms.ort.axis[0], &surface, &camera, newParms.ort.axis[0]);
	R_MirrorVector (oldParms.ort.axis[1], &surface, &camera, newParms.ort.axis[1]);
	R_MirrorVector (oldParms.ort.axis[2], &surface, &camera, newParms.ort.axis[2]);

	// OPTIMIZE: restrict the viewport on the mirrored view

	// render the mirror view
	R_RenderView( &newParms );

	tr.viewParms = oldParms;

	return true;
}


/*
=================
R_SpriteFogNum

See if a sprite is inside a fog volume
=================
*/
static int R_SpriteFogNum( const trRefEntity_t *ent ) {
	int				i, j;
	const fog_t		*fog;

	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return 0;
	}

	if ( ent->e.renderfx & RF_CROSSHAIR ) {
		return 0;
	}

	for ( i = 1 ; i < tr.world->numfogs ; i++ ) {
		fog = &tr.world->fogs[i];
		for ( j = 0 ; j < 3 ; j++ ) {
			if ( ent->e.origin[j] - ent->e.radius >= fog->bounds[1][j] ) {
				break;
			}
			if ( ent->e.origin[j] + ent->e.radius <= fog->bounds[0][j] ) {
				break;
			}
		}
		if ( j == 3 ) {
			return i;
		}
	}

	return 0;
}

/*
==========================================================================================

DRAWSURF SORTING

==========================================================================================
*/

/*
===============
R_Radix
===============
*/
static ID_INLINE void R_Radix(int byte, int size, const drawSurf_t *source, drawSurf_t *dest)
{
    int count[256] = {0};
    int index[256] = {0};
    int i;
    const unsigned char *sortKey = (const unsigned char *)&source[0].sort + byte;
    const unsigned char *end = sortKey + (size * sizeof(drawSurf_t));

    // Count occurrences of each byte value
    for (; sortKey < end; sortKey += sizeof(drawSurf_t))
        ++count[*sortKey];

    // Calculate starting index for each byte value
    index[0] = 0;
    for (i = 1; i < 256; ++i)
        index[i] = index[i - 1] + count[i - 1];

    // Move elements to their sorted positions
    sortKey = (const unsigned char *)&source[0].sort + byte;
    for (i = 0; i < size; ++i, sortKey += sizeof(drawSurf_t))
        dest[index[*sortKey]++] = source[i];
}


/*
===============
R_RadixSort

Radix sort with 4 byte size buckets
===============
*/
static void R_RadixSort( drawSurf_t *source, int size )
{
  static drawSurf_t scratch[ MAX_DRAWSURFS ];
#ifdef Q3_LITTLE_ENDIAN
  R_Radix( 0, size, source, scratch );
  R_Radix( 1, size, scratch, source );
  R_Radix( 2, size, source, scratch );
  R_Radix( 3, size, scratch, source );
#else
  R_Radix( 3, size, source, scratch );
  R_Radix( 2, size, scratch, source );
  R_Radix( 1, size, source, scratch );
  R_Radix( 0, size, scratch, source );
#endif //Q3_LITTLE_ENDIAN
}


#ifdef USE_PMLIGHT
// comes from hpp file of c++ lib
// typedef struct litSurf_tape_s {
// 	struct litSurf_s *first;
// 	struct litSurf_s *last;
// 	unsigned count;
// } litSurf_tape_t;

// Philip Erdelsky gets all the credit for this one...

static void R_SortLitsurfs( dlight_t* dl )
{
	litSurf_tape_t tape[ 4 ];
	int				base;
	litSurf_t		*p;
	litSurf_t		*next;
	unsigned		block_size;
	litSurf_tape_t	*tape0;
	litSurf_tape_t	*tape1;
	int				dest;
	litSurf_tape_t	*output_tape;
	litSurf_tape_t	*chosen_tape;
	unsigned		n0, n1;

	// distribute the records alternately to tape[0] and tape[1]

	tape[0].count = tape[1].count = 0;
	tape[0].first = tape[1].first = NULL;

	base = 0;
	p = dl->head;

	while ( p ) {
		next = p->next;
		p->next = tape[base].first;
		tape[base].first = p;
		tape[base].count++;
		p = next;
		base ^= 1;
	}

	// merge from the two active tapes into the two idle ones
	// doubling the number of records and pingponging the tape sets as we go

	block_size = 1;
	for ( base = 0; tape[base+1].count; base ^= 2, block_size <<= 1 )
	{
		tape0 = tape + base;
		tape1 = tape + base + 1;
		dest = base ^ 2;

		tape[dest].count = tape[dest+1].count = 0;
		for (; tape0->count; dest ^= 1)
		{
			output_tape = tape + dest;
			n0 = n1 = block_size;

			while (1)
			{
				if (n0 == 0 || tape0->count == 0)
				{
					if (n1 == 0 || tape1->count == 0)
						break;
					chosen_tape = tape1;
					n1--;
				}
				else if (n1 == 0 || tape1->count == 0)
				{
					chosen_tape = tape0;
					n0--;
				}
				else if (tape0->first->sort > tape1->first->sort)
				{
					chosen_tape = tape1;
					n1--;
				}
				else
				{
					chosen_tape = tape0;
					n0--;
				}
				chosen_tape->count--;
				p = chosen_tape->first;
				chosen_tape->first = p->next;
				if (output_tape->count == 0)
					output_tape->first = p;
				else
					output_tape->last->next = p;
				output_tape->last = p;
				output_tape->count++;
			}
		}
	}

	if (tape[base].count > 1)
		tape[base].last->next = NULL;

	dl->head = tape[base].first;
}


/*
=================
R_AddLitSurf
=================
*/
void R_AddLitSurf( surfaceType_t *surface, shader_t *shader, int fogIndex )
{
	R_AddLitSurf_plus(surface, shader, fogIndex);
}


/*
=================
R_DecomposeLitSort
=================
*/
void R_DecomposeLitSort( unsigned sort, int *entityNum, shader_t **shader, int *fogNum ) {
	R_DecomposeLitSort_plus(sort, entityNum, shader, fogNum);
}

#endif // USE_PMLIGHT


//==========================================================================================

/*
=================
R_AddDrawSurf
=================
*/
void R_AddDrawSurf( surfaceType_t *surface, shader_t *shader, 
				   int fogIndex, int dlightMap ) {
R_AddDrawSurf_plus(surface, shader, fogIndex, dlightMap);
}


/*
=================
R_DecomposeSort
=================
*/
void R_DecomposeSort( unsigned sort, int *entityNum, shader_t **shader, 
					 int *fogNum, int *dlightMap ) {
	R_DecomposeSort_plus(sort, entityNum, shader, fogNum, dlightMap);
}


/*
=================
R_SortDrawSurfs
=================
*/
static void R_SortDrawSurfs( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	shader_t		*shader;
	int				fogNum;
	int				entityNum;
	int				dlighted;
	int				i;

	// it is possible for some views to not have any surfaces
	if ( numDrawSurfs < 1 ) {
		// we still need to add it for hyperspace cases
		R_AddDrawSurfCmd( drawSurfs, numDrawSurfs );
		return;
	}

	// sort the drawsurfs by sort type, then orientation, then shader
	R_RadixSort( drawSurfs, numDrawSurfs );

	// check for any pass through drawing, which
	// may cause another view to be rendered first
	for ( i = 0 ; i < numDrawSurfs ; i++ ) {
		R_DecomposeSort( (drawSurfs+i)->sort, &entityNum, &shader, &fogNum, &dlighted );

		if ( shader->sort > SS_PORTAL ) {
			break;
		}

		// no shader should ever have this sort type
		if ( shader->sort == SS_BAD ) {
			ri.Error (ERR_DROP, "Shader '%s'with sort == SS_BAD", shader->name );
		}

		// if the mirror was completely clipped away, we may need to check another surface
		if ( R_MirrorViewBySurface( (drawSurfs+i), entityNum) ) {
			// this is a debug option to see exactly what is being mirrored
			if ( r_portalOnly->integer ) {
				return;
			}
			if ( r_fastsky->integer == 0 || !vk.fastSky ) {
				break;	// only one mirror view at a time
			}
		}
	}

#ifdef USE_PMLIGHT
#ifdef USE_LEGACY_DLIGHTS
	if ( r_dlightMode->integer ) 
#endif
	{
		dlight_t *dl;
		// all the lit surfaces are in a single queue
		// but each light's surfaces are sorted within its subsection
		for ( i = 0; i < tr.refdef.num_dlights; ++i ) { 
			dl = &tr.refdef.dlights[ i ];
			if ( dl->head ) {
				R_SortLitsurfs( dl );
			}
		}
	}
#endif // USE_PMLIGHT

	R_AddDrawSurfCmd( drawSurfs, numDrawSurfs );
}


/*
=============
R_AddEntitySurfaces
=============
*/
static void R_AddEntitySurfaces( void ) {
	trRefEntity_t	*ent;
	shader_t		*shader;

	if ( !r_drawentities->integer ) {
		return;
	}

	for ( tr.currentEntityNum = 0;
			tr.currentEntityNum < tr.refdef.num_entities;
			tr.currentEntityNum++ ) {
		ent = tr.currentEntity = &tr.refdef.entities[tr.currentEntityNum];
#ifdef USE_LEGACY_DLIGHTS
		ent->needDlights = 0;
#endif
		// preshift the value we are going to OR into the drawsurf sort
		tr.shiftedEntityNum = tr.currentEntityNum << QSORT_REFENTITYNUM_SHIFT;

		//
		// the weapon model must be handled special --
		// we don't want the hacked first person weapon position showing in 
		// mirrors, because the true body position will already be drawn
		//
		if ( (ent->e.renderfx & RF_FIRST_PERSON) && (tr.viewParms.portalView != PV_NONE) ) {
			continue;
		}

		// simple generated models, like sprites and beams, are not culled
		switch ( ent->e.reType ) {
		case RT_PORTALSURFACE:
			break;		// don't draw anything
		case RT_SPRITE:
		case RT_BEAM:
		case RT_LIGHTNING:
		case RT_RAIL_CORE:
		case RT_RAIL_RINGS:
			// self blood sprites, talk balloons, etc should not be drawn in the primary
			// view.  We can't just do this check for all entities, because md3
			// entities may still want to cast shadows from them
			if ( (ent->e.renderfx & RF_THIRD_PERSON) && (tr.viewParms.portalView == PV_NONE) ) {
				continue;
			}
			shader = R_GetShaderByHandle( ent->e.customShader );
			R_AddDrawSurf( &entitySurface, shader, R_SpriteFogNum( ent ), 0 );
			break;

		case RT_MODEL:
			// we must set up parts of tr.ort for model culling
			R_RotateForEntity( ent, &tr.viewParms, &tr.ort );

			tr.currentModel = R_GetModelByHandle( ent->e.hModel );
			if (!tr.currentModel) {
				R_AddDrawSurf( &entitySurface, tr.defaultShader, 0, 0 );
			} else {
				switch ( tr.currentModel->type ) {
				case MOD_MESH:
					R_AddMD3Surfaces( ent );
					break;
				case MOD_MDR:
					R_MDRAddAnimSurfaces( ent );
					break;
				case MOD_IQM:
					R_AddIQMSurfaces( ent );
					break;
				case MOD_BRUSH:
					R_AddBrushModelSurfaces( ent );
					break;
				case MOD_BAD:		// null model axis
					if ( (ent->e.renderfx & RF_THIRD_PERSON) && (tr.viewParms.portalView == PV_NONE) ) {
						break;
					}
					R_AddDrawSurf( &entitySurface, tr.defaultShader, 0, 0 );
					break;
				default:
					ri.Error( ERR_DROP, "R_AddEntitySurfaces: Bad modeltype" );
					break;
				}
			}
			break;
		default:
			ri.Error( ERR_DROP, "R_AddEntitySurfaces: Bad reType" );
		}
	}

}


/*
====================
R_GenerateDrawSurfs
====================
*/
static void R_GenerateDrawSurfs( void ) {
	R_AddWorldSurfaces ();

	R_AddPolygonSurfaces();

	// set the projection matrix with the minimum zfar
	// now that we have the world bounded
	// this needs to be done before entities are
	// added, because they use the projection
	// matrix for lod calculation

	// dynamically compute far clip plane distance
	R_SetFarClip();

	// we know the size of the clipping volume. Now set the rest of the projection matrix.
	R_SetupProjectionZ( &tr.viewParms );

	R_AddEntitySurfaces();
}


/*
================
R_RenderView

A view may be either the actual camera view,
ort a mirror / remote location
================
*/
void R_RenderView( const viewParms_t *parms ) {
	int		firstDrawSurf;
	int		numDrawSurfs;

	if ( parms->viewportWidth <= 0 || parms->viewportHeight <= 0 ) {
		return;
	}

	tr.viewCount++;

	tr.viewParms = *parms;
	tr.viewParms.frameSceneNum = tr.frameSceneNum;
	tr.viewParms.frameCount = tr.frameCount;

	firstDrawSurf = tr.refdef.numDrawSurfs;

	// set viewParms.world
	R_RotateForViewer();

	R_SetupProjection( &tr.viewParms, r_zproj->value, true );

	R_GenerateDrawSurfs();

	// if we overflowed MAX_DRAWSURFS, the drawsurfs
	// wrapped around in the buffer and we will be missing
	// the first surfaces, not the last ones
	numDrawSurfs = tr.refdef.numDrawSurfs;
	if ( numDrawSurfs > MAX_DRAWSURFS ) {
		numDrawSurfs = MAX_DRAWSURFS;
	}

	R_SortDrawSurfs( tr.refdef.drawSurfs + firstDrawSurf, numDrawSurfs - firstDrawSurf );
}
