# PIOUS Root Make File
#
# install : compile the PIOUS system for host architecture (default)
# examples: compile the PIOUS demonstration programs
# clean   : remove the PIOUS object files for host architecture
# tidy    : clean-up the PIOUS source file directories
#
#
# Install options:
#
#   USEPVM33FNS   - utilize new functions available in PVM 3.3 (or later)
#   USEPVMDIRECT  - employ PvmRouteDirect message routing policy
#   USEPVMRAW     - employ PvmDataRaw message encoding
#   VTCOMMITNOACK - employ volatile transaction optimization (no commit ACK)
#   STABLEACCESS  - allow stable mode access (for benchmarking ONLY)
#   PDSPROFILE    - turn on PIOUS Data Server profiling
#
#
# January 1995 Moyer
#


# define desired PIOUS options for install
IFLAGS = -DUSEPVM33FNS -DUSEPVMDIRECT -DUSEPVMRAW -DVTCOMMITNOACK

# define desired compiler options for install
CFLAGS = -O

# define desired shell
SHELL = /bin/sh


install:
#
# compile PIOUS daemons and library functions, in required order, and move to
# appropriate PVM directories
#
	cd src/pdce; ../../lib/archmk MKFLAGS="$(IFLAGS) $(CFLAGS)"
	cd src/pfs;  ../../lib/archmk MKFLAGS="$(IFLAGS) $(CFLAGS)"
	cd src/psys; ../../lib/archmk MKFLAGS="$(IFLAGS) $(CFLAGS)"
	cd src/misc; ../../lib/archmk MKFLAGS="$(IFLAGS) $(CFLAGS)"
	cd src/pds;  ../../lib/archmk MKFLAGS="$(IFLAGS) $(CFLAGS)"
	cd src/psc;  ../../lib/archmk MKFLAGS="$(IFLAGS) $(CFLAGS)"
	cd src/plib; ../../lib/archmk MKFLAGS="$(IFLAGS) $(CFLAGS)"

	cd fsrc; ../lib/archmk MKFLAGS="$(CFLAGS)"

#
# copy PIOUS include files and start up script to appropriate PVM directories;
# at this point know that PVM_ROOT is defined/valid since compilation via
# archmk was successful
#
	rm -f           $(PVM_ROOT)/include/pious1.h
	cat src/include/pious_types.h src/include/pious_errno.h \
	    src/include/pious_std.h src/plib/plib.h > \
	                $(PVM_ROOT)/include/pious1.h
	chmod 444       $(PVM_ROOT)/include/pious1.h
	rm -f           $(PVM_ROOT)/lib/pious
	cp  lib/pious   $(PVM_ROOT)/lib


examples: FORCE
	cd examples; ../lib/archmk MKFLAGS=""

clean:
	cd src/pdce; ../../lib/archmk clean
	cd src/pfs;  ../../lib/archmk clean
	cd src/psys; ../../lib/archmk clean
	cd src/misc; ../../lib/archmk clean
	cd src/pds;  ../../lib/archmk clean
	cd src/psc;  ../../lib/archmk clean
	cd src/plib; ../../lib/archmk clean
	cd fsrc;        ../lib/archmk clean


tidy:
	cd src/pdce;     rm -f *~; rm -f *.ln; rm -f lint.out
	cd src/pfs;      rm -f *~; rm -f *.ln; rm -f lint.out
	cd src/psys;     rm -f *~; rm -f *.ln; rm -f lint.out
	cd src/misc;     rm -f *~; rm -f *.ln; rm -f lint.out
	cd src/pds;      rm -f *~; rm -f *.ln; rm -f lint.out
	cd src/psc;      rm -f *~; rm -f *.ln; rm -f lint.out
	cd src/plib;     rm -f *~; rm -f *.ln; rm -f lint.out
	cd src/config;   rm -f *~; rm -f *.ln; rm -f lint.out
	cd src/include;  rm -f *~; rm -f *.ln; rm -f lint.out
	cd fsrc;         rm -f *~


FORCE:
