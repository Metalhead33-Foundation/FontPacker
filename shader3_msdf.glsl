#version 430 core
layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
// Output MSDF texture (RGB channels for different distance fields)
layout (binding = 1, rgba32f) writeonly uniform image2D msdfTexture;
layout (binding = 2, r8) writeonly uniform image2D isInsideTex;

// Edge types matching C++ enum
const int LINEAR = 0;
const int QUADRATIC = 1;
const int CUBIC = 2;

struct EdgeSegment {
    int type;
    vec2 points[4];
};

layout(std430, binding = 3) buffer EdgeBuffer {
    EdgeSegment edges[];
};

// Structure to hold edge calculations
struct EdgeInfo {
    float distance;  // Euclidean distance to edge
    vec2 position;   // Closest point on edge
    vec2 normal;     // Normal at closest point (pointing away from shape)
    int edgeIndex;   // Index of the edge in the buffer
};

// Calculate distance info for a line segment (p1, p2)
EdgeInfo distanceToLineSegment(vec2 p, vec2 p1, vec2 p2, int edgeIndex) {
    EdgeInfo info;
    info.edgeIndex = edgeIndex;

    vec2 v = p2 - p1;
    vec2 w = p - p1;

    // Project point onto line segment
    float c1 = dot(w, v);
    float c2 = dot(v, v);

    if (c1 <= 0.0) {
	// Point is closest to p1
	info.distance = length(p - p1);
	info.position = p1;
	info.normal = normalize(p - p1);
    } else if (c2 <= c1) {
	// Point is closest to p2
	info.distance = length(p - p2);
	info.position = p2;
	info.normal = normalize(p - p2);
    } else {
	// Point is closest to a point on the segment
	float b = c1 / c2;
	vec2 pb = p1 + b * v;
	info.distance = length(p - pb);
	info.position = pb;
	info.normal = normalize(p - pb);
    }

    return info;
}

// Get the tangent direction at point t on a quadratic Bezier curve
vec2 quadraticBezierTangent(vec2 p1, vec2 p2, vec2 p3, float t) {
    float t1 = 1.0 - t;
    return normalize(2.0 * t1 * (p2 - p1) + 2.0 * t * (p3 - p2));
}

// Calculate distance info for a quadratic Bezier curve (p1, p2, p3)
EdgeInfo distanceToQuadraticBezier(vec2 p, vec2 p1, vec2 p2, vec2 p3, int edgeIndex) {
    EdgeInfo info;
    info.edgeIndex = edgeIndex;
    info.distance = 1e10;

    // We'll use an iterative approach to find the closest point on the curve
    const int STEPS = 15;  // Higher for better accuracy
    float bestT = 0.0;

    for (int i = 0; i <= STEPS; i++) {
	float t = float(i) / float(STEPS);
	float t1 = 1.0 - t;

	// Quadratic Bezier formula: B(t) = (1-t)²p1 + 2(1-t)tp2 + t²p3
	vec2 pt = t1 * t1 * p1 + 2.0 * t1 * t * p2 + t * t * p3;
	float dist = length(p - pt);

	if (dist < info.distance) {
	    info.distance = dist;
	    info.position = pt;
	    bestT = t;
	}
    }

    // Get tangent at the closest point and use it to compute the normal
    vec2 tangent = quadraticBezierTangent(p1, p2, p3, bestT);
    // Normal is perpendicular to tangent
    info.normal = normalize(vec2(-tangent.y, tangent.x));

    // Ensure normal points away from shape (this might need additional logic
    // based on your specific shape orientation)
    if (dot(info.normal, p - info.position) < 0.0) {
	info.normal = -info.normal;
    }

    return info;
}

// Get the tangent direction at point t on a cubic Bezier curve
vec2 cubicBezierTangent(vec2 p1, vec2 p2, vec2 p3, vec2 p4, float t) {
    float t1 = 1.0 - t;
    return normalize(3.0 * t1 * t1 * (p2 - p1) +
                    6.0 * t1 * t * (p3 - p2) +
                    3.0 * t * t * (p4 - p3));
}

// Calculate distance info for a cubic Bezier curve (p1, p2, p3, p4)
EdgeInfo distanceToCubicBezier(vec2 p, vec2 p1, vec2 p2, vec2 p3, vec2 p4, int edgeIndex) {
    EdgeInfo info;
    info.edgeIndex = edgeIndex;
    info.distance = 1e10;

    // Similar to quadratic, but with cubic formula
    const int STEPS = 20;  // Higher for better accuracy
    float bestT = 0.0;

    for (int i = 0; i <= STEPS; i++) {
	float t = float(i) / float(STEPS);
	float t1 = 1.0 - t;

	// Cubic Bezier formula: B(t) = (1-t)³p1 + 3(1-t)²tp2 + 3(1-t)t²p3 + t³p4
	vec2 pt = t1 * t1 * t1 * p1 +
	          3.0 * t1 * t1 * t * p2 +
	          3.0 * t1 * t * t * p3 +
	          t * t * t * p4;
	float dist = length(p - pt);

	if (dist < info.distance) {
	    info.distance = dist;
	    info.position = pt;
	    bestT = t;
	}
    }

    // Get tangent at the closest point and use it to compute the normal
    vec2 tangent = cubicBezierTangent(p1, p2, p3, p4, bestT);
    // Normal is perpendicular to tangent
    info.normal = normalize(vec2(-tangent.y, tangent.x));

    // Ensure normal points away from shape
    if (dot(info.normal, p - info.position) < 0.0) {
	info.normal = -info.normal;
    }

    return info;
}


// Generate pseudorandom angle for consistent edge selection
float generateAngle(int edgeIndex) {
    // Simple hash function to map edge indices to consistent angles
    float angle = float(edgeIndex * 1673 + 4931) * 0.001;
    return mod(angle, 2.0 * 3.14159265);
}

// Determine if an edge belongs to a specific MSDF channel based on its angle
bool edgeBelongsToChannel(vec2 normal, int channel) {
    // Convert normal to angle in [0, 2π]
    float angle = atan(normal.y, normal.x);
    if (angle < 0.0) angle += 2.0 * 3.14159265;

    // Divide angle space into three 120° sectors
    float sectorAngle = 2.0 * 3.14159265 / 3.0;
    float sectorStart = float(channel) * sectorAngle;
    float sectorEnd = sectorStart + sectorAngle;

    return (angle >= sectorStart && angle < sectorEnd);
}

void main(void)
{
    ivec2 threadId = ivec2(gl_GlobalInvocationID.xy);
    ivec2 imageDimensions = imageSize(msdfTexture);
    if (threadId.x >= imageDimensions.x || threadId.y >= imageDimensions.y) {
	return;
    }

    vec2 pos = vec2(threadId) + vec2(0.5);
    vec2 p = pos;

    // Initialize channel distances to max value
    vec3 channelDistances = vec3(1e10);

    // Find closest edge for each channel
    for (int i = 0; i < edges.length(); ++i) {
	EdgeInfo info;

	// Calculate distance info based on edge type
	if (edges[i].type == LINEAR) {
	    info = distanceToLineSegment(pos, edges[i].points[0], edges[i].points[1], i);
	}
	else if (edges[i].type == QUADRATIC) {
	    info = distanceToQuadraticBezier(pos, edges[i].points[0], edges[i].points[1], edges[i].points[2], i);
	}
	else if (edges[i].type == CUBIC) {
	    info = distanceToCubicBezier(pos, edges[i].points[0], edges[i].points[1],
	                                edges[i].points[2], edges[i].points[3], i);
	}

	// Determine which channel this edge contributes to based on its normal
	float angle = atan(info.normal.y, info.normal.x);
	if (angle < 0.0) angle += 2.0 * 3.14159265;

	// Map angle to channel (RGB) - dividing 360° into three 120° sectors
	int channel = int(floor(mod(angle, 2.0 * 3.14159265) / (2.0 * 3.14159265 / 3.0)));

	// Update channel distance if this edge is closer
	channelDistances[channel] = min(channelDistances[channel], info.distance);
    }

    // Determine if the point is inside or outside the shape

    int winding = 0;
    int edgeCount = edges.length();

    for (int i = 0; i < edgeCount; i++) {
	EdgeSegment edge = edges[i];

	// Process based on the edge type
	if (edge.type == LINEAR) {
	    vec2 p1 = edge.points[0];
	    vec2 p2 = edge.points[1];

	    // Check if the ray from p going right crosses this edge
	    if ((p1.y <= p.y && p2.y > p.y) || (p1.y > p.y && p2.y <= p.y)) {
		float intersectX = p1.x + (p.y - p1.y) * (p2.x - p1.x) / (p2.y - p1.y);
		if (p.x < intersectX) {
		    winding += (p1.y <= p.y) ? 1 : -1;
		}
	    }
	}
	else if (edge.type == QUADRATIC || edge.type == CUBIC) {
	    // For curved segments, we approximate with multiple line segments
	    const int CURVE_STEPS = 12;
	    vec2 prevPoint;

	    for (int j = 0; j <= CURVE_STEPS; j++) {
		float t = float(j) / float(CURVE_STEPS);
		vec2 currPoint;

		if (edge.type == QUADRATIC) {
		    float t1 = 1.0 - t;
		    currPoint = t1 * t1 * edge.points[0] +
		                2.0 * t1 * t * edge.points[1] +
		                t * t * edge.points[2];
		}
		else { // CUBIC
		    float t1 = 1.0 - t;
		    currPoint = t1 * t1 * t1 * edge.points[0] +
		                3.0 * t1 * t1 * t * edge.points[1] +
		                3.0 * t1 * t * t * edge.points[2] +
		                t * t * t * edge.points[3];
		}

		if (j > 0) {
		    // Check if the ray from p going right crosses this segment
		    if ((prevPoint.y <= p.y && currPoint.y > p.y) || (prevPoint.y > p.y && currPoint.y <= p.y)) {
			float intersectX = prevPoint.x + (p.y - prevPoint.y) * (currPoint.x - prevPoint.x) / (currPoint.y - prevPoint.y);
			if (p.x < intersectX) {
			    winding += (prevPoint.y <= p.y) ? 1 : -1;
			}
		    }
		}

		prevPoint = currPoint;
	    }
	}
    }

    // If winding number is non-zero, point is inside
    bool inside = winding != 0;

    // Apply sign based on inside/outside
    //vec3 signedDistances = inside ? -channelDistances : channelDistances;
    vec3 signedDistances = channelDistances;

    // Store MSDF in RGB channels and inside state in alpha channel
    imageStore(msdfTexture, threadId, vec4(signedDistances, 1.0));
    imageStore(isInsideTex, threadId, vec4(float(inside), float(inside), float(inside), 1.0));
}
