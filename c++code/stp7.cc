//
// File:   stp7.cc
// Author: erik
//
// Created on 22. April 2007, 17:06
//

#include "polynomial.h"
#include "complex.h"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include "tools.h"
#include "stp3.h"
#include "stp7.h"
#include "stp7Formulars.h"
#include <stdexcept>
#include <sstream>
#include "math.h"
#include <cxxtest/TestSuite.h>

using namespace std;

// initialize Stp7 string constants...
const string Stp7::PROFILE_STOP = "stop profile";
const string Stp7::PROFILE_TT = "TT profile";
const string Stp7::PROFILE_TW = "TW profile";
const string Stp7::PROFILE_WT = "WT profile";
const string Stp7::PROFILE_WW = "WW profile";

void Stp7::planProfile() {
    /* Calculates the time optimal third-order trajectory to reach the target
     * position with the given start conditions and according to the limitations
     * for jerk, acc and vel.
     * There are two arrays filled up with the correct values: The array holding
     * the jerk impulses and the one describing the time intervalls for these
     * impulses. Both have a fixed length of 7 entries, however, several entries
     * in the t-array might be zero - indicating that this paricular phase is
     * not needed in the profil.
     * The elements of the jerk array are the jerk values (NOT: either of the
     * three values -1, 0, 1. NOT: Multiplication with the max-jerk gives the
     * actual jerk-value.)
     */
    
    int dir = 0;
    double xTarget = _x[7];
    double x_dummy, a_dummy, v_dummy;
    double xStop;
    
    // (1) Calculation of the direction flag (direction of potential cruising
    // phase) by comparing the position we reach at an immideate halt to the
    // desired target position.
    Stp3 stp3Stop;
    stp3Stop.planFastestProfile(_v[0], 0, _a[0], _amax, _jmax);
    stp3Stop.getTimeArray(_t);
    stp3Stop.getAccArray(_j);
    
    Stp7::calcjTracks(_t, _j, 3, _x[0], _v[0], _a[0], xStop, v_dummy, a_dummy);
    dir = sign(xTarget-xStop);
    if (dir == 0) {
        for (int i = 4; i < 8; i++) {
            _t[i] = _t[3]; _j[i] = 0;
        }
        _sProfileType = Stp7::PROFILE_STOP;
        _bIsddec = false;
        _bHasCruise = false;
        return;
    } else {
        // position change just from acc and dec phase:
        Stp3 stp3Acc, stp3Dec;
        stp3Acc.planFastestProfile(_v[0], dir*_vmax, _a[0], _amax, _jmax);
        stp3Dec.planFastestProfile(dir*_vmax, 0, 0, _amax, _jmax);
        // position change:
        stp3Acc.getTimeArray(_t);
        stp3Acc.getAccArray(_j);
        stp3Dec.getTimeArray(&(_t[4]));
        stp3Dec.getAccArray(&(_j[4]));
        _t[4] = 0; _j[4] = 0;
        for (int i = 4; i < 8; i++) _t[i] += _t[3];
        Stp7::calcjTracks(_t, _j, 7, _x[0], _v[0], _a[0],
        xStop, v_dummy, a_dummy);
        // distance we need to go in cruising phase:
        double xDelta = (xTarget-xStop);
        double tDelta = xDelta / (dir*_vmax);
        
        // case differentiation: Do we have a cruising phase?
        if (tDelta >= 0) {
            // with cruising phase, insert t_delta as cruising phase (t[4])
            for (int i = 4; i < 8; i++) _t[i] += tDelta;
            _bIsddec = false;
            _bHasCruise = true;
            if (stp3Acc.isTrapezoid())
                _sProfileType = (stp3Dec.isTrapezoid()) ? Stp7::PROFILE_TT
                : Stp7::PROFILE_TW;
            else 
                _sProfileType = (stp3Dec.isTrapezoid()) ? Stp7::PROFILE_WT
                : Stp7::PROFILE_WW;
            _bIsddec = (_j[1]==_j[5]);
        } else {
            // without cruising phase TODO TODO
            planProfileNoCruise(dir);
            _bHasCruise = false;
        }
    }
}

void Stp7::convertTimePointsToIntervalls() {
    for (int i = 7; i > 0; i--) {
        _t[i] = _t[i] - _t[i-1];
    }
}

void Stp7::convertTimeIntervallsToPoints() {
    for (int i = 1; i < 8; i++) {
        _t[i] = _t[i] + _t[i-1];
    }
}

void Stp7::planProfileNoCruise(int dir) {
    // The function must be called with a valid 7-phases profile stored in
    // _t and _j arrays, which is overshooting the target. It will then first do
    // a case distinction to check, which kind of profile we currently have.
    // Next step is to cut out / shift parts of the acc-profile to shorten the
    // profile until it is not overshooting the target anymore. At this time we
    // know which profile-type the final solution will have and can call the
    // appropriate function to find the final solution.

    // (0) Check whether a normal profile has to switch into double deceleration
    // (1) Check whether we have a double deceleration profile.
    // (2) Case distinction: TT / TW / WT / WW
    
    // its easier to calculate in time intervalls than time points, so we
    // convert the time array into intervalls first. In the end we just
    // convert it back.
    _bHasCruise = false;
    convertTimePointsToIntervalls();
    
    double x_dummy, v_dummy, a_dummy;
    
    // (0)
    if (sign(_j[3]) == sign(_j[5]) && _t[3] < _t[1]) {
        double tAcc[] = {0, _t[3],  _t[2], _t[3]};
        double tDec[] = {0, _t[5],  _t[6], _t[7]};
        double deltaAcc, deltaDec;
        Stp7::calcjTracksTimeInt(tAcc, _j, 3, 0, 0, 0, x_dummy, deltaAcc, a_dummy);
        Stp7::calcjTracksTimeInt(tDec, &(_j[4]), 3, 0, 0, 0, x_dummy, deltaDec, a_dummy);
        deltaAcc = fabs(deltaAcc);
        deltaDec = fabs(deltaDec);
        if (deltaAcc < deltaDec) {
            tAcc[1] = tAcc[2] = 0; tAcc[3] = _t[1]-_t[3];
            Stp7::removeAreaTimeInt(tDec, deltaAcc, _amax, _jmax);
            double jNew[] = {0, _j[3], 0, _j[1], 0, _j[5], _j[6], _j[7]};
            double tNew[8];
            for (int i = 0; i < 8; i++) tNew[i] = (i<4) ? tAcc[i] : tDec[i-4];
            // If we still overshoot after putting as much area under the acc
            // graph from the front to the back, the profile becomes double
            // deceleration.
            if (stillOvershootsTimeInt(tNew, jNew, dir, _x[0], _x[7], _v[0], _a[0]))
                for (int i = 0; i < 8; i++) {_t[i] = tNew[i]; _j[i] = jNew[i]; }
        }
    }

    double da;
    da = _j[1] == _j[3] ? -1 : 1;

    // (1)
    // check if we have a double deceleration profile
    if (sign(_j[3]) != sign (_j[5])) {
        _sProfileType = Stp7::PROFILE_WW;
        if (_t[6] == 0) {
            // second part is currently wedge, may become trapez
            // calculate maximal shift from first to second deceleration phase
            // in order to reach the W-T-border case
            double a2, v2;
            Stp7::calcjTracksTimeInt(_t, _j, 2, _x[0], _v[0], _a[0], x_dummy, v2, a2);
            double tDelta, t5, t7;
            Stp7::calc7st_opt_shiftTimeInt(_t, _j, dir, _amax, _jmax, v2, a2, tDelta, t5,t7);
            if (tDelta < 0) throw out_of_range("DeltaT negative at opt_shift!");
            if (tDelta < _t[3]) {
                // adapt profile by shortening t[3]
                double tNew[] = {0, _t[1], _t[2], _t[3]-tDelta, _t[4], t5, 0, t7};
                // if we still overshoot, the profile becomes trapezoidal
                if (stillOvershootsTimeInt(tNew, _j, dir, _x[0], _x[7], _v[0], _a[0])) {
                    for (int i = 0; i < 8; i++) _t[i] = tNew[i];
                    // allow trapez in second part when generating formulas:
                    _t[6] = 1;
                    _sProfileType = Stp7::PROFILE_TT;
                }
            } else {
                // velocity delta in phase 3 is not enough to extend
                // wedge-shaped second decleration phase to trapezoidal shape
                // so we stay at a triangular profile    
            }  
        }

        _bIsddec = true;
        
        // Calculate exact phase duration from given profile t, j
        Stp7Formulars::solveProfile(_t, _sProfileType, _bIsddec, da==-1, dir==1,
                    _x[0], _x[7], _v[0], _vmax, _a[0], _amax, _jmax);
        _sProfileType = getProfileString(_t);
        
        convertTimeIntervallsToPoints();
        return;
    }
    
    // (2)
    // we don't have double deceleration --> cut out instead of merging
    // find correct profile by cutting pieces and descending to shorter profiles
    _bIsddec = false;
    _sProfileType = Stp7::findProfileTimeInt(_t, _j, dir, _x[0], _x[7], _v[0], _a[0],
                                     _amax, _jmax);
    
    // Calculate exact phase duration for choosen profile t, j
    // j[0..7] are already the correct values!
    Stp7Formulars::solveProfile(_t, _sProfileType, _bIsddec, da==-1, dir==1,
                    _x[0], _x[7], _v[0], _vmax, _a[0], _amax, _jmax);
    
    convertTimeIntervallsToPoints();
    return;
}

/**
 * This function stretches the computed profile to a new (longer) duration.
 */
double Stp7::scaleToDuration(double newDuration) {
    convertTimePointsToIntervalls();
    
    splitNoCruiseProfileTimeInt(_t, _j, _a[0]);

    double a_dummy, v_dummy, x_dummy;
    
    double v3;
    calcjTracksTimeInt(_t, _j, 3, _x[0], _v[0], _a[0], x_dummy, v3, a_dummy);
    double dir = sign(v3); // TODO: what happens when dir=0?
    
    // check whether we must use the double deceleration branch
    bool useDDec = false;
    double t_orig[8]; for (int i = 0; i < 8; i++) t_orig[i] = _t[i];
    double j_orig[8]; for (int i = 0; i < 8; i++) j_orig[i] = _j[i];
    {
       // Tests for a given profile, whether stretching it to the time T would
       // nead a double deceleration profile.    
       // TODO TODO TODO:
       // folgenden einfachen algorithmus verwenden:
       // Profil erzeugen: erst a auf null, dann v auf null
       // ist dieses profil zu langsam --> double deceleration / normal

        if (sign(_j[3]) != sign(_j[5])) {
            // that was easy - it already is a double dec. profile :)
            useDDec = true;
        } else {
            // If the velocity change within the deceleration part is larger in magnitude than
            // the velocity change within acceleration part (starting from max velocity,
            // i.e. at a=0, the cutting process in findProfileNormal may lead to the
            // situation, where no more area can be cut from the acceleration part,
            // although the desired duration T is not yet reached. In this case we have
            // to switch to the double deceleration branch as well.
            // We check whether we are still too early at the target, when employing a
            // full stop profile, reducing acceleration to zero immidiately in the first
            // phase. 
            double j1,t1;    
            if (_a[0] == 0) {
                j1 = _j[5]; // direction of double deceleration
                t1 = 0;
            } else {
                // jerk to decrease acceleration to zero
                j1 = -sign(_a[0]) * _jmax; 
                // time needed to reach zero acceleration
                t1 = fabs(_a[0]/j1); 
            }

            // position and velocity reached after this time
            double a1, v1, x1;
            calcjTrack(t1, _x[0], _v[0], _a[0], j1, x1, v1, a1); // a1 == 0
            // profile to reach zero velocity, starting from v1
            Stp3 stp3Dec;
            stp3Dec.planFastestProfile(v1, 0, a1, _amax, _jmax);
            double tdec[4], jdec[4];
            stp3Dec.getTimeIntArray(tdec);
            stp3Dec.getAccArray(jdec);
            
            // If the a(t) profile in deceleration part has the same direction as before,
            // we may need to switch to a double deceleration profile. 
            if (sign(jdec[1]) == sign(_j[5])) { // we may need to switch
                if (j1 == jdec[3]) { 
                    _t[1] = 0; _t[2] = 0; _t[3] = t1; _t[4] = 0;
                    _t[5] = tdec[1]; _t[6] = tdec[2]; _t[7] = tdec[3];
                    _j[1] = jdec[1]; _j[2] = jdec[2]; _j[3] = jdec[3];
                    _j[4] = 0; _j[5] = jdec[1]; _j[6] = jdec[2]; _j[7] = jdec[3];
                } else {
                    _t[1] = t1; _t[2] = 0; _t[3] = 0; _t[4] = 0;
                    _t[5] = tdec[1]; _t[6] = tdec[2]; _t[7] = tdec[3];
                    _j[1] = jdec[1]; _j[2] = jdec[2]; _j[3] = jdec[3];
                    _j[4] = 0; _j[5] = jdec[1]; _j[6] = jdec[2]; _j[7] = jdec[3];
                }
                // insert cruising phase, such that the target is still reached
                adaptProfile (_t, _j, _x[7], _x[0], _v[0], _a[0]);
                useDDec = stillTooShort (_t, newDuration);
            } else {
                useDDec = false;
            }
        }
    }
    if (!useDDec) {
        for (int i = 0; i < 8; i++) {
            _t[i] = t_orig[i];
            _j[i] = j_orig[i];
        }
    }
    
    double da;
    da = _j[1] == _j[3] ? -1 : 1;
    
    if (useDDec) {
        // double deceleration branch
        findProfileTypeStretchDoubleDec(newDuration);
        // extend simple profile computed in useDoubleDeceleration() to
        // wedge-shaped profile
        if (_t[1] != 0 && _t[3] == 0) _t[3] = 1;
        if (_t[1] == 0 && _t[3] != 0) _t[1] = 1;
        _sProfileType = getProfileString(_t);
        _bIsddec = true;
        if (_sProfileType == Stp7::PROFILE_TT || _sProfileType == Stp7::PROFILE_WT) {
            TS_WARN("stretch ddec ?T - TODO");
            //solveProfileTT(_t, _x[0], _x[7], _v[0], _a[0], _amax, _jmax, da, dir);
        } else if (_sProfileType == Stp7::PROFILE_TW || _sProfileType == Stp7::PROFILE_WW) {
            //TS_WARN("stretch ddec ?W - TODO");
            Stp7Formulars::solveProfile(_t, _sProfileType, _bIsddec, da==-1, dir==1,
                    _x[0], _x[7], _v[0], _vmax, _a[0], _amax, _jmax, newDuration);
        } else throw invalid_argument("Unknown profile!");
        _bHasCruise = (_t[4] != 0);
    } else {
        // normal profile branch
        _bIsddec = false;
        findProfileTypeStretchCanonical(newDuration);
        _sProfileType = getProfileString(_t);
        Stp7Formulars::solveProfile(_t, _sProfileType, _bIsddec, da==-1, dir==1,
                    _x[0], _x[7], _v[0], _vmax, _a[0], _amax, _jmax, newDuration);
        this->_bHasCruise = (_t[4] != 0);
    }
    convertTimeIntervallsToPoints();
    
    return getDuration();
}

void Stp7::adaptProfile(double t[8], double j[8], double xtarget, double x0, double v0, double a0) {
    // Given a profile (t,j) where acceleration and deceleration phase was
    // already changed (cutted or shifted) such that velocity v(3) is smaller
    // in magnitude than before, this function extends the cruising phase (or
    // inserts one), such that the target is reach again.
    // This is a simple linear equation...
    double a_dummy, v_dummy, x_dummy;
    double xend, v3new;
    calcjTracksTimeInt(t, j, 7, x0, v0, a0, xend, v_dummy, a_dummy);
    calcjTracksTimeInt(t, j, 3, x0, v0, a0, x_dummy, v3new, a_dummy);
    //  enlarge cruising time, such that area below velocity profile equals dp again
    t[4] = t[4] + (xtarget - xend) / v3new;
}

bool Stp7::stillTooShort(double t[8], double newDuration) {
     return (t[1]+t[2]+t[3]+t[4]+t[5]+t[6]+t[7] < newDuration);
}

void Stp7::findProfileTypeStretchCanonical(double newDuration) {
    // find correct profile by cutting pieces and descending to shorter profiles

    _sProfileType = getProfileString(_t);
    
    // if profile type does not change, we just insert cruising phase into t
    double t_orig[8];
    for (int i = 0; i < 8; i++) t_orig[i] = _t[i];
    if (_t[4] == 0) t_orig[4]=1;
    
    if (_sProfileType == PROFILE_TT) {
        // cut out smaller a=const. part
        double dt = min(_t[2], _t[6]);
        _t[2] = _t[2] - dt;
        _t[6] = _t[6] - dt;
        adaptProfile (_t, _j, _x[7], _x[0], _v[0], _a[0]);
        if (stillTooShort(_t, newDuration)) {
            // recursively calling this function even cuts further
            findProfileTypeStretchCanonical(newDuration);
        } else {
            // now we stop after duration time newDuration, hence profile stays TT
           for (int i = 0; i < 8; i++) _t[i] = t_orig[i]; // allow for a cruising phase
        }
        return;
    }

    if (_sProfileType == PROFILE_WW) {
        for (int i = 0; i < 8; i++) _t[i] = t_orig[i]; // allow for a cruising phase
        return; // nothing to do, WW stays WW anytime
    }

    if (_sProfileType == PROFILE_WT) {
        double a1 = _a[0] + _j[1]*_t[1];
        double dt_w = min(_t[1],_t[3]);
        double area_w_max = 0.5*dt_w*dt_w*_jmax;
        if (_t[1] > _t[3]) area_w_max += fabs(2.*_a[0]*dt_w);
        double area_t_max = _t[6]*_amax;
        if (area_w_max > area_t_max) {
            // we will cut out the whole t(6) WT -> WW
            _t[6] = 0;
            double dt = (fabs(a1)-sqrt(a1*a1-area_t_max))/_jmax;
            _t[1] = _t[1]-dt;
            _t[3] = _t[3]-dt;
            adaptProfile(_t,_j,_x[7],_x[0],_v[0],_a[0]);
            if (stillTooShort(_t,newDuration)) {
                _sProfileType = PROFILE_WW; // type switches to WW
            } else {
                // now we stop after duration time newDuration, hence profile stays WT
                for (int i = 0; i < 8; i++) _t[i] = t_orig[i]; // allow for a cruising phase
            }
        } else {
            // nothing to cut out, stays at WT
            for (int i = 0; i < 8; i++) _t[i] = t_orig[i]; // allow for a cruising phase
        }
        return;
    }
    
    if (_sProfileType == PROFILE_TW) {
        double a5 = _j[5]*_t[5];
        double area_w_max = fabs(_t[5]*a5);
        double area_t_max = _t[2]*_amax;
        if (area_w_max > area_t_max) {
            // we will cut out the whole t(2)
            _t[2] = 0;
            _t[5] = sqrt((area_w_max-area_t_max)/fabs(_j[5]));
            _t[7] = _t[5];
            adaptProfile(_t,_j,_x[7],_x[0],_v[0],_a[0]);
            if (stillTooShort(_t,newDuration)) {
                _sProfileType = PROFILE_WW; // type switches to WW
            } else {
                // now we stop after duration time newDuration, hence profile stays TW
                for (int i = 0; i < 8; i++) _t[i] = t_orig[i]; // allow for a cruising phase
            }
        } else {
            // nothing to cut out, stays at WT
            for (int i = 0; i < 8; i++) _t[i] = t_orig[i]; // allow for a cruising phase
        }
        return;
    }
    
    return;
}

void Stp7::findProfileTypeStretchDoubleDec(double newDuration) {
    // find correct double deceleration profile by shifting area from second
    // deceleration to first deceleration part.
    // We can get two type of deceleration profiles here:
    // 1) The time-optimal profile was already double deceleraton, leading to a(3) ~= 0
    // 2) all other profiles: a(3) == 0, there might be profile [0 0 t3] / [t1 0 0]
    
    double v_dummy, x_dummy;
    double a3, a7;
    calcjTracksTimeInt(_t, _j, 3, _x[0], _v[0], _a[0], x_dummy, v_dummy, a3);
    
    double tdec[4];
    
    if (!isZero(a3)) {
        // 1) Time-optiomal profile was double deceleration already
        // In this case we must check, whether we can reach a3 = 0 (to insert
        // a cruising phase
        double tz = fabs(a3/_jmax); // time needed to reach zero acceleration
        // try to shorten deceleration profile, such that a3 reaches zero in between
        tdec[1] = _t[5]+tz; tdec[2] = _t[6]; tdec[3] = _t[7];
        removeAreaTimeInt(tdec, fabs(a3*tz), _amax, _jmax);
        // adapt profile to reach target again
        double tn[8];
        tn[1] = _t[1]; tn[2] = _t[2]; tn[3] = _t[3]; tn[4] = 0;
        tn[5] = tdec[1]; tn[6] = tdec[2]; tn[7] = tdec[3];
        adaptProfile (tn, _j, _x[7], _x[0], _v[0], _a[0]);
        // if target is overshooted if trying to set a3=0, 
        // adaptProfile will return negative time tn(4) (and we must keep the current profile)
        if (tn[4] < 0 || !stillTooShort(tn,newDuration))
            return; // keep existing profile
        for (int i = 0; i < 6; i++) _t[i] = tn[i]; // use adapted profile for further checks
    }

    // Compute current velocity decrease achieved during first and second part
    double curFirst, curLast, t_0acc;
    calcjTracksTimeInt(_t, _j, 3, 0, 0, _a[0], x_dummy, curFirst, a3); // a3=0
    calcjTracksTimeInt(&(_t[4]), _j, 3, 0., 0., 0., x_dummy, curLast, a7); // a7=0
    //////////////// WARUM nicht _j[4] ???
    curFirst = fabs(curFirst); curLast = fabs(curLast);

    // We enlarge curFirst such, that it contains the area of the (full) triangle 
    // which reaches peak acceleration a2
    if (_a[0] !=0 && sign(_a[0]) == sign(_j[7])) {
        // time needed to reach zero acceleration
        t_0acc = fabs(_a[0]/_j[1]);
        // add area on the other side of x-axis 
        // (because it was substracted during integration)
        curFirst = curFirst + fabs(_a[0])*t_0acc/2;
    } else {
        // time needed to reach zero acceleration (now backwards)
        t_0acc = -fabs(_a[0]/_j[1]);
        // add (virtual) area of the missing triangle
        curFirst = curFirst + fabs(_a[0]*t_0acc/2);
    }
    
    double wedgeMax = _amax*_amax/_jmax;
    double deltaFirst, deltaLast, deltaV, tacc[4], tn[8];
    
    while (1) {
        // area needed to extend first part to full wedge
        deltaFirst = wedgeMax - curFirst;
        if (_t[2] == 0 && !isZero(deltaFirst)) { // first part is not yet full wedge 
            if (_t[6] == 0) {
                deltaLast = curLast; // area available in second part
                // if last part has not enough area to extend first one to full
                // triangle, the profile will keep WW shape
                if (deltaFirst >= deltaLast) return;
            } else {
                deltaLast = _t[6]*_amax; // area below const-trapezoidal part
            }
            deltaV = min(deltaFirst, deltaLast);
        } else {
            if (_t[2] == 0) _t[2] = 1; // allow const-part in trapez
            if (_t[6] == 0) return; // profile will keep TW shape
            deltaV = _t[6]*_amax; // area below const-trapezoidal part
        }
        
        addAreaTimeInt (t_0acc, curFirst + deltaV - wedgeMax, _amax,_jmax, tacc);
        tdec[1] = _t[5]; tdec[2] = _t[6]; tdec[3] = _t[7];
        removeAreaTimeInt (tdec, deltaV, _amax,_jmax);
        double tn[8];
        tn[1] = tacc[1]; tn[2] = tacc[2]; tn[3] = tacc[3]; tn[4] = _t[4];
        tn[5] = tdec[1]; tn[6] = tdec[2]; tn[7] = tdec[3];
        adaptProfile (tn, _j, _x[7], _x[0], _v[0], _a[0]);
        // if we overshoot in time, t contains the correct profile
        if (~stillTooShort(tn,newDuration)) return;
        // otherwise continue probing with adapted profile
        for (int i = 0; i < 6; i++) _t[i] = tn[i]; // use adapted profile for further checkst = tn;
        curFirst = curFirst + deltaV;
        curLast  = curLast  - deltaV;
    }
}

void Stp7::addAreaTimeInt(double deltaT1, double deltaV, double amax, double jmax, double t[4]) {
    // Compute a profile [t1 t2 t3] such that its area is wedgeMax + deltaV.
    // deltaT1 is extra time added to t1
    // The result is written in t[1..3].
    double tmax = amax/jmax;
    if (deltaV >= 0) { // full wedge + const trapezoidal part
        t[1] = tmax + deltaT1;
        t[2] = deltaV/amax;
        t[3] = tmax;
    } else {
        double deltaT = tmax - sqrt (tmax*tmax + deltaV/jmax);
        t[1] = tmax + deltaT1 - deltaT;
        t[2] = 0;
        t[3] = tmax - deltaT;
    }
}

string Stp7::getProfileString(double t[8]) {
    if (t[2] != 0) { // T? profile
        if (t[6] != 0) return Stp7::PROFILE_TT;
        else return Stp7::PROFILE_TW;
    } else { // W? profile
        if (t[6] != 0) return Stp7::PROFILE_WT;
        else return Stp7::PROFILE_WW;
    } 
}

string Stp7::findProfileTimeInt(double t[8], double j[8], int dir, double x0,
                         double xTarget, double v0, double a0, double amax,
                         double jmax) {
    // find correct profile by cutting pieces and descending to shorter profiles
    // uses the values t[1..7] and j[1..7] and changes them accodingly to the
    // new profile type.
    string type = getProfileString(t);
    
    double tOld[8];
    for (int i = 0; i < 8; i++) tOld[i] = t[i];

    if (type == Stp7::PROFILE_TT) {
        // cut out smaller a=const. part
        double dt = min(t[2], t[6]);
        t[2] = t[2] - dt;
        t[6] = t[6] - dt;
        if (stillOvershootsTimeInt(t, j, dir, x0, xTarget, v0, a0)) {
            // recursively calling this function even cuts further
            type = findProfileTimeInt(t,j,dir,x0,xTarget,v0,a0,amax,jmax);
        } else {
            // now we stop before the target, hence profile stays TT
            for (int i = 0; i < 8; i++) t[i] = tOld[i];
        }
        return type;
    }

    if (type == Stp7::PROFILE_WW) {
        // nothing to do, WW stays WW anytime
        return type;
    }
    
    if (type == Stp7::PROFILE_WT) {
        double a1 = a0 + j[1]*t[1];
        double dt_w = min(t[1],t[3]);
        double area_w_max = fabs(dt_w * (2*a1 - dt_w*j[1]));
        double area_t_max = t[6]*amax;
        if (area_w_max > area_t_max) {
            // we will cut out the whole t[6] WT -> WW
            t[6] = 0;
            double dt = (fabs(a1)-sqrt(a1*a1-area_t_max))/jmax;
            t[1] = t[1]-dt;
            t[3] = t[3]-dt;
            if (stillOvershootsTimeInt(t, j, dir, x0, xTarget, v0, a0)) {
                type = Stp7::PROFILE_WW; // type switches to WW
            } else {
                // now we stop before the target, hence profile stays WT
                for (int i = 0; i < 8; i++) t[i] = tOld[i];
            }
        } else; // nothing to cut out, stays WT
        return type;
    }
    
    if (type == Stp7::PROFILE_TW) {
        double a5 = j[5]*t[5];
        double area_w_max = fabs(t[5]*a5);
        double area_t_max = t[2]*amax;
        if (area_w_max > area_t_max) {
            // we will cut out the whole t[2]
            t[2] = 0;
            t[5] = sqrt((area_w_max-area_t_max)/fabs(j[5]));
            t[7] = t[5];
            if (stillOvershootsTimeInt(t, j, dir, x0, xTarget, v0, a0)) {
                type = Stp7::PROFILE_WW;
            } else {
                // now we stop before the target, hence profile stays TW
                for (int i = 0; i < 8; i++) t[i] = tOld[i];
            }
        }
        return type;
    }
}

void Stp7::calc7st_opt_shiftTimeInt(double t[8], double j[8], int dir, double amax,
                                  double jmax, double v2, double a2,
                                  double &tDelta, double &t5, double &t7) {
    // Given a deceleration - deceleration profile with wedge-shaped second part, 
    // compute the period DeltaT which must be cut from third phase (and
    // inserted in second part), such that the second part becomes a triangular
    // profile exactly hitting -d*amax.
    double v3, a3, v6, a6, x_dummy;
    // compute a3 and a6
    Stp7::calcjTrack(t[3], 0., v2, a2, j[3], x_dummy, v3, a3);
    Stp7::calcjTracksTimeInt(&(t[3]), &(j[3]), 3, 0, v3, a3, x_dummy, v6, a6);
    
    // compute discriminant of quadratic polynomial solution
    double diskriminant = 4*amax*amax + 2*jmax*jmax*(t[7]*t[7] - t[5]*t[5])
                          + 4*dir*jmax*(a6*t[7] + a3*t[5]) + 2*a3*a3;
    if (isZero(diskriminant)) diskriminant = 0;
    double root = sqrt(diskriminant);
    
    // compute T5
    if (dir < 0) t5 = dir * amax + root/2;
    else t5 = dir * amax - root/2;
    t5 = t5 / (dir * jmax);

    // compute DeltaT and T7
    tDelta = (a3 + dir * amax) / (dir * jmax) - t5;
    if (isZero(tDelta)) tDelta = 0;
    t7 = amax / jmax;                                 
}

    
void Stp7::removeAreaTimeInt(double t[4], double deltaV, double amax, double jmax) {
    // Takes an array t[1..3] and deletes the passed deltaV from the area under
    // the acceleration graph. The elements of t are altered accordingly.
    
    // we only decrease the area...
    deltaV = fabs(deltaV);
    double A_now = t[1]*t[1]*jmax+amax*t[2];
    if (A_now < deltaV) return;        // not enough to cut out...
    double Aw_max = amax*amax/jmax;
        
    if (isZero(t[2]) || (Aw_max > A_now - deltaV)) {
        // result wedge shaped
        t[1] = sqrt(A_now/jmax - deltaV/jmax);
        t[3] = t[1];
        t[2] = 0;
    } else {
        // result trapezoid shaped
        t[2] = t[2] - deltaV/amax;
    }
}

void Stp7::splitNoCruiseProfileTimeInt(double t[8], double j[8], double a0) {
    // In case of a profile without cruising phase, the time intervalls
    // t(3) and t(5) might be joined together into one of them so the other
    // one is zero. This can only occour if j(3) and j(5) have the same
    // sign.
    // For the stretching algorithm, we need to split the time intervall up
    // so the acc-graph reaches zero after t(3).
    if (t[4] != 0) return;
    if (j[3] != j[5]) return;
    double tsum = t[3] + t[5];
    t[3] = fabs((a0 + j[1]*t[1]) / j[3]);   // = -a2/j(3)
    t[5] = tsum - t[3];
}
    
bool Stp7::stillOvershootsTimeInt(double t[8], double j[8], int dir,
                            double x0, double xTarget, double v0, double a0) {
    double v_dummy, a_dummy, xEnd;
    Stp7::calcjTracksTimeInt(t, j, 7, x0, v0, a0, xEnd, v_dummy, a_dummy);
    return (sign(xEnd - xTarget)*dir == 1);                            
}

double Stp7::planFastestProfile(double x0, double xtarget, double v0,
                                double vmax, double a0, double amax,
                                double jmax) {
    // first set object fields
    _vmax = vmax; _amax = amax; _jmax = jmax;
    _x[0] = x0; _x[7] = xtarget;
    _v[0] = v0; _a[0] = a0; _j[0] = 0; _t[0] = 0;
    
    // Do the planning algorithm --> we get back the jerks and times.
    planProfile();
    
    for (int i = 1; i < 8; i++) {
        // calc x,v,a values for next switch point
        calcjTrack(_t[i]-_t[i-1], _x[i-1], _v[i-1], _a[i-1], _j[i], _x[i], _v[i], _a[i]);
    }
    
    _plannedProfile = true;
    
    
    // test, wether algorithm is correct
    //cout << this->getProfileType() << endl;
    //cout << "delta p: " << _x[7]-xtarget << endl;
    //if (_x[7] != xtarget)
      //  throw logic_error("The planned profile does not reach the goal!");
    
    
    return _t[7];
}

bool Stp7::isDoubleDecProfile() const {
    if (!_plannedProfile)
        throw invalid_argument("Consider to call planFastestProfile(.) first.");
    return _bIsddec;
}

bool Stp7::hasCruisingPhase() const {
    if (!_plannedProfile)
        throw invalid_argument("Consider to call planFastestProfile(.) first.");
    return _bHasCruise;
}

string Stp7::getProfileType() const {
    if (!_plannedProfile)
        throw invalid_argument("Consider to call planFastestProfile(.) first.");
    return _sProfileType;
}

double Stp7::getSwitchTime(int i) const {
    if (!_plannedProfile)
        throw invalid_argument("Consider to call planFastestProfile(.) first.");
    if (i<0 || i>7)
        throw out_of_range("Index for time must be in {0,...,7}!");
    return _t[i];
}

double Stp7::getTimeIntervall(int i) const {
    if (!_plannedProfile)
        throw invalid_argument("Consider to call planFastestProfile(.) first.");
    if (i<=0 || i>7)
        throw out_of_range("Index for time must be in {1,...,7}!");
    return _t[i]-_t[i-1];
}

void Stp7::getJerkArray(double j[8]) const {
    if (!_plannedProfile)
        throw invalid_argument("Consider to call planFastestProfile(.) first.");
    for (int i = 0; i < 8; i++) j[i] = _j[i];
}

void Stp7::getTimeArray(double t[8]) const {
    if (!_plannedProfile)
        throw invalid_argument("Consider to call planFastestProfile(.) first.");
    for (int i = 0; i < 8; i++) t[i] = _t[i];
}

void Stp7::getTimeIntArray(double t[8]) const {
    if (!_plannedProfile)
        throw invalid_argument("Consider to call planFastestProfile(.) first.");
    t[0] = 0; t[1] = _t[1];
    for (int i = 2; i < 8; i++) t[i] = _t[i] - _t[i-1];
}

int Stp7::getPhaseIndex(double t) const {
    if (!_plannedProfile)
        throw invalid_argument("Consider to call planFastestProfile(.) first.");
    if (t < 0)
        throw invalid_argument("Negative time.");
    if (t < _t[1]) return 0;
    if (t < _t[2]) return 1;
    if (t < _t[3]) return 2;
    if (t < _t[4]) return 3;
    if (t < _t[5]) return 4;
    if (t < _t[6]) return 5;
    if (t < _t[7]) return 6;
    return 7;
}

void Stp7::calcjTrack(double dt, double x0, double v0, double a0,
double j, double &newx, double &newv, double &newa) {
    double dt2 = dt*dt;
    newx = x0 + v0*dt + (1./2.)*a0*dt2 + (1./6.)*j*dt2*dt;
    newv = v0 + a0*dt + (1./2.)*j*dt2;
    newa = a0 + j*dt;
}

void Stp7::calcjTracks(double t[], double j[], int length, double x0,
double v0, double a0, double &x, double &v, double &a) {
    x = x0; v = v0; a=a0;
    for (int i = 1; i <= length; i++) {
        calcjTrack(t[i]-t[i-1], x, v, a, j[i], x, v, a);
    }
}

void Stp7::calcjTracksTimeInt(double t[], double j[], int length,
             double x0, double v0, double a0, double &x, double &v, double &a) {
    x = x0; v = v0; a=a0;
    for (int i = 1; i <= length; i++) {
        calcjTrack(t[i], x, v, a, j[i], x, v, a);
    }
}    

void Stp7::move(double t, double &x, double &v, double &a, double &j) const {
    if (!_plannedProfile)
        throw invalid_argument("Consider to call planFastestProfile(.) first.");
    if (t < 0)
        throw invalid_argument("Negative time.");
    int i = getPhaseIndex(t);
    if (i==7) {
        x = _x[7];
        v = a = j = 0;
    } else {
        Stp7::calcjTrack(t-_t[i], _x[i], _v[i], _a[i], _j[i+1], x, v, a);
        j = _j[i+1];
    }
}

double Stp7::pos(double t) const {
    double x, v, a, j;
    move(t, x, v, a, j);
    return x;
}

double Stp7::vel(double t) const {
    double x, v, a, j;
    move(t, x, v, a, j);
    return v;
}

double Stp7::acc(double t) const {
    double x, v, a, j;
    move(t, x, v, a, j);
    return a;
}

double Stp7::jer(double t) const {
    double x, v, a, j;
    move(t, x, v, a, j);
    return j;
}

std::string Stp7::toString() const {
    std::ostringstream oss;
    if (_plannedProfile) {
        if (_bIsddec) oss << "double decceleration ";
        else oss << "canonical ";
        oss << getProfileType() << " ";
        if (_bHasCruise) oss << "with ";
        else oss << " without ";
        oss << "cruising phase (t=";
        writedArrayToStream(oss, _t, 1, 7);
        oss << ", j=";
        writedArrayToStream(oss, _j, 1, 7);
        oss << ", x0 = " << _x[0] << ", xTarget = " << _x[7] << ", v0 = ";
        oss << _v[0] << ", vmax = " << _vmax << ", a0 = " << _a[0];
        oss << ", amax = " << _amax << ")";
    } else {
        oss << "unplanned profile";
    }
    return oss.str();
}

std::ostream& operator<<(std::ostream& os, const Stp7& c) {
    os << c.toString();
    return os;
}
