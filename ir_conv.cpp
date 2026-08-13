#include "ir_conv.h"

float DirectConvolver::Process(float input)
{
    delay_[write_index_] = input;

    float  output = 0.0f;
    size_t read   = write_index_;

    for(size_t i = 0; i < kIrSize; i++)
    {
        output += ir[i] * delay_[read];
        read = read == 0 ? kIrSize - 1 : read - 1;
    }

    write_index_ = (write_index_ + 1) % kIrSize;

    return output;
}
