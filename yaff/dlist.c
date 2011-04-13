// YAFF is yet another force-field code
// Copyright (C) 2008 - 2011 Toon Verstraelen <Toon.Verstraelen@UGent.be>, Center
// for Molecular Modeling (CMM), Ghent University, Ghent, Belgium; all rights
// reserved unless otherwise stated.
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
// --


#include "dlist.h"
#include "mic.h"

void dlist_forward(double *pos, double *rvecs, double *gvecs, long nvec, dlist_row_type* deltas, long ndelta) {
  long k;
  dlist_row_type *delta;
  for (k=0; k<ndelta; k++) {
    delta = (deltas + k);
    (*delta).dx = pos[3*(*delta).i    ] - pos[3*(*delta).j    ];
    (*delta).dy = pos[3*(*delta).i + 1] - pos[3*(*delta).j + 1];
    (*delta).dz = pos[3*(*delta).i + 2] - pos[3*(*delta).j + 2];
    if (nvec > 0) mic((double*)delta, rvecs, gvecs, nvec);
    (*delta).gx = 0.0;
    (*delta).gy = 0.0;
    (*delta).gz = 0.0;
  }
}

void dlist_back(double *gradient, dlist_row_type* deltas, long ndelta) {
  long k;
  dlist_row_type *delta;
  for (k=0; k<ndelta; k++) {
    delta = (deltas + k);
    gradient[3*(*delta).i    ] += (*delta).gx;
    gradient[3*(*delta).i + 1] += (*delta).gy;
    gradient[3*(*delta).i + 2] += (*delta).gz;
    gradient[3*(*delta).j    ] -= (*delta).gx;
    gradient[3*(*delta).j + 1] -= (*delta).gy;
    gradient[3*(*delta).j + 2] -= (*delta).gz;
  }
}
