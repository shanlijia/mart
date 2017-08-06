/**
 * -==== Mart-Selection.cpp
 *
 *                Mart Multi-Language LLVM Mutation Framework
 *
 * This file is distributed under the University of Illinois Open Source
 * License. See LICENSE.TXT for details.
 *
 * \brief     Main source file that implement Mutants Selections
 */

#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>  //mkdir, stat
#include <sys/types.h> //mkdir, stat
//#include <libgen.h> //basename
//#include <unistd.h>  // fork, execl
//#include <sys/wait.h> //wait

#include "../lib/mutantsSelection/MutantSelection.h"
#include "ReadWriteIRObj.h"

#include "llvm/Support/CommandLine.h" //llvm::cl

#include "llvm/Support/FileSystem.h" //for llvm::sys::fs::directory_iterator
#include "llvm/Support/Path.h"       //for llvm::sys::path::filename

using namespace mart;
using namespace mart::selection;

#define TOOLNAME "Mart-Selection"
#include "tools_commondefs.h"

static const std::string selectionFolder("Selection.out");
static const std::string generalInfo("info");
static const std::string defaultFeaturesFilename("mutants-features.csv");
static const std::string
    defaultTrainedModel("trained-models/default-trained.model");
static std::stringstream loginfo;
static std::string outFile;

template <typename T>
void mutantListAsJsON(std::vector<std::vector<T>> const &lists,
                      std::string jsonName) {
  std::ofstream xxx(jsonName);
  if (xxx.is_open()) {
    xxx << "{\n";
    for (unsigned repet = 0, repeat_last = lists.size() - 1;
         repet <= repeat_last; ++repet) {
      xxx << "\t\"" << repet << "\": [";
      bool isNotFirst = false;
      for (T data : lists[repet]) {
        if (isNotFirst)
          xxx << ", ";
        else
          isNotFirst = true;
        xxx << data;
      }
      if (repet == repeat_last)
        xxx << "]\n";
      else
        xxx << "],\n";
    }
    xxx << "\n}\n";
    xxx.close();
  } else {
    llvm::errs() << "Unable to create info file:" << jsonName << "\n";
    assert(false);
  }
}

int main(int argc, char **argv) {
// Remove the option we don't want to display in help
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 5)
  llvm::StringMap<llvm::cl::Option *> optMap;
  llvm::cl::getRegisteredOptions(optMap);
#else
  llvm::StringMap<llvm::cl::Option *> &optMap =
      llvm::cl::getRegisteredOptions();
#endif
  for (auto &option : optMap) {
    auto optstr = option.getKey();
    if (!(optstr.startswith("help") || optstr.equals("version")))
      optMap[optstr]->setHiddenFlag(llvm::cl::Hidden);
  }

  llvm::cl::opt<std::string> martOutTopDir(
      llvm::cl::Positional, llvm::cl::Required,
      llvm::cl::desc("<mutation topdir or out topdir>"));
  llvm::cl::opt<std::string> mutantInfoJsonfile(
      "mutant-infos",
      llvm::cl::desc("Specify the mutants info JSON file. (useful if running "
                     "with topdir different that the one generated by Mart "
                     "mutation)"),
      llvm::cl::value_desc("filename"), llvm::cl::init(""));
  llvm::cl::opt<std::string> inputIRfile(
      "preprocessed-bc-file",
      llvm::cl::desc("specify the pathfile of the preprocessed BC file. "
                     "(useful if running with topdir different that the one "
                     "generated by Mart mutation)"),
      llvm::cl::value_desc("BC filename"), llvm::cl::init(""));
  llvm::cl::opt<bool> mut_dep_cache(
      "mutant-dep-cache",
      llvm::cl::desc("Enable caching of mutant dependence computation"));
  llvm::cl::opt<unsigned> numberOfRandomSelections(
      "rand-repeat-num",
      llvm::cl::desc("(optional) Specify the number of repetitions for random "
                     "selections: default is 100 times"),
      llvm::cl::init(100));
  llvm::cl::opt<std::string> smartSelectionTrainedModel(
      "smart-trained-model",
      llvm::cl::desc(
          "(optional) Specify the alternative to use for prediction for smart"),
      llvm::cl::init(""));
  llvm::cl::opt<bool> dumpMutantsFeaturesToCSV(
      "dump-features",
      llvm::cl::desc("(optional) enable dumping features to CSV file"));
  llvm::cl::opt<bool> disable_selection(
      "no-selection",
      llvm::cl::desc("(optional) Disable selection. useful when only want to "
                     "get the mutants features into a CSV file for training"));

  llvm::cl::SetVersionPrinter(printVersion);

  llvm::cl::ParseCommandLineOptions(argc, argv, "Mart Mutant Selection");

  time_t totalRunTime = time(NULL);
  clock_t curClockTime;

  char *mutantDependencyJsonfile = nullptr;

  bool rundg = true;

  // unsigned numberOfRandomSelections = 100;  //How many time do we repead
  // random

  if (mut_dep_cache)
    mutantDependencyJsonfile = "mutantDependencies.cache.json";

  assert(llvm::sys::fs::is_directory(martOutTopDir) &&
         "Error: the topdir given do not exist!");

  if (mutantInfoJsonfile.empty()) { // try to get it from martOutTopDir
    mutantInfoJsonfile = martOutTopDir + "/" + mutantsInfosFileName;
    // make sure it exists
    assert(llvm::sys::fs::is_regular_file(mutantInfoJsonfile) &&
           "The specified topdir do not contain mutantInfofile and none was "
           "specified");
  } else {
    assert(llvm::sys::fs::is_regular_file(mutantInfoJsonfile) &&
           "The specified mutantInfofile do not exist");
  }
  if (inputIRfile.empty()) { // try to get it from martOutTopDir: end with .bc
                             // and is neither .WM.bc or .preTCE.MetaMu.bc, or
                             // .MetaMu.bc
    std::error_code ec;
#if (LLVM_VERSION_MAJOR <= 3) && (LLVM_VERSION_MINOR < 9)
    llvm::sys::fs::directory_iterator dit(martOutTopDir, ec);
#else
    llvm::sys::fs::directory_iterator dit(martOutTopDir, ec,
                                          false /*no symlink*/);
#endif
    llvm::sys::fs::directory_iterator de;
    for (; dit != de; dit.increment(ec)) {
      assert(!ec && "failed to list topdir when looking for input BC");
      llvm::StringRef fpath = dit->path();
      if (fpath.endswith(commonIRSuffix)) {
        if (!fpath.endswith(wmOutIRFileSuffix) &&
            !fpath.endswith(preTCEMetaIRFileSuffix) &&
            !fpath.endswith(metaMuIRFileSuffix)) {
          assert(inputIRfile.empty() && "multiple preprocessed IRs in the "
                                        "specified topdir. Please specify one");
          inputIRfile.assign(martOutTopDir + "/" +
                             llvm::sys::path::filename(fpath).str());
        }
      }
    }
    assert(llvm::sys::fs::is_regular_file(inputIRfile) &&
           "The specified topdir does not contain preprocessed BC file and "
           "none was specified");
  } else {
    assert(llvm::sys::fs::is_regular_file(inputIRfile) &&
           "The specified input BC file do not exist");
  }

  llvm::Module *moduleM;
  std::unique_ptr<llvm::Module> _M;

  // Read IR into moduleM
  /// llvm::LLVMContext context;
  if (!ReadWriteIRObj::readIR(inputIRfile, _M))
    return 1;
  moduleM = _M.get();
  // ~

  if (smartSelectionTrainedModel.empty()) {
    smartSelectionTrainedModel.assign(getUsefulAbsPath(argv[0]) + "/" +
                                      defaultTrainedModel);
  }

  MutantInfoList mutantInfo;
  mutantInfo.loadFromJsonFile(mutantInfoJsonfile);

  std::string outDir(martOutTopDir);
  outDir = outDir + "/" + selectionFolder;
  struct stat st;
  if (stat(outDir.c_str(), &st) == -1) // do not exists
  {
    if (mkdir(outDir.c_str(), 0777) != 0)
      assert(false && "Failed to create output directory");
  } else {
    if (!disable_selection)
      llvm::errs() << "Mart-Selection@Warning: Overriding existing...\n";
  }

  std::string mutDepCacheName;
  if (mutantDependencyJsonfile) {
    mutDepCacheName = outDir + "/" + mutantDependencyJsonfile;
    if (stat(mutDepCacheName.c_str(), &st) != -1) // exists
    {
      rundg = false;
    } else {
      rundg = true;
    }
  } else {
    rundg = true;
  }

  llvm::outs() << "Computing mutant dependencies...\n";
  curClockTime = clock();
  MutantSelection selection(*moduleM, mutantInfo, mutDepCacheName, rundg,
                            false /*is flow-sensitive?*/, disable_selection);
  llvm::outs() << "Mart@Progress: dependencies construction took: "
               << (float)(clock() - curClockTime) / CLOCKS_PER_SEC
               << " Seconds.\n";
  loginfo << "Mart@Progress: dependencies construction took: "
          << (float)(clock() - curClockTime) / CLOCKS_PER_SEC << " Seconds.\n";

  if (dumpMutantsFeaturesToCSV) {
    selection.dumpMutantsFeaturesToCSV(outDir + "/" + defaultFeaturesFilename);
  }

  if (disable_selection) {
    if (!dumpMutantsFeaturesToCSV)
      llvm::errs() << "Mart@Warning: Nothing Done because selection and dump "
                      "features disabled\n";
  } else {
    std::string smartSelectionOutJson = outDir + "/" + "smartSelection.json";
    std::string mlOnlySelectionOutJson = outDir + "/" + "mlOnlySelection.json";
    // std::string scoresForSmartSelectionOutJson =
    //    outDir + "/" + "scoresForSmartSelection.json";
    std::string randomSDLelectionOutJson =
        outDir + "/" + "randomSDLSelection.json";
    std::string spreadRandomSelectionOutJson =
        outDir + "/" + "spreadRandomSelection.json";
    std::string dummyRandomSelectionOutJson =
        outDir + "/" + "dummyRandomSelection.json";

    std::vector<std::vector<MutantIDType>> selectedMutants1, selectedMutants2;
    unsigned long number = 0;

    llvm::outs() << "Doing Smart Selection...\n";
    curClockTime = clock();
    selectedMutants1.clear();
    selectedMutants1.resize(numberOfRandomSelections);
    // std::vector<double> selectedScores;
    // to make experiment faster XXX: Note to take this in consideration when
    // measuring the algorithm time
    std::vector<float> cachedPrediction;
    for (unsigned si = 0; si < numberOfRandomSelections; ++si) {
      selection.smartSelectMutants(selectedMutants1[si], cachedPrediction,
                                   smartSelectionTrainedModel,
                                   false /*mlOnly*/);
    }
    mutantListAsJsON<MutantIDType>(selectedMutants1, smartSelectionOutJson);
    // mutantListAsJsON<double>(std::vector<std::vector<double>>({selectedScores}),
    //                         scoresForSmartSelectionOutJson);
    llvm::outs() << "Mart@Progress: smart selection took: "
                 << (float)(clock() - curClockTime) / CLOCKS_PER_SEC
                 << " Seconds.\n";
    loginfo << "Mart@Progress: smart selection took: "
            << (float)(clock() - curClockTime) / CLOCKS_PER_SEC
            << " Seconds.\n";

    number = selectedMutants1.back().size();
    assert(number == mutantInfo.getMutantsNumber() &&
           "The number of mutants mismatch. Bug in Selection function!");

    llvm::outs() << "Doing ML Only Selection...\n";
    curClockTime = clock();
    selectedMutants1.clear();
    selectedMutants1.resize(numberOfRandomSelections);
    // std::vector<double> selectedScores;
    // to make experiment faster XXX: Note to take this in consideration when
    // measuring the algorithm time
    for (unsigned si = 0; si < numberOfRandomSelections; ++si) {
      selection.smartSelectMutants(selectedMutants1[si], cachedPrediction,
                                   smartSelectionTrainedModel, true /*mlOnly*/);
    }
    mutantListAsJsON<MutantIDType>(selectedMutants1, mlOnlySelectionOutJson);
    llvm::outs() << "Mart@Progress: ML Only selection took: "
                 << (float)(clock() - curClockTime) / CLOCKS_PER_SEC
                 << " Seconds.\n";
    loginfo << "Mart@Progress: ML Only selection took: "
            << (float)(clock() - curClockTime) / CLOCKS_PER_SEC
            << " Seconds.\n";

    llvm::outs() << "Doing dummy and spread random selection...\n";
    curClockTime = clock();
    selectedMutants1.clear();
    selectedMutants2.clear();
    selectedMutants1.resize(numberOfRandomSelections);
    selectedMutants2.resize(numberOfRandomSelections);
    for (unsigned si = 0; si < numberOfRandomSelections; ++si)
      selection.randomMutants(selectedMutants1[si], selectedMutants2[si],
                              number);
    mutantListAsJsON<MutantIDType>(selectedMutants1,
                                   spreadRandomSelectionOutJson);
    mutantListAsJsON<MutantIDType>(selectedMutants2,
                                   dummyRandomSelectionOutJson);
    llvm::outs() << "Mart@Progress: dummy and spread random took: "
                 << (float)(clock() - curClockTime) / CLOCKS_PER_SEC
                 << " Seconds. (" << numberOfRandomSelections
                 << " repetitions)\n";
    loginfo << "Mart@Progress: dummy and spread random took: "
            << (float)(clock() - curClockTime) / CLOCKS_PER_SEC << " Seconds. ("
            << numberOfRandomSelections << " repetitions)\n";

    llvm::outs()
        << "Doing random SDL selection...\n"; // select only SDL mutants
    curClockTime = clock();
    selectedMutants1.clear();
    selectedMutants1.resize(numberOfRandomSelections);
    for (unsigned si = 0; si < numberOfRandomSelections; ++si)
      selection.randomSDLMutants(selectedMutants1[si], number);
    mutantListAsJsON<MutantIDType>(selectedMutants1, randomSDLelectionOutJson);
    llvm::outs() << "Mart@Progress: random SDL took: "
                 << (float)(clock() - curClockTime) / CLOCKS_PER_SEC
                 << " Seconds. (" << numberOfRandomSelections
                 << " repetitions)\n";
    loginfo << "Mart@Progress: random SDL took: "
            << (float)(clock() - curClockTime) / CLOCKS_PER_SEC << " Seconds. ("
            << numberOfRandomSelections << " repetitions)\n";

    std::ofstream xxx(outDir + "/" + generalInfo);
    if (xxx.is_open()) {
      xxx << loginfo.str();
      xxx.close();
    } else {
      llvm::errs() << "Unable to create info file:"
                   << outDir + "/" + generalInfo << "\n\n";
      assert(false);
    }
  }
  return 0;
}
