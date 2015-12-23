/**
 * Simple Quaternion class to convert euler angles from Cortex to quaternions.
 */

#pragma once

#define _USE_MATH_DEFINES
#include "math.h"

#define RADIANS(x) (x * M_PI / 180.0)
#define DEGREES(x) (x / M_PI * 180.0)


class Vector3D
{
public:
	float x, y, z;

public:
	Vector3D()
	{
		x = y = z = 0;
	}

	void set(float x, float y, float z)
	{
		this->x = x; this->y = y; this->z = z;
	}

};


class Quaternion
{
public:
	float x, y, z, w;

public:
	Quaternion()
	{
		x = y = z = 0;
		w = 1;
	}

	Quaternion(float x, float y, float z, float angle)
	{
		float s2 = sinf(angle / 2);
		this->x = x * s2;
		this->y = y * s2;
		this->z = z * s2;
		this->w = cosf(angle / 2);
	}

	void fromAxisAngle(float x, float y, float z, float angle)
	{
		float s2 = sinf(angle / 2);
		this->x = x * s2;
		this->y = y * s2;
		this->z = z * s2;
		this->w = cosf(angle / 2);
	}

	Quaternion& mult(const Quaternion& q)
	{
		float _w = q.w*w - q.x*x - q.y*y - q.z*z;
		float _x = q.w*x + q.x*w - q.y*z + q.z*y;
		float _y = q.w*y + q.x*z + q.y*w - q.z*x;
		float _z = q.w*z - q.x*y + q.y*x + q.z*w;
		w = _w; x = _x; y = _y; z = _z;
		return *this;
	}
};
