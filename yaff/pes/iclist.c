// YAFF is yet another force-field code
// Copyright (C) 2011 - 2013 Toon Verstraelen <Toon.Verstraelen@UGent.be>,
// Louis Vanduyfhuys <Louis.Vanduyfhuys@UGent.be>, Center for Molecular Modeling
// (CMM), Ghent University, Ghent, Belgium; all rights reserved unless otherwise
// stated.
//
// This file is part of YAFF.
//
// YAFF is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 3
// of the License, or (at your option) any later version.
//
// YAFF is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>
//
//--

#include <math.h>
#include "iclist.h"

typedef double (*ic_forward_type)(iclist_row_type*, dlist_row_type*);

double forward_bond(iclist_row_type* ic, dlist_row_type* deltas) {
  double *delta;
  delta = (double*)(deltas + (*ic).i0);
  return sqrt(delta[0]*delta[0] + delta[1]*delta[1] + delta[2]*delta[2]);
}

double forward_bend_cos(iclist_row_type* ic, dlist_row_type* deltas) {
  double *delta0, *delta1;
  double d0, d1, dot;
  delta0 = (double*)(deltas + (*ic).i0);
  delta1 = (double*)(deltas + (*ic).i1);
  d0 = sqrt(delta0[0]*delta0[0] + delta0[1]*delta0[1] + delta0[2]*delta0[2]);
  d1 = sqrt(delta1[0]*delta1[0] + delta1[1]*delta1[1] + delta1[2]*delta1[2]);
  if ((d0 == 0) || (d1 == 0)) return 0.0;
  dot = delta0[0]*delta1[0] + delta0[1]*delta1[1] + delta0[2]*delta1[2];
  return (*ic).sign0*(*ic).sign1*dot/d0/d1;
}

double forward_bend_angle(iclist_row_type* ic, dlist_row_type* deltas) {
  double c;
  c = forward_bend_cos(ic, deltas);
  return acos(c);
}

double forward_dihed_cos(iclist_row_type* ic, dlist_row_type* deltas) {
  long i;
  double *delta0, *delta1, *delta2;
  double a[3], b[3];
  double tmp0, tmp1, tmp2;
  delta0 = (double*)(deltas + (*ic).i0);
  delta1 = (double*)(deltas + (*ic).i1);
  delta2 = (double*)(deltas + (*ic).i2);
  tmp1 = sqrt(delta1[0]*delta1[0] + delta1[1]*delta1[1] + delta1[2]*delta1[2]);
  tmp0 = (delta0[0]*delta1[0] + delta0[1]*delta1[1] + delta0[2]*delta1[2])/tmp1;
  tmp2 = (delta1[0]*delta2[0] + delta1[1]*delta2[1] + delta1[2]*delta2[2])/tmp1;
  for (i=0; i<3; i++) {
    a[i] = delta0[i] - tmp0*delta1[i]/tmp1;
    b[i] = delta2[i] - tmp2*delta1[i]/tmp1;
  }
  tmp0 = sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
  tmp2 = sqrt(b[0]*b[0] + b[1]*b[1] + b[2]*b[2]);
  tmp1 = a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
  return (*ic).sign0*(*ic).sign2*tmp1/tmp0/tmp2;
}

double forward_dihed_angle(iclist_row_type* ic, dlist_row_type* deltas) {
  double c;
  c = forward_dihed_cos(ic, deltas);
  // Guard against round-off errors before taking the dot product.
  if (c > 1) {
    c = 1;
  } else if (c < -1) {
    c = -1;
  }
  return acos(c);
}

double forward_oop_cos(iclist_row_type* ic, dlist_row_type* deltas) {
  double *delta0, *delta1, *delta2;
  double n[3];
  double n_sq, tmp0, tmp1;
  delta0 = (double*)(deltas + (*ic).i0);
  delta1 = (double*)(deltas + (*ic).i1);
  delta2 = (double*)(deltas + (*ic).i2);
  // The normal to the plane spanned by the first and second vector
  n[0] = delta0[1]*delta1[2] - delta0[2]*delta1[1];
  n[1] = delta0[2]*delta1[0] - delta0[0]*delta1[2];
  n[2] = delta0[0]*delta1[1] - delta0[1]*delta1[0];
  // The norm squared of this normal
  n_sq = n[0]*n[0] + n[1]*n[1] + n[2]*n[2];
  // Norm squared of the third vector
  tmp0 = delta2[0]*delta2[0] + delta2[1]*delta2[1] + delta2[2]*delta2[2];
  // Dot product of the normal with the third vector
  tmp1 = n[0]*delta2[0] + n[1]*delta2[1] + n[2]*delta2[2];
  // The cosine of the oop angle (assumed to be positive)
  return sqrt(1.0 - tmp1*tmp1/tmp0/n_sq);
}

double forward_oop_angle(iclist_row_type* ic, dlist_row_type* deltas) {
  double c;
  c = forward_oop_cos(ic, deltas);
  // Guard against round-off errors before taking the dot product.
  if (c > 1) {
    c = 1;
  } else if (c < -1) {
    c = -1;
  }
  return acos(c);
}

ic_forward_type ic_forward_fns[8] = {
  forward_bond, forward_bend_cos, forward_bend_angle, forward_dihed_cos, forward_dihed_angle, forward_bond,
  forward_oop_cos, forward_oop_angle
};

void iclist_forward(dlist_row_type* deltas, iclist_row_type* ictab, long nic) {
  long i;
  for (i=0; i<nic; i++) {
    ictab[i].value = ic_forward_fns[ictab[i].kind](ictab + i, deltas);
    ictab[i].grad = 0.0;
  }
}


typedef void (*ic_back_type)(iclist_row_type*, dlist_row_type*, double, double);

void back_bond(iclist_row_type* ic, dlist_row_type* deltas, double value, double grad) {
  dlist_row_type *delta;
  double x;
  delta = deltas + (*ic).i0;
  x = grad/value;
  (*delta).gx += x*(*delta).dx;
  (*delta).gy += x*(*delta).dy;
  (*delta).gz += x*(*delta).dz;
}

void back_bend_cos(iclist_row_type* ic, dlist_row_type* deltas, double value, double grad) {
  dlist_row_type *delta0, *delta1;
  double e0[3], e1[3];
  double d0, d1, fac;
  delta0 = deltas + (*ic).i0;
  delta1 = deltas + (*ic).i1;
  d0 = sqrt((*delta0).dx*(*delta0).dx + (*delta0).dy*(*delta0).dy + (*delta0).dz*(*delta0).dz);
  d1 = sqrt((*delta1).dx*(*delta1).dx + (*delta1).dy*(*delta1).dy + (*delta1).dz*(*delta1).dz);
  e0[0] = (*delta0).dx/d0;
  e0[1] = (*delta0).dy/d0;
  e0[2] = (*delta0).dz/d0;
  e1[0] = (*delta1).dx/d1;
  e1[1] = (*delta1).dy/d1;
  e1[2] = (*delta1).dz/d1;
  fac = (*ic).sign0*(*ic).sign1;
  grad *= fac;
  value *= fac;
  fac = grad/d0;
  (*delta0).gx += fac*(e1[0] - value*e0[0]);
  (*delta0).gy += fac*(e1[1] - value*e0[1]);
  (*delta0).gz += fac*(e1[2] - value*e0[2]);
  fac = grad/d1;
  (*delta1).gx += fac*(e0[0] - value*e1[0]);
  (*delta1).gy += fac*(e0[1] - value*e1[1]);
  (*delta1).gz += fac*(e0[2] - value*e1[2]);
}

void back_bend_angle(iclist_row_type* ic, dlist_row_type* deltas, double value, double grad) {
  back_bend_cos(ic, deltas, cos(value), -grad/sin(value));
}

void back_dihed_cos(iclist_row_type* ic, dlist_row_type* deltas, double value, double grad) {
  long i;
  dlist_row_type *delta0, *delta1, *delta2;
  double a[3], b[3], dcos_da[3], dcos_db[3], da_ddel0[9], da_ddel1[9], db_ddel1[9];
  double dot0, dot2, n1, na, nb;

  delta0 = deltas + (*ic).i0;
  delta1 = deltas + (*ic).i1;
  delta2 = deltas + (*ic).i2;
  n1   = sqrt((*delta1).dx*(*delta1).dx + (*delta1).dy*(*delta1).dy + (*delta1).dz*(*delta1).dz);
  dot0 =      (*delta0).dx*(*delta1).dx + (*delta0).dy*(*delta1).dy + (*delta0).dz*(*delta1).dz;
  dot2 =      (*delta1).dx*(*delta2).dx + (*delta1).dy*(*delta2).dy + (*delta1).dz*(*delta2).dz;

  a[0] = ( (*delta0).dx - dot0*(*delta1).dx/(n1*n1) );
  a[1] = ( (*delta0).dy - dot0*(*delta1).dy/(n1*n1) );
  a[2] = ( (*delta0).dz - dot0*(*delta1).dz/(n1*n1) );
  b[0] = ( (*delta2).dx - dot2*(*delta1).dx/(n1*n1) );
  b[1] = ( (*delta2).dy - dot2*(*delta1).dy/(n1*n1) );
  b[2] = ( (*delta2).dz - dot2*(*delta1).dz/(n1*n1) );

  na = sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
  nb = sqrt(b[0]*b[0] + b[1]*b[1] + b[2]*b[2]);

  value *= (*ic).sign0*(*ic).sign2;
  grad *= (*ic).sign0*(*ic).sign2;

  for (i=0; i<3; i++) {
    dcos_da[i] = (b[i]/nb - value*a[i]/na)/na;
    dcos_db[i] = (a[i]/na - value*b[i]/nb)/nb;
  }

  da_ddel0[0] = 1 - (*delta1).dx*(*delta1).dx/(n1*n1);
  da_ddel0[1] =   - (*delta1).dx*(*delta1).dy/(n1*n1);
  da_ddel0[2] =   - (*delta1).dx*(*delta1).dz/(n1*n1);
  da_ddel0[3] =   - (*delta1).dy*(*delta1).dx/(n1*n1);
  da_ddel0[4] = 1 - (*delta1).dy*(*delta1).dy/(n1*n1);
  da_ddel0[5] =   - (*delta1).dy*(*delta1).dz/(n1*n1);
  da_ddel0[6] =   - (*delta1).dz*(*delta1).dx/(n1*n1);
  da_ddel0[7] =   - (*delta1).dz*(*delta1).dy/(n1*n1);
  da_ddel0[8] = 1 - (*delta1).dz*(*delta1).dz/(n1*n1);

  da_ddel1[0] = ( - dot0/(n1*n1) - (*delta0).dx*(*delta1).dx/(n1*n1) + 2*dot0/(n1*n1*n1*n1)*(*delta1).dx*(*delta1).dx );
  da_ddel1[1] = (                - (*delta0).dx*(*delta1).dy/(n1*n1) + 2*dot0/(n1*n1*n1*n1)*(*delta1).dx*(*delta1).dy );
  da_ddel1[2] = (                - (*delta0).dx*(*delta1).dz/(n1*n1) + 2*dot0/(n1*n1*n1*n1)*(*delta1).dx*(*delta1).dz );
  da_ddel1[3] = (                - (*delta0).dy*(*delta1).dx/(n1*n1) + 2*dot0/(n1*n1*n1*n1)*(*delta1).dy*(*delta1).dx );
  da_ddel1[4] = ( - dot0/(n1*n1) - (*delta0).dy*(*delta1).dy/(n1*n1) + 2*dot0/(n1*n1*n1*n1)*(*delta1).dy*(*delta1).dy );
  da_ddel1[5] = (                - (*delta0).dy*(*delta1).dz/(n1*n1) + 2*dot0/(n1*n1*n1*n1)*(*delta1).dy*(*delta1).dz );
  da_ddel1[6] = (                - (*delta0).dz*(*delta1).dx/(n1*n1) + 2*dot0/(n1*n1*n1*n1)*(*delta1).dz*(*delta1).dx );
  da_ddel1[7] = (                - (*delta0).dz*(*delta1).dy/(n1*n1) + 2*dot0/(n1*n1*n1*n1)*(*delta1).dz*(*delta1).dy );
  da_ddel1[8] = ( - dot0/(n1*n1) - (*delta0).dz*(*delta1).dz/(n1*n1) + 2*dot0/(n1*n1*n1*n1)*(*delta1).dz*(*delta1).dz );

  db_ddel1[0] = ( - dot2/(n1*n1) - (*delta2).dx*(*delta1).dx/(n1*n1) + 2*dot2/(n1*n1*n1*n1)*(*delta1).dx*(*delta1).dx );
  db_ddel1[1] = (                - (*delta2).dx*(*delta1).dy/(n1*n1) + 2*dot2/(n1*n1*n1*n1)*(*delta1).dx*(*delta1).dy );
  db_ddel1[2] = (                - (*delta2).dx*(*delta1).dz/(n1*n1) + 2*dot2/(n1*n1*n1*n1)*(*delta1).dx*(*delta1).dz );
  db_ddel1[3] = (                - (*delta2).dy*(*delta1).dx/(n1*n1) + 2*dot2/(n1*n1*n1*n1)*(*delta1).dy*(*delta1).dx );
  db_ddel1[4] = ( - dot2/(n1*n1) - (*delta2).dy*(*delta1).dy/(n1*n1) + 2*dot2/(n1*n1*n1*n1)*(*delta1).dy*(*delta1).dy );
  db_ddel1[5] = (                - (*delta2).dy*(*delta1).dz/(n1*n1) + 2*dot2/(n1*n1*n1*n1)*(*delta1).dy*(*delta1).dz );
  db_ddel1[6] = (                - (*delta2).dz*(*delta1).dx/(n1*n1) + 2*dot2/(n1*n1*n1*n1)*(*delta1).dz*(*delta1).dx );
  db_ddel1[7] = (                - (*delta2).dz*(*delta1).dy/(n1*n1) + 2*dot2/(n1*n1*n1*n1)*(*delta1).dz*(*delta1).dy );
  db_ddel1[8] = ( - dot2/(n1*n1) - (*delta2).dz*(*delta1).dz/(n1*n1) + 2*dot2/(n1*n1*n1*n1)*(*delta1).dz*(*delta1).dz );

  (*delta0).gx += grad*(  dcos_da[0]*da_ddel0[0] + dcos_da[1]*da_ddel0[3] + dcos_da[2]*da_ddel0[6]);
  (*delta0).gy += grad*(  dcos_da[0]*da_ddel0[1] + dcos_da[1]*da_ddel0[4] + dcos_da[2]*da_ddel0[7]);
  (*delta0).gz += grad*(  dcos_da[0]*da_ddel0[2] + dcos_da[1]*da_ddel0[5] + dcos_da[2]*da_ddel0[8]);
  (*delta1).gx += grad*(  dcos_da[0]*da_ddel1[0] + dcos_da[1]*da_ddel1[3] + dcos_da[2]*da_ddel1[6]
                        + dcos_db[0]*db_ddel1[0] + dcos_db[1]*db_ddel1[3] + dcos_db[2]*db_ddel1[6]);
  (*delta1).gy += grad*(  dcos_da[0]*da_ddel1[1] + dcos_da[1]*da_ddel1[4] + dcos_da[2]*da_ddel1[7]
                        + dcos_db[0]*db_ddel1[1] + dcos_db[1]*db_ddel1[4] + dcos_db[2]*db_ddel1[7]);
  (*delta1).gz += grad*(  dcos_da[0]*da_ddel1[2] + dcos_da[1]*da_ddel1[5] + dcos_da[2]*da_ddel1[8]
                        + dcos_db[0]*db_ddel1[2] + dcos_db[1]*db_ddel1[5] + dcos_db[2]*db_ddel1[8]);
  (*delta2).gx += grad*(  dcos_db[0]*da_ddel0[0] + dcos_db[1]*da_ddel0[3] + dcos_db[2]*da_ddel0[6]);
  (*delta2).gy += grad*(  dcos_db[0]*da_ddel0[1] + dcos_db[1]*da_ddel0[4] + dcos_db[2]*da_ddel0[7]);
  (*delta2).gz += grad*(  dcos_db[0]*da_ddel0[2] + dcos_db[1]*da_ddel0[5] + dcos_db[2]*da_ddel0[8]);
}

void back_dihed_angle(iclist_row_type* ic, dlist_row_type* deltas, double value, double grad) {
  double tmp = sin(value);
  if (tmp!=0.0) tmp = -grad/tmp;
  back_dihed_cos(ic, deltas, cos(value), tmp);
}

void back_oop_cos(iclist_row_type* ic, dlist_row_type* deltas, double value, double grad) {
  // This calculation is tedious. Expressions are checked with the following
  // maple commands (assuming the maple worksheet is bug-free)
  /*
  restart:
  with(LinearAlgebra):
  d0:=Vector([d0x,d0y,d0z]):
  d1:=Vector([d1x,d1y,d1z]):
  d2:=Vector([d2x,d2y,d2z]):
  assume(d0x,real,d0y,real,d0z,real,d1x,real,d1y,real,d1z,real,d2x,real,d2y,real,d2z,real):
  d0_cross_d1:=CrossProduct(d0,d1):
  d1_cross_d2:=CrossProduct(d1,d2):
  d2_cross_d0:=CrossProduct(d2,d0):
  n:=d0_cross_d1:
  n_sq:=Norm(n,2,conjugate=false)**2:
  d2_sq:=Norm(d2,2,conjugate=false)**2:
  n_dot_d2:=DotProduct(n,d2):
  fac:=n_dot_d2/d2_sq/n_sq:
  f:=DotProduct(d2,CrossProduct(d0,d1))/Norm(d2,2,conjugate=false)/Norm(CrossProduct(d0,d1),2,conjugate=false):
  cosphi:= (1-f**2)**(1/2):

  dcosphi_d0x:=diff(cosphi,d0x):
  dcosphi_d0y:=diff(cosphi,d0y):
  dcosphi_d0z:=diff(cosphi,d0z):
  dcosphi_d1x:=diff(cosphi,d1x):
  dcosphi_d1y:=diff(cosphi,d1y):
  dcosphi_d1z:=diff(cosphi,d1z):
  dcosphi_d2x:=diff(cosphi,d2x):
  dcosphi_d2y:=diff(cosphi,d2y):
  dcosphi_d2z:=diff(cosphi,d2z):

  simplify(dcosphi_d0x + fac/cosphi*( d1_cross_d2[1] - n_dot_d2/n_sq*(d1[2]*n[3]-d1[3]*n[2])));
  simplify(dcosphi_d0y + fac/cosphi*( d1_cross_d2[2] - n_dot_d2/n_sq*(d1[3]*n[1]-d1[1]*n[3])));
  simplify(dcosphi_d0z + fac/cosphi*( d1_cross_d2[3] - n_dot_d2/n_sq*(d1[1]*n[2]-d1[2]*n[1])));
  simplify(dcosphi_d1x + fac/cosphi*( d2_cross_d0[1] - n_dot_d2/n_sq*(d0[3]*n[2]-d0[2]*n[3])));
  simplify(dcosphi_d1y + fac/cosphi*( d2_cross_d0[2] - n_dot_d2/n_sq*(d0[1]*n[3]-d0[3]*n[1])));
  simplify(dcosphi_d1z + fac/cosphi*( d2_cross_d0[3] - n_dot_d2/n_sq*(d0[2]*n[1]-d0[1]*n[2])));
  simplify(dcosphi_d2x + fac/cosphi*( d0_cross_d1[1] - n_dot_d2/d2_sq*d2[1]) );
  simplify(dcosphi_d2y + fac/cosphi*( d0_cross_d1[2] - n_dot_d2/d2_sq*d2[2]) );
  simplify(dcosphi_d2z + fac/cosphi*( d0_cross_d1[3] - n_dot_d2/d2_sq*d2[3]) );
  */


  dlist_row_type *delta0, *delta1, *delta2;
  double n[3], d0_cross_d1[3], d1_cross_d2[3], d2_cross_d0[3];
  double n_sq, d2_sq, n_dot_d2, fac;

  delta0 = deltas + (*ic).i0;
  delta1 = deltas + (*ic).i1;
  delta2 = deltas + (*ic).i2;
  // Cross products of delta vectors (introduce a function vectorproduct() ?)
  d0_cross_d1[0] = (*delta0).dy * (*delta1).dz - (*delta0).dz * (*delta1).dy;
  d0_cross_d1[1] = (*delta0).dz * (*delta1).dx - (*delta0).dx * (*delta1).dz;
  d0_cross_d1[2] = (*delta0).dx * (*delta1).dy - (*delta0).dy * (*delta1).dx;

  d1_cross_d2[0] = (*delta1).dy * (*delta2).dz - (*delta1).dz * (*delta2).dy;
  d1_cross_d2[1] = (*delta1).dz * (*delta2).dx - (*delta1).dx * (*delta2).dz;
  d1_cross_d2[2] = (*delta1).dx * (*delta2).dy - (*delta1).dy * (*delta2).dx;

  d2_cross_d0[0] = (*delta2).dy * (*delta0).dz - (*delta2).dz * (*delta0).dy;
  d2_cross_d0[1] = (*delta2).dz * (*delta0).dx - (*delta2).dx * (*delta0).dz;
  d2_cross_d0[2] = (*delta2).dx * (*delta0).dy - (*delta2).dy * (*delta0).dx;

  n[0] = d0_cross_d1[0], n[1] = d0_cross_d1[1],n[2] = d0_cross_d1[2]; //normal to plane of first two vectors
  // The squared norm of crossproduct of first two vectors
  n_sq = n[0]*n[0] + n[1]*n[1] + n[2]*n[2];
  // Squared norm of the third vector
  d2_sq = (*delta2).dx * (*delta2).dx + (*delta2).dy * (*delta2).dy + (*delta2).dz * (*delta2).dz;
  // Dot product of the crossproduct of first two vectors with the third vector
  n_dot_d2 = n[0]*(*delta2).dx + n[1]*(*delta2).dy + n[2]*(*delta2).dz;
  // The expression for the cosine of the out-of-plane angle can be written as:
  // cos(phi) = sqrt(1-f**2), so the derivatives can be computed as
  // d cos(phi) / d x = -f / sqrt(1-f**2) * d f / d x
  fac = n_dot_d2/d2_sq/n_sq;
  (*delta0).gx += -fac/value*grad*( d1_cross_d2[0] - n_dot_d2/n_sq*( (*delta1).dy*n[2] - (*delta1).dz*n[1] ) );
  (*delta0).gy += -fac/value*grad*( d1_cross_d2[1] - n_dot_d2/n_sq*( (*delta1).dz*n[0] - (*delta1).dx*n[2] ) );
  (*delta0).gz += -fac/value*grad*( d1_cross_d2[2] - n_dot_d2/n_sq*( (*delta1).dx*n[1] - (*delta1).dy*n[0] ) );
  (*delta1).gx += -fac/value*grad*( d2_cross_d0[0] - n_dot_d2/n_sq*( (*delta0).dz*n[1] - (*delta0).dy*n[2] ) );
  (*delta1).gy += -fac/value*grad*( d2_cross_d0[1] - n_dot_d2/n_sq*( (*delta0).dx*n[2] - (*delta0).dz*n[0] ) );
  (*delta1).gz += -fac/value*grad*( d2_cross_d0[2] - n_dot_d2/n_sq*( (*delta0).dy*n[0] - (*delta0).dx*n[1] ) );
  (*delta2).gx += -fac/value*grad*( d0_cross_d1[0] - n_dot_d2/d2_sq*(*delta2).dx );
  (*delta2).gy += -fac/value*grad*( d0_cross_d1[1] - n_dot_d2/d2_sq*(*delta2).dy );
  (*delta2).gz += -fac/value*grad*( d0_cross_d1[2] - n_dot_d2/d2_sq*(*delta2).dz );
}

void back_oop_angle(iclist_row_type* ic, dlist_row_type* deltas, double value, double grad) {
  double tmp = sin(value);
  if (tmp!=0.0) tmp = -grad/tmp;
  back_oop_cos(ic, deltas, cos(value), tmp);
}

ic_back_type ic_back_fns[8] = {
  back_bond, back_bend_cos, back_bend_angle, back_dihed_cos, back_dihed_angle, back_bond,
  back_oop_cos, back_oop_angle
};

void iclist_back(dlist_row_type* deltas, iclist_row_type* ictab, long nic) {
  long i;
  for (i=0; i<nic; i++) {
    ic_back_fns[ictab[i].kind](ictab + i, deltas, ictab[i].value, ictab[i].grad);
  }
}
