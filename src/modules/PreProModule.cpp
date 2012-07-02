/*
 *  SMT-RAT - Satisfiability-Modulo-Theories Real Algebra Toolbox
 * Copyright (C) 2012 Florian Corzilius, Ulrich Loup, Erika Abraham, Sebastian Junges
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SMT-RAT.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 * File:   PreProModule.cpp
 * Author: Dennis Scully
 *
 * Created on 23. April 2012, 14:53
 */

#include "../Manager.h"
#include "PreProModule.h"

using namespace std;
using namespace GiNaC;

namespace smtrat
{
    /**
     * Constructor
     */
    PreProModule::PreProModule( Manager* const _tsManager, const Formula* const _formula ):
        Module( _tsManager, _formula ),
        mFreshConstraintReceived( false ),
        mReceivedConstraints( std::vector<const Constraint* >() ),
        mConstraintOrigins( std::vector<const Formula* >() ),
        mConstraintBacktrackPoints( std::vector< pair< pair< bool, unsigned >, pair< unsigned, unsigned > > >() ),
        mNewFormulaReceived( false ),
        mNumberOfComparedConstraints( 0 ),
        mNumberOfCheckedFormulas( 0 ),
        mSubstitutionOrigins( std::vector< vec_set_const_pFormula >() ),
        mNumberOfVariables( std::map< std::string, unsigned >() ),
        mSubstitutions( std::vector< pair< pair< std::string, bool >, pair< pair< GiNaC::symtab, GiNaC::symtab >, pair< GiNaC::ex, GiNaC::ex> > > >() )    
    {
        this->mModuleType = MT_PreProModule;
    }

    /**
     * Destructor:
     */
    PreProModule::~PreProModule(){}

    /**
     * Methods:
     */

    /**
     * Adds a constraint to this modul.
     *
     * @param _constraint The constraint to add to the already added constraints.
     *
     * @return  true,   if the constraint and all previously added constraints are consistent;
     *          false,  if the added constraint or one of the previously added ones is inconsistent.
     */
    bool PreProModule::assertSubFormula( const Formula* const _formula )
    {
        addReceivedSubformulaToPassedFormula( getPositionOfReceivedFormula( _formula ) );
        _formula->FormulaToConstraints( mReceivedConstraints );
        if( mReceivedConstraints.size() != mConstraintOrigins.size() ) mFreshConstraintReceived = true;
        while( mReceivedConstraints.size() > mConstraintOrigins.size() )
        {
            mConstraintOrigins.push_back( _formula );
        }     
        mNewFormulaReceived = true;
        return true;
    }

    /**
     * Checks the so far received constraints for consistency.
     *
     * @return  True,    if the conjunction of received constraints is consistent;
     *          False,   if the conjunction of received constraints is inconsistent;
     *          Unknown, otherwise.
     */
    Answer PreProModule::isConsistent()
    {
        if( mNewFormulaReceived )
        {
            simplifyConstraints();
            if( mFreshConstraintReceived )
            {
                addLearningClauses();
            }
            proceedSubstitution();
        }
    
        Answer a = runBackends();
        if( a == False )
        {
            getInfeasibleSubsets();
        }

        return a;
    }
    
    void PreProModule::simplifyConstraints()
    {
        std::vector<const Constraint*>  essentialConstraints;
        for( Formula::const_iterator iterator = passedFormulaBegin(); iterator != passedFormulaEnd(); ++iterator )
        {
            if( (*iterator).getType() == REALCONSTRAINT ) essentialConstraints.push_back( (*iterator) );   
        }
        for( Formula::const_iterator iterator = passedFormulaBegin(); iterator != passedFormulaEnd(); ++iterator )
        {
            std::vector<const Constraint*> constraints;
            (*iterator).FormulaToConstraints( constraints );
            for( unsigned i = 0; i < constraints.size(); ++i )
            {
                for( unsigned j = i+1; j < constraints.size(); ++j )
                {
                    for( unsigned k = 0; k < essentialConstraints.size(); ++k )
                    {
                        if( Constraint::combineConstraints( constraints.at( i ), constraints.at( j ), essentialConstraints.at( k ) ) )
                        {
                            removeSubformulaFromPassedFormula( iterator );
                        }
                    }
                }
            }
        }
    }
     
    void PreProModule::addLearningClauses()
    {
        for( unsigned posConsA = mNumberOfComparedConstraints; posConsA < mReceivedConstraints.size(); ++posConsA )
        {
            const Constraint* tempConstraintA = mReceivedConstraints.at( posConsA );
            mNumberOfComparedConstraints++;
            for( unsigned posConsB = 0; posConsB < posConsA; ++posConsB )
            {
                const Constraint* tempConstraintB = mReceivedConstraints.at( posConsB );
                Formula* _tSubformula = NULL;
                switch( Constraint::compare( *tempConstraintA, *tempConstraintB ) )
                {
                    case 1: // not A or B
                    {
                        _tSubformula = new Formula( OR );
                        Formula* tmpFormula = new Formula( NOT );
                        tmpFormula->addSubformula( new Formula( tempConstraintA ) );
                        _tSubformula->addSubformula( tmpFormula );
                        _tSubformula->addSubformula( new Formula( tempConstraintB ) );
                        break;
                    }

                    case -1: // not B or A
                    {
                        _tSubformula = new Formula( OR );
                        Formula* tmpFormula = new Formula( NOT );
                        tmpFormula->addSubformula( new Formula( tempConstraintB ) );
                        _tSubformula->addSubformula( tmpFormula );
                        _tSubformula->addSubformula( new Formula( tempConstraintA ) );
                        break;
                    }
                    case -2: // not A or not B
                    {
                        _tSubformula = new Formula( OR );
                        Formula* tmpFormulaA = new Formula( NOT );
                        tmpFormulaA->addSubformula( new Formula( tempConstraintA ) );
                        Formula* tmpFormulaB = new Formula( NOT );
                        tmpFormulaB->addSubformula( new Formula( tempConstraintB ) );
                        _tSubformula->addSubformula( tmpFormulaA );
                        _tSubformula->addSubformula( tmpFormulaB );
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
                if( _tSubformula != NULL )
                {
                    // Create Origins
                    std::set< const Formula* > originset;
                    originset.insert( mConstraintOrigins.at( posConsA ) );
                    originset.insert( mConstraintOrigins.at( posConsB ) );
                    vec_set_const_pFormula origins = vec_set_const_pFormula();
                    origins.push_back( originset );
                    // Add learned Subformula and Origins to PassedFormula
                    addSubformulaToPassedFormula( _tSubformula, origins );
                }
            }
        }
        mFreshConstraintReceived = false;
    }
    
    void PreProModule::proceedSubstitution()
    {
        //--------------------------------------------------------------------------- Apply Old Substitutions on new Formulas
        unsigned t_passedFormulaSize = passedFormulaSize();
        for( unsigned i = mNumberOfCheckedFormulas; i < t_passedFormulaSize; ++i)
        {
            for( unsigned j = 0; j < mSubstitutions.size(); ++j)
            {
                if( substituteConstraint( passedFormulaAt( i ), mSubstitutions.at( j ), mSubstitutionOrigins.at( j ) ) ) // If substitution was succesfull manipulate count variables
                {
                    --i;                                                             //    c1,c2,c3 =>  c1,c3,c2'  (only c3 should be checked)
                    --t_passedFormulaSize;                                           //        c2 is substituted to c2'
                }
            }
        }
        //--------------------------------------------------------------------------- Update Number of occurences of variables
        for( unsigned i = mNumberOfCheckedFormulas; i != passedFormulaSize(); ++i)
        {
            const Formula* father = pPassedFormula()->at( i );
            pair< const Formula*, const Formula* > _pair = isCandidateforSubstitution( father );
            const Formula* constraintfather = _pair.first;
            if( (father != NULL) && (constraintfather != NULL) && (_pair.second != NULL) && constraintfather->constraint().relation() == CR_EQ )
            {
                assert( constraintfather->getType() == REALCONSTRAINT);
                GiNaC::symtab var = constraintfather->constraint().variables();
                for( GiNaC::symtab::const_iterator iteratorvar = var.begin();
                        iteratorvar != var.end(); ++iteratorvar)
                {
                    bool foundvar = false;
                    for( map< std::string, unsigned >::iterator iteratormap = mNumberOfVariables.begin();
                        iteratormap != mNumberOfVariables.end(); ++iteratormap )
                    {
                        if( iteratormap->first == iteratorvar->first )
                        {
                            mNumberOfVariables[ iteratorvar->first ] = iteratormap->second + 1;
                            foundvar = true;
                            break;
                        }
                    }
                    if( !foundvar )
                    {
                        mNumberOfVariables[ iteratorvar->first ] = 1;
                    }
                }
            }
        }
        //--------------------------------------------------------------------------- Search in new Formulas for Substitutions
        for( unsigned i = mNumberOfCheckedFormulas; i != passedFormulaSize(); ++i)
        {
            const Formula* father = pPassedFormula()->at( i );
            pair< const Formula*, const Formula* > _pair = isCandidateforSubstitution( father );
            const Formula* constraintfather = _pair.first;
            const Formula* boolfather = _pair.second;
            vec_set_const_pFormula _origins;
            if( (father != NULL) && (constraintfather != NULL) && (boolfather != NULL) && constraintfather->constraint().relation() == CR_EQ )
            {
                assert( constraintfather->getType() == REALCONSTRAINT);
                GiNaC::ex expression = constraintfather->constraint().lhs();
                GiNaC::symtab var = constraintfather->constraint().variables();
                GiNaC::ex maxvarex;
                GiNaC::ex varex;
                std::string maxvarstr;
                unsigned maxocc = 0;
                for( GiNaC::symtab::const_iterator iteratorvar = var.begin();                        // Seach for variable with the most occurences
                        iteratorvar != var.end(); ++iteratorvar)
                {
                    varex = (*iteratorvar).second;
                    if( (!expression.coeff( varex, 1 ).is_zero()) && (!expression.coeff( varex, 0 ).is_zero()) )
                    {
                        if( mNumberOfVariables.at( (*iteratorvar).first ) > maxocc )
                        {
                            maxvarstr = (*iteratorvar).first;
                            maxvarex = (*iteratorvar).second;
                            maxocc = mNumberOfVariables.at( (*iteratorvar).first );
                        }
                        // cout << mNumberOfVariables.at( (*iteratorvar).first ) << ": curvar   " << maxocc << ": lastvar   ";
                    }
                }
                if( !maxvarex.is_zero() )
                {
                    // Create new Expression for Substitution
                    GiNaC::ex t_expression = - expression.coeff( maxvarex, 0 ) / expression.coeff( maxvarex, 1 );
                    // Create substitution pair
                    pair< GiNaC::ex, GiNaC::ex > SubstitutionExpression( maxvarex, t_expression );
                    assert( boolfather->getType() == BOOL );
                    bool constraintisnegated = false;
                    if( constraintfather->father().getType() == NOT ) constraintisnegated = true;
                    pair< std::string, bool > SubstitutionIdentifier( boolfather->identifier(), constraintisnegated );
                    GiNaC::symtab t_var = var;
                    var.erase( maxvarstr );
                    pair< GiNaC::symtab, GiNaC::symtab > varpair( t_var, var );
                    pair< pair< GiNaC::symtab, GiNaC::symtab >, pair< GiNaC::ex, GiNaC::ex > >
                                subplusvars( varpair, SubstitutionExpression);
                    pair< pair< std::string, bool >, pair< pair< GiNaC::symtab, GiNaC::symtab >,
                                pair< GiNaC::ex, GiNaC::ex > > > substitution( SubstitutionIdentifier, subplusvars );
                    // Save Substitution Pair
                    mSubstitutions.push_back( substitution );
                    // Add origins
                    getOrigins( father, _origins );
                    mSubstitutionOrigins.push_back( _origins );
                    // Stop searching for Variables in this Constraint
                    t_passedFormulaSize = passedFormulaSize();
                    for( unsigned j = 0; j < t_passedFormulaSize; ++j )                          // If substitution was found apply to all Formula
                    {
                        if( j != i )                                                             // except the origin
                        {
                            if( substituteConstraint( passedFormulaAt( j ), substitution, _origins ) )
                            {
                                --j;                                                             //    c1,c2,c3 =>  c1,c3,c2'  (only c3 should be checked)
                                --t_passedFormulaSize;                                           //        c2 is substituted to c2'
                            }
                        }
                    }
                }
            }
            ++mNumberOfCheckedFormulas;
        }
    }
    
    /**
     * Applies so far found Substitutions on _formula and his Subformulas.
     */
    bool PreProModule::substituteConstraint( const Formula* _formula, pair< pair< std::string, bool >,
            pair< pair<GiNaC::symtab, GiNaC::symtab>, pair< GiNaC::ex, GiNaC::ex> > > _substitution,
            vec_set_const_pFormula _suborigin )
    {
        pair< const Formula*, const Formula* > t_pair = isCandidateforSubstitution( _formula );                                 // Check form of _formula
        const Formula* constraintfather = t_pair.first;
        const Formula* boolfather = t_pair.second;
        GiNaC::ex substitutedExpression;
        GiNaC::symtab _newvars;
        if( (constraintfather != NULL) && (boolfather != NULL) )
        {
            std::string t_identifier;
            Constraint t_constraint = constraintfather->constraint();
            Formula* newformula = new Formula( OR );
            t_identifier = boolfather->identifier();
            if( _substitution.first.first == t_identifier
                    && (constraintfather->father().getType() == NOT) == _substitution.first.second )
            {
                if( !t_constraint.rLhs().coeff( _substitution.second.second.first, 1 ).is_zero() )
                {
                    t_constraint.rLhs().subs( _substitution.second.second.first==_substitution.second.second.second );
                    substitutedExpression = t_constraint.rLhs().subs( _substitution.second.second.first==_substitution.second.second.second );    // Substitute
                    _newvars = t_constraint.variables();
                    vec_set_const_pFormula t_origins;
                    getOrigins( _formula, t_origins );
                    t_origins = merge( t_origins, _suborigin );
                    Formula* newnotformula = new Formula( NOT );
                    Formula* newboolformula = new Formula( *boolfather );
                    newnotformula->addSubformula( newboolformula);
                    newformula->addSubformula( newnotformula );
                    const Constraint* newconstraint = Formula::newConstraint( substitutedExpression, CR_EQ );
                    Formula* newconstraintformula = new Formula( newconstraint );     // Create new ConstraintFormula for Substituted constraint
                    if( constraintfather->father().getType() == NOT )
                    {
                        Formula* newnotformulaforconstraint = new Formula( NOT );
                        newnotformulaforconstraint->addSubformula( newconstraintformula );
                        newformula->addSubformula( newnotformulaforconstraint );
                    }
                    else newformula->addSubformula( newconstraintformula );
                    removeSubformulaFromPassedFormula( getPositionOfPassedFormula( _formula ) );

                    addSubformulaToPassedFormula( newformula, t_origins );
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * Checks form of _formula
     * @return  pair< Formula*( Constraint ), Formula*( Bool ) >
     */
    pair< const Formula*, const Formula*> PreProModule::isCandidateforSubstitution( const Formula* const _formula ) const
    {
        if( _formula != NULL )
        {
            if( _formula->getType() == OR)
            {
                std::vector< Formula* > subformulas = _formula->subformulas();
                if( subformulas.at(0) != NULL && subformulas.at(1)!= NULL )
                {
                    Type type0 = subformulas.at(0)->getType();
                    Type type1 = subformulas.at(1)->getType();
                    if( type0 == REALCONSTRAINT && type1 == NOT )
                    {
                        if( subformulas.at( 1 )->subformulas().at( 0 ) != NULL )
                        {
                            if( subformulas.at( 1 )->subformulas().at( 0 )->getType() == BOOL )
                            {
                                pair< const Formula*, const Formula*> _pair(subformulas.at(0), subformulas.at( 1 )->subformulas().at( 0 ));
                                return _pair;
                            }
                        }
                    }
                    else if( type1 == REALCONSTRAINT && type0 == NOT )
                    {
                        if( subformulas.at( 0 )->subformulas().at( 0 ) != NULL )
                        {
                            if( subformulas.at( 0 )->subformulas().at( 0 )->getType() == BOOL )
                            {
                                pair< const Formula*, const Formula*> _pair(subformulas.at(1), subformulas.at( 0 )->subformulas().at( 0 ));
                                return _pair;
                            }
                        }
                    }
                    else if( type0 == NOT && type1 == NOT)
                    {
                        if( subformulas.at( 0 )->subformulas().at( 0 ) != NULL && subformulas.at( 1 )->subformulas().at( 0 ) != NULL )
                        {
                            if( subformulas.at( 0 )->subformulas().at( 0 )->getType() == REALCONSTRAINT &&
                                    subformulas.at( 1 )->subformulas().at( 0 )->getType() == BOOL )
                            {
                                pair< const Formula*, const Formula*> _pair( subformulas.at( 0 )->subformulas().at( 0 ), subformulas.at( 1 )->subformulas().at( 0 ) );
                                return _pair;
                            }
                            else if( subformulas.at( 0 )->subformulas().at( 0 )->getType() == BOOL &&
                                subformulas.at( 1 )->subformulas().at( 0 )->getType() == REALCONSTRAINT )
                            {
                                pair< const Formula*, const Formula*> _pair( subformulas.at( 1 )->subformulas().at( 0 ), subformulas.at( 0 )->subformulas().at( 0 ) );
                                return _pair;
                            }
                        }
                    }
                }
            }
        }
        pair< Formula*, Formula*> _pair( NULL, NULL);
        return _pair;
    }          

     /**
     * Pushs a backtrackpoint, to the stack of backtrackpoints.
     */
    void PreProModule::pushBacktrackPoint()
    {
        pair< bool, unsigned > firstpair( mFreshConstraintReceived, mNumberOfComparedConstraints );
        pair< unsigned, unsigned > secondpair( pPassedFormula()->size(), mReceivedConstraints.size() );
        pair< pair< bool, unsigned >, pair< unsigned, unsigned > > newbacktrack( firstpair, secondpair );
        mConstraintBacktrackPoints.push_back( newbacktrack );
    }

    /**
     * Pops the last backtrackpoint, from the stack of backtrackpoints.
     */
    void PreProModule::popBacktrackPoint()
    {
        mFreshConstraintReceived = mConstraintBacktrackPoints.back().first.first;
        mNumberOfComparedConstraints = mConstraintBacktrackPoints.back().first.second;
        while( mConstraintBacktrackPoints.back().second.first != pPassedFormula()->size() )
        {
            removeSubformulaFromPassedFormula( pPassedFormula()->size()-1 );
        }
        while( mReceivedConstraints.size() != mConstraintBacktrackPoints.back().second.second )
        {
            mReceivedConstraints.pop_back();
            mConstraintOrigins.pop_back();
        }
        mConstraintBacktrackPoints.pop_back();
    }
}    // namespace smtrat