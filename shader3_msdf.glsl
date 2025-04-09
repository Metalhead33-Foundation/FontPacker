#version 430 core
layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout (binding = 1, rgba32f) writeonly uniform image2D rawSdfTexture;
layout (binding = 2, rgba8) writeonly uniform image2D isInsideTex;

const int LINEAR = 0;
const int QUADRATIC = 1;
const int CUBIC = 2;

struct EdgeSegment {
    int type;
    int shapeId;
    uint clr;
    vec2 points[4];
};

layout(std430, binding = 3) buffer EdgeBuffer {
    EdgeSegment edges[];
};

layout (binding = 4, std140) uniform Dimensions {
    int intendedSampleWidth;
    int intendedSampleHeight;
};

#ifdef USE_MANHATTAN_DISTANCE
    #define DISTANCE_FUNC(p1, p2) (abs((p1).x - (p2).x) + abs((p1).y - (p2).y))
#else
    #define DISTANCE_FUNC(p1, p2) length((p1) - (p2))
#endif

vec3 unpackRGB(uint packedRgb) {
    const float inv255 = 1.0 / 255.0;
    return vec3(
        float((packedRgb >> 16) & 0xFF) * inv255,
        float((packedRgb >> 8)  & 0xFF) * inv255,
        float( packedRgb        & 0xFF) * inv255
    );
}
#define FLT_MAX 3.402823466e+38
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
    float minDist = FLT_MAX;
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
    float minDist = FLT_MAX;
    int STEPS = imageSize(rawSdfTexture).x / 4;
    for (int i = 0; i <= STEPS; i++) {
	float t = float(i) / float(STEPS);
	float t1 = 1.0 - t;
	vec2 pt = t1*t1*t1*p1 + 3.0*t1*t1*t*p2 + 3.0*t1*t*t*p3 + t*t*t*p4;
	minDist = min(minDist, DISTANCE_FUNC(p, pt));
    }
    return minDist;
}

float calculateDistance(float maxDistance, vec2 pos, uint i)
{
    EdgeSegment edge = edges[i];
    float distance = FLT_MAX;
    if (edge.type == LINEAR) {
	distance = distanceToLineSegment(maxDistance, pos, edge.points[0], edge.points[1]);
    }
    else if (edge.type == QUADRATIC) {
	distance = distanceToQuadraticBezier(maxDistance, pos, edge.points[0], edge.points[1], edge.points[2]);
    }
    else if (edge.type == CUBIC) {
	distance = distanceToCubicBezier(maxDistance, pos, edge.points[0], edge.points[1], edge.points[2], edge.points[3]);
    }
    return distance;
}
// 2D cross product (returns scalar)
float cross2D(vec2 a, vec2 b) {
    return a.x * b.y - a.y * b.x;
}
const float EPSILON = 1e-6;

float pseudoDistanceToLineSegment(float maxDist, vec2 p, vec2 a, vec2 b) {
    // Vector from a to b
    vec2 ab = b - a;
    // Vector from a to p
    vec2 ap = p - a;

    // Squared length of the line segment ab.
    // Using dot product for efficiency (avoids sqrt).
    float abLenSq = dot(ab, ab);

    // Handle degenerate case (a and b are the same point)
    if (abLenSq < EPSILON * EPSILON) {
	return length(ap); // Distance from p to a (or b)
    }

    // Project vector ap onto vector ab to find the parameter t
    // t represents how far along the infinite line (defined by a and b)
    // the projection of p falls.
    // t = dot(ap, ab) / dot(ab, ab)
    float t = dot(ap, ab) / abLenSq;

    // Clamp t to the range [0, 1] to stay within the line segment.
    // If t < 0, the closest point on the segment is a.
    // If t > 1, the closest point on the segment is b.
    // Otherwise, the closest point is along the segment.
    t = clamp(t, 0.0, 1.0);

    // Calculate the closest point on the line segment to p
    vec2 closestPointOnSegment = a + t * ab;

    // Calculate the distance between p and the closest point on the segment
    float dist = length(p - closestPointOnSegment);

    // Clamp the distance to the maximum value
    return dist;
}

// Helper function to evaluate a quadratic Bezier curve at parameter t
vec2 evaluateQuadraticBezier(vec2 p0, vec2 p1, vec2 p2, float t) {
    vec2 p01 = mix(p0, p1, t);
    vec2 p12 = mix(p1, p2, t);
    return mix(p01, p12, t);
    /* Direct formula (equivalent):
    float omt = 1.0 - t;
    float omt2 = omt * omt;
    float t2 = t * t;
    return p0 * omt2 + p1 * 2.0 * omt * t + p2 * t2;
    */
}

// Helper function to evaluate a cubic Bezier curve at parameter t
vec2 evaluateCubicBezier(vec2 p0, vec2 p1, vec2 p2, vec2 p3, float t) {
    vec2 p01 = mix(p0, p1, t);
    vec2 p12 = mix(p1, p2, t);
    vec2 p23 = mix(p2, p3, t);
    vec2 p012 = mix(p01, p12, t);
    vec2 p123 = mix(p12, p23, t);
    return mix(p012, p123, t);
    /* Direct formula (equivalent):
    float omt = 1.0 - t;
    float omt2 = omt * omt;
    float omt3 = omt2 * omt;
    float t2 = t * t;
    float t3 = t2 * t;
    return p0 * omt3 + p1 * 3.0 * omt2 * t + p2 * 3.0 * omt * t2 + p3 * t3;
    */
}

float pseudoDistanceToQuadraticBezier(float maxDist, vec2 p, vec2 p0, vec2 p1, vec2 p2) {
    float min_dist_sq = FLT_MAX;
    int BEZIER_STEPS = imageSize(rawSdfTexture).x / 4;
    float step = 1.0 / float(BEZIER_STEPS);

    // Check distance at sample points along the curve
    for (int i = 0; i <= BEZIER_STEPS; ++i) {
	float t = float(i) * step;
	vec2 curve_p = evaluateQuadraticBezier(p0, p1, p2, t);
	vec2 diff = p - curve_p;
	min_dist_sq = min(min_dist_sq, dot(diff, diff));
    }

    return sqrt(min_dist_sq);
}
float pseudoDistanceToCubicBezier(float maxDist, vec2 p, vec2 p0, vec2 p1, vec2 p2, vec2 p3) {
    float min_dist_sq = FLT_MAX;
    int BEZIER_STEPS = imageSize(rawSdfTexture).x / 4;
    float step = 1.0 / float(BEZIER_STEPS);

    // Check distance at sample points along the curve
    for (int i = 0; i <= BEZIER_STEPS; ++i) {
	float t = float(i) * step;
	vec2 curve_p = evaluateCubicBezier(p0, p1, p2, p3, t);
	vec2 diff = p - curve_p;
	min_dist_sq = min(min_dist_sq, dot(diff, diff));
    }

    return sqrt(min_dist_sq);
}
float calculatePseudoDistance(float maxDistance, vec2 pos, uint i)
{
    EdgeSegment edge = edges[i];
    float distance = FLT_MAX;
    if (edge.type == LINEAR) {
	distance = pseudoDistanceToLineSegment(maxDistance, pos, edge.points[0], edge.points[1]);
    }
    else if (edge.type == QUADRATIC) {
	distance = pseudoDistanceToQuadraticBezier(maxDistance, pos, edge.points[0], edge.points[1], edge.points[2]);
    }
    else if (edge.type == CUBIC) {
	distance = pseudoDistanceToCubicBezier(maxDistance, pos, edge.points[0], edge.points[1], edge.points[2], edge.points[3]);
    }
    return distance;
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

int calculateWinding(vec2 pos) {
    int windingNumber = 0;
    for (int i = 0; i < edges.length(); ++i) {
	windingNumber += calculateWindingFor(pos, i);
    }
    return windingNumber;
}

void main(void) {
    ivec2 threadId = ivec2(gl_GlobalInvocationID.xy);
    ivec2 imageDimensions = imageSize(rawSdfTexture);
    if (threadId.x >= imageDimensions.x || threadId.y >= imageDimensions.y) return;

    /*
    #ifdef USE_MANHATTAN_DISTANCE
    float maxDistance = abs(float(intendedSampleWidth)) + abs(float(intendedSampleHeight));
    #else
    float maxDistance = length(vec2(intendedSampleWidth, intendedSampleHeight));
    #endif
    */
    float maxDistance = FLT_MAX;

    vec2 pos = vec2(threadId) + vec2(0.5);
    ivec4 winding = ivec4(0,0,0,0);
    int currentShapeId = -1;

    // First pass: compute accurate winding number
    winding.a = calculateWinding(pos);

    ivec3 closestShapeIds = ivec3(0,0,0);
    vec3 minDistance = vec3(maxDistance,maxDistance,maxDistance);
    // Second pass: compute minimum distance (now that we know winding)
    for (int i = 0; i < edges.length(); ++i) {
	EdgeSegment edge = edges[i];
	vec3 clr = unpackRGB(edge.clr);
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

	if( (clr.r >= 0.003921568627451) && (distance <= minDistance.r)) {
	    minDistance.r = distance;
	    closestShapeIds.r = i;
	}

	if( (clr.g >= 0.003921568627451) && (distance <= minDistance.g)) {
	    minDistance.g = distance;
	    closestShapeIds.g = i;
	}

	if( (clr.b >= 0.003921568627451) && (distance <= minDistance.b)) {
	    minDistance.b = distance;
	    closestShapeIds.b = i;
	}
    }
    minDistance = vec3( calculatePseudoDistance(maxDistance, pos, closestShapeIds.r ),
                        calculatePseudoDistance(maxDistance, pos, closestShapeIds.g ),
                        calculatePseudoDistance(maxDistance, pos, closestShapeIds.b ) );
    bool inside = winding.a != 0;
    // We do NOT normalize sdfValue to [-0.5, 0.5] or [0, 1] here - we do it in a separate pass on the CPU.

    imageStore(rawSdfTexture, threadId, vec4(minDistance, 1.0));
    imageStore(isInsideTex, threadId, vec4(float(inside)));
}

