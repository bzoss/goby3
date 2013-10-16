// s. petillo spetillo@mit.edu 07.22.10
// ocean engineering graduate student - mit / whoi joint program
// massachusetts institute of technology (mit)
// laboratory for autonomous marine sensing systems (lamss)
// 
// this is pressure.h
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this software.  If not, see <http://www.gnu.org/licenses/>.

#ifndef PRESSUREH
#define PRESSUREH

#include <cmath>


// Calculate water density anomaly at a given Salinity, Temperature, Pressure using the seawater Equation of State.
// Taken directly from MATLAB OceansToolbox pressure.m

inline double depth2pressure(double DPTH, double XLAT)
{
    // function P80=pressure(DPTH,XLAT);
    /*
      Computes pressure given the depth at some latitude
      P = depth2pressure(DPTH,XLAT) gives the pressure P (dbars) at a depth DPTH (m) at some latitude XLAT (degrees).

      This probably works best in mid-latitude oceans, if anywhere!

      Ref: Saunders, "Practical Conversion of Pressure to Depth",
            J. Phys. Oceanog., April 1981.

      I copied this from the Matlab OceansToolbox pressure.m, copied directly from the UNESCO algorithms.


      CHECK VALUE: P80=7500.004 DBARS;FOR LAT=30 DEG., DEPTH=7321.45 METERS
                              ^P80 test value should be 7500.006 (from spetillo@mit.edu)
    */

    using namespace std; // for math functions
    
    
    const double pi = 3.14159;
    
    double PLAT=abs(XLAT*pi/180);
    double D=sin(PLAT);
    double C1=(5.92E-3)+(D*D)*(5.25E-3);
    double P80=((1-C1)-sqrt(((1-C1)*(1-C1))-((8.84E-6)*DPTH)))/4.42E-6;

    return P80;
}

#endif
