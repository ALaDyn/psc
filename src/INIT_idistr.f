c THIS SUBROUTINE INITIALIZES THE PARTICLE DISTRIBUTION
c AND LOAD BALANCE.

c***************************************************************
c Keine adaptiven Gewichte
c***************************************************************


      subroutine INIT_idistr

      use PIC_variables
      use VLA_variables

      implicit none
      include './mpif.h'

      integer :: cell,ncel,part_label
      integer :: wwa,wwb,wwc,wwo,wwp,wwq
      integer :: a,b,c,o,p,q,r,s,t
      integer :: count_clo,count_mem,count_max
      integer :: eval_clo,eval_mem,deval,dvel
      integer :: nodei,nodej,nodek
      integer :: rds1,rds2,rds3

      integer :: xmin,xmax     ! ab
      integer :: ymin,ymax     ! ab
      integer :: zmin,zmax     ! ab

      real(kind=8) :: x,y,z
      real(kind=8) :: px,py,pz
      real(kind=8) :: ran1,ran2,ran3,ran4,ran5,ran6
      real(kind=8) :: qni,mni,cni,lni,wni,tnxi,tnyi,tnzi
      real(kind=8) :: norm_x,norm_y,norm_z
      real(kind=8) :: sh_x,sh_y,sh_z,sh,sh_o
      real(kind=8) :: M_lim,M_min,M_max,M_ref,M_tot
      real(kind=8) :: W_lim,W_min,W_max,W_ref,W_tot
      real(kind=8) :: dens,INIT_den

c Massenzahl der Neutralteiclchen bzw. der Ionen

      integer,allocatable,dimension(:) :: part_label_offset
      integer,allocatable,dimension(:) :: part_num_remote
      integer,allocatable,dimension(:) :: i1ln,i1lx
      integer,allocatable,dimension(:) :: i2ln,i2lx
      integer,allocatable,dimension(:) :: i3ln,i3lx
      integer,allocatable,dimension(:) :: rd1n,rd1x
      integer,allocatable,dimension(:) :: rd2n,rd2x
      integer,allocatable,dimension(:) :: rd3n,rd3x
      integer,allocatable,dimension(:,:,:) :: N_par

      real(kind=8),allocatable,dimension(:) :: dens_x
      real(kind=8),allocatable,dimension(:) :: dens_y
      real(kind=8),allocatable,dimension(:) :: dens_z
      real(kind=8),allocatable,dimension(:) :: rndmv
      real(kind=8),allocatable,dimension(:,:,:) :: M_loc,W_loc


      allocate(i1ln(1:xnpe))
      allocate(i1lx(1:xnpe))
      allocate(i2ln(1:ynpe))
      allocate(i2lx(1:ynpe))
      allocate(i3ln(1:znpe))
      allocate(i3lx(1:znpe))

      allocate(rd1n(1:xnpe))
      allocate(rd1x(1:xnpe))
      allocate(rd2n(1:ynpe))
      allocate(rd2x(1:ynpe))
      allocate(rd3n(1:znpe))
      allocate(rd3x(1:znpe))

      allocate(dens_x(i1n:2*i1x-i1n+1))
      allocate(dens_y(i2n:2*i2x-i2n+1))
      allocate(dens_z(i3n:2*i3x-i3n+1))

      allocate(rndmv(1:6*npe))
      allocate(N_par(1:xnpe,1:ynpe,1:znpe))
      allocate(M_loc(1:xnpe,1:ynpe,1:znpe))
      allocate(W_loc(1:xnpe,1:ynpe,1:znpe))


c This routine has to be set up in compliance with the density function
c setup in INIT_den.f. The predictor for memory allocation requires
c an estimate of the particle number. So does the load distributer. For
c this reason multiple particle counts are required to determine the 
c appropriate parameters.

      xmin=i1n       ! ab
      xmax=i1x       ! ab
      ymin=i2n       ! ab
      ymax=i2x       ! ab
      zmin=i3n       ! ab
      zmax=i3x       ! ab

      if (boundary_pml_x1.ne.'false') xmin=i1n+size+1       ! ab
      if (boundary_pml_x2.ne.'false') xmax=i1x-size-1       ! ab
      if (boundary_pml_y1.ne.'false') ymin=i2n+size+1       ! ab
      if (boundary_pml_y2.ne.'false') ymax=i2x-size-1       ! ab
      if (boundary_pml_z1.ne.'false') zmin=i3n+size+1       ! ab
      if (boundary_pml_z2.ne.'false') zmax=i3x-size-1       ! ab
 

      rds1=rd1
      rds2=rd2
      rds3=rd3


      if (i1n==i1x) rds1=0
      if (i2n==i2x) rds2=0
      if (i3n==i3x) rds3=0


      nodei=seg_i1(mpe)
      nodej=seg_i2(mpe) 
      nodek=seg_i3(mpe) 


      count_clo=0
      count_mem=0
      eval_clo=0
      eval_mem=0


c The numbers dvel and deval determine the accuracy of the memory optimization
c procedure. dvel=1 and deval=1 yield the best values but take a lot of
c computing time. For large grids dvel=2 and deval=10 prove to be good
c enough. If dvel and deval are chosen too large no memory optimization
c will happen and the algorithm may loop.


      M_lim=1.9d3             ! maximum possible memory per node in MB
      count_max=530           ! maximum number of iterations for memory and wall clock optimization
      dvel=2                  ! grid adaption velocity for memory optimization
      deval=1                 ! particle number evaluation interval
      nicell=200              ! maximum number of particles per spezies and cell


c QUASI-PARTICLE MASS FACTOR


      if (nicell.gt.0) then
         cori=1.0/nicell
      else
         cori=1.0e20
      endif


c PREDICTION OF REQUIRED LOAD DISTRIBUTION (first density setup)

c The criterion is the number of quasi-particles in the simulation
c and the number of cells in the simulation box. Particles
c and cells have different weight factors. The load is distributed
c in a way that the required local wall clock times are approximately
c equal on each node. If this strategy conflicts with the available
c memory on the distributed system optimization for memory sets in.
c If insufficient memory is available the job will terminate with
c an error message.


      dens_x=0.0
      dens_y=0.0
      dens_z=0.0


      do i3=i3n,i3x                           
         z=i3*dz
         do i2=i2n,i2x
            y=i2*dy
            do i1=i1n,i1x
               x=i1*dx

               niloc=0                           ! niloc muss hier stehen zur Berechnung der Gewichte

               dens=INIT_den(x,y,z)*pmlcheck(i1,i2,i3)       ! ab
c               if (dens > 0.9) write(6,*) 'INIT_den',dens, i1, i2, i3

               if (dens.gt.0.001) then
               ncel=nint(dens/cori)
               do l=1,ncel
                  niloc=niloc+1     ! e-
               enddo
               endif

               dens_x(i1)=dens_x(i1)+niloc    ! a quasi-particle has weight 1
               dens_y(i2)=dens_y(i2)+niloc    ! a quasi-particle has weight 1
               dens_z(i3)=dens_z(i3)+niloc    ! a quasi-particle has weight 1

            enddo
         enddo
      enddo

      do i3=i3n,i3x                      
         z=i3*dz
         do i2=i2n,i2x
            y=i2*dy
            do i1=i1n,i1x
               x=i1*dx

               niloc=0                           ! niloc muss hier stehen zur Berechnung der Gewichte

               dens=INIT_den(x,y,z)*pmlcheck(i1,i2,i3)       ! ab
               if (dens.gt.0.001) then
               ncel=nint(dens/cori)
               do l=1,ncel
                  niloc=niloc+1     ! p+
               enddo
               endif

               dens_x(i1)=dens_x(i1)+niloc    ! a quasi-particle has weight
               dens_y(i2)=dens_y(i2)+niloc    ! a quasi-particle has weight
               dens_z(i3)=dens_z(i3)+niloc    ! a quasi-particle has weight

            enddo
         enddo
      enddo


      norm_x=0.0
      do i1=i1n,i1x                      
         norm_x=norm_x+dens_x(i1)
      enddo
      sh_x=norm_x/xnpe

      norm_y=0.0
      do i2=i2n,i2x                      
         norm_y=norm_y+dens_y(i2)
      enddo
      sh_y=norm_y/ynpe

      norm_z=0.0
      do i3=i3n,i3x                      
         norm_z=norm_z+dens_z(i3)
      enddo
      sh_z=norm_z/znpe


c MINIMUM PARTITION SIZE IS rdi+1 IN EACH OF THE x, y, AND z DIRECTIONS IF POSSIBLE


      if (i1x-i1n+1.ge.(rd1+1)*xnpe) then
         sh=0.0
         sh_o=0.0
         i1ln(1)=i1n
         i1lx(1)=i1n+rd1
         do i1=i1n,i1n+rd1
            sh=sh+dens_x(i1)
            sh_o=sh
         enddo
         do i1=i1n+rd1+1,i1x
            sh=sh+dens_x(i1)
            if (abs(sh-sh_x).lt.abs(sh_o-sh_x)) i1lx(1)=i1
            sh_o=sh
         enddo
         do o=2,xnpe
            sh=0.0
            sh_o=0.0
            i1ln(o)=i1lx(o-1)+1
            i1lx(o)=i1lx(o-1)+rd1+1
            do i1=i1ln(o),i1ln(o)+rd1
               sh=sh+dens_x(i1)
               sh_o=sh
            enddo
            do i1=i1ln(o)+rd1+1,i1x
               sh=sh+dens_x(i1)
               if (abs(sh-sh_x).lt.abs(sh_o-sh_x)) i1lx(o)=i1
               sh_o=sh
            enddo
         enddo
      else
         sh=0.0
         sh_o=0.0
         i1ln(1)=i1n
         i1lx(1)=i1n
         do i1=i1n,i1x
            sh=sh+dens_x(i1)
            if (abs(sh-sh_x).lt.abs(sh_o-sh_x)) i1lx(1)=i1
            sh_o=sh
         enddo
         do o=2,xnpe
            sh=0.0
            sh_o=0.0
            i1ln(o)=i1lx(o-1)+1
            i1lx(o)=i1lx(o-1)+1
            do i1=i1ln(o),i1x
               sh=sh+dens_x(i1)
               if (abs(sh-sh_x).lt.abs(sh_o-sh_x)) i1lx(o)=i1
               sh_o=sh
            enddo
         enddo
      endif


      if (i1lx(xnpe).lt.i1x) then
         i1lx(xnpe)=i1x
      endif
 1111 if (i1lx(xnpe).gt.i1x) then
         wwo=i1lx(1)-i1ln(1)+1
         o=1
         do a=1,xnpe
            wwa=i1lx(a)-i1ln(a)+1
            if (wwa.gt.wwo) then
                o=a
                wwo=wwa
            endif
         enddo
         if (wwo.gt.rd1+1) then
            i1lx(o)=i1lx(o)-1
            do r=o+1,xnpe
               i1ln(r)=i1ln(r)-1
               i1lx(r)=i1lx(r)-1
            enddo
         endif
         goto 1111
      endif


      if (i2x-i2n+1.ge.(rd2+1)*ynpe) then
         sh=0.0
         sh_o=0.0
         i2ln(1)=i2n
         i2lx(1)=i2n+rd2
         do i2=i2n,i2n+rd2
            sh=sh+dens_y(i2)
            sh_o=sh
         enddo
         do i2=i2n+rd2+1,i2x
            sh=sh+dens_y(i2)
            if (abs(sh-sh_y).lt.abs(sh_o-sh_y)) i2lx(1)=i2
            sh_o=sh
         enddo
         do p=2,ynpe
            sh=0.0
            sh_o=0.0
            i2ln(p)=i2lx(p-1)+1
            i2lx(p)=i2lx(p-1)+rd2+1
            do i2=i2ln(p),i2ln(p)+rd2
               sh=sh+dens_y(i2)
               sh_o=sh
            enddo
            do i2=i2ln(p)+rd2+1,i2x
               sh=sh+dens_y(i2)
               if (abs(sh-sh_y).lt.abs(sh_o-sh_y)) i2lx(p)=i2
               sh_o=sh
            enddo
         enddo
      else
         sh=0.0
         sh_o=0.0
         i2ln(1)=i2n
         i2lx(1)=i2n
         do i2=i2n,i2x
            sh=sh+dens_y(i2)
            if (abs(sh-sh_y).lt.abs(sh_o-sh_y)) i2lx(1)=i2
            sh_o=sh
         enddo
         do p=2,ynpe
            sh=0.0
            sh_o=0.0
            i2ln(p)=i2lx(p-1)+1
            i2lx(p)=i2lx(p-1)+1
            do i2=i2ln(p),i2x
               sh=sh+dens_y(i2)
               if (abs(sh-sh_y).lt.abs(sh_o-sh_y)) i2lx(p)=i2
               sh_o=sh
            enddo
         enddo
      endif

      if (i2lx(ynpe).lt.i2x) then
         i2lx(ynpe)=i2x
      endif
 1112 if (i2lx(ynpe).gt.i2x) then
         wwp=i2lx(1)-i2ln(1)+1
         p=1
         do b=1,ynpe
            wwb=i2lx(b)-i2ln(b)+1
            if (wwb.gt.wwp) then
                p=b
                wwp=wwb
            endif
         enddo
         if (wwp.gt.rd2+1) then
            i2lx(p)=i2lx(p)-1
            do s=p+1,ynpe
               i2ln(s)=i2ln(s)-1
               i2lx(s)=i2lx(s)-1
            enddo
         endif
         goto 1112
      endif


      if (i3x-i3n+1.ge.(rd3+1)*znpe) then
         sh=0.0
         sh_o=0.0
         i3ln(1)=i3n
         i3lx(1)=i3n+rd3
         do i3=i3n,i3n+rd3
            sh=sh+dens_z(i3)
            sh_o=sh
         enddo
         do i3=i3n+rd3+1,i3x
            sh=sh+dens_z(i3)
            if (abs(sh-sh_z).lt.abs(sh_o-sh_z)) i3lx(1)=i3
            sh_o=sh
         enddo
         do q=2,znpe
            sh=0.0
            sh_o=0.0
            i3ln(q)=i3lx(q-1)+1
            i3lx(q)=i3lx(q-1)+rd3+1
            do i3=i3ln(q),i3ln(q)+rd3
               sh=sh+dens_z(i3)
               sh_o=sh
            enddo
            do i3=i3ln(q)+rd3+1,i3x
               sh=sh+dens_z(i3)
               if (abs(sh-sh_z).lt.abs(sh_o-sh_z)) i3lx(q)=i3
               sh_o=sh
            enddo
         enddo
      else
         sh=0.0
         sh_o=0.0
         i3ln(1)=i3n
         i3lx(1)=i3n
         do i3=i3n,i3x
            sh=sh+dens_z(i3)
            if (abs(sh-sh_z).lt.abs(sh_o-sh_z)) i3lx(1)=i3
            sh_o=sh
         enddo
         do q=2,znpe
            sh=0.0
            sh_o=0.0
            i3ln(q)=i3lx(q-1)+1
            i3lx(q)=i3lx(q-1)+1
            do i3=i3ln(q),i3x
               sh=sh+dens_z(i3)
               if (abs(sh-sh_z).lt.abs(sh_o-sh_z)) i3lx(q)=i3
               sh_o=sh
            enddo
         enddo
      endif

      if (i3lx(znpe).lt.i3x) then
         i3lx(znpe)=i3x
      endif
 1113 if (i3lx(znpe).gt.i3x) then
         wwq=i3lx(1)-i3ln(1)+1
         q=1
         do c=1,znpe
            wwc=i3lx(c)-i3ln(c)+1
            if (wwc.gt.wwq) then
                q=c
                wwq=wwc
            endif
         enddo
         if (wwq.gt.rd3+1) then
            i3lx(q)=i3lx(q)-1
            do t=q+1,znpe
               i3ln(t)=i3ln(t)-1
               i3lx(t)=i3lx(t)-1
            enddo
         endif
         goto 1113
      endif


c ESTIMATE WALL CLOCK TIME AND MEMORY (second and third density setup)


 1114 W_max=0.0d0
      W_tot=0.0d0

      do c=1,znpe
         wwc=i3lx(c)-i3ln(c)+1
         do b=1,ynpe
            wwb=i2lx(b)-i2ln(b)+1
            do a=1,xnpe
               wwa=i1lx(a)-i1ln(a)+1

               if (count_clo==eval_clo) then

                  niloc=0
                  do i3=i3ln(c),i3lx(c)
                     z=i3*dz
                     do i2=i2ln(b),i2lx(b)
                        y=i2*dy
                        do i1=i1ln(a),i1lx(a)
                           x=i1*dx

                           dens=INIT_den(x,y,z)*pmlcheck(i1,i2,i3)  ! ab
                           if (dens.gt.0.001) then
                           ncel=nint(dens/cori)
                           do l=1,ncel
                              niloc=niloc+1     ! e-
                           enddo
                           endif

                        enddo
                     enddo
                  enddo
                  do i3=i3ln(c),i3lx(c)
                     z=i3*dz
                     do i2=i2ln(b),i2lx(b)
                        y=i2*dy
                        do i1=i1ln(a),i1lx(a)
                           x=i1*dx

                           dens=INIT_den(x,y,z)*pmlcheck(i1,i2,i3)  ! ab
                           if (dens.gt.0.001) then
                           ncel=nint(dens/cori)
                           do l=1,ncel
                              niloc=niloc+1     ! p+
                           enddo
                           endif

                        enddo
                     enddo
                  enddo

                  N_par(a,b,c)=niloc

               endif


c Wall clock time predictor. The assumption is that on average a quasi-
c particle contributes the load 1 and a cell the load 0.0.


               W_loc(a,b,c)=N_par(a,b,c)+1.0*wwa*wwb*wwc
               W_tot=W_tot+W_loc(a,b,c)


               if (W_loc(a,b,c).gt.W_max) then
                   o=a
                   p=b
                   q=c
                   wwo=wwa
                   wwp=wwb
                   wwq=wwc
                   W_max=W_loc(a,b,c)
               endif

            enddo
         enddo
      enddo


      W_lim=1.01*W_tot/npe


c RE-ARRANGE PARTITIONS TOO REDUCE LOCAL WALL CLOCK TIME


      if (W_max.gt.W_lim) then

         if (xnpe.gt.1) then
            if (wwo.gt.rd1+2*dvel+1) then
               if (1.eq.o) then
                  i1lx(o)=i1lx(o)-dvel
                  i1ln(o+1)=i1ln(o+1)-dvel
               endif
               if (1.lt.o.and.o.lt.xnpe) then
                  i1lx(o-1)=i1lx(o-1)+dvel
                  i1ln(o)=i1ln(o)+dvel
                  i1lx(o)=i1lx(o)-dvel
                  i1ln(o+1)=i1ln(o+1)-dvel
               endif
               if (o.eq.xnpe) then
                  i1lx(o-1)=i1lx(o-1)+dvel
                  i1ln(o)=i1ln(o)+dvel
               endif
            endif
         endif
         if (ynpe.gt.1) then
            if (wwp.gt.rd2+2*dvel+1) then
               if (1.eq.p) then
                  i2lx(p)=i2lx(p)-dvel
                  i2ln(p+1)=i2ln(p+1)-dvel
               endif
               if (1.lt.p.and.p.lt.ynpe) then
                  i2lx(p-1)=i2lx(p-1)+dvel
                  i2ln(p)=i2ln(p)+dvel
                  i2lx(p)=i2lx(p)-dvel
                  i2ln(p+1)=i2ln(p+1)-dvel
               endif
               if (p.eq.ynpe) then
                  i2lx(p-1)=i2lx(p-1)+dvel
                  i2ln(p)=i2ln(p)+dvel
               endif
            endif
         endif
         if (znpe.gt.1) then
            if (wwq.gt.rd3+2*dvel+1) then
               if (1.eq.q) then
                  i3lx(q)=i3lx(q)-dvel
                  i3ln(q+1)=i3ln(q+1)-dvel
               endif
               if (1.lt.q.and.q.lt.znpe) then
                  i3lx(q-1)=i3lx(q-1)+dvel
                  i3ln(q)=i3ln(q)+dvel
                  i3lx(q)=i3lx(q)-dvel
                  i3ln(q+1)=i3ln(q+1)-dvel
               endif
               if (q.eq.znpe) then
                  i3lx(q-1)=i3lx(q-1)+dvel
                  i3ln(q)=i3ln(q)+dvel
               endif
            endif
         endif

         if (count_clo==eval_clo) then
            eval_clo=eval_clo+deval
         endif
         count_clo=count_clo+1

         if (count_clo.gt.count_max) then
            goto 1115
         endif

         goto 1114

      endif


 1115 M_max=0.0d0
      M_tot=0.0d0

      do c=1,znpe
         wwc=i3lx(c)-i3ln(c)+1
         do b=1,ynpe
            wwb=i2lx(b)-i2ln(b)+1
            do a=1,xnpe
               wwa=i1lx(a)-i1ln(a)+1

               if (count_mem==eval_mem) then

                  niloc=0
                  do i3=i3ln(c),i3lx(c)
                     z=i3*dz
                     do i2=i2ln(b),i2lx(b)
                        y=i2*dy
                        do i1=i1ln(a),i1lx(a)
                           x=i1*dx

                           dens=INIT_den(x,y,z)*pmlcheck(i1,i2,i3)  ! ab
                           if (dens.gt.0.001) then
                           ncel=nint(dens/cori)
                           do l=1,ncel
                              niloc=niloc+1     ! e-
                           enddo
                           endif

                        enddo
                     enddo
                  enddo
                  do i3=i3ln(c),i3lx(c)
                     z=i3*dz
                     do i2=i2ln(b),i2lx(b)
                        y=i2*dy
                        do i1=i1ln(a),i1lx(a)
                           x=i1*dx

                           dens=INIT_den(x,y,z)*pmlcheck(i1,i2,i3)  ! ab
                           if (dens.gt.0.001) then
                           ncel=nint(dens/cori)
                           do l=1,ncel
                              niloc=niloc+1     ! p+
                           enddo
                           endif

                        enddo
                     enddo
                  enddo

                  N_par(a,b,c)=niloc

               endif


c Memory allocation predictor based on 24 REAL*4 and 12 REAL*8 fields plus the 
c particle array. In addition, about 250MB of memory are required for communication. 
c Helper fields have only a minor impact on memory allocation.


               M_loc(a,b,c)=0.96d-4*(wwa+2*rd1)*(wwb+2*rd2)*(wwc+2*rd3)
     &                      +0.96d-4*wwa*wwb*wwc
     &                      +8.5d-5*N_par(a,b,c)
     &                      +2.5d2


               M_tot=M_tot+M_loc(a,b,c)


               if (M_loc(a,b,c).gt.M_max) then
                   o=a
                   p=b
                   q=c
                   wwo=wwa
                   wwp=wwb
                   wwq=wwc
                   M_max=M_loc(a,b,c)
               endif

            enddo
         enddo
      enddo


c RE-ARRANGE PARTITIONS TOO REDUCE LOCAL MEMORY ALLOCATION


      if (M_tot.le.M_lim*npe) then
   
         if (M_max.gt.M_lim) then

            if (xnpe.gt.1) then
               if (wwo.gt.rd1+2*dvel+1) then
                  if (1.eq.o) then
                     i1lx(o)=i1lx(o)-dvel
                     i1ln(o+1)=i1ln(o+1)-dvel
                  endif
                  if (1.lt.o.and.o.lt.xnpe) then
                     i1lx(o-1)=i1lx(o-1)+dvel
                     i1ln(o)=i1ln(o)+dvel
                     i1lx(o)=i1lx(o)-dvel
                     i1ln(o+1)=i1ln(o+1)-dvel
                  endif
                  if (o.eq.xnpe) then
                     i1lx(o-1)=i1lx(o-1)+dvel
                     i1ln(o)=i1ln(o)+dvel
                  endif
               endif
            endif
            if (ynpe.gt.1) then
               if (wwp.gt.rd2+2*dvel+1) then
                  if (1.eq.p) then
                     i2lx(p)=i2lx(p)-dvel
                     i2ln(p+1)=i2ln(p+1)-dvel
                  endif
                  if (1.lt.p.and.p.lt.ynpe) then
                     i2lx(p-1)=i2lx(p-1)+dvel
                     i2ln(p)=i2ln(p)+dvel
                     i2lx(p)=i2lx(p)-dvel
                     i2ln(p+1)=i2ln(p+1)-dvel
                  endif
                  if (p.eq.ynpe) then
                     i2lx(p-1)=i2lx(p-1)+dvel
                     i2ln(p)=i2ln(p)+dvel
                  endif
               endif
            endif
            if (znpe.gt.1) then
               if (wwq.gt.rd3+2*dvel+1) then
                  if (1.eq.q) then
                     i3lx(q)=i3lx(q)-dvel
                     i3ln(q+1)=i3ln(q+1)-dvel
                  endif
                  if (1.lt.q.and.q.lt.znpe) then
                     i3lx(q-1)=i3lx(q-1)+dvel
                     i3ln(q)=i3ln(q)+dvel
                     i3lx(q)=i3lx(q)-dvel
                     i3ln(q+1)=i3ln(q+1)-dvel
                  endif
                  if (q.eq.znpe) then
                     i3lx(q-1)=i3lx(q-1)+dvel
                     i3ln(q)=i3ln(q)+dvel
                  endif
               endif
            endif

            if (count_mem==eval_mem) then
               eval_mem=eval_mem+deval
            endif
            count_mem=count_mem+1

            if (count_mem.gt.count_max) then
               goto 1116
            endif

            goto 1115

         endif

      else
         if (mpe.eq.0) then
            write(6,*) 'REQUIRED MEMORY ALLOCATION TOO LARGE!'
         endif
         call MPI_FINALIZE(info) 
         stop
      endif


 1116 i1mn=i1ln(nodei)
      i1mx=i1lx(nodei)
      i2mn=i2ln(nodej)
      i2mx=i2lx(nodej)
      i3mn=i3ln(nodek)
      i3mx=i3lx(nodek)


c EXACT LOCAL NUMBER OF QUASI-PARTICLES (fourth density setup)


      rd1n=0
      rd1x=0
      rd2n=0
      rd2x=0
      rd3n=0
      rd3x=0
      if (boundary_part_x.eq.1) then
         if (i1n.ne.i1x) then
            do a=1,xnpe
               if (i1ln(a).eq.i1n) then
                  rd1n(a)=-1
               endif
               if (i1lx(a).eq.i1x) then
                  rd1x(a)=+1
               endif
            enddo
        endif
      endif
      if (boundary_part_y.eq.1) then
         if (i2n.ne.i2x) then
            do b=1,ynpe
               if (i2ln(b).eq.i2n) then
                  rd2n(b)=-1
               endif
               if (i2lx(b).eq.i2x) then
                  rd2x(b)=+1
               endif
            enddo
         endif
      endif
      if (boundary_part_z.eq.1) then
         if (i3n.ne.i3x) then
            do c=1,znpe
               if (i3ln(c).eq.i3n) then
                  rd3n(c)=-1
               endif
               if (i3lx(c).eq.i3x) then
                  rd3x(c)=+1
               endif
            enddo
         endif
      endif


      niloc=0
      do i3=i3mn+rd3n(nodek),i3mx+rd3x(nodek)
         z=i3*dz
         do i2=i2mn+rd2n(nodej),i2mx+rd2x(nodej)
            y=i2*dy
            do i1=i1mn+rd1n(nodei),i1mx+rd1x(nodei)
               x=i1*dx

               dens=INIT_den(x,y,z)*pmlcheck(i1,i2,i3)  ! ab
               if (dens.gt.0.001) then
               ncel=nint(dens/cori)
               do l=1,ncel
                  niloc=niloc+1     ! e-
               enddo
               endif

            enddo
         enddo
      enddo
      do i3=i3mn+rd3n(nodek),i3mx+rd3x(nodek)
         z=i3*dz
         do i2=i2mn+rd2n(nodej),i2mx+rd2x(nodej)
            y=i2*dy
            do i1=i1mn+rd1n(nodei),i1mx+rd1x(nodei)
               x=i1*dx

               dens=INIT_den(x,y,z)*pmlcheck(i1,i2,i3)  ! ab
               if (dens.gt.0.001) then
               ncel=nint(dens/cori)
               do l=1,ncel
                  niloc=niloc+1     ! p+
               enddo
               endif

            enddo
         enddo
      enddo


c SETTING UP UNIQUE PARTICLE LABELS

c part_label_offset: index, at which a unique particle label has to start on this node
c part_num_remote: particle numbers on remote nodes


      allocate(part_label_offset(0:npe-1))
      allocate(part_num_remote(0:npe-1))

      part_num_remote=niloc
      do pec=0,npe-1
         part_label=part_num_remote(pec)
         call MPI_BCAST(part_label,1,MPI_INTEGER,pec,
     &                  MPI_COMM_WORLD,info)
         part_num_remote(pec)=part_label
      enddo

      part_label_offset=0
      if (mpe.gt.0) then
         do pec=0,mpe-1
            part_label_offset(mpe)=part_label_offset(mpe)
     &                             +part_num_remote(pec)
         enddo
      endif

      do pec=0,npe-1
         part_label=part_label_offset(pec)
         call MPI_BCAST(part_label,1,MPI_INTEGER,pec,
     &                  MPI_COMM_WORLD,info)
         part_label_offset(pec)=part_label
      enddo


c WRITING OUT PARAMETER LIST


      if (mpe.eq.0) then
         write(6,*) ' '
         write(6,*) '!!OUTPUT FROM "INIT_idistr.f"!!'
         write(6,*) 'NUMBER OF ITERATIONS FOR SYNCHRONIZATION'
         write(6,*) 'clo:',count_clo
         write(6,*) 'mem:',count_mem
         write(6,*) ' '
         write(6,*) 'TOTAL NUMBER OF NODES, GRID SIZE'
         write(6,*) 'xnpe: ',xnpe
         write(6,*) 'ynpe: ',ynpe
         write(6,*) 'znpe: ',znpe
         write(6,*) 'npe: ',npe
         write(6,*) 'i1n:',i1n
         write(6,*) 'i1x:',i1x
         write(6,*) 'i2n:',i2n
         write(6,*) 'i2x:',i2x
         write(6,*) 'i3n:',i3n
         write(6,*) 'i3x:',i3x
         write(6,*) 'rd1:',rd1
         write(6,*) 'rd2:',rd2
         write(6,*) 'rd3:',rd3
         write(6,*) ' '
         write(6,*) 'DATA SUBDIVISION'
         do p=0,npe-1
            a=seg_i1(p)
            b=seg_i2(p)
            c=seg_i3(p)
            write(6,*) 'PE:',p
            write(6,*) 'i1mn:',i1ln(a),'i1mx:',i1lx(a)
            write(6,*) 'i2mn:',i2ln(b),'i2mx:',i2lx(b)
            write(6,*) 'i3mn:',i3ln(c),'i3mx:',i3lx(c)
            write(6,*) 'rd1n:',rd1n(a),'rd1x:',rd1x(a)
            write(6,*) 'rd2n:',rd2n(b),'rd2x:',rd2x(b)
            write(6,*) 'rd3n:',rd3n(c),'rd3x:',rd3x(c)
            write(6,*) 'N_par:',N_par(a,b,c)
            write(6,*) 'M_loc:',M_loc(a,b,c)
            write(6,*) 'W_loc:',W_loc(a,b,c)
            write(6,*) 'Label offset:',part_label_offset(p)
         enddo
         write(6,*) ' '
      endif


c SETTING UP PARTICLE PHASE SPACE


      nialloc=nint(1.2*niloc+10)
      allocate(p_niloc(0:11*nialloc+10))
      p_niloc=0.0d0
 

      niloc=0
      do i3=i3mn+rd3n(nodek),i3mx+rd3x(nodek)
         z=i3*dz
         do i2=i2mn+rd2n(nodej),i2mx+rd2x(nodej)
            y=i2*dy
            do i1=i1mn+rd1n(nodei),i1mx+rd1x(nodei)
               x=i1*dx

               cell=(i1-i1mn+rds1+1)
     &              +(i1mx-i1mn+2*rds1+1)
     &              *(i2-i2mn+rds2)
     &              +(i1mx-i1mn+2*rds1+1)
     &              *(i2mx-i2mn+2*rds2+1)
     &              *(i3-i3mn+rds3)

               dens=INIT_den(x,y,z)*pmlcheck(i1,i2,i3)    ! ab
               if (dens.gt.0.001) then
               ncel=nint(dens/cori)
               do l=1,ncel

                  qni=-1.0               ! e-
                  mni=+1.0
                  cni=+cell
                  lni=+part_label_offset(mpe)+niloc
                  wni=+dens
                  tnxi=0.0
                  tnyi=0.0
                  tnzi=0.05

c MAXWELLIAN DISTRIBUTION

                  call random_number(rndmv)
                  ran1=min(0.99999999999999999d0,rndmv(6*mpe+1))
                  ran2=rndmv(6*mpe+2)
                  ran3=min(0.99999999999999999d0,rndmv(6*mpe+3))
                  ran4=rndmv(6*mpe+4)
                  ran5=min(0.99999999999999999d0,rndmv(6*mpe+5))
                  ran6=rndmv(6*mpe+6)

                  px=sqrt(-tnxi*beta**2*log(1.0-ran1)/mni)
     &               *cos(6.2831853*ran2)
                  py=sqrt(-tnyi*beta**2*log(1.0-ran3)/mni)
     &               *cos(6.2831853*ran4)
                  pz=sqrt(-tnzi*beta**2*log(1.0-ran5)/mni)
     &               *cos(6.2831853*ran6)

                  niloc=niloc+1
                  p_niloc(11*niloc+0)=x
                  p_niloc(11*niloc+1)=y
                  p_niloc(11*niloc+2)=z
                  p_niloc(11*niloc+3)=px
                  p_niloc(11*niloc+4)=py
                  p_niloc(11*niloc+5)=pz
                  p_niloc(11*niloc+6)=qni
                  p_niloc(11*niloc+7)=mni
                  p_niloc(11*niloc+8)=cni
                  p_niloc(11*niloc+9)=lni
                  p_niloc(11*niloc+10)=wni

               enddo
               endif

            enddo
         enddo
      enddo


      do i3=i3mn+rd3n(nodek),i3mx+rd3x(nodek)
         z=i3*dz
         do i2=i2mn+rd2n(nodej),i2mx+rd2x(nodej)
            y=i2*dy
            do i1=i1mn+rd1n(nodei),i1mx+rd1x(nodei)
               x=i1*dx

               cell=(i1-i1mn+rds1+1)
     &              +(i1mx-i1mn+2*rds1+1)
     &              *(i2-i2mn+rds2)
     &              +(i1mx-i1mn+2*rds1+1)
     &              *(i2mx-i2mn+2*rds2+1)
     &              *(i3-i3mn+rds3)

               dens=INIT_den(x,y,z)*pmlcheck(i1,i2,i3)    ! ab
               if (dens.gt.0.001) then
               ncel=nint(dens/cori)
               do l=1,ncel

                  qni=+1.0               ! p+
                  mni=+1836.0
                  cni=+cell
                  lni=+part_label_offset(mpe)+niloc
                  wni=+dens
                  tnxi=+0.0
                  tnyi=+0.0
                  tnzi=+0.0

c MAXWELLIAN DISTRIBUTION

                  call random_number(rndmv)
                  ran1=min(0.99999999999999999d0,rndmv(6*mpe+1))
                  ran2=rndmv(6*mpe+2)
                  ran3=min(0.99999999999999999d0,rndmv(6*mpe+3))
                  ran4=rndmv(6*mpe+4)
                  ran5=min(0.99999999999999999d0,rndmv(6*mpe+5))
                  ran6=rndmv(6*mpe+6)

                  px=sqrt(-tnxi*beta**2*log(1.0-ran1)/mni)
     &               *cos(6.2831853*ran2)
                  py=sqrt(-tnyi*beta**2*log(1.0-ran3)/mni)
     &               *cos(6.2831853*ran4)
                  pz=sqrt(-tnzi*beta**2*log(1.0-ran5)/mni)
     &               *cos(6.2831853*ran6)

                  niloc=niloc+1
                  p_niloc(11*niloc+0)=x
                  p_niloc(11*niloc+1)=y
                  p_niloc(11*niloc+2)=z
                  p_niloc(11*niloc+3)=px
                  p_niloc(11*niloc+4)=py
                  p_niloc(11*niloc+5)=pz
                  p_niloc(11*niloc+6)=qni
                  p_niloc(11*niloc+7)=mni
                  p_niloc(11*niloc+8)=cni
                  p_niloc(11*niloc+9)=lni
                  p_niloc(11*niloc+10)=wni

               enddo
               endif

            enddo
         enddo
      enddo


c CHECK FOR ZERO PARTICLE WEIGHTS


      if (niloc.gt.0) then
         do l=1,niloc
            wni=p_niloc(11*l+10)
            if (wni.eq.0.0) then
               if (mpe.eq.0) then
                  write(6,*) 'Zero particle weights not permissible!'
               endif
               call MPI_finalize(info)
               stop
            endif
         enddo
      endif


      deallocate(i1ln,i2ln,i3ln)
      deallocate(i1lx,i2lx,i3lx)
      deallocate(rd1n,rd2n,rd3n)
      deallocate(rd1x,rd2x,rd3x)
      deallocate(dens_x,dens_y,dens_z)

      deallocate(rndmv)
      deallocate(N_par)
      deallocate(M_loc)
      deallocate(W_loc)

      deallocate(part_label_offset)
      deallocate(part_num_remote)

      contains                         ! ab
      function pmlcheck(x,y,z)
      implicit none
      integer :: x,y,z
      real(kind=8) :: pmlcheck
      
      pmlcheck = 1.0
      if (x.lt.xmin.or.x.gt.xmax
     &     .or.y.lt.ymin.or.y.gt.ymax
     &     .or.z.lt.zmin.or.z.gt.zmax) then
         pmlcheck = 0.0
      end if
      end function

      end subroutine INIT_idistr
