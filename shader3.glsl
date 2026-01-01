/**
 * @file shader3.glsl
 * @brief OpenGL compute shader for generating single-channel SDF from vector outlines.
 * 
 * This shader computes signed distance fields directly from vector edge segments
 * (lines, quadratic and cubic Bezier curves). It uses winding number calculation to
 * determine inside/outside and computes distances analytically for lines and
 * numerically for Bezier curves.
 * 
 * @version 430 core
 * @requires OpenGL 4.3+ with compute shader support
 */

#version 430 core
layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

/**
 * @brief Output SDF texture (32-bit float, single channel).
 * @binding 1
 */
layout (binding = 1, r32f) writeonly uniform image2D rawSdfTexture;

/**
 * @brief Output inside/outside mask texture (8-bit).
 * @binding 2
 */
layout (binding = 2, r8) writeonly uniform image2D isInsideTex;

const int LINEAR = 0;     ///< Edge type: line segment
const int QUADRATIC = 1;  ///< Edge type: quadratic Bezier curve
const int CUBIC = 2;     ///< Edge type: cubic Bezier curve

/**
 * @brief Edge segment structure matching C++ EdgeSegment.
 * @struct EdgeSegment
 */
struct EdgeSegment {
    int type;        ///< Edge type (LINEAR, QUADRATIC, or CUBIC)
    int shapeId;     ///< Shape/contour ID this edge belongs to
    uint clr;        ///< Edge color (currently unused, reserved for MSDF)
    vec2 points[4];  ///< Control points (up to 4 depending on type)
};

/**
 * @brief Shader storage buffer containing all edge segments.
 * @binding 3
 */
layout(std430, binding = 3) buffer EdgeBuffer {
    EdgeSegment edges[];  ///< Array of edge segments
};

/**
 * @brief Uniform buffer containing dimensions.
 * @binding 4
 * @struct Dimensions
 */
layout (binding = 4, std140) uniform Dimensions {
    int intendedSampleWidth;   ///< Sample search width (used for max distance calculation)
    int intendedSampleHeight; ///< Sample search height (used for max distance calculation)
};

#ifdef USE_MANHATTAN_DISTANCE
    #define DISTANCE_FUNC(p1, p2) (abs((p1).x - (p2).x) + abs((p1).y - (p2).y))
#else
    #define DISTANCE_FUNC(p1, p2) length((p1) - (p2))
#endif

float distanceToLineSegment(float maxDistance, vec2 p, vec2 p1, vec2 p2) {
    vec2 v = p2 - p1;
    vec2 w = p - p1;
    float c1 = dot(w, v);
    if (c1 <= 0.0) return DISTANCE_FUNC(p, p1);
    float c2 = dot(v, v);
    if (c2 <= c1) return DISTANCE_FUNC(p, p2);
    float b = c1 / c2;
    return DISTANCE_FUNC(p, p1 + b * v);
}

float distanceToQuadraticBezier(float maxDistance, vec2 p, vec2 p1, vec2 p2, vec2 p3) {
    float minDist = maxDistance;
    int STEPS = imageSize(rawSdfTexture).x / 4;
    for (int i = 0; i <= STEPS; i++) {
	float t = float(i) / float(STEPS);
	float t1 = 1.0 - t;
	vec2 pt = t1 * t1 * p1 + 2.0 * t1 * t * p2 + t * t * p3;
	minDist = min(minDist, DISTANCE_FUNC(p, pt));
    }
    return minDist;
}

float distanceToCubicBezier(float maxDistance, vec2 p, vec2 p1, vec2 p2, vec2 p3, vec2 p4) {
    float minDist = maxDistance;
    int STEPS = imageSize(rawSdfTexture).x / 4;
    for (int i = 0; i <= STEPS; i++) {
	float t = float(i) / float(STEPS);
	float t1 = 1.0 - t;
	vec2 pt = t1*t1*t1*p1 + 3.0*t1*t1*t*p2 + 3.0*t1*t*t*p3 + t*t*t*p4;
	minDist = min(minDist, DISTANCE_FUNC(p, pt));
    }
    return minDist;
}

int calculateWindingFor(vec2 pos, uint i) {
    int winding = 0;
    EdgeSegment edge = edges[i];

    if (edge.type == LINEAR) {
	vec2 p1 = edge.points[0];
	vec2 p2 = edge.points[1];
	if ((p1.y <= pos.y && p2.y > pos.y) || (p1.y > pos.y && p2.y <= pos.y)) {
	    float intersectX = p1.x + (pos.y - p1.y) * (p2.x - p1.x) / (p2.y - p1.y);
	    if (pos.x < intersectX) {
		winding += (p1.y <= pos.y) ? 1 : -1;
	    }
	}
    }
    else if (edge.type == QUADRATIC || edge.type == CUBIC) {
	const int CURVE_STEPS = 8;
	vec2 prevPoint = edge.points[0];

	for (int j = 1; j <= CURVE_STEPS; j++) {
	    float t = float(j) / float(CURVE_STEPS);
	    vec2 currPoint;

	    if (edge.type == QUADRATIC) {
		float t1 = 1.0 - t;
		currPoint = t1*t1*edge.points[0] + 2.0*t1*t*edge.points[1] + t*t*edge.points[2];
	    } else {
		float t1 = 1.0 - t;
		currPoint = t1*t1*t1*edge.points[0] + 3.0*t1*t1*t*edge.points[1] + 3.0*t1*t*t*edge.points[2] + t*t*t*edge.points[3];
	    }

	    if ((prevPoint.y <= pos.y && currPoint.y > pos.y) || (prevPoint.y > pos.y && currPoint.y <= pos.y)) {
		float intersectX = prevPoint.x + (pos.y - prevPoint.y) * (currPoint.x - prevPoint.x) / (currPoint.y - prevPoint.y);
		if (pos.x < intersectX) {
		    winding += (prevPoint.y <= pos.y) ? 1 : -1;
		}
	    }
	    prevPoint = currPoint;
	}
    }
    return winding;
}

/**
 * @brief Calculate winding number for a position across all edges.
 * @param pos Position to test.
 * @return Winding number (non-zero means inside).
 */
int calculateWinding(vec2 pos) {
    int windingNumber = 0;
    for (int i = 0; i < edges.length(); ++i) {
	windingNumber += calculateWindingFor(pos, i);
    }
    return windingNumber;
}

/**
 * @brief Main compute shader entry point.
 * 
 * For each pixel:
 * 1. Calculates winding number to determine inside/outside
 * 2. Finds minimum distance to all edge segments
 * 3. Writes signed distance and inside/outside flag
 * 
 * Note: SDF values are not normalized here; normalization is performed
 * in a separate CPU pass.
 */
void main(void) {
    ivec2 threadId = ivec2(gl_GlobalInvocationID.xy);
    ivec2 imageDimensions = imageSize(rawSdfTexture);
    if (threadId.x >= imageDimensions.x || threadId.y >= imageDimensions.y) return;

    #ifdef USE_MANHATTAN_DISTANCE
    float maxDistance = abs(float(intendedSampleWidth)) + abs(float(intendedSampleHeight));
    #else
    float maxDistance = length(vec2(intendedSampleWidth, intendedSampleHeight));
    #endif

    vec2 pos = vec2(threadId) + vec2(0.5);
    float minDistance = maxDistance;
    int winding = 0;
    int currentShapeId = -1;

    // First pass: compute accurate winding number
    winding = calculateWinding(pos);

    // Second pass: compute minimum distance (now that we know winding)
    for (int i = 0; i < edges.length(); ++i) {
	EdgeSegment edge = edges[i];
	float distance = maxDistance;

	if (edge.type == LINEAR) {
	    distance = distanceToLineSegment(maxDistance, pos, edge.points[0], edge.points[1]);
	}
	else if (edge.type == QUADRATIC) {
	    distance = distanceToQuadraticBezier(maxDistance, pos, edge.points[0], edge.points[1], edge.points[2]);
	}
	else if (edge.type == CUBIC) {
	    distance = distanceToCubicBezier(maxDistance, pos, edge.points[0], edge.points[1], edge.points[2], edge.points[3]);
	}

	minDistance = min(minDistance, distance);
    }

    bool inside = winding != 0;
    // We do NOT normalize sdfValue to [-0.5, 0.5] or [0, 1] here - we do it in a separate pass on the CPU.
    float signedDistance = minDistance;

    imageStore(rawSdfTexture, threadId, vec4(signedDistance));
    imageStore(isInsideTex, threadId, vec4(float(inside)));
}
