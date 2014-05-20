/*
 * SMT-RAT - Satisfiability-Modulo-Theories Real Algebra Toolbox
 * Copyright (C) 2013 Florian Corzilius, Ulrich Loup, Erika Abraham, Sebastian Junges
 *
 * This file is part of SMT-RAT.
 *
 * SMT-RAT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SMT-RAT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SMT-RAT. If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * @file Manager.cpp
 *
 * @author  Florian Corzilius
 * @author  Ulrich Loup
 * @author  Sebastian Junges
 * @author  Henrik Schmitz
 * @since   2012-01-18
 * @version 2013-01-11
 */

#include "Manager.h"
#include "StrategyGraph.h"
#include "modules/Modules.h"

#include <typeinfo>
#include <cln/cln.h>

using namespace std;

namespace smtrat
{
    
    // Constructor.
    
    Manager::Manager():
        mPrimaryBackendFoundAnswer( vector< std::atomic_bool* >( 1, new std::atomic_bool( false ) ) ),
        mpPassedFormula( new ModuleInput() ),
        mBacktrackPoints(),
        mGeneratedModules(),
        mBackendsOfModules(),
        mpPrimaryBackend( new Module( MT_Module, mpPassedFormula, mPrimaryBackendFoundAnswer, this ) ),
        mStrategyGraph(),
        mDebugOutputChannel( cout.rdbuf() ),
        mLogic( Logic::UNDEFINED )
        #ifdef SMTRAT_DEVOPTION_Statistics
        ,
        mpStatistics( new GeneralStatistics() )
        #endif
        #ifdef SMTRAT_STRAT_PARALLEL_MODE
        ,
        mpThreadPool( NULL ),
        mNumberOfBranches( 0 ),
        mNumberOfCores( 1 ),
        mRunsParallel( false )
        #endif
    {
        mGeneratedModules.push_back( mpPrimaryBackend );
        mpModuleFactories = new map<const ModuleType, ModuleFactory*>();
        // inform it about all constraints
        for( auto constraint = constraintPool().begin(); constraint != constraintPool().end(); ++constraint )
            mpPrimaryBackend->inform( (*constraint) );
    }

    // Destructor.
    
    Manager::~Manager()
    {
        Module::storeAssumptionsToCheck( *this );
        #ifdef SMTRAT_DEVOPTION_Statistics
        delete mpStatistics;
        #endif
        while( !mGeneratedModules.empty() )
        {
            Module* ptsmodule = mGeneratedModules.back();
            mGeneratedModules.pop_back();
            delete ptsmodule;
        }
        while( !mpModuleFactories->empty() )
        {
            const ModuleFactory* pModuleFactory = mpModuleFactories->begin()->second;
            mpModuleFactories->erase( mpModuleFactories->begin() );
            delete pModuleFactory;
        }
        delete mpModuleFactories;
        while( !mPrimaryBackendFoundAnswer.empty() )
        {
            std::atomic_bool* toDelete = mPrimaryBackendFoundAnswer.back();
            mPrimaryBackendFoundAnswer.pop_back();
            delete toDelete;
        }
        #ifdef SMTRAT_STRAT_PARALLEL_MODE
        if( mpThreadPool!=NULL )
            delete mpThreadPool;
        #endif
        delete mpPassedFormula;
    }

    // Methods.

    #ifdef SMTRAT_STRAT_PARALLEL_MODE
    /**
     * Initializes some members of the manager, which are only needed for supporting parallel module calls.
     */
    void Manager::initialize()
    {
        mNumberOfBranches = mStrategyGraph.numberOfBranches();
        if( mNumberOfBranches > 1 )
        {
            mNumberOfCores = std::thread::hardware_concurrency();
            if( mNumberOfCores > 1 )
            {
                mStrategyGraph.setThreadAndBranchIds();
//                mStrategyGraph.tmpPrint();
//                std::this_thread::sleep_for(std::chrono::seconds(29));
                mRunsParallel = true;
                mpThreadPool = new ThreadPool( mNumberOfBranches, mNumberOfCores );
            }
        }
    }
    #endif
    
    /**
     * Prints the so far added formulas.
     * @param _out The stream to print on.
     */
    void Manager::printAssertions( ostream& _out ) const
    {
        _out << "(";
        if( mpPassedFormula->size() == 1 )
        {
            _out << *mpPassedFormula->back();
        }
        else
        {
            for( auto subFormula = mpPassedFormula->begin(); subFormula != mpPassedFormula->end(); ++subFormula )
            {
                _out << **subFormula << endl;
            }
        }
        _out << ")" << endl;
    }

    /**
     * Prints the first found infeasible subset of the set of received formulas.
     * @param _out The stream to print on.
     */
    void Manager::printInfeasibleSubset( ostream& _out ) const
    {
        _out << "(";
        if( !mpPrimaryBackend->infeasibleSubsets().empty() )
        {
            const PointerSet<Formula>& infSubSet = *mpPrimaryBackend->infeasibleSubsets().begin();
            if( infSubSet.size() == 1 )
            {
                _out << **infSubSet.begin();
            }
            else
            {
                for( auto subFormula = infSubSet.begin(); subFormula != infSubSet.end(); ++subFormula )
                {
                    _out << **subFormula << endl;
                }
            }
        }
        _out << ")" << endl;
    }
    
    /**
     * Get the backends to call for the given problem instance required by the given module.
     *
     * @param _formula     The problem instance.
     * @param _requiredBy  The module asking for a backend.
     * @param _foundAnswer A conditional
     *
     * @return  A vector of modules, which the module defined by _requiredBy calls in parallel to achieve
     *           an answer to the given instance.
     */
    vector<Module*> Manager::getBackends( Module* _requiredBy, atomic_bool* _foundAnswer )
    {
        #ifdef SMTRAT_STRAT_PARALLEL_MODE
        std::lock_guard<std::mutex> lock( mBackendsMutex );
        #endif
        vector<Module*>  backends;
        vector<Module*>& allBackends = mBackendsOfModules[_requiredBy];
        /*
         * Get the types of the modules, which the given module needs to call to solve its passedFormula.
         */
        vector< pair< thread_priority, ModuleType > > backendValues = mStrategyGraph.getNextModuleTypes( _requiredBy->threadPriority().second, _requiredBy->pPassedFormula()->properties() );
        for( auto iter = backendValues.begin(); iter != backendValues.end(); ++iter )
        {
            assert( iter->second != _requiredBy->type() );
            // If for this module type an instance already exists, we just add it to the modules to return.
            vector<Module*>::iterator backend = allBackends.begin();
            while( backend != allBackends.end() )
            {
                if( (*backend)->threadPriority() == iter->first )
                {
                    // The backend already exists.
                    backends.push_back( *backend );
                    break;
                }
                ++backend;
            }
            // Otherwise, we create a new instance of this module type and add it to the modules to return.
            if( backend == allBackends.end() )
            {
                auto backendFactory = mpModuleFactories->find( iter->second );
                assert( backendFactory != mpModuleFactories->end() );
                vector< atomic_bool* > foundAnswers = vector< atomic_bool* >( _requiredBy->answerFound() );
                foundAnswers.push_back( _foundAnswer );
                Module* pBackend = backendFactory->second->create( iter->second, _requiredBy->pPassedFormula(), foundAnswers, this );
                mGeneratedModules.push_back( pBackend );
                pBackend->setId( (unsigned)mGeneratedModules.size()-1 );
                pBackend->setThreadPriority( iter->first );
                allBackends.push_back( pBackend );
                backends.push_back( pBackend );
                // Inform it about all constraints.
                for( auto cons = _requiredBy->informedConstraints().begin(); cons != _requiredBy->informedConstraints().end(); ++cons )
                {
                    pBackend->inform( *cons );
                }
            }
        }
        return backends;
    }

    #ifdef SMTRAT_STRAT_PARALLEL_MODE
    /**
     * 
     * @param _pModule
     * @return
     */
    std::future<Answer> Manager::submitBackend( Module* _pModule )
    {
        assert( mRunsParallel );
        return mpThreadPool->submitBackend( _pModule );
    }

    /**
     * 
     * @param _pModule
     */
    void Manager::checkBackendPriority( Module* _pModule )
    {
        assert( mRunsParallel );
        mpThreadPool->checkBackendPriority( _pModule );
    }
    #endif
}    // namespace smtrat
