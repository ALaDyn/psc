
#include <mpi.h>

extern "C" {

#include "ggcm_mhd_step_gkeyll_lua.h"

#include <ggcm_mhd_private.h>
#include <mrc_fld.h>
#include <mrc_domain.h>

}

#include <LcLogStream.h>
#include <LcLogger.h>
#include <LcLuaState.h>
#include <LcRegisterModules.h>
#include <LcStreamHandler.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <iostream>

// FIXME, should be part of the ggcm_mhd_step_gkeyll state

static Lucee::LuaState L;

static int ggcm_mhd_get_3d_fld_lua(lua_State *L) {
  int nr_comps = lua_tointeger(L, -1);
  struct ggcm_mhd *mhd = (struct ggcm_mhd *) lua_touserdata(L, -2);
  struct mrc_fld *fld = ggcm_mhd_get_3d_fld(mhd, nr_comps);
  mrc_fld_dict_add_int(fld, "mhd_type", MT_FULLY_CONSERVATIVE);
  lua_pushlightuserdata(L, fld);
  return 1;
}

static int ggcm_mhd_put_3d_fld_lua(lua_State *L) {
  struct mrc_fld *fld = (struct mrc_fld *) lua_touserdata(L, -1);
  struct ggcm_mhd *mhd = (struct ggcm_mhd *) lua_touserdata(L, -2);
  ggcm_mhd_put_3d_fld(mhd, fld);
  return 0;
}

static int mrc_fld_get_arr_lua(lua_State *L) {
  struct mrc_fld *fld = (struct mrc_fld *) lua_touserdata(L, -1);
  lua_pushlightuserdata(L, fld->_arr);
  return 1;
}

static int ggcm_mhd_fill_ghosts_lua (lua_State *L) {
  double bntim = lua_tonumber(L, -1);
  struct mrc_fld *fld = (struct mrc_fld *) lua_touserdata(L, -2);
  struct ggcm_mhd *mhd = (struct ggcm_mhd *) lua_touserdata(L, -3);
  ggcm_mhd_fill_ghosts (mhd, fld, 0, bntim); // starting from 0?
  return 0;
}

void
ggcm_mhd_step_gkeyll_setup_flds_lua(const char*script_common, int *nr_comps, int *nr_ghosts)
{
  std::string inpFile = script_common;

  Lucee::LuaState L_temp;
  Lucee::registerModules(L_temp);

  if (luaL_loadfile(L_temp, inpFile.c_str()) 
   || lua_pcall(L_temp, 0, 0, 0)) {
    std::cerr << "Error parsing file: " << inpFile << std::endl;
    std::string err(lua_tostring(L_temp, -1));
    lua_pop(L_temp, 1);
    std::cerr << err << std::endl;
    exit(1);
  }

  lua_getglobal(L_temp, "nr_comps");
  lua_getglobal(L_temp, "nr_ghosts");
  *nr_ghosts = (int)lua_tonumber(L_temp, -1);
  *nr_comps = (int)lua_tonumber(L_temp, -2);
}

void
ggcm_mhd_step_gkeyll_lua_setup(const char *script, struct ggcm_mhd *mhd,
			       struct mrc_fld *fld)
{
  // determine input file
  std::string inpFile = script;

  // create output prefix
  std::string outPrefix;
  // use input file name sans the .lua extension
  std::string snm = inpFile;
  unsigned trunc = inpFile.find_last_of(".", snm.size());
  if (trunc > 0)
    snm.erase(trunc, snm.size());
  outPrefix = snm;

  bool isRestarting = false;
  int rFrame = 0;

  // create top-level logger
  Lucee::Logger& logger = Lucee::Logger::create("lucee");
  logger.setLevel("debug");

  // create console logger
  Lucee::Logger& conLogger = Lucee::Logger::create("lucee.console");
  conLogger.setLevel("info");
  // create console stream
  Lucee::StreamHandler conStrm(std::cout);
  conStrm.attachToLogger("lucee.console");

  // load lua library: this must be done before loading input file
  Lucee::registerModules(L);

  // add command line options to the top-level module
  static const luaL_Reg topFuncs[] = { {NULL, NULL} };
  luaL_register(L, "Lucee", topFuncs);

  lua_pushstring(L, inpFile.c_str());
  lua_setfield(L, -2, "InputFile");

  lua_pushstring(L, outPrefix.c_str());
  lua_setfield(L, -2, "OutPrefix");

  lua_pushboolean(L, isRestarting);
  lua_setfield(L, -2, "IsRestarting");

  lua_pushnumber(L, rFrame);
  lua_setfield(L, -2, "RestartFrame");

  lua_pop(L, 1); // done adding command line stuff

  lua_pushcfunction(L, ggcm_mhd_get_3d_fld_lua);
  lua_setglobal(L, "ggcm_get_3d_fld");

  lua_pushcfunction(L, ggcm_mhd_put_3d_fld_lua);
  lua_setglobal(L, "ggcm_put_3d_fld");

  lua_pushcfunction(L, mrc_fld_get_arr_lua);
  lua_setglobal(L, "mrc_fld_get_arr");

  lua_pushcfunction(L, ggcm_mhd_fill_ghosts_lua);
  lua_setglobal(L, "ggcm_fill_ghosts");

  if (luaL_loadfile(L, inpFile.c_str())) {
    std::cerr << "Error loading file: " << inpFile << std::endl;
    std::string err(lua_tostring(L, -1));
    lua_pop(L, 1);
    std::cerr << err << std::endl;
    exit(1);
  }

  int nargs = 0;

  int gdims[3];
  mrc_domain_get_global_dims(mhd->domain, gdims);
  const int *ldims = mrc_fld_spatial_dims(fld);
  lua_pushinteger(L, ldims[0]);
  lua_pushinteger(L, ldims[1]);
  lua_pushinteger(L, ldims[2]);
  nargs += 3;

  struct mrc_crds *crds = mrc_domain_get_crds(mhd->domain);
  double dx[3];
  mrc_crds_get_dx_base(crds, dx);
  double ll[3], lh[3];
  ll[0] = MRC_DMCRDX(crds, 0, 0) - .5 * dx[0];
  ll[1] = MRC_DMCRDY(crds, 0, 0) - .5 * dx[1];
  ll[2] = MRC_DMCRDZ(crds, 0, 0) - .5 * dx[2];
  lh[0] = MRC_DMCRDX(crds, ldims[0]-1, 0) + .5 * dx[0];
  lh[1] = MRC_DMCRDY(crds, ldims[1]-1, 0) + .5 * dx[1];
  lh[2] = MRC_DMCRDZ(crds, ldims[2]-1, 0) + .5 * dx[2];
  lua_pushnumber(L, ll[0]);
  lua_pushnumber(L, ll[1]);
  lua_pushnumber(L, ll[2]);
  nargs += 3;
  lua_pushnumber(L, lh[0]);
  lua_pushnumber(L, lh[1]);
  lua_pushnumber(L, lh[2]);
  nargs += 3;

  double l[3], h[3];
  mrc_crds_get_param_double3(mrc_domain_get_crds(mhd->domain), "l", l);
  mrc_crds_get_param_double3(mrc_domain_get_crds(mhd->domain), "h", h);
  lua_pushnumber(L, l[0]);
  lua_pushnumber(L, l[1]);
  lua_pushnumber(L, l[2]);
  nargs += 3;
  lua_pushnumber(L, h[0]);
  lua_pushnumber(L, h[1]);
  lua_pushnumber(L, h[2]);
  nargs += 3;

  if (lua_pcall(L, nargs, 0, 0)) {
    std::cerr << "Error in input file: " << inpFile << std::endl;
    std::string err(lua_tostring(L, -1));
    lua_pop(L, 1);
    std::cerr << err << std::endl;
    exit(1);
  }
}

void
ggcm_mhd_step_gkeyll_lua_run(struct ggcm_mhd *mhd, struct mrc_fld *fld)
{
  lua_getglobal(L, "runTimeStep");
  lua_pushnumber(L, mhd->dt);
  lua_pushnumber(L, mhd->time);
  lua_pushinteger(L, mhd->istep);
  lua_pushlightuserdata(L, fld->_arr);
  lua_pushlightuserdata(L, mhd);

  if (lua_pcall(L, 5, 1, 0)) {
    std::cerr << "LUA Error:" << std::endl;
    std::string err(lua_tostring(L, -1));
    lua_pop(L, 1);
    std::cerr << err << std::endl;
    exit(1);
  }
  mhd->dt = lua_tonumber(L,-1);

  float myDt = mhd->dt;
  MPI_Allreduce(&myDt, &mhd->dt, 1, MPI_FLOAT, MPI_MIN, mrc_domain_comm(mhd->domain));

  lua_pop(L,1);
}

