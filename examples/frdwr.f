c
c frdwr.f - a simple PIOUS demonstration program
c
c Performs the following actions:
c    1) open/create a PIOUS file
c    2) write data to the file
c    3) read data from the file and validate
c    4) close the file
c    5) unlink the file
c
c
c @(#)frdwr.f	2.2  28 Apr 1995  Moyer
c


      program frdwr
      include 'fpvm3.h'
      include 'fpious1.h'

c define read/write buffer size and file size (in number of buffers)

      integer BUFSZ, FILESZ

      parameter (BUFSZ  = 1024)
      parameter (FILESZ =   16)


c define program variables

      integer i, j, dscnt, fd, rcode, mytid

      character*(BUFSZ) wbuf, rbuf
      character*(PIOUS_PATH_MAX) filename


c print heading

      print *
      print *, 'FRDWR - Create and access a PIOUS file'
      print *
      print *

c enroll in PVM

      call pvmfmytid(mytid)

      if (mytid .lt. 0) then
         print *, 'frdwr: unable to enroll in PVM'
         stop
      endif

c determine if PIOUS started with a default set of data servers

      call piousfsysinfo(PIOUS_DS_DFLT, dscnt)

      if (dscnt .le. 0) then
         if (dscnt .eq. 0) then
            print *, 'frdwr: configure PIOUS with default data servers'
         else
            print *, 'frdwr: start PIOUS before executing application'
         endif

         goto 1000
      endif


c get name of file to work with

      print *, 'Enter file name:'
      read  (*, '(A)') filename
      print *


c open/create PIOUS file

      print *, 'Opening PIOUS file...'
      print *

      call piousfopen(filename,
     >                (PIOUS_RDWR + PIOUS_CREAT + PIOUS_TRUNC),
     >                (PIOUS_IRUSR + PIOUS_IWUSR),
     >                fd)

      if (fd .lt. 0) then
         print *, 'frdwr: can not open/create PIOUS file'
         goto 1000
      endif



c write PIOUS file

      print *, 'Writing PIOUS file...'
      print *

      do 10, i = 1, BUFSZ
         wbuf(i:i) = 'A'
 10   continue

      do 20, i = 1, FILESZ
         rcode = 0

         call piousfwrite(fd, wbuf, BUFSZ, rcode)

         if (rcode .lt. BUFSZ) then
            if (rcode .lt. 0) then
               print *, 'frdwr: error writing PIOUS file'
            else
               print *, 'frdwr: can not write full buffer to PIOUS file'
            endif

            goto 1000
         endif
 20   continue


c seek to beginning of PIOUS file

      call piousflseek(fd, 0, PIOUS_SEEK_SET, rcode)

      if (rcode .lt. 0) then
         print *, 'frdwr: error seeking in PIOUS file'
         goto 1000
      endif


c read PIOUS file and validate contents */

      print *, 'Reading PIOUS file...'
      print *

      do 30, i = 1, FILESZ

         do 40, j = 1, BUFSZ
            rbuf(j:j) = 'Z'
 40      continue

         call piousfread(fd, rbuf, BUFSZ, rcode)

         if (rcode .lt. BUFSZ) then
            if (rcode .lt. 0) then
               print *, 'frdwr: error reading PIOUS file'
            else
               print *, 'frdwr: can''t read full buffer from PIOUS file'
            endif

            goto 1000
         endif

         if (rbuf .ne. wbuf) then
            print *, 'frdwr: PIOUS file data corrupted'
            goto 1000
         endif
 30   continue


c close/unlink PIOUS file

      print *, 'Closing PIOUS file...'
      print *

      call piousfclose(fd, rcode)

      print *, 'Unlinking PIOUS file...'

      call piousfunlink(filename, rcode)


c exit PVM

      call pvmfexit(rcode)

      stop


c bailout on error

 1000 continue

      print *, 'frdwr: Bailing - reset PVM to shutdown PIOUS as state'
      print *, '       may be inconsistent'

      call pvmfexit(rcode)
      stop
      end
