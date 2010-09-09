import unittest
import sys
import os
import xml.etree.ElementTree as ET

from cactus.shared.test import parseCactusSuiteTestOptions
from sonLib.bioio import TestStatus
from sonLib.bioio import system

from cactus.shared.test import getCactusInputs_random
from cactus.shared.test import getCactusInputs_blanchette
from cactus.shared.test import runWorkflow_multipleExamples

from sonLib.misc import sonTraceRootPath
from sonLib.bioio import getTempFile

class TestCase(unittest.TestCase):
    def testCactus_Random_Greedy(self):
        testCactus_Random(self, "greedy")
        
    def testCactus_Random_Blossum(self):
        testCactus_Random(self, "blossom")
        
    def testCactus_Random_Edmonds(self):
        testCactus_Random(self, "edmonds")
        
    def testCactus_Blanchette_Greedy(self):
        testCactus_Blanchette(self, "greedy")
        
    def testCactus_Blanchette_Blossum(self):
        testCactus_Blanchette(self, "blossom")
        
    def testCactus_Blanchette_Edmonds(self):
        testCactus_Blanchette(self, "edmonds")
        
    def testCuTest(self):
        system("referenceTests")
            
def testCactus_Blanchette(self, matchingAlgorithm):
    configFile = getConfigFile(matchingAlgorithm)
    runWorkflow_multipleExamples(getCactusInputs_blanchette, 
                                 testRestrictions=(TestStatus.TEST_SHORT,), inverseTestRestrictions=True, 
                                 buildTrees=False, buildFaces=False, buildReference=True,
                                 configFile=configFile)

def testCactus_Random(self, matchingAlgorithm):
    configFile = getConfigFile(matchingAlgorithm)
    runWorkflow_multipleExamples(getCactusInputs_random, 
                                 testNumber=TestStatus.getTestSetup(), 
                                 buildTrees=False, buildFaces=False, buildReference=True,
                                 configFile=configFile)
    
def getConfigFile(matchingAlgorithm="greedy"):
    tempConfigFile = getTempFile(rootDir="./", suffix=".xml")
    config = ET.parse(os.path.join(sonTraceRootPath(), "src", "cactus", "pipeline", "cactus_workflow_config.xml")).getroot()
    #Set the matching algorithm
    config.find("reference").attrib["matching_algorithm"] = matchingAlgorithm
    #Now print the file..
    fileHandle = open(tempConfigFile, 'w')
    ET.ElementTree(config).write(fileHandle)
    fileHandle.close()
    return tempConfigFile
    
def main():
    parseCactusSuiteTestOptions()
    sys.argv = sys.argv[:1]
    unittest.main()
        
if __name__ == '__main__':
    main()