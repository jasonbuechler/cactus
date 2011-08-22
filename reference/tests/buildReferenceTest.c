/*
 * Copyright (C) 2009-2011 by Benedict Paten (benedictpaten@gmail.com)
 *
 * Released under the MIT license, see LICENSE.txt
 */

#include "CuTest.h"
#include "sonLib.h"
#include "cactusReference.h"

CuSuite* buildReferenceTestSuite(void) {
    CuSuite* suite = CuSuiteNew();
    return suite;
}
