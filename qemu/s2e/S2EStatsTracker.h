#ifndef S2E_STATSTRACKER_H
#define S2E_STATSTRACKER_H

#include <klee/Statistic.h>
#include <klee/StatsTracker.h>

namespace klee {
namespace stats {
    extern klee::Statistic translationBlocks;
    extern klee::Statistic translationBlocksConcrete;
    extern klee::Statistic translationBlocksKlee;

    extern klee::Statistic cpuInstructions;
    extern klee::Statistic cpuInstructionsConcrete;
    extern klee::Statistic cpuInstructionsKlee;

    extern klee::Statistic concreteModeTime;
    extern klee::Statistic symbolicModeTime;
} // namespace stats
} // namespace klee

namespace s2e {

class S2EStatsTracker: public klee::StatsTracker
{
public:
    S2EStatsTracker(klee::Executor &_executor, std::string _objectFilename,
                    bool _updateMinDistToUncovered)
        : StatsTracker(_executor, _objectFilename, _updateMinDistToUncovered) {}

    static uint64_t getProcessMemoryUsage();
protected:
    void writeStatsHeader();
    void writeStatsLine();
};

} // namespace s2e

#endif // S2ESTATSTRACKER_H