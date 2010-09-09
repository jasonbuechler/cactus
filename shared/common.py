"""Setup functions for assisting in running the various programs of the cactus package.
"""

import os

from sonLib.bioio import logger
from sonLib.bioio import getTempFile
from sonLib.bioio import getTempDirectory
from sonLib.bioio import system
from sonLib.bioio import nameValue

from workflow.jobTree.jobTree import runJobTree 
from workflow.jobTree.jobTreeTest import runJobTreeStatusAndFailIfNotComplete

#############################################
#############################################
#All the following provide command line wrappers
#for core programs in the cactus pipeline.
#############################################
#############################################  

def runCactusSetup(cactusDiskDatabaseString, sequences, 
                   newickTreeString, logLevel="DEBUG", debug=False):
    debugString = nameValue("debug", debug, bool)
    system("cactus_setup %s --speciesTree '%s' --cactusDisk '%s' \
--logLevel %s %s" \
           % (" ".join(sequences), newickTreeString,
              cactusDiskDatabaseString, logLevel, debugString))
    logger.info("Ran cactus setup okay")
    
def runCactusAligner(cactusDiskDatabaseString, alignmentFile, tempDir, useDummy=True, flowerName="0", logLevel="DEBUG"):        
    """Runs job tree and fails if not complete.
    """
    tempDir = getTempDirectory(tempDir)
    jobTreeDir = os.path.join(tempDir, "jobTree")
    useDummy = nameValue("useDummy", useDummy, bool)
    command = "cactus_aligner.py --cactusDisk '%s' --flowerName %s \
--resultsFile %s %s --job JOB_FILE" % (cactusDiskDatabaseString, flowerName, alignmentFile, useDummy)
    runJobTree(command, jobTreeDir, logLevel=logLevel)
    runJobTreeStatusAndFailIfNotComplete(jobTreeDir)
    system("rm -rf %s" % tempDir)
    logger.info("Ran the cactus aligner okay")
            
def runCactusCore(cactusDiskDatabaseString, alignmentFile, 
                  flowerName=0,
                  logLevel="DEBUG", 
                  writeDebugFiles=False,
                  annealingRounds=False,
                  alignRepeatsAtRound=False,
                  trim=None,
                  trimChange=None,
                  minimumTreeCoverage=None,
                  minimumBlockLength=None,
                  minimumBlockLengthChange=None,
                  minimumChainLength=None,
                  minimumChainLengthChange=None,
                  deannealingRounds=None):
    writeDebugFiles = nameValue("writeDebugFiles", writeDebugFiles, bool)
    annealingRounds = nameValue("annealingRounds", annealingRounds, int)
    alignRepeatsAtRound = nameValue("alignRepeatsAtRound", alignRepeatsAtRound, int)
    trim = nameValue("trim", trim, int)
    trimChange = nameValue("trimChange", trimChange, float)
    minimumTreeCoverage = nameValue("minimumTreeCoverage", minimumTreeCoverage, float)
    minimumBlockLength = nameValue("minimumBlockLength", minimumBlockLength, int)
    minimumBlockLengthChange = nameValue("minimumBlockLengthChange", minimumBlockLengthChange, float)
    minimumChainLength = nameValue("minimumChainLength", minimumChainLength, int)
    minimumChainLengthChange = nameValue("minimumChainLengthChange", minimumChainLengthChange, float)
    deannealingRounds = nameValue("deannealingRounds", deannealingRounds, int)
    
    command = "cactus_core --cactusDisk '%s' --flowerName %s --alignments %s --logLevel %s %s %s %s %s %s %s %s %s %s %s %s" % \
    (cactusDiskDatabaseString, flowerName, alignmentFile, logLevel, writeDebugFiles, annealingRounds, alignRepeatsAtRound,
     trim, trimChange, minimumTreeCoverage, minimumBlockLength,
     minimumBlockLengthChange, minimumChainLength, minimumChainLengthChange, deannealingRounds)

    system(command)
    logger.info("Ran cactus_core okay")
    
def runCactusPhylogeny(cactusDiskDatabaseString,
                  flowerNames=("0",),
                  logLevel="DEBUG"):
    command = "cactus_phylogeny --cactusDisk '%s' --logLevel %s %s" % \
    (cactusDiskDatabaseString, logLevel, " ".join(flowerNames))
    system(command)
    logger.info("Ran cactus_phylogeny okay")
    
def runCactusAdjacencies(cactusDiskDatabaseString, flowerNames=("0",), logLevel="DEBUG"):
    command = "cactus_fillAdjacencies --cactusDisk '%s' --logLevel %s %s" %\
    (cactusDiskDatabaseString, logLevel, " ".join(flowerNames))
    system(command)
    logger.info("Ran cactus_fillAdjacencies OK")
    
def readFlowerNamesFile(flowerNamesFile):
    fileHandle = open(flowerNamesFile, 'r')
    line = fileHandle.readline()
    l = []
    while line != '':
        childFlowerName = line.split()[0]
        childFlowerSize = float(line.split()[1])
        l.append((childFlowerName, childFlowerSize))
        line = fileHandle.readline()
    fileHandle.close()
    return l
    
def runCactusGetFlowers(cactusDiskDatabaseString, flowerNames, tempDir):
    """Gets a list of flowers attached to the given flower. 
    """
    flowerNamesFile = getTempFile(".txt", tempDir)
    system("cactus_workflow_getFlowers '%s' %s %s" % (cactusDiskDatabaseString,  flowerNamesFile, " ".join(flowerNames)))
    l = readFlowerNamesFile(flowerNamesFile)
    os.remove(flowerNamesFile)
    return l

def runCactusExtendFlowers(cactusDiskDatabaseString, flowerName, tempDir,
                        minSizeToExtend=1):
    """Extends the terminal groups in the cactus and returns the list
    of their child flowers with which to pass to core.
    The order of the flowers is by ascending depth first discovery time.
    """
    flowerNamesFile = getTempFile(".txt", tempDir)
    system("cactus_workflow_extendFlowers '%s' %s %s %i" % (cactusDiskDatabaseString, flowerName, flowerNamesFile, int(minSizeToExtend)))
    l = readFlowerNamesFile(flowerNamesFile)
    os.remove(flowerNamesFile)
    return l

def runCactusGetUniqueName(cactusDiskDatabaseString, tempDir):
    """Gets a globally unique name.
    """
    uniqueNameFile = getTempFile(".txt", tempDir)
    system("cactus_workflow_getUniqueName '%s' %s" % (cactusDiskDatabaseString, uniqueNameFile))
    fileHandle = open(uniqueNameFile, 'r')
    nameString = fileHandle.readline()[:-1]
    os.remove(uniqueNameFile)
    return nameString

def runCactusMakeNormal(cactusDiskDatabaseString, flowerNames, maxNumberOfChains=0, logLevel="DEBUG"):
    """Makes the given flowers normal (see normalisation for the various phases)
    """
    system("cactus_normalisation --cactusDisk '%s' --maxNumberOfChains %i --logLevel %s %s" % (cactusDiskDatabaseString, maxNumberOfChains, logLevel, " ".join(flowerNames)))

def runCactusBaseAligner(cactusDiskDatabaseString, flowerNames, logLevel="DEBUG"):
    """Runs cactus base aligner.
    """
    system("cactus_baseAligner --cactusDisk '%s' --logLevel %s %s" % (cactusDiskDatabaseString, logLevel, " ".join(flowerNames)))
    
def runCactusReference(cactusDiskDatabaseString, flowerNames, logLevel="DEBUG", bottomUp=False,
                       matchingAlgorithm=None):
    """Runs cactus reference.
    """
    #print "running", "cactus_reference --cactusDisk '%s' --logLevel %s %s" % (cactusDiskDatabaseString, logLevel, " ".join(flowerNames))
    #assert False
    bottomUp = nameValue("bottomUp", bottomUp, bool)
    matchingAlgorithm = nameValue("matchingAlgorithm", matchingAlgorithm)
    system("cactus_reference --cactusDisk '%s' --logLevel %s %s %s %s" % (cactusDiskDatabaseString, logLevel, bottomUp, matchingAlgorithm, " ".join(flowerNames)))

def runCactusCheck(cactusDiskDatabaseString, 
                    flowerNames=("0",), 
                    logLevel="DEBUG", 
                    recursive=False):
    recursive = nameValue("recursive", recursive, bool)
    system("cactus_check --cactusDisk '%s' %s --logLevel %s %s"  % (cactusDiskDatabaseString, " ".join(flowerNames), logLevel, recursive))
    logger.info("Ran cactus check")
    
def runCactusWorkflow(experimentFile,
                      jobTreeDir, 
                      logLevel="DEBUG", retryCount=0, 
                      batchSystem="single_machine", 
                      rescueJobFrequency=None,
                      setupAndBuildAlignments=True,
                      buildTrees=True, buildFaces=True, buildReference=True):
    buildFaces=False
    setupAndBuildAlignments = nameValue("setupAndBuildAlignments", setupAndBuildAlignments, bool)
    buildTrees = nameValue("buildTrees", buildTrees, bool)
    buildFaces = nameValue("buildFaces", buildFaces, bool)
    buildReference = nameValue("buildReference", buildReference, bool)
    command = "cactus_workflow.py --experiment %s %s %s %s %s --job JOB_FILE" % \
            (experimentFile, setupAndBuildAlignments, buildTrees, buildFaces, buildReference)
    #print "going to run the command:", command
    #assert False
    runJobTree(command, jobTreeDir, logLevel, retryCount, batchSystem, rescueJobFrequency)
    logger.info("Ran the cactus workflow okay")
    
#############################################
#############################################
#Runs cactus utilities.
#############################################
#############################################    
    
def runCactusTreeStats(outputFile, cactusDiskDatabaseString, flowerName='0'):
    command = "cactus_treeStats --cactusDisk '%s' --flowerName %s --outputFile %s" % (cactusDiskDatabaseString, flowerName, outputFile)
    system(command)
    logger.info("Ran the cactus tree stats command apprently okay")

def runCactusTreeStatsToLatexTables(inputFiles, regionNames, outputFile):
    assert len(regionNames) == len(inputFiles)
    k = " ".join([ "%s %s" % (i, j) for i, j in zip(inputFiles, regionNames) ])
    command = "cactus_treeStatsToLatexTables.py --outputFile %s %s" % (outputFile, k)
    system(command)
    logger.info("Ran cactus_treeStatsToLatexTables okay")
    
def runCactusTreeViewer(graphFile,
                        cactusDiskDatabaseString, 
                        flowerName="0", 
                        logLevel="DEBUG"):
    system("cactus_treeViewer --cactusDisk '%s' --flowerName %s --outputFile %s --logLevel %s" \
                    % (cactusDiskDatabaseString, flowerName, graphFile, logLevel))
    logger.info("Created a cactus tree graph")
    
def runCactusAdjacencyGraphViewer(graphFile,
                             cactusDiskDatabaseString, flowerName="0",
                             logLevel="DEBUG", includeInternalAdjacencies=False):
    includeInternalAdjacencies = nameValue("includeInternalAdjacencies", includeInternalAdjacencies, bool)
    system("cactus_adjacencyGraphViewer --cactusDisk '%s' --flowerName %s --outputFile %s --logLevel %s" \
                    % (cactusDiskDatabaseString, flowerName, graphFile, logLevel))
    logger.info("Created a break point graph of the problem")
    
def runCactusReferenceGraphViewer(graphFile,
                                  cactusDiskDatabaseString, flowerName="0",
                                  logLevel="DEBUG"):
    system("cactus_referenceViewer --cactusDisk '%s' --flowerName %s --outputFile %s --logLevel %s" \
                    % (cactusDiskDatabaseString, flowerName, graphFile, logLevel))
    logger.info("Created a cactus reference graph")
    
def runCactusMAFGenerator(mAFFile, cactusDiskDatabaseString, flowerName="0",
                          logLevel="DEBUG"):
    system("cactus_MAFGenerator --cactusDisk '%s' --flowerName %s --outputFile %s --logLevel %s" \
            % (cactusDiskDatabaseString, flowerName, mAFFile, logLevel))
    logger.info("Created a MAF for the given cactusDisk")
