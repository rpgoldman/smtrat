/**
 * @file IncWidthModule.h
 * @author YOUR NAME <YOUR EMAIL ADDRESS>
 *
 * @version 2015-06-29
 * Created on 2015-06-29.
 */

#pragma once

#include "../../solver/Module.h"
#include "../../datastructures/VariableBounds.h"
#include "../ICPModule/ICPModule.h"
#include "IncWidthStatistics.h"
#include "IncWidthSettings.h"
namespace smtrat
{
    template<typename Settings>
    class IncWidthModule : public Module
    {
        private:
            // Members.
            ///
            bool mRestartCheck;
            /// Stores the current width for the variable domains.
            Rational mHalfOfCurrentWidth;
            /// Stores the substitutions of variables to variable plus a value, being the shift used to arrange the variable's domain.
            std::map<carl::Variable,Poly> mVariableShifts;
            /// Collection of bounds of all received formulas.
			vb::VariableBounds<FormulaT> mVarBounds;
            ///
            ModuleInput* mICPFormula;
            std::vector<std::atomic_bool*> mICPFoundAnswer;
            RuntimeSettings* mICPRuntimeSettings;
            ICPModule<ICPSettings4>* mICP;
            carl::FastMap<FormulaT,ModuleInput::iterator> mICPFormulaPositions;
            

        public:
			typedef Settings SettingsType;
            
            std::string moduleName() const
            {
                return SettingsType::moduleName;
            }
            
            IncWidthModule( const ModuleInput* _formula, RuntimeSettings* _settings, Conditionals& _conditionals, Manager* _manager = NULL );

            ~IncWidthModule();

            // Main interfaces.

            /**
             * The module has to take the given sub-formula of the received formula into account.
             *
             * @param _subformula The sub-formula to take additionally into account.
             * @return false, if it can be easily decided that this sub-formula causes a conflict with
             *          the already considered sub-formulas;
             *          true, otherwise.
             */
            bool addCore( ModuleInput::const_iterator _subformula );

            /**
             * Removes the subformula of the received formula at the given position to the considered ones of this module.
             * Note that this includes every stored calculation which depended on this subformula, but should keep the other
             * stored calculation, if possible, untouched.
             *
             * @param _subformula The position of the subformula to remove.
             */
            void removeCore( ModuleInput::const_iterator _subformula );

            /**
             * Updates the current assignment into the model.
             * Note, that this is a unique but possibly symbolic assignment maybe containing newly introduced variables.
             */
            void updateModel() const;

            /**
             * Checks the received formula for consistency.
             * @param _final true, if this satisfiability check will be the last one (for a global sat-check), if its result is SAT
             * @param _full false, if this module should avoid too expensive procedures and rather return unknown instead.
             * @param _minimize true, if the module should find an assignment minimizing its objective variable; otherwise any assignment is good.
             * @return SAT,    if the received formula is satisfiable;
             *         UNSAT,   if the received formula is not satisfiable;
             *         Unknown, otherwise.
             */
            Answer checkCore( bool _final = false, bool _full = true, bool _minimize = false );
            
        private:
            void reset();
            std::pair<ModuleInput::iterator,bool> addToICP( const FormulaT& _formula, bool _guaranteedNew = true );
            void removeFromICP( const FormulaT& _formula );
            void clearICP();
    };
}
