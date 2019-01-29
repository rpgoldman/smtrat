#pragma once

#include "../SolverTypes.h"

#include "BaseBackend.h"
#include "../ClauseChecker.h"

#include "MCSATStatistics.h"

#include <carl/formula/model/Assignment.h>

#include <smtrat-mcsat/smtrat-mcsat.h>

#include <functional>
#include <map>
#include <set>
#include <vector>

namespace smtrat {
namespace mcsat {

using carl::operator<<;
    
struct InformationGetter {
	std::function<Minisat::lbool(Minisat::Var)> getVarValue;
	std::function<Minisat::lbool(Minisat::Lit)> getLitValue;
	std::function<Minisat::lbool(Minisat::Var)> getBoolVarValue;
	std::function<int(Minisat::Var)> getDecisionLevel;
	std::function<int(Minisat::Var)> getTrailIndex;
	std::function<Minisat::CRef(Minisat::Var)> getReason;
	std::function<const Minisat::Clause&(Minisat::CRef)> getClause;
	std::function<const Minisat::vec<Minisat::CRef>&()> getClauses;
	std::function<const Minisat::vec<Minisat::CRef>&()> getLearntClauses;
	std::function<bool(Minisat::Var)> isTheoryAbstraction;
	std::function<bool(const FormulaT&)> isAbstractedFormula;
	std::function<Minisat::Var(const FormulaT&)> abstractVariable;
	std::function<const FormulaT&(Minisat::Var)> reabstractVariable;
	std::function<const FormulaT&(Minisat::Lit)> reabstractLiteral;
	std::function<const Minisat::vec<Minisat::Watcher>&(Minisat::Lit)> getWatches;
};
struct TheoryLevel {
	/// Theory variable for this level
	carl::Variable variable = carl::Variable::NO_VARIABLE;
	/// Literal that assigns this theory variable
	Minisat::Lit decisionLiteral = Minisat::lit_Undef;
	/// Boolean variables univariate in this theory variable
	std::vector<Minisat::Var> univariateVariables;
};

template<typename Settings>
class MCSATMixin {
  
private:
	InformationGetter mGetter;
	
	/**
	 * The first entry of the stack always contains an entry for the non-theory
	 * variables: the variable is set to NO_VARIABLE and the lists contain
	 * variables that do not contain any theory variables.
	 */
	using TheoryStackT = std::vector<TheoryLevel>;
	TheoryStackT mTheoryStack;

	/// Variables that are not univariate in any variable yet.
	std::vector<Minisat::Var> mUndecidedVariables;
	
	MCSATBackend<Settings> mBackend;

	#ifdef SMTRAT_DEVOPTION_Statistics
	MCSATStatistics*& mpStatistics;
	#endif

	struct ModelAssignmentCache {
		ModelValues mContent;
		Model mModel;
		const Model& mBaseModel;

		ModelAssignmentCache(const Model& baseModel) : mBaseModel(baseModel) {
			mModel = mBaseModel;
		}

		bool empty() const {
			return mContent.empty();
		}

		void clear() {
			mContent.clear();
			mModel = mBaseModel;
		}

		void cache(const ModelValues& val) {
			assert(empty());
			mModel = mBaseModel;
			mContent = val;
			for (const auto& assignment : content()) {
				mModel.emplace(assignment.first, assignment.second);
			}
		}

		const ModelValues& content() const {
			return mContent;
		}

		const Model& model() const {
			return mModel;
		}
	};
	/// Cache for the next model assignemt(s)
	ModelAssignmentCache mModelAssignmentCache;

	struct VarProperties {
		std::size_t maxTheoryLevel = std::numeric_limits<std::size_t>::max();
		boost::optional<std::size_t> maxDegree = boost::none;
	};
	/// Cache for static information about variables
	std::vector<VarProperties> mVarPropertyCache;

private:
	// ***** private helper methods
	
	/// Updates lookup for the current level
	void updateCurrentLevel();
	/// Undo a decision on the current level
	void undoDecision();

	std::size_t varid(Minisat::Var var) const {
		assert(var >= 0);
		return static_cast<std::size_t>(var);
	}
public:
	
	template<typename BaseModule>
	explicit MCSATMixin(BaseModule& baseModule):
		mGetter({
			[&baseModule](Minisat::Var v){ return baseModule.value(v); },
			[&baseModule](Minisat::Lit l){ return baseModule.value(l); },
			[&baseModule](Minisat::Var l){ return baseModule.bool_value(l); },
			[&baseModule](Minisat::Var v){ return baseModule.vardata[v].level; },
			[&baseModule](Minisat::Var v){ return baseModule.vardata[v].mTrailIndex; },
			[&baseModule](Minisat::Var v){ return baseModule.reason(v); },
			[&baseModule](Minisat::CRef c) -> const auto& { return baseModule.ca[c]; },
			[&baseModule]() -> const auto& { return baseModule.clauses; },
			[&baseModule]() -> const auto& { return baseModule.learnts; },
			[&baseModule](Minisat::Var v){ return (baseModule.mBooleanConstraintMap.size() > v) && (baseModule.mBooleanConstraintMap[v].first != nullptr); },
			[&baseModule](const FormulaT& f) { return baseModule.mConstraintLiteralMap.count(f) > 0; },
			[&baseModule](const FormulaT& f) { return var(baseModule.mConstraintLiteralMap.at(f).front()); },
			[&baseModule](Minisat::Var v) -> const auto& { return baseModule.mBooleanConstraintMap[v].first->reabstraction; },
			[&baseModule](Minisat::Lit l) -> const auto& { return sign(l) ? baseModule.mBooleanConstraintMap[var(l)].second->reabstraction : baseModule.mBooleanConstraintMap[var(l)].first->reabstraction; },
			[&baseModule](Minisat::Lit l) -> const auto& { return baseModule.watches[l]; }
		}),
		mTheoryStack(1, TheoryLevel()),
		#ifdef SMTRAT_DEVOPTION_Statistics
		mpStatistics(baseModule.mpMCSATStatistics),
		#endif
		mModelAssignmentCache(model())
	{}
	
	std::size_t level() const {
		return mTheoryStack.size() - 1;
	}
	const Model& model() const {
		return mBackend.getModel();
	}
	const std::vector<Minisat::Var>& undecidedVariables() const {
		return mUndecidedVariables;
	}
	/// Returns the respective theory level
	const TheoryLevel& get(std::size_t level) const {
		assert(level < mTheoryStack.size());
		return mTheoryStack[level];
	}
	/// Returns the current theory level
	const TheoryLevel& current() const {
		return mTheoryStack.back();
	}
	TheoryLevel& current() {
		return mTheoryStack.back();
	}
	/// Retrieve the current theory variable
	carl::Variable currentVariable() const {
		return variable(level());
	}
	carl::Variable variable(std::size_t level) const {
		SMTRAT_LOG_TRACE("smtrat.sat.mcsat", "Obtaining variable " << level << " from " << mBackend.variableOrder());
		if (level == 0) return carl::Variable::NO_VARIABLE;
		if (level > mBackend.variableOrder().size()) return carl::Variable::NO_VARIABLE;
		assert(level <= mBackend.variableOrder().size());
		return mBackend.variableOrder()[level - 1];
	}
	
	bool hasNextVariable() const {
		SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "Current level: " << level() << " with variables " << mBackend.variableOrder());
		return level() < mBackend.variableOrder().size();
	}
	carl::Variable nextVariable() const {
		assert(hasNextVariable());
		return mBackend.variableOrder()[level()];
	}
	bool mayDoAssignment() const {
		return current().variable != carl::Variable::NO_VARIABLE && current().decisionLiteral == Minisat::lit_Undef;
	}
	// ***** Modifier
	
	/// Advance to the next level using this variable
	void pushLevel(carl::Variable var);
	/// Retract one level (without removing the lookups)
	void popLevel();
	
	/**
	 * Add a new constraint.
	 */
	void doAssignment(Minisat::Lit lit) {
		SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "Assigned " << lit);
		if (!mGetter.isTheoryAbstraction(var(lit))) return;
		const auto& f = mGetter.reabstractLiteral(lit);
		if (f.getType() == carl::FormulaType::VARASSIGN) {
			SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "Skipping assignment.");
			return;
		}
		if (!mModelAssignmentCache.empty()) {
			#ifdef SMTRAT_DEVOPTION_Statistics
			mpStatistics->modelAssignmentCacheHit();
			#endif
			auto res = carl::model::evaluate(f, mModelAssignmentCache.model());
			if (!res.isBool() || !res.asBool()) {
				mModelAssignmentCache.clear(); // clear model assignment cache
			}
		}
		mBackend.pushConstraint(f);
	}
	/**
	 * Remove the last constraint. f must be the same as the one passed to the last call of pushConstraint().
	 */
	void undoAssignment(Minisat::Lit lit) {
		SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "Unassigned " << lit);
		if (!mGetter.isTheoryAbstraction(var(lit))) return;
		const auto& f = mGetter.reabstractLiteral(lit);
		if (f.getType() == carl::FormulaType::VARASSIGN) {
			SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "Skipping assignment.");
			return;
		}
		mBackend.popConstraint(f);
	}
	
	/// Add a variable, return the level it was inserted on
	std::size_t addVariable(Minisat::Var variable);

	// ***** Auxiliary getter
	
	/// Checks whether the given formula is univariate on the given level
	bool isFormulaUnivariate(const FormulaT& formula, std::size_t level) const;
	
	/// Make a decision and push a new level
	void makeDecision(Minisat::Lit decisionLiteral);
	/// Backtracks to the theory decision represented by the given literal. 
	bool backtrackTo(Minisat::Lit literal);
	
	// ***** Helper methods
	
	/// Evaluate a literal in the theory, set lastReason to last theory decision involved.
	Minisat::lbool evaluateLiteral(Minisat::Lit lit) const;
	
	std::pair<bool, boost::optional<Explanation>> isDecisionPossible(Minisat::Lit lit, bool check_feasibility_before = true);
	
	boost::optional<Explanation> isFeasible() {
		if (!mayDoAssignment()) {
			SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "Trail is feasible as there is no next variable to be assigned.");
			return boost::none;
		}
		SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "Checking whether trail is feasible (w.r.t. " << currentVariable() << ")");
		AssignmentOrConflict res;
		if (!mModelAssignmentCache.empty()) {
			#ifdef SMTRAT_DEVOPTION_Statistics
			mpStatistics->modelAssignmentCacheHit();
			#endif
			SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "Found cached assignment.");
			return boost::none;
		} else {
			auto res = mBackend.findAssignment(currentVariable());
			if (carl::variant_is_type<ModelValues>(res)) {
				mModelAssignmentCache.cache(boost::get<ModelValues>(res));
				return boost::none;
			} else {
				const auto& confl = boost::get<FormulasT>(res);
				SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "Explaining " << confl);
				return mBackend.explain(currentVariable(), confl);
			}
		}
	}
	
	boost::variant<Explanation,FormulasT> makeTheoryDecision() {
		SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "Obtaining assignment");
		SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", mBackend);
		AssignmentOrConflict res;
		if (mModelAssignmentCache.empty()) {
			res = mBackend.findAssignment(currentVariable());
		} else {
			SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "Found cached assignment.");
			#ifdef SMTRAT_DEVOPTION_Statistics
			mpStatistics->modelAssignmentCacheHit();
			#endif
			res = mModelAssignmentCache.content();
			mModelAssignmentCache.clear();
		}
		if (carl::variant_is_type<ModelValues>(res)) {
			const auto& values = boost::get<ModelValues>(res);
			SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "-> " << values);
			FormulasT reprs;
			for (const auto& value : values) {
				FormulaT repr = carl::representingFormula(value.first, value.second);
				mBackend.pushAssignment(value.first, value.second, repr);
				reprs.push_back(std::move(repr));
			}
			assert(trailIsConsistent());
			return reprs;
		} else {
			const auto& confl = boost::get<FormulasT>(res);
			auto explanation = mBackend.explain(currentVariable(), confl);
			SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "Got a conflict: " << explanation);
			return explanation;
		}
	}
	
	Explanation explainTheoryPropagation(Minisat::Lit literal) {
		SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "Current state: " << (*this));
		SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "Explaining " << literal << " under " << mBackend.getModel());
		const auto& f = mGetter.reabstractLiteral(literal);
		auto conflict = mBackend.isInfeasible(currentVariable(), !f);
		assert(carl::variant_is_type<FormulasT>(conflict));
		auto& confl = boost::get<FormulasT>(conflict);
		assert( std::find(confl.begin(), confl.end(), !f) != confl.end() );
		SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "Explaining " << f << " from " << confl);
		auto res = mBackend.explain(currentVariable(), !f, confl);
		SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "Explaining " << f << " by " << res);
		// f is part of the conflict, because the trail is feasible without f:
		if (carl::variant_is_type<FormulaT>(res)) {
			if (boost::get<FormulaT>(res).isFalse()) {
				SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "Explanation failed.");
			} else {
				assert(boost::get<FormulaT>(res).contains(f));
			}
		}
		else {
			assert(boost::get<ClauseChain>(res).chain().back().clause().contains(f));
		}
		return res;
	}
	
	template<typename Constraints>
	void resetVariableOrdering(const Constraints& c) {
		mBackend.resetVariableOrdering(c);
	}
	
	// ***** Auxliary getter
	
	/// Checks whether the given formula is currently univariate
	bool isFormulaUnivariate(const FormulaT& formula) const {
		return isFormulaUnivariate(formula, level());
	}
	
	std::size_t theoryLevel(Minisat::Var var) const {
		if (!mGetter.isTheoryAbstraction(var)) {
			return 0;
		}
		return theoryLevel(mGetter.reabstractVariable(var));
	}
	
	std::size_t theoryLevel(const FormulaT& f) const {
		SMTRAT_LOG_TRACE("smtrat.sat.mcsat", "Computing theory level for " << f);
		carl::Variables vars;
		f.arithmeticVars(vars);
		if (vars.empty()) {
			SMTRAT_LOG_TRACE("smtrat.sat.mcsat", f << " has no variable, thus on level 0");
			return 0;
		}
		
		//for (std::size_t lvl = level(); lvl > 0; lvl--) {
		//	if (variable(lvl) == carl::Variable::NO_VARIABLE) continue;
		//	if (vars.count(variable(lvl)) > 0) {
		//		SMTRAT_LOG_TRACE("smtrat.sat.mcsat", f << " is univariate in " << variable(lvl));
		//		return lvl;
		//	}
		//}
		//SMTRAT_LOG_TRACE("smtrat.sat.mcsat", f << " contains undecided variables.");
		//return std::numeric_limits<std::size_t>::max();
	
		Model m = model();
		if (!carl::model::evaluate(f, m).isBool()) {
			SMTRAT_LOG_TRACE("smtrat.sat.mcsat", f << " is undecided.");
			return std::numeric_limits<std::size_t>::max();
		}
		for (std::size_t lvl = level(); lvl > 0; lvl--) {
			if (variable(lvl) == carl::Variable::NO_VARIABLE) continue;
			m.erase(variable(lvl));
			if (vars.count(variable(lvl)) == 0) continue;
			if (!carl::model::evaluate(f, m).isBool()) {
				return lvl;
			}
		}
		assert(false);
		return 0;
		
		for (std::size_t level = 1; level < mTheoryStack.size(); level++) {
			vars.erase(variable(level));
			if (vars.empty()) {
				SMTRAT_LOG_TRACE("smtrat.sat.mcsat", f << " is univariate in " << variable(level));
				return level;
			}
		}
		SMTRAT_LOG_TRACE("smtrat.sat.mcsat", f << " is undecided.");
		return std::numeric_limits<std::size_t>::max();
	}
	
	Minisat::Lit getDecisionLiteral(Minisat::Var var) const {
		if (!mGetter.isTheoryAbstraction(var)) {
			return Minisat::lit_Undef;
		}
		return getDecisionLiteral(mGetter.reabstractVariable(var));
	}
	Minisat::Lit getDecisionLiteral(const FormulaT& f) const {
		std::size_t level = theoryLevel(f);
		SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "Theory level of " << f << " is " << level);
		if (level >= mTheoryStack.size()) return Minisat::lit_Undef;
		return get(level).decisionLiteral;
	}
	
	int assignedAtTrailIndex(Minisat::Var variable) const {
		auto lit = getDecisionLiteral(variable);
		if (lit == Minisat::lit_Undef) {
			SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", variable << " was not assigned yet.");
			return std::numeric_limits<int>::max();
		}
		return mGetter.getTrailIndex(var(lit));
	}
	
	int decisionLevel(Minisat::Var var) const {
		if (!mGetter.isTheoryAbstraction(var)) {
			return std::numeric_limits<int>::max();
		}
		return decisionLevel(mGetter.reabstractVariable(var));
	}
	
	int decisionLevel(const FormulaT& f) const {
		auto lit = getDecisionLiteral(f);
		if (lit == Minisat::lit_Undef) {
			return std::numeric_limits<int>::max();
		}
		return mGetter.getDecisionLevel(var(lit));
	}
	
	bool trailIsConsistent() const {
		const auto& trail = mBackend.getTrail();
		SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", "Checking trail against " << trail.model());
		auto evaluator = [&trail](const auto& c){
			auto res = carl::model::evaluate(c, trail.model());
			SMTRAT_LOG_DEBUG("smtrat.sat.mcsat", c << " evaluates to " << res);
			if (res.isBool()) {
				if (!res.asBool()) return false;
			}
			return true;
		};
		for (const auto& c: trail.constraints()) {
			auto category = mcsat::constraint_type::categorize(c, model(), carl::Variable::NO_VARIABLE);
			if (category != mcsat::ConstraintType::Assigned) continue;
			if (!evaluator(c)) return false;
		}
		for (const auto& b: trail.mvBounds()) {
			auto category = mcsat::constraint_type::categorize(b, model(), carl::Variable::NO_VARIABLE);
			if (category != mcsat::ConstraintType::Assigned) continue;
			if (!evaluator(b)) return false;
		}
		return true;
	}

	/**
	 * Calculates the syntactic (maximal) theory level of the given variable.
	 */
	std::size_t maxTheoryLevel(Minisat::Var var) {
		// if variableOrder has not been initialized yet
		if (mBackend.variableOrder().empty()) {
			return 0;
		}

		std::size_t v = varid(var);
		assert(v < mVarPropertyCache.size());

		if (mVarPropertyCache[v].maxTheoryLevel == std::numeric_limits<std::size_t>::max()) {
			if (!mGetter.isTheoryAbstraction(var)) {
				mVarPropertyCache[v].maxTheoryLevel = 0;
			} else {
				auto reabstraction = mGetter.reabstractVariable(var);
				carl::Variables vars;
				reabstraction.arithmeticVars(vars);
				if (vars.empty()) {
					mVarPropertyCache[v].maxTheoryLevel = 0;
				} else {
					for (std::size_t i = mBackend.variableOrder().size(); i > 0; i--) {
						if (vars.find(mBackend.variableOrder()[i-1]) != vars.end()) {
							mVarPropertyCache[v].maxTheoryLevel = i;
							break;
						}
					}
				}	
			}
		}
		assert(mVarPropertyCache[v].maxTheoryLevel < std::numeric_limits<std::size_t>::max());

		return mVarPropertyCache[v].maxTheoryLevel;
	}

	std::size_t maxDegree(const Minisat::Var& var) { // TODO test
		std::size_t v = varid(var);
		assert(v < mVarPropertyCache.size());

		if (mVarPropertyCache[v].maxDegree == boost::none) {
			if (!mGetter.isTheoryAbstraction(var)) {
				mVarPropertyCache[v].maxDegree = 0;
			} else {
				const auto& reabstraction = mGetter.reabstractVariable(var);
				if (reabstraction.getType() == carl::FormulaType::CONSTRAINT) {
					const auto& constr = reabstraction.constraint();
					carl::Variables vars;
					reabstraction.arithmeticVars(vars);
					std::size_t maxDeg = 0;
					for (const auto& tvar : vars) {
						std::size_t deg = constr.lhs().degree(tvar);
						if (deg > maxDeg) maxDeg = deg;
					}
					mVarPropertyCache[v].maxDegree = maxDeg;
				} else if (reabstraction.getType() == carl::FormulaType::VARCOMPARE) {
					mVarPropertyCache[v].maxDegree = std::numeric_limits<std::size_t>::max();
				} else {
					assert(false);
				}
				
			}
		}
		assert(mVarPropertyCache[v].maxDegree != boost::none);

		return *mVarPropertyCache[v].maxDegree;
	}

	std::size_t degreeInLevel(const Minisat::Var& var, std::size_t level) {
		if (!mGetter.isTheoryAbstraction(var)) {
			return std::numeric_limits<std::size_t>::max();
		}
		else {
			const carl::Variable& theoryVar = variable(level);
			const auto& reabstraction = mGetter.reabstractVariable(var);
			if (reabstraction.getType() == carl::FormulaType::CONSTRAINT || reabstraction.getType() == carl::FormulaType::TRUE || reabstraction.getType() == carl::FormulaType::FALSE) {
				// TODO how exactly handle degree = 0?
				const auto& constr = reabstraction.constraint();
				return constr.maxDegree(theoryVar);
			} else if (reabstraction.getType() == carl::FormulaType::VARCOMPARE) {
				return std::numeric_limits<std::size_t>::max();
			} else {
				assert(false);
			}
		}
	}

	// ***** Output
	/// Prints a single clause
	void printClause(std::ostream& os, Minisat::CRef clause) const;
	template<typename Sett>
	friend std::ostream& operator<<(std::ostream& os, const MCSATMixin<Sett>& mcm);
};

}
}

#include "MCSATMixin.tpp"
