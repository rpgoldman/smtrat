#pragma once

#include "../common.h"

#include <carl/formula/model/ran/RealAlgebraicNumberOperations.h>

namespace smtrat {
namespace cad {
	class CADInterval {
    public: 
        /** bound types for CAD interval bounds */
        enum CADBoundType {
            INF,        /**< infinity */
            OPEN,       /**< open but not infinitiy (does not include bound value) */
            CLOSED      /**< closed (does include bound value) */
        };

	private:
        RAN lower = (RAN) 0;                                                /**< lower bound */
        RAN upper = (RAN) 0;                                                /**< upper bound */
        CADBoundType lowertype = INF;                                       /**< lower bound type */
        CADBoundType uppertype = INF;                                       /**< upper bound type */
        std::set<std::pair<Poly, std::vector<ConstraintT>>> lowerreason;    /**< collection of polynomials for lower bound and from which constraints they originated */
        std::set<std::pair<Poly, std::vector<ConstraintT>>> upperreason;    /**< collection of polynomials for upper bound and from which constraints they originated */
        std::set<ConstraintT> constraints;                                  /**< interval represents the bounds computed from these polynomials (containing x_i) */
        std::set<ConstraintT> lowerconss;                                   /**< polynomials not containing main variable x_i */

    public:
        /** initializes interval as (-inf, +inf) */
		CADInterval(){}

        /** initializes open interval with given bounds */
        CADInterval(RAN lowerbound, RAN upperbound): 
            lower(lowerbound), upper(upperbound) {
            lowertype = OPEN;
            uppertype = OPEN;
        }

        /** initializes closed point interval */
        CADInterval(RAN point): 
            lower(point), upper(point) {
            lowertype = CLOSED;
            uppertype = CLOSED;
        }

        /** initializes closed point interval with constraints */
        CADInterval(RAN point, const std::set<ConstraintT> newconss): 
            lower(point), upper(point), constraints(newconss) {
            lowertype = CLOSED;
            uppertype = CLOSED;
        }

        /** initializes closed point interval with constraint */
        CADInterval(RAN point, const ConstraintT newcons): 
            lower(point), upper(point) {
            lowertype = CLOSED;
            uppertype = CLOSED;
            constraints.insert(newcons);
        }

        /** initializes open interval with given bounds and constraints */
        CADInterval(RAN lowerbound, RAN upperbound, const std::set<ConstraintT> newconss): 
            lower(lowerbound), upper(upperbound), constraints(newconss) {
            lowertype = OPEN;
            uppertype = OPEN;
        }

        /** initializes open interval with given bounds and constraint */
        CADInterval(RAN lowerbound, RAN upperbound, const ConstraintT newcons): 
            lower(lowerbound), upper(upperbound){
            lowertype = OPEN;
            uppertype = OPEN;
            constraints.insert(newcons);
        }

        /** initializes (-inf, +inf) interval with given constraints */
        CADInterval(const std::set<ConstraintT> newconss):
            constraints(newconss) {
        }

        /** initializes (-inf, +inf) interval with given constraint */
        CADInterval(const ConstraintT newcons) {
            constraints.insert(newcons);
        }

        /** initializes interval with given bounds and bound types */
        CADInterval(
            RAN lowerbound, 
            RAN upperbound, 
            CADBoundType lowerboundtype, 
            CADBoundType upperboundtype):
            lower(lowerbound), upper(upperbound), lowertype(lowerboundtype), uppertype(upperboundtype) {
        }

        /** initializes interval with given bounds, bound types and constraints */
        CADInterval(
            RAN lowerbound, 
            RAN upperbound, 
            CADBoundType lowerboundtype, 
            CADBoundType upperboundtype, 
            std::set<ConstraintT> newconss):
            lower(lowerbound), upper(upperbound), lowertype(lowerboundtype), uppertype(upperboundtype), 
            constraints(newconss) {
        }

        /** initializes interval with given bounds, bound types and constraint */
        CADInterval(
            RAN lowerbound, 
            RAN upperbound, 
            CADBoundType lowerboundtype, 
            CADBoundType upperboundtype, 
            ConstraintT newcons):
            lower(lowerbound), upper(upperbound), lowertype(lowerboundtype), uppertype(upperboundtype) {
            constraints.insert(newcons);
        }

        /** initializes interval with given bounds, bound types, reasons and constraints */
        CADInterval(
            RAN lowerbound, 
            RAN upperbound, 
            CADBoundType lowerboundtype, 
            CADBoundType upperboundtype, 
            std::set<std::pair<Poly, std::vector<ConstraintT>>> lowerres,
            std::set<std::pair<Poly, std::vector<ConstraintT>>> upperres, 
            std::set<ConstraintT> newconss):
            lower(lowerbound), upper(upperbound), lowertype(lowerboundtype), uppertype(upperboundtype), 
            lowerreason(lowerres), upperreason(upperres), constraints(newconss) {
        }

        /** initializes interval with given bounds, bound types, reasons and constraint */
        CADInterval(
            RAN lowerbound, 
            RAN upperbound, 
            CADBoundType lowerboundtype, 
            CADBoundType upperboundtype, 
            std::set<std::pair<Poly, std::vector<ConstraintT>>> lowerres,
            std::set<std::pair<Poly, std::vector<ConstraintT>>> upperres, 
            ConstraintT newcons):
            lower(lowerbound), upper(upperbound), lowertype(lowerboundtype), uppertype(upperboundtype), 
            lowerreason(lowerres), upperreason(upperres){
            
            constraints.insert(newcons);
        }

        /** initializes interval with given bounds, bound types, reasons, constraints and constraints without leading term */
        CADInterval(
            RAN lowerbound, 
            RAN upperbound, 
            CADBoundType lowerboundtype, 
            CADBoundType upperboundtype, 
            std::set<std::pair<Poly, std::vector<ConstraintT>>> lowerres, 
            std::set<std::pair<Poly, std::vector<ConstraintT>>> upperres, 
            std::set<ConstraintT> newconss, 
            std::set<ConstraintT> newredconss):
            lower(lowerbound), upper(upperbound), lowertype(lowerboundtype), uppertype(upperboundtype),
            lowerreason(lowerres), upperreason(upperres), constraints(newconss), lowerconss(newredconss) {
        }

        /** gets lower bound */
        const auto& getLower() const {
            return lower;
        }

        /** gets lower bound type */
        const auto& getLowerBoundType() const {
            return lowertype;
        }

        /** gets upper bound */
        const auto& getUpper() const {
            return upper;
        }

        /** gets upper bound type */
        const auto& getUpperBoundType() const {
            return uppertype;
        }

        /** gets polynomials the lower bound is reasoned from */
        const auto& getLowerReason() {
            return lowerreason;
        }

        /** gets polynomials the upper bound is reasoned from */
        const auto& getUpperReason() {
            return upperreason;
        }

        /** gets constraints the interval originated from */
        const auto& getConstraints() {
            return constraints;
        }

        /** gets the reduced constraints */
        const auto& getLowerConstraints() {
            return lowerconss;
        }

        /** sets lower bound value and bound type */
        void setLowerBound(const RAN& value, CADBoundType type) {
            lower = value;
            lowertype = type;
        }

        /** sets lower bound value and bound type */
        void setUpperBound(const RAN& value, CADBoundType type) {
            upper = value;
            uppertype = type;
        }

        /** sets constraints */
        void setConstraint(const ConstraintT& cons) {
            constraints.clear();
            constraints.insert(cons);
        }

        /** adds poly to lowerreason */
        void addLowerReason(const std::pair<Poly, std::vector<ConstraintT>>& poly) {
            lowerreason.insert(poly);
        }

        /** adds poly to upperreason */
        void addUpperReason(const std::pair<Poly, std::vector<ConstraintT>>& poly) {
            upperreason.insert(poly);
        }

        /** adds cons to constraints */
        void addConstraint(const ConstraintT& cons) {
            constraints.insert(cons);
        }

        /** checks whether the interval is (-inf, +inf) */
        bool isInfinite() {
            if(lowertype == INF && uppertype == INF) {
                return true;
            }
            return false;
        }

        /** checks whether one of the bounds is infinite */
        bool isHalfBounded() {
            if((lowertype == INF && !uppertype == INF) || 
                (!lowertype == INF && uppertype == INF)) {
                return true;
            }
            return false;
        }

        /** checks whether the interval contains the given value */
        bool contains(const RAN& val) {
            if((lowertype == INF || (lowertype == CLOSED && lower <= val) ||
                (lowertype == OPEN && lower < val)) &&
                (uppertype == INF || (uppertype == CLOSED && upper >= val) ||
                (uppertype == OPEN && upper > val))) {
                return true;
            }
            return false;
        }


        /** @brief checks whether the lower bound is lower than the one of the given interval
         * 
         * @note can handle inf bounds and considers upper bounds if equal
         * @returns true iff the lower bound is lower than the one of the given interval
         */
        bool isLowerThan(const CADInterval& inter) {
            // (-inf
            if(lowertype == INF) {
                // (-inf vs. (l
                if(inter.getLowerBoundType() != INF) return true;
                // (-inf, +inf) vs. (-inf is always not lower
                else if(uppertype == INF) return false;
                // (-inf, u) vs. (-inf, +inf)
                else if(inter.getUpperBoundType() == INF) return true;
                // (-inf, u1) vs. (-inf, u2), u1 < u2
                else if(upper < inter.getUpper()) return true;
                // (-inf, u1) vs. (-inf, u2), u1 > u2
                else if(upper > inter.getUpper()) return false;
                // (-inf, u)/] vs. (-inf, u)/]
                else if(upper == inter.getUpper()) {
                    // only lower if (-inf, u) vs. (-inf, u]
                    if(uppertype == OPEN && inter.getUpperBoundType() == CLOSED) return true;
                    else return false;
                }
            }
            // (l vs. (-inf
            else if(inter.getLowerBoundType() == INF) return false;
            // (l1 vs. (l2 with l1 < l2
            else if(lower < inter.getLower()) return true;
            // (l1 vs. (l2 with l1 > l2
            else if(lower > inter.getLower()) return false;
            // (l, vs. [l
            else if(lowertype == OPEN && inter.getLowerBoundType() == CLOSED) return false;
            // [l, vs. (l
            else if(lowertype == CLOSED && inter.getLowerBoundType() == OPEN) return true;
            // [l vs. [l or (l vs. (l
            else {
                // (l, -inf) vs. (l,
                if(uppertype == INF) return false;
                // (l, u) vs. (l, +inf)
                else if(inter.getUpperBoundType() == INF) return true;
                // (l, u1) vs. (l, u2) with u1 < u2
                else if(upper < inter.getUpper()) return true;
                // (l, u1) vs. (l, u2) with u1 > u2
                else if(upper > inter.getUpper()) return false;
                // u) vs. u]
                else if(uppertype == OPEN && inter.getUpperBoundType() == CLOSED) return true;
                // u] vs. u)
                else if(uppertype == CLOSED && inter.getUpperBoundType() == OPEN) return false;
                // (l, u) vs. same (l,u)
                else return false;
            }
            // should not be reachable
            return false;
        }

        /** gets a value within the interval
        */
        RAN getRepresentative() {
            if(lowertype == INF && uppertype == INF) {
                return RAN(0);
            }

            if(lowertype == INF) {
                if(uppertype == CLOSED)
                    return upper;
                if(uppertype == OPEN)
                    return sampleBelow(upper);
            }
            
            if(uppertype == INF) {
                if(lowertype == CLOSED)
                    return lower;
                if(lowertype == OPEN) 
                    return sampleAbove(lower);
            }
            
            if(lower == upper)
                return lower;

            return sampleBetween(lower, upper);
        }
	};

inline std::ostream& operator<<(std::ostream& os, const CADInterval* i) {
	switch (i->getLowerBoundType()) {
		case CADInterval::CADBoundType::INF: os << "(-oo, "; break;
		case CADInterval::CADBoundType::CLOSED: os << "[" << i->getLower() << ", "; break;
		case CADInterval::CADBoundType::OPEN: os << "(" << i->getLower() << ", "; break;
	}
	switch (i->getUpperBoundType()) {
		case CADInterval::CADBoundType::INF: os << "oo)"; break;
		case CADInterval::CADBoundType::CLOSED: os << i->getUpper() << "]"; break;
		case CADInterval::CADBoundType::OPEN: os << i->getUpper() << ")"; break;
	}
	return os;
}

}   //cad
};  //smtrat
