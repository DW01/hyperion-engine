#ifndef GLIMMER_SPANS_HLSLI
#define GLIMMER_SPANS_HLSLI

#include "GlimmerCommon.hlsli"

struct GlimmerSpanSample
{
    float solidMin;
    float solidMax;
    uint solidBins;
    uint4 solidBands;
    float3 solidAlbedo;

    float canopyMin;
    float canopyMax;
    uint canopyBins;
    uint4 canopyBands;
    float3 canopyAlbedo;
};

bool GlimmerSampleSpans(GlimmerSpanParams params, uint level, float2 worldXZ, out GlimmerSpanSample outSample)
{
    outSample = (GlimmerSpanSample)0;

    const GlimmerSpanLevel spanLevel = params.levels[level];

    if (spanLevel.window.z == 0)
    {
        return false;
    }

    const int2 texel = int2(floor(worldXZ * spanLevel.params.y));

    if (any(texel < spanLevel.window.xy) || any(texel >= spanLevel.window.xy + GLIMMER_GROUND_RESOLUTION))
    {
        return false;
    }

    const uint baseIndex = GlimmerSpanTexelIndex(level, texel);

    const uint solidMin = glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_MIN];
    const uint solidMax = glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_MAX];
    const uint canopyMin = glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_MIN];
    const uint canopyMax = glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_MAX];

    outSample.solidMin = 1e30;
    outSample.solidMax = -1e30;
    outSample.canopyMin = 1e30;
    outSample.canopyMax = -1e30;

    if (solidMin <= solidMax)
    {
        const uint solidAreaFixed = glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_AREA];

        outSample.solidMin = GlimmerFloatFromOrderedUint(solidMin);
        outSample.solidMax = GlimmerFloatFromOrderedUint(solidMax);
        
        outSample.solidBins = glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_BINS];
        
        outSample.solidBands = uint4(
            glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_BANDS + 0],
            glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_BANDS + 1],
            glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_BANDS + 2],
            glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_BANDS + 3]);
        
        outSample.solidAlbedo = float3(
            glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_ALBEDO + 0],
            glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_ALBEDO + 1],
            glimmerSpans[baseIndex + GLIMMER_SPAN_SOLID_ALBEDO + 2]) / float(max(solidAreaFixed, 1u));
    }

    if (canopyMin <= canopyMax)
    {
        const uint leafAreaFixed = glimmerSpans[baseIndex + GLIMMER_SPAN_LEAF_AREA];

        outSample.canopyMin = GlimmerFloatFromOrderedUint(canopyMin);
        outSample.canopyMax = GlimmerFloatFromOrderedUint(canopyMax);
        
        outSample.canopyBins = glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_BINS];
        
        outSample.canopyBands = uint4(
            glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_BANDS + 0],
            glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_BANDS + 1],
            glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_BANDS + 2],
            glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_BANDS + 3]);
        
        outSample.canopyAlbedo = float3(
            glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_ALBEDO + 0],
            glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_ALBEDO + 1],
            glimmerSpans[baseIndex + GLIMMER_SPAN_CANOPY_ALBEDO + 2]) / float(max(leafAreaFixed, 1u));
    }

    return true;
}

#endif
