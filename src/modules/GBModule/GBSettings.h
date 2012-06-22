/* 
 * File:   GBSettings.h
 * Author: square
 *
 * Created on June 18, 2012, 7:50 PM
 */

#include <ginacra/MultivariatePolynomialMR.h>

#ifndef GBSETTINGS_H
#define	GBSETTINGS_H

namespace smtrat {
	/**
	 * Only active if we check inequalities.
	 * AS_RECEIVED: Do not change the received inequalities.
	 * FULL_REDUCED: Pass the fully reduced received inequalities.
	 * REDUCED: Pass the reduced received inequalities.
	 * REDUCED_ONLYSTRICT: Pass the nonstrict inequalities reduced, the others unchanged
	 * FULL_REDUCED_ONLYNEW: Do only a full reduce on the newly added received inequalities.
	 */
	enum pass_inequalities { AS_RECEIVED, FULL_REDUCED, REDUCED, REDUCED_ONLYSTRICT, FULL_REDUCED_ONLYNEW };
	
	
	struct GBSettings {
		typedef GiNaCRA::GradedLexicgraphic Order;
		typedef GiNaCRA::MultivariatePolynomialMR<Order> Polynomial;
		typedef GiNaCRA::MultivariateIdeal<Order> MultivariateIdeal;
		typedef GiNaCRA::BaseReductor<Order> Reductor;
		
		static const bool passGB = true;
		static const bool getReasonsForInfeasibility = true;
		static const bool passWithMinimalReasons = true;
		static const bool checkInequalities = true;
		static const pass_inequalities passInequalities = AS_RECEIVED;
		static const bool checkInequalitiesForTrivialSumOfSquares = false;
		static const bool checkEqualitiesForTrivialSumOfSquares = false;

		static const unsigned maxSDPdegree = 12;
		static const unsigned SDPupperBoundNrVariables = 6;
	};
}

#endif	/* GBSETTINGS_H */

