#include "property_accessor.hpp"

#include <string>

#include <gtest/gtest.h>

using namespace data_accessor;

TEST(PropertyValue, VariantInteger)
{
    int value = 5;
    PropertyVariant variant = value;
    PropertyValue prop{variant};
    EXPECT_EQ(prop.getInt64(), 5);
    EXPECT_EQ(prop.isValid(), true);
}

TEST(PropertyValue, VariantHexa)
{
    int value = 0xff;
    PropertyVariant variant = value;
    PropertyValue prop{variant};
    EXPECT_EQ(prop.getInt64(), 255);
    EXPECT_EQ(prop.isValid(), true);
}

TEST(PropertyValue, VariantBoolean)
{
    bool boolean = false;
    PropertyVariant variant = boolean;
    PropertyValue prop{variant};
    EXPECT_EQ(prop.getInt64(), 0);
    EXPECT_EQ(prop.isValid(), true);
}

TEST(PropertyValue, VariantDouble)
{
    double value = 10.0;
    PropertyVariant variant = value;
    PropertyValue prop{variant};
    EXPECT_EQ(prop.getInt64(), 10);
    EXPECT_EQ(prop.isValid(), true);
}

TEST(PropertyValue, VariantFloat)
{
    float value = 10.0;
    PropertyVariant variant = value;
    PropertyValue prop{variant};
    EXPECT_EQ(prop.getInt64(), 10);
    EXPECT_EQ(prop.isValid(), true);
}

TEST(PropertyValue, Bitmask16Bits)
{
    int16_t value = 0xff; // bits 0-3 set
    PropertyVariant variant = value;
    PropertyValue prop{variant};
    EXPECT_EQ(prop.bitmask(0x07), true);
    EXPECT_EQ(prop.bitmask(0x03), true);
    EXPECT_EQ(prop.bitmask(0x01), true);
}

TEST(PropertyValue, Bitmask32Bits)
{
    int32_t value = 0x1001; // bit 0, 12
    PropertyVariant variant = value;
    PropertyValue prop{variant};
    EXPECT_EQ(prop.getInt64(), 0x1001);
    EXPECT_EQ(prop.bitmask(0x01), true);   // bit 0
    EXPECT_EQ(prop.bitmask(0x1000), true); // bit 12
    EXPECT_EQ(prop.bitmask(0x03), false);  // bits 1 and 2
}

TEST(PropertyValue, Bitmask64Bits)
{
    int32_t value = 0x100001; // bit 0, 20
    PropertyVariant variant = value;
    PropertyValue prop{variant};
    EXPECT_EQ(prop.getInt64(), 0x100001);
    EXPECT_EQ(prop.bitmask(0x01), true);     // bit 0
    EXPECT_EQ(prop.bitmask(0x100000), true); // bit 20
    EXPECT_EQ(prop.bitmask(0x03), false);    // bits 1 and 2
}

TEST(PropertyValue, Bitmask2Properties)
{
    int32_t value = 0x100001; // bit 0, 20
    PropertyVariant variant = value;
    PropertyValue prop{variant};
    EXPECT_EQ(prop.getInt64(), 0x100001);

    PropertyVariant variantBit0 = 0x01;
    PropertyValue propBit0{variantBit0};
    EXPECT_EQ(prop.bitmask(propBit0), true);

    PropertyVariant variantBit20 = 0x100000;
    PropertyValue propBit20{variantBit20};
    EXPECT_EQ(prop.bitmask(propBit20), true);

    PropertyVariant variantBit3 = 0x3;
    PropertyValue propBit3{variantBit3};
    EXPECT_EQ(prop.bitmask(propBit3), false);
}