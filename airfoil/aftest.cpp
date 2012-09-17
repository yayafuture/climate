/************************************************************************************
FronTier is a set of libraries that implements differnt types of Front Traking algorithms.
Front Tracking is a numerical method for the solution of partial differential equations 
whose solutions have discontinuities.  


Copyright (C) 1999 by The University at Stony Brook. 
 

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

******************************************************************************/

#include <iFluid.h>
#include <airfoil.h>
#include "solver.h"

static void set_equilibrium_mesh2d(Front*);
static void set_equilibrium_mesh3d(Front*);
static void adjust_for_node_type(NODE*,int,STRING_NODE_TYPE,double**,double**,
				double,double,double*);
static void adjust_for_curve_type(CURVE*,int,double**,double**,double,double*);
static void adjust_for_cnode_type(NODE*,int,double**,double**,double,double*);
static void propagate_curve(PARACHUTE_SET*,CURVE*,double**);
static void print_airfoil_stat2d(Front*,char*);
static void print_airfoil_stat2d_1(Front*,char*);
static void print_airfoil_stat2d_2(Front*,char*);
static void print_airfoil_stat3d(Front*,char*);
static void print_airfoil_stat3d_1(Front*,char*);
static void print_airfoil_stat3d_2(Front*,char*);
static void record_stretching_length(SURFACE*,char*,double);

extern void second_order_elastic_curve_propagate(
	Front           *fr,
        Front           *newfr,
        INTERFACE       *intfc,
        CURVE           *oldc,
        CURVE           *newc,
        double           fr_dt)
{
	static int size = 0;
	static double **x_old,**x_new,**v_old,**v_new,**f_old,**f_new;
	AF_PARAMS *af_params = (AF_PARAMS*)fr->extra2;
	int i,j,num_pts,count;
	int n,n_tan = af_params->n_tan;
	double dt = fr_dt/(double)n_tan;
	NODE *ns,*ne;
	int is,ie;
        double *g = af_params->gravity;
        double mass, payload = af_params->payload;
        PARACHUTE_SET geom_set;
        STRING_NODE_TYPE start_type = af_params->start_type;
        STRING_NODE_TYPE end_type = af_params->end_type;
	void (*compute_node_accel)(PARACHUTE_SET*,NODE*,double**,
				double**,double **,int*);
	void (*compute_curve_accel)(PARACHUTE_SET*,CURVE*,double**,
				double**,double **,int*);

	switch (af_params->spring_model)
	{
	case MODEL1:
	    compute_curve_accel = compute_curve_accel1;
	    compute_node_accel = compute_node_accel1;
	    break;
	case MODEL2:
	    compute_curve_accel = compute_curve_accel2;
	    compute_node_accel = compute_node_accel2;
	    break;
	case MODEL3:
	    compute_curve_accel = compute_curve_accel3;
	    compute_node_accel = compute_node_accel3;
	    break;
	default:
	    (void) printf("Model function not implemented yet!\n");
	    clean_up(ERROR);
	}


	if (wave_type(newc) != ELASTIC_BOUNDARY)
	    return;
	if (debugging("trace"))
	    (void) printf("Entering "
			"second_order_elastic_curve_propagate()\n");

	num_pts = FT_NumOfCurvePoints(oldc);
	if (size < num_pts)
	{
	    size = num_pts;
	    if (x_old != NULL)
		free_these(6,x_old,x_new,v_old,v_new,f_old,f_new);
            FT_MatrixMemoryAlloc((POINTER*)&x_old,size,2,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&x_new,size,2,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&v_old,size,2,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&v_new,size,2,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&f_old,size,2,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&f_new,size,2,sizeof(double));
	}

	geom_set.front = fr;
        geom_set.kl = af_params->kl;
        geom_set.lambda_l = af_params->lambda_l;
        geom_set.m_l = mass = af_params->m_l;
        geom_set.dt = dt;

	ns = newc->start;	ne = newc->end;
	is = 0;                 ie = size - 1;

	count = 0;
        compute_node_accel(&geom_set,ns,f_old,x_old,v_old,&count);
        compute_curve_accel(&geom_set,newc,f_old,x_old,v_old,&count);
        compute_node_accel(&geom_set,ne,f_old,x_old,v_old,&count);

	for (n = 0; n < n_tan; ++n)
	{
	    adjust_for_node_type(ns,is,start_type,f_old,v_old,mass,payload,g);
            adjust_for_node_type(ne,ie,end_type,f_old,v_old,mass,payload,g);
	    for (i = 0; i < size; ++i)
	    for (j = 0; j < 2; ++j)
	    {
	    	x_new[i][j] = x_old[i][j] + v_old[i][j]*dt;
	    	v_new[i][j] = v_old[i][j] + f_old[i][j]*dt;
	    }

	    count = 0;
	    assign_node_field(ns,x_new,v_new,&count);
	    assign_curve_field(newc,x_new,v_new,&count);
	    assign_node_field(ne,x_new,v_new,&count);
	    count = 0;
            compute_node_accel(&geom_set,ns,f_new,x_new,v_new,&count);
            compute_curve_accel(&geom_set,newc,f_new,x_new,v_new,&count);
            compute_node_accel(&geom_set,ne,f_new,x_new,v_new,&count);
            adjust_for_node_type(ns,is,start_type,f_new,v_new,mass,payload,g);
            adjust_for_node_type(ne,ie,end_type,f_new,v_new,mass,payload,g);

	    for (i = 0; i < size; ++i)
	    for (j = 0; j < 2; ++j)
	    {
	    	x_new[i][j] = x_old[i][j] + 0.5*dt*(v_old[i][j] + v_new[i][j]);
	    	v_new[i][j] = v_old[i][j] + 0.5*dt*(f_old[i][j] + f_new[i][j]);
	    }
	    count = 0;
	    propagate_curve(&geom_set,newc,x_new);
	    assign_node_field(ns,x_new,v_new,&count);
	    assign_curve_field(newc,x_new,v_new,&count);
	    assign_node_field(ne,x_new,v_new,&count);
	    if (n != n_tan-1)
	    {
		count = 0;
                compute_node_accel(&geom_set,ns,f_old,x_old,v_old,&count);
                compute_curve_accel(&geom_set,newc,f_old,x_old,v_old,&count);
                compute_node_accel(&geom_set,ne,f_old,x_old,v_old,&count);
	    }
	}
	
	if (debugging("trace"))
	    (void) printf("Leaving "
			"second_order_elastic_curve_propagate()\n");
}	/* end second_order_elastic_curve_propagate */

extern void second_order_elastic_surf_propagate(
	Front           *fr,
        Front           *newfr,
        INTERFACE       *intfc,
        SURFACE         *olds,
        SURFACE         *news,
        double           fr_dt)
{
	static int size = 0;
	static double **v_old,**v_new,**x_old,**x_new,**f_old,**f_new;
	AF_PARAMS *af_params = (AF_PARAMS*)fr->extra2;
	int i,j,num_pts,count;
	int n,n_tan = af_params->n_tan;
	double dt = fr_dt/(double)n_tan;
	PARACHUTE_SET geom_set;
	CURVE **oc,**nc,*oldc,*newc;
        NODE *oldn,*newn;
	void (*compute_node_accel)(PARACHUTE_SET*,NODE*,double**,
				double**,double **,int*);
	void (*compute_curve_accel)(PARACHUTE_SET*,CURVE*,double**,
				double**,double **,int*);
	void (*compute_surf_accel)(PARACHUTE_SET*,SURFACE*,double**,
				double**,double **,int*);

	switch (af_params->spring_model)
	{
	case MODEL1:
	    compute_curve_accel = compute_curve_accel1;
	    compute_node_accel = compute_node_accel1;
	    compute_surf_accel = compute_surf_accel1;
	    break;
	case MODEL2:
	    compute_curve_accel = compute_curve_accel2;
	    compute_node_accel = compute_node_accel2;
	    compute_surf_accel = compute_surf_accel2;
	    break;
	case MODEL3:
	default:
	    (void) printf("Model function not implemented yet!\n");
	    clean_up(ERROR);
	}

	if (wave_type(olds) != ELASTIC_BOUNDARY)
	    return;

	if (debugging("trace"))
	    (void) printf("Entering "
			"second_order_elastic_surf_propagate()\n");

	geom_set.ks = af_params->ks;
        geom_set.lambda_s = af_params->lambda_s;
        geom_set.m_s = af_params->m_s;
        geom_set.kl = af_params->kl;
        geom_set.lambda_l = af_params->lambda_l;
        geom_set.m_l = af_params->m_l;
	geom_set.front = fr;
	geom_set.dt = dt;
	
	/* Assume there is only one closed boundary curve */
        for (oc = olds->pos_curves, nc = news->pos_curves; oc && *oc;
                                ++oc, ++nc)
        {
            oldc = *oc;
            newc = *nc;
        }
        for (oc = olds->neg_curves, nc = news->neg_curves; oc && *oc;
                                ++oc, ++nc)
        {
            oldc = *oc;
            newc = *nc;
        }
        oldn = oldc->start;
        newn = newc->start;

	num_pts = FT_NumOfSurfPoints(olds);
	if (size < num_pts)
	{
	    size = num_pts;
	    if (v_old != NULL)
	    {
		FT_FreeThese(6,v_old,v_new,x_old,x_new,f_old,f_new);
	    }
	    FT_MatrixMemoryAlloc((POINTER*)&v_old,size,3,sizeof(double));
	    FT_MatrixMemoryAlloc((POINTER*)&v_new,size,3,sizeof(double));
	    FT_MatrixMemoryAlloc((POINTER*)&x_old,size,3,sizeof(double));
	    FT_MatrixMemoryAlloc((POINTER*)&x_new,size,3,sizeof(double));
	    FT_MatrixMemoryAlloc((POINTER*)&f_old,size,3,sizeof(double));
	    FT_MatrixMemoryAlloc((POINTER*)&f_new,size,3,sizeof(double));
	}

	count = 0;
        compute_surf_accel(&geom_set,news,f_old,x_old,v_old,&count);
        compute_curve_accel(&geom_set,newc,f_old,x_old,v_old,&count);
        compute_node_accel(&geom_set,newn,f_old,x_old,v_old,&count);

	for (n = 0; n < n_tan; ++n)
	{
	    for (i = 0; i < size; ++i)
	    for (j = 0; j < 3; ++j)
	    {
	    	x_new[i][j] = x_old[i][j] + v_old[i][j]*dt;
	    	v_new[i][j] = v_old[i][j] + f_old[i][j]*dt; 
	    }

	    count = 0;
            assign_surf_field(news,x_new,v_new,&count);
            assign_curve_field(newc,x_new,v_new,&count);
            assign_node_field(newn,x_new,v_new,&count);
            count = 0;
            compute_surf_accel(&geom_set,news,f_new,x_new,v_new,&count);
            compute_curve_accel(&geom_set,newc,f_new,x_new,v_new,&count);
            compute_node_accel(&geom_set,newn,f_new,x_new,v_new,&count);

	    for (i = 0; i < size; ++i)
	    for (j = 0; j < 3; ++j)
	    {
	    	x_new[i][j] = x_old[i][j] + 0.5*dt*(v_old[i][j] + v_new[i][j]);
	    	v_new[i][j] = v_old[i][j] + 0.5*dt*(f_old[i][j] + f_new[i][j]);
	    }

	    count = 0;
            assign_surf_field(news,x_new,v_new,&count);
            assign_curve_field(newc,x_new,v_new,&count);
            assign_node_field(newn,x_new,v_new,&count);
	    if (n != n_tan-1)
	    {
		count = 0;
                compute_surf_accel(&geom_set,news,f_old,x_old,v_old,&count);
                compute_curve_accel(&geom_set,newc,f_old,x_old,v_old,&count);
                compute_node_accel(&geom_set,newn,f_old,x_old,v_old,&count);
	    }
	}

	if (debugging("trace"))
	    (void) printf("Leaving "
			"second_order_elastic_surf_propagate()\n");
}	/* end second_order_elastic_surf_propagate */

extern void set_equilibrium_mesh(
	Front *front)
{
	switch (front->rect_grid->dim)
	{
	case 2:
	    set_equilibrium_mesh2d(front);
	    return;
	case 3:
	    set_equilibrium_mesh3d(front);
	    return;
	}
}	/* end set_equilibrium_mesh */

static void set_equilibrium_mesh2d(
	Front *front)
{
	CURVE **c,*curve;
	BOND *b;
	short unsigned int seed[3] = {2,72,7172};
	double len0,total_length = 0.0;
	int i,n = 0;
	INTERFACE *intfc = front->interf;
	AF_PARAMS *af_params = (AF_PARAMS*)front->extra2;
	int dim = front->rect_grid->dim;

	for (c = intfc->curves; c && *c; ++c)
	{
	    if (wave_type(*c) != ELASTIC_BOUNDARY) continue;
	    curve = *c;
	    for (b = curve->first; b != NULL; b = b->next)
	    {
		total_length +=  bond_length(b);
		n++;
	    }
	    len0 = total_length/(double)n;
	    for (b = curve->first; b != NULL; b = b->next)
	    {
		b->length0 = len0;
		for (i = 0; i < dim; ++i)
		    b->dir0[i] = (Coords(b->end)[i] - Coords(b->start)[i])
				/b->length0;	
	    }
	    for (b = curve->first; b != curve->last; b = b->next)
	    {
		if (af_params->pert_params.pert_type == PARALLEL_RAND_PERT) 
		{
		    double dx_rand;
		    double tan[MAXD];
		    tangent(b->end,b,curve,tan,front); 
		    dx_rand = (erand48(seed) - 0.5)*len0;
		    for (i = 0; i < dim; ++i)
		    	Coords(b->end)[i] += 0.2*dx_rand*tan[i];
		}
		else if (af_params->pert_params.pert_type == 
				ORTHOGONAL_RAND_PERT)
		{
		    double dx_rand;
		    double nor[MAXD];
		    double amp = af_params->pert_params.pert_amp;
		    FT_NormalAtPoint(b->end,front,nor,NO_COMP); 
		    if (amp > 1.0) amp = 1.0;
		    dx_rand = (erand48(seed) - 0.5)*amp*len0;
		    for (i = 0; i < dim; ++i)
		    	Coords(b->end)[i] += dx_rand*nor[i];
		}
		else if (af_params->pert_params.pert_type == SINE_PERT)
		{
		    /* This assumes the curve is horizontal */
		    double amp = af_params->pert_params.pert_amp;
		    double L = Coords(curve->start->posn)[0]; 
		    double U = Coords(curve->end->posn)[0];
		    double x = Coords(b->end)[0];
		    for (i = 1; i < dim; ++i)
		    	Coords(b->end)[i] += amp*sin(PI*(x-L)/(U-L));
		}
	    }
	    for (b = curve->first; b != NULL; b = b->next)
		set_bond_length(b,2);
	    never_redistribute(Hyper_surf(curve)) = YES;
	}
}	/* end set_equilibrium_mesh2d */

static void set_equilibrium_mesh3d(
	Front *front)
{
	SURFACE **s,*surf;
	CURVE **c;
	TRI *t;
	int i,j,dir;
	short unsigned int seed[3] = {2,72,7172};
	double max_len,min_len,ave_len,len;
	double vec[3],*cen,radius,amp;
	double x0,xl,xu;
	double dx_rand;
	double count;
	INTERFACE *intfc = front->interf;
	AF_PARAMS *af_params = (AF_PARAMS*)front->extra2;
	BOND *b;
	double gore_len_fac = af_params->gore_len_fac;

	for (c = intfc->curves; c && *c; ++c)
	{
	    for (b = (*c)->first; b != NULL; b = b->next)
	    {
		set_bond_length(b,3);
		b->length0 = bond_length(b);
		if (hsbdry_type(*c) == GORE_HSBDRY)
		    b->length0 *= gore_len_fac;
		for (i = 0; i < 3; ++i)
		    b->dir0[i] = (Coords(b->end)[i] - Coords(b->start)[i])/
					b->length0;	
	    }
	}
	for (s = intfc->surfaces; s && *s; ++s)
	{
	    if (wave_type(*s) != ELASTIC_BOUNDARY) continue;
	    surf = *s;
	    ave_len = 0.0;
	    max_len = 0.0;
	    min_len = HUGE;
	    count = 0.0;
	    for (t = first_tri(surf); !at_end_of_tri_list(t,surf); t = t->next)
	    {
		for (i = 0; i < 3; ++i)
		{
		    t->side_length0[i] = separation(Point_of_tri(t)[i],
			Point_of_tri(t)[(i+1)%3],3);
		    for (j = 0; j < 3; ++j)
		    {
		    	t->side_dir0[i][j] = 
				(Coords(Point_of_tri(t)[(i+1)%3])[j] -
				 Coords(Point_of_tri(t)[i])[j])/
				 t->side_length0[i];
		    }
		    if (max_len < t->side_length0[i]) 
			max_len = t->side_length0[i];
		    if (min_len > t->side_length0[i])
			min_len = t->side_length0[i];
		    ave_len += t->side_length0[i];
		    count += 1.0;
		}
	    }
	    never_redistribute(Hyper_surf(surf)) = YES;
	}
	af_params->min_len = min_len;
	printf("Original length:\n");
	printf("min_len = %16.12f\n",min_len);
	printf("max_len = %16.12f\n",max_len);
	printf("ave_len = %16.12f\n",ave_len/count);

	for (s = intfc->surfaces; s && *s; ++s)
	{
	    if (wave_type(*s) != ELASTIC_BOUNDARY) continue;
	    surf = *s;
	    switch (af_params->pert_params.pert_type)
	    {
	    case ORTHOGONAL_RAND_PERT:
	        for (t = first_tri(surf); !at_end_of_tri_list(t,surf); 
				t = t->next)
		for (i = 0; i < 3; ++i)
		{
		    for (j = 0; j < 3; ++j)
		    {
			vec[j] = Coords(Point_of_tri(t)[i])[j] -
				 Coords(Point_of_tri(t)[(i+1)%3])[j];
		    }
		    for (j = 0; j < 3; ++j)
		    {
		    	dx_rand = (2.0 + erand48(seed))/3.0;
			vec[j] *= dx_rand;
			Coords(Point_of_tri(t)[j])[2] += vec[j]; 
		    }
		}
		break;
	    case PARALLEL_RAND_PERT:
	        for (t = first_tri(surf); !at_end_of_tri_list(t,surf); 
				t = t->next)
		for (i = 0; i < 3; ++i)
		{
		    for (j = 0; j < 3; ++j)
		    {
			vec[j] = Coords(Point_of_tri(t)[i])[j] -
				 Coords(Point_of_tri(t)[(i+1)%3])[j];
		    }
		    dx_rand = (2.0 + erand48(seed))/3.0;
		    for (j = 0; j < 3; ++j)
		    {
			vec[j] *= dx_rand;
			Coords(Point_of_tri(t)[i])[j] = vec[j] +
				Coords(Point_of_tri(t)[(i+1)%3])[j];
		    }
		}
		break;
	    case RADIAL_PERT:
		cen = af_params->pert_params.cen;
		radius = af_params->pert_params.pert_radius;
		amp = af_params->pert_params.pert_amp;
	        for (t = first_tri(surf); !at_end_of_tri_list(t,surf); 
				t = t->next)
		for (i = 0; i < 3; ++i)
		    sorted(Point_of_tri(t)[i]) = NO;
	        for (t = first_tri(surf); !at_end_of_tri_list(t,surf); 
				t = t->next)
		for (i = 0; i < 3; ++i)
		{
		    POINT *p = Point_of_tri(t)[i];
		    double r;
		    if (Boundary_point(p)) continue;
		    if (sorted(p)) continue;
		    r = sqr(Coords(p)[0] - cen[0]) + 
			sqr(Coords(p)[1] - cen[1]);
		    r = sqrt(r);
		    if (r < radius)
		    	Coords(p)[2] += amp*(1.0 - r/radius);
		    sorted(p) = YES;
		}
		break;
	    case LINEAR_PERT:
		x0 = af_params->pert_params.x0;
		xl = af_params->pert_params.xl;
		xu = af_params->pert_params.xu;
		amp = af_params->pert_params.pert_amp;
		dir = af_params->pert_params.dir;
	        for (t = first_tri(surf); !at_end_of_tri_list(t,surf); 
				t = t->next)
		for (i = 0; i < 3; ++i)
		    sorted(Point_of_tri(t)[i]) = NO;
	        for (t = first_tri(surf); !at_end_of_tri_list(t,surf); 
				t = t->next)
		for (i = 0; i < 3; ++i)
		{
		    POINT *p = Point_of_tri(t)[i];
		    if (sorted(p)) continue;
		    if (Coords(p)[dir] < x0)
			Coords(p)[2] += amp*(Coords(p)[dir] - xl)/(x0 - xl);
		    else
			Coords(p)[2] += amp*(xu - Coords(p)[dir])/(xu - x0);
		    sorted(p) = YES;
		}
		break;
	    case NO_PERT:
	    default:
		break;
	    }
	}
	for (s = intfc->surfaces; s && *s; ++s)
	{
	    if (wave_type(*s) != ELASTIC_BOUNDARY) continue;
	    surf = *s;
	    ave_len = 0.0;
	    max_len = 0.0;
	    min_len = HUGE;
	    count = 0.0;
	    for (t = first_tri(surf); !at_end_of_tri_list(t,surf); t = t->next)
	    {
		for (i = 0; i < 3; ++i)
		{
		    len = separation(Point_of_tri(t)[i],
			Point_of_tri(t)[(i+1)%3],3);
		    if (max_len < len) 
			max_len = len;
		    if (min_len > len)
			min_len = len;
		    ave_len += len;
		    count += 1.0;
		}
	    }
	}
	printf("Perturbed length:\n");
	printf("min_len = %16.12f\n",min_len);
	printf("max_len = %16.12f\n",max_len);
	printf("ave_len = %16.12f\n",ave_len/count);
}	/* end set_equilibrium_mesh3d */

extern void unsort_surf_point(SURFACE *surf)
{
	TRI *tri;
	POINT *p;
	int i;

	for (tri = first_tri(surf); !at_end_of_tri_list(tri,surf); 
			tri = tri->next)
	{
	    for (i = 0; i < 3; ++i)
	    {
		p = Point_of_tri(tri)[i];
		sorted(p) = NO;
	    }
	}
}	/* end unsort_surf_point */

#define 	MAX_NUM_RING1		30

extern void print_airfoil_stat(
	Front *front,
	char *out_name)
{
	switch (front->rect_grid->dim)
	{
	case 2:
	    print_airfoil_stat2d(front,out_name);
	    return;
	case 3:
	    print_airfoil_stat3d(front,out_name);
	    return;
	}
}	/* end print_airfoil_stat */


static void print_airfoil_stat2d(
	Front *front,
	char *out_name)
{
	AF_PARAMS *af_params = (AF_PARAMS*)front->extra2;
	switch (af_params->spring_model)
	{
	case MODEL1:
	    print_airfoil_stat2d_1(front,out_name);
	    break;
	case MODEL2:
	    print_airfoil_stat2d_2(front,out_name);
	    break;
	case MODEL3:
	default:
	    (void) printf("print_airfoil_stat2d_12() not implemented!\n");
	}
}	/* end print_airfoil_stat2d */

static void print_airfoil_stat2d_1(
	Front *front,
	char *out_name)
{
	INTERFACE *intfc = front->interf;
	CURVE **c,*curve;
	BOND *b;
	POINT *p;
	static FILE *ekfile,*epfile,*exfile,*efile,*sfile;
	char fname[256];
	AF_PARAMS *af_params = (AF_PARAMS*)front->extra2;
	double ek,ep,enp;
	double kl,m_l,x_diff;
	int i,dim = intfc->dim;
	double str_length;
	STRING_NODE_TYPE start_type = af_params->start_type;
        STRING_NODE_TYPE end_type = af_params->end_type;
	double *g,payload;

	if (ekfile == NULL && pp_mynode() == 0)
        {
	    sprintf(fname,"%s/ek.xg",out_name);
            ekfile = fopen(fname,"w");
	    sprintf(fname,"%s/ep.xg",out_name);
            epfile = fopen(fname,"w");
	    sprintf(fname,"%s/ex.xg",out_name);
            exfile = fopen(fname,"w");
	    sprintf(fname,"%s/en.xg",out_name);
            efile = fopen(fname,"w");
	    sprintf(fname,"%s/str_length.xg",out_name);
            sfile = fopen(fname,"w");
            fprintf(ekfile,"\"Kinetic enegy vs. time\"\n");
            fprintf(epfile,"\"Potential enegy vs. time\"\n");
            fprintf(exfile,"\"External enegy vs. time\"\n");
            fprintf(efile,"\"Total enegy vs. time\"\n");
            fprintf(sfile,"\"String length vs. time\"\n");
        }

	kl = af_params->kl;
        m_l = af_params->m_l;
	payload = af_params->payload;
	g = af_params->gravity;
	ek = ep = enp = str_length = 0.0;
	for (c = intfc->curves; c && *c; ++c)
	{
		if (wave_type(*c) != ELASTIC_BOUNDARY)
		continue;

		curve = *c;
		p = curve->first->start;
		if (start_type == FREE_END)
		{
		    for (i = 0; i < dim; ++i)
		    {
		    	ek += 0.5*m_l*sqr(p->vel[i]);
		    }
		}
		else if(start_type == LOADED_END)
		{
		    for (i = 0; i < dim; ++i)
		    {
		    	ek += 0.5*payload*sqr(p->vel[i]);
			enp -= payload*g[i]*Coords(p)[i];
		    }
		}
		p = curve->last->end;
		if (end_type == FREE_END)
		{
		    for (i = 0; i < dim; ++i)
		    {
		    	ek += 0.5*m_l*sqr(p->vel[i]);
		    }
		}
		else if(end_type == LOADED_END)
		{
		    for (i = 0; i < dim; ++i)
		    {
		    	ek += 0.5*payload*sqr(p->vel[i]);
			enp -= payload*g[i]*Coords(p)[i];
		    }
		}

		for (b = curve->first; b != NULL; b = b->next)
		{
		    p = b->end;
		    x_diff = bond_length(b) - bond_length0(b);
		    if (b != curve->last)
		    {
		    	for (i = 0; i < dim; ++i)
			{
		    	    ek += 0.5*m_l*sqr(p->vel[i]);
			}
		    }
		    ep += 0.5*kl*sqr(x_diff);
		    str_length += bond_length(b);
		}
	}
	if (pp_mynode() == 0)
	{
	    fprintf(ekfile,"%16.12f  %16.12f\n",front->time,ek);
            fprintf(epfile,"%16.12f  %16.12f\n",front->time,ep);
            fprintf(exfile,"%16.12f  %16.12f\n",front->time,enp);
            fprintf(efile,"%16.12f  %16.12f\n",front->time,ek+ep+enp);
            fprintf(sfile,"%16.12f  %16.12f\n",front->time,str_length);
	    fflush(ekfile);
	    fflush(epfile);
	    fflush(exfile);
	    fflush(efile);
	    fflush(sfile);
	}
}	/* end print_airfoil_stat2d_1 */

static void print_airfoil_stat3d(
	Front *front,
	char *out_name)
{
	AF_PARAMS *af_params = (AF_PARAMS*)front->extra2;
	switch (af_params->spring_model)
	{
	case MODEL1:
	    print_airfoil_stat3d_1(front,out_name);
	    break;
	case MODEL2:
	    print_airfoil_stat3d_2(front,out_name);
	    break;
	case MODEL3:
	default:
	    (void) printf("print_airfoil_stat3d_12() not implemented!\n");
	}
}	/* end print_airfoil_stat3d */

static void print_airfoil_stat3d_1(
	Front *front,
	char *out_name)
{
	INTERFACE *intfc = front->interf;
	NODE **n,*node;
	CURVE **c,*curve;
	SURFACE **s,*surf;
	BOND *b;
	TRI *tri;
	POINT *p;
	static FILE *eskfile,*espfile,*egpfile,*efile,*exkfile,*enkfile;
	static FILE *afile,*sfile,*pfile,*vfile;
	static FILE *xcom_file,*vcom_file;
	static FILE *samplex,*sampley,*samplez;
	static double ep0;
	static boolean first = YES;
	char fname[256];
	AF_PARAMS *af_params = (AF_PARAMS*)front->extra2;
	double esk,esp,epi,epb,egp,exk,enk;
	double ks,m_s,kl,m_l,x_diff,side_length;
	int j,k,nc,dim = intfc->dim;
	double cnp_area,str_length,pz,pv;
	double zcom,vcom;
	double payload = af_params->payload;
	double *g = af_params->gravity;
	STATE *st;
	static int np,ip;
	static POINT **pts;
	POINT *psample;
	static double p0[MAXD];

	if (eskfile == NULL)
        {
	    sprintf(fname,"%s/esk.xg",out_name);
            eskfile = fopen(fname,"w");
	    sprintf(fname,"%s/esp.xg",out_name);
            espfile = fopen(fname,"w");
	    sprintf(fname,"%s/egp.xg",out_name);
            egpfile = fopen(fname,"w");
	    sprintf(fname,"%s/exk.xg",out_name);
            exkfile = fopen(fname,"w");
	    sprintf(fname,"%s/enk.xg",out_name);
            enkfile = fopen(fname,"w");
	    sprintf(fname,"%s/eng.xg",out_name);
            efile = fopen(fname,"w");
	    sprintf(fname,"%s/area.xg",out_name);
            afile = fopen(fname,"w");
	    sprintf(fname,"%s/str_length.xg",out_name);
            sfile = fopen(fname,"w");
	    sprintf(fname,"%s/payload.xg",out_name);
            pfile = fopen(fname,"w");
	    sprintf(fname,"%s/loadvel.xg",out_name);
            vfile = fopen(fname,"w");
	    sprintf(fname,"%s/xcom.xg",out_name);
            xcom_file = fopen(fname,"w");
	    sprintf(fname,"%s/vcom.xg",out_name);
            vcom_file = fopen(fname,"w");
	    sprintf(fname,"%s/samplex.xg",out_name);
            samplex = fopen(fname,"w");
	    sprintf(fname,"%s/sampley.xg",out_name);
            sampley = fopen(fname,"w");
	    sprintf(fname,"%s/samplez.xg",out_name);
            samplez = fopen(fname,"w");
            fprintf(eskfile,"\"Spr-kinetic energy vs. time\"\n");
            fprintf(espfile,"\"Spr-potentl energy vs. time\"\n");
            fprintf(exkfile,"\"Ext-kinetic energy vs. time\"\n");
            fprintf(egpfile,"\"Ext-potentl energy vs. time\"\n");
            fprintf(enkfile,"\"Kinetic energy vs. time\"\n");
            fprintf(efile,"\"Total energy vs. time\"\n");
            fprintf(afile,"\"Canopy area vs. time\"\n");
            fprintf(sfile,"\"String length vs. time\"\n");
            fprintf(pfile,"\"Payload hight vs. time\"\n");
            fprintf(vfile,"\"Payload velo vs. time\"\n");
            fprintf(xcom_file,"\"COM vs. time\"\n");
            fprintf(vcom_file,"\"V-COM vs. time\"\n");
            fprintf(samplex,"\"x-coords vs. time\"\n");
            fprintf(sampley,"\"y-coords vs. time\"\n");
            fprintf(samplez,"\"z-coords vs. time\"\n");
        }
	ks = af_params->ks;
        m_s = af_params->m_s;

	esk = esp = epi = epb = egp = exk = enk = 0.0;
	cnp_area = 0.0;
	for (s = intfc->surfaces; s && *s; ++s)
	{
	    if (wave_type(*s) != ELASTIC_BOUNDARY)
	    	continue;
	    surf = *s;
	    zcom = center_of_mass(Hyper_surf(surf))[2];
	    vcom = center_of_mass_velo(Hyper_surf(surf))[2];
	    if (first)
	    {
		np = FT_NumOfSurfPoints(surf);
		ip = np/2;
		FT_VectorMemoryAlloc((POINTER*)&pts,np,sizeof(POINT*));
	    }
	    FT_ArrayOfSurfPoints(surf,pts);
	    psample = pts[ip];
	    for (tri = first_tri(surf); !at_end_of_tri_list(tri,surf);
                        tri = tri->next)
	    {
		cnp_area += tri_area(tri);
		for (j = 0; j < 3; ++j)
            	{
		    side_length = separation(Point_of_tri(tri)[j],
                                Point_of_tri(tri)[(j+1)%3],3);
		    x_diff = side_length - tri->side_length0[j];
		    if (!is_side_bdry(tri,j))
                    	epi += 0.5*ks*sqr(x_diff);
		}
	    }
	    unsort_surf_point(surf);
	    for (tri = first_tri(surf); !at_end_of_tri_list(tri,surf);
                        tri = tri->next)
	    {
		for (j = 0; j < 3; ++j)
		{
                    p = Point_of_tri(tri)[j];
		    if (sorted(p) || Boundary_point(p)) continue;
		    for (k = 0; k < dim; ++k)
		    {
                    	esk += 0.5*m_s*sqr(p->vel[k]);
			egp += -g[k]*m_s*Coords(p)[k];
			st = (STATE*)left_state(p);
			exk += 0.5*m_s*sqr(st->Impct[k]);
			enk += 0.5*m_s*sqr(p->vel[k] + st->Impct[k]);
		    }
		    sorted(p) = YES;
		}
	    }
	}

	record_stretching_length(surf,out_name,front->time);

	epi *= 0.5;	//Each side is counted twice
	for (c = intfc->curves; c && *c; ++c)
	{
	    if (hsbdry_type(*c) == STRING_HSBDRY)
	    {
		kl = af_params->kl;
        	m_l = af_params->m_l;
	    }
	    else if (hsbdry_type(*c) == MONO_COMP_HSBDRY)
	    {
		kl = af_params->ks;
        	m_l = af_params->m_s;
	    }
	    else
		continue;
	    curve = *c;
	    for (b = curve->first; b != NULL; b = b->next)
	    {
		x_diff = bond_length(b) - bond_length0(b);
		epb += 0.5*kl*sqr(x_diff);
		if (b != curve->last)
		    for (k = 0; k < dim; ++k)
		    {
		    	esk += 0.5*m_l*sqr(b->end->vel[k]);
			egp += -g[k]*m_l*Coords(b->end)[k];
			st = (STATE*)left_state(b->end);
			exk += 0.5*m_l*sqr(st->Impct[k]);
			enk += 0.5*m_l*sqr(b->end->vel[k] + st->Impct[k]);
		    }
	    }
	    node = curve->start;
	    if (!is_load_node(node))
	    {
	    	for (k = 0; k < dim; ++k)
	    	{
		    esk += 0.5*m_l*sqr(node->posn->vel[k]);
		    egp += -g[k]*m_l*Coords(node->posn)[k];
		    st = (STATE*)left_state(node->posn);
		    exk += 0.5*m_l*sqr(st->Impct[k]);
		    enk += 0.5*m_l*sqr(node->posn->vel[k] + st->Impct[k]);
	    	}
	    }
	    if (!is_closed_curve(curve))
	    {
	    	node = curve->end;
	    	if (!is_load_node(node))
	    	{
		    for (k = 0; k < dim; ++k)
		    {
		    	esk += 0.5*m_l*sqr(node->posn->vel[k]);
		    	egp += -g[k]*m_l*Coords(node->posn)[k];
			st = (STATE*)left_state(node->posn);
			exk += 0.5*m_l*sqr(st->Impct[k]);
		        enk += 0.5*m_l*sqr(node->posn->vel[k] + st->Impct[k]);
		    }
	    	}
	    }
	}
	esp = epi + epb;

	for (n = intfc->nodes; n && *n; ++n)
	{
	    STATE *sl;
	    if (!is_load_node(*n)) continue;
	    pz = Coords((*n)->posn)[2];
	    sl = (STATE*)left_state((*n)->posn);
	    pv = sl->vel[2];
	    for (k = 0; k < dim; ++k)
	    {
		egp += -g[k]*payload*Coords(node->posn)[k];
		st = (STATE*)left_state(node->posn);
		exk += 0.5*payload*sqr(st->Impct[k]);
		enk += 0.5*payload*sqr(node->posn->vel[k] + st->Impct[k]);
	    }
	}

	nc = 0;		str_length = 0.0;
	for (c = intfc->curves; c && *c; ++c)
	{
	    if (hsbdry_type(*c) != STRING_HSBDRY)
		continue;
	    str_length += curve_length(*c);
	    nc++;
	}
	if (nc != 0)
	    str_length /= (double)nc;
	if (first)
	{
	    for (k = 0; k < dim; ++k)
		p0[k] = Coords(psample)[k];
	    first = NO;
	    ep0 = egp;
	}
	egp -= ep0;

	fprintf(eskfile,"%16.12f  %16.12f\n",front->time,esk);
        fprintf(espfile,"%16.12f  %16.12f\n",front->time,esp);
        fprintf(egpfile,"%16.12f  %16.12f\n",front->time,egp);
        fprintf(exkfile,"%16.12f  %16.12f\n",front->time,exk);
        fprintf(enkfile,"%16.12f  %16.12f\n",front->time,enk);
        fprintf(efile,"%16.12f  %16.12f\n",front->time,esp+egp+enk);
	fflush(eskfile);
	fflush(espfile);
	fflush(egpfile);
	fflush(exkfile);
	fflush(enkfile);
	fflush(efile);

        fprintf(afile,"%16.12f  %16.12f\n",front->time,cnp_area);
        fprintf(sfile,"%16.12f  %16.12f\n",front->time,str_length);
        fprintf(pfile,"%16.12f  %16.12f\n",front->time,pz);
        fprintf(vfile,"%16.12f  %16.12f\n",front->time,pv);
	fflush(afile);
	fflush(sfile);
	fflush(pfile);
	fflush(vfile);

        fprintf(xcom_file,"%16.12f  %16.12f\n",front->time,zcom);
        fprintf(vcom_file,"%16.12f  %16.12f\n",front->time,vcom);
	fflush(xcom_file);
	fflush(vcom_file);
        fprintf(samplex,"%16.12f  %16.12f\n",front->time,Coords(psample)[0]
				- p0[0]);
        fprintf(sampley,"%16.12f  %16.12f\n",front->time,Coords(psample)[1]
				- p0[1]);
        fprintf(samplez,"%16.12f  %16.12f\n",front->time,Coords(psample)[2]
				- p0[2]);
	fflush(samplex);
	fflush(sampley);
	fflush(samplez);
}	/* end print_airfoil_stat3d_1 */

static void print_airfoil_stat3d_2(
	Front *front,
	char *out_name)
{
	INTERFACE *intfc = front->interf;
	NODE **n,*node;
	CURVE **c,*curve;
	SURFACE **s,*surf;
	BOND *b;
	TRI *tri;
	POINT *p;
	static FILE *eskfile,*espfile,*egpfile,*efile,*exkfile,*enkfile;
	static FILE *afile,*sfile,*pfile,*vfile;
	static FILE *xcom_file,*vcom_file;
	static double ep0;
	static boolean first = YES;
	char fname[256];
	AF_PARAMS *af_params = (AF_PARAMS*)front->extra2;
	double esk,esp,epi,epb,egp,exk,enk;
	double ks,m_s,kl,m_l,x_diff,x_sqr,side_length,vect[3];
	int j,k,nc,dim = intfc->dim;
	double cnp_area,str_length,pz,pv;
	double zcom,vcom;
	double payload = af_params->payload;
	double *g = af_params->gravity;
	STATE *st;

	if (eskfile == NULL)
        {
	    sprintf(fname,"%s/esk.xg",out_name);
            eskfile = fopen(fname,"w");
	    sprintf(fname,"%s/esp.xg",out_name);
            espfile = fopen(fname,"w");
	    sprintf(fname,"%s/egp.xg",out_name);
            egpfile = fopen(fname,"w");
	    sprintf(fname,"%s/exk.xg",out_name);
            exkfile = fopen(fname,"w");
	    sprintf(fname,"%s/enk.xg",out_name);
            enkfile = fopen(fname,"w");
	    sprintf(fname,"%s/eng.xg",out_name);
            efile = fopen(fname,"w");
	    sprintf(fname,"%s/area.xg",out_name);
            afile = fopen(fname,"w");
	    sprintf(fname,"%s/str_length.xg",out_name);
            sfile = fopen(fname,"w");
	    sprintf(fname,"%s/payload.xg",out_name);
            pfile = fopen(fname,"w");
	    sprintf(fname,"%s/loadvel.xg",out_name);
            vfile = fopen(fname,"w");
	    sprintf(fname,"%s/xcom.xg",out_name);
            xcom_file = fopen(fname,"w");
	    sprintf(fname,"%s/vcom.xg",out_name);
            vcom_file = fopen(fname,"w");
            fprintf(eskfile,"\"Spr-kinetic energy vs. time\"\n");
            fprintf(espfile,"\"Spr-potentl energy vs. time\"\n");
            fprintf(exkfile,"\"Ext-kinetic energy vs. time\"\n");
            fprintf(egpfile,"\"Ext-potentl energy vs. time\"\n");
            fprintf(enkfile,"\"Kinetic energy vs. time\"\n");
            fprintf(efile,"\"Total energy vs. time\"\n");
            fprintf(afile,"\"Canopy area vs. time\"\n");
            fprintf(sfile,"\"String length vs. time\"\n");
            fprintf(pfile,"\"Payload hight vs. time\"\n");
            fprintf(vfile,"\"Payload velo vs. time\"\n");
            fprintf(xcom_file,"\"COM vs. time\"\n");
            fprintf(vcom_file,"\"V-COM vs. time\"\n");
        }
	ks = af_params->ks;
        m_s = af_params->m_s;

	esk = esp = epi = epb = egp = exk = enk = 0.0;
	cnp_area = 0.0;
	for (s = intfc->surfaces; s && *s; ++s)
	{
	    if (wave_type(*s) != ELASTIC_BOUNDARY)
	    	continue;
	    surf = *s;
	    zcom = center_of_mass(Hyper_surf(surf))[2];
	    vcom = center_of_mass_velo(Hyper_surf(surf))[2];
	    for (tri = first_tri(surf); !at_end_of_tri_list(tri,surf);
                        tri = tri->next)
	    {
		cnp_area += tri_area(tri);
		for (j = 0; j < 3; ++j)
            	{
		    side_length = separation(Point_of_tri(tri)[j],
                                Point_of_tri(tri)[(j+1)%3],3);
		    x_diff = side_length - tri->side_length0[j];
		    x_sqr = 0.0;
		    for (k = 0; k < 3; ++k)
		    {
			vect[k] = Coords(Point_of_tri(tri)[(j+1)%3])[k]
				- Coords(Point_of_tri(tri)[j])[k] -
				tri->side_length0[j]*tri->side_dir0[j][k];
			x_sqr += sqr(vect[k]);
		    }
		    if (!is_side_bdry(tri,j))
                    	//epi += 0.5*ks*sqr(x_diff);
                    	epi += 0.5*ks*x_sqr;
		}
	    }
	    unsort_surf_point(surf);
	    for (tri = first_tri(surf); !at_end_of_tri_list(tri,surf);
                        tri = tri->next)
	    {
		for (j = 0; j < 3; ++j)
		{
                    p = Point_of_tri(tri)[j];
		    if (sorted(p) || Boundary_point(p)) continue;
		    for (k = 0; k < dim; ++k)
		    {
                    	esk += 0.5*m_s*sqr(p->vel[k]);
			egp += -g[k]*m_s*Coords(p)[k];
			st = (STATE*)left_state(p);
			exk += 0.5*m_s*sqr(st->Impct[k]);
			enk += 0.5*m_s*sqr(p->vel[k] + st->Impct[k]);
		    }
		    sorted(p) = YES;
		}
	    }
	}
	epi *= 0.5;	//Each side is counted twice
	for (c = intfc->curves; c && *c; ++c)
	{
	    if (hsbdry_type(*c) == STRING_HSBDRY)
	    {
		kl = af_params->kl;
        	m_l = af_params->m_l;
	    }
	    else if (hsbdry_type(*c) == MONO_COMP_HSBDRY)
	    {
		kl = af_params->ks;
        	m_l = af_params->m_s;
	    }
	    else
		continue;
	    curve = *c;
	    for (b = curve->first; b != NULL; b = b->next)
	    {
		x_diff = bond_length(b) - bond_length0(b);
		x_sqr = 0.0;
		for (k = 0; k < 3; ++k)
		{
		    vect[k] = Coords(b->end)[k] - Coords(b->start)[k]
				- bond_length0(b)*b->dir0[k];
		    x_sqr += sqr(vect[k]);
		}
		//epb += 0.5*kl*sqr(x_diff);
		epb += 0.5*kl*x_sqr;
		if (b != curve->last)
		    for (k = 0; k < dim; ++k)
		    {
		    	esk += 0.5*m_l*sqr(b->end->vel[k]);
			egp += -g[k]*m_l*Coords(b->end)[k];
			st = (STATE*)left_state(b->end);
			exk += 0.5*m_l*sqr(st->Impct[k]);
			enk += 0.5*m_l*sqr(b->end->vel[k] + st->Impct[k]);
		    }
	    }
	    node = curve->start;
	    if (!is_load_node(node))
	    {
	    	for (k = 0; k < dim; ++k)
	    	{
		    esk += 0.5*m_l*sqr(node->posn->vel[k]);
		    egp += -g[k]*m_l*Coords(node->posn)[k];
		    st = (STATE*)left_state(node->posn);
		    exk += 0.5*m_l*sqr(st->Impct[k]);
		    enk += 0.5*m_l*sqr(node->posn->vel[k] + st->Impct[k]);
	    	}
	    }
	    if (!is_closed_curve(curve))
	    {
	    	node = curve->end;
	    	if (!is_load_node(node))
	    	{
		    for (k = 0; k < dim; ++k)
		    {
		    	esk += 0.5*m_l*sqr(node->posn->vel[k]);
		    	egp += -g[k]*m_l*Coords(node->posn)[k];
			st = (STATE*)left_state(node->posn);
			exk += 0.5*m_l*sqr(st->Impct[k]);
		        enk += 0.5*m_l*sqr(node->posn->vel[k] + st->Impct[k]);
		    }
	    	}
	    }
	}
	esp = epi + epb;

	for (n = intfc->nodes; n && *n; ++n)
	{
	    STATE *sl;
	    if (!is_load_node(*n)) continue;
	    pz = Coords((*n)->posn)[2];
	    sl = (STATE*)left_state((*n)->posn);
	    pv = sl->vel[2];
	    for (k = 0; k < dim; ++k)
	    {
		egp += -g[k]*payload*Coords(node->posn)[k];
		st = (STATE*)left_state(node->posn);
		exk += 0.5*payload*sqr(st->Impct[k]);
		enk += 0.5*payload*sqr(node->posn->vel[k] + st->Impct[k]);
	    }
	}

	nc = 0;		str_length = 0.0;
	for (c = intfc->curves; c && *c; ++c)
	{
	    if (hsbdry_type(*c) != STRING_HSBDRY)
		continue;
	    str_length += curve_length(*c);
	    nc++;
	}
	if (nc != 0)
	    str_length /= (double)nc;
	if (first)
	{
	    first = NO;
	    ep0 = egp;
	}
	egp -= ep0;

	fprintf(eskfile,"%16.12f  %16.12f\n",front->time,esk);
        fprintf(espfile,"%16.12f  %16.12f\n",front->time,esp);
        fprintf(egpfile,"%16.12f  %16.12f\n",front->time,egp);
        fprintf(exkfile,"%16.12f  %16.12f\n",front->time,exk);
        fprintf(enkfile,"%16.12f  %16.12f\n",front->time,enk);
        fprintf(efile,"%16.12f  %16.12f\n",front->time,esp+egp+enk);
	fflush(eskfile);
	fflush(espfile);
	fflush(egpfile);
	fflush(exkfile);
	fflush(enkfile);
	fflush(efile);

        fprintf(afile,"%16.12f  %16.12f\n",front->time,cnp_area);
        fprintf(sfile,"%16.12f  %16.12f\n",front->time,str_length);
        fprintf(pfile,"%16.12f  %16.12f\n",front->time,pz);
        fprintf(vfile,"%16.12f  %16.12f\n",front->time,pv);
	fflush(afile);
	fflush(sfile);
	fflush(pfile);
	fflush(vfile);

        fprintf(xcom_file,"%16.12f  %16.12f\n",front->time,zcom);
        fprintf(vcom_file,"%16.12f  %16.12f\n",front->time,vcom);
	fflush(xcom_file);
	fflush(vcom_file);
}	/* end print_airfoil_stat3d_2 */

extern void fourth_order_elastic_curve_propagate(
	Front           *fr,
        Front           *newfr,
        INTERFACE       *intfc,
        CURVE           *oldc,
        CURVE           *newc,
        double           fr_dt)
{
	static int size = 0;
	static double **x_old,**x_new,**v_old,**v_new,**f_old,**f_new;
	static double **x_mid,**v_mid,**f_mid;
	AF_PARAMS *af_params = (AF_PARAMS*)fr->extra2;
	double mass;
	int i,j,num_pts,count;
	int n,n_tan = af_params->n_tan;
	double dt = fr_dt/(double)n_tan;
	int dim = fr->rect_grid->dim;
	NODE *ns,*ne;
	int is,ie;
	double *g = af_params->gravity;
	double payload = af_params->payload;
	PARACHUTE_SET geom_set;
	STRING_NODE_TYPE start_type = af_params->start_type;
	STRING_NODE_TYPE end_type = af_params->end_type;
	void (*compute_node_accel)(PARACHUTE_SET*,NODE*,double**,
				double**,double **,int*);
	void (*compute_curve_accel)(PARACHUTE_SET*,CURVE*,double**,
				double**,double **,int*);

	switch (af_params->spring_model)
	{
	case MODEL1:
	    compute_curve_accel = compute_curve_accel1;
	    compute_node_accel = compute_node_accel1;
	    break;
	case MODEL2:
	    compute_curve_accel = compute_curve_accel2;
	    compute_node_accel = compute_node_accel2;
	    break;
	case MODEL3:
	    compute_curve_accel = compute_curve_accel3;
	    compute_node_accel = compute_node_accel3;
	    break;
	default:
	    (void) printf("Model function not implemented yet!\n");
	    clean_up(ERROR);
	}

	if (wave_type(newc) != ELASTIC_BOUNDARY)
	    return;
	if (debugging("trace"))
	    (void) printf("Entering "
			"fourth_order_elastic_curve_propagate()\n");

	num_pts = FT_NumOfCurvePoints(oldc);
	if (size < num_pts)
	{
	    size = num_pts;
		FT_FreeThese(9,v_old,v_new,v_mid,x_old,x_new,x_mid,
				f_old,f_new,f_mid);
            FT_MatrixMemoryAlloc((POINTER*)&x_old,size,dim,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&v_old,size,dim,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&f_old,size,dim,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&x_mid,size,dim,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&v_mid,size,dim,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&f_mid,size,dim,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&x_new,size,dim,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&v_new,size,dim,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&f_new,size,dim,sizeof(double));
	}

	geom_set.front = fr;
	geom_set.kl = af_params->kl;
	geom_set.lambda_l = af_params->lambda_l;
	geom_set.m_l = mass = af_params->m_l;
	geom_set.dt = dt;

	ns = newc->start;	ne = newc->end;
	is = 0;			ie = size - 1;

	count = 0;
	compute_node_accel(&geom_set,ns,f_old,x_old,v_old,&count);
	compute_curve_accel(&geom_set,newc,f_old,x_old,v_old,&count);
	compute_node_accel(&geom_set,ne,f_old,x_old,v_old,&count);

	for (n = 0; n < n_tan; ++n)
	{
	    adjust_for_node_type(ns,is,start_type,f_old,v_old,mass,payload,g);
	    adjust_for_node_type(ne,ie,end_type,f_old,v_old,mass,payload,g);
	    for (i = 0; i < size; ++i)
	    for (j = 0; j < dim; ++j)
	    {
		x_new[i][j] = x_old[i][j] + dt*v_old[i][j]/6.0;
                v_new[i][j] = v_old[i][j] + dt*f_old[i][j]/6.0;
	    	x_mid[i][j] = x_old[i][j] + 0.5*v_old[i][j]*dt;
	    	v_mid[i][j] = v_old[i][j] + 0.5*f_old[i][j]*dt;
	    }
	    printf("x_mid[20] = %f %f\n",x_mid[20][0],x_mid[20][1]);

	    count = 0;
	    assign_node_field(ns,x_mid,v_mid,&count);
	    assign_curve_field(newc,x_mid,v_mid,&count);
	    assign_node_field(ne,x_mid,v_mid,&count);
	    count = 0;
	    compute_node_accel(&geom_set,ns,f_mid,x_mid,v_mid,&count);
	    compute_curve_accel(&geom_set,newc,f_mid,x_mid,v_mid,&count);
	    compute_node_accel(&geom_set,ne,f_mid,x_mid,v_mid,&count);
	    adjust_for_node_type(ns,is,start_type,f_mid,v_mid,mass,payload,g);
	    adjust_for_node_type(ne,ie,end_type,f_mid,v_mid,mass,payload,g);

	    for (i = 0; i < size; ++i)
	    for (j = 0; j < dim; ++j)
	    {
		x_new[i][j] += dt*v_mid[i][j]/3.0;
                v_new[i][j] += dt*f_mid[i][j]/3.0;
	    	x_mid[i][j] = x_old[i][j] + 0.5*v_mid[i][j]*dt;
	    	v_mid[i][j] = v_old[i][j] + 0.5*f_mid[i][j]*dt;
	    }
	
	    count = 0;
	    assign_node_field(ns,x_mid,v_mid,&count);
	    assign_curve_field(newc,x_mid,v_mid,&count);
	    assign_node_field(ne,x_mid,v_mid,&count);
	    count = 0;
	    compute_node_accel(&geom_set,ns,f_mid,x_mid,v_mid,&count);
	    compute_curve_accel(&geom_set,newc,f_mid,x_mid,v_mid,&count);
	    compute_node_accel(&geom_set,ne,f_mid,x_mid,v_mid,&count);
	    adjust_for_node_type(ns,is,start_type,f_mid,v_mid,mass,payload,g);
	    adjust_for_node_type(ne,ie,end_type,f_mid,v_mid,mass,payload,g);

	    for (i = 0; i < size; ++i)
	    for (j = 0; j < dim; ++j)
	    {
		x_new[i][j] += dt*v_mid[i][j]/3.0;
                v_new[i][j] += dt*f_mid[i][j]/3.0;
	    	x_mid[i][j] = x_old[i][j] + v_mid[i][j]*dt;
	    	v_mid[i][j] = v_old[i][j] + f_mid[i][j]*dt; 
	    }

	    count = 0;
	    assign_node_field(ns,x_mid,v_mid,&count);
	    assign_curve_field(newc,x_mid,v_mid,&count);
	    assign_node_field(ne,x_mid,v_mid,&count);
	    count = 0;
	    compute_node_accel(&geom_set,ns,f_mid,x_mid,v_mid,&count);
	    compute_curve_accel(&geom_set,newc,f_mid,x_mid,v_mid,&count);
	    compute_node_accel(&geom_set,ne,f_mid,x_mid,v_mid,&count);
	    adjust_for_node_type(ns,is,start_type,f_mid,v_mid,mass,payload,g);
	    adjust_for_node_type(ne,ie,end_type,f_mid,v_mid,mass,payload,g);

	    for (i = 0; i < size; ++i)
	    for (j = 0; j < dim; ++j)
	    {
		x_new[i][j] += dt*v_mid[i][j]/6.0;
                v_new[i][j] += dt*f_mid[i][j]/6.0;
	    }

	    count = 0;
	    propagate_curve(&geom_set,newc,x_new);
	    assign_node_field(ns,x_new,v_new,&count);
	    assign_curve_field(newc,x_new,v_new,&count);
	    assign_node_field(ne,x_new,v_new,&count);
	    if (n != n_tan-1)
	    {
	    	count = 0;
	    	compute_node_accel(&geom_set,ns,f_old,x_old,v_old,&count);
	    	compute_curve_accel(&geom_set,newc,f_old,x_old,v_old,&count);
	    	compute_node_accel(&geom_set,ne,f_old,x_old,v_old,&count);
	    }
	}
	
	if (debugging("trace"))
	    (void) printf("Leaving "
			"fourth_order_elastic_curve_propagate()\n");
}	/* end fourth_order_elastic_curve_propagate */

extern void fixed_length_tan_curve_propagate(
	Front           *fr,
        Front           *newfr,
        INTERFACE       *intfc,
        CURVE           *oldc,
        CURVE           *newc,
        double           dt)
{
	BOND *b,*bs,*bs2,*be;
	int nb,n;
	double seg_len,total_length,total_length0;
	static boolean first = YES;

	if (debugging("trace"))
	    (void) printf("Entering fixed_length_tan_curve_propagate()\n");
	if (fr->rect_grid->dim != 2) return;

	if (wave_type(newc) != ELASTIC_BOUNDARY)
	    return;

	if (first)
	{
	    first = NO;
	    nb = 0;
	    total_length  = 0.0;
	    for (b = newc->first; b != NULL; b = b->next)
	    {
	    	total_length += bond_length(b);
		nb++;
	    }
	    for (b = newc->first; b != NULL; b = b->next)
		b->length0 = total_length/(double)nb;
	}
	if (debugging("airfoil"))
	{
	    printf("Entering test_tan_curve_propagate1()\n");
	    nb = 0;
	    total_length  = 0.0;
	    total_length0 = 0.0;
	    for (b = newc->first; b != NULL; b = b->next)
	    {
	    	total_length += bond_length(b);
	    	total_length0 += b->length0;
		nb++;
	    }
	    printf("Entering: total_length  = %16.12f\n", total_length);
	    printf("Entering: total_length0 = %16.12f\n", total_length0);
	}

	nb = 0;
	for (b = newc->first; b != NULL; b = b->next) nb++;
	if (nb%2 == 0)	n = nb/2;
	else n = nb/2 + 1;

	seg_len = 0.0;
	bs = newc->first;
	for (nb = 0, b = bs; nb < n; b = b->next) 
	{
	    nb++;
	    seg_len += b->length0;
	    be = b;
	}
	bs2 = be->next;
	FT_CurveSegLengthConstr(newc,bs,be,nb,seg_len,
				BACKWARD_REDISTRIBUTION);

	seg_len = 0.0;
	bs = bs2;
	for (nb = 0, b = bs; b != NULL; b = b->next) 
	{
	    nb++;
	    seg_len += b->length0;
	    be = b;
	}
	FT_CurveSegLengthConstr(newc,bs,be,nb,seg_len,
				FORWARD_REDISTRIBUTION);
	
	total_length = 0.0;
	for (b = newc->first; b != NULL; b = b->next)
	{
	    total_length += bond_length(b);
	    b->length0 = seg_len/(double)nb;
	}
	if (debugging("trace"))
	    (void) printf("Leaving fixed_length_tan_curve_propagate()\n");
}	/* end test_tan_curve_propagate1 */

#define		MAX_SURF_CURVES		10
#define		MAX_SURF_NODES		20

extern void fourth_order_elastic_surf_propagate(
	Front           *fr,
        Front           *newfr,
        INTERFACE       *intfc,
        SURFACE         *olds,
        SURFACE         *news,
        double           fr_dt)
{
	static int size = 0;
	static double **x_old,**x_new,**v_old,**v_new,**f_old,**f_new;
        static double **x_mid,**v_mid,**f_mid;
	AF_PARAMS *af_params = (AF_PARAMS*)fr->extra2;
        double *g = af_params->gravity;
	double mass;
	int i,j,num_pts,count;
	int n,n0,n_tan = af_params->n_tan;
	double dt = fr_dt/(double)n_tan;
	PARACHUTE_SET geom_set;
	CURVE **oc,**nc,*oldc[MAX_SURF_CURVES],*newc[MAX_SURF_CURVES];
	NODE *oldn[MAX_SURF_NODES],*newn[MAX_SURF_NODES];
	int num_nodes,num_lurves;	/* Numbers of nodes and curves */
	void (*compute_node_accel)(PARACHUTE_SET*,NODE*,double**,
				double**,double **,int*);
	void (*compute_curve_accel)(PARACHUTE_SET*,CURVE*,double**,
				double**,double **,int*);
	void (*compute_surf_accel)(PARACHUTE_SET*,SURFACE*,double**,
				double**,double **,int*);

	if (wave_type(olds) != ELASTIC_BOUNDARY)
	    return;

	switch (af_params->spring_model)
	{
	case MODEL1:
	    compute_curve_accel = compute_curve_accel1;
	    compute_node_accel = compute_node_accel1;
	    compute_surf_accel = compute_surf_accel1;
	    break;
	case MODEL2:
	    compute_curve_accel = compute_curve_accel2;
	    compute_node_accel = compute_node_accel2;
	    compute_surf_accel = compute_surf_accel2;
	    break;
	case MODEL3:
	default:
	    (void) printf("Model function not implemented yet!\n");
	    clean_up(ERROR);
	}

	if (debugging("trace"))
	    (void) printf("Entering "
			"fourth_order_elastic_surf_propagate()\n");

	geom_set.ks = af_params->ks;
	geom_set.lambda_s = af_params->lambda_s;
	geom_set.m_s = af_params->m_s;
	geom_set.kl = af_params->kl;
	geom_set.lambda_l = af_params->lambda_l;
	geom_set.m_l = mass = af_params->m_l;
	geom_set.front = fr;
	geom_set.dt = dt;

	/* Assume there is only one closed boundary curve */
	num_nodes = num_lurves = 0;
	for (oc = olds->pos_curves, nc = news->pos_curves; oc && *oc; 
				++oc, ++nc)
	{
	    if (hsbdry_type(*oc) == FIXED_HSBDRY) continue;
	    oldc[num_lurves] = *oc;
	    newc[num_lurves] = *nc;
	    num_lurves++;
	}
	for (oc = olds->neg_curves, nc = news->neg_curves; oc && *oc; 
				++oc, ++nc)
	{
	    if (hsbdry_type(*oc) == FIXED_HSBDRY) continue;
	    oldc[num_lurves] = *oc;
	    newc[num_lurves] = *nc;
	    num_lurves++;
	}
	for (i = 0; i < num_lurves; ++i)
	{
	    if (!pointer_in_list((POINTER)oldc[i]->start,num_nodes,
				(POINTER*)oldn))
	    {
		oldn[num_nodes] = oldc[i]->start;
		newn[num_nodes] = newc[i]->start;
		num_nodes++;
	    }
	    if (is_closed_curve(oldc[i])) continue;
	    if (!pointer_in_list((POINTER)oldc[i]->end,num_nodes,
				(POINTER*)oldn))
	    {
		oldn[num_nodes] = oldc[i]->end;
		newn[num_nodes] = newc[i]->end;
		num_nodes++;
	    }
	}

	num_pts = FT_NumOfSurfPoints(olds);
	if (size < num_pts)
	{
	    size = num_pts;
	    if (v_old != NULL)
	    {
		FT_FreeThese(9,v_old,v_new,x_old,x_new,f_old,f_new,
				v_mid,x_mid,f_mid);
	    }
	    FT_MatrixMemoryAlloc((POINTER*)&x_old,size,3,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&v_old,size,3,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&f_old,size,3,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&x_mid,size,3,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&v_mid,size,3,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&f_mid,size,3,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&x_new,size,3,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&v_new,size,3,sizeof(double));
            FT_MatrixMemoryAlloc((POINTER*)&f_new,size,3,sizeof(double));
	}

	count = 0;
	compute_surf_accel(&geom_set,news,f_old,x_old,v_old,&count);
	for (i = 0; i < num_lurves; ++i)
	{
	    n0 = count;
	    compute_curve_accel(&geom_set,newc[i],f_old,x_old,v_old,&count);
	    adjust_for_curve_type(newc[i],n0,f_old,v_old,mass,g);
	}
	for (i = 0; i < num_nodes; ++i)
	{
	    n0 = count;
	    compute_node_accel(&geom_set,newn[i],f_old,x_old,v_old,&count);
	    adjust_for_cnode_type(newn[i],n0,f_old,v_old,mass,g);
	}

	for (n = 0; n < n_tan; ++n)
	{
	    for (i = 0; i < size; ++i)
            for (j = 0; j < 3; ++j)
            {
		x_new[i][j] = x_old[i][j] + dt*v_old[i][j]/6.0;
                v_new[i][j] = v_old[i][j] + dt*f_old[i][j]/6.0;
                x_mid[i][j] = x_old[i][j] + 0.5*v_old[i][j]*dt;
                v_mid[i][j] = v_old[i][j] + 0.5*f_old[i][j]*dt;
            }
	    count = 0;
            assign_surf_field(news,x_mid,v_mid,&count);
	    for (i = 0; i < num_lurves; ++i)
            	assign_curve_field(newc[i],x_mid,v_mid,&count);
	    for (i = 0; i < num_nodes; ++i)
            	assign_node_field(newn[i],x_mid,v_mid,&count);
	    count = 0;
	    compute_surf_accel(&geom_set,news,f_mid,x_mid,v_mid,&count);
	    for (i = 0; i < num_lurves; ++i)
	    {
		n0 = count;
	    	compute_curve_accel(&geom_set,newc[i],f_mid,x_mid,v_mid,&count);
	    	adjust_for_curve_type(newc[i],n0,f_mid,v_mid,mass,g);
	    }
	    for (i = 0; i < num_nodes; ++i)
	    {
		n0 = count;
	    	compute_node_accel(&geom_set,newn[i],f_mid,x_mid,v_mid,&count);
	    	adjust_for_cnode_type(newn[i],n0,f_old,v_old,mass,g);
	    }

	    for (i = 0; i < size; ++i)
            for (j = 0; j < 3; ++j)
            {
		x_new[i][j] += dt*v_mid[i][j]/3.0;
                v_new[i][j] += dt*f_mid[i][j]/3.0;
                x_mid[i][j] = x_old[i][j] + 0.5*v_mid[i][j]*dt;
                v_mid[i][j] = v_old[i][j] + 0.5*f_mid[i][j]*dt;
            }
	    count = 0;
            assign_surf_field(news,x_mid,v_mid,&count);
	    for (i = 0; i < num_lurves; ++i)
	    {
            	assign_curve_field(newc[i],x_mid,v_mid,&count);
	    }
	    for (i = 0; i < num_nodes; ++i)
            	assign_node_field(newn[i],x_mid,v_mid,&count);
	    count = 0;
	    compute_surf_accel(&geom_set,news,f_mid,x_mid,v_mid,&count);
	    for (i = 0; i < num_lurves; ++i)
	    {
		n0 = count;
	    	compute_curve_accel(&geom_set,newc[i],f_mid,x_mid,v_mid,&count);
	    	adjust_for_curve_type(newc[i],n0,f_mid,v_mid,mass,g);
	    }
	    for (i = 0; i < num_nodes; ++i)
	    {
		n0 = count;
	    	compute_node_accel(&geom_set,newn[i],f_mid,x_mid,v_mid,&count);
	    	adjust_for_cnode_type(newn[i],n0,f_old,v_old,mass,g);
	    }

	    for (i = 0; i < size; ++i)
            for (j = 0; j < 3; ++j)
            {
		x_new[i][j] += dt*v_mid[i][j]/3.0;
                v_new[i][j] += dt*f_mid[i][j]/3.0;
                x_mid[i][j] = x_old[i][j] + v_mid[i][j]*dt;
                v_mid[i][j] = v_old[i][j] + f_mid[i][j]*dt;
            }
	    count = 0;
            assign_surf_field(news,x_mid,v_mid,&count);
	    for (i = 0; i < num_lurves; ++i)
            	assign_curve_field(newc[i],x_mid,v_mid,&count);
	    for (i = 0; i < num_nodes; ++i)
            	assign_node_field(newn[i],x_mid,v_mid,&count);
	    count = 0;
	    compute_surf_accel(&geom_set,news,f_mid,x_mid,v_mid,&count);
	    for (i = 0; i < num_lurves; ++i)
	    {
		n0 = count;
	    	compute_curve_accel(&geom_set,newc[i],f_mid,x_mid,v_mid,&count);
	    	adjust_for_curve_type(newc[i],n0,f_mid,v_mid,mass,g);
	    }
	    for (i = 0; i < num_nodes; ++i)
	    {
		n0 = count;
	    	compute_node_accel(&geom_set,newn[i],f_mid,x_mid,v_mid,&count);
	    	adjust_for_cnode_type(newn[i],n0,f_old,v_old,mass,g);
	    }

	    for (i = 0; i < size; ++i)
            for (j = 0; j < 3; ++j)
            {
		x_new[i][j] += dt*v_mid[i][j]/6.0;
                v_new[i][j] += dt*f_mid[i][j]/6.0;
            }
	    count = 0;
            assign_surf_field(news,x_new,v_new,&count);
	    for (i = 0; i < num_lurves; ++i)
            	assign_curve_field(newc[i],x_new,v_new,&count);
	    for (i = 0; i < num_nodes; ++i)
            	assign_node_field(newn[i],x_new,v_new,&count);
	    if (n != n_tan-1)
	    {
	    	count = 0;
	    	compute_surf_accel(&geom_set,news,f_old,x_old,v_old,&count);
	    	for (i = 0; i < num_lurves; ++i)
		{
		    n0 = count;
	    	    compute_curve_accel(&geom_set,newc[i],f_old,x_old,
						v_old,&count);
	    	    adjust_for_curve_type(newc[i],n0,f_mid,v_mid,mass,g);
		}
	    	for (i = 0; i < num_nodes; ++i)
		{
		    n0 = count;
	    	    compute_node_accel(&geom_set,newn[i],f_old,x_old,
						v_old,&count);
	    	    adjust_for_cnode_type(newn[i],n0,f_old,v_old,mass,g);
	    	}
	    }
	}

	if (debugging("trace"))
	    (void) printf("Leaving "
			"fourth_order_elastic_surf_propagate()\n");
}	/* end fourth_order_elastic_surf_propagate */

static void adjust_for_curve_type(
	CURVE *c,
	int index0,
	double **f,
	double **v,
	double mass,
	double *g)
{
	C_PARAMS *c_params =  (C_PARAMS*)c->extra;
	int j,dir,dim = 3;
	double payload;
	BOND *b;
	int n,index = index0;
	double ave_accel;

	if (c_params == NULL) return;
	dir = c_params->dir;
	payload = c_params->point_mass;

	if (c_params->load_type == NO_LOAD) return;
	else if (c_params->load_type == FREE_LOAD)
	{
	    for (b = c->first; b != c->last; b = b->next)
	    {
	    	for (j = 0; j < dim; ++j)
                    f[index][j] = f[index][j]*mass/payload + g[j];
		index++;
	    }
	}
	else if (c_params->load_type == RIGID_LOAD)
	{
	    ave_accel = 0.0;
	    n = 0;
	    for (b = c->first; b != c->last; b = b->next)
	    {
            	ave_accel += f[index][dir];
		n++;
		index++;
	    }
	    ave_accel /= n;
	    index = index0;
	    for (b = c->first; b != c->last; b = b->next)
	    {
	    	for (j = 0; j < dim; ++j)
	    	{
		    if (j == dir)
            	    	f[index][dir] = ave_accel*mass/payload + g[dir];
		    else
		    	f[index][j] = v[index][j] = 0.0;
	    	}
		index++;
	    }
	}
}	/* end adjust_for_curve_type */

static void adjust_for_cnode_type(
	NODE *n,
	int index,
	double **f,
	double **v,
	double mass,
	double *g)
{
	CURVE **c;
	C_PARAMS *c_params =  NULL;
	int j,dir,dim = 3;
	double payload;
	for (c = n->in_curves; c && *c; ++c)
	{
	    if ((*c)->extra != NULL)
	    	c_params = (C_PARAMS*)(*c)->extra;
	}
	for (c = n->out_curves; c && *c; ++c)
	{
	    if ((*c)->extra != NULL)
	    	c_params = (C_PARAMS*)(*c)->extra;
	}
	if (c_params == NULL) return;

	dir = c_params->dir;
	payload = c_params->point_mass;
	if (c_params->load_type == NO_LOAD) return;
	else if (c_params->load_type == FREE_LOAD)
	{
	    for (j = 0; j < dim; ++j)
                f[index][j] = f[index][j]*mass/payload + g[j];
	}
	else if (c_params->load_type == RIGID_LOAD)
	{
	    for (j = 0; j < dim; ++j)
	    {
		if (j == dir)
            	    f[index][dir] = f[index][dir]*mass/payload + g[dir];
		else
		    f[index][j] = v[index][j] = 0.0;
	    }
	}
}	/* end adjust_for_cnode_type */

static void adjust_for_node_type(
	NODE *n,
	int index,
	STRING_NODE_TYPE end_type,
	double **f,
	double **v,
	double mass,
	double payload,
	double *g)
{
	int j;
	int dim = n->interface->dim;

	if (end_type == FIXED_END)
	{
	    for (j = 0; j < dim; ++j)
		f[index][j] = v[index][j] = 0.0;
	}
	else if (end_type == LOADED_END)
	{
	    for (j = 0; j < dim; ++j)
		f[index][j] = f[index][j]*mass/payload + g[j];
	}
}	/* end adjust_for_node_type */

static void propagate_curve(
	PARACHUTE_SET *geom_set,
	CURVE *curve,
	double **x)
{
	int j;
	POINT *p;
	BOND *b;
	STATE *sl,*sr;
	HYPER_SURF_ELEMENT *hse;
        HYPER_SURF         *hs;
	double *v;
	double dt = geom_set->dt;
	int n = 1;

	if (debugging("trace"))
	    (void) printf("Entering propagate_curve()\n");
	hs = Hyper_surf(curve);
	for (b = curve->first;  b != curve->last; b = b->next)
	{
	    hse = Hyper_surf_element(b);
	    p = b->end;
	    FT_GetStatesAtPoint(p,hse,hs,(POINTER*)&sl,(POINTER*)&sr);
	    v = sl->vel;
	    for (j = 0; j < 2; ++j)
		x[n][j] += v[j]*dt;
	    ++n;
	}
}	/* end propagate_curve */

static void print_airfoil_stat2d_2(
	Front *front,
	char *out_name)
{
	INTERFACE *intfc = front->interf;
	CURVE **c,*curve;
	BOND *b;
	POINT *p;
	static FILE *ekfile,*epfile,*exfile,*efile,*sfile;
	char fname[256];
	AF_PARAMS *af_params = (AF_PARAMS*)front->extra2;
	double ek,ep,enp;
	double kl,m_l,x_diff;
	int i,dim = intfc->dim;
	double str_length;
	STRING_NODE_TYPE start_type = af_params->start_type;
        STRING_NODE_TYPE end_type = af_params->end_type;
	double *g,payload;
	double vect[MAXD],len0;

	if (ekfile == NULL && pp_mynode() == 0)
        {
	    sprintf(fname,"%s/ek.xg",out_name);
            ekfile = fopen(fname,"w");
	    sprintf(fname,"%s/ep.xg",out_name);
            epfile = fopen(fname,"w");
	    sprintf(fname,"%s/ex.xg",out_name);
            exfile = fopen(fname,"w");
	    sprintf(fname,"%s/en.xg",out_name);
            efile = fopen(fname,"w");
	    sprintf(fname,"%s/str_length.xg",out_name);
            sfile = fopen(fname,"w");
            fprintf(ekfile,"\"Kinetic enegy vs. time\"\n");
            fprintf(epfile,"\"Potential enegy vs. time\"\n");
            fprintf(exfile,"\"External enegy vs. time\"\n");
            fprintf(efile,"\"Total enegy vs. time\"\n");
            fprintf(sfile,"\"String length vs. time\"\n");
        }

	kl = af_params->kl;
        m_l = af_params->m_l;
	payload = af_params->payload;
	g = af_params->gravity;
	ek = ep = enp = str_length = 0.0;
	for (c = intfc->curves; c && *c; ++c)
	{
		if (wave_type(*c) != ELASTIC_BOUNDARY)
		continue;

		curve = *c;
		p = curve->first->start;
		if (start_type == FREE_END)
		{
		    for (i = 0; i < dim; ++i)
		    {
		    	ek += 0.5*m_l*sqr(p->vel[i]);
		    }
		}
		else if(start_type == LOADED_END)
		{
		    for (i = 0; i < dim; ++i)
		    {
		    	ek += 0.5*payload*sqr(p->vel[i]);
			enp -= payload*g[i]*Coords(p)[i];
		    }
		}
		p = curve->last->end;
		if (end_type == FREE_END)
		{
		    for (i = 0; i < dim; ++i)
		    {
		    	ek += 0.5*m_l*sqr(p->vel[i]);
		    }
		}
		else if(end_type == LOADED_END)
		{
		    for (i = 0; i < dim; ++i)
		    {
		    	ek += 0.5*payload*sqr(p->vel[i]);
			enp -= payload*g[i]*Coords(p)[i];
		    }
		}

		for (b = curve->first; b != NULL; b = b->next)
		{
		    p = b->end;
		    x_diff = bond_length(b) - bond_length0(b);
		    if (b != curve->last)
		    {
		    	for (i = 0; i < dim; ++i)
			{
		    	    ek += 0.5*m_l*sqr(p->vel[i]);
			}
		    }
		    len0 = bond_length0(b);
		    x_diff = 0.0;
		    for (i = 0; i < dim; ++i)
		    {
			vect[i] = Coords(b->end)[i] - Coords(b->start)[i]
					- len0*b->dir0[i];
			x_diff += sqr(vect[i]);
		    }
		    ep += 0.5*kl*x_diff;
		    str_length += bond_length(b);
		}
	}
	if (pp_mynode() == 0)
	{
	    fprintf(ekfile,"%16.12f  %16.12f\n",front->time,ek);
            fprintf(epfile,"%16.12f  %16.12f\n",front->time,ep);
            fprintf(exfile,"%16.12f  %16.12f\n",front->time,enp);
            fprintf(efile,"%16.12f  %16.12f\n",front->time,ek+ep+enp);
            fprintf(sfile,"%16.12f  %16.12f\n",front->time,str_length);
	    fflush(ekfile);
	    fflush(epfile);
	    fflush(exfile);
	    fflush(efile);
	    fflush(sfile);
	}
}	/* end print_airfoil_stat2d_2 */

static void record_stretching_length(
	SURFACE *surf,
	char *out_name,
	double time)
{
	int dir;
	CURVE **c,*c1,*c2;
	static FILE *lfile;
	char lname[200];
	C_PARAMS *c_params;
	POINT *p1,*p2;
	double length;

	c1 = c2 = NULL;
	for (c = surf->pos_curves; c && *c; ++c)
	{
	    if (hsbdry_type(*c) == MONO_COMP_HSBDRY &&
		(*c)->extra != NULL)
	    {
		c1 = *c;
		c_params = (C_PARAMS*)c1->extra;
	    }
	    else if (hsbdry_type(*c) == FIXED_HSBDRY)
		c2 = *c;
	}
	for (c = surf->neg_curves; c && *c; ++c)
	{
	    if (hsbdry_type(*c) == MONO_COMP_HSBDRY &&
		(*c)->extra != NULL)
	    {
		c1 = *c;
		c_params = (C_PARAMS*)c1->extra;
	    }
	    else if (hsbdry_type(*c) == FIXED_HSBDRY)
		c2 = *c;
	}
	if (c1 == NULL || c2 == NULL) return;
	if (lfile == NULL)
	{
	    sprintf(lname,"%s/length.xg",out_name);
	    lfile = fopen(lname,"w");
	    fprintf(lfile,"\"Length vs. Time\"\n");
	}
	dir = c_params->dir;
	p1 = c1->first->end;
	p2 = c2->first->end;
	length = fabs(Coords(p1)[dir] - Coords(p2)[dir]);
	fprintf(lfile,"%f  %f\n",time,length);
	fflush(lfile);
}	/* end record_stretching_length */
