
main {
  HEADERS += \
             $$MAIN_HH/cAnalyze.h \
             $$MAIN_HH/cAnalyzeUtil.h \
             $$MAIN_HH/avida.h \
             $$MAIN_HH/cBirthChamber.h \
             $$MAIN_HH/cCallbackUtil.h \
             $$MAIN_HH/cConfig.h \
             $$MAIN_HH/cEnvironment.h \
             $$MAIN_HH/cFitnessMatrix.h \
             $$MAIN_HH/cGenebank.h \
             $$MAIN_HH/cGenome.h \
             $$MAIN_HH/cGenomeUtil.h \
             $$MAIN_HH/cGenotype.h \
             $$MAIN_HH/cInjectGenotype.h \
             $$MAIN_HH/cInjectGenebank.h \
             $$MAIN_HH/cInstruction.h \
             $$MAIN_HH/cInstLibBase.h \
             $$MAIN_HH/cInstSet.h \
             $$MAIN_HH/cInstUtil.h \
             $$MAIN_HH/cLandscape.h \
             $$MAIN_HH/cLineage.h \
             $$MAIN_HH/cLineageControl.h \
             $$MAIN_HH/cOrganism.h \
             $$MAIN_HH/cPhenotype.h \
             $$MAIN_HH/cPopulationInterface.h \
             $$MAIN_HH/cPopulation.h \
             $$MAIN_HH/cPopulationCell.h \
             $$MAIN_HH/cReaction.h \
             $$MAIN_HH/cReactionResult.h \
             $$MAIN_HH/cResource.h \
             $$MAIN_HH/cResourceCount.h \
             $$MAIN_HH/cResourceLib.h \
             $$MAIN_HH/cSpecies.h \
             $$MAIN_HH/cStats.h

  SOURCES += \
             $$MAIN_CC/cAnalyze.cc \
             $$MAIN_CC/cAnalyzeUtil.cc \
             $$MAIN_CC/cAnalyzeGenotype.cc \
             $$MAIN_CC/avida.cc \
             $$MAIN_CC/cAvidaDriver_Analyze.cc \
             $$MAIN_CC/cAvidaDriver_Base.cc \
             $$MAIN_CC/cAvidaDriver_Population.cc \
             $$MAIN_CC/cBirthChamber.cc \
             $$MAIN_CC/cCallbackUtil.cc \
             $$MAIN_CC/cConfig.cc \
             $$MAIN_CC/cEnvironment.cc \
             $$MAIN_CC/cFitnessMatrix.cc \
             $$MAIN_CC/cGenebank.cc \
             $$MAIN_CC/cGenome.cc \
             $$MAIN_CC/cGenomeUtil.cc \
             $$MAIN_CC/cGenotype.cc \
             $$MAIN_CC/cGenotype_BirthData.cc \
             $$MAIN_CC/cGenotypeControl.cc \
             $$MAIN_CC/cGenotype_TestData.cc \
             $$MAIN_CC/cInstruction.cc \
             $$MAIN_CC/cInstSet.cc \
             $$MAIN_CC/cInstUtil.cc \
             $$MAIN_CC/cInjectGenebank.cc \
             $$MAIN_CC/cInjectGenotype.cc \
             $$MAIN_CC/cInjectGenotype_BirthData.cc \
             $$MAIN_CC/cInjectGenotypeControl.cc \
             $$MAIN_CC/cInjectGenotypeQueue.cc \
             $$MAIN_CC/cLandscape.cc \
             $$MAIN_CC/cLineage.cc \
             $$MAIN_CC/cLineageControl.cc \
             $$MAIN_CC/cLocalMutations.cc \
             $$MAIN_CC/cMutationLib.cc \
             $$MAIN_CC/cMutationRates.cc \
             $$MAIN_CC/cMutation.cc \
             $$MAIN_CC/cMxCodeArray.cc \
             $$MAIN_CC/cOrganism.cc \
             $$MAIN_CC/cPhenotype.cc \
             $$MAIN_CC/cPopulationInterface.cc \
             $$MAIN_CC/cPopulation.cc \
             $$MAIN_CC/cPopulationCell.cc \
             $$MAIN_CC/cReaction.cc \
             $$MAIN_CC/cReactionLib.cc \
             $$MAIN_CC/cReactionProcess.cc \
             $$MAIN_CC/cReactionRequisite.cc \
             $$MAIN_CC/cReactionResult.cc \
             $$MAIN_CC/cResource.cc \
             $$MAIN_CC/cResourceCount.cc \
             $$MAIN_CC/cResourceLib.cc \
             $$MAIN_CC/cSpatialCountElem.cc \
             $$MAIN_CC/cSpatialResCount.cc \
             $$MAIN_CC/cSpecies.cc \
             $$MAIN_CC/cSpeciesControl.cc \
             $$MAIN_CC/cSpeciesQueue.cc \
             $$MAIN_CC/cStats.cc \
             $$MAIN_CC/cTaskEntry.cc \
             $$MAIN_CC/cTaskLib.cc
}