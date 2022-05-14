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
    EXPECT_NE(prop.bitmask(0x02), true);   // bit 2 not set
}

TEST(PropertyValue, Bitmask64Bits)
{
    int32_t value = 0x100001; // bit 0, 20
    PropertyVariant variant = value;
    PropertyValue prop{variant};
    EXPECT_EQ(prop.getInt64(), 0x100001);
    EXPECT_EQ(prop.bitmask(0x01), true);     // bit 0
    EXPECT_EQ(prop.bitmask(0x100000), true); // bit 20
    EXPECT_EQ(prop.bitmask(0x02), false);    // bit 2 not set
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

    PropertyVariant variantBit2 = 0x2;
    PropertyValue propBit2{variantBit2};
    EXPECT_EQ(prop.bitmask(propBit2), false);
}

TEST(PropertyValue, CheckNegativeBitmaskEmptyPropertyValue)
{
    CheckDefinitionMap accessorCHECK = {{"bitmask", "0x01"}};
    PropertyValue propertyValueFail(std::string{""});
    EXPECT_NE(propertyValueFail.check(accessorCHECK), true);
}

TEST(PropertyValue, CheckPositiveBitmaskPropertyValue)
{
    CheckDefinitionMap accessorCHECK = {{"bitmask", "0x01"}};
    PropertyValue propertyValuePass(std::string{"0x03"});
    EXPECT_EQ(propertyValuePass.check(accessorCHECK), true);
}

TEST(PropertyValue, CheckNegativePropertyValue)
{
    const CheckDefinitionMap accessorCHECK = {{"bitmask", "0x01"}};
    PropertyValue propertyValueFail{std::string{"2"}};
    EXPECT_NE(propertyValueFail.check(accessorCHECK), true);
}

TEST(PropertyValue, CheckPositiveLookupString)
{
    CheckDefinitionMap accessorCHECK = {{"lookup", "0x40"}};
    PropertyValue propertyValue{std::string{"0x30 0x40 0x50"}};
    EXPECT_EQ(propertyValue.check(accessorCHECK), true);
}

TEST(PropertyValue, CheckPositiveBitmaskPropertyValueRedefinition)
{
    CheckDefinitionMap accessorCHECK = {{"bitmask", "0x01"}};
    PropertyValue propertyValuePass(std::string{"0x02"});

    PropertyVariant maskRedefinition(int64_t(0x02));
    EXPECT_EQ(propertyValuePass.check(accessorCHECK, maskRedefinition), true);
}

TEST(PropertyValue, CheckNegativeBitmaskPropertyValueRedefinition)
{
    CheckDefinitionMap accessorCHECK = {{"bitmask", "0x01"}};
    PropertyValue propertyValueFail(std::string{"0x03"});

    PropertyVariant maskRedefinition(int64_t(0x04));
    EXPECT_NE(propertyValueFail.check(accessorCHECK, maskRedefinition), true);
}
