/**
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#pragma once
#include "json_schema.hpp"

#include <memory>

namespace oob_aml
{

constexpr auto SERVICE_BUSNAME = "xyz.openbmc_project.OOBAML";
constexpr auto TOP_OBJPATH = "/xyz/openbmc_project/OOBAML";
constexpr auto SERVICE_IFCNAME = "xyz.openbmc_project.OOBAML";

} // namespace oob_aml

namespace aml
{

std::shared_ptr<json_schema::JsonSchema> dataAccessorSchema();
std::shared_ptr<json_schema::JsonSchema> eventNodeJsonSchema();
std::shared_ptr<json_schema::JsonSchema> eventInfoJsonSchema();

std::shared_ptr<json_schema::JsonSchema> datSchema();

} // namespace aml
