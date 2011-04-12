# AX_CHECK_HDF5
# --------------------------------------
# checks for HDF5 lib

AC_DEFUN([AX_CHECK_HDF5],
  [AC_ARG_WITH(
    [hdf5],
    [AS_HELP_STRING([--with-hdf5=[ARG]],[use hdf5 in directory ARG])]
    [AS_IF(["$withwal" != "yes"], [HDF5_DIR="$withval"])]
   )

dnl echo HDF5_DIR $HDF5_DIR

   AS_IF(
     [test "$HDF5_DIR" != "no"],
     [AS_IF([test -n "$HDF5_DIR"],
            [H5_CFLAGS="-I${HDF5_DIR}/include"
             H5_LIBS="-L${HDF5_DIR}/lib"])
      
      save_CPPFLAGS="$CPPFLAGS"
      CPPFLAGS="$H5_CFLAGS $CPPFLAGS"
      AC_CHECK_HEADERS([hdf5.h],)
      CPPFLAGS="$save_CPPFLAGS"
     ]
   )

dnl echo HAVE_HDF5_H $ac_cv_header_hdf5_h

   AS_IF([test "$ac_cv_header_hdf5_h" = "yes"],
   	 [save_LIBS="$LIBS"
          LIBS="$H5_LIBS $LIBS"
          AC_CHECK_LIB([hdf5], [H5Gcreate2])
	  LIBS="$save_LIBS"
	  LDFLAGS="$save_LDFLAGS"
         ])

dnl echo HAVE_LIBHDF5 $ac_cv_lib_hdf5_H5Gcreate2

   AS_IF([test "$ac_cv_lib_hdf5_H5Gcreate2" = "yes"],
   	 [H5_LIBS="$H5_LIBS -lhdf5"
	  save_LIBS="$LIBS"
          LIBS="$H5_LIBS $LIBS"
      	  AC_CHECK_LIB([hdf5_hl], [H5LTmake_dataset_float])
	  LIBS="$save_LIBS"
	  LDFLAGS="$save_LDFLAGS"
         ])

   AS_IF([test "$ac_cv_lib_hdf5_hl_H5LTmake_dataset_float" = "yes"],
   	 [H5_LIBS="$H5_LIBS -lhdf5_hl"
	  have_hdf5="yes"
         ],
	 [have_hdf5="no"])
])

