#pragma once

#include "MCSATSettings.h"

#include <smtrat-mcsat/smtrat-mcsat.h>

namespace smtrat {
namespace mcsat {

template<typename Settings>
class MCSATBackend {
	mcsat::Bookkeeping mBookkeeping;
	typename Settings::AssignmentFinderBackend mAssignmentFinder;
	typename Settings::ExplanationBackend mExplanation;

public:
	void pushConstraint(const FormulaT& f) {
		mBookkeeping.pushConstraint(f);
	}

	void popConstraint(const FormulaT& f) {
		mBookkeeping.popConstraint(f);
	}

	void pushAssignment(carl::Variable v, const ModelValue& mv, const FormulaT& f) {
		mBookkeeping.pushAssignment(v, mv, f);
	}

	void popAssignment(carl::Variable v) {
		mBookkeeping.popAssignment(v);
	}

	const auto& getModel() const {
		return mBookkeeping.model();
	}
	const auto& getTrail() const {
		return mBookkeeping;
	}
	
	template<typename Constraints>
	void initVariables(const Constraints& c) {
		if (mBookkeeping.variables().empty()) {
			carl::carlVariables vars;
			for (int i = 0; i < c.size(); ++i) {
				if (c[i].first == nullptr) continue;
				if (c[i].first->reabstraction.getType() != carl::FormulaType::CONSTRAINT) continue;
				const ConstraintT& constr = c[i].first->reabstraction.constraint(); 
				constr.gatherVariables(vars);
			}
			const auto& varvec = vars.underlyingVariables();
			mBookkeeping.updateVariables(carl::Variables(varvec.begin(), varvec.end()));
			SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "Got variables " << variables());
		}
	}
	
	const auto& variables() const {
		return mBookkeeping.variables();
	}

	const auto& assignedVariables() const {
		return mBookkeeping.assignedVariables();
	}

	AssignmentOrConflict findAssignment(carl::Variable var) const {
		auto res = mAssignmentFinder(getTrail(), var);
		if (res) {
			return *res;
		} else {
			SMTRAT_LOG_ERROR("smtrat.mcsat", "AssignmentFinder backend failed.");
			assert(false);
			return ModelValues();
		}
	}

	AssignmentOrConflict isInfeasible(carl::Variable var, const FormulaT& f) {
		SMTRAT_LOG_DEBUG("smtrat.mcsat", "Checking whether " << f << " is feasible");
		pushConstraint(f);
		auto res = findAssignment(var);
		popConstraint(f);
		if (carl::variant_is_type<ModelValues>(res)) {
			SMTRAT_LOG_DEBUG("smtrat.mcsat", f << " is feasible");
		} else {
			SMTRAT_LOG_DEBUG("smtrat.mcsat", f << " is infeasible with reason " << boost::get<FormulasT>(res));
		}
		return res;
	}

	Explanation explain(carl::Variable var, const FormulasT& reason) const {
		if (getModel().empty()) {
			FormulasT expl;
			for (const auto& r: reason) expl.emplace_back(r.negated());
			return FormulaT(carl::FormulaType::OR, std::move(expl));
		}
		boost::optional<Explanation> res = mExplanation(getTrail(), var, reason);
		if (res) {
			SMTRAT_LOG_DEBUG("smtrat.mcsat", "Got explanation " << *res);
			return *res;
		} else {
			SMTRAT_LOG_ERROR("smtrat.mcsat", "Explanation backend failed.");
			return Explanation(FormulaT(carl::FormulaType::FALSE));
		}
	}

	Explanation explain(carl::Variable var, const FormulaT& f, const FormulasT& reason) {
		pushConstraint(f);
		auto res = explain(var, reason);
		popConstraint(f);
		return res;
	}
};


template<typename Settings>
std::ostream& operator<<(std::ostream& os, const MCSATBackend<Settings>& backend) {
	return os << backend.getTrail();
}

} // namespace mcsat
} // namespace smtrat
