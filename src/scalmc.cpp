/*
 ScalMC

 Copyright (c) 2009-2015, Mate Soos. All rights reserved.
 Copyright (c) 2014, Supratik Chakraborty, Kuldeep S. Meel, Moshe Y. Vardi
 Copyright (c) 2015, Supratik Chakraborty, Daniel J. Fremont,
 Kuldeep S. Meel, Sanjit A. Seshia, Moshe Y. Vardi

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 */

#if defined(__GNUC__) && defined(__linux__)

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fenv.h>
#endif

#include <ctime>
#include <cstring>
#include <errno.h>
#include <string.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <set>
#include <fstream>
#include <sys/stat.h>
#include <string.h>
#include <list>
#include <array>
#include <cmath>
#include <complex>

#include "scalmc.h"
#include "time_mem.h"
#include "cryptominisat5/cryptominisat.h"
#include "cryptominisat5/dimacsparser.h"
#include "cryptominisat5/streambuffer.h"
#include "cryptominisat5/solvertypesmini.h"
#include "GitSHA1.h"

using std::cout;
using std::cerr;
using std::endl;
using boost::lexical_cast;
using std::list;
using std::map;

bool ScalMC::gen_rhs()
{
    std::uniform_int_distribution<uint32_t> dist{0, 1};
    bool rhs = dist(randomEngine);
    //cout << "rnd rhs:" << (int)rhs << endl;
    return rhs;
}

string ScalMC::GenerateRandomBits(const uint32_t size, const uint32_t num_hashes)
{
    string randomBits;
    std::uniform_int_distribution<uint32_t> dist{0, 1000};
    uint32_t cutoff = 500;
    if (sparse && num_hashes > 132) {
        double probability = 13.46*std::log(num_hashes)/num_hashes;
        assert(probability < 0.5);
        cutoff = std::ceil(1000.0*probability);
        cout << "[scalmc] sparse hashing used, cutoff: " << cutoff << endl;
    }

    while (randomBits.size() < size) {
        bool val = dist(randomEngine) < cutoff;
        randomBits += '0' + val;
    }
    assert(randomBits.size() >= size);

    //cout << "rnd bits: " << randomBits << endl;
    return randomBits;
}

void printVersionInfoScalMC()
{
    cout << "c ScalMC SHA revision " << ::get_version_sha1() << endl;
    cout << "c ScalMC compilation env " << ::get_compilation_env() << endl;
    #ifdef __GNUC__
    cout << "c compiled with gcc version " << __VERSION__ << endl;
    #else
    cout << "c compiled with non-gcc compiler" << endl;
    #endif
}

void ScalMC::add_scalmc_options()
{
    scalmc_options.add_options()
    ("help,h", "Prints help")
    ("version", "Print version info")
    ("input", po::value< vector<string> >(), "file(s) to read")
    ("verb,v", po::value(&verb)->default_value(verb), "verbosity")
    ("seed,s", po::value(&seed)->default_value(seed), "Seed")
    ("pivot", po::value(&pivot)->default_value(pivot)
        , "Number of solutions to check for")
    ("measure", po::value(&tScalMC)->default_value(tScalMC)
        , "Number of measurements")
    ("start", po::value(&start_iter)->default_value(start_iter),
         "Start at this many XORs")
    ("log", po::value(&logfilename)->default_value(logfilename),
         "Log of SCALMC iterations.")
    ("break", po::value(&what_to_break)->default_value(what_to_break),
         "What thing to break in CMS")
    ("maple", po::value(&maple)->default_value(maple),
         "Should Maple be enabled")
    ("th", po::value(&num_threads)->default_value(num_threads),
         "How many solving threads to use per solver call")
    ("simp", po::value(&dosimp)->default_value(dosimp),
         "Perform simplifications in CMS")
    ("vcl", po::value(&verb_scalmc_cls)->default_value(verb_scalmc_cls)
        ,"Print banning clause + xor clauses. Highly verbose.")
    ("samples", po::value(&samples)->default_value(samples)
        , "Number of random samples to generate")
    ("sparse", po::value(&sparse)->default_value(sparse)
        , "Generate sparse XORs when possible")
    ("kappa", po::value(&kappa)->default_value(kappa)
        , "Uniformity parameter (see TACAS-15 paper)")
    ("multisample", po::value(&multisample)->default_value(multisample)
        , "Return multiple samples from each call")
    ("sampleout", po::value(&sampleFilename)
        , "Write samples to this file")
    ("indep", po::value(&indep_only)->default_value(indep_only)
        , "Don't extend solution by SAT solver")
    ("startiter", po::value(&startiter)->default_value(startiter)
        , "If positive, use instead of startiter computed by ScalMC")
    ("callsPerSolver", po::value(&callsPerSolver)->default_value(callsPerSolver)
        , "Number of ScalGen calls to make in a single solver, or 0 to use a heuristic")
    ;

    help_options.add(scalmc_options);
    //help_options_complicated.add(scalmc_options);
}

void ScalMC::add_supported_options()
{
    add_scalmc_options();
    p.add("input", 1);

    try {
        po::store(po::command_line_parser(argc, argv).options(help_options).positional(p).run(), vm);
        if (vm.count("help"))
        {
            cout
            << "Approximate counter" << endl;

            cout
            << "scalmc [options] inputfile" << endl << endl;

            cout << help_options << endl;
            std::exit(0);
        }

        if (vm.count("version")) {
            printVersionInfo();
            std::exit(0);
        }

        po::notify(vm);
    } catch (boost::exception_detail::clone_impl<
        boost::exception_detail::error_info_injector<po::unknown_option> >& c
    ) {
        cerr
        << "ERROR: Some option you gave was wrong. Please give '--help' to get help" << endl
        << "       Unkown option: " << c.what() << endl;
        std::exit(-1);
    } catch (boost::bad_any_cast &e) {
        std::cerr
        << "ERROR! You probably gave a wrong argument type" << endl
        << "       Bad cast: " << e.what()
        << endl;

        std::exit(-1);
    } catch (boost::exception_detail::clone_impl<
        boost::exception_detail::error_info_injector<po::invalid_option_value> >& what
    ) {
        cerr
        << "ERROR: Invalid value '" << what.what() << "'" << endl
        << "       given to option '" << what.get_option_name() << "'"
        << endl;

        std::exit(-1);
    } catch (boost::exception_detail::clone_impl<
        boost::exception_detail::error_info_injector<po::multiple_occurrences> >& what
    ) {
        cerr
        << "ERROR: " << what.what() << " of option '"
        << what.get_option_name() << "'"
        << endl;

        std::exit(-1);
    } catch (boost::exception_detail::clone_impl<
        boost::exception_detail::error_info_injector<po::required_option> >& what
    ) {
        cerr
        << "ERROR: You forgot to give a required option '"
        << what.get_option_name() << "'"
        << endl;

        std::exit(-1);
    } catch (boost::exception_detail::clone_impl<
        boost::exception_detail::error_info_injector<po::too_many_positional_options_error> >& what
    ) {
        cerr
        << "ERROR: You gave too many positional arguments. Only the input CNF can be given as a positional option." << endl;
        std::exit(-1);
    } catch (boost::exception_detail::clone_impl<
        boost::exception_detail::error_info_injector<po::ambiguous_option> >& what
    ) {
        cerr
        << "ERROR: The option you gave was not fully written and matches" << endl
        << "       more than one option. Please give the full option name." << endl
        << "       The option you gave: '" << what.get_option_name() << "'" <<endl
        << "       The alternatives are: ";
        for(size_t i = 0; i < what.alternatives().size(); i++) {
            cout << what.alternatives()[i];
            if (i+1 < what.alternatives().size()) {
                cout << ", ";
            }
        }
        cout << endl;

        std::exit(-1);
    } catch (boost::exception_detail::clone_impl<
        boost::exception_detail::error_info_injector<po::invalid_command_line_syntax> >& what
    ) {
        cerr
        << "ERROR: The option you gave is missing the argument or the" << endl
        << "       argument is given with space between the equal sign." << endl
        << "       detailed error message: " << what.what() << endl
        ;
        std::exit(-1);
    }
}

void print_xor(const vector<uint32_t>& vars, const uint32_t rhs)
{
    cout << "[scalmc] Added XOR ";
    for (size_t i = 0; i < vars.size(); i++) {
        cout << vars[i]+1;
        if (i < vars.size()-1) {
            cout << " + ";
        }
    }
    cout << " = " << (rhs ? "True" : "False") << endl;
}

void ScalMC::openLogFile()
{
    if (!logfilename.empty()) {
        logfile.open(logfilename.c_str());
        if (!logfile.is_open()) {
            cout << "[scalmc] Cannot open ScalMC log file '" << logfilename
                 << "' for writing." << endl;
            exit(1);
        }
    }
}

template<class T>
inline T findMedian(vector<T>& numList)
{
    std::sort(numList.begin(), numList.end());
    size_t medIndex = (numList.size() + 1) / 2;
    size_t at = 0;
    if (medIndex >= numList.size()) {
        at += numList.size() - 1;
        return numList[at];
    }
    at += medIndex;
    return numList[at];
}

template<class T>
inline T findMin(vector<T>& numList)
{
    T min = std::numeric_limits<T>::max();
    for (const auto a: numList) {
        if (a < min) {
            min = a;
        }
    }
    return min;
}

bool ScalMC::AddHash(uint32_t num_xor_cls, vector<Lit>& assumps, uint32_t total_num_hashes)
{
    const string randomBits =
        GenerateRandomBits(independent_vars.size() * num_xor_cls, total_num_hashes);

    bool rhs;
    vector<uint32_t> vars;

    for (uint32_t i = 0; i < num_xor_cls; i++) {
        //new activation variable
        solver->new_var();
        uint32_t act_var = solver->nVars()-1;
        assumps.push_back(Lit(act_var, true));

        vars.clear();
        vars.push_back(act_var);
        rhs = gen_rhs();

        for (uint32_t j = 0; j < independent_vars.size(); j++) {
            if (randomBits.at(independent_vars.size() * i + j) == '1') {
                vars.push_back(independent_vars[j]);
            }
        }
        solver->add_xor_clause(vars, rhs);
        if (verb_scalmc_cls) {
            print_xor(vars, rhs);
        }
    }
    return true;
}

int64_t ScalMC::bounded_sol_count(
        uint32_t maxSolutions,
        uint32_t minSolutions,
        const vector<Lit>& assumps,
        const uint32_t hashCount,
        std::map<std::string, uint32_t>* solutionMap
) {
    cout << "[scalmc] "
    "[ " << std::setw(7) << std::setprecision(2) << std::fixed
    << (cpuTimeTotal()-total_runtime)
    << " ]"
    << " bounded_sol_count looking for " << std::setw(4) << maxSolutions << " solutions"
    << " -- hashes active: " << hashCount << endl;

    //Set up things for adding clauses that can later be removed
    std::vector<vector<lbool>> modelsSet;
    vector<lbool> model;
    vector<Lit> new_assumps(assumps);
    solver->new_var();
    uint32_t act_var = solver->nVars()-1;
    new_assumps.push_back(Lit(act_var, true));

    uint64_t solutions = 0;
    lbool ret;
    double last_found_time = cpuTimeTotal();
    while (solutions < maxSolutions) {
        ret = solver->solve(&new_assumps, indep_only);
        if (verb >=2 ) {
            cout << "[scalmc] bounded_sol_count ret: " << std::setw(7) << ret;
            if (ret == l_True) {
                cout << " sol no. " << std::setw(3) << solutions;
            }
            cout << " T: "
            << std::setw(7) << std::setprecision(2) << std::fixed << (cpuTimeTotal()-total_runtime)
            << " -- hashes act: " << hashCount
            << " -- T since last: "
            << std::setw(7) << std::setprecision(2) << std::fixed << (cpuTimeTotal()-last_found_time)
            << endl;
            last_found_time = cpuTimeTotal();
        }
        if (ret != l_True) {
            break;
        }
        model = solver->get_model();
        modelsSet.push_back(model);

        if (solutions < maxSolutions) {
            vector<Lit> lits;
            lits.push_back(Lit(act_var, false));
            for (const uint32_t var: independent_vars) {
                if (solver->get_model()[var] != l_Undef) {
                    lits.push_back(Lit(var, solver->get_model()[var] == l_True));
                } else {
                    assert(false);
                }
            }
            if (verb_scalmc_cls) {
                cout << "[scalmc] Adding banning clause: " << lits << endl;
            }
            solver->add_clause(lits);
        }
        solutions++;
    }

    //we have all solutions now, if not l_False
    if (solutions < maxSolutions && solutions >= minSolutions && solutionMap) {
        assert(minSolutions > 0);
        std::vector<int> modelIndices;
        for (uint32_t i = 0; i < modelsSet.size(); i++) {
            modelIndices.push_back(i);
        }
        std::shuffle(modelIndices.begin(), modelIndices.end(), randomEngine);

        uint32_t numSolutionsToReturn = SolutionsToReturn(solutions);
        for (uint32_t i = 0; i < numSolutionsToReturn; i++) {
            model = modelsSet.at(modelIndices.at(i));
            add_solution_to_map(model, solutionMap);
        }
    }

    //Remove clauses added
    vector<Lit> cl_that_removes;
    cl_that_removes.push_back(Lit(act_var, false));
    solver->add_clause(cl_that_removes);

    assert(ret != l_Undef);
    return solutions;
}

void ScalMC::add_solution_to_map(
    const vector<lbool>& model
    , std::map<std::string, uint32_t>* solutionMap
) const {
    assert(solutionMap != NULL);

    std::stringstream  solution;
    solution << "v ";
    for (uint32_t j = 0; j < independent_vars.size(); j++) {
        uint32_t var = independent_vars[j];
        if (model[var] != l_Undef) {
            solution << ((model[var] != l_True) ? "-":"") << var + 1 << " ";
        }
    }
    solution << "0";

    std::string sol_str = solution.str();
    std::map<string, uint32_t>::iterator it = solutionMap->find(sol_str);
    if (it == solutionMap->end()) {
        (*solutionMap)[sol_str] = 0;
    }
    (*solutionMap)[sol_str] += 1;
}

void ScalMC::readInAFile(SATSolver* solver2, const string& filename)
{
    solver2->add_sql_tag("filename", filename);
    #ifndef USE_ZLIB
    FILE * in = fopen(filename.c_str(), "rb");
    DimacsParser<StreamBuffer<FILE*, FN> > parser(solver, NULL, 2);
    #else
    gzFile in = gzopen(filename.c_str(), "rb");
    DimacsParser<StreamBuffer<gzFile, GZ> > parser(solver, NULL, 2);
    #endif

    if (in == NULL) {
        std::cerr
        << "ERROR! Could not open file '"
        << filename
        << "' for reading: " << strerror(errno) << endl;

        std::exit(1);
    }

    if (!parser.parse_DIMACS(in, false)) {
        exit(-1);
    }

    independent_vars.swap(parser.independent_vars);

    #ifndef USE_ZLIB
        fclose(in);
    #else
        gzclose(in);
    #endif
}

void ScalMC::readInStandardInput(SATSolver* solver2)
{
    cout
    << "c Reading from standard input... Use '-h' or '--help' for help."
    << endl;

    #ifndef USE_ZLIB
    FILE * in = stdin;
    #else
    gzFile in = gzdopen(0, "rb"); //opens stdin, which is 0
    #endif

    if (in == NULL) {
        std::cerr << "ERROR! Could not open standard input for reading" << endl;
        std::exit(1);
    }

    #ifndef USE_ZLIB
    DimacsParser<StreamBuffer<FILE*, FN> > parser(solver2, NULL, 2);
    #else
    DimacsParser<StreamBuffer<gzFile, GZ> > parser(solver2, NULL, 2);
    #endif

    if (!parser.parse_DIMACS(in, false)) {
        exit(-1);
    }

    #ifdef USE_ZLIB
        gzclose(in);
    #endif
}

int ScalMC::solve()
{
    total_runtime = cpuTimeTotal();
    //set seed
    cout << "[scalmc] using seed: " << seed << endl;
    randomEngine.seed(seed);

    if (vm.count("log") == 0) {
        if (vm.count("input") != 0) {
            logfilename = vm["input"].as<vector<string> >()[0] + ".log";
            cout << "[scalmc] Logfile name not given, assumed to be " << logfilename << endl;
        } else {
            std::cerr << "[scalmc] ERROR: You must provide the logfile name" << endl;
            exit(-1);
        }
    }
    printVersionInfo();

    openLogFile();
    startTime = cpuTimeTotal();

    //solver = new SATSolver(&must_interrupt);
    CMSat::GaussConf gconf;
    conf.gaussconf.max_num_matrixes = 2;
    conf.gaussconf.autodisable = false;
    conf.burst_search_len = 0;
    conf.global_multiplier_multiplier_max = 3;
    conf.global_timeout_multiplier_multiplier = 1.5;

    conf.simplify_at_startup = 1;
    conf.varElimRatioPerIter = 1;
    conf.restartType = Restart::geom;
    conf.polarity_mode = CMSat::PolarityMode::polarmode_neg;
    conf.maple = maple;

    //simplify broken
    if (what_to_break == 40) {
        conf.simplify_at_every_startup = true;
    }

    //polarity cached
    if (what_to_break == 41) {
        conf.polarity_mode = CMSat::PolarityMode::polarmode_automatic;
    }

    //burst broken
    if (what_to_break == 43) {
        conf.burst_broken = true;
        conf.burst_search_len = 500;
    }

    //polarity mess-up
    if (what_to_break == 44) {
        conf.mess_up_polarity = true;
    }

    //simplify broken + polarity mess-up + burst + glue-restart-only
    if (what_to_break == 44) {
        conf.burst_broken = true;
        conf.burst_search_len = 500;

        conf.mess_up_polarity = true;

        conf.polarity_mode = CMSat::PolarityMode::polarmode_automatic;

        conf.simplify_at_every_startup = true;
    }
    conf.do_simplify_problem = dosimp;

    solver = new SATSolver((void*)&conf);

    if (verb > 2) {
        solver->set_verbosity(verb-2);
    }
    solver->set_allow_otf_gauss();

    if (num_threads > 1) {
        solver->set_num_threads(num_threads);
    }

    if (vm.count("input") != 0) {
        vector<string> inp = vm["input"].as<vector<string> >();
        if (inp.size() > 1) {
            cout << "[scalmc] ERROR: can only parse in one file" << endl;
        }
        readInAFile(solver, inp[0].c_str());
    } else {
        readInStandardInput(solver);
    }
    call_after_parse();

    //TODO this somehow messes up things.. but why? This is a bug in CMS.
    //solver->simplify();

    if (start_iter > independent_vars.size()) {
        cout << "[scalmc] ERROR: Manually-specified start_iter"
             "is larger than the size of the independent set.\n" << endl;
        return -1;
    }

    if (samples == 0) {
        if (vm.count("sampleout")){
            cerr << "ERROR: You did not give the '--samples N' option, but you gave the '--sampleout FNAME' option." << endl;
            cout << "ERROR: This is confusing. Please give '--samples N' if you give '--sampleout FNAME'" << endl;
            exit(-1);
        }
        cout << "[scalmc] Using start iteration " << start_iter << endl;

        SATCount solCount;
        bool finished = count(solCount);
        assert(finished);

        cout << "[scalmc] FINISHED ScalMC T: " << (cpuTimeTotal() - startTime) << " s" << endl;
        if (solCount.hashCount == 0 && solCount.cellSolCount == 0) {
            cout << "[scalmc] Formula was UNSAT " << endl;
        }

        if (verb > 2) {
            solver->print_stats();
        }

        cout << "[scalmc] Number of solutions is: "
        << solCount.cellSolCount
         << " x 2^" << solCount.hashCount << endl;
    } else {
        if (startiter > independent_vars.size()) {
            cerr << "ERROR: Manually-specified startiter for ScalGen"
                 "is larger than the size of the independent set.\n" << endl;
            return -1;
        }

        /* Compute pivot via formula from TACAS-15 paper */
        pivotScalGen = ceil(4.03 * (1 + (1/kappa)) * (1 + (1/kappa)));

        if (samples == 0 || startiter == 0) {
            if (samples > 0)
            {
                cout << "Using scalmc to compute startiter for ScalGen" << endl;
                if (!vm["pivotAC"].defaulted() || !vm["tScalMC"].defaulted()) {
                    cout << "WARNING: manually-specified pivotAC and/or tScalMC may"
                         << " not be large enough to guarantee correctness of ScalGen." << endl
                         << "Omit those arguments to use safe default values." << endl;
                } else {
                    /* Fill in here the best parameters for scalmc achieving
                     * epsilon=0.8 and delta=0.177 as required by ScalGen */
                    pivot = 73;
                    tScalMC = 11;
                }
            }
            else if(vm["tScalMC"].defaulted())
            {
                /* Compute tscalmc */
                double delta = 0.2;
                double confidence = 1.0 - delta;
                int bestIteration = iterationConfidences.size() - 1;
                int worstIteration = 0;
                int currentIteration = (worstIteration + bestIteration) / 2;
                if (iterationConfidences[bestIteration] >= confidence)
                {
                    while (currentIteration != worstIteration)
                    {
                        if (iterationConfidences[currentIteration] >= confidence)
                        {
                            bestIteration = currentIteration;
                            currentIteration = (worstIteration + currentIteration) / 2;
                        }
                        else
                        {
                            worstIteration = currentIteration;
                            currentIteration = (currentIteration + bestIteration) / 2;
                        }
                    }
                    tScalMC = (2 * bestIteration) + 1;
                }
                else
                    tScalMC = ceil(17 * log2(3.0 / delta));
            }

            SATCount solCount;
            cout << "ScalGen starting from iteration " << startiter << endl;

            bool finished = false;
            finished = count(solCount);

            cout << "ScalMC finished in " << (cpuTimeTotal() - startTime) << " s" << endl;
            assert(finished);

            if (solCount.hashCount == 0 && solCount.cellSolCount == 0) {
                cout << "The input formula is unsatisfiable." << endl;
                return correctReturnValue(l_False);
            }

            if (conf.verbosity) {
                solver->print_stats();
            }

            if (samples == 0)
            {
                cout << "Number of solutions is: " << solCount.cellSolCount
                     << " x 2^" << solCount.hashCount << endl;

                return correctReturnValue(l_True);
            }
            else
            {
                double si = round(solCount.hashCount + log2(solCount.cellSolCount)
                    + log2(1.8) - log2(pivotScalGen)) - 2;
                if (si > 0)
                    startiter = si;
                else
                    startiter = 0;   /* Indicate ideal sampling case */
            }
        }
        else
        {
            cout << "Using manually-specified startiter for ScalGen" << endl;
        }
        /* Run ScalGen */
        generate_samples();

        /* Output samples */
        std::ostream* os;
        std::ofstream* sampleFile = NULL;
        if (vm.count("sampleout"))
        {
            sampleFile = new std::ofstream;
            sampleFile->open(sampleFilename.c_str());
            if (!(*sampleFile)) {
                cout
                << "ERROR: Couldn't open file '"
                << sampleFilename
                << "' for writing!"
                << endl;
                std::exit(-1);
            }
            os = sampleFile;
        } else {
            os = &cout;
        }

        for (const auto& sol: globalSolutionMap) {
            std::vector<uint32_t> counts = sol.second;
            // TODO this will need to be changed once multithreading is implemented
            *os << std::setw(5) << counts[0] << " : "  << sol.first.c_str() << endl;
        }
        delete sampleFile;
    }

    return correctReturnValue(l_True);
}

int main(int argc, char** argv)
{
    #if defined(__GNUC__) && defined(__linux__)
    feenableexcept(FE_INVALID   |
                   FE_DIVBYZERO |
                   FE_OVERFLOW
                  );
    #endif

    ScalMC main(argc, argv);
    main.add_supported_options();
    return main.solve();
}

void ScalMC::call_after_parse()
{
    if (independent_vars.empty()) {
        cout
        << "[scalmc] WARNING! No independent vars were set using 'c ind var1 [var2 var3 ..] 0'"
        "notation in the CNF." << endl
        << "[scalmc] we may work substantially worse!" << endl;
        for (size_t i = 0; i < solver->nVars(); i++) {
            independent_vars.push_back(i);
        }
    }
    cout << "[scalmc] Num independent vars: " << independent_vars.size() << endl;
    cout << "[scalmc] Independent vars: ";
    for (auto v: independent_vars) {
        cout << v+1 << ", ";
    }
    cout << endl;
    solver->set_independent_vars(&independent_vars);
}

void ScalMC::SetHash(uint32_t clausNum, std::map<uint64_t,Lit>& hashVars, vector<Lit>& assumps)
{
    if (clausNum < assumps.size()) {
        uint64_t numberToRemove = assumps.size()- clausNum;
        for (uint64_t i = 0; i<numberToRemove; i++) {
            assumps.pop_back();
        }
    } else {
        if (clausNum > assumps.size() && assumps.size() < hashVars.size()) {
            for (uint32_t i = assumps.size(); i< hashVars.size() && i < clausNum; i++) {
                assumps.push_back(hashVars[i]);
            }
        }
        if (clausNum > hashVars.size()) {
            AddHash(clausNum-hashVars.size(), assumps, clausNum);
            for (uint64_t i = hashVars.size(); i < clausNum; i++) {
                hashVars[i] = assumps[i];
            }
        }
    }
}

bool ScalMC::count(SATCount& count)
{
    count.clear();
    vector<uint64_t> numHashList;
    vector<int64_t> numCountList;
    vector<Lit> assumps;

    uint64_t hashCount = start_iter;
    uint64_t hashPrev = 0;
    uint64_t mPrev = 0;

    double myTime = cpuTimeTotal();
    cout << "[scalmc] Starting up, initial measurement" << endl;
    if (hashCount == 0) {
        int64_t currentNumSolutions = bounded_sol_count(pivot+1, 0, assumps, count.hashCount);
        if (!logfilename.empty()) {
            logfile << "scalmc:"
            << "breakmode-" << what_to_break << ":"
            <<"0:0:"
            << std::fixed << std::setprecision(2) << (cpuTimeTotal() - myTime) << ":"
            << (int)(currentNumSolutions == (pivot + 1)) << ":"
            << currentNumSolutions << endl;
        }

        //Din't find at least pivot+1
        if (currentNumSolutions <= pivot) {
            cout << "[scalmc] Did not find at least pivot+1 (" << pivot << ") we found only " << currentNumSolutions << ", exiting ScalMC" << endl;
            count.cellSolCount = currentNumSolutions;
            count.hashCount = 0;
            return true;
        }
        hashCount++;
    }

    for (uint32_t j = 0; j < tScalMC; j++) {
        map<uint64_t,int64_t> countRecord;
        map<uint64_t,uint32_t> succRecord;
        map<uint64_t,Lit> hashVars; //map assumption var to XOR hash

        uint64_t numExplored = 0;
        uint64_t lowerFib = 0, upperFib = independent_vars.size();

        while (numExplored < independent_vars.size()) {
            cout << "[scalmc] Explored: " << std::setw(4) << numExplored
                 << " ind set size: " << std::setw(6) << independent_vars.size() << endl;
            myTime = cpuTimeTotal();
            uint64_t swapVar = hashCount;
            SetHash(hashCount,hashVars,assumps);
            cout << "[scalmc] hashes active: " << std::setw(6) << hashCount << endl;
            int64_t currentNumSolutions = bounded_sol_count(pivot + 1, 0, assumps, hashCount);

            //cout << currentNumSolutions << ", " << pivot << endl;
            if (!logfilename.empty()) {
                logfile << "scalmc:"
                << "breakmode-" << what_to_break << ":"
                << j << ":" << hashCount << ":"
                << std::fixed << std::setprecision(2) << (cpuTimeTotal() - myTime) << ":"
                << (int)(currentNumSolutions == (pivot + 1)) << ":"
                << currentNumSolutions << endl;
            }

            if (currentNumSolutions < pivot + 1) {
                numExplored = lowerFib+independent_vars.size()-hashCount;
                if (succRecord.find(hashCount-1) != succRecord.end()
                    && succRecord[hashCount-1] == 1
                ) {
                    numHashList.push_back(hashCount);
                    numCountList.push_back(currentNumSolutions);
                    mPrev = hashCount;
                    //less than pivot solutions
                    break;
                }
                succRecord[hashCount] = 0;
                countRecord[hashCount] = currentNumSolutions;
                if (std::abs<int64_t>((int64_t)hashCount - (int64_t)mPrev) <= 2 && mPrev != 0) {
                    upperFib = hashCount;
                    hashCount--;
                } else {
                    if (hashPrev > hashCount) {
                        hashPrev = 0;
                    }
                    upperFib = hashCount;
                    if (hashPrev > lowerFib) {
                        lowerFib = hashPrev;
                    }
                    hashCount = (upperFib+lowerFib)/2;
                }
            } else {
                assert(currentNumSolutions == pivot+1);

                numExplored = hashCount + independent_vars.size()-upperFib;
                if (succRecord.find(hashCount+1) != succRecord.end()
                    && succRecord[hashCount+1] == 0
                ) {
                    numHashList.push_back(hashCount+1);
                    numCountList.push_back(countRecord[hashCount+1]);
                    mPrev = hashCount+1;
                    break;
                }
                succRecord[hashCount] = 1;
                if (std::abs<int64_t>((int64_t)hashCount - (int64_t)mPrev) < 2 && mPrev!=0) {
                    lowerFib = hashCount;
                    hashCount ++;
                } else if (lowerFib + (hashCount - lowerFib)*2 >= upperFib-1) {
                    lowerFib = hashCount;
                    hashCount = (lowerFib+upperFib)/2;
                } else {
                    //cout << "hashPrev: " << hashPrev << " hashCount: " << hashCount << endl;
                    hashCount = lowerFib + (hashCount -lowerFib)*2;
                }
            }
            hashPrev = swapVar;
        }
        assumps.clear();
        solver->simplify(&assumps);
        hashCount =mPrev;
    }
    if (numHashList.size() == 0) {
        //UNSAT
        return true;
    }

    auto minHash = findMin(numHashList);
    auto cnt_it = numCountList.begin();
    for (auto hash_it = numHashList.begin()
        ; hash_it != numHashList.end() && cnt_it != numCountList.end()
        ; hash_it++, cnt_it++
    ) {
        *cnt_it *= pow(2, (*hash_it) - minHash);
    }
    int medSolCount = findMedian(numCountList);

    count.cellSolCount = medSolCount;
    count.hashCount = minHash;
    return true;
}

/*void ScalMC::check_confidence()
{
    std::ifstream probmapfile;
    probmapfile.open("./ProbMapFile_36.txt");
    if (probmapfile.is_open()){
     while (getline(probmapfile, line)) {
       pch = strtok(strdup(line.c_str()), ":");
       val = std::atoi(pch);
       pch = strtok(NULL, ":");
       if (std::atof(pch) > (1 - conf.delta))
	        break;
     }
   }
   probmapfile.close();
   if (val == 0) {
     std::cerr << "Probmapfile failed, delta is " << conf.delta << std::endl;
     exit(-1);
   }
   conf.tscalmc = val;
   std::cout<<"t scalmc:"<<conf.tscalmc<<std::endl;
     confidence = (float *) malloc(sizeof(float)*(2+conf.tscalmc));
   if(confidence == NULL){
     cout << "Out of memory, could not allocate confidence list" << endl;
     exit(-1);
   }

   // user gives: delta, epsilon
   // experiments to run: delta = 0.1 (t=21)
   //                     epsilon = 0.8 (71 maybe?)

   // confidece = 1-delta
   // 1+epsilon = desired distance from the ground truth
   conf.pivot = = int(1 + 9.84*(1+(1/conf.epsilon))*(1+(1/conf.epsilon))*(1+(conf.epsilon/(1+conf.epsilon))));
}*/

///////////
// Useful helper functions
///////////

void ScalMC::printVersionInfo() const
{
    ::printVersionInfoScalMC();
    cout << "c CryptoMiniSat version " << solver->get_version() << endl;
    cout << "c CryptoMiniSat SHA revision " << solver->get_version_sha1() << endl;
    cout << "c CryptoMiniSat compilation env " << solver->get_compilation_env() << endl;
    #ifdef __GNUC__
    cout << "c compiled with gcc version " << __VERSION__ << endl;
    #else
    cout << "c compiled with non-gcc compiler" << endl;
    #endif
}

int ScalMC::correctReturnValue(const lbool ret) const
{
    int retval = -1;
    if (ret == l_True) {
        retval = 10;
    } else if (ret == l_False) {
        retval = 20;
    } else if (ret == l_Undef) {
        retval = 15;
    } else {
        std::cerr << "Something is very wrong, output is neither l_Undef, nor l_False, nor l_True" << endl;
        exit(-1);
    }

    return retval;
}


/////////////// scalgen ////////////////
/* Number of solutions to return from one invocation of ScalGen. */
uint32_t ScalMC::SolutionsToReturn(uint32_t numSolutions)
{
    if (startiter == 0)   // TODO improve hack for ideal sampling case?
        return numSolutions;
    else if (multisample)
        return loThresh;
    else
        return 1;
}

void ScalMC::generate_samples()
{
    hiThresh = ceil(1 + (1.4142136 * (1 + kappa) * pivotScalGen));
    loThresh = floor(pivotScalGen / (1.4142136 * (1 + kappa)));
    uint32_t samplesPerCall = SolutionsToReturn(samples);
    uint32_t callsNeeded = (samples + samplesPerCall - 1) / samplesPerCall;
    cout << "loThresh " << loThresh
    << ", hiThresh " << hiThresh
    << ", startiter " << startiter << endl;

    cout << "Outputting " << samplesPerCall << " solutions from each ScalGen call" << endl;
    uint32_t numCallsInOneLoop = 0;
    if (callsPerSolver == 0) {
        // TODO: does this heuristic still work okay?
        uint32_t si = startiter > 0 ? startiter : 1;
        numCallsInOneLoop = std::min(solver->nVars() / (si * 14), callsNeeded);
        if (numCallsInOneLoop == 0) {
            numCallsInOneLoop = 1;
        }
    } else {
        numCallsInOneLoop = callsPerSolver;
        cout << "Using manually-specified callsPerSolver" << endl;
    }

    uint32_t numCallLoops = callsNeeded / numCallsInOneLoop;
    uint32_t remainingCalls = callsNeeded % numCallsInOneLoop;

    cout << "Making " << numCallLoops << " loops."
         << " calls per loop: " << numCallsInOneLoop
         << " remaining: " << remainingCalls << endl;
    bool timedOut = false;
    uint32_t sampleCounter = 0;
    std::map<string, uint32_t> threadSolutionMap;
    double allThreadsTime = 0;
    uint32_t allThreadsSampleCount = 0;
    double threadStartTime = cpuTimeTotal();
    uint32_t lastSuccessfulHashOffset = 0;

    if (startiter > 0)
    {
        ///Perform extra ScalGen calls that don't fit into the loops
        if (remainingCalls > 0) {
            sampleCounter = ScalGenCall(
                                remainingCalls, sampleCounter
                                , threadSolutionMap
                                , &lastSuccessfulHashOffset, threadStartTime);
        }

        // Perform main ScalGen call loops
        for (uint32_t i = 0; i < numCallLoops; i++) {
            if (!timedOut) {
                sampleCounter = ScalGenCall(
                                    numCallsInOneLoop, sampleCounter, threadSolutionMap
                                    , &lastSuccessfulHashOffset, threadStartTime
                                );
            }
        }
    }
    else
    {
        /* Ideal sampling case; enumerate all solutions */
        vector<Lit> assumps;
        uint32_t count = 0;
        bounded_sol_count(std::numeric_limits<uint32_t>::max(), 0, assumps, 0, &threadSolutionMap);

        std::uniform_int_distribution<unsigned> uid {0, count-1};
        for (uint32_t i = 0; i < samples; ++i)
        {
            map<string, uint32_t>::iterator it = threadSolutionMap.begin();
            for (uint32_t j = uid(randomEngine); j > 0; --j)    // TODO improve hack
                ++it;
            it->second += 1;
        }
    }

    for (map<string, uint32_t>::iterator itt = threadSolutionMap.begin()
            ; itt != threadSolutionMap.end()
            ; itt++
        ) {
        string solution = itt->first;
        map<string, std::vector<uint32_t>>::iterator itg = globalSolutionMap.find(solution);
        if (itg == globalSolutionMap.end()) {
            globalSolutionMap[solution] = std::vector<uint32_t>(1, 0);
        }
        globalSolutionMap[solution][0] += itt->second;
        allThreadsSampleCount += itt->second;
    }

    double timeTaken = cpuTimeTotal() - threadStartTime;
    allThreadsTime += timeTaken;
    cout
    << "Total time for ScalGen: " << timeTaken << " s"
    << (timedOut ? " (TIMED OUT)" : "")
    << endl;

    // TODO put this back once multithreading is implemented
    //cout << "Total time for all ScalGen calls: " << allThreadsTime << " s" << endl;
    cout << "Samples generated: " << allThreadsSampleCount << endl;
}

uint32_t ScalMC::ScalGen(
    uint32_t loc_samples
    , uint32_t sampleCounter
    , std::map<string, uint32_t>& solutionMap
    , uint32_t* lastSuccessfulHashOffset
    , double timeReference
)
{
    lbool ret = l_False;
    uint32_t i, currentHashCount, currentHashOffset, hashOffsets[3];
    vector<Lit> assumps;
    for (i = 0; i < loc_samples; i++) {
        map<uint64_t,Lit> hashVars; //map assumption var to XOR hash
        sampleCounter ++;
        ret = l_Undef;

        hashOffsets[0] = *lastSuccessfulHashOffset;   // Start at last successful hash offset
        if (hashOffsets[0] == 0) { // Starting at q-2; go to q-1 then q
            hashOffsets[1] = 1;
            hashOffsets[2] = 2;
        } else if (hashOffsets[0] == 2) { // Starting at q; go to q-1 then q-2
            hashOffsets[1] = 1;
            hashOffsets[2] = 0;
        }
        for (uint32_t j = 0; j < 3; j++) {
            currentHashOffset = hashOffsets[j];
            currentHashCount = currentHashOffset + startiter;
            SetHash(currentHashCount, hashVars, assumps);

            const uint64_t solutionCount = bounded_sol_count(hiThresh, loThresh, assumps, currentHashCount, &solutionMap);
            if (solutionCount < hiThresh && solutionCount >= loThresh) {
                ret = l_True;
            } else {
                ret = l_False;
            }

            if (!logfilename.empty()) {
                logfile << "scalgen:"
                << sampleCounter << ":" << currentHashCount << ":"
                << std::fixed << std::setprecision(2) << (cpuTimeTotal() - timeReference) << ":"
                << (int)(ret == l_False ? 1 : (ret == l_True ? 0 : 2)) << ":"
                << solutionCount << endl;
            }

            // Number of solutions in correct range
            if (ret == l_True) {
                *lastSuccessfulHashOffset = currentHashOffset;
                break;
            } else { // Number of solutions too small or too large

                // At q-1, and need to pick next hash count
                if ((j == 0) && (currentHashOffset == 1)) {
                    if (solutionCount < loThresh) {
                        // Go to q-2; next will be q
                        hashOffsets[1] = 0;
                        hashOffsets[2] = 2;
                    } else {
                        // Go to q; next will be q-2
                        hashOffsets[1] = 2;
                        hashOffsets[2] = 0;
                    }
                }
            }
        }
        if (ret != l_True) {
            i --;
        }
        assumps.clear();
        solver->simplify(&assumps);
    }
    return sampleCounter;
}

int ScalMC::ScalGenCall(
    uint32_t loc_samples
    , uint32_t sampleCounter
    , std::map<string, uint32_t>& solutionMap
    , uint32_t* lastSuccessfulHashOffset
    , double timeReference
)
{
    //delete solver;
    //solver = new SATSolver(&conf, &must_interrupt);
    //solverToInterrupt = solver;

    /* Heuristic: running solver once before adding any hashes
     * tends to help performance (need to do this for ScalGen since
     * we aren't necessarily starting from hashCount zero) */
    solver->solve();

    sampleCounter = ScalGen(
                        loc_samples
                        , sampleCounter
                        , solutionMap
                        , lastSuccessfulHashOffset
                        , timeReference
                    );
    return sampleCounter;
}
