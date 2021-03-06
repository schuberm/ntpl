/* spglib.c */
/* Copyright (C) 2008 Atsushi Togo */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spglib.h"
#include "refinement.h"
#include "cell.h"
#include "mathfunc.h"
#include "pointgroup.h"
#include "primitive.h"
#include "spacegroup.h"
#include "symmetry.h"
#include "kpoint.h"

SpglibDataset * spg_get_dataset( SPGCONST double lattice[3][3],
				 SPGCONST double position[][3],
				 const int types[],
				 const int num_atom,
				 const double symprec )
{
  int i, j;
  int *mapping_table, *wyckoffs, *equiv_atoms, *equiv_atoms_prim;
  Spacegroup spacegroup;
  SpglibDataset *dataset;
  Cell *cell, *primitive;
  double inv_mat[3][3];
  Symmetry *symmetry;
  VecDBL *pure_trans;

  dataset = (SpglibDataset*) malloc( sizeof( SpglibDataset ) );

  cell = cel_alloc_cell( num_atom );
  cel_set_cell( cell, lattice, position, types );

  pure_trans = sym_get_pure_translation( cell, symprec );
  mapping_table = (int*) malloc( sizeof(int) * cell->size );
  primitive = prm_get_primitive( mapping_table, cell, pure_trans, symprec );
  mat_free_VecDBL( pure_trans );

  spacegroup = spa_get_spacegroup_with_primitive( primitive, symprec );

  /* Spacegroup type, transformation matrix, origin shift */
  if ( spacegroup.number > 0 ) {
    dataset->spacegroup_number = spacegroup.number;
    strcpy( dataset->international_symbol, spacegroup.international_short);
    strcpy( dataset->hall_symbol, spacegroup.hall_symbol);
    mat_inverse_matrix_d3( inv_mat, lattice, symprec );
    mat_multiply_matrix_d3( dataset->transformation_matrix,
			    inv_mat,
			    spacegroup.bravais_lattice );
    mat_copy_vector_d3( dataset->origin_shift, spacegroup.origin_shift );
  }

  /* Wyckoff positions */
  wyckoffs = (int*) malloc( sizeof(int) * primitive->size );
  equiv_atoms_prim = (int*) malloc( sizeof(int) * primitive->size );
  for ( i = 0; i < primitive->size; i++ ) {
    wyckoffs[i] = -1;
    equiv_atoms_prim[i] = -1;
  }
  ref_get_Wyckoff_positions( wyckoffs, 
			     equiv_atoms_prim,
			     primitive,
			     &spacegroup,
			     symprec );
  dataset->n_atoms = cell->size;
  dataset->wyckoffs = (int*) malloc( sizeof(int) * cell->size ); 
  for ( i = 0; i < cell->size; i++ ) {
    dataset->wyckoffs[i] = wyckoffs[ mapping_table[i] ];
  }
  free( wyckoffs );
  wyckoffs = NULL;

  dataset->equivalent_atoms = (int*) malloc( sizeof(int) * cell->size );
  equiv_atoms = (int*) malloc( sizeof(int) * primitive->size );
  for ( i = 0; i < primitive->size; i++ ) {
    for ( j = 0; j < cell->size; j++ ) {
      if ( mapping_table[j] == equiv_atoms_prim[i] ) {
	equiv_atoms[i] = j;
	break;
      }
    }
  }
  for ( i = 0; i < cell->size; i++ ) {
    dataset->equivalent_atoms[i] = equiv_atoms[ mapping_table[i] ];
  }
  free( equiv_atoms );
  equiv_atoms = NULL;
  free( equiv_atoms_prim );
  equiv_atoms_prim = NULL;
  free( mapping_table );
  mapping_table = NULL;

  /* Symmetry operations */
  symmetry = ref_get_refined_symmetry_operations( cell,
						  primitive->lattice,
						  &spacegroup,
						  symprec );
  cel_free_cell( cell );
  cel_free_cell( primitive );

  dataset->rotations = (int (*)[3][3]) malloc(sizeof(int[3][3]) * symmetry->size );
  dataset->translations = (double (*)[3]) malloc(sizeof(double[3]) * symmetry->size );
  dataset->n_operations = symmetry->size;
  for ( i = 0; i < symmetry->size; i++ ) {
    mat_copy_matrix_i3( dataset->rotations[i], symmetry->rot[i] );
    mat_copy_vector_d3( dataset->translations[i], symmetry->trans[i] );
  }

  sym_free_symmetry( symmetry );

  return dataset;
}

void spg_free_dataset( SpglibDataset *dataset )
{
  if ( dataset->n_operations > 0 ) {
    free( dataset->rotations );
    dataset->rotations = NULL;
    free( dataset->translations );
    dataset->translations = NULL;
  }
  free( dataset->wyckoffs );
  dataset->wyckoffs = NULL;
  free( dataset->equivalent_atoms );
  dataset->equivalent_atoms = NULL;
  free( dataset );
  dataset = NULL;
}

int spg_get_symmetry( int rotation[][3][3],
		      double translation[][3],
		      const int max_size,
		      SPGCONST double lattice[3][3],
		      SPGCONST double position[][3],
		      const int types[],
		      const int num_atom,
		      const double symprec )
{
  int i, j, size;
  Symmetry *symmetry;
  Cell *cell;

  cell = cel_alloc_cell( num_atom );
  cel_set_cell( cell, lattice, position, types );
  symmetry = sym_get_operation( cell, symprec );

  if (symmetry->size > max_size) {
    fprintf(stderr, "spglib: Indicated max size(=%d) is less than number ", max_size);
    fprintf(stderr, "spglib: of symmetry operations(=%d).\n", symmetry->size);
    sym_free_symmetry( symmetry );
    return 0;
  }

  for (i = 0; i < symmetry->size; i++) {
    mat_copy_matrix_i3(rotation[i], symmetry->rot[i]);
    for (j = 0; j < 3; j++) {
      translation[i][j] = symmetry->trans[i][j];
    }
  }

  size = symmetry->size;

  cel_free_cell( cell );
  sym_free_symmetry( symmetry );

  return size;
}

int spg_get_multiplicity( SPGCONST double lattice[3][3],
			  SPGCONST double position[][3],
			  const int types[],
			  const int num_atom,
			  const double symprec )
{
  Symmetry *symmetry;
  Cell *cell;
  int size;

  cell = cel_alloc_cell( num_atom );
  cel_set_cell( cell, lattice, position, types );
  symmetry = sym_get_operation( cell, symprec );

  size = symmetry->size;

  cel_free_cell( cell );
  sym_free_symmetry( symmetry );

  return size;
}

int spg_get_smallest_lattice( double smallest_lattice[3][3],
			      SPGCONST double lattice[3][3],
			      const double symprec )
{
  return lat_smallest_lattice_vector(smallest_lattice, lattice, symprec);
}

int spg_get_max_multiplicity( SPGCONST double lattice[3][3],
			      SPGCONST double position[][3],
			      const int types[],
			      const int num_atom,
			      const double symprec )
{
  Cell *cell;
  int num_max_multi;

  cell = cel_alloc_cell( num_atom );
  cel_set_cell( cell, lattice, position, types );
  /* 48 is the magic number, which is the number of rotations */
  /* in the highest point symmetry Oh. */
  num_max_multi = sym_get_multiplicity( cell, symprec ) * 48;
  cel_free_cell( cell );

  return num_max_multi;
}

int spg_find_primitive( double lattice[3][3],
			double position[][3],
			int types[],
			const int num_atom,
			const double symprec )
{
  int i, j, num_prim_atom=0;
  int *mapping_table;
  Cell *cell, *primitive;
  VecDBL *pure_trans;

  cell = cel_alloc_cell( num_atom );
  cel_set_cell( cell, lattice, position, types );
  pure_trans = sym_get_pure_translation( cell, symprec );

  /* find primitive cell */
  if ( pure_trans->size > 1 ) {
    mapping_table = (int*) malloc( sizeof(int) * cell->size );
    primitive = prm_get_primitive( mapping_table, cell, pure_trans, symprec );
    free( mapping_table );
    mapping_table = NULL;
    num_prim_atom = primitive->size;
    if ( num_prim_atom < num_atom && num_prim_atom > 0  ) {
      mat_copy_matrix_d3( lattice, primitive->lattice );
      for ( i = 0; i < primitive->size; i++ ) {
	types[i] = primitive->types[i];
	for (j=0; j<3; j++) {
	  position[i][j] = primitive->position[i][j];
	}
      }
    }
    cel_free_cell( primitive );
  } else {
    num_prim_atom = 0;
  }

  mat_free_VecDBL( pure_trans );
  cel_free_cell( cell );
    
  return num_prim_atom;
}

int spg_get_international( char symbol[11],
			   SPGCONST double lattice[3][3],
			   SPGCONST double position[][3],
			   const int types[],
			   const int num_atom,
			   const double symprec )
{
  Cell *cell;
  Spacegroup spacegroup;

  cell = cel_alloc_cell( num_atom );
  cel_set_cell( cell, lattice, position, types );
  spacegroup = spa_get_spacegroup( cell, symprec );
  if ( spacegroup.number > 0 ) {
    strcpy(symbol, spacegroup.international_short);
  }

  cel_free_cell( cell );
  
  return spacegroup.number;
}

int spg_get_schoenflies( char symbol[10],
			 SPGCONST double lattice[3][3],
			 SPGCONST double position[][3],
			 const int types[], const int num_atom,
			 const double symprec )
{
  Cell *cell;
  Spacegroup spacegroup;

  cell = cel_alloc_cell( num_atom );
  cel_set_cell( cell, lattice, position, types );

  spacegroup = spa_get_spacegroup( cell, symprec );
  if ( spacegroup.number > 0 ) {
    strcpy(symbol, spacegroup.schoenflies);
  }

  cel_free_cell( cell );

  return spacegroup.number;
}

int spg_refine_cell( double lattice[3][3],
		     double position[][3],
		     int types[],
		     const int num_atom,
		     const double symprec )
{
  int i, num_atom_bravais;
  Cell *cell, *bravais;

  cell = cel_alloc_cell( num_atom );
  cel_set_cell( cell, lattice, position, types );

  bravais = ref_refine_cell( cell, symprec );
  cel_free_cell( cell );

  if ( bravais->size > 0 ) {
    mat_copy_matrix_d3( lattice, bravais->lattice );
    num_atom_bravais = bravais->size;
    for ( i = 0; i < bravais->size; i++ ) {
      types[i] = bravais->types[i];
      mat_copy_vector_d3( position[i], bravais->position[i] );
    }
  } else {
    num_atom_bravais = 0;
  }

  cel_free_cell( bravais );
  
  return num_atom_bravais;
}

int spg_get_ir_kpoints( int map[],
			SPGCONST double kpoints[][3],
			const int num_kpoint,
			SPGCONST double lattice[3][3],
			SPGCONST double position[][3],
			const int types[],
			const int num_atom,
			const int is_time_reversal,
			const double symprec )
{
  Symmetry *symmetry;
  Cell *cell;
  int num_ir_kpoint;

  cell = cel_alloc_cell( num_atom );
  cel_set_cell( cell, lattice, position, types );
  symmetry = sym_get_operation( cell, symprec );

  num_ir_kpoint = kpt_get_irreducible_kpoints( map, kpoints, num_kpoint,
					       lattice, symmetry,
					       is_time_reversal, symprec );


  cel_free_cell( cell );
  sym_free_symmetry( symmetry );

  return num_ir_kpoint;
}

int spg_get_ir_reciprocal_mesh( int grid_point[][3],
				int map[],
				const int mesh[3],
				const int is_shift[3],
				const int is_time_reversal,
				SPGCONST double lattice[3][3],
				SPGCONST double position[][3],
				const int types[],
				const int num_atom,
				const double symprec )
{
  Symmetry *symmetry;
  Cell *cell;
  int num_ir;

  cell = cel_alloc_cell( num_atom );
  cel_set_cell( cell, lattice, position, types );
  symmetry = sym_get_operation( cell, symprec );

  num_ir = kpt_get_irreducible_reciprocal_mesh( grid_point,
						map,
						mesh,
						is_shift,
						is_time_reversal,
						lattice,
						symmetry,
						symprec );


  cel_free_cell( cell );
  sym_free_symmetry( symmetry );

  return num_ir;
}

int spg_get_stabilized_reciprocal_mesh( int grid_point[][3],
				        int map[],
				        const int mesh[3],
				        const int is_shift[3],
				        const int is_time_reversal,
				        SPGCONST double lattice[3][3],
					const int num_rot,
				        SPGCONST int rotations[][3][3],
				        const int num_q,
				        SPGCONST double qpoints[][3],
				        const double symprec )
{
  MatINT *rot_real;
  int i, num_ir;
  
  rot_real = mat_alloc_MatINT( num_rot );
  for ( i = 0; i < num_rot; i++ ) {
    mat_copy_matrix_i3( rot_real->mat[i], rotations[i] );
  }

  num_ir = kpt_get_stabilized_reciprocal_mesh( grid_point,
					       map,
					       mesh,
					       is_shift,
					       is_time_reversal,
					       lattice,
					       rot_real,
					       num_q,
					       qpoints,
					       symprec );

  mat_free_MatINT( rot_real );

  return num_ir;
}

SpglibTriplets * spg_get_triplets_reciprocal_mesh( const int mesh[3],
						   const int is_time_reversal,
						   SPGCONST double lattice[3][3],
						   const int num_rot,
						   SPGCONST int rotations[][3][3],
						   const double symprec )
{
  int i, j, num_grid;
  MatINT *rot_real;
  Triplets *tps;
  SpglibTriplets *spg_triplets;
  
  num_grid = mesh[0] * mesh[1] * mesh[2];
  rot_real = mat_alloc_MatINT( num_rot );
  for ( i = 0; i < num_rot; i++ ) {
    mat_copy_matrix_i3( rot_real->mat[i], rotations[i] );
  }

  tps = kpt_get_triplets_reciprocal_mesh( mesh,
					  is_time_reversal,
					  lattice,
					  rot_real,
					  symprec );
  mat_free_MatINT( rot_real );

  spg_triplets = (SpglibTriplets*) malloc( sizeof( SpglibTriplets ) );
  spg_triplets->size = tps->size;
  spg_triplets->triplets = (int (*)[3]) malloc( sizeof(int[3]) * tps->size );
  spg_triplets->weights = (int*) malloc( sizeof(int) * tps->size );
  spg_triplets->mesh_points = (int (*)[3]) malloc( sizeof(int[3]) * num_grid );

  for ( i = 0; i < 3; i++ ) {
    spg_triplets->mesh[i] = tps->mesh[i];
  }
  for ( i = 0; i < num_grid; i++ ) {
    for ( j = 0; j < 3; j++ ) {
      spg_triplets->mesh_points[i][j] = tps->mesh_points[i][j];
    }
  }

  for ( i = 0; i < tps->size; i++ ) {
    for ( j = 0; j < 3; j++ ) {
      spg_triplets->triplets[i][j] = tps->triplets[i][j];
    }
    spg_triplets->weights[i] = tps->weights[i];
  }
  kpt_free_triplets( tps );

  return spg_triplets;
}

void spg_free_triplets( SpglibTriplets * spg_triplets )
{
  free( spg_triplets->triplets );
  spg_triplets->triplets = NULL;
  free( spg_triplets->weights );
  spg_triplets->weights = NULL;
  free( spg_triplets );
  free( spg_triplets->mesh_points );
  spg_triplets->mesh_points = NULL;
  spg_triplets = NULL;
}

int spg_get_triplets_reciprocal_mesh_at_q( int weights[],
					   int grid_points[][3],
					   int third_q[],
					   const int grid_point,
					   const int mesh[3],
					   const int is_time_reversal,
					   SPGCONST double lattice[3][3],
					   const int num_rot,
					   SPGCONST int rotations[][3][3],
					   const double symprec )
{
  MatINT *rot_real;
  int i, num_ir;
  
  rot_real = mat_alloc_MatINT( num_rot );
  for ( i = 0; i < num_rot; i++ ) {
    mat_copy_matrix_i3( rot_real->mat[i], rotations[i] );
  }

  num_ir = kpt_get_ir_triplets_at_q( weights,
				     grid_points,
				     third_q,
				     grid_point,
				     mesh,
				     is_time_reversal,
				     lattice,
				     rot_real,
				     symprec );

  mat_free_MatINT( rot_real );

  return num_ir;
}

int spg_extract_triplets_reciprocal_mesh_at_q( int triplets_at_q[][3],
					       int weight_triplets_at_q[],
					       const int fixed_grid_number,
					       const int num_triplets,
					       SPGCONST int triplets[][3],
					       const int weight_triplets[],
					       const int mesh[3],
					       const int is_time_reversal,
					       SPGCONST double lattice[3][3],
					       const int num_rot,
					       SPGCONST int rotations[][3][3],
					       const double symprec )
{
  MatINT *rot_real;
  int i, num_ir;
  
  rot_real = mat_alloc_MatINT( num_rot );
  for ( i = 0; i < num_rot; i++ ) {
    mat_copy_matrix_i3( rot_real->mat[i], rotations[i] );
  }

  num_ir = kpt_extract_triplets_reciprocal_mesh_at_q( triplets_at_q,
							weight_triplets_at_q,
							fixed_grid_number,
							num_triplets,
							triplets,
							weight_triplets,
							mesh,
							is_time_reversal,
							lattice,
							rot_real,
							symprec );

  
  mat_free_MatINT( rot_real );

  return num_ir;
}
