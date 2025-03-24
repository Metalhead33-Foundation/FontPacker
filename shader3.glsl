#version 430 core
layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
// Output SDF texture
layout (binding = 1, r32f) writeonly uniform image2D rawSdfTexture;
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

// Calculate distance to line segment (p1, p2)
float distanceToLineSegment(vec2 p, vec2 p1, vec2 p2) {
    vec2 v = p2 - p1;
    vec2 w = p - p1;

    // Project point onto line segment
    float c1 = dot(w, v);
    if (c1 <= 0.0) {
	return length(p - p1);
    }

    float c2 = dot(v, v);
    if (c2 <= c1) {
	return length(p - p2);
    }

    float b = c1 / c2;
    vec2 pb = p1 + b * v;
    return length(p - pb);
}

// Calculate distance to quadratic Bezier curve (p1, p2, p3)
float distanceToQuadraticBezier(vec2 p, vec2 p1, vec2 p2, vec2 p3) {
    // We'll use an iterative approach to find the closest point on the curve
    float minDist = 1e10;
    const int STEPS = 10;  // Increase for better accuracy

    for (int i = 0; i <= STEPS; i++) {
	float t = float(i) / float(STEPS);
	float t1 = 1.0 - t;

	// Quadratic Bezier formula: B(t) = (1-t)²p1 + 2(1-t)tp2 + t²p3
	vec2 pt = t1 * t1 * p1 + 2.0 * t1 * t * p2 + t * t * p3;
	float dist = length(p - pt);

	minDist = min(minDist, dist);
    }

    return minDist;
}

// Calculate distance to cubic Bezier curve (p1, p2, p3, p4)
float distanceToCubicBezier(vec2 p, vec2 p1, vec2 p2, vec2 p3, vec2 p4) {
    // Similar to quadratic, but with cubic formula
    float minDist = 1e10;
    const int STEPS = 12;  // Increase for better accuracy

    for (int i = 0; i <= STEPS; i++) {
	float t = float(i) / float(STEPS);
	float t1 = 1.0 - t;

	// Cubic Bezier formula: B(t) = (1-t)³p1 + 3(1-t)²tp2 + 3(1-t)t²p3 + t³p4
	vec2 pt = t1 * t1 * t1 * p1 +
	          3.0 * t1 * t1 * t * p2 +
	          3.0 * t1 * t * t * p3 +
	          t * t * t * p4;
	float dist = length(p - pt);

	minDist = min(minDist, dist);
    }

    return minDist;
}

void main(void)
{
    ivec2 threadId = ivec2(gl_GlobalInvocationID.xy);
    ivec2 imageDimensions = imageSize(rawSdfTexture);
    if (threadId.x >= imageDimensions.x || threadId.y >= imageDimensions.y) {
	return;
    }

    vec2 pos = vec2(threadId) + vec2(0.5);
    vec2 p = pos;
    float minDistance = 1e30;

    // Calculate minimum distance to any edge
    for(int i = 0; i < edges.length(); ++i) {
	float distance = 1e30;

	// Calculate distance based on edge type
	if (edges[i].type == LINEAR) {
	    distance = distanceToLineSegment(pos, edges[i].points[0], edges[i].points[1]);
	}
	else if (edges[i].type == QUADRATIC) {
	    distance = distanceToQuadraticBezier(pos, edges[i].points[0], edges[i].points[1], edges[i].points[2]);
	}
	else if (edges[i].type == CUBIC) {
	    distance = distanceToCubicBezier(pos, edges[i].points[0], edges[i].points[1], edges[i].points[2], edges[i].points[3]);
	}

	minDistance = min(minDistance, distance);
    }

    // Determine if the point is inside or outside the shape

    int winding = 0;
    int edgeCount = edges.length();

    for (int i = 0; i < edgeCount; i++) {
	EdgeSegment edge = edges[i];

	// We need to process based on the edge type
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
	    const int CURVE_STEPS = 8;
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
		    // Check if the ray from p going right crosses this line segment
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
    bool inside = winding != 0;
    float signedDistance = minDistance;

    // Store results
    imageStore(rawSdfTexture, threadId, vec4(signedDistance, signedDistance, signedDistance, 1.0));
    imageStore(isInsideTex, threadId, vec4(float(inside), float(inside), float(inside), 1.0));
}
