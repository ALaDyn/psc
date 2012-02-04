﻿/*
 *  XDMF/HDF5 photon output by Simon Jagoda
 *  Spread version, 1 pair of files per node and output step
 */



#include "psc_output_photons_private.h"
#include "psc_output_photons_xdmf.h"

#include "mrc_io.h"
#include "mpi.h"
#include "psc.h"
#include "mrc_domain.h"
#include "mrc_fld.h"
#include <mrc_params.h>
#include <assert.h>
#include <string.h>
#include "hdf5.h"
#include <stdlib.h>

#include <mrc_profile.h>

#define to_psc_output_photons_xdmf(out) ((struct psc_output_photons_xdmf *)((out)->obj.subctx))

static void create_hdf5 (struct psc_output_photons_xdmf *out_c);
static void create_xdmf (struct psc_output_photons_xdmf *out_c);
static void write_node_hdf5(struct psc_output_photons_xdmf *out_c, mphotons_t *photons);
static void write_node_xdmf(struct psc_output_photons_xdmf *out_c, mphotons_t *photons);
static void write_patch_xdmf(struct psc_output_photons_xdmf *out_c, int *partnumber, int *patch);
static void write_patch_hdf5(struct psc_output_photons_xdmf *out_c, photons_t *patchdata, int *patch);
static void close_xdmf(struct psc_output_photons_xdmf *out_c);
static void close_hdf5(struct psc_output_photons_xdmf *out_c);

//----------------------------------------------------------------------
// master xdmf connecting the node xdmfs both spatial and temporal
// rewrite each step in case foolish user prematurely kills simulation
static void create_temporal_master(struct psc_output_photons_xdmf *out_c) {
    char filename[30+strlen(out_c->data_dir)];
//	int mpi_size;
//	MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
	sprintf(filename,"%s/photons_master.xdmf",out_c->data_dir);
    FILE *master = fopen (filename,"w");
	fprintf(master,"<?xml version=\"1.0\" ?> \n");
	fprintf(master,"<!DOCTYPE Xdmf SYSTEM \"Xdmf.dtd\" []> \n");
	fprintf(master,"<Xdmf xmlns:xi='http://www.w3.org/2001/XInclude' Version='2.0'> \n");
	fprintf(master,"<Domain> \n"); 
	fprintf(master,"<Grid GridType=\"Collection\" CollectionType=\"Temporal\"> \n");
	fprintf(master,"<Time TimeType=\"List\"> \n");
	fprintf(master,"<DataItem Format=\"XML\" NumberType=\"Float\" Dimensions=\"%i\"> \n",out_c->times_written);
	for(int i=0; i<=ppsc->timestep; i++) {
		if(out_c->write_photons[i]) {
			fprintf(master,"%e \n",ppsc->dt*i);
		}
	}
	fprintf(master,"</DataItem> \n");
	fprintf(master,"</Time> \n"); 
	for(int time=0; time<=ppsc->timestep; time++) {
		if(out_c->write_photons[time]) {
			fprintf(master,"<xi:include href=\"./%s.t%06d.xdmf\" xpointer=\"xpointer(//Xdmf/Domain/Grid)\"/> \n",out_c->basename,time);
		}
	}//end temporal loop
	fprintf(master,"</Grid> \n");
	fprintf(master,"</Domain> \n");
	fprintf(master,"</Xdmf> \n");
	fclose(master);
}
static void create_spatial_master(struct psc_output_photons_xdmf *out_c, int *timestep) {
    
    char filename[30+strlen(out_c->data_dir)+strlen(out_c->basename)];
    sprintf(filename,"%s/%s.t%06d.xdmf",out_c->data_dir,out_c->basename,*timestep);
    FILE *master = fopen (filename,"w");
    
	fprintf(master,"<?xml version=\"1.0\" ?> \n");
	fprintf(master,"<!DOCTYPE Xdmf SYSTEM \"Xdmf.dtd\" []> \n");
	fprintf(master,"<Xdmf xmlns:xi='http://www.w3.org/2001/XInclude' Version='2.0'> \n");
	fprintf(master,"<Domain> \n"); 
	fprintf(master,"<Grid GridType=\"Collection\" CollectionType=\"Spatial\"> \n"); 
		//loop over nodes "node"
		for(int node=0; node<out_c->mpi_size; node++) {
			fprintf(master,"<xi:include href=\"./%s.t%06d_n%06d.xdmf\" xpointer=\"xpointer(//Xdmf/Domain/Grid)\"/> \n",out_c->basename,*timestep,node);
		}//end spatial loop			
	fprintf(master,"</Grid> \n");
	fprintf(master,"</Domain> \n");
	fprintf(master,"</Xdmf> \n");
	fclose(master);
}

//----------------------------------------------------------------------
// set some variables -> call once at t=0
static void initialize_output (struct psc_output_photons_xdmf *out_c) {
	out_c->typenames[0] = "x";
    out_c->typenames[1] = "y";
    out_c->typenames[2] = "z"; 
    out_c->typenames[3] = "px";
    out_c->typenames[4] = "py";	//names hardcoded for now
    out_c->typenames[5] = "pz";
    out_c->typenames[6] = "w";
    out_c->basename = "photons";
    MPI_Comm_size(MPI_COMM_WORLD, &(out_c->mpi_size));    
    out_c->write_photons=(bool*) malloc(ppsc->prm.nmax*sizeof(bool));
    out_c->times_written=0;
}

//----------------------------------------------------------------------
// create files for output of one specific timestep -> have to call these all the time
//filename=<dir><basename><node>.<timestep>.<type>
static void create_hdf5 (struct psc_output_photons_xdmf *out_c) {
    char charbuffer[30+strlen(out_c->data_dir)+strlen(out_c->basename)];   
    int node;     
    MPI_Comm_rank(MPI_COMM_WORLD, &node);	
    sprintf(out_c->hdf5filename,"%s.t%06d_n%06d.h5",out_c->basename,ppsc->timestep,node);
    sprintf(charbuffer,"%s/%s",out_c->data_dir,out_c->hdf5filename);
    out_c->hdf5 = H5Fcreate (charbuffer, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT); 
}
static void create_xdmf(struct psc_output_photons_xdmf *out_c) {
    char filename[30+strlen(out_c->data_dir)+strlen(out_c->basename)];
    int node;    
    MPI_Comm_rank(MPI_COMM_WORLD, &node);
    sprintf(filename,"%s/%s.t%06d_n%06d.xdmf",out_c->data_dir,out_c->basename,ppsc->timestep,node);
    out_c->xdmf = fopen (filename,"w");
	fprintf(out_c->xdmf,"<?xml version=\"1.0\" ?> \n");
	fprintf(out_c->xdmf,"<!DOCTYPE Xdmf SYSTEM \"Xdmf.dtd\" []> \n");
	fprintf(out_c->xdmf,"<Xdmf Version=\"2.0\"> \n");
	fprintf(out_c->xdmf,"<Domain> \n");  
//	fprintf(out_c->xdmf,"<Grid GridType=\"Collection\" CollectionType=\"Temporal\"> \n"); 

}

//---------------------------------------------------------------------
//write container and data for the whole node
static void write_node_xdmf(struct psc_output_photons_xdmf *out_c, mphotons_t *photons) {
	fprintf(out_c->xdmf,"<Grid GridType=\"Collection\" CollectionType=\"Spatial\"> \n");
//	fprintf(out_c->xdmf,"Time Value=\"%i\" /> \n",ppsc->timestep);
	for(int i=0; i<photons->nr_patches; i++) {
		photons_t *pp = &photons->p[i];
		write_patch_xdmf(out_c, &(pp->nr), &i);
	}
	fprintf(out_c->xdmf,"</Grid>"); 
}
static void write_node_hdf5(struct psc_output_photons_xdmf *out_c, mphotons_t *photons)
{    
  for (int p = 0; p < photons->nr_patches; p++) {	
    photons_t *pp = &photons->p[p];
    write_patch_hdf5(out_c, pp, &p);    
  } 
}


//----------------------------------------------------------------------
//write container and data for one patch
static void write_patch_xdmf(struct psc_output_photons_xdmf *out_c, int *partnumber, int *patch) {
	fprintf(out_c->xdmf,"<Grid GridType=\"Uniform\"> \n ");
	fprintf(out_c->xdmf,"<Topology TopologyType=\"3DCoRectMesh\" Dimensions=\"%i 1 1\" /> \n",*partnumber);
	fprintf(out_c->xdmf,"<Geometry GeometryType=\"Origin_DxDyDz\"> \n");
	fprintf(out_c->xdmf,"<DataItem Name=\"Origin\" Datatype=\"Float\" Dimensions=\"3\" Format=\"XML\"> \n");
	fprintf(out_c->xdmf,"0 0 0 \n");
	fprintf(out_c->xdmf,"</DataItem> \n");
	fprintf(out_c->xdmf,"<DataItem Name=\"DxDyDz\" Datatype=\"Float\" Dimensions=\"3\" Format=\"XML\"> \n");
	fprintf(out_c->xdmf,"1 1 1 \n");
	fprintf(out_c->xdmf,"</DataItem> \n");
	fprintf(out_c->xdmf,"</Geometry> \n");
	// loop over photon attributes begins here
	for(int offset=0; offset<7; offset++) {
		if(!out_c->write_variable[offset]) {continue;}	//skip this iteration if variable should not be written 
		fprintf(out_c->xdmf,"<Attribute Name=\"%s\" AttributeType=\"Scalar\" Center=\"Node\"> \n", out_c->typenames[offset]);
		fprintf(out_c->xdmf,"<DataItem ItemType=\"Uniform\" Dimensions=\"%i 1 1\" NumberType=\"Float\" Format=\"HDF\"> \n", *partnumber);
		fprintf(out_c->xdmf,"./%s:/%s.%06d \n",out_c->hdf5filename,out_c->typenames[offset],*patch);
		fprintf(out_c->xdmf,"</DataItem> \n");
		fprintf(out_c->xdmf,"</Attribute> \n");
	}
	fprintf(out_c->xdmf,"</Grid> \n"); 
}
static void write_patch_hdf5(struct psc_output_photons_xdmf *out_c, photons_t *patchdata, int *patch) {    
    hid_t       memspace, filespace, dataset, dcpl;                                         
    hsize_t     memdims[1] = {7*patchdata->nr}; 
    hsize_t	filedims[1] = {patchdata->nr};
    hsize_t	start[1],
                stride[1],
                count[1],
                block[1];                     
    char datasetname[80];


    //create dataspace for memory and file
    //these wont change for different photon properties
    memspace = H5Screate_simple (1, memdims, NULL);
    filespace = H5Screate_simple (1, filedims, NULL);
   
    //dataset creation property list (tm)
    //chunksize = everything there is
    dcpl = H5Pcreate (H5P_DATASET_CREATE);
    H5Pset_chunk (dcpl, 1, filedims);

    //constant hyperslab properties	
    stride[0] = 7;	//take each n-th element
    count[0] = patchdata->nr;	//number of repetitions: array has to be of size stride*count or larger
    block[0] = 1;	//best not to touch this
                
    //loop over all photon variables                           
    for(int offset=0; offset<7; offset++) {
    	if(!out_c->write_variable[offset]) {continue;}	//skip this iteration if variable should not be written                       
    	sprintf(datasetname,"%s.%06d",out_c->typenames[offset],*patch);   
     
    	//hyperslab offset: determines photon variable to be written
    	start[0] = offset;	
    
    	//set hyperslab
    	H5Sselect_hyperslab (memspace, H5S_SELECT_SET, start, stride, count, block);

    	//create dataset: have to do this once for every variable, since name changes
    	dataset = H5Dcreate2 (out_c->hdf5, datasetname, H5T_IEEE_F64LE, filespace, H5P_DEFAULT, dcpl, H5P_DEFAULT);
    
    	//write data to dataset
    	H5Dwrite (dataset, H5T_IEEE_F64LE, memspace, filespace, H5P_DEFAULT, patchdata->photons);

    	//close dataset, so the next iteration can create it anew with another name
    	H5Dclose (dataset);
    }    
    //close dataspaces   
    H5Sclose (memspace);	//TODO: maybe leave them open and pass them around ?
    H5Sclose (filespace); 
}



//----------------------------------------------------------------------
//close files
static void close_xdmf(struct psc_output_photons_xdmf *out_c) {
//	fprintf(out_c->xdmf,"</Grid> \n"); //close temporal grid
	fprintf(out_c->xdmf,"</Domain> \n");
	fprintf(out_c->xdmf,"</Xdmf> \n");
	fclose(out_c->xdmf); 
}
static void close_hdf5(struct psc_output_photons_xdmf *out_c) {
    H5Fclose (out_c->hdf5);
}

// ---------------------------------------------------------------------
// this is what the psc actually calls each step
static void
psc_output_photons_xdmf_run(struct psc_output_photons *out, mphotons_t *photons) {
	static int pr;
  	if (!pr) {
  		pr = prof_register("c_out_part", 1., 0, 0);
  	}
  	prof_start(pr);  
  	struct psc *psc = ppsc;
  	struct psc_output_photons_xdmf *out_c = to_psc_output_photons_xdmf(out);
  	
  	//ensure expected functionality:
  	if(psc->timestep==0) {
    	for(int time=out_c->first; time<ppsc->prm.nmax; time+=out_c->step) {
    		out_c->write_photons[time]=true;
    	}
  	}

  	if (out_c->write_photons[psc->timestep]) {
  		out_c->times_written++;
    	create_hdf5(out_c);
    	create_xdmf(out_c);
    	write_node_hdf5(out_c,photons);	 
    	write_node_xdmf(out_c,photons);
    	close_hdf5(out_c);
    	close_xdmf(out_c);
    	int rank=42;
    	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    	if(rank==0){
    		create_spatial_master(out_c, &ppsc->timestep);
    		create_temporal_master(out_c);
    	}
  	}

	prof_stop(pr); 
}



//----------------------------------------------------------------------
// every function below this point is legacy stuff from nils' original C output.
// I see no reason to touch any of this, it looks dangerous


void
psc_output_photons_xdmf_setfilter(struct psc_output_photons *out, bool (*new_filter_func)(photon_t *part))
{
  struct psc_output_photons_xdmf *out_c = to_psc_output_photons_xdmf(out);
  out_c->filter_func=new_filter_func;
}

static void
psc_output_photons_xdmf_create(struct psc_output_photons *out)
{
  struct psc_output_photons_xdmf *out_c = to_psc_output_photons_xdmf(out);
  out_c->io = mrc_io_create(psc_output_photons_comm(out));
  //I'd like to have the outdir from mrc_io as the default value of data_dir
  mrc_io_set_from_options(out_c->io);
  const char *outdir=NULL;
  mrc_obj_get_param_string((struct mrc_obj*)out_c->io, "outdir" ,&outdir);
  psc_output_photons_set_param_string(out, "data_dir" ,outdir);
  initialize_output(out_c);
}

static void
psc_output_photons_xdmf_setup(struct psc_output_photons *out)
{
  struct psc_output_photons_xdmf *out_c = to_psc_output_photons_xdmf(out);

  const char *outdir=NULL;
  mrc_obj_get_param_string((struct mrc_obj*)out, "data_dir" ,&outdir);

  mrc_io_set_name(out_c->io, "photons_out");
  mrc_io_set_param_string(out_c->io, "basename", "photons");
  mrc_io_set_param_string(out_c->io, "outdir",outdir);
  mrc_io_setup(out_c->io);
  mrc_io_view(out_c->io);
}

static void
psc_output_photons_xdmf_destroy(struct psc_output_photons *out)
{
  struct psc_output_photons_xdmf *out_c = to_psc_output_photons_xdmf(out);
  mrc_io_destroy(out_c->io);
}

static void
psc_output_photons_xdmf_set_from_options(struct psc_output_photons *out)
{
  struct psc_output_photons_xdmf *out_c = to_psc_output_photons_xdmf(out);
  mrc_io_set_from_options(out_c->io);
}

#define VAR(x) (void *)offsetof(struct psc_output_photons_xdmf, x)

static struct param psc_output_photons_xdmf_descr[] = {
  { "data_dir"           , VAR(data_dir)             , PARAM_STRING(".")       },//This default value will NOT be used. The defaul value of the parameter outdir of mrc_io will be used instead.
  { "filter_function"   , VAR(filter_func) , PARAM_PTR(NULL) },
  { "photon_first"       , VAR(first)         , PARAM_INT(0)            },
  { "photon_step"        , VAR(step)          , PARAM_INT(10)           },
  { "write_x"        , VAR(write_variable[0])          , PARAM_BOOL(true)           },
  { "write_y"        , VAR(write_variable[1])          , PARAM_BOOL(true)           },
  { "write_z"        , VAR(write_variable[2])          , PARAM_BOOL(true)           },
  { "write_px"       , VAR(write_variable[3])          , PARAM_BOOL(true)           },
  { "write_py"       , VAR(write_variable[4])          , PARAM_BOOL(true)           },
  { "write_pz"       , VAR(write_variable[5])          , PARAM_BOOL(true)           },
  { "write_weight"   , VAR(write_variable[6])          , PARAM_BOOL(true)           },
  {},
};
#undef VAR


// ======================================================================
// psc_output_photons: subclass "c"

struct psc_output_photons_ops psc_output_photons_xdmf_spread_ops = {
  .name                  = "xdmf_spread",
  .size                  = sizeof(struct psc_output_photons_xdmf),
  .param_descr           = psc_output_photons_xdmf_descr,
  .create                = psc_output_photons_xdmf_create,
  .setup                 = psc_output_photons_xdmf_setup,
  .set_from_options      = psc_output_photons_xdmf_set_from_options,
  .destroy               = psc_output_photons_xdmf_destroy,
//  .write                 = psc_output_photons_c_write,
//  .read                  = psc_output_photons_c_read,
//  .view                  = psc_output_photons_c_view,
  .run                   = psc_output_photons_xdmf_run,
};
