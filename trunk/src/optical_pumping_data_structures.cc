// Authors: Benjamin Fenker 2013
// Copyright 2012 Benjamin Fenker

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include <gsl/gsl_const_mksa.h>
#include "optical_pumping_data_structures.h"
#include "units.h"


extern bool op_verbose;
bool isZero;

Laser_data::Laser_data() {}

Laser_data::Laser_data(double set_nu, double set_power, double set_detune,
                       double set_linewidth, double set_s3_over_s0,
                       double tau) :
  nu(set_nu), power(set_power), detune(set_detune), linewidth(set_linewidth) {
  set_saturation_intensity(tau);
  // s1 and s2 concern the phase difference of E_+ and E_- light, but have
  // no relevance and I don't even think we measure them.  0.0 is arbitrary.
  // See Jackson (3rd edition) for definition of Stokes parameters
  stokes[0] = 2*set_power / (_epsilon_0*_speed_of_light);
  stokes[1] = 0.0;
  stokes[2] = 0.0;
  stokes[3] = stokes[0] * set_s3_over_s0;
  set_field_components();
  set_intensity_components();
}

void Laser_data::set_saturation_intensity(double tau) {
  bool debug = false;
  double I_s = M_PI / (3.0*tau);
  I_s /= pow(_speed_of_light, 2.0);
  I_s *= _planck_h * pow(nu, 3);
  saturation_intensity = I_s;
  //  saturation_intensity = 1.7397808 * _mW/_cm/_cm;
  if (debug) printf("I_s = %10.8G mW/cm^2\n", saturation_intensity/(_mW/_cm2));
}

void Laser_data::set_field_components() {
  // field[0] = sigma^- and field[1] = sigma^+ are the two basis vectors.
  // No capability at this time to do x-hat and y-hat basis
  if (stokes[0] < 0.0) {
    printf("Stokes vectors not possible.  Aborting\n");
    exit(1);
  }
  field[0] = sqrt((stokes[0] - stokes[3]) / 2.0);  // l_z = -1
  field[1] = 0.0;                                  // l_z = 0
  field[2] = sqrt((stokes[0] + stokes[3]) / 2.0);  // l_z = +1
}

void Laser_data::set_intensity_components() {
  // intensity[0] = sigma^- and intensity[1] = sigma^+ are the two basis
  // vectors. No capability at this time to do x-hat and y-hat basis
  if (field[0] < 0.0 || field[2] < 0.0) {
    printf("Electric field not possible.  Aborting\n");
    exit(1);
  }
  for (int i = 0; i < 3; i++) {
    intensity[i] = 0.5 * _epsilon_0 * _speed_of_light * pow(field[i], 2.0);
    // printf("intensity[%d] = %8.6G mW/cm2\n", i, intensity[i]/(_mW/_cm2));
  }
}

void Laser_data::switch_off(double tau) {
  power = 0.0;
  set_saturation_intensity(tau);
  for (int i = 0; i < 4; i++) {
    stokes[i] = 0.0;
  }
  set_field_components();
  set_intensity_components();
}

DM_container::DM_container(int setNumEStates, int setNumFStates,
                           int setNumGStates) :
    numEStates(setNumEStates), numFStates(setNumFStates),
    numGStates(setNumGStates),
    ee(numEStates,
       vector<gsl_complex>(numEStates, gsl_complex_rect(0.0, 0.0))),
    ff(numFStates,
       vector<gsl_complex>(numFStates, gsl_complex_rect(0.0, 0.0))),
    gg(numGStates,
       vector<gsl_complex>(numGStates, gsl_complex_rect(0.0, 0.0))),
    ef(numEStates,
       vector<gsl_complex>(numFStates, gsl_complex_rect(0.0, 0.0))),
    eg(numEStates,
       vector<gsl_complex>(numGStates, gsl_complex_rect(0.0, 0.0))),
    fg(numFStates,
       vector<gsl_complex>(numGStates, gsl_complex_rect(0.0, 0.0))) {
}

void DM_container::add(DM_container* dm, DM_container *other) {
  for (int e = 0; e < dm -> numEStates; e++) {
    for (int ep = 0; ep < dm -> numEStates; ep++) {
      dm -> ee[e][ep] = gsl_complex_add(dm -> ee[e][ep],
                                            other -> ee[e][ep]);
    }
    for (int f = 0; f < dm -> numFStates; f++) {
      dm -> ef[e][f] = gsl_complex_add(dm -> ef[e][f],
                                           other -> ef[e][f]);
    }
    for (int g = 0; g < dm -> numGStates; g++) {
      dm -> eg[e][g] = gsl_complex_add(dm -> eg[e][g],
                                           other -> eg[e][g]);
    }
  }
  for (int f = 0; f < dm -> numFStates; f++) {
    for (int fp = 0; fp < dm -> numFStates; fp++) {
      dm -> ff[f][fp] = gsl_complex_add(dm -> ff[f][fp],
                                            other -> ff[f][fp]);
    }
    for (int g = 0; g < dm -> numGStates; g++) {
      dm -> fg[f][g] = gsl_complex_add(dm -> fg[f][g],
                                           other -> fg[f][g]);
    }
  }
  for (int g = 0; g < dm -> numGStates; g++) {
    for (int gp = 0; gp < dm -> numGStates; gp++) {
      dm -> gg[g][gp] = gsl_complex_add(dm -> gg[g][gp],
                                            other -> gg[g][gp]);
    }
  }
}

bool DM_container::okay_to_add(DM_container *dm, DM_container *other) {
  DM_container *tmp = new DM_container(*dm);
  DM_container::add(tmp, other);
  for (int e = 0; e < dm -> numEStates; e++) {
    double epop = GSL_REAL(dm->ee[e][e]);
    if (epop > 1 || epop < 0) return false;
  }
  for (int f = 0; f < dm -> numFStates; f++) {
    double fpop = GSL_REAL(dm->ff[f][f]);
    if (fpop > 1 || fpop < 0) return false;
  }
  for (int g = 0; g < dm -> numGStates; g++) {
    double gpop = GSL_REAL(dm->gg[g][g]);
    if (gpop > 1 || gpop < 0) return false;
  }
  return true;
}

void DM_container::mul(DM_container *dm, double c) {
  for (int e = 0; e < dm -> numEStates; e++) {
    for (int ep = 0; ep < dm -> numEStates; ep++) {
      dm -> ee[e][ep] = gsl_complex_mul_real(dm -> ee[e][ep], c);
    }
    for (int f = 0; f < dm -> numFStates; f++) {
      dm -> ef[e][f] = gsl_complex_mul_real(dm -> ef[e][f], c);
    }
    for (int g = 0; g < dm -> numGStates; g++) {
      dm -> eg[e][g] = gsl_complex_mul_real(dm -> eg[e][g], c);
    }
  }
  for (int f = 0; f < dm -> numFStates; f++) {
    for (int fp = 0; fp < dm -> numFStates; fp++) {
      dm -> ff[f][fp] = gsl_complex_mul_real(dm -> ff[f][fp], c);
    }
    for (int g = 0; g < dm -> numGStates; g++) {
      dm -> fg[f][g] = gsl_complex_mul_real(dm -> fg[f][g], c);
    }
  }
  for (int g = 0; g < dm -> numGStates; g++) {
    for (int gp = 0; gp < dm -> numGStates; gp++) {
      dm -> gg[g][gp] = gsl_complex_mul_real(dm -> gg[g][gp], c);
    }
  }
}

bool DM_container::equalsZero() {
  isZero = false;
  double eps = pow(10, -6);
  for (int e = 0; e < numEStates; e++) {
    for (int ep = 0; ep < numEStates; ep++) {
      if (fabs(GSL_REAL(ee[e][ep])) > eps) return false;
      if (fabs(GSL_IMAG(ee[e][ep])) > eps) return false;
    }
    for (int f = 0; f < numFStates; f++) {
      if (fabs(GSL_REAL(ef[e][f])) > eps) return false;
      if (fabs(GSL_IMAG(ef[e][f])) > eps) return false;
    }
    for (int g = 0; g < numGStates; g++) {
      if (fabs(GSL_REAL(eg[e][g])) > eps) return false;
      if (fabs(GSL_IMAG(eg[e][g])) > eps) return false;
    }
  }
  for (int f = 0; f < numFStates; f++) {
    for (int fp = 0; fp < numFStates; fp++) {
      if (fabs(GSL_REAL(ff[f][fp])) > eps) return false;
      if (fabs(GSL_IMAG(ff[f][fp])) > eps) return false;
    }
    for (int g = 0; g < numGStates; g++) {
      if (fabs(GSL_REAL(fg[f][g])) > eps) return false;
      if (fabs(GSL_IMAG(fg[f][g])) > eps) return false;
    }
  }
  for (int g = 0; g < numGStates; g++) {
    for (int gp = 0; gp < numGStates; gp++) {
      if (fabs(GSL_REAL(gg[g][gp])) > eps) return false;
      if (fabs(GSL_IMAG(gg[g][gp])) > eps) return false;
    }
  }
  isZero = true;
  return true;
}
