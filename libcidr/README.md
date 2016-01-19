NOTE!  This repository has moved to:

https://github.com/wikimedia/analytics-libcidr



# Overview

This is libcidr, a library to handle manipulating CIDR netblocks in
IPv4 and IPv6 in a host of ways.  It's useful for parsing textual
representations of addresses and blocks, comparing blocks in various
ways, and outputting them in different formats.

# History
- This library was written by [Matthew Fuller](mailto:fullermd@over-yonder.net).
- Debian packaging was added by [Andrew Otto](mailto:otto@wikimedia.org)
  for the [Wikimedia Foundation](http://wikimediafoundation.org).

This repository was cloned from Matthew Fullers main bazaar repository
using [git-bzr-ng](https://github.com/termie/git-bzr-ng).  The upstream
branch is how the upstream repository looked at bzr revno 243 as of
July 13 2012.

To bring in upstream changes:

    # create a local bzr/libcidr branch from the upstream bazaar repository.
    git-bzr import http://bzr.over-yonder.net/~fullermd/libcidr
    
    # checkout master and merge the local branch into it.  There might be conflicts.
    git checkout master
    git merge bzr/libcidr
    
    # resolve any conflicts.
    git push
  
_NOTE: I have not actually tried this, but I think it should work :)_
  

# Installation

To install from a release tarball, all you should need to do is a
standard "make ; make install" procedure.

To install from a raw source tree, you'll need to either build the docs
manually, or set NO_DOCS in your install environment.  You may also
need to run the `mkgmake.sh` script to generate GNU make Makefiles, if
you use GNU make (this should be already done on released tarballs).

The Makefiles are written with the assumption that you're using gcc.
There is, however, to my knowledge, nothing in the code that requires
gcc specifically.  The code is aimed at C99-compliant systems (with a
few extremely common POSIX/SUS extensions in sample programs), but
_should_ work on most C89 systems as well.  If you're using a compiler
other than gcc, you may need to adjust the flags given to the compiler.

This library requires the POSIX/C99 `uintX_t` types, as well as basic
IPv6 header file support.  If your system lacks either of these, you
can probably get the system to build by typedef'ing `uint8_t` as an
unsigned char in the former case, and by removing the
`cidr_{from,to}_in6addr()` functions in the latter.  I've not tested
this, and neither should be necessary on a reasonably modern system.

In both cases, there are a few variables you can tweak to determine
where things get installed.  Generally, you should only need to set
PREFIX, and the other variables will be set based on that.  See the
beginning of the Makefile for more details if you need them.

PREFIX defaults to `/usr/local`, which is probably the right choice for
most sites.  Installation will install several things:

- The shared library itself, in `${PREFIX}/lib`,
- The global header file `libcidr.h`, in `${PREFIX}/include`,
- The compiled `cidrcalc` example program, in `${PREFIX}/bin`,
- The `libcidr(3)` manpage, in `${PREFIX}/man/man3`,
- Several useful extra bits, under `${PREFIX}/share/libcidr`:
  - The reference manual in various formats, under docs/
  - The source and Make infrastructure for the `cidrcalc` and `acl` example
    programs, under `examples/`


# Roadmap

Here's a quick overview of the pieces of this dist.  See README files
in the various subdirs for more information about them.

- `docs/`    - The documentation for libcidr, in various formats.
               See the README in here for more info.
- `include/` - The public header file which will be installed for
               programs using libcidr to `#include`, defining the various
               interfaces and flags.
- `src/`     - The actual source for libcidr, as well as various examples
               and test programs.  Particularly, the test and example
               programs are invaluable in understanding how the functions
               are meant to be used, and should certainly be perused by
               anybody looking at the functionality libcidr offers.
- `tools/`   - Various bits&pieces of things used to build libcidr.  No
               user-serviceable parts inside.


# Support

libcidr is intended to be pretty self-explanatory, particularly with
the aid of the example programs.  And don't neglect the programs in
`test/`; they're intended for me to use to test the functions when I
write or change them, but because of that, they show how the functions
work fairly cleanly.  The mkstr program in particular, for its ability
to exercise all the different flags to `cidr_to_str()`, is invaluable.
libcidr is also very well documented, with the full reference manual
explaining the calling conventions and usage of the various functions.
And, I like to think the source is generally pretty readable in
extremis.

However, if you have further questions or suggestions, please feel free
to email me.  See the project homepage at
<http://www.over-yonder.net/~fullermd/projects/libcidr> for updates.

-- 
Matthew Fuller
<fullermd@over-yonder.net>

<http://www.over-yonder.net/~fullermd/>


# To Do
Update to 1.2.1.  Matthew recently updated his upstream libcidr to 1.2.1.  
This includes changes in the build process that should make building
packages easier.
