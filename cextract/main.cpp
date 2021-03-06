/* Copyright (c) 2009, Simeon Bird <spb41@cam.ac.uk>
 *               Based on code (c) 2005 by J. Bolton
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
//For getopt
#include <unistd.h>
//For strerror
#include <errno.h>
#include <cassert>
#include <string.h>
#include "global_vars.h"
#include "absorption.h"
#include "part_int.h"
#include <string>
#include <sstream>
#include <iostream>
/* Atomic data for hydrogen (from VPFIT) */
#define  LAMBDA_LYA_H1 1215.6701e-8
#define  LAMBDA_LYA_HE2 303.7822e-8
#define  FOSC_LYA 0.416400
#define  HMASS 1.00794   /* Hydrogen mass in a.m.u. */
#define  HEMASS 4.002602 /* Helium-4 mass in a.m.u. */
#define  GAMMA_LYA_H1 6.265e8
#define  GAMMA_LYA_HE2 6.27e8

//Star formation threshold
#define PHYSDENSTHRESH 0.12948869200298072
//The gadget mass unit is 1e10 M_sun/h in g/h
#define  GADGET_MASS 1.98892e43
//The gadget length unit is 1 kpc/h in cm/h
#define  GADGET_LENGTH 3.085678e21

/* Model parameters outwith header */
#define XH 0.76  /* hydrogen fraction by mass */

#define NBINS 1024 /* number of pixels */

#ifndef NOHDF5
#include <hdf5.h>

std::string find_first_hdf_file(const std::string& infname)
{
  /*Switch off error handling so that we can check whether a
   * file is HDF5 */
  /* Save old error handler */
  hid_t error_stack=0;
  herr_t (*old_func)(hid_t, void*);
  void *old_client_data;
  H5Eget_auto(error_stack, &old_func, &old_client_data);
  /* Turn off error handling */
  H5Eset_auto(error_stack, NULL, NULL);
  std::string fname = infname;

  /*Were we handed an HDF5 file?*/
  if(H5Fis_hdf5(fname.c_str()) <= 0){
     /*If we weren't, were we handed an HDF5 file without the suffix?*/
     fname = infname+std::string(".0.hdf5");
     if (H5Fis_hdf5(fname.c_str()) <= 0)
        fname = std::string();
  }

  /* Restore previous error handler */
  H5Eset_auto(error_stack, old_func, old_client_data);
  return fname;
}
#endif

/*Open a file for reading to check it exists*/
int file_readable(const char * filename)
{
     FILE * file;
     if ((file = fopen(filename, "r"))){
          fclose(file);
          return 1;
     }
     return 0;
}


int main(int argc, char **argv)
{
  int64_t Npart=0;
  int NumLos=0;

  FILE *output;
  double *cofm=NULL;
  int *axis=NULL;
  char *ext_table=NULL;
  std::string outname;
  std::string indir;
  char c;
  int pad[32]={0};
  double  atime, redshift, Hz, box100, h100;
  struct particle_data P;
  double * tau_H1=NULL, * colden_H1=NULL;
  /*Make sure stdout is line buffered even when not 
   * printing to a terminal but, eg, perl*/
  setlinebuf(stdout);
  while((c = getopt(argc, argv, "o:i:t:n:h")) !=-1)
  {
    switch(c)
      {
        case 'o':
           outname=std::string(optarg)+std::string("_spectra.dat");
           break;
        case 'i':
           indir=std::string(optarg);
           break;
        case 'n':
           NumLos=atoi(optarg);
           break;
        case 't':
           ext_table=optarg;
           break;
        case 'h':
        case '?':
           help();
        default:
           exit(1);
      }
  }
  if(NumLos <=0) {
          std::cerr<<"Need NUMLOS >0"<<std::endl;
          help();
          exit(99);
  }
  if(indir.empty()) {
         std::cerr<<"Specify input ("<<indir<<") directory."<<std::endl;
         help();
         exit(99);
  }
  axis=(int *) malloc(NumLos*sizeof(int));
  cofm=(double *) malloc(3*NumLos*sizeof(double));
  if(!axis || !cofm){
          std::cerr<<"Error allocating memory for sightline table"<<std::endl;
          exit(2);
  }
  if(!(tau_H1 = (double *) calloc((NumLos * NBINS) , sizeof(double))) ||
      !(colden_H1 = (double *) calloc((NumLos * NBINS) , sizeof(double)))
     ){
        fprintf(stderr, "Error allocating memory for tau\n");
        exit(2);
  }
  unsigned i_fileno=0;
#ifndef NOHDF5
    /*ffname is a copy of input filename for extension*/
    /*First open first file to get header properties*/
    std::string fname = find_first_hdf_file(indir);
    std::string ffname = fname;
    int fileno=0;
    if ( !fname.empty() && load_hdf5_header(fname.c_str(), &atime, &redshift, &Hz, &box100, &h100) == 0 ){
            /*See if we have been handed the first file of a set:
             * our method for dealing with this closely mirrors
             * HDF5s family mode, but we cannot use this, because
             * our files may not all be the same size.*/
	    i_fileno = fname.find(".0.hdf5")+1;
    }
#ifndef NOGREAD
    else
#endif
#endif
#ifndef NOGREAD
    /*If not an HDF5 file, try opening as a gadget file*/
     if(load_header(indir.c_str(),&atime, &redshift, &Hz, &box100, &h100) < 0){
                std::cerr<<"No data loaded\n";
                exit(2);
    }
#endif
    /*Setup the los tables*/
    populate_los_table(cofm, axis,NumLos, ext_table, box100);
    /*Setup the interpolator*/
    const double velfac = atime/h100*Hz/1e3;
    ParticleInterp pint(NBINS, LAMBDA_LYA_H1, GAMMA_LYA_H1, FOSC_LYA, HMASS, box100, velfac, atime, cofm, axis,NumLos,1, 1e-5);
  if(!(output=fopen(outname.c_str(),"w")))
  {
          fprintf(stderr, "Error opening %s: %s\n",outname.c_str(), strerror(errno));
          exit(1);
  }
        /*Loop over files. Keep going until we run out, skipping over broken files.
         * The call to file_readable is an easy way to shut up HDF5's error message.*/
          /* P is allocated inside load_snapshot*/
  do{
          if(i_fileno){
#ifndef NOHDF5
            fileno++;
            if(i_fileno != std::string::npos){
		        std::ostringstream convert;
		        convert<<fileno;
                ffname = fname.replace(i_fileno, 1, convert.str());
		    }
            else
             break;
            /*If we ran out of files, we're done*/
            if(!(file_readable(ffname.c_str()) && H5Fis_hdf5(ffname.c_str()) > 0))
                    break;
              Npart=load_hdf5_snapshot(ffname.c_str(), &P,fileno);
#endif
          }
          else{
#ifndef NOGREAD
              Npart=load_snapshot(indir.c_str(), 0,&P);
              if (! Npart){
                  std::cerr<<"Could not read particles from snapshot"<<std::endl;
                  exit(2);
              }
#endif
          }
           /*Find mass fraction of neutral hydrogen*/
           /*Note P.Mass actually contains density, currently in gadget internal units.*/
           const double rscale = GADGET_LENGTH*atime/h100;
           /*Converts density to amu/cm^3*/
           const double dscale = GADGET_MASS/pow(GADGET_LENGTH,3)*h100*h100/(HMASS*PROTONMASS)/pow(atime,3);
           for(int ii = 0; ii< Npart; ii++){
             P.Mass[ii] *= dscale*XH;
             /*If we are above the star formation threshold, assume gas is fully neutral*/
             if(P.Mass[ii] <= PHYSDENSTHRESH)
                P.Mass[ii] *= P.fraction[ii];
             P.Mass[ii] *= rscale;
             P.temp[ii] = compute_temp(P.U[ii], P.Ne[ii], XH);
           }
          /*Do the hard SPH interpolation*/
          pint.compute_tau(tau_H1,P.Pos, P.Vel, P.Mass, P.temp, P.h, Npart);
          pint.compute_colden(colden_H1,P.Pos, P.Mass, P.h, Npart);

          /*Free the particle list once we don't need it*/
          free_parts(&P);
  } while(i_fileno > 0);
  printf("%lg, %lg\n", colden_H1[0], colden_H1[NumLos*NBINS-1]);
  printf("%lg, %lg\n", tau_H1[0], tau_H1[NumLos*NBINS-1]);
  free(cofm);
  free(axis);
  printf("Done interpolating, now calculating absorption\n");
  fwrite(&redshift,sizeof(double),1,output);
  /*Write a bit of a header. */
  int i=NBINS;
  fwrite(&box100,sizeof(double),1,output);
  fwrite(&i,sizeof(int),1,output);
  fwrite(&NumLos,sizeof(int),1,output);
  /*Write some space for future header data: total header size is
   * 128 bytes, with 24 full.*/
  fwrite(&pad,sizeof(int),32-6,output);
  fwrite(tau_H1,sizeof(double),NBINS*NumLos,output);    /* HI optical depth */
  fwrite(colden_H1,sizeof(double),NBINS*NumLos,output);    /* HI optical depth */
  fclose(output);
  /*Free other memory*/
  free(tau_H1);
  free(colden_H1);
  return 0;
}
/**********************************************************************/

void help()
{
           fprintf(stderr, "Usage: ./extract -n NUMLOS -i filename (ie, without the .0) -o output_file (_flux_power.txt or _spectra.dat will be appended)\n"
                  "-t table_file will read line of sight coordinates from a table.\n");
           return;
}

