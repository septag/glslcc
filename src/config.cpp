//
// Copyright 2018 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/glslcc#license-bsd-2-clause
//

#include "config.h"
#include <sstream>

const TBuiltInResource k_default_conf = {
/* .MaxLights = */ 32,
/* .MaxClipPlanes = */ 6,
/* .MaxTextureUnits = */ 32,
/* .MaxTextureCoords = */ 32,
/* .MaxVertexAttribs = */ 64,
/* .MaxVertexUniformComponents = */ 4096,
/* .MaxVaryingFloats = */ 64,
/* .MaxVertexTextureImageUnits = */ 32,
/* .MaxCombinedTextureImageUnits = */ 80,
/* .MaxTextureImageUnits = */ 32,
/* .MaxFragmentUniformComponents = */ 4096,
/* .MaxDrawBuffers = */ 32,
/* .MaxVertexUniformVectors = */ 128,
/* .MaxVaryingVectors = */ 8,
/* .MaxFragmentUniformVectors = */ 16,
/* .MaxVertexOutputVectors = */ 16,
/* .MaxFragmentInputVectors = */ 15,
/* .MinProgramTexelOffset = */ -8,
/* .MaxProgramTexelOffset = */ 7,
/* .MaxClipDistances = */ 8,
/* .MaxComputeWorkGroupCountX = */ 65535,
/* .MaxComputeWorkGroupCountY = */ 65535,
/* .MaxComputeWorkGroupCountZ = */ 65535,
/* .MaxComputeWorkGroupSizeX = */ 1024,
/* .MaxComputeWorkGroupSizeY = */ 1024,
/* .MaxComputeWorkGroupSizeZ = */ 64,
/* .MaxComputeUniformComponents = */ 1024,
/* .MaxComputeTextureImageUnits = */ 16,
/* .MaxComputeImageUniforms = */ 8,
/* .MaxComputeAtomicCounters = */ 8,
/* .MaxComputeAtomicCounterBuffers = */ 1,
/* .MaxVaryingComponents = */ 60,
/* .MaxVertexOutputComponents = */ 64,
/* .MaxGeometryInputComponents = */ 64,
/* .MaxGeometryOutputComponents = */ 128,
/* .MaxFragmentInputComponents = */ 128,
/* .MaxImageUnits = */ 8,
/* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
/* .MaxCombinedShaderOutputResources = */ 8,
/* .MaxImageSamples = */ 0,
/* .MaxVertexImageUniforms = */ 0,
/* .MaxTessControlImageUniforms = */ 0,
/* .MaxTessEvaluationImageUniforms = */ 0,
/* .MaxGeometryImageUniforms = */ 0,
/* .MaxFragmentImageUniforms = */ 8,
/* .MaxCombinedImageUniforms = */ 8,
/* .MaxGeometryTextureImageUnits = */ 16,
/* .MaxGeometryOutputVertices = */ 256,
/* .MaxGeometryTotalOutputComponents = */ 1024,
/* .MaxGeometryUniformComponents = */ 1024,
/* .MaxGeometryVaryingComponents = */ 64,
/* .MaxTessControlInputComponents = */ 128,
/* .MaxTessControlOutputComponents = */ 128,
/* .MaxTessControlTextureImageUnits = */ 16,
/* .MaxTessControlUniformComponents = */ 1024,
/* .MaxTessControlTotalOutputComponents = */ 4096,
/* .MaxTessEvaluationInputComponents = */ 128,
/* .MaxTessEvaluationOutputComponents = */ 128,
/* .MaxTessEvaluationTextureImageUnits = */ 16,
/* .MaxTessEvaluationUniformComponents = */ 1024,
/* .MaxTessPatchComponents = */ 120,
/* .MaxPatchVertices = */ 32,
/* .MaxTessGenLevel = */ 64,
/* .MaxViewports = */ 16,
/* .MaxVertexAtomicCounters = */ 0,
/* .MaxTessControlAtomicCounters = */ 0,
/* .MaxTessEvaluationAtomicCounters = */ 0,
/* .MaxGeometryAtomicCounters = */ 0,
/* .MaxFragmentAtomicCounters = */ 8,
/* .MaxCombinedAtomicCounters = */ 8,
/* .MaxAtomicCounterBindings = */ 1,
/* .MaxVertexAtomicCounterBuffers = */ 0,
/* .MaxTessControlAtomicCounterBuffers = */ 0,
/* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
/* .MaxGeometryAtomicCounterBuffers = */ 0,
/* .MaxFragmentAtomicCounterBuffers = */ 1,
/* .MaxCombinedAtomicCounterBuffers = */ 1,
/* .MaxAtomicCounterBufferSize = */ 16384,
/* .MaxTransformFeedbackBuffers = */ 4,
/* .MaxTransformFeedbackInterleavedComponents = */ 64,
/* .MaxCullDistances = */ 8,
/* .MaxCombinedClipAndCullDistances = */ 8,
/* .MaxSamples = */ 4,
#ifdef NV_EXTENSIONS
/* .maxMeshOutputVerticesNV = */ 256,
/* .maxMeshOutputPrimitivesNV = */ 512,
/* .maxMeshWorkGroupSizeX_NV = */ 32,
/* .maxMeshWorkGroupSizeY_NV = */ 1,
/* .maxMeshWorkGroupSizeZ_NV = */ 1,
/* .maxTaskWorkGroupSizeX_NV = */ 32,
/* .maxTaskWorkGroupSizeY_NV = */ 1,
/* .maxTaskWorkGroupSizeZ_NV = */ 1,
/* .maxMeshViewCountNV = */ 4,
#endif

/* .limits = */ {
    /* .nonInductiveForLoops = */ 1,
    /* .whileLoops = */ 1,
    /* .doWhileLoops = */ 1,
    /* .generalUniformIndexing = */ 1,
    /* .generalAttributeMatrixVectorIndexing = */ 1,
    /* .generalVaryingIndexing = */ 1,
    /* .generalSamplerIndexing = */ 1,
    /* .generalVariableIndexing = */ 1,
    /* .generalConstantMatrixVectorIndexing = */ 1,
}};

std::string get_default_conf_str()
{
    std::ostringstream ostream;
    const TBuiltInResource& conf = k_default_conf;
    ostream << "MaxLights "                                 << conf.maxLights << "\n"
            << "MaxClipPlanes "                             << conf.maxClipPlanes << "\n"
            << "MaxTextureUnits "                           << conf.maxTextureUnits << "\n"
            << "MaxTextureCoords "                          << conf.maxTextureCoords << "\n"
            << "MaxVertexAttribs "                          << conf.maxVertexAttribs << "\n"
            << "MaxVertexUniformComponents "                << conf.maxVertexUniformComponents << "\n"
            << "MaxVaryingFloats "                          << conf.maxVaryingFloats << "\n"
            << "MaxVertexTextureImageUnits "                << conf.maxVertexTextureImageUnits << "\n"
            << "MaxCombinedTextureImageUnits "              << conf.maxCombinedTextureImageUnits << "\n"
            << "MaxTextureImageUnits "                      << conf.maxTextureImageUnits << "\n"
            << "MaxFragmentUniformComponents "              << conf.maxFragmentUniformComponents << "\n"
            << "MaxDrawBuffers "                            << conf.maxDrawBuffers << "\n"
            << "MaxVertexUniformVectors "                   << conf.maxVertexUniformVectors << "\n"
            << "MaxVaryingVectors "                         << conf.maxVaryingVectors << "\n"
            << "MaxFragmentUniformVectors "                 << conf.maxFragmentUniformVectors << "\n"
            << "MaxVertexOutputVectors "                    << conf.maxVertexOutputVectors << "\n"
            << "MaxFragmentInputVectors "                   << conf.maxFragmentInputVectors << "\n"
            << "MinProgramTexelOffset "                     << conf.minProgramTexelOffset << "\n"
            << "MaxProgramTexelOffset "                     << conf.maxProgramTexelOffset << "\n"
            << "MaxClipDistances "                          << conf.maxClipDistances << "\n"
            << "MaxComputeWorkGroupCountX "                 << conf.maxComputeWorkGroupCountX << "\n"
            << "MaxComputeWorkGroupCountY "                 << conf.maxComputeWorkGroupCountY << "\n"
            << "MaxComputeWorkGroupCountZ "                 << conf.maxComputeWorkGroupCountZ << "\n"
            << "MaxComputeWorkGroupSizeX "                  << conf.maxComputeWorkGroupSizeX << "\n"
            << "MaxComputeWorkGroupSizeY "                  << conf.maxComputeWorkGroupSizeY << "\n"
            << "MaxComputeWorkGroupSizeZ "                  << conf.maxComputeWorkGroupSizeZ << "\n"
            << "MaxComputeUniformComponents "               << conf.maxComputeUniformComponents << "\n"
            << "MaxComputeTextureImageUnits "               << conf.maxComputeTextureImageUnits << "\n"
            << "MaxComputeImageUniforms "                   << conf.maxComputeImageUniforms << "\n"
            << "MaxComputeAtomicCounters "                  << conf.maxComputeAtomicCounters << "\n"
            << "MaxComputeAtomicCounterBuffers "            << conf.maxComputeAtomicCounterBuffers << "\n"
            << "MaxVaryingComponents "                      << conf.maxVaryingComponents << "\n"
            << "MaxVertexOutputComponents "                 << conf.maxVertexOutputComponents << "\n"
            << "MaxGeometryInputComponents "                << conf.maxGeometryInputComponents << "\n"
            << "MaxGeometryOutputComponents "               << conf.maxGeometryOutputComponents << "\n"
            << "MaxFragmentInputComponents "                << conf.maxFragmentInputComponents << "\n"
            << "MaxImageUnits "                             << conf.maxImageUnits << "\n"
            << "MaxCombinedImageUnitsAndFragmentOutputs "   << conf.maxCombinedImageUnitsAndFragmentOutputs << "\n"
            << "MaxCombinedShaderOutputResources "          << conf.maxCombinedShaderOutputResources << "\n"
            << "MaxImageSamples "                           << conf.maxImageSamples << "\n"
            << "MaxVertexImageUniforms "                    << conf.maxVertexImageUniforms << "\n"
            << "MaxTessControlImageUniforms "               << conf.maxTessControlImageUniforms << "\n"
            << "MaxTessEvaluationImageUniforms "            << conf.maxTessEvaluationImageUniforms << "\n"
            << "MaxGeometryImageUniforms "                  << conf.maxGeometryImageUniforms << "\n"
            << "MaxFragmentImageUniforms "                  << conf.maxFragmentImageUniforms << "\n"
            << "MaxCombinedImageUniforms "                  << conf.maxCombinedImageUniforms << "\n"
            << "MaxGeometryTextureImageUnits "              << conf.maxGeometryTextureImageUnits << "\n"
            << "MaxGeometryOutputVertices "                 << conf.maxGeometryOutputVertices << "\n"
            << "MaxGeometryTotalOutputComponents "          << conf.maxGeometryTotalOutputComponents << "\n"
            << "MaxGeometryUniformComponents "              << conf.maxGeometryUniformComponents << "\n"
            << "MaxGeometryVaryingComponents "              << conf.maxGeometryVaryingComponents << "\n"
            << "MaxTessControlInputComponents "             << conf.maxTessControlInputComponents << "\n"
            << "MaxTessControlOutputComponents "            << conf.maxTessControlOutputComponents << "\n"
            << "MaxTessControlTextureImageUnits "           << conf.maxTessControlTextureImageUnits << "\n"
            << "MaxTessControlUniformComponents "           << conf.maxTessControlUniformComponents << "\n"
            << "MaxTessControlTotalOutputComponents "       << conf.maxTessControlTotalOutputComponents << "\n"
            << "MaxTessEvaluationInputComponents "          << conf.maxTessEvaluationInputComponents << "\n"
            << "MaxTessEvaluationOutputComponents "         << conf.maxTessEvaluationOutputComponents << "\n"
            << "MaxTessEvaluationTextureImageUnits "        << conf.maxTessEvaluationTextureImageUnits << "\n"
            << "MaxTessEvaluationUniformComponents "        << conf.maxTessEvaluationUniformComponents << "\n"
            << "MaxTessPatchComponents "                    << conf.maxTessPatchComponents << "\n"
            << "MaxPatchVertices "                          << conf.maxPatchVertices << "\n"
            << "MaxTessGenLevel "                           << conf.maxTessGenLevel << "\n"
            << "MaxViewports "                              << conf.maxViewports << "\n"
            << "MaxVertexAtomicCounters "                   << conf.maxVertexAtomicCounters << "\n"
            << "MaxTessControlAtomicCounters "              << conf.maxTessControlAtomicCounters << "\n"
            << "MaxTessEvaluationAtomicCounters "           << conf.maxTessEvaluationAtomicCounters << "\n"
            << "MaxGeometryAtomicCounters "                 << conf.maxGeometryAtomicCounters << "\n"
            << "MaxFragmentAtomicCounters "                 << conf.maxFragmentAtomicCounters << "\n"
            << "MaxCombinedAtomicCounters "                 << conf.maxCombinedAtomicCounters << "\n"
            << "MaxAtomicCounterBindings "                  << conf.maxAtomicCounterBindings << "\n"
            << "MaxVertexAtomicCounterBuffers "             << conf.maxVertexAtomicCounterBuffers << "\n"
            << "MaxTessControlAtomicCounterBuffers "        << conf.maxTessControlAtomicCounterBuffers << "\n"
            << "MaxTessEvaluationAtomicCounterBuffers "     << conf.maxTessEvaluationAtomicCounterBuffers << "\n"
            << "MaxGeometryAtomicCounterBuffers "           << conf.maxGeometryAtomicCounterBuffers << "\n"
            << "MaxFragmentAtomicCounterBuffers "           << conf.maxFragmentAtomicCounterBuffers << "\n"
            << "MaxCombinedAtomicCounterBuffers "           << conf.maxCombinedAtomicCounterBuffers << "\n"
            << "MaxAtomicCounterBufferSize "                << conf.maxAtomicCounterBufferSize << "\n"
            << "MaxTransformFeedbackBuffers "               << conf.maxTransformFeedbackBuffers << "\n"
            << "MaxTransformFeedbackInterleavedComponents " << conf.maxTransformFeedbackInterleavedComponents << "\n"
            << "MaxCullDistances "                          << conf.maxCullDistances << "\n"
            << "MaxCombinedClipAndCullDistances "           << conf.maxCombinedClipAndCullDistances << "\n"
            << "MaxSamples "                                << conf.maxSamples << "\n"
#ifdef NV_EXTENSIONS
            << "MaxMeshOutputVerticesNV "                   << conf.maxMeshOutputVerticesNV << "\n"
            << "MaxMeshOutputPrimitivesNV "                 << conf.maxMeshOutputPrimitivesNV << "\n"
            << "MaxMeshWorkGroupSizeX_NV "                  << conf.maxMeshWorkGroupSizeX_NV << "\n"
            << "MaxMeshWorkGroupSizeY_NV "                  << conf.maxMeshWorkGroupSizeY_NV << "\n"
            << "MaxMeshWorkGroupSizeZ_NV "                  << conf.maxMeshWorkGroupSizeZ_NV << "\n"
            << "MaxTaskWorkGroupSizeX_NV "                  << conf.maxTaskWorkGroupSizeX_NV << "\n"
            << "MaxTaskWorkGroupSizeY_NV "                  << conf.maxTaskWorkGroupSizeY_NV << "\n"
            << "MaxTaskWorkGroupSizeZ_NV "                  << conf.maxTaskWorkGroupSizeZ_NV << "\n"
            << "MaxMeshViewCountNV "                        << conf.maxMeshViewCountNV << "\n"
#endif
            << "nonInductiveForLoops "                      << conf.limits.nonInductiveForLoops << "\n"
            << "whileLoops "                                << conf.limits.whileLoops << "\n"
            << "doWhileLoops "                              << conf.limits.doWhileLoops << "\n"
            << "generalUniformIndexing "                    << conf.limits.generalUniformIndexing << "\n"
            << "generalAttributeMatrixVectorIndexing "      << conf.limits.generalAttributeMatrixVectorIndexing << "\n"
            << "generalVaryingIndexing "                    << conf.limits.generalVaryingIndexing << "\n"
            << "generalSamplerIndexing "                    << conf.limits.generalSamplerIndexing << "\n"
            << "generalVariableIndexing "                   << conf.limits.generalVariableIndexing << "\n"
            << "generalConstantMatrixVectorIndexing "       << conf.limits.generalConstantMatrixVectorIndexing << "\n"
      ;

    return ostream.str();
}
