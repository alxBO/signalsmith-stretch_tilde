#include <gtest/gtest.h>
#include "deinterleave.hpp" // Include your external's header

#include <tuple>

void getData(int num_elements, int num_channels, int test_channel, std::vector<float>& input, std::vector<float>& expected){
    for(int i = 0; i < num_channels * num_elements; i++){
        input.push_back(i);
        if((i%num_channels)==test_channel)
            expected.push_back(i);
    }
}

void doTest(int num_elements, int num_channels, int test_channel){
    std::vector<float> input;
    std::vector<float> expected;

    getData(num_elements, num_channels, test_channel, input, expected);

    std::vector<std::vector<REAL>> output;
    deinterleave(input.data(), output, num_elements, num_channels);
    EXPECT_EQ(output[test_channel], expected);
}

TEST(TestSignalsmithStretch, Error0) {
    std::vector<float> data {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8};
    std::vector<std::vector<REAL>> output;
    deinterleave(data.data(), output, 8, 0);
    EXPECT_EQ(output.size(), 0) << getCurrentSIMD();
}

// --- Mono


TEST(TestSignalsmithStretch, TestMono_0) {
    doTest(8, 1, 0);
}

TEST(TestSignalsmithStretch, TestMono_1) {
    doTest(127, 1, 0);
}

TEST(TestSignalsmithStretch, TestMono_2) {
    doTest(1077, 1, 0);
}

TEST(TestSignalsmithStretch, TestMono_3) {
    doTest(64, 1, 0);
}

TEST(TestSignalsmithStretch, TestMono_4)
{
    doTest(1, 1, 0);
}

// --- Stereo

TEST(TestSignalsmithStretch, Test2channels_0) {
    doTest(8, 2, 0);
}

TEST(TestSignalsmithStretch, Test2channels_1) {
    doTest(48, 2, 1);
}

TEST(TestSignalsmithStretch, Test2channels_2) {
    doTest(57, 2, 0);
}

TEST(TestSignalsmithStretch, Test2channels_3) {
    doTest(73, 2, 1);
}

TEST(TestSignalsmithStretch, Test2channels_4)
{
    doTest(2, 2, 1);
}

// ----- 3 channels

TEST(TestSignalsmithStretch, Test3channels_0)
{
    doTest(8, 3, 0);
}


TEST(TestSignalsmithStretch, Test3channels_2)
{
    doTest(16, 3, 2);
}


TEST(TestSignalsmithStretch, Test3channels_3)
{
    doTest(57, 3, 1);
}

TEST(TestSignalsmithStretch, Test3channels_4)
{
    doTest(3, 3, 1);
}

// ----- 4 channels

TEST(TestSignalsmithStretch, Test4channels_0)
{
    doTest(57, 4, 0);
}

TEST(TestSignalsmithStretch, Test4channels_1)
{
    doTest(32, 4, 1);
}

TEST(TestSignalsmithStretch, Test4channels_2)
{
    doTest(23, 4, 2);
}

TEST(TestSignalsmithStretch, Test4channels_3)
{
    doTest(126, 4, 3);
}

TEST(TestSignalsmithStretch, Test4channels_4)
{
    doTest(4, 4, 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
