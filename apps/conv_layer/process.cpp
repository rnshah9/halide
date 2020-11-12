#include <chrono>
#include <cstdio>

#include "conv_layer.h"
#include "conv_layer_auto_schedule.h"
#include "conv_layer_c.h"

#include "HalideBuffer.h"
#include "halide_benchmark.h"

using namespace Halide::Tools;
using namespace Halide::Runtime;

int main(int argc, char **argv) {
    const int N = 1, CI = 128, CO = 128, W = 25, H = 20;

    Buffer<float> input(CI, W + 2, H + 2, N);
    Buffer<float> filter(CO, 3, 3, CI);
    Buffer<float> bias(CO);

    for (int c = 0; c < input.dim(3).extent(); c++) {
        for (int z = 0; z < input.channels(); z++) {
            for (int y = 0; y < input.height(); y++) {
                for (int x = 0; x < input.width(); x++) {
                    input(x, y, z, c) = (rand() % 256) / 255.0f;
                }
            }
        }
    }

    for (int c = 0; c < filter.dim(3).extent(); c++) {
        for (int z = 0; z < filter.channels(); z++) {
            for (int y = 0; y < filter.height(); y++) {
                for (int x = 0; x < filter.width(); x++) {
                    filter(x, y, z, c) = (rand() % 256) / 255.0f;
                }
            }
        }
    }

    for (int x = 0; x < bias.width(); x++) {
        bias(x) = (rand() % 256) / 255.0f;
    }

    Buffer<float> output(CO, W, H, N);

// This is necessary to get the PTX compiler to do a good
// job. TODO: This should be a scheduling directive or a runtime
// function.
#ifdef _WIN32
    _putenv_s("HL_CUDA_JIT_MAX_REGISTERS", "256");
#else
    setenv("HL_CUDA_JIT_MAX_REGISTERS", "256", 1);
#endif

    conv_layer(input, filter, bias, output);

    // Timing code

    // Manually-tuned version
    double min_t_manual = benchmark(10, 10, [&]() {
        conv_layer(input, filter, bias, output);
        output.device_sync();
    });
    printf("Manually-tuned time: %gms\n", min_t_manual * 1e3);

    // Auto-scheduled version
    double min_t_auto = benchmark(10, 10, [&]() {
        conv_layer_auto_schedule(input, filter, bias, output);
        output.device_sync();
    });
    printf("Auto-scheduled time: %gms\n", min_t_auto * 1e3);

    printf("Running generated C++ code...\n");
    Buffer<float> output_c(CO, W, H, N);
    conv_layer_c(input, filter, bias, output_c);

    int mismatch_count = 0;
    for (int c = 0; c < output_c.dim(3).extent(); c++) {
        for (int z = 0; z < output_c.channels(); z++) {
            for (int y = 0; y < output_c.height(); y++) {
                for (int x = 0; x < output_c.width(); x++) {
                    if (abs(output(x, y, z, c) - output_c(x, y, z, c)) > 0.0001) {
                        mismatch_count++;
                    }
                }
            }
        }
    }
    printf("Mismtach count for generated C++ code: %d\n", mismatch_count);
    printf("Success!\n");
    return 0;
}
