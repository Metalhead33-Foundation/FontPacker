/**
 * @file sdf_opencl.cl
 * @brief OpenCL 1.2 kernels for bitmap SDF, outline SDF and outline MSDF/MSDFA.
 *
 * Buffer-based port of the GLSL compute shaders. The host compiles this source
 * with the fast-math flags and, for Manhattan mode, with
 * `USE_MANHATTAN_DISTANCE` defined.
 */

/*
 * OpenCL 1.2 kernels for FontPacker.
 *
 * This is a buffer-based port of shader1.glsl (bitmap SDF), shader3.glsl
 * (outline SDF) and shader3_msdf.glsl (outline MSDF/MSDFA). It intentionally
 * follows the per-pixel algorithm of the GL shaders; the OpenCL build uses
 * fast-math options, so results are close but not guaranteed bit-identical to
 * the OpenGL backend.
 */

/**
 * @brief Select the Manhattan or Euclidean distance formula.
 */
#ifdef USE_MANHATTAN_DISTANCE
#define DISTANCE_FUNC(p1, p2) (fabs((p1).x - (p2).x) + fabs((p1).y - (p2).y))
#else
#define DISTANCE_FUNC(p1, p2) length((p1) - (p2))
#endif

/** @brief Largest finite 32-bit float, used as an initial minimum. */
#define FLT_MAX_VALUE 3.402823466e+38f

typedef struct {
    int type;                  ///< Edge kind: 0=line, 1=quadratic, 2=cubic.
    int shapeId;               ///< Contour/shape identifier.
    unsigned int clr;          ///< RRGGBBXX edge color for MSDF channel selection.
    int padding;               ///< Matches the C++ EdgeSegment padding field.
    float2 points[4];          ///< Control points.

} EdgeSegment;

/**
 * @brief Result of a pseudo-distance search on a curve.
 * @struct PseudoResult
 */
typedef struct {
    float dist;               ///< Unsigned distance to the closest sample.
    float2 point;              ///< Closest point on the sampled curve.
    float t;                   ///< Curve parameter of the closest sample.

} PseudoResult;

/* ------------------------------------------------------------------------- */
/* Bitmap SDF (shader1.glsl)                                                 */
/* ------------------------------------------------------------------------- */

/**
 * @brief Generate a single-channel raw SDF from a raster bitmap.
 * @param fontTexture Input 8-bit grayscale bitmap.
 * @param rawSdf Output raw distance buffer.
 * @param insideTex Output inside/outside mask.
 * @param width Image width.
 * @param height Image height.
 * @param sampleWidth Horizontal search radius.
 * @param sampleHeight Vertical search radius.
 */
__kernel void sdf_bitmap(__global const uchar* fontTexture,
                         __global float* rawSdf,
                         __global unsigned int* insideTex,
                         int width, int height,
                         int sampleWidth, int sampleHeight)
{
    int x = (int)get_global_id(0);
    int y = (int)get_global_id(1);
    if (x >= width || y >= height) return;

    const int index = y * width + x;
    const bool isInside = fontTexture[index] > 127;

#ifdef USE_MANHATTAN_DISTANCE
    const float maxDistance = (float)sampleWidth + (float)sampleHeight;
#else
    const float maxDistance = sqrt((float)sampleWidth * (float)sampleWidth +
                                   (float)sampleHeight * (float)sampleHeight);
#endif

    float minDistance = maxDistance;
    for (int offsetY = -sampleHeight; offsetY <= sampleHeight; ++offsetY) {
        const int sampleY = min(max(y + offsetY, 0), height - 1);
        for (int offsetX = -sampleWidth; offsetX <= sampleWidth; ++offsetX) {
            const int sampleX = min(max(x + offsetX, 0), width - 1);
            const bool sampleIsInside = fontTexture[sampleY * width + sampleX] > 127;
            if (sampleIsInside != isInside) {
#ifdef USE_MANHATTAN_DISTANCE
                const float dist = fabs((float)offsetX) + fabs((float)offsetY);
#else
                const float dist = sqrt((float)offsetX * (float)offsetX +
                                        (float)offsetY * (float)offsetY);
#endif
                if (dist < minDistance) minDistance = dist;
            }
        }
    }

    rawSdf[index] = minDistance / maxDistance;
    insideTex[index] = isInside ? 1u : 0u;
}

/* ------------------------------------------------------------------------- */
/* Outline SDF (shader3.glsl)                                                */
/* ------------------------------------------------------------------------- */

/**
 * @brief Compute the distance from a point to a finite line segment.
 * @param maxDistance Maximum search distance (unused; kept for symmetry).
 * @param p Query point.
 * @param p1 Segment start.
 * @param p2 Segment end.
 * @return Distance from @p p to the segment.
 */
float distanceToLineSegment(float maxDistance, float2 p, float2 p1, float2 p2)
{
    (void)maxDistance;
    float2 v = p2 - p1;
    float2 w = p - p1;
    float c1 = dot(w, v);
    if (c1 <= 0.0f) return DISTANCE_FUNC(p, p1);
    float c2 = dot(v, v);
    if (c2 <= c1) return DISTANCE_FUNC(p, p2);
    float b = c1 / c2;
    return DISTANCE_FUNC(p, p1 + b * v);
}

/**
 * @brief Approximate the distance to a quadratic Bezier curve.
 * @param maxDistance Initial upper bound.
 * @param p Query point.
 * @param p1 Curve start.
 * @param p2 Control point.
 * @param p3 Curve end.
 * @param width Image width, used to choose the sample count.
 * @return Distance from @p p to the sampled curve.
 */
float distanceToQuadraticBezier(float maxDistance, float2 p, float2 p1, float2 p2, float2 p3, int width)
{
    float minDist = maxDistance;
    int steps = width / 4;
    if (steps < 1) steps = 1;
    for (int i = 0; i <= steps; ++i) {
        float t = (float)i / (float)steps;
        float t1 = 1.0f - t;
        float2 pt = t1 * t1 * p1 + 2.0f * t1 * t * p2 + t * t * p3;
        minDist = min(minDist, DISTANCE_FUNC(p, pt));
    }
    return minDist;
}

/**
 * @brief Approximate the distance to a cubic Bezier curve.
 * @param maxDistance Initial upper bound.
 * @param p Query point.
 * @param p1 Curve start.
 * @param p2 First control point.
 * @param p3 Second control point.
 * @param p4 Curve end.
 * @param width Image width, used to choose the sample count.
 * @return Distance from @p p to the sampled curve.
 */
float distanceToCubicBezier(float maxDistance, float2 p, float2 p1, float2 p2, float2 p3, float2 p4, int width)
{
    float minDist = maxDistance;
    int steps = width / 4;
    if (steps < 1) steps = 1;
    for (int i = 0; i <= steps; ++i) {
        float t = (float)i / (float)steps;
        float t1 = 1.0f - t;
        float2 pt = t1 * t1 * t1 * p1 + 3.0f * t1 * t1 * t * p2 +
                    3.0f * t1 * t * t * p3 + t * t * t * p4;
        minDist = min(minDist, DISTANCE_FUNC(p, pt));
    }
    return minDist;
}

/**
 * @brief Compute the winding contribution of one edge at a position.
 * @param pos Position to test.
 * @param edges Edge array.
 * @param i Edge index.
 * @param curveSteps Number of linear samples for curved edges.
 * @return Signed winding contribution (0, +1 or -1).
 */
int calculateWindingFor(float2 pos, __global const EdgeSegment* edges, unsigned int i, int curveSteps)
{
    int winding = 0;
    EdgeSegment edge = edges[i];
    if (edge.type == 0) {
        float2 p1 = edge.points[0];
        float2 p2 = edge.points[1];
        if ((p1.y <= pos.y && p2.y > pos.y) || (p1.y > pos.y && p2.y <= pos.y)) {
            float intersectX = p1.x + (pos.y - p1.y) * (p2.x - p1.x) / (p2.y - p1.y);
            if (pos.x < intersectX) winding += (p1.y <= pos.y) ? 1 : -1;
        }
    } else if (edge.type == 1 || edge.type == 2) {
        float2 prevPoint = edge.points[0];
        for (int j = 1; j <= curveSteps; ++j) {
            float t = (float)j / (float)curveSteps;
            float2 currPoint;
            if (edge.type == 1) {
                float t1 = 1.0f - t;
                currPoint = t1 * t1 * edge.points[0] + 2.0f * t1 * t * edge.points[1] + t * t * edge.points[2];
            } else {
                float t1 = 1.0f - t;
                currPoint = t1 * t1 * t1 * edge.points[0] + 3.0f * t1 * t1 * t * edge.points[1] +
                            3.0f * t1 * t * t * edge.points[2] + t * t * t * edge.points[3];
            }
            if ((prevPoint.y <= pos.y && currPoint.y > pos.y) ||
                (prevPoint.y > pos.y && currPoint.y <= pos.y)) {
                float intersectX = prevPoint.x + (pos.y - prevPoint.y) * (currPoint.x - prevPoint.x) / (currPoint.y - prevPoint.y);
                if (pos.x < intersectX) winding += (prevPoint.y <= pos.y) ? 1 : -1;
            }
            prevPoint = currPoint;
        }
    }
    return winding;
}

/**
 * @brief Compute the total winding number across all edges.
 * @param pos Position to test.
 * @param edges Edge array.
 * @param edgeCount Number of edges.
 * @param curveSteps Number of linear samples for curved edges.
 * @return Winding number (non-zero means inside).
 */
int calculateWinding(float2 pos, __global const EdgeSegment* edges, unsigned int edgeCount, int curveSteps)
{
    int windingNumber = 0;
    for (unsigned int i = 0; i < edgeCount; ++i) {
        windingNumber += calculateWindingFor(pos, edges, i, curveSteps);
    }
    return windingNumber;
}

/**
 * @brief Generate a single-channel raw SDF from vector edges.
 * @param edges Input edge array.
 * @param edgeCount Number of edges.
 * @param rawSdf Output raw distance buffer.
 * @param insideTex Output inside/outside mask.
 * @param width Image width.
 * @param height Image height.
 * @param sampleWidth Horizontal search radius.
 * @param sampleHeight Vertical search radius.
 */
__kernel void sdf_outline(__global const EdgeSegment* edges, unsigned int edgeCount,
                          __global float* rawSdf,
                          __global unsigned int* insideTex,
                          int width, int height,
                          int sampleWidth, int sampleHeight)
{
    int x = (int)get_global_id(0);
    int y = (int)get_global_id(1);
    if (x >= width || y >= height) return;

#ifdef USE_MANHATTAN_DISTANCE
    const float maxDistance = (float)sampleWidth + (float)sampleHeight;
#else
    const float maxDistance = sqrt((float)sampleWidth * (float)sampleWidth +
                                   (float)sampleHeight * (float)sampleHeight);
#endif

    float2 pos = (float2)((float)x + 0.5f, (float)y + 0.5f);
    float minDistance = maxDistance;
    for (unsigned int i = 0; i < edgeCount; ++i) {
        EdgeSegment edge = edges[i];
        float distance = maxDistance;
        if (edge.type == 0) {
            distance = distanceToLineSegment(maxDistance, pos, edge.points[0], edge.points[1]);
        } else if (edge.type == 1) {
            distance = distanceToQuadraticBezier(maxDistance, pos, edge.points[0], edge.points[1], edge.points[2], width);
        } else if (edge.type == 2) {
            distance = distanceToCubicBezier(maxDistance, pos, edge.points[0], edge.points[1], edge.points[2], edge.points[3], width);
        }
        minDistance = min(minDistance, distance);
    }

    const bool inside = calculateWinding(pos, edges, edgeCount, 8) != 0;
    rawSdf[y * width + x] = minDistance;
    insideTex[y * width + x] = inside ? 1u : 0u;
}

/* ------------------------------------------------------------------------- */
/* Outline MSDF (shader3_msdf.glsl)                                         */
/* ------------------------------------------------------------------------- */

/**
 * @brief Unpack an RRGGBBXX edge color into normalized RGB components.
 * @param packedRgb Packed edge color.
 * @return RGB components in the range [0, 1].
 */
float3 unpackRGB(unsigned int packedRgb)
{
    const float inv255 = 1.0f / 255.0f;
    return (float3)(
        (float)((packedRgb >> 16) & 0xFFu) * inv255,
        (float)((packedRgb >> 8) & 0xFFu) * inv255,
        (float)(packedRgb & 0xFFu) * inv255);
}

/**
 * @brief Distance from a point to the line through two points.
 * @param p Query point.
 * @param a First line point.
 * @param b Second line point.
 * @return Distance to the line.
 */
float distanceToLinePseudo(float2 p, float2 a, float2 b)
{
    float2 pa = p - a;
    float2 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0f, 1.0f);
    return DISTANCE_FUNC(p, a + h * ba);
}

/**
 * @brief Find the closest pseudo-distance on a quadratic Bezier curve.
 * @param p Query point.
 * @param p0 Curve start.
 * @param p1 Control point.
 * @param p2 Curve end.
 * @param subdivisions Number of line subdivisions.
 * @return Closest-distance result.
 */
PseudoResult distanceToQuadraticPseudo(float2 p, float2 p0, float2 p1, float2 p2, int subdivisions)
{
    PseudoResult result;
    result.dist = 1e20f;
    result.point = p0;
    result.t = 0.0f;
    float subdRecipr = 1.0f / (float)subdivisions;
    float2 prev = p0;
    for (int i = 1; i <= subdivisions; ++i) {
        float t = (float)i * subdRecipr;
        float2 curr = mix(mix(p0, p1, t), mix(p1, p2, t), t);
        float dist = distanceToLinePseudo(p, prev, curr);
        if (fabs(dist) <= result.dist) {
            result.dist = dist;
            result.point = 0.5f * (prev + curr);
            result.t = t - 0.5f * subdRecipr;
        }
        prev = curr;
    }
    return result;
}

/**
 * @brief Find the closest pseudo-distance on a cubic Bezier curve.
 * @param p Query point.
 * @param p0 Curve start.
 * @param p1 First control point.
 * @param p2 Second control point.
 * @param p3 Curve end.
 * @param subdivisions Number of line subdivisions.
 * @return Closest-distance result.
 */
PseudoResult distanceToCubicPseudo(float2 p, float2 p0, float2 p1, float2 p2, float2 p3, int subdivisions)
{
    PseudoResult result;
    result.dist = 1e20f;
    result.point = p0;
    result.t = 0.0f;
    float subdRecipr = 1.0f / (float)subdivisions;
    float2 prev = p0;
    for (int i = 1; i <= subdivisions; ++i) {
        float t = (float)i * subdRecipr;
        float2 a = mix(p0, p1, t);
        float2 b = mix(p1, p2, t);
        float2 c = mix(p2, p3, t);
        float2 curr = mix(mix(a, b, t), mix(b, c, t), t);
        float dist = distanceToLinePseudo(p, prev, curr);
        if (fabs(dist) <= result.dist) {
            result.dist = dist;
            result.point = 0.5f * (prev + curr);
            result.t = t - 0.5f * subdRecipr;
        }
        prev = curr;
    }
    return result;
}

/**
 * @brief Compute a signed pseudo-distance to one edge.
 * @param p Query point.
 * @param edge Edge segment.
 * @param width Image width, used to choose the sample count.
 * @return Signed distance to the edge.
 */
float signedDistancePseudo(float2 p, EdgeSegment edge, int width)
{
    if (edge.type == 0) {
        float2 a = edge.points[0];
        float2 b = edge.points[1];
        float dist = distanceToLinePseudo(p, a, b);
        float2 dir = normalize(b - a);
        float2 normal = (float2)(-dir.y, dir.x);
        float signValue = dot(p - a, normal);
        return signValue >= 0.0f ? dist : -dist;
    } else if (edge.type == 1) {
        int subdivisions = width / 4;
        if (subdivisions < 1) subdivisions = 1;
        PseudoResult r = distanceToQuadraticPseudo(p, edge.points[0], edge.points[1], edge.points[2], subdivisions);
        float2 d1 = edge.points[1] - edge.points[0];
        float2 d2 = edge.points[2] - edge.points[1];
        float2 tangent = 2.0f * mix(d1, d2, r.t);
        float len2 = dot(tangent, tangent);
        if (len2 < 1e-6f) return r.dist;
        float signValue = sign(tangent.x * (p.y - r.point.y) - tangent.y * (p.x - r.point.x));
        return signValue * r.dist;
    } else if (edge.type == 2) {
        int subdivisions = width / 4;
        if (subdivisions < 1) subdivisions = 1;
        PseudoResult r = distanceToCubicPseudo(p, edge.points[0], edge.points[1], edge.points[2], edge.points[3], subdivisions);
        float2 d10 = edge.points[1] - edge.points[0];
        float2 d21 = edge.points[2] - edge.points[1];
        float2 d32 = edge.points[3] - edge.points[2];
        float2 tangent = 3.0f * mix(mix(d10, d21, r.t), mix(d21, d32, r.t), r.t);
        float len2 = dot(tangent, tangent);
        if (len2 < 1e-6f) return r.dist;
        float signValue = sign(tangent.x * (p.y - r.point.y) - tangent.y * (p.x - r.point.x));
        return signValue * r.dist;
    }
    return 1e20f;
}

/**
 * @brief Calculate the distance from a point to one edge for MSDF.
 * @param pos Query position.
 * @param edges Edge array.
 * @param i Edge index.
 * @param width Image width, used to choose the sample count.
 * @return Distance to the edge.
 */
float calculateDistanceMSDF(float2 pos, __global const EdgeSegment* edges, unsigned int i, int width)
{
    EdgeSegment edge = edges[i];
    float distance = FLT_MAX_VALUE;
    if (edge.type == 0) {
        distance = distanceToLineSegment(FLT_MAX_VALUE, pos, edge.points[0], edge.points[1]);
    } else if (edge.type == 1) {
        distance = distanceToQuadraticBezier(FLT_MAX_VALUE, pos, edge.points[0], edge.points[1], edge.points[2], width);
    } else if (edge.type == 2) {
        distance = distanceToCubicBezier(FLT_MAX_VALUE, pos, edge.points[0], edge.points[1], edge.points[2], edge.points[3], width);
    }
    return distance;
}

/**
 * @brief Generate per-channel raw MSDF distances from vector edges.
 * @param edges Input edge array.
 * @param edgeCount Number of edges.
 * @param rawSdf Output interleaved RGBA distance buffer.
 * @param insideTex Output per-channel inside/outside mask.
 * @param width Image width.
 * @param height Image height.
 * @param sampleWidth Horizontal search radius.
 * @param sampleHeight Vertical search radius.
 */
__kernel void msdf_outline(__global const EdgeSegment* edges, unsigned int edgeCount,
                           __global float* rawSdf,
                           __global unsigned int* insideTex,
                           int width, int height,
                           int sampleWidth, int sampleHeight)
{
    int x = (int)get_global_id(0);
    int y = (int)get_global_id(1);
    if (x >= width || y >= height) return;

    float2 pos = (float2)((float)x + 0.5f, (float)y + 0.5f);
    const float maxDistance = FLT_MAX_VALUE;
#ifdef USE_MANHATTAN_DISTANCE
    const float realMaxDistance = (float)sampleWidth + (float)sampleHeight;
#else
    const float realMaxDistance = sqrt((float)sampleWidth * (float)sampleWidth +
                                       (float)sampleHeight * (float)sampleHeight);
#endif

    float4 minDistance = (float4)(maxDistance);
    int4 closestEdgeIds = (int4)(-1);

    for (unsigned int i = 0; i < edgeCount; ++i) {
        EdgeSegment edge = edges[i];
        float3 clr = unpackRGB(edge.clr);
        float dist = calculateDistanceMSDF(pos, edges, i, width);
        if (dist <= minDistance.w) {
            minDistance.w = dist;
            closestEdgeIds.w = (int)i;
        }
        if (clr.x >= 0.003921568627451f && dist <= minDistance.x) {
            minDistance.x = dist;
            closestEdgeIds.x = (int)i;
        }
        if (clr.y >= 0.003921568627451f && dist <= minDistance.y) {
            minDistance.y = dist;
            closestEdgeIds.y = (int)i;
        }
        if (clr.z >= 0.003921568627451f && dist <= minDistance.z) {
            minDistance.z = dist;
            closestEdgeIds.z = (int)i;
        }
    }

    int4 winding = (int4)(0);
    if (closestEdgeIds.x >= 0) winding.x = calculateWindingFor(pos, edges, (unsigned int)closestEdgeIds.x, width / 4);
    if (closestEdgeIds.y >= 0) winding.y = calculateWindingFor(pos, edges, (unsigned int)closestEdgeIds.y, width / 4);
    if (closestEdgeIds.z >= 0) winding.z = calculateWindingFor(pos, edges, (unsigned int)closestEdgeIds.z, width / 4);
    winding.w = calculateWinding(pos, edges, edgeCount, width / 4);

    minDistance = (float4)(
        closestEdgeIds.x >= 0 ? signedDistancePseudo(pos, edges[closestEdgeIds.x], width) : maxDistance,
        closestEdgeIds.y >= 0 ? signedDistancePseudo(pos, edges[closestEdgeIds.y], width) : maxDistance,
        closestEdgeIds.z >= 0 ? signedDistancePseudo(pos, edges[closestEdgeIds.z], width) : maxDistance,
        winding.w <= 0 ? -minDistance.w : minDistance.w);

    minDistance.x = clamp(minDistance.x, -realMaxDistance, realMaxDistance);
    minDistance.y = clamp(minDistance.y, -realMaxDistance, realMaxDistance);
    minDistance.z = clamp(minDistance.z, -realMaxDistance, realMaxDistance);
    minDistance.w = fabs(clamp(minDistance.w, -realMaxDistance, realMaxDistance));

    if (closestEdgeIds.w >= 0) {
        float3 clr = unpackRGB(edges[closestEdgeIds.w].clr);
        float windingSign = winding.w <= 0 ? -1.0f : 1.0f;
        if (clr.x >= 0.003921568627451f) minDistance.x = fabs(minDistance.x) * windingSign;
        if (clr.y >= 0.003921568627451f) minDistance.y = fabs(minDistance.y) * windingSign;
        if (clr.z >= 0.003921568627451f) minDistance.z = fabs(minDistance.z) * windingSign;
    }
    if (winding.w == 0) minDistance.w = minDistance.w * -1.0f;

    const int base = (y * width + x) * 4;
    rawSdf[base + 0] = minDistance.x;
    rawSdf[base + 1] = minDistance.y;
    rawSdf[base + 2] = minDistance.z;
    rawSdf[base + 3] = minDistance.w;
    insideTex[base + 0] = winding.x != 0 ? 1u : 0u;
    insideTex[base + 1] = winding.y != 0 ? 1u : 0u;
    insideTex[base + 2] = winding.z != 0 ? 1u : 0u;
    insideTex[base + 3] = winding.w != 0 ? 1u : 0u;
}
