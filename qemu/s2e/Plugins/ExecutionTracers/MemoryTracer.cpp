#include <iomanip>

#include <s2e/S2E.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/S2EExecutor.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include "MemoryTracer.h"

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(MemoryTracer, "Memory tracer plugin", "MemoryTracer", "ExecutionTracer");

MemoryTracer::MemoryTracer(S2E* s2e)
        : Plugin(s2e)
{

}

void MemoryTracer::initialize()
{

    m_tracer = (ExecutionTracer*)s2e()->getPlugin("ExecutionTracer");
    assert(m_tracer);

    //Catch all accesses to the stack
    m_monitorStack = s2e()->getConfig()->getBool(getConfigKey() + ".monitorStack");

    //Catch accesses that are above the specified address
    m_catchAbove = s2e()->getConfig()->getInt(getConfigKey() + ".catchAccessesAbove");

    //Start monitoring after the specified number of seconds
    m_timeTrigger = s2e()->getConfig()->getInt(getConfigKey() + ".timeTrigger");
    m_elapsedTics = 0;

    m_monitorMemory = s2e()->getConfig()->getBool(getConfigKey() + ".monitorMemory");
    m_monitorPageFaults = s2e()->getConfig()->getBool(getConfigKey() + ".monitorPageFaults");
    m_monitorTlbMisses  = s2e()->getConfig()->getBool(getConfigKey() + ".monitorTlbMisses");

    s2e()->getDebugStream() << "MonitorMemory: " << m_monitorMemory << 
    " PageFaults: " << m_monitorPageFaults << " TlbMisses: " << m_monitorTlbMisses << std::endl;

    if (!m_timeTrigger) {
        enableTracing();
    }else {
        m_timerConnection = s2e()->getCorePlugin()->onTimer.connect(
                sigc::mem_fun(*this, &MemoryTracer::onTimer));
    }
}

bool MemoryTracer::decideTracing(S2EExecutionState *state, uint64_t addr, uint64_t data) const
{
    if (addr < m_catchAbove) {
        //Skip uninteresting ranges
        return false;
    }

    if (m_monitorStack) {
        //Assume that the stack is 8k and 8k-aligned
        if ((state->getSp() & ~0x3FFFF) == (addr & ~0x3FFFF)) {
            return true;
        }
        return false;
    }

    return true;
}

void MemoryTracer::onDataMemoryAccess(S2EExecutionState *state,
                               klee::ref<klee::Expr> address,
                               klee::ref<klee::Expr> hostAddress,
                               klee::ref<klee::Expr> value,
                               bool isWrite, bool isIO)
{

    if(!isa<klee::ConstantExpr>(address) || !isa<klee::ConstantExpr>(value)) {
        //We do not support symbolic values yet...
        return;
    }

    uint64_t addr = cast<klee::ConstantExpr>(address)->getZExtValue(64);
    uint64_t val = cast<klee::ConstantExpr>(value)->getZExtValue(64);

    if (decideTracing(state, addr, val)) {
       //Output to the trace entry here
       ExecutionTraceMemory e;
       e.pc = state->getPc();
       e.address = addr;
       e.value = val;
       e.size = klee::Expr::getMinBytesForWidth(value->getWidth());
       e.flags = isWrite*EXECTRACE_MEM_WRITE |
                 isIO*EXECTRACE_MEM_IO;

       m_tracer->writeData(state, &e, sizeof(e), TRACE_MEMORY);
    }
}

void MemoryTracer::onTlbMiss(S2EExecutionState *state, uint64_t addr, bool is_write)
{
    ExecutionTraceTlbMiss e;
    e.pc = state->getPc();
    e.address = addr;
    e.isWrite = is_write;

    m_tracer->writeData(state, &e, sizeof(e), TRACE_TLBMISS);
}

void MemoryTracer::onPageFault(S2EExecutionState *state, uint64_t addr, bool is_write)
{
    ExecutionTracePageFault e;
    e.pc = state->getPc();
    e.address = addr;
    e.isWrite = is_write;

    m_tracer->writeData(state, &e, sizeof(e), TRACE_PAGEFAULT);
}

void MemoryTracer::enableTracing()
{
    if (m_monitorMemory) {
        s2e()->getMessagesStream() << "MemoryTracer Plugin: Enabling memory tracing" << std::endl;
        s2e()->getCorePlugin()->onDataMemoryAccess.connect(
                sigc::mem_fun(*this, &MemoryTracer::onDataMemoryAccess));
    }

    if (m_monitorPageFaults) {
        s2e()->getMessagesStream() << "MemoryTracer Plugin: Enabling page fault tracing" << std::endl;
        s2e()->getCorePlugin()->onPageFault.connect(
                sigc::mem_fun(*this, &MemoryTracer::onPageFault));
    }

    if (m_monitorTlbMisses) {
        s2e()->getMessagesStream() << "MemoryTracer Plugin: Enabling TLB miss tracing" << std::endl;
        s2e()->getCorePlugin()->onTlbMiss.connect(
                sigc::mem_fun(*this, &MemoryTracer::onTlbMiss));
    }
}

void MemoryTracer::onTimer()
{
    if (m_elapsedTics++ < m_timeTrigger) {
        return;
    }

    enableTracing();

    m_timerConnection.disconnect();
}

}
}