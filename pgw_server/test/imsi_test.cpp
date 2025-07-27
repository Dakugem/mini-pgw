#include "imsi.h"

#include <gtest/gtest.h>

class IMSITest : public ::testing::Test {};

TEST_F(IMSITest, StringConversion) {
    PGW::IMSI imsi;
    ASSERT_TRUE(imsi.set_IMSI_from_str("123456789012345"));
    ASSERT_EQ(imsi.get_IMSI_to_str(), "123456789012345");
}

TEST_F(IMSITest, IEConversion) {
    PGW::IMSI imsi;
    imsi.set_IMSI_from_str("1234567890");
    auto ie = imsi.get_IMSI_to_IE();
    
    // Проверка структуры IE
    ASSERT_EQ(ie[0], 0x01);
    ASSERT_EQ(ie[3], 0x00);

    EXPECT_EQ(ie[1], 0x00);
    EXPECT_EQ(ie[2], 0x05);

    EXPECT_EQ(ie[4], 0x21);
    EXPECT_EQ(ie[5], 0x43);
    EXPECT_EQ(ie[6], 0x65);
    EXPECT_EQ(ie[7], 0x87);
    EXPECT_EQ(ie[8], 0x09);
}

TEST_F(IMSITest, InvalidInput) {
    PGW::IMSI imsi;
    ASSERT_FALSE(imsi.set_IMSI_from_str("invalid_imsi"));
    ASSERT_FALSE(imsi.set_IMSI_from_IE({0x00, 0x00, 0x00})); // Невалидный IE
}