/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
   Contributing author: Taylor Barnes (MolSSI)
   MolSSI Driver Interface (MDI) support for LAMMPS
------------------------------------------------------------------------- */

#include "fix_driver.h"
#include "atom.h"
#include "domain.h"
#include "comm.h"
#include "update.h"
#include "force.h"
#include "error.h"
#include "group.h"
#include "memory.h"
#include "modify.h"
#include "compute.h"

#include <stdlib.h>
#include <string.h>
//
#include "irregular.h"
#include "min.h"
#include "minimize.h"
#include "modify.h"
#include "output.h"
#include "timer.h"
#include "verlet.h"
extern "C" {
#include "mdi.h"
}
#include <iostream>
using namespace std;
//

using namespace LAMMPS_NS;
using namespace FixConst;

/****************************************************************************/


/***************************************************************
 * create class and parse arguments in LAMMPS script. Syntax:
 * fix ID group-ID driver [couple <group-ID>]
 ***************************************************************/
FixMDI::FixMDI(LAMMPS *lmp, int narg, char **arg) :
  Fix(lmp, narg, arg),
  id_pe(NULL), pe(NULL)
{

  if (narg > 3)
    error->all(FLERR,"Illegal fix mdi command");

  // allocate arrays
  memory->create(add_force,3*atom->natoms,"mdi:add_force");
  for (int i=0; i< 3*atom->natoms; i++) {
    add_force[i] = 0.0;
  }

  // create a new compute pe style
  // id = fix-ID + pe, compute group = all

  master = (comm->me==0) ? 1 : 0;

  // create instance of the Irregular class
  irregular = new Irregular(lmp);

  most_recent_init = 0;
  exit_flag = false;
  local_exit_flag = false;
  target_command = new char[MDI_COMMAND_LENGTH+1];

  int n = strlen(id) + 4;
  id_pe = new char[n];
  strcpy(id_pe,id);
  strcat(id_pe,"_pe");

  char **newarg = new char*[3];
  newarg[0] = id_pe;
  newarg[1] = (char *) "all";
  newarg[2] = (char *) "pe";
  modify->add_compute(3,newarg);
  delete [] newarg;

  // accept a communicator to the driver
  int ierr;
  if (master) {
    driver_socket = MDI_Accept_Communicator();
    if (driver_socket <= 0)
      error->all(FLERR,"Unable to connect to driver");
  } else driver_socket=0;

}

/*********************************
 * Clean up on deleting the fix. *
 *********************************/
FixMDI::~FixMDI()
{
  modify->delete_compute(id_pe);
  delete irregular;
  delete [] id_pe;
  delete [] target_command;
}

/* ---------------------------------------------------------------------- */
int FixMDI::setmask()
{
  int mask = 0;

  // MD masks
  mask |= POST_INTEGRATE;
  mask |= POST_FORCE;
  mask |= END_OF_STEP;

  // Minimizer masks
  mask |= MIN_PRE_FORCE;
  mask |= MIN_PRE_REVERSE;
  mask |= MIN_POST_FORCE;

  return mask;
}

/* ---------------------------------------------------------------------- */

void FixMDI::exchange_forces()
{
  double **f = atom->f;
  const int * const mask  = atom->mask;
  const int nlocal = atom->nlocal;

  // add the forces from the driver
  for (int i=0; i < nlocal; ++i) {
    if (mask[i] & groupbit) {
      f[i][0] += add_force[3*(atom->tag[i]-1)+0];
      f[i][1] += add_force[3*(atom->tag[i]-1)+1];
      f[i][2] += add_force[3*(atom->tag[i]-1)+2];
    }
  }

}

/* ---------------------------------------------------------------------- */

void FixMDI::init()
{

  int icompute = modify->find_compute(id_pe);
  if (icompute < 0)
    error->all(FLERR,"Potential energy ID for fix mdi does not exist");
  pe = modify->compute[icompute];

  return;

}

/* ---------------------------------------------------------------------- */

void FixMDI::setup(int)
{
  //exchange_forces();

  //compute the potential energy
  potential_energy = pe->compute_scalar();

  // trigger potential energy computation on next timestep
  pe->addstep(update->ntimestep+1);
}

/* ---------------------------------------------------------------------- */

void FixMDI::post_integrate()
{
  cout << "@@@ In post_integrate" << endl;

  engine_mode(1);
}

/* ---------------------------------------------------------------------- */

void FixMDI::post_force(int vflag)
{
  cout << "@@@ In post_force" << endl;

  // calculate the energy
  potential_energy = pe->compute_scalar();

  //exchange_forces();
  engine_mode(2);

  // trigger potential energy computation on next timestep
  pe->addstep(update->ntimestep+1);
}


/* ---------------------------------------------------------------------- */

void FixMDI::min_pre_force(int vflag)
{
  cout << "@@@ In min_pre_force" << endl;

  engine_mode(1);
}


/* ---------------------------------------------------------------------- */

void FixMDI::min_pre_reverse(int vflag, int eflag)
{
  cout << "@@@ In min_pre_reverse" << endl;

  engine_mode(2);
}


/* ---------------------------------------------------------------------- */

void FixMDI::min_post_force(int vflag)
{
  cout << "@@@ In min_post_force" << endl;

  // calculate the energy
  //potential_energy = pe->compute_scalar();

  //exchange_forces();
  engine_mode(3);

  // trigger potential energy computation on next timestep
  //pe->addstep(update->ntimestep+1);
}

/* ---------------------------------------------------------------------- */

void FixMDI::end_of_step()
{
  cout << "$$$$ end_of_step" << endl;
  if ( most_recent_init == 1 ) { // md
    // when running md, the simulation only runs for a single iteration
    // after the iteration terminates, control will return to engine mode
    // set current_node so that engine_mode is using the correct node
    current_node = 3;
  }
  else if ( most_recent_init == 2 ) { // optg
    engine_mode(3);
  }
}

/* ---------------------------------------------------------------------- */

void FixMDI::engine_mode(int node)
{
  //master = (comm->me==0) ? 1 : 0;

  /*
  // open the socket
  int ierr;
  if (master) {
    driver_socket = MDI_Accept_Communicator();
    if (driver_socket <= 0)
      error->all(FLERR,"Unable to connect to driver");
  } else driver_socket=0;
  */

  // flag to indicate whether the engine should continue listening for commands at this node
  current_node = node;
  if ( target_node != 0 and target_node != current_node ) {
    local_exit_flag = true;
  }

  cout << "ENGINE_MODE: " << target_node << " " << current_node << " " << local_exit_flag << endl;

  /* ----------------------------------------------------------------- */
  // Answer commands from the driver
  /* ----------------------------------------------------------------- */
  char command[MDI_COMMAND_LENGTH+1];

  while (not exit_flag and not local_exit_flag) {

    if (master) { 
      // read the next command from the driver
      ierr = MDI_Recv_Command(command, driver_socket);
      if (ierr != 0)
        error->all(FLERR,"Unable to receive command from driver");
      command[MDI_COMMAND_LENGTH]=0;
    }
    // broadcast the command to the other tasks
    MPI_Bcast(command,MDI_COMMAND_LENGTH,MPI_CHAR,0,world);

    if (screen)
      fprintf(screen,"MDI command: %s\n",command);
    if (logfile)
      fprintf(logfile,"MDI command: %s:\n",command);

    if (strcmp(command,"STATUS      ") == 0 ) {
      // send the calculation status to the driver
      if (master) {
	ierr = MDI_Send_Command("READY", driver_socket);
        if (ierr != 0)
          error->all(FLERR,"Unable to return status to driver");
      }
    }
    else if (strcmp(command,">NATOMS") == 0 ) {
      // receive the number of atoms from the driver
      if (master) {
        ierr = MDI_Recv((char*) &atom->natoms, 1, MDI_INT, driver_socket);
        if (ierr != 0)
          error->all(FLERR,"Unable to receive number of atoms from driver");
      }
      MPI_Bcast(&atom->natoms,1,MPI_INT,0,world);
    }
    else if (strcmp(command,"<NATOMS") == 0 ) {
      // send the number of atoms to the driver
      if (master) {
        ierr = MDI_Send((char*) &atom->natoms, 1, MDI_INT, driver_socket);
        if (ierr != 0)
          error->all(FLERR,"Unable to send number of atoms to driver");
      }
    }
    else if (strcmp(command,"<NTYPES") == 0 ) {
      // send the number of atom types to the driver
      if (master) {
        ierr = MDI_Send((char*) &atom->ntypes, 1, MDI_INT, driver_socket);
        if (ierr != 0)
          error->all(FLERR,"Unable to send number of atom types to driver");
      }
    }
    else if (strcmp(command,"<TYPES") == 0 ) {
      // send the atom types
      send_types(error);
    }
    else if (strcmp(command,"<MASSES") == 0 ) {
      // send the atom types
      send_masses(error);
    }
    else if (strcmp(command,"<CELL") == 0 ) {
      // send the cell dimensions to the driver
      send_cell(error);
    }
    else if (strcmp(command,">COORDS") == 0 ) {
      // receive the coordinate information
      receive_coordinates(error);
    }
    else if (strcmp(command,"<COORDS") == 0 ) {
      // send the coordinate information
      send_coordinates(error);
    }
    else if (strcmp(command,"<CHARGES") == 0 ) {
      // send the charges
      send_charges(error);
    }
    else if (strcmp(command,"<ENERGY") == 0 ) {
      // send the potential energy to the driver
      send_energy(error);
    }
    else if (strcmp(command,"<FORCES") == 0 ) {
      // send the forces to the driver
      send_forces(error);
    }
    else if (strcmp(command,">FORCES") == 0 ) {
      // receive the forces from the driver
      receive_forces(error);
    }
    else if (strcmp(command,"+PRE-FORCES") == 0 ) {
      // receive additional forces from the driver
      // these are added prior to SHAKE or other post-processing
      add_forces(error);
    }
    else if (strcmp(command,"MD_INIT") == 0 ) {
      // initialize a new MD simulation
      md_init(error);
    }
    else if (strcmp(command,"OPTG_INIT") == 0 ) {
      // initialize a new geometry optimization
      optg_init(error);
    }
    else if (strcmp(command,"ATOM_STEP") == 0 ) {
      // perform an single iteration of MD or geometry optimization
      if ( current_node == -1 ) {
	// for the first iteration, md_setup calculates the forces
	md_setup(error);
      }
      target_node = 1;
      timestep(error);

      // It is possible for node commands, like @PRE-FORCES to request that the code cross from one
      // MD iteration to another.  In this case, the timestep function should be called again.
      while ( target_node != 0 and target_node != current_node and
	      most_recent_init == 1 and current_node == 3 and not exit_flag and not local_exit_flag ) {
	// start another MD iteration
	timestep(error);
      }
    }
    else if (strcmp(command,"@PRE-FORCES") == 0 ) {
      if ( current_node == -1 ) {
	// for the first iteration, md_setup calculates the forces
	md_setup(error);
	current_node = -2; // special case:
                           // tells @FORCES command not to move forward
      }
      else {
	target_node = 2;
	local_exit_flag = true;
      }
    }
    else if (strcmp(command,"@FORCES") == 0 ) {
      if ( most_recent_init == 1 and current_node == -1 ) {
	// for the first iteration, md_setup calculates the forces
	md_setup(error);
	current_node = 3;
      }
      else if ( current_node == -2 ) {
	// for the special case when MD_INIT is followed by @PREFORCES, which is followed by @FORCES
	current_node = 3;
      }
      else {
	target_node = 3;
	local_exit_flag = true;
      }
    }
    else if (strcmp(command,"EXIT") == 0 ) {
      // exit the driver code
      exit_flag = true;
    }
    else {
      // the command is not supported
      error->all(FLERR,"Unknown command from driver");
    }

    // check if the target node is something other than the current node
    if ( target_node != 0 and target_node != current_node ) {
      /*
      if ( most_recent_init == 1 and current_node == 3 ) { // program control has reached the outer engine_mode
	// start another MD iteration
	timestep(error);
      }
      else {
      */
	local_exit_flag = true;
	//}
    }

  }

  // a local exit has completed, so turn off the local exit flag
  cout << "ENGINE_MODE: EXITED " << target_node << " " << current_node << endl;
  local_exit_flag = false;

}


void FixMDI::receive_coordinates(Error* error)
{
  double posconv;
  posconv=force->angstrom/MDI_ANGSTROM_TO_BOHR;

  // create a buffer to hold the coordinates
  double *buffer;
  buffer = new double[3*atom->natoms];

  if (master) {
    ierr = MDI_Recv((char*) buffer, 3*atom->natoms, MDI_DOUBLE, driver_socket);
    if (ierr != 0)
      error->all(FLERR,"Unable to receive coordinates from driver");
  }
  MPI_Bcast(buffer,3*atom->natoms,MPI_DOUBLE,0,world);

  // pick local atoms from the buffer
  double **x = atom->x;
  int *mask = atom->mask;
  int nlocal = atom->nlocal;
  for (int i = 0; i < nlocal; i++) {
    x[i][0]=buffer[3*(atom->tag[i]-1)+0]*posconv;
    x[i][1]=buffer[3*(atom->tag[i]-1)+1]*posconv;
    x[i][2]=buffer[3*(atom->tag[i]-1)+2]*posconv;
  }

  // ensure atoms are in current box & update box via shrink-wrap
  // has to be be done before invoking Irregular::migrate_atoms() 
  //   since it requires atoms be inside simulation box
  if (domain->triclinic) domain->x2lamda(atom->nlocal);
  domain->pbc();
  domain->reset_box();
  if (domain->triclinic) domain->lamda2x(atom->nlocal);

  // move atoms to new processors via irregular()
  // only needed if migrate_check() says an atom moves to far
  if (domain->triclinic) domain->x2lamda(atom->nlocal);
  if (irregular->migrate_check()) irregular->migrate_atoms();
  if (domain->triclinic) domain->lamda2x(atom->nlocal);

  delete [] buffer;
}


void FixMDI::send_coordinates(Error* error)
{
  double posconv;
  posconv=force->angstrom/MDI_ANGSTROM_TO_BOHR;

  double *coords;
  double *coords_reduced;

  coords = new double[3*atom->natoms];
  coords_reduced = new double[3*atom->natoms];

  // pick local atoms from the buffer
  double **x = atom->x;
  int *mask = atom->mask;
  int nlocal = atom->nlocal;
  for (int i = 0; i < nlocal; i++) {
    coords[3*(atom->tag[i]-1)+0] = x[i][0]/posconv;
    coords[3*(atom->tag[i]-1)+1] = x[i][1]/posconv;
    coords[3*(atom->tag[i]-1)+2] = x[i][2]/posconv;
  }

  MPI_Reduce(coords, coords_reduced, 3*atom->natoms, MPI_DOUBLE, MPI_SUM, 0, world);

  if (master) {
    ierr = MDI_Send((char*) coords_reduced, 3*atom->natoms, MDI_DOUBLE, driver_socket);
    if (ierr != 0)
      error->all(FLERR,"Unable to send coordinates to driver");
  }

  delete [] coords;
  delete [] coords_reduced;
}


void FixMDI::send_charges(Error* error)
{
  double *charges;
  double *charges_reduced;

  charges = new double[atom->natoms];
  charges_reduced = new double[atom->natoms];

  // pick local atoms from the buffer
  double *charge = atom->q;
  int *mask = atom->mask;
  int nlocal = atom->nlocal;
  for (int i = 0; i < nlocal; i++) {
    charges[atom->tag[i]-1] = charge[i];
  }

  MPI_Reduce(charges, charges_reduced, atom->natoms, MPI_DOUBLE, MPI_SUM, 0, world);

  if (master) { 
    ierr = MDI_Send((char*) charges_reduced, atom->natoms, MDI_DOUBLE, driver_socket);
    if (ierr != 0)
      error->all(FLERR,"Unable to send charges to driver");
  }

  delete [] charges;
  delete [] charges_reduced;
}


void FixMDI::send_energy(Error* error)
{

  double pe;
  double *send_pe = &pe;

  // be certain that the MD simulation has been initialized
  /*
  if ( not md_initialized ) {
    error->all(FLERR,"Unable to compute energy");
  }
  */

  pe = potential_energy;

  // convert the energy to atomic units
  pe *= MDI_KELVIN_TO_HARTREE/force->boltz;

  if (master) {
    ierr = MDI_Send((char*) send_pe, 1, MDI_DOUBLE, driver_socket);
    if (ierr != 0)
      error->all(FLERR,"Unable to send potential energy to driver");
  }
}


void FixMDI::send_types(Error* error)
{
  int * const type = atom->type;

  if (master) { 
    ierr = MDI_Send((char*) type, atom->natoms, MDI_INT, driver_socket);
    if (ierr != 0)
      error->all(FLERR,"Unable to send atom types to driver");
  }
}


void FixMDI::send_masses(Error* error)
{
  double * const mass = atom->mass;

  if (master) { 
    ierr = MDI_Send((char*) mass, atom->ntypes+1, MDI_DOUBLE, driver_socket);
    if (ierr != 0)
      error->all(FLERR,"Unable to send atom masses to driver");
  }
}


void FixMDI::send_forces(Error* error)
{
  double potconv, posconv, forceconv;
  potconv=MDI_KELVIN_TO_HARTREE/force->boltz;
  posconv=force->angstrom/MDI_ANGSTROM_TO_BOHR;
  forceconv=potconv*posconv;

  double *forces;
  double *forces_reduced;
  double *x_buf;

  int *mask = atom->mask;
  int nlocal = atom->nlocal;

  //
  double **ftest = atom->f;
  cout << "$$$ Forces at start of send_forces: " << ftest[0][0] << endl;
  //

  forces = new double[3*atom->natoms];
  forces_reduced = new double[3*atom->natoms];
  x_buf = new double[3*atom->natoms];

  //certain fixes, such as shake, move the coordinates
  //to ensure that the coordinates do not change, store a copy
  double **x = atom->x;
  for (int i = 0; i < nlocal; i++) {
    x_buf[3*i+0] = x[i][0];
    x_buf[3*i+1] = x[i][1];
    x_buf[3*i+2] = x[i][2];
  }


  // calculate the forces
  update->whichflag = 1; // 1 for dynamics
  update->nsteps = 1;
  lmp->init();
  update->integrate->setup_minimal(1);

  // pick local atoms from the buffer
  double **f = atom->f;
  for (int i = 0; i < nlocal; i++) {
    forces[3*(atom->tag[i]-1)+0] = f[i][0]*forceconv;
    forces[3*(atom->tag[i]-1)+1] = f[i][1]*forceconv;
    forces[3*(atom->tag[i]-1)+2] = f[i][2]*forceconv;
  }

  MPI_Reduce(forces, forces_reduced, 3*atom->natoms, MPI_DOUBLE, MPI_SUM, 0, world);

  if (master) {
    ierr = MDI_Send((char*) forces_reduced, 3*atom->natoms, MDI_DOUBLE, driver_socket);
    if (ierr != 0)
      error->all(FLERR,"Unable to send atom forces to driver");
  }

  //
  cout << "$$$ Forces at end of send_forces: " << f[0][0] << endl;
  //


  //restore the original set of coordinates
  double **x_new = atom->x;
  for (int i = 0; i < nlocal; i++) {
    x_new[i][0] = x_buf[3*i+0];
    x_new[i][1] = x_buf[3*i+1];
    x_new[i][2] = x_buf[3*i+2];
  }

  delete [] forces;
  delete [] forces_reduced;
  delete [] x_buf;

}


void FixMDI::receive_forces(Error* error)
{
  double potconv, posconv, forceconv;
  potconv=MDI_KELVIN_TO_HARTREE/force->boltz;
  posconv=force->angstrom/MDI_ANGSTROM_TO_BOHR;
  forceconv=potconv*posconv;

  double *forces;
  forces = new double[3*atom->natoms];

  if (master) {
    ierr = MDI_Recv((char*) forces, 3*atom->natoms, MDI_DOUBLE, driver_socket);
    if (ierr != 0)
      error->all(FLERR,"Unable to receive atom forces to driver");
  }
  MPI_Bcast(forces,3*atom->natoms,MPI_DOUBLE,0,world);

  // pick local atoms from the buffer
  double **f = atom->f;
  int *mask = atom->mask;
  int nlocal = atom->nlocal;

  for (int i = 0; i < nlocal; i++) {
    f[i][0] = forces[3*(atom->tag[i]-1)+0]/forceconv;
    f[i][1] = forces[3*(atom->tag[i]-1)+1]/forceconv;
    f[i][2] = forces[3*(atom->tag[i]-1)+2]/forceconv;
  }

  delete [] forces;
}


void FixMDI::add_forces(Error* error)
{
  double potconv, posconv, forceconv;
  potconv=MDI_KELVIN_TO_HARTREE/force->boltz;
  posconv=force->angstrom/MDI_ANGSTROM_TO_BOHR;
  forceconv=potconv*posconv;

  double *forces;
  forces = new double[3*atom->natoms];

  if (master) {
    ierr = MDI_Recv((char*) forces, 3*atom->natoms, MDI_DOUBLE, driver_socket);
    if (ierr != 0)
      error->all(FLERR,"Unable to receive atom +forces to driver");
  }
  MPI_Bcast(forces,3*atom->natoms,MPI_DOUBLE,0,world);
  for (int i = 0; i < 3*atom->natoms; i++) {
    forces[i] /= forceconv;
  }

  for (int j = 0; j < 3*atom->natoms; j++) {
    add_force[j] = forces[j];
  }

  delete [] forces;
}


void FixMDI::send_cell(Error* error)
{
  double celldata[9];

  celldata[0] = domain->boxlo[0];
  celldata[1] = domain->boxlo[1];
  celldata[2] = domain->boxlo[2];
  celldata[3] = domain->boxhi[0];
  celldata[4] = domain->boxhi[1];
  celldata[5] = domain->boxhi[2];
  celldata[6] = domain->xy;
  celldata[7] = domain->xz;
  celldata[8] = domain->yz;

  if (master) { 
    ierr = MDI_Send((char*) celldata, 9, MDI_DOUBLE, driver_socket);
    if (ierr != 0)
      error->all(FLERR,"Unable to send cell dimensions to driver");
  }
}


void FixMDI::md_init(Error* error)
{
  if ( most_recent_init != 0 ) {
      error->all(FLERR,"Atomic propagation method already initialized");
  }

  // calculate the forces
  update->whichflag = 1; // 1 for dynamics
  timer->init_timeout();
  update->nsteps = 1;
  update->ntimestep = 0;
  update->firststep = update->ntimestep;
  update->laststep = update->ntimestep + update->nsteps;
  update->beginstep = update->firststep;
  update->endstep = update->laststep;
  lmp->init();
  ///////////
  current_node = -1; // after MD_INIT
  most_recent_init = 1;
  ///////////

  update->integrate->setup(1);
}


void FixMDI::md_setup(Error* error)
{
  update->integrate->setup(1);

  //
  double **f = atom->f;
  cout << "$$$ Forces at end of md_init: " << f[0][0] << endl;
  //
}


void FixMDI::timestep(Error* error)
{
  if ( most_recent_init == 1 ) {
    cout << "$$$ ATOM_STEP: " << current_node << " " << target_node << endl;
    if ( current_node == -2 or current_node == -1 or current_node == 3 ) {

      update->whichflag = 1; // 1 for dynamics
      timer->init_timeout();
      update->nsteps += 1;
      update->laststep += 1;
      update->endstep = update->laststep;
      output->next = update->ntimestep + 1;

      update->integrate->run(1);

    }
    else {
      local_exit_flag = true;
    }

  }
  else if ( most_recent_init == 2 ) {
    target_node = 1;
    local_exit_flag = true;
    //update->minimize->iterate(1);
  }
}


void FixMDI::optg_init(Error* error)
{
  if ( most_recent_init != 0 ) {
      error->all(FLERR,"Atomic propagation method already initialized");
  }

  // create instance of Minimizer class
  minimizer = new Minimize(lmp);

  int narg = 4;
  char* arg[] = {"1.0e-100","1.0e-100","10000000","10000000"};

  update->etol = force->numeric(FLERR,arg[0]);
  update->ftol = force->numeric(FLERR,arg[1]);
  update->nsteps = force->inumeric(FLERR,arg[2]);
  update->max_eval = force->inumeric(FLERR,arg[3]);

  update->whichflag = 2; // 2 for minimization
  update->beginstep = update->firststep = update->ntimestep;
  update->endstep = update->laststep = update->firststep + update->nsteps;
  lmp->init();
  update->minimize->setup();

  current_node = -1; // after OPTG_INIT
  most_recent_init = 2;

  update->minimize->iterate(10);
}

