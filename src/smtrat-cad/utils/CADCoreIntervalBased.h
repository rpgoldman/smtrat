#pragma once

#include "../common.h"
#include "../Settings.h"

#include <smtrat-cad/lifting/Sample.h>
#include <smtrat-cad/lifting/CADInterval.h>
// @todo #include <smtrat-cad/lifting/LiftingLevel.h>

#include <carl/core/polynomialfunctions/RootFinder.h>

namespace smtrat {
namespace cad {

template<CoreIntervalBasedHeuristic CH>
struct CADCoreIntervalBased {};

template<>
struct CADCoreIntervalBased<CoreIntervalBasedHeuristic::UnsatCover> {
	// template<typename CADIntervalBased>
	// bool doProjection(CADIntervalBased& cad) {
	// 	auto r = cad.mProjection.projectNewPolynomial();
	// 	if (r.none()) {
	// 		SMTRAT_LOG_INFO("smtrat.cad", "Projection has finished.");
	// 		return false;
	// 	}
	// 	SMTRAT_LOG_INFO("smtrat.cad", "Projected into " << r << ", new projection is" << std::endl << cad.mProjection);
	// 	return true;
	// }
	// template<typename CADIntervalBased>
	// bool doLifting(CADIntervalBased& cad) {
	// 	//@todo is mLifting.back() applicable here? maybe specify lifting level or iterator instead for recursion
    //     // no lifting is possible if an unsat cover was found
	// 	if (cad.mLifting.back().isUnsatCover()) return false;

    //     while(!cad.mLifting.back().isUnsatCover()) {
    //         // compute a new sample outside of known unsat intervals
    //         auto s = cad.mLifting.back().chooseSample();
    //         SMTRAT_LOG_TRACE("smtrat.cad", "Next sample" << std::endl << s);
    //         //@todo check whether all former levels + new sample are sat, if true return sat

    //         auto intervals = cad.mLifting.getUnsatIntervals();
	// 	    SMTRAT_LOG_TRACE("smtrat.cad", "Current intervals" << std::endl << intervals);
    //     }

    //     //@todo this is code from CADCore, adapt
	// 	/*assert(0 <= it.depth() && it.depth() < cad.dim());
	// 	SMTRAT_LOG_DEBUG("smtrat.cad", "Processing " << cad.mLifting.extractSampleMap(it));
	// 	if (it.depth() > 0 && cad.checkPartialSample(it, cad.idLP(it.depth())) == Answer::UNSAT) {
	// 		cad.mLifting.removeNextSample();
	// 		return false;
	// 	}
	// 	auto polyID = cad.mProjection.getPolyForLifting(cad.idLP(it.depth() + 1), s.liftedWith());
	// 	if (polyID) {
	// 		const auto& poly = cad.mProjection.getPolynomialById(cad.idLP(it.depth() + 1), *polyID);
	// 		SMTRAT_LOG_DEBUG("smtrat.cad", "Lifting " << cad.mLifting.extractSampleMap(it) << " with " << poly);
	// 		cad.mLifting.liftSample(it, poly, *polyID);
	// 	} else {
	// 		SMTRAT_LOG_DEBUG("smtrat.cad", "Current lifting" << std::endl << cad.mLifting.getTree());
	// 		SMTRAT_LOG_TRACE("smtrat.cad", "Queue" << std::endl << cad.mLifting.getLiftingQueue());
	// 		cad.mLifting.removeNextSample();
	// 		cad.mLifting.addTrivialSample(it);
	// 	} */
	// 	return true;
	// }


	/** custom sort for sets of CADIntervals */
	struct SortByLowerBound
	{
		bool operator ()(CADInterval* a, CADInterval* b) const {
			return a->isLowerThan(*b);
		}
	};


	/** @brief checks whether the first variable is at least as high in the var order as the second one 
	 * 
	 * @returns true iff the first var is at least as high in the var order as the second one
	*/
	template<typename CADIntervalBased>
	bool isAtLeastCurrVar(
		CADIntervalBased& cad,	/**< corresponding CAD */
		carl::Variable v, 		/**< variable to check */
		carl::Variable currVar	/**< current variable */
	) {
		if(cad.getDepthOfVar(v) >= cad.getDepthOfVar(currVar))
			return true;

		return false;
	}


	/** @brief gets highest var in var order that is present in the polynom
	 * 
	 * with respect to variable order in cad
	 * 
	 * @returns highest var in polynom
	*/
	template<typename CADIntervalBased>
	carl::Variable getHighestVar(
		CADIntervalBased& cad,	/**< corresponding CAD */
		smtrat::Poly poly		/**< polynom */
	) {
		carl::Variable var;
		for(auto v : carl::variables(poly).underlyingVariables()) {
			if(cad.getDepthOfVar(v) > cad.getDepthOfVar(var)) 
				var = v;
		}
		return var;
	}


	/**
	 * Calculates the regions between the real roots of the polynom (left-hand side) of given constraint
	 * @param samples variables to be substituted by given values, can be empty
	 * @param currVar variable of current level
	 * 
	 * @returns set of intervals ordered by lower bounds, ascending
	 * 
	 * (Paper Alg. 1, l.9-11)
	 */
	template<typename CADIntervalBased>
	std::set<CADInterval*, SortByLowerBound> calcRegionsFromPoly(
		CADIntervalBased& cad,					/**< corresponding CAD */
		ConstraintT c, 							/**< constraint containing the polynom */
		Assignment samples,	/**< variables to be substituted by given values, may be empty */
		carl::Variable currVar					/**< constraint is considered as univariate in this variable */
	) {
		std::set<CADInterval*, SortByLowerBound> regions;

		// find real roots of constraint corresponding to current var
		auto r = carl::rootfinder::realRoots(carl::to_univariate_polynomial(c.lhs(), currVar), samples);
		std::sort(r.begin(), r.end());
		
		// go through roots to build region intervals and add them to the lifting level
		for (int i = 0; i < r.size(); i++) {
			// add closed point interval for each root
			regions.insert( new CADInterval(r.at(i), c.lhs()) );

			// add (-inf, x) for first root x
			if (i == 0)
				regions.insert( new CADInterval((RAN) 0, r.at(i), CADInterval::CADBoundType::INF, CADInterval::CADBoundType::OPEN, c.lhs()) );
			
			// for last root x add (x, inf)
			if (i == r.size()-1)
				regions.insert( new CADInterval(r.at(i), (RAN) 0, CADInterval::CADBoundType::OPEN, CADInterval::CADBoundType::INF, c.lhs()) );
			else // add open interval to next root
				regions.insert( new CADInterval(r.at(i), r.at(i+1), c.lhs()) );
		}

		return regions;
	}

	/** converts a Assignment to EvalRationalMap
	 * as different carl classes need the same information in different format
	 */
	template<typename CADIntervalBased>
	 EvalRationalMap makeEvalMap( 
		 CADIntervalBased& cad,						/**< corresponding CAD */ 
		 const Assignment& orig						/**< map to convert */
	) {
		// convert eval map to usable format
		EvalRationalMap map;
		for(auto entry : orig) {
			std::pair<carl::Variable, Rational> newentry = std::make_pair(entry.first, entry.second.value());
			map.insert(newentry);
		}
		return map;
	}


	/** @brief gets unsat intervals for depth i corresponding to given sample map
	 * 
	 * @returns unsat intervals ordered by lower bound, ascending
	 * 
	 * (Paper Alg. 1)
	*/
	template<typename CADIntervalBased>
	std::set<CADInterval*, SortByLowerBound> get_unsat_intervals(
		CADIntervalBased& cad,			/**< corresponding CAD */ 
		Assignment samples,				/**< values for variables of depth till i-1 (only used if dimension is > 1) */
		carl::Variable currVar			/**< variable of current depth i */
	) {

		// @todo this checks all vars, not just main vars
		/* constraints are filtered for ones with main var currVar or higher */
		std::vector<ConstraintT> constraints;
		for(auto c : cad.getConstraints()) {
			auto consvars = c.variables();
			for(auto v : consvars.underlyingVariables()) {
				if(isAtLeastCurrVar(cad, v, currVar)) {
					constraints.push_back(c);
					break;
				}
			}
		}

		/* gather intervals from each constraint */
		std::set<CADInterval*, SortByLowerBound> newintervals;
		for(auto c : constraints) {
			unsigned issat = c.satisfiedBy(makeEvalMap(cad, samples));
			/* if unsat, return (-inf, +inf) */
			if(issat == 0) { /*@todo is this equiv to "c(s) == false"? */
				newintervals.clear();
				newintervals.insert(new CADInterval(c.lhs()));
				return newintervals;
			}
			/* if sat, constraint is finished */
			else if(issat == 1)
				continue;
			else { /* @todo this should be the satisfiedBy result with open vars */
				// get unsat intervals for constraint
				auto regions = calcRegionsFromPoly(cad, c, samples, currVar);
				for(auto region : regions) {
					auto r = region->getRepresentative();
					auto eval = new Assignment(samples);
					eval->insert(std::pair<carl::Variable, RAN>(currVar, r));
					std::set<Poly> lowerreason;
					std::set<Poly> upperreason;
					if(c.satisfiedBy(makeEvalMap(cad, *eval)) == 0) { //@todo again, is this right?
						if(region->getLowerBoundType() != CADInterval::CADBoundType::INF)
							lowerreason.insert(c.lhs());
						if(region->getUpperBoundType() != CADInterval::CADBoundType::INF)
							upperreason.insert(c.lhs());
						newintervals.insert(new CADInterval(region->getLower(), region->getUpper(), region->getLowerBoundType(), region->getUpperBoundType(), lowerreason, upperreason, c.lhs()));
					}
				} 
			}
		}
		return newintervals;
	}


	/** 
	 * @brief Gives the lowest bound followed by an unexplored region
	 * 
	 * Goes through the unsat intervals starting from -inf,
	 * if -inf is not a bound yet it is determined to be the first "upper" bound.
	 * Else the first bound not followed by another interval is returned.
	 * 
	 * @returns bool 				(1st value of tuple) true iff a bound was found
	 * @returns RAN  				(2nd value of tuple) bound iff one was found, 0 otherwise
	 * @returns CADBoundType 		(3rd value of tuple) type of bound if one was found or OPEN if none was found (if this is INF, the found "bound" is -inf)
	 * @returns set<CADInterval> 	(4th value of tuple) contains intervals of covering iff no bound was found, else empty
	 * 
	 * @note The output (true, 0, INF, emptyset) stands for an unexplored interval before the first given interval
	 */
	template<typename CADIntervalBased>
	std::tuple<bool, RAN, CADInterval::CADBoundType, std::set<CADInterval*, SortByLowerBound>> getLowestUpperBound(
		CADIntervalBased& cad,				/**< corresponding CAD */
		std::set<CADInterval*, SortByLowerBound> intervals	/**< set of intervals to be checked for unexplored regions */
	) {
		// check whether there are intervals
		if(intervals.empty()) {
			auto tuple = std::make_tuple(false, (RAN) 0, CADInterval::CADBoundType::OPEN, std::set<CADInterval*, SortByLowerBound>());
			return tuple;
		}

		// check for (-inf, +inf) interval
		for(auto inter : intervals) {
			if(inter->isInfinite()) {
				auto infset = std::set<CADInterval*, SortByLowerBound>();
				infset.insert(inter);
				auto tuple = std::make_tuple(false, (RAN) 0, CADInterval::CADBoundType::OPEN, infset);
			}
		}

		std::set<CADInterval*, SortByLowerBound> cover;

		// get an interval with -inf bound, store its higher bound
		RAN highestbound = (RAN) 0;
		CADInterval::CADBoundType boundopen = CADInterval::CADBoundType::OPEN;
		bool hasminf = false;
		for(auto inter : intervals) {
			// check for singleton cover
			if(inter->isInfinite()) {
				auto infset = std::set<CADInterval*, SortByLowerBound>();
				infset.insert(inter);
				auto tuple = std::make_tuple(false, (RAN) 0, CADInterval::CADBoundType::OPEN, infset);
				return tuple;
			}
			if(inter->getLowerBoundType() == CADInterval::CADBoundType::INF) {
				// note: the higher bound cannot be +inf as there is singleton cover was checked before
				highestbound = inter->getUpper();
				boundopen = inter->getUpperBoundType();
				cover.insert(inter);
				hasminf = true;
				break;
			}
		}
		// if -inf is no bound in any interval, there is some unexplored region before the first interval
		if(!hasminf) {
			auto tuple = std::make_tuple(true, (RAN) 0, CADInterval::CADBoundType::INF, std::set<CADInterval*, SortByLowerBound>());
			return tuple;
		}

		// iteratively check for highest reachable bound
		bool stop = false;
		while(!stop) {
			bool updated = false;
			for(auto inter : intervals) {
				updated = false;
				// if the upper bound is the highest bound but is included only update bound type
				if(highestbound == inter->getUpper() && boundopen == CADInterval::CADBoundType::OPEN && inter->getUpperBoundType() == CADInterval::CADBoundType::CLOSED) {
					boundopen = inter->getUpperBoundType();
					cover.insert(inter);
					updated = true;
				}
				// update if the upper bound is not equal to highestbound 
				// note: checking (highestbound < upper bound) would ommit (upper bound == INF) case
				else if( !(highestbound == inter->getUpper() && boundopen == inter->getUpperBoundType()) ) {
					// and if highestbound is contained in the interval or is bordered by the lower bound of the interval
					// (also excludes the case upper bound < highestbound)
					assert(boundopen != CADInterval::CADBoundType::INF);
					if(inter->contains(highestbound) ||
						(highestbound == inter->getLower() && boundopen != inter->getLowerBoundType() && 
						inter->getLowerBoundType() != CADInterval::CADBoundType::INF)) {
						
						cover.insert(inter);
						if(inter->getUpperBoundType() == CADInterval::CADBoundType::INF) {
							// an unset cover was found
							auto tuple = std::make_tuple(false, (RAN) 0, CADInterval::CADBoundType::OPEN, cover);
							return tuple;
						}
						// update to next higher bound
						highestbound = inter->getUpper();
						boundopen = inter->getUpperBoundType();
						updated = true;
					}
				}
			}
			// if the highest bound could not be updated (& was not +inf), break
			if(!updated) {
				stop = true;
			}
		}
		
		auto tuple = std::make_tuple(true, highestbound, boundopen, std::set<CADInterval*, SortByLowerBound>());
		return tuple;
	}


	/**@brief computes a cover from the given set of intervals
	 * 
	 * @returns subset of intervals that form a cover or an empty set if none was found
	 * (Paper Alg. 2)
	 */
	template<typename CADIntervalBased>
	std::set<CADInterval*, SortByLowerBound>compute_cover(
		CADIntervalBased& cad, 			/**< corresponding CAD */
		std::set<CADInterval*, SortByLowerBound> inters	/**< set of intervals over which to find a cover */
	) {
		// return cover or empty set if none was found
		auto boundtuple = getLowestUpperBound(cad, inters);
		return std::get<3>(boundtuple);
	}

	/** @brief computes the next sample
	 * 
	 * Chooses a Sample outside the currently known unsat intervals
	 * 
	 * @returns RAN in unexplored interval
	 * @note check whether an unsat cover has been found before calling this!
	 */
	template<typename CADIntervalBased>
	RAN chooseSample(
		CADIntervalBased& cad,			/**< corresponding CAD */
		std::set<CADInterval*, SortByLowerBound> inters	/**< known unsat intervals */
	) {
		// if -inf is not a bound find sample in (-inf, first bound)
		bool hasminf = false;
		bool first = true;
		RAN lowestval = RAN(0);
		for(auto inter : inters) {
			if(inter->getLowerBoundType() == CADInterval::CADBoundType::INF) {
				hasminf = true;
				break;
			}
			else {
				// get lowest bound
				if(first) {
					lowestval = inter->getLower();
					first = false;
				}
				else {
					if(inter->getLower() < lowestval)
						lowestval = inter->getLower();
				}
			}
		}
		// if no -inf bound, get val below
		if(!hasminf) {
			return sampleBelow(lowestval);
		}

		// get first unexplored region
		auto boundtuple = getLowestUpperBound(cad, inters);
		assert(std::get<0>(boundtuple)); //@todo handle this instead
		RAN bound = std::get<1>(boundtuple);
		// note: at this point the bound is not -inf (case is handled above)

		// get lower bound of next interval after the unexplored region iff one exists
		bool found = false;
		RAN upperbound;
		CADInterval::CADBoundType upperboundtype;
		for(auto inter : inters) {
			if(bound < inter->getLower() && inter->getLowerBoundType() != CADInterval::CADBoundType::INF) {
				found = true;
				upperbound = inter->getLower();
				upperboundtype = inter->getLowerBoundType();
			}
			// case bound == inter.lower can only happen if found == true, initially was covered by getLowestUpperBound
			else if(bound == inter->getLower() && upperboundtype == CADInterval::CADBoundType::OPEN 
				&& inter->getLowerBoundType() == CADInterval::CADBoundType::CLOSED) {
				upperboundtype == CADInterval::CADBoundType::CLOSED;
			}
		}
		// if none was found, next bound is +inf
		if(!found) {
			upperboundtype = CADInterval::CADBoundType::INF;
			upperbound = (RAN) 0;
		}

		// create interval in which to find the next sample
		CADInterval* sampleinterval = new CADInterval(bound, upperbound, std::get<2>(boundtuple), upperboundtype);
		return sampleinterval->getRepresentative();
	}


	/** 
	 * @brief get all coefficients required for the given sample set
	 * (Paper Alg. 5)
	*/
	template<typename CADIntervalBased>
	std::set<smtrat::Poly> required_coefficients(
		CADIntervalBased& cad,					/**< corresponding CAD */
		Assignment samples,						/**< values for variables till depth i */
		std::set<smtrat::Poly> polys			/**< polynoms */
	) {
		std::set<smtrat::Poly> coeffs;
		for(auto poly : polys) {
			while(!carl::isZero(poly)) {
				// add leading coefficient
				smtrat::Poly lcpoly = smtrat::Poly(poly.lcoeff());
				coeffs.insert(lcpoly);
				// bring samples in right form for evaluation
				std::map<carl::Variable, smtrat::Rational> map;
				for(auto sample : samples)
					map.insert(std::make_pair(sample.first, sample.second.value()));
				// if leading coeff evaluated at sample is non zero, stop
				if(lcpoly.evaluate(map) != (smtrat::Rational) 0)
					break;
				// remove leading term
				poly = poly.stripLT();
			}
		}

		return coeffs;
	}


	/**@brief checks whether (polynom with given offset == 0) is satisfied by given sample */
	template<typename CADIntervalBased>
	bool isSatWithOffset(
		CADIntervalBased& cad,					/**< corresponding CAD */
		RAN offset,								/**< offset */
	 	Assignment samples,	/**< values for variables till depth i-1 */
		smtrat::Poly poly,						/**< polynom */
		carl::Relation relation					/**< relation to use for constraint */
	) {
		smtrat::Poly offsetpoly = poly;
		const carl::Term<smtrat::Rational>* term = new carl::Term(offset.value());
		offsetpoly.addTerm((*term)); // @todo is this what is meant in alg.6, l.6-7?
		ConstraintT eqzero = ConstraintT(offsetpoly, relation);

		// check whether poly(samples x offset) == 0
		unsigned issat = eqzero.satisfiedBy(makeEvalMap(cad, samples));
		if(issat == 1)
			return true;

		return false;
	}


	/**
	 * 
	 * @note asserts that there is a cover in the given intervals
	 * (Paper Alg. 4)
	 * */
	template<typename CADIntervalBased>
	std::set<smtrat::Poly> construct_characterization(
		CADIntervalBased& cad,					/**< corresponding CAD */
		Assignment samples,						/**< values for variables till depth i */
		std::set<CADInterval*, SortByLowerBound> intervals		/**< intervals containing a cover */
	) {
		// get subset of intervals that has no intervals contained in any other one
		std::set<CADInterval*, SortByLowerBound> subinters = compute_cover(cad, intervals);
		assert(!subinters.empty());

		// create the characterization of the unsat region
		std::set<smtrat::Poly> characterization;
		for(auto inter : subinters) {
			// add all polynoms not containing the main var
			for(auto lowpoly : inter->getLowerPolynoms()) {
				characterization.insert(lowpoly);
			}
			// add discriminant of polynoms containing main var
			for(auto poly : inter->getPolynoms()) {
				smtrat::cad::UPoly upoly = carl::discriminant(carl::to_univariate_polynomial(poly, getHighestVar(cad, poly)));
				// convert polynom to multivariate
				smtrat::Poly inspoly = smtrat::Poly(upoly);
				characterization.insert(inspoly);
			}
			// add relevant coefficients
			auto coeffs = required_coefficients(cad, samples, inter->getPolynoms());
			characterization.insert(coeffs.begin(), coeffs.end());
			// add polynoms that guarantee bounds to be closest
			for(auto q : inter->getPolynoms()) {
				// @todo does the following correctly represent Alg. 4, l. 7-8?
				// idea: P(s x \alpha) = 0, \alpha < l => P(s x l) > 0
				if(isSatWithOffset(cad, inter->getLower(), samples, q, carl::Relation::GREATER)) {
					for(auto p : inter->getLowerReason()) {
						smtrat::cad::UPoly upoly = carl::resultant(
							carl::to_univariate_polynomial(p, getHighestVar(cad, p)),
							carl::to_univariate_polynomial(q, getHighestVar(cad, q))
						);
						// convert polynom to multivariate
						smtrat::Poly inspoly = smtrat::Poly(upoly);
						characterization.insert(inspoly);
					}
				}
				// analogously: P(s x \alpha) = 0, \alpha > u => P(s x u) < 0
				if(isSatWithOffset(cad, inter->getUpper(), samples, q, carl::Relation::LESS)) {
					for(auto p : inter->getUpperReason()) {
						smtrat::cad::UPoly upoly = carl::resultant(
							carl::to_univariate_polynomial(p, getHighestVar(cad, p)),
							carl::to_univariate_polynomial(q, getHighestVar(cad, q))
						);
						// convert polynom to multivariate
						smtrat::Poly inspoly = smtrat::Poly(upoly);
						characterization.insert(inspoly);
					}
				}
			}
		}

		// add resultants of upper & lower reasons
		auto itlower = subinters.begin();
		itlower++;
		auto itupper = subinters.begin();
		while(itlower != subinters.end()) {
			for(auto lower : (*itlower)->getLowerReason()) {
				for(auto upper : (*itupper)->getUpperReason()) {
					// convert polynom to multivariate
					smtrat::cad::UPoly upoly = carl::resultant(
						carl::to_univariate_polynomial(upper, getHighestVar(cad, upper)),
						carl::to_univariate_polynomial(lower, getHighestVar(cad, lower))
					);
					smtrat::Poly inspoly = smtrat::Poly(upoly);
					characterization.insert(inspoly);
				}
			}
			itlower++;
			itupper++;
		}

		//@todo Alg. 4, l. 11: Perform standard CAD simplifications to characterization
		return characterization;
	}


	/**
	 * (Paper Alg. 6)
	 * */
	template<typename CADIntervalBased>
	CADInterval* interval_from_characterization(
		CADIntervalBased& cad,					/**< corresponding CAD */
	 	Assignment samples,	/**< values for variables till depth i-1 */
		carl::Variable currVar,					/**< var of depth i (current depth) */
		RAN val, 								/**< value for currVar */
		std::set<smtrat::Poly> butler			/**< poly characterization */
	) {
		// partition polynomials for containing currVar
		std::set<smtrat::Poly> notp_i;
		std::set<smtrat::Poly> p_i;
		std::set<RAN> realroots;
		for(auto poly : butler) {
			if(!poly.has(currVar))
				notp_i.insert(poly);
			else
			{
				p_i.insert(poly);
				// store the real roots with substituted values until depth i-1
				auto roots = carl::rootfinder::realRoots(carl::to_univariate_polynomial(poly, currVar), samples);
				realroots.insert(roots.begin(), roots.end());
			}
		}

		// find lower/upper bounds in roots
		bool foundlower = false;
		bool foundupper = false;
		RAN lower;
		RAN upper;
		for(auto r : realroots) {
			// is the root a max lower bound
			if(!foundlower && r <= val) {
				foundlower = true;
				lower = r;
			}
			else if(foundlower && r <= val && r > lower)
				lower = r;

			// is the root a min upper bound
			if(!foundupper && r >= val) {
				foundupper = true;
				upper = r;
			}
			else if(foundupper && r >= val && r < upper)
				upper = r;
		}

		// find reasons for bounds
		std::set<smtrat::Poly> lowerres;
		std::set<smtrat::Poly> upperres; 
		for(auto poly : p_i) {
			//@todo is the isSatWithOffset function what was meant in Alg. 6, l. 6-7?
			// if no lower bound was found it is -inf, no polys will bound it
			if(foundlower) {
				if(isSatWithOffset(cad, lower, samples, poly, carl::Relation::EQ))
					lowerres.insert(poly);
			}
			// analogously to lower bound
			if(foundupper) {
				if(isSatWithOffset(cad, upper, samples, poly, carl::Relation::EQ))
					upperres.insert(poly);
			}
		}

		// determine bound types
		CADInterval::CADBoundType lowertype = CADInterval::CADBoundType::CLOSED;
		CADInterval::CADBoundType uppertype = CADInterval::CADBoundType::CLOSED;
		if(!foundlower){
			lowertype = CADInterval::CADBoundType::INF;
			lower = (RAN) 0;
		}
		if(!foundupper){
			uppertype = CADInterval::CADBoundType::INF;
			upper = (RAN) 0;
		}
		
		return new CADInterval(lower, upper, lowertype, uppertype, lowerres, upperres, p_i, notp_i);
	}

	/**
	 * computes whether an unsat cover can be found or whether there is a satisfying witness
	 * 
	 * @returns true if SAT, false if UNSAT
	 * @returns in UNSAT case: intervals forming the unsat cover
	 * @returns in SAT case: satisfying witness
	 * 
	 * (Paper Alg. 3)
	 */
	template<typename CADIntervalBased>
	std::tuple<bool, std::set<CADInterval*, SortByLowerBound>, Assignment> get_unsat_cover(
		CADIntervalBased& cad,					/**< corresponding CAD */
		Assignment samples,						/**< values for variables of depth i-1 (initially empty set) */
		carl::Variable currVar					/**< var of depth i (current depth) */
	) {
		// get known unsat intervals
		auto unsatinters = get_unsat_intervals(cad, samples, currVar);

		// run until a cover was found
		while(compute_cover(cad, unsatinters).empty()) {
			// add new sample for current variable
			auto newsamples = new Assignment(samples);
			RAN newval = chooseSample(cad, unsatinters);
			SMTRAT_LOG_TRACE("smtrat.cad", "Next sample " << std::endl << newval);
			newsamples->insert(std::make_pair(currVar, newval));

			// if the sample set has full dimension we have a satifying witness
			if(cad.dim() == cad.getDepthOfVar(currVar))
				return std::make_tuple(true, std::set<CADInterval*, SortByLowerBound>(), *newsamples);

			// recursive recall for next dimension
			auto nextcall = get_unsat_cover(cad, *newsamples, cad.getNextVar(currVar));
			// if SAT
			if(std::get<0>(nextcall))
				return nextcall;
			else {
				auto butler = construct_characterization(cad, *newsamples, std::get<1>(nextcall));
				CADInterval* butlerinter = interval_from_characterization(cad, samples, currVar, newval, butler);
				unsatinters.insert(butlerinter);
			}
		}

		// in case the loop exits a cover was found
		auto res = std::make_tuple(false, unsatinters, Assignment());
		return res;
	}


	template<typename CADIntervalBased>
	Answer operator()(Assignment& assignment, CADIntervalBased& cad) {
		// look for unsat cover
		auto res = get_unsat_cover(cad, Assignment(), cad.getVariables().at(0));
		Answer ans = std::get<0>(res) ? Answer::SAT : Answer::UNSAT;

		// if no cover was found, set SAT assignment
		if(ans == Answer::SAT)
			assignment = std::get<2>(res);
		else assignment = Assignment();

		return ans;
	}
};

}
}