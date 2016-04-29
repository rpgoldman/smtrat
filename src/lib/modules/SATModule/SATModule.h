/*
 * ***************************************************************************************[Solver.h]
 * Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
 * Copyright (c) 2007-2010, Niklas Sorensson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 * NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
 * OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/**
 * @file SATModule.h
 * @author Florian Corzilius <corzilius@cs.rwth-aachen.de>
 * @since 2012-01-18
 * @version 2014-10-02
 */

#pragma once

#include "../../config.h"
#include "SATSettings.h"
#include "Vec.h"
#include "Heap.h"
#include "Alg.h"
#include "Options.h"
#include "SolverTypes.h"
#include "Sort.h"
#include <math.h>
#include <carl/formula/DIMACSExporter.h>
#include "../../solver/Module.h"
#include "../../solver/RuntimeSettings.h"

#ifdef SMTRAT_DEVOPTION_Statistics
#include "SATModuleStatistics.h"
#endif

namespace smtrat
{
    /**
     * Implements a module performing DPLL style SAT checking. It is mainly based on Minisat 2.0 and 
     * extends it by enabling the SMT-RAT module interfaces and some optimizations.
     */
    template<class Settings>
    class SATModule:
        public Module
    {
        private:

            /**
             * Stores information about a Minisat variable.
             */
            struct VarData
            {
                /**
                 * A reference to the clause, which implied the current assignment of this variable.
                 * It is not defined, if the assignment of the variable follows from a clause of size 0
                 * or if the variable is unassigned.
                 */
                Minisat::CRef reason;
                
                /// The level in which the variable has been assigned, if it is not unassigned.
                int level;
            };

            /**
             * Stores all information regarding a Boolean abstraction of a constraint.
             */
            struct Abstraction
            {
                /**
                 * A flag, which is set to false, if the constraint corresponding to the abstraction does
                 * not occur in the received formula and, hence, does not need to be part of a consistency check.
                 */ 
                bool consistencyRelevant;
                
                /**
                 * A flag, which is set to false, if the constraint corresponding to the abstraction is redundant
                 * and hence there is no need to include it to a consistency check. (NOT YET USED)
                 */ 
                bool isDeduction;
                
                /**
                 * <0, if the corresponding constraint must still be added to the passed formula;
                 * >0, if the corresponding constraint must still be removed to the passed formula;
                 * 0, otherwise.
                 */
                int updateInfo;
                
                /**
                 * The position of the corresponding constraint in the passed formula. It points to the end
                 * if the constraint is not part of the passed formula.
                 */
                ModuleInput::iterator position;
                
                /**
                 * The constraint corresponding to this abstraction. It is NULL, if the literal for which we 
                 * store this abstraction does actually not belong to an abstraction.
                 */
                FormulaT reabstraction;
                
                // The origins of this constraint. Usually it is its own origin, but the origins can be extended during solving.
                std::shared_ptr<std::vector<FormulaT>> origins;
                
                /**
                 * Constructs abstraction information, for a literal which does actually not belong to an abstraction.
                 * @param _position The end of the passed formula of this module.
                 * @param _constraint The constraint to abstract.
                 */
                Abstraction( ModuleInput::iterator _position, const FormulaT& _reabstraction ):
                    consistencyRelevant( false ),
                    isDeduction( true ),
                    updateInfo( 0 ),
                    position( _position ),
                    reabstraction( _reabstraction ),
                    origins( nullptr )
                {}
                
                Abstraction( const Abstraction& ) = delete;
            };
            
            struct ClauseInformation
            {
                bool mStoredInSatisfied;
                int mPosition;
                std::vector<FormulaT> mOrigins;
                
                ClauseInformation() = delete;
                ClauseInformation( int _position ):
                    mStoredInSatisfied( false ),
                    mPosition( _position ),
                    mOrigins()
                {}
                ClauseInformation( const ClauseInformation& ) = default;
                ClauseInformation( ClauseInformation&& ) = default;
                
                void addOrigin( const FormulaT& _formula )
                {
                    mOrigins.push_back( _formula );
                }
                
                void removeOrigin( const FormulaT& _formula )
                {
                    auto iter = std::find( mOrigins.begin(), mOrigins.end(), _formula );
                    if( iter != mOrigins.end() )
                    {
                        if( iter != --mOrigins.end() )
                        {
                            *iter = mOrigins.back();
                            mOrigins.pop_back();
                        }
                        else
                        {
                            mOrigins.pop_back();
                        }
                    }
                }
            };

            /// [Minisat related code]
            struct Watcher
            {
                /// [Minisat related code]
                Minisat::CRef cref;
                
                /// [Minisat related code]
                Minisat::Lit  blocker;

                /// [Minisat related code]
                Watcher( Minisat::CRef cr, Minisat::Lit p ):
                    cref( cr ),
                    blocker( p )
                {}
                
                /// [Minisat related code]
                bool operator ==( const Watcher& w ) const
                {
                    return cref == w.cref;
                }

                /// [Minisat related code]
                bool operator !=( const Watcher& w ) const
                {
                    return cref != w.cref;
                }
            };

            /// [Minisat related code]
            struct WatcherDeleted
            {
                /// [Minisat related code]
                const Minisat::ClauseAllocator& ca;

                /// [Minisat related code]
                WatcherDeleted( const Minisat::ClauseAllocator& _ca ):
                    ca( _ca )
                {}
                
                /// [Minisat related code]
                bool operator ()( const Watcher& w ) const
                {
                    return ca[w.cref].mark() == 1;
                }
            };

            /// [Minisat related code]
            struct VarOrderLt
            {
                /// [Minisat related code]
                const Minisat::vec<double>& activity;

                /// [Minisat related code]
                bool operator ()( Minisat::Var x, Minisat::Var y ) const
                {
                    return activity[x] > activity[y];
                }

                /// [Minisat related code]
                VarOrderLt( const Minisat::vec<double>& act ):
                    activity( act )
                {}
            };
            
            struct CNFInfos
            {
                carl::uint mCounter;
                Minisat::Lit mLiteral;
                std::vector<Minisat::CRef> mClauses;
                
                CNFInfos():
                    mCounter( 1 ),
                    mLiteral( Minisat::lit_Undef ),
                    mClauses()
                {}
            };

            /**
             * Maps the constraints occurring in the SAT module to their abstractions. We store a vector of literals
             * for each constraints, which is only used in the optimization, which applies valid substitutions.
             */
            typedef carl::FastMap<FormulaT, std::vector<Minisat::Lit>> ConstraintLiteralsMap;
            
            /// Maps the Boolean variables to their corresponding Minisat variable.
            typedef carl::FastMap<carl::Variable, Minisat::Var> BooleanVarMap;
            
            /// Maps the Minisat variables to their corresponding boolean variable.
            typedef std::unordered_map<int,FormulaT> MinisatVarMap;

            /**
             * Maps each Minisat variable to a pair of Abstractions, one contains the abstraction information of the literal
             * being the variable and one contains the abstraction information of the literal being the variables negation.
             */
            typedef Minisat::vec<std::pair<Abstraction*,Abstraction*>> BooleanConstraintMap;
            
            /// Maps the clauses in the received formula to the corresponding Minisat clause.
            typedef carl::FastMap<FormulaT, CNFInfos> FormulaCNFInfosMap;

            /// Maps the minisat variable to the formulas which influence its value
            typedef std::map<Minisat::Var, FormulasT> VarLemmaMap;

            /// A vector of vectors of literals representing a vector of clauses.
            typedef std::vector<std::vector<Minisat::Lit>> ClauseVector;
            
            /// A set of vectors of integer representing a set of clauses.
            typedef std::set<std::vector<int>> ClauseSet;
            
            ///
            typedef carl::FastMap<signed,std::unordered_set<signed>> TseitinVarShadows;
            
            ///
            struct LiteralClauses
            {
            private:
                std::vector<Minisat::CRef> mPositives;
                std::vector<Minisat::CRef> mNegatives;
                
            public:
                
                LiteralClauses(): mPositives(), mNegatives() {}
                LiteralClauses( const LiteralClauses& ) = delete;
                LiteralClauses( LiteralClauses&& _toMove ):
                    mPositives( std::move(_toMove.mPositives) ),
                    mNegatives( std::move(_toMove.mNegatives) )
                {}
                ~LiteralClauses(){}
                
                const std::vector<Minisat::CRef>& positives() const
                {
                    return mPositives;
                }
                
                const std::vector<Minisat::CRef>& negatives() const
                {
                    return mNegatives;
                }
                
                void addPositive( Minisat::CRef _cref )
                {
                    mPositives.push_back( _cref );
                }
                
                void addNegative( Minisat::CRef _cref )
                {
                    mNegatives.push_back( _cref );
                }
                
                void removePositive( Minisat::CRef _cref )
                {
                    auto iter = std::find( mPositives.begin(), mPositives.end(), _cref );
                    if( iter != mPositives.end() )
                    {
                        *iter = mPositives.back();
                        mPositives.pop_back();
                    }
                }
                
                void removeNegative( Minisat::CRef _cref )
                {
                    auto iter = std::find( mNegatives.begin(), mNegatives.end(), _cref );
                    if( iter != mNegatives.end() )
                    {
                        *iter = mNegatives.back();
                        mNegatives.pop_back();
                    }
                }
                
                void reloc( Minisat::ClauseAllocator& _ca, Minisat::ClauseAllocator& _to )
                {
                    for( Minisat::CRef& cr : mPositives )
                        _ca.reloc( cr, _to );
                    for( Minisat::CRef& cr : mNegatives )
                        _ca.reloc( cr, _to );
                }
                
                size_t numOfNegatives() const
                {
                    return mNegatives.size();
                }
                
                size_t numOfPositives() const
                {
                    return mPositives.size();
                }
            };

            /// [Minisat related code]
            static inline VarData mkVarData( Minisat::CRef cr, int l )
            {
                VarData d = { cr, l };
                return d;
            }
            
            // Minisat related members.

            // Mode of operation:
            /// [Minisat related code]
            int verbosity;
            /// [Minisat related code]
            double var_decay;
            /// [Minisat related code]
            double clause_decay;
            /// [Minisat related code]
            double random_var_freq;
            /// [Minisat related code]
            double random_seed;
            /// [Minisat related code]
            bool   luby_restart;
            /// Controls conflict clause minimization (0=none, 1=basic, 2=deep).
            int ccmin_mode;
            /// Controls the level of phase saving (0=none, 1=limited, 2=full).
            int phase_saving;
            /// Use random polarities for branching heuristics.
            bool rnd_pol;
            /// Initialize variable activities with a small random value.
            bool rnd_init_act;
            /// The fraction of wasted memory allowed before a garbage collection is triggered.
            double garbage_frac;
            /// The initial restart limit. (default 100)
            int restart_first;
            /// The factor with which the restart limit is multiplied in each restart. (default 1.5)
            double restart_inc;
            /// The initial limit for learned clauses is a factor of the original clauses.(default 1 / 3)
            double learntsize_factor;
            /// The limit for learned clauses is multiplied with this factor each restart.(default 1.1)
            double learntsize_inc;
            /// [Minisat related code]
            int    learntsize_adjust_start_confl;
            /// [Minisat related code]
            double learntsize_adjust_inc;

            // Statistics: (read-only member variable)
            /// [Minisat related code]
            uint64_t solves, starts, decisions, rnd_decisions, propagations, conflicts;
            /// [Minisat related code]
            uint64_t dec_vars, clauses_literals, learnts_literals, max_literals, tot_literals;

            // Solver state:
            /// If FALSE, the constraints are already unsatisfiable. No part of the solver state may be used!
            bool ok;
            /// List of problem clauses.
            Minisat::vec<Minisat::CRef> clauses;
            /// List of problem clauses.
            Minisat::vec<Minisat::CRef> satisfiedClauses;
            /// List of learned clauses.
            Minisat::vec<Minisat::CRef> learnts;
            /// List of clauses which exclude a call resulted in unknown.
            Minisat::vec<Minisat::CRef> unknown_excludes;
            /// Amount to bump next clause with.
            double cla_inc;
            /// A heuristic measurement of the activity of a variable.
            Minisat::vec<double> activity;
            /// Amount to bump next variable with.
            double var_inc;
            /// 'watches[lit]' is a list of constraints watching 'lit' (will go there if literal becomes true).
            Minisat::OccLists<Minisat::Lit, Minisat::vec<Watcher>, WatcherDeleted> watches;
            /// The current assignments.
            Minisat::vec<Minisat::lbool> assigns;
            /// The preferred polarity of each variable.
            Minisat::vec<char> polarity;
            /// Declares if a variable is eligible for selection in the decision heuristic.
            Minisat::vec<char> decision;
            /// Assignment stack; stores all assignments made in the order they were made.
            Minisat::vec<Minisat::Lit> trail;
            /// Separator indices for different decision levels in 'trail'.
            Minisat::vec<int> trail_lim;
            /// Stores reason and level for each variable.
            Minisat::vec<VarData> vardata;
            /// Head of queue (as index into the trail -- no more explicit propagation queue in MiniSat).
            int qhead;
            /// Number of top-level assignments since last execution of 'simplify()'.
            int simpDB_assigns;
            /// Remaining number of propagations that must be made before next execution of 'simplify()'.
            int64_t simpDB_props;
            /// Current set of assumptions provided to solve by the user.
            Minisat::vec<Minisat::Lit> assumptions;
            /// A priority queue of variables ordered with respect to the variable activity.
            Minisat::Heap<VarOrderLt> order_heap;
            /// Set by 'search()'.
            double progress_estimate;
            /// Indicates whether possibly inefficient linear scan for satisfied clauses should be performed in 'simplify'.
            bool remove_satisfied;
            /// [Minisat related code]
            Minisat::ClauseAllocator ca;

            // Temporaries (to reduce allocation overhead).
            /// Each variable is prefixed by the method in which it is used, except 'seen' which is used in several places.
            Minisat::vec<char> seen;
            /// [Minisat related code]
            Minisat::vec<Minisat::Lit> analyze_stack;
            /// [Minisat related code]
            Minisat::vec<Minisat::Lit> analyze_toclear;
            /// [Minisat related code]
            Minisat::vec<Minisat::Lit> add_tmp;
            /// [Minisat related code]
            double max_learnts;
            /// [Minisat related code]
            double learntsize_adjust_confl;
            /// [Minisat related code]
            int learntsize_adjust_cnt;

            // Resource constraints:
            /// -1 means no budget.
            int64_t conflict_budget;
            /// -1 means no budget.
            int64_t propagation_budget;
            /// [Minisat related code]
            bool asynch_interrupt;
            /// For temporary usage.
            Minisat::vec<Minisat::Lit> learnt_clause;

            // Module related members.
            /// A flag, which is set to true, if anything has been changed in the passed formula between now and the last consistency check.
            bool mChangedPassedFormula;
			/// A flag, which is set to true, if all satisfying assignments should be computed.
			bool mComputeAllSAT;
            ///
            bool mFullAssignmentCheckedForConsistency;
            ///
            bool mOptimumComputed;
            /**
             * Stores gained information about the current assignment's consistency. If we know from the last consistency check, whether the
             * current assignment is consistent, this member is SAT, if we know that it is inconsistent it is UNSAT, otherwise Unknown.
             */
            Answer mCurrentAssignmentConsistent;
            /// The number of full laze calls made.
            size_t mNumberOfFullLazyCalls;
            /// The number of restarts made.
            int mCurr_Restarts;
            /// The number of theory calls made.
            unsigned mNumberOfTheoryCalls;
            ///
            bool mReceivedFormulaPurelyPropositional;
            /**
             * Maps each Minisat variable to a pair of Abstractions, one contains the abstraction information of the literal
             * being the variable and one contains the abstraction information of the literal being the variables negation.
             */
            BooleanConstraintMap mBooleanConstraintMap;
            /**
             * Maps the constraints occurring in the SAT module to their abstractions. We store a vector of literals
             * for each constraints, which is only used in the optimization, which applies valid substitutions.
             */
            ConstraintLiteralsMap mConstraintLiteralMap;
            /// Maps the Boolean variables to their corresponding Minisat variable.
            BooleanVarMap mBooleanVarMap;
            /// Maps the Minisat variables to their corresponding boolean variable.
            MinisatVarMap mMinisatVarMap;
            ///
            carl::FastMap<FormulaT, Minisat::Lit> mFormulaAssumptionMap;
            /// Maps the clauses in the received formula to the corresponding Minisat clause and Minisat literal.
            FormulaCNFInfosMap mFormulaCNFInfosMap;
            /// If problem is unsatisfiable (possibly under assumptions), this vector represent the final conflict clause expressed in the assumptions.
            ClauseSet mLearntDeductions;
            ///
            carl::FastMap<Minisat::CRef,ClauseInformation> mClauseInformation;
            ///
            std::unordered_map<int,std::unordered_set<Minisat::CRef>> mLiteralClausesMap;
            ///
            size_t mNumberOfSatisfiedClauses;
            /// Stores all Literals for which the abstraction information might be changed.
            std::vector<signed> mChangedBooleans;
            /// Is true, if we have to communicate all activities to the backends. (This might be the case after rescaling the activities.)
            bool mAllActivitiesChanged;
            /// Stores all clauses in which the activities have been changed.
            std::vector<signed> mChangedActivities;
            /// Stores the just introduced Boolean variables for theory splitting decisions.
            std::vector<signed> mNewSplittingVars;
            /// Stores for each variable the corresponding formulas which control its value
            VarLemmaMap mPropagatedLemmas;
			/// Stores Minisat indexes of all relevant variables
			std::vector<int> mRelevantVariables;
            ///
            Minisat::vec<unsigned> mNonTseitinShadowedOccurrences;
            ///
            TseitinVarShadows mTseitinVarShadows;
            ///
            carl::FastMap<FormulaT, TseitinVarShadows::iterator> mFormulaTseitinVarMap;
            ///
            carl::FastMap<int, FormulaT> mTseitinVarFormulaMap;
            ///
            std::vector<Minisat::vec<Minisat::Lit>> mCurrentTheoryConflicts;
            ///
            std::vector<unsigned> mCurrentTheoryConflictTypes;
            ///
            std::map<std::pair<size_t,size_t>,size_t> mCurrentTheoryConflictEvaluations;
            ///
            std::unordered_set<int> mLevelCounter;
            ///
            size_t mTheoryConflictIdCounter;
            ///
            ModuleInput::iterator mUpperBoundOnMinimal;
            ///
            std::vector<LiteralClauses> mLiteralsClausesMap;
            ///
            std::vector<std::pair<size_t,size_t>> mLiteralsActivOccurrences;
            ///
            std::vector<Minisat::Lit> mPropagationFreeDecisions;
            
            #ifdef SMTRAT_DEVOPTION_Statistics
            /// Stores all collected statistics during solving.
            SATModuleStatistics* mpStatistics;
            #endif

        public:
			typedef Settings SettingsType;
			std::string moduleName() const {
				return SettingsType::moduleName;
			}

            /**
             * Constructs a SATModule.
             * @param _type The type of this module being SATModule.
             * @param _formula The formula passed to this module, called received formula.
             * @param _settings [Not yet used.]
             * @param _foundAnswer Vector of Booleans: If any of them is true, we have to terminate a running check procedure.
             * @param _manager A reference to the manager of the solver using this module.
             */
            SATModule( const ModuleInput* _formula, RuntimeSettings* _settings, Conditionals& _foundAnswer, Manager* const _manager = NULL );

            /**
             * Destructs this SATModule.
             */
            ~SATModule();

            // Interfaces.
            
            /**
             * The module has to take the given sub-formula of the received formula into account.
             * @param _subformula The sub-formula to take additionally into account.
             * @return false, if it can be easily decided that this sub-formula causes a conflict with
             *          the already considered sub-formulas;
             *          true, otherwise.
             */
            bool addCore( ModuleInput::const_iterator );
            
            /**
             * Checks the received formula for consistency.
             * @return SAT,    if the received formula is satisfiable;
             *         UNSAT,   if the received formula is not satisfiable;
             *         Unknown, otherwise.
             */
            Answer checkCore();
            
            /**
             * Removes everything related to the given sub-formula of the received formula.
             * @param _subformula The sub formula of the received formula to remove.
             */
            void removeCore( ModuleInput::const_iterator );
            
            /**
             * Updates the model, if the solver has detected the consistency of the received formula, beforehand.
             */
            void updateModel() const;

			/**
             * Updates all satisfying models, if the solver has detected the consistency of the received formula, beforehand.
             */
            void updateAllModels();
            
            /**
             * Updates the infeasible subset found by the SATModule, if the received formula is unsatisfiable.
             */
            void updateInfeasibleSubset();
            
            void cleanUpAfterOptimizing( const std::vector<Minisat::CRef>& _excludedAssignments );
            
            void removeUpperBoundOnMinimal();
            
            /**
             * Adds the Boolean assignments to the given assignments, which were determined by the Minisat procedure.
             * Note: Assignments in the given map are not overwritten.
             * @param _rationalAssignment The assignments to add the Boolean assignments to.
             */
            void addBooleanAssignments( EvalRationalMap& _rationalAssignment ) const;

            /**
             * Prints everything.
             * @param _out  The output stream where the answer should be printed.
             * @param _init The line initiation.
             */
            void print( std::ostream& _out = std::cout, const std::string _init = "" ) const;
            
            /**
             * Prints the current assignment of the SAT solver.
             * @param _out  The output stream where the answer should be printed.
             * @param _init The line initiation.
             */
            void printCurrentAssignment( std::ostream& = std::cout, const std::string = "" ) const;
            
            /**
             * Prints the constraints to literal map.
             * @param _out  The output stream where the answer should be printed.
             * @param _init The line initiation.
             */
            void printConstraintLiteralMap( std::ostream& _out = std::cout, const std::string _init = "" ) const;
            
            /**
             * Prints the formulas to clauses map.
             * @param _out  The output stream where the answer should be printed.
             * @param _init The line initiation.
             */
            void printFormulaClausesMap( std::ostream& _out = std::cout, const std::string _init = "" ) const;
            
            /**
             * Prints the clause information.
             * @param _out  The output stream where the answer should be printed.
             * @param _init The line initiation.
             */
            void printClauseInformation( std::ostream& _out = std::cout, const std::string _init = "" ) const;
            
            /**
             * Prints map of the Boolean within the SAT solver to the given Booleans.
             * @param _out  The output stream where the answer should be printed.
             * @param _init The line initiation.
             */
            void printBooleanVarMap( std::ostream& _out = std::cout, const std::string _init = "" ) const;
            
            /**
             * Prints the literal to constraint map.
             * @param _out  The output stream where the answer should be printed.
             * @param _init The line initiation.
             */
            void printBooleanConstraintMap( std::ostream& _out = std::cout, const std::string _init = "" ) const;
            
            /**
             * Prints the clause at the given reference.
             * @param _clause The reference of the clause.
             * @param _withAssignment A flag indicating if true, that the assignments should be printed too.
             * @param _out The output stream where the answer should be printed.
             * @param _init The prefix of each printed line.
             */
            void printClause( Minisat::CRef _clause, bool _withAssignment = false, std::ostream& _out = std::cout, const std::string& _init = "" ) const;
            
            /**
             * Prints the clause resulting from the given vector of literals.
             * @param _clause The reference of the clause.
             * @param _withAssignment A flag indicating if true, that the assignments should be printed too.
             * @param _out The output stream where the answer should be printed.
             * @param _init The prefix of each printed line.
             */
            void printClause( const Minisat::vec<Minisat::Lit>&, bool _withAssignment = false, std::ostream& _out = std::cout, const std::string& _init = "" ) const;
            
            /**
             * Prints all given clauses.
             * @param _clauses The clauses to print.
             * @param _name The name of the clauses to print. (e.g. learnts, clauses, ..)
             * @param _out The output stream where the answer should be printed.
             * @param _init The prefix of each printed line.
             * @param _from The position of the first clause to print within the given vector of clauses.
             * @param _withAssignment A flag indicating if true, that the assignments should be printed too.
             */
            void printClauses( const Minisat::vec<Minisat::CRef>& _clauses, const std::string _name, std::ostream& _out = std::cout, const std::string _init = "", int = 0, bool _withAssignment = false, bool _onlyNotSatisfied = false ) const;
            
            /**
             * Prints the decisions the SAT solver has made.
             * @param _out  The output stream where the answer should be printed.
             * @param _init The line initiation.
             */
            void printDecisions( std::ostream& _out = std::cout, std::string _init = "" ) const;

            /**
             * Prints the propagated lemmas for each variables which influence its value.
             * @param _out  The output stream where the answer should be printed.
             * @param _init The line initiation.
             */
            void printPropagatedLemmas( std::ostream& _out = std::cout, std::string _init = "" ) const;
            
            /**
             * Prints the literals' active occurrences in all clauses.
             * @param _out  The output stream where the answer should be printed.
             * @param _init The line initiation.
             */
            void printLiteralsActiveOccurrences( std::ostream& _out = std::cout, std::string _init = "" ) const;

            /**
             * Collects the taken statistics.
             */
            void collectStats();

        private:
            // Problem specification:
            
            /**
             * Creates a new SAT variable in the solver. If 'decision' is cleared, variable will not be
             * used as a decision variable (NOTE! This has effects on the meaning of a SATISFIABLE result).
             * @param polarity A flag, which is true, if the variable preferably is assigned to false.
             * @param dvar A flag, which is true, if the variable to create needs to considered in the solving.
             * @param _activity The initial activity of the variable to create.
             * @param _tseitinShadowed A flag, which is true, if the variable to create is a sub-formula of a formula represented by a Tseitin variable.
             * @return The created Minisat variable.
             */
            Minisat::Var newVar( bool polarity = true, bool dvar = true, double _activity = 0 );

            // Solving:
            
            /**
             * Removes already satisfied clauses and performs simplifications on all clauses.
             */
            void simplify();
            
            /**
             * Adds the clause of the given type with the given literals to the clauses managed by Minisat.
             * @param _clause The clause to add.
             * @param _type The type of the clause (NORMAL_CLAUSE, DEDUCTED_CLAUSE or CONFLICT_CLAUSE).
             * @param _force If true backtracking won't stop at the first assumption-decision-level.
             * @return  true, if a clause has been added;
             *          false, otherwise.
             */
            bool addClause( const Minisat::vec<Minisat::Lit>& _clause, unsigned _type = 0, bool _force = false );
            
            /**
             * Moves two literals which are not assigned to false to the beginning of the clause.
             * If only one literal is not assigned to false, it is moved to the beginning.
             * If all literals are false, the first literal is one of the literals with the highest decision level.
             * If all literals are false but the first one, the second literal has the highest decision level.
             * @param _clause The clause in which the literals shall be reordered.
             */
            void arrangeForWatches( Minisat::Clause& _clause );
            
            void removeLiteralOrigin( Minisat::Lit _litToRemove, const FormulaT& _origin );
            
            /**
             * @return FALSE means solver is in a conflicting state
             */
            inline bool okay() const
            {
                return ok;
            }

            // Variable mode:
            
            /**
             * Declare which polarity the decision heuristic should use for a variable. Requires mode 'polarity_user'.
             * @param v The variable to set the polarity for.
             * @param b true, if the variables should be preferably assigned to false.
             */
            inline void setPolarity( Minisat::Var v, bool b )
            {
                polarity[v] = b;
            }
            
            /**
             * Declare if a variable should be eligible for selection in the decision heuristic.
             * @param v The variable to change the eligibility for selection in the decision heuristic.
             * @param b true, if the variable should be eligible for selection in the decision heuristic.
             */
            inline void setDecisionVar( Minisat::Var v, bool b )
            {
                if( b &&!decision[v] )
                    dec_vars++;
                else if( !b && decision[v] )
                    dec_vars--;
                decision[v] = b;
                insertVarOrder( v );
            }

            // Read state:
            
            /**
             * @param x The variable to get its value for.
             * @return The current value of a variable.
             */
            inline Minisat::lbool value( Minisat::Var x ) const
            {
                return assigns[x];
            }
            
            /**
             * @param p The literal to get its value for.
             * @return The current value of a literal.
             */
            inline Minisat::lbool value( Minisat::Lit p ) const
            {
                return assigns[Minisat::var( p )] ^ Minisat::sign( p );
            }
            
            /**
             * @return The current number of assigned literals.
             */
            inline int nAssigns() const
            {
                return trail.size();
            }
            
            /**
             * @return The current number of original clauses.
             */
            inline int nClauses() const
            {
                return clauses.size();
            }
            
            /**
             * @return The current number of learned clauses.
             */
            inline int nLearnts() const
            {
                return learnts.size();
            }
            
            /**
             * @return The current number of variables.
             */
            inline int nVars() const
            {
                return vardata.size();
            }
            
            /**
             * @return [Minisat related code]
             */
            inline int nFreeVars() const
            {
                return (int)dec_vars - (trail_lim.size() == 0 ? trail.size() : trail_lim[0]);
            }

            // Resource constraints:
            
            /**
             * [Minisat related code]
             * @param x [Minisat related code]
             */
            inline void setConfBudget( int64_t x )
            {
                conflict_budget = (int64_t)conflicts + x;
            }
            
            /**
             * [Minisat related code]
             * @param x [Minisat related code]
             */
            inline void setPropBudget( int64_t x )
            {
                propagation_budget = (int64_t)propagations + x;
            }
            
            /**
             * [Minisat related code]
             */
            inline void budgetOff()
            {
                conflict_budget = propagation_budget = -1;
            }
            
            /**
             * Trigger a (potentially asynchronous) interruption of the solver.
             */
            inline void interrupt()
            {
                asynch_interrupt = true;
            }
            
            /**
             * Clear interrupt indicator flag.
             */
            inline void clearInterrupt()
            {
                asynch_interrupt = false;
            }

            // Memory management:
            
            /**
             * [Minisat related code]
             */
            void garbageCollect();
            
            /**
             * [Minisat related code]
             * @param gf [Minisat related code]
             */
            inline void checkGarbage( double gf )
            {
                if( ca.wasted() > ca.size() * gf )
                    garbageCollect();
            }
            
            /**
             * [Minisat related code]
             */
            void checkGarbage( void )
            {
                return checkGarbage( garbage_frac );
            }
            
            /**
             * [Minisat related code]
             * @param cout [Minisat related code]
             * @param [Minisat related code]
             */
            void printSatState( std::ostream& = std::cout, const std::string = "" );

            // Main internal methods:
            
            /**
             * Insert a variable in the decision order priority queue.
             * @param x [Minisat related code]
             */
            inline void insertVarOrder( Minisat::Var x )
            {
                if( !order_heap.inHeap( x ) && decision[x] )
                    order_heap.insert( x );
            }
            
            /**
             * [Minisat related code]
             */
            void decrementLearntSizeAdjustCnt();
            
            /**
             * @return true, if the current assignment is a full one.
             */
            bool fullAssignment();
            
            /**
             * @return The next decision variable meant to invoke a splitting..
             */
            Minisat::Var pickSplittingVar();
            
            /**
             * @return The next decision variable.
             */
            Minisat::Lit pickBranchLit();
            
            /**
             * @return The best decision variable under consideration of the decision heuristic.
             */
            Minisat::Lit bestBranchLit( bool _conflictFirst );
            
            /**
             * Begins a new decision level.
             */
            inline void newDecisionLevel()
            {
                trail_lim.push( trail.size() );
            }
            
            void decrementTseitinShadowOccurrences( signed _var )
            {
                unsigned& ntso = mNonTseitinShadowedOccurrences[_var];
                assert( ntso > 0 );
                --ntso;
                if( ntso == 0 )
                {
                    setDecisionVar( _var, false );
                }
            }
            
            void incrementTseitinShadowOccurrences( signed _var )
            {
                unsigned& ntso = mNonTseitinShadowedOccurrences[_var];
                if( ntso == 0 )
                {
                    setDecisionVar( _var, true );
                }
                ++ntso;
            }
            
            /**
             * Enqueue a literal. Assumes value of literal is undefined.
             * @param p The literal to enqueue. (The variable in the literal is set to true, if the literal is positive,
             *          and to false, if the literal is negative).s
             * @param from A reference to the clause, which implied this assignment.
             */
            void uncheckedEnqueue( Minisat::Lit p, Minisat::CRef from = Minisat::CRef_Undef );
            
            /**
             * Test if fact 'p' contradicts current state, enqueue otherwise.
             * NOTE: enqueue does not set the ok flag! (only public methods do)
             * @param p [Minisat related code]
             * @param from [Minisat related code]
             * @return [Minisat related code]
             */
            inline bool enqueue( Minisat::Lit p, Minisat::CRef from = Minisat::CRef_Undef )
            {
                return value( p ) != l_Undef ? value( p ) != l_False : (uncheckedEnqueue( p, from ), true);
            }
            
            /**
             * propagate : [void]  ->  [Clause*]
             *
             * Description:
             *   Propagates all enqueued facts. If a conflict arises, the conflicting clause is returned,
             *   otherwise CRef_Undef.
             *
             * Post-conditions:
             *   - the propagation queue is empty, even if there was a conflict.
             *
             * @return A reference to the conflicting clause, if a conflict has been detected.
             */
            Minisat::CRef propagate();
            
            /**
             * Revert to the state at given level (keeping all assignments at 'level' but not beyond).
             *
             * @param level The level to backtrack to.
             */
            void cancelUntil( int level, bool force = false );
            
            /**
             * Revert the variables assignment until a given level (keeping all assignments at 'level')
             *
             * @param level The level to backtrack to
             */
            void cancelAssignmentUntil( int level );
            void resetVariableAssignment( Minisat::Var _var );

            /**
             *  analyze : (confl : Clause*) (out_learnt : vec<Lit>&) (out_btlevel : int&)  ->  [void]
             *
             *  Description:
             *    Analyze conflict and produce a reason clause.
             *
             *    Pre-conditions:
             *      - 'out_learnt' is assumed to be cleared.
             *      - Current decision level must be greater than root level.
             *
             *    Post-conditions:
             *      - 'out_learnt[0]' is the asserting literal at level 'out_btlevel'.
             *      - If out_learnt.size() > 1 then 'out_learnt[1]' has the greatest decision level of the
             *        rest of literals. There may be others from the same level though.
             *
             * @param confl A reference to the conflicting clause.
             * @param out_learnt The asserting clause derived by this method.
             * @param out_btlevel The level to backtrack to according the analysis of this method.
             */
            void analyze( Minisat::CRef confl, Minisat::vec<Minisat::Lit>& out_learnt, int& out_btlevel );
            
            
            /**
             * Check if 'p' can be removed. 'abstract_levels' is used to abort early if the algorithm is
             * visiting literals at levels that cannot be removed later.
             * @param p [Minisat related code]
             * @param abstract_levels [Minisat related code]
             * @return [Minisat related code]
             */
            bool litRedundant( Minisat::Lit p, uint32_t abstract_levels );
            
            /**
             * Adds clauses representing the lemmas which should be added to this SATModule. This may provoke backtracking.
             * @return true, if any clause has been added.
             */
            bool processLemmas();
            
            /**
             * Adds the clauses representing all conflicts generated by all backends.
             * @return A reference to the clause representing the best infeasible subset.
             */
            Minisat::CRef learnTheoryConflict( bool& _foundConflictOfSizeOne );
            
            void adaptConflictEvaluation( size_t& _clauseEvaluation, Minisat::Lit _lit, bool _firstLiteral );
            
            /**
             * Propagate and check the consistency of the constraints, whose abstraction literals have been assigned to true.
             * @param _madeTheoryCall A flag which is set to true, if at least one theory call has been made within this method.
             * @return A reference to a conflicting clause, if a clause has been added.
             */
            Minisat::CRef propagateConsistently( bool& _madeTheoryCall, bool& _foundConflictOfSizeOne  );
            
            /**
             * Checks the received formula for consistency.
             * @return l_True,  if the received formula is satisfiable;
             *         l_False, if the received formula is not satisfiable;
             *         l_Undef, otherwise.
             */
            Minisat::lbool checkFormula();
            
            void computeAdvancedLemmas();

            /**
             * search : (nof_conflicts : int) (params : const SearchParams&)  ->  [lbool]
             *
             *  Description:
             *    Search for a model the specified number of conflicts.
             *    NOTE! Use negative value for 'nof_conflicts' indicate infinity.
             *
             *  Output:
             *    'l_True' if a partial assignment that is consistent with respect to the clause set is found. If
             *    all variables are decision variables, this means that the clause set is satisfiable. 'l_False'
             *    if the clause set is unsatisfiable. 'l_Undef' if the bound on number of conflicts is reached.
             *
             * @param nof_conflicts The number of conflicts after which a restart is forced.
             * @return l_True, if the considered clauses are satisfiable;
             *         l_False, if the considered clauses are unsatisfiable;
             *         l_Undef, if it could not been detected whether the given set of clauses is satisfiable or not.
             */
            Minisat::lbool search( int nof_conflicts = 100 );

			/**
			 * Handles conflict
			 * @param conf conflict clause
			 * @return if return is not l_True, search can be aborted
             */
            void handleConflict( Minisat::CRef _confl );
//            Minisat::lbool handleConflict( Minisat::CRef conf, bool madeTheoryCall );
            
            /**
             * reduceDB : ()  ->  [void]
             *
             * Description:
             *   Remove half of the learnt clauses, minus the clauses locked by the current assignment. Locked
             *   clauses are clauses that are reason to some assignment. Binary clauses are never removed.
             */
            void reduceDB();
            
            void clearLearnts( int n );
            
            // Shrink 'cs' to contain only non-satisfied clauses.
            
            /**
             * Removes all satisfied clauses from the given vector of clauses, which should only be performed in decision level 0.
             * @param cs The vector of clauses wherein to remove all satisfied clauses.
             */
            void removeSatisfied( Minisat::vec<Minisat::CRef>& cs );
            
            /**
             * [Minisat related code]
             */
            void rebuildOrderHeap();
            
            // Maintaining Variable/Clause activity:
            
            /**
             * @return The maximum of all activities of the occurring literals.
             */
            inline double maxActivity() const
            {
                double result = 0;
                for( int i = 0; i < activity.size(); ++i )
                {
                    if( result < activity[i] )
                        result = activity[i];
                }
                return result;
            }
            
            /**
             * Decay all variables with the specified factor. Implemented by increasing the 'bump' value instead.
             */
            inline void varDecayActivity()
            {
                var_inc *= (1 / var_decay);
            }
            
            /**
             * Increase a variable with the current 'bump' value.
             * @param v [Minisat related code]
             * @param inc [Minisat related code]
             */
            inline void varBumpActivity( Minisat::Var v, double inc )
            {
                if( (activity[v] += inc) > 1e100 )
                {
                    // Rescale:
                    for( int i = 0; i < nVars(); i++ )
                        activity[i] *= 1e-100;
                    var_inc *= 1e-100;
                    if( !mReceivedFormulaPurelyPropositional )
                        mAllActivitiesChanged = true;
                }
                else if( !mReceivedFormulaPurelyPropositional )
                {
                    mChangedActivities.push_back( (signed) v );
                }

                // Update order_heap with respect to new activity:
                if( order_heap.inHeap( v ) )
                    order_heap.decrease( v );
            }
            
            /**
             * Increase a variable with the current 'bump' value.
             * @param v [Minisat related code]
             */
            inline void varBumpActivity( Minisat::Var v )
            {
                varBumpActivity( v, var_inc );
            }
            
            /**
             * Decay all clauses with the specified factor. Implemented by increasing the 'bump' value instead.
             */
            inline void claDecayActivity()
            {
                cla_inc *= (1 / clause_decay);
            }
            
            /**
             * Increase a clause with the current 'bump' value.
             * @param c [Minisat related code]
             */
            inline void claBumpActivity( Minisat::Clause& c )
            {
                if( (c.activity() += (float)cla_inc) > 1e20 )
                {
                    // Rescale:
                    for( int i = 0; i < learnts.size(); i++ )
                    {
                        ca[learnts[i]].activity() *= (float)1e-20;
                    }
                    cla_inc *= 1e-20;
                }
            }

            // Operations on clauses:
            
            /**
             * Attach a clause to watcher lists.
             * @param cr [Minisat related code]
             */
            void attachClause( Minisat::CRef cr );
            
            /**
             * Detach a clause to watcher lists.
             * @param cr [Minisat related code]
             * @param strict [Minisat related code]
             */
            void detachClause( Minisat::CRef cr, bool strict = false );
            
            /**
             * Detach and free a clause.
             * @param cr [Minisat related code]
             */
            void removeClause( Minisat::CRef cr );
            
            /**
             * @param c [Minisat related code]
             * @return TRUE if a clause is a reason for some implication in the current state.
             */
            inline bool locked( const Minisat::Clause& c ) const
            {
                return value( c[0] ) == l_True && reason( Minisat::var( c[0] ) ) != Minisat::CRef_Undef && ca.lea( reason( Minisat::var( c[0] ) ) ) == &c;
            }
            
            /**
             * @param c [Minisat related code]
             * @return TRUE if a clause is satisfied in the current state.
             */
            bool satisfied( const Minisat::Clause& c ) const;

            /**
             * [Minisat related code]
             * @param x [Minisat related code]
             * @param map [Minisat related code]
             * @param max [Minisat related code]
             * @return [Minisat related code]
             */
            static Minisat::Var mapVar( Minisat::Var x, Minisat::vec<Minisat::Var>& map, Minisat::Var& max );
            
            /**
             * Finite subsequences of the Luby-sequence:
             *
             * 0: 1
             * 1: 1 1 2
             * 2: 1 1 2 1 1 2 4
             * 3: 1 1 2 1 1 2 4 1 1 2 1 1 2 4 8
             * ...
             *
             * @param y
             * @param x
             *
             * @return
             */
            static double luby( double y, int x );
            
            /**
             * [Minisat related code]
             * @param to [Minisat related code]
             */
            void relocAll( Minisat::ClauseAllocator& to );

            // Misc:
            
            /**
             * @return The current decision level.
             */
            inline int decisionLevel() const
            {
                return trail_lim.size();
            }
            
            /**
             * Used to represent an abstraction of sets of decision levels.
             * @param x [Minisat related code]
             * @return [Minisat related code]
             */
            inline uint32_t abstractLevel( Minisat::Var x ) const
            {
                return 1 << (level( x ) & 31);
            }
            
            /**
             * @param x The variable to get the reason for it's assignment.
             * @return A reference to the clause, which implied the current assignment of this variable.
             *         It is not defined, if the assignment of the variable follows from a clause of size 0
             *         or if the variable is unassigned.
             */
            Minisat::CRef reason( Minisat::Var x ) const
            {
                return vardata[x].reason;
            }
            
            /**
             * @param x The variable for which we to get the level in which it has been assigned to a value.
             * @return The level in which the variable has been assigned, if it is not unassigned.
             */
            int level( Minisat::Var x ) const
            {
                return vardata[x].level;
            }
            
            /**
             * @param _clause The clause to get the highest decision level in which assigned one of its literals has been assigned. 
             * @return The highest decision level which assigned a literal of the given clause.
             */
            int level( const Minisat::vec< Minisat::Lit >& ) const;
            
            /**
             * @return An estimation of the progress the SAT solver has been made, depending on how many assignments have been excluded
             *         and how many assignments are in general possible. The calculated value is between 0 and 1.
             */
            double progressEstimate() const;
            
            /**
             * @return [Minisat related code]
             */
            inline bool withinBudget() const
            {
                return !asynch_interrupt && (conflict_budget < 0 || conflicts < (uint64_t)conflict_budget)
                       && (propagation_budget < 0 || propagations < (uint64_t)propagation_budget);
            }

            // Static helpers:

            /**
             * @param seed [Minisat related code]
             * @return A random float 0 <= x < 1. Seed must never be 0.
             */
            static inline double drand( double& seed )
            {
                seed *= 1389796;
                int q = (int)(seed / 2147483647);
                seed -= (double)q * 2147483647;
                return seed / 2147483647;
            }

            /**
             * @param seed [Minisat related code]
             * @param size [Minisat related code]
             * @return A random integer 0 <= x < size. Seed must never be 0.
             */
            static inline int irand( double& seed, int size )
            {
                return (int)(drand( seed ) * size);
            }
            
            void updateCNFInfoCounter( typename FormulaCNFInfosMap::iterator _iter, const FormulaT& _origin, bool _increment );
            
            void addClause_( const Minisat::vec<Minisat::Lit>& _clause, unsigned _type, const FormulaT& _original, std::vector<Minisat::CRef>& _addedClauses )
            {
                if( addClause( _clause, _type ) && _type == Minisat::NORMAL_CLAUSE )
                {
                    _addedClauses.push_back( clauses.last() );
                    auto cfRet = mClauseInformation.emplace( clauses.last(), ClauseInformation( clauses.size()-1 ) );
                    assert( cfRet.second );
                    cfRet.first->second.addOrigin( _original );
                }
            }
            
            Minisat::Lit addClauses( const FormulaT& _formula, unsigned _type, unsigned _depth = 0, const FormulaT& _original = FormulaT( carl::FormulaType::TRUE ), bool _polarity = false );
            void addXorClauses( const Minisat::vec<Minisat::Lit>& _literals, const Minisat::vec<Minisat::Lit>& _negLiterals, int _from, bool _numOfNegatedLitsEven, unsigned _type, Minisat::vec<Minisat::Lit>& _clause, bool _ignorePolarity, bool _polarity, const FormulaT& _original, std::vector<Minisat::CRef>& _addedClauses );
            
            /**
             * Creates or simply returns the literal belonging to the formula being the first argument. 
             * @param _formula The formula to get the literal for.
             * @param _origin The origin of the formula to get the literal for.
             * @param _decisionRelevant true, if the variable of the literal needs to be involved in the decision process of the SAT solving.
             * @return The corresponding literal.
             */
            Minisat::Lit getLiteral( const FormulaT& _formula, const FormulaT& _origin, bool _decisionRelevant = true );
            
            /**
             * Adapts the passed formula according to the current assignment within the SAT solver.
             * @return  true,   if the passed formula has been changed;
             *          false,  otherwise.
             */
            void adaptPassedFormula();
            
            /**
             * Adapts the passed formula according to the given abstraction information.
             * @param _abstr The information of a Boolean abstraction of a constraint (contains no 
             *               information, if the Boolean does not correspond to a constraint's abstraction).
             */
            void adaptPassedFormula( Abstraction& _abstr );

			/**
			 * Updates the model, if the solver has detected the consistency of the received formula, beforehand.
			 * @param model The model to update with the current assignment
			 * @param only_relevant_variables If true, only variables in mRelevantVariables are part of the model
			 */
			void updateModel( Model& model, bool only_relevant_variables = false ) const;
    };
}    // namespace smtrat
