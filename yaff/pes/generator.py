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


import numpy as np

from molmod.units import parse_unit

from yaff.log import log
from yaff.pes.ext import PairPotEI, PairPotExpRep
from yaff.pes.ff import ForcePartPair, ForcePartValence, \
    ForcePartEwaldReciprocal, ForcePartEwaldCorrection, \
    ForcePartEwaldNeutralizing
from yaff.pes.iclist import Bond, BendAngle, BendCos
from yaff.pes.nlists import NeighborLists
from yaff.pes.scaling import Scalings
from yaff.pes.vlist import Harmonic, Fues


__all__ = [
    'ParsedPars', 'FFArgs', 'Generator', 'ValenceGenerator', 'BondGenerator',
    'BondHarmGenerator', 'BondFuesGenerator', 'BendGenerator',
    'BendAngleHarmGenerator', 'BendCosHarmGenerator', 'NonbondedGenerator',
    'ExpRepGenerator', 'FixedChargeGenerator', 'generators'
]


class ParsedPars(object):
    def __init__(self, fn, info=None):
        self.fn = fn
        if info is None:
            f = open(fn)
            try:
                self.load(f)
            finally:
                f.close()
        else:
            self.info = info

    def complain(self, counter, message=None):
        if counter is None:
            raise IOError('The parameter file %s %s.' % (self.fn, message))
        else:
            raise IOError('Line %i in the parameter file %s %s.' % (counter, self.fn, message))

    def load(self, f):
        self.info = {}
        counter = 1
        for line in f:
            line = line[:line.find('#')].strip()
            if len(line) > 0:
                pos = line.find(':')
                if pos == -1:
                    self.complain(counter, 'does not contain a colon')
                prefix = line[:pos].upper()
                rest = line[pos+1:].strip()
                if len(rest) == 0:
                    self.complain(counter, 'does not have text after the colon')
                if len(prefix.split()) > 1:
                    self.complain(counter, 'has a prefix that contains whitespace')
                pos = rest.find(' ')
                if pos == 0:
                    self.complain(counter, 'does not have a command after the prefix')
                elif pos == -1:
                    self.complain(counter, 'does not have data after the command')
                command = rest[:pos].upper()
                data = rest[pos+1:].strip()
                l1 = self.info.setdefault(prefix, {})
                l2 = l1.setdefault(command, [])
                l2.append((counter, data))
            counter += 1

    def get_section(self, key):
        if key in self.info:
            return ParsedPars(self.fn, self.info[key])
        else:
            return ParsedPars(self.fn, {})


class FFArgs(object):
    def __init__(self, rcut=18.89726133921252, smooth=True):
        """
           **Optional arguments:**

           Some optional arguments only make sense if related parameters in the
           parameter file are present.

           rcut
                The real space cutoff used by all pair potentials.

           smooth
                Flag to force the non-bonding terms smoothly to zero at the
                cutoff distance. This does not apply to the real-space part of
                the electrostatic term. For non-periodic systems, set the
                cutoff such that all pairs are included. For periodic systems,
                the Ewald summation is used.
        """
        self.parts = []
        self.nlists = None
        self.rcut = rcut
        self.smooth = smooth

    def get_nlists(self, system):
        if self.nlists is None:
            self.nlists = NeighborLists(system)
        return self.nlists

    def get_part(self, ForcePartClass):
        for part in self.parts:
            if isinstance(part, ForcePartClass):
                return part

    def get_part_pair(self, PairPotClass):
        for part in self.parts:
            if isinstance(part, ForcePartPair) and isinstance(part.pair_pot, PairPotClass):
                return part

    def get_part_valence(self, system):
        part_valence = self.get_part(ForcePartValence)
        if part_valence is None:
            part_valence = ForcePartValence(system)
            self.parts.append(part_valence)
        return part_valence

    def add_electrostatic_parts(self, system, scalings):
        if self.get_part_pair(PairPotEI) is not None:
            return
        # TODO: make it possible to tune alpha and gcut scaling parameters.
        # TODO: document the effects of these scaling parameters, i.e. define
        #       four regimes such as fast, normal, accurate and very_accurate.
        nlists = self.get_nlists(system)
        if system.cell.nvec == 0:
            alpha = 0.0
        elif system.cell.nvec == 3:
            alpha = 4.5/self.rcut
        else:
            raise NotImplementedError('Only zero- and three-dimensional electrostatics are supported.')
        # Real-space electrostatics
        pair_pot_ei = PairPotEI(system.charges, alpha, self.rcut)
        part_pair_ei = ForcePartPair(system, nlists, scalings, pair_pot_ei)
        self.parts.append(part_pair_ei)
        if system.cell.nvec == 3:
            # Reciprocal-space electrostatics
            part_ewald_reci = ForcePartEwaldReciprocal(system, alpha, gcut=alpha/0.75)
            self.parts.append(part_ewald_reci)
            # Ewald corrections
            part_ewald_corr = ForcePartEwaldCorrection(system, alpha, scalings)
            self.parts.append(part_ewald_corr)
            # Neutralizing background
            part_ewald_neut = ForcePartEwaldNeutralizing(system, alpha)
            self.parts.append(part_ewald_neut)


class Generator(object):
    """Creates (part of a) ForceField object automatically.

       A generator is a class that describes how a part of a parameter file
       must be turned into a part of ForceField object. As the generator
       proceeds, it will modify and extend the current arguments of the FF. They
       should be implemented such that the order of the generators is not
       important.
    """
    prefix = None
    num_ffatypes = None
    par_names = None
    commands = None

    def __call__(self, system, parsed_pars, ff_args):
        raise NotImplementedError

    def check_commands(self, parsed_pars):
        for command in parsed_pars.info:
            if command not in self.commands:
                parsed_pars.complain(None, 'contains a command (%s) that is not recognized by generator %s.' % (command, self.prefix))

    def process_units(self, parsed_pars):
        result = {}
        for counter, line in parsed_pars.info:
            words = line.split()
            if len(words) != 2:
                parsed_pars.complain(counter, 'must have two arguments in UNIT command.')
            name = words[0].upper()
            if name not in self.par_names:
                parsed_pars.complain(counter, 'specifies a unit for an unknown parameter. (Must be one of %s, but got %s.)' % (self.par_names, name))
            try:
                result[name] = parse_unit(words[1])
            except (NameError, ValueError):
                parsed_pars.complain(counter, 'has a UNIT command with an unknown unit.')
        if len(result) != len(self.par_names):
            raise IOError('Not all units are specified for generator %s in file %s. Got %s, should have %s.' % (
                self.prefix, parsed_pars.fn, result.keys(), self.par_names
            ))
        return result

    def process_pars(self, parsed_pars, conversions):
        par_table = {}
        for counter, line in parsed_pars.info:
            words = line.split()
            num_args = self.num_ffatypes + len(self.par_names)
            if len(words) != num_args:
                parsed_pars.complain(counter, 'should have %s arguments.' % num_args)
            key = tuple(words[:self.num_ffatypes])
            try:
                pars = tuple(
                    float(words[i+self.num_ffatypes])*conversions[par_name]
                    for i, par_name in enumerate(self.par_names)
                )
            except ValueError:
                parsed_pars.complain(counter, 'has parameters that can not be converted to floating point numbers.')
            if key in par_table:
                parsed_pars.complain(counter, 'contains duplicate parameters.')
            for key in self.iter_alt_keys(key):
                par_table[key] = pars
        return par_table

    def iter_alt_keys(self, key):
        if len(key) == 1:
            yield key
        else:
            raise NotImplementedError


class ValenceGenerator(Generator):
    commands = ['UNIT', 'PARS']

    def __call__(self, system, parsed_pars, ff_args):
        self.check_commands(parsed_pars)
        conversions = self.process_units(parsed_pars.get_section('UNIT'))
        par_table = self.process_pars(parsed_pars.get_section('PARS'), conversions)
        if len(par_table) > 0:
            self.apply(par_table, system, ff_args)

    def apply(self, par_table, system, ff_args):
        if system.topology is None:
            raise ValueError('The system must have a topology (i.e. bonds) in order to define valence terms.')
        part_valence = ff_args.get_part_valence(system)
        from yaff.pes.iclist import Bond
        from yaff.pes.vlist import Harmonic
        for indexes in self.iter_indexes(system):
            key = tuple(system.ffatypes[i] for i in indexes)
            pars = par_table.get(key)
            pars = self.mod_pars(pars)
            args = pars + (self.ICClass(*indexes),)
            part_valence.add_term(self.VClass(*args))

    def mod_pars(self, pars):
        # By default, the parameters do not have to be modified.
        return pars


class BondGenerator(ValenceGenerator):
    num_ffatypes = 2
    par_names = ['K', 'R0']
    ICClass = Bond
    VClass = None

    def iter_alt_keys(self, key):
        yield key
        yield key[::-1]

    def iter_indexes(self, system):
        for i0, i1 in system.topology.bonds:
            yield i0, i1


class BondHarmGenerator(BondGenerator):
    prefix = 'BONDHARM'
    VClass = Harmonic


class BondFuesGenerator(BondGenerator):
    prefix = 'BONDFUES'
    VClass = Fues


class BendGenerator(ValenceGenerator):
    num_ffatypes = 3
    par_names = ['K', 'THETA0']
    ICClass = None
    VClass = Harmonic

    def iter_alt_keys(self, key):
        yield key
        yield key[::-1]

    def iter_indexes(self, system):
        for i1 in xrange(system.natom):
            for i0 in system.topology.neighs1[i1]:
                for i2 in system.topology.neighs1[i1]:
                    if i0 > i2:
                        yield i0, i1, i2


class BendAngleHarmGenerator(BendGenerator):
    prefix = 'BENDAHARM'
    ICClass = BendAngle


class BendCosHarmGenerator(BendGenerator):
    prefix = 'BENDCHARM'
    ICClass = BendCos

    def mod_pars(self, pars):
        # The rest angle has to be transformed into a rest cosine
        k, a0 = pars
        c0 = np.cos(a0)
        return k, c0


class NonbondedGenerator(Generator):
    mixing_rules = None

    def process_scales(self, parsed_pars):
        result = {}
        for counter, line in parsed_pars.info:
            words = line.split()
            if len(words) != 2:
                parsed_pars.complain(counter, 'must have 2 arguments.')
            try:
                num_bonds = int(words[0])
                scale = float(words[1])
            except ValueError:
                parsed_pars.complain(counter, 'has parameters that can not be converted. The first argument must be an integer. The second argument must be a float.')
            if num_bonds in result:
                parsed_pars.complain(counter, 'contains a duplicate scale command.')
            if scale < 0 or scale > 1:
                parsed_pars.complain(counter, 'has a scale that is not in the range [0,1].')
            result[num_bonds] = scale
        if len(result) != 3:
            parsed_pars.complain(None, 'must contain three SCALE commands for each non-bonding term.')
        if 1 not in result or 2 not in result or 3 not in result:
            parsed_pars.complain(None, 'must contain a scale parameter for atoms separated by 1, 2 and 3 bonds, for each non-bonding term.')
        return result

    def process_mix(self, parsed_pars):
        result = {}
        for counter, line in parsed_pars.info:
            words = line.split()
            if len(words) < 2:
                parsed_pars.complain(counter, 'contains a mixing rule with to few arguments. At least 2 are required.')
            par_name = words[0].upper()
            rule_name = words[1].upper()
            key = par_name, rule_name
            if key not in self.mixing_rules:
                parsed_pars.complain(counter, 'contains an unknown mixing rule.')
            narg, rule_id = self.mixing_rules[key]
            if len(words) != narg+2:
                parsed_pars.complain(counter, 'does not have the correct number of arguments. %i arguments are required.' % (narg+2))
            try:
                args = tuple([float(word) for word in words[2:]])
            except ValueError:
                parsed_pars.complain(counter, 'contains parameters that could not be converted to floating point numbers.')
            result[par_name] = rule_id, args
        expected_num_rules = len(set([par_name for par_name, rule_id in self.mixing_rules]))
        if len(result) != expected_num_rules:
            parsed_pars.complain(None, 'does not contain enough mixing rules for the generator %s.' % self.prefix)
        return result


class ExpRepGenerator(NonbondedGenerator):
    prefix = 'EXPREP'
    commands = ['UNIT', 'SCALE', 'MIX', 'PARS']
    num_ffatypes = 1
    par_names = ['A', 'B']
    mixing_rules = {
        ('A', 'GEOMETRIC'): (0, 0),
        ('A', 'GEOMETRIC_COR'): (1, 1),
        ('B', 'ARITHMETIC'): (0, 0),
        ('B', 'ARITHMETIC_COR'): (1, 1),
    }

    def __call__(self, system, parsed_pars, ff_args):
        self.check_commands(parsed_pars)
        conversions = self.process_units(parsed_pars.get_section('UNIT'))
        par_table = self.process_pars(parsed_pars.get_section('PARS'), conversions)
        scale_table = self.process_scales(parsed_pars.get_section('SCALE'))
        mixing_rules = self.process_mix(parsed_pars.get_section('MIX'))
        self.apply(par_table, scale_table, mixing_rules, system, ff_args)

    def apply(self, par_table, scale_table, mixing_rules, system, ff_args):
        # Prepare the atomic parameters
        amps = np.zeros(system.natom)
        bs = np.zeros(system.natom)
        for i in xrange(system.natom):
            key = (system.ffatypes[i],)
            pars = par_table.get(key)
            if pars is None:
                if log.do_warning:
                    log.warn('No EXPREP parameters found for atom %i with fftype %s.' % (i, system.ffatypes[i]))
            else:
                amps[i], bs[i] = pars

        # Prepare the global parameters
        scalings = Scalings(system.topology, scale_table[1], scale_table[2], scale_table[3])
        amp_mix, amp_mix_coeff = mixing_rules['A']
        if amp_mix == 0:
            amp_mix_coeff = 0.0
        elif amp_mix == 1:
            amp_mix_coeff = amp_mix_coeff[0]
        b_mix, b_mix_coeff = mixing_rules['B']
        if b_mix == 0:
            b_mix_coeff = 0.0
        elif b_mix == 1:
            b_mix_coeff = b_mix_coeff[0]

        # Get the part. It should not exist yet.
        part_pair = ff_args.get_part_pair(PairPotExpRep)
        if part_pair is not None:
            raise RuntimeError('Internal inconsistency: the EXPREP part should not be present yet.')

        pair_pot = PairPotExpRep(amps, amp_mix, amp_mix_coeff, bs, b_mix, b_mix_coeff, ff_args.rcut, ff_args.smooth)
        nlists = ff_args.get_nlists(system)
        part_pair = ForcePartPair(system, nlists, scalings, pair_pot)
        ff_args.parts.append(part_pair)


class FixedChargeGenerator(NonbondedGenerator):
    prefix = 'FIXQ'
    commands = ['UNIT', 'SCALE', 'ATOM', 'BOND', 'DIELECTRIC']
    par_names = ['Q0', 'P', 'R']

    def __call__(self, system, parsed_pars, ff_args):
        self.check_commands(parsed_pars)
        conversions = self.process_units(parsed_pars.get_section('UNIT'))
        atom_table = self.process_atoms(parsed_pars.get_section('ATOM'), conversions)
        bond_table = self.process_bonds(parsed_pars.get_section('BOND'), conversions)
        scale_table = self.process_scales(parsed_pars.get_section('SCALE'))
        dielectric = self.process_dielectric(parsed_pars.get_section('DIELECTRIC'))
        self.apply(atom_table, bond_table, scale_table, dielectric, system, ff_args)

    def process_atoms(self, parsed_pars, conversions):
        result = {}
        for counter, line in parsed_pars.info:
            words = line.split()
            if len(words) != 3:
                parsed_pars.complain(counter, 'should have 3 arguments.')
            ffatype = words[0]
            if ffatype in result:
                parsed_pars.complain(counter, 'has an atom type that was already encountered earlier.')
            try:
                charge = float(words[1])*conversions['Q0']
                radius = float(words[2])*conversions['R']
            except ValueError:
                parsed_pars.complain(counter, 'contains a parameter that can not be converted to a floating point number.')
            result[ffatype] = charge, radius
        return result

    def process_bonds(self, parsed_pars, conversions):
        result = {}
        for counter, line in parsed_pars.info:
            words = line.split()
            if len(words) != 3:
                parsed_pars.complain(counter, 'should have 3 arguments.')
            key = tuple(words[:2])
            if key in result:
                parsed_pars.complain(counter, 'has a combination of atom types that were already encountered earlier.')
            try:
                charge_transfer = float(words[2])*conversions['P']
            except ValueError:
                parsed_pars.complain(counter, 'contains a parameter that can not be converted to floating point numbers.')
            result[key] = charge_transfer
            result[key[::-1]] = -charge_transfer
        return result

    def process_dielectric(self, parsed_pars):
        result = None
        for counter, line in parsed_pars.info:
            if result is not None:
                parsed_pars.complain(counter, 'is redundant. The DIELECTRIC command may only occur once.')
            words = line.split()
            if len(words) != 1:
                parsed_pars.complain(counter, 'must have one argument.')
            try:
                result = float(words[0])
            except ValueError:
                parsed_pars.complain(counter, 'must have a floating point argument.')
        return result

    def apply(self, atom_table, bond_table, scale_table, dielectric, system, ff_args):
        if system.charges is None:
            system.charges = np.zeros(system.natom)
        else:
            if log.do_warning:
                log.warn('Adding charges to system that already has charges.')
        radii = np.zeros(system.natom)

        # compute the charges
        for i in xrange(system.natom):
            pars = atom_table.get(system.ffatypes[i])
            if pars is None and log.do_warning:
                log.warn('No charge defined for atom %i with fftype %s.' % (i, system.ffatypes[i]))
            else:
                charge, radius = pars
                if radius > 0:
                    raise NotImplementedError('TODO: support smeared charges.')
                system.charges[i] += charge
                radii[i] = radius
        for i0, i1 in system.topology.bonds:
            charge_transfer = bond_table.get((system.ffatypes[i0], system.ffatypes[i1]))
            if charge_transfer is None and log.do_warning:
                log.warn('No charge transfer parameter for atom pair (%i,%i) with fftype (%s,%s).' % (i0, i1, system.ffatypes[i0], system.ffatypes[i1]))
            else:
                system.charges[i0] += charge_transfer
                system.charges[i1] -= charge_transfer

        # prepare other parameters
        scalings = Scalings(system.topology, scale_table[1], scale_table[2], scale_table[3])

        if dielectric != 1.0:
            raise NotImplementedError('Only a relative dielectric constant of 1 is supported.')

        # Setup the electrostatic pars
        ff_args.add_electrostatic_parts(system, scalings)


# Collect all the generators that have a prefix.
generators = {}
for x in globals().values():
    if isinstance(x, type) and issubclass(x, Generator) and x.prefix is not None:
        generators[x.prefix] = x()