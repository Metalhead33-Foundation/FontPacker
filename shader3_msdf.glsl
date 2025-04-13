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
        float dst = DISTANCE_FUNC(p, pt);
        if(abs(dst) <= minDist) minDist = dst;
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
        float dst = DISTANCE_FUNC(p, pt);
        if(abs(dst) <= minDist) minDist = dst;
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

vec2 lerp(vec2 a, vec2 b, float t) {
    return mix(a, b, t);
}

vec2 bezier2(vec2 p0, vec2 p1, vec2 p2, float t) {
    vec2 a = lerp(p0, p1, t);
    vec2 b = lerp(p1, p2, t);
    return lerp(a, b, t);
}

vec2 bezier3(vec2 p0, vec2 p1, vec2 p2, vec2 p3, float t) {
    vec2 a = lerp(p0, p1, t);
    vec2 b = lerp(p1, p2, t);
    vec2 c = lerp(p2, p3, t);
    vec2 d = lerp(a, b, t);
    vec2 e = lerp(b, c, t);
    return lerp(d, e, t);
}

float distanceToLinePseudo(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a;
    vec2 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    vec2 nearest = a + h * ba;
    return DISTANCE_FUNC(p, nearest);
}

float distanceToQuadraticPseudo(vec2 p, vec2 p0, vec2 p1, vec2 p2, out vec2 closestPoint, out float closestT) {
    int SUBDIVISIONS = imageSize(rawSdfTexture).x / 4;
    float minDist = 1e20;
    float subdRecipr = 1.0 / float(SUBDIVISIONS);

    vec2 prev = p0;
    closestT = 0.0;

    for (int i = 1; i <= SUBDIVISIONS; ++i) {
        float t = float(i) * subdRecipr;
        vec2 curr = bezier2(p0, p1, p2, t);
        float dist = distanceToLinePseudo(p, prev, curr);
        if (abs(dist) <= minDist) {
            minDist = dist;
            closestPoint = 0.5 * (prev + curr);
            closestT = t - 0.5 * subdRecipr;
        }
        prev = curr;
    }

    return minDist;
}

float distanceToCubicPseudo(vec2 p, vec2 p0, vec2 p1, vec2 p2, vec2 p3, out vec2 closestPoint, out float closestT) {
    int SUBDIVISIONS = imageSize(rawSdfTexture).x / 4;
    float minDist = 1e20;
    float subdRecipr = 1.0 / float(SUBDIVISIONS);

    vec2 prev = p0;
    closestT = 0.0;

    for (int i = 1; i <= SUBDIVISIONS; ++i) {
        float t = float(i) * subdRecipr;
        vec2 curr = bezier3(p0, p1, p2, p3, t);
        float dist = distanceToLinePseudo(p, prev, curr);
        if (abs(dist) <= minDist) {
            minDist = dist;
            closestPoint = 0.5 * (prev + curr);
            closestT = t - 0.5 * subdRecipr;
        }
        prev = curr;
    }

    return minDist;
}

float signedDistancePseudo(vec2 p, EdgeSegment edge) {
    if (edge.type == LINEAR) {
        vec2 a = edge.points[0];
        vec2 b = edge.points[1];
        float dist = distanceToLinePseudo(p, a, b);
        vec2 dir = normalize(b - a);
        vec2 normal = vec2(-dir.y, dir.x);
        float sign = dot(p - a, normal);
        return sign >= 0.0 ? dist : -dist;
    }

    else if (edge.type == QUADRATIC) {
        vec2 closest;
        float t;
        float dist = distanceToQuadraticPseudo(p, edge.points[0], edge.points[1], edge.points[2], closest, t);

        // Analytical tangent of quadratic Bezier
        vec2 d1 = edge.points[1] - edge.points[0];
        vec2 d2 = edge.points[2] - edge.points[1];
        vec2 tangent = 2.0 * mix(d1, d2, t);
        float len2 = dot(tangent, tangent);
        if (len2 < 1e-6)
            return dist; // Unsure of sign, return unsigned distance
        vec2 normal = vec2(-tangent.y, tangent.x);
        float sign = sign(tangent.x * (p.y - closest.y) - tangent.y * (p.x - closest.x));
        return sign * dist;
    }

    else if (edge.type == CUBIC) {
        vec2 closest;
        float t;
        float dist = distanceToCubicPseudo(p, edge.points[0], edge.points[1], edge.points[2], edge.points[3], closest, t);

        // Analytical tangent of cubic Bezier
        vec2 d10 = edge.points[1] - edge.points[0];
        vec2 d21 = edge.points[2] - edge.points[1];
        vec2 d32 = edge.points[3] - edge.points[2];
        vec2 tangent = 3.0 * mix(mix(d10, d21, t), mix(d21, d32, t), t);
        float len2 = dot(tangent, tangent);
        if (len2 < 1e-6)
            return dist; // Unsure of sign, return unsigned distance
        vec2 normal = vec2(-tangent.y, tangent.x);
        float sign = sign(tangent.x * (p.y - closest.y) - tangent.y * (p.x - closest.x));
        return sign * dist;
    }

    return 1e20;
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
        const int CURVE_STEPS = imageSize(rawSdfTexture).x / 4;
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

int calculateWindingForContour(vec2 pos, int shapeId) {
    int winding = 0;
    for (int i = 0; i < edges.length(); ++i) {
	if (edges[i].shapeId == shapeId) {
	    winding += calculateWindingFor(pos, i);
	}
    }
    return winding;
}

int calculateWindingForChannel(vec2 pos, int channel) {
    int winding = 0;
    for (int i = 0; i < edges.length(); ++i) {
	vec3 clr = unpackRGB(edges[i].clr);
	bool contributes = (channel == 0 && clr.r >= 0.003921568627451) ||
	                  (channel == 1 && clr.g >= 0.003921568627451) ||
	                  (channel == 2 && clr.b >= 0.003921568627451);
	if (contributes) {
	    winding += calculateWindingFor(pos, i);
	}
    }
    return winding;
}

int calculateWinding(vec2 pos) {
    int windingNumber = 0;
    for (int i = 0; i < edges.length(); ++i) {
	int tmpNum = calculateWindingFor(pos, i);
	windingNumber += tmpNum;
    }
    return windingNumber;
}

void main(void) {
    ivec2 threadId = ivec2(gl_GlobalInvocationID.xy);
    ivec2 dims = imageSize(rawSdfTexture);
    if (threadId.x >= dims.x || threadId.y >= dims.y) return;

    vec2 pos = vec2(threadId) + 0.5;
    float maxDistance = FLT_MAX;
    #ifdef USE_MANHATTAN_DISTANCE
    float realMaxDistance = abs(float(intendedSampleWidth)) + abs(float(intendedSampleHeight));
    #else
    float realMaxDistance = length(vec2(intendedSampleWidth, intendedSampleHeight));
    #endif

    // Find closest edge per channel and track contour IDs
    vec4 minDistance = vec4(maxDistance);
    ivec3 closestEdgeIds = ivec3(-1);
    ivec3 closestContourIds = ivec3(-1);
    for (int i = 0; i < edges.length(); ++i) {
	EdgeSegment edge = edges[i];
	vec3 clr = unpackRGB(edge.clr);
	float dist = calculateDistance(maxDistance, pos, i);
        if (dist <= minDistance.a) {
            minDistance.a = dist;
        }
        if (clr.r >= 0.003921568627451 && dist <= minDistance.r) {
	    minDistance.r = dist;
	    closestEdgeIds.r = i;
	    closestContourIds.r = edge.shapeId;
	}
        if (clr.g >= 0.003921568627451 && dist <= minDistance.g) {
	    minDistance.g = dist;
	    closestEdgeIds.g = i;
	    closestContourIds.g = edge.shapeId;
	}
        if (clr.b >= 0.003921568627451 && dist <= minDistance.b) {
	    minDistance.b = dist;
	    closestEdgeIds.b = i;
	    closestContourIds.b = edge.shapeId;
	}
    }

    // Compute winding based on the contour of the closest edge per channel
    ivec4 winding = ivec4(0);
    if (closestEdgeIds.r >= 0) winding.r = calculateWindingFor(pos, closestEdgeIds.r);
    if (closestEdgeIds.g >= 0) winding.g = calculateWindingFor(pos, closestEdgeIds.g);
    if (closestEdgeIds.b >= 0) winding.b = calculateWindingFor(pos, closestEdgeIds.b);
    winding.a = calculateWinding(pos); // Global winding for all contours


    // Refine distances
    minDistance = vec4(
        closestEdgeIds.r >= 0 ? signedDistancePseudo(pos, edges[closestEdgeIds.r]) : maxDistance,
        closestEdgeIds.g >= 0 ? signedDistancePseudo(pos, edges[closestEdgeIds.g]) : maxDistance,
        closestEdgeIds.b >= 0 ? signedDistancePseudo(pos, edges[closestEdgeIds.b]) : maxDistance,
        winding.a <= 0 ? -minDistance.a : minDistance.a
    );
    minDistance.r = clamp(minDistance.r, -realMaxDistance, realMaxDistance);
    minDistance.g = clamp(minDistance.g, -realMaxDistance, realMaxDistance);
    minDistance.b = clamp(minDistance.b, -realMaxDistance, realMaxDistance);
    minDistance.a = abs(clamp(minDistance.a, -realMaxDistance, realMaxDistance));
    if(winding.a == 0) minDistance.a = minDistance.a * -1.0;
    imageStore(rawSdfTexture, threadId, minDistance);
    imageStore(isInsideTex, threadId, vec4(
        float(winding.r != 0),
        float(winding.g != 0),
        float(winding.b != 0),
        float(winding.a != 0)
    ));
}

