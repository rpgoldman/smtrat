/**
 * @file NewCoveringStatistics.h
 * @author Philip Kroll <Philip.Kroll@rwth-aachen.de>
 *
 * @version 21.01.2022
 * Created on 21.01.2022
 */

#pragma once

#include <chrono>
#include <smtrat-common/smtrat-common.h>
#ifdef SMTRAT_DEVOPTION_Statistics
#include <smtrat-common/statistics/Statistics.h>

namespace smtrat {
class NewCoveringStatistics : public Statistics {
private:
	std::size_t mTotalCalls = 0;
	std::size_t mIncrementalOnlyCalls = 0;
	std::size_t mBacktrackingOnlyCalls = 0;
	std::size_t mIncrementalAndBacktrackingCalls = 0;
	std::size_t mDimension = 0;
	std::size_t mCalledComputeCovering = 0;
	std::size_t mCalledConstructDerivation = 0;
	carl::statistics::timer mTimerComputeCovering;
	carl::statistics::timer mTimerConstructDerivation;

public:
	void collect() {
		Statistics::addKeyValuePair("total_calls", mTotalCalls);
		Statistics::addKeyValuePair("incremental_only_calls", mIncrementalOnlyCalls);
		Statistics::addKeyValuePair("backtracking_only_calls", mBacktrackingOnlyCalls);
		Statistics::addKeyValuePair("incremental_and_backtracking_calls", mIncrementalAndBacktrackingCalls);
		Statistics::addKeyValuePair("dimension", mDimension);
		Statistics::addKeyValuePair("time_compute_covering", mTimerComputeCovering);
		Statistics::addKeyValuePair("time_construct_derivation", mTimerConstructDerivation);
	}
	void called() {
		mTotalCalls++;
	}

	void calledIncrementalOnly() {
		mIncrementalOnlyCalls++;
	}
	void calledBacktrackingOnly() {
		mBacktrackingOnlyCalls++;
	}
	void calledIncrementalAndBacktracking() {
		mIncrementalAndBacktrackingCalls++;
	}

	void setDimension(std::size_t dimension) {
		mDimension = dimension;
	}

	auto& timeForComputeCovering() {
		return mTimerComputeCovering;
	}

	auto& timeForConstructDerivation() {
		return mTimerConstructDerivation;
	}
};

static auto& getStatistics() {
	SMTRAT_STATISTICS_INIT_STATIC(NewCoveringStatistics, stats, "NewCoveringModule");
	return stats;
}

} // namespace smtrat

#endif
