#include "ir_conv.h"

void DirectConvolver::Init()
{
    LoadIrs();
    SetIr(0);
}

void DirectConvolver::SetIr(size_t index)
{
    ir_index_ = index < kIrCount ? index : 0;
    for(size_t i = 0; i < kIrSize; i++)
        delay_[i] = 0.0f;
    write_index_ = 0;
}

float DirectConvolver::Process(float input)
{
    delay_[write_index_] = input;

    float  output = 0.0f;
    size_t read   = write_index_;

    for(size_t i = 0; i < kIrSize; i++)
    {
        output += irs[ir_index_][i] * delay_[read];
        read = read == 0 ? kIrSize - 1 : read - 1;
    }

    write_index_ = (write_index_ + 1) % kIrSize;

    return output;
}
