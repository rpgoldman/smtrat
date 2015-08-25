/*
 * File:   CNFerModule.cpp
 * Author: Florian Corzilius
 *
 * Created on 02. May 2012, 20:53
 */

#include "../../solver/Manager.h"
#include "CNFerModule.h"

using namespace std;
using namespace carl;

namespace smtrat
{
    CNFerModule::CNFerModule( ModuleType _type, const ModuleInput* _formula, RuntimeSettings*, Conditionals& _conditionals, Manager* const _manager ):
        Module( _type, _formula, _conditionals, _manager )
    {
        #ifdef SMTRAT_DEVOPTION_Statistics
        stringstream s;
        s << moduleName( type() ) << "_" << id();
        mpStatistics = new CNFerModuleStatistics( s.str() );
        #endif
    }

    CNFerModule::~CNFerModule(){}

    Answer CNFerModule::checkCore( bool _full )
    {
        auto receivedSubformula = firstUncheckedReceivedSubformula();
        while( receivedSubformula != rReceivedFormula().end() )
        {
            /*
             * Add the currently considered formula of the received constraint as clauses
             * to the passed formula.
             */
//            const Formula* formulaQF = (*receivedSubformula)->toQF(mpManager->quantifiedVariables());
//            const Formula* formulaToAssertInCnf = formulaQF->toCNF( true );
//            cout << (**receivedSubformula) << endl;
            FormulaT formulaToAssertInCnf = receivedSubformula->formula().toCNF( true, true, true );
            if( formulaToAssertInCnf.getType() == TRUE )
            {
                // No need to add it.
            }
            else if( formulaToAssertInCnf.getType() == FALSE )
            {
                FormulaSetT reason;
                reason.insert( receivedSubformula->formula() );
                mInfeasibleSubsets.push_back( reason );
                return False;
            }
            else
            {
                if( formulaToAssertInCnf.getType() == AND )
                {
                    for( const FormulaT& subFormula : formulaToAssertInCnf.subformulas()  )
                    {
                        #ifdef SMTRAT_DEVOPTION_Statistics
                        mpStatistics->addClauseOfSize( subFormula.size() );
                        #endif
                        addSubformulaToPassedFormula( subFormula, receivedSubformula->formula() );
                    }
                }
                else
                {
                    #ifdef SMTRAT_DEVOPTION_Statistics
                    mpStatistics->addClauseOfSize( receivedSubformula->formula().size() );
                    #endif
                    addSubformulaToPassedFormula( formulaToAssertInCnf, receivedSubformula->formula() );
                }
            }
            ++receivedSubformula;
        }
        //No given formulas is SAT but only if no other run was before
        if( rPassedFormula().empty() && solverState() == Unknown )
        {
            return True;
        }
        else
        {
            #ifdef SMTRAT_DEVOPTION_Statistics
            carl::Variables avars;
            rPassedFormula().arithmeticVars( avars );
            mpStatistics->nrOfArithVariables() = avars.size();
            carl::Variables bvars;
            rPassedFormula().booleanVars( bvars );
            mpStatistics->nrOfBoolVariables() = bvars.size();
            #endif
            Answer a = runBackends( _full );

            if( a == False )
            {
                getInfeasibleSubsets();
            }
            return a;
        }
    }
}    // namespace smtrat
