-------------------------------
-- SCRIPT FOR INITIALIZATION --
-------------------------------

------------------------------------------
-- LOAD LOCAL DOMAIN PARAMS FROM LIBMRC --
------------------------------------------
mx, my, mz, lx, ly, lz, hx, hy, hz, lxg, lyg, lzg, hxg, hyg, hzg, cptr = ...

----------------------------------------
-- LOAD COMMON CODES SHARED WITH STEP --
----------------------------------------
showlog = false
showlocallog = true
dofile("common.lua")

----------------------------
-- I.C. FOR LIBMRC-GKEYLL --
----------------------------
grid = createGrid()
q = createFields(grid)
q:set(init)
q:copy_to_cptr(cptr)


