import os
import re

from taskinit import *

import asap as sd
from asap._asap import Scantable
import pylab as pl
from asap import _to_list
from asap.scantable import is_scantable
import sdutil

@sdutil.sdtask_decorator
def sdcal2(infile, calmode, skytable, tsystable, interp, ifmap, scanlist, field, iflist, pollist, outfile, overwrite):
    worker = sdcal2_worker(**locals())
    worker.initialize()
    worker.execute()
    worker.finalize()


class sdcal2_worker(sdutil.sdtask_template):
    def __init__(self, **kwargs):
        super(sdcal2_worker,self).__init__(**kwargs)
        self.suffix = ''
        self.interp_time = ''
        self.interp_freq = ''
        self.doapply = False
        self.dosky = False
        self.dotsys = False
        self.insitu = False
        self.skymode = ''
        self.manager = sd._asap._calmanager()

    def initialize(self):
        # override initialize method
        self.parameter_check()
        self.initialize_scan()
        
    def parameter_check(self):
        self.check_infile()
        sep = ','
        num_separator = self.calmode.count(sep)
        if num_separator == 0:
            # single calibration process
            if self.calmode == 'apply':
                # apply sky (and Tsys) tables
                self.check_skytable()
                self.check_tsystable()
                self.check_outfile()
                self.check_interp()
                self.check_ifmap()
                self.check_update()
                self.doapply = True
            elif self.calmode == 'tsys':
                # generate Tsys table
                self.check_outfile()
                self.check_iflist()
                self.dotsys = True
            else:
                # generate sky table
                self.check_outfile()
                self.dosky = True
                self.skymode = self.calmode
        elif num_separator == 1:
            # generate sky table and apply it on-the-fly
            self.check_tsystable()
            self.check_interp()
            self.check_update()
            self.dosky = True
            self.doapply = True
            self.skymode = self.calmode.split(sep)[0]
        else:
            # generate sky and Tsys table and apply them on-the-fly
            self.check_iflist()
            self.check_update()
            self.dosky = True
            self.dotsys = True
            self.doapply = True
            self.skymode = self.calmode.split(sep)[0]

    def check_infile(self):
        if not is_scantable(self.infile):
            raise Exception('infile must be in scantable format.')

    def check_outfile(self):
        if len(self.outfile) > 0:
            # outfile is specified
            if os.path.exists(self.outfile):
                if not self.overwrite:
                    raise Exception('Output file \'%s\' exists.'%(self.outfile))
                else:
                    casalog.post('INFO','Overwrite %s ...'%(self.outfile))
                    os.system('rm -rf %s'%(self.outfile))
    def check_skytable(self):
        # length should be > 0 either skytable is string or string list
        if len(self.skytable) == 0:
            raise Exception('Name of the sky table must be given.')

        if type(self.skytable) == str:
            # string 
            if not os.path.exists(self.skytable):
                raise Exception('Sky table \'%s\' does not exist.'%(self.skytable))
        else:
            # string list
            for tab in self.skytable:
                if len(tab) == 0:
                    raise Exception('Name of the sky table must be given.')
                elif not os.path.exists(tab):
                    raise Exception('Sky table \'%s\' does not exist.'%(self.skytable))

    def check_tsystable(self):
        # length should be > 0 either tsystable is string or string list
        if len(self.tsystable) == 0:
            casalog.post('WARN','Tsys table is not given, no Tsys scaling will be done.')
            return

        if type(self.tsystable) == str:
            # string 
            if not os.path.exists(self.tsystable):
                raise Exception('Tsys table \'%s\' does not exist.'%(self.tsystable))
        else:
            # string list
            for tab in self.tsystable:
                if len(tab) == 0:
                    pass
                elif not os.path.exists(tab):
                    raise Exception('Tsys table \'%s\' does not exist.'%(self.tsystable))

    def check_interp(self):
        if len(self.interp) == 0:
            # default
            self.interp_time = ''
            self.interp_freq = ''
        else:
            valid_types = '^(nearest|linear|cspline|[0-9]+)$'
            interp_types = self.interp.split(',')
            if len(interp_types) > 2:
                raise Exception('Invalid format of the parameter interp: \'%s\''%(self.interp))
            self.interp_time = interp_types[0].lower()
            self.interp_freq = interp_types[-1].lower()
            if not re.match(valid_types,self.interp_time):
                raise Exception('Interpolation type \'%s\' is invalid or not supported yet.'%(self.interp_time))
            if not re.match(valid_types,self.interp_freq):
                raise Exception('Interpolation type \'%s\' is invalid or not supported yet.'%(self.interp_freq))
            
    def check_ifmap(self):
        if not isinstance(self.ifmap, dict) or len(self.ifmap) == 0:
            raise Exception('ifmap must be non-empty dictionary.')

    def check_iflist(self):
        if len(self.iflist) == 0:
            raise Exception('You must specify iflist as a list of IFNOs for Tsys calibration.')

    def check_update(self):
        if len(self.outfile) == 0:
            if not self.overwrite:
                raise Exception('You should set overwrite to True if you want to update infile.')
            else:
                casalog.post('INFO','%s will be overwritten by the calibrated spectra.'%(self.infile))
                self.insitu = True

    def initialize_scan(self):
        sel = self.get_selector()
        if self.insitu:
            # update infile 
            storage = sd.rcParams['scantable.storage']
            sd.rcParams['scantable.storage'] = 'disk'
            self.scan = sd.scantable(self.infile,average=False)
            sd.rcParams['scantable.storage'] = storage
        else:
            self.scan = sd.scantable(self.infile,average=False)
        self.scan.set_selection(sel)
    
    def execute(self):
        self.manager.set_data(self.scan)
        if self.dosky:
            self.manager.set_calmode(self.skymode)
            self.manager.calibrate()
        if self.dotsys:
            self.manager.set_calmode('tsys')
            self.manager.set_tsys_spw(self.iflist)
            self.manager.calibrate()
        if self.doapply:
            if len(self.skytable) > 0:
                if isinstance(self.skytable,str):
                    self.manager.add_skytable(self.skytable)
                else:
                    for tab in self.skytable:
                        self.manager.add_skytable(tab)
            if len(self.tsystable) > 0:
                if isinstance(self.tsystable,str):
                    self.manager.add_tsystable(self.tsystable)
                else:
                    for tab in self.tsystable:
                        self.manager.add_tsystable(tab)
            if len(self.interp_time) > 0:
                self.manager.set_time_interpolation(self.interp_time)
            if len(self.interp_freq) > 0:
                self.manager.set_freq_interpolation(self.interp_freq)
            for (k,v) in self.ifmap.items():
                self.manager.set_tsys_transfer(k,v)
            self.manager.apply(self.insitu, True)

    def save(self):
        if self.doapply:
            if not self.insitu:
                self.manager.split(self.outfile)
        elif self.dosky:
            self.manager.save_caltable(self.outfile)
        elif self.dotsys:
            self.manager.save_caltable(self.outfile)
            
    def cleanup(self):
        self.manager.reset()
