#include "imsi.h"

#include <gtest/gtest.h>

TEST(IMSITest, ValidStringConversion) {
    PGW::IMSI imsi;
    ASSERT_TRUE(imsi.set_IMSI_from_str("123456789012345"));
    EXPECT_EQ(imsi.get_IMSI_to_str(), "123456789012345");
}

TEST(IMSITest, InvalidStringConversion) {
    PGW::IMSI imsi;
    EXPECT_FALSE(imsi.set_IMSI_from_str("invalid_imsi"));
    EXPECT_FALSE(imsi.set_IMSI_from_str("1234567890123456")); // слишком длинный
    EXPECT_FALSE(imsi.set_IMSI_from_str("")); // слишком короткий
}

TEST(IMSITest, IEConversion) {
    PGW::IMSI imsi;
    imsi.set_IMSI_from_str("1234567890");
    auto ie = imsi.get_IMSI_to_IE();
    
    EXPECT_EQ(ie[0], 0x01);
    EXPECT_EQ(ie[1], 0x00);
    EXPECT_EQ(ie[2], 0x05);
    EXPECT_EQ(ie[3], 0x00);
    
    EXPECT_EQ(ie[4], 0x21); 
    EXPECT_EQ(ie[5], 0x43); 
    EXPECT_EQ(ie[6], 0x65); 
    EXPECT_EQ(ie[7], 0x87); 
    EXPECT_EQ(ie[8], 0x09); 
}

TEST(IMSITest, InvalidIEConversion) {
    PGW::IMSI imsi;
    EXPECT_FALSE(imsi.set_IMSI_from_IE({0x00, 0x00, 0x00})); // Невалидный IE
    EXPECT_FALSE(imsi.set_IMSI_from_IE({0x01, 0x00, 0x05, 0x00})); // Неполный IE
}

TEST(IMSITest, EqualityOperator) {
    PGW::IMSI imsi1, imsi2;
    imsi1.set_IMSI_from_str("123456789");
    imsi2.set_IMSI_from_str("123456789");
    EXPECT_TRUE(imsi1 == imsi2);
    
    imsi2.set_IMSI_from_str("987654321");
    EXPECT_FALSE(imsi1 == imsi2);
}