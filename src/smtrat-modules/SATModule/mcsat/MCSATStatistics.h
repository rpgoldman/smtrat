#pragma once

#include <smtrat-common/statistics/Statistics.h>

#ifdef SMTRAT_DEVOPTION_Statistics

namespace smtrat {
namespace mcsat {

class MCSATStatistics: public Statistics {
private:
	std::size_t mInsertedLazyExplanation = 0;
	std::size_t mUsedLazyExplanation = 0;
	std::size_t mModelAssignmentCacheHit = 0;
public:
	bool enabled() const {
		return
			(mInsertedLazyExplanation > 0) ||
			(mUsedLazyExplanation > 0) ||
			(mModelAssignmentCacheHit > 0)
		;
	}
	void collect() {
		Statistics::addKeyValuePair("insertedLazyExplanation", mInsertedLazyExplanation);
		Statistics::addKeyValuePair("usedLazyExplanation", mUsedLazyExplanation);
		Statistics::addKeyValuePair("modelAssignmentCacheHit", mModelAssignmentCacheHit);
	}
	
	void insertedLazyExplanation() {
		++mInsertedLazyExplanation;
	}

	void usedLazyExplanation() {
		++mUsedLazyExplanation;
	}

	void modelAssignmentCacheHit() {
		++mModelAssignmentCacheHit;
	}
	
};

}
}

#endif
