# YAFF is yet another force-field code
# Copyright (C) 2008 - 2011 Toon Verstraelen <Toon.Verstraelen@UGent.be>, Center
# for Molecular Modeling (CMM), Ghent University, Ghent, Belgium; all rights
# reserved unless otherwise stated.
#
# This file is part of YAFF.
#
# YAFF is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 3
# of the License, or (at your option) any later version.
#
# YAFF is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>
#
# --


from StringIO import StringIO

from yaff.log import ScreenLog


def test_line_wrapping():
    f = StringIO()
    log = ScreenLog(f)
    log._active = True
    log.enter('NVE')
    log('This is just a long test message that should get splitted into two lines properly.')
    assert f.getvalue() == '\n    NVE This is just a long test message that should get splitted into two\n    NVE lines properly.\n'
    f.close()


def test_levels():
    log = ScreenLog()
    assert log.do_medium
    assert not log.do_high
    log.set_level(log.low)
    assert not log.do_medium


def test_hline():
    f = StringIO()
    log = ScreenLog(f)
    log._active = True
    log.enter('FOOBAR')
    log.hline()
    assert f.getvalue() == '\n FOOBAR ' + '~'*71 + '\n'
    f.close()

def test_unitsys():
    from molmod.units import kjmol, kcalmol
    f = StringIO()
    log = ScreenLog(f)
    assert abs(log.energy - kjmol) < 1e-10
    log.set_unitsys(log.cal)
    assert abs(log.energy - kcalmol) < 1e-10

def test_lead():
    f = StringIO()
    log = ScreenLog(f)
    log._active = True
    log.enter('AAA')
    log('Some prefix:&followed by a long text that needs to be wrapped over multiple lines.')
    assert f.getvalue() == '\n    AAA Some prefix: followed by a long text that needs to be wrapped over\n    AAA              multiple lines.\n'
    f.close()

def test_enter_leave():
    f = StringIO()
    log = ScreenLog(f)
    log.enter('FOO')
    assert log.prefix == '    FOO'
    log.enter('BAR')
    assert log.prefix == '    BAR'
    log.leave()
    assert log.prefix == '    FOO'
    log.leave()
    assert log.prefix == '       '