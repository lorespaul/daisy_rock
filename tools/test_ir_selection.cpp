#include "../src/convolution/ir_conv.h"
#include <cassert>

int main()
{
    DirectConvolver convolver;
    convolver.Init();
    convolver.SetIr(kIrCount + 1);
    assert(convolver.NextIr() == (kIrCount > 1 ? 1u : 0u));
    convolver.SetIr(kIrCount - 1);
    assert(convolver.NextIr() == 0);
    assert(convolver.Process(1.0f) == irs_qspi[0][0]);
}
