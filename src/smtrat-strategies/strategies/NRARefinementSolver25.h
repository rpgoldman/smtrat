#pragma once

#include <smtrat-solver/Manager.h>
#include <smtrat-modules/SATModule/SATModule.h>
#include <smtrat-modules/LRAModule/LRAModule.h>
#include <smtrat-modules/NRAILModule/NRAILModule.h>

namespace smtrat
{
    /**
     * Strategy description.
     *
     * @author
     * @since
     * @version
     *
     */
    class NRARefinementSolver25:
            public Manager
    {
    public:
        NRARefinementSolver25(): Manager()
        {
            setStrategy(
                    {
                            addBackend<NRAILModule<NRAILSettings25>>(
                                    {
                                        addBackend<SATModule<SATSettings1>>(
                                                {
                                                    addBackend<LRAModule<LRASettings1>>()
                                                })
                                    })
                    });
        }
    };
} // namespace smtrat
