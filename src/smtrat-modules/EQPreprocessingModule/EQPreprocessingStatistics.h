/**
 * @file EQPreprocessingStatistics.h
 * @author YOUR NAME <YOUR EMAIL ADDRESS>
 *
 * @version 2014-12-05
 * Created on 2014-12-05.
 */


#pragma once

#include <smtrat-common/smtrat-common.h>
#ifdef SMTRAT_DEVOPTION_Statistics
#include <smtrat-common/statistics/Statistics.h>

namespace smtrat {
	class EQPreprocessingStatistics : public Statistics {
		private:
			// Members.
			/**
			 * Number of congruences explicitly added to the formula during UF => EQ rewriting.
			 */
			size_t mCongruencesAdded = 0;

		public:
			// Override Statistics::collect.
			void collect() {
				Statistics::addKeyValuePair("congruences_added", mCongruencesAdded);
			}

			void countCongruencesAdded(std::size_t congruences) {
				mCongruencesAdded += congruences;
			}
	};
}

#endif
