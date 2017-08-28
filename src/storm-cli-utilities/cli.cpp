#include "cli.h"


#include "storm/utility/resources.h"
#include "storm/utility/file.h"
#include "storm/utility/storm-version.h"
#include "storm/utility/macros.h"

#include "storm/utility/initialize.h"
#include "storm/utility/Stopwatch.h"

#include <type_traits>

#include "storm-cli-utilities/model-handling.h"


// Includes for the linked libraries and versions header.
#ifdef STORM_HAVE_INTELTBB
#	include "tbb/tbb_stddef.h"
#endif
#ifdef STORM_HAVE_GLPK
#	include "glpk.h"
#endif
#ifdef STORM_HAVE_GUROBI
#	include "gurobi_c.h"
#endif
#ifdef STORM_HAVE_Z3
#	include "z3.h"
#endif
#ifdef STORM_HAVE_MSAT
#   include "mathsat.h"
#endif
#ifdef STORM_HAVE_CUDA
#include <cuda.h>
#include <cuda_runtime.h>
#endif

#ifdef STORM_HAVE_SMTRAT
#include "lib/smtrat.h"
#endif

namespace storm {
    namespace cli {
        
        int64_t process(const int argc, const char** argv) {
            storm::utility::setUp();
            storm::cli::printHeader("Storm", argc, argv);
            storm::settings::initializeAll("Storm", "storm");

            storm::utility::Stopwatch totalTimer(true);
            if (!storm::cli::parseOptions(argc, argv)) {
                return -1;
            }

            processOptions();

            totalTimer.stop();
            if (storm::settings::getModule<storm::settings::modules::ResourceSettings>().isPrintTimeAndMemorySet()) {
                storm::cli::printTimeAndMemoryStatistics(totalTimer.getTimeInMilliseconds());
            }

            storm::utility::cleanUp();
            return 0;
        }
        
        
        void printHeader(std::string const& name, const int argc, const char* argv[]) {
            STORM_PRINT(name << " " << storm::utility::StormVersion::shortVersionString() << std::endl << std::endl);
            
            // "Compute" the command line argument string with which storm was invoked.
            std::stringstream commandStream;
            for (int i = 1; i < argc; ++i) {
                commandStream << argv[i] << " ";
            }
            
            std::string command = commandStream.str();
            
            if (!command.empty()) {
                STORM_PRINT("Command line arguments: " << commandStream.str() << std::endl);
                STORM_PRINT("Current working directory: " << storm::utility::cli::getCurrentWorkingDirectory() << std::endl << std::endl);
            }
        }
        
        void printVersion(std::string const& name) {
            STORM_PRINT(storm::utility::StormVersion::longVersionString() << std::endl);
            STORM_PRINT(storm::utility::StormVersion::buildInfo() << std::endl);
            
#ifdef STORM_HAVE_INTELTBB
            STORM_PRINT("Linked with Intel Threading Building Blocks v" << TBB_VERSION_MAJOR << "." << TBB_VERSION_MINOR << " (Interface version " << TBB_INTERFACE_VERSION << ")." << std::endl);
#endif
#ifdef STORM_HAVE_GLPK
            STORM_PRINT("Linked with GNU Linear Programming Kit v" << GLP_MAJOR_VERSION << "." << GLP_MINOR_VERSION << "." << std::endl);
#endif
#ifdef STORM_HAVE_GUROBI
            STORM_PRINT("Linked with Gurobi Optimizer v" << GRB_VERSION_MAJOR << "." << GRB_VERSION_MINOR << "." << GRB_VERSION_TECHNICAL << "." << std::endl);
#endif
#ifdef STORM_HAVE_Z3
            unsigned int z3Major, z3Minor, z3BuildNumber, z3RevisionNumber;
            Z3_get_version(&z3Major, &z3Minor, &z3BuildNumber, &z3RevisionNumber);
            STORM_PRINT("Linked with Microsoft Z3 Optimizer v" << z3Major << "." << z3Minor << " Build " << z3BuildNumber << " Rev " << z3RevisionNumber << "." << std::endl);
#endif
#ifdef STORM_HAVE_MSAT
            char* msatVersion = msat_get_version();
            STORM_PRINT("Linked with " << msatVersion << "." << std::endl);
            msat_free(msatVersion);
#endif
#ifdef STORM_HAVE_SMTRAT
            STORM_PRINT("Linked with SMT-RAT " << SMTRAT_VERSION << "." << std::endl);
#endif
#ifdef STORM_HAVE_CARL
            // TODO get version string
            STORM_PRINT("Linked with CArL." << std::endl);
#endif
            
#ifdef STORM_HAVE_CUDA
            int deviceCount = 0;
            cudaError_t error_id = cudaGetDeviceCount(&deviceCount);
            
            if (error_id == cudaSuccess) {
                STORM_PRINT("Compiled with CUDA support, ");
                // This function call returns 0 if there are no CUDA capable devices.
                if (deviceCount == 0){
                    STORM_PRINT("but there are no available device(s) that support CUDA." << std::endl);
                } else {
                    STORM_PRINT("detected " << deviceCount << " CUDA capable device(s):" << std::endl);
                }
                
                int dev, driverVersion = 0, runtimeVersion = 0;
                
                for (dev = 0; dev < deviceCount; ++dev) {
                    cudaSetDevice(dev);
                    cudaDeviceProp deviceProp;
                    cudaGetDeviceProperties(&deviceProp, dev);
                    
                    STORM_PRINT("CUDA device " << dev << ": \"" << deviceProp.name << "\"" << std::endl);
                    
                    // Console log
                    cudaDriverGetVersion(&driverVersion);
                    cudaRuntimeGetVersion(&runtimeVersion);
                    STORM_PRINT("  CUDA Driver Version / Runtime Version          " << driverVersion / 1000 << "." << (driverVersion % 100) / 10 << " / " << runtimeVersion / 1000 << "." << (runtimeVersion % 100) / 10 << std::endl);
                    STORM_PRINT("  CUDA Capability Major/Minor version number:    " << deviceProp.major << "." << deviceProp.minor << std::endl);
                }
                STORM_PRINT(std::endl);
            }
            else {
                STORM_PRINT("Compiled with CUDA support, but an error occured trying to find CUDA devices." << std::endl);
            }
#endif
        }
        
        bool parseOptions(const int argc, const char* argv[]) {
            try {
                storm::settings::mutableManager().setFromCommandLine(argc, argv);
            } catch (storm::exceptions::OptionParserException& e) {
                storm::settings::manager().printHelp();
                throw e;
                return false;
            }
            
            storm::settings::modules::GeneralSettings const& general = storm::settings::getModule<storm::settings::modules::GeneralSettings>();

            bool result = true;
            if (general.isHelpSet()) {
                storm::settings::manager().printHelp(storm::settings::getModule<storm::settings::modules::GeneralSettings>().getHelpModuleName());
                result = false;
            }
            
            if (general.isVersionSet()) {
                printVersion("storm");
                result = false;;
            }

            return result;
        }

        void setResourceLimits() {
            storm::settings::modules::ResourceSettings const& resources = storm::settings::getModule<storm::settings::modules::ResourceSettings>();
            
            // If we were given a time limit, we put it in place now.
            if (resources.isTimeoutSet()) {
                storm::utility::resources::setCPULimit(resources.getTimeoutInSeconds());
            }
        }
        
        void setLogLevel() {
            storm::settings::modules::GeneralSettings const& general = storm::settings::getModule<storm::settings::modules::GeneralSettings>();
            storm::settings::modules::DebugSettings const& debug = storm::settings::getModule<storm::settings::modules::DebugSettings>();
            
            if (general.isVerboseSet()) {
                storm::utility::setLogLevel(l3pp::LogLevel::INFO);
            }
            if (debug.isDebugSet()) {
                storm::utility::setLogLevel(l3pp::LogLevel::DEBUG);
            }
            if (debug.isTraceSet()) {
                storm::utility::setLogLevel(l3pp::LogLevel::TRACE);
            }
            if (debug.isLogfileSet()) {
                storm::utility::initializeFileLogging();
            }
        }
        
        void setFileLogging() {
            storm::settings::modules::DebugSettings const& debug = storm::settings::getModule<storm::settings::modules::DebugSettings>();
            if (debug.isLogfileSet()) {
                storm::utility::initializeFileLogging();
            }
        }
        
        void setUrgentOptions() {
            setResourceLimits();
            setLogLevel();
            setFileLogging();
        }

        
        void processOptions() {
            // Start by setting some urgent options (log levels, resources, etc.)
            setUrgentOptions();
            
            // Parse and preprocess symbolic input (PRISM, JANI, properties, etc.)
            SymbolicInput symbolicInput = parseAndPreprocessSymbolicInput();
            
            auto generalSettings = storm::settings::getModule<storm::settings::modules::GeneralSettings>();
            if (generalSettings.isParametricSet()) {
#ifdef STORM_HAVE_CARL
                processInputWithValueType<storm::RationalFunction>(symbolicInput);
#else
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "No parameters are supported in this build.");
#endif
            } else if (generalSettings.isExactSet()) {
#ifdef STORM_HAVE_CARL
                processInputWithValueType<storm::RationalNumber>(symbolicInput);
#else
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "No exact numbers are supported in this build.");
#endif
            } else {
                processInputWithValueType<double>(symbolicInput);
            }
        }

        void printTimeAndMemoryStatistics(uint64_t wallclockMilliseconds) {
            struct rusage ru;
            getrusage(RUSAGE_SELF, &ru);
            
            std::cout << std::endl << "Performance statistics:" << std::endl;
#ifdef MACOS
            // For Mac OS, this is returned in bytes.
            uint64_t maximumResidentSizeInMegabytes = ru.ru_maxrss / 1024 / 1024;
#endif
#ifdef LINUX
            // For Linux, this is returned in kilobytes.
            uint64_t maximumResidentSizeInMegabytes = ru.ru_maxrss / 1024;
#endif
            std::cout << "  * peak memory usage: " << maximumResidentSizeInMegabytes << "MB" << std::endl;
            char oldFillChar = std::cout.fill('0');
            std::cout << "  * CPU time: " << ru.ru_utime.tv_sec << "." << std::setw(3) << ru.ru_utime.tv_usec/1000 << "s" << std::endl;
            if (wallclockMilliseconds != 0) {
                std::cout << "  * wallclock time: " << (wallclockMilliseconds/1000) << "." << std::setw(3) << (wallclockMilliseconds % 1000) << "s" << std::endl;
            }
            std::cout.fill(oldFillChar);
        }
        
    }
}
