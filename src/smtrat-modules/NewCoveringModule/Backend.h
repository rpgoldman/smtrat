/**
 * @file Backend.h
 * @author Philip Kroll <Philip.Kroll@rwth-aachen.de>
 *
 * @version 2021-07-08
 * Created on 2021-07-08.
 */

// https://arxiv.org/pdf/2003.05633.pdf

#pragma once

#include <algorithm>

#include <carl/ran/ran.h>

#include "CoveringUtils.h"
#include "NewCoveringModule.h"
#include "NewCoveringSettings.h"

namespace smtrat {

template<typename Settings>
class Backend {
	using SettingsT = Settings;

private:
	//Variable Ordering, Initialized once in checkCore
	std::vector<carl::Variable> mVariableOrdering;

	//Main Variables of constraints need to be in the same ordering as the variable ordering
	std::vector<PolyRefVector> mConstraints;

	Helpers helpers;

	carl::ran_assignment<Rational> mCurrentAssignment;

	std::vector<std::vector<CellInformation>> mCoveringInformation;

public:
	//Init with empty variable ordering
	Backend()
		: mVariableOrdering() {
		SMTRAT_LOG_DEBUG("smtrat.covering", " Dry Init of Covering Backend");
	}

	Backend(const std::vector<carl::Variable>& varOrdering, std::vector<PolyRefVector>& constraints)
		: mVariableOrdering(varOrdering), mConstraints(constraints) {
		SMTRAT_LOG_DEBUG("smtrat.covering", "Init of Covering Backend with Variable Ordering: " << mVariableOrdering);
		mCoveringInformation.resize(mVariableOrdering.size());
	}

	size_t dimension() {
		return mVariableOrdering.size();
	}

	void setHelpers(Helpers& hp) {
		helpers = hp;
	}

	void setConstraints(std::vector<PolyRefVector>& constraints) {
		mConstraints = constraints;
	}

	void reduceConstraints() {
		for (auto& refVector : mConstraints) {
			//Remove duplicates
			refVector.reduce();
		}
	}

	//The new Variable ordering must be an "extension" to the old one
	void setVariableOrdering(const std::vector<carl::Variable>& newVarOrdering) {
		SMTRAT_LOG_DEBUG("smtrat.covering", "Old Variable ordering: " << mVariableOrdering);

		assert(newVarOrdering.size() > mVariableOrdering.size());

		for (std::size_t i = 0; i < mVariableOrdering.size(); i++) {
			assert(newVarOrdering[i] == mVariableOrdering[i]);
		}

		std::copy(newVarOrdering.begin() + mVariableOrdering.size(), newVarOrdering.end(), std::back_inserter(mVariableOrdering));
		mCoveringInformation.resize(mVariableOrdering.size());
		SMTRAT_LOG_DEBUG("smtrat.covering", "New Variable ordering: " << mVariableOrdering);
	}

	//Delete all stored data with level higher or equal
	void resetStoredData(std::size_t level) {
		for (size_t i = level; i < dimension(); i++) {
			mCoveringInformation[i].clear();
			mCurrentAssignment.erase(mVariableOrdering[i]);
		}
	}

	bool hasRootAbove(const cadcells::datastructures::PolyRef& poly, const RAN& number) {
		std::vector<RAN> roots = helpers.mProjections->real_roots(mCurrentAssignment, poly);
		return std::any_of(roots.begin(), roots.end(), [&number](const RAN& root) {
			return root >= number;
		});
	}

	bool hasRootBelow(const cadcells::datastructures::PolyRef& poly, const RAN& number) {
		std::vector<RAN> roots = helpers.mProjections->real_roots(mCurrentAssignment, poly);
		return std::any_of(roots.begin(), roots.end(), [&number](const RAN& root) {
			return root <= number;
		});
	}

	PolyRefVector requiredCoefficients(const cadcells::datastructures::PolyRef& poly) {
		PolyRefVector result;
		//Get "real" poly object to substract leading term if necessary
		carl::MultivariatePolynomial<Rational> p = helpers.mPool->get(poly);
		SMTRAT_LOG_DEBUG("smtrat.covering", "Get required Coefficients of: " << p);
		while (!carl::is_zero(p)) {
			cadcells::datastructures::PolyRef ldcf = helpers.mProjections->ldcf(helpers.mPool->insert(p));
			SMTRAT_LOG_DEBUG("smtrat.covering", "Found leading coefficient: " << ldcf);
			result.add(ldcf);
			if (helpers.mProjections->is_zero(mCurrentAssignment, ldcf)) {
				SMTRAT_LOG_DEBUG("smtrat.covering", "Evaluated to zero at current assignment");
				break;
			}
			p = p - p.lterm();
			SMTRAT_LOG_DEBUG("smtrat.covering", "After substracting leading term: " << p);
		}

		return result;
	}

	PolyRefVector constructCharacterization(const std::size_t level) {
		orderAndCleanIntervals(mCoveringInformation[level + 1]);
		PolyRefVector result;
		for (const CellInformation& cell : mCoveringInformation[level + 1]) {
			result.add(cell.mBottomPolys); //Algo line 5
			for (const auto& mainPoly : cell.mMainPolys) {
				result.add(helpers.mProjections->disc(mainPoly)); //Algo line 6
				result.add(requiredCoefficients(mainPoly));		  //Algo line 7
				for (const auto& lowerReasonPoly : cell.mLowerPolys) {
					if (hasRootBelow(lowerReasonPoly, cell.mLowerBound.value())) {
						result.add(helpers.mProjections->res(mainPoly, lowerReasonPoly)); //Algo line 8
					}
				}
				for (const auto& upperReasonPoly : cell.mUpperPolys) {
					if (hasRootAbove(upperReasonPoly, cell.mUpperBound.value())) {
						result.add(helpers.mProjections->res(mainPoly, upperReasonPoly)); //Algo line 9
					}
				}
			}
		}
		for (std::size_t i = 0; i < mCoveringInformation[level + 1].size() - 1; i++) {
			for (const auto& p : mCoveringInformation[level + 1][i].mUpperPolys) {
				for (const auto& q : mCoveringInformation[level + 1][i + 1].mLowerPolys) {
					result.add(helpers.mProjections->res(p, q)); //Algo line 11
				}
			}
		}
		//Todo: standard CAD simplifications to result vector
		result.reduce();
		return result;
	}

	CellInformation intervalFromCharacterization(const PolyRefVector& characterization, const RAN& sample, const std::size_t level) {
		PolyRefVector lowerReasons;
		PolyRefVector upperReasons;
		PolyRefVector main;
		PolyRefVector bottom;
		std::optional<RAN> l;
		std::optional<RAN> u;
		std::vector<RAN> roots;

		for (const auto& poly : characterization) {
			if (poly.level == level) {
				main.add(poly); //Algo line 2
			} else {
				bottom.add(poly); //Algo line 1
			}
			std::vector<RAN> cur_roots = helpers.mProjections->real_roots(mCurrentAssignment, poly);
			roots.insert(roots.end(), cur_roots.begin(), cur_roots.end());
		}

		std::sort(roots.begin(), roots.end());

		auto tmp_l = std::lower_bound(roots.begin(), roots.end(), sample); //Algo line 4
		if (tmp_l != roots.end()) {
			l = *tmp_l;
		}
		auto tmp_u = std::upper_bound(roots.begin(), roots.end(), sample); //Algo line 5
		if (tmp_u != roots.end()) {
			u = *tmp_u;
		}
		//todo make special case for u = l != +-infty
		if (l) { //if l is not -infty
			mCurrentAssignment[mVariableOrdering[level]] = l.value();
			for (const auto& poly : main) {
				if (helpers.mProjections->is_zero(mCurrentAssignment, poly)) {
					lowerReasons.add(poly);
				}
			}
			mCurrentAssignment.erase(mVariableOrdering[level]);
		}
		if (u) { //if u is not infty
			mCurrentAssignment[mVariableOrdering[level]] = u.value();
			for (const auto& poly : main) {
				if (helpers.mProjections->is_zero(mCurrentAssignment, poly)) {

					upperReasons.add(poly);
				}
			}
			mCurrentAssignment.erase(mVariableOrdering[level]);
		}

		//Todo make more specific
		return CellInformation{l, u, main, bottom, lowerReasons, upperReasons};
	}

	//TODO: Use substitute instead of evaluate!
	//TODO: REFACTOR!
	std::vector<CellInformation> getUnsatIntervals(const std::size_t level) {
		SMTRAT_LOG_DEBUG("smtrat.covering", "getUnsatIntervals for level: " << level);
		std::vector<CellInformation> result;
		carl::Variable mainVar = mVariableOrdering[level];
		assert(mCurrentAssignment.count(mainVar) == 0);
		for (const auto& constraint : mConstraints[level]) {
			SMTRAT_LOG_DEBUG("smtrat.covering", "Current constraint: " << constraint << " ;  Current assignment: " << mCurrentAssignment);
			//Note: Roots are sorted in ascending order
			std::vector<RAN> roots = helpers.mProjections->real_roots(mCurrentAssignment, constraint);
			SMTRAT_LOG_DEBUG("smtrat.covering", "Found roots: " << roots);

			if (roots.empty()) {
				//either true or false for whole number line
				//Just check at 0
				bool wasSet = mCurrentAssignment.count(mainVar) != 0;
				RAN oldValue;
				if (wasSet) {
					oldValue = mCurrentAssignment[mainVar];
				}
				mCurrentAssignment[mainVar] = RAN(0);
				RAN value = carl::evaluate(helpers.mPool->get(constraint), mCurrentAssignment).value();
				if (carl::compare(value, mpq_class(0), carl::Relation::GEQ)) {
					//True case
					continue; //Algo line 9
				} else {
					//False case ;
					result.push_back(CellInformation{std::nullopt, std::nullopt, {constraint}, {}, {}, {}});
					return result; //Algo line 7
				}
				if (wasSet) {
					mCurrentAssignment[mainVar] = oldValue;
				} else {
					mCurrentAssignment.erase(mainVar);
				}
			}

			//Todo: Use proper infeasible intervals and bound types
			//Todo: Think about how to clean up the following code
			//Todo: Polynomial simplifications of section 4.4.3 (they are already normalized but not square free -> to that in PolyRefVector::add?)

			bool wasSet = mCurrentAssignment.count(mainVar) != 0;
			RAN oldValue;
			RAN value ;
			if (wasSet) {
				oldValue = mCurrentAssignment[mainVar];
			}
			//Algo line 11 and following
			//Add (-infty, roots[0])
			mCurrentAssignment[mainVar] = carl::sample_below(roots.front());
			value = carl::evaluate(helpers.mPool->get(constraint), mCurrentAssignment).value();
			if (!carl::compare(value, mpq_class(0), carl::Relation::GEQ)) {
				//If constraint evaluates to false
				result.push_back(CellInformation{std::nullopt, roots.front(), {constraint}, {}, {}, {constraint}});
			}

			//add [roots[0], roots[0]]
			mCurrentAssignment[mainVar] = roots.front();
			value = carl::evaluate(helpers.mPool->get(constraint), mCurrentAssignment).value();
			if (!carl::compare(value, mpq_class(0), carl::Relation::GEQ)) {
				//If constraint evaluates to false
				result.push_back(CellInformation{roots.front(), roots.front(), {constraint}, {}, {constraint}, {constraint}});
			}

			for (std::size_t i = 0; i < roots.size() - 1; i++) {
				//add (roots[i], roots[i+1])
				mCurrentAssignment[mainVar] = carl::sample_between(roots[i], roots[i+1]);
				value = carl::evaluate(helpers.mPool->get(constraint), mCurrentAssignment).value();
				if (!carl::compare(value, mpq_class(0), carl::Relation::GEQ)) {
					//If constraint evaluates to false
					result.push_back(CellInformation{roots[i], roots[i+1], {constraint}, {}, {}, {constraint}});
				}
				//add [roots[i+1], roots[i+1]]
				mCurrentAssignment[mainVar] = roots[i+1];
				value = carl::evaluate(helpers.mPool->get(constraint), mCurrentAssignment).value();
				if (!carl::compare(value, mpq_class(0), carl::Relation::GEQ)) {
					//If constraint evaluates to false
					result.push_back(CellInformation{roots[i+1], roots[i+1], {constraint}, {}, {}, {constraint}});
				}
			}

			
			//Add (roots[roots.size()], infty])
			mCurrentAssignment[mainVar] = carl::sample_above(roots.back());
			value = carl::evaluate(helpers.mPool->get(constraint), mCurrentAssignment).value();
			if (!carl::compare(value, mpq_class(0), carl::Relation::GEQ)) {
				//If constraint evaluates to false
				result.push_back(CellInformation{roots.back(), std::nullopt, {constraint}, {}, {constraint}, {}});
			}

			//Set Assignment to before state
			if (wasSet) {
				mCurrentAssignment[mainVar] = oldValue;
			} else {
				mCurrentAssignment.erase(mainVar);
			}
		}
		
		SMTRAT_LOG_DEBUG("smtrat.covering", "Found Unsat Intervals: " << result);
		return result;
	}

	Answer getUnsatCover(const std::size_t level) {
		SMTRAT_LOG_DEBUG("smtrat.covering", " getUnsatCover for level: " << level);
		std::vector<CellInformation> cells = getUnsatIntervals(level);
		mCoveringInformation[level].insert(mCoveringInformation[level].end(), cells.begin(), cells.end());
		orderAndCleanIntervals(mCoveringInformation[level]);
		RAN sample;
		while (sampleOutside(mCoveringInformation[level], sample)) {
			mCurrentAssignment[mVariableOrdering[level]] = sample;
			if (level == mVariableOrdering.size()) {
				//sample is full dimensional -> return SAT
				return Answer::SAT;
			}
			Answer recursiveCall = getUnsatCover(level + 1);
			if (recursiveCall == Answer::SAT) {
				return Answer::SAT;
			}
			PolyRefVector characterization = constructCharacterization(level);
			mCurrentAssignment.erase(mVariableOrdering[level]);
			CellInformation interval = intervalFromCharacterization(characterization, sample, level);
			//todo: Add directly into the correct position according to interval ordering
			mCoveringInformation[level].push_back(interval);
			//delete stored information of next higher dimension
			mCoveringInformation[level + 1].clear();
		}
		mCoveringInformation[level + 1].clear(); //Todo is that necessary?
		return Answer::UNSAT;
	}

	~Backend() {
	}
};
} // namespace smtrat
