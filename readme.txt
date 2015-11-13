This archive should contain:

makefile        makefile for emx
readme          guess what :-)
clipbrd.c       source for the textmode part; the actual program
pmclip.c        source for the PM server, that actually reads the
                clipboard and is called by clipbrd.exe
clipbrd.def     definition file for clipbrd.c
pmclip.def      definition file for pmclip.c
emx.dll         emx runtime DLL
clipbrd.exe     textmode part; the actual program
pmclip.exe      PM server for clipbrd.exe
common.h        definitions shared between the two sources


Requirements: OS/2 2.x and the emx runtime DLLs (available at
              ftp.cdrom.com in the directory */2_x/unix/emx08g)

Installation: Put the two executables into the same or two different
              directories listed in the PATH statement of your
              CONFIG.SYS. If you don't have emx.dll already installed
              (if you're not sure, try "emxrev"; if you get a line
              "EMX: revision <number>", you already have emx.dll
              installed), copy emx.dll to a directory listed in the
              LIBPATH statement of your CONFIG.SYS.

Usage: Simply call clipbrd without parameters, it should be self
       explaining.


Some useful examples:

 - Suppose you're in
   D:\wow\what\a\really\long\and\complicated\directory\name\this\is
   and want to create a WPS object for foo.exe; now all you have to
   do is call

     "dir /fb foo.exe | clipbrd",

   drag a new program object from the templates folder and press
   <Shift>-<Ins>. Easy, isnt'it?

 - You want to edit all c source files that contain a certain regular
   expression <regexp> with the enhanced editor:

     "grep -l <regexp> *.c | tr -f | clipbrd"

   puts a blank separated list of the appropriate filenames into the
   clipboard. Now

     "clipbrd -p"

   and typing "epm <Ctrl>-Z", then

     "clipbrd -x"

   will do the trick. (tr and grep are available at ftp.cdrom.com
   in the directory */2_x/unix)


Note: Compiling clipbrd.exe with emx and optimization turned on
      ("-O" or "-O2") causes problems, so I turned it off in the
      makefile.


Licence: You are free to use and modify this source, as long as you
         make your source freely available according to the usual
         GNU Copyleft license.

NO WARRANTY: No guarantee is made as to the proper functioning of the
             software. No liability will be admitted for damage
             resulting from using the software.


Author: Stefan Gruendel                 (TeX: Stefan Gr\"undel)
        Stresemannstr.5
        97209 Veitshoechheim, Germany   (TeX: Veitsh\"ochheim)

        Tel: +49 931 94227
        Fax: +49 931 98219

        EMail: Internet:         sgruen@vax.rz.uni-wuerzburg.d400.de
               (german) MausNet: Stefan Gruendel @ Wš
