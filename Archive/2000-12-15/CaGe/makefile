# makefile for CaGe
# Thomas Harmuth
# October 1996

#=========================================================================
# Paths for library and include files:
# (several possibilities given - choose the right one)

# for SGI:
#TCLINCLUDE = /vol/local/include
#TCLLIB     = /vol/local/lib
#X11LIB     = /usr/lib

# for Linux:
TCLINCLUDE = /vol/X11/include
TCLLIB     = /vol/X11/lib
X11LIB     = /usr/lib

#cage : cage.c fullgen HCgen spring schlegel tubetype CPF
#	cc -o cage cage.c -I$(TCLINCLUDE) -L$(TCLLIB) -L$(X11LIB) \
#           -lm -ltcl -ltk -lX11 -ldl <- for LINUX

#=========================================================================

cage : cage.c fullgen HCgen spring schlegel tubetype CPF\
              plantri plantri_md6 plantri_ad
	cc -o cage cage.c -I$(TCLINCLUDE) -L$(TCLLIB) -L$(X11LIB) \
           -lm -ltcl8.0 -ltk8.0 -lX11 -ldl 

fullgen : fullgen.c
	cc -O2 -o fullgen fullgen.c

CPF : CPF.c 
	cc -O2 -o CPF CPF.c

tubetype : tubetype.c
	cc -O2 -o tubetype tubetype.c

HCgen : HCgen.c
	cc -O2 -o HCgen HCgen.c

#schlegel:	tkschlegel.c
#		cc -O2 -o schlegel -DUSE_TCL=0 tkschlegel.c -lm
schlegel:	schlegel.c
		cc -O2 -o schlegel -DUSE_TCL=0 schlegel.c -lm

#vschlegel:	vega-schlegel.c
#		cc -O2 -o vschlegel -DUSE_TCL=0 vega-schlegel.c -lm

spring:		tkspring.c
		cc -O2 -o spring -DUSE_TCL=0 tkspring.c -lm

#plantri_min4 : plantri.c 
#	cc -o plantri_min4 -O '-DPLUGIN="min4.c"' plantri.c
# Old plantri plugin "min4.c" now deleted.
# Functionality integrated since v3.1, use plantri -m4.
# More info: see the plantri/fullgen page, http://cs.anu.edu.au/~bdm/plantri/

plantri_md6 : plantri_md6.c
	cc -O2 -o plantri_md6 plantri_md6.c -lm 

plantri_ad : plantri.c 
	cc -o plantri_ad -O '-DPLUGIN="allowed_deg.c"' plantri.c

clean :
	rm cage fullgen HCgen CPF spring schlegel vschlegel tubetype\
           plantri plantri_md6 plantri_ad










