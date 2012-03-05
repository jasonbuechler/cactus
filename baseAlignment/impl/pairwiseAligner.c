/*
 * diagonalIterator.c
 *
 *  Created on: 1 Mar 2012
 *      Author: benedictpaten
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

#include "sonLib.h"
#include "pairwiseAligner.h"
#include "pairwiseAlignment.h"

///////////////////////////////////
///////////////////////////////////
//Diagonal
//
//Structure for working with x-y diagonal of dp matrix
///////////////////////////////////
///////////////////////////////////

const char *PAIRWISE_ALIGNMENT_EXCEPTION_ID = "PAIRWISE_ALIGNMENT_EXCEPTION";

Diagonal diagonal_construct(int32_t xay, int32_t xmyL, int32_t xmyR) {
    if ((xay + xmyL) % 2 != 0 || (xay + xmyR) % 2 != 0 || xmyL > xmyR) {
        stThrowNew(PAIRWISE_ALIGNMENT_EXCEPTION_ID,
                "Attempt to create diagonal with invalid coordinates: xay %i xmyL %i xmyR %i", xay, xmyL, xmyR);
    }
    Diagonal diagonal;
    diagonal.xay = xay;
    diagonal.xmyL = xmyL;
    diagonal.xmyR = xmyR;
    assert(xmyL <= xmyR);
    assert(xay >= 0);
    return diagonal;
}

inline int32_t diagonal_getXay(Diagonal diagonal) {
    return diagonal.xay;
}

inline int32_t diagonal_getMinXmy(Diagonal diagonal) {
    return diagonal.xmyL;
}

inline int32_t diagonal_getMaxXmy(Diagonal diagonal) {
    return diagonal.xmyR;
}

inline int32_t diagonal_getWidth(Diagonal diagonal) {
    return diagonal.xmyR - diagonal.xmyL + 1;
}

inline int32_t diagonal_getXCoordinate(int32_t xay, int32_t xmy) {
    assert((xay + xmy) % 2 == 0);
    return (xay + xmy) / 2;
}

inline int32_t diagonal_equals(Diagonal diagonal1, Diagonal diagonal2) {
    return diagonal1.xay == diagonal2.xay && diagonal1.xmyL == diagonal2.xmyL && diagonal1.xmyR == diagonal2.xmyR;
}

inline int32_t diagonal_getYCoordinate(int32_t xay, int32_t xmy) {
    assert((xay - xmy) % 2 == 0);
    return (xay - xmy) / 2;
}

inline char *diagonal_getString(Diagonal diagonal) {
    return stString_print("Diagonal, xay: %i xmyL %i, xmyR: %i", diagonal_getXay(diagonal),
            diagonal_getMinXmy(diagonal), diagonal_getMaxXmy(diagonal));
}

///////////////////////////////////
///////////////////////////////////
//Band Iterator
//
//Iterator for walking along x+y diagonals in banded fashion
//(using a set of anchor constraints)
///////////////////////////////////
///////////////////////////////////

struct _bandIterator {
    stListIterator *anchorPairsIt; //Iterator over input anchor pairs
    int32_t xay, lX, lY, expansion;
    int32_t xU, yU, xL, yL;
    int32_t pxay, pxmy;
    int32_t nxay, nxmy;
};

int32_t boundCoordinate(int32_t z, int32_t lZ) {
    return z < 0 ? 0 : (z > lZ ? lZ : z);
}

static void bandIterator_setBandCoordinates(BandIterator *bandIterator) {
    bandIterator->xL
            = boundCoordinate(
                    diagonal_getXCoordinate(bandIterator->pxay, bandIterator->pxmy - bandIterator->expansion),
                    bandIterator->lX);
    bandIterator->yL
            = boundCoordinate(
                    diagonal_getYCoordinate(bandIterator->nxay, bandIterator->nxmy - bandIterator->expansion),
                    bandIterator->lY);
    bandIterator->xU
            = boundCoordinate(
                    diagonal_getXCoordinate(bandIterator->nxay, bandIterator->nxmy + bandIterator->expansion),
                    bandIterator->lX);
    bandIterator->yU
            = boundCoordinate(
                    diagonal_getYCoordinate(bandIterator->pxay, bandIterator->pxmy + bandIterator->expansion),
                    bandIterator->lY);
}

BandIterator *bandIterator_construct(stList *anchorPairs, int32_t lX, int32_t lY, int32_t expansion) {
    BandIterator *bandIterator = st_malloc(sizeof(BandIterator));
    bandIterator->anchorPairsIt = stList_getIterator(anchorPairs);
    bandIterator->lX = lX;
    bandIterator->lY = lY;
    if (expansion % 2 != 0) {
        stThrowNew(PAIRWISE_ALIGNMENT_EXCEPTION_ID, "Attempt to specify non-even expansion coordinate: %i ", expansion);
    }
    bandIterator->expansion = expansion;
    bandIterator->pxay = 0;
    bandIterator->pxmy = 0;
    bandIterator->nxay = 0;
    bandIterator->nxmy = 0;
    bandIterator->xay = 0;
    bandIterator_setBandCoordinates(bandIterator); //This establishes the coordinates.
    return bandIterator;
}

BandIterator *bandIterator_clone(BandIterator *bandIterator) {
    BandIterator *bandIterator2 = st_malloc(sizeof(BandIterator));
    memcpy(bandIterator2, bandIterator, sizeof(BandIterator));
    bandIterator2->anchorPairsIt = stList_copyIterator(bandIterator->anchorPairsIt);
    return bandIterator2;
}

void bandIterator_destruct(BandIterator *bandIterator) {
    stList_destructIterator(bandIterator->anchorPairsIt);
    free(bandIterator);
}

static int32_t bandIterator_avoidOffByOne(int32_t xay, int32_t xmy) {
    return (xay + xmy) % 2 == 0 ? xmy : xmy + 1;
}

static void bandIterator_setCurrentDiagonalP(int32_t *xmy, int32_t i, int32_t j, int32_t k) {
    if (i < j) {
        *xmy += 2 * (j - i) * k;
    }
}

static Diagonal bandIterator_setCurrentDiagonal(BandIterator *bandIterator) {
    Diagonal diagonal;
    diagonal.xay = bandIterator->xay;
    diagonal.xmyL = bandIterator->xL - bandIterator->yL;
    diagonal.xmyR = bandIterator->xU - bandIterator->yU;

    //Avoid in-between undefined x,y coordinate positions when intersecting xay and xmy.
    diagonal.xmyL = bandIterator_avoidOffByOne(diagonal.xay, diagonal.xmyL);
    diagonal.xmyR = bandIterator_avoidOffByOne(diagonal.xay, diagonal.xmyR);

    //Bound the xmy coordinates by the x and y boundaries
    bandIterator_setCurrentDiagonalP(&diagonal.xmyL, diagonal_getXCoordinate(diagonal.xay, diagonal.xmyL),
            bandIterator->xL, 1);
    bandIterator_setCurrentDiagonalP(&diagonal.xmyL, bandIterator->yL,
            diagonal_getYCoordinate(diagonal.xay, diagonal.xmyL), 1);
    bandIterator_setCurrentDiagonalP(&diagonal.xmyR, bandIterator->xU,
            diagonal_getXCoordinate(diagonal.xay, diagonal.xmyR), -1);
    bandIterator_setCurrentDiagonalP(&diagonal.xmyR, diagonal_getYCoordinate(diagonal.xay, diagonal.xmyR),
            bandIterator->yU, -1);

    return diagonal;
}

Diagonal bandIterator_getNext(BandIterator *bandIterator) {
    Diagonal diagonal = bandIterator_setCurrentDiagonal(bandIterator);
    if (bandIterator->xay < bandIterator->lX + bandIterator->lY) {
        if (bandIterator->nxay == bandIterator->xay++) {
            //The previous diagonals become the next
            bandIterator->pxay = bandIterator->nxay;
            bandIterator->pxmy = bandIterator->nxmy;

            int32_t x = bandIterator->lX, y = bandIterator->lY;
            stIntTuple *anchorPair = stList_getNext(bandIterator->anchorPairsIt);
            if (anchorPair != NULL) { //There is another anchor point
                x = stIntTuple_getPosition(anchorPair, 0) + 1; //Plus ones, because matrix coordinates are +1 the sequence ones
                y = stIntTuple_getPosition(anchorPair, 1) + 1;
                if (x + y == bandIterator->nxay) { //This is required because mixture of nexts and previous calls can cause the anchor iterator to get out of sync.
                    assert(bandIterator->nxmy == x - y);
                    anchorPair = stList_getNext(bandIterator->anchorPairsIt);
                    if (anchorPair != NULL) {
                        x = stIntTuple_getPosition(anchorPair, 0) + 1; //Plus ones, because matrix coordinates are +1 the sequence ones
                        y = stIntTuple_getPosition(anchorPair, 1) + 1;
                    }
                    else {
                        x = bandIterator->lX;
                        y = bandIterator->lY;
                    }
                }
            }
            bandIterator->nxay = x + y;
            bandIterator->nxmy = x - y;

            //Now call to set the lower and upper x,y coordinates
            bandIterator_setBandCoordinates(bandIterator);
        }
    }
    return diagonal;
}

Diagonal bandIterator_getPrevious(BandIterator *bandIterator) {
    //Function just inverts steps of bandIterator_getNext.
    Diagonal diagonal = bandIterator_setCurrentDiagonal(bandIterator);

    if (bandIterator->xay > 0) { //Coordinates of diagonal do not change when we hit 0 == xay;
        if (bandIterator->pxay == bandIterator->xay--) { //We've reach the x+y diagonal of the previous anchor pair
            //The next diagonals become the previous
            bandIterator->nxay = bandIterator->pxay;
            bandIterator->nxmy = bandIterator->pxmy;

            stIntTuple *anchorPair = stList_getPrevious(bandIterator->anchorPairsIt);
            int32_t x = 0, y = 0;
            if (anchorPair != NULL) { //There is another anchor point
                x = stIntTuple_getPosition(anchorPair, 0) + 1; //Plus ones, because matrix coordinates are +1 the sequence ones
                y = stIntTuple_getPosition(anchorPair, 1) + 1;
                if (x + y == bandIterator->pxay) { //This is required because mixture of nexts and previous calls can cause the anchor iterator to get out of sync.
                    assert(bandIterator->pxmy == x - y);
                    anchorPair = stList_getPrevious(bandIterator->anchorPairsIt);
                    if (anchorPair != NULL) {
                        x = stIntTuple_getPosition(anchorPair, 0) + 1; //Plus ones, because matrix coordinates are +1 the sequence ones
                        y = stIntTuple_getPosition(anchorPair, 1) + 1;
                    }
                    else {
                        x = 0;
                        y = 0;
                    }
                }
            }
            bandIterator->pxay = x + y;
            bandIterator->pxmy = x - y;

            //Now call to set the lower and upper x,y coordinates
            bandIterator_setBandCoordinates(bandIterator);
        }
    }

    return diagonal;
}

///////////////////////////////////
///////////////////////////////////
//Log Add functions
//
//Interpolation function for doing log add
///////////////////////////////////
///////////////////////////////////

#define logUnderflowThreshold 7.5
#define posteriorMatchThreshold 0.01

static inline double lookup(double x) {
    //return log (exp (x) + 1);
#ifdef BEN_DEBUG
    assert (x >= 0.00f);
    assert (x <= logUnderflowThreshold);
#endif
    if (x <= 1.00f)
        return ((-0.009350833524763f * x + 0.130659527668286f) * x + 0.498799810682272f) * x + 0.693203116424741f;
    if (x <= 2.50f)
        return ((-0.014532321752540f * x + 0.139942324101744f) * x + 0.495635523139337f) * x + 0.692140569840976f;
    if (x <= 4.50f)
        return ((-0.004605031767994f * x + 0.063427417320019f) * x + 0.695956496475118f) * x + 0.514272634594009f;
    return ((-0.000458661602210f * x + 0.009695946122598f) * x + 0.930734667215156f) * x + 0.168037164329057f;
}

double logAdd(double x, double y) {
    if (x < y)
        return (x == LOG_ZERO || y - x >= logUnderflowThreshold) ? y : lookup(y - x) + x;
    return (y == LOG_ZERO || x - y >= logUnderflowThreshold) ? x : lookup(x - y) + y;
}

///////////////////////////////////
///////////////////////////////////
//Symbols
//
//Emissions probs/functions to convert to symbol sequence
///////////////////////////////////
///////////////////////////////////

Symbol symbol_convertCharToSymbol(char i) {
    switch (i) {
        case 'A':
        case 'a':
            return a;
        case 'C':
        case 'c':
            return c;
        case 'G':
        case 'g':
            return g;
        case 'T':
        case 't':
            return t;
        default:
            return n;
    }
}

Symbol *symbol_convertStringToSymbols(const char *s, int32_t sL) {
    assert(sL >= 0);
    assert(strlen(s) == sL);
    Symbol *cS = st_malloc(sL * sizeof(Symbol));
    for (int32_t i = 0; i < sL; i++) {
        cS[i] = symbol_convertCharToSymbol(s[i]);
    }
    return cS;
}

#define EMISSION_MATCH -2.1149196655034745 //log(0.12064298095701059);
#define EMISSION_TRANSVERSION -4.5691014376830479 //log(0.010367271172731285);
#define EMISSION_TRANSITION -3.9833860032220842 //log(0.01862247669752685);
#define EMISSION_MATCH_N -3.2188758248682006 //log(0.04);
double symbol_matchProb(Symbol cX, Symbol cY) {
    //Symmetric matrix of transition probabilities.
    static const double matchM[25] = { EMISSION_MATCH, EMISSION_TRANSVERSION, EMISSION_TRANSITION,
            EMISSION_TRANSVERSION, EMISSION_MATCH_N, EMISSION_TRANSVERSION, EMISSION_MATCH, EMISSION_TRANSVERSION,
            EMISSION_TRANSITION, EMISSION_MATCH_N, EMISSION_TRANSITION, EMISSION_TRANSVERSION, EMISSION_MATCH,
            EMISSION_TRANSVERSION, EMISSION_MATCH_N, EMISSION_TRANSVERSION, EMISSION_TRANSITION, EMISSION_TRANSVERSION,
            EMISSION_MATCH, EMISSION_MATCH_N, EMISSION_MATCH_N, EMISSION_MATCH_N, EMISSION_MATCH_N, EMISSION_MATCH_N,
            EMISSION_MATCH_N };
    return matchM[cX * 5 + cY];
}

double symbol_gapProb(Symbol cZ) {
    return -1.6094379124341003; //log(0.2) = -1.6094379124341003
}

///////////////////////////////////
///////////////////////////////////
//State
//
//A cell is a set of states associated with an x, y coordinate.
//These functions do the state transitions for the pairwise
//alignment model.
///////////////////////////////////
///////////////////////////////////

double state_startStateProb(State state) {
    static const double startStates[5] = { TRANSITION_MATCH_CONTINUE, TRANSITION_GAP_SHORT_OPEN,
            TRANSITION_GAP_SHORT_OPEN, TRANSITION_GAP_LONG_OPEN, TRANSITION_GAP_LONG_OPEN };
    return startStates[state];
}

double state_endStateProb(State state) {
    return -1.6094379124341; //math.log(1.0/5.0) = -1.6094379124341
}

///////////////////////////////////
///////////////////////////////////
//Cell calculations
//
//A cell is a set of states associated with an x, y coordinate.
//These functions do the forward/backward calculations for the pairwise
//alignment model.
///////////////////////////////////
///////////////////////////////////

static inline void cell_calculate(double *current, double *lower, double *middle, double *upper, Symbol cX, Symbol cY,
        void(*doTransition)(double *, double *, int32_t, int32_t, double, double)) {
    if (lower != NULL) {
        double eP = symbol_gapProb(cX);
        doTransition(lower, current, match, shortGapX, eP, TRANSITION_GAP_SHORT_OPEN);
        doTransition(lower, current, shortGapX, shortGapX, eP, TRANSITION_GAP_SHORT_EXTEND);
        doTransition(lower, current, shortGapY, shortGapX, eP, TRANSITION_GAP_SHORT_SWITCH);
        doTransition(lower, current, match, longGapX, eP, TRANSITION_GAP_LONG_OPEN);
        doTransition(lower, current, longGapX, longGapX, eP, TRANSITION_GAP_LONG_EXTEND);
    }
    if (middle != NULL) {
        double eP = symbol_matchProb(cX, cY);
        doTransition(middle, current, match, match, eP, TRANSITION_MATCH_CONTINUE);
        doTransition(middle, current, shortGapX, match, eP, TRANSITION_MATCH_FROM_SHORT_GAP);
        doTransition(middle, current, shortGapY, match, eP, TRANSITION_MATCH_FROM_SHORT_GAP);
        doTransition(middle, current, longGapX, match, eP, TRANSITION_MATCH_FROM_LONG_GAP);
        doTransition(middle, current, longGapY, match, eP, TRANSITION_MATCH_FROM_LONG_GAP);
    }
    if (upper != NULL) {
        double eP = symbol_gapProb(cY);
        doTransition(lower, current, match, shortGapY, eP, TRANSITION_GAP_SHORT_OPEN);
        doTransition(lower, current, shortGapY, shortGapY, eP, TRANSITION_GAP_SHORT_EXTEND);
        doTransition(lower, current, shortGapX, shortGapY, eP, TRANSITION_GAP_SHORT_SWITCH);
        doTransition(lower, current, match, longGapY, eP, TRANSITION_GAP_LONG_OPEN);
        doTransition(lower, current, longGapY, longGapY, eP, TRANSITION_GAP_LONG_EXTEND);
    }
}

static inline void doTransitionForward(double *fromCells, double *toCells, int32_t from, int32_t to, double eP,
        double tP) {
    toCells[to] = logAdd(toCells[to], fromCells[from] + eP + tP);
}

void cell_calculateForward(double *current, double *lower, double *middle, double *upper, Symbol cX, Symbol cY) {
    cell_calculate(current, lower, middle, upper, cX, cY, doTransitionForward);
}

static inline void doTransitionBackward(double *fromCells, double *toCells, int32_t from, int32_t to, double eP,
        double tP) {
    fromCells[from] = logAdd(fromCells[from], toCells[to] + eP + tP);
}

void cell_calculateBackward(double *current, double *lower, double *middle, double *upper, Symbol cX, Symbol cY) {
    cell_calculate(current, upper, middle, lower, cX, cY, doTransitionBackward);
}

///////////////////////////////////
///////////////////////////////////
//DpDiagonal
//
//Structure for storing a x-y diagonal of the dp matrix
///////////////////////////////////
///////////////////////////////////

struct _dpDiagonal {
    Diagonal diagonal;
    double *cells;
};

DpDiagonal *dpDiagonal_construct(Diagonal diagonal) {
    DpDiagonal *dpDiagonal = st_malloc(sizeof(DpDiagonal));
    dpDiagonal->diagonal = diagonal;
    dpDiagonal->cells = st_malloc(sizeof(double) * STATE_NUMBER * diagonal_getWidth(diagonal));
    return dpDiagonal;
}

DpDiagonal *dpDiagonal_clone(DpDiagonal *diagonal) {
    DpDiagonal *diagonal2 = dpDiagonal_construct(diagonal->diagonal);
    memcpy(diagonal2->cells, diagonal->cells, sizeof(double) * diagonal_getWidth(diagonal->diagonal) * STATE_NUMBER);
    return diagonal2;
}

void dpDiagonal_destruct(DpDiagonal *dpDiagonal) {
    free(dpDiagonal->cells);
    free(dpDiagonal);
}

double *dpDiagonal_getCell(DpDiagonal *dpDiagonal, int32_t xmy) {
    if (dpDiagonal->diagonal.xmyL < xmy || dpDiagonal->diagonal.xmyR > xmy) {
        return NULL;
    }
    return &dpDiagonal->cells[(xmy - dpDiagonal->diagonal.xmyL) * STATE_NUMBER];
}

double dpDiagonal_sum(DpDiagonal *diagonal) {
    double totalProbability = LOG_ZERO;
    for (int32_t i = diagonal_getWidth(diagonal->diagonal) * STATE_NUMBER; i >= 0; i--) {
        totalProbability += logAdd(totalProbability, diagonal->cells[i]);
    }
    return totalProbability;
}

void dpDiagonal_zeroValues(DpDiagonal *diagonal) {
    for (int32_t i = diagonal_getWidth(diagonal->diagonal) * STATE_NUMBER; i >= 0; i--) {
        diagonal->cells[i] = LOG_ZERO;
    }
}

void dpDiagonal_initialiseValues(DpDiagonal *diagonal, double(*getStateValue)(State)) {
    for (int32_t i = diagonal_getWidth(diagonal->diagonal); i >= 0; i--) {
        double *cell = dpDiagonal_getCell(diagonal, i);
        assert(cell != NULL);
        for (int32_t j = 0; j < STATE_NUMBER; j++) {
            cell[j] = getStateValue(j);
        }
    }
}

///////////////////////////////////
///////////////////////////////////
//DpMatrix
//
//Structure for storing dp-matrix
///////////////////////////////////
///////////////////////////////////

struct _dpMatrix {
    DpDiagonal **diagonals;
    int32_t diagonalNumber;
    int32_t activeDiagonals;
};

DpMatrix *dpMatrix_construct(int32_t lX, int32_t lY) {
    DpMatrix *dpMatrix = st_malloc(sizeof(DpMatrix));
    dpMatrix->diagonalNumber = lX + lY;
    dpMatrix->diagonals = st_calloc(dpMatrix->diagonalNumber, sizeof(DpDiagonal *));
    dpMatrix->activeDiagonals = 0;
    return dpMatrix;
}

void dpMatrix_destruct(DpMatrix *dpMatrix) {
    assert(dpMatrix->activeDiagonals == 0);
    free(dpMatrix->diagonals);
    free(dpMatrix);
}

DpDiagonal *dpMatrix_getDiagonal(DpMatrix *dpMatrix, int32_t xay) {
    if (xay < 0 || xay >= dpMatrix->diagonalNumber) {
        return NULL;
    }
    return dpMatrix->diagonals[xay];
}

int32_t dpMatrix_getActiveDiagonalNumber(DpMatrix *dpMatrix) {
    return dpMatrix->activeDiagonals;
}

DpDiagonal *dpMatrix_createDiagonal(DpMatrix *dpMatrix, Diagonal diagonal) {
    assert(diagonal.xay >= 0);
    assert(diagonal.xay < dpMatrix->diagonalNumber);
    DpDiagonal *dpDiagonal = dpDiagonal_construct(diagonal);
    dpMatrix->diagonals[diagonal_getXay(diagonal)] = dpDiagonal;
    dpMatrix->activeDiagonals++;
    return dpDiagonal;
}

void dpMatrix_deleteDiagonal(DpMatrix *dpMatrix, int32_t xay) {
    assert(xay >= 0);
    assert(xay < dpMatrix->diagonalNumber);
    if (dpMatrix->diagonals[xay] != NULL) {
        dpMatrix->activeDiagonals--;
        assert(dpMatrix->activeDiagonals >= 0);
        dpDiagonal_destruct(dpMatrix->diagonals[xay]);
        dpMatrix->diagonals[xay] = NULL;
    }
}

///////////////////////////////////
///////////////////////////////////
//Diagonal DP Calculations
//
//Functions which do forward/backward/posterior calculations
//between diagonal rows of a dp-matrix
///////////////////////////////////
///////////////////////////////////

static Symbol getXCharacter(const Symbol *sX, int32_t lX, int32_t xay, int32_t xmy) {
    int32_t x = diagonal_getXCoordinate(xmy, xay);
    assert(x <= lX);
    return x > 0 ? sX[x - 1] : 'N';
}

static Symbol getYCharacter(const Symbol *sY, int32_t lY, int32_t xay, int32_t xmy) {
    int32_t y = diagonal_getYCoordinate(xmy, xay);
    assert(y <= lY);
    return y > 0 ? sY[y - 1] : 'N';
}

static void diagonalCalculation(DpDiagonal *dpDiagonal, DpMatrix *dpMatrix, const Symbol *sX, const Symbol *sY,
        int32_t lX, int32_t lY, void(*cellCalculation)(double *, double *, double *, double *, Symbol, Symbol)) {
    Diagonal diagonal = dpDiagonal->diagonal;
    assert(dpDiagonal != NULL);
    DpDiagonal *dpDiagonalM1 = dpMatrix_getDiagonal(dpMatrix, diagonal_getXay(diagonal) - 1);
    assert(dpDiagonalM1 != NULL);
    DpDiagonal *dpDiagonalM2 = dpMatrix_getDiagonal(dpMatrix, diagonal_getXay(diagonal) - 2);

    int32_t xmy = diagonal_getMinXmy(diagonal) - 1;
    while (++xmy <= diagonal_getMaxXmy(diagonal)) {
        char x = getXCharacter(sX, lX, diagonal_getXay(diagonal), xmy);
        char y = getYCharacter(sY, lY, diagonal_getXay(diagonal), xmy);
        double *current = dpDiagonal_getCell(dpDiagonal, xmy);
        double *lower = dpDiagonal_getCell(dpDiagonalM1, xmy - 1);
        double *middle = dpDiagonalM2 == NULL ? NULL : dpDiagonal_getCell(dpDiagonalM2, xmy);
        double *upper = dpDiagonal_getCell(dpDiagonalM1, xmy + 1);
        cellCalculation(current, lower, middle, upper, x, y);
    }
}

void diagonalCalculationForward(Diagonal diagonal, DpMatrix *dpMatrix, const Symbol *sX, const Symbol *sY, int32_t lX,
        int32_t lY) {
    diagonalCalculation(dpMatrix_getDiagonal(dpMatrix, diagonal_getXay(diagonal)), dpMatrix, sX, sY, lX, lY,
            cell_calculateForward);
}

void diagonalCalculationBackward(Diagonal diagonal, DpMatrix *dpMatrix, const Symbol *sX, const Symbol *sY, int32_t lX,
        int32_t lY) {
    diagonalCalculation(dpMatrix_getDiagonal(dpMatrix, diagonal_getXay(diagonal)), dpMatrix, sX, sY, lX, lY,
            cell_calculateBackward);
}

void diagonalCalculationPosterior(Diagonal diagonal, DpMatrix *forwardDpMatrix, DpMatrix *backwardDpMatrix,
        const Symbol *sX, const Symbol *sY, int32_t lX, int32_t lY, double threshold, stList *alignedPairs) {
    //Copy backward diagonal
    DpDiagonal *posteriorDiagonal = dpDiagonal_clone(dpMatrix_getDiagonal(backwardDpMatrix, diagonal_getXay(diagonal)));
    //Do forward calc
    diagonalCalculation(posteriorDiagonal, forwardDpMatrix, sX, sY, lX, lY, cell_calculateForward);
    //Sum diagonal
    double totalProbability = dpDiagonal_sum(posteriorDiagonal);

    //Do posteriors
    int32_t xmy = diagonal_getMinXmy(diagonal) - 1;
    while (++xmy <= diagonal_getMaxXmy(diagonal)) {
        double *cell = dpDiagonal_getCell(posteriorDiagonal, xmy);
        double posteriorProbability = exp(cell[match] - totalProbability);
        if (posteriorProbability >= threshold) {
            if (posteriorProbability > 1.0) {
                posteriorProbability = 1.0;
            }
            int32_t x = diagonal_getXCoordinate(xmy, diagonal_getXay(diagonal));
            int32_t y = diagonal_getYCoordinate(xmy, diagonal_getXay(diagonal));
            assert(x > 0 && x <= lX);
            assert(y > 0 && y <= lY);
            stList_append(
                    alignedPairs,
                    stIntTuple_construct(3, (int32_t) floor(posteriorProbability * PAIR_ALIGNMENT_PROB_1), x - 1, y - 1));
        }
    }
    //Cleanup
    dpDiagonal_destruct(posteriorDiagonal);
}

///////////////////////////////////
///////////////////////////////////
//Banded alignment routine to calculate posterior match probs
//
//
///////////////////////////////////
///////////////////////////////////

stList *getAlignedPairsWithBanding(stList *anchorPairs, const Symbol *sX, const Symbol *sY, const int32_t lX,
        const int32_t lY, PairwiseAlignmentParameters *p) {
    //Prerequisites
    assert(p->traceBackDiagonals >= 2);
    assert(p->threshold >= 0.0);
    assert(p->threshold <= 1.0);
    assert(p->diagonalExpansion >= 0);
    assert(p->minDiagsBetweenTraceBack >= 1);
    assert(p->traceBackDiagonals < p->minDiagsBetweenTraceBack);

    //This list of pairs to be returned. Not in any order, but points must be unique
    stList *alignedPairs = stList_construct3(0, (void(*)(void *)) stIntTuple_destruct);

    //Primitives for the forward matrix recursion
    BandIterator *forwardBandIterator = bandIterator_construct(anchorPairs, lX, lY, p->diagonalExpansion);
    DpMatrix *forwardDpMatrix = dpMatrix_construct(lX, lY);
    dpDiagonal_initialiseValues(dpMatrix_createDiagonal(forwardDpMatrix, diagonal_construct(0, 0, 0)),
            state_startStateProb); //Initialise forward matrix.

    //Backward matrix.
    DpMatrix *backwardDpMatrix = dpMatrix_construct(lX, lY);

    int32_t tracedBackTo = 0;
    while (1) { //Loop that moves through the matrix forward
        Diagonal diagonal = bandIterator_getNext(forwardBandIterator);
        if (diagonal_getXay(diagonal) > lX + lY) { //Termination
            break;
        }

        //Forward calculation
        dpDiagonal_zeroValues(dpMatrix_createDiagonal(forwardDpMatrix, diagonal));
        diagonalCalculationForward(diagonal, forwardDpMatrix, sX, sY, lX, lY);

        bool atEnd = diagonal_getXay(diagonal) == lX + lY; //Condition true at the end of the matrix
        bool tracebackPoint = diagonal_getXay(diagonal) >= p->minDiagsBetweenTraceBack && diagonal_getWidth(diagonal)
                <= p->diagonalExpansion * 2 + 1; //Condition true when we want to do an intermediate traceback.

        //Traceback
        if (atEnd || tracebackPoint) {
            //Initialise the last row (until now) of the backward matrix to represent an end point
            dpDiagonal_initialiseValues(dpMatrix_createDiagonal(backwardDpMatrix, diagonal), state_endStateProb);
            if (diagonal_getXay(diagonal) > tracedBackTo + 1) {
                DpDiagonal *j = dpMatrix_getDiagonal(forwardDpMatrix, diagonal_getXay(diagonal) - 1);
                assert(j != NULL);
                dpDiagonal_zeroValues(dpMatrix_createDiagonal(backwardDpMatrix, j->diagonal));
            }

            BandIterator *backwardBandIterator = bandIterator_clone(forwardBandIterator);

            //Do walk back
            Diagonal diagonal2 = bandIterator_getPrevious(backwardBandIterator);
            assert(diagonal_getXay(diagonal2) == diagonal_getXay(diagonal));
            int32_t tracedBackFrom = diagonal_getXay(diagonal2);
            int32_t remainingTraceBackDiagonals = atEnd ? 0 : p->traceBackDiagonals;
            while (diagonal_getXay(diagonal2) > tracedBackTo) {
                //Create the earlier diagonal
                if (diagonal_getXay(diagonal2) > tracedBackTo + 2) {
                    DpDiagonal *j = dpMatrix_getDiagonal(forwardDpMatrix, diagonal_getXay(diagonal2) - 2);
                    assert(j != NULL);
                    dpDiagonal_zeroValues(dpMatrix_createDiagonal(backwardDpMatrix, j->diagonal));
                }
                if (diagonal_getXay(diagonal2) > tracedBackTo + 1) {
                    diagonalCalculationBackward(diagonal2, backwardDpMatrix, sX, sY, lX, lY);
                }
                if (remainingTraceBackDiagonals-- <= 0) {
                    diagonalCalculationPosterior(diagonal2, forwardDpMatrix, backwardDpMatrix, sX, sY, lX, lY,
                            p->threshold, alignedPairs);
                    dpMatrix_deleteDiagonal(forwardDpMatrix, diagonal_getXay(diagonal2) - 1); //Delete forward diagonal after last access in posterior calculation
                }
                dpMatrix_deleteDiagonal(backwardDpMatrix, diagonal_getXay(diagonal2)); //Delete backward diagonal after last access in backward calculation

                diagonal2 = bandIterator_getPrevious(backwardBandIterator);
            }
            tracedBackTo = tracedBackFrom;
            bandIterator_destruct(backwardBandIterator);
            //Check memory state.
            assert(dpMatrix_getActiveDiagonalNumber(backwardDpMatrix) == 0);
            if (!atEnd) {
                assert(dpMatrix_getActiveDiagonalNumber(forwardDpMatrix) == p->traceBackDiagonals + 1);
            }
        }
    }
    assert(tracedBackTo == lX + lY);
    //Check memory
    assert(dpMatrix_getActiveDiagonalNumber(forwardDpMatrix) == 1);
    dpMatrix_deleteDiagonal(forwardDpMatrix, lX + lY);
    assert(dpMatrix_getActiveDiagonalNumber(forwardDpMatrix) == 0);
    dpMatrix_destruct(forwardDpMatrix);
    dpMatrix_destruct(backwardDpMatrix);
    bandIterator_destruct(forwardBandIterator);

    return alignedPairs;
}

///////////////////////////////////
///////////////////////////////////
//Blast anchoring functions
//
//Use lastz to get sets of anchors
///////////////////////////////////
///////////////////////////////////

static int sortByXPlusYCoordinate(const void *i, const void *j) {
    int64_t k = stIntTuple_getPosition((stIntTuple *) i, 0) + stIntTuple_getPosition((stIntTuple *) i, 1);
    int64_t l = stIntTuple_getPosition((stIntTuple *) j, 0) + stIntTuple_getPosition((stIntTuple *) j, 1);
    return k > l ? 1 : (k < l ? -1 : 0);
}

static char *makeUpperCase(const char *s, int32_t l) {
    char *s2 = stString_copy(s);
    for (int32_t i = 0; i < l; i++) {
        s2[i] = toupper(s[i]);
    }
    return s2;
}

static void writeSequenceToFile(char *file, const char *name, const char *sequence) {
    FILE *fileHandle = fopen(file, "w");
    fprintf(fileHandle, ">%s\n%s\n", name, sequence);
    fclose(fileHandle);
}

stList *getBlastPairs(const char *sX, const char *sY, int32_t lX, int32_t lY, int32_t trim, bool repeatMask) {
    /*
     * Uses lastz to compute a bunch of monotonically increasing pairs such that for any pair of consecutive pairs in the list
     * (x1, y1) (x2, y2) in the set of aligned pairs x1 appears before x2 in X and y1 appears before y2 in Y.
     */
    stList *alignedPairs = stList_construct3(0, (void(*)(void *)) stIntTuple_destruct); //the list to put the output in

    if (lX == 0 || lY == 0) {
        return alignedPairs;
    }

    if (!repeatMask) {
        sX = makeUpperCase(sX, lX);
        sY = makeUpperCase(sY, lY);
    }

    //Write one sequence to file..
    char *tempFile1 = getTempFile();
    char *tempFile2 = NULL;

    writeSequenceToFile(tempFile1, "a", sX);

    char *command;

    if (lY > 10000) {
        tempFile2 = getTempFile();
        writeSequenceToFile(tempFile2, "b", sY);
        command = stString_print(
                "lastz --hspthresh=800 --chain --strand=plus --gapped --format=cigar --ambiguous=iupac %s %s",
                tempFile1, tempFile2);
    } else {
        command
                = stString_print(
                        "echo '>b\n%s\n' | lastz --hspthresh=800 --chain --strand=plus --gapped --format=cigar --ambiguous=iupac %s",
                        sY, tempFile1);
    }
    FILE *fileHandle = popen(command, "r");
    free(command);
    if (fileHandle == NULL) {
        st_errAbort("Problems with lastz pipe");
    }
    //Read from stream
    int32_t pxay = -1;
    struct PairwiseAlignment *pA;
    while ((pA = cigarRead(fileHandle)) != NULL) {
        int32_t j = pA->start1;
        int32_t k = pA->start2;

        assert(strcmp(pA->contig1, "a") == 0);
        assert(strcmp(pA->contig2, "b") == 0);
        assert(pA->strand1);
        assert(pA->strand2);

        for (int32_t i = 0; i < pA->operationList->length; i++) {
            struct AlignmentOperation *op = pA->operationList->list[i];
            if (op->opType == PAIRWISE_MATCH) {
                for (int32_t l = trim; l < op->length - trim; l++) {
                    int32_t x = j + l;
                    int32_t y = k + l;
                    if (x + y > pxay) {
                        stList_append(alignedPairs, stIntTuple_construct(2, j + l, k + l));
                        pxay = x + y;
                    }
                }
            }
            if (op->opType != PAIRWISE_INDEL_Y) {
                j += op->length;
            }
            if (op->opType != PAIRWISE_INDEL_X) {
                k += op->length;
            }
        }

        assert(j == pA->end1);
        assert(k == pA->end2);
        destructPairwiseAlignment(pA);
    }
    int32_t status = pclose(fileHandle);
    if (status != 0) {
        st_errnoAbort("pclose failed when getting rid of lastz pipe with value %i", status);
    }

    stList_sort(alignedPairs, sortByXPlusYCoordinate); //Ensure the coordinates are increasing

    //Remove old files
    st_system("rm %s", tempFile1);
    free(tempFile1);
    if (tempFile2 != NULL) {
        st_system("rm %s", tempFile2);
        free(tempFile2);
    }

    if (!repeatMask) {
        free((char *) sX);
        free((char *) sY);
    }

    return alignedPairs;
}

static char *getSubString(const char *cA, int32_t start, int32_t length) {
    char *cA2 = memcpy(st_malloc(sizeof(char) * (length + 1)), cA + start, length);
    cA2[length] = '\0';

#ifdef BEN_DEBUG
    for(int32_t i=0; i<length; i++) {
        assert(cA2[i] == cA[i + start]);
        assert(cA2[i] != '\0');
    }
    assert(cA2[length] == '\0');
#endif
    return cA2;
}

static void convertPairs(stList *alignedPairs2, int32_t offsetX, int32_t offsetY) {
    /*
     * Convert the coordinates of the computed pairs.
     */
    for (int32_t k = 0; k < stList_length(alignedPairs2); k++) {
        stIntTuple *i = stList_get(alignedPairs2, k);
        assert(stIntTuple_length(i) == 3);
        stList_set(
                alignedPairs2,
                k,
                stIntTuple_construct(3, stIntTuple_getPosition(i, 0), stIntTuple_getPosition(i, 1) + offsetX,
                        stIntTuple_getPosition(i, 2) + offsetY));
        stIntTuple_destruct(i);
    }
}

static void getBlastPairsForPairwiseAlignmentParametersP(const char *sX, const char *sY, int32_t pX, int32_t pY,
        int32_t x, int32_t y, PairwiseAlignmentParameters *p, stList *combinedAnchorPairs) {
    int32_t lX2 = x - pX;
    assert(lX2 >= 0);
    int32_t lY2 = y - pY;
    assert(lY2 >= 0);
    int64_t matrixSize = (int64_t) lX2 * lY2;
    if (matrixSize > p->repeatMaskMatrixBiggerThanThis) {
        char *sX2 = getSubString(sX, pX, lX2);
        char *sY2 = getSubString(sY, pY, lY2);
        stList *bottomLevelAnchorPairs = getBlastPairs(sX2, sY2, lX2, lY2, p->constraintDiagonalTrim, 0);
        convertPairs(bottomLevelAnchorPairs, pX, pY);
        free(sX2);
        free(sY2);
        stList_appendAll(combinedAnchorPairs, bottomLevelAnchorPairs);
        stList_setDestructor(bottomLevelAnchorPairs, NULL);
        stList_destruct(bottomLevelAnchorPairs);
    }
}

stList *getBlastPairsForPairwiseAlignmentParameters(const char *sX, const char *sY, const int32_t lX, const int32_t lY,
        PairwiseAlignmentParameters *p) {
    if ((int64_t) lX * lY <= p->anchorMatrixBiggerThanThis) {
        return stList_construct();
    }
    stList *topLevelAnchorPairs = getBlastPairs(sX, sY, lX, lY, p->constraintDiagonalTrim, 1);
    int32_t pX = 0;
    int32_t pY = 0;
    stList *combinedAnchorPairs = stList_construct3(0, (void(*)(void *)) stIntTuple_destruct);
    for (int32_t i = 0; i < stList_length(topLevelAnchorPairs); i++) {
        stIntTuple *anchorPair = stList_get(topLevelAnchorPairs, i);
        int32_t x = stIntTuple_getPosition(anchorPair, 0);
        int32_t y = stIntTuple_getPosition(anchorPair, 1);
        getBlastPairsForPairwiseAlignmentParametersP(sX, sY, pX, pY, x, y, p, combinedAnchorPairs);
        stList_append(combinedAnchorPairs, anchorPair);
        pX = x + 1;
        pY = y + 1;
    }
    getBlastPairsForPairwiseAlignmentParametersP(sX, sY, pX, pY, lX, lY, p, combinedAnchorPairs);
    stList_setDestructor(topLevelAnchorPairs, NULL);
    stList_destruct(topLevelAnchorPairs);
    return combinedAnchorPairs;
}

///////////////////////////////////
///////////////////////////////////
//Split large gap functions
//
//Functions to split up alignment around gaps in the anchors that are too large.
///////////////////////////////////
///////////////////////////////////

static void getSplitPointsP(int32_t pX, int32_t pY, int32_t x, int32_t y, stList *splitPoints,
        PairwiseAlignmentParameters *p) {
    int32_t lX2 = x - pX;
    int32_t lY2 = y = pY;
    int64_t matrixSize = (int64_t) lX2 * lY2;
    if (matrixSize > p->splitMatrixBiggerThanThis) {
        int32_t maxSequenceLength = sqrt(p->splitMatrixBiggerThanThis);
        int32_t hX = lX2 / 2 > maxSequenceLength ? maxSequenceLength : lX2 / 2;
        int32_t hY = lY2 / 2 > maxSequenceLength ? maxSequenceLength : lY2 / 2;
        stList_append(splitPoints, stIntTuple_construct(2, pX + hX + 1, pY + hY + 1));
        stList_append(splitPoints, stIntTuple_construct(2, x - hX, y - hY));
    }
}

stList *getSplitPoints(stList *anchorPairs, int32_t lX, int32_t lY, PairwiseAlignmentParameters *p) {
    int32_t pX = 0;
    int32_t pY = 0;
    stList *splitPoints = stList_construct3(0, (void(*)(void *)) stIntTuple_destruct);
    stList_append(splitPoints, stIntTuple_construct(2, 0, 0));
    for (int32_t i = 0; i < stList_length(anchorPairs); i++) {
        stIntTuple *anchorPair = stList_get(anchorPairs, i);
        int32_t x = stIntTuple_getPosition(anchorPair, 0);
        int32_t y = stIntTuple_getPosition(anchorPair, 1);
        getSplitPointsP(pX, pY, x, y, splitPoints, p);
        pX = x + 1;
        pY = y + 1;
    }
    getSplitPointsP(pX, pY, lX, lY, splitPoints, p);
    stList_append(splitPoints, stIntTuple_construct(2, lX, lY));
    return splitPoints;
}

stList *splitAlignmentsByLargeGaps(stList *anchorPairs, const char *sX, const char *sY, int32_t lX, int32_t lY,
        PairwiseAlignmentParameters *p) {
    stList *splitPoints = getSplitPoints(anchorPairs, lX, lY, p);
    stListIterator *anchorPairIterator = stList_getIterator(splitPoints);
    stList *alignedPairs = stList_construct3(0, (void(*)(void *)) stIntTuple_destruct);
    //Now to the actual alignments
    for (int32_t i = 0; i < stList_length(splitPoints); i += 2) {
        stIntTuple *from = stList_get(splitPoints, i);
        stIntTuple *to = stList_get(splitPoints, i + 1);
        int32_t x = stIntTuple_getPosition(from, 0);
        int32_t y = stIntTuple_getPosition(from, 1);
        int32_t lX2 = stIntTuple_getPosition(to, 0) - x;
        int32_t lY2 = stIntTuple_getPosition(to, 1) - y;
        char *sX2 = getSubString(sX, x, lX2);
        char *sY2 = getSubString(sY, y, lY2);
        stList *subListOfAnchorPoints = stList_construct();
        stIntTuple *anchorPair;
        while ((anchorPair = stList_getNext(anchorPairIterator)) != NULL) {
            int32_t xay = stIntTuple_getPosition(anchorPair, 0) + stIntTuple_getPosition(anchorPair, 1);
            assert(xay >= x + y);
            if (xay > x + y + lX2 + lY2) {
                stList_getPrevious(anchorPairIterator);
                break;
            }
            stList_append(subListOfAnchorPoints, anchorPair);
        }
        Symbol *sX3 = symbol_convertStringToSymbols(sX2, lX2);
        Symbol *sY3 = symbol_convertStringToSymbols(sY2, lY2);
        stList *subListOfAlignedPairs = getAlignedPairsWithBanding(subListOfAnchorPoints, sX3, sY3, lX2, lY2, p);
        stList_appendAll(alignedPairs, subListOfAlignedPairs);
        stList_setDestructor(subListOfAlignedPairs, NULL);
        stList_destruct(subListOfAlignedPairs);
        stList_destruct(subListOfAnchorPoints);
        free(sX2);
        free(sY2);
        free(sX3);
        free(sY3);
    }
    stList_destructIterator(anchorPairIterator);
    stList_destruct(splitPoints);
    return alignedPairs;
}

///////////////////////////////////
///////////////////////////////////
//Core public functions
///////////////////////////////////
///////////////////////////////////

PairwiseAlignmentParameters *pairwiseAlignmentBandingParameters_construct() {
    PairwiseAlignmentParameters *p = st_malloc(sizeof(PairwiseAlignmentParameters));
    p->threshold = 0.01;
    p->minDiagsBetweenTraceBack = 1000;
    p->traceBackDiagonals = 20;
    p->diagonalExpansion = 10;
    p->constraintDiagonalTrim = 5;
    p->anchorMatrixBiggerThanThis = 500 * 500;
    p->repeatMaskMatrixBiggerThanThis = 500 * 500;
    p->splitMatrixBiggerThanThis = 3000 * 3000;
    p->alignAmbiguityCharacters = 1;
    return p;
}

void pairwiseAlignmentBandingParameters_destruct(PairwiseAlignmentParameters *p) {
    free(p);
}

stList *getAlignedPairs(const char *sX, const char *sY, PairwiseAlignmentParameters *p) {
    const int32_t lX = strlen(sX);
    const int32_t lY = strlen(sY);
    stList *anchorPairs = getBlastPairsForPairwiseAlignmentParameters(sX, sY, lX, lY, p);
    stList *alignedPairs = splitAlignmentsByLargeGaps(anchorPairs, sX, sY, lX, lY, p);
    stList_destruct(anchorPairs);
    return alignedPairs;
}

