/*
	c3geometry.h

	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>

 	This file is part of libc3.

	libc3 is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	libc3 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with libc3.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * c3geometry is a structure containing one set of vertices and various
 * bits related to it. Ultimately it contains a pre-cached projected
 * version of the vertices that the drawing code can use directly.
 * c3geometry is aways attached to a c3object as a parent.
 */

#ifndef __C3GEOMETRY_H___
#define __C3GEOMETRY_H___

#include "c3types.h"
#include "c_utils.h"

#if __cplusplus
extern "C" {
#endif

struct c3object_t;
struct c3pixels_t;
struct c3program_t;

/*
 * Allow per-array storage of an extra buffer object
 * if 'mutable' is not set (default), the array can be clear()ed after
 * the buffer is bound.
 * If the array is mutable, setting the 'dirty' flag will signal
 * the rendering layer that it needs to update the buffer object
 */
typedef struct c3geometry_buffer_t {
	c3apiobject_t bid;	// buffer object
	void * refCon;		// reference constant for application use
	int mutable : 1, dirty : 1;
} c3geometry_buffer_t, * c3geometry_buffer_p;

DECLARE_C_ARRAY(c3vertex_t, c3vertex_array, 16, c3geometry_buffer_t buffer);
DECLARE_C_ARRAY(c3tex_t, c3tex_array, 16, c3geometry_buffer_t buffer);
DECLARE_C_ARRAY(c3colorf_t, c3colorf_array, 16, c3geometry_buffer_t buffer);
DECLARE_C_ARRAY(c3index_t, c3indices_array, 16, c3geometry_buffer_t buffer);

//! Geometry material.
typedef struct c3material_t {
	c3colorf_t	color;
	c3f shininess;
	struct c3pixels_t * texture;
	struct c3program_t * program;
	struct {
		uint32_t src, dst;
	} blend;
} c3material_t;

#define C3_TYPE(_a,_b,_c,_d) \
	(((uint32_t)(_a)<<24)|((uint32_t)(_b)<<16)|((uint32_t)(_c)<<8)|(_d))

//! Generic geometry type
enum {
	C3_RAW_TYPE = C3_TYPE('r','a','w','g'),
	C3_TRIANGLE_TYPE = C3_TYPE('t','r','i','a'),
};

/*!
 * geometry type.
 * The type is used as non-opengl description of what the geometry
 * contains, like "texture", and the subtype can be used to store the
 * real format of the vertices. like GL_LINES etc
 */
typedef struct c3geometry_type_t {
	uint32_t type;			// C3_RAW_TYPE etc
	c3apiobject_t subtype;	// GL_LINES etc
} c3geometry_type_t;

/*!
 * Geometry object. Describes a set of vertices, texture coordinates,
 * normals, colors and material
 * The projection is not set here, a geometry is always attached to a
 * c3object that has the projection
 */
typedef struct c3geometry_t {
	c3geometry_type_t	type;	// geometry type
	int					dirty : 1,
						debug : 1,
						custom : 1,		// has a custom driver
						hidden : 8;		// hidden from context_view, bitfield
	str_p 				name;	// optional
	c3apiobject_t 		bid;	// buffer id for opengl

	c3material_t		mat;
	struct c3object_t * object;	// parent object
	const struct c3driver_geometry_t ** driver;

	c3bbox_t			bbox;		// bounding box
	c3bbox_t			wbbox;		// world aligned bounding box
	c3vertex_array_t 	vertice;
	c3tex_array_t		textures;	// optional: texture coordinates
	c3vertex_array_t 	normals;	// optional: vertex normals
	c3colorf_array_t	colorf;		// optional: vertex colors
	c3indices_array_t	indices;	// optional: vertex indices

	/*
	 * Some shared attributes
	 */
	union {
		struct {
			float width;
		} line;
	};
} c3geometry_t, *c3geometry_p;

DECLARE_C_ARRAY(c3geometry_p, c3geometry_array, 4);

//! Allocates a new geometry, init it, and attached it to parent 'o' (optional)
c3geometry_p
c3geometry_new(
		c3geometry_type_t type,
		struct c3object_t * o /* = NULL */);
//! Init an existing new geometry, and attached it to parent 'o' (optional)
c3geometry_p
c3geometry_init(
		c3geometry_p g,
		c3geometry_type_t type,
		struct c3object_t * o /* = NULL */);
//! Disposes (via the driver interface) the geometry
void
c3geometry_dispose(
		c3geometry_p g);

//! Prepares a geometry. 
/*!
 * The project phase is called only when the container object is 'dirty'
 * for example if it's projection has changed.
 * The project call is responsible for reprojecting the geometry and that
 * sort of things
 */
void
c3geometry_project(
		c3geometry_p g,
		c3mat4p m);

//! Draw the geometry
/*
 * Called when drawing the context. Typicaly this calls the geometry 
 * driver, which in turn will call the 'context' draw method, and the 
 * application to draw this particular geometry
 */
void
c3geometry_draw(
		c3geometry_p g );

//! Sets or clear geometry dirty bit
/*
 * This will dirty parent objects if set to 1
 */
void
c3geometry_set_dirty(
		c3geometry_p g,
		int dirty );

/*
 * if not present, create an index array, and collapses
 * the vertex array by removing vertices that are within
 * 'tolerance' (as a distance between vertices considered equals).
 * If the normals exists, they are also compared
 * to normaltolerance (in radian) and if both position and normals
 * are within tolerance, the vertices are collapsed and the normals
 * are averaged.
 * This code allows smooth rendering of STL files generated by
 * CAD programs that generate only triangle normals.
 */
void
c3geometry_factor(
		c3geometry_p g,
		c3f tolerance,
		c3f normaltolerance);

//! allocate (if not there) and return a custom driver for this geometry
/*!
 * Geometries come with a default, read only driver stack.. It is a constant
 * global to save memory for each of the 'generic' object.
 * This call will duplicate that stack and allocate (if not there) a read/write
 * empty driver that the application can use to put their own, per object,
 * callback. For example you can add your own project() or draw() function
 * and have it called first
 */
struct c3driver_geometry_t *
c3geometry_get_custom(
		c3geometry_p g );

IMPLEMENT_C_ARRAY(c3geometry_array);
IMPLEMENT_C_ARRAY(c3vertex_array);
IMPLEMENT_C_ARRAY(c3tex_array);
IMPLEMENT_C_ARRAY(c3colorf_array);
IMPLEMENT_C_ARRAY(c3indices_array);

static inline c3geometry_type_t
c3geometry_type(uint32_t type, int subtype)
{
	// older gcc <4.6 doesn't like this
	c3geometry_type_t r;// = { .type = type, .subtype = subtype };
	r.type = type; r.subtype = C3APIO(subtype);
	return r;
}

#if __cplusplus
}
#endif

#endif /* __C3GEOMETRY_H___ */
