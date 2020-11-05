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

#include <mpi.h>
#include <string.h>
#include <limits>
#include "driver.h"
#include "atom.h"
#include "domain.h"
#include "update.h"
#include "force.h"
#include "finish.h"
#include "min.h"
#include "minimize.h"
#include "modify.h"
#include "output.h"
#include "timer.h"
#include "error.h"
#include "comm.h"
#include "fix_driver.h"
#include "mdi.h"

#include "verlet.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

CommandMDI::CommandMDI(LAMMPS *lmp) : Pointers(lmp) {
  return;
}

CommandMDI::~CommandMDI() {
  return;
}

/* ---------------------------------------------------------------------- */

void CommandMDI::command(int narg, char **arg)
{

  // register the default node
  MDI_Register_Node("@DEFAULT");
  MDI_Register_Command("@DEFAULT", "<@");
  MDI_Register_Command("@DEFAULT", "<CELL");
  MDI_Register_Command("@DEFAULT", "<CHARGES");
  MDI_Register_Command("@DEFAULT", "<COORDS");
  MDI_Register_Command("@DEFAULT", "<LABELS");
  MDI_Register_Command("@DEFAULT", "<NATOMS");
  MDI_Register_Command("@DEFAULT", "<MASSES");
  MDI_Register_Command("@DEFAULT", ">COORDS");
  MDI_Register_Command("@DEFAULT", "@INIT_MD");
  MDI_Register_Command("@DEFAULT", "@INIT_OPTG");
  MDI_Register_Command("@DEFAULT", "EXIT");

  // register the MD initialization node
  MDI_Register_Node("@INIT_MD");
  MDI_Register_Command("@INIT_MD", "<@");
  MDI_Register_Command("@INIT_MD", "<CELL");
  MDI_Register_Command("@INIT_MD", "<CHARGES");
  MDI_Register_Command("@INIT_MD", "<COORDS");
  MDI_Register_Command("@INIT_MD", "<ENERGY");
  MDI_Register_Command("@INIT_MD", "<FORCES");
  MDI_Register_Command("@INIT_MD", "<KE");
  MDI_Register_Command("@INIT_MD", "<LABELS");
  MDI_Register_Command("@INIT_MD", "<MASSES");
  MDI_Register_Command("@INIT_MD", "<NATOMS");
  MDI_Register_Command("@INIT_MD", "<PE");
  MDI_Register_Command("@INIT_MD", ">COORDS");
  MDI_Register_Command("@INIT_MD", ">FORCES");
  MDI_Register_Command("@INIT_MD", "@");
  MDI_Register_Command("@INIT_MD", "@COORDS");
  MDI_Register_Command("@INIT_MD", "@FORCES");
  MDI_Register_Command("@INIT_MD", "@PRE-FORCES");
  MDI_Register_Command("@INIT_MD", "EXIT");

  // register the OPTG initialization node
  MDI_Register_Node("@INIT_OPTG");
  MDI_Register_Command("@INIT_OPTG", "<@");
  MDI_Register_Command("@INIT_OPTG", "<CELL");
  MDI_Register_Command("@INIT_OPTG", "<CHARGES");
  MDI_Register_Command("@INIT_OPTG", "<COORDS");
  MDI_Register_Command("@INIT_OPTG", "<ENERGY");
  MDI_Register_Command("@INIT_OPTG", "<FORCES");
  MDI_Register_Command("@INIT_OPTG", "<KE");
  MDI_Register_Command("@INIT_OPTG", "<LABELS");
  MDI_Register_Command("@INIT_OPTG", "<MASSES");
  MDI_Register_Command("@INIT_OPTG", "<NATOMS");
  MDI_Register_Command("@INIT_OPTG", "<PE");
  MDI_Register_Command("@INIT_OPTG", ">COORDS");
  MDI_Register_Command("@INIT_OPTG", ">FORCES");
  MDI_Register_Command("@INIT_OPTG", "@");
  MDI_Register_Command("@INIT_OPTG", "@COORDS");
  MDI_Register_Command("@INIT_OPTG", "@FORCES");
  MDI_Register_Command("@INIT_OPTG", "EXIT");

  // register the pre-forces node
  MDI_Register_Node("@PRE-FORCES");
  MDI_Register_Command("@PRE-FORCES", "<@");
  MDI_Register_Command("@PRE-FORCES", "<CELL");
  MDI_Register_Command("@PRE-FORCES", "<CHARGES");
  MDI_Register_Command("@PRE-FORCES", "<COORDS");
  MDI_Register_Command("@PRE-FORCES", "<ENERGY");
  MDI_Register_Command("@PRE-FORCES", "<FORCES");
  MDI_Register_Command("@PRE-FORCES", "<KE");
  MDI_Register_Command("@PRE-FORCES", "<LABELS");
  MDI_Register_Command("@PRE-FORCES", "<MASSES");
  MDI_Register_Command("@PRE-FORCES", "<NATOMS");
  MDI_Register_Command("@PRE-FORCES", "<PE");
  MDI_Register_Command("@PRE-FORCES", ">COORDS");
  MDI_Register_Command("@PRE-FORCES", ">FORCES");
  MDI_Register_Command("@PRE-FORCES", "@");
  MDI_Register_Command("@PRE-FORCES", "@COORDS");
  MDI_Register_Command("@PRE-FORCES", "@FORCES");
  MDI_Register_Command("@PRE-FORCES", "@PRE-FORCES");
  MDI_Register_Command("@PRE-FORCES", "EXIT");

  // register the forces node
  MDI_Register_Node("@FORCES");
  MDI_Register_Callback("@FORCES", ">FORCES");
  MDI_Register_Command("@FORCES", "<@");
  MDI_Register_Command("@FORCES", "<CELL");
  MDI_Register_Command("@FORCES", "<CHARGES");
  MDI_Register_Command("@FORCES", "<COORDS");
  MDI_Register_Command("@FORCES", "<ENERGY");
  MDI_Register_Command("@FORCES", "<FORCES");
  MDI_Register_Command("@FORCES", "<KE");
  MDI_Register_Command("@FORCES", "<LABELS");
  MDI_Register_Command("@FORCES", "<MASSES");
  MDI_Register_Command("@FORCES", "<NATOMS");
  MDI_Register_Command("@FORCES", "<PE");
  MDI_Register_Command("@FORCES", ">COORDS");
  MDI_Register_Command("@FORCES", ">FORCES");
  MDI_Register_Command("@FORCES", "@");
  MDI_Register_Command("@FORCES", "@COORDS");
  MDI_Register_Command("@FORCES", "@FORCES");
  MDI_Register_Command("@FORCES", "@PRE-FORCES");
  MDI_Register_Command("@FORCES", "EXIT");

  // register the coordinates node
  MDI_Register_Node("@COORDS");
  MDI_Register_Command("@COORDS", "<@");
  MDI_Register_Command("@COORDS", "<CELL");
  MDI_Register_Command("@COORDS", "<CHARGES");
  MDI_Register_Command("@COORDS", "<COORDS");
  MDI_Register_Command("@COORDS", "<ENERGY");
  MDI_Register_Command("@COORDS", "<FORCES");
  MDI_Register_Command("@COORDS", "<KE");
  MDI_Register_Command("@COORDS", "<LABELS");
  MDI_Register_Command("@COORDS", "<MASSES");
  MDI_Register_Command("@COORDS", "<NATOMS");
  MDI_Register_Command("@COORDS", "<PE");
  MDI_Register_Command("@COORDS", ">COORDS");
  MDI_Register_Command("@COORDS", ">FORCES");
  MDI_Register_Command("@COORDS", "@");
  MDI_Register_Command("@COORDS", "@COORDS");
  MDI_Register_Command("@COORDS", "@FORCES");
  MDI_Register_Command("@COORDS", "@PRE-FORCES");
  MDI_Register_Command("@COORDS", "EXIT");


  // identify the driver fix
  for (int i = 0; i < modify->nfix; i++) {
    if (strcmp(modify->fix[i]->style,"mdi") == 0) {
      mdi_fix = static_cast<FixMDI*>(modify->fix[i]);
    }
  }

  /* format for MDI command:
   * mdi
   */
  if (narg > 0) error->all(FLERR,"Illegal MDI command");

  if (atom->tag_enable == 0)
    error->all(FLERR,"Cannot use MDI command without atom IDs");

  if (atom->tag_consecutive() == 0)
    error->all(FLERR,"MDI command requires consecutive atom IDs");

  // begin engine_mode
  char *command = NULL;
  while ( true ) {
    command = mdi_fix->engine_mode("@DEFAULT");

    if (strcmp(command,"@INIT_MD") == 0 ) {
      // enter MDI simulation control loop
      int received_exit = mdi_md();
      if ( received_exit == 1 ) {
	return;
      }
    }
    if (strcmp(command,"@INIT_OPTG") == 0 ) {
      // enter MDI simulation control loop
      int received_exit = mdi_optg();
      if ( received_exit == 1 ) {
	return;
      }
    }
    else if (strcmp(command,"EXIT") == 0 ) {
      return;
    }
    else {
      error->all(FLERR,strcat("MDI received unsupported command: ",command));
    }
  }

  // flush the final output
  Finish finish(lmp);
  finish.end(0);

  return;

}



int CommandMDI::mdi_md()
{
  // initialize an MD simulation
  update->whichflag = 1; // 1 for dynamics
  timer->init_timeout();
  update->nsteps = 1;
  update->ntimestep = 0;
  update->firststep = update->ntimestep;
  update->laststep = update->ntimestep + update->nsteps;
  update->beginstep = update->firststep;
  update->endstep = update->laststep;
  lmp->init();

  // the MD simulation is now at the @INIT_MD node
  char *command = NULL;
  command = mdi_fix->engine_mode("@INIT_MD");

  // only two commands are valid at this point
  // SHOULD PROBABLY HAVE:
  // if ( command.scope < "GLOBAL" ) {
  //    pass
  // }
  if (strcmp(command,"MD_EXIT") == 0 ) {
    // return, but do not flag for global exit
    return 0;
  }
  else if (strcmp(command,"EXIT") == 0 ) {
    // return, and flag for global exit
    return 1;
  }
  // Don't include the error until after the command.scope check is added above
  /*
  else {
    error->all(FLERR,strcat("MDI received unsupported command: ",command));
  }
  */

  // NOTE: CURRENTLY TRUSTING THAT command = @FORCES

  // continue the MD simulation
  update->integrate->setup(1);

  //<<<<
  double **f2 = atom->f;
  fprintf(screen,"SSS MDI2: %f %f %f\n",f2[0][0], f2[0][1], f2[0][2]);
  //>>>>

  // the MD simulation is now at the @FORCES node
  command = mdi_fix->engine_mode("@FORCES");
  //command = mdi_fix->engine_mode(2);
  command = mdi_fix->command;

  // only two commands are valid at this point
  // SHOULD PROBABLY HAVE:
  // if ( command.scope < "GLOBAL" ) {
  //    pass
  // }
  if (strcmp(command,"EXIT_SIM") == 0 ) {
    // return, but do not flag for global exit
    return 0;
  }
  else if (strcmp(command,"EXIT") == 0 ) {
    // return, and flag for global exit
    return 1;
  }
  // Don't include the error until after the command.scope check is added above
  /*
  else {
    error->all(FLERR,strcat("MDI received unsupported command: ",command));
  }
  */

  // do MD iterations until told to exit
  while ( true ) {

    // run an MD timestep
    update->whichflag = 1; // 1 for dynamics
    timer->init_timeout();
    update->nsteps += 1;
    update->laststep += 1;
    update->endstep = update->laststep;
    output->next = update->ntimestep + 1;
    update->integrate->run(1);

    // get the most recent command the MDI engine received
    command = mdi_fix->command;

    // only two commands are valid at this point
    // SHOULD PROBABLY HAVE:
    // if ( command.scope < "GLOBAL" ) {
    //    pass
    // }
    if (strcmp(command,"EXIT_SIM") == 0 ) {
      // return, but do not flag for global exit
      return 0;
    }
    else if (strcmp(command,"EXIT") == 0 ) {
      // return, and flag for global exit
      return 1;
    }
    // Don't include the error until after the command.scope check is added above
    /*
    else {
      error->all(FLERR,strcat("MDI received unsupported command: ",command));
    }
    */

  }

}



int CommandMDI::mdi_optg()
{
  char *command = NULL;

  // create instance of the Minimizer class
  Minimize *minimizer = new Minimize(lmp);

  // initialize the minimizer in a way that ensures optimization will continue until the driver exits
  update->etol = std::numeric_limits<double>::min();
  update->ftol = std::numeric_limits<double>::min();
  update->nsteps = std::numeric_limits<int>::max();
  update->max_eval = std::numeric_limits<int>::max();

  update->whichflag = 2; // 2 for minimization
  update->beginstep = update->firststep = update->ntimestep;
  update->endstep = update->laststep = update->firststep + update->nsteps;
  lmp->init();

  command = mdi_fix->engine_mode("@INIT_OPTG");

  // only two commands are valid at this point
  // SHOULD PROBABLY HAVE:
  // if ( command.scope < "GLOBAL" ) {
  //    pass
  // }
  if (strcmp(command,"EXIT_SIM") == 0 ) {
    // return, but do not flag for global exit
    return 0;
  }
  else if (strcmp(command,"EXIT") == 0 ) {
    // return, and flag for global exit
    return 1;
  }
  // Don't include the error until after the command.scope check is added above
  /*
  else {
    error->all(FLERR,strcat("MDI received unsupported command: ",command));
  }
  */

  update->minimize->setup();

  // only two commands are valid at this point
  // SHOULD PROBABLY HAVE:
  // if ( command.scope < "GLOBAL" ) {
  //    pass
  // }
  if (strcmp(command,"EXIT_SIM") == 0 ) {
    // return, but do not flag for global exit
    return 0;
  }
  else if (strcmp(command,"EXIT") == 0 ) {
    // return, and flag for global exit
    return 1;
  }
  // Don't include the error until after the command.scope check is added above
  /*
  else {
    error->all(FLERR,strcat("MDI received unsupported command: ",command));
  }
  */

  update->minimize->iterate(update->nsteps);

  // only two commands are valid at this point
  // SHOULD PROBABLY HAVE:
  // if ( command.scope < "GLOBAL" ) {
  //    pass
  // }
  if (strcmp(command,"EXIT_SIM") == 0 ) {
    // return, but do not flag for global exit
    return 0;
  }
  else if (strcmp(command,"EXIT") == 0 ) {
    // return, and flag for global exit
    return 1;
  }
  // Don't include the error until after the command.scope check is added above
  /*
  else {
    error->all(FLERR,strcat("MDI received unsupported command: ",command));
  }
  */

  error->all(FLERR,strcat("MDI reached end of OPTG simulation with invalid command: ",command));
  return 0;
}
