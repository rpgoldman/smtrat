#pragma once

#include <vector>

#include "../Common.h"

#include "ProjectionOperator.h"
#include "PolynomialLiftingQueue.h"

namespace smtrat {
namespace cad {
	
	class BaseProjection {
	public:
		using PolynomialSelection = Bitset;
	protected:
		/// List of variables.
		Variables mVariables;
		/// Lift of id pools to generate fresh IDs for polynomials.
		std::vector<IDPool> mIDPools;
		/// List of lifting queues that can be used for incremental projection.
		std::vector<PolynomialLiftingQueue<BaseProjection>> mLiftingQueues;
		/// The projection operator.
		ProjectionOperator mOperator;
		
		/// Returns the dimension of the projection.
		std::size_t dim() const {
			assert(mVariables.size() == mIDPools.size());
			assert(mVariables.size() == mLiftingQueues.size());
			return mVariables.size();
		}
		/// Returns a fresh polynomial id for the given level.
		std::size_t getID(std::size_t level) {
			assert(level < dim());
			return mIDPools[level].get();
		}
		/// Frees a currently used polynomial id for the given level.
		void freeID(std::size_t level, std::size_t id) {
			assert(level < dim());
			mIDPools[level].free(id);
		}
		/// Returns the variable that corresponds to the given level, that is the variable eliminated in this level.
		carl::Variable var(std::size_t level) const {
			assert(level < dim());
			return mVariables[level];
		}
		/// Checks whether a polynomial can safely be ignored.
		bool canBePurged(const UPoly& p) const {
			return p.isZero() || p.isNumber();
		}
		/// Checks whether a polynomial can safely be forwarded to the next level.
		bool canBeForwarded(std::size_t level, const UPoly& p) const {
			return p.isConstant();
		}
	public:
		/// Returns the variables used for projection.
		const Variables& vars() const {
			return mVariables;
		}
		/// Resets all datastructures, use the given variables from now on.
		void reset(const Variables& vars) {
			mVariables = vars;
			mIDPools = std::vector<IDPool>(vars.size());
			mLiftingQueues.clear();
			for (std::size_t i = 0; i < vars.size(); i++) {
				mLiftingQueues.emplace_back(this, i);
			}
		}
		/// Adds the given polynomial to the projection. Converts to a UPoly and calls the appropriate overload.
		void addPolynomial(const Poly& p, std::size_t cid) {
			addPolynomial(p.toUnivariatePolynomial(var(0)), cid);
		}
		/// Adds the given polynomial to the projection.
		virtual void addPolynomial(const UPoly& p, std::size_t cid) = 0;
		/// Removes the given polynomial from the projection. Converts to a UPoly and calls the appropriate overload.
		void removePolynomial(const Poly& p, std::size_t cid, const std::function<void(std::size_t,SampleLiftedWith)>& callback) {
			removePolynomial(p.toUnivariatePolynomial(var(0)), cid, callback);
		}
		/// Removes the given polynomial from the projection.
		virtual void removePolynomial(const UPoly& p, std::size_t cid, const std::function<void(std::size_t,SampleLiftedWith)>& callback) = 0;
		
		/// Cleans up the given list of polynomial ids a sample was lifted with from polynomials that got removed in the meantime.
		void cleanLiftedWith(std::size_t level, SampleLiftedWith& slw) const {
			mIDPools[level].purgeUnusedIDs(slw);
		}
		
		/// Retrieves a polynomial from its id.
		virtual const UPoly& getPolynomialById(std::size_t level, std::size_t id) const = 0;
	};
	
}
}