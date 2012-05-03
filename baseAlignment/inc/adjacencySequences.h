/*
 * Copyright (C) 2009-2011 by Benedict Paten (benedictpaten@gmail.com)
 *
 * Released under the MIT license, see LICENSE.txt
 */

/*
 * adjacencySequences.h
 *
 *  Created on: 1 Jul 2010
 *      Author: benedictpaten
 */

#ifndef ADJACENCYSEQUENCES_H_
#define ADJACENCYSEQUENCES_H_

#include "cactus.h"
#include "sonLib.h"

/*
 * Datastructure to hold adjacency sequence.
 */
typedef struct _AdjacencySequence {
        char *string;
        int64_t subsequenceIdentifier;
        bool strand;
        int32_t start;
        int32_t length;
} AdjacencySequence;

/*
 * Gets an adjacency sequence struct for the given adjacency from the cap.
 */
AdjacencySequence *adjacencySequence_construct(Cap *cap, int32_t maxLength);

/*
 * Destructs the adjacency sequence.
 */
void adjacencySequence_destruct(AdjacencySequence *subSequence);


#endif /* ADJACENCYSEQUENCES_H_ */
