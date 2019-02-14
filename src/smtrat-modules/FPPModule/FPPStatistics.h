/**
 * @file FPPStatistics.h
 * @author YOUR NAME <YOUR EMAIL ADDRESS>
 *
 * @version 2015-09-10
 * Created on 2015-09-10.
 */


#pragma once

#include <smtrat-common/smtrat-common.h>
#ifdef SMTRAT_DEVOPTION_Statistics
#include <smtrat-common/statistics/Statistics.h>

namespace smtrat
{
    class FPPStatistics : public Statistics
    {
    private:
        // Members.
        std::size_t mInputVariables = 0;
		std::size_t mOutputVariables = 0;

    public:
        // Override Statistics::collect.
        void collect()
        {
           Statistics::addKeyValuePair( "input_variables", mInputVariables );
		   Statistics::addKeyValuePair( "output_variables", mOutputVariables );
        }

        void setInput(std::size_t variables) {
            mInputVariables = variables;
        }
		void setOutput(std::size_t variables) {
            mOutputVariables = variables;
        }
    };
}

#endif
