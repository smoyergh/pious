c
c flibtest.f - PIOUS Fortran library test
c
c This is a test of the PIOUS Fortran library ONLY, and is NOT intended
c to be a complete PIOUS functionality test.
c
c @(#)flibtest.f	2.2  28 Apr 1995  Moyer
c


      program flibtest
      include 'fpvm3.h'
      include 'fpious1.h'

c define group/file name constants and read/write parameters

      character*(PIOUS_NAME_MAX) GROUPNAME1, GROUPNAME2
      character*(PIOUS_PATH_MAX) FILENAME

      parameter (GROUPNAME1 = 'flibtest1')
      parameter (GROUPNAME2 = 'flibtest2')
      parameter (FILENAME   = 'flibtest.dat')

      integer BUFSZ, FILESZ, SU, PDSMAX

      parameter (BUFSZ  = 1024)
      parameter (FILESZ = 8)
      parameter (SU     = 7)
      parameter (PDSMAX = 8)

c define file mode parameters

      integer REGMODE, DIRMODE

      parameter (REGMODE = (PIOUS_IRUSR + PIOUS_IWUSR +
     >                      PIOUS_IRGRP + PIOUS_IWGRP +
     >                      PIOUS_IROTH + PIOUS_IWOTH))
      parameter (DIRMODE = (PIOUS_IRWXU +
     >                      PIOUS_IRGRP + PIOUS_IXGRP +
     >                      PIOUS_IROTH + PIOUS_IXOTH))

c define program variables

      integer mytid, dscnt, fd(4), i, j, k, pbp, wbp, trial, acode
      integer statdscnt, statsegcnt, oldmask, buffersz, rdwrsz, rdwroff

      character*(BUFSZ) wbuf, rbuf, pbuf
      character*(PIOUS_PATH_MAX) dirname, fullfilepath, dsfile

      integer pds_cnt
      character*(PIOUS_NAME_MAX * PDSMAX) pds_hname
      character*(PIOUS_PATH_MAX * PDSMAX) pds_spath, pds_lpath

c print heading

      print *
      print *, 'FLIBTEST - perform test of the PIOUS Fortran library'
      print *
      print *

c enroll in PVM

      call pvmfmytid(mytid)

      if (mytid .lt. 0) then
         print *, 'flibtest: unable to enroll in PVM'
         stop
      endif

c determine if PIOUS started with a default set of data servers

      call piousfsysinfo(PIOUS_DS_DFLT, dscnt)

      if (dscnt .le. 0) then
         if (dscnt .eq. 0) then
            print *, 'flibtest: config PIOUS with default data servers'
         else
            print *, 'flibtest: start PIOUS before executing test'
         endif
         goto 1000
      endif

c get name of configuration file

      print *
      print *, 'Enter active dsfile path.'
      print *, 'If dsfile is not correct then results are erroneous!'
      print *
      print *, 'Dsfile:'
      read  (*, '(A)') dsfile
      print *

c parse configuration file

      call cffparse(dsfile,
     >              pds_cnt, pds_hname, pds_spath, pds_lpath, acode)

      if (acode .ne. PIOUS_OK) then
         if (acode .eq. PIOUS_EINVAL) then
            print *, 'flibtest: can''t access dsfile or dsfile invalid'
         else
            print *, 'flibtest: insufficient memory or too many servers'
         endif

         goto 1000
      endif

      if (pds_cnt .ne. dscnt) then
         print *, 'flibtest: dsfile host count inconsistent with config'
         goto 1000
      endif


c get name of directory to work in

      print *
      print *, 'Enter a NEW directory name.'
      print *, 'If directory is EXTANT then test will fail!'
      print *
      print *, 'Directory:'
      read  (*, '(A)') dirname
      print *


c ----------------------------------------------------------------------
c Attempt to call all PIOUS Fortran functions
c
c Note: piousfsysinfo() already called
c ----------------------------------------------------------------------

      print *
      print *, 'Calling all PIOUS Fortran functions....please wait.'
      print *

c set umask

      call piousfumask((PIOUS_IWGRP + PIOUS_IWOTH), oldmask, acode)

      if ((acode .ne. PIOUS_OK) .or. (oldmask .ne. 0)) then
         print *, 'flibtest: piousfumask() failed'
         goto 1000
      endif

c create directory file

      call piousfmkdir(dirname, DIRMODE, acode)

      if ((acode .ne. PIOUS_OK) .and. (acode .ne. PIOUS_EEXIST)) then
         print *, 'flibtest: piousfmkdir() failed - can not create user'
         print *, '          supplied directory'
         goto 1000
      endif

      call piousfsmkdir(pds_hname, pds_spath, pds_lpath, pds_cnt,
     >                  dirname, DIRMODE, acode)

      if (acode .ne. PIOUS_EEXIST) then
         print *, 'flibtest: piousfsmkdir() failed'
         goto 1000
      endif

c set current working directory path

      call piousfsetcwd(dirname, acode)

      if (acode .ne. PIOUS_OK) then
         print *, 'flibtest: piousfsetcwd() failed'
         goto 1000
      endif

c open, read, and write file in all views with all read/write variants

      buffersz = (BUFSZ / (SU * dscnt)) * (SU * dscnt)

      do 10, i = 1, buffersz
         wbuf(i:i) = char(ichar('0') + mod((i - 1), dscnt))
 10   continue



      do 20, trial = 1, 4

c     global view - default servers

         if (trial .eq. 1) then
            call piousfpopen(GROUPNAME1,
     >                       FILENAME,
     >                       PIOUS_GLOBAL,
     >                       SU,
     >                       PIOUS_VOLATILE,
     >                       (PIOUS_RDWR + PIOUS_CREAT + PIOUS_TRUNC),
     >                       REGMODE,
     >                       dscnt,
     >                       fd(trial))

            if (fd(trial) .lt. 0) then
               print *, 'flibtest: piousfpopen() failed'
               goto 1000
            endif

c     independent view - default servers

         else if (trial .eq. 2) then

            call piousfopen(FILENAME,
     >                      (PIOUS_RDWR + PIOUS_CREAT + PIOUS_TRUNC),
     >                      REGMODE,
     >                      fd(trial))

            if (fd(trial) .lt. 0) then
               print *, 'flibtest: piousfopen() failed'
               goto 1000
            endif

c     independent view - specified servers

         else if (trial .eq. 3) then

            call piousfsopen(pds_hname, pds_spath, pds_lpath, pds_cnt,
     >                       FILENAME,
     >                       (PIOUS_RDWR + PIOUS_CREAT + PIOUS_TRUNC),
     >                       REGMODE,
     >                       fd(trial))

            if (fd(trial) .lt. 0) then
               print *, 'flibtest: piousfsopen() failed'
               goto 1000
            endif

c     segmented view - specified servers


         else

            call piousfspopen(pds_hname, pds_spath, pds_lpath, pds_cnt,
     >                        GROUPNAME2,
     >                        FILENAME,
     >                        PIOUS_SEGMENTED,
     >                        (dscnt - 1),
     >                        PIOUS_VOLATILE,
     >                        (PIOUS_RDWR + PIOUS_CREAT + PIOUS_TRUNC),
     >                        REGMODE,
     >                        (dscnt + 5),
     >                        fd(trial))

            if (fd(trial) .lt. 0) then
               print *, 'flibtest: piousfspopen() failed'
               goto 1000
            endif
         endif


c check file status

         call piousffstat(fd(trial), statdscnt, statsegcnt, acode)

         if (acode .ne. PIOUS_OK) then
            print *, 'flibtest: piousffstat() failed - trial ', trial
            goto 1000
         endif

         if ((statdscnt .ne. dscnt) .or. (statsegcnt .ne. dscnt)) then
            print *, 'flibtest: piousffstat() inconsistent - trial ',
     >               trial
            goto 1000
         endif

c initialize file

         if (trial .eq. 1) then

            do 30, i = 1, FILESZ
               call piousfwrite(fd(trial), wbuf, buffersz, acode)

               if (acode .lt. buffersz) then
                  if (acode .lt. 0) then
                     print *, 'flibtest: piousfwrite() failed (init)'
                  else
                     print *, 'flibtest: can''t write full buf (init)'
                  endif

                  goto 1000
               endif
 30         continue

            call piousflseek(fd(trial), 0, PIOUS_SEEK_SET, acode)

            if (acode .ne. 0) then
               print *, 'flibtest: piousflseek() failed (init)'
               goto 1000
            endif
         endif

c set pattern buffer; expected result reading from previous iteration

         if ((trial .eq. 1) .or. (trial .eq. 3)) then
            pbuf(1:buffersz) = wbuf(1:buffersz)
            rdwrsz           = buffersz

         else if (trial .eq. 2) then
            pbp = 1

            do 40, i = 0, (buffersz - 1), (SU * dscnt)
               do 50, j = 0, (SU - 1)
                  do 60, k = 0, (dscnt - 1)
                     wbp = (i + (j + (SU * k))) + 1

                     pbuf(pbp:pbp) = wbuf(wbp:wbp)

                     pbp = pbp + 1
 60               continue
 50            continue
 40         continue

            rdwrsz = buffersz

         else
            pbuf(1:1) = char(ichar('0') + (dscnt - 1))

            do 70, i = 2, (buffersz / dscnt)
               pbuf(i:i) = pbuf((i - 1):(i - 1))
 70         continue

            rdwrsz = buffersz / dscnt
         endif

c read/validate data

         i = 0

 500     continue

         do 80, j = 1, rdwrsz
            rbuf(j:j) = 'x'
 80      continue

         if (mod(i, 3) .eq. 0) then
            call piousfread(fd(trial), rbuf, rdwrsz, acode)

         else if (mod(i, 3) .eq. 1) then
            call piousforead(fd(trial), rbuf, rdwrsz, rdwroff, acode)

         else
            call piousfpread(fd(trial),
     >                       rbuf, rdwrsz, (i * rdwrsz), acode)
         endif

         if ((acode .ne. rdwrsz) .and.
     >       ((acode .ne. 0) .or. (i .ne. FILESZ))) then

            if (acode .lt. 0) then
               print *, 'flibtest: piousf*read() failed - trial ', trial
            else
               print *, 'flibtest: unexpected EOF - trial ', trial
            endif

            goto 1000
         endif

         if (acode .eq. rdwrsz) then
            if (rbuf(1:acode) .ne. pbuf(1:acode)) then
               print *, 'flibtest: file data corrupted - trial ', trial
               goto 1000
            endif
         endif

         if (mod(i, 3) .eq. 1) then
            if (rdwroff .ne. (i * rdwrsz)) then
               print *, 'flibtest: piousforead() bad offset - trial ',
     >                  trial
               goto 1000
            endif

         else if (mod(i, 3) .eq. 2) then
            call piousflseek(fd(trial), rdwrsz, PIOUS_SEEK_CUR, j)

            if (j .ne. ((i + 1) * rdwrsz)) then
               print *, 'flibtest: piousflseek() adjust bad - trial ',
     >                  trial
               goto 1000
            endif
         endif

         i = i + 1

         if (acode .ne. 0) then
            goto 500
         endif


c seek back to beginning of file

         call piousflseek(fd(trial), 0, PIOUS_SEEK_SET, acode)

         if (acode .ne. 0) then
            print *, 'flibtest: piousflseek() failed - trial ', trial
            goto 1000
         endif

c write to file

         do 90, i = 0, (FILESZ - 1)

            if (mod(i, 3) .eq. 0) then
               call piousfwrite(fd(trial), wbuf, rdwrsz, acode)

            else if (mod(i, 3) .eq. 1) then
               call piousfowrite(fd(trial),
     >                           wbuf, rdwrsz, rdwroff, acode)

            else
               call piousfpwrite(fd(trial),
     >                           wbuf, rdwrsz, (i * rdwrsz), acode)
            endif


            if (acode .ne. rdwrsz) then
               if (acode .lt. 0) then
                  print *, 'flibtest: piousf*write() failed - trial ',
     >                     trial
               else
                  print *, 'flibtest: can''t write full buf - trial ',
     >                     trial
               endif

               goto 1000
            endif

            if (mod(i, 3) .eq. 1) then
               if (rdwroff .ne. (i * rdwrsz)) then
                  print *,'flibtest: piousfowrite() bad offset- trial ',
     >                    trial
                  goto 1000
               endif


            else if (mod(i, 3) .eq. 2) then
               call piousflseek(fd(trial), rdwrsz, PIOUS_SEEK_CUR, j)

               if (j .ne. ((i + 1) * rdwrsz)) then
                  print *, 'flibtest: piousflseek() adjust bad- trial ',
     >                     trial
                  goto 1000
               endif
            endif
 90      continue
 20   continue



c write file in the context of a transaction

      call piousftbegin(PIOUS_VOLATILE, acode)

      if (acode .ne. PIOUS_OK) then
         print *, 'flibtest: piousftbegin() failed'
         goto 1000
      endif

      call piousflseek(fd(1), 0, PIOUS_SEEK_SET, acode)

      if (acode .ne. 0) then
         print *, 'flibtest: piousflseek() failed - trans'
         goto 1000
      endif

      call piousfwrite(fd(1), wbuf, buffersz, acode)

      if (acode .lt. buffersz) then
         if (acode .lt. 0) then
            print *, 'flibtest: piousfwrite() failed - trans'
         else
            print *, 'flibtest: can not write full buffer - trans'
         endif

         goto 1000
      endif

      call piousftend(acode)

      if (acode .ne. PIOUS_OK) then
         print *, 'flibtest: piousftend() failed'
         goto 1000
      endif


      call piousftbegin(PIOUS_VOLATILE, acode)

      if (acode .ne. PIOUS_OK) then
         print *, 'flibtest: piousftbegin() failed'
         goto 1000
      endif

      call piousflseek(fd(1), 0, PIOUS_SEEK_SET, acode)

      if (acode .ne. 0) then
         print *, 'flibtest: piousflseek() failed - trans'
         goto 1000
      endif

      call piousfwrite(fd(1), wbuf, buffersz, acode)

      if (acode .lt. buffersz) then
         if (acode .lt. 0) then
            print *, 'flibtest: piousfwrite() failed - trans'
         else
            print *, 'flibtest: can not write full buffer - trans'
         endif

         goto 1000
      endif

      call piousftabort(acode)

      if (acode .ne. PIOUS_OK) then
         print *, 'flibtest: piousftabort() failed'
         goto 1000
      endif


c close all file descriptors

      do 100, trial = 1, 4
         call piousfclose(fd(trial), acode)

         if (acode .ne. PIOUS_OK) then
            print *, 'flibtest: piousfclose() failed - trial ', trial
            goto 1000
         endif
 100  continue


c change file permission bits

      call piousfchmod(FILENAME, (PIOUS_IRUSR + PIOUS_IWUSR), acode)

      if (acode .lt. 0) then
         print *, 'flibtest: piousfchmod() failed'
         goto 1000
      endif

      call piousfschmod(pds_hname, pds_spath, pds_lpath, pds_cnt,
     >                  FILENAME, PIOUS_IRUSR, acode)

      if (acode .lt. 0) then
         print *, 'flibtest: piousfschmod() failed'
         goto 1000
      endif


c reset current working directory path

      call piousfgetcwd(fullfilepath, acode)

      if (acode .ne. PIOUS_OK) then
         print *, 'flibtest: piousfgetcwd() failed'
         goto 1000
      endif

      if (fullfilepath .ne. dirname) then
         print *, 'flibtest: piousfgetcwd() value erroneous'
         goto 1000
      endif

      fullfilepath = ' '

      call piousfsetcwd(fullfilepath, acode)

      if (acode .ne. PIOUS_OK) then
         print *, 'flibtest: piousfsetcwd() failed'
         goto 1000
      endif

      fullfilepath = 'A'

      call piousfgetcwd(fullfilepath, acode)

      if (acode .ne. PIOUS_OK) then
         print *, 'flibtest: piousfgetcwd() failed'
         goto 1000
      endif

      if (fullfilepath(1:1) .ne. ' ') then
         print *, 'flibtest: piousfgetcwd() value erroneous'
         goto 1000
      endif


c unlink working file, checking file placement

      fullfilepath = dirname(:(index(dirname, ' ') - 1)) // '/' //
     >               FILENAME

      call piousfsunlink(pds_hname, pds_spath, pds_lpath, pds_cnt,
     >                   fullfilepath, acode)

      if (acode .ne. PIOUS_OK) then
         print *, 'flibtest: piousfsunlink() failed'
         goto 1000
      endif

      call piousfunlink(fullfilepath, acode)

      if ((acode .ne. PIOUS_OK) .and. (acode .ne. PIOUS_ENOENT)) then
         print *, 'flibtest: piousfunlink() failed'
         goto 1000
      endif


c change directory permission bits

      call piousfchmoddir(dirname, 0, acode)

      if (acode .ne. PIOUS_OK) then
         print *, 'flibtest: piousfchmoddir() failed'
         goto 1000
      endif

      call piousfschmoddir(pds_hname, pds_spath, pds_lpath, pds_cnt,
     >                     dirname, PIOUS_IRWXU, acode)

      if (acode .ne. PIOUS_OK) then
         print *, 'flibtest: piousfschmoddir() failed'
         goto 1000
      endif


c remove directory file

      call piousfsrmdir(pds_hname, pds_spath, pds_lpath, pds_cnt,
     >                  dirname, acode)

      if ((acode .ne. PIOUS_OK) .and. (acode .ne. PIOUS_ENOENT)) then
         print *, 'flibtest: piousfsrmdir() failed'
         goto 1000
      endif

      call piousfrmdir(dirname, acode)

      if (acode .ne. PIOUS_ENOENT) then
         print *, 'flibtest: piousfrmdir() failed'
         goto 1000
      endif

c test completed, exit PVM

      print *
      print *, 'PIOUS passed Fortran library test for this system'
      print *

      call pvmfexit(acode)

      stop


c bailout on error

 1000 continue

      print *, 'flibtest: Bailing - reset PVM to shutdown PIOUS as'
      print *, '          state may be inconsistent'

      call pvmfexit(acode)
      stop
      end
